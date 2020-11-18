# ufmadm_eth.py

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
