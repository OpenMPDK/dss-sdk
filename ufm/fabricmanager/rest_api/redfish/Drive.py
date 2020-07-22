# External imports
import traceback

# Flask imports
from flask_restful import reqparse, Api, Resource

# Internal imports
import config
from .templates.Drive import get_drive_instance
from rest_api.redfish import redfish_constants

members = {}


class DriveEmulationAPI(Resource):
    """
    Drive
    """

    def __init__(self, **kwargs):
        pass

    def get(self, ident1, ident2, ident3):
        """
        HTTP GET
        """
        if ident1 not in members:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident2 not in members[ident1]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND
        if ident3 not in members[ident1][ident2]:
            return 'Client Error: Not Found', redfish_constants.NOT_FOUND

        return members[ident1][ident2][ident3], redfish_constants.SUCCESS


class DriveCollectionEmulationAPI(Resource):
    """
    Drive Collection
    """

    def __init__(self, rest_base, suffix):
        self.cfg = {
            '@odata.context': rest_base + '$metadata#DriveCollection.DriveCollection',
            '@odata.id': rest_base + suffix,
            '@odata.type': '#DriveCollection.DriveCollection',
            'Description': 'Collection of drives',
            'Name': 'Drive Collection'
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
            drives = []
            for drive in members[ident1].get(ident2, {}).values():
                drives.append({'@odata.id': drive['@odata.id']})
            self.cfg['@odata.id'] = self.cfg['@odata.id'] + \
                '/' + ident1 + '/Storage/' + ident2 + '/Drives'
            self.cfg['Members'] = drives
            self.cfg['Members@odata.count'] = len(drives)
            response = self.cfg, redfish_constants.SUCCESS
        except Exception:
            traceback.print_exc()
            response = redfish_constants.SERVER_ERROR

        return response


def CreateDriveEmulation(**kwargs):
    sys_id = kwargs['sys_id']
    storage_id = kwargs['storage_id']
    drive_id = kwargs['drive_id']
    if sys_id not in members:
        members[sys_id] = {}
    if storage_id not in members[sys_id]:
        members[sys_id][storage_id] = {}
    members[sys_id][storage_id][drive_id] = get_drive_instance(kwargs)
