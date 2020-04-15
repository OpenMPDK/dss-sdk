#!/usr/bin/python
import os
from subprocess import Popen, PIPE
import argparse
from datetime import date
import multiprocessing

mp = multiprocessing

global_text = """[Global]
  LogFacility "local7"

[Nvmf]
  AcceptorPollRate 10000

[Transport]
  Type TCP
  MaxIOSize 2097152
  IOUnitSize 2097152
  MaxQueuesPerSession 64

[DFLY]
  KV_PoolEnabled Yes
  Qos No
  Qos_host  default_host        "R:100K L:3000K P:90"
 
  fuse_enabled No
 
  wal_cache_enabled No               # disable cache by No
  wal_log_enabled No                 # disable log by No
"""

nvme_global_text = """
[Nvme]
"""

nvme_pcie_text = """  TransportID \"trtype:PCIe traddr:%(pcie_addr)s\" Nvme%(num)d
"""

subsystem_common_text = """
[Subsystem%(subsys_num)d]
  NQN %(nqn_text)s
  AllowAnyHost Yes
  Host nqn.2019-05.io.nkv-tgt:init
  SN NKV00000000000001"""

subsystem_listen_text = """
  Listen TCP %(ip_addr)s:1023"""


subsystem_namespace_text = """
  Namespace  Nvme%(num)dn1 %(nsid)d"""


##################
# KV Firmware #
##################

kv_firmware = ["EGA41K00","EHA40S04"]


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

def generate_core_mask(core_count, dedicate_core_percent):
    mask_count = 64
    mask = 0xFFFFFFFFFFFFFFFF
    while core_count > mask_count:
        mask_count *= 2
        mask = (1 << mask_count) - 1
    core_half = int((core_count * dedicate_core_percent) / 2)
    core_allow = core_half * 2
    avail_mask = mask >> (mask_count - core_count)
    reserve_cores = core_count - core_allow
    reserve_mask = mask >> (mask_count - reserve_cores)
    core_mask = "{0:#x}".format((mask & ~(reserve_mask << core_half)) & avail_mask)
    return core_mask

def get_pcie_address_firmware_mapping():
    """
    Filterout KV firmware and map with pcie address.
    return: <dict> , a dictionary of pcie and firmware mapping.
    """

    signature ="ls /sys/class/pci_bus/0000:*/device/*/nvme/nvme*/firmware_rev"
    ret, fw_revision_files, err = exec_cmd(signature)
    address_firmware = {}
    fw_revision_files = fw_revision_files.split("\n")
    if  fw_revision_files:
        for fw_file in fw_revision_files:
            dirs = fw_file.split("/")
            pcie_address = dirs[-4]
            with open(fw_file, "r") as FR:
                fw_revision = FR.readline().strip()
                if fw_revision in kv_firmware:
                    address_firmware[pcie_address] = fw_revision

    return address_firmware
                    
                 


def get_nvme_list_numa():
    '''
    Get the PCIe IDs of all NVMe devices in the system.
    Create 2 lists of IDs for each NUMA Node.
    @return: Two lists for drives in each NUMA.
    '''
    #lspci_cmd = "lspci -v | grep NVM | awk '{print $1}'" 
    lspci_cmd = "lspci -mm -n -D | grep 0108 | awk -F ' ' '{print $1}'"
    numa0_drives = []
    numa1_drives = []

    ret, out, err =  exec_cmd(lspci_cmd)
    if not out:
        return numa0_drives, numa1_drives
    out = out.split('\n')

    for p in out:
        h, i, j = p.split(':')
        i = '0x' + i
        # TODO: 0x7f (decimal 127) is approximate number chosen for dividing NUMA.
        if int(i, 16) < int('0x7f', 16):
            numa0_drives.append(p)
        else:
            numa1_drives.append(p)

    return numa0_drives, numa1_drives


def create_nvmf_config_file(config_file, ip_addrs):
    '''
    Create configuration file for NKV NVMf by writing all 
    the subsystems and appropriate numa and NVMe PCIe IDs.
    '''
    # Get NVMe drives and their PCIe IDs.
    n0, n1 = get_nvme_list_numa()
    if not n0 and not n1:
        print "No NVMe drives found"
        return -1

    # Get hostname
    ret, hostname, err = exec_cmd('hostname -s')

    # Get number of processors
    retcode, nprocs, err = exec_cmd('nproc')

    # Initialize list for all cores. It will be used in for loops below.
    proc_list = [0 for i in range(0,int(nprocs))]

    today = date.today()
    todayiso = today.isoformat()
    yemo, da = todayiso.rsplit('-', 1)
    # Assigning ReactorMask based on 4-drive per core strategy. May change.
    # Change ReactorMask and AcceptorCore as per number of drives in the system. 
    # Below code assumes 24 drives and uses (24/4 + 1) = 7 Cores (+1 is for AcceptorCore)

    # Default NQN text in config.
    nqn_text = 'nqn.' + yemo + '.io:' + hostname + '-data'
    subtext = ""

    subtext += nvme_global_text
    
    drive_count = 0;
    
    # pcie address for kv 
    pcie_address = get_pcie_address_firmware_mapping()
    bad_index = []
    if n0:
        for i, pcie in enumerate (n0):
            if pcie in pcie_address:
                subtext += nvme_pcie_text % {"pcie_addr": pcie, "num":i}
            else:
                bad_index.append(i)
            drive_count = drive_count + 1
    if n1:
        for i, pcie in enumerate(n1):
            if pcie in pcie_address:
                subtext += nvme_pcie_text % {"pcie_addr": pcie, "num":drive_count}
            else:
                bad_index.append(drive_count)
            drive_count = drive_count + 1 

    subsystem_text = ""

    subsystem_text += subsystem_common_text % {"subsys_num": 1, "nqn_text": nqn_text}

    for ip in ip_addrs:
        subsystem_text += subsystem_listen_text % {"ip_addr":str(ip)}

    nvme_index = 1
    for drive_count_index in range(drive_count):
        if drive_count_index not in bad_index:
            subsystem_text += subsystem_namespace_text % {"num":drive_count_index, "nsid":nvme_index}
            nvme_index +=1
            



    subtext += subsystem_text

    gtext = global_text
#    # Write the file out again
    with open(config_file, 'w') as fe:
        fe.write(gtext)
        fe.write(subtext)

    return 0

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config_file", type=str, help="Configuration file. One will be created if it doesn't exist. Need -ip_addrs argument")
    parser.add_argument("-ip_addrs", "--ip_addresses", type=str, nargs='+', help="List of space seperated ip_addresses to listen. Atleast one address is needed")
    parser.add_argument("-kv_fw", "--kv_firmware", type=str, nargs='+', help="List of space seperated kv_firmware")

    args = parser.parse_args()
    # Call to create config file.
    if args.config_file:
        if os.path.exists(args.config_file):
            print "Configuration file " + args.config_file + " exists. Skipping create"
        elif not args.ip_addresses:
            print "Atleast one IP address (-ip_addrs) is  needed"
        else:
            print "------ Creating configuration file: " + os.path.abspath(args.config_file) + " ------"
            if args.kv_firmware:
                kv_firmware = args.kv_firmware
            print("INFO: Creating config file with following firmwares\n {}".format(kv_firmware))
            retcode = create_nvmf_config_file(args.config_file, args.ip_addresses)
        
            if retcode != 0:
                print "*** ERROR: Creating configuration file ***"
            else:
                print "NVMf Configuration File created: " + os.path.abspath(args.config_file)

        print "\nSuggested core mask for the server " + generate_core_mask(mp.cpu_count(), 0.777) + "\n"
    else:
        parser.print_help()
