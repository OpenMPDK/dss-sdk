import ast
import json
import os
import signal
import sys
import threading
import time
import socket
import time
# import pprint
from datetime import datetime
from datetime import timedelta

from subprocess import PIPE, Popen, STDOUT
import zlib

import requests
from netifaces import interfaces, ifaddresses, AF_INET

from common.clusterlib import lib, lib_constants
import common.events.events_def_en as events_def
from common.events.events import save_events_to_etcd_db, ETCD_EVENT_KEY_PREFIX
from common.events.events import ETCD_EVENT_TO_PROCESS_KEY_PREFIX
from common.events.event_notification import EventNotification

from common.system.monitor import Monitor


g_hostname = None
g_etcd = None
g_clustername = None
leader_change = 0
old_leader_name = None

g_update = False
g_servers_out = None

# A map of UUID to the node name
g_node_uuid_map = {}

POLL_DISK_SPACE_INTERVAL = 120
UPDATE_NODE_INFO_INTERVAL = 60

LEASE_INTERVAL = (2 * UPDATE_NODE_INFO_INTERVAL)
AGENT_HEARTBEAT_INTERVAL = 60
ZMQ_BROKER_PORT = 6000

g_event_queue = None
g_event_queue_tevent = threading.Event()

g_event_notifier_fn = None

# TODO(ER) - This queue should be moved into a class
def initialize_worker_queue(logger=None):
    global g_event_queue
    g_event_queue_tevent.clear()
    g_event_queue = {}

    logger.debug('Initialize worker queue')
    try:
        # pull all the events listed under /cluster/events_to_process into the dictionary
        global g_etcd
        res = g_etcd.get_key_with_prefix(ETCD_EVENT_TO_PROCESS_KEY_PREFIX, raw=True)
        if res:
            logger.debug('Dictonary is empty')
    except:
        logger.exception('Exception in getting events to process')
        return

    if res:
        for k, v in res.items():
            g_event_queue[k] = v

    g_event_queue_tevent.set()


def destroy_worker_queue(logger=None):
    logger.debug('destroy worker queue')

    global g_event_queue
    g_event_queue = {}
    g_event_queue_tevent.set()


def add_event_to_worker_queue(key, value):
    global g_event_queue
    if not g_event_queue:
        # For now dict is good enough to handle. Add queue structure
        # depending on needs in future
        g_event_queue = {}

    g_event_queue[key] = value
    g_event_queue_tevent.set()


def remove_event_from_worker_queue(key):
    global g_event_queue
    try:
        g_event_queue.pop(key)
    except:
        pass


