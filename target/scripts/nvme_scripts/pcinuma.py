#!/usr/bin/python
import os
from subprocess import Popen, PIPE
import argparse

def exec_cmd(cmd):
   '''
   Execute any given command on shell
   @return: Return code, output, error if any.
   '''
   p = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
   out, err = p.communicate()
   out = out.decode()
   out = out.strip()
   err = err.decode()
   ret = p.returncode

   return ret, out, err

def get_nvme_list_numa(cmdtext):

    lspci_cmd = "lspci -v | grep NVM | awk '{print $1}'" 

    ret, out, err =  exec_cmd(lspci_cmd)
    out = out.splitlines()
    for a in out:
        i = a.split(':')[0]
        cmd = 'cd /sys/class/pci_bus/0000\:' + i + '/device/0000\:' + i + '\:00.0/nvme/nvme*/ && pwd'
        if cmdtext == 'numa':
            cmd = cmd + ' && cd ../.. && cat numa_node'
        else:
            cmd = cmd + '&& cat firmware_rev'
        ret, fw, err = exec_cmd(cmd)
        print a + ' = ' + fw

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument("-numa", action='store_true', help="Print NUMA info")

    args = parser.parse_args()
    cmdtext = ''
    if args.numa:
        cmdtext = 'numa'
    get_nvme_list_numa(cmdtext)
