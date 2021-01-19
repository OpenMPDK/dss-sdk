"""
Usage: kv-cli.py daemon [options] [--] <endpoint> <port>
       kv-cli.py daemon [options] [--]
       kv-cli.py daemon --start_tgt [internal_flag]
       kv-cli.py daemon --stop_tgt [internal_flag]

Options:
  -h --help                    Show this screen.
  --version                    Show version.
  --stats                      Enable stats poll and send threads.
  --attribute_poll=<period>    Poll per seconds [default: 60].
  --monitor_poll=<period>      Subsystem monitor per seconds [default: 50].
  --performance_poll=<period>  Performance stats poll per seconds [default: 10].
  --start_tgt                   Start the target
  --stop_tgt                    Stop the target
"""
import collections
import copy
import exceptions
import hashlib
import os
import platform
import re
import signal
import subprocess
import sys
import time
import threading
import uuid
# import json

import pexpect
import psutil
from etcd3.utils import increment_last_byte, to_bytes

# This has to be before the imports of local modules
# This initializes the logger module for the other imports as well
import utils.daemon_config as agent_conf
from utils import log_setup
logger = log_setup.get_logger('KV-Agent', agent_conf.CONFIG_DIR + agent_conf.AGENT_CONF_NAME)
logger.info("Log config file: %s" % agent_conf.CONFIG_DIR + agent_conf.AGENT_CONF_NAME)

from minio_stats import MinioStats
from sflow_collection import SFlowStatsCollector
from network_stats import MellanoxRDMAStats
import constants
import graphite
import server_info.server_attributes_aggregator as ServerAttr
import spdk_setup
import utils.backend_layer as backend_layer
import utils.key_prefix_constants as key_cons
import utils.usr_signal as usr_signal
from device_driver_setup.driver_setup import DriverSetup
from diamond_conf import DiamondConf
from spdk_config_file.spdk_config import SPDKConfig
from server_info.server_hardware import collect_system_info
from utils.jsonrpc import SPDKJSONRPC
from utils.utils import find_process_pid
from utils.utils import pidfile_is_running
from utils.utils import check_spdk_running
# from utils.utils import _eval
from utils.target_events_processing import TargetEvents
# from events.events import get_events_from_etcd_db, save_events_to_etcd_db


# All the input args given to daemon
g_input_args = {}

g_node_name = None
g_cluster_name = None
g_daemon_obj = None
g_thread_arr = []
g_udev_monitor = None
g_nvmf_pid = None
g_poll_event = threading.Event()

# Lease refresh is moved to timer as the threads GIL is causing the main
# loop to hold during the target stop call causing the agent lease to expire
#  and generate a false alert
g_agent_lease_timer = None

# Global flag for the etcd server attributes initialized or not
g_etcd_server_attribute_init = False

# Global flag for wait/proceed in case of maintenance mode
NODE_MODE_MAINTENANCE_STRING = 'maintenance'
g_mode_event = threading.Event()

TARGET_COMMAND_KEY_SUFFIX = 'tgt_command'

g_restart_monitor_threads = 0
g_target_op_lock = None
g_mlnx_cmd_flag = True


