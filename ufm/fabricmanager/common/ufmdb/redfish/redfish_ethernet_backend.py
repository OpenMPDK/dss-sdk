from os.path import join

from common.ufmdb.redfish import ufmdb_redfish_resource
from common.ufmdb.redfish.redfish_system_backend import RedfishNkvSystemBackend

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
        if sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE,
                        redfish_constants.SYSTEMS, sys_id,
                        redfish_constants.ETH_INTERFACES, eth_id)
            # A bit strange but that is how NKV Redfish UFMDB is wired.
            # It's a single API entry point for all endpoints.
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError


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
        if sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE,
                        redfish_constants.SYSTEMS, sys_id,
                        redfish_constants.ETH_INTERFACES)
            # A bit strange but that is how NKV Redfish UFMDB is wired.
            # It's a single API entry point for all endpoints.
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError
