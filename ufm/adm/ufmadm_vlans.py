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
        print("*  VLANs Collection:")

        for member in rsp["Members"]:
            vlan = member["@odata.id"].split("/")[8]
            vlan_display = "VLAN " + vlan
            print("      VLAN: ("+str(count)+")", vlan_display)

            self.add_item(labels=[str(count)],
                          action=self._menu_action, priv=vlan, desc=vlan_display)

            count = count + 1

        if "Actions" in rsp and rsp["Actions"]["#CreateVLAN"]:

            crt = rsp["Actions"]["#CreateVLAN"]
            print("    Action: (CreateVLAN) ")
            self.add_item(labels=["crt", "crt"], args=["<vlan_id>"],
                          action=self._create_action,
                          desc=crt["description"])
            self.crt = crt

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
        payload["VLANId"] = vlan_id

        rsp = ufmapi.redfish_post(self.crt["target"], payload)
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

        if rsp["Name"]:
            print("           Name: ", rsp["Name"])
        else:
            print("           Name: (empty)")

        print("     VLANEnable: ", rsp["VLANEnable"])
        print("         VLANId: ", rsp["VLANId"])
        print()

        if "Actions" in rsp and rsp["Actions"]["#DeleteVLAN"]:

            dlt = rsp["Actions"]["#DeleteVLAN"]
            print("    Action: (DeleteVLAN) ")
            self.add_item(labels=["dlt", "dlt"],
                          action=self._delete_vlan_action,
                          desc=dlt["description"])
            self.dlt = dlt

        if "Links" in rsp and rsp["Links"]["Ports"]:
            if len(rsp["Links"]["Ports"]) > 0:
                count = 0
                print()
                print("*  Ports Collection:")

                for member in rsp["Links"]["Ports"]:
                    pt = member["@odata.id"].split("/")[8]
                    pt_display = 'Eth1/' + pt
                    print("      Port: ("+str(count)+")", pt_display)

                    self.add_item(labels=[str(count)],
                                  action=self._menu_action, priv=pt,
                                  desc="Port: " + pt_display)

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


    def _delete_vlan_action(self, menu, item):
        rsp = ufmapi.redfish_post(self.dlt["target"])
        '''
            TODO: remove these two lines once the backend UFM supports the
            Post command
        '''
        print(rsp)
        return

        if rsp['Status'] != 200:
            print("VLAN deleted")
        else:
            print("Failed to delete VLAN")

        return


