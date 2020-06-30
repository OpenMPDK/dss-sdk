
from ufmmenu import UfmMenu
import ufmapi
import ufmadm_util


class PortsMenu(UfmMenu):
    def __init__(self, fab="", sw=""):
        UfmMenu.__init__(self, name="pts", back_func=self._back_action)

        self.fab = fab
        self.sw = sw
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
            pt_display = 'Eth1/' + pt
            print("      Port: ("+str(count)+")", pt_display)

            self.add_item(labels=[str(count)],
                          action=self._menu_action, priv=pt, desc=pt_display)

            count = count + 1

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
        self.fab = fab
        self.sw = sw
        self.pt = pt

        self._refresh()
        return

    def _refresh(self):
        self.clear_items()

        rsp = ufmapi.redfish_get("/Fabrics/"+self.fab+"/Switches/"+self.sw+"/Ports/"+self.pt)
        if rsp is None:
            return

        if "PortId" not in rsp:
            return

        print()
        print("*        Fabric: ", self.fab)
        print("*        Switch: ", self.sw)
        print("*          Port: ", rsp["PortId"])
        print("*            Id: ", rsp["Id"])
        print("           Name: ", rsp["Name"])
        print("           Mode: ", rsp["Mode"])
        count = 0

        if "Links" in rsp and rsp["Links"]["AccessVLAN"]:
            access_vlan = rsp["Links"]["AccessVLAN"]["@odata.id"].split("/")[8]
            print("     AccessVLAN:  VLAN: " + access_vlan)

            self.add_item(labels=[str(count)],
                          action=self._menu_action, priv=access_vlan,
                          desc="VLAN: " + access_vlan)

            count = count + 1

        if "Links" in rsp and rsp["Links"]["AllowedVLANs"]:
            for member in rsp["Links"]["AllowedVLANs"]:
                vlan = member["@odata.id"].split("/")[8]
                print("   Allowed VLAN: ("+str(count)+")", vlan)

                self.add_item(labels=[str(count)],
                              action=self._menu_action, priv=vlan,
                              desc="VLAN: " + vlan)

                count = count + 1

        print()

        if "Actions" in rsp and rsp["Actions"]["#SetAccessPortVLAN"]:

            sap = rsp["Actions"]["#SetAccessPortVLAN"]
            print("    Action: (SetAccessPortVLAN) ")
            self.add_item(labels=["sap", "sap"], args=["<vlan_id>"],
                          action=self._set_access_port_action,
                          desc=sap["description"])
            self.sap = sap


        if "Actions" in rsp and rsp["Actions"]["#UnassignAccessPortVLAN"]:

            uap = rsp["Actions"]["#UnassignAccessPortVLAN"]
            print("    Action: (UnassignAccessPortVLAN) ")
            self.add_item(labels=["uap", "uap"],
                          action=self._unassign_access_port_action,
                          desc=uap["description"])
            self.uap = uap


        if "Actions" in rsp and rsp["Actions"]["#SetTrunkPortVLANsAll"]:

            tpa = rsp["Actions"]["#SetTrunkPortVLANsAll"]
            print("    Action: (SetTrunkPortVLANsAll) ")
            self.add_item(labels=["tpa", "tpa"],
                          action=self._set_trunk_port_all_action,
                          desc=tpa["description"])
            self.tpa = tpa

        if "Actions" in rsp and rsp["Actions"]["#SetTrunkPortVLANsRange"]:

            tpr = rsp["Actions"]["#SetTrunkPortVLANsRange"]
            print("    Action: (SetTrunkPortVLANsRange) ")
            self.add_item(labels=["tpr", "tpr"], args=["<start_vlan_id>",
                          "<end_vlan_id>"],
                          action=self._set_trunk_port_range_action,
                          desc=tpr["description"])
            self.tpr = tpr

        return

    def _back_action(self, menu, item):
        ports_menu = PortsMenu(fab=self.fab, sw=self.sw)
        self.set_menu(ports_menu)
        return

    def _menu_action(self, menu, item):
        from ufmadm_vlans import VlanMenu

        vlan_menu = VlanMenu(fab=self.fab, sw=self.sw, vlan=item.priv)
        self.set_menu(vlan_menu)
        return

    def _set_access_port_action(self, menu, item):
        argv = item.argv

        vlan_id = int(argv[1], 10)
        if vlan_id is None:
            print("VLAN ID Undefined, invalid or missing")
            return

        payload = {}
        payload["VLANId"] = vlan_id

        rsp = ufmapi.redfish_post(self.sap["target"], payload)

        ufmadm_util.print_switch_result(rsp,
            'switchport access vlan ' + str(vlan_id),
            'Successfully set Access Port VLAN',
            'Failed to set Access Port VLAN')

        self._refresh()
        return


    def _unassign_access_port_action(self, menu, item):
        payload = {}
        payload["port_id"] = self.pt

        rsp = ufmapi.redfish_post(self.uap["target"], payload)

        ufmadm_util.print_switch_result(rsp,
            'no switchport access vlan',
            'Successfully unassign VLAN from Access Port',
            'Failed to Unassign VLAN from Access Port')

        self._refresh()
        return


    def _set_trunk_port_all_action(self, menu, item):
        payload = {}
        payload["port_id"] = self.pt

        rsp = ufmapi.redfish_post(self.tpa["target"], payload)

        ufmadm_util.print_switch_result(rsp,
                                        'switchport trunk allowed-vlan all',
                                        'Successfully set port to trunk mode and allow vlan all',
                                        'Failed to set port to trunk mode and allow vlan all')

        self._refresh()
        return


    def _set_trunk_port_range_action(self, menu, item):
        argv = item.argv

        start_vlan_id = int(argv[1], 10)
        if start_vlan_id is None:
            print("Start VLAN ID Undefined, invalid or missing")
            return

        end_vlan_id = int(argv[2], 10)
        if end_vlan_id is None:
            print("End VLAN ID Undefined, invalid or missing")
            return

        payload = {}
        payload["RangeFromVLANId"] = start_vlan_id
        payload["RangeToVLANId"] = end_vlan_id

        rsp = ufmapi.redfish_post(self.tpr["target"], payload)

        ufmadm_util.print_switch_result(rsp,
            'switchport trunk allowed-vlan ' + str(start_vlan_id) + '-' + str(end_vlan_id),
            'Successfully set port to trunk mode and allow vlan range',
            'Failed to set port to trunk mode and allow vlan range')

        self._refresh()
        return
