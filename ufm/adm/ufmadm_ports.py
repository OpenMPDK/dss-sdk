
import json
import ufmapi
from ufmmenu import UfmMenu


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
        print("            PFC: ", rsp["Oem"]["PFC"])
        print("      TrustMode: ", rsp["Oem"]["TrustMode"])
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

        if "Actions" in rsp and rsp["Actions"]["#SetHybridPortAccessVLAN"]:

            hpa = rsp["Actions"]["#SetHybridPortAccessVLAN"]
            print("    Action: (SetHybridPortAccessVLAN) ")
            self.add_item(labels=["hpa", "hpa"], args=["<vlan_id>"],
                          action=self._set_hybrid_port_access_vlan_action,
                          desc=hpa["description"])
            self.hpa = hpa

        if "Actions" in rsp and rsp["Actions"]["#SetHybridPortAllowedVLAN"]:

            hpl = rsp["Actions"]["#SetHybridPortAllowedVLAN"]
            print("    Action: (SetHybridPortAllowedVLAN) ")
            self.add_item(labels=["hpl", "hpl"], args=["<vlan_id>"],
                          action=self._set_hybrid_port_allowed_vlan_action,
                          desc=hpl["description"])
            self.hpl = hpl

        if "Actions" in rsp and rsp["Actions"]["#RemoveHybridPortAllowedVLAN"]:

            rhl = rsp["Actions"]["#RemoveHybridPortAllowedVLAN"]
            print("    Action: (RemoveHybridPortAllowedVLAN) ")
            self.add_item(labels=["rhl", "rhl"], args=["<vlan_id>"],
                          action=self._remove_hybrid_port_allowed_vlan_action,
                          desc=rhl["description"])
            self.rhl = rhl

        if "Actions" in rsp and rsp["Actions"]["#EnablePortPfc"]:

            epp = rsp["Actions"]["#EnablePortPfc"]
            print("    Action: (EnablePortPfc) ")
            self.add_item(labels=["epp", "epp"],
                          action=self._enable_port_pfc_action,
                          desc=epp["description"])
            self.epp = epp

        if "Actions" in rsp and rsp["Actions"]["#DisablePortPfc"]:

            dpp = rsp["Actions"]["#DisablePortPfc"]
            print("    Action: (DisablePortPfc) ")
            self.add_item(labels=["dpp", "dpp"],
                          action=self._disable_port_pfc_action,
                          desc=dpp["description"])
            self.dpp = dpp

        if "Actions" in rsp and rsp["Actions"]["#EnableEcnMarking"]:

            eem = rsp["Actions"]["#EnableEcnMarking"]
            print("    Action: (EnableEcnMarking) ")
            self.add_item(labels=["eem", "eem"],
                          args=["<traffic_cls>", "<min>", "<max>"],
                          action=self._enable_ecn_marking_action,
                          desc=eem["description"])
            self.eem = eem

        if "Actions" in rsp and rsp["Actions"]["#DisableEcnMarking"]:

            dem = rsp["Actions"]["#DisableEcnMarking"]
            print("    Action: (DisableEcnMarking) ")
            self.add_item(labels=["dem", "dem"],
                          args=["<traffic_cls>"],
                          action=self._disable_ecn_marking_action,
                          desc=dem["description"])
            self.dem = dem

        if "Actions" in rsp and rsp["Actions"]["#ShowPfcCounters"]:

            spc = rsp["Actions"]["#ShowPfcCounters"]
            print("    Action: (ShowPfcCounters) ")
            self.add_item(labels=["spc", "spc"], args=["<prio_val>"],
                          action=self._show_pfc_counters_action,
                          desc=spc["description"])
            self.spc = spc

        if "Actions" in rsp and rsp["Actions"]["#ShowCongestionControl"]:

            scc = rsp["Actions"]["#ShowCongestionControl"]
            print("    Action: (ShowCongestionControl) ")
            self.add_item(labels=["scc", "scc"],
                          action=self._show_congestion_control_action,
                          desc=scc["description"])
            self.scc = scc

        if "Actions" in rsp and rsp["Actions"]["#ShowBufferDetails"]:

            sbd = rsp["Actions"]["#ShowBufferDetails"]
            print("    Action: (ShowBufferDetails) ")
            self.add_item(labels=["sbd", "sbd"],
                          action=self._show_buffer_details_action,
                          desc=sbd["description"])
            self.sbd = sbd

        if "Actions" in rsp and rsp["Actions"]["#BindPriorityToBuffer"]:

            bpb = rsp["Actions"]["#BindPriorityToBuffer"]
            print("    Action: (BindPriorityToBuffer) ")
            self.add_item(labels=["bpb", "bpb"], args=["<buf_name>", "<prio>"],
                          action=self._bind_port_priority_to_specific_buffer_action,
                          desc=bpb["description"])
            self.bpb = bpb

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

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' switchport access vlan ' + str(vlan_id),
            'Successfully set Access Port VLAN',
            'Failed to set Access Port VLAN')

        if succeeded:
            self._refresh()
        return

    def _unassign_access_port_action(self, menu, item):
        payload = {}
        payload["port_id"] = self.pt

        rsp = ufmapi.redfish_post(self.uap["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' no switchport access vlan',
            'Successfully unassign VLAN from Access Port',
            'Failed to Unassign VLAN from Access Port')

        if succeeded:
            self._refresh()
        return

    def _set_trunk_port_all_action(self, menu, item):
        payload = {}
        payload["port_id"] = self.pt

        rsp = ufmapi.redfish_post(self.tpa["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' switchport trunk allowed-vlan all',
            'Successfully set port to trunk mode and allow vlan all',
            'Failed to set port to trunk mode and allow vlan all')

        if succeeded:
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

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' switchport trunk allowed-vlan ' +
            str(start_vlan_id) + '-' + str(end_vlan_id),
            'Successfully set port to trunk mode and allow vlan range',
            'Failed to set port to trunk mode and allow vlan range')

        if succeeded:
            self._refresh()
        return

    def _set_hybrid_port_access_vlan_action(self, menu, item):
        argv = item.argv

        vlan_id = int(argv[1], 10)
        if vlan_id is None:
            print("VLAN ID Undefined, invalid or missing")
            return

        payload = {}
        payload["VLANId"] = vlan_id

        rsp = ufmapi.redfish_post(self.hpa["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' switchport access vlan ' + str(vlan_id),
            'Successfully set Hybrid Port Access VLAN',
            'Failed to set Hybrid Port Access VLAN')

        if succeeded:
            self._refresh()
        return

    def _set_hybrid_port_allowed_vlan_action(self, menu, item):
        argv = item.argv

        vlan_id = int(argv[1], 10)
        if vlan_id is None:
            print("VLAN ID Undefined, invalid or missing")
            return

        payload = {}
        payload["VLANId"] = vlan_id

        rsp = ufmapi.redfish_post(self.hpl["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' switchport hybrid allowed-vlan add ' + str(vlan_id),
            'Successfully add Hybrid Port Allowed VLAN',
            'Failed to add Hybrid Port Allowed VLAN')

        if succeeded:
            self._refresh()
        return

    def _remove_hybrid_port_allowed_vlan_action(self, menu, item):
        argv = item.argv

        vlan_id = int(argv[1], 10)
        if vlan_id is None:
            print("VLAN ID Undefined, invalid or missing")
            return

        payload = {}
        payload["VLANId"] = vlan_id

        rsp = ufmapi.redfish_post(self.rhl["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' switchport hybrid allowed-vlan remove ' + str(vlan_id),
            'Successfully Remove  Hybrid Port Allowed VLAN',
            'Failed to Remove Hybrid Port Allowed VLAN')

        if succeeded:
            self._refresh()
        return

    def _enable_port_pfc_action(self, menu, item):
        payload = {}
        payload["port_id"] = self.pt

        rsp = ufmapi.redfish_post(self.epp["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' dcb priority-flow-control mode on force',
            'Successfully Enable Port PFC',
            'Failed to Enable Port PFC')

        if succeeded:
            self._refresh()
        return

    def _disable_port_pfc_action(self, menu, item):
        payload = {}
        payload["port_id"] = self.pt

        rsp = ufmapi.redfish_post(self.dpp["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' no dcb priority-flow-control mode force',
            'Successfully Disable Port PFC',
            'Failed to Disable Port PFC')

        if succeeded:
            self._refresh()
        return

    def _show_pfc_counters_action(self, menu, item):
        argv = item.argv

        prio = int(argv[1], 10)
        if prio is None:
            print("Priority Val Undefined, invalid or missing")
            return

        payload = {}
        payload["port_id"] = self.pt
        payload["Priority"] = prio

        rsp = ufmapi.redfish_post(self.spc["target"], payload)

        ufmapi.print_switch_result(
            rsp,
            'show interfaces ethernet 1/' + str(self.pt) + ' counters pfc prio ' + str(prio),
            'Successfully Show Port PFC Counters',
            'Failed to Show Port PFC Counters')

        print(json.dumps(rsp, indent=4))
        return

    def _show_congestion_control_action(self, menu, item):
        payload = {}
        payload["port_id"] = self.pt

        rsp = ufmapi.redfish_post(self.scc["target"], payload)

        ufmapi.print_switch_result(
            rsp,
            'show interfaces ethernet 1/' + str(self.pt) + ' congestion-control',
            'Successfully Show port congestion control info',
            'Failed to Show port congestion control info')

        print(json.dumps(rsp, indent=4))
        return

    def _enable_ecn_marking_action(self, menu, item):
        argv = item.argv

        tc = int(argv[1], 10)
        if tc is None:
            print("Traffic Class Undefined, invalid or missing")
            return

        min_ab = int(argv[2], 10)
        if min_ab is None:
            print("Minimum Absolute Val Undefined, invalid or missing")
            return

        max_ab = int(argv[3], 10)
        if min_ab is None:
            print("Maxmum Absolute Val Undefined, invalid or missing")
            return

        payload = {}
        payload["port_id"] = self.pt
        payload["TrafficClass"] = tc
        payload["MinAbsoluteInKBs"] = min_ab
        payload["MaxAbsoluteInKBs"] = max_ab

        rsp = ufmapi.redfish_post(self.eem["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' traffic-class ' + str(tc) +
            ' congestion-control ecn minimum-absolute ' + str(min_ab)
            + ' maximum-absolute ' + str(max_ab),
            'Successfully Enabled ECN Marking',
            'Failed to Enable ECN Marking')

        if succeeded:
            self._refresh()
        return

    def _disable_ecn_marking_action(self, menu, item):
        argv = item.argv

        tc = int(argv[1], 10)
        if tc is None:
            print("Traffic Class Undefined, invalid or missing")
            return

        payload = {}
        payload["port_id"] = self.pt
        payload["TrafficClass"] = tc

        rsp = ufmapi.redfish_post(self.dem["target"], payload)

        succeeded = ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' no traffic-class ' + str(tc) + ' congestion-control',
            'Successfully Disabled ECN Marking',
            'Failed to Disable ECN Marking')

        if succeeded:
            self._refresh()
        return

    def _show_buffer_details_action(self, menu, item):
        payload = {}
        payload["port_id"] = self.pt

        rsp = ufmapi.redfish_post(self.sbd["target"], payload)

        ufmapi.print_switch_result(
            rsp,
            'show buffers details interfaces ethernet 1/' + str(self.pt),
            'Successfully Show port buffer details',
            'Failed to Show port buffer details')

        print(json.dumps(rsp, indent=4))
        return

    def _bind_port_priority_to_specific_buffer_action(self, menu, item):
        argv = item.argv

        buf_name = argv[1]
        if buf_name is None:
            print("Buffer name Undefined, invalid or missing")
            return
        buf_name = buf_name.replace('iport', 'iPort')

        prio = int(argv[2], 10)
        if prio is None:
            print("Switch Priority Undefined, invalid or missing")
            return

        payload = {}
        payload["port_id"] = self.pt
        payload["Buffer"] = buf_name
        payload["Priority"] = prio

        rsp = ufmapi.redfish_post(self.bpb["target"], payload)

        ufmapi.print_switch_result(
            rsp,
            'interface ethernet 1/' + str(self.pt) + ' ingress-buffer ' + buf_name +
            ' bind switch-priority ' + str(prio),
            'Successfully Bind Switch Priority to Specific Buffer',
            'Failed to Bind Priority to Specific Buffer')

        return
