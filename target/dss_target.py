#!/usr/bin/python
import os
from subprocess import Popen, PIPE
import argparse
import sys
from datetime import date
import multiprocessing
from random import randint 


mp = multiprocessing

g_conf_global_text = """[Global]
  LogFacility "local7"

[Nvmf]
  AcceptorPollRate 10000

[Transport1]
  Type TCP
  MaxIOSize 2097152
  IOUnitSize 2097152
  MaxQueuesPerSession 64

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

g_transport = """
[Transport2]
  Type RDMA
  MaxIOSize 2097152
  IOUnitSize 2097152
  MaxQueuesPerSession 64
"""
g_nvme_global_text = """
[Nvme]
"""

g_nvme_pcie_text = """  TransportID \"trtype:PCIe traddr:%(pcie_addr)s\" %(num)s
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
  Namespace  Nvme%(num)dn1 %(nsid)d"""


g_kv_firmware = ["ETA41KBU"]
#g_block_firmware = ["EDB8000Q"]
g_block_firmware = [""]
g_conf_path = "nvmf.in.conf"
g_tgt_path = ""
g_set_drives = 0
g_reset_drives = 0
g_config = 0
g_ip_addrs=""
g_wal=0
g_tgt_build = 0
g_tgt_launch = 0
g_core_mask = 0
g_tgt_checkout = 0
g_rdma = 1
g_tgt_bin = ""
g_path = ""
g_kv_ssc = 1

def random_with_N_digits(n):
   range_start = 10**(n-1)
   range_end = (10**n)-1
   #seed(1)
   return randint(range_start, range_end)

def exec_cmd(cmd):
   '''
   Execute any given command on shell
   @return: Return code, output, error if any.
   '''

   print("Executing command %s..." %(cmd))
   p = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
   out, err = p.communicate()
   out = out.decode(encoding='UTF-8',errors='ignore')
   out = out.strip()
   err = err.decode(encoding='UTF-8',errors='ignore')
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

    signature ="ls /sys/class/pci_bus/0000:*/device/*/nvme/nvme*/firmware_rev"
    ret, fw_revision_files, err = exec_cmd(signature)
    address_kv_firmware = {}
    address_block_firmware = {}
    fw_revision_files = fw_revision_files.split("\n")
    global g_kv_firmware, g_block_firmware
    if  fw_revision_files:
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


def create_nvmf_config_file(config_file, ip_addrs, kv_pcie_address, block_pcie_address):
    '''
    Create configuration file for NKV NVMf by writing all 
    the subsystems and appropriate numa and NVMe PCIe IDs.
    '''
    # Get NVMe drives and their PCIe IDs.
    n0, n1 = get_nvme_list_numa()
    if not n0 and not n1:
        print "No NVMe drives found"
        return -1

    #print n0
    #print n1
    #print kv_pcie_address
    #print block_pcie_address

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
    nqn_text = 'nqn.' + yemo + '.io:' + hostname
    subtext = ""

    subtext += g_nvme_global_text
    
    global g_conf_global_text, g_wal, g_rdma
    
    drive_count = 0;
    kv_drive_count = 0;
    block_drive_count = 0;
    wal_drive_count = 0;
    kv_list = []
    block_list = []
    bad_index = []
    
    if n0:
    	subtext += "#adding NUMA0 Drives to the list\n"
        for i, pcie in enumerate (n0):
            if pcie in kv_pcie_address:
    		subtext += "#KV Drive\n"
                subtext += g_nvme_pcie_text % {"pcie_addr": pcie, "num":"Nvme"+str(drive_count)}
            	kv_drive_count = kv_drive_count + 1
		kv_list.append(drive_count)
            elif pcie in block_pcie_address:
    		subtext += "#Block Drive\n"
		if g_wal > wal_drive_count:
   	           subtext += g_nvme_pcie_text % {"pcie_addr": pcie, "num":"walbdev-"+str(wal_drive_count)}
		   g_conf_global_text += g_dfly_wal_log_dev_name % {"num":str(wal_drive_count)}        
    	           wal_drive_count = wal_drive_count + 1
		else:
                   subtext += g_nvme_pcie_text % {"pcie_addr": pcie, "num":"Nvme"+str(drive_count)}
            	   block_drive_count = block_drive_count + 1
		   block_list.append(drive_count)
            else:
                bad_index.append(drive_count)
            drive_count = drive_count + 1
    if n1:
    	subtext += "#adding NUMA1 Drives to the list\n"
        for i, pcie in enumerate(n1):
            if pcie in kv_pcie_address:
    		subtext += "#KV Drive\n"
                subtext += g_nvme_pcie_text % {"pcie_addr": pcie, "num":"Nvme"+str(drive_count)}
            	kv_drive_count = kv_drive_count + 1
		kv_list.append(drive_count)
            elif pcie in block_pcie_address:
    		subtext += "#Block Drive\n"
		if g_wal > wal_drive_count:
                   subtext += g_nvme_pcie_text % {"pcie_addr": pcie, "num":"walbdev-"+str(wal_drive_count)}
		   g_conf_global_text += g_dfly_wal_log_dev_name % {"num":str(wal_drive_count)}        
            	   wal_drive_count = wal_drive_count + 1
		else:
                   subtext += g_nvme_pcie_text % {"pcie_addr": pcie, "num":"Nvme"+str(drive_count)}
            	   block_drive_count = block_drive_count + 1
		   block_list.append(drive_count)
            else:
                bad_index.append(drive_count)
            drive_count = drive_count + 1 

    subsystem_text = ""
    ss_number = 1
    
    g_conf_global_text += g_dfly_wal_cache_dev_nqn_name % {"nqn": nqn_text+'-kv_data'+str(ss_number)}        
    if g_rdma:
	g_conf_global_text += g_transport
    
    kv_ss_drive_count = kv_drive_count/g_kv_ssc
    if (kv_ss_drive_count == 0):
        kv_ss_drive_count = 1
 
    nvme_index = 1
    for drive_count_index in range(len(kv_list)):
        if (nvme_index == 1):
            subsystem_text += g_subsystem_common_text % {"subsys_num": ss_number, "pool": "Yes", "nqn_text": nqn_text+'-kv_data'+str(ss_number), "serial_number": random_with_N_digits(10)}
            for ip in ip_addrs:
                subsystem_text += g_subsystem_listen_text % {"transport":"TCP", "ip_addr":str(ip), "port":1023}
	        if g_rdma:
                     subsystem_text += g_subsystem_listen_text % {"transport":"RDMA", "ip_addr":str(ip), "port":1024}

    	subsystem_text += g_subsystem_namespace_text % {"num":kv_list[drive_count_index], "nsid":nvme_index}
        if (kv_ss_drive_count == nvme_index):
            nvme_index = 1
            ss_number = ss_number + 1
        else:
            nvme_index +=1
        if(ss_number > g_kv_ssc):
            break


    ss_number = ss_number + 1
    for i in range(block_drive_count):
    	subsystem_text += g_subsystem_common_text % {"subsys_num": ss_number, "pool": "No", "nqn_text": nqn_text+'-block_data'+str(ss_number), "serial_number": random_with_N_digits(10)}
    	ss_number = ss_number + 1
    	for ip in ip_addrs:
        	subsystem_text += g_subsystem_listen_text % {"transport":"TCP", "ip_addr":str(ip), "port":1023}
		if g_rdma:
             	     subsystem_text += g_subsystem_listen_text % {"transport":"RDMA", "ip_addr":str(ip), "port":1024}
	nvme_index = 1
   	subsystem_text += g_subsystem_namespace_text % {"num":block_list[i], "nsid":nvme_index}


    subtext += subsystem_text

    # Write the file out again
    with open(config_file, 'w') as fe:
        fe.write(g_conf_global_text)
        fe.write(subtext)
    
    return 0

'''
Functions being called in the main function 
'''
def buildtgt():
    '''
    Build the executable
    '''
    if os.path.exists("build.sh"):
    	ret, out, err = exec_cmd("sh build.sh")
        print (out)
	return ret
    else:
	return -1
   
def setup_hugepage():
    '''
    hugepage setup
    '''
    ret, out, err = exec_cmd("export NRHUGE=8192")
    if ret != 0:
        return ret
    
    sys_hugepage_path = "/sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages"
    if not os.path.exists(sys_hugepage_path):
	with open(sys_hugepage_path, 'w') as file:
	    file.write('40')
    else:
	ret, out, err = exec_cmd("echo 40 > " + sys_hugepage_path)
	if ret != 0:
	    return ret

    if not os.path.exists("/dev/hugepages1G"):
        ret, out, err = exec_cmd("mkdir /dev/hugepages1G")
	if ret != 0:
	    return ret

    ret, out, err = exec_cmd("mount -t hugetlbfs -o pagesize=1G hugetlbfs_1g /dev/hugepages1G")
    if ret != 0:
	return ret

    print("****** hugepage setup is done ******")
    return 0    	
    
def setup_drive():
    '''
    Bring all drives to the userspace"
    '''
    cmd = "sh " +g_path + "/../scripts/setup.sh"
    print 'Executing: ' + cmd + '...'
    ret, out, err = exec_cmd(cmd)
    if ret != 0:
        print("****** Assign drives to user is failed ******")

    print(out)
    print("****** drive setup to userspace is done ******")
    return 0


def reset_drive():
    '''
    Bring back drives to system"
    '''
    cmd = "sh "+ g_path + "/../scripts/setup.sh reset"
    print 'Executing: ' + cmd + '...'
    ret, out, err = exec_cmd(cmd)
    if ret != 0:
        print("****** Bring back drives to system is failed ******")

    print(out)
    print("****** drive setup to system is done ******")
    return 0

def execute_tgt(tgt_binary):
    '''
    Execute the tgt binary with conf and core-mask
    '''
    global g_core_mask, g_conf_path
    cmd = g_path + '/nvmf_tgt -c ' + g_conf_path + ' -r /var/run/spdk.sock -m ' + g_core_mask + ' -L dfly_list'
    print 'Executing: ' + cmd + '...'
    ret, out, err = exec_cmd(cmd)
    if ret != 0:
	print("Failed to execute target binary.....")
	return ret   

    print("****** Target Binary is executed ******")    
    return 0

class dss_tgt_args(object):

    def __init__(self):
	# disable the following commands for now
   	#prereq     Install necessary components for build and deploy
   	#checkout   Do git checkout of DSS Target software
   	#build      Build target software
   	#launch     Launch DSS Target software
        parser = argparse.ArgumentParser(
            description='DSS Target Commands',
            usage='''dss_target <command> [<args>]

The most commonly used dss target commands are:
   reset      Assign all NVME drives back to system
   set        Assign NVME drives to UIO
   huge_pages Setup system huge pages
   configure  Configures the system/device(s) and generates necessary config file to run target application 
''')
        parser.add_argument('command', help='Subcommand to run')
        # parse_args defaults to [1:] for args, but you need to
        # exclude the rest of the args too, or validation will fail
        args = parser.parse_args(sys.argv[1:2])
        #if not hasattr(self, args.command):
        #    print 'Unrecognized command'
        #    parser.print_help()
        #    exit(1)
        # use dispatch pattern to invoke method with same name
        getattr(self, args.command)()

    def configure(self):
        parser = argparse.ArgumentParser(
            description='Configures the system/device(s) and generates necessary config file to run target application')
        # prefixing the argument with -- means it's optional
    	parser.add_argument("-c", "--config_file", type=str, default="nvmf.in.conf", help="Configuration file. One will be created if it doesn't exist. Need -ip_addrs argument")
    	parser.add_argument("-ip_addrs", "--ip_addresses", type=str, nargs='+', required=True, help="List of space seperated ip_addresses to listen. Atleast one address is needed")
    	parser.add_argument("-kv_fw", "--kv_firmware", type=str, nargs='+', required=True, help="Use the deivce(s) which has this KV formware to form KV pool sub-system")
    	parser.add_argument("-block_fw", "--block_firmware", type=str, nargs='+',required=False, help="Use the device(s) which has this Block firmware to create Block device sub-system")
    	parser.add_argument("-wal", "--wal", type=int, required=False, help="number of block devices to handle write burst")
    	parser.add_argument("-rdma", "--rdma", type=int, required=False, help="enable RDMA support")
    	parser.add_argument("-kv_ssc", "--kv_ssc", type=int, required=False, help="number of KV sub systems to create")
        # now that we're inside a subcommand, ignore the first
        # TWO argvs, ie the command (dss_tgt) and the subcommand (config)
        args = parser.parse_args(sys.argv[2:])
    	global g_conf_path, g_kv_firmware, g_block_firmware, g_ip_addrs, g_wal, g_rdma, g_kv_ssc
	if args.config_file:
		g_conf_path = args.config_file
    	if args.kv_firmware:
		g_kv_firmware = args.kv_firmware
    	if args.block_firmware:
		g_block_firmware = args.block_firmware
    	if args.ip_addresses:
		g_ip_addrs = args.ip_addresses
    	if args.wal:
		g_wal = args.wal
    	if args.rdma:
		g_rdma = args.rdma
    	if args.kv_ssc:
		g_kv_ssc = args.kv_ssc
        print 'dss_tgt config, config_file='+g_conf_path+'ip_addrs='+str(g_ip_addrs)[1:-1]+'kv_fw='+str(g_kv_firmware)[1:-1]+'block_fw='+str(g_block_firmware)[1:-1]+' wal_devs='+str(g_wal)+' rdma='+str(g_rdma)
        global g_config
	g_config = 1

    def reset(self):
        parser = argparse.ArgumentParser(
            description='Assign back all NVME devices to system')
        print 'Running dss_tgt reset'
        global g_reset_drives
        g_reset_drives = 1
	
    def set(self):
        parser = argparse.ArgumentParser(
            description='Assign NVME devices to UIO')
        print 'Running dss_tgt set'
        global g_set_drives
        g_set_drives = 1

    def huge_pages(self):
        parser = argparse.ArgumentParser(
            description='Setup system huge pages')
        print 'Running dss_target huge_pages'
        setup_hugepage()
   
    def build(self):
        parser = argparse.ArgumentParser(
            description='Build target software')
        print 'Running dss_tgt build'
        global g_tgt_build
        g_tgt_build = 1

    def launch(self):
        parser = argparse.ArgumentParser(
            description='Launching target software')
    	parser.add_argument("-c", "--config_file", type=str, default="nvmf.in.conf", help="Configuration file. One will be created if it doesn't exist. Need -ip_addrs argument")
        parser.add_argument("-tgt_bin", "--target_binary", type=str, default="df_out/oss/spdk_tcp/app/nvmf_tgt/nvmf_tgt", 
					help="Target Binary needed to execute the tgt. Default path will be tried if it doesn't exist")
        args = parser.parse_args(sys.argv[2:])
	print 'Running dss_tgt launch'
    	global g_conf_path, g_tgt_launch, g_tgt_bin
	if args.config_file:
		g_conf_path = args.config_file
	g_tgt_bin = args.target_binary
        g_tgt_launch = 1

    def checkout(self):
        parser = argparse.ArgumentParser(
            description='Checkout target software')
        print 'Running dss_tgt checkout\n do \"git clone git@msl-dc-gitlab.ssi.samsung.com:ssd/nkv-target.git\"'
        global g_tgt_checkout
        g_tgt_checkout = 1

if __name__ == '__main__':

    ret = 0
 
    g_path = os.getcwd()
    print 'Make sure this script is executed from DragonFly/bin diretory, running command under path' + g_path +'...'
 
    dss_tgt_args()
    
    if g_reset_drives  == 1:
	ret = reset_drive()

    if g_set_drives  == 1:
        setup_hugepage()
        ret = setup_drive()

    if g_config  == 1:
	reset_drive()
    	# pcie address for kv 
    	kv_pcie_address, block_pcie_address = get_pcie_address_firmware_mapping()
        ret = create_nvmf_config_file(g_conf_path, g_ip_addrs, kv_pcie_address, block_pcie_address)
        if ret != 0:
            print ("*** ERROR: Creating configuration file ***")
        generate_core_mask(mp.cpu_count(), 0.50)
    	setup_hugepage()
	ret = setup_drive()
    	print("Make sure config file " + g_conf_path + " parameters are correct and update if needed.")
        print("Execute the following command to start the target application: ./nvmf_tgt -c " + g_conf_path + " -r /var/run/spdk.sock -m " + g_core_mask)
    	print("Make necessary changes to core mask (-m option, # of cores that app should use) if needed.")
 
    if g_tgt_build  == 1:
	ret = buildtgt()
    
    if g_tgt_launch  == 1:
    	ret = setup_hugepage()
	#Coremask generation
        generate_core_mask(mp.cpu_count(), 0.50)
	setup_drive()
	ret = execute_tgt(g_tgt_bin)
    
    if ret != 0:
        print("****** Target running is failed ******")
    else:
        print("****** Target starts successfully ******")
