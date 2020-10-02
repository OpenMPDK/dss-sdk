#!/usr/bin/python
import argparse
from datetime import date
import json
import multiprocessing
import os
from random import randint
import re
from subprocess import Popen, PIPE
import sys

g_env = None

mp = multiprocessing

g_conf_global_text = """[Global]
  LogFacility "local7"

[Nvmf]
  AcceptorPollRate 10000


[DFLY]
  KV_PoolEnabled Yes

  Qos No
  Qos_host  default_host        "R:100K L:3000K P:90"
  Qos_host  flusher.internal    "R:100K L:2000K P:10"

  wal_cache_enabled No               # disable cache by No
  wal_log_enabled No                 # disable log by No
  wal_nr_zone_per_pool 20            # default nr of zones per pool. 
  wal_cache_zone_sz_mb 1024          # default capacity of the zone, 1024 M is max per zone due to huge page size limitation.
  wal_nr_cores 5
 
  fuse_enabled No
 
  list_enabled Yes
"""

g_dfly_wal_log_dev_name = """  wal_log_dev_name \"walbdev-%(num)sn1\"
"""

g_dfly_wal_cache_dev_nqn_name = """  wal_cache_dev_nqn_name \"%(nqn)s\"
"""

g_tcp_transport = """
[Transport1]
  Type TCP
  MaxIOSize 2097152
  IOUnitSize 2097152
  MaxQueuesPerSession 64
"""

g_rdma_transport = """
[Transport2]
  Type RDMA
  MaxIOSize 2097152
  IOUnitSize 2097152
  MaxQueuesPerSession 64
"""
g_nvme_global_text = """
[Nvme]
"""

g_nvme_pcie_text = """  TransportID \"trtype:PCIe traddr:%(pcie_addr)s\" "%(num)s"
"""

g_subsystem_common_text = """

[Subsystem%(subsys_num)d]
  KV_PoolEnabled %(pool)s
  NQN %(nqn_text)s
  AllowAnyHost Yes
  Host nqn.2019-05.io.nkv-tgt:init
  SN DSS%(serial_number)d"""

g_subsystem_listen_text = """
  Listen %(transport)s %(ip_addr)s:%(port)d"""


g_subsystem_namespace_text = """
  Namespace  "%(num)sn1" %(nsid)d"""
#  Namespace  Nvme%(num)sn1 %(nsid)d"""


g_kv_firmware = ["ETA41KBU"]
# g_block_firmware = ["EDB8000Q"]
g_block_firmware = [""]
g_conf_path = "nvmf.in.conf"
g_tgt_path = ""
g_set_drives = 0
g_reset_drives = 0
g_config = 0
g_ip_addrs = ""
g_wal = 0
g_tgt_build = 0
g_tgt_launch = 0
g_core_mask = 0
g_tgt_checkout = 0
g_tcp = 1
g_rdma = 1
g_tgt_bin = ""
g_path = ""
g_kv_ssc = 1


def random_with_N_digits(n):
    range_start = 10 ** (n - 1)
    range_end = (10 ** n) - 1
    # seed(1)
    return randint(range_start, range_end)


def cmd_to_str(cmd, env=None):
    ret = ""
    if env:
        for key, val in env.items():
            ret += key + "=" + val + " "
    return ret + cmd


def setenv(name, value):
    """
   Add or modify new env variable
   """
    global g_env
    if g_env is None:
        g_env = {}
    g_env[name] = str(value)


def exec_cmd(cmd, use_env=True):
    """
   Execute any given command on shell
   if use_env == True, use the environment built by setenv (g_env in this script)
   @return: Return code, output, error if any.
   """
    global g_env
    env = g_env if use_env else None
    full_cmd = cmd_to_str(cmd, env)
    print ("Executing command %s..." % (full_cmd))
    p = Popen(full_cmd, stdout=PIPE, stderr=PIPE, shell=True)
    out, err = p.communicate()
    out = out.decode(encoding="UTF-8", errors="ignore")
    out = out.strip()
    err = err.decode(encoding="UTF-8", errors="ignore")
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
    global g_core_mask
    g_core_mask = "{0:#x}".format((mask & ~(reserve_mask << core_half)) & avail_mask)
    return 0


