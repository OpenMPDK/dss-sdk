
from rest_api.redfish import redfish_constants
from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.ufmdb_util import ufmdb_util

class RedfishPortBackend():
    def __init__(self):
        self.cfg = {
            "@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{Ports}/{port_id}",
            "@odata.type": "#Port.v1_2_1.Port",
            "PortId": "{port_id}",
            "Description": "Port Interface",
            "Name": "Eth1/{port_id}",
            "Mode": "{port_mode}"
        }

    def get(self, fab_id, sw_id, port_id):
        try:
            self.cfg["@odata.id"] = self.cfg["@odata.id"].format(rest_base = redfish_constants.REST_BASE,
                                                                 Fabrics = redfish_constants.FABRICS,
                                                                 fab_id = fab_id,
                                                                 Switches = redfish_constants.SWITCHES,
                                                                 switch_id = sw_id,
                                                                 Ports = redfish_constants.PORTS,
                                                                 port_id = port_id)

            self.cfg["PortId"] = self.cfg["PortId"].format(port_id = port_id)
            self.cfg["Name"] = self.cfg["Name"].format(port_id = port_id)
            port = self.get_port(sw_id, port_id)
            self.cfg["Mode"] = self.cfg["Mode"].format(port_mode = port["mode"])

            self.cfg["Links"] = {"AccessVlan": [], "AllowedVlans": []}
            if port["access_vlan"]:
                vlan_path = "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{VLANs}/{vlan_id}".format(
                            rest_base = redfish_constants.REST_BASE,
                            Fabrics = redfish_constants.FABRICS,
                            fab_id = fab_id,
                            Switches = redfish_constants.SWITCHES,
                            switch_id = sw_id,
                            VLANs = redfish_constants.VLANS,
                            vlan_id = port["access_vlan"])

                self.cfg["Links"]["AccessVlan"].append({"@odata.id": vlan_path})

            if port["allowed_vlans"]:
                for vlan_id in port["allowed_vlans"]:
                    vlan_path = "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{VLANs}/{vlan_id}".format(
                                rest_base = redfish_constants.REST_BASE,
                                Fabrics = redfish_constants.FABRICS,
                                fab_id = fab_id,
                                Switches = redfish_constants.SWITCHES,
                                switch_id = sw_id,
                                VLANs = redfish_constants.VLANS,
                                vlan_id = vlan_id)
                    self.cfg["Links"]["AllowedVlans"].append({"@odata.id": vlan_path})

            response = self.cfg, redfish_constants.SUCCESS

        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response()
        return response

    def put(self, payload):
        pass


    def get_port(self, sw_id, port_id):
        ret = {"mode": "", "access_vlan": "", "allowed_vlans": []}
        pre = "/switches/" + sw_id + "/ports/" + str(port_id)

        prefix = pre + "/mode"
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            ret["mode"] = k.split('/')[-1]

        prefix = pre + "/network/access_vlan"
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            ret["access_vlan"] = k.split("/")[-1]

        prefix = pre + "/network/allowed_vlans"
        kv_dict = ufmdb_util.query_prefix(prefix)
        for k in kv_dict:
            kk = k.split("/")[-1]
            if kk: #non-empty string
                ret["allowed_vlans"].append(kk)

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
                self.cfg['@odata.id'] = self.cfg['@odata.id'].format(rest_base = redfish_constants.REST_BASE,
                                                                     Fabrics = redfish_constants.FABRICS,
                                                                     fab_id = fab_id,
                                                                     Switches = redfish_constants.SWITCHES,
                                                                     switch_id = sw_id,
                                                                     Ports = redfish_constants.PORTS)
                members = []
                ports = self.get_ports_for_switch(sw_id)
                for p in ports:
                    members.append({'@odata.id': self.cfg['@odata.id'] + '/' + p})

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
    def get_ports_for_switch(self,sw_id):
        prefix = '/switches/' + sw_id + '/ports/list'
        kv_dict = ufmdb_util.query_prefix(prefix)

        ret = []
        for k in kv_dict:
            ret.append(k.split('/')[-1])

        return ret






