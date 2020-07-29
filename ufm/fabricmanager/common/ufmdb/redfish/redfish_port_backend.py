
from rest_api.redfish import redfish_constants
from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.ufmdb_util import ufmdb_util


class RedfishPortBackend():
    def __init__(self):
        self.cfg = {
            "@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{Ports}/{port_id}",
            "@odata.type": "#Port.v1_2_1.Port",
            "Id": "{id}",
            "Description": "Port Interface",
            "PortId": "{port_id}",
            "Name": "Eth1/{port_id}",
            "Mode": "{port_mode}",
            "Oem": {
                "PFC": "{pfc_status}",
                "TrustMode": "{trust_mode}"
            }
        }

    def get(self, fab_id, sw_id, port_id):
        try:
            self.cfg["@odata.id"] = self.cfg["@odata.id"].format(rest_base=redfish_constants.REST_BASE,
                                                                 Fabrics=redfish_constants.FABRICS,
                                                                 fab_id=fab_id,
                                                                 Switches=redfish_constants.SWITCHES,
                                                                 switch_id=sw_id,
                                                                 Ports=redfish_constants.PORTS,
                                                                 port_id=port_id)

            self.cfg["Id"] = self.cfg["Id"].format(id=port_id)
            self.cfg["PortId"] = self.cfg["PortId"].format(port_id=port_id)
            self.cfg["Name"] = self.cfg["Name"].format(port_id=port_id)

            port = self.get_port(sw_id, port_id)
            self.cfg["Mode"] = self.cfg["Mode"].format(port_mode=port["mode"])
            self.cfg["Oem"]["PFC"] = self.cfg["Oem"]["PFC"].format(pfc_status=port["pfc_status"])
            self.cfg["Oem"]["TrustMode"] = self.cfg["Oem"]["TrustMode"].format(trust_mode=port["trust_mode"])

            self.cfg["Links"] = {"AccessVLAN": {}, "AllowedVLANs": []}
            if port["access_vlan"]:
                vlan_path = "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{VLANs}/{vlan_id}".format(
                            rest_base=redfish_constants.REST_BASE,
                            Fabrics=redfish_constants.FABRICS,
                            fab_id=fab_id,
                            Switches=redfish_constants.SWITCHES,
                            switch_id=sw_id,
                            VLANs=redfish_constants.VLANS,
                            vlan_id=port["access_vlan"])

                self.cfg["Links"]["AccessVLAN"]["@odata.id"] = vlan_path

            if port["allowed_vlans"]:
                for vlan_id in port["allowed_vlans"]:
                    vlan_path = "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{VLANs}/{vlan_id}".format(
                                rest_base=redfish_constants.REST_BASE,
                                Fabrics=redfish_constants.FABRICS,
                                fab_id=fab_id,
                                Switches=redfish_constants.SWITCHES,
                                switch_id=sw_id,
                                VLANs=redfish_constants.VLANS,
                                vlan_id=vlan_id)
                    self.cfg["Links"]["AllowedVLANs"].append({"@odata.id": vlan_path})

            ###############################
            self.cfg['Actions'] = {}
            self.cfg['Actions']['#SetAccessPortVLAN'] = {}
            self.cfg['Actions']['#SetAccessPortVLAN']['description'] = \
                'Set this port to access mode that connects to a host. ' \
                + 'Must specify a default configured VLAN.'
            self.cfg['Actions']['#SetAccessPortVLAN']['target'] = self.cfg['@odata.id'] + '/Actions/SetAccessPortVLAN'
            self.cfg['Actions']['#SetAccessPortVLAN']['Parameters'] = []

            param = {}
            param['Name'] = 'VLANId'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '1'
            param['MaximumValue'] = '4094'
            self.cfg['Actions']['#SetAccessPortVLAN']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#UnassignAccessPortVLAN'] = {}
            self.cfg['Actions']['#UnassignAccessPortVLAN']['description'] = 'Set this port access VLAN to default 1.'
            self.cfg['Actions']['#UnassignAccessPortVLAN']['target'] = \
                self.cfg['@odata.id'] + '/Actions/UnassignAccessPortVLAN'

            ###############################
            self.cfg['Actions']['#SetTrunkPortVLANsAll'] = {}
            self.cfg['Actions']['#SetTrunkPortVLANsAll']['description'] = \
                'Set this port to trunk mode connecting 2 switches. ' + \
                'By default, a trunk port is automatically a member on all current and future VLANs.'
            self.cfg['Actions']['#SetTrunkPortVLANsAll']['target'] = \
                self.cfg['@odata.id'] + '/Actions/SetTrunkPortVLANsAll'

            ###############################
            self.cfg['Actions']['#SetTrunkPortVLANsRange'] = {}
            self.cfg['Actions']['#SetTrunkPortVLANsRange']['description'] = \
                'Set this port to trunk mode connecting 2 switches. ' + \
                'This trunk port is a member on the range of VLANs specified by RangeFromVLANId and RangeToVLANId.'
            self.cfg['Actions']['#SetTrunkPortVLANsRange']['target'] = \
                self.cfg['@odata.id'] + '/Actions/SetTrunkPortVLANsRange'
            self.cfg['Actions']['#SetTrunkPortVLANsRange']['Parameters'] = []

            param = {}
            param['Name'] = 'RangeFromVLANId'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '1'
            param['MaximumValue'] = '4094'
            self.cfg['Actions']['#SetTrunkPortVLANsRange']['Parameters'].append(param)

            param = {}
            param['Name'] = 'RangeToVLANId'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '1'
            param['MaximumValue'] = '4094'
            self.cfg['Actions']['#SetTrunkPortVLANsRange']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#SetHybridPortAccessVLAN'] = {}
            self.cfg['Actions']['#SetHybridPortAccessVLAN']['description'] = \
                'Set this port to hybrid mode and specify the access VLAN. '
            self.cfg['Actions']['#SetHybridPortAccessVLAN']['target'] = \
                self.cfg['@odata.id'] + '/Actions/SetHybridPortAccessVLAN'
            self.cfg['Actions']['#SetHybridPortAccessVLAN']['Parameters'] = []

            param = {}
            param['Name'] = 'VLANId'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '1'
            param['MaximumValue'] = '4094'
            self.cfg['Actions']['#SetHybridPortAccessVLAN']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#SetHybridPortAllowedVLAN'] = {}
            self.cfg['Actions']['#SetHybridPortAllowedVLAN']['description'] = \
                'Set this port to hybrid mode and add the allowed VLAN. '
            self.cfg['Actions']['#SetHybridPortAllowedVLAN']['target'] = \
                self.cfg['@odata.id'] + '/Actions/SetHybridPortAllowedVLAN'
            self.cfg['Actions']['#SetHybridPortAllowedVLAN']['Parameters'] = []

            param = {}
            param['Name'] = 'VLANId'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '1'
            param['MaximumValue'] = '4094'
            self.cfg['Actions']['#SetHybridPortAllowedVLAN']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#RemoveHybridPortAllowedVLAN'] = {}
            self.cfg['Actions']['#RemoveHybridPortAllowedVLAN']['description'] = \
                'Remove the specified VLAN from the hybrid port. '
            self.cfg['Actions']['#RemoveHybridPortAllowedVLAN']['target'] = \
                self.cfg['@odata.id'] + '/Actions/RemoveHybridPortAllowedVLAN'
            self.cfg['Actions']['#RemoveHybridPortAllowedVLAN']['Parameters'] = []

            param = {}
            param['Name'] = 'VLANId'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '1'
            param['MaximumValue'] = '4094'
            self.cfg['Actions']['#RemoveHybridPortAllowedVLAN']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#EnablePortPfc'] = {}
            self.cfg['Actions']['#EnablePortPfc']['description'] = 'Enables PFC mode on port.'
            self.cfg['Actions']['#EnablePortPfc']['target'] = \
                self.cfg['@odata.id'] + '/Actions/EnablePortPfc'

            ###############################
            self.cfg['Actions']['#DisablePortPfc'] = {}
            self.cfg['Actions']['#DisablePortPfc']['description'] = 'Disables PFC mode on port.'
            self.cfg['Actions']['#DisablePortPfc']['target'] = \
                self.cfg['@odata.id'] + '/Actions/DisablePortPfc'

            ###############################
            self.cfg['Actions']['#EnableEcnMarking'] = {}
            self.cfg['Actions']['#EnableEcnMarking']['description'] = \
                'Enables explicit congestion notification for traffic class queue.'
            self.cfg['Actions']['#EnableEcnMarking']['target'] = \
                self.cfg['@odata.id'] + '/Actions/EnableEcnMarking'
            self.cfg['Actions']['#EnableEcnMarking']['Parameters'] = []

            param = {}
            param['Name'] = 'TrafficClass'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '0'
            param['MaximumValue'] = '7'
            self.cfg['Actions']['#EnableEcnMarking']['Parameters'].append(param)

            param = {}
            param['Name'] = 'MinAbsoluteInKBs'
            param['Required'] = True
            param['DataType'] = 'Number'
            self.cfg['Actions']['#EnableEcnMarking']['Parameters'].append(param)

            param = {}
            param['Name'] = 'MaxAbsoluteInKBs'
            param['Required'] = True
            param['DataType'] = 'Number'
            self.cfg['Actions']['#EnableEcnMarking']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#DisableEcnMarking'] = {}
            self.cfg['Actions']['#DisableEcnMarking']['description'] = 'Disables ECN marking for traffic class queue.'
            self.cfg['Actions']['#DisableEcnMarking']['target'] = \
                self.cfg['@odata.id'] + '/Actions/DisableEcnMarking'

            ###############################
            self.cfg['Actions']['#ShowPfcCounters'] = {}
            self.cfg['Actions']['#ShowPfcCounters']['description'] = \
                'Display priority flow control counters for the specified interface and priority.'
            self.cfg['Actions']['#ShowPfcCounters']['target'] = \
                self.cfg['@odata.id'] + '/Actions/ShowPfcCounters'
            self.cfg['Actions']['#ShowPfcCounters']['Parameters'] = []

            param = {}
            param['Name'] = 'Priority'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '0'
            param['MaximumValue'] = '7'
            self.cfg['Actions']['#ShowPfcCounters']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#ShowCongestionControl'] = {}
            self.cfg['Actions']['#ShowCongestionControl']['description'] = \
                'Displays specific interface congestion control information.'
            self.cfg['Actions']['#ShowCongestionControl']['target'] = \
                self.cfg['@odata.id'] + '/Actions/ShowCongestionControl'

            ###############################
            self.cfg['Actions']['#ShowBufferDetails'] = {}
            self.cfg['Actions']['#ShowBufferDetails']['description'] = \
                'Displays specific interface buffer information.'
            self.cfg['Actions']['#ShowBufferDetails']['target'] = \
                self.cfg['@odata.id'] + '/Actions/ShowBufferDetails'

            ###############################
            self.cfg['Actions']['#BindPriorityToBuffer'] = {}
            self.cfg['Actions']['#BindPriorityToBuffer']['description'] = \
                'Bind switch priority to specific buffer.'
            self.cfg['Actions']['#BindPriorityToBuffer']['target'] = \
                self.cfg['@odata.id'] + '/Actions/BindPriorityToBuffer'
            self.cfg['Actions']['#BindPriorityToBuffer']['Parameters'] = []

            param = {}
            param['Name'] = 'Buffer'
            param['Required'] = True
            param['DataType'] = 'String'
            self.cfg['Actions']['#BindPriorityToBuffer']['Parameters'].append(param)

            param = {}
            param['Name'] = 'Priority'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '0'
            param['MaximumValue'] = '7'
            self.cfg['Actions']['#BindPriorityToBuffer']['Parameters'].append(param)

            ###############################
            response = self.cfg, redfish_constants.SUCCESS

        except Exception as e:
            # print('Caught exc {} in RedfishPortBackend.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def put(self, payload):
        pass

    def get_port(self, sw_id, port_id):
        ret = {
            "mode": "",
            "pfc_status": "",
            "trust_mode": "",
            "access_vlan": "",
            "allowed_vlans": []
        }
        pre = "/switches/" + sw_id + "/ports/" + str(port_id)

        prefix = pre + "/mode/"
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            ret["mode"] = k.split('/')[-1]

        prefix = pre + "/pfc/pfc_status/"
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            print(k)
            ret["pfc_status"] = k.split('/')[-1]

        prefix = pre + "/pfc/trust_mode/"
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            ret["trust_mode"] = k.split('/')[-1]

        prefix = pre + "/network/access_vlan/"
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            ret["access_vlan"] = k.split("/")[-1]

        prefix = pre + "/network/allowed_vlans/"
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            kk = k.split("/")[-1]
            if kk:
                '''
                allowed_vlans: 1-8,10,14-20
                need to convert numeric string ranges to a list
                '''
                temp = ufmdb_util.convert_ranges_to_list(kk)
                ret["allowed_vlans"] = temp

        return ret


