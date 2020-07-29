
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