class OSMDaemon:
    def __init__(self,
                 endpoint, port, fn_ptrs, ustat,
                 check_spdk, spdk_conf, driver_setup, stats_mode,
                 stats_server, stats_port, metrics_blacklist_regex=None):
        self.jsonrpc_recv_size = constants.JSONRPC_RECV_PACKET_SIZE
        self.default_spdk_rpc = constants.DEFAULT_SPDK_SOCKET_PATH
        self.synchronize_etcd = 0
        self.check_spdk = check_spdk
        self.spdk_running = -1
        self.spdk_conf = spdk_conf
        self.driver_setup = driver_setup
        self.uptime_hr = 0
        self.default_etcd_ttl = 120
        self.metrics_blacklist_regex = metrics_blacklist_regex

        self.ustat_path = ustat['ustat_binary_path']
        self.nvmf_pid = ustat['nvmf_pid']

        # etcd carbon address overrides local configuration file
        self.stats_server = stats_server
        self.stats_port = int(stats_port)
        self.stats_mode = stats_mode

        # ETCD watcher ids for cancellation
        self.etcd_watcher_ids = {}

        # Class device type constants
        self.TYPE_UNKNOWN = 0
        self.TYPE_NETWORK = 1
        self.TYPE_STORAGE = 2

        # Function pointers
        self.identity_fn = fn_ptrs['identity']
        self.cpu_fn = fn_ptrs['cpu']
        self.network_fn = fn_ptrs['network']
        self.storage_fn = fn_ptrs['storage']

        # Access backend and client for etcd connection/put/get/deletes
        self.backend = backend_layer.BackendLayer(endpoint, port)
        self.client = self.backend.client
        self.db_handle = self.backend.db_handle
        self.diamond_conf = DiamondConf()

        # Key-Value Database Key Prefixes
        system_identity = self.identity_fn()
        self.SERVER_UUID = system_identity["UUID"]
        self.SERVER_NAME = system_identity['Hostname']
        self.SERVER_BASE_KEY_PREFIX = (self.backend.ETCD_SRV_BASE +
                                       self.SERVER_UUID + '/')
        self.SERVER_ATTR_KEY_PREFIX = (self.SERVER_BASE_KEY_PREFIX +
                                       key_cons.SERVER_ATTRIBUTES)
        self.SERVER_CONFIG_KEY_PREFIX = (self.SERVER_BASE_KEY_PREFIX +
                                         key_cons.KV_ATTRIBUTES_SUBSYSTEMS)
        self.SERVER_LIST = (self.backend.ETCD_SRV_BASE + key_cons.SERVER_LIST
                            + self.SERVER_UUID)
        # MLNX performance statistics command
        self.MLNX_PERF_COMMAND = "mlnx_perf -i %s -c 1"

        # Cluster UUID
        try:
            self.CLUSTER_ID = self.client.get("/cluster/id")[0]
            self.shared_etcd_target_lock = self.client.lock(
                str(self.SERVER_UUID), ttl=self.default_etcd_ttl)
        except Exception as e:
            logger.error('Exception in getting the cluster ID', exc_info=True)
            raise e

        # Lock and queue shared in daemon
        self.catchup_lock = threading.Lock()
        self.subsys_op_lock = threading.Lock()
        self.poll_event = g_poll_event
        self.q = collections.deque()
        self.removed_nqn_q = collections.deque()

        # Local and etcd server attributes daemon start-up synchronization
        self.spdk_conf.delete_temp_local_config()

        # Target Events
        self.cached_target_status = {}
        self.target_event = TargetEvents(endpoint, port)

        logger.info("Synchronizing agent version")
        rpc_req = SPDKJSONRPC.build_payload("dfly_oss_version_info")
        results = SPDKJSONRPC.call(rpc_req, self.jsonrpc_recv_size,
                                   self.default_spdk_rpc)
        if "error" in results:
            tgt_version = None
            tgt_hash = None
        else:
            tgt_version = results["result"][0]["OSS_TARGET_VER"]
            tgt_hash = results["result"][0]["OSS_TARGET_HASH"]

        agent_info = {"agent": {"version": CLI_VERSION},
                      "target": {"version": tgt_version,
                                 "hash": tgt_hash}}
        try:
            self.backend.write_dict_to_etcd(agent_info,
                                            self.SERVER_BASE_KEY_PREFIX)
        except Exception as e:
            logger.error('Exception in writing agent version details to etcd',
                         exc_info=True)
            raise e

        # NQN <--> UUID Mapping
        self.nqn_uuid_map = {}

        # Retrieve existing NQN <--> UUID Mapping from etcd
        try:
            subsystems = self.backend.get_json_prefix(
                self.SERVER_CONFIG_KEY_PREFIX)
        except Exception:
            logger.error('Exception in getting NQN data', exc_info=True)
            subsystems = {}
        for subsystem in subsystems.values():
            try:
                self.nqn_uuid_map[subsystem["NQN"]] = subsystem["UUID"]
            except Exception as e:
                logger.error('nqn mapping failed ... {}'.format(e))

        self.local_server_attributes = {"identity": self.identity_fn(),
                                        "cpu": self.cpu_fn(),
                                        "network": self.network_fn(),
                                        "storage": self.storage_fn(), }
        entry = {"identity": self.local_server_attributes["identity"],
                 "cpu": self.local_server_attributes["cpu"], }
        logger.info("Synchronizing server identity and CPU attributes")
        try:
            self.backend.write_dict_to_etcd(entry, self.SERVER_ATTR_KEY_PREFIX)

        except Exception as e:
            logger.error('Exception in updating the server attributes to DB',
                         exc_info=True)
            raise e
        try:
            self.etcd_server_attributes = self.backend.get_json_prefix(
                self.SERVER_ATTR_KEY_PREFIX)
        except Exception as e:
            logger.error('Exception in getting the server attributes from DB',
                         exc_info=True)
            raise e

        if "network" not in self.etcd_server_attributes:
            self.etcd_server_attributes["network"] = {}
        if "storage" not in self.etcd_server_attributes:
            self.etcd_server_attributes["storage"] = {}
        try:
            logger.info("Updating network interface metadata")
            self.synchronize_device_metadata(self.TYPE_NETWORK)
            logger.info("Updating storage device metadata")
            self.synchronize_device_metadata(self.TYPE_STORAGE)
        except Exception as e:
            logger.error('Exception in synchronizing device metadata',
                         exc_info=True)
            raise e

        # Statsd/Graphite server address
        try:
            if 'stats' in self.etcd_server_attributes and 'address' in \
                    self.etcd_server_attributes['stats']:
                stats_mode, stats_server, stats_port = \
                    self.etcd_server_attributes["stats"]["address"].split(':')
                self.stats_mode = stats_mode
                self.stats_server = stats_server
                self.stats_port = int(stats_port)
                logger.info("Overriding Stats server address in configuration"
                            " file for etcd stats address")
        except Exception:
            logger.exception("Error in parsing stats server")

        if self.stats_mode == "graphite":
            # Create CarbonMessageManager object
            self.stats_obj = graphite.CarbonMessageManager(self.nvmf_pid,
                                                           self.stats_server,
                                                           self.stats_port)
        elif self.stats_mode == "statsd":
            # Create StatsDMessageManager object
            self.stats_obj = graphite.StatsdManager(self.nvmf_pid,
                                                    self.stats_server,
                                                    self.stats_port)
        else:
            # stats_obj can be created only for statsd/graphite mode
            # Other modes are not supported. So exit here
            logger.error("Invalid stats server mode given. Should be one of "
                         "graphite/statsd")
            sys.exit(-1)

        try:
            self.diamond_conf.update_diamond_conf(self.stats_mode, self.stats_server,
                                                  self.stats_port,
                                                  "cluster_id_" + g_cluster_name,
                                                  "target_id_" + self.SERVER_NAME + ".system_stats")
        except:
            logger.exception('Exception in updating diamond configuration')
            
    @staticmethod
    def find_set_delta(set_a, set_b):
        delta_arr = []
        set_delta = set_a - set_b
        for element in set_delta:
            delta_arr.append(element)
        return delta_arr

    def create_storage_metadata_entries(self):
        if "nvme" not in self.etcd_server_attributes["storage"]:
            self.etcd_server_attributes["storage"]["nvme"] = {}

        if "devices" not in self.etcd_server_attributes["storage"]["nvme"]:
            self.etcd_server_attributes["storage"]["nvme"]["devices"] = {}

        dev_cnt = len(self.etcd_server_attributes["storage"]["nvme"]["devices"])
        total_bytes = 0
        for dev in self.etcd_server_attributes["storage"]["nvme"]["devices"].values():
            total_bytes += int(dev["SizeInBytes"])
        entry = {"storage": {"Count": dev_cnt,
                             "Capacity": total_bytes,
                             "nvme": {"Count": dev_cnt,
                                      "Capacity": total_bytes, }, }, }
        return entry

    def create_network_interface_metadata_entries(self):
        if "interfaces" not in self.etcd_server_attributes["network"]:
            self.etcd_server_attributes["network"]["interfaces"] = {}

        interface_count = len(self.etcd_server_attributes["network"]["interfaces"])
        entry = {"network": {"Count": interface_count}}
        return entry

    def create_metadata_entries(self, _type):
        if _type == self.TYPE_NETWORK:
            entry = self.create_network_interface_metadata_entries()
        elif _type == self.TYPE_STORAGE:
            entry = self.create_storage_metadata_entries()
        else:
            assert 0
        return entry

    def synchronize_device_metadata(self, _type):
        entry = self.create_metadata_entries(_type)
        try:
            self.backend.write_dict_to_etcd(entry, self.SERVER_ATTR_KEY_PREFIX)
        except Exception as e:
            logger.error('Exception in writing device metadata', exc_info=True)
            raise e

    def synchronize_device(self, local_devices, etcd_devices, key_prefix, _type):
        # Identify different devices in local compared to etcd
        # Remove missing devices
        local_devices_set = set(local_devices.keys())
        etcd_devices_set = set(etcd_devices.keys())
        remove_dev = self.find_set_delta(etcd_devices_set, local_devices_set)

        for dev in remove_dev:
            # Remove device since it doesn't exist
            if _type == self.TYPE_STORAGE:
                if not self.spdk_running or \
                        "ignore" in etcd_devices[dev] and \
                        int(etcd_devices[dev]["ignore"]) == 1:
                    continue
            logger.info(">>>>>'%s' no longer found deleting from etcd" % dev)
            full_prefix = "%s%s/" % (key_prefix, dev)
            try:
                self.client.delete_prefix(full_prefix)
                del (etcd_devices[dev])
                self.synchronize_device_metadata(_type)
            except Exception:
                logger.exception('Exception in DB operation')

        # Compare CRC with each interface
        for dev in local_devices:
            entry = self.create_metadata_entries(_type)  # type: dict
            if _type == self.TYPE_NETWORK:
                entry["network"]["interfaces"] = {dev: local_devices[dev], }
            elif _type == self.TYPE_STORAGE:
                entry["storage"]["nvme"]["devices"] = {dev: local_devices[dev], }
            else:
                assert 0

            try:
                # Update the same device
                if int(etcd_devices[dev]["CRC"]) != int(local_devices[dev]["CRC"]):
                    logger.debug("Mismatched CRC for '%s'" % dev)
                    try:
                        self.backend.write_dict_to_etcd(entry, self.SERVER_ATTR_KEY_PREFIX)
                    except Exception:
                        logger.exception('Update device info to DB failed')
            except Exception:
                # Add new device
                logger.info("Adding new entry '%s' to etcd" % dev)
                try:
                    self.backend.write_dict_to_etcd(entry, self.SERVER_ATTR_KEY_PREFIX)
                except Exception:
                    logger.exception('Adding new device info to DB failed')
            etcd_devices[dev] = local_devices[dev]

    def synchronize_type(self, _type):
        update_existing = 0
        local_devices = None
        etcd_devices = None
        key_prefix = None
        if _type == self.TYPE_NETWORK:
            etcd_type_root_key = self.etcd_server_attributes["network"]
            local_type_root_key = self.local_server_attributes["network"]
            local_entry = {"network": local_type_root_key, }
            if "interfaces" in etcd_type_root_key:
                etcd_devices = etcd_type_root_key["interfaces"]
                if etcd_devices:
                    update_existing = 1
                    local_devices = local_type_root_key["interfaces"]
                    etcd_devices = self.etcd_server_attributes["network"]["interfaces"]
                    key_prefix = self.SERVER_ATTR_KEY_PREFIX + 'network/interfaces/'
        elif _type == self.TYPE_STORAGE:
            etcd_type_root_key = self.etcd_server_attributes["storage"]
            local_type_root_key = self.local_server_attributes["storage"]
            local_entry = {"storage": local_type_root_key, }
            if "nvme" in etcd_type_root_key and \
                    "devices" in etcd_type_root_key["nvme"]:
                etcd_devices = etcd_type_root_key["nvme"]["devices"]
                if etcd_devices:
                    update_existing = 1
                    local_devices = self.local_server_attributes["storage"]["nvme"]["devices"]
                    etcd_devices = self.etcd_server_attributes["storage"]["nvme"]["devices"]
                    key_prefix = self.SERVER_ATTR_KEY_PREFIX + 'storage/nvme/devices/'
        else:
            assert 0

        if update_existing:
            if _type == self.TYPE_STORAGE and not local_devices:
                # Fixing a case when all drives are configured in subsystem & we can't connect to RPC
                # because Target is down & there are no local devices. Skipping synchronize..
                logger.info("No devices to synchronize. Check if Target is running..")
            elif local_devices and etcd_devices and key_prefix:
                self.synchronize_device(local_devices, etcd_devices, key_prefix, _type)
            else:
                assert 0
        else:
            # Devices doesn't exist then write what we have to etcd
            temp_dict = copy.deepcopy(local_entry)
            if _type == self.TYPE_NETWORK:
                devices = temp_dict["network"].pop("interfaces")
            elif _type == self.TYPE_STORAGE:
                devices = temp_dict["storage"]["nvme"].pop("devices")
            else:
                assert 0

            while devices:
                _uuid = devices.keys()[0]
                device = devices.pop(_uuid)
                if _type == self.TYPE_NETWORK:
                    entry = {"network": {"interfaces": {device["MACAddress"]: device}}}
                elif _type == self.TYPE_STORAGE:
                    entry = {"storage": {"nvme": {"devices": {device["Serial"]: device}}}}
                else:
                    assert 0
                self.backend.write_dict_to_etcd(entry, self.SERVER_ATTR_KEY_PREFIX)
            self.backend.write_dict_to_etcd(temp_dict, self.SERVER_ATTR_KEY_PREFIX)
            etcd_type_root_key.update(local_type_root_key)

    def server_info_to_db(self):
        """
        Prepare to send server information to database.
        :return:
        """
        global g_etcd_server_attribute_init

        logger.debug("Server UUID:        " + self.SERVER_UUID)
        self.shared_etcd_target_lock.acquire(None)
        spdk_running = self.check_spdk()[1]
        if spdk_running != self.spdk_running:
            self.spdk_running = spdk_running
        logger.debug("Updating etcd with SPDK status")
        try:
            if g_input_args:
                lease_obj = self.client.lease(g_input_args[
                                                  'attribute_poll'] + 10)
            else:
                lease_obj = None

            if spdk_running:
                self.client.put(self.SERVER_ATTR_KEY_PREFIX +
                                "spdk_status", "up", lease_obj)
            else:
                self.client.put(self.SERVER_ATTR_KEY_PREFIX +
                                "spdk_status", "down", lease_obj)
        except Exception:
            logger.exception('Exception in updating the spdk_status to DB')

        if self.etcd_server_attributes == {}:
            # First time sending to etcd
            try:
                self.backend.write_dict_to_etcd(self.local_server_attributes,
                                                self.SERVER_ATTR_KEY_PREFIX)
            except Exception:
                logger.exception('Exception in writing the server attributes to DB')
            self.etcd_server_attributes = self.local_server_attributes
        else:
            # Compare network
            self.synchronize_type(self.TYPE_NETWORK)
            # Compare storage
            self.synchronize_type(self.TYPE_STORAGE)

        g_etcd_server_attribute_init = True

        heartbeat = str(int(time.time()))
        try:
            self.client.put(self.SERVER_LIST, heartbeat)
        except Exception:
            logger.exception('Exception in writing heartbeat time to DB')
        uptime_hr = str(int(time.time() - psutil.boot_time()) // 3600)
        if self.uptime_hr != uptime_hr:
            try:
                self.client.put(self.SERVER_ATTR_KEY_PREFIX + "uptime", uptime_hr)
            except Exception:
                logger.exception('Exception in writing uptime to DB')
            self.uptime_hr = uptime_hr
            logger.info("Server Uptime:      %s hrs" % uptime_hr)
        self.shared_etcd_target_lock.release()
        logger.debug("Heartbeat:          %s" % heartbeat)
        logger.debug("==== Server polled ====")

    def get_mlnx_perf_metrics(self):
        global g_mlnx_cmd_flag
        mlnx_perf_metrics = []

        # If mlnx_perf command not found, then disable this flag to avoid
        # further monitoring
        if not g_mlnx_cmd_flag:
            return mlnx_perf_metrics
        etcd_network_prefix = self.SERVER_ATTR_KEY_PREFIX + "network"
        logger.debug("etcd network prefix %s", etcd_network_prefix)
        try:
            ifaces = self.backend.get_json_prefix(etcd_network_prefix)
            logger.debug('ifaces from etcd backend %s', ifaces)
            if ifaces:
                ifaces = ifaces.values()[0]
            if ifaces and 'interfaces' in ifaces:
                for mac in ifaces['interfaces']:
                    iface = ifaces['interfaces'][mac]
                    if 'Mellanox' not in iface['Vendor']:
                        logger.debug('%s is not mellanox device', iface)
                        continue
                    if iface['Status'] != 'up' and int(iface['Speed']) > 0:
                        logger.debug('%s is down', iface)
                        continue
                    logger.debug('Running mlnx_perf on dev %s',
                                 iface['InterfaceName'])
                    # cmd = "mlnx_perf -i " + iface['InterfaceName'] + ' -c 1'
                    cmd = self.MLNX_PERF_COMMAND % iface['InterfaceName']
                    try:
                        timestamp = time.time()
                        pipe = subprocess.Popen(cmd.split(),
                                                stdout=subprocess.PIPE,
                                                stderr=subprocess.PIPE)
                        out, err = pipe.communicate()
                        if err:
                            logger.error('Error in running mlnx_perf %s', err)
                        if out:
                            logger.debug('mlnx_perf on dev %s output <%s>',
                                         iface['InterfaceName'], out)
                            out_list = out.split('\n')
                            rx_bytes_kb = None
                            tx_bytes_kb = None
                            for ln in out_list:
                                if 'rx_bytes_phy' in ln:
                                    rx_bytes = re.findall(r'rx_bytes_phy: ([0-9,'']+) Bps', ln)[0].split(',')
                                    rx_bytes_kb = int("".join(rx_bytes))/1024

                                if 'tx_bytes_phy' in ln:
                                    tx_bytes = re.findall(r'x_bytes_phy: ([0-9,]+) Bps', ln)[0].split(',')
                                    tx_bytes_kb = int("".join(tx_bytes))/1024
                            if rx_bytes_kb:
                                metric = \
                                    'rx_bandwidth_kbps;cluster_id=%s;target_id=%s;network_mac=%s' % \
                                    (g_cluster_name, self.SERVER_NAME, mac)
                                mlnx_perf_metrics.append((metric, (timestamp,
                                                                   str(rx_bytes_kb))))

                            if tx_bytes_kb:
                                metric = \
                                    'tx_bandwidth_kbps;cluster_id=%s;target_id=%s;network_mac=%s' % \
                                    (g_cluster_name, self.SERVER_NAME, mac)
                                mlnx_perf_metrics.append((metric, (timestamp,
                                                                   str(tx_bytes_kb))))
                            logger.debug('Mellanox NIC metrics %s',
                                         str(mlnx_perf_metrics))
                    except Exception as e:
                        if type(e) == exceptions.OSError and e.errno == 2:
                            logger.info('mlnx_perf command not found')
                            g_mlnx_cmd_flag = False
                        else:
                            logger.exception('Exception in getting the network'
                                             ' interfaces stats')
        except Exception:
            logger.exception('Exception in getting the network interfaces '
                             'from etcd')
        return mlnx_perf_metrics

    def send_disk_network_utilization_to_graphite(self, stopper_event, sec):
        while not stopper_event.is_set():
            subsystems = self.backend.get_json_prefix(self.SERVER_CONFIG_KEY_PREFIX)
            tuples = []
            for subsystem in subsystems.values():
                if 'namespaces' not in subsystem:
                    continue
                for ns in subsystem["namespaces"]:
                    serial = subsystem["namespaces"][ns]["Serial"]
                    try:
                        device = self.etcd_server_attributes["storage"]["nvme"]["devices"][serial]
                    except Exception:
                        continue

                    if 'DiskUtilizationPercentage' not in device:
                        logger.error('DiskUtilizationPercentage not found for device %s', device['Serial'])
                        continue

                    diskUsage = float(device["DiskUtilizationPercentage"])
                    if diskUsage < 0.0 or diskUsage > 1.0:
                        logger.error('Invalid disk usage range (%s) for device (%s)', diskUsage, serial)
                        # tmpStr = json.dumps(device, indent=4, sort_keys=True)
                        # logger.error("Device = {}\n".format(tmpStr))

                    if 'NQN' not in subsystem:
                        continue

                    if subsystem["NQN"] not in self.nqn_uuid_map:
                        continue

                    # subsystem_uuid = self.nqn_uuid_map[subsystem["NQN"]]

                    nqn_name = subsystem["NQN"].replace('.', '-')

                    metric_path = "freespacepercent;cluster_id=%s;target_id=%s;nqn_id=%s;disk_id=%s;type=disk" % \
                                  (g_cluster_name, self.SERVER_NAME, nqn_name, serial)

                    timestamp = time.time()
                    value = str(1.0 - diskUsage)
                    if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                        continue
                    tuples.append((metric_path, (timestamp, value)))
            nw_tuples = self.get_mlnx_perf_metrics()
            tuples += nw_tuples
            if tuples:
                self.stats_obj.submit_message(tuples)
            stopper_event.wait(sec)

    def sync_count_inc(self):
        self.catchup_lock.acquire()
        self.synchronize_etcd += 1
        self.catchup_lock.release()

    def sync_count_dec(self):
        self.catchup_lock.acquire()
        self.synchronize_etcd -= 1
        assert (self.synchronize_etcd >= 0)
        self.catchup_lock.release()

    def sync_with_etcd(self):
        if self.shared_etcd_target_lock.is_acquired():
            logger.info("Daemon start catch-up with etcd")
            try:
                all_config = self.backend.get_json_prefix(self.SERVER_CONFIG_KEY_PREFIX)
            except Exception:
                logger.exception('Exception in getting the data from DB for %s', self.SERVER_CONFIG_KEY_PREFIX)
                all_config = {}
            for dev in all_config.values():
                if 'Command' not in dev:
                    continue
                config_cmd = dev["Command"]
                config_status = dev["cmd_status"]
                subsystem_path = self.SERVER_UUID + "_" + dev["NQN"]
                subsystem_lock = self.client.lock(str(subsystem_path), ttl=self.default_etcd_ttl)
                if config_cmd == "construct_nvmf_subsystem":
                    if config_status == "pending" or config_status == "processing":
                        logger.info("Catching up on %s for %s %s" % (dev["NQN"], config_status, config_cmd))
                        self.sync_count_inc()
                        self.q.append((subsystem_lock, dev))
                elif config_cmd == "delete_nvmf_subsystem":
                    logger.info("Catching up on %s for %s" % (config_cmd, config_status))
                    self.sync_count_inc()
                    self.q.append((subsystem_lock, dev))
                else:
                    logger.error("Unknown command found in etcd for %s" % dev["NQN"])

            if self.synchronize_etcd:
                logger.info("Waiting for catch-up queue to empty")
                while self.synchronize_etcd:
                    time.sleep(5)
            logger.info("Daemon stop catch-up with etcd")
            logger.info("==== Daemon catch-up complete ====")
        else:
            logger.info("Sync with etcd requires lock")
            sys.exit(-1)

    def subsystem_config_watcher_callback(self, event):
        try:
            if event.key.endswith('status') and event.value:
                tokens = event.key.split('/')
                nqn = tokens[-2]
                value = event.value
                if value == "pending":
                    search_prefix = self.SERVER_CONFIG_KEY_PREFIX + nqn
                    subsystem_path = self.SERVER_UUID + "_" + nqn
                    subsystem_lock = self.client.lock(subsystem_path, ttl=self.default_etcd_ttl)
                    config_dict = self.backend.get_json_prefix(search_prefix).values()[0]
                    if len(config_dict) < constants.SPDK_CONF_NUM_OF_MD_PARAMS:
                        logger.info("Developer error: %d SPDK parameters received, but expected at least %d arguments" %
                                    (len(config_dict), constants.SPDK_CONF_NUM_OF_MD_PARAMS))
                        assert 0
                    if config_dict:
                        self.q.append((subsystem_lock, config_dict))
        except Exception:
            logger.exception('Exception in subsystem_config_watcher_cb fn')

    def subsystem_config_watcher(self):
        """
        Start watching this daemon's server configuration
        directory on etcdv3. Will continually wait on
        events_iterator for new events and add it into
        self.q for function server_config_poll to process.
        :return:
        """
        self.shared_etcd_target_lock.acquire(None)
        server_config_key_prefix = self.SERVER_CONFIG_KEY_PREFIX
        if 'subsystem_config_watcher' not in self.etcd_watcher_ids:
            try:
                watch_id = self.client.add_watch_callback(
                    server_config_key_prefix,
                    self.subsystem_config_watcher_callback,
                    range_end=increment_last_byte(to_bytes(server_config_key_prefix)))
                self.etcd_watcher_ids['subsystem_config_watcher'] = watch_id
                logger.info("Daemon is watching server config key %s", server_config_key_prefix)
            except Exception:
                logger.error('subsystem_config_watcher failed', exc_info=True)
        else:
            logger.info("Daemon is already watching server config key %s",
                        server_config_key_prefix)

        self.sync_with_etcd()
        self.shared_etcd_target_lock.release()
        logger.info("Daemon is ready for new commands")
        logger.info("==== Watchers ready ====")

    def set_device_config_status(self, lock, nqn_key, status_msg):
        """
        Set status for the subsystem with the provided NQN from etcdv3.
        :param lock: etcd lock to check.
        :param nqn_key: NQN that identifies the subsystem key prefix.
        :param status_msg: Value to set status to.
        :return:
        """
        lock_flag = False
        if not lock.is_acquired():
            logger.info('lock not acquired for key %s. Acquiring one', nqn_key)
            lock.acquire(None)
            lock_flag = True

        status_key = self.SERVER_CONFIG_KEY_PREFIX + nqn_key + '/cmd_status'
        try:
            self.client.put(status_key, status_msg)
            logger.info('Updated the device config[%s] to %s',
                        status_key, status_msg)
        except Exception:
            logger.exception('Exception in setting device config status %s to %s',
                             status_key, status_msg)

        if lock_flag:
            lock.release()

    def remove_device_config(self, lock, nqn_key):
        """
        Remove the subsystem with the provided NQN from etcdv3.
        :param lock: etcd lock to check.
        :param nqn_key: NQN that identifies the subsystem key prefix.
        :return:
        """
        full_prefix = "%s%s/" % (self.SERVER_CONFIG_KEY_PREFIX, nqn_key)
        if lock.is_acquired():
            try:
                self.client.delete_prefix(full_prefix)
                logger.info("Removed key prefixed with %s" % full_prefix)
            except Exception:
                logger.exception("Exception in removing key prefixed with %s" % full_prefix)
        else:
            logger.info("Requires server prefix to be locked to delete NQN config")
            sys.exit(-1)

    def get_namespaces(self):
        rpc_req = SPDKJSONRPC.build_payload("get_bdevs")
        results = SPDKJSONRPC.call(rpc_req, self.jsonrpc_recv_size, self.default_spdk_rpc)
        if "error" in results:
            return None
        else:
            return results["result"]

    def create_namespaces(self, namespace):
        sn = namespace["Serial"]
        pci = namespace["PCIAddress"]
        rpc_nvme_bdev_args = {"trtype": "PCIe",
                              "name": sn,
                              "traddr": pci, }

        rpc_req = SPDKJSONRPC.build_payload("construct_nvme_bdev", rpc_nvme_bdev_args)
        results = SPDKJSONRPC.call(rpc_req, self.jsonrpc_recv_size, self.default_spdk_rpc)
        if "error" in results:
            # TODO Handle partially created namespaces
            logger.info("Failed to construct_nvme_bdev for '%s'" % sn)
            assert 0
        return 0

    def subsystem_fail(self, command, lock, nqn, namespaces):
        if command == "construct_nvmf_subsystem":
            # construct_nvmf_subsystem failure
            if constants.SPDK_RESET:
                for ns in namespaces.values():
                    pci_addr = ns["PCIAddress"]
                    self.driver_setup.setup("nvme", pci_addr)
            self.remove_device_config(lock, nqn)
            self.spdk_conf.delete_temp_local_config()
        elif command == "delete_nvmf_subsystem":
            # TODO Handle delete_nvmf_subsystem error
            # We should not be failing here, but if it does happen
            # just revert things to where they were
            # delete_nvmf_subsystem failure
            # Revert status and command version
            logger.info("Revert here")
            assert 0
        else:
            sys.exit(-1)

    def server_config_poll(self, stopper_event):
        """
        Checks self.q for events every second.
        Processes and applies new configuration
        requests. Runs indefinitely.
        :return:
        """
        global g_restart_monitor_threads

        def execute_subsystem_rpc():
            spdk_namespaces = self.get_namespaces()
            for nsid in namespaces:
                pci_addr = namespaces[nsid]["PCIAddress"]
                found = 0
                for ns in spdk_namespaces:
                    try:
                        spdk_pci_addr = ns["driver_specific"]["nvme"]["trid"]["traddr"]
                        if pci_addr == spdk_pci_addr:
                            found = 1
                            break
                    except Exception:
                        continue
                if found:
                    continue
                else:
                    # Build the SPDK RPC req for the command
                    # construct_nvme_bdev and execute it.
                    self.create_namespaces(namespaces[nsid])

            rpc_req = SPDKJSONRPC.build_payload(command, rpc_args)
            # logger.info(json.dumps(rpc_req, indent=2))
            subsystem_lock.refresh()
            logger.info("Sending JSON RPC call for nqn %s", nqn)
            results = SPDKJSONRPC.call(rpc_req, self.jsonrpc_recv_size, self.default_spdk_rpc)
            subsystem_lock.refresh()
            logger.info("Sending JSON RPC call complete for nqn %s", nqn)
            # logger.info("Results: \n" + json.dumps(results, indent=2))
            return results

        def get_numa_alignment(build_rpc=True):
            numa_aligned = 1
            numa_node = None
            for nsid in namespaces:
                sn = namespaces[nsid]["Serial"]
                if build_rpc:
                    rpc_args["namespaces"].append({"bdev_name": "%sn1" % sn,
                                                    "nsid": int(nsid), })

                # NUMA Alignment check for storage devices
                if numa_aligned:
                    cur_numa_node = self.etcd_server_attributes["storage"]["nvme"]["devices"][sn]["NUMANode"]
                    if numa_node is None:
                        numa_node = cur_numa_node
                    elif cur_numa_node != numa_node:
                        numa_aligned = 0
            return numa_aligned

        while not stopper_event.is_set():
            # If the etcd server attributes are not initialized, then wait
            # till it gets initialized. Otherwise the nvmf subsystem calls fail
            if not g_etcd_server_attribute_init:
                stopper_event.wait(5)
                continue

            # Flag used for restarting the target and monitoring threads
            target_op_flag = False

            config_dict = None
            subsystem_lock = None
            if self.q:
                self.subsys_op_lock.acquire()
                subsystem_lock, config_dict = self.q.popleft()
                logger.info("Attempting to lock %s (%s)" %
                            (subsystem_lock.key, subsystem_lock))
                subsystem_lock.acquire(None)

            if config_dict:
                command = config_dict["Command"]
                nqn = config_dict["NQN"]
                namespaces = config_dict["namespaces"]
                transport_addresses = config_dict["transport_addresses"]

                if subsystem_lock:
                    self.set_device_config_status(subsystem_lock, nqn, "processing")
                else:
                    logger.error('ERROR - subsystem lock is missing. '
                                 'Something went wrong.')
                    # assert 0

                d = {}
                numa_aligned = None
                if command == "construct_nvmf_subsystem":
                    for ns in namespaces.values():
                        pci_addr = ns["PCIAddress"]
                        driver = self.driver_setup.get_driver_name(pci_addr)
                        if driver != "uio_pci_generic":
                            try:
                                self.driver_setup.setup("uio_pci_generic", pci_addr)
                            except Exception:
                                logger.exception('Failed to initialize the '
                                                 'device %s to '
                                                 'uio_pci_generic', pci_addr)

                            target_op_flag = True

                    if target_op_flag:
                        try:
                            logger.info('Restarting target because of driver change')
                            ret = os.system('systemctl restart nvmf_tgt@internal_flag.service')
                            logger.info('Restart Target with return code %s',
                                        ret)
                        except Exception:
                            logger.exception('Failed to restart nvmf_tgt service')
                        time.sleep(60)

                    serial_number = hashlib.sha1(nqn.encode("UTF-8")).hexdigest()[:20]
                    rpc_args = {"nqn": nqn,
                                "listen_addresses": [],
                                "serial_number": serial_number,
                                "namespaces": [],  # type: dict
                                "allow_any_host": True, }  # Temporarily allow_any_host

                    for mac in transport_addresses:
                        rpc_args["listen_addresses"].append(transport_addresses[mac])
                        interface_speed = self.etcd_server_attributes["network"]["interfaces"][mac]["Speed"]
                        d["transport_addresses"] = {mac: {"interface_speed": interface_speed, }, }

                    numa_aligned = get_numa_alignment()

                    self.spdk_conf.add_subsystem_temp_local_config(config_dict)
                    results = execute_subsystem_rpc()
                    if "error" in results:
                        logger.info("==== construct_nvmf_subsystem [%s] failed ====", nqn)
                        self.subsystem_fail(command, subsystem_lock, nqn, namespaces)
                    else:
                        # construct_nvmf_subsystem success
                        logger.info("==== construct_nvmf_subsystem [%s] completed ====", nqn)
                        self.spdk_conf.rename_temp_local_config()
                        self.nqn_uuid_map[nqn] = str(uuid.uuid4())
                        d["UUID"] = self.nqn_uuid_map[nqn]
                        d["time_created"] = time.time()
                        if numa_aligned is not None:
                            d["numa_aligned"] = numa_aligned
                        self.backend.write_dict_to_etcd(d, self.SERVER_CONFIG_KEY_PREFIX + nqn + '/')
                        self.set_device_config_status(subsystem_lock, nqn, "success")
                        if target_op_flag:
                            logger.info('Restarting the monitoring threads '
                                        'because of target restart')
                            g_restart_monitor_threads += 1
                elif command == "delete_nvmf_subsystem":
                    self.remove_device_config(subsystem_lock, nqn)
                    self.spdk_conf.delete_subsystem_temp_local_config(config_dict)
                    rpc_args = {"nqn": nqn, }
                    results = execute_subsystem_rpc()
                    if "error" in results:
                        logger.info("==== delete_nvmf_subsystem [%s] failed ====", nqn)
                        self.subsystem_fail(command, subsystem_lock, nqn, namespaces)
                    else:
                        # delete_nvmf_subsystem success
                        logger.info("==== delete_nvmf_subsystem [%s] completed ====", nqn)
                        self.removed_nqn_q.append(nqn)
                        self.nqn_uuid_map.pop(nqn, None)
                        if constants.SPDK_RESET:
                            for ns in namespaces.values():
                                pci_addr = ns["PCIAddress"]
                                self.driver_setup.setup("nvme", pci_addr)
                        self.spdk_conf.rename_temp_local_config()
                        if target_op_flag:
                            logger.info('Restarting the monitoring threads '
                                        'because of target restart')
                            g_restart_monitor_threads += 1
                elif command == "store_nvmf_subsystem":
                    for mac in transport_addresses:
                        interface_speed = self.etcd_server_attributes["network"]["interfaces"][mac]["Speed"]
                        d["transport_addresses"] = {mac: {"interface_speed": interface_speed, }, }

                    numa_aligned = get_numa_alignment(build_rpc=False)
                    d["numa_aligned"] = numa_aligned
                    self.nqn_uuid_map[nqn] = str(uuid.uuid4())
                    d["UUID"] = self.nqn_uuid_map[nqn]
                    d["time_created"] = time.time()
                    self.backend.write_dict_to_etcd(d, self.SERVER_CONFIG_KEY_PREFIX + nqn + '/')
                    self.set_device_config_status(subsystem_lock, nqn, "success")
                    logger.info("==== store_nvmf_subsystem [%s] completed ====", nqn)
                elif command == "erase_nvmf_subsystem":
                    self.remove_device_config(subsystem_lock, nqn)
                    self.removed_nqn_q.append(nqn)
                    self.nqn_uuid_map.pop(nqn, None)
                    logger.info("==== erase_nvmf_subsystem [%s] completed ====", nqn)
                else:
                    logger.info("Unknown command [%s]. Exiting the system", command)
                    sys.exit(-1)

                if self.synchronize_etcd:
                    self.sync_count_dec()
                subsystem_lock.release()
                self.subsys_op_lock.release()
                del config_dict
            time.sleep(1)

    def poll_attributes(self, stopper_event, sec):
        """
        Calls class function pointers to store
        identity, cpu, network, and storage information
        in dictionary object, server_info. The
        dictionary is then written to etcdv3.
        Function is usually called in tandem with poll_function
        to poll every X seconds.
        :return:
        """
        while not stopper_event.is_set():
            self.subsys_op_lock.acquire()
            self.local_server_attributes = {"identity": self.identity_fn(),
                                            "cpu": self.cpu_fn(),
                                            "network": self.network_fn(),
                                            "storage": self.storage_fn()}
            self.subsys_op_lock.release()
            self.server_info_to_db()
            self.poll_event.wait(sec)
            self.poll_event.clear()

    def poll_subsystems(self, stopper_event, sec):
        subsystems_table = {}
        target_lease_obj = None
        lease_time = 2 * sec
        rpc_req = SPDKJSONRPC.build_payload("get_nvmf_subsystems")

        while not stopper_event.is_set():
            try:
                results = SPDKJSONRPC.call(rpc_req, self.jsonrpc_recv_size, self.default_spdk_rpc)
            except Exception:
                results = None

            while self.removed_nqn_q:
                remove_nqn = self.removed_nqn_q.popleft()
                logger.info("Removing '%s' from subsystem monitoring" % remove_nqn)
                subsystems_table.pop(remove_nqn, None)

            # Target Events
            if results and results.get("status", False):
                self.process_target_status(results["status"])

            if results and "error" not in results:
                subsystems = results["result"]
                for subsystem in subsystems:
                    if subsystem["subtype"] != "NVMe":
                        continue

                    nqn = subsystem["nqn"]
                    key = self.SERVER_CONFIG_KEY_PREFIX + nqn + '/status'

                    # Search for existing lease and if not in table then check etcd
                    if nqn not in subsystems_table:
                        try:
                            status, metadata = self.client.get(key)
                            if metadata:
                                subsystems_table[nqn] = self.client.lease(lease_time, metadata.lease_id)
                        except Exception:
                            logger.warning("Failed to read etcd lease for '%s'" % key)
                            pass

                    if nqn in subsystems_table and subsystems_table[nqn].remaining_ttl != -1:
                        # Lease is still alive so refresh its ttl
                        subsystems_table[nqn].refresh()
                    else:
                        # Create new lease and write key to etcd
                        subsystems_table[nqn] = self.client.lease(lease_time)
                        # g_target_op_lock = self.client.lock(key, ttl=self.default_etcd_ttl)
                        # if g_target_op_lock.acquire(None):
                        #    self.client.put(key, "up", subsystems_table[nqn])
                        #    g_target_op_lock.release()
                    self.backend.set_lock(self.SERVER_UUID)
                    self.backend.acquire_lock()
                    self.client.put(key, "up", subsystems_table[nqn])
                    self.backend.release_lock()

                    logger.info("Adding '%s' to subsystem monitoring" % nqn)

                target_status = "up"
            else:
                # Got error either because of target not running or errors
                if results:
                    logger.error("get_nvmf_subsystems rpc call failed with "
                                 "error %s", results)
                else:
                    logger.info('get_nvmf_subsystems rpc call returned None')
                target_status = "down"

            # Update the target status
            key = self.SERVER_BASE_KEY_PREFIX + 'target/status'
            try:
                if target_lease_obj and target_lease_obj.remaining_ttl > 0:
                    target_lease_obj.refresh()
                else:
                    target_lease_obj = self.client.lease(lease_time)
                # if g_target_op_lock and g_target_op_lock.acquire(None):
                #    self.client.put(key, target_status, target_lease_obj)
                #     g_target_op_lock.release()
                self.backend.set_lock(self.SERVER_UUID)
                self.backend.acquire_lock()
                self.client.put(key, target_status, target_lease_obj)
                self.backend.release_lock()
            except Exception:
                logger.exception('Exception in updating the target '
                                 'status to %s', target_status)
            stopper_event.wait(sec)

    def statistics_helper(self, items, prev_counters, output, client, non_cum_counters):
        for counter, value in items:
            counter_value = int(value)
            try:
                if non_cum_counters[counter]:
                    output[counter] = counter_value
                    continue
            except:
                pass

            if counter not in prev_counters:
                prev_counters[counter] = counter_value
            else:
                t = counter_value
                # TODO Handle client/session overflow
                # TODO Limitation: Send UINT64 max of each type of a key-value cmd only
                # TODO or it risks overflowing the counters and producing really funky values
                # TODO Distinguish between client/session getting reset or overflow occurring
                # TODO Need to communicate that a counters table is destroyed so we can do the same
                if not client and counter_value < prev_counters[counter]:
                    # Overflow case
                    counter_value += int(self.stats_obj.COUNTER_MAX - prev_counters[counter])
                else:
                    counter_value -= prev_counters[counter]
                    prev_counters[counter] = t
            if counter not in output:
                output[counter] = 0
            output[counter] += counter_value

    def subsystem_statistics_to_message(self, subsystem, timestamp,
                                        prev_counters, non_cum_counters,
                                        message_tuples):
        if 'id' not in subsystem or 'c_nqn' not in subsystem['id']:
            return

        subsystem_nqn = subsystem["id"]["c_nqn"]
        if subsystem_nqn in self.nqn_uuid_map:
            subsystem_uuid = self.nqn_uuid_map[subsystem_nqn]
        else:
            logger.error("Subsystem NQN -{} is not present in NQN UUID mapping - {} ".format(subsystem_nqn,
                                                                                            self.nqn_uuid_map))
            return

        nqn_name = subsystem_nqn.replace('.', '-')
        for element in subsystem:
            if element.startswith('drive'):
                try:
                    # TODO The serial number coming from ustat is the
                    # TODO device name in nmaepsace. The proper serial
                    # TODO number should be sent by the ustat command.
                    # TODO For now, just strip the last two characters which is
                    # TODO more of the form <dev_serial> + "n1" to just serial
                    serial = subsystem[element]["id"]["c_serial"][:-2]
                except Exception:
                    logger.exception('ID or Serial not found for drive %s',
                                     element)
                    continue
                if serial not in prev_counters:
                    prev_counters[serial] = {}

                if 'kvio' in subsystem[element]:
                    counters = {}
                    self.statistics_helper(subsystem[element]["kvio"].iteritems(),
                                           prev_counters[serial], counters, 0, non_cum_counters)
                    for counter in counters:
                        metric_path = "%s;cluster_id=%s;target_id=%s;nqn_id=%s;disk_id=%s;type=subsystem" % \
                                      (counter, g_cluster_name, self.SERVER_NAME, nqn_name, serial)
                        value = str(counters[counter])
                        if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                            continue
                        message_tuples.append((metric_path, (timestamp, value)))
            elif element.startswith('kvio'):
                # Gather subsystem level statistics
                if subsystem_uuid not in prev_counters:
                    prev_counters[subsystem_uuid] = {}

                counters = {}
                self.statistics_helper(subsystem[element].iteritems(),
                                       prev_counters[subsystem_uuid],
                                       counters,
                                       0, non_cum_counters)
                for counter in counters:
                    metric_path = "%s;cluster_id=%s;target_id=%s;nqn_id=%s;type=subsystem" % \
                                      (counter, g_cluster_name, self.SERVER_NAME, nqn_name)
                    value = str(counters[counter])
                    if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                        continue
                    message_tuples.append((metric_path, (timestamp, value)))
            elif element.startswith('ctrlr'):
                counters = subsystem[element]
                if 'c_initiator_ip' in counters:
                    host_id = counters.pop('c_initiator_ip')
                    for counter in counters:
                        metric_path = "%s;cluster_id=%s;target_id=%s;nqn_id=%s;host_id=%s;type=subsystem" % \
                                      (counter, g_cluster_name, self.SERVER_NAME,
                                       nqn_name, host_id)
                        value = str(counters[counter])
                        if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                            continue
                        message_tuples.append((metric_path, (timestamp, value)))
            else:
                counters = subsystem[element]
                for counter in counters:
                    metric_path = "subsystem.%s.%s;cluster_id=%s;target_id=%s;nqn_id=%s;type=subsystem" % \
                                  (element, counter, g_cluster_name, self.SERVER_NAME, nqn_name)
                    value = str(counters[counter])
                    if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                        continue
                    message_tuples.append((metric_path, (timestamp, value)))

    def session_statistics_to_message(self, timestamp, session_aggregate_counters, message_tuples):
        for session_ip in session_aggregate_counters:
            for counter in session_aggregate_counters[session_ip]:
                metric_path = "%s;cluster_id=%s;target_id=%s;client_ip=%s;type=session" % \
                              (counter, g_cluster_name, self.SERVER_NAME, session_ip)
                value = str(session_aggregate_counters[session_ip][counter])
                if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                    continue
                message_tuples.append((metric_path, (timestamp, value)))

    @staticmethod
    def remove_stale_prev_sessions(valid_sessions, prev_session_counters):
        # TODO Optimize
        sessions = prev_session_counters.keys()
        for session_ip in sessions:
            if session_ip not in valid_sessions:
                prev_session_counters.pop(session_ip, None)
                continue
            session_uuids = prev_session_counters[session_ip].keys()
            for session_uuid in session_uuids:
                if session_uuid not in valid_sessions[session_ip]:
                    prev_session_counters[session_ip].pop(session_uuid, None)

    @staticmethod
    def _convert_stats_dict_to_json_dict(stats_dict):
        json_dict = dict()
        for k, v in stats_dict.iteritems():
            keys = k.split('.')
            json_kv = json_dict
            for key in keys[:-1]:
                json_kv = json_kv.setdefault(key, {})
            json_kv[keys[-1]] = v
        return json_dict

    def poll_statistics(self, stopper_event, sec):
        prev_disk_counters = {}
        prev_session_counters = {}
        try:
            # Wrap ustat to read counters from nvmf_tgt
            # The subprocess.Popen does buffer the data because of
            # which the real time data not available
            # Also for huge data output, the Popen call fails or deadlock.
            # Modified ustat library to print empty line after the end of
            # each dump as a delimiter across multiple runs
            cmd = self.ustat_path + ' -p ' + self.nvmf_pid + ' ' + str(sec)
            proc = pexpect.spawn(cmd, timeout=sec+1)
        except Exception:
            logger.exception('Caught exception while running USTAT')
            return

        stats_output = {}
        while not proc.eof() and not stopper_event.is_set():
            line = proc.readline()
            line = line.strip()
            if line:
                try:
                    k, v = line.split('=')
                    '''
                    # Commented out as the ustat prints an empty line - used
                    # as a delimiter. Otherwise, we need to see if the value
                    # already presents and initialize to empty dict with a
                    # caveat that some values (new ones in this loop) may be
                    # lost
                    if k in stats_output:
                        stats_output = {}
                    '''
                    stats_output[k] = v
                except Exception:
                    logger.exception('Failed to handle line %s', line)
            else:
                # Emtpy line encountered for ustat output
                # Process the data and update stats in the DB
                if stats_output:
                    # Convert the flat key/value to json dict
                    stats_json_dict = self._convert_stats_dict_to_json_dict(stats_output)['target']
                    stats_output = {}
                    logger.debug('STATS JSON %s', stats_json_dict)

                    timestamp = time.time()

                    session_aggregate_counters = {}
                    valid_sessions = {}
                    tuples = []

                    nc_ctrs = stats_json_dict.pop('counters', {})
                    non_cumulative_counters = {}
                    for c in nc_ctrs:
                        nc_vals = nc_ctrs[c][1:-1].split(',')
                        for v in nc_vals:
                            non_cumulative_counters[v.strip()] = True

                    for key in stats_json_dict:
                        if key == 'subsystem0':
                            # Dummy subsystem for the initialization stats
                            # Ignore it
                            continue

                        if key.startswith('subsystem'):
                            subsystem = stats_json_dict[key]
                            self.subsystem_statistics_to_message(subsystem, timestamp,
                                                                 prev_disk_counters, non_cumulative_counters, tuples)
                        elif key.startswith('session'):
                            session = stats_json_dict[key]
                            if 'id' in session and 'ip' in session['id']:
                                session_ip = session["id"]["ip"]
                                session_uuid = session["id"]["nqn"]

                                # Track valid session IP + UUIDs
                                if session_ip not in valid_sessions:
                                    valid_sessions[session_ip] = {}
                                valid_sessions[session_ip][session_uuid] = None

                                if session_ip not in session_aggregate_counters:
                                    session_aggregate_counters[session_ip] = {}

                                if session_ip not in prev_session_counters:
                                    prev_session_counters[session_ip] = {}

                                if session_uuid not in prev_session_counters[session_ip]:
                                    prev_session_counters[session_ip][session_uuid] = {}

                                if 'kvio' in session:
                                    self.statistics_helper(session['kvio'].iteritems(),
                                                           prev_session_counters[session_ip][session_uuid],
                                                           session_aggregate_counters[session_ip], 1,
                                                           non_cumulative_counters)
                                for k in session:
                                    if k.startswith('ctrl') and (k[-1] != '0' or k[-2].isdigit()):
                                        metric_path = "cluster_id_%s.target_id_%s.%s.client_%s.qd_reqs" % \
                                                       (g_cluster_name, self.SERVER_NAME,
                                                        "client_id_%s" % session_ip, k)
                                        for k_req in session[k]:
                                            if k_req == 'reqs':
                                                value = str(session[k][k_req])
                                                tuples.append((metric_path, (timestamp, value)))

                    # Use aggregated client/session stats to create
                    # graphite messages
                    self.session_statistics_to_message(
                        timestamp, session_aggregate_counters, tuples)

                    # Remove unused client session UUIDs
                    self.remove_stale_prev_sessions(valid_sessions,
                                                    prev_session_counters)

                    if tuples:
                        self.stats_obj.submit_message(tuples)

        try:
            ret = proc.terminate()
            if ret:
                logger.info('ustat thread terminated successfully')
            else:
                logger.error('ustat thread termination failed')
        except Exception:
            logger.info('ustat process termination exception ', exc_info=True)
        logger.info('Poll statistics thread exited')

    def stop_etcd_watchers(self):
        while self.etcd_watcher_ids:
            watch_name, watch_id = self.etcd_watcher_ids.popitem()
            logger.info('Cancelling the watcher %s', watch_name)
            try:
                self.client.cancel_watch(watch_id)
            except Exception:
                logger.exception('Exception in cancel watcher %s', watch_name)

    def stats_config_watcher_callback(self, event):
        try:
            if event.value:
                mode, ip, port = event.value.split(':')
                port = int(port)
                if self.stats_mode != mode:
                    # Mode switching requires creating the stats_obj instance
                    # This requires a proper close of previous socket and flush
                    # data before starting the new instance. Not supported now
                    logger.error("Changed the stats mode from %s to %s. NOT "
                                 "SUPPORTED", self.stats_mode, mode)
                    return

                if self.stats_server != ip or self.stats_port != port:
                    logger.info('Received new stats server configuration '
                                'address - %s', event.value)
                    self.stats_server, self.stats_port = \
                        self.stats_obj.set_address(ip, port)
        except Exception:
            logger.exception('Exception in stats_config_watcher_callback fn')

    def stats_config_watcher(self):
        """
        Watch etcd DB for changes to carbon server ip and port
        :return:
        """
        carbon_key = self.SERVER_ATTR_KEY_PREFIX + "stats/address"
        if 'carbon_watcher' not in self.etcd_watcher_ids:
            try:
                watch_id = self.client.add_watch_callback(
                    carbon_key, self.stats_config_watcher_callback)
                self.etcd_watcher_ids['carbon_watcher'] = watch_id
                logger.info("Daemon is watching stats server configuration on "
                            "key %s", carbon_key)
            except Exception:
                logger.exception('stats_config_watcher failed with '
                                 'exception %s')
        else:
            logger.info('Daemon is already watching stats configuration on '
                        'key %s', carbon_key)

    def process_target_status(self, status):
        """
        Process target status.
        - Check if there is any cached status else store what it receive from Target
        - Compare new status with cached status and update cached status if there is any changes.
        :param status:
        :return:
        """
        logger.info("Processing target events ... ")
        # Check if there is any cached status.
        if self.cached_target_status:
            if self.target_event.compare_target_status(self.cached_target_status, status):
                self.cached_target_status = status
        else:
            # Store status if not there
            self.cached_target_status = status
            self.target_event.write_target_status(status)

    def initialize_target_events(self):
        """
        At the launch of agent create event cache and store in demon object memory memory
        - Fetch event data from ETCD database for corresponding machine
        - Cache data in agent side in dictionary format
        :return: None
        """
        logger.info("Initializing the events cache ... ")
        # Fetch data from ETCD
        events = self.target_event.read_target_status()
        self.cached_target_status = events


class ConfigWatcher(object):
    def __init__(self):
        endpoint = g_input_args['endpoint']
        port = g_input_args['port']
        self.watcher_id = None
        try:
            uuid = ServerAttr.OSMServerIdentity().server_identity_helper()[
                'UUID']
            self.backend = backend_layer.BackendLayer(endpoint, port)
        except Exception as e:
            logger.exception('Error in connecting to DB')
            raise e

        self.target_config_prefix = self.backend.ETCD_SRV_BASE + uuid + '/'

    def config_key_watcher_callback(self, event):
        try:
            if event.key.endswith(TARGET_COMMAND_KEY_SUFFIX) and event.value:
                logger.info('Received target command %s', event.value)
                if event.key.endswith('mode'):
                    if not event.value or (event.value != NODE_MODE_MAINTENANCE_STRING):
                        g_mode_event.set()
                    else:
                        g_mode_event.clear()
        except Exception:
            logger.exception('Exception in config_key_watcher_cb fn')

    def config_watcher(self, suffix):
        """
        Start watching this daemon's server configuration
        directory on etcdv3. Will continually wait on
        events_iterator for new events and add it into
        self.q for function server_config_poll to process.
        :return:
        """
        target_config_key = self.target_config_prefix + suffix
        if not self.watcher_id:
            try:
                watch_id = self.backend.client.add_watch_callback(
                    target_config_key,
                    self.config_key_watcher_callback,
                    range_end=increment_last_byte(to_bytes(target_config_key)))
                self.watcher_id = watch_id
                logger.info("Daemon is watching target config key %s",
                            target_config_key)
            except Exception:
                logger.error('target_config_watcher failed', exc_info=True)
        else:
            logger.info("Daemon is already watching target config key %s",
                        target_config_key)

        logger.info("Daemon is ready for new commands")
        logger.info("==== Watchers ready ====")

    def stop_watcher_id(self):
        if self.watcher_id is not None:
            logger.info('Cancelling the target watcher')
            try:
                self.backend.client.cancel_watch(self.watcher_id)
            except Exception:
                logger.exception('Exception in cancelling target watcher')


def create_pidfile(pid, pidfile):
    """
    Create a pid file to track what the
    current running pid is.
    :param pid: Process ID to write.
    :param pidfile: Path to pid file.
    :return:
    """
    try:
        fh = open(pidfile, 'wb')
    except Exception as e:
        logger.info(e)
        raise
    fh.write(pid)
    fh.close()


def remove_pidfile(pidfile):
    """
    Remove the pid file.
    :param pidfile: Path to pid file.
    :return:
    """
    try:
        os.unlink(pidfile)
    except Exception as e:
        logger.info(e)
        raise


def thread_monitor(threads):
    """
    Checks if any threads have sys.exited and kills the daemon.
    :param threads: Array of threads to monitor.
    :param nvmf_tgt_proc: NVMF target process structure from psutil.Process
    :return: Return 0 on no issue otherwise -1 for error.
    """
    for t_ele in threads:
        t_name, t_obj = t_ele
        if not t_obj.isAlive():
            logger.info("Thread [%s] sys.exited" % t_name)
            return t_obj
    return None


def get_disk_info_from_backend():
    devices = {}
    try:
        disk_list = ServerAttr.OSMServerStorage().get_storage_db_dict()
        key_dict = disk_list['nvme']['devices']
        for key in key_dict.keys():
            if key not in devices:
                devices[key] = {}
            devices[key]['PCIAddress'] = key_dict[key]['PCIAddress']
            devices[key]['Serial'] = key_dict[key]['Serial']
    except Exception:
        logger.exception('Exception in getting the disk list from the backend')
        pass

    return devices


def start_target_only(internal_flag=False):
    """
    Launch NVMF target along with monitoring threads
    If internal_flag is true, then the monitoring threads will not be
    started
    :return:
    """
    if os.geteuid() != 0:
        logger.info("You need to be root to perform this command.")
        sys.exit(-1)

    try:
        backend = backend_layer.BackendLayer()
        lock = backend.client.lock('target_op', ttl=60)
        while not lock.acquire():
            logger.info('Trying to acquire target operation lock')
            time.sleep(5)
    except Exception:
        logger.exception('Exception in creating target operation lock')
        return False

    logger.info('Starting target process')
    pid, status = check_spdk_running()
    if status:
        logger.info('NVMF target already running with pid %d', pid)
        lock.release()
        return 0

    settings = agent_conf.load_config(agent_conf.CONFIG_DIR +
                                      agent_conf.AGENT_CONF_NAME)["agent"]

    process_path = settings["nvmf_tgt"]
    # process_name = settings["nvmf_tgt"].split('/')[-1]
    nvmf_conf_file = settings["nvmf_conf_file"]
    nr_hugepages = int(settings["hugepages"])

    logger.info("nvmf_tgt path: " + process_path)
    logger.info("nvmf_conf:     " + nvmf_conf_file)
    logger.info("hugepages:     " + str(nr_hugepages))
    logger.info("==== Settings loaded ====")

    spdk_conf_obj = SPDKConfig(nvmf_conf_file)
    driver_setup_obj = DriverSetup()
    spdk_obj = spdk_setup.SPDKSetup(constants.DEFAULT_SPDK_SOCKET_PATH,
                                    process_path, nvmf_conf_file, nr_hugepages,
                                    (agent_conf.LOG_DIR +
                                     agent_conf.TARGET_LOG_NAME),
                                    spdk_conf_obj, driver_setup_obj)
    nvmf_pid = spdk_obj.pid
    if nvmf_pid <= 0:
        try:
            spdk_obj.setup_hugetlbfs()
        except Exception as e:
            logger.exception('Failed in setting the hugetlbfs')
            lock.release()
            raise e
        logger.info("==== HugeTLB FS setup complete ====")

        try:
            devices = get_disk_info_from_backend()
            spdk_obj.create_nvmf_conf()
            spdk_obj.namespace_setup(devices)
            _, core_count = ServerAttr.OSMServerCPU().get_cpu_cores()
            nvmf_pid = spdk_obj.launch_spdk_nvmf_tgt(core_count)
        except Exception as e:
            logger.exception('Exception in setting the NVMF target namespace')
            lock.release()
            raise e

        create_pidfile(str(nvmf_pid), spdk_obj.nvmf_tgt_pidfile)
        logger.info("==== Launching nvmf_tgt (pid:%d) ====" % nvmf_pid)
        while not os.path.exists(constants.DEFAULT_SPDK_SOCKET_PATH):
            logger.info("Waiting for SPDK socket to finish initialization")
            time.sleep(10)
        logger.info("==== Launched nvmf_tgt ====")
    else:
        logger.info("==== Existing nvmf_tgt (pid:%d) ====" % nvmf_pid)

    if not internal_flag:
        pid, status = check_agent_running()
        if status:
            logger.info('Sending SIGUSR1 to agent pid %d', pid)
            try:
                proc = psutil.Process(pid)
                proc.send_signal(signal.SIGUSR1)
            except Exception:
                logger.exception('Error in getting the process info for %s',
                                 pid)
                lock.release()
                return False

    lock.release()
    return True


def start_minio_stats_collector(osm_daemon_obj, metrics_blacklist_regex):
    minio_stats_obj = MinioStats(g_cluster_name, osm_daemon_obj.SERVER_NAME,
                                 statsdb_obj=osm_daemon_obj.stats_obj,
                                 metrics_blacklist_regex=metrics_blacklist_regex)
    minio_stats_obj.run_stats_collector()


def start_sflow_stats_collector(osm_daemon_obj, metrics_blacklist_regex):
    sflow_stats_obj = SFlowStatsCollector(g_cluster_name, osm_daemon_obj.SERVER_NAME,
                                          osm_daemon_obj.stats_obj, logger, metrics_blacklist_regex)
    sflow_stats_obj.run_stats_collector()


def start_mlnx_rdma_stats_collector(osm_daemon_obj, metrics_blacklist_regex):
    mlnx_rdma_stats_obj = MellanoxRDMAStats(g_cluster_name, osm_daemon_obj.SERVER_NAME,
                                            osm_daemon_obj.stats_obj, logger,
                                            metrics_blacklist_regex=metrics_blacklist_regex)
    mlnx_rdma_stats_obj.run_stats_collector()


def start_monitoring_threads(attribute_poll=60, monitor_poll=60,
                             performance_poll=1, stats=0,
                             endpoint="localhost", port=23790):
    """
    Starting target threads to check for server info updates in
    poll_serv_thread to etcdv3. Watch_db_thread Watches for configuration
    changes in etcdv3 then puts the work into a queue
    for processing by the process_config_thread
    :param attribute_poll: Server information polling frequency in seconds.
    :param monitor_poll: Subsystem up/down polling frequency in seconds.
    :param performance_poll: Performance statistics poll frequency in seconds.
    :param stats: Enable statistics polling and pushing.
    :param endpoint: IP of etcdv3 database.
    :param port: Port of etcdv3 database.
    :param tgt_start: Start the target or not. If True, the target will be
    started. Else, it checks if target is already started and starts
    corresponding monitoring threads
    :return:
    """
    global g_daemon_obj
    global g_nvmf_pid
    global g_thread_arr
    metrics_blacklist_re = None

    if 'attribute_poll' in g_input_args:
        attribute_poll = g_input_args['attribute_poll']
    if 'monitor_poll' in g_input_args:
        monitor_poll = g_input_args['monitor_poll']
    if 'performance_poll' in g_input_args:
        performance_poll = g_input_args['performance_poll']
    if 'stats' in g_input_args:
        stats = g_input_args['stats']
    if 'endpoint' in g_input_args:
        endpoint = g_input_args['endpoint']
    if 'port' in g_input_args:
        port = g_input_args['port']

    settings = agent_conf.load_config(agent_conf.CONFIG_DIR +
                                      agent_conf.AGENT_CONF_NAME)["agent"]

    if settings.get('metrics_blacklist', None):
        logger.info('Metrics blacklisted regex pattern %s', settings['metrics_blacklist'])
        metrics_blacklist_re = re.compile(settings['metrics_blacklist'])

    process_path = settings["nvmf_tgt"]
    # process_name = settings["nvmf_tgt"].split('/')[-1]
    nvmf_conf_file = settings["nvmf_conf_file"]
    nr_hugepages = int(settings["hugepages"])
    ustat_binary = settings["ustat_binary"]
    stats_mode = settings.get("stats_proto")
    stats_server = settings.get("stats_server") or "0.0.0.0"
    stats_port = int(settings.get("stats_port") or 2004)
    stats_poll = int(settings.get("stats_poll") or 10)

    if stats_mode not in ["graphite", "statsd"]:
        logger.error("Invalid stats server mode. should be one of "
                     "graphite/statsd")
        sys.exit(-1)
    elif stats_mode == "statsd":
        if performance_poll < stats_poll:
            performance_poll = stats_poll

    logger.info("ustat_binary:  " + ustat_binary)
    logger.info("==== Settings loaded ====")

    spdk_conf_obj = SPDKConfig(nvmf_conf_file)
    driver_setup_obj = DriverSetup()
    nvmf_pid, status = check_spdk_running()
    if not status:
        logger.info('NVMF target is not running. Skipping to start monitor '
                    'threads')
        return 0

    storage_aggregator_obj = ServerAttr.OSMServerStorage(
        constants.JSONRPC_RECV_PACKET_SIZE,
        constants.DEFAULT_SPDK_SOCKET_PATH)
    fn_ptrs = {
        'identity': ServerAttr.OSMServerIdentity().server_identity_helper,
        'cpu': ServerAttr.OSMServerCPU().cpu_helper,
        'network': ServerAttr.OSMServerNetwork().network_helper,
        'storage': storage_aggregator_obj.get_storage_db_dict,
    }
    ustat = {'ustat_binary_path': str(ustat_binary),
             'nvmf_pid': str(int(nvmf_pid)), }

    try:
        daemon_obj = OSMDaemon(endpoint, port, fn_ptrs, ustat,
                               check_spdk_running,
                               spdk_conf_obj, driver_setup_obj,
                               stats_mode, stats_server, stats_port,
                               metrics_blacklist_re)
    except Exception as e:
        logger.exception('Exception in initializing OSMDaemon object')
        raise e

    logger.info("Using attribute polling delay of: %d seconds" %
                attribute_poll)
    logger.info("Using monitor polling delay of: %d seconds" % monitor_poll)
    daemon_obj.initialize_target_events()

    '''
    # Create bdevs for all the NVMe devices moved into UIO
    # This is to get SMART information from the disks, moved into UIO PCI
    # generic driver, that are not part of subsystem
    try:
        db_handle = backend_layer.BackendLayer()
        devices = get_disk_info_from_backend(db_handle)
        spdk_namespaces = daemon_obj.get_namespaces()
        for entry in devices.values():
            pci_addr = entry["PCIAddress"]
            found = 0
            for ns in spdk_namespaces:
                try:
                    spdk_pci_addr = ns["driver_specific"]["nvme"]["trid"][
                        "traddr"]
                    if pci_addr == spdk_pci_addr:
                        found = 1
                        break
                except:
                    continue
            if found:
                continue
            else:
                logger.info('creating namespace for dev SN %s PCI %s',
                            entry['Serial'], entry['PCIAddress'])
                daemon_obj.create_namespaces(entry)
    except:
        logger.exception('Creating bdevs for all the NVMe devices failed')
    '''
    if stats:
        logger.info("Using performance polling delay of: %d seconds" %
                    performance_poll)

    thread_arr = []
    stats_thr_tuples = []
    if stats:
        # Create a watcher on carbon config in etcd to update the
        # carbon IP address and port details
        daemon_obj.stats_config_watcher()

        # (target_function, args, thread name)
        stats_thr_tuples = [
            (daemon_obj.stats_obj.push_statistics,
             [threading.Event(), performance_poll], 'push_stats_thread'),
            (daemon_obj.poll_statistics, [threading.Event(), performance_poll],
             'poll_stats_thread'),
            (daemon_obj.send_disk_network_utilization_to_graphite,
             [threading.Event(), 10], 'disk_util_thread')
        ]

    # (target_function, args, thread name)
    monitor_thr_tuples = [
        (daemon_obj.poll_subsystems,
         [threading.Event(), monitor_poll], 'subsys_monitor_thread'),
        (daemon_obj.server_config_poll, [threading.Event()],
         'process_config_thread'),
        (daemon_obj.poll_attributes, [threading.Event(), attribute_poll],
         'poll_serv_thread')
    ]

    thread_list = monitor_thr_tuples + stats_thr_tuples
    for element in thread_list:
        target_fn, thr_args, thr_name = element
        thread = threading.Thread(target=target_fn, args=thr_args)
        thread.daemon = True
        thread.stopper_event = thr_args[0]
        thread_arr.append((thr_name, thread))
        logger.info('Thread for %s created', thr_name)

    # Start all threads
    for thread in thread_arr:
        logger.info("'%s' starting" % thread[0])
        thread[1].start()
        logger.info("'%s' running" % thread[0])

    if stats:
        # Start MINIO Stats collection
        start_minio_stats_collector(daemon_obj, metrics_blacklist_re)
        # Start SFLOW metrics collection
        start_sflow_stats_collector(daemon_obj, metrics_blacklist_re)
        # Start Mellanox RDMA congestion stats collection
        start_mlnx_rdma_stats_collector(daemon_obj, metrics_blacklist_re)

    # Watches etcd for configuration changes pertaining to this server
    daemon_obj.subsystem_config_watcher()

    g_daemon_obj = daemon_obj
    g_nvmf_pid = nvmf_pid
    g_thread_arr = thread_arr
    return True


def stop_target_only(internal_flag=False):
    """
    Stop the NVMF target already running along with all monitoring threads.
    If internal_flag is true, then only the target is stopped. The
    monitoring threads will be alive
    :param internal_flag To indicate whether the target stop call came from
                         service or internally from agent
    :return status: True if stopped successfully
                    False if failed.
    """

    try:
        backend = backend_layer.BackendLayer()
        lock = backend.client.lock('target_op', ttl=30)
        while not lock.acquire():
            logger.info('Trying to acquire target operation lock')
            time.sleep(5)
    except Exception:
        logger.exception('Exception in creating target operation lock')
        return False

    nvmf_pid, status = check_spdk_running()
    if not status:
        # Process is already down. Nothing much to do
        logger.info('No NVMF target running. Returning back')
        lock.release()
        return True

    if not internal_flag:
        pid, status = check_agent_running()
        if status:
            logger.info('Sending SIGUSR2 to agent pid %d', pid)
            try:
                proc = psutil.Process(pid)
                proc.send_signal(signal.SIGUSR2)
                time.sleep(5)
            except Exception:
                logger.exception('Error in sending SIGUSR2 to agent pid %s', pid)

    try:
        proc = psutil.Process(nvmf_pid)
    except psutil.NoSuchProcess:
        logger.error('No nvmf_tgt process with the pid %s', nvmf_pid)
        lock.release()
        return True
    except Exception:
        logger.exception('Error in getting the process info for %s', nvmf_pid)
        lock.release()
        return False

    # Stop the process sending INT Signal
    logger.info('Sending SIGINT to the nvmf_tgt thread with PID %s', nvmf_pid)
    try:
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=120)
    except Exception:
        logger.exception('Received exception when sending signal to NVMF tgt')
    try:
        p_status = proc.status()
    except psutil.NoSuchProcess:
        logger.info('NVMF tgt successfully terminated')
        p_status = None
        status = True

    if p_status:
        logger.info('NVMF target %d still running with status (%s) even '
                    'after sending the SIGINT', nvmf_pid, p_status)
        # Stop the process sending KILL Signal
        logger.info('Sending SIGKILL to the nvmf_tgt thread with PID %s',
                    nvmf_pid)
        try:
            proc.send_signal(signal.SIGKILL)
            proc.wait(timeout=120)
        except Exception:
            logger.exception('Received exception when sending signal to '
                             'NVMF tgt')
        try:
            p_status = proc.status()
            if p_status:
                logger.info('NVMF target %d still running with status (%s) '
                            'even after sending the SIGKILL', nvmf_pid,
                            p_status)
        except psutil.NoSuchProcess:
            logger.info('NVMF tgt successfully terminated')
            status = True

    if status:
        # Clear the polling event for the next use
        g_poll_event.clear()

    lock.release()
    return status


