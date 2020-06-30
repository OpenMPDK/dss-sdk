# External imports
import uuid
import zmq
import json

# Flask imports
from flask_restful import request, Resource

# Internal imports
import config
from rest_api.redfish.templates.VLAN import get_vlan_instance
from rest_api.redfish import redfish_constants

from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from common.ufmdb.redfish.redfish_vlan_backend import RedfishVlanBackend, RedfishVlanCollectionBackend

members = {}


class VlanAPI(Resource):

    def get(self, fab_id, sw_id, vlan_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishVlanBackend()
            response = redfish_backend.get(fab_id, sw_id, vlan_id)
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self):
        raise NotImplementedError


class VlanActionAPI(Resource):

    def get(self, fab_id, sw_id, vlan_id, act_str):
        raise NotImplementedError

    def post(self, fab_id, sw_id, vlan_id, act_str):
        print('LUFAN: VlanActionAPI post received:' + act_str)
        ctx = zmq.Context()
        skt = ctx.socket(zmq.REQ)
        skt.connect("tcp://localhost:5515")

        data = dict()
        data['cmd'] = act_str
        data['request_id'] = str(uuid.uuid4())

        json_request = json.dumps(data, indent=4, sort_keys=True)
        json_response = None
        try:
            skt.send_json(json_request)
            print("request {}".format(json_request))

            # block until response is received
            json_response = skt.recv_json()
        except KeyboardInterrupt:
            pass

        print("Received: {}".format(json_response) )

        skt.close()
        ctx.destroy()

        return json_response


class VlanCollectionAPI(Resource):

    def get(self, fab_id, sw_id):
        # """
        # HTTP GET
        # """
        try:
            redfish_backend = RedfishVlanCollectionBackend()
            response = redfish_backend.get(fab_id, sw_id)
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response

    def post(self, payload):
        raise NotImplementedError


class VlanCollectionActionAPI(Resource):

    def get(self, fab_id, sw_id, act_str):
        raise NotImplementedError

    def post(self, fab_id, sw_id, act_str):
        print('VlanCollectionActionAPI: post ' + act_str)
        try:
            payload = request.get_json(force=True)
        except Exception as e:
            print(e)

        print('Printing payload ......')
        print(payload)

        return {}






class VlanEmulationAPI(Resource):
    """
    VLAN
    """

    def __init__(self, **kwargs):
        pass

    def get(self, ident1, ident2, ident3):
        # """
        # HTTP GET
        # """
        if ident1 not in members:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident3 not in members[ident1][ident2]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND

        return members[ident1][ident2][ident3], redfish_constants.SUCCESS


class VlanCollectionEmulationAPI(Resource):
    """
    VLAN Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#VLANCollection.VLANCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#VLANCollection.VLANCollection',
            'Description': 'Collection of VLANs',
            'Name': 'VLANs Collection'
        }

    def get(self, ident1, ident2):
        """
        HTTP GET
        """
        try:
            if ident1 not in members:
                return redfish_constants.NOT_FOUND
            if ident2 not in members[ident1]:
                return redfish_constants.NOT_FOUND
            vlans = []
            for vlan in members[ident1].get(ident2, {}).values():
                vlans.append({'@odata.id': vlan['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident1 + '/Switches/' + ident2 + '/VLANs'
            self.cfg['Members'] = vlans
            self.cfg['Members@odata.count'] = len(vlans)
            response = self.cfg, redfish_constants.SUCCESS
        except Exception as e:
            response = RedfishErrorResponse.get_server_error_response(e)
        return response


def CreateVlanEmulation(**kwargs):
    fab_id = kwargs['fab_id']
    switch_id = kwargs['switch_id']
    vlan_id = kwargs['vlan_id']
    if fab_id not in members:
        members[fab_id] = {}
    if switch_id not in members[fab_id]:
        members[fab_id][switch_id] = {}
    members[fab_id][switch_id][vlan_id] = get_vlan_instance(kwargs)
