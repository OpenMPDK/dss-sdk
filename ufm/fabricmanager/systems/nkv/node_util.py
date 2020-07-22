import time


def get_clustername(db):
    cluster_out = db.get_key_with_prefix('/cluster/')
    if cluster_out:
        return cluster_out['cluster']['name']

    return None


def get_network_ipv4_address(db, node_uuid, mac_address):
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
        return db.get_key_value(key).decode('utf-8')
    except:
        return None


# A map of UUID to the node name
g_node_uuid_map = {}

def get_node_name_from_uuid(db, servers_out, node_uuid):
    #/object_storage/servers/f929de88-64b3-3334-8cf4-48769b1f73b4/server_attributes/identity/Hostname
    global g_node_uuid_map

    if node_uuid in g_node_uuid_map:
        node_name = g_node_uuid_map[node_uuid]
    elif servers_out and (node_uuid in servers_out) and ('server_attributes' in servers_out[node_uuid].keys()) and ('identity' in servers_out[node_uuid]['server_attributes'].keys()) and ('Hostname' in servers_out[node_uuid]['server_attributes']['identity'].keys()):
        node_name = (servers_out[node_uuid]['server_attributes']['identity']['Hostname'])
    else:
        key = '/object_storage/servers/' + node_uuid + '/server_attributes/'
        key += 'identity/Hostname'
        try:
            node_name = db.get_key_value(key).decode('utf-8')
        except:
            node_name = None

    if node_name and node_uuid not in g_node_uuid_map:
        g_node_uuid_map[node_uuid] = node_name
    elif not node_name:
        node_name = node_uuid

    return node_name


def format_event(event, db, clustername, servers_out, key, val=None):

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
        if key_list[-1].endswith('status'):
            #/object_storage/servers/f929de88-64b3-3334-8cf4-48769b1f73b4/kv_attributes/config/subsystems/nqn.2018-09.samsung:ssg-test3-data/time_created
            node_name = get_node_name_from_uuid(db, servers_out, key_list[3])
            event['args'] = {'nqn': key_list[7], 'node': node_name, 'cluster': clustername}
            event['node'] = node_name
            if not val or val == b'down':
                event['name'] = 'SUBSYSTEM_DOWN'
            elif val == b'up':
                event['name'] = 'SUBSYSTEM_UP'
        elif key_list[-1].endswith('time_created'):
            node_name = get_node_name_from_uuid(db, servers_out, key_list[3])
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
            mac = key_list[7]
            ipv4_address = get_network_ipv4_address(db, key_list[3], mac)

            state = 'NETWORK_DOWN'
            if ipv4_address == None:
                ipv4_address = "0.0.0.0"
            else:
                if val:
                    if val == b'up':
                        state = 'NETWORK_UP'

                    if val == b'down':
                        state = 'NETWORK_DOWN'

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


def start_stop_service(logger, service_name, action):
    cmd = ' '.join(['systemctl', str(action), str(service_name)])
    pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)

    out, err = pipe.communicate()
    if out:
        logger.info('cmd %s output %s', cmd, str(out))

    if err:
        logger.info('cmd %s error %s', cmd, str(err))



