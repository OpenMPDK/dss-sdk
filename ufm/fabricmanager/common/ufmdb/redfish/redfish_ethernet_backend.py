from os.path import join

from common.ufmdb.redfish import ufmdb_redfish_resource
from common.ufmdb.redfish.redfish_system_backend import RedfishNkvSystemBackend
from common.ufmlog import ufmlog
from rest_api.redfish import redfish_constants


class RedfishEthernetBackend:
    def get(self):
        # The derived classes need to override this method
        raise NotImplementedError

    def put(self, payload):
        # The derived classes need to override this method
        raise NotImplementedError

    @classmethod
    def create_instance(cls, sys_id, eth_id):
        (sys_type, entry) = ufmdb_redfish_resource.lookup_resource_in_db(
            redfish_constants.SYSTEMS, sys_id)
        if sys_type == 'essd':
            return RedfishEssdEthernetBackend(entry, eth_id)
        elif sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE,
                        redfish_constants.SYSTEMS, sys_id,
                        redfish_constants.ETH_INTERFACES, eth_id)
            # A bit strange but that is how NKV Redfish UFMDB is wired.
            # It's a single API entry point for all endpoints.
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError


class RedfishEssdEthernetBackend(RedfishEthernetBackend):
    def __init__(self, entry, eth_id):
        self.entry = entry
        self.eth_id = eth_id

    def get(self):
        key = join(self.entry['key'], redfish_constants.ETH_INTERFACES, self.eth_id)
        resp = ufmdb_redfish_resource.get_resource_value(key)
        ufmdb_redfish_resource.sub_resource_values(resp, '@odata.id', 'System.eSSD.1', self.entry['type_specific_data']['suuid'])
        return resp

    def put(self, payload):
        pass


class RedfishEthernetCollectionBackend:
    def get(self):
        # The derived classes need to override this method
        raise NotImplementedError

    def put(self, payload):
        # The derived classes need to override this method
        raise NotImplementedError

    @classmethod
    def create_instance(cls, sys_id):
        (sys_type, entry) = ufmdb_redfish_resource.lookup_resource_in_db(
            redfish_constants.SYSTEMS, sys_id)
        if sys_type == 'essd':
            return RedfishEssdEthernetCollectionBackend(entry)
        elif sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE,
                        redfish_constants.SYSTEMS, sys_id,
                        redfish_constants.ETH_INTERFACES)
            # A bit strange but that is how NKV Redfish UFMDB is wired.
            # It's a single API entry point for all endpoints.
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError


class RedfishEssdEthernetCollectionBackend(RedfishEthernetBackend):
    def __init__(self, entry):
        self.entry = entry

    def get(self):
        key = join(self.entry['key'], redfish_constants.ETH_INTERFACES)
        resp = ufmdb_redfish_resource.get_resource_value(key)
        ufmdb_redfish_resource.sub_resource_values(resp, '@odata.id', 'System.eSSD.1', self.entry['type_specific_data']['suuid'])
        return resp

    def put(self, payload):
        pass