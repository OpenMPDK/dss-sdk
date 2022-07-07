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


"""
Template class for switch clients
"""
# from systems.switch import switch_constants
# from systems.switch.switch_arg import SwitchArg


class SwitchClientTemplate(object):
    def __init__(self, swArg):
        self.swArg = swArg
        self.log = swArg.log

        self.log.info("SwitchClientTemplate ip = {}".format(self.swArg.sw_ip))
        self.log.info("Init {}".format(self.__class__.__name__))

    def show_version(self):
        print('ERROR: SwitchClientTemplate.show_version must be implemented!')
        pass

    def show_vlan(self):
        print('ERROR: SwitchClientTemplate.show_vlan must be implemented!')
        pass

    def show_port(self):
        print('ERROR: SwitchClientTemplate.show_port must be implemented!')
        pass

    def delete_vlan(self, vlan_id):
        print('ERROR: SwitchClientTemplate.delete_vlan must be implemented!')
        pass

    def create_vlan(self, vlan_id):
        print('ERROR: SwitchClientTemplate. create_vlan must be implemented!')
        pass

    def associate_ip_to_vlan(self, vlan_id, ip_address):
        print('ERROR: SwitchClientTemplate.associate_ip_to_vlan must be implemented!')
        pass

    def remove_ip_from_vlan(self, vlan_id, ip_address):
        print('ERROR: SwitchClientTemplate.remove_ip_from_vlan must be implemented!')
        pass

    def assign_port_to_vlan(self, port, vlan_id):
        print('ERROR: SwitchClientTemplate.assign_port_to_vlan must be implemented!')
        pass

    def add_mode_port_to_vlan(self, port, mode, vlan_id):
        print('ERROR: SwitchClientTemplate.add_mode_port_to_vlan must be implemented!')
        pass

    # Major function called by SwitchMonitor to populate switch, port, vlan info into db
    def poll_to_db(self):
        print('ERROR: SwitchClientTemplate.poll_to_db must be implemented!')
        pass
