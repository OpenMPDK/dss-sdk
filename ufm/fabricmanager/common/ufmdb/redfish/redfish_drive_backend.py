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


from os.path import join

from common.ufmdb.redfish import ufmdb_redfish_resource
from common.ufmdb.redfish.redfish_system_backend import RedfishNkvSystemBackend
from rest_api.redfish import redfish_constants


class RedfishDriveBackend:
    def get(self):
        # The derived classes need to override this method
        raise NotImplementedError

    def put(self, payload):
        # The derived classes need to override this method
        raise NotImplementedError

    @classmethod
    def create_instance(cls, sys_id, storage_id, drive_id):
        (sys_type, entry) = ufmdb_redfish_resource.lookup_resource_in_db(
            redfish_constants.SYSTEMS, sys_id)
        if sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE,
                        redfish_constants.SYSTEMS, sys_id,
                        redfish_constants.STORAGE, storage_id,
                        redfish_constants.DRIVES, drive_id)
            # A bit strange but that is how NKV Redfish UFMDB is wired.
            # It's a single API entry point for all endpoints.
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError


class RedfishDriveCollectionBackend:
    def get(self):
        # The derived classes need to override this method
        raise NotImplementedError

    def put(self, payload):
        # The derived classes need to override this method
        raise NotImplementedError

    @classmethod
    def create_instance(cls, sys_id, storage_id):
        (sys_type, entry) = ufmdb_redfish_resource.lookup_resource_in_db(
            redfish_constants.SYSTEMS, sys_id)
        if sys_type == 'nkv':
            path = join(redfish_constants.REST_BASE,
                        redfish_constants.SYSTEMS, sys_id,
                        redfish_constants.STORAGE, storage_id,
                        redfish_constants.DRIVES)
            # A bit strange but that is how NKV Redfish UFMDB is wired.
            # It's a single API entry point for all endpoints.
            return RedfishNkvSystemBackend(path)
        else:
            raise NotImplementedError
