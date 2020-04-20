# Resource Manager module

# External imports
from uuid import uuid4
import copy
from enum import Enum

# Internal Imports
import config
from .resource_dictionary import ResourceDictionary
from .static_data_loader import get_static_data


class ResourceManager(object):
    """
    Defines ServiceRoot
    """

    def __init__(self, rest_base, mode):
        """
        Argument:
            rest_base - Base URL for the REST interface
            mode - Data source
        """

        self.rest_base = rest_base
        self.uuid = str(uuid4())

        self.resource_dictionary = ResourceDictionary()
        # TODO: Load data from DB
        # Load static data
        if(mode is not None and "static" == mode.lower()):
            if "Redfish" == config.static_data:
                print('Loading static resources for Redfish API')
                self.Systems = get_static_data(
                    self.resource_dictionary, rest_base, "Redfish")

    @property
    def configuration(self):
        """
        Configuration property - Service Root
        """
        config = {
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

        }

        return config

    def load_resource_dictionary(self, path):
        """
        Load resource for a specific path
        """
        config = self.resource_dictionary.get_resource(path)
        return config
