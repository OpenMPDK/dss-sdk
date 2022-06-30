# ufmadm_eth.py

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


from ufmmenu import UfmMenu
import ufmapi


class EthMenu(UfmMenu):
    def __init__(self, sys=""):
        UfmMenu.__init__(self, name="eth", back_func=self._back_action)

        rsp = ufmapi.redfish_get("/Systems/{}/EthernetInterfaces".format(sys))
        if rsp is None:
            return

        if "Members" not in rsp:
            return

        count = 0
        print()
        print("*  Subsystem:", sys)

        for member in rsp["Members"]:
            mac = member['@odata.id'].split("/")[6]
            print("         MAC: ( {} ) {}".format(str(count), mac))

            self.add_item(labels=[str(count)], action=self._mac_action, priv=mac, desc=mac)

            count = count + 1

        self.sys = sys

    def _back_action(self, menu, item):
        from ufmadm_sys import SysMenu

        sys_menu = SysMenu(sys=self.sys)
        self.set_menu(sys_menu)

    def _mac_action(self, menu, item):
        mac_menu = MacMenu(sys=self.sys, mac=item.priv)
        self.set_menu(mac_menu)


class MacMenu(UfmMenu):
    def __init__(self, sys=None, mac=None):
        if mac is None:
            print("ERROR: Null MAC provided.")
            return

        if sys is None:
            print("ERROR: Null SYS provided.")
            return

        UfmMenu.__init__(self, name="mac", back_func=self._back_action)

        rsp = ufmapi.redfish_get("/Systems/{}/EthernetInterfaces/{}".format(sys, mac))
        if rsp is None:
            return

        if "LinkStatus" not in rsp:
            return

        print()
        print("*  Subsystem:", sys)
        print("*        MAC:", mac)
        print("  LinkStatus:", rsp["LinkStatus"])
        print("       Speed:", rsp["SpeedMbps"], "Mbps")

        for address in rsp["IPv4Addresses"]:
            print("IPv4 Address:", address["Address"])
            print("          Subnet:", address["SubnetMask"])
            print("          Origin:", address["AddressOrigin"])
            print("         Gateway:", address["Gateway"])
            try:
                print("            Port:", address["oem"]['Port'])
                print("        Protocol:", address["oem"]['SupportedProtocol'])
            except KeyError:
                pass

        print("IPv6 Gateway: ", rsp["IPv6DefaultGateway"])
        for address in rsp["IPv6Addresses"]:
            print("IPv6 Address:", address["Address"])
            print("    PrefixLength:", address["PrefixLength"])
            print("          Origin:", address["AddressOrigin"])
            print("           State:", address["AddressState"])
            try:
                print("            Port:", address["oem"]['Port'])
                print("        Protocol:", address["oem"]['SupportedProtocol'])
            except KeyError:
                pass

        self.sys = sys
        self.mac = mac

    def _back_action(self, menu, item):
        eth_menu = EthMenu(sys=self.sys)
        self.set_menu(eth_menu)
