
import json
import ufmapi
from ufmmenu import UfmMenu


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

        UfmMenu.__init__(self, name="sw", back_func=self._back_action)
        self.fab = fab
        self.sw = sw

        self._refresh()
        return

    def _refresh(self):
        self.clear_items()

        rsp = ufmapi.redfish_get("/Fabrics/" + self.fab + "/Switches/" + self.sw)
        if rsp is None:
            return

        if "SerialNumber" not in rsp:
            return

        print()
        print("*        Fabric:", self.fab)
        print("*        Switch:", self.sw)
        print("             Id:", rsp["Id"])
        print("    Description:", rsp["Description"])
        print("           Name:", rsp["Name"])
        print("  Serial Number:", rsp["SerialNumber"])
        print("           UUID:", rsp["UUID"])
        print("            PFC:", rsp["Oem"]["PFC"])
        print("PriorityEnabledList :", rsp["Oem"]["PriorityEnabledList"])
        print("PriorityDisabledList:", rsp["Oem"]["PriorityDisabledList"])
        print()

        if "Actions" in rsp and rsp["Actions"]["#EnablePfcGlobally"]:

            epg = rsp["Actions"]["#EnablePfcGlobally"]
            print("    Action: (EnablePfcGlobally) ")
            self.add_item(labels=["epg", "epg"],
                          action=self._enable_pfc_globally_action,
                          desc=epg["description"])
            self.epg = epg

        if "Actions" in rsp and rsp["Actions"]["#DisablePfcGlobally"]:

            dpg = rsp["Actions"]["#DisablePfcGlobally"]
            print("    Action: (DisablePfcGlobally) ")
            self.add_item(labels=["dpg", "dpg"],
                          action=self._disable_pfc_globally_action,
                          desc=dpg["description"])
            self.dpg = dpg

        if "Actions" in rsp and rsp["Actions"]["#EnablePfcPerPriority"]:

            epp = rsp["Actions"]["#EnablePfcPerPriority"]
            print("    Action: (EnablePfcPerPriority) ")
            self.add_item(labels=["epp", "epp"], args=["<prio_val>"],
                          action=self._enable_pfc_per_priority_action,
                          desc=epp["description"])
            self.epp = epp

        if "Actions" in rsp and rsp["Actions"]["#DisablePfcPerPriority"]:

            dpp = rsp["Actions"]["#DisablePfcPerPriority"]
            print("    Action: (DisablePfcPerPriority) ")
            self.add_item(labels=["dpp", "dpp"], args=["<prio_val>"],
                          action=self._disable_pfc_per_priority_action,
                          desc=dpp["description"])
            self.dpp = dpp

        if "Actions" in rsp and rsp["Actions"]["#AnyCmd"]:

            any_cmd = rsp["Actions"]["#AnyCmd"]
            print("    Action: (AnyCmd) ")
            self.add_item(labels=["any_cmd", "any_cmd"],
                          action=self._any_cmd_action,
                          desc=any_cmd["description"])
            self.any_cmd = any_cmd

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

    def _enable_pfc_globally_action(self, menu, item):
        rsp = ufmapi.redfish_post(self.epg["target"])
        succeeded = ufmapi.print_switch_result(rsp,
                                               'dcb priority-flow-control enable force',
                                               'Successfully Enabled PFC Globally',
                                               'Failed to Enable PFC Globally')
        if succeeded:
            self._refresh()
        return

    def _disable_pfc_globally_action(self, menu, item):
        rsp = ufmapi.redfish_post(self.dpg["target"])

        succeeded = ufmapi.print_switch_result(rsp,
                                               'no dcb priority-flow-control enable force',
                                               'Successfully Disabled PFC Globally',
                                               'Failed to Disable PFC Globally')
        if succeeded:
            self._refresh()
        return

    def _enable_pfc_per_priority_action(self, menu, item):
        argv = item.argv

        prio = int(argv[1], 10)
        if prio is None:
            print("Priority value Undefined, invalid or missing")
            return

        payload = {}
        payload['Priority'] = prio
        rsp = ufmapi.redfish_post(self.epp["target"], payload)

        succeeded = ufmapi.print_switch_result(rsp,
                                               'dcb priority-flow-control priority ' + str(prio) + ' enable',
                                               'Successfully Enabled PFC Per Priority',
                                               'Failed to Enable PFC Per Priority')
        if succeeded:
            self._refresh()
        return

    def _disable_pfc_per_priority_action(self, menu, item):
        argv = item.argv

        prio = int(argv[1], 10)
        if prio is None:
            print("Priority value Undefined, invalid or missing")
            return

        payload = {}
        payload['Priority'] = prio
        rsp = ufmapi.redfish_post(self.dpp["target"], payload)

        succeeded = ufmapi.print_switch_result(rsp,
                                               'no dcb priority-flow-control priority ' + str(prio) + ' enable',
                                               'Successfully Disabled PFC Per Priority',
                                               'Failed to Disable PFC Per Priority')
        if succeeded:
            self._refresh()
        return

    def _any_cmd_action(self, menu, item):
        lst = self.get_all_args()
        any_cmd_str = ' '.join(lst)

        if any_cmd_str is None:
            print("Any Cmd str Undefined, invalid or missing")
            return

        payload = {}
        payload['AnyCmdStr'] = any_cmd_str
        rsp = ufmapi.redfish_post(self.any_cmd["target"], payload)

        succeeded = ufmapi.print_switch_result(rsp,
                                               any_cmd_str,
                                               'Successfully Executed',
                                               'Failed to execute')
        print(json.dumps(rsp, indent=4))
        self.clear_all_args()
        if succeeded:
            self._refresh()
        return
