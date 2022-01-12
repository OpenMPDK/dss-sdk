# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


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
