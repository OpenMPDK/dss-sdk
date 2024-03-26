#!/usr/bin/python3

"""
# The Clear BSD License
#
# Copyright (c) 2023 Samsung Electronics Co., Ltd.
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
"""

import pytest
import shutil
import os


@pytest.fixture
def copy_dss_host_script():
    origin_file_path = "./host/scripts/dss_host.py"
    test_file_path = "/usr/dss/nkv-sdk/bin/dss_host_test.py"
    try:
        shutil.copyfile(origin_file_path, test_file_path)
    except Exception as e:
        print(f'copy file {test_file_path} to {test_file_path} error {e}')
    yield
    try:
        os.remove(test_file_path)
    except Exception as e:
        print(f'remove file {test_file_path} error {e}')


@pytest.fixture
def copy_conf_files():
    origin_dir = "/usr/dss/nkv-sdk/conf"
    bak_dir = "/usr/dss/nkv-sdk/conf.bak"
    try:
        shutil.copytree(origin_dir, bak_dir)
    except Exception as e:
        print(f'{e}')
    yield
    try:
        shutil.rmtree(origin_dir)
        shutil.copytree(bak_dir, origin_dir)
        shutil.rmtree(bak_dir)
    except Exception as e:
        print(f'{e}')


@pytest.fixture
def copy_dss_target_script():
    origin_file_path = "./target/dss_target.py"
    test_file_path = "/usr/dss/nkv-target/bin/dss_target_test.py"
    try:
        shutil.copyfile(origin_file_path, test_file_path)
    except Exception as e:
        print(f'copy file {test_file_path} to {test_file_path} error {e}')
    yield
    try:
        os.remove(test_file_path)
    except Exception as e:
        print(f'remove file {test_file_path} error {e}')


@pytest.fixture
def read_host_script_cmd():
    cmd_file_path = "/usr/dss/nkv-sdk/bin/dss_host_config_host.sh"
    with open(cmd_file_path, 'r') as cmd_file:
        cmd = cmd_file.read()
    return cmd


@pytest.fixture
def copy_target_output_file():
    file_path = "/etc/dss/nvmf.in.conf"
    bak_file_path = file_path + ".bak"
    try:
        shutil.copyfile(file_path, bak_file_path)
    except Exception as e:
        print(f'copy {file_path} to {bak_file_path} failed with error{e}')
    yield
    try:
        os.remove(file_path)
        shutil.copyfile(bak_file_path, file_path)
        os.remove(bak_file_path)
    except Exception as e:
        print(f'remove and copy file failed with error {e}')


@pytest.fixture
def read_target_script_cmd():
    cmd_file_path = "/usr/dss/nkv-target/bin/dss_target_config.sh"
    with open(cmd_file_path, 'r') as cmd_file:
        cmd = cmd_file.readlines()
    return cmd
