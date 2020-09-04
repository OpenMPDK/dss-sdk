# ufm.py

import time
import subprocess
import sys


def main():
    if sys.argv[1] == 'restart':
        print("UFM: Restarting in 3 seconds!")
        time.sleep(3)

        ufm_fm_stop()
        ufm_fm_start()

    if sys.argv[1] == 'start':
        ufm_fm_start()

    elif sys.argv[1] == 'stop':
        print("UFM: Shutting down in 3 seconds!")
        time.sleep(3)

        ufm_fm_stop()

    return


def ufm_fm_start():
    cmd = "python fabricmanager.py"
    subprocess.call(cmd, shell=True)


def ufm_fm_stop():
    cmd = "sudo kill -9 $(ps -ef | grep fabricmanager | awk '/python/ { print $2 }')"
    subprocess.call(cmd, shell=True)


def ufm_service_stop():
    cmd = "sudo systemctl stop ufm"
    subprocess.call(cmd, shell=True)

    cmd = "sudo systemctl stop ufm_api.service"
    subprocess.call(cmd, shell=True)


def ufm_service_start():
    cmd = "sudo systemctl start ufm_api.service"
    subprocess.call(cmd, shell=True)

    cmd = "sudo systemctl start ufm"
    subprocess.call(cmd, shell=True)


def ufm_service_restart():
    cmd = "sudo systemctl restart ufm_api.service"
    subprocess.call(cmd, shell=True)

    cmd = "sudo systemctl restart ufm"
    subprocess.call(cmd, shell=True)


if __name__ == '__main__':
    main()
