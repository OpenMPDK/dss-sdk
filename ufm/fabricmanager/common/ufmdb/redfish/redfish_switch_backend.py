
from rest_api.redfish import redfish_constants
from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.ufmdb_util import ufmdb_util

class RedfishSwitchBackend():
    def __init__(self):
        self.cfg = {
            "@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}",
            "@odata.type": "#Switch.v1_3_1.Switch",
            "Id": "{switch_id}",
            "Description": "Ethernet Switch information",
            "Name": "Switch",
            "SerialNumber": "{serial_number}",
            "UUID":"{uuid}",
        }

    def get(self, fab_id, sw_id):
        try:
            self.cfg["@odata.id"] = self.cfg["@odata.id"].format(rest_base = redfish_constants.REST_BASE,
                                                                 Fabrics = redfish_constants.FABRICS,
                                                                 fab_id = fab_id,
                                                                 Switches = redfish_constants.SWITCHES,
                                                                 switch_id = sw_id)

            self.cfg["Id"] = self.cfg["Id"].format(switch_id = sw_id)

            sw = self.get_switch(fab_id, sw_id)
            self.cfg["SerialNumber"] = self.cfg["SerialNumber"].format(serial_number = sw["serial_number"])
            self.cfg["UUID"] = self.cfg["UUID"].format(uuid = sw["uuid"])

            if sw["ports"]:
                self.cfg["Ports"] = { "@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{Ports}" }
                self.cfg["Ports"]["@odata.id"] = self.cfg["Ports"]["@odata.id"].format(rest_base = redfish_constants.REST_BASE,
                                                                                       Fabrics = redfish_constants.FABRICS,
                                                                                       fab_id = fab_id,
                                                                                       Switches = redfish_constants.SWITCHES,
                                                                                       switch_id = sw_id,
                                                                                       Ports = redfish_constants.PORTS)
            if sw["vlans"]:
                self.cfg["VLANs"] = { "@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{VLANs}" }
                self.cfg["VLANs"]["@odata.id"] = self.cfg["VLANs"]["@odata.id"].format(rest_base = redfish_constants.REST_BASE,
                                                                                       Fabrics = redfish_constants.FABRICS,
                                                                                       fab_id = fab_id,
                                                                                       Switches = redfish_constants.SWITCHES,
                                                                                       switch_id = sw_id,
                                                                                       VLANs = redfish_constants.VLANS)

            response = self.cfg, redfish_constants.SUCCESS

        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response()
        return response


    def put(self, payload):
        pass


    # return the attributes for a given switch
    def get_switch(self, fab_id, sw_id):
        prefix = "/switches/" + sw_id + "/switch_attributes"
        kv_dict = ufmdb_util.query_prefix(prefix)
        ret = {}
        for k in kv_dict:
            key = k.split("/")[-2]
            val = k.split("/")[-1]
            ret[key] = val

        prefix = "/switches/" + sw_id + "/ports/list"
        kv_dict = ufmdb_util.query_prefix(prefix)
        ret["ports"] = []
        for k in kv_dict:
            port_id = k.split("/")[-1]
            ret["ports"].append(port_id)

        prefix = "/switches/" + sw_id + "/VLANs/list"
        kv_dict = ufmdb_util.query_prefix(prefix)
        ret["vlans"] = []
        for k in kv_dict:
            vlan_id = k.split("/")[-1]
            ret["vlans"].append(vlan_id)
        return ret



class RedfishSwitchCollectionBackend():
    def __init__(self):
        self.cfg = {
            '@odata.id': '{rest_base}/{Fabrics}/{fab_id}/{Switches}',
            '@odata.type': '#SwitchCollection.SwitchCollection',
            'Description': 'Collection of Switches',
            'Name': 'Ethernet Switches Collection'
            }


    def get(self, fab_id):
        try:
            if ufmdb_util.is_valid_fabric(fab_id):
                self.cfg['@odata.id'] = self.cfg['@odata.id'].format(rest_base = redfish_constants.REST_BASE,
                                                                     Fabrics = redfish_constants.FABRICS,
                                                                     fab_id = fab_id,
                                                                     Switches = redfish_constants.SWITCHES)
                members = []
                switches = self.get_switches_for_fabric(fab_id)
                for sw in switches:
                    members.append({'@odata.id': self.cfg['@odata.id'] + '/' + sw})

                self.cfg['Members'] = members
                self.cfg['Members@odata.count'] = len(members)
                response = self.cfg, redfish_constants.SUCCESS

            else:
                response = redfish_constants.NOT_FOUND
        except Exception:
            response = RedfishErrorResponse.get_server_error_response()
        return response


    def put(self, payload):
        pass


    # return a list of switch ids that belong to this fabric
    def get_switches_for_fabric(self,fab_id):
        prefix = '/' + redfish_constants.FABRICS + '/' + fab_id + '/list'
        kv_dict = ufmdb_util.query_prefix(prefix)

        ret = []
        for k in kv_dict:
            ret.append(k.split('/')[-1])

        return ret






