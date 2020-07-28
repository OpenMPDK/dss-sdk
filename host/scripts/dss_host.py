#!/usr/bin/python
from __future__ import print_function

#as3:/usr/local/lib/python2.7/site-packages# cat sitecustomize.py
# encoding=utf8  

# Standard libraries
import argparse
import json
import os
import re
import shutil
from subprocess import Popen, PIPE
import sys
import time

# Non-standard libraries
import paramiko

gl_nkv_config = """
{ 
  "fm_address": "10.1.51.2:8080",
  "contact_fm": 0,
  "nkv_transport" : 0,
  "min_container_required" : 1,
  "min_container_path_required" : 1,
  "nkv_container_path_qd" : 16384,
  "nkv_core_pinning_required" : 0,
  "nkv_app_thread_core" : 22,
  "nkv_queue_depth_monitor_required" : 0,
  "nkv_queue_depth_threshold_per_path" : 8,
  "drive_iter_support_required" : 1,
  "iter_prefix_to_filter" : "meta",
  "nkv_listing_with_cached_keys" : 1,
  "nkv_num_path_per_container_to_iterate" : 0,
  "nkv_stat_thread_polling_interval_in_sec": 30,
  "nkv_need_path_stat" : 0,
  "nkv_need_detailed_path_stat" : 0,
  "nkv_enable_debugging": 0,
  "nkv_listing_cache_num_shards" : 32,
  "nkv_is_on_local_kv" : 1,
  "nkv_use_read_cache" : 0,
  "nkv_read_cache_size" : 400000,
  "nkv_read_cache_shard_size" : 512,
  "nkv_data_size_threshold" : 4096,
  "nkv_use_data_cache" : 0,
  "nkv_remote_listing" : 1,
  "nkv_max_key_length" : 256,
  "nkv_max_value_length" : 1048576,
  "nkv_check_alignment" : 4,

  "nkv_local_mounts": [
]
}
"""

nkv_config_file = "../conf/nkv_config.json"

gl_minio_start_sh = """

export LD_LIBRARY_PATH="../lib"
export MINIO_NKV_CONFIG="../conf/nkv_config.json"
export MINIO_ACCESS_KEY=minio
export MINIO_SECRET_KEY=minio123
export MINIO_STORAGE_CLASS_STANDARD=EC:%(EC)d
export MINIO_PER_HOST_INSTANCE_COUNT=%(IC)d
#export MINIO_ERASURE_SET_DRIVE_COUNT=4
#export MINIO_NKV_MAX_VALUE_SIZE=2097152
export MINIO_NKV_MAX_VALUE_SIZE=786432
export MINIO_NKV_TIMEOUT=20
export MINIO_NKV_SYNC=1
#export MINIO_NKV_CHECKSUM=1
export MINIO_NKV_SHARED_SYNC_INTERVAL=2
export MINIO_NKV_SHARED=%(DIST)d
ulimit -n 65535
ulimit -c unlimited
#yum install boost-devel
#yum install jemalloc-devel
./minio server --address %(IP)s:%(PORT)s """

gl_minio_standalone = "/dev/nvme{%(start)s...%(end)s}n1"
gl_minio_dist_node = "http://dssminio%(node)s:%(port)s/dev/nvme{%(start)s...%(end)s}n1"
g_minio_dist = [""]
g_minio_stand_alone = [""]
g_mini_ec = 0

g_etc_hosts = """
%(IP)s    dssminio%(node)s 
"""

def exec_cmd(cmd):
   '''
   Execute any given command on shell
   @return: Return code, output, error if any.
   '''
   print("Executing cmd %s..." %(cmd))
   p = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
   out, err = p.communicate()
   out = out.decode()
   out = out.strip()
   err = err.decode()
   ret = p.returncode

   return ret, out, err

def exec_cmd_remote(cmd, host, user="root", pw="msl-ssg"):
    '''
    Execute any given command on the specified host
    @return: Return code, stdout, stderr
    '''
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username=user, password=pw)
    stdin, stdout, stderr = client.exec_command(cmd)
    status = stdout.channel.recv_exit_status()
    client.close()
    stdout_result = [x.strip() for x in stdout.readlines()]
    stderr_result = [x.strip() for x in stderr.readlines()]
    return status, stdout_result, stderr_result

def get_list_diff(li1, li2): 
        return (list(set(li1) - set(li2))) 

def get_ip_port_nqn_info(out, proto):
    '''
    get list of ip,port,nqn pairs
    '''
    tuplist = []
    trtype = ""
    for line in out.splitlines():
        if "trtype" in line:
            trtype = line.split(':')[1].strip()
        elif "trsvcid" in line:
            trsvcid = line.split(':')[1].strip()
        elif "subnqn" in line:
            subnqn = line.split(':',1)[1].strip()
        elif "traddr" in line:
            traddr = line.split(':')[1].strip()
        elif "Discovery Log" in line:
            if trtype == proto:
                tuplist.append([trtype, trsvcid, subnqn, traddr])

    # for the last one after "Discover Log" text
    if trtype == proto:
        tuplist.append([trtype, trsvcid, subnqn, traddr])
    tuplist.sort(key = lambda x: x[3])
    print (tuplist)
    return tuplist


def nvme_discover(proto, ip, port):
    '''
    Do NVMe Discovery using the IP/Port info
    '''
    print("----- Doing nvme discovery -----")
    discover_cmd = "nvme discover -t %s -a %s -s %d" %(proto, ip, port)
    print(discover_cmd)
    ret, disc_out, err = exec_cmd(discover_cmd)
    if not disc_out:
        print("Discovery Failed: %s, %s" %(disc_out,err))
        return []

    nqn_info = get_ip_port_nqn_info(disc_out, proto)

    return nqn_info
    
def nvme_connect(nqn_info_sorted, qpair):
    for nqn_info in nqn_info_sorted:
        cmd = "nvme connect -t %s -a %s -s %s -n %s -i %d " \
                %(nqn_info[0], nqn_info[3], nqn_info[1], nqn_info[2], qpair)
        ret, out, err = exec_cmd(cmd)
        if ret != 0:
            print("Command Failed: %s, %s, %s" %(cmd, out, err))
            continue
    print("Waiting for connect to happen 2 sec..")
    time.sleep(2) 

def build_driver():
    '''
    Get kernel version to build appropriate driver
    '''
    print("----- Building kernel drivers -----")
    cmd = "uname -r"
    ret, out, err = exec_cmd(cmd)
    cwd = os.getcwd()
    if "5.1.0" in out:
        os.chdir("../openmpdk_driver/kernel_v5.1_nvmf")
    elif "3.10.0" in out:
        os.chdir("../openmpdk_driver/kernel_v3.10.0-693-centos-7_4")
    else:
        print("Drivers not found. Exiting")
        sys.exit(1)

    shutil.copyfile("linux_nvme.h", "/usr/src/kernels/5.1.0/include/linux/nvme.h")
    shutil.copyfile("linux_nvme_ioctl.h", "/usr/src/kernels/5.1.0/include/uapi/linux/nvme_ioctl.h")
    build = "make all"
    exec_cmd(build)
    if ret != 0:
        print("Build Failed: %s, %s" %(bo, err))
    os.chdir(cwd)

def install_kernel_driver(align):
    '''
    Get kernel version to insert appropriate driver
    '''
    print("----- Installing kernel drivers -----")
    cmd = "uname -r"
    ret, out, err = exec_cmd(cmd)
    cwd = os.getcwd()

    if "5.1.0" in out:
        os.chdir("../openmpdk_driver/kernel_v5.1_nvmf")
    elif "3.10.0" in out:
        os.chdir("../openmpdk_driver/kernel_v3.10.0-693-centos-7_4")
    else:
        print("Drivers not found. Exiting")
        sys.exit(1)

    # Check if the kernel modules were built
    kernel_modules = ["nvme-core.ko", "nvme-fabrics.ko", "nvme-tcp.ko", "nvme-rdma.ko"]
    for module in kernel_modules:
        if not os.path.exists(module):
                os.chdir(cwd)
                raise IOError("File not found: " + os.path.join(os.getcwd(), module) + ", did you forget to run config_driver?")

    disconnect_cmd = "nvme disconnect-all"
    rmmod = "modprobe -r nvme-tcp nvme-rdma nvme-fabrics nvme nvme-core"
    insmod = "insmod ./nvme-core.ko mem_align=%d; insmod ./nvme-fabrics.ko; \
            insmod ./nvme-tcp.ko; insmod ./nvme-rdma.ko" % (align)

    ret, do, err = exec_cmd(disconnect_cmd)
    if ret != 0:
        print("Disconnect Failed: %s, %s" %(do, err))

    ret, rmo, err = exec_cmd(rmmod)
    if ret != 0:
        print("rmmod Failed: %s, %s" %(rmo, err))

    ret, ino, err = exec_cmd(insmod)
    if ret != 0:
        print("insmod Failed: %s, %s" %(ino, err))

    ret, dmesg, err = exec_cmd('dmesg | grep "Samsung Key-Value\|BIO_MAX_PAGES\|mem_align" | tail -3')
    if ret != 0:
        print("Couldn't find Samsung KV driver in dmesg")
    else:
        print(dmesg)

    os.chdir(cwd)


def get_nvme_drives():
    '''
    Get nvme drive list
    '''
    #time.sleep(2)
    cmd = "nvme list | grep nvme | awk '{ print $1 }' | paste -sd, "
    ret, out, err = exec_cmd(cmd)
    #if ret != 0:
    return out

def get_vlan_ips(target_vlan_id, host, root_pw="msl-ssg"):
    """
    Get the list of IPs corresponding to the specified vlan id on the specified host
    Returns: list() of IPs in the specified vlan, or None if the vlan doesn't exist
    """
    lshw_cmd = "lshw -c network -json"
    iplink_cmd = "ip -d link show dev "
    ret, stdout_lines, stderr_lines = exec_cmd_remote(lshw_cmd, host, user="root", pw=root_pw)
    # Fix malformed json...
    fixed_lines = []
    fixed_lines.append("[")
    for line in stdout_lines:
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
        get_vlanid_cmd = iplink_cmd + devname
        _, lines, _ = exec_cmd_remote(get_vlanid_cmd, host, user="root", pw=root_pw)
        for line in lines:
            if "vlan" in line:
                vlan_id = vlan_id_regex.search(line).group().split()[-1]
                if vlan_id not in vlanid_ip_map:
                    vlanid_ip_map[vlan_id] = []
                vlanid_ip_map[vlan_id].append(ip)
    if target_vlan_id in vlanid_ip_map:
        return vlanid_ip_map[target_vlan_id]
    else:
        return []
    
def get_addrs(vlan_ids, hosts, ports):
    '''
    Get IP addresses of all interfaces with vlan ID in vlan_ids on all hosts in hosts
    '''
    ips = []
    for host in hosts:
        for vlan_id in vlan_ids:
            vlan_ips = get_vlan_ips(vlan_id, host)
            for port in ports:
                ips += [x + ":" + str(port) for x in vlan_ips]
    return list(set(ips))

def write_json(data, filename=nkv_config_file): 
    '''
    Write nkv_config.json configuration file
    '''
    with open(filename,'w') as f: 
        json.dump(data, f, indent=4) 


def create_config_file():
    '''
    Create nkv_config.json file in conf directory
    '''
    print("----- Creating nkv_config.json -----")
    # Get list of drives connected to NVMf Target
    drives_list = get_nvme_drives()
    print("drive list: %s" % drives_list)
    if drives_list:
        drives_list = drives_list.split(',')
    else:
        print("Drive list is empty. Exiting.")
        sys.exit(1)

    os.remove(nkv_config_file)

    # Write the initial configuration skeleton in file.
    with open(nkv_config_file, 'w') as f:
        f.write(gl_nkv_config)

    # Loop through drives and add them in local mounts.
    # This is the place to do any other variable changes of config file.
    with open(nkv_config_file, 'r') as f:
        data = json.load(f)
        temp = data['nkv_local_mounts'] 
         
        for drive in drives_list:
            #print("{ mount_point: %s },")
            y = { "mount_point": drive }

            temp.append(y)
        os.remove(nkv_config_file)
        write_json(data)


def run_nkv_test_cli():
    '''
    Run Sample nkv_test_cli after setting up drives"
    '''
    print("----- Running sample nkv_test_cli -----")
    nkv_cmd = "LD_LIBRARY_PATH=../lib ./nkv_test_cli -c " + nkv_config_file + \
            " -i msl-ssg-dl04 -p 1030 -b meta/first/testing -k 60 -v 1048576 -n 100 -t 128 -o 3"
    ret, out, err = exec_cmd(nkv_cmd)
    if ret != 0:
        print("nkv_test_cli Failed: %s" %(err)) 
    else:
        with open("nkv.out", 'w') as f:
            f.write(out)
        print("nkv_test_cli run output written to nkv.out")
            
def config_host(disc_addrs, disc_proto, disc_qpair, driver_memalign):
    # Build and install kernel driver based on kernel version
    install_kernel_driver(driver_memalign)
    nqn_infos = []

    for addr in disc_addrs:
        if addr.count(":") != 1:
            raise ValueError("Error: malformed address " + addr + " expected ip:port.")
        disc_ip, disc_port = addr.split(":")
        nqn_infos += nvme_discover(disc_proto, disc_ip, int(disc_port))
    # Sort NQN infos
    nqn_infos.sort(key=lambda x : x[3].split(":")[0])
    # Connect to all discovered NQNs and their IPs.
    nvme_connect(nqn_infos, disc_qpair)

    # Create nkv_config.json file
    create_config_file()


def config_minio_sa(node, ec):
    print("node details ec %d %s" %(ec, node))
    ip,port,dev_start,dev_end = node
    
    minio_node = ""
    minio_node += gl_minio_standalone % {"start":dev_start, "end":dev_end} +" "
    minio_startup = "minio_startup_sa" + ip + ".sh"
    minio_settings = gl_minio_start_sh % {"EC": ec, "IC":1, "DIST":1, "IP":ip, "PORT":port}
    minio_settings += minio_node 
    with open(minio_startup, 'w') as f:
        f.write(minio_settings)
    print("Successfully created MINIO startup script %s" %(minio_startup))
    
def getSubnet(addr):
    # FIXME: Find true subnet based on remote routing table
    return addr.split(".")[0]

def discover_dist(port):
    '''
    Run nvme list-subsys on all targets to infe
    '''
    ret, out, err = exec_cmd("nvme list-subsys -o json")
    subsystems = json.loads(out)
    subsystems = subsystems["Subsystems"]
    subnet_device_map = {}
    addrs = []
    for subsys in subsystems:
        if not "Paths" in subsys:
            continue
        for dev in subsys["Paths"]:
            name = dev["Name"]
            transport = dev["Transport"]
            addr = re.search("traddr=(\S+)", dev["Address"]).group(1)
            subnet = getSubnet(addr)
            if not subnet in subnet_device_map:
                subnet_device_map[subnet] = []
            subnet_device_map[subnet].append(name)
            addrs.append(addr)
    # ret is a list of tuples (IP, devlow, devhigh)
    # where the closed interval [devlow, devhigh] are 
    # all device numbers on that particular IP
    ips_devs = []
    for addr in addrs:
        subnet = getSubnet(addr)
        devs = subnet_device_map[subnet]
        # Extract device numbers using regex (i.e. nvme3 -> 3)
        dev_numbers = sorted([int(re.search("(\d+)", dev).group(1)) for dev in devs])
        runs = []
        # Find contiguous blocks of devices in the list
        while len(dev_numbers) > 0:
            for idx, num in enumerate(dev_numbers):
                # If there is a gap between the dev number at idx-1 and idx,
                # mark [0] - [idx - 1] as a contiguous range and then remove
                # all of those dev numbers from the list
                if num - dev_numbers[0] != idx:
                    runs.append((dev_numbers[0], dev_numbers[idx - 1]))
                    dev_numbers = dev_numbers[idx:]
                    break
                # If we reach the end of the list, the entire remaining list is contiguous
                if idx == len(dev_numbers) - 1:
                    runs.append((dev_numbers[0], dev_numbers[idx]))
                    dev_numbers = []
        for run in runs:
            ips_devs.append((addr, run[0], run[1]))
    ret = []
    # Expected format for dist is [ip, port, low, high, ip, port, low, high, ...]
    for ip, devlow, devhigh in ips_devs:
        ret.append(ip)
        ret.append(port)
        ret.append(devlow)
        ret.append(devhigh)
    return ret


def config_minio_dist(node_details, ec, instances):
    print("node details ec %d %s" %(ec, node_details))
    node_count = 0
    node_index = 0
    minio_dist_node = ""
    i = 0
    j = 0
    etc_hosts_map = ""
    while (i < len(node_details)):
        while (j < len(node_details)):
            ip,port,dev_start,dev_end = node_details[j:j+4]
            node_count += 1
            minio_dist_node += gl_minio_dist_node % {"node":node_count, "port":port, "start":dev_start, "end":dev_end} +" "
            j += 4
        ip,port,dev_start, dev = node_details[i:i+4]
        i += 4
        node_index += 1
        minio_startup = "minio_startup_" + ip + ".sh"
        minio_settings = gl_minio_start_sh % {"EC": ec, "IC":instances, "DIST":1, "IP":ip, "PORT":port}
        minio_settings += minio_dist_node 
        with open(minio_startup, 'w') as f:
            f.write(minio_settings)
        print("Successfully created MINIO startup script %s" %(minio_startup))
        etc_hosts_map += g_etc_hosts % {"node":node_index, "IP":ip}
    with open("etc_hosts", 'w') as f:
        f.write(etc_hosts_map)
    print("Successfully created etc host file, add this into your MINIO server \"etc_hosts\"")

def config_minio(dist, sa, ec, instances):
    if(sa):
        config_minio_sa(sa, ec)
    elif(dist):
        config_minio_dist(dist, ec, instances)


class dss_host_args(object):

    def __init__(self):
        # disable the following commands for now
        #prereq     Install necessary components for build and deploy
        #checkout   Do git checkout of DSS Target software
        #build      Build target software
        #launch     Launch DSS Target software
        parser = argparse.ArgumentParser(
            description='DSS Host Commands',
            usage='''dss_host <command> [<args>]

The most commonly used dss target commands are:
   config_host  Discovers/connects device(s), and creates config file for DSS API layer
   config_minio Generates MINIO scripts based on parameters
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

    def config_host(self):
        parser = argparse.ArgumentParser(
            description='Discovers/connects device(s), and creates config file for DSS API layer')
        parser.add_argument("-vids", "--vlan-ids", nargs='+', help="Space delimited list of vlan IDs")
        parser.add_argument("-p", "--ports", type=int, nargs='+', required=False, help="Port numbers to be used for nvme discover.")
        parser.add_argument("--hosts", nargs='+', help="Space delimited list of target hostnames")
        parser.add_argument("-a", "--addrs", type=str, nargs='+', help="Space-delimited list of ip:port for nvme discovery (required)")
        parser.add_argument("-t", "--proto", type=str, help="Protocol for nvme discovery (default: rdma)", \
            default="rdma")
        parser.add_argument("-i", "--qpair", type=int, help="Queue Pair for connect (default: 32)", default=32)
        parser.add_argument("-m", "--memalign", type=int, help="Memory alignment for driver (default: 512)", \
            default=512)
        args = parser.parse_args(sys.argv[2:])

        if args.addrs != None:
            disc_addrs = args.addrs
        elif args.vlan_ids != None and args.hosts != None and args.ports != None:
            disc_addrs = get_addrs(args.vlan_ids, args.hosts, args.ports)
        else:
            print("Must specify --addrs or --hosts AND --vlan-ids AND --ports")
            sys.exit(-1)

        disc_proto = args.proto
        driver_memalign = args.memalign
        disc_qpair = args.qpair
        config_host(disc_addrs, disc_proto, disc_qpair, driver_memalign)

    def config_minio(self):
        parser = argparse.ArgumentParser(
            description='Generates MINIO scripts based on parameters')
        parser.add_argument("-dist", "--dist", type=str, nargs='+', required=False, help="Enter space separated node info \"ip port start_dev end_dev\" for all MINDIST IO nodes")
        parser.add_argument("-p", "--port", type=int, required=False, help="Port number to be used for minio, must specify -p or -dist but not both.")
        parser.add_argument("-stand_alone", "--stand_alone", type=str, nargs='+', help="Enter space separated node info \"ip port start_dev end_dev\" for all MINDIST IO nodes")
        parser.add_argument("-ec", "--ec", type=int, required=False, help="Erasure Code, specify 0 for no EC", default=0)
        parser.add_argument("-instances", "--instances", type=int, required=False, help="Number of MINIO instances per Node", default=2)
        args = parser.parse_args(sys.argv[2:])
       
        global g_minio_dist, g_minio_stand_alone
        if args.dist and not args.port:
            minio_dist = args.dist 
        elif args.port and not args.dist:
            minio_dist = discover_dist(args.port)
        else:
            print("Must specify either --dist or --port, but not both.")
            return
        if args.stand_alone:
            g_minio_stand_alone = args.stand_alone 
        config_minio(minio_dist, args.stand_alone, args.ec, args.instances)

    def config_driver(self):
        build_driver()

    def verify_nkv_cli(self):
        run_nkv_test_cli()

    def remove(self):
        parser = argparse.ArgumentParser(
                description='Resets the system to stock nvme'
        )
        # Disconnect from all subsystems
        exec_cmd("nvme disconnect-all")
        # Remove all nvme drivers
        exec_cmd("rmmod nvme_tcp")
        exec_cmd("rmmod nvme_rdma")
        exec_cmd("rmmod nvme_fabrics")
        exec_cmd("rmmod nvme")
        exec_cmd("rmmod nvme_core")
        # Load stock nvme driver
        exec_cmd("modprobe nvme")


if __name__ == '__main__':

    ret = 0

    #reload(sys)
    #sys.setdefaultencoding('utf8')

    g_path = os.getcwd()
    print("Make sure this script is executed from nkv-sdk/bin diretory, running command under path:%s" %(g_path))

    dss_host_args()

