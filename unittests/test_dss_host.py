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

from host.scripts.dss_host import (
    is_ipv4, getSubnet, get_ip_port_nqn_info, decode,
    build_driver, install_kernel_driver, get_ips_for_vlan,
    drive_to_addr_map, mountpt_to_nqn_addr_map, subnet_drive_map,
    discover_dist, config_minio_dist, config_minio_sa, dss_host_args
)
import pytest
import secrets
import os
from conftest import (
    mock_exec_cmd, mock_os_dirname_host,
    mock_exec_cmd_remote,
    MockArgParser
)


def mock_functions(mocker, *args):
    for arg in args:
        if arg == "host.scripts.dss_host.exec_cmd":
            mocker.patch(arg, mock_exec_cmd)
        elif arg == "os.path.dirname":
            mocker.patch(arg, mock_os_dirname_host)
        elif arg == "host.scripts.dss_host.exec_cmd_remote":
            mocker.patch(arg, mock_exec_cmd_remote)
        elif arg == "argparse.ArgumentParser":
            mocker.patch(arg, MockArgParser)
        else:
            mocker.patch(arg)


@pytest.mark.usefixtures(
    "get_sample_ipv4_addr",
    "get_sample_ipv6_addr",
    "get_sample_nvme_discover_output"
)
class TestDSSHost():
    def test_is_ipv4(self, get_sample_ipv4_addr, get_sample_ipv6_addr):
        assert bool(is_ipv4(get_sample_ipv4_addr))
        assert not is_ipv4(get_sample_ipv6_addr)

    def test_getSubnet(self, get_sample_ipv4_addr):
        addr = get_sample_ipv4_addr
        subnet = '.'.join(addr.split('.')[0:3])
        assert subnet == getSubnet(addr)

    def test_get_ip_port_nqn_info(self, get_sample_nvme_discover_output):
        data = get_ip_port_nqn_info(get_sample_nvme_discover_output, "rdma")
        proto = 'rdma'
        port = '1024'
        subsystem = 'nqn.2023-06.io:msl-dpe-perf35-kv_data1'
        ip = 'x.x.x.x'
        assert data == [[proto, port, subsystem, ip]]

    def test_decode(self):
        bytes = bytearray(secrets.randbits(8) for _ in range(200))
        assert isinstance(decode(bytes), str)

    def test_build_driver(self, mocker):
        mock_functions(mocker, "host.scripts.dss_host.exec_cmd",
                       "os.path.dirname")
        build_driver()

    def test_install_kernel_driver(self, mocker):
        mock_functions(mocker, "host.scripts.dss_host.exec_cmd",
                       "os.path.dirname")
        install_kernel_driver(512)

    def test_get_ips_for_vlan(self, mocker):
        mock_functions(mocker, "host.scripts.dss_host.exec_cmd_remote")
        res = get_ips_for_vlan("1234", "host", ["pw"])
        assert len(res) > 0

    def test_drive_to_addr_map(self, mocker):
        mock_functions(mocker, "host.scripts.dss_host.exec_cmd")
        res = drive_to_addr_map()
        assert len(res) == 4

    def test_mountpt_to_nqn_addr_map(self, mocker):
        mock_functions(mocker, "host.scripts.dss_host.exec_cmd")
        res = mountpt_to_nqn_addr_map()
        assert len(res) == 4
        for dev in res.keys():
            assert "nqn" in res[dev]
            assert "addr" in res[dev]

    def test_subnet_drive_map(self, mocker):
        mock_functions(mocker, "host.scripts.dss_host.exec_cmd")
        res = subnet_drive_map()
        assert len(res) > 0

    def test_discover_dist(self, mocker):
        mock_functions(mocker, "host.scripts.dss_host.exec_cmd",
                       "host.scripts.dss_host.exec_cmd_remote")
        res = discover_dist(1234, ["1234"], ["1234"], "pw")
        assert len(res) > 0

    def test_config_minio_dist(self, mocker):
        mock_functions(mocker, "builtins.open", "os.chmod")
        config_minio_dist(["ip", "port", "dev_start", "dev"], 1)

    def test_config_minio_sa(self, mocker):
        mock_functions(mocker, "builtins.open")
        config_minio_sa(["ip", "port", "dev_start", "dev"], 1)

    def test_dss_host_args_config_host(self, mocker):
        mock_functions(mocker, "argparse.ArgumentParser",
                       "host.scripts.dss_host.exec_cmd",
                       "host.scripts.dss_host.exec_cmd_remote",
                       "builtins.open", "os.chmod", "os.chdir",
                       "os.path.exists", "json.load", "json.dump")
        d = dss_host_args()
