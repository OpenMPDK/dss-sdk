# External imports
import traceback

# Flask imports
from flask_restful import reqparse, Api, Resource

# Internal imports
import config
from .templates.Storage import get_storage_instance

members = {}

SERVER_ERROR = 500
NOT_FOUND = 404
SUCCESS = 200


class Storage(Resource):
    """
    Storage
    """

    def __init__(self, **kwargs):
        pass

    def get(self, ident1, ident2):
        """
        HTTP GET
        """
        if ident1 not in members:
            return 'Client Error: Not Found', NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', NOT_FOUND

        return members[ident1][ident2], SUCCESS


class StorageCollection(Resource):
    """
    Storage Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#StorageCollection.StorageCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#StorageCollection.StorageCollection',
            'Description': 'Collection of Storage information',
            'Name': 'Storage Collection',
            "oem": {
                "CapacityBytes": 107541167505408,
                "PercentAvailable": 99
            }
        }

    def get(self, ident):
        """
        HTTP GET
        """
        try:
            if ident not in members:
                return NOT_FOUND
            storage = []
            for store in members.get(ident, {}).values():
                storage.append({'@odata.id': store['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident + '/Storage'
            self.cfg['Members'] = storage
            self.cfg['Members@odata.count'] = len(storage)
            response = self.cfg, SUCCESS
        except Exception:
            traceback.print_exc()
            response = SERVER_ERROR

        return response


def CreateStorage(**kwargs):
    sys_id = kwargs['sys_id']
    storage_id = kwargs['storage_id']
    if sys_id not in members:
        members[sys_id] = {}
    members[sys_id][storage_id] = get_storage_instance(kwargs)
