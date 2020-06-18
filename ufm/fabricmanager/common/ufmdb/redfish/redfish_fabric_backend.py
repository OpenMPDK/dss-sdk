
from rest_api.redfish import redfish_constants
from common.ufmdb.redfish.ufmdb_util import ufmdb_util

class RedfishFabricBackend():
    def __init__(self):
        self.cfg = {
            "@odata.id": "{rest_base}/{Fabrics}/{fab_id}",
            "@odata.type": "#Fabric.v1_1_1.Fabric",
            "Id": "{fab_id}",
            "Description": "Fabric information",
            "Name": "Fabric",
            "FabricType": "{fab_type}",
        }

    def get(self, fab_id):
        try:
            if ufmdb_util.is_valid_fabric(fab_id):
                self.cfg["@odata.id"] = self.cfg["@odata.id"].format(rest_base = redfish_constants.REST_BASE,
                                                                     Fabrics = redfish_constants.FABRICS,
                                                                     fab_id = fab_id)

                self.cfg["Id"] = self.cfg["Id"].format(fab_id = fab_id)

                fab = self.get_fabric(fab_id)
                self.cfg["FabricType"] = self.cfg["FabricType"].format(fab_type = fab["type"])

                if fab["switches"]:
                    self.cfg["Switches"] = { "@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}" }
                    self.cfg["Switches"]["@odata.id"] = self.cfg["Switches"]["@odata.id"].format(
                                                        rest_base = redfish_constants.REST_BASE,
                                                        Fabrics = redfish_constants.FABRICS,
                                                        fab_id = fab_id,
                                                        Switches = redfish_constants.SWITCHES)

                response = self.cfg, redfish_constants.SUCCESS
            else:
                response = redfish_constants.NOT_FOUND
        except Exception as e:
            self.log.exception(e)
            response = {"Status": redfish_constants.SERVER_ERROR,
                        "Message": "Internal Server Error"}
        return response

    def put(self, payload):
        pass


    def get_fabric(self, fab_id):
        prefix = "/Fabrics/" + fab_id + "/type"
        kv_dict = ufmdb_util.query_prefix(prefix)

        ret = {}
        for k in kv_dict:
            key = k.split("/")[-2]
            val = k.split("/")[-1]
            ret[key] = val

        prefix = "/Fabrics/" + fab_id + "/list"
        kv_dict = ufmdb_util.query_prefix(prefix)
        ret["switches"] = []
        for k in kv_dict:
            sw_id = k.split("/")[-1]
            ret["switches"].append(sw_id)

        return ret



class RedfishFabricCollectionBackend():
    def __init__(self):
        self.cfg = {
            '@odata.id': '{rest_base}/{Fabrics}',
            '@odata.type': '#FabricCollection.FabricCollection',
            'Description': 'Collection of Fabrics',
            'Name': 'Fabric Collection'
            }


    def get(self):
        try:
            self.cfg['@odata.id'] = self.cfg['@odata.id'].format(rest_base = redfish_constants.REST_BASE,
                                                                 Fabrics = redfish_constants.FABRICS)

            members = []
            fabrics = ufmdb_util.query_prefix('/Fabrics/list')
            for fab in fabrics:
                fab_id = fab.split('/')[-1]
                members.append({'@odata.id': self.cfg['@odata.id'] + '/' + fab_id})

            self.cfg['Members'] = members
            self.cfg['Members@odata.count'] = len(members)
            response = self.cfg, redfish_constants.SUCCESS

        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR

        return response


    def put(self, payload):
        pass