def process_events_from_worker_queue(stopper_event, logger=None, sleep_interval=30):
    global g_event_queue
    global g_event_notifier_fn

    logger.info("======> starting process events from worker queue <=========")

    while not stopper_event.is_set():
        if g_event_queue:
            event_tuple = g_event_queue.popitem()
            try:
                evt0 = event_tuple[0].decode("utf-8")
                evt1 = event_tuple[1].decode("utf-8")
                # process the event
                # Validate event
                if 'null' in evt1 or 'None' in evt1:
                    logger.error('Malformed event, will skip - {}'.format(event_tuple[1]))
                    continue

                event_info = ast.literal_eval(evt1)
                logger.debug('Processing the event %s', event_tuple[1])
                if g_event_notifier_fn:
                    ret = g_event_notifier_fn([event_info])
                    ret_status = False
                    if ret:
                        if ret == [event_info]:
                            ret_status = True
                        elif not ret:
                            logger.error('Error in processing event %s with '
                                         'ret None', event_tuple[1])
                        else:
                            logger.error('Error in processing event %s with '
                                         'ret %s', event_tuple[1], str(ret))
                    if not ret_status:
                        add_event_to_worker_queue(event_tuple[0],
                                                  event_tuple[1])
                        continue

                # Run global handlers
                if events_def.global_event_handlers:
                    event_fn = events_def.global_event_handlers
                    logger.debug('Global event handler %s on event %s',
                                 event_fn, event_tuple[1])
                    ret = event_fn([event_info])
                    if ret:
                        if not ret or ret != [event_info]:
                            logger.error('Error in processing event %s with '
                                         'ret None', event_tuple[1])
                        else:
                            logger.error('Error in processing event %s with '
                                         'ret %s', event_tuple[1], str(ret))

                # Run local handlers if any
                local_handlers = events_def.events[event_info['name']]['handler']
                if local_handlers:
                    for lh in local_handlers:
                        logger.debug('Local event handler %s on event %s',
                                     lh, event_tuple[1])
                        ret = lh(event_info)
                        if not ret:
                            logger.error('Error in processing event %s by '
                                         'handler %s', str(event_info), lh)

                logger.debug('Sending the event %s to MQ library',
                             event_tuple[1])
                key = evt0.replace(ETCD_EVENT_TO_PROCESS_KEY_PREFIX,
                                   ETCD_EVENT_KEY_PREFIX)

                logger.debug('Updating the key %s in DB', key)
                g_etcd.save_key_value(key, event_tuple[1])
                g_etcd.delete_key_value(event_tuple[0])
            except Exception as e:
                logger.error('Exception %s in processing the event %s',
                             e, event_tuple[1])
                raise
        else:
            logger.debug('Clearing the thread event and waiting for signal')
            g_event_queue_tevent.clear()

        g_event_queue_tevent.wait(sleep_interval)

    logger.info("======> process events from worker queue has stopped <=========")


def get_node_name_from_uuid(node_uuid):
    #/object_storage/servers/f929de88-64b3-3334-8cf4-48769b1f73b4/server_attributes/identity/Hostname
    global g_etcd
    global g_node_uuid_map
    global g_servers_out
    logger = g_etcd.logger

    # pp = pprint.PrettyPrinter(indent=1)

    # pp.pprint(node_uuid)
    # pp.pprint(g_node_uuid_map)
    # pp.pprint(g_servers_out)

    if node_uuid in g_node_uuid_map:
        # print("Found node_uuid in g_node_uuid_map")
        logger.info("Found node_uuid in g_node_uuid_map")
        node_name = g_node_uuid_map[node_uuid]
    elif g_servers_out and (node_uuid in g_servers_out) and ('server_attributes' in g_servers_out[node_uuid].keys()) and ('identity' in g_servers_out[node_uuid]['server_attributes'].keys()) and ('Hostname' in g_servers_out[node_uuid]['server_attributes']['identity'].keys()):
        # print("Found node_uuid in g_servers_out")
        logger.info("Found node_uuid in g_servers_out")
        node_name = (g_servers_out[node_uuid]['server_attributes']['identity']['Hostname'])
    else:
        # print("node_uuid not found in g_node_uuid_map nor g_servers_out")
        logger.info("node_uuid not found in g_node_uuid_map nor g_servers_out")
        key = '/object_storage/servers/' + node_uuid + '/server_attributes/'
        key += 'identity/Hostname'
        try:
            node_name = g_etcd.get_key_value(key)
        except:
            logger.exception('Error in getting the node name for the UUID '
                             '%s', node_uuid)
            node_name = None

    if node_name and node_uuid not in g_node_uuid_map:
        g_node_uuid_map[node_uuid] = node_name
    elif not node_name:
        logger.info('Failed to get node name for UUID %s', node_uuid)
        node_name = node_uuid

    return node_name


