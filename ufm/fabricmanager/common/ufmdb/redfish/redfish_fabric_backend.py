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


from rest_api.redfish import redfish_constants
from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.ufmdb_util import ufmdb_util


class RedfishFabricBackend():
    def __init__(self):
        self.cfg = {
            "@odata.id": "{rest_base}/{Fabrics}/{fab_id}",
            "@odata.type": "#Fabric.v1_1_1.Fabric",
            "Id": "{fab_id}",
            "Description": "Fabric information",
            "Name": "Fabric",
            "FabricType": "{fab_type}",
        }

    def get(self, fab_id):
        try:
            if ufmdb_util.is_valid_fabric(fab_id):
                self.cfg["@odata.id"] = self.cfg["@odata.id"].format(rest_base=redfish_constants.REST_BASE,
                                                                     Fabrics=redfish_constants.FABRICS,
                                                                     fab_id=fab_id)

                self.cfg["Id"] = self.cfg["Id"].format(fab_id=fab_id)

                fab = self.get_fabric(fab_id)
                self.cfg["FabricType"] = self.cfg["FabricType"].format(fab_type=fab["type"])

                if fab["switches"]:
                    self.cfg["Switches"] = {"@odata.id": "{rest_base}/{Fabrics}/{fab_id}/{Switches}"}
                    self.cfg["Switches"]["@odata.id"] = self.cfg["Switches"]["@odata.id"].format(
                                                        rest_base=redfish_constants.REST_BASE,
                                                        Fabrics=redfish_constants.FABRICS,
                                                        fab_id=fab_id,
                                                        Switches=redfish_constants.SWITCHES)

                response = self.cfg, redfish_constants.SUCCESS
            else:
                response = redfish_constants.NOT_FOUND
        except Exception as e:
            # print('Caught exc {} in RedfishFabricBackend.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def put(self, payload):
        pass

    def get_fabric(self, fab_id):
        prefix = "/Fabrics/" + fab_id + "/type"
        kv_dict = ufmdb_util.query_prefix(prefix)

        ret = {}
        for k in kv_dict:
            key = k.split("/")[-2]
            val = k.split("/")[-1]
            ret[key] = val

        prefix = "/Fabrics/" + fab_id + "/list"
        kv_dict = ufmdb_util.query_prefix(prefix)
        ret["switches"] = []
        for k in kv_dict:
            sw_id = k.split("/")[-1]
            ret["switches"].append(sw_id)

        return ret


class RedfishFabricCollectionBackend():
    def __init__(self):
        self.cfg = {
            '@odata.id': '{rest_base}/{Fabrics}',
            '@odata.type': '#FabricCollection.FabricCollection',
            'Description': 'Collection of Fabrics',
            'Name': 'Fabric Collection'
            }

    def get(self):
        try:
            self.cfg['@odata.id'] = self.cfg['@odata.id'].format(rest_base=redfish_constants.REST_BASE,
                                                                 Fabrics=redfish_constants.FABRICS)

            members = []
            fabrics = ufmdb_util.query_prefix('/Fabrics/list')
            for fab in fabrics:
                fab_id = fab.split('/')[-1]
                members.append({'@odata.id': self.cfg['@odata.id'] + '/' + fab_id})

            self.cfg['Members'] = members
            self.cfg['Members@odata.count'] = len(members)
            response = self.cfg, redfish_constants.SUCCESS

        except Exception as e:
            # print('Caught exc {} in RedfishFabricCollectionBackend.get()'.format(e))
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def put(self, payload):
        pass
