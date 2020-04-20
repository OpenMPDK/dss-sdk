# External imports
import traceback

# Flask imports
from flask_restful import reqparse, Api, Resource

# Internal imports
import config
from .templates.EthernetInterface import get_ethernet_interface_instance

members = {}

SERVER_ERROR = 500
NOT_FOUND = 404
SUCCESS = 200


class EthernetInterface(Resource):
    """
    EthernetInterface
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


class EthernetInterfaceCollection(Resource):
    """
    EthernetInterface Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#EthernetInterfaceCollection.EthernetInterfaceCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#EthernetInterfaceCollection.EthernetInterfaceCollection',
            'Description': 'Collection of Ethernet Interfaces',
            'Name': 'Ethernet interfaces Collection'
        }

    def get(self, ident):
        """
        HTTP GET
        """
        try:
            if ident not in members:
                return NOT_FOUND
            ethernet_interface = []
            for nic in members.get(ident, {}).values():
                ethernet_interface.append({'@odata.id': nic['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident + '/EthernetInterfaces'
            self.cfg['Members'] = ethernet_interface
            self.cfg['Members@odata.count'] = len(ethernet_interface)
            response = self.cfg, SUCCESS
        except Exception:
            traceback.print_exc()
            response = SERVER_ERROR

        return response


def CreateEthernetInterface(**kwargs):
    sys_id = kwargs['sys_id']
    nic_id = kwargs['nic_id']
    if sys_id not in members:
        members[sys_id] = {}
    members[sys_id][nic_id] = get_ethernet_interface_instance(kwargs)
