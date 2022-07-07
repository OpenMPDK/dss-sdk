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


from uuid import uuid4
from flask_restful import Resource

from rest_api.redfish import redfish_constants
from common.ufmdb.redfish.ufmdb_util import ufmdb_util


class ServiceRoot(Resource):

    uuid = str(uuid4())

    def __init__(self, rest_base):
        self.rest_base = rest_base

    def get(self):
        resp = {'@odata.context': self.rest_base + '$metadata#ServiceRoot.ServiceRoot',
                '@odata.type': '#ServiceRoot.v1_0_0.ServiceRoot',
                '@odata.id': self.rest_base,
                'Id': 'RootService',
                'Name': 'Root Service',
                'ProtocolFeaturesSupported': {
                    'ExpandQuery': {
                        'ExpandAll': False
                    },
                    'SelectQuery': False
                },
                'RedfishVersion': '1.8.0',
                'UUID': self.uuid,
                'Vendor': "Samsung",
                'JSONSchemas': {
                    '@odata.id': self.rest_base + 'JSONSchemas'
                },
                'Systems': {
                    '@odata.id': self.rest_base + 'Systems'}}

        if ufmdb_util.has_fabrics():
            resp['Fabrics'] = {'@odata.id': self.rest_base + 'Fabrics'}

        return resp, redfish_constants.SUCCESS
