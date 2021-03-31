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

import json
from common.ufmdb.redfish.redfish_ufmdb import redfish_ufmdb

rfdb = redfish_ufmdb(root_uuid='0000-0000-0000-0000', auto_update=False)

print('\nTest 1: *********Retrieve /redfish/v1 **********\n')
rsp = rfdb.get(request='/redfish/v1')
print(json.dumps(rsp, sort_keys = False, indent = 4))

print('\nTest 2: *********Retrieve /redfish/v1/JSONSchemas **********\n')
rsp = rfdb.get(request='/redfish/v1/JSONSchemas')
print(json.dumps(rsp, sort_keys = False, indent = 4))

print('\nTest 3: *********Retrieve /redfish/v1/Hello World **********\n')
rsp = rfdb.get(request='Hello World')
print(rsp)

print('\nTest 4: *********Retrieve /redfish/v1/Systems **********\n')
rsp = rfdb.get(request='/redfish/v1/Systems')
print(json.dumps(rsp, sort_keys = False, indent = 4))

print('\nTest 5: *********Retrieve Members of /redfish/v1/Systems **********\n')
members = rsp['Members']
for member in members:
    endpoint = member['@odata.id']
    rsp_member = rfdb.get(request=endpoint)
    print('\nResponse for {} \n'.format(endpoint))
    print(json.dumps(rsp_member, sort_keys = False, indent = 4))
    #List of Ethernet Interfaces
    ethernet = rsp_member.get('EthernetInterfaces','Invalid')
    if ethernet != "Invalid":
        endpoint = ethernet['@odata.id']
        rsp_ether = rfdb.get(request=endpoint)
        print('\nResponse for {} \n'.format(endpoint))
        print(json.dumps(rsp_ether, sort_keys = False, indent = 4))
        ether_members = rsp_ether['Members']
        for ether_member in ether_members:
            endpoint = ether_member['@odata.id']
            rsp_ether = rfdb.get(request=endpoint)
            print('\nResponse for {} \n'.format(endpoint))
            print(json.dumps(rsp_ether, sort_keys = False, indent = 4))
    #List of Storage drives
    storage = rsp_member.get('Storage','Invalid')
    if storage != "Invalid":
        endpoint = storage['@odata.id']
        rsp_storage = rfdb.get(request=endpoint)
        print('\nResponse for {} \n'.format(endpoint))
        print(json.dumps(rsp_storage, sort_keys = False, indent = 4))
        storage_members = rsp_storage['Members']
        for storage_member in storage_members:
            endpoint = storage_member['@odata.id']
            rsp_storage = rfdb.get(request=endpoint)
            print('\nResponse for {} \n'.format(endpoint))
            print(json.dumps(rsp_storage, sort_keys = False, indent = 4))
            #List of drives
            drives = rsp_storage.get('Drives','Invalid')
            if drives != "Invalid":
                for drive in drives:
                    endpoint = drive['@odata.id']
                    rsp_drive = rfdb.get(request=endpoint)
                    print('\nResponse for {} \n'.format(endpoint))
                    print(json.dumps(rsp_drive, sort_keys = False, indent = 4))