def get_network_ipv4_address(node_uuid, mac_address):
    global g_etcd
    logger = g_etcd.logger

    """
    This function is suppose to get IPv4 address of the NIC
    params:
    node_uuid:<string> , Contains uuid
    mac_address:<string> , Contains mac address of NIC
    Return: <string> , IPv4 address
    """
    #/object_storage/servers/65f05d2b-cf03-3c60-91f4-03678639c0ac/server_attributes/network/interfaces/ec:0d:9a:98:a8:22/IPv4
    key = '/object_storage/servers/' + node_uuid + '/server_attributes'
    key += '/network/interfaces/' + mac_address + '/IPv4'
    try:
        ipv4_address = g_etcd.get_key_value(key)
    except:
        logger.exception('Error in getting the IPv4 address for the UUID '
                         '%s', node_uuid)
        ipv4_address = None
    return ipv4_address


def check_and_update_target_nodes_status(stopper_event, logger=None, sleep_interval=30):
    """
    This function checks the heartbeat value of each target node. If it was
    last updated more than twice the heartbeat interval, then try to ping the
    system using all the IPs available. If one of the IP is pingable,
    then mark the node as UP, but the respective down NICs will be marked as
    'down' along with the respective CRC field for the interface. If the CRC
    field is not updated, then the agent will not update the new value on
    detection.
    Also if the machine is pingable and the heartbeat value is alive, then we
    assume that the agent will update the NICs status.
    """
    logger.debug("Check & Update status {}".format(__name__))

    global g_servers_out
    global g_update
    global g_etcd

    g_servers_out = None
    try:
        tgt_node_status_lease = g_etcd.create_lease(4 * sleep_interval)
    except:
        logger.error('Error in creating lease for target nodes status')
        tgt_node_status_lease = None

    new_ip_status = {}
    tgt_node_key = '/object_storage/servers/{uuid}/node_status'
    tgt_interface_key = '/object_storage/servers/{uuid}/server_attributes' \
                        '/network/interfaces/{if_name}/Status'
    tgt_interface_crc = '/object_storage/servers/{uuid}/server_attributes' \
                        '/network/interfaces/{if_name}/CRC'

    while not stopper_event.is_set():
        logger.debug("check_and_update_target_nodes_status: looping")

        # Get all the target nodes and its attributes
        # if g_update or not g_servers_out:
        try:
            g_servers_out = g_etcd.get_key_with_prefix('/object_storage/servers')
            g_servers_out = g_servers_out['object_storage']['servers']

            hb_list = []
            if 'list' in g_servers_out:
                hb_list = g_servers_out.pop('list')
            g_update = False
        except:
            logger.error('Error in getting KVs from DB with prefix '
                         '/object_storage/servers')
            g_servers_out = None
            hb_list = []

        kv_dict = dict()
        old_ip_status = new_ip_status
        if g_servers_out:
            strings_with_lease = []
            for server in g_servers_out:
                try:
                    server_name = g_servers_out[server]['server_attributes'][
                        'identity']['Hostname']
                    ifaces = g_servers_out[server]['server_attributes'][
                        'network']['interfaces']
                    hb_time = hb_list[server]
                    if ((int(time.time()) - int(hb_time)) <
                            (2 * AGENT_HEARTBEAT_INTERVAL)):
                        continue
                    logger.info('Heartbeat last update is too old on server %s', server_name)
                except:
                    logger.error('Exception in getting server name or '
                                 'interfaces for %s', server)
                # The previous update of heartbeat for the server is more than
                # twice the HB interval. Lets ping the IP address and see if
                #  there is any issue with the node
                ipaddr_dict = {}
                status = False
                for iface in ifaces:
                    if 'IPv4' in ifaces[iface]:
                        ipaddr_dict[ifaces[iface]['IPv4']] = iface

                if len(ipaddr_dict):
                    # Run fping command on all the IP addresses at once. The
                    # output will show up on stderr only as following
                    # 10.1.30.134    : xmt/rcv/%loss = 3/0/100%
                    # 111.100.10.121 : xmt/rcv/%loss = 3/3/0%
                    cmd = 'fping -q -c 3 ' + ' '.join(list(ipaddr_dict.keys()))
                    pipe = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
                    out, err = pipe.communicate()
                    logger.debug('fping cmd %s out [%s] err [%s]', cmd, out, err)
                    if err:
                        status = False
                        lines_list = err.decode("utf-8").split('\n')
                        for line in lines_list:
                            logger.detail(line)
                            if not len(line):
                                continue
                            ipaddr_out = line.split(':')[0].strip()
                            iface = ipaddr_dict[ipaddr_out]
                            logger.detail("Updting NIC status for iface: [%s]", iface)
                            # Update the NIC status and the CRC on the DB
                            if 'min/avg/max' not in line:
                                logger.detail("min/avg/max not detected in line: [%s]", line)
                                if ifaces[iface]['Status'] != 'down':
                                    logger.detail("iface status not previously down, changing to down")
                                    logger.debug('interface: [%s], down', iface)
                                    ifaces[iface]['Status'] = 'down'
                                    crc = zlib.crc32(json.dumps(
                                        ifaces[iface], sort_keys=True).encode())
                                    ifaces[iface]['CRC'] = crc
                                    kv_dict[tgt_interface_key.format(
                                        uuid=server, if_name=iface)] = 'down'
                                    kv_dict[tgt_interface_crc.format(
                                        uuid=server, if_name=iface)] = str(crc)
                                else:
                                    logger.detail("NIC previously down, skipping")
                            elif ifaces[iface]['Status'] != 'up':
                                logger.detail("iface status not previously up, changing to up")
                                ifaces[iface]['Status'] = 'up'
                                crc = zlib.crc32(json.dumps(
                                    ifaces[iface], sort_keys=True).encode())
                                ifaces[iface]['CRC'] = crc
                                kv_dict[tgt_interface_key.format(
                                    uuid=server, if_name=ipaddr_dict[
                                        ipaddr_out])] = 'up'
                                kv_dict[tgt_interface_crc.format(
                                    uuid=server, if_name=iface)] = str(crc)
                                status = True
                            else:
                                logger.detail("NIC not detected, and not previously up, skipping")

                else:
                    status = False

                # Update the node status. If one of the NICs pingable,
                # then 'up'
                if server_name not in old_ip_status:
                    old_ip_status[server_name] = status
                new_ip_status[server_name] = status
                if old_ip_status[server_name] != new_ip_status[server_name]:
                    if status:
                        kv_dict[tgt_node_key.format(uuid=server)] = 'up'
                    else:
                        kv_dict[tgt_node_key.format(uuid=server)] = 'down'

                    strings_with_lease.append(tgt_node_key.format(server))

            logger.debug('KVs updated to ETCD DB %s', str(kv_dict))
            try:
                if kv_dict:
                    g_etcd.refresh_lease(lease=tgt_node_status_lease)
                    g_etcd.save_multiple_key_values(kv_dict,
                                                    strings_with_lease,
                                                    tgt_node_status_lease)
            except:
                logger.exception('Exception in saving target node status KVs')

        stopper_event.wait(sleep_interval)


