from os.path import join

from common.ufmdb.redfish import ufmdb_redfish_resource
from common.ufmdb.redfish.redfish_ufmdb import RedfishUfmdb
from common.ufmlog import ufmlog
from rest_api.redfish import redfish_constants


class RedfishSystemBackend:
    def get(self):
        # The derived classes need to override this method
        raise NotImplementedError

    def put(self, payload):
        # The derived classes need to override this method
        raise NotImplementedError

    @classmethod
    def create_instance(cls, sys_type, entry, ident):
        if sys_type == 'essd':
            return RedfishEssdSystemBackend(entry)
        elif sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE, 'Systems', ident)
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError


class RedfishEssdSystemBackend(RedfishSystemBackend):
    def __init__(self, entry):
        self.entry = entry

    def get(self):
        resp = ufmdb_redfish_resource.get_resource_value(self.entry['key'])
        ufmdb_redfish_resource.sub_resource_values(resp, '@odata.id', 'System.eSSD.1', self.entry['type_specific_data']['suuid'])
        ufmdb_redfish_resource.sub_resource_values(resp, 'Id', 'System.eSSD.1', self.entry['type_specific_data']['suuid'])
        ufmdb_redfish_resource.sub_resource_values(resp, '@odata.id', 'Chassis/1', 'Chassis/' + self.entry['type_specific_data']['suuid'])
        return resp

    def put(self, payload):
        pass


class RedfishNkvSystemBackend(RedfishSystemBackend):
    def __init__(self, path):
        self.rfdb = RedfishUfmdb(auto_update=True)
        self.log = ufmlog.log(module="RFDB", mask=ufmlog.UFM_REDFISH_DB)
        self.log.detail("UfmdbSystemAPI started.")
        self.path = path

    def __del__(self):
        self.log.detail("UfmdbSystemAPI stopped.")

    def get(self):
        return self.rfdb.get(self.path, "{}")

    def put(self, payload):
        return self.rfdb.post(self.path, payload)


class RedfishCollectionBackend:
    def __init__(self, collection):
        self.collection = collection

    def get(self):
        resp = self.get_response_base()
        resources = ufmdb_redfish_resource.get_resources_list(self.collection)
        if resources:
            resp['Members'] = resp['Members'] + resources
            resp['Members@odata.count'] = resp['Members@odata.count'] + len(resources)
        return resp

    @staticmethod
    def get_response_base():
        # The derived classes need to override this method
        raise NotImplementedError

    @classmethod
    def create_instance(cls, collection):
        if collection == 'Systems':
            return RedfishSystemCollectionBackend()
        else:
            raise NotImplementedError


class RedfishSystemCollectionBackend(RedfishCollectionBackend):

    def __init__(self):
        super().__init__('Systems')
        self.path = join(redfish_constants.REST_BASE, 'Systems')

    def get(self):
        # Get the nkv systems
        resp = self.get_response_base()
        nkv_sys_backend = RedfishNkvSystemBackend(self.path)
        nkv_resp = nkv_sys_backend.get()
        if nkv_resp:
            # Update the Members and count
            resp['Members'] = resp['Members'] + nkv_resp['Members']
            resp['Members@odata.count'] = resp['Members@odata.count'] + len(nkv_resp['Members'])
        # Get All other systems from the lookup prefix
        systems = ufmdb_redfish_resource.get_resources_list('Systems')
        if systems:
            # Update the Members and count
            resp['Members'] = resp['Members'] + systems
            resp['Members@odata.count'] = resp['Members@odata.count'] + len(systems)

        return resp

    @staticmethod
    def get_response_base():
        return {
            '@odata.context': redfish_constants.REST_BASE + '$metadata#ComputerSystemCollection.ComputerSystemCollection',
            '@odata.id': redfish_constants.REST_BASE + 'Systems',
            '@odata.type': '#ComputerSystemCollection.ComputerSystemCollection',
            'Description': 'Collection of Computer Systems',
            'Members': [],
            'Members@odata.count': 0,
            'Name': 'System Collection'
        }

    def post(self):
        raise NotImplementedError

