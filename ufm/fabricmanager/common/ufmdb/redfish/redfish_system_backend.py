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

from os.path import join

from common.ufmdb.redfish import ufmdb_redfish_resource
from common.ufmdb.redfish.redfish_ufmdb import RedfishUfmdb
from common.ufmlog import ufmlog
from rest_api.redfish import redfish_constants

# TODO:
#  1. Replace strings like type_specific_data with constants in all backends
#  2. Add logging


class RedfishSystemBackend:
    def get(self):
        # The derived classes need to override this method
        raise NotImplementedError

    def put(self, payload):
        # The derived classes need to override this method
        raise NotImplementedError

    @classmethod
    def create_instance(cls, ident):
        (resource_type, entry) = ufmdb_redfish_resource.lookup_resource_in_db(redfish_constants.SYSTEMS, ident)
        if resource_type == 'nkv':
            path = join(redfish_constants.REST_BASE, redfish_constants.SYSTEMS, ident)
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError


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
        if collection == redfish_constants.SYSTEMS:
            return RedfishSystemCollectionBackend()
        else:
            raise NotImplementedError


class RedfishSystemCollectionBackend(RedfishCollectionBackend):

    def __init__(self):
        super().__init__(redfish_constants.SYSTEMS)
        self.path = join(redfish_constants.REST_BASE, redfish_constants.SYSTEMS)

    def get(self):
        # Get the nkv systems
        resp = self.get_response_base()
        nkv_sys_backend = RedfishNkvSystemBackend(self.path)
        nkv_resp = nkv_sys_backend.get()
        if 'Members' in nkv_resp:
            # Update the Members and count
            resp['Members'] = resp['Members'] + nkv_resp['Members']
            resp['Members@odata.count'] = resp['Members@odata.count'] + len(nkv_resp['Members'])
        # Get All other systems from the lookup prefix
        systems = ufmdb_redfish_resource.get_resources_list(redfish_constants.SYSTEMS)
        if systems:
            # Update the Members and count
            resp['Members'] = resp['Members'] + systems
            resp['Members@odata.count'] = resp['Members@odata.count'] + len(systems)

        return resp

    @staticmethod
    def get_response_base():
        return {
            '@odata.context': redfish_constants.REST_BASE+'$metadata#ComputerSystemCollection.ComputerSystemCollection',
            '@odata.id': redfish_constants.REST_BASE + 'Systems',
            '@odata.type': '#ComputerSystemCollection.ComputerSystemCollection',
            'Description': 'Collection of Computer Systems',
            'Members': [],
            'Members@odata.count': 0,
            'Name': 'System Collection'
        }

    def put(self, payload):
        raise NotImplementedError
