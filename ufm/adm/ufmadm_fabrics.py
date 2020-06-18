
# ufmadm_sys.py

from ufmmenu import UfmMenu
import ufmapi


class FabricsMenu(UfmMenu):
    def __init__(self):
        UfmMenu.__init__(self, name="fab", back_func=self._back_action)

        rsp = ufmapi.redfish_get("/Fabrics")

        if rsp is None:
            return

        if "Members" not in rsp:
            return

        count = 0
        print()
        print("*  Fabrics Collection:")

        for member in rsp["Members"]:
            fab = member["@odata.id"].split("/")[4]
            print("      Fabric: ("+str(count)+")", fab)

            self.add_item(labels=[str(count)],
                          action=self._menu_action, priv=fab, desc=fab)

            count = count + 1

        return

    def _back_action(self, menu, item):
        from ufmadm_main import MainMenu

        main_menu = MainMenu()
        self.set_menu(main_menu)
        return

    def _menu_action(self, menu, item):
        fab_menu = FabricMenu(fab=item.priv)
        self.set_menu(fab_menu)
        return


class FabricMenu(UfmMenu):
    def __init__(self, fab=""):
        UfmMenu.__init__(self, back_func=self._back_action)

        rsp = ufmapi.redfish_get("/Fabrics/" + fab)

        if rsp is None:
            return

        if "Switches" not in rsp:
            return

        if len(rsp["Switches"]) > 0:
            print("    Switches: Present")
            self.add_item(labels=["sws", "sws"],
                          action=self._switches_action,
                          desc="Switches")

        self.fab = fab
        return

    def _back_action(self, menu, item):
        fabrics_menu = FabricsMenu()
        self.set_menu(fabrics_menu)
        return

    def _switches_action(self, menu, item):
        from ufmadm_switches import SwitchesMenu

        sw_menu = SwitchesMenu(fab=self.fab)
        self.set_menu(sw_menu)
        return
