
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
            "UUID": "{uuid}",
            "Oem": {
                "PFC": "{pfc_status}",
                "PriorityEnabledList": [],
                "PriorityDisabledList": []
            }
        }

    def get(self, fab_id, sw_id):
        try:
            self.cfg["@odata.id"] = self.cfg["@odata.id"].format(rest_base=redfish_constants.REST_BASE,
                                                                 Fabrics=redfish_constants.FABRICS,
                                                                 fab_id=fab_id,
                                                                 Switches=redfish_constants.SWITCHES,
                                                                 switch_id=sw_id)

            self.cfg["Id"] = self.cfg["Id"].format(switch_id=sw_id)

            sw = self.get_switch(fab_id, sw_id)
            self.cfg["SerialNumber"] = self.cfg["SerialNumber"].format(serial_number=sw["serial_number"])
            self.cfg["UUID"] = self.cfg["UUID"].format(uuid=sw["uuid"])
            self.cfg["Oem"]["PFC"] = self.cfg["Oem"]["PFC"].format(pfc_status=sw["pfc_status"])
            self.cfg["Oem"]["PriorityEnabledList"] = sw["prio_enabled_list"]
            self.cfg["Oem"]["PriorityDisabledList"] = sw["prio_disabled_list"]

            ###############################
            self.cfg['Actions'] = {}
            self.cfg['Actions']['#EnablePfcGlobally'] = {}
            self.cfg['Actions']['#EnablePfcGlobally']['description'] = \
                'Enables priority flow control globally on the switch.'
            self.cfg['Actions']['#EnablePfcGlobally']['target'] = self.cfg['@odata.id'] + '/Actions/EnablePfcGlobally'

            ###############################
            self.cfg['Actions']['#DisablePfcGlobally'] = {}
            self.cfg['Actions']['#DisablePfcGlobally']['description'] = \
                'Disables priority flow control globally on the switch.'
            self.cfg['Actions']['#DisablePfcGlobally']['target'] = self.cfg['@odata.id'] + '/Actions/DisablePfcGlobally'

            ###############################
            self.cfg['Actions']['#EnablePfcPerPriority'] = {}
            self.cfg['Actions']['#EnablePfcPerPriority']['description'] = \
                'Enables PFC per priority on the switch.'
            self.cfg['Actions']['#EnablePfcPerPriority']['target'] = \
                self.cfg['@odata.id'] + '/Actions/EnablePfcPerPriority'
            self.cfg['Actions']['#EnablePfcPerPriority']['Parameters'] = []

            param = {}
            param['Name'] = 'Priority'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '0'
            param['MaximumValue'] = '7'
            self.cfg['Actions']['#EnablePfcPerPriority']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#DisablePfcPerPriority'] = {}
            self.cfg['Actions']['#DisablePfcPerPriority']['description'] = \
                'Disables PFC per priority on the switch.'
            self.cfg['Actions']['#DisablePfcPerPriority']['target'] = \
                self.cfg['@odata.id'] + '/Actions/DisablePfcPerPriority'
            self.cfg['Actions']['#DisablePfcPerPriority']['Parameters'] = []

            param = {}
            param['Name'] = 'Priority'
            param['Required'] = True
            param['DataType'] = 'Number'
            param['MinimumValue'] = '0'
            param['MaximumValue'] = '7'
            self.cfg['Actions']['#DisablePfcPerPriority']['Parameters'].append(param)

            ###############################
            self.cfg['Actions']['#AnyCmd'] = {}
            self.cfg['Actions']['#AnyCmd']['description'] = \
                'Send any cmd directly to the switch.'
            self.cfg['Actions']['#AnyCmd']['target'] = self.cfg['@odata.id'] + '/Actions/AnyCmd'
            self.cfg['Actions']['#AnyCmd']['Parameters'] = []

            param = {}
            param['Name'] = 'AnyCmdStr'
            param['Required'] = True
            self.cfg['Actions']['#AnyCmd']['Parameters'].append(param)

            if sw["ports"]:
                self.cfg["Ports"] = {"@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{Ports}"}
                self.cfg["Ports"]["@odata.id"] = self.cfg["Ports"]["@odata.id"].format(
                    rest_base=redfish_constants.REST_BASE,
                    Fabrics=redfish_constants.FABRICS,
                    fab_id=fab_id,
                    Switches=redfish_constants.SWITCHES,
                    switch_id=sw_id,
                    Ports=redfish_constants.PORTS)

            if sw["vlans"]:
                self.cfg["VLANs"] = {"@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}/{switch_id}/{VLANs}"}
                self.cfg["VLANs"]["@odata.id"] = self.cfg["VLANs"]["@odata.id"].format(
                    rest_base=redfish_constants.REST_BASE,
                    Fabrics=redfish_constants.FABRICS,
                    fab_id=fab_id,
                    Switches=redfish_constants.SWITCHES,
                    switch_id=sw_id,
                    VLANs=redfish_constants.VLANS)

            response = self.cfg, redfish_constants.SUCCESS

        except Exception as e:
            # print('Caught exc {e} in RedfishSwitchBackend.get()')
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def put(self, payload):
        pass

    # return the attributes for a given switch
    def get_switch(self, fab_id, sw_id):
        ret = {}

        prefix = "/switches/" + sw_id + "/switch_attributes/"
        kv_dict = ufmdb_util.query_prefix(prefix)
        ret["prio_enabled_list"] = []
        ret["prio_disabled_list"] = []
        for k in kv_dict:
            key = k.split("/")[-2]
            val = k.split("/")[-1]

            if key == "priority_enabled_list":
                ret["prio_enabled_list"].append(val)
            elif key == "priority_disabled_list":
                ret["prio_disabled_list"].append(val)
            else:
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
                self.cfg['@odata.id'] = self.cfg['@odata.id'].format(rest_base=redfish_constants.REST_BASE,
                                                                     Fabrics=redfish_constants.FABRICS,
                                                                     fab_id=fab_id,
                                                                     Switches=redfish_constants.SWITCHES)
                members = []
                switches = self.get_switches_for_fabric(fab_id)
                for sw in switches:
                    members.append({'@odata.id': self.cfg['@odata.id'] + '/' + sw})

                self.cfg['Members'] = members
                self.cfg['Members@odata.count'] = len(members)
                response = self.cfg, redfish_constants.SUCCESS

            else:
                response = redfish_constants.NOT_FOUND
        except Exception as e:
            # print('Caught exc {e} in RedfishSwitchCollectionBackend.get()')
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def put(self, payload):
        pass

    # return a list of switch ids that belong to this fabric
    def get_switches_for_fabric(self, fab_id):
        prefix = '/' + redfish_constants.FABRICS + '/' + fab_id + '/list'
        kv_dict = ufmdb_util.query_prefix(prefix)

        ret = []
        for k in kv_dict:
            ret.append(k.split('/')[-1])

        return ret
