
from ufmmenu import UfmMenu
from ufmadm_vlans import VlanMenu
import ufmapi


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

        rsp = ufmapi.redfish_get("/Fabrics/"+fab+"/Switches/"+sw+"/Ports/"+pt)

        if rsp is None:
            return

        if "PortId" not in rsp:
            return

        print()
        print("*        Fabric: ", fab)
        print("*        Switch: ", sw)
        print("*        PortId: ", rsp["PortId"])
        print("           Name: ", rsp["Name"])
        print("           Mode: ", rsp["Mode"])
        count = 0

        if "Links" in rsp and rsp["Links"]["AccessVLAN"]:
            access_vlan = rsp["Links"]["AccessVLAN"]["@odata.id"].split("/")[8]
            print("     AccessVLAN:  VLAN: " + access_vlan)

        if "Links" in rsp and rsp["Links"]["AllowedVLANs"]:
            for member in rsp["Links"]["AllowedVLANs"]:
                vlan = member["@odata.id"].split("/")[8]
                print("      VLAN: ("+str(count)+")", vlan)

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
            self.add_item(labels=["uap", "uap"], args=["<vlan_id>"],
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
            self.add_item(labels=["tpr", "tpr"], args=["<vlan_id_start>",
                          "<vlan_id_end>"],
                          action=self._set_trunk_port_range_action,
                          desc=tpr["description"])
            self.tpr = tpr

        return

    def _back_action(self, menu, item):
        ports_menu = PortsMenu(fab=self.fab, sw=self.sw)
        self.set_menu(ports_menu)
        return

    def _menu_action(self, menu, item):
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
        '''
            TODO: remove these two lines once the backend UFM supports the
            Post command
        '''
        print(rsp)
        return

        if rsp['Status'] != 200:
            print("Access Port Set")
        else:
            print("Failed to set Access Port")

        return


    def _unassign_access_port_action(self, menu, item):
        rsp = ufmapi.redfish_post(self.uap["target"])
        '''
            TODO: remove these two lines once the backend UFM supports the
            Post command
        '''
        print(rsp)
        return

        if rsp['Status'] != 200:
            print("Unassigned VLAN from Access Port")
        else:
            print("Failed to Unassign VLAN from Access Port")

        return


    def _set_trunk_port_all_action(self, menu, item):
        rsp = ufmapi.redfish_post(self.tpa["target"])
        '''
            TODO: remove these two lines once the backend UFM supports the
            Post command
        '''
        print(rsp)
        return

        if rsp['Status'] != 200:
            print("Trunk Ports Set")
        else:
            print("Failed to Set Trunk Port")

        return


    def _set_trunk_port_range_action(self, menu, item):
        argv = item.argv

        vlan_id_start = int(argv[1], 10)
        if vlan_id_start is None:
            print("VLAN ID Undefined, invalid or missing")
            return

        vlan_id_end = int(argv[1], 10)
        if vlan_id_end is None:
            print("VLAN ID Undefined, invalid or missing")
            return

        payload = {}
        payload["RangeFromVLANId"] = vlan_id_start
        payload["RangeToVLANId"] = vlan_id_end

        rsp = ufmapi.redfish_post(self.tpr["target"], payload)
        '''
            TODO: remove these two lines once the backend UFM supports the
            Post command
        '''
        print(rsp)
        return

        if rsp['Status'] != 200:
            print("Trunk Ports Set")
        else:
            print("Failed to Set Trunk Port")

        return
