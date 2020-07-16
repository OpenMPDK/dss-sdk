# External imports
import traceback
import uuid

# Flask imports
from flask_restful import request, Resource

# Internal imports
import config
from rest_api.redfish.templates.Switch import get_switch_instance
from rest_api.redfish import redfish_constants
from rest_api.redfish import util

from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.redfish_switch_backend import RedfishSwitchBackend, RedfishSwitchCollectionBackend

members = {}

class SwitchAPI(Resource):

    def get(self, fab_id, sw_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishSwitchBackend()
            response = redfish_backend.get(fab_id, sw_id)
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self):
        raise NotImplementedError


class SwitchActionAPI(Resource):
    def get(self):
        raise NotImplementedError

    def post(self, fab_id, sw_id, act_str):
        try:
            data = {
                    'cmd': act_str,
                    'request_id': str(uuid.uuid4()),
                    'sw_id': sw_id
                   }
            payload = request.get_json(force=True)
            data.update(payload)

            resp = util.post_to_switch(sw_id, data)

        except Exception as e:
            print('SwitchActionAPI.post() failed')
            print(e)
            resp = RedfishErrorResponse.get_server_error_response(e)

        return resp


class SwitchCollectionAPI(Resource):

    def get(self, fab_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishSwitchCollectionBackend()
            response = redfish_backend.get(fab_id)
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self):
        raise NotImplementedError




class SwitchEmulationAPI(Resource):
    """
    Switch
    """

    def __init__(self, **kwargs):
        pass

    def get(self, ident1, ident2):
        # """
        # HTTP GET
        # """
        if ident1 not in members:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND

        return members[ident1][ident2], redfish_constants.SUCCESS


class SwitchCollectionEmulationAPI(Resource):
    """
    Switch Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#SwitchCollection.SwitchCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#SwitchCollection.SwitchCollection',
            'Description': 'Collection of Switches',
            'Name': 'Ethernet Switches Collection'
        }

    def get(self, ident):
        """
        HTTP GET
        """
        try:
            if ident not in members:
                return redfish_constants.NOT_FOUND
            switches = []
            for switch in members.get(ident, {}).values():
                switches.append({'@odata.id': switch['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident + '/Switches'
            self.cfg['Members'] = switches
            self.cfg['Members@odata.count'] = len(switches)
            response = self.cfg, redfish_constants.SUCCESS
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response


def CreateSwitchEmulation(**kwargs):
    fab_id = kwargs['fab_id']
    switch_id = kwargs['switch_id']
    if fab_id not in members:
        members[fab_id] = {}
    members[fab_id][switch_id] = get_switch_instance(kwargs)
