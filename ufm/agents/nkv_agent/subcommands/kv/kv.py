"""
Usage:
kv-cli.py kv list <endpoint> <port> [--server=<uuid>]
kv-cli.py kv add <endpoint> <port> (--server=<uuid>) (--devices=<devnode>)
                                   (--nqn=<nqn>) (--ip=<ip>) (--trtype=<trtype>)
                                   [--core=<core_id>] [--async]
kv-cli.py kv remove <endpoint> <port> (--server=<uuid>) (--nqn=<nqn>) [--async]
kv-cli.py kv stats <endpoint> <port> [--server=<uuid>] [--ip=<ip>]

Options:
  -h --help            Show this screen.
  --version            Show version.
  --ip_addresses=<ip>  Comma separated list of "<IP>:<port>" addresses.
"""
from __future__ import print_function

import re
import sys

import utils.backend_layer
import utils.key_prefix_constants as key_cons
import utils.validate_kv as validate_kv
from utils.utils import KVLog as KVL
from utils.utils import time_delta, valid_ip

SPDK_NVMF_NQN_MAX_LEN = 223
SPDK_DOMAIN_LABEL_MAX_LEN = 63


class KVManager:
    def __init__(self, endpoint, port):
        try:
            self.backend = utils.backend_layer.BackendLayer(endpoint, port)
            self.client = self.backend.client
        except Exception as e:
            raise e

    @staticmethod
    def display_remote_server_uuid(servers):
        list_servers = servers["list"]
        for uuid in list_servers:
            print("Hostname: '%s', UUID: '%s'" %
                  (servers[uuid][key_cons.SERVER_ATTRIBUTES_NAME]["identity"]["Hostname"],
                   servers[uuid][key_cons.SERVER_ATTRIBUTES_NAME]["identity"]["UUID"]))

    @staticmethod
    def display_remote_server_info(server_uuid, servers):
        """
        Takes dictionary object, servers, and
        displays it in neat, readable format.
        :param server_uuid: UUID to match and display information.
        :param servers: Dictionary object of server information.
        :return:
        """
        found = 0
        if "list" in servers:
            servers_list = servers["list"]
            for uuid in servers_list:
                server = servers[uuid]
                server_attributes = server[key_cons.SERVER_ATTRIBUTES_NAME]
                if key_cons.KV_ATTRIBUTES_NAME in server:
                    kv_attributes = server[key_cons.KV_ATTRIBUTES_NAME]
                else:
                    kv_attributes = None
                if 'target' in server:
                    target_attributes = server['target']
                else:
                    target_attributes = None
                identity = server_attributes["identity"]
                if server_uuid == "*" or \
                   server_uuid == identity["UUID"]:
                    found = 1
                else:
                    continue

                print("==============================")
                print("Hostname:        %s" % identity["Hostname"])
                print("UUID:            %s" % identity["UUID"])
                if target_attributes:
                    if 'status' not in target_attributes:
                        print("SPDK:            No status")
                    elif target_attributes["status"] == "down":
                        KVL.kvprint(KVL.ERROR, "SPDK:            %s" %
                                    target_attributes["status"])
                    else:
                        print("SPDK:            %s" %
                              target_attributes["status"])
                print("Uptime (Hrs):    %s" % server_attributes["uptime"])
                elapsed = time_delta(servers_list[uuid])
                if elapsed < 0:
                    KVL.kvprint(KVL.WARN, "Heartbeat:       %d seconds ago" % elapsed)
                    KVL.kvprint(KVL.WARN, "Target Server time not synchronized")
                elif elapsed > 120:
                    KVL.kvprint(KVL.WARN, "Heartbeat:       %d seconds ago" % elapsed)
                    KVL.kvprint(KVL.WARN, "(>2 minutes since last heartbeat)")
                else:
                    print("Heartbeat:       %d seconds ago" % elapsed)
                print("")

                print("CPU Information:")
                cpu = server_attributes["cpu"]
                for cpu_attr in sorted(cpu.keys()):
                    if cpu_attr != 'NUMAMap' and cpu_attr != 'NUMACount':
                        print("  %-15s %s" % (cpu_attr + ':', cpu[cpu_attr]))

                # Group and print NUMA information near tail-end of CPU info section
                if int(cpu["NUMACount"]) > 0:
                    print("  %-15s %s" % ("NUMACount:", cpu["NUMACount"]))
                    for group in range(int(cpu["NUMACount"])):
                        print("  NUMA %-10s %s" % (str(group) + ':', cpu["NUMAMap"][str(group)]))
                else:
                    KVL.kvprint(KVL.WARN, "  %-15s" % "NUMA not detected")
                print("")

                print("Network Interfaces:")
                if "interfaces" in server_attributes["network"]:
                    interfaces = server_attributes["network"]["interfaces"]
                    for interface in interfaces.values():
                        for key in sorted(interface.keys()):
                            if key == "CRC":
                                continue
                            print("  %-15s %s" % (key + ':', interface[key]))
                        print("")
                else:
                    print("  None")
                    print("")

                print("NVMe Devices:")
                sn_reverse_map = {}
                # free_disk_serial_list is used to collect all the serial
                # numbers that are not part of subsystema and can be used by
                #  subsystem_add.py for consumption to create subsystem
                free_disk_serial_list = []
                if int(server_attributes["storage"]["nvme"]["Count"]) > 0:
                    nvme_devices = server_attributes["storage"]["nvme"]["devices"]
                    convert = lambda text: int(text) if text.isdigit() else text.lower()
                    alphanum_key = lambda key: [convert(c) for c in re.split('([0-9]+)', key)]
                    devnode_lambda = lambda value: (alphanum_key(nvme_devices[value]['PCIAddress']))
                    subsystem_list = []
                    if kv_attributes:
                        kv_subsystems = kv_attributes["config"]["subsystems"]
                        subsystem_list = kv_subsystems.values()
                    for serial in sorted(nvme_devices, key=devnode_lambda):
                        device = nvme_devices[serial]
                        sn_reverse_map[device["Serial"]] = device
                        nqn_name = None
                        for subsystem in subsystem_list:
                            for ns in subsystem["namespaces"].values():
                                if ns['Serial'] == serial:
                                    nqn_name = subsystem['NQN']
                        if "DeviceNode" in device:
                            if nqn_name:
                                print("  Device Node:    %s (%s)" % (device["DeviceNode"], nqn_name))
                            else:
                                print("  Device Node:    %s" % device["DeviceNode"])
                        print("  NUMA Node:      %d" % (int(device["NUMANode"])))
                        print("  PCI Address:    %s" % device["PCIAddress"])
                        if device["Model"] != "":
                            print("  Model:          %s" % device["Model"])
                        print("  Serial:          %s" % device["Serial"])
                        print("  Size (MiB):     %d" % (int(device["SizeInBytes"]) / (1024*1024)))
                        print("")

                        if not nqn_name:
                            free_disk_serial_list.append(device['Serial'])
                else:
                    print("  None")
                    print("")

                if free_disk_serial_list:
                    print("Free Disks: %s" % (",".join(free_disk_serial_list)))
                    print("")

                if kv_attributes:
                    print("KV Devices:")
                    kv_subsystems = kv_attributes["config"]["subsystems"]
                    for config in kv_subsystems.values():
                        print("  Command:        %s" % config["Command"])
                        print("  Command Status: %s" % config["cmd_status"])
                        if "status" in config:
                            print("  Status:         up")
                        else:
                            print("  Status:         down")
                        print("  NQN:            %s" % config["NQN"])

                        # UUID Available on successful subsystem creation
                        if "UUID" in config:
                            print("  UUID:           %s" % config["UUID"])

                        for idx, address in enumerate(config["transport_addresses"].values()):
                            print("  Listen %d:       %s - %s:%s (%s)" %
                                  (idx, address["trtype"], address["traddr"], address["trsvcid"], address["adrfam"]))

                        for ns in config["namespaces"].values():
                            serial = ns["Serial"]
                            nsid = ns["nsid"]
                            if serial in sn_reverse_map:
                                device = sn_reverse_map[serial]
                                print("  Namespace ID %d" % int(nsid))
                                print("    Model:          %s" % device["Model"])
                                print("    PCI Address:    %s" % device["PCIAddress"])
                                print("    Serial:          %s" % device["Serial"])
                                print("    Size (MiB):     %s" % (int(device["SizeInBytes"]) / (1024*1024)))
                            else:
                                continue
                        print("")

        if found == 0:
            KVL.kvprint(KVL.ERROR, "Unable to find '%s'" % server_uuid)

    def create_subsystem(self, server_uuid, devices, nqn, ip_addresses, asynchronous, tr_type, core_id=None):
        """
        Add new subsystem to SPDK with the given parameters. The
        subsystem configuration arguments are written to etcdv3.
        :param server_uuid:
        :param devices: Device node to use in the subsystem.
        :param nqn: NVMe Qualified Name to use with the subsystem.
        :param ip_addresses: Transport addresses list.
        :param asynchronous: Asynchronous code path instead of waiting for response.
        :param core_id: Core to run the subsystem on.
        :return:
        """
        search_prefix = self.backend.ETCD_SRV_BASE + server_uuid
        self.backend.acquire_lock()
        resp = self.backend.get_json_prefix(search_prefix).values()[0]
        if resp == {}:
            KVL.kvprint(KVL.WARN, "No servers detected, is daemon running?")
            sys.exit(-1)
        else:
            server = resp

        if server is not None:
            server_attributes = server[key_cons.SERVER_ATTRIBUTES_NAME]
            if key_cons.KV_ATTRIBUTES_NAME not in server:
                server[key_cons.KV_ATTRIBUTES_NAME] = {"config": {"subsystems": {}, }, }
            kv_attributes = server[key_cons.KV_ATTRIBUTES_NAME]
            kv_subsystems = kv_attributes["config"]["subsystems"]

            # Validate core id
            if core_id is not None:
                cpu = server_attributes["cpu"]
                cpu_logical_count = int(cpu["LogicalCount"])
                if 0 <= int(core_id) < cpu_logical_count:
                    print("Core argument is within valid range!")
                else:
                    print("Invalid core, not within valid range")
                    sys.exit(-1)

            # Check if device is valid to be added to SPDK
            if "devices" not in server_attributes["storage"]["nvme"]:
                print("No NVMe devices available")
                sys.exit(-1)

            nvme_devices = server_attributes["storage"]["nvme"]["devices"]
            dev_found = []
            dev = None
            for dev in devices:
                t = validate_kv.find_storage_device(dev, nvme_devices)
                if t:
                    dev_found.append(t)
                else:
                    break
            if len(devices) == len(dev_found):
                for dev in dev_found:
                    print("Found storage device '%s'" % dev["Serial"])
            else:
                if dev:
                    print("Could not find %s" % dev)
                else:
                    print("Unable to find the device for new subsystem")
                sys.exit(-1)

            # Check for valid traddr (Listens for incoming NVMe commands)
            transport_addresses = []
            for ip_port in ip_addresses:
                ip, port = ip_port.split(':')
                interfaces = server_attributes["network"]["interfaces"]
                adrfam, interface_found = validate_kv.find_network_interface(ip, interfaces)
                if interface_found is not None:
                    print("Found network interface for '%s'" % ip)
                else:
                    print("Could not find %s" % ip)
                    sys.exit(-1)

                # TODO: Remove if not needed
                # Check for valid trtype
                """
                if validate_kv.valid_trtype(trtype):
                    print("Valid transfer type requested - %s" % trtype)
                else:
                    print("Invalid transfer type requested - %s" % trtype)
                    sys.exit(-1)
                """

                # Check for valid trsvcid
                if validate_kv.valid_port(port):
                    print("Valid port requested - %s" % port)
                else:
                    KVL.kvprint(KVL.ERROR, "Invalid port requested - %s" % port)
                    sys.exit(-1)

                address = {interface_found["MACAddress"]: {"trtype": tr_type,
                                                           "traddr": ip,
                                                           "trsvcid": port,
                                                           "adrfam": adrfam, }, }
                transport_addresses.append(address)

            namespaces = []
            for dev in dev_found:
                namespaces.append((dev["Serial"], dev["PCIAddress"]))

            # Check for valid nqn rules
            # Validate if nqn is already used
            if len(nqn) > SPDK_NVMF_NQN_MAX_LEN:
                KVL.kvprint(KVL.ERROR, "NQN max length 223 bytes")
                sys.exit(-1)

            m = re.search(r"^nqn\.\d{4}-\d{2}\.", nqn)
            if m is None:
                KVL.kvprint(KVL.ERROR, "NQN must start with 'nqn.<yyyy>-<mm>.'")
                sys.exit(-1)

            m = re.search(r"^nqn\.\d{4}-\d{2}\.(.+?):(.+?)$", nqn)
            if m is None:
                KVL.kvprint(KVL.ERROR, "NQN requires nqn.<yyyy>-<mm>.<reverse-domain>:<user-string>")
                sys.exit(-1)

            domain_label = m.group(1)
            domain_label_length = len(domain_label)
            if not domain_label[0].isalpha():
                KVL.kvprint(KVL.ERROR, "Domain label must start with letter.")
                sys.exit(-1)

            if not domain_label[-1].isalnum():
                KVL.kvprint(KVL.ERROR, "Domain label must end with letter or number.")
                sys.exit(-1)

            domain_m = re.search(r"^[a-zA-Z][a-zA-Z0-9]+?[a-zA-Z0-9]$", m.group(1))
            if not domain_m:
                KVL.kvprint(KVL.ERROR, "Domain label may only use letters, numbers, and hyphens.")
                sys.exit(-1)

            if domain_label_length > SPDK_DOMAIN_LABEL_MAX_LEN:
                KVL.kvprint(KVL.ERROR, "Domain label length is too long (>%d characters)" % SPDK_DOMAIN_LABEL_MAX_LEN)
                sys.exit(-1)

            if "config" in kv_attributes:
                if nqn in kv_subsystems:
                    KVL.kvprint(KVL.ERROR, "NQN(%s) is already in-use" % nqn)
                    sys.exit(-1)

                # Validate if storage device is already used
                for obj_dev in kv_subsystems.values():
                    for ns in namespaces:
                        if ns[0] in obj_dev["namespaces"]:
                            KVL.kvprint(KVL.ERROR, "Device S/N (%s) is already in-use" % str(ns[0]))
                            sys.exit(-1)

            if core_id is not None:
                core_numa = validate_kv.find_numa_for_core(core_id, cpu["NUMAMap"])
                # Warn user of improperly configured core NUMA for the provided network interface
                # May encounter reduced performance
                interface_numa = int(interface_found["NUMANode"])
                if core_numa != interface_numa:
                    KVL.kvprint(KVL.WARN, "Warning - Mismatched NUMA between core and network interface")
                    print("Core (NUMA %d) and network interface (%s)\n" %
                          (int(core_numa),
                           "No NUMA" if interface_numa == -1 else "NUMA " + str(interface_numa)))

                # Warn user of improperly configured core NUMA for the storage device
                # May encounter reduced performance
                for dev in dev_found:
                    device_numa = int(dev["NUMANode"])
                    if core_numa != device_numa:
                        KVL.kvprint(KVL.WARN, "Warning - Mismatched NUMA between core and storage device")
                        print("Core (NUMA %d) and storage device (%s)\n" %
                              (int(core_numa),
                               "No NUMA" if device_numa == -1 else "NUMA " + str(device_numa)))

            config_args = {"Command": "construct_nvmf_subsystem",
                           "NQN": nqn,
                           "namespaces": {},
                           "transport_addresses": {},
                           "adrfam": adrfam,
                           "cmd_status": "pending", }

            for address in transport_addresses:
                mac = address.keys()[0]
                config_args["transport_addresses"][mac] = address.values()

            for nsid, sn_pci in enumerate(namespaces):
                nsid += 1
                nsid = str(nsid)
                sn, pci = sn_pci
                config_args["namespaces"][nsid] = {}
                config_args["namespaces"][nsid]["PCIAddress"] = pci
                config_args["namespaces"][nsid]["Serial"] = sn
                config_args["namespaces"][nsid]["nsid"] = nsid

            event_queue, watch_id = self.backend.write_spdk_command(server_uuid, nqn, config_args)
            self.backend.release_lock()
            event, resp_msg = self.backend.event_response(event_queue, watch_id)
            if event is None:
                KVL.kvprint(KVL.ERROR, resp_msg)
                sys.exit(-1)

            if asynchronous == 1:
                self.client.cancel_watch(watch_id)
                KVL.kvprint(KVL.SUCCESS, "Add subsystem command written")
            else:
                # TODO: Rework and clean it up
                if str(event.__class__) == str(utils.backend_layer.etcd3.events.PutEvent) \
                        and str(event.value) == "success":
                    KVL.kvprint(KVL.SUCCESS, "Successfully configured device")
                elif str(event.__class__) == str(utils.backend_layer.etcd3.events.DeleteEvent) \
                        and str(event.value) == "":
                    KVL.kvprint(KVL.ERROR, "Failed to configure device")
        else:
            self.backend.release_lock()
            KVL.kvprint(KVL.ERROR, "Unknown server %s" % server_uuid)

    def delete_subsystem(self, server_uuid, nqn, asynchronous):
        """
        Remove subsystem with provided NQN from etcdv3 and
        local configuration file.
        :param server_uuid: Server UUID to perform removal.
        :param nqn: NQN to identify the subsystem on the server.
        :param asynchronous: Asynchronous code path instead of waiting for response.
        :return:
        """
        search_prefix = self.backend.ETCD_SRV_BASE + server_uuid
        self.backend.acquire_lock()
        resp = self.backend.get_json_prefix(search_prefix).values()
        if resp == {}:
            KVL.kvprint(KVL.WARN, "No servers detected, is daemon running?")
            sys.exit(-1)
        else:
            server = resp[0]

        if server == {}:
            self.backend.release_lock()
            KVL.kvprint(KVL.ERROR, "Failed to retrieve JSON data from server")
            sys.exit(-1)

        if key_cons.KV_ATTRIBUTES_NAME not in server:
            server[key_cons.KV_ATTRIBUTES_NAME] = {"config": {"subsystems": {}, }, }
        kv_attributes = server[key_cons.KV_ATTRIBUTES_NAME]
        kv_subsystems = kv_attributes["config"]["subsystems"]
        print("Remove %s from %s" % (nqn, server_uuid))
        if "config" in kv_attributes and nqn in kv_subsystems:
            config_cmd = kv_subsystems[nqn]["Command"]
            config_status = kv_subsystems[nqn]["cmd_status"]

            if config_cmd == "construct_nvmf_subsystem":
                if config_status == "pending" or config_status == "processing":
                    config_prefix = self.backend.ETCD_SRV_BASE + server_uuid + "/kv_attributes/config/subsystems/" + nqn
                    if self.backend.etcd_target_lock.is_acquired():
                        self.client.delete_prefix(config_prefix)
                        print("Removed key prefixed with %s" % config_prefix)
                        KVL.kvprint(KVL.SUCCESS, "Successfully removed device")
                        self.backend.release_lock()
                        return
                    else:
                        print("Requires server prefix to be locked to delete NQN config")
                        sys.exit(-1)
            elif config_cmd == "delete_nvmf_subsystem":
                self.backend.release_lock()
                print("Already pending removal by server")
                return
            else:
                KVL.kvprint(KVL.ERROR, "Unknown command")
                sys.exit(-1)
            config_args = {"Command": "delete_nvmf_subsystem",
                           "cmd_status": "pending", }
            event_queue, watch_id = self.backend.write_spdk_command(server_uuid, nqn, config_args)
            self.backend.release_lock()
            if event_queue is None or watch_id is None:
                KVL.kvprint(KVL.ERROR, "Failed to write requested command")
                sys.exit(-1)

            if asynchronous == 1:
                self.client.cancel_watch(watch_id)
                KVL.kvprint(KVL.SUCCESS, "Remove subsystem command written")
            else:
                event, resp_msg = self.backend.event_response(event_queue, watch_id)
                if event is None:
                    KVL.kvprint(KVL.ERROR, resp_msg)
                    sys.exit(-1)

                # TBD: Rework and clean it up
                if str(event.__class__) == str(utils.backend_layer.etcd3.events.PutEvent) \
                        and str(event.value) == "failed":
                    KVL.kvprint(KVL.ERROR, "Failed to remove device")
                elif str(event.__class__) == str(utils.backend_layer.etcd3.events.DeleteEvent) \
                        and str(event.value) == "":
                    KVL.kvprint(KVL.SUCCESS, "Successfully removed device")
        else:
            KVL.kvprint(KVL.ERROR, "Unknown NQN \"%s\" for server %s" % (nqn, server_uuid))

    def set_carbon_address(self, server_uuid, ip_address):
        ip, port = ip_address.split(':')
        if valid_ip(ip) and validate_kv.valid_port(port):
            key = self.backend.ETCD_SRV_BASE + server_uuid + "/server_attributes/"
            d = {"carbon": {"address": "%s:%s" % (ip, port)}}
            self.backend.write_dict_to_etcd(d, key)
        else:
            KVL.kvprint(KVL.ERROR, "Invalid IP Address '%s:%s'" % (ip, port))


