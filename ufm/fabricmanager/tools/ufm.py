# ufm.py

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
