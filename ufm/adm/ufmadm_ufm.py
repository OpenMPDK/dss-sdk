# ufmadm_ufm.py

from ufmmenu import UfmMenu
import ufmapi
import time

class UFMMenu(UfmMenu):
    def __init__(self):
        Hidden = False

        UfmMenu.__init__(self, name="ufm", back_func=self._back_action)

        rsp = ufmapi.rf_get_ufm()

        if rsp == None:
            return

        if "UUID" not in rsp:
            return

        print()
        print("*  UFM System Management:")
        print("        UUID:",rsp["UUID"])

        print("     Manager: (log) UFM Log Management")
        self.add_item(labels=["log", "lo"], action=self._log_action, desc="UFM Log Management")

        if Hidden == False:
            print("      Action: (kill) Shutdown UFM")
            print("      Action: (restart) Restart UFM")

        self.add_item(labels=["kill", "ki"], action=self._kill_action, desc="Shutdown UFM", \
            hidden=Hidden)

        self.add_item(labels=["restart", "re"], action=self._restart_action, desc="Restart UFM", \
            hidden=Hidden)

        return

    def _back_action(self, menu, item):
        from ufmadm_main import MainMenu

        main_menu = MainMenu()
        self.set_menu(main_menu)
        return

    def _log_action(self, menu, item):
        log_menu = LogMenu()
        self.set_menu(log_menu)
        return

    def _kill_action(self, menu, item):
        error = ufmapi.ufm_shutdown()

        if error == 0:
            print("UFM Shutdown requested.")
        else:
            print("UFM Shutdown request failed.")

        return

    def _restart_action(self, menu, item):
        error = ufmapi.ufm_restart()

        if error == 0:
            print("UFM Restart requested.")
        else:
            print("UFM Restart request failed.")

        return

class LogMenu(UfmMenu):
    def __init__(self):
        UfmMenu.__init__(self, name="log", back_func=self._back_action)

        rsp = ufmapi.rf_get_ufm_log()

        if rsp == None:
           return

        if "MaxNumberOfRecords" not in rsp:
            return

        print()
        print("*  UFM Log Management:")

        self.max_entries = rsp["MaxNumberOfRecords"]
        print("  MaxRecords:", self.max_entries)

        print("      Policy:", rsp["OverWritePolicy"])

        print("      Action: (clear) Clear the log buffer")
        self.add_item(labels=["clear","cl"], action=self._clear_action, \
            desc="Clear the log buffer")

        print("      Action: (show) Show the last <count> log entries")
        self.add_item(labels=["show","sh"], action=self._show_action, args=['<count>'], \
            desc="Show the last <count> log entries")

        print("      Action: (disp) Continuously display log entries")
        self.add_item(labels=["disp","di"], action=self._disp_action, \
            desc="Continuously display log entries")

        print("      Action: (getreg) Get the module mask registry")
        self.add_item(labels=["getreg", "gr"], \
            action=self._getreg_action, desc="Get the module mask registry")
 
        print("      Action: (getmask) Get verbosity mask(s)")
        self.add_item(labels=["getmask", "gm"], args=["<all,error,warning,info,debug,detail>"], \
            action=self._getmask_action, desc="Get verbosity mask(s)")
 
        print("      Action: (setmask) Set verbosity mask")
        self.add_item(labels=["setmask", "sm"], args=["<all,error,warning,info,debug,detail>", "<hex_value>"], \
            action=self._setmask_action, desc="Set verbosity mask(s)")
 
        return

    def _back_action(self, menu, item):
        ufm_menu = UFMMenu()
        self.set_menu(ufm_menu)
        return

    def _clear_action(self, menu, item):
        error = ufmapi.ufm_clear_log()

        if error == 1:
            print("Clear log request failed.")
        else:
            print("Clear log requested.")

        return

    def _disp_action(self, menu, item):
        offset = 0
        count  = self.max_entries

        print("Break (Ctrl-C) to exit.")
        print()

        try:
            while True:
                entries = ufmapi.ufm_get_log_entries(offset, count)

                if entries == None:
                    break 

                entry_count = len(entries)
                if entry_count == 0:
                    continue 

                for entry in entries:
                    tim = time.ctime(entry.timestamp)
                    print("%-18s: %d: %s: %s: %s" % (tim, entry.id, entry.module,\
                        entry.type, entry.msg))

                offset = entries[entry_count-1].id
                
                if entry_count < self.max_entries:
                    time.sleep(2)
        except:
            print("Log Display Terminated.")

        return

    def _show_action(self, menu, item):
        count = int(item.argv[1])
        entries = ufmapi.ufm_get_log_entries(-1, count)

        if entries == None:
            print()
            print("ERROR: Unable to contact UFM.")
            return 

        if len(entries) == 0:
            print("Log buffer empty.")
            return

        for entry in entries:
            tim = time.ctime(entry.timestamp)
            print("%-18s: %d: %s: %s: %s" % (tim, entry.id, entry.module,\
                entry.type, entry.msg))

        if menu.cli == False:
            input("Press enter to continue:")

        return

    
    def _getreg_action(self, menu, item):
        reg = ufmapi.ufm_get_module_registry()

        if reg == None:
            return

        print("  Module Mask Registry:")
        for mod in reg:
            print("%20s: 0x%08X" % (mod, reg[mod]))

        return


    def _setmask_action(self, menu, item):
        argv = item.argv

        if argv[2] == "0":
            # Special case
            pass
        elif argv[2][:2] != "0x":
            print("Error: Value must be hexidecimal and start with '0x'")
            return

        mask = int(argv[2],16)
        level = argv[1]

        rsp = ufmapi.ufm_set_log_mask(level, mask)

        if rsp == None:
            return

        print()

        if "ErrorMask" in rsp:
            print("   ErrorMask: 0x%08X" % rsp['ErrorMask'])

        if "WarningMask" in rsp:
            print(" WarningMask: 0x%08X" % rsp['WarningMask'])

        if "InfoMask" in rsp:
            print("    InfoMask: 0x%08X" % rsp['InfoMask'])

        if "DebugMask" in rsp:
            print("   DebugMask: 0x%08X" % rsp['DebugMask'])

        if "DetailMask" in rsp:
            print("  DetailMask: 0x%08X" % rsp['DetailMask'])

        return

    def _getmask_action(self, menu, item):
        argv = item.argv
        level = argv[1]

        masks = ufmapi.ufm_get_log_mask()

        if masks == None:
            return

        if level == "all":
            print("   ErrorMask: 0x%08X" % masks['ErrorMask'])
            print(" WarningMask: 0x%08X" % masks['WarningMask'])
            print("    InfoMask: 0x%08X" % masks['InfoMask'])
            print("   DebugMask: 0x%08X" % masks['DebugMask'])
            print("  DetailMask: 0x%08X" % masks['DetailMask'])

        elif level == "error":
            print("   ErrorMask: 0x%08X" % masks['ErrorMask'])

        elif level == "warning":
            print(" WarningMask: 0x%08X" % masks['WarningMask'])

        elif level == "info":
            print("    InfoMask: 0x%08X" % masks['InfoMask'])

        elif level == "debug":
            print("   DebugMask: 0x%08X" % masks['DebugMask'])

        elif level == "detail":
            print("  DetailMask: 0x%08X" % masks['DetailMask'])

        return