def stop_monitoring_threads():
    """
    Stop the NVMF target related threads
    """
    global g_daemon_obj

    if g_daemon_obj:
        g_daemon_obj.stop_etcd_watchers()

    thread_arr = g_thread_arr[:]
    for thread in thread_arr:
        # Set the event for the thread to stop
        logger.info("'%s' stopping" % thread[0])
        thread[1].stopper_event.set()

    # Join all threads
    for thread in thread_arr:
        thread[1].join()
        logger.info("'%s' stopped" % thread[0])
        g_thread_arr.remove(thread)

    g_daemon_obj = None
    # Clear the polling event for the next use
    g_poll_event.clear()

    return True


def check_agent_running(endpoint='127.0.0.1', port=23790):
    daemon_pidfile = "kv_daemon.pid"

    proc_name = re.compile(r'^python|^%s' % CLI_NAME)
    cmds = [CLI_NAME, 'daemon', str(endpoint), str(port)]
    d_pid = pidfile_is_running(proc_name, cmds, daemon_pidfile)
    if d_pid:
        logger.info("Agent running with pid %d" % d_pid)
        return d_pid, True

    d_pid = find_process_pid(proc_name, cmds)
    if d_pid:
        logger.info("Agent running with pid %d" % d_pid)
        return d_pid, True

    return None, False