def format_event(event, key, val=None):
    global g_update
    status = True

    global g_etcd
    logger = g_etcd.logger

    cluster_out = g_etcd.get_key_with_prefix('/cluster/')
    if cluster_out:
        g_clustername = cluster_out['cluster']['name']

    key_list = key.decode().split('/')
    if 'cluster' in key_list:
        if key_list[-1].endswith('status'):
            g_update = True
            event['node'] = key_list[2]
            event['args'] = {'node': key_list[2], 'cluster': g_clustername}
            if not val or val == b'down':
                event['name'] = 'CM_NODE_DOWN'
            else:
                event['name'] = 'CM_NODE_UP'
        elif key_list[-1].endswith('time_created'):
            g_update = True
            event['node'] = key_list[2]
            event['args'] = {'node': key_list[2], 'cluster': g_clustername}
            if not val:
                event['name'] = 'CM_NODE_REMOVED'
            else:
                event['name'] = 'CM_NODE_ADDED'
    elif 'subsystems' in key_list:
        if key_list[-1].endswith('status'):
            #/object_storage/servers/f929de88-64b3-3334-8cf4-48769b1f73b4/kv_attributes/config/subsystems/nqn.2018-09.samsung:ssg-test3-data/time_created
            node_name = get_node_name_from_uuid(key_list[3])
            event['args'] = {'nqn': key_list[7], 'node': node_name,
                             'cluster': g_clustername}
            event['node'] = node_name
            if not val or val == b'down':
                event['name'] = 'SUBSYSTEM_DOWN'
            elif val == b'up':
                event['name'] = 'SUBSYSTEM_UP'
        elif key_list[-1].endswith('time_created'):
            node_name = get_node_name_from_uuid(key_list[3])
            event['args'] = {'nqn': key_list[7], 'node': node_name,
                             'cluster': g_clustername}
            event['node'] = node_name
            if not val:
                event['name'] = 'SUBSYSTEM_DELETED'
            else:
                event['name'] = 'SUBSYSTEM_CREATED'
        '''
        # Commented out for now as there is no proper way of identifying
        subsystem down
        elif 'target' in key_list:
            if key_list[-1].endswith('status'):
                node_name = get_node_name_from_uuid(key_list[3])
                event['node'] = node_name
                event['args'] = {'node': node_name, 'cluster': g_clustername}
                if not val or val == b'down':
                    event['name'] = 'TARGET_APPLICATION_DOWN'
                else:
                    event['name'] = 'TARGET_APPLICATION_UP'
        '''
    elif 'agent' in key_list:
        if key_list[-1].endswith('status'):
            node_name = get_node_name_from_uuid(key_list[3])
            event['node'] = node_name
            event['args'] = {'node': node_name, 'cluster': g_clustername}
            if not val or val == b'down':
                event['name'] = 'AGENT_DOWN'
            elif val == b'up':
                event['name'] = 'AGENT_UP'

    elif 'network' in key_list:
        if key_list[-1].endswith('Status'):
            g_update = True
            mac = key_list[7]
            node_name = get_node_name_from_uuid(key_list[3])
            ipv4_address = get_network_ipv4_address(key_list[3], mac)
            event['node'] = node_name
            event['args'] = {'net_interface': mac, 'node': node_name,
                             'address': ipv4_address.decode('utf-8'),
                             'cluster': g_clustername}
            if not val or val == b'down':
                event['name'] = 'NETWORK_DOWN'
            elif val == b'up':
                event['name'] = 'NETWORK_UP'
    elif 'servers' in key_list:
        if key_list[-1].endswith('node_status'):
            node_name = get_node_name_from_uuid(key_list[3])
            event['node'] = node_name
            event['args'] = {'node': node_name, 'cluster': g_clustername}
            if not val or val == b'down':
                event['name'] = 'TARGET_NODE_UNREACHABLE'
            else:
                event['name'] = 'TARGET_NODE_ACCESSIBLE'
    elif 'locks' in key_list:
        pass
    else:
        logger.error('Unhandled event')
        status = False

    return status


