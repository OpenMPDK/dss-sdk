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