class RedfishPortCollectionBackend():
    def __init__(self):
        self.cfg = {
            '@odata.id': '{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{Ports}',
            '@odata.type': '#PortCollection.PortCollection',
            'Description': 'Collection of Ports',
            'Name': 'Port Collection'
            }

    def get(self, fab_id, sw_id):
        try:
            if ufmdb_util.is_valid_fabric(fab_id) and ufmdb_util.is_valid_switch(sw_id):
                self.cfg['@odata.id'] = self.cfg['@odata.id'].format(rest_base=redfish_constants.REST_BASE,
                                                                     Fabrics=redfish_constants.FABRICS,
                                                                     fab_id=fab_id,
                                                                     Switches=redfish_constants.SWITCHES,
                                                                     switch_id=sw_id,
                                                                     Ports=redfish_constants.PORTS)
                members = []
                ports = self.get_ports_for_switch(sw_id)
                for p in ports:
                    members.append({'@odata.id': self.cfg['@odata.id'] + '/' + p})

                self.cfg['Members'] = members
                self.cfg['Members@odata.count'] = len(members)

                response = self.cfg, redfish_constants.SUCCESS
            else:
                response = redfish_constants.NOT_FOUND

        except Exception as e:
            # print('Caught exc {} in RedfishPortCollectionBackend.get()'.format(e).format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def put(self, payload):
        pass

    # return a list of switch ids that belong to this fabric
    def get_ports_for_switch(self, sw_id):
        prefix = '/switches/' + sw_id + '/ports/list/'
        kv_dict = ufmdb_util.query_prefix(prefix)

        ret = []
        for k in kv_dict:
            ret.append(k.split('/')[-1])

        return ret