def process_event(event):
    global g_etcd
    logger = g_etcd.logger

    # Check for the cluster status key and stop/start the keepalived service
    # depending on its status.
    hostname = socket.gethostname()

    cluster_status_key = '/cluster/' + hostname + '/status'
    if event.key == cluster_status_key:
        if not event.value:
            start_stop_service('keepalived', 'stop')
        else:
            old_value = event._event.prev_kv.value
            if old_value != event.value:
                if event.value == 'up':
                    start_stop_service('keepalived', 'start')
                else:
                    start_stop_service('keepalived', 'stop')

    # Start reading the events with old value and new value and start
    # processing
    if event.key.startswith(ETCD_EVENT_TO_PROCESS_KEY_PREFIX.encode('utf-8')):
        logger.debug('Events getting processed %s - %s', str(event.key), event.value and str(event.value) or None)
        if event.value:
            add_event_to_worker_queue(event.key, event.value)
        return

    if event.key.startswith(ETCD_EVENT_KEY_PREFIX.encode('utf-8')):
        return

    if event.value:
        old_value = event._event.prev_kv.value
        if old_value == event.value:
            return

    logger.debug('Events getting processed %s - %s', str(event.key), event.value and str(event.value) or None)

    t_event = dict()

    event_list = []
    status = format_event(t_event, event.key, event.value)
    if t_event and status:
        t_event['timestamp'] = str(int(time.time()))
        event_list.append(t_event)

    if event_list:
        logger.debug('Saving events to DB %s', str(event_list))
        try:
            save_events_to_etcd_db(g_etcd, event_list)
        except:
            logger.error('Exception in saving events')


