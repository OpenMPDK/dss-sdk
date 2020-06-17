# External imports
import traceback

# Flask imports
from flask import request
from flask_restful import reqparse, Api, Resource

# Internal imports
import config
from common.ufmdb.redfish.redfish_ethernet_backend import RedfishEthernetBackend
from common.ufmdb.redfish.redfish_storage_backend import RedfishStorageCollectionBackend, RedfishStorageBackend
from .redfish_error_response import RedfishErrorResponse
from .templates.Storage import get_storage_instance
from rest_api.redfish import redfish_constants

members = {}


class StorageAPI(Resource):
    def __init__(self):
        self.rf_err_resp = RedfishErrorResponse()

    def get(self, sys_id, storage_id):
        try:
            redfish_backend = RedfishStorageBackend.create_instance(
                sys_id, storage_id)
            response = redfish_backend.get()
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response

    def post(self, sys_id, storage_id):
        try:
            payload = request.get_json(force=True)
            redfish_backend = RedfishStorageBackend.create_instance(
                sys_id, storage_id)
            response = redfish_backend.put(payload)
        except NotImplementedError as e:
            response = self.rf_err_resp.get_method_not_allowed_response('Storage: ' + storage_id)
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response


class StorageCollectionAPI(Resource):
    def __init__(self):
        self.rf_err_resp = RedfishErrorResponse()

    def get(self, sys_id):
        try:
            redfish_backend = RedfishStorageCollectionBackend.create_instance(
                sys_id)
            response = redfish_backend.get()
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response

    def post(self):
        return self.rf_err_resp.get_method_not_allowed_response('Storage')


class StorageEmulationAPI(Resource):
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
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND

        return members[ident1][ident2], redfish_constants.SUCCESS


class StorageCollectionEmulationAPI(Resource):
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
        }

    def get(self, ident):
        """
        HTTP GET
        """
        try:
            if ident not in members:
                return redfish_constants.NOT_FOUND
            storage = []
            for store in members.get(ident, {}).values():
                storage.append({'@odata.id': store['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident + '/Storage'
            self.cfg['Members'] = storage
            self.cfg['Members@odata.count'] = len(storage)
            self.cfg['oem'] = {"CapacityBytes": 107541167505408,
                               "UtilizationBytes": 1075411675054,
                               "PercentAvailable": 99
                               }
            response = self.cfg, redfish_constants.SUCCESS
        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR

        return response


def CreateStorageEmulation(**kwargs):
    sys_id = kwargs['sys_id']
    storage_id = kwargs['storage_id']
    if sys_id not in members:
        members[sys_id] = {}
    members[sys_id][storage_id] = get_storage_instance(kwargs)
