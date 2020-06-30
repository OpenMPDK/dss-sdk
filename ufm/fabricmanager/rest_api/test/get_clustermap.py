import redfish_client
import argparse
import json
import socket
from datetime import datetime

from rest_api.test.clustermap_utils import get_transport_type, get_percent_available


def add_transport(transport, address, family, speed, transport_type, port, status):
    transport['subsystem_address'] = address
    transport['subsystem_addr_fam'] = family
    transport['subsystem_speed_Mbps'] = speed
    transport['subsystem_type'] = transport_type
    transport['subsystem_port'] = port
    if status == 'LinkDown':
        status_val = 0
    elif status == 'LinkUp':
        status_val = 1
    else:
        status_val = -1

    transport['subsystem_interface_status'] = status_val


def add_cm_maps(cluster_map):
    hostname = socket.gethostname()
    address = socket.gethostbyname(hostname)
    cluster_map['cm_maps'] = [
        {
            'cm_server_address': address,
            'cm_server_name': hostname
        }
    ]


'''
Create a connection to the UFM redfish API service using the redfish-client
package.
redfish_client abstracts the connection and operations to a redfish compliant
service. The connect call returns the redfish service root.
Client code can iterate through redfish endpoints hierarchy without making any
rest calls, the redfish_client makes those calls internally and returns the
data.
'''


def get_cm(service_addr):
    s_time = datetime.now()
    # Will throw if connection fails. No need to catch it as nothing more to do
    # in that case
    root = redfish_client.connect(service_addr, '', '')

    # If the Systems collection is empty return an empty dictionary
    if not root.Systems.Members:
        return {}
    cluster_map = {}
    # Add CM/FM info
    add_cm_maps(cluster_map)
    subsystems_map = []
    for system in root.Systems.Members:
        subsystem = {}

        # Add nqn for any subsystems
        if 'Identifiers' in system:
            for identifier in system.Identifiers:
                if identifier.DurableNameFormat == 'NQN':
                    subsystem['subsystem_nqn'] = identifier.DurableName
                    subsystem['subsystem_nqn_id'] = system.oem['NSID']
                    subsystem['target_server_name'] = system.oem['ServerName']
                    subsystem['subsystem_numa_aligned'] = system.oem['NumaAligned']

                    if 'EthernetInterfaces' in system:
                        # No transports to add since no NICs
                        if not system.EthernetInterfaces.Members:
                            continue
                        # Add the transport fields for each NIC
                        subsystem_transport_list = []
                        for interface in system.EthernetInterfaces.Members:
                            if 'IPv4Addresses' in interface:
                                for ipv4addr in interface.IPv4Addresses:
                                    if ipv4addr.Address:
                                        subsystem_transport = {}
                                        transport_type, port = get_transport_type(ipv4addr)
                                        add_transport(subsystem_transport,
                                                      ipv4addr.Address,
                                                      socket.AF_INET,
                                                      interface.SpeedMbps,
                                                      transport_type,
                                                      port,
                                                      interface.LinkStatus)
                                        subsystem_transport_list.append(subsystem_transport)
                            if 'IPv6Addresses' in interface:
                                for ipv6addr in interface.IPv6Addresses:
                                    if ipv6addr.Address:
                                        subsystem_transport = {}
                                        transport_type, port = get_transport_type(ipv6addr)
                                        add_transport(subsystem_transport,
                                                      ipv6addr.Address,
                                                      socket.AF_INET6,
                                                      interface.SpeedMbps,
                                                      transport_type,
                                                      port,
                                                      interface.LinkStatus)
                                        subsystem_transport_list.append(subsystem_transport)
                        subsystem['subsystem_transport'] = subsystem_transport_list
                    if 'Storage' in system:
                        subsystem['subsystem_avail_percent'] = get_percent_available(system.Storage)

                    subsystems_map.append(subsystem)
    cluster_map['subsystem_maps'] = subsystems_map

    print(f"Building the clustermap took {(datetime.now() - s_time).seconds} seconds")
    return cluster_map


def validate_cm(service_addr, cm_file):
    cm_read = get_cm(service_addr)
    cm_expected = {}
    with open(cm_file) as cm_handle:
        cm_expected = json.load(cm_handle)
    if cm_read != cm_expected:
        print('Read Cluster Map and Expected Cluster Map do not match!\n')
        print('Read Cluster Map:\n', json.dumps(cm_read, indent=2))
        print('\nExpected Cluster Map:\n', {json.dumps(cm_expected, indent=2)})


if __name__ == '__main__':

    parser = argparse.ArgumentParser(
        description='Process Server\'s Configuration Settings.')
    parser.add_argument("--host", help="IP Address or FQDN of Server",
                        dest="host", required=True)
    parser.add_argument("--port", help="Port of Server",
                        dest="port", default=5000)
    parser.add_argument("--cm", help="Path to clustermap to validate against",
                        dest="clustermap", default='')

    args = parser.parse_args()

    service_addr = 'http://' + args.host + ':' + str(args.port)

    cm = {}
    if args.clustermap:
        validate_cm(service_addr, args.clustermap)
    else:
        cm = get_cm(service_addr)
        print('Cluster Map:\n', json.dumps(cm, indent=2))
