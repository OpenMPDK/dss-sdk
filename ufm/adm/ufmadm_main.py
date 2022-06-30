# ufmadm_main.py

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


class MainMenu(UfmMenu):
    def __init__(self):
        UfmMenu.__init__(self, title="Samsung UFM Managment Utility", usage=self._cli_usage)
        self.add_item(labels=["sys", "sy"], action=self._sys_action, desc="Storage System Management")
        self.add_item(labels=["ufm", "uf"], action=self._ufm_action, desc="UFM System Management")
        self.add_item(labels=["fab", "fb"], action=self._fab_action, desc="Fabrics Management")

    def _sys_action(self, menu, item):
        from ufmadm_sys import SystemsMenu

        sys_menu = SystemsMenu()
        self.set_menu(sys_menu)

    def _ufm_action(self, menu, item):
        from ufmadm_ufm import UFMMenu

        ufm_menu = UFMMenu()
        self.set_menu(ufm_menu)

    def _fab_action(self, menu, item):
        from ufmadm_fabrics import FabricsMenu

        fab_menu = FabricsMenu()
        self.set_menu(fab_menu)

    def _cli_usage(self, menu, item):
        prog = menu.name + ".py"
        print()
        print("        UFM System Managment Adminstration Utility")
        print()
        print("This program can be run in two modes: MENU mode or CLI mode.")
        print()
        print("1) MENU Mode:")
        print()
        print("   USAGE: python " + prog)
        print()
        print("2) CLI Mode:")
        print()
        print("   USAGE: python " + prog + " <command list>")
        print()
        print("     <command list>   :Space separated list of commands.")
        print("                      :The commands will match the order of")
        print("                      :commands as if entering them while")
        print("                      :in MENU mode.")
        print()
        print("     Example: python " + prog + " sys 1 stor 0 1")
        print()
