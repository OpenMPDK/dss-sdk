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


ALLOWED_TRTYPES = ["rdma", ]
ADRFAM = ["IPv4", "IPv6", ]

MIN_PORT = 1
MAX_PORT = 65536

MAXIMUM_LISTEN_DIRECTIVES = 255
MAXIMUM_NAMESPACE_DIRECTIVES = 255


def find_network_interface(ipaddr, interface_list):
    for interface in interface_list:
        for adrfam in ADRFAM:
            if adrfam in interface_list[interface]:
                if ipaddr == interface_list[interface][adrfam]:
                    return adrfam, interface_list[interface]
            else:
                for iface in interface_list[interface]['Interfaces']:
                    if adrfam in interface_list[interface]['Interfaces'][iface] and \
                            ipaddr == interface_list[interface]['Interfaces'][iface][adrfam]:
                        return adrfam, interface_list[interface]
    return None, None


def find_storage_device(device_identifier, device_list):
    for dev in device_list.values():
        if device_identifier == dev["Serial"]:
            return dev
    return None


def valid_trtype(trtype):
    trtype_lower = trtype.lower()
    for allowed in ALLOWED_TRTYPES:
        if trtype_lower == allowed:
            return True
    return False


def valid_port(port):
    return MIN_PORT <= int(port) < MAX_PORT


def exceeded_maximum_listen_addresses(ip_addresses):
    return len(ip_addresses) > MAXIMUM_LISTEN_DIRECTIVES


def exceeded_maximum_namespaces(namespaces):
    return len(namespaces) > MAXIMUM_NAMESPACE_DIRECTIVES


def find_numa_for_core(core_id, numa_group):
    core_numa = -1
    for cur_numa, numa_map in enumerate(numa_group.values()):
        numa_ranges = numa_map.split(',')
        for numa_range in numa_ranges:
            try:
                _min, _max = numa_range.split('-')
            except:
                _min = _max = numa_range
            if _min <= core_id <= _max:
                core_numa = cur_numa
                break
        if core_numa != -1:
            break
    return core_numa
