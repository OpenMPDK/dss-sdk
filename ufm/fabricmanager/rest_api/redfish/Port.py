# External imports
import traceback

# Flask imports
from flask_restful import reqparse, Api, Resource

# Internal imports
import config
from rest_api.redfish.templates.Port import get_port_instance
from rest_api.redfish import redfish_constants

members = {}

class Port(Resource):
    """
    Port
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


class PortCollection(Resource):
    """
    Port Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#PortCollection.PortCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#PortCollection.PortCollection',
            'Description': 'Collection of Ports',
            'Name': 'Ports Collection'
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
            ports = []
            for port in members[ident1].get(ident2, {}).values():
                ports.append({'@odata.id': port['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident1 + '/Switches/' + ident2 + '/Ports'
            self.cfg['Members'] = ports
            self.cfg['Members@odata.count'] = len(ports)
            response = self.cfg, redfish_constants.SUCCESS
        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR

        return response


def CreatePort(**kwargs):
    fab_id = kwargs['fab_id']
    switch_id = kwargs['switch_id']
    port_id = kwargs['port_id']
    if fab_id not in members:
        members[fab_id] = {}
    if switch_id not in members[fab_id]:
        members[fab_id][switch_id] = {}
    members[fab_id][switch_id][port_id] = get_port_instance(kwargs)
