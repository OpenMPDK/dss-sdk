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
from rest_api.redfish.templates.VLAN import get_vlan_instance
from rest_api.redfish import redfish_constants
from rest_api.redfish import util

from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.redfish_vlan_backend import RedfishVlanBackend, RedfishVlanCollectionBackend

members = {}


class VlanAPI(Resource):
    def get(self, fab_id, sw_id, vlan_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishVlanBackend()
            response = redfish_backend.get(fab_id, sw_id, vlan_id)
        except Exception as e:
            # print('Caught exc {} in VlanAPI.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self):
        raise NotImplementedError


class VlanActionAPI(Resource):
    def get(self, fab_id, sw_id, vlan_id, act_str):
        raise NotImplementedError

    def post(self, fab_id, sw_id, vlan_id, act_str):
        try:
            data = {
                    'cmd': act_str,
                    'request_id': str(uuid.uuid4()),
                    'VLANId': vlan_id
                   }
            payload = request.get_json(force=True)
            data.update(payload)

            resp = util.post_to_switch(sw_id, data)
        except Exception as e:
            # print('Caught exc {} in VlanActionAPI.post()'.format(e))
            resp = RedfishErrorResponse.get_server_error_response(e)
        return resp


class VlanCollectionAPI(Resource):
    def get(self, fab_id, sw_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishVlanCollectionBackend()
            response = redfish_backend.get(fab_id, sw_id)
        except Exception as e:
            # print('Caught exc {} in VlanCollectionAPI.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self, payload):
        raise NotImplementedError


class VlanCollectionActionAPI(Resource):
    def get(self, fab_id, sw_id, act_str):
        raise NotImplementedError

    def post(self, fab_id, sw_id, act_str):
        try:
            payload = request.get_json(force=True)
            data = {
                    'cmd': act_str,
                    'request_id': str(uuid.uuid4()),
                    'VLANId': payload['VLANId']
                   }

            resp = util.post_to_switch(sw_id, data)
        except Exception as e:
            # print('Caught exc {} in VlanCollectionActionAPI.post()'.format(e))
            resp = RedfishErrorResponse.get_server_error_response(e)
        return resp


class VlanEmulationAPI(Resource):
    """
    VLAN
    """

    def __init__(self, **kwargs):
        pass

    def get(self, ident1, ident2, ident3):
        # """
        # HTTP GET
        # """
        if ident1 not in members:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident3 not in members[ident1][ident2]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND

        return members[ident1][ident2][ident3], redfish_constants.SUCCESS


class VlanCollectionEmulationAPI(Resource):
    """
    VLAN Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#VLANCollection.VLANCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#VLANCollection.VLANCollection',
            'Description': 'Collection of VLANs',
            'Name': 'VLANs Collection'
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
            vlans = []
            for vlan in members[ident1].get(ident2, {}).values():
                vlans.append({'@odata.id': vlan['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident1 + '/Switches/' + ident2 + '/VLANs'
            self.cfg['Members'] = vlans
            self.cfg['Members@odata.count'] = len(vlans)
            response = self.cfg, redfish_constants.SUCCESS
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response


def CreateVlanEmulation(**kwargs):
    fab_id = kwargs['fab_id']
    switch_id = kwargs['switch_id']
    vlan_id = kwargs['vlan_id']
    if fab_id not in members:
        members[fab_id] = {}
    if switch_id not in members[fab_id]:
        members[fab_id][switch_id] = {}
    members[fab_id][switch_id][vlan_id] = get_vlan_instance(kwargs)
