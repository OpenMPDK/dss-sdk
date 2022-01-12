# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


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