def get_pcie_address_firmware_mapping():
    """
    Filterout KV firmware and map with pcie address.
    return: <dict> , a dictionary of pcie and firmware mapping.
    """

    signature = "ls /sys/class/pci_bus/0000:*/device/*/nvme/nvme*/firmware_rev"
    ret, fw_revision_files, err = exec_cmd(signature)
    address_kv_firmware = {}
    address_block_firmware = {}
    fw_revision_files = fw_revision_files.split("\n")
    global g_kv_firmware, g_block_firmware
    if fw_revision_files:
        for fw_file in fw_revision_files:
            dirs = fw_file.split("/")
            pcie_address = dirs[-4]
            with open(fw_file, "r") as FR:
                fw_revision = FR.readline().strip()
                if fw_revision in g_kv_firmware:
                    address_kv_firmware[pcie_address] = fw_revision
                elif fw_revision in g_block_firmware:
                    address_block_firmware[pcie_address] = fw_revision
    return address_kv_firmware, address_block_firmware

def get_pcie_address_serial_mapping(list_of_pci_dev):
    """
    Filterout KV firmware and map with pcie address.
    return: <dict> , a dictionary of pcie and serial number mapping.
    """
    address_serial = {}
    #pcie = dict.keys(list_of_pci_dev)
    #for item in pcie:
         #signature = "cat /sys/class/pci_bus/0000:{}/device/*/nvme/nvme*/serial".format(item)
    signature = "ls /sys/class/pci_bus/0000:*/device/*/nvme/nvme*/serial"
    ret, serial_files, err = exec_cmd(signature)
    #    serial = serial.strip()
    #    address_serial[item] = serial
    serial_files = serial_files.split("\n")
    global g_kv_firmware, g_block_firmware
    if serial_files:
        for sr_file in serial_files:
            dirs = sr_file.split("/")
            pcie_address = dirs[-4]
            with open(sr_file, "r") as SR:
                serial = SR.readline().strip()
                address_serial[pcie_address] = serial

    return address_serial

def get_nvme_list_numa():
    """
    Get the PCIe IDs of all NVMe devices in the system.
    Create 2 lists of IDs for each NUMA Node.
    @return: Two lists for drives in each NUMA.
    """
    # lspci_cmd = "lspci -v | grep NVM | awk '{print $1}'"
    lspci_cmd = "lspci -mm -n -D | grep 0108 | awk -F ' ' '{print $1}'"
    drives = []

    ret, out, err = exec_cmd(lspci_cmd)
    if not out:
        return drives
    out = out.split("\n")

    for p in out:
        h, i, j = p.split(":")
        i = "0x" + i
        # TODO: 0x7f (decimal 127) is approximate number chosen for dividing NUMA.
        drives.append(p)

    return drives


