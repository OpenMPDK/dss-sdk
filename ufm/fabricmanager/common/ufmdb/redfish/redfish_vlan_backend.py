
from rest_api.redfish import redfish_constants
from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.ufmdb_util import ufmdb_util

class RedfishVlanBackend():
    def __init__(self):
        self.cfg = {
            "@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{VLANs}/{vlan_id}",
            "@odata.type": "#VLanNetworkInterface.v1_1_5.VLanNetworkInterface",
            "Id": "{vlan_id}",
            "Description": "VLAN Network Interface",
            "Name": "{vlan_name}",
            "VLANEnable": "{vlan_enabled}",
            "VLANId":"{vlan_id}",
        }

    def get(self, fab_id, sw_id, vlan_id):
        try:
            self.cfg["@odata.id"] = self.cfg["@odata.id"].format(rest_base = redfish_constants.REST_BASE,
                                                                 Fabrics = redfish_constants.FABRICS,
                                                                 fab_id = fab_id,
                                                                 Switches = redfish_constants.SWITCHES,
                                                                 switch_id = sw_id,
                                                                 VLANs = redfish_constants.VLANS,
                                                                 vlan_id = vlan_id)
            self.cfg['Id'] = self.cfg['Id'].format(vlan_id = vlan_id)
            self.cfg['VLANEnable'] = self.cfg['VLANEnable'].format(vlan_enabled = True)
            self.cfg['VLANId'] = self.cfg['VLANId'].format(vlan_id = vlan_id)

            vlan = self.get_vlan(sw_id, vlan_id)
            self.cfg['Name'] = self.cfg['Name'].format(vlan_name = vlan['name'])

            self.cfg['Actions'] = {}
            self.cfg['Actions']['#DeleteVLAN'] = {}
            self.cfg['Actions']['#DeleteVLAN']['description'] = 'Delete VLAN'
            self.cfg['Actions']['#DeleteVLAN']['target'] = self.cfg['@odata.id'] + '/Actions/DeleteVLAN'

            port_links = []
            for p in vlan['ports']:
                port_id = p.split('/')[-1]
                port_path = '{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{Ports}/{port_id}'.format(
                    rest_base = redfish_constants.REST_BASE,
                    Fabrics = redfish_constants.FABRICS,
                    fab_id = fab_id,
                    Switches = redfish_constants.SWITCHES,
                    switch_id = sw_id,
                    Ports = redfish_constants.PORTS,
                    port_id = port_id)

                port_links.append({'@odata.id': port_path})

            self.cfg['Links'] = {}
            self.cfg['Links']['Ports'] = port_links
            response = self.cfg, redfish_constants.SUCCESS

        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response()
        return response

    def put(self, payload):
        pass


    def get_vlan(self, sw_id, vlan_id):
        ret = {}
        ret['ports'] = []

        prefix = '/switches/' + sw_id + '/VLANs/' + vlan_id
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            key = k.split("/")[-2]
            val = k.split("/")[-1]

            if key == 'ports':
                ret['ports'].append(val)
            else:
                ret[key] = val

        return ret



class RedfishVlanCollectionBackend():
    def __init__(self):
        self.cfg = {
            '@odata.id': '{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{VLANs}',
            '@odata.type': '#VLanCollection.VLanCollection',
            'Description': 'Collection of VLANs',
            'Name': 'VLANs Collection'
            }


    def get(self, fab_id, sw_id):
        try:
            if ufmdb_util.is_valid_fabric(fab_id) and ufmdb_util.is_valid_switch(sw_id):
                self.cfg['@odata.id'] = self.cfg['@odata.id'].format(rest_base = redfish_constants.REST_BASE,
                                                                     Fabrics = redfish_constants.FABRICS,
                                                                     fab_id = fab_id,
                                                                     Switches = redfish_constants.SWITCHES,
                                                                     switch_id = sw_id,
                                                                     VLANs = redfish_constants.VLANS)
                members = []
                vlans = self.get_vlans_for_switch(sw_id)
                for vlan_id in vlans:
                    members.append({'@odata.id': self.cfg['@odata.id'] + '/' + vlan_id})

                self.cfg['Members'] = members
                self.cfg['Members@odata.count'] = len(members)

                self.cfg['Actions'] = {}
                self.cfg['Actions']['#CreateVLAN'] = {}
                self.cfg['Actions']['#CreateVLAN']['description'] = 'Create a VLAN with Id'
                self.cfg['Actions']['#CreateVLAN']['target'] = self.cfg['@odata.id'] + '/Actions/CreateVLAN'
                self.cfg['Actions']['#CreateVLAN']['Parameters'] = []

                param = {}
                param['Name'] = 'VLANId'
                param['Required'] = True
                param['DataType'] = 'Number'
                param['MinimumValue'] = '1'
                param['MaximumValue'] = '4094'
                self.cfg['Actions']['#CreateVLAN']['Parameters'].append(param)

                response = self.cfg, redfish_constants.SUCCESS
            else:
                response = redfish_constants.NOT_FOUND
        except Exception:
            response = RedfishErrorResponse.get_server_error_response()
        return response


    def put(self, payload):
        pass

    def get_vlans_for_switch(self,sw_id):
        prefix = '/switches/' + sw_id + '/VLANs/list'
        kv_dict = ufmdb_util.query_prefix(prefix)

        ret = []
        for k in kv_dict:
            ret.append(k.split('/')[-1])

        return ret