def agent_lease_refresh(lease):
    global g_agent_lease_timer
    try:
        lease.refresh()
        g_agent_lease_timer = threading.Timer(5.0, agent_lease_refresh,
                                              args=[lease])
        g_agent_lease_timer.start()
    except Exception:
        logger.exception('Exception in refreshing the agent status lease')


def wait_for_etcd_initialization():
    """
    Wait for the ETCD DB to be initialized in case the target node starts
    earlier than the CM nodes.
    :return: None
    """
    init_done = False
    global g_cluster_name
    global g_node_name
    g_node_name = platform.node().split('.', 1)[0]

    while not init_done:
        try:
            backend = backend_layer.BackendLayer()
            cluster_name = backend.client.get('/cluster/name')
            if cluster_name:
                logger.info('Cluster %s DB is accessible', cluster_name[0])
                g_cluster_name = cluster_name[0]
                init_done = True
        except Exception:
            logger.info('ETCD DB is not accessible. Retrying...')
            time.sleep(5)


def wait_for_agent_initialization():
    agent_init = False
    # Two minutes worth of loop
    max_loops = 24
    loop_count = 0

    while not agent_init and loop_count < max_loops:
        try:
            uuid = ServerAttr.OSMServerIdentity().server_identity_helper()['UUID']
            backend = backend_layer.BackendLayer()
            agent_status_key = backend.ETCD_SRV_BASE + uuid + '/agent/status'
            status = backend.client.get(agent_status_key)
            if status and (status[0] == 'initialized' or status[0] == 'up'):
                logger.info('Agent is initialized')
                agent_init = True
        except Exception:
            logger.exception('Error in connecting to DB. Exiting')

        time.sleep(5)
        loop_count += 1

    if not agent_init:
        logger.info('Agent not initialized')


