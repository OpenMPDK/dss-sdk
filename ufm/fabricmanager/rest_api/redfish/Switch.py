# External imports
import traceback

# Flask imports
from flask_restful import reqparse, Api, Resource

# Internal imports
import config
from .templates.Switch import get_switch_instance

members = {}

SERVER_ERROR = 500
NOT_FOUND = 404
SUCCESS = 200


class SwitchApi(Resource):
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
            return 'Client Error: Not Found', NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', NOT_FOUND

        return members[ident1][ident2], SUCCESS


class SwitchCollection(Resource):
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
                return NOT_FOUND
            switches = []
            for switch in members.get(ident, {}).values():
                switches.append({'@odata.id': switch['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident + '/Switches'
            self.cfg['Members'] = switches
            self.cfg['Members@odata.count'] = len(switches)
            response = self.cfg, SUCCESS
        except Exception:
            traceback.print_exc()
            response = SERVER_ERROR

        return response


def CreateSwitch(**kwargs):
    fab_id = kwargs['fab_id']
    switch_id = kwargs['switch_id']
    if fab_id not in members:
        members[fab_id] = {}
    members[fab_id][switch_id] = get_switch_instance(kwargs)