def create_nvmf_config_file(config_file, ip_addrs, kv_pcie_address, block_pcie_address):
    """
    Create configuration file for NKV NVMf by writing all 
    the subsystems and appropriate numa and NVMe PCIe IDs.
    """
    # Get NVMe drives and their PCIe IDs.
    list_of_drives = get_nvme_list_numa()
    if not list_of_drives:
        print ("No NVMe drives found")
        return -1

    # print list_of_drives
    # print kv_pcie_address
    # print block_pcie_address

    # Get hostname
    ret, hostname, err = exec_cmd("hostname -s")

    # Get number of processors
    retcode, nprocs, err = exec_cmd("nproc")

    # Initialize list for all cores. It will be used in for loops below.
    proc_list = [0 for i in range(0, int(nprocs))]

    today = date.today()
    todayiso = today.isoformat()
    yemo, da = todayiso.rsplit("-", 1)
    # Assigning ReactorMask based on 4-drive per core strategy. May change.
    # Change ReactorMask and AcceptorCore as per number of drives in the system.
    # Below code assumes 24 drives and uses (24/4 + 1) = 7 Cores (+1 is for AcceptorCore)

    # Default NQN text in config.
    nqn_text = "nqn." + yemo + ".io:" + hostname
    subtext = ""

    subtext += g_nvme_global_text

    global g_conf_global_text, g_wal, g_rdma, g_tcp

    drive_count = 0
    kv_drive_count = 0
    block_drive_count = 0
    wal_drive_count = 0
    kv_list = []
    block_list = []
    bad_index = []

    kv_pc_ad = get_pcie_address_serial_mapping(kv_pcie_address)
    bl_pc_ad = get_pcie_address_serial_mapping(block_pcie_address)
    if list_of_drives:
        for i, pcie in enumerate(list_of_drives):
            if pcie in kv_pcie_address:
                subtext += "#KV Drive\n"
                subtext += g_nvme_pcie_text % {
                    "pcie_addr": pcie,
                    #"num": "Nvme" + str(drive_count),
                    "num": kv_pc_ad[pcie],
                }
                kv_drive_count = kv_drive_count + 1
                #kv_list.append(drive_count)
                kv_list.append(kv_pc_ad[pcie])
            elif pcie in block_pcie_address:
                subtext += "#Block Drive\n"
                if g_wal > wal_drive_count:
                    subtext += g_nvme_pcie_text % {
                        "pcie_addr": pcie,
                        #"num": "walbdev-" + str(wal_drive_count),
                        "num": bl_pc_ad[pcie],
                    }
                    g_conf_global_text += g_dfly_wal_log_dev_name % {
                        "num": str(wal_drive_count)
                    }
                    wal_drive_count = wal_drive_count + 1
                else:
                    subtext += g_nvme_pcie_text % {
                        "pcie_addr": pcie,
                        #"num": "Nvme" + str(drive_count),
                        "num": bl_pc_ad[pcie],
                    }
                    block_drive_count = block_drive_count + 1
                    #block_list.append(drive_count)
                    block_list.append(bl_pc_ad[pcie])
            else:
                bad_index.append(drive_count)
            drive_count = drive_count + 1

    subsystem_text = ""
    ss_number = 1

    g_conf_global_text += g_dfly_wal_cache_dev_nqn_name % {
        "nqn": nqn_text + "-kv_data" + str(ss_number)
    }

    if g_tcp:
        g_conf_global_text += g_tcp_transport

    if g_rdma:
        g_conf_global_text += g_rdma_transport

    kv_ss_drive_count = kv_drive_count / g_kv_ssc
    if kv_ss_drive_count == 0:
        kv_ss_drive_count = 1

    nvme_index = 1
    #for drive_count_index in range(len(kv_list)):
    for i in kv_list:
        if nvme_index == 1:
            subsystem_text += g_subsystem_common_text % {
                "subsys_num": ss_number,
                "pool": "Yes",
                "nqn_text": nqn_text + "-kv_data" + str(ss_number),
                "serial_number": random_with_N_digits(10),
            }
            for ip in ip_addrs:
                if g_tcp:
                    subsystem_text += g_subsystem_listen_text % {
                        "transport": "TCP",
                        "ip_addr": str(ip),
                        "port": 1023,
                    }
                if g_rdma:
                    subsystem_text += g_subsystem_listen_text % {
                        "transport": "RDMA",
                        "ip_addr": str(ip),
                        "port": 1024,
                    }

        subsystem_text += g_subsystem_namespace_text % {
            #"num": kv_list[drive_count_index],
            "num": i,
            "nsid": nvme_index,
        }
        if kv_ss_drive_count == nvme_index:
            nvme_index = 1
            ss_number = ss_number + 1
        else:
            nvme_index += 1
        if ss_number > g_kv_ssc:
            break

    #ss_number = ss_number + 1
    #for i in range(block_drive_count):
    for i in block_list:
        subsystem_text += g_subsystem_common_text % {
            "subsys_num": ss_number,
            "pool": "No",
            "nqn_text": nqn_text + "-block_data" + str(ss_number),
            "serial_number": random_with_N_digits(10),
        }
        ss_number = ss_number + 1
        for ip in ip_addrs:
            if g_tcp:
                subsystem_text += g_subsystem_listen_text % {
                    "transport": "TCP",
                    "ip_addr": str(ip),
                    "port": 1023,
                }
            if g_rdma:
                subsystem_text += g_subsystem_listen_text % {
                    "transport": "RDMA",
                    "ip_addr": str(ip),
                    "port": 1024,
                }
        nvme_index = 1
        subsystem_text += g_subsystem_namespace_text % {
            #"num": block_list[i],
            "num": i,
            "nsid": nvme_index,
        }

    subtext += subsystem_text

    # Write the file out again
    with open(config_file, "w") as fe:
        fe.write(g_conf_global_text)
        fe.write(subtext)

    return 0

