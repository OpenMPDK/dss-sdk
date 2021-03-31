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

from rest_api.redfish import redfish_constants
from common.ufmdb import ufmdb


class ufmdb_util():
    # Given a prefix, return a dict of full key and value pairs
    @classmethod
    def query_prefix(self, qstring):
        db = ufmdb.client(db_type='etcd')

        kv_dict = dict()
        query = db.get_prefix(qstring)
        if query is None:
            query = db.get_prefix(qstring)
            if query is None:
                return dict()

        for value, metadata in query:
            kv_dict.update({metadata.key.decode('utf-8'): value.decode('utf-8')})

        return(kv_dict)

    @classmethod
    def has_fabrics(self):
        return True

        db = ufmdb.client(db_type='etcd')
        key = '/' + redfish_constants.FABRICS + '/list'
        result = db.get_prefix(key)
        if result:
            return True
        else:
            return False

    @classmethod
    def is_valid_fabric(self, fab_id):
        return True

        db = ufmdb.client(db_type='etcd')
        key = '/' + redfish_constants.FABRICS + '/list/' + fab_id
        result = db.get(key)
        if result:
            return True
        else:
            return False

    @classmethod
    def is_valid_switch(self, sw_id):
        return True

        db = ufmdb.client(db_type='etcd')
        key = '/switches/list/' + sw_id
        result = db.get(key)
        if result:
            return True
        else:
            return False

    @classmethod
    def get_mq_port(self, sw_uuid):
        pre = '/switches/' + sw_uuid + '/switch_attributes/mq_port/'
        kv_dict = self.query_prefix(pre)

        mq_port = None
        for k in kv_dict:
            mq_port = k.split('/')[-1]

        return mq_port

    @classmethod
    def convert_ranges_to_list(self, ranges_str):
        ranges = [(lambda l: range(l[0], l[-1]+1))(list(map(int, r.split('-')))) for r in ranges_str.split(',')]
        ret_list = [y for x in ranges for y in x]
        return ret_list
