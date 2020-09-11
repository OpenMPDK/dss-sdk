import time
from subprocess import PIPE, Popen


# A map of UUID to the node name
g_node_uuid_map = {}


def get_clustername(db):
    info = db.get_with_prefix('/cluster/')

    try:
        cluster_name = info['cluster']['name']
    except Exception:
        cluster_name = None

    return cluster_name


def get_network_ipv4_address(db, node_uuid, mac_address):
    """
    This function is suppose to get IPv4 address of the NIC
    params:
    node_uuid:<string> , Contains uuid
    mac_address:<string> , Contains mac address of NIC
    Return: <string> , IPv4 address
    """
    key = "/object_storage/servers/{}/server_attributes/network/interfaces/{}/IPv4".format(node_uuid, mac_address)

    try:
        ipv4_address, _ = db.get(key)
    except Exception:
        return None

    if not ipv4_address:
        return None

    return ipv4_address.decode('utf-8')


def read_node_name_from_db(db, node_uuid):
    key = "/object_storage/servers/{}/server_attributes/identity/Hostname".format(node_uuid)

    try:
        node_name, _ = db.get(key)
        return node_name.decode('utf-8')
    except Exception:
        node_name = None


def read_node_name_from_map(node_uuid):
    global g_node_uuid_map

    if node_uuid in g_node_uuid_map:
        return g_node_uuid_map[node_uuid]

    return None


def read_node_name_from_variable(node_uuid, servers):
    if not servers:
        return None

    if node_uuid not in servers:
        return None

    if 'server_attributes' not in servers[node_uuid].keys():
        return None

    if 'identity' not in servers[node_uuid]['server_attributes'].keys():
        return None

    if 'Hostname' not in servers[node_uuid]['server_attributes']['identity'].keys():
        return None

    return servers[node_uuid]['server_attributes']['identity']['Hostname']


def get_node_name_from_uuid(db, servers_out, node_uuid):
    node_name = read_node_name_from_map(node_uuid=node_uuid)
    if node_name:
        return node_name

    node_name = read_node_name_from_variable(node_uuid=node_uuid, servers=servers_out)
    if node_name:
        return node_name

    node_name = read_node_name_from_db(db=db, node_uuid=node_uuid)

    return node_name


def format_event(event, db, log, clustername, servers_out, key, val=None):
    key_list = key.decode().split('/')

    if 'cluster' in key_list:
        if key_list[-1].endswith('status'):
            status = 'CM_NODE_UP'
            if val and val == b'down':
                status = 'CM_NODE_DOWN'

            event['node'] = key_list[2]
            event['args'] = {'node': key_list[2], 'cluster': clustername}
            event['name'] = status
        elif key_list[-1].endswith('time_created'):
            event['node'] = key_list[2]
            event['args'] = {'node': key_list[2], 'cluster': clustername}
            if not val:
                event['name'] = 'CM_NODE_REMOVED'
            else:
                event['name'] = 'CM_NODE_ADDED'
        return True

    if 'subsystems' in key_list:
        node_name = get_node_name_from_uuid(db, servers_out, key_list[3])
        if not node_name:
            return False

        if key_list[-1].endswith('status'):
            event['args'] = {'nqn': key_list[7], 'node': node_name, 'cluster': clustername}
            event['node'] = node_name
            if not val or val == b'down':
                event['name'] = 'SUBSYSTEM_DOWN'
            elif val == b'up':
                event['name'] = 'SUBSYSTEM_UP'
        elif key_list[-1].endswith('time_created'):
            event['args'] = {'nqn': key_list[7], 'node': node_name, 'cluster': clustername}
            event['node'] = node_name
            if not val:
                event['name'] = 'SUBSYSTEM_DELETED'
            else:
                event['name'] = 'SUBSYSTEM_CREATED'
        return True

    if 'agent' in key_list:
        if key_list[-1].endswith('status'):
            node_name = get_node_name_from_uuid(db, servers_out, key_list[3])
            if not node_name:
                return False

            event['node'] = node_name
            event['args'] = {'node': node_name, 'cluster': clustername}

            if not val or val == b'down':
                event['name'] = 'AGENT_DOWN'
            elif val == b'up':
                event['name'] = 'AGENT_UP'
                time.sleep(5)
        return True

    if 'network' in key_list:
        if key_list[-1].endswith('Status'):
            node_name = get_node_name_from_uuid(db, servers_out, key_list[3])
            if not node_name:
                return False

            mac = key_list[7]
            state = 'NETWORK_DOWN'
            ipv4_address = get_network_ipv4_address(db, key_list[3], mac)
            if not ipv4_address:
                ipv4_address = "0.0.0.0"
            else:
                if val:
                    if val == b'up':
                        state = 'NETWORK_UP'

                    if val == b'down':
                        state = 'NETWORK_DOWN'

                    log.info("Network state changed. {} state={}".format(ipv4_address, state))

            event['node'] = node_name
            event['args'] = {'net_interface': mac,
                             'node': node_name,
                             'address': ipv4_address,
                             'cluster': clustername}
            event['name'] = state
        return True

    if 'servers' in key_list:
        if key_list[-1].endswith('node_status'):
            node_name = get_node_name_from_uuid(db, servers_out, key_list[3])
            if not node_name:
                return False

            event['node'] = node_name
            event['args'] = {'node': node_name, 'cluster': clustername}

            if not val or val == b'down':
                event['name'] = 'TARGET_NODE_UNREACHABLE'
            else:
                event['name'] = 'TARGET_NODE_ACCESSIBLE'
        return True

    if 'locks' in key_list:
        return True

    return False


def start_stop_service(log, service_name, action):
    cmd = ' '.join(['systemctl', str(action), str(service_name)])
    pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)

    out, err = pipe.communicate()
    if out:
        log.info('cmd %s output %s', cmd, str(out))

    if err:
        log.info('cmd %s error %s', cmd, str(err))
