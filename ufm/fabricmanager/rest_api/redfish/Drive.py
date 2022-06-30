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
from common.ufmdb.redfish.redfish_drive_backend import RedfishDriveCollectionBackend, RedfishDriveBackend
from .redfish_error_response import RedfishErrorResponse
from .templates.Drive import get_drive_instance
from rest_api.redfish import redfish_constants

members = {}


class DriveAPI(Resource):
    def __init__(self):
        self.rf_err_resp = RedfishErrorResponse()

    def get(self, sys_id, storage_id, drive_id):
        try:
            redfish_backend = RedfishDriveBackend.create_instance(
                sys_id, storage_id, drive_id)
            response = redfish_backend.get()
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response

    def post(self, sys_id, storage_id, drive_id):
        try:
            payload = request.get_json(force=True)
            redfish_backend = RedfishDriveBackend.create_instance(
                sys_id, storage_id, drive_id)
            response = redfish_backend.put(payload)
        except NotImplementedError as e:
            response = self.rf_err_resp.get_method_not_allowed_response('Drive: ' + drive_id)
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response


class DriveCollectionAPI(Resource):
    def __init__(self):
        self.rf_err_resp = RedfishErrorResponse()

    def get(self, sys_id, storage_id):
        try:
            redfish_backend = RedfishDriveCollectionBackend.create_instance(
                sys_id, storage_id)
            response = redfish_backend.get()
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response

    def post(self):
        return self.rf_err_resp.get_method_not_allowed_response('Drives')


class DriveEmulationAPI(Resource):
    """
    Drive
    """

    def __init__(self, **kwargs):
        pass

    def get(self, ident1, ident2, ident3):
        """
        HTTP GET
        """
        if ident1 not in members:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident3 not in members[ident1][ident2]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND

        return members[ident1][ident2][ident3], redfish_constants.SUCCESS


class DriveCollectionEmulationAPI(Resource):
    """
    Drive Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#DriveCollection.DriveCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#DriveCollection.DriveCollection',
            'Description': 'Collection of drives',
            'Name': 'Drive Collection'
        }

    def get(self, ident1, ident2):
        """
        HTTP GET
        """
        try:
            if ident1 not in members:
                return redfish_constants.NOT_FOUND
            if ident2 not in members[ident1]:
                return redfish_constants.NOT_FOUND
            drives = []
            for drive in members[ident1].get(ident2, {}).values():
                drives.append({'@odata.id': drive['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident1 + '/Storage/' + ident2 + '/Drives'
            self.cfg['Members'] = drives
            self.cfg['Members@odata.count'] = len(drives)
            response = self.cfg, redfish_constants.SUCCESS
        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR

        return response


def CreateDriveEmulation(**kwargs):
    sys_id = kwargs['sys_id']
    storage_id = kwargs['storage_id']
    drive_id = kwargs['drive_id']
    if sys_id not in members:
        members[sys_id] = {}
    if storage_id not in members[sys_id]:
        members[sys_id][storage_id] = {}
    members[sys_id][storage_id][drive_id] = get_drive_instance(kwargs)