def wait_for_hostname_populated():
    name_populated = False
    max_loops = 24  # Two minutes worth of loop
    loop_count = 0

    while not name_populated and loop_count < max_loops:
        try:
            uuid = ServerAttr.OSMServerIdentity().server_identity_helper()['UUID']
            backend = backend_layer.BackendLayer()
            hostname_key = backend.ETCD_SRV_BASE + uuid + '/server_attributes/identity/Hostname'
            name = backend.client.get(hostname_key)[0]
            if (name):
                logger.info('Hostname is populated: %s', name)
                name_populated = True

        except Exception:
            logger.exception('Error in connecting to DB. Exiting')

        time.sleep(5)
        loop_count += 1

    if not name_populated:
        logger.info('Hostname not populated yet')


def daemon(endpoint="localhost", port=23790):
    """
    Daemon to start the target and wait till the service is stopped.
    :param endpoint: IP of etcdv3 database.
    :param port: Port of etcdv3 database.
    :return:
    """
    global g_agent_lease_timer
    global g_restart_monitor_threads
    global g_daemon_obj

    if os.geteuid() != 0:
        logger.info("You need to be root to perform this command.")
        sys.exit(-1)

    # Set daemon priority (Higher nice is lower priority)
    p = psutil.Process()
    p.nice(10)

    _, status = check_agent_running(endpoint, port)
    if status:
        logger.info("Agent is already running.")
        sys.exit(0)

    try:
        collect_system_info()
    except:
        logger.exception('Error in collecting system info')

    try:
        s_uuid = ServerAttr.OSMServerIdentity().server_identity_helper()['UUID']
        backend = backend_layer.BackendLayer(endpoint, port)
        agent_status_key = backend.ETCD_SRV_BASE + s_uuid + '/agent/status'
    except Exception:
        logger.exception('Error in connecting to DB. Exiting')
        sys.exit(-1)

    # Class setup to listen for SIGUSR1 signals from UDEV Monitor
    udev_monitor = usr_signal.UDEVMonitorSignalHandler(
        g_poll_event, g_mode_event,
        start_threads_fn=start_monitoring_threads,
        stop_threads_fn=stop_monitoring_threads)

    # Begin listening for SIGINT for daemon clean-up
    udev_monitor.catch_SIGINT()

    try:
        server_mode_watcher = ConfigWatcher()
        server_mode_watcher.config_watcher('mode')
    except Exception:
        logger.exception('Exception in adding server mode watcher')
        sys.exit(-1)

    server_mode_key = backend.ETCD_SRV_BASE + s_uuid + '/mode'
    try:
        val = backend.client.get(server_mode_key)[0]
        if val and val == NODE_MODE_MAINTENANCE_STRING:
            logger.info('Node is in maintenance state. Waiting ...')
            try:
                backend.client.put(agent_status_key, 'maintenance')
            except Exception:
                logger.exception(
                    'Exception in updating the agent status to MAINTENANCE')
            while not g_mode_event.is_set():
                time.sleep(2)
            g_mode_event.clear()
            if udev_monitor.exit == 1:
                logger.info('Stopping the Agent')
                sys.exit(0)
    except Exception:
        logger.exception('Exception in getting the value for target mode')
        sys.exit(-1)

    # Update the target status
    try:
        backend.client.put(agent_status_key, 'starting')
    except Exception:
        logger.exception('Exception in updating the agent status to STARTING')

    daemon_pidfile = "kv_daemon.pid"
    if os.path.exists(daemon_pidfile):
        logger.error('Agent started after ungraceful restart')

    create_pidfile(str(os.getpid()), daemon_pidfile)

    # Begin listening for SIGUSR1 signals to trigger poll_attributes
    udev_monitor.catch_signal()

    pid, status = check_spdk_running()
    if status and not g_daemon_obj:
        status = start_monitoring_threads()

    # Update the target status to initialized
    # help to kick off poll_attributes
    try:
        backend.client.put(agent_status_key, 'initialized')
    except Exception:
        logger.exception('Exception in updating the agent status to INITIALIZED')

    logger.info('Agent initialized successfully')

    wait_for_hostname_populated()

    # Update the target status to up
    # To signal other modules like nkv_monitor
    try:
        agent_status_lease = backend.client.lease(10)
        backend.client.put(agent_status_key, 'up', agent_status_lease)
        g_agent_lease_timer = threading.Timer(5.0, agent_lease_refresh,
                                              args=[agent_status_lease])
        g_agent_lease_timer.start()
    except Exception:
        logger.exception('Exception in updating the agent status to up')

    logger.info('Agent started successfully')

    while True:
        if udev_monitor.exit == 1:
            break

        # Check if the node is set to maintenance mode
        # In maintenance mode, the g_mode_event flag is cleared
        if not g_mode_event.is_set():
            # Stop the target??
            pass

        if g_restart_monitor_threads > 0:
            logger.info('Stopping monitoring threads because of subsystem creation/deletion')
            status = stop_monitoring_threads()
            logger.info('Stop monitor threads with status %s', status)
            logger.info('Starting monitoring threads because of subsystem creation/deletion')
            status = start_monitoring_threads()
            logger.info('Start monitor threads with status %s', status)
            g_restart_monitor_threads -= 1

        thread_monitor(g_thread_arr)
        time.sleep(5)

    # Stop etcd watchers
    server_mode_watcher.stop_watcher_id()

    status = stop_monitoring_threads()
    logger.info('Stopped monitoring threads with status %s', status)

    remove_pidfile(daemon_pidfile)

    try:
        backend.client.put(agent_status_key, 'down')
    except Exception:
        logger.exception('Exception in updating the agent status to DOWN')

    logger.info('Agent stopped succesfully')


