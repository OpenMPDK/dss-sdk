import os,sys
import requests
import json
import socket
import redfish_client
from utility import exception


HTTP_RESPONSE_CODE = {
    "SUCCESS": 200
}

PROTOCOL = {
    "TCP":0,
    "RDMA":1
}

class FabricManager(object):

    def __init__(self, address, port, endpoint):
        self.address = address
        self.port = port
        self.endpoint = endpoint

    @exception
    def get_url(self):
        """
        Get URL
        :return:
        """
        return "http://{}:{}{}".format(self.address,self.port, self.endpoint)

    @exception
    def get_service_address(self):
        """
        Return service address for UFM
        :return:<string>  service address with port.
        """
        return "http://{}:{}".format(self.address,str(self.port))



class NativeFabricManager(FabricManager):
    """
    A FM based on REST API only.
    """

    def __init__(self, address, port, endpoint):
        FabricManager.__init__(self,address, port, endpoint)
        self.url = self.get_url()
        self.clustermap = self.get_response()

    @exception
    def get_response(self):
        """
        Make a REST call to get cluster_map.
        :return:
        """
        json_response = {}
        print("INFO: Calling rest API {}".format(self.url))
        response = requests.get(self.url)
        if response.status_code == HTTP_RESPONSE_CODE["SUCCESS"]:
            json_response = json.loads(response.content.decode('utf-8'))
        else:
            print("WARNING: Bad Response with code {}".format(response.status_code))
            self.response_code = response.status_code

        return json_response


    def get_cluster_map(self):
        """
        Return cluster map for NativeFabricManager
        :return:<dict> - ClusterMap
        """

        if self.clustermap:
            return self.clustermap
        else:
            return {}



class UnifiedFabricManager(FabricManager):
    """
    A FM based on Redfish API
    """

    def __init__(self, address,port,endpoint):
        FabricManager.__init__(self,address,port,endpoint)
        self.cluestermap = {}
        self.service_addr = self.get_service_address()

    @exception
    def add_transport(self,transport, address, family, speed, transport_type, port, status, numa):
        """
        Add interface information for nkv config.
        :param transport:
        :param address:
        :param family:
        :param speed:
        :param transport_type:
        :param port:
        :param status:
        :param numa:
        :return: None
        """
        transport['subsystem_address'] = address
        transport['subsystem_addr_fam'] = family
        transport['subsystem_interface_speed'] = speed
        transport['subsystem_interface_numa_aligned'] = numa
        transport['subsystem_type'] = PROTOCOL[transport_type]
        transport['subsystem_port'] = port

        interface_status = {"LinkDown":0,"LinkUp":1}
        transport['subsystem_interface_status'] = interface_status.get(status, -1 )


    @exception
    def add_fm_cluster_maps(self, cluster_map):
        pass

    @exception
    def get_cluster_map(self):
        """
        Process Redfish api and generate clustermap.
        :return: <dict> - Complete clustermap.
        """
        root = redfish_client.connect(self.service_addr, '', '')

        # If the Systems collection is empty return an empty dictionary
        if not root.Systems.Members:
            return {}
        cluster_map = {}

        #self.add_fm_cluster_maps(cluster_map)
        subsystems_map = []
        for system in root.Systems.Members:
            subsystem = {}
            # Add nqn for any subsystems
            subsystem['subsystem_nqn_id'] = (system.Id.split('.'))[-1]
            if system.Identifiers:
                for identifier in system.Identifiers:
                    if identifier.DurableNameFormat == 'NQN':
                        subsystem['subsystem_nqn'] = identifier.DurableName
                        subsystem['subsystem_nqn_nsid'] = system.oem['NSID']
                        subsystem['target_server_name'] = system.oem['ServerName']
                        subsystem['subsystem_status'] = 1
                        if system.Status:
                            if system.Status.State and system.Status.State == "Enabled"\
                                    and system.Status.Health and  system.Status.Health == "OK":
                                subsystem['subsystem_status'] = 0
                        if system.EthernetInterfaces:
                            # No transports to add since no NICs
                            if not system.EthernetInterfaces.Members:
                                continue
                            # Add the transport fields for each NIC
                            subsystem_transport_list = []
                            for interface in system.EthernetInterfaces.Members:
                                if interface.IPv4Addresses:
                                    for ipv4addr in interface.IPv4Addresses:
                                        if ipv4addr.Address:
                                            subsystem_transport = {}
                                            self.add_transport(subsystem_transport,
                                                         ipv4addr.Address,
                                                         socket.AF_INET,
                                                         interface.SpeedMbps,
                                                         ipv4addr.oem.SupportedProtocol,
                                                         ipv4addr.oem.Port,
                                                         interface.LinkStatus,
                                                         system.oem['NumaAligned'])
                                            subsystem_transport_list.append(subsystem_transport)
                                if interface.IPv6Addresses:
                                    for ipv6addr in interface.IPv6Addresses:
                                        if ipv6addr.Address:
                                            subsystem_transport = {}
                                            self.add_transport(subsystem_transport,
                                                         ipv6addr.Address,
                                                         socket.AF_INET6,
                                                         interface.SpeedMbps,
                                                         ipv6addr.oem.SupportedProtocol,
                                                         ipv6addr.oem.Port,
                                                         interface.LinkStatus,
                                                         system.oem['NumaAligned'])
                                            subsystem_transport_list.append(subsystem_transport)
                            subsystem['subsystem_transport'] = subsystem_transport_list
                        if system.Storage:
                            subsystem['subsystem_avail_percent'] = system.Storage.oem.PercentAvailable

                        subsystems_map.append(subsystem)
        cluster_map['subsystem_maps'] = subsystems_map
        self.cluestermap = cluster_map
        return cluster_map