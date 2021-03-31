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
import copy

# Flask imports
from flask_restful import Resource

# Internal imports
from rest_api.redfish.templates.Fabric import get_Fabric_instance
from rest_api.redfish import redfish_constants
import config

from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.redfish_fabric_backend import RedfishFabricBackend, RedfishFabricCollectionBackend

members = {}


class FabricAPI(Resource):

    def get(self, fab_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishFabricBackend()
            response = redfish_backend.get(fab_id)
        except Exception as e:
            # print('Caught exc {} in FabricAPI.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self):
        raise NotImplementedError


class FabricCollectionAPI(Resource):

    def get(self):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishFabricCollectionBackend()
            response = redfish_backend.get()
        except Exception as e:
            # print('Caught exc {} in FabricCollectionAPI.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self):
        raise NotImplementedError


class FabricEmulationAPI(Resource):
    """
    Fabric Singleton API
    """

    def __init__(self):
        pass

    def get(self, ident):
        """
        HTTP GET
        """
        try:
            response = redfish_constants.NOT_FOUND
            if ident in members:
                con = members[ident]
                response = con, redfish_constants.SUCCESS
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response


class FabricCollectionEmulationAPI(Resource):
    """
    Fabric Collection API
    """

    def __init__(self):
        self.rb = config.rest_base
        self.config = {
            '@odata.context': self.rb + '$metadata#FabricCollection.FabricCollection',
            '@odata.id': self.rb + 'Fabrics',
            '@odata.type': '#FabricCollection.FabricCollection',
            'Description': 'Collection of Fabrics',
            'Members@odata.count': len(members),
            'Name': 'Fabric Collection'
        }
        self.config['Members'] = [
            {'@odata.id': item['@odata.id']} for item in list(members.values())]

    def get(self):
        """
        HTTP GET
        """
        try:
            response = self.config, redfish_constants.SUCCESS
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response


class CreateFabricEmulation(Resource):
    """
    CreateFabric
    """

    def __init__(self, **kwargs):
        if 'resource_class_kwargs' in kwargs:
            global wildcards
            wildcards = copy.deepcopy(kwargs['resource_class_kwargs'])

    def put(self, ident, nqn, ns):
        try:
            global cfg
            global wildcards
            wildcards['fab_id'] = ident
            cfg = get_Fabric_instance(wildcards)
            members[ident] = cfg
            response = cfg, redfish_constants.SUCCESS

        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response