def key_watcher_cb(event):
    global g_etcd
    logger = g_etcd.logger

    if isinstance(event.events, list):
        for e in event.events:
            # print("RAIN ---> EVENT ")
            # print(e)
            process_event(e)
    else:
        logger.error(f'Expected a list of events, received: {type(event.events)}')


def get_etcd_metrics(ip_address="127.0.0.1", port=2379):
    global g_etcd
    logger = g_etcd.logger

    url = "http://" + ip_address + ':' + str(port) + '/metrics'
    metrics_dict = dict()

    try:
        resp = requests.get(url)
        metric_list = resp.text.split('\n')
        for metric in metric_list:
            if metric.startswith('etcd_server_has_leader'):
                metrics_dict['leader'] = int(metric.split()[1])
            elif metric.startswith('etcd_server_leader_changes_seen_total'):
                metrics_dict['leader_changes'] = int(metric.split()[1])
            elif metric.startswith('etcd_server_proposals_failed_total'):
                metrics_dict['proposals_failed'] = int(
                    float(metric.split()[1]))
            elif metric.startswith('etcd_server_proposals_pending'):
                metrics_dict['proposals_pending'] = int(
                    float(metric.split()[1]))
            elif metric.startswith('etcd_server_proposals_committed_total'):
                metrics_dict['proposals_committed'] = int(
                    float(metric.split()[1]))
            elif metric.startswith('process_start_time_seconds'):
                metrics_dict['process_start_time_seconds'] = int(
                    time.time()- float(metric.split()[1]))
    except Exception as exc:
        logger.error('Error in getting the metrics from ETCD server %s' % exc.message)

    return metrics_dict


def start_stop_service(service_name, action):
    global g_etcd
    logger = g_etcd.logger

    cmd = ' '.join(['systemctl', str(action), str(service_name)])
    pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    if out:
        logger.info('cmd %s output %s', cmd, str(out))

    if err:
        logger.info('cmd %s error %s', cmd, str(err))


class Read_System_Space(threading.Thread):
    def __init__(self, stopper_event=None, db=None, hostname=None, check_interval=60, logger=None):
        super(Read_System_Space, self).__init__()
        self.stopper_event = stopper_event
        self.db = db
        self.hostname = hostname
        self.check_interval = check_interval
        self.logger = logger
        self.logger.debug("Monitor disk space thread Init")


    def run(self):
        self.logger.debug("Monitor disk space thread Started")

        while not self.stopper_event.is_set():
            # Read System Disk Space and write it to db
            df_struct = os.statvfs('/')
            if df_struct.f_blocks > 0:
                df_out = df_struct.f_bfree * 100 / df_struct.f_blocks
                if df_out:
                    self.db.save_key_value('/cluster/' + self.hostname + '/space_avail_percent', df_out)

            self.stopper_event.wait(self.check_interval)

        self.logger.debug("Monitor disk space thread Stopped")


