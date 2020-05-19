# ufmadm_main.py

from ufmmenu import UfmMenu

class MainMenu(UfmMenu):
    def __init__(self):
        UfmMenu.__init__(self, title="Samsung UFM Managment Utility", \
            usage=self._cli_usage)

        self.add_item(labels=["sys", "sy"], action=self._sys_action, \
            desc="Storage System Management")

        self.add_item(labels=["ufm", "uf"], action=self._ufm_action, \
            desc="UFM System Management")
        return

    def _sys_action(self, menu, item):
        from ufmadm_sys import SystemsMenu

        sys_menu = SystemsMenu()
        self.set_menu(sys_menu)
        return

    def _ufm_action(self, menu, item):
        from ufmadm_ufm import UFMMenu

        ufm_menu = UFMMenu()
        self.set_menu(ufm_menu)
        return

    def _cli_usage(self, menu, item):
        prog = menu.name + ".py"
        print()
        print("        UFM System Managment Adminstration Utility")
        print()
        print("This program can be run in two modes: MENU mode or CLI mode.")
        print()
        print("1) MENU Mode:") 
        print()
        print("   USAGE: python "+prog)
        print()
        print("2) CLI Mode:") 
        print()
        print("   USAGE: python "+prog+" <command list>")
        print()
        print("     <command list>   :Space separated list of commands.")
        print("                      :The commands will match the order of")
        print("                      :commands as if entering them while")
        print("                      :in MENU mode.")
        print()
        print("     Example: python "+prog+" sys 1 stor 0 1")
        print()
        return

