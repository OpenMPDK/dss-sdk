# redfish_ufmdb.py

import pprint
import copy
import time
import uuid

from common.ufmdb.ufmdb import client
from common.ufmlog import ufmlog
from common.ufmdb.redfish.redfish_responses import redfish_responses
from common.ufmdb.redfish.redfish_resource import *
from common.ufmdb.redfish.redfish_action import *
from common.utils.ufm_decorators import singleton


@singleton
class RedfishUfmdb(object):
    root_uuid = str(uuid.uuid4())

    def __init__(self,root_uuid=None, auto_update=True, expire=5):
        """
        Create connection to database.
        """
        global g_ufmlog

        self.log = ufmlog.log(module="RFDB", mask=ufmlog.UFM_REDFISH_DB)
        g_ufmlog = self.log
        self.log.log_detail_on()
        #self.pp = pprint.PrettyPrinter(indent=4, sort_dicts=False)

        self.auto_update = auto_update

        self.ufmdb = client(db_type = 'etcd')
        self.redfish = dict()
        self.systems = list()
        self.action = dict()
        self.fabrics = list()

        self.data_expiration = 0.0
        self.expiration = float(expire)

        if self.auto_update == False:
            self.update()

    def pprint(self):
        print("SYSTEMS:")
        for s in self.systems:
            s.pprint()

    def _query_prefix(self, qstring):
        kv_dict = dict()

        query = self.ufmdb.get_prefix(qstring)

        if query == None:
            self.log.error('get_prefix(\"%s\") failed.  Retrying...', qstring)
            query = self.ufmdb.get_prefix(qstring)
            if query == None:
                self.log.error('get_prefix(\"%s\") retry failed. Exiting.', qstring)
                return dict()

        for value, metadata in query:
            kv_dict.update({metadata.key.decode('utf-8'):value.decode('utf-8')})

        return(kv_dict)

    def update(self):
        # Update action URL database
        self.log.detail('Update actions: requested.  Updating now ...')
        del(self.action)
        self.action = {}
        self._build_redfish_actions()

        # Update get URL database
        current_time = time.time()
        attempts = 2
        while True:
            try:
                if (current_time > self.data_expiration):
                    self.log.detail('Update data: requested.  Updating now ...')
                    self.data_expiration = current_time + self.expiration

                    del(self.redfish)
                    del(self.systems)

                    self.redfish = {}
                    self.systems = []
                    self.fabrics = []

                    self._process_database_for_systems()
                    self._process_database_for_fabrics()
                    self._build_redfish_root()
                    self._build_redfish_systems()
                    self._build_redfish_fabrics()

                else:
                    self.log.detail('Update data: requested.  Data expires in %d second(s).', 1 + int(self.data_expiration - current_time))
                break

            except Exception as e:
                attempts -= 1
                if attempts == 0:
                    break

                self.log.exception(e)
                self.data_expiration = 0.0
                self.log.error("Unable to update data.  Retrying...")

        return

    def _process_database_for_systems(self):

        self.log.detail('_process_database_for_systems: requested.')

        systems = self._query_prefix("/object_storage/servers/list")

        if len(systems) == 0:
            self.log.detail('_process_database_for_systems: done.')
            return

        for key in systems:
            sys_list = key.split("/")
            sys_uuid = sys_list[4]
            sys = system(uuid=sys_uuid)
            self.systems.append(sys)

            subsystems = self._query_prefix("/object_storage/servers/"+sys_uuid+"/kv_attributes/config/subsystems/")

            target_key = "/object_storage/servers/"+sys_uuid+"/server_attributes/identity/Hostname"
            target_dict = self._query_prefix(target_key)

            for ss_key in subsystems:
                if ss_key.find("UUID") != -1:
                    ss_uuid = sys_uuid+"."+subsystems[ss_key]
                    subsys = subsystem(uuid=ss_uuid)
                    sys.subsystems.append(subsys)

                    ss_list = ss_key.split("/")
                    subsys.nqn  = ss_list[7]
                    subsys.name = subsystems[ss_key]
                    subsys.servername = target_dict.get(target_key,"")

                    # Set the NQN ID for the subsystem
                    subsys.nsid = 1 #Set the subsystem nsid to the default of 1 for now. Needs to be revisited

                    transports = self._query_prefix("/object_storage/servers/"+sys_uuid+"/kv_attributes/config/subsystems/"+subsys.nqn+"/transport_addresses/")
                    for transport_key in transports:
                        if transport_key.find("traddr") != -1:
                            intr = interface()
                            subsys.interfaces.append(intr)
                            line_list = transport_key.split("/")
                            intr.mac = line_list[9]
                            intr.name = line_list[9]
                            traddr = transports[transport_key]

                            # Read status of ip interface
                            tran_mac = self._query_prefix("/object_storage/servers/"+sys_uuid+"/server_attributes/network/interfaces/"+intr.mac+"/Status")
                            for tran_mac_key in tran_mac:
                                if tran_mac_key.find("Status") != -1:
                                    status = tran_mac[tran_mac_key]
                                    if status == 'up':
                                        intr.status = "LinkUp"
                                    elif status == 'down':
                                        intr.status = "LinkDown"
                                    else:
                                        intr.status = "NoLink"

                            sub_attrs = self._query_prefix("/object_storage/servers/"+sys_uuid+"/kv_attributes/config/subsystems/"+subsys.nqn)
                            for sub_attr_key in sub_attrs:
                                if sub_attr_key.find("numa_aligned") != -1:
                                    numa = sub_attrs[sub_attr_key]
                                    if (int(numa) == 1):
                                        subsys.numa_aligned = True
                                    else:
                                        subsys.numa_aligned = False

                            # Read remaining info about interface
                            tran_mac = self._query_prefix("/object_storage/servers/"+sys_uuid+"/kv_attributes/config/subsystems/"+subsys.nqn+"/transport_addresses/"+intr.mac)
                            for tran_mac_key in tran_mac:
                                if tran_mac_key.find("adrfam") != -1:
                                    adrfam = tran_mac[tran_mac_key]
                                    if adrfam == "IPv4":
                                        ip4 = ipv4()
                                        ip4.addr = traddr
                                        intr.ip4_intf.append(ip4)
                                    elif adrfam == "IPv6":
                                        ip6 = ipv6()
                                        ip6.addr = traddr
                                        intr.ip6_intf.append(ip6)

                                if tran_mac_key.find("trsvcid") != -1:
                                    trsvcid = tran_mac[tran_mac_key]
                                    intr.port = int(trsvcid)

                                if tran_mac_key.find("trtype") != -1:
                                    trtype = tran_mac[tran_mac_key]
                                    intr.type = trtype

                                if tran_mac_key.find("interface_speed") != -1:
                                    speed = tran_mac[tran_mac_key]
                                    intr.speed = int(speed)

            # create a storage object
            stor = storage()

            # set its uuid to the hostname
            storage_attr = self._query_prefix("/object_storage/servers/"+sys_uuid+"/server_attributes/identity/Hostname")
            for attr_key in storage_attr:
                if attr_key.find("Hostname") != -1:
                    stor.uuid = storage_attr[attr_key]
                    stor.uuid = stor.uuid.split('.')[0]

            # Find and add devices to storage object
            storage_devices = self._query_prefix("/object_storage/servers/"+sys_uuid+"/server_attributes/storage/nvme/devices/")
            for device_key in storage_devices:
                if device_key.find("Serial") != -1:
                    device_sn = storage_devices[device_key]
                    drv = drive()
                    stor.drives.append(drv)

                    dev_info = self._query_prefix("/object_storage/servers/"+sys_uuid+"/server_attributes/storage/nvme/devices/"+device_sn)
                    for device_info_key in dev_info:
                        drv.uuid = device_sn
                        drv.sn = device_sn
                        drv.protocol = 'NVMeOverFabrics'
                        drv.manufacturer = 'Samsung'            #????
                        drv.type = 'SSD'                        #????

                        if device_info_key.find("LogiocalBlockSize") != -1:
                            drv.block_size = int(dev_info[device_info_key])
                            continue
                        if device_info_key.find("Model") != -1:
                            model_list = dev_info[device_info_key].split(' ')

                            if len(model_list) > 1:
                                drv.model = model_list[1]
                            else:
                                drv.model = model_list[0]
                            continue
                        if device_info_key.find("FirmwareRevision") != -1:
                            drv.revision = dev_info[device_info_key]
                            continue
                        if device_info_key.find("DiskCapacityInBytes") != -1:
                            drv.capacity = int(dev_info[device_info_key])
                            continue
                        if device_info_key.find("DiskUtilizationInBytes") != -1:
                            drv.utilization = int(dev_info[device_info_key])
                            continue

                        if device_info_key.find("DiskUtilizationPercentage") != -1:
                            drv.percent_avail = dev_info[device_info_key]
                            continue

                    stor.capacity = stor.capacity + drv.capacity
                    stor.utilization = stor.utilization + drv.utilization

            stor.percent_avail = float((1.0 - (stor.utilization/stor.capacity)) * 100.0)

            # Find the subsystem to attach this storage
            for subsys in sys.subsystems:
                nqn = subsys.nqn.lower()
                uuid = stor.uuid.lower()
                if nqn.find(uuid) != -1:
                    subsys.storage.append(stor)
                    subsys.capacity = subsys.capacity + stor.capacity
                    subsys.utilization = subsys.utilization + stor.utilization


        # Calculate subsystem storage level data
        for subsys in sys.subsystems:
                if len(subsys.storage):
                    subsys.percent_avail = int((1.0 - (subsys.utilization/subsys.capacity)) * 100)

        self.log.detail('_process_database: done.')

        return

    def _process_database_for_fabrics(self):

        '''
        Imagine Switch, Port, VLAN representation in DB
        /switches/list/{uuid_1}
        /switches/list/{uuid_2}
        /switches/{uuid_x}/switch_attributes/Manufacturer
        /switches/{uuid_x}/switch_attributes/Model
        /switches/{uuid_x}/switch_attributes/SerialNumber
        /switches/{uuid_x}/switch_attributes/UUID
        /switches/{uuid_x}/switch_attributes/IPv4
        /switches/{uuid_x}/switch_attributes/uptime
        /switches/{uuid_x}/ports/list/{port_id_1}
        /switches/{uuid_x}/ports/list/{port_id_2}
        /switches/{uuid_x}/ports/list/{port_id_3}
        /switches/{uuid_x}/ports/{port_id_y}/type
        /switches/{uuid_x}/ports/{port_id_y}/status
        /switches/{uuid_x}/ports/{port_id_y}/network/VLANs/{VLAN_id_a}
        /switches/{uuid_x}/ports/{port_id_y}/network/VLANs/{VLAN_id_b}
        /switches/{uuid_x}/VLANs/list/{VLAN_id_1}
        /switches/{uuid_x}/VLANs/list/{VLAN_id_2}
        /switches/{uuid_x}/VLANs/list/{VLAN_id_3}
        /switches/{uuid_x}/VLANs/{VLAN_id_z}/status
        /switches/{uuid_x}/VLANs/{VLAN_id_z}/network/ports/{port_id_c}
        /switches/{uuid_x}/VLANs/{VLAN_id_z}/network/ports/{port_id_d}
        '''

        self.log.detail('_process_database_for_fabrics: requested.')

        switches = self._query_prefix("/switches/list")

        if len(switches) == 0:
            self.log.detail('_process_database_for_fabrics: done.')
            return

        self.fabrics.append(fabrics())

        for fabric in self.fabrics:
            for key in switches:
                sw_list = key.split("/")
                sw_uuid = sw_list[3] #/switches/list/{uuid}
                sw = switch(uuid=sw_uuid)
                fabric.switches.append(sw)

                ports = "/switches/"+sw_uuid+"/ports/list"
                for port_key in ports:
                    port_list = port_key.split("/")
                    port_id = port_list[5] #/switches/{uuid}/ports/list/{port_id}
                    port = port(port_id=port_id)
                    sw.ports.append(port)

                    port_attr = self._query_prefix("/switches/"+sw_uuid+"/ports/"+port_id+"/status")
                    for attr_key in port_attr:
                        if attr_key.find("status") != -1:
                            port.status = port_attr[attr_key]

                vlans = "/switches/"+sw_uuid+"/VLANs/list"
                for vlan_key in vlans:
                    vlan_list = vlan_key.split("/")
                    vlan_id = vlan_list[5] #/switches/{uuid}/VLANs/list/{vlan_id}
                    vlan = vlan(vlan_id=vlan_id)
                    sw.vlans.append(vlan)

        self.log.detail('_process_database_for_fabrics: done.')

        return


    def _build_redfish_actions(self):
        self.log.detail('_build_redfish_actions: requested.')

        self.action['/redfish/v1/Managers/ufm/Actions/Ufm.Reset'] = ufm_reset_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.ClearLog'] = ufm_clearlog_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.Entries'] = ufm_entries_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.GetMask'] = ufm_getmask_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.SetMask'] = ufm_setmask_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.GetRegistry'] = ufm_getregistry_action

        self.log.detail('_build_redfish_actions: done.  entries=%d',len(self.action))
        return

    def _build_redfish_root(self):

        self.log.detail('_build_redfish_root: requested.')

        # 1
        response_1 = copy.deepcopy(redfish_responses['1'])
        response_1['UUID'] = self.root_uuid
        self.redfish[response_1['@odata.id']] = response_1   # 1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        if  len(self.systems) == 0:
            self.log.detail('_build_redfish_root: done.  entries=%d',len(self.redfish))
            return

        self.log.detail('_build_redfish_root: done.  entries=%d',len(self.redfish))
        return

    def _build_redfish_systems(self):

        self.log.detail('_build_redfish_systems: requested.')

        if  len(self.systems) == 0:
            self.log.detail('_build_redfish_systems: done.  entries=%d',len(self.redfish))
            return

        # 1.1
        response_1_1 = copy.deepcopy(redfish_responses['1.1'])
        response_1_1['@odata.id'] = '/redfish/v1/Systems'

        for sys in self.systems:
            response_1_1['Members'].append({'@odata.id': '/redfish/v1/Systems/'+sys.uuid})

            # Add the subsystems uuids too
            for subsys in sys.subsystems:
                response_1_1['Members'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid})

        response_1_1['Members@odata.count'] = len(response_1_1['Members'])
        self.redfish[response_1_1['@odata.id']] = response_1_1  # 1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.2
        response_1_2 = copy.deepcopy(redfish_responses['1.2'])
        self.redfish[response_1_2['@odata.id']] = response_1_2  # 1.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3
        response_1_3 = copy.deepcopy(redfish_responses['1.3'])
        self.redfish[response_1_3['@odata.id']] = response_1_3  # 1.3   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3.1
        response_1_3_1 = copy.deepcopy(redfish_responses['1.3.1'])
        response_1_3_1['UUID'] = sys.uuid
        self.redfish[response_1_3_1['@odata.id']] = response_1_3_1  # 1.3.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3.1.1
        response_1_3_1_1 = copy.deepcopy(redfish_responses['1.3.1.1'])
        self.redfish[response_1_3_1_1['@odata.id']] = response_1_3_1_1  # 1.3.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3.1.1.1
        response_1_3_1_1_1 = copy.deepcopy(redfish_responses['1.3.1.1.1'])
        response_1_3_1_1_1['MaxNumberOfRecords'] = g_ufmlog.ufmlog.max_entries
        self.redfish[response_1_3_1_1_1['@odata.id']] = response_1_3_1_1_1  # 1.3.1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        '''
        # 1.3.1.1.1.1
        response_1_3_1_1_1_1 = copy.deepcopy(redfish_responses['1.3.1.1.1.1'])
        self.redfish[response_1_3_1_1_1_1['@odata.id']] = response_1_3_1_1_1_1  # 1.3.1.1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
        '''

        # 1.3.1.1.1.2
        response_1_3_1_1_1_2 = copy.deepcopy(redfish_responses['1.3.1.1.1.2'])
        self.redfish[response_1_3_1_1_1_2['@odata.id']] = response_1_3_1_1_1_2  # 1.3.1.1.1.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3.1.1.1.3
        response_1_3_1_1_1_3 = copy.deepcopy(redfish_responses['1.3.1.1.1.3'])
        self.redfish[response_1_3_1_1_1_3['@odata.id']] = response_1_3_1_1_1_3  # 1.3.1.1.1.3   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.1.1
        for sys in self.systems:
            response_1_1_1 = copy.deepcopy(redfish_responses['1.1.1'])

            response_1_1_1['@odata.id'] = '/redfish/v1/Systems/'+sys.uuid
            response_1_1_1['Id'] = sys.uuid

            ident  = response_1_1_1['Identifiers'][0]
            ident['DurableName'] = sys.uuid

            ipv4  = response_1_1_1['IPv4Addresses'][0]
            ipv6  = response_1_1_1['IPv6Addresses'][0]
            links  = response_1_1_1['Links']

            # Add ip addresses and links
            for subsys in sys.subsystems:
                for interface in subsys.interfaces:
                    for ip4 in interface.ip4_intf:
                        ipv4['Address'] = ip4.addr
                        break
                    for ip6 in interface.ip6_intf:
                        ipv6['Address'] = ip6.addr
                        break
                links['SupplyingComputerSystems'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid})

            self.redfish[response_1_1_1['@odata.id']] = response_1_1_1  # 1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

            for subsys in sys.subsystems:
                response_1_1_2 = copy.deepcopy(redfish_responses['1.1.2'])

                response_1_1_2['@odata.id'] = '/redfish/v1/Systems/'+subsys.uuid
                response_1_1_2['Id'] = subsys.uuid

                ident  = response_1_1_2['Identifiers'][0]
                ident['DurableName'] = subsys.nqn

                if len(subsys.storage) != 0:
                    response_1_1_2['Storage'] = ({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/Storage"})

                if len(subsys.interfaces) != 0:
                    response_1_1_2['EthernetInterfaces'] = ({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces"})

                response_1_1_2['oem'] = { "ServerName":subsys.servername, "NSID":subsys.nsid, "NumaAligned":subsys.numa_aligned }

                response_1_1_2['Links']['ConsumingComputerSystems'].append({"@odata.id":"/redfish/v1/Systems/"+sys.uuid})

                self.redfish[response_1_1_2['@odata.id']] = response_1_1_2  # 1.1.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                if len(subsys.storage) != 0:
                    response_1_1_2_1 = copy.deepcopy(redfish_responses['1.1.2.1'])
                    response_1_1_2_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/Storage"

                    for stor in subsys.storage:
                        response_1_1_2_1['Members'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid})

                    response_1_1_2_1['Members@odata.count'] = len(response_1_1_2_1['Members'])
                    response_1_1_2_1['oem'] = { "CapacityBytes":subsys.capacity, "UtilizationBytes":subsys.utilization,"PercentAvailable":subsys.percent_avail}

                    self.redfish[response_1_1_2_1['@odata.id']] = response_1_1_2_1  # 1.1.2.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


                    response_1_1_2_1_1 = copy.deepcopy(redfish_responses['1.1.2.1.1'])
                    response_1_1_2_1_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid
                    response_1_1_2_1_1['Id'] = stor.uuid

                    for drive in stor.drives:
                        response_1_1_2_1_1['Drives'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid+"/Drives/"+drive.uuid})

                    response_1_1_2_1_1['oem'] = { "CapacityBytes":stor.capacity, "UtilizationBytes":stor.utilization, "PercentAvailable":stor.percent_avail}

                    self.redfish[response_1_1_2_1_1['@odata.id']] = response_1_1_2_1_1  # 1.1.2.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


                    response_1_1_2_1_1_1 = copy.deepcopy(redfish_responses['1.1.2.1.1.1'])
                    response_1_1_2_1_1_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid+"/Drives"
                    response_1_1_2_1_1_1['Id'] = stor.uuid

                    for drive in stor.drives:
                        response_1_1_2_1_1_1['Members'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid+"/Drives/"+drive.uuid})

                    response_1_1_2_1_1_1['Members@odata.count'] = len(response_1_1_2_1_1_1['Members'])
                    self.redfish[response_1_1_2_1_1_1['@odata.id']] = response_1_1_2_1_1_1  # 1.1.2.1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                    for drive in stor.drives:
                        response_1_1_2_1_1_1_1 = copy.deepcopy(redfish_responses['1.1.2.1.1.1.1'])
                        response_1_1_2_1_1_1_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid+"/Drives/"+drive.uuid
                        response_1_1_2_1_1_1_1['Id'] = drive.uuid
                        response_1_1_2_1_1_1_1['BlockSizeBytes'] = drive.block_size
                        response_1_1_2_1_1_1_1['CapacityBytes'] = drive.capacity
                        response_1_1_2_1_1_1_1['Id'] = drive.uuid
                        response_1_1_2_1_1_1_1['Manufacturer'] = drive.manufacturer
                        response_1_1_2_1_1_1_1['MediaType'] = drive.type
                        response_1_1_2_1_1_1_1['Model'] = drive.model
                        response_1_1_2_1_1_1_1['Protocol'] = drive.protocol
                        response_1_1_2_1_1_1_1['Revision'] = drive.revision
                        response_1_1_2_1_1_1_1['SerialNumber'] = drive.sn
                        response_1_1_2_1_1_1_1['oem'] = {"UtilizationBytes":drive.utilization, "PercentAvailable":drive.percent_avail}

                        self.redfish[response_1_1_2_1_1_1_1['@odata.id']] = response_1_1_2_1_1_1_1  # 1.1.2.1.1.1.1  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


                if len(subsys.interfaces) != 0:
                    response_1_1_2_2 = copy.deepcopy(redfish_responses['1.1.2.2'])
                    response_1_1_2_2['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces"

                    for intf in subsys.interfaces:
                        response_1_1_2_2['Members'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces/"+intf.mac})
                    response_1_1_2_2['Members@odata.count'] = len(response_1_1_2_2['Members'])
                    #print("members=", response_1_1_2_2['Members@odata.count'])
                    self.redfish["/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces"] = response_1_1_2_2  # 1.1.2.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                    for intf in subsys.interfaces:
                        response_1_1_2_2_1 = copy.deepcopy(redfish_responses['1.1.2.2.1'])
                        response_1_1_2_2_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces/"+intf.mac
                        response_1_1_2_2_1['Id'] = intf.mac
                        response_1_1_2_2_1['SpeedMbps'] = intf.speed
                        response_1_1_2_2_1['MACAddress'] = intf.mac
                        response_1_1_2_2_1['LinkStatus'] = intf.status

                        ipv4  = response_1_1_2_2_1['IPv4Addresses'][0]
                        for ip4 in intf.ip4_intf:
                            ipv4['Address'] = ip4.addr
                            ipv4['oem']['Port'] = intf.port
                            ipv4['oem']['SupportedProtocol'] = intf.type
                            break

                        ipv6  = response_1_1_2_2_1['IPv6Addresses'][0]
                        for ip6 in intf.ip6_intf:
                            ipv6['Address'] = ip6.addr
                            ipv4['oem']['Port'] = intf.port
                            ipv4['oem']['SupportedProtocol'] = intf.type
                            break

                        self.redfish["/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces/"+intf.mac] = response_1_1_2_2_1  # 1.1.2.2.1  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        self.log.detail('_build_redfish_systems: done.  entries=%d',len(self.redfish))
        return



    def _build_redfish_fabrics(self):

        self.log.detail('_build_redfish_fabrics: requested.')

        if  len(self.fabrics) == 0:
            self.log.detail('_build_redfish_fabrics: done.  entries=%d',len(self.redfish))
            return

        # 1.4
        response_1_4 = copy.deepcopy(redfish_responses['1.4'])
        self.redfish[response_1_4['@odata.id']] = response_1_4  # 1.4   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        for fabric in self.fabrics:
            response_1_4['Members'].append({'@odata.id': '/redfish/v1/Fabrics/' + fabric.id})

        response_1_4['Members@odata.count'] = len(response_1_4['Members'])

        for fabric in self.fabrics:
            # 1.4.1
            response_1_4_1 = copy.deepcopy(redfish_responses['1.4.1'])
            response_1_4_1['id'] = fabric.id
            response_1_4_1['@odata.id'] = response_1_4['@odata.id']+'/'+fabric.id
            self.redfish[response_1_4_1['@odata.id']] = response_1_4_1  # 1.4.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

            if len(fabric.switches) == 0:
                continue

            response_1_4_1['Switches'] = ({'@odata.id': response_1_4_1['@odata.id'] + '/Switches'})

            # 1.4.1.1
            response_1_4_1_1 = copy.deepcopy(redfish_responses['1.4.1.1'])
            response_1_4_1_1['@odata.id'] = response_1_4_1['@odata.id'] + '/Switches'
            self.redfish[response_1_4_1_1['@odata.id']] = response_1_4_1_1  # 1.4.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

            for sw in fabric.switches:
                # 1.4.1.1.1
                response_1_4_1_1_1 = copy.deepcopy(redfish_responses['1.4.1.1.1'])
                response_1_4_1_1_1['id'] = sw.id
                response_1_4_1_1_1['@odata.id'] = response_1_4_1_1['@odata.id'] + '/' + sw.id
                self.redfish[response_1_4_1_1_1['@odata.id']] = response_1_4_1_1_1  # 1.4.1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                if len(sw.ports) > 0:
                    response_1_4_1_1_1['Ports'] = ({"@odata.id": response_1_4_1_1_1['@odata.id'] + "/Ports"})

                    # 1.4.1.1.1.1
                    response_1_4_1_1_1_1 = copy.deepcopy(redfish_responses['1.4.1.1.1.1'])
                    response_1_4_1_1_1_1['@odata.id'] = response_1_4_1_1_1['@odata.id'] + "/Ports"
                    self.redfish[response_1_4_1_1_1_1['@odata.id']] = response_1_4_1_1_1_1 # 1.4.1.1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                    for port in sw.ports:
                        response_1_4_1_1_1_1['Members'].append({'@odata.id': response_1_4_1_1_1_1['@odata.id'] + '/' + port.id})

                    response_1_4_1_1_1_1['Members@odata.count'] = len(response_1_4_1_1_1_1['Members'])

                    for port in sw.ports:
                        response_1_4_1_1_1_1_1['@odata.id'] = response_1_4_1_1_1_1['@odata.id'] + '/' + port.id
                        response_1_4_1_1_1_1_1['id'] = port.id
                        self.redfish[response_1_4_1_1_1_1_1['@odata.id']] = response_1_4_1_1_1_1_1

                if len(sw.vlans) > 0:
                    response_1_4_1_1_1['VLANs'] = ({"@odata.id": response_1_4_1_1_1['@odata.id'] + "/VLANs"})

                    # 1.4.1.1.1.2
                    response_1_4_1_1_1_2 = copy.deepcopy(redfish_responses['1.4.1.1.1.2'])
                    response_1_4_1_1_1_2['@odata.id'] = response_1_4_1_1_1['@odata.id'] + "/VLANs"
                    self.redfish[response_1_4_1_1_1_2['@odata.id']] = response_1_4_1_1_1_2 # 1.4.1.1.1.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                    for vlan in sw.vlans:
                        response_1_4_1_1_1_2['Members'].append({'@odata.id': response_1_4_1_1_1_2['@odata.id'] + '/' + vlan.id})

                    response_1_4_1_1_1_2['Members@odata.count'] = len(response_1_4_1_1_1_2['Members'])

                    for vlan in sw.vlans:
                        response_1_4_1_1_1_2_1['@odata.id'] = response_1_4_1_1_1_2['@odata.id'] + '/' + vlan.id
                        response_1_4_1_1_1_2_1['id'] = vlan.id
                        self.redfish[response_1_4_1_1_1_2_1['@odata.id']] = response_1_4_1_1_1_2_1

        return



    def get(self, request=None, payload={}):
        '''
        Translates a redfish request string request to a json file response
        '''
        self.log.detail('GET: %s', request)

        try:
            if self.auto_update == True:
                self.update()

            if request in self.action:
                func = self.action[request]
                response = func(payload)

            elif request in self.redfish:
                response = self.redfish[request]

            else:
                if request != "/favicon.ico":
                    self.log.error('GET: Invalid Request. %s', request)
                response = { "Status": 404, "Message": "Not Found" }

            return response
        except Exception as e:
            self.log.exception(e)

            if request != "/favicon.ico":
                self.log.error('GET: Invalid Request. %s', request)

            response = { "Status": 404, "Message": "Not Found" }
            return response

    def post(self, request=None, payload={}):
        '''
        Translates a redfish request string request to an action function
        '''
        self.log.detail('POST: %s', request)

        try:
            if self.auto_update == True:
                self.update()

            if request in self.action:
                func = self.action[request]
                response = func(payload)

            else:
                if request != "/favicon.ico":
                    self.log.error('GET: Invalid Request. %s', request)
                response = { "Status": 404, "Message": "Not Found" }

            return response
        except:
            if request != "/favicon.ico":
                self.log.error('GET: Invalid Request. %s', request)

            response = { "Status": 404, "Message": "Not Found" }
            return response





