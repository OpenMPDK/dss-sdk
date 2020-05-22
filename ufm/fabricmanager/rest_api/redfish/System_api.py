# External imports
import copy
import sys
import traceback

# Flask imports
from flask import Flask, request, make_response, render_template
from flask_restful import reqparse, Api, Resource

from common.ufmdb.redfish_ufmdb import RedfishUfmdb
from common.ufmlog import ufmlog
from uuid import uuid4

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
