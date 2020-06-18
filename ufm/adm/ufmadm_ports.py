
from ufmmenu import UfmMenu
import ufmapi


class PortsMenu(UfmMenu):
    def __init__(self, fab="", sw=""):
        UfmMenu.__init__(self, name="pts", back_func=self._back_action)

        rsp = ufmapi.redfish_get("/Fabrics/"+fab+"/Switches/"+sw+"/Ports")

        if rsp is None:
            return

        if "Members" not in rsp:
            return

        count = 0
        print()
        print("*  Ports Collection:")

        for member in rsp["Members"]:
            pt = member["@odata.id"].split("/")[8]
            print("      Port: ("+str(count)+")", pt)

            self.add_item(labels=[str(count)],
                          action=self._menu_action, priv=pt, desc=pt)

            count = count + 1

        self.fab = fab
        self.sw = sw
        self.pt = pt
        return

    def _back_action(self, menu, item):
        from ufmadm_switches import SwitchMenu

        switch_menu = SwitchMenu(fab=self.fab, sw=self.sw)
        self.set_menu(switch_menu)
        return

    def _menu_action(self, menu, item):
        pt_menu = PortMenu(fab=self.fab, sw=self.sw, pt=item.priv)
        self.set_menu(pt_menu)
        return


class PortMenu(UfmMenu):
    def __init__(self, fab="", sw="", pt=""):
        if fab is None:
            print("ERROR: Null Port provided.")
            return

        if sw is None:
            print("ERROR: Null Switch provided.")
            return

        if fab is None:
            print("ERROR: Null Fabric provided.")
            return

        UfmMenu.__init__(self, name="pt", back_func=self._back_action)

        rsp = ufmapi.redfish_get("/Fabrics/"+fab+"/Switches/"+sw+"/Ports/"+pt)

        if rsp is None:
            return

        if "Id" not in rsp:
            return

        print()
        print("*        Fabric: ", fab)
        print("*        Switch: ", sw)
        print("*          Port: ", pt)
        print("             Id: ", rsp["Id"])
        print("           Name: ", rsp["Name"])

        self.fab = fab
        self.sw = sw

        return

    def _back_action(self, menu, item):
        ports_menu = PortsMenu(fab=self.fab, sw=self.sw)
        self.set_menu(ports_menu)
        return
