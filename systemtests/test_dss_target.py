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

from target.dss_target import (
    exec_cmd
)
import pytest
import os


@pytest.mark.usefixtures(
    "copy_dss_target_script",
    "copy_target_output_file",
    "read_target_script_cmd"
)
class TestDSSTarget():

    def test_dss_target_script(self, copy_dss_target_script,
                               read_target_script_cmd,
                               copy_target_output_file):
        os.chdir("/usr/dss/nkv-target/bin")
        output_path = "/etc/dss/nvmf.in.conf"
        cmd = "export PCI_BLACKLIST="" &&\
              python3 ./dss_target_test.py configure \\\n"
        cmd += "--config_file " + output_path + " \\\n"
        cmd += "".join(read_target_script_cmd[3:])
        ret, out, err = exec_cmd(cmd)
        assert ret == 0
        assert os.path.exists(output_path)
        test_content = comp_content = ""
        with open(output_path, "r") as test_file:
            test_content = test_file.read()
        with open(output_path + ".bak", "r") as comp_file:
            comp_content = comp_file.read()
        assert test_content == comp_content