def get_vlan_ips(target_vlan_id):
    """
    Get the list of IPs corresponding to the specified vlan id
    Returns: list() of IPs in the specified vlan, or None if the vlan doesn't exist
    """
    lshw_cmd = "lshw -c network -json"
    ret, out, err = exec_cmd(lshw_cmd)
    lines = out.split("\n")
    # Fix malformed json...
    fixed_lines = []
    fixed_lines.append("[")
    for line in lines:
        split = line.strip().split()
        if split == ["}", "{"]:
            fixed_lines.append("}, {")
        else:
            fixed_lines.append(line)
    fixed_lines.append("]")

    # Parse json after fixing malformed output
    lshw_json = json.loads("".join(fixed_lines))
    vlanid_ip_map = {}
    vlan_id_regex = re.compile("id\s*[0-9]+")
    for portalias in lshw_json:
        if "logicalname" in portalias and "ip" in portalias["configuration"]:
            devname = portalias["logicalname"]
            ip = portalias["configuration"]["ip"]
        else:
            continue
        new_cmd = "ip -d link show dev " + devname
        ret, out, err = exec_cmd(new_cmd)
        for line in out.split("\n"):
            if "vlan" in line:
                vlan_id = vlan_id_regex.search(line).group().split()[-1]
                if vlan_id not in vlanid_ip_map:
                    vlanid_ip_map[vlan_id] = []
                vlanid_ip_map[vlan_id].append(ip)
    if target_vlan_id in vlanid_ip_map:
        return vlanid_ip_map[target_vlan_id]
    else:
        return None


"""
Functions being called in the main function 
"""


def buildtgt():
    """
    Build the executable
    """
    if os.path.exists("build.sh"):
        ret, out, err = exec_cmd("sh build.sh")
        print (out)
        return ret
    else:
        return -1


def setup_hugepage():
    """
    hugepage setup
    """
    setenv("NRHUGE", "8192")

    sys_hugepage_path = "/sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages"
    if not os.path.exists(sys_hugepage_path):
        with open(sys_hugepage_path, "w") as file:
            file.write("40")
    else:
        ret, out, err = exec_cmd("echo 40 > " + sys_hugepage_path)
        if ret != 0:
            return ret

    if not os.path.exists("/dev/hugepages1G"):
        ret, out, err = exec_cmd("mkdir /dev/hugepages1G")
        if ret != 0:
            return ret
    if not os.path.ismount("/dev/hugepages1G"):
        ret, out, err = exec_cmd(
            "mount -t hugetlbfs -o pagesize=1G hugetlbfs_1g /dev/hugepages1G"
        )
    else:
        print ("/dev/hugepages1G already exists and is mounted")
    if ret != 0:
        return ret

    print ("****** hugepage setup is done ******")
    return 0


def setup_drive():
    """
    Bring all drives to the userspace"
    """
    cmd = "sh " + g_path + "/../scripts/setup.sh"
    print "Executing: " + cmd + "..."
    ret, out, err = exec_cmd(cmd)
    if ret != 0:
        print ("****** Assign drives to user is failed ******")

    print (out)
    print ("****** drive setup to userspace is done ******")
    return 0


def reset_drive():
    """
    Bring back drives to system"
    """
    cmd = "sh " + g_path + "/../scripts/setup.sh reset"
    print "Executing: " + cmd + "..."
    ret, out, err = exec_cmd(cmd)
    if ret != 0:
        print ("****** Bring back drives to system is failed ******")

    print (out)
    print ("****** drive setup to system is done ******")
    return 0


