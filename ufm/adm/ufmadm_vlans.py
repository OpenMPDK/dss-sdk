from ufmmenu import UfmMenu
import ufmapi


class VlansMenu(UfmMenu):
    def __init__(self, fab="", sw=""):
        UfmMenu.__init__(self, name="vlans", back_func=self._back_action)

        rsp = ufmapi.redfish_get(
            "/Fabrics/" + fab + "/Switches/" + sw + "/VLANs")

        if rsp is None:
            return

        if "Members" not in rsp:
            return

        count = 0
        print()
        print("*  Vlans Collection:")

        for member in rsp["Members"]:
            vlan = member["@odata.id"].split("/")[8]
            print("      vlan: ("+str(count)+")", vlan)

            self.add_item(labels=[str(count)],
                          action=self._menu_action, priv=vlan, desc=vlan)

            count = count + 1

        print("    Action: (create) Create VLAN")
        self.add_item(labels=["create", "cr"], args=["<vlan_id>"],
                      action=self._create_action,
                      desc="Create new vlan with id")

        self.fab = fab
        self.sw = sw
        self.vlan = vlan
        return

    def _back_action(self, menu, item):
        from ufmadm_switches import SwitchMenu

        switch_menu = SwitchMenu(fab=self.fab, sw=self.sw)
        self.set_menu(switch_menu)
        return

    def _menu_action(self, menu, item):
        vlan_menu = VlanMenu(fab=self.fab, sw=self.sw, vlan=item.priv)
        self.set_menu(vlan_menu)
        return

    def _create_action(self, menu, item):
        argv = item.argv

        vlan_id = int(argv[1], 10)
        if vlan_id is None:
            print("VLAN ID Undefined, invalid or missing")
            return

        payload = {}
        payload["id"] = vlan_id

        rsp = ufmapi.redfish_post(
            "/Fabrics/"
            + self.fab + "/Switches/" + self.sw + "/VLANs/" + self.vlan,
            payload)
        '''
            TODO: remove these two lines once the backend UFM supports the
            Post command
        '''
        print(rsp)
        return

        if rsp['Status'] != 200:
            print("VLAN Create request failed.")
        else:
            print("VLAN Created.")

        return


class VlanMenu(UfmMenu):
    def __init__(self, fab="", sw="", vlan=""):
        if vlan is None:
            print("ERROR: Null vlan provided.")
            return

        if sw is None:
            print("ERROR: Null Switch provided.")
            return

        if fab is None:
            print("ERROR: Null Fabric provided.")
            return

        self.fab = fab
        self.sw = sw
        UfmMenu.__init__(self, name="vlan", back_func=self._back_action)

        rsp = ufmapi.redfish_get(
                "/Fabrics/" + fab + "/Switches/" + sw + "/VLANs/" + vlan)

        if rsp is None:
            return

        if "Id" not in rsp:
            return

        print()
        print("*        Fabric: ", fab)
        print("*        Switch: ", sw)
        print("*          VLAN: ", vlan)
        print("             Id: ", rsp["Id"])
        print("    Description: ", rsp["Description"])
        print("           Name: ", rsp["Name"])
        print("     VLANEnable: ", rsp["VLANEnable"])
        print("         VLANId: ", rsp["VLANId"])

        if "Links" in rsp and rsp["Links"]["Ports"]:
            count = 0
            print()
            print("*  Ports Collection:")

            for member in rsp["Links"]["Ports"]:
                pt = member["@odata.id"].split("/")[8]
                print("      Port: ("+str(count)+")", pt)

                self.add_item(labels=[str(count)],
                              action=self._menu_action, priv=pt,
                              desc="Port: " + pt)

                count = count + 1

        return

    def _back_action(self, menu, item):
        switches_menu = VlansMenu(fab=self.fab, sw=self.sw)
        self.set_menu(switches_menu)
        return

    def _menu_action(self, menu, item):
        from ufmadm_ports import PortMenu

        port_menu = PortMenu(fab=self.fab, sw=self.sw, pt=item.priv)
        self.set_menu(port_menu)
        return
