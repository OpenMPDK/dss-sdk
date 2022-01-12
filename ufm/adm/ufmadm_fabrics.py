# ufmadm_sys.py

# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


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

    def _back_action(self, menu, item):
        from ufmadm_main import MainMenu

        main_menu = MainMenu()
        self.set_menu(main_menu)

    def _menu_action(self, menu, item):
        fab_menu = FabricMenu(fab=item.priv)
        self.set_menu(fab_menu)


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

    def _back_action(self, menu, item):
        fabrics_menu = FabricsMenu()

        self.set_menu(fabrics_menu)

    def _switches_action(self, menu, item):
        from ufmadm_switches import SwitchesMenu

        sw_menu = SwitchesMenu(fab=self.fab)
        self.set_menu(sw_menu)