def main(meta, args):
    """
    Entry point for daemon.py.
    :param meta: CLI metadata
    :param args: Copy of arguments received by caller.
    :return:
    """
    global CLI_NAME
    CLI_NAME = meta["__CLI_NAME__"]
    global CLI_VERSION
    CLI_VERSION = meta["__CLI_VERSION__"]
    global g_input_args

    # Logger initialization
    # Create if a default configuration file if it does not exists
    agent_conf.create_config(agent_conf.CONFIG_DIR +
                             agent_conf.AGENT_CONF_NAME)

    if int(args["--attribute_poll"]) < 0:
        logger.info("Invalid attribute polling delay of %d seconds" %
                    int(args["--attribute_poll"]))
        sys.exit(-1)

    if int(args["--monitor_poll"]) < 0:
        logger.info("Invalid monitor polling delay of %d seconds" %
                    int(args["--monitor_poll"]))
        sys.exit(-1)

    if args["--stats"]:
        if int(args["--performance_poll"]) < 0:
            logger.info("Invalid performance polling delay of %d seconds" %
                        int(args["performance_poll"]))
            sys.exit(-1)
        logger.info("Performance statistics collection has been enabled")
        stats = 1
    else:
        logger.info("Performance statistics collection has been disabled")
        stats = 0

    attribute_poll = int(args["--attribute_poll"])
    monitor_poll = int(args["--monitor_poll"])
    performance_poll = int(args["--performance_poll"])
    g_input_args['attribute_poll'] = attribute_poll
    g_input_args['monitor_poll'] = monitor_poll
    g_input_args['performance_poll'] = performance_poll

    wait_for_etcd_initialization()

    if args["daemon"]:
        if args['--start_tgt']:
            wait_for_agent_initialization()
            if args['internal_flag']:
                logger.info('Starting target with internal flag')
                start_target_only(True)
            else:
                start_target_only()
        elif args['--stop_tgt']:
            wait_for_agent_initialization()
            if args['internal_flag']:
                logger.info('Stopping target with internal flag')
                stop_target_only(True)
            else:
                stop_target_only()
        elif args["<endpoint>"] is None and args["<port>"] is None:
            daemon()
        else:
            g_input_args['stats'] = stats
            g_input_args['endpoint'] = args['<endpoint>']
            g_input_args['port'] = args['<port>']
            daemon(args["<endpoint>"], args["<port>"])
