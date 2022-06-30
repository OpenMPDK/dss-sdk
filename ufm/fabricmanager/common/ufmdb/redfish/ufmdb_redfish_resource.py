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


import ast
from os.path import join
import json
from jsonpath_ng import jsonpath, parse

from common.ufmdb import ufmdb
from rest_api.redfish import redfish_constants

# TODO: Bundle all of these in a utility class?


def get_prefix_keys(prefix):
    db = ufmdb.client(db_type='etcd')
    return [meta.key.decode('UTF-8') for (_, meta) in db.get_prefix(prefix)]


# TODO: Make it generic for any lookup resource and accept prefix as argument.
def get_resources_list(resource):
    prefix = '/' + resource
    keys = get_prefix_keys(prefix)
    resources = []
    for key in keys:
        resource_id = key.split('/')[2]
        resource_entry = {'@odata.id': join(redfish_constants.REST_BASE, resource, resource_id)}
        resources.append(resource_entry)
    return resources


def get_resource_value(key):
    db = ufmdb.client(db_type='etcd')
    (res, meta) = db.get(key)
    if res:
        value = ast.literal_eval(res.decode('UTF-8'))
        return value
    else:
        return {}


def get_resp_from_db(key):
    db = ufmdb.client(db_type='etcd')
    (res, meta) = db.get(key)
    if res:
        res = res.decode('UTF-8')
        value = json.loads(res)
        return value
    else:
        return {}


def lookup_resource_in_db(prefix, id):
    key = join('/', prefix, id)
    value = get_resource_value(key)
    if value:
        return value['type'], value
    else:
        return 'nkv', {}


def sub_resource_values(data, field, old, new):
    if not isinstance(data, dict):
        return

    def substituter(x, y, z):
        if isinstance(x, str):
            y[z] = x.replace(old, new)
        return x

    # Construct the expression this way to make the parser syntax happy.
    parse_expr = "$..['" + field + "']"
    jsonpath_expr = parse(parse_expr)
    jsonpath_expr.find(data)
    jsonpath_expr.update(data, substituter)
    return
