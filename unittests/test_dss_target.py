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
    random_with_N_digits, generate_core_mask, generate_core_mask_vmmode,
    get_pcie_address_firmware_mapping, get_pcie_address_serial_mapping,
    get_nvme_list_numa, ip4_addresses, only_mtu9k, get_rdma_ips,
    get_numa_boundary, get_numa_ip, create_nvmf_config_file, get_vlan_ips,
    setup_hugepage
)
import pytest
from conftest import (
    mock_exec_cmd, default_mock_ip,
    mock_interfaces, mock_ifaddresses
)


EXEC = "target.dss_target.exec_cmd"
INTERFACE = "target.dss_target.interfaces"
IFADDRESS = "target.dss_target.ifaddresses"
AFINET = "target.dss_target.AF_INET"
OPEN = "builtins.open"
READLINES = "typing.IO.readlines"


def mock_functions(mocker, *args):
    for arg in args:
        if arg == EXEC:
            mocker.patch(arg, mock_exec_cmd)
        elif arg == INTERFACE:
            mocker.patch(arg, mock_interfaces)
        elif arg == IFADDRESS:
            mocker.patch(arg, mock_ifaddresses)
        elif arg == AFINET:
            mocker.patch(arg, 0)
        else:
            mocker.patch(arg)


class TestDSSTarget():
    def test_random_with_N_digits(self):
        num_digits = 10
        num = random_with_N_digits(10)
        assert len(str(num)) == num_digits

    def test_generate_core_mask(self):
        core_count = 12
        dedicated_core_percent = 0.6
        return 0 == generate_core_mask(core_count, dedicated_core_percent)

    def test_generate_core_mask_vmmode(self):
        core_count = 12
        return 0 == generate_core_mask_vmmode(core_count)

    def test_get_pcie_address_firmware_mapping(self, mocker):
        mock_functions(mocker, EXEC, OPEN, READLINES)
        r1, r2 = get_pcie_address_firmware_mapping()

    def test_get_pcie_address_serial_mapping(self, mocker):
        mock_functions(mocker, EXEC, OPEN, READLINES)
        r = get_pcie_address_serial_mapping([])
        assert len(r) > 0

    def test_get_nvme_list_numa(self, mocker):
        mock_functions(mocker, EXEC)
        r = get_nvme_list_numa()
        assert len(r) > 0

    def test_ip4_addresses(self, mocker):
        r = ip4_addresses()
        assert len(r) > 0

    def test_only_mtu9k(self, mocker):
        mock_functions(mocker, EXEC)
        r = only_mtu9k({"ip": 100})
        assert len(r) > 0

    def test_get_rdma_ips(self, mocker):
        r = get_rdma_ips({"nic": default_mock_ip}, [default_mock_ip])
        assert len(r) > 0

    def test_get_numa_boundary(self, mocker):
        mock_functions(mocker, EXEC)
        r = get_numa_boundary()
        assert int(r) == 1

    def test_get_numa_ip(self, mocker):
        mock_functions(mocker, EXEC, INTERFACE,
                       IFADDRESS, AFINET)
        r1, r2 = get_numa_ip([default_mock_ip])
        assert len(r1) > 0
        assert len(r2) == 0

    def test_create_nvmf_config_file(self, mocker):
        mock_functions(mocker, EXEC, INTERFACE,
                       IFADDRESS, AFINET, OPEN)
        r = create_nvmf_config_file("", [default_mock_ip], [], [])
        assert r == 0

    def test_get_vlan_ips(self, mocker):
        mock_functions(mocker, EXEC)
        r = get_vlan_ips("1234")
        assert r

    def test_setup_hugepage(self, mocker):
        mock_functions(mocker, EXEC, OPEN)
        r = setup_hugepage()
        assert r == 0
