# External imports
import os
import json
import re


class Loader():
    """
    Helper class to load static data for Redfish API
    """

    def __init__(self, config):
        self.config = config

    @property
    def configuration(self):
        return self.config


def get_static_data(resource_dictionary, rest_base, spec):
    """
    Load the static data from rest_api/<spec>/static folder
    """
    dir_path = os.path.dirname(__file__)
    base_path = os.path.join(dir_path, spec.lower(), 'static')

    for dir, sub_dir_list, file_list in os.walk(base_path):
        for file in file_list:
            if file != 'index.json':
                continue
            file_path = os.path.join(dir, file)
            file_content = open(file_path)
            index_data = json.load(file_content)
            static_loader_obj = Loader(index_data)
            relative_path = os.path.join(base_path)
            sub_path = os.path.relpath(file_path, relative_path)
            sub_path = re.sub('/index.json', '', sub_path)
            resource_dictionary.add_resource(sub_path, static_loader_obj)
    return sub_path
