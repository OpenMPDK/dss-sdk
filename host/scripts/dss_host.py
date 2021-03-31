#!/usr/bin/python
"""
 *   BSD LICENSE
 *
 *   Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

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
import socket

# Non-standard libraries
import paramiko

gl_minio_start_sh = """
export LD_LIBRARY_PATH="../lib"
export MINIO_NKV_CONFIG="../conf/nkv_config.json"
export MINIO_ACCESS_KEY=minio
export MINIO_SECRET_KEY=minio123
export MINIO_STORAGE_CLASS_STANDARD=EC:%(EC)d
export MINIO_NKV_MAX_VALUE_SIZE=1048576
export MINIO_NKV_TIMEOUT=20
export MINIO_NKV_SYNC=1
export MINIO_ON_KV=1
export MINIO_NKV_USE_CUSTOM_READER=1
export MINIO_NKV_SHARED_SYNC_INTERVAL=2
export MINIO_INSTANCE_HOST_PORT=%(IP)s:%(PORT)s
export MINIO_NKV_SHARED=%(DIST)d
export MINIO_EC_BLOCK_SIZE=65536
export MINIO_ENABLE_NO_LOCK_READ=1
export MINIO_ENABLE_NO_READ_VERIFY=1
#export MINIO_NKV_CHECKSUM=1
ulimit -n 65535
ulimit -c unlimited
./minio server --address %(IP)s:%(PORT)s """

gl_minio_standalone = "/dev/nvme%(devnum)s""n1"
gl_minio_dist_node = "http://%(node)s:%(port)s/dev/nvme{%(start)s...%(end)s}n1"
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
    stdout_result = [x.strip().decode() for x in stdout.readlines()]
    stderr_result = [x.strip().decode() for x in stderr.readlines()]
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
    dss_script_path = os.path.dirname(os.path.realpath(__file__))
    os.chdir(dss_script_path)

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
    rc, bo, err = exec_cmd(build)
    if rc != 0:
        print("Build Failed: %s, %s" %(bo, err))
        sys.exit(rc)        
    os.chdir(cwd)

def install_kernel_driver(align):
    '''
    Get kernel version to insert appropriate driver
    '''
    print("----- Installing kernel drivers -----")
    cmd = "uname -r"
    ret, out, err = exec_cmd(cmd)
    cwd = os.getcwd()
    dss_script_path = os.path.dirname(os.path.realpath(__file__))
    os.chdir(dss_script_path)

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
    #rmmod = "rmmod nvme; rmmod nvme-tcp; rmmod nvme-rdma; rmmod nvme-fabrics; rmmod nvme-core;"
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


def get_nvme_drives(cmd):
    '''
    Get nvme drive list
    '''
    ret, out, err = exec_cmd(cmd)
    #if ret != 0:
    return out

G_VLAN_IPS_CACHE = {}
def get_ips_for_vlan(target_vlan_id, host, root_pws):
    """
    Get the list of IPs corresponding to the specified vlan id on the specified host
    Returns: list() of IPs in the specified vlan, or [] if the vlan doesn't exist
    """
    if host not in G_VLAN_IPS_CACHE:
        lshw_cmd = "lshw -c network -json"
        iplink_cmd = "ip -d link show dev "
        stdout_lines = None
        for root_pw in root_pws:
            try:
                ret, stdout_lines, stderr_lines = exec_cmd_remote(lshw_cmd, host, user="root", pw=root_pw)
                break
            except:
                pass
        # Fix malformed json...
        if stdout_lines is None:
            print("Error: all attempts to authenticate failed, aborting...")
            sys.exit(-1)
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
        G_VLAN_IPS_CACHE[host] = {}
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
                    if vlan_id not in G_VLAN_IPS_CACHE[host]:
                        G_VLAN_IPS_CACHE[host][vlan_id] = []
                    G_VLAN_IPS_CACHE[host][vlan_id].append(ip)
    if target_vlan_id in G_VLAN_IPS_CACHE[host]:
        return G_VLAN_IPS_CACHE[host][target_vlan_id]
    else:
        return []

def get_vlan_for_ips(ip, host, root_pws):
    """
    Get the vlan ID corresponding to the specified IP on the specified host
    Returns: string VLAN ID for the specified IP, or None if the ip doesn't exist
    """
    # Call get_ips_for_vlan to cache the mapping
    if host not in G_VLAN_IPS_CACHE:
        get_ips_for_vlan(0, host, root_pws)
    # Naive search through all VLAN ids to find target IP
    for vlan_id, ips in G_VLAN_IPS_CACHE[host].items():
        if ip in ips:
            return vlan_id
    return None
    
def get_addrs(vlan_ids, hosts, ports, root_pws):
    '''
    Get IP addresses of all interfaces with vlan ID in vlan_ids on all hosts in hosts
    '''
    ips = []
    for host in hosts:
        for vlan_id in vlan_ids:
            vlan_ips = get_ips_for_vlan(vlan_id, host, root_pws)
            for port in ports:
                ips += [x + ":" + str(port) for x in vlan_ips]
    return list(set(ips))

def write_json(data, filename):
    '''
    Write nkv_config.json configuration file
    '''
    with open(filename,'w') as f: 
        json.dump(data, f, indent=4) 


def get_drives_list(nvme_list_cmd):
    # Get list of drives connected to NVMf Target
    drives_list = get_nvme_drives(nvme_list_cmd)
    print("drive list: %s" % drives_list)
    if drives_list:
        drives_list = drives_list.split(',')
    else:
        print("Drive list is empty. Exiting.")
        sys.exit(1)

    return drives_list


def create_config_file(nkv_kv_pair, drives_list, nkv_conf_file="../conf/nkv_config.json"):
    '''
    Update nkv_config.json file in conf directory
    '''
    print("----- Updating %s -----" %nkv_conf_file)

    # Loop through drives and add them in local mounts.
    # This is the place to do any other variable changes of config file.
    with open("../conf/nkv_config.json", 'r') as f:
        data = json.load(f)
        data['nkv_local_mounts'] = []
         
        if nkv_kv_pair:
            for pair in nkv_kv_pair:
                key, value = pair.split('=')
                if key in data:
                    data[key] = int(value)
                else:
                    print('Invalid key "%s" not added to config file' % key)

        for drive in drives_list:
            #print("{ mount_point: %s },")
            y = { "mount_point": drive }
            data['nkv_local_mounts'].append(y)

    write_json(data, nkv_conf_file)


def num(s):
    try:
        return int(s)
    except ValueError:
        return float(s)


def run_nkv_test_cli(nkv_conf_file, numa_num, workload, meta_prefix, key, value, threads, numobjects):
    '''
    Run Sample nkv_test_cli after setting up drives"
    '''
    print("----- Running sample nkv_test_cli -----")
    nkv_cmd = "LD_LIBRARY_PATH=../lib numactl -N " + numa_num + " -m " + numa_num + " ./nkv_test_cli -c " + nkv_conf_file + \
            " -i msl-ssg-dl04 -p 1030 -b test/" + meta_prefix + " -k " + key + " -v " + value + " -n " + numobjects + " -t " + \
            threads + " -o " + workload
    #print(nkv_cmd)
    result_file = "nkv_numa" + numa_num + ".out"
    ret, out, err = exec_cmd(nkv_cmd)
    if ret != 0:
        print("nkv_test_cli Failed: %s" %(err)) 
    else:
        with open(result_file, 'w') as f:
            f.write(out)
        print("nkv_test_cli run output written to %s" % result_file)
        nkv_test_result(result_file)


def nkv_test_result(result_file):
    '''
    Parse nkv_test_cli results
    '''
    bw_lines = []
    with open(result_file, 'r') as w:
        for line in w:
            if 'Throughput' in line:
                bw_lines.append(line)

    bw = 0.0
    for i in bw_lines:
        bw = bw + num(re.split(r'[=MB]', i)[2].lstrip())

    with open(result_file, 'a+') as w:
        w.write("\n------------------------------\n")
        w.write("BW = %s GB/s\n" % round(float(bw/1000),2))
        w.write("------------------------------")

    print("------------------------------")
    print("BW = %s GB/s" % round(float(bw/1000),2))
    print("------------------------------")


def config_host(disc_addrs, disc_proto, disc_qpair, driver_memalign, nkv_kv_pair):
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
    nqn_infos_dedup = []
    for nqn_info in nqn_infos:
        if nqn_info not in nqn_infos_dedup:
            nqn_infos_dedup.append(nqn_info)
    # Connect to all discovered NQNs and their IPs.
    nvme_connect(nqn_infos_dedup, disc_qpair)

    cmd = "nvme list | grep nvme | awk '{ print $1 }' | paste -sd, "
    list_of_drives = get_drives_list(cmd)
    # Create nkv_config.json file
    create_config_file(nkv_kv_pair, list_of_drives)


def config_minio_sa(node, ec):
    print("node details ec %d %s" %(ec, node))
    ip = node[0]
    port = node[1]
    devs = node[2:]
    
    minio_node = ""
    for dev in devs:
        minio_node += gl_minio_standalone % {"devnum":dev} + " "
    minio_startup = "minio_startup_sa" + ip + ".sh"
    #minio_settings = gl_minio_start_sh % {"EC": ec, "IC":1, "DIST":1, "IP":ip, "PORT":port}
    minio_settings = gl_minio_start_sh % {"EC": ec, "DIST": 0, "IP":ip, "PORT":port}
    minio_settings += minio_node 
    with open(minio_startup, 'w') as f:
        f.write(minio_settings)
    print("Successfully created MINIO startup script %s" %(minio_startup))
    
def getSubnet(addr):
    # FIXME: Find true subnet based on remote routing table
    return addr.split(".")[0]

def discover_dist(port, frontend_vlan_ids, backend_vlan_ids, root_pws):
    '''
    Run nvme list-subsys on all targets to infe
    '''
    print("Gathering information from nvme list-subsys")
    # Build a mapping from frontend vlan ids to backend vlan ids
    vlan_mapping = {}
    for front, back in zip(frontend_vlan_ids, backend_vlan_ids):
        vlan_mapping[back] = front

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
        backend_vlan_id = get_vlan_for_ips(addr, addr, root_pws)
        frontend_ip = get_ips_for_vlan(vlan_mapping[backend_vlan_id], addr, root_pws)[0]
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
            print("Adding: backend IP: {}, nvme{}:{}n1, frontend IP: {}".format(addr, run[0], run[1], frontend_ip))
            ips_devs.append((frontend_ip, run[0], run[1]))
    ret = []
    # Expected format for dist is [ip, port, low, high, ip, port, low, high, ...]
    for ip, devlow, devhigh in ips_devs:
        ret.append(ip)
        ret.append(port)
        ret.append(devlow)
        ret.append(devhigh)
    return ret


def is_ipv4(string):
    try:
        socket.inet_aton(string)
        return True
    except socket.error:
        return False


def config_minio_dist(node_details, ec):
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
            minio_dist_node += gl_minio_dist_node % {"node":ip, "port":port, "start":dev_start, "end":dev_end} +" "
            j += 4
        ip,port,dev_start, dev = node_details[i:i+4]
        i += 4
        node_index += 1
        minio_startup = "minio_startup_" + ip + ".sh"
        minio_settings = gl_minio_start_sh % {"EC": ec, "DIST":1, "IP":ip, "PORT":port}
        minio_settings += minio_dist_node 
        with open(minio_startup, 'w') as f:
            f.write("#!/bin/bash")
            f.write(minio_settings)
            f.write("\n")
        os.chmod(minio_startup, 0o755)

        print("Successfully created MINIO startup script %s" %(minio_startup))
        if is_ipv4(ip):
            etc_hosts_map += g_etc_hosts % {"node":node_index, "IP":ip}

    if etc_hosts_map:
        with open("etc_hosts", 'w') as f:
            f.write(etc_hosts_map)
        print("Successfully created etc host file, add this into your MINIO server \"etc_hosts\"")

def config_minio(dist, sa, ec):
    if(sa):
        config_minio_sa(sa, ec)
    elif(dist):
        config_minio_dist(dist, ec)


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
   config_host     Discovers/connects device(s), and creates config file for DSS API layer
   config_minio    Generates MINIO scripts based on parameters
   config_driver   Build Kernel driver
   create_nkv_conf Creates nkv_config.json based on search string for drives
   verify_nkv_cli  Run an instance of nkv_test_cli
   remove          Disconnects all drives and remove kernel driver

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
        parser.add_argument("-a", "--addrs", type=str, nargs='+', help="Space-delimited list of ip for nvme discovery (required)")
        parser.add_argument("-t", "--proto", type=str, help="Protocol for nvme discovery (default: rdma)", \
            default="rdma")
        parser.add_argument("-i", "--qpair", type=int, help="Queue Pair for connect (default: 32)", default=32)
        parser.add_argument("-m", "--memalign", type=int, help="Memory alignment for driver (default: 512)", \
            default=512)
        parser.add_argument("-r", "--root-pws", nargs='+', required=False, default=["msl-ssg"], help="List of root passwords for all machines in cluster to be tried in order")
        parser.add_argument("-x", "--kvpair", type=str, default=None, nargs="+", help="one or more key=value pairs for nkv_config.json \
                            file update. For e.g., nkv_need_path_stat=1 nkv_max_key_length=1024 and so on.")

        args = parser.parse_args(sys.argv[2:])

        if args.addrs != None and args.ports != None:
            disc_addrs = []
            for port in args.ports:
                disc_addrs = ["{}:{}".format(addr, port) for addr in args.addrs]
        elif args.vlan_ids != None and args.hosts != None and args.ports != None:
            disc_addrs = get_addrs(args.vlan_ids, args.hosts, args.ports, args.root_pws)
        else:
            print("Must specify --addrs AND --ports or --hosts AND --vlan-ids AND --ports")
            sys.exit(-1)

        disc_proto = args.proto
        driver_memalign = args.memalign
        disc_qpair = args.qpair
        nkv_kv_pair = args.kvpair
        config_host(disc_addrs, disc_proto, disc_qpair, driver_memalign, nkv_kv_pair)

    def config_minio(self):
        parser = argparse.ArgumentParser(
            description='Generates MINIO scripts based on parameters')
        parser.add_argument("-dist", "--dist", type=str, nargs='+', required=False, help="Enter space separated node info \"ip port start_dev end_dev\" for all MINDIST IO nodes. start_dev, end_dev should be integers.")
        parser.add_argument("-p", "--port", type=int, required=False, help="Port number to be used for minio, must specify -p or -dist but not both.")
        parser.add_argument("-stand_alone", "--stand_alone", type=str, nargs='+', help="Enter space separated node info \"ip port <dev num 0> <dev num 1> ...\" for the local node. Dev numbers should be integers.")
        parser.add_argument("-ec", "--ec", type=int, required=False, help="Erasure Code, leave as default (2) unless you know what you're doing.", default=2)
        parser.add_argument("-r", "--root-pws", nargs='+', required=False, default=["msl-ssg"], help="List of root passwords for all machines in cluster to be tried in order")
        parser.add_argument("-f", "--frontend-vlan-ids", nargs='+', required=False, type=str, default=[], help="Space delimited list of vlan IDs")
        parser.add_argument("-b", "--backend-vlan-ids", nargs='+', required=False, type=str, default=[], help="Space delimited list of vlan IDs")
        args = parser.parse_args(sys.argv[2:])
       
        minio_dist = args.dist
        # Validate args
        if args.dist and args.stand_alone:
            print("Both --dist and --stand_alone specified, please specify only one")
            return
        if args.dist and not args.port:
            print("Configuring using user-provided --dist")
        elif args.port and not args.dist:
            print("Configuring using VLAN ID")
            if len(set(args.frontend_vlan_ids)) != len(args.frontend_vlan_ids):
                print("Duplicate frontend vlan ID not supported")
                return
            if len(set(args.backend_vlan_ids)) != len(args.backend_vlan_ids):
                print("Duplicate backend vlan ID not supported")
                return
            if len(args.frontend_vlan_ids) != len(args.backend_vlan_ids):
                print("Must specify exactly 1 frontend vlan ID per backend vlan ID")
                return
            if len(args.frontend_vlan_ids) == 0:
                print("No frontend vlan ids specified, exiting without doing anything...")
                return
            minio_dist = discover_dist(args.port, args.frontend_vlan_ids, args.backend_vlan_ids, args.root_pws)
        elif args.dist and args.port:
            print("Must specify either --dist or --port, but not both.")
            return
        # Generate minio run scripts
        config_minio(minio_dist, args.stand_alone, args.ec)

    def config_driver(self):
        build_driver()

    def verify_nkv_cli(self):
        '''
        Run nkv_test_cli using the arguments provided.
        Find out the drives to run by running nvme list-subsys command first.
        '''
        parser = argparse.ArgumentParser(
            description='Generates nkv_config.json to run nkv_test_cli on each node')
        parser.add_argument("-c", "--conf", type=str, help="nkv config json file name")
        parser.add_argument("-a", "--addr", type=str, help="Provide IP octets to distinguish the drives in 'nvme list-subsys' \
                           output (e.g., 201.0)", default="traddr")
        parser.add_argument("-o", "--workload", type=str, help="Workload type - PUT(0), GET(1), DELETE(2)")
        parser.add_argument("-m", "--numa", type=str, help="NUMA node to run the nkv_test_cli on")
        parser.add_argument("-k", "--keysize", type=str, help="Key size in bytes. Should be < 255B, default=60", default="60")
        parser.add_argument("-v", "--valsize", type=str, help="Value size in bytes. Default/Max=1048576", default="1048576")
        parser.add_argument("-t", "--threads", type=str, help="Number of threads to run (default=128)", default="128")
        parser.add_argument("-n", "--numobj", type=str, help="Number of objects for each thread (default=1000)", default="1000")
        parser.add_argument("-x", "--kvpair", type=str, default=None, nargs="+", help="one or more key=value pairs for nkv_config.json \
                            file update. For e.g., nkv_need_path_stat=1 nkv_max_key_length=1024 and so on.")

        args = parser.parse_args(sys.argv[2:])

        if args.conf:
            nkv_conf_file = "../conf/" + args.conf
        else:
            print("No config file provided")
            return

        if args.addr:
            addr_octet = args.addr
        else:
            print("No address octet provided")
            return

        if args.numa:
            numa = args.numa
        else:
            print("No numa number given")
            return

        if args.workload:
            workload = args.workload
        else:
            print("provide workload type")
            return

        nkv_kv_pair = args.kvpair

        cmd = 'nvme list-subsys | grep ' + addr_octet + ' | awk \'{ print "/dev/" $2 "n1" }\' | paste -sd,'
        drive_list = get_drives_list(cmd)
        create_config_file(nkv_kv_pair, drive_list, nkv_conf_file)

        meta_str = socket.gethostname() + "/numa" + numa

        run_nkv_test_cli(nkv_conf_file, numa, workload, meta_str, args.keysize, args.valsize, args.threads, args.numobj)

    def create_nkv_conf(self):
        '''
        Create config file for nkv_test_cli using the arguments provided.
        Find out the drives to run by running nvme list-subsys command.
        '''
        parser = argparse.ArgumentParser(
            description='Generates nkv_config.json to run nkv_test_cli on each node')
        parser.add_argument("-c", "--conf", type=str, help="nkv config json file name to create")
        parser.add_argument("-x", "--kvpair", type=str, default=None, nargs="+", help="one or more key=value pairs for nkv_config.json \
                            file update. For e.g., nkv_need_path_stat=1 nkv_max_key_length=1024 and so on.")
        parser.add_argument("-a", "--addr", type=str, help="Provide IP octets to distinguish the drives in 'nvme list-subsys' \
                           output (e.g., 201.0 (for drives connected using 1 IP). Or 20[13].0 to find drives connected using 2 IPs. \
                           i.e., 201.0 or 203.0 )", default="traddr")
        args = parser.parse_args(sys.argv[2:])

        if args.conf:
            nkv_conf_file = "../conf/" + args.conf
        else:
            print("No config file provided")
            return

        if args.addr:
            addr_octet = args.addr
        else:
            print("No address octet provided")
            return

        if args.kvpair:
            nkv_kv_pair = args.kvpair

        cmd = 'nvme list-subsys | grep ' + addr_octet + ' | awk \'{ print "/dev/" $2 "n1" }\' | paste -sd,'
        drive_list = get_drives_list(cmd)
        create_config_file(nkv_kv_pair, drive_list, nkv_conf_file)


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

