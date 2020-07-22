# ufmadm_stor.py

from ufmmenu import UfmMenu
import ufmapi


class StorMenu(UfmMenu):
    def __init__(self, sys=""):
        UfmMenu.__init__(self, name="stor",  back_func=self._back_action)

        rsp = ufmapi.rf_get_storage_list(sys)

        if rsp == None:
            return

        if "oem" not in rsp:
            return

        count = 0

        print()
        print("*  Subsystem:",sys)
        #print("---------------------------------------------")
        print("    Capacity:",int(rsp["oem"]["CapacityBytes"]/(1024*1024)),"(Mbytes)")
        print("   Available:",rsp["oem"]["PercentAvailable"], "%")

        for member in rsp["Members"]:
            sn = member['@odata.id'].split("/")[6]
            print("     Storage: ("+str(count)+")",sn)

            self.add_item(labels=[str(count)], action=self._sn_action, \
                priv=sn, desc=sn)

            count = count+1

        self.sys = sys
        return

    def _back_action(self, menu, item):
        from ufmadm_sys import SysMenu

        sys_menu = SysMenu(sys=self.sys)
        self.set_menu(sys_menu)
        return

    def _sn_action(self, menu, item):
        sn_menu = SnMenu(sys=self.sys, sn=item.priv)
        self.set_menu(sn_menu)
        return


class SnMenu(UfmMenu):
    def __init__(self, sys="", sn=""):
        UfmMenu.__init__(self, name="sn",  back_func=self._back_action)

        rsp = ufmapi.rf_get_storage(sys, sn)

        if rsp == None:
            return

        if "oem" not in rsp:
            return

        count = 0

        print()
        print("*  Subsystem:",sys)
        print("*    Storage:",sn)
        #print("---------------------------------------------")
        print("    Capacity:",int(rsp["oem"]["CapacityBytes"]/(1024*1024)),"(Mbytes)")
        print("   Available:",rsp["oem"]["PercentAvailable"], "%")

        for drive in rsp["Drives"]:
            drv = drive['@odata.id'].split("/")[8]
            print("       Drive: ("+str(count)+")", drv)

            self.add_item(labels=[str(count)], action=self._drv_action, \
                priv=drv, desc=drv)
            count = count+1

        self.sys = sys
        self.sn = sn
        return

    def _back_action(self, menu, item):
        stor_menu = StorMenu(sys=self.sys)
        self.set_menu(stor_menu)
        return

    def _drv_action(self, menu, item):
        drv_menu = DrvMenu(sys=self.sys, sn=self.sn, drv=item.priv)
        self.set_menu(drv_menu)
        return



class DrvMenu(UfmMenu):
    def __init__(self, sys="", sn="", drv=""):
        UfmMenu.__init__(self, name="drv", back_func=self._back_action)

        rsp = ufmapi.rf_get_storage_drive(sys, sn, drv)

        if rsp == None:
            return

        print()
        print("*  Subsystem:",sys)
        print("*    Storage:",sn)
        print("*      Drive:",drv)
        #print("---------------------------------------------")
        print("   BlockSize:",rsp["BlockSizeBytes"], "(Bytes)")
        print("    Capacity:",int(rsp["CapacityBytes"]/(1024*1024)),"(Mbytes)")
        print("   Available:",rsp["oem"]["PercentAvailable"], "%")
        print("Manufacturer:",rsp["Manufacturer"])
        print("   MediaType:",rsp["MediaType"])
        print("       Model:",rsp["Model"])
        print("    Protocol:",rsp["Protocol"])
        print("    Revision:",rsp["Revision"])
        print("SerialNumber:",rsp["SerialNumber"])

        self.sys = sys
        self.sn = sn
        self.drv = drv
        return

    def _back_action(self, menu, item):
        sn_menu = SnMenu(sys=self.sys, sn=self.sn)
        self.set_menu(sn_menu)
        return
