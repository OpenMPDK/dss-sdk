from ufmmenu import UfmMenu
import ufmapi


class SwitchesMenu(UfmMenu):
    def __init__(self, fab=""):
        UfmMenu.__init__(self, name="sws", back_func=self._back_action)

        rsp = ufmapi.redfish_get("/Fabrics/" + fab + "/Switches")

        if rsp is None:
            return

        if "Members" not in rsp:
            return

        count = 0
        print()
        print("*  Switches Collection:")

        for member in rsp["Members"]:
            sw = member["@odata.id"].split("/")[6]
            print("      Switch: ("+str(count)+")", sw)

            self.add_item(labels=[str(count)],
                          action=self._menu_action, priv=sw, desc=sw)

            count = count + 1

        self.fab = fab
        return

    def _back_action(self, menu, item):
        from ufmadm_fabrics import FabricMenu

        fabric_menu = FabricMenu(self.fab)
        self.set_menu(fabric_menu)
        return

    def _menu_action(self, menu, item):
        sw_menu = SwitchMenu(fab=self.fab, sw=item.priv)
        self.set_menu(sw_menu)
        return


class SwitchMenu(UfmMenu):
    def __init__(self, fab="", sw=""):
        if sw is None:
            print("ERROR: Null Switch provided.")
            return

        if fab is None:
            print("ERROR: Null Fabric provided.")
            return

        print("fab: " + fab + ", sw: " + sw)
        UfmMenu.__init__(self, name="sw", back_func=self._back_action)

        rsp = ufmapi.redfish_get("/Fabrics/" + fab + "/Switches/" + sw)

        if rsp is None:
            return

        if "SerialNumber" not in rsp:
            return

        print()
        print("*        Fabric:", fab)
        print("*        Switch:", sw)
        print("             Id:", rsp["Id"])
        print("    Description:", rsp["Description"])
        print("           Name:", rsp["Name"])
        print("  Serial Number:", rsp["SerialNumber"])
        print("           UUID:", rsp["UUID"])

        if len(rsp["Ports"]) > 0:
            print("    Ports: Present")
            self.add_item(labels=["pts", "pts"],
                          action=self._ports_action,
                          desc="Ports")

        if len(rsp["VLANs"]) > 0:
            print("    VLANs: Present")
            self.add_item(labels=["vlans", "vlans"],
                          action=self._vlans_action,
                          desc="VLANs")

        self.fab = fab
        self.sw = sw
        return

    def _back_action(self, menu, item):
        switches_menu = SwitchesMenu(fab=self.fab)
        self.set_menu(switches_menu)
        return

    def _ports_action(self, menu, item):
        from ufmadm_ports import PortsMenu

        ports_menu = PortsMenu(fab=self.fab, sw=self.sw)
        self.set_menu(ports_menu)
        return

    def _vlans_action(self, menu, item):
        from ufmadm_vlans import VlansMenu

        vlans_menu = VlansMenu(fab=self.fab, sw=self.sw)
        self.set_menu(vlans_menu)
        return
