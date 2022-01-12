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
import uuid

# Flask imports
from flask_restful import request, Resource

# Internal imports
from rest_api.redfish.templates.Switch import get_switch_instance
from rest_api.redfish import redfish_constants
from rest_api.redfish import util

from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.redfish_switch_backend import RedfishSwitchBackend, RedfishSwitchCollectionBackend

members = {}


class SwitchAPI(Resource):

    def get(self, fab_id, sw_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishSwitchBackend()
            response = redfish_backend.get(fab_id, sw_id)
        except Exception as e:
            # print('Caught exc {} in SwitchAPI.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self):
        raise NotImplementedError


class SwitchActionAPI(Resource):
    def get(self):
        raise NotImplementedError

    def post(self, fab_id, sw_id, act_str):
        try:
            data = {
                    'cmd': act_str,
                    'request_id': str(uuid.uuid4()),
                    'sw_id': sw_id
                   }
            payload = request.get_json(force=True)
            data.update(payload)

            resp = util.post_to_switch(sw_id, data)

        except Exception as e:
            # print('Caught exc {} in SwitchActionAPI.post()'.format(e))
            resp = RedfishErrorResponse.get_server_error_response(e)

        return resp


class SwitchCollectionAPI(Resource):

    def get(self, fab_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishSwitchCollectionBackend()
            response = redfish_backend.get(fab_id)
        except Exception as e:
            # print('Caught exc {} in SwitchCollectionAPI.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self):
        raise NotImplementedError


class SwitchEmulationAPI(Resource):
    """
    Switch
    """

    def __init__(self, **kwargs):
        pass

    def get(self, ident1, ident2):
        # """
        # HTTP GET
        # """
        if ident1 not in members:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND

        return members[ident1][ident2], redfish_constants.SUCCESS


class SwitchCollectionEmulationAPI(Resource):
    """
    Switch Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#SwitchCollection.SwitchCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#SwitchCollection.SwitchCollection',
            'Description': 'Collection of Switches',
            'Name': 'Ethernet Switches Collection'
        }

    def get(self, ident):
        """
        HTTP GET
        """
        try:
            if ident not in members:
                return redfish_constants.NOT_FOUND
            switches = []
            for switch in members.get(ident, {}).values():
                switches.append({'@odata.id': switch['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident + '/Switches'
            self.cfg['Members'] = switches
            self.cfg['Members@odata.count'] = len(switches)
            response = self.cfg, redfish_constants.SUCCESS
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response


def CreateSwitchEmulation(**kwargs):
    fab_id = kwargs['fab_id']
    switch_id = kwargs['switch_id']
    if fab_id not in members:
        members[fab_id] = {}
    members[fab_id][switch_id] = get_switch_instance(kwargs)