class Update_Node_Info(threading.Thread):
    def __init__(self, stopper_event=None, db=None, hostname=None, check_interval=600, logger=None):
        super(Update_Node_Info, self).__init__()
        self.stopper_event = stopper_event
        self.db = db
        self.hostname = hostname
        self.check_interval = check_interval
        self.logger = logger
        self.node_status_lease = None


    def read_system_uptime(self):
        uptime = 0
        with open('/proc/uptime') as f:
            out = f.read()
            uptime = int(float(out.split()[0]))/3600  # Convert seconds to hours

        return uptime


    def read_db_status(self):
        cmd = "ETCDCTL_API=3 etcdctl endpoint health"

        pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
        status = "down"
        out, err = pipe.communicate()
        if out:
            for line in out.decode('utf-8').splitlines():
                if 'healthy' in line:
                    status = "up"
                    break

        return status


    def run(self):
        self.start_time = datetime.now()

        self.logger.debug("Start Update Node Info thread")

        while not self.stopper_event.is_set():
            try:
                if not self.node_status_lease or self.node_status_lease.remaining_ttl < 0:
                    self.node_status_lease = self.db.create_lease(LEASE_INTERVAL)
                else:
                    _, lease_id = self.db.refresh_lease(lease=self.node_status_lease)
                    if lease_id is None:
                        self.logger.error("Lease %s not refreshed" % self.node_status_lease.id)
            except:
                self.logger.exception('Failed to creating/renewing lease')
                return

            db_status = self.read_db_status()

            try:
                status_obj = self.db.get_status()
            except:
                status_obj = None

            self.db.save_key_value('/cluster/uptime_in_seconds', (datetime.now() - self.start_time).seconds)

            kv_dict = dict()
            system_uptime = self.read_system_uptime()
            if system_uptime:
                kv_dict['/cluster/' + self.hostname + '/uptime'] = str(system_uptime)

            kv_dict['/cluster/' + self.hostname + '/status'] = db_status
            kv_dict['/cluster/' + self.hostname + '/status_updated'] = str(int(time.time()))
            strings_with_lease = ['/cluster/leader', '/cluster/' + self.hostname + '/status']
            if status_obj:
                kv_dict['/cluster/' + self.hostname + '/db_size'] = str(status_obj.db_size)
                kv_dict['/cluster/leader'] = str(status_obj.leader.name)

            try:
                self.db.save_multiple_key_values(kv_dict, strings_with_lease, self.node_status_lease)
            except:
                self.logger.exception('Failed to save multiple KVs')

            self.stopper_event.wait(self.check_interval)

        self.db.save_key_value('/cluster/uptime_in_seconds', 0)
        self.logger.info("Update Node Info thread has Stopped")


