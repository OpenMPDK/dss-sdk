# External imports
import copy
import sys
import traceback

# Flask imports
from flask import Flask, request, make_response, render_template
from flask_restful import reqparse, Api, Resource
from os.path import basename

from common.ufmdb.redfish.redfish_ufmdb import RedfishUfmdb
from common.ufmlog import ufmlog
from common.ufmdb.redfish import ufmdb_redfish_resource
from common.ufmdb.redfish.redfish_system_backend import RedfishSystemBackend, RedfishSystemCollectionBackend, \
    RedfishCollectionBackend

# Internal imports
from rest_api.redfish.templates.System import get_System_instance
from rest_api.redfish import redfish_constants
import config

members = {}


class UfmdbSystemAPI(Resource):
    """
    System UFMDB API
    """
    def __init__(self, **kwargs):
        self.rfdb = RedfishUfmdb(auto_update=True)
        self.log = ufmlog.log(module="RFDB", mask=ufmlog.UFM_REDFISH_DB)
        self.log.detail("UfmdbSystemAPI started.")

    def __del__(self):
        self.log.detail("UfmdbSystemAPI stopped.")

    def post(self, path="", **kwargs):
        """
        HTTP POST
        """
        try:
            path = path.strip('/')
            path = "/" + path
            payload = request.get_json(force=True)
            response = self.rfdb.post(path, payload)

        except Exception as e:
            self.log.exception(e)
            #traceback.print_exc()
            response = { "Status": 500, "Message": "Internal Server Error" }

        return response

    def get(self, path="", **kwargs):
        """
        HTTP GET
        """
        try:
            path = path.strip('/')
            path = "/" + path
            response = self.rfdb.get(path, "{}")

        except Exception as e:
            self.log.exception(e)
            #traceback.print_exc()
            response = { "Status": 500, "Message": "Internal Server Error" }

        return response


class SystemAPI(Resource):
    def get(self, ident):
        try:
            (resource_type, entry) = ufmdb_redfish_resource.lookup_resource_in_db('Systems', ident)
            redfish_backend = RedfishSystemBackend.create_instance(resource_type, entry, ident)
            response = redfish_backend.get()
        except Exception as e:
            self.log.exception(e)
            response = {"Status": redfish_constants.SERVER_ERROR, "Message": "Internal Server Error"}
        return response

    def post(self):
        raise NotImplementedError


class CommonCollectionAPI(Resource):

    def __init__(self):
        self.collection = basename(request.path.strip('/'))

    def get(self):
        try:
            collection_backend = RedfishCollectionBackend.create_instance(self.collection)
            response = collection_backend.get()
        except Exception as e:
            self.log.exception(e)
            response = {"Status": redfish_constants.SERVER_ERROR, "Message": "Internal Server Error"}
        return response

    def post(self):
        raise NotImplementedError


class SystemEmulationAPI(Resource):
    """
    System Singleton API
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
        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR
        return response


class SystemCollectionEmulationAPI(Resource):
    """
    System Collection API
    """

    def __init__(self):
        self.rb = config.rest_base
        self.config = {
            '@odata.context': self.rb + '$metadata#ComputerSystemCollection.ComputerSystemCollection',
            '@odata.id': self.rb + 'Systems',
            '@odata.type': '#ComputerSystemCollection.ComputerSystemCollection',
            'Description': 'Collection of Computer Systems',
            'Members@odata.count': len(members),
            'Name': 'System Collection'
        }
        self.config['Members'] = [
            {'@odata.id': item['@odata.id']} for item in list(members.values())]

    def get(self):
        """
        HTTP GET
        """
        try:
            response = self.config, redfish_constants.SUCCESS
        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR

        return response


class CreateSystemEmulation(Resource):
    """
    CreateSystem
    """

    def __init__(self, **kwargs):
        if 'resource_class_kwargs' in kwargs:
            global wildcards
            wildcards = copy.deepcopy(kwargs['resource_class_kwargs'])

    def put(self, ident, nqn, ns):
        try:
            global cfg
            global wildcards
            wildcards['sys_id'] = ident
            wildcards['nqn_id'] = nqn
            wildcards['ns_id'] = ns
            cfg = get_System_instance(wildcards)
            members[ident] = cfg
            response = cfg, redfish_constants.SUCCESS

        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR

        return response