def execute_tgt(tgt_binary):
    """
    Execute the tgt binary with conf and core-mask
    """
    global g_core_mask, g_conf_path
    cmd = (
        g_path
        + "/nvmf_tgt -c "
        + g_conf_path
        + " -r /var/run/spdk.sock -m "
        + g_core_mask
        + " -L dfly_list"
    )
    print "Executing: " + cmd + "..."
    ret, out, err = exec_cmd(cmd)
    if ret != 0:
        print ("Failed to execute target binary.....")
        return ret

    print ("****** Target Binary is executed ******")
    return 0


class dss_tgt_args(object):

    ret = 0

    def __init__(self):
        # disable the following commands for now
        # prereq     Install necessary components for build and deploy
        # checkout   Do git checkout of DSS Target software
        # build      Build target software
        # launch     Launch DSS Target software
        parser = argparse.ArgumentParser(
            description="DSS Target Commands",
            usage="""dss_target <command> [<args>]

The most commonly used dss target commands are:
   reset      Assign all NVME drives back to system
   set        Assign NVME drives to UIO
   huge_pages Setup system huge pages
   configure  Configures the system/device(s) and generates necessary config file to run target application 
""",
        )
        parser.add_argument("command", help="Subcommand to run")
        # parse_args defaults to [1:] for args, but you need to
        # exclude the rest of the args too, or validation will fail
        args = parser.parse_args(sys.argv[1:2])
        # if not hasattr(self, args.command):
        #    print 'Unrecognized command'
        #    parser.print_help()
        #    exit(1)
        # use dispatch pattern to invoke method with same name
        getattr(self, args.command)()

    def configure(self):
        parser = argparse.ArgumentParser(
            description="Configures the system/device(s) and generates necessary config file to run target application"
        )
        # prefixing the argument with -- means it's optional
        parser.add_argument(
            "-c",
            "--config_file",
            type=str,
            default="nvmf.in.conf",
            help="Configuration file. One will be created if it doesn't exist. Need -ip_addrs argument",
        )
        parser.add_argument(
            "-vids",
            "--vlan-ids",
            type=str,
            required=False,
            nargs="+",
            help="ID of VLAN to determine which IPs to listen on",
        )
        parser.add_argument(
            "-ip_addrs",
            "--ip_addresses",
            type=str,
            nargs="+",
            required=False,
            help="List of space seperated ip_addresses to listen. Atleast one address is needed",
        )
        parser.add_argument(
            "-kv_fw",
            "--kv_firmware",
            type=str,
            nargs="+",
            required=False,
            help="Use the deivce(s) which has this KV formware to form KV pool sub-system",
        )
        parser.add_argument(
            "-block_fw",
            "--block_firmware",
            type=str,
            nargs="+",
            required=False,
            help="Use the device(s) which has this Block firmware to create Block device sub-system",
        )
        parser.add_argument(
            "-wal",
            "--wal",
            type=int,
            required=False,
            help="number of block devices to handle write burst",
        )
        parser.add_argument(
            "-tcp", "--tcp", type=int, required=False, help="enable TCP support"
        )
        parser.add_argument(
            "-rdma", "--rdma", type=int, required=False, help="enable RDMA support"
        )
        parser.add_argument(
            "-kv_ssc",
            "--kv_ssc",
            type=int,
            required=False,
            help="number of KV sub systems to create",
        )
        # now that we're inside a subcommand, ignore the first
        # TWO argvs, ie the command (dss_tgt) and the subcommand (config)
        args = parser.parse_args(sys.argv[2:])
        global g_conf_path, g_kv_firmware, g_block_firmware, g_ip_addrs, g_wal, g_tcp, g_rdma, g_kv_ssc
        if args.config_file:
            g_conf_path = args.config_file
        if args.kv_firmware:
            g_kv_firmware = args.kv_firmware
        if args.block_firmware:
            g_block_firmware = args.block_firmware
        if args.wal:
            g_wal = args.wal
        if args.tcp is not None:
            g_tcp = args.tcp
        if args.rdma is not None:
            g_rdma = args.rdma
        if args.kv_ssc:
            g_kv_ssc = args.kv_ssc
        if args.ip_addresses:
            g_ip_addrs = args.ip_addresses
        elif args.vlan_ids:
            g_ip_addrs = []
            for vlan_id in args.vlan_ids:
                g_ip_addrs += get_vlan_ips(vlan_id)
        else:
            print("Must specify either --vlan-ids or --ip-addresses")
            sys.exit(-1)

        print "dss_tgt config, config_file=" + g_conf_path + "ip_addrs=" + str(
            g_ip_addrs
        )[1:-1] + "kv_fw=" + str(g_kv_firmware)[1:-1] + "block_fw=" + str(
            g_block_firmware
        )[
            1:-1
        ] + " wal_devs=" + str(
            g_wal
        ) + " rdma=" + str(
            g_rdma
        ) + " tcp=" + str(
            g_tcp
        )
        global g_config
        g_config = 1
        reset_drive()
        # pcie address for kv
        kv_pcie_address, block_pcie_address = get_pcie_address_firmware_mapping()
        ret = create_nvmf_config_file(
            g_conf_path, g_ip_addrs, kv_pcie_address, block_pcie_address
        )
        if ret != 0:
            print ("*** ERROR: Creating configuration file ***")
        generate_core_mask(mp.cpu_count(), 0.50)
        setup_hugepage()
        ret = setup_drive()
        print (
            "Make sure config file "
            + g_conf_path
            + " parameters are correct and update if needed."
        )
        print (
            "Execute the following command to start the target application: ./nvmf_tgt -c "
            + g_conf_path
            + " -r /var/run/spdk.sock -m "
            + g_core_mask
        )
        print (
            "Make necessary changes to core mask (-m option, # of cores that app should use) if needed."
        )
        with open("run_nvmf_tgt.sh", 'w') as f:
            f.write(
                "./nvmf_tgt -c "
                + g_conf_path
                + " -r /var/run/spdk.sock -m "
                + g_core_mask
            )
        return ret

    def reset(self):
        parser = argparse.ArgumentParser(
            description="Assign back all NVME devices to system"
        )
        print "Running dss_tgt reset"
        global g_reset_drives
        g_reset_drives = 1
        ret = reset_drive()
        return ret

    def set(self):
        parser = argparse.ArgumentParser(description="Assign NVME devices to UIO")
        print "Running dss_tgt set"
        global g_set_drives
        g_set_drives = 1
        setup_hugepage()
        ret = setup_drive()
        return ret

    def huge_pages(self):
        parser = argparse.ArgumentParser(description="Setup system huge pages")
        print "Running dss_target huge_pages"
        setup_hugepage()

    def build(self):
        parser = argparse.ArgumentParser(description="Build target software")
        print "Running dss_tgt build"
        global g_tgt_build
        g_tgt_build = 1

    def launch(self):
        parser = argparse.ArgumentParser(description="Launching target software")
        parser.add_argument(
            "-c",
            "--config_file",
            type=str,
            default="nvmf.in.conf",
            help="Configuration file. One will be created if it doesn't exist. Need -ip_addrs argument",
        )
        parser.add_argument(
            "-tgt_bin",
            "--target_binary",
            type=str,
            default="df_out/oss/spdk_tcp/app/nvmf_tgt/nvmf_tgt",
            help="Target Binary needed to execute the tgt. Default path will be tried if it doesn't exist",
        )
        args = parser.parse_args(sys.argv[2:])
        print "Running dss_tgt launch"
        global g_conf_path, g_tgt_launch, g_tgt_bin
        if args.config_file:
            g_conf_path = args.config_file
        g_tgt_bin = args.target_binary
        g_tgt_launch = 1

    def checkout(self):
        parser = argparse.ArgumentParser(description="Checkout target software")
        print 'Running dss_tgt checkout\n do "git clone git@msl-dc-gitlab.ssi.samsung.com:ssd/nkv-target.git"'
        global g_tgt_checkout
        g_tgt_checkout = 1


if __name__ == "__main__":

    ret = 0

    g_path = os.getcwd()
    print "Make sure this script is executed from DragonFly/bin diretory, running command under path" + g_path + "..."

    dss_tgt_args()
