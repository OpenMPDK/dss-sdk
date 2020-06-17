import redfish_client
import argparse
import json
import socket


def addTransport(transport, address, family, speed, transport_type, port, status):
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

def addCMMaps(cluster_map):
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
def getCM(service_addr):
    # Will throw if connection fails. No need to catch it as nothing more to do
    # in that case
    root = redfish_client.connect(service_addr, '', '')

    # If the Systems collection is empty return an empty dictionary
    if not root.Systems.Members:
        return {}
    cluster_map = {}
    # Add CM/FM info
    addCMMaps(cluster_map)
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
                                    if 'Address' in ipv4addr:
                                        subsystem_transport = {}
                                        addTransport(subsystem_transport,
                                                     ipv4addr.Address,
                                                     socket.AF_INET,
                                                     interface.SpeedMbps,
                                                     ipv4addr.oem.SupportedProtocol,
                                                     ipv4addr.oem.Port,
                                                     interface.LinkStatus)
                                        subsystem_transport_list.append(subsystem_transport)
                            if 'IPv6Addresses'  in interface:
                                for ipv6addr in interface.IPv6Addresses:
                                    if 'Address' in ipv6addr:
                                        subsystem_transport = {}
                                        addTransport(subsystem_transport,
                                                     ipv6addr.Address,
                                                     socket.AF_INET6,
                                                     interface.SpeedMbps,
                                                     ipv6addr.oem.SupportedProtocol,
                                                     ipv6addr.oem.Port,
                                                     interface.LinkStatus)
                                        subsystem_transport_list.append(subsystem_transport)
                        subsystem['subsystem_transport'] = subsystem_transport_list
                    if system.Storage:
                        subsystem['subsystem_avail_percent'] = system.Storage.oem.PercentAvailable

                    subsystems_map.append(subsystem)
    cluster_map['subsystem_maps'] = subsystems_map

    return cluster_map

def validateCM(service_addr, cm_file):
    cm_read = getCM(service_addr)
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
        cm = validateCM(service_addr, args.clustermap)
    else:
        cm = getCM(service_addr)
        print('Cluster Map:\n', json.dumps(cm, indent=2))
