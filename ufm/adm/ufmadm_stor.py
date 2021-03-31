# ufmadm_stor.py

"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

from ufmmenu import UfmMenu
import ufmapi


class StorMenu(UfmMenu):
    def __init__(self, sys=""):
        UfmMenu.__init__(self, name="stor",  back_func=self._back_action)

        rsp = ufmapi.rf_get_storage_list(sys)
        if rsp is None:
            return

        count = 0

        print()
        print("*  Subsystem:", sys)
        # print("---------------------------------------------")
        if "oem" in rsp:
            print("    Capacity:", int(rsp["oem"]["CapacityBytes"]/(1024*1024)), "(Mbytes)")
            print("   Available:", rsp["oem"]["PercentAvailable"], "%")

        for member in rsp["Members"]:
            sn = member['@odata.id'].split("/")[6]
            print("     Storage: ("+str(count)+")", sn)

            self.add_item(labels=[str(count)],
                          action=self._sn_action, priv=sn, desc=sn)

            count = count+1

        self.sys = sys
        return

    def _back_action(self, menu, item):
        from ufmadm_sys import SysMenu

        sys_menu = SysMenu(sys=self.sys)
        self.set_menu(sys_menu)

    def _sn_action(self, menu, item):
        sn_menu = SnMenu(sys=self.sys, sn=item.priv)
        self.set_menu(sn_menu)


class SnMenu(UfmMenu):
    def __init__(self, sys="", sn=""):
        UfmMenu.__init__(self, name="sn",  back_func=self._back_action)

        rsp = ufmapi.rf_get_storage(sys, sn)
        if rsp is None:
            return

        count = 0

        print()
        print("*  Subsystem:", sys)
        print("*    Storage:", sn)
        # print("---------------------------------------------")
        if "oem" in rsp:
            print("    Capacity:", int(rsp["oem"]["CapacityBytes"]/(1024*1024)), "(Mbytes)")
            print("   Available:", rsp["oem"]["PercentAvailable"], "%")

        for drive in rsp["Drives"]:
            drv = drive['@odata.id'].split("/")[8]
            print("       Drive: ("+str(count)+")", drv)

            self.add_item(labels=[str(count)],
                          action=self._drv_action, priv=drv, desc=drv)
            count = count+1

        self.sys = sys
        self.sn = sn

    def _back_action(self, menu, item):
        stor_menu = StorMenu(sys=self.sys)
        self.set_menu(stor_menu)

    def _drv_action(self, menu, item):
        drv_menu = DrvMenu(sys=self.sys, sn=self.sn, drv=item.priv)
        self.set_menu(drv_menu)


class DrvMenu(UfmMenu):
    def __init__(self, sys="", sn="", drv=""):
        UfmMenu.__init__(self, name="drv", back_func=self._back_action)

        rsp = ufmapi.rf_get_storage_drive(sys, sn, drv)

        if rsp is None:
            return

        print()
        print("*  Subsystem:", sys)
        print("*    Storage:", sn)
        print("*      Drive:", drv)
        # print("---------------------------------------------")
        print("   BlockSize:", rsp["BlockSizeBytes"], "(Bytes)")
        print("    Capacity:", int(rsp["CapacityBytes"]/(1024*1024)), "(Mbytes)")
        try:
            print("   Available:", rsp["oem"]["PercentAvailable"], "%")
        except KeyError:
            pass

        print("Manufacturer:", rsp["Manufacturer"])
        print("   MediaType:", rsp["MediaType"])
        print("       Model:", rsp["Model"])
        print("    Protocol:", rsp["Protocol"])
        print("    Revision:", rsp["Revision"])
        print("SerialNumber:", rsp["SerialNumber"])

        self.sys = sys
        self.sn = sn
        self.drv = drv

    def _back_action(self, menu, item):
        sn_menu = SnMenu(sys=self.sys, sn=self.sn)
        self.set_menu(sn_menu)