def main(args):
    """
    Entry point for kv.py.
    :param args: Copy of arguments received by caller.
    :return:
    """
    if args["kv"]:
        kv_manager = KVManager(args["<endpoint>"], args["<port>"])
        if args["list"]:
            servers = kv_manager.backend.get_json_prefix(kv_manager.backend.ETCD_SRV_BASE)
            if servers == {}:
                KVL.kvprint(KVL.WARN, "No servers detected, is daemon running?")
                sys.exit(-1)

            if args["--server"] is not None:
                kv_manager.display_remote_server_info(args["--server"], servers)
            else:
                kv_manager.display_remote_server_uuid(servers)
        elif args["add"]:
            kv_manager.backend.set_lock(args["--server"])
            ip_addresses = list(set(args["--ip"].split(',')))
            devices = list(set(args["--devices"].split(',')))

            if validate_kv.exceeded_maximum_listen_addresses(ip_addresses):
                print("Maximum of %d listen addresses allowed" % validate_kv.MAXIMUM_LISTEN_DIRECTIVES)
                sys.exit(-1)
            elif validate_kv.exceeded_maximum_namespaces(devices):
                print("Maximum of %d namespaces allowed" % validate_kv.MAXIMUM_NAMESPACE_DIRECTIVES)
                sys.exit(-1)

            kv_manager.create_subsystem(args["--server"], devices, args["--nqn"], ip_addresses,
                                        args["--async"], args["--trtype"], args["--core"])
        elif args["remove"]:
            kv_manager.backend.set_lock(args["--server"])
            kv_manager.delete_subsystem(args["--server"], args["--nqn"], args["--async"])
        elif args["stats"]:
            kv_manager.set_carbon_address(args["--server"], list(set(args["--ip"].split(',')))[0])
        else:
            sys.exit(-1)