class NkvMonitor(Monitor):
    def __init__(self, hostname=None, logger=None, db=None):
        Monitor.__init__(self)
        self.hostname = hostname
        g_hostname = self.hostname
        self.logger = logger
        self.db = db
        self.thread = None
        self.ev_worker_thread = None
        self.read_system_space = None
        self.update_node_info = None
        self.watch_id = None
        self.logger.info("NkvMonitor hostname = {}".format(self.hostname))

        self.thread_event = threading.Event()
        self.thread_event.clear()

        self.running = False
        self.external_msg_broker = False
        self.en = None
        time.sleep(180)


    def __del__(self):
        self.thread_event.clear()


    def read_node_capacity_in_kb(self):
        df = os.statvfs('/')
        if df.f_blocks > 0:
            return df.f_blocks * 4

        return 0


    def get_ip_of_nic(self):
        '''
         Return the first valid nic ip
         else return (127.0.0.1)
        '''
        localhost = None
        for ifaceName in interfaces():
            addresses = [i['addr'] for i in ifaddresses(ifaceName).setdefault(AF_INET, [{'addr':'No IP addr'}])]
            if ifaceName != 'lo':
                # print('{}: {}'.format(ifaceName, ', '.join(addresses)) )
                return addresses[0]

        return "127.0.0.1"


    # Note: This function can only be called after the start function
    def save_node_capacity(self, capacity_in_kb):
        try:
            self.db.save_key_value('/cluster/' + self.hostname + '/total_capacity_in_kb', str(capacity_in_kb))
        except:
            self.logger.exception('Exception in saving total_capacity_in_kb')


    def key_watcher(self):
        watch_id = self.db.watch_callback('/', key_watcher_cb, previous_kv=True)
        return watch_id


    def get_host_info(self):
        try:
            cluster_out = self.db.get_key_with_prefix('/cluster/')
            if cluster_out:
                return (cluster_out['cluster'][self.hostname]['ip_address'], cluster_out['cluster']['name'])
        except:
            self.logger.exception('Error in getting KVs from DB with prefix /cluster')
            sys.exit(-1)

        return None


    def start(self):
        self.logger.info("======> NkvMonitor <=========")

        if self.running:
            self.logger.debug("==> NkvMonitor is already running <==")
            return

        vip_address = None
        if self.external_msg_broker:
            vip_address = self.db.get_key_value('/cluster/ip_address')
            vip_address = vip_address.decode('utf-8')
        else:
            vip_address = self.get_ip_of_nic()

        if not vip_address:
            self.logger.error('Could not find VIP address of cluster. Exiting')
            sys.exit(-1)

        self.logger.debug("vip address: [%s]", vip_address)
        global g_event_notifier_fn
        if not g_event_notifier_fn:
            try:
                self.logger.debug('IP for the borker {}'.format(vip_address))

                self.en = EventNotification(vip_address, ZMQ_BROKER_PORT, logger=self.logger)
                g_event_notifier_fn = self.en.process_events
            except:
                self.logger.error('EventNotification instance not created')

        node_ip_address, g_clustername = self.get_host_info()
        if not node_ip_address:
            self.logger.error("Error in getting the node IP address. Exiting")
            sys.exit(-1)

        self.watch_id = self.key_watcher()

        # TODO(ER) - This needs to be done right way
        global g_etcd
        g_etcd = self.db

        self.logger.debug("initializing worker queue")
        initialize_worker_queue(self.logger)

        self.save_node_capacity(self.read_node_capacity_in_kb())

        self.read_system_space = Read_System_Space(stopper_event=self.thread_event,
                                                   db=self.db,
                                                   hostname=self.hostname,
                                                   check_interval=POLL_DISK_SPACE_INTERVAL,
                                                   logger=self.logger)
        self.read_system_space.start()

        self.update_node_info = Update_Node_Info(stopper_event=self.thread_event,
                                                 db=self.db,
                                                 hostname=self.hostname,
                                                 check_interval=UPDATE_NODE_INFO_INTERVAL,
                                                 logger=self.logger)
        self.update_node_info.start()

        try:
            self.thread = threading.Thread(target=check_and_update_target_nodes_status, args=[self.thread_event, self.logger])
            self.thread.daemon = True
            self.thread.start()
        except:
            self.logger.exception('Exception in spawning check and update target nodes status')
            return

        try:
            self.ev_worker_thread = threading.Thread(target=process_events_from_worker_queue, args=[self.thread_event, self.logger])
            self.ev_worker_thread.daemon = True
            self.ev_worker_thread.start()
        except:
            self.logger.exception('Exception in spawning process events thread')
            return

        self.running = True


    def stop(self):
        # TODO(ER) - Add this this in later
        # self.db.client.cancel_watch(self.watch_id)

        self.thread_event.set()

        if self.read_system_space and self.read_system_space.is_alive():
            self.read_system_space.join()

        if self.update_node_info and self.update_node_info.is_alive():
            self.update_node_info.join()

        if self.thread and self.thread.is_alive():
            self.thread.join()
            self.thread = None

        destroy_worker_queue(self.logger)

        if self.ev_worker_thread and self.ev_worker_thread.is_alive():
            self.ev_worker_thread.join()
            self.ev_worker_thread = None

        self.running = False
        self.logger.info("======> NkvMonitor Stopped <=========")


    def is_running(self):
        return self.running


