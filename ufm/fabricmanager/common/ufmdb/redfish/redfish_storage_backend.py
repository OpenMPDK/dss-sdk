from os.path import join

from common.ufmdb.redfish import ufmdb_redfish_resource
from common.ufmdb.redfish.redfish_system_backend import RedfishNkvSystemBackend
from rest_api.redfish import redfish_constants


class RedfishStorageBackend:
    def get(self):
        # The derived classes need to override this method
        raise NotImplementedError

    def put(self, payload):
        # The derived classes need to override this method
        raise NotImplementedError

    @classmethod
    def create_instance(cls, sys_id, storage_id):
        (sys_type, entry) = ufmdb_redfish_resource.lookup_resource_in_db(
            redfish_constants.SYSTEMS, sys_id)
        if sys_type == 'essd':
            return RedfishEssdStorageBackend(entry, storage_id)
        elif sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE,
                        redfish_constants.SYSTEMS, sys_id,
                        redfish_constants.STORAGE, storage_id)
            # A bit strange but that is how NKV Redfish UFMDB is wired.
            # It's a single API entry point for all endpoints.
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError


class RedfishEssdStorageBackend(RedfishStorageBackend):
    def __init__(self, entry, storage_id):
        self.entry = entry
        self.storage_id = storage_id

    def get(self):
        key = join(self.entry['key'], redfish_constants.STORAGE, self.storage_id)
        resp = ufmdb_redfish_resource.get_resource_value(key)
        ufmdb_redfish_resource.sub_resource_values(resp, '@odata.id', 'System.eSSD.1', self.entry['type_specific_data']['suuid'])
        return resp

    def put(self, payload):
        pass


class RedfishStorageCollectionBackend:
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
            return RedfishEssdStorageCollectionBackend(entry)
        elif sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE,
                        redfish_constants.SYSTEMS, sys_id,
                        redfish_constants.STORAGE)
            # A bit strange but that is how NKV Redfish UFMDB is wired.
            # It's a single API entry point for all endpoints.
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError


class RedfishEssdStorageCollectionBackend(RedfishStorageCollectionBackend):
    def __init__(self, entry):
        self.entry = entry

    def get(self):
        key = join(self.entry['key'], redfish_constants.STORAGE)
        resp = ufmdb_redfish_resource.get_resource_value(key)
        ufmdb_redfish_resource.sub_resource_values(resp, '@odata.id', 'System.eSSD.1', self.entry['type_specific_data']['suuid'])
        return resp

    def put(self, payload):
        pass