#!/usr/bin/python
import os
from subprocess import Popen, PIPE
import argparse
from datetime import date

global_text = """
[Global]
  ReactorMask %(reactor_mask)s
  LogFacility "local7"

[Nvmf]
  MaxQueuesPerSession 4
  AcceptorCore %(acceptor_core)d
  AcceptorPollRate 10
"""

subsystem_text = """
[Subsystem%(subsys_num)d]
NQN %(nqn_text)s
Core %(core_number)d
Mode Direct
Listen RDMA %(ip_addr)s:1023
NVMe %(pcie_addr)s
"""

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


def create_nvmf_config_file(config_file, ip0, ip1):
    '''
    Create configuration file for SPDK NVMf by writing all 
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

    # NUMA aligned IP addresses.
    ip_numa0 = str(ip0) 
    ip_numa1 = str(ip1)
    #print ip_numa0, ip_numa1

    # Default NQN text in config.
    nqn_text = 'nqn.' + yemo + '.io.' + hostname + '-css'
    subtext = ""

    # Write list of NUMA 0 to file.
    if n0:
        count = 0
        corenumber = 0
        for i, pcie in enumerate(n0):
            sub_num =  i + 1
            nqn = nqn_text + str(i + 1)
            # Reset counter for every 4 drives and increase core number by 1.
            #if count > 3:
            #    count = 0
            corenumber = corenumber + 1
            # 12 drives in NUMA. So assigning 4 drives per core (starting core 0)
            subsystem_tab_for_one_drive = subsystem_text % {"subsys_num": sub_num, "nqn_text": nqn, "core_number": corenumber,
                                                            "ip_addr":  ip_numa0, "pcie_addr": pcie}
            subtext+= subsystem_tab_for_one_drive
            # Mark the processor number in the list to prepare Mask
            proc_list[corenumber] = 1
            count = count + 1

    # Write list of NUMA 1 to file.
    if n1:
        count = 0
        core_number = 24
        for i, pcie in enumerate(n1):
            # Starting subsystem number and nqn number after first FOR loop (above).
            sub2_num = i + 1 + len(n0)
            nqn2 = nqn_text + str(i + 1 + len(n0))
            # Reset counter for every 4 drives and increase core number by 1.
            #if count > 3:
            #    count = 0
            core_number = core_number + 1
            # 12 drives in NUMA. So assigning 4 drives per core starting core 20 (NUMA 1).
            subsystem_tab_for_one_drive2 = subsystem_text % {"subsys_num": sub2_num, "nqn_text": nqn2, "core_number": core_number,
                                                            "ip_addr":  ip_numa1, "pcie_addr": pcie}
            subtext+= subsystem_tab_for_one_drive2
            # Mark the processor number in the list to prepare Mask
            proc_list[core_number] = 1
            count = count + 1

    # For Acceptorcore we need to assign core.
    if n1:
        cnum =  core_number
    else:
        cnum =  corenumber

    # Assign separate acceptorcore after above core numbers.
    AcceptorCore =  cnum + 1
    # Mark the AcceptorCore in the list.
    proc_list[cnum+1] = 1
    # Join the core numbers (list), reverse them and convert to binary, then Hex.
    bin_proc_list = ''.join(map(str, proc_list))
    bin_proc_list = bin_proc_list[::-1]
    bin_proc_list = '0b' + bin_proc_list
    ReactorMask = hex(int(bin_proc_list, 2))

    gtext = global_text % {"reactor_mask": ReactorMask, "acceptor_core": AcceptorCore}
    # Write the file out again
    with open(config_file, 'w') as fe:
        fe.write(gtext)
        fe.write(subtext)

    return 0

def install_spdk_dpdk_fio(spdk_loc, fio_loc):
    '''
    Installs FIO, DPDK and SPDK.
    @param spdk_loc: Path to install SPDK
    @param fio_loc: Path to install FIO
    return: None
    '''

    spdk_location = os.path.abspath(spdk_loc)
    dpdk_location = spdk_location + '/dpdk'
    fio_location = fio_loc
    dpdk_gcc_location = dpdk_location + '/x86_64-native-linuxapp-gcc'
    dpdk_config_location = dpdk_location + '/config/defconfig_x86_64-native-linuxapp-gcc'

    dependency_install_cmd = 'yum install -y gcc gcc-c++ CUnit-devel libaio-devel openssl-devel libibverbs-devel librdmacm-devel'
    fio_install_cmd = 'git clone https://github.com/axboe/fio ' + fio_location + ' && cd ' + fio_location + ' && make'
    spdk_download_cmd = 'git clone https://github.com/spdk/spdk ' + spdk_location
    dpdk_download_cmd = 'cd ' + spdk_location + ' && git submodule update --init'
    build_dpdk_cmd = 'cd ' + dpdk_location + ' && make install T=x86_64-native-linuxapp-gcc DESTDIR=.'
    build_spdk_cmd = 'cd ' + spdk_location + ' && ./configure --with-dpdk=' + dpdk_gcc_location + ' --with-rdma --enable-debug  --with-fio=' + fio_location + ' && make'

    for command in dependency_install_cmd, fio_install_cmd, spdk_download_cmd, dpdk_download_cmd, build_dpdk_cmd, build_spdk_cmd:
        # For FIO plugin build, we need to add CFLAGS parameter fPIC in dpdk config file and then build
        if command == build_dpdk_cmd:
            with open(dpdk_config_location, "a") as f:
                f.write("EXTRA_CFLAGS=-fPIC")

        ret, out, err = exec_cmd(command)
        if ret != 0:
            print "------------ Command Failure: "  + command + ' ------------'
            print "Return code: " + str(ret)
            print "Error: " + err
        print "Output: " + out

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config_file", type=str, help="Configuration file. One will be created if it doesn't exist. Need -ip0 & -ip1 arguments")
    parser.add_argument("-ip0", "--ip_numa0", type=str, help="IP address for NUMA 0")
    parser.add_argument("-ip1", "--ip_numa1", type=str, help="IP address for NUMA 1")
    parser.add_argument("-i", "--install_spdk", action='store_true', help="Install FIO Plugin, DPDK & SPDK. Need -l, -f arguments.")
    parser.add_argument("-l", "--spdk_loc", type=str, help="Path to install SPDK to. To be used with -i, -s or -n arguments.")
    parser.add_argument("-f", "--fio_loc", type=str, help="Path to install FIO to. Only to be used with -i argument.")
    parser.add_argument("-s", "--start_spdk", action='store_true', help="Start SPDK. Need -l argument.")
    parser.add_argument("-n", "--start_spdk_nvmf", action='store_true', help="Start SPDK for NVMeoF. Need -l & -c arguments.")

    args = parser.parse_args()
    # Call to create config file.
    if args.config_file:
        if os.path.exists(args.config_file):
            print "Configuration file " + args.config_file + " exists. Skipping create"
        elif not args.ip_numa0 or not args.ip_numa1:
            print "IP addresses for each NUMA (-ip0 & -ip1)are needed"
        else:
            print "------ Creating configuration file: " + os.path.abspath(args.config_file) + " ------"
            retcode = create_nvmf_config_file(args.config_file, args.ip_numa0, args.ip_numa1)
        
            if retcode != 0:
                print "*** ERROR: Creating configuration file ***"
            else:
                print "NVMf Configuration File created: " + os.path.abspath(args.config_file)

    # Call to install SPDK
    if args.install_spdk:
        if not args.spdk_loc or not args.fio_loc:
            print "Need SPDK (-l) & FIO location (-f) arguments."
        else:
            print "------ Calling Install SPDK", args.spdk_loc, args.fio_loc 
            install_spdk_dpdk_fio(args.spdk_loc, args.fio_loc)

    # Start SPDK
    if args.start_spdk:
        if not args.spdk_loc:
            print "Need SPDK location (-l) argument for running scripts/setup.sh"
        else:
            spdk_start_cmd = args.spdk_loc + '/scripts/setup.sh'
            print spdk_start_cmd
            ret, out, err = exec_cmd(spdk_start_cmd)
            if ret != 0:
                print "scripts/setup.sh failed to execute."

    # SPDK NVMf start
    if args.start_spdk_nvmf:
        if not args.config_file or not args.spdk_loc:
            print "Need -c & -l arguments to start NVMeoF"
        elif os.path.exists(args.config_file) is False:
            print "*** No Configuration file exists: " + os.path.abspath(args.config_file) + " ***" 
        else:
            spdk_nvmf_start = args.spdk_loc + '/app/nvmf_tgt/nvmf_tgt -c ' + args.config_file + ' &'
            print spdk_nvmf_start
            ret, out, err = exec_cmd(spdk_nvmf_start)
            if ret != 0:
                print "Error starting NVMf Target"
                print err
            print out

