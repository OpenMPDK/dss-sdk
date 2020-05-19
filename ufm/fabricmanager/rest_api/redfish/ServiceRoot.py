from uuid import uuid4
from flask_restful import Resource

from rest_api.redfish import redfish_constants


class ServiceRoot(Resource):

    uuid = str(uuid4())

    def __init__(self, rest_base):
        self.rest_base = rest_base

    def get(self):
        return {
                   '@odata.context': self.rest_base + '$metadata#ServiceRoot.ServiceRoot',
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
                       '@odata.id': self.rest_base + 'Systems'
                   }

               }, redfish_constants.SUCCESS