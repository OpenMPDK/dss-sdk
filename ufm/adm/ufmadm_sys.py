# ufmadm_sys.py

from ufmmenu import UfmMenu
import ufmapi


class SystemsMenu(UfmMenu):
    def __init__(self):
        UfmMenu.__init__(self, name="sys", back_func=self._back_action)

        rsp = ufmapi.rf_get_systems()

        if rsp is None:
            return

        if "Members" not in rsp:
            return

        count = 0
        print()
        print("*  System Collection:")

        for member in rsp["Members"]:
            sys = member["@odata.id"].split("/")[4]
            print("      System: ("+str(count)+")", sys)

            self.add_item(labels=[str(count)],
                          action=self._menu_action, priv=sys, desc=sys)

            count = count + 1

        return

    def _back_action(self, menu, item):
        from ufmadm_main import MainMenu

        main_menu = MainMenu()
        self.set_menu(main_menu)
        return

    def _menu_action(self, menu, item):
        sys_menu = SysMenu(sys=item.priv)
        self.set_menu(sys_menu)
        return


class SysMenu(UfmMenu):
    def __init__(self, sys=""):
        UfmMenu.__init__(self, back_func=self._back_action)

        rsp = ufmapi.rf_get_system(sys)

        if rsp is None:
            return

        if "Identifiers" not in rsp:
            return

        format = rsp["Identifiers"][0]["DurableNameFormat"]

        if format == "NQN":
            self.name = "subsys"
            print()
            print("*  Subsystem:", sys)
            print("         NQN:", rsp["Identifiers"][0]["DurableName"])
            print("       State:", rsp["Status"]["State"])
            print("      Health:", rsp["Status"]["Health"])
            print(" Server Name:", rsp["oem"]["ServerName"])
            print("        NSID:", str(rsp["oem"]["NSID"]))
            print(" NumaAligned:", rsp["oem"]["NumaAligned"])

            if len(rsp["Storage"]) > 0:
                print("     Storage: (stor) Present")
                self.add_item(labels=["stor", "st"], action=self._stor_action,
                              desc="Storage Management")
            else:
                print("     Storage: Not Present")

            if len(rsp["EthernetInterfaces"]) > 0:
                print("    Ethernet: (ethr) Present")
                self.add_item(labels=["ethr", "et"], action=self._eth_action,
                              desc="Ethernet Management")
            else:
                print("    Ethernet: Not Present")

        elif format == "UUID":
            self.name = "system"
            print()
            print("*     System:", sys)
            print("        UUID:", rsp["Identifiers"][0]["DurableName"])

            for address in rsp["IPv4Addresses"]:
                print("IPv4 Address:", address["Address"])
                print("          Subnet:", address["SubnetMask"])
                print("          Origin:", address["AddressOrigin"])
                print("         Gateway:", address["Gateway"])

            print("IPv6 Gateway: ", rsp["IPv6DefaultGateway"])
            for address in rsp["IPv6Addresses"]:
                print("IPv6 Address:", address["Address"])
                print("    PrefixLength:", address["PrefixLength"])
                print("          Origin:", address["AddressOrigin"])
                print("           State:", address["AddressState"])

        else:
            self.name = "unknown"
            print("Unknown DurableNameFormat:", format)

        self.sys = sys
        return

    def _back_action(self, menu, item):
        systems_menu = SystemsMenu()
        self.set_menu(systems_menu)
        return

    def _stor_action(self, menu, item):
        from ufmadm_stor import StorMenu

        stor_menu = StorMenu(sys=self.sys)
        self.set_menu(stor_menu)
        return

    def _eth_action(self, menu, item):
        from ufmadm_eth import EthMenu

        eth_menu = EthMenu(sys=self.sys)
        self.set_menu(eth_menu)
        return
