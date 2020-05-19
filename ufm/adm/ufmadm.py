# ufmadm.py

import sys
import json
from ufmapi import cfg_ufm_address
from ufmadm_main import MainMenu

CONFIG_FILE = "ufmadm.json"

def main():
    cfg = json.load(open(CONFIG_FILE))
    cfg_ufm_address(cfg["UFM_ADDRESS"], cfg["UFM_PORT"])

    main_menu = MainMenu()
    main_menu.run(sys.argv)
    exit

if __name__ == '__main__':
    main()