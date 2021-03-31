"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

# External imports
import traceback

# Flask imports
from flask import request
from flask_restful import reqparse, Api, Resource

# Internal imports
import config
from common.ufmlog import ufmlog
from common.ufmdb.redfish.redfish_ethernet_backend import RedfishEthernetBackend, RedfishEthernetCollectionBackend
from .redfish_error_response import RedfishErrorResponse
from .templates.EthernetInterface import get_ethernet_interface_instance
from rest_api.redfish import redfish_constants

members = {}


class EthernetInterfaceAPI(Resource):
    def __init__(self):
        self.rf_err_resp = RedfishErrorResponse()

    def get(self, sys_id, eth_id):
        try:
            redfish_backend = RedfishEthernetBackend.create_instance(
                sys_id, eth_id)
            response = redfish_backend.get()
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response

    def post(self, sys_id, eth_id):
        try:
            payload = request.get_json(force=True)
            redfish_backend = RedfishEthernetBackend.create_instance(
                sys_id, eth_id)
            response = redfish_backend.put(payload)
        except NotImplementedError as e:
            response = self.rf_err_resp.get_method_not_allowed_response('EthernetInterface: ' + eth_id)
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response


class EthernetInterfaceCollectionAPI(Resource):
    def __init__(self):
        self.rf_err_resp = RedfishErrorResponse()

    def get(self, sys_id):
        try:
            redfish_backend = RedfishEthernetCollectionBackend.create_instance(
                sys_id)
            response = redfish_backend.get()
        except Exception as e:
            response = self.rf_err_resp.get_server_error_response(e)
        return response

    def post(self):
        return self.rf_err_resp.get_method_not_allowed_response('EthernetInterfaces')


class EthernetInterfaceEmulationAPI(Resource):
    """
    EthernetInterface
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


class EthernetInterfaceCollectionEmulationAPI(Resource):
    """
    EthernetInterface Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#EthernetInterfaceCollection.EthernetInterfaceCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#EthernetInterfaceCollection.EthernetInterfaceCollection',
            'Description': 'Collection of Ethernet Interfaces',
            'Name': 'Ethernet interfaces Collection'
        }

    def get(self, ident):
        """
        HTTP GET
        """
        try:
            if ident not in members:
                return redfish_constants.NOT_FOUND
            ethernet_interface = []
            for nic in members.get(ident, {}).values():
                ethernet_interface.append({'@odata.id': nic['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident + '/EthernetInterfaces'
            self.cfg['Members'] = ethernet_interface
            self.cfg['Members@odata.count'] = len(ethernet_interface)
            response = self.cfg, redfish_constants.SUCCESS
        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR

        return response


def CreateEthernetInterfaceEmulation(**kwargs):
    sys_id = kwargs['sys_id']
    nic_id = kwargs['nic_id']
    if sys_id not in members:
        members[sys_id] = {}
    members[sys_id][nic_id] = get_ethernet_interface_instance(kwargs)
