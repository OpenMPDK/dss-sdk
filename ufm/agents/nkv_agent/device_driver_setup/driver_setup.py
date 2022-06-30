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


import os
import subprocess
import sys
from subprocess import Popen, PIPE

from utils.log_setup import agent_logger
from utils.utils import read_linux_file


def exec_cmd(cmd):
    """
    Execute any given command on shell
    @return: Return code, output, error if any.
    """
    p = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
    out, err = p.communicate()
    # out = out.decode('utf-8')
    out = out.strip()
    # err = err.decode('utf-8')
    ret = p.returncode

    return ret, out, err


class DriverSetup:
    def __init__(self):
        self.logger = agent_logger

    def get_ven_dev_id(self, pci_addr):
        """
        Uses a device pci address to identify the
        vendor and device id from /sys/bus.
        :param pci_addr: Device PCI address.
        :return: List of [ vendor ID, device ID ].
        """
        ret = []
        ids = ['vendor', 'device']
        for get_id in ids:
            buf = read_linux_file("/sys/bus/pci/devices/%s/%s" % (pci_addr, get_id))
            if buf is None:
                self.logger.error("Unable to retrieve %s ID" % get_id)
                sys.exit(-1)
            else:
                id_str = buf.rstrip('\r\n')[2:]
                ret.append(id_str)
        return ret

    def get_driver_name(self, pci_addr):
        try:
            driver_name = os.path.basename(os.readlink("/sys/bus/pci/devices/%s/driver" % pci_addr))
        except Exception as e:
            self.logger.error("Device %s doesn't exist - %s" % (pci_addr, e))
            driver_name = None
        return driver_name

    def setup(self, new_driver_name, pci_addr):
        """
        Uses vendor and device ID to unbind,
        unregister, bind, and register.
        :param command: SPDK command to use the add or removal code.
        :param pci_addr: Device PCI address.
        :return:
        """
        # In case driver is not loaded in the system or unloaded.
        subprocess.call(['modprobe', new_driver_name])

        vendor_id, device_id = self.get_ven_dev_id(pci_addr)
        ven_dev_id = vendor_id + " " + device_id

        rem_ven_dev_path = '/sys/bus/pci/devices/%s/driver/remove_id' % pci_addr
        unbind_dev_path = '/sys/bus/pci/devices/%s/driver/unbind' % pci_addr
        add_ven_dev_path = '/sys/bus/pci/drivers/%s/new_id' % new_driver_name
        bind_dev_path = '/sys/bus/pci/drivers/%s/bind' % new_driver_name

        setup_cmds = [(rem_ven_dev_path, "w", ven_dev_id),
                      (unbind_dev_path, "a", pci_addr),
                      (add_ven_dev_path, "w", ven_dev_id),
                      (bind_dev_path, "a", pci_addr)]

        for cmd in setup_cmds:
            try:
                f = open(cmd[0], cmd[1])
            except Exception as e:
                self.logger.exception("Failed to open %s - %s" % (cmd[0], e))
                raise
            try:
                f.write(cmd[2] + '\n')
                f.close()
            except Exception as e:
                if '/remove_id' in cmd[0]:
                    self.logger.info("Unable to do '%s' > '%s'"
                                     % (cmd[2], cmd[0]))
                    continue
                if '/bind' in cmd[0]:
                    driver_name = self.get_driver_name(pci_addr)
                    if driver_name == new_driver_name:
                        continue
                self.logger.exception("Failed '%s' > '%s'" % (cmd[2], cmd[0]))
                raise
