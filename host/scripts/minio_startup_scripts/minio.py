import os
import sys
import argparse
from config import Config
from fabric_manager import NativeFabricManager, UnifiedFabricManager
from auto_discovery import AutoDiscovery
from utility import exception, exec_cmd
from os.path import abspath, dirname
import time
import socket
import re
import json

nvme_pattern = re.compile(r'/dev/nvme(\d)n1')
base_path = abspath(dirname(abspath(__file__)) + "/..")

MIN_MINIO_PATH = 4
MINIO_PORT = 9000
HOST_NAME = socket.gethostname()
IP_ADDRESS = socket.gethostbyname(HOST_NAME)


def get_hosts():
    """
    Read host and ip information for distributed minio from /etc/hosts file.
    The ip and host should be with the following TAG
    #START
    <ip address1> minio1
    <ip address2> minio2
    #END
    :return: <list> - return list of hosts retrieve from /etc/hosts file.
    """
    etc_hosts = "/etc/hosts"
    host_list = []
    with open(etc_hosts, "r") as FH:
        is_host_ip_mapping_present = False
        for line in FH.readlines():
            if line.strip():
                if line.startswith("#START"):
                    is_host_ip_mapping_present = True
                elif line.startswith("#END"):
                    is_host_ip_mapping_present = False
                elif is_host_ip_mapping_present:
                    ip, host = line.split()
                    host_list.append(host)
    if not host_list:
        print("ERROR: Empty hostlist, please check /etc/hosts file")
    return host_list


def setup_distributed_minio(config={}):
    """
    Set the environment variables for distributed Minio for remote KV drives
    :param config:
    :return: None
    """
    nkv_library = config.get("nkv_library", False)
    nkv_config = config.get("nkv_config", False)
    if not os.path.exists(nkv_config):
        print("ERROR: nkv configuration file doesn't exist - {}".format(nkv_config))
        sys.exit()
    os.environ["LD_LIBRARY_PATH"] = nkv_library
    os.environ["MINIO_NKV_CONFIG"] = nkv_config
    os.environ["MINIO_ACCESS_KEY"] = "minio"
    os.environ["MINIO_SECRET_KEY"] = "minio123"
    os.environ["MINIO_STORAGE_CLASS_STANDARD"] = "EC:2"
    os.environ["MINIO_NKV_MAX_VALUE_SIZE"] = "2097152"
    os.environ["MINIO_NKV_TIMEOUT"] = "20"
    os.environ["MINIO_NKV_SYNC"] = "1"
    os.environ["MINIO_NKV_SHARED_SYNC_INTERVAL"] = "2"
    os.environ["MINIO_NKV_SHARED"] = "1"
    os.environ["MINIO_PER_HOST_INSTANCE_COUNT"] = "1"


def setup_minio(config={}):
    """
    Set the environment variables for standalone Minio for remote KV drives.
    :param config:
    :return:
    """
    nkv_library = config.get("nkv_library", False)
    nkv_config = config.get("nkv_config", False)

    os.environ["LD_LIBRARY_PATH"] = nkv_library
    os.environ["MINIO_NKV_CONFIG"] = nkv_config
    os.environ["MINIO_ACCESS_KEY"] = "minio"
    os.environ["MINIO_SECRET_KEY"] = "minio123"
    os.environ["MINIO_STORAGE_CLASS_STANDARD"] = "EC:2"
    os.environ["MINIO_NKV_MAX_VALUE_SIZE"] = "2097152"
    os.environ["MINIO_NKV_TIMEOUT"] = "20"
    os.environ["MINIO_NKV_SYNC"] = "1"


@exception
def run_minio(remote_mount_paths, config, minio_index=0):
    """
    Bring up Minio
    :param remote_mount_paths:
    :param config:
    :return:
    """

    # Set ulimit
    ulimit = "ulimit -n 65535; ulimit -c unlimited; "
    print("Base Path:{}".format(base_path))
    # minio_bin = base_path + "/scripts/" + config["minio_bin"]
    minio_bin = config["minio_bin"]
    minio_port = MINIO_PORT + minio_index

    minio_run_cmd = "{}  server  --address {}:{} ".format(minio_bin, IP_ADDRESS, MINIO_PORT + minio_index)

    mount_paths = []
    if config.get("minio_distributed", False):
        # Update environment variables
        setup_distributed_minio(config)
        host_list = get_hosts()
        print("INFO: Launching Distributed Minio with following remote mount paths ...")
        for host in host_list:
            for path in remote_mount_paths:
                remote_path = " http://" + host + path
                mount_paths.append(remote_path)
    else:
        # Update environment variables
        setup_minio(config)
        print("INFO: Launching Minio with following remote mount paths ...")
        mount_paths = remote_mount_paths

    os.system(ulimit)
    mount_paths = sorted(mount_paths)
    for path in mount_paths:
        print("\t\t\t\t - {}".format(path))
    minio_run_cmd += " ".join(mount_paths)
    ret, output = exec_cmd(minio_run_cmd, False, False)
    return ret


@exception
def validate_minio_paths(nqn_to_remote_paths, remote_mount_paths=[], nic_index=0):
    """
    Validate minio path requirements. Minimum 4 subsystem is required for remote KV
    :param: nqn_to_remote_paths = {nqn:[path1, path2]}
    :param: remote_mount_paths = ["/dev/nvme0n1","/dev/nvme1n1","/dev/nvme2n1","/dev/nvme3n1"]
    :param: nic_index =  0 (default)
    :return:
    """
    subsystem_count = len(nqn_to_remote_paths)

    if subsystem_count < MIN_MINIO_PATH:
        print("ERROR: Require minimum {} remote paths ".format(MIN_MINIO_PATH))
        return False
    else:
        for nqn in nqn_to_remote_paths:
            paths = sorted(nqn_to_remote_paths[nqn])
            if(nic_index + 1 <= len(paths)):
                remote_mount_paths.append(paths[nic_index])
            else:
                print("ERROR: Doesn't have enough path for subsystem nqn - {}".format(nqn))

    if len(remote_mount_paths) < MIN_MINIO_PATH:
        print("ERROR: Minio minimum path requirement (MIN_MINIO_PATHS = {}) failed - Total Path:{}".format(MIN_MINIO_PATH, len(remote_mount_paths)))
        return False

    return True


def dump_cluster_map(cluster_map):
    """
    Dump cluster_map in the cluster_map.json file in the present working directory.
    :param clustermap:
    :return:
    """
    print("INFO: Writing cluster_map into {}".format(os.getcwd()))
    with open("cluster_map.json", "w") as FH:
        FH.write(json.dumps(cluster_map, indent=2))


def process_command_line():
    """
    Process command line arguments.
    :return: <dict> return parameters and corresponding values.
    """
    parser = argparse.ArgumentParser(description='')
    parser.add_argument("--config", "-c", type=str, required=False, help='Specify minio configuration file path')
    parser.add_argument("--transport", "-t", type=str, required=False, help='Specify transport such as tcp,rdma')
    parser.add_argument("--minio_host", "-mh", type=str, required=False, default="myminio", help='Specify minio host name')
    parser.add_argument("--minio_distributed", "-dist", action='store_true', required=False,
                        help='Minio in distributed mode')
    parser.add_argument("--minio_instances", "-mi", type=int, required=False, help='Minio instances')
    parser.add_argument("--different_nic", "-dn", action='store_true', required=False, help='Allow to program run for different target NIC.')
    parser.add_argument("--nic_index", "-ni", type=int, required=False, default=0, help='Target NIC Index, default 0')
    parser.add_argument("--cluster_map", "-cm", action='store_true', required=False, help='Dump ClusterMap')

    options = parser.parse_args()

    return vars(options)


def main():

    # Process Command Line arguments
    params = process_command_line()
    # get config
    config_obj = Config(params)
    config = config_obj.get_config()

    FM = {}
    if config.get("ufm", False):
        FM = UnifiedFabricManager(config.get("fm_address"), config.get("fm_port"), config.get("fm_endpoint"))
    else:
        FM = NativeFabricManager(config.get("fm_address"), config.get("fm_port"), config.get("fm_endpoint"))

    cluster_map = FM.get_cluster_map()
    if config.get("cluster_map"):
        dump_cluster_map(cluster_map)

    if cluster_map:
        auto_discovery = AutoDiscovery(cluster_map["subsystem_maps"])
        subsytem_to_remote_mount_paths = auto_discovery.get_remote_mount_paths()
        # Lunch Minio with mount paths
        print("Remote Mount Paths: {}".format(subsytem_to_remote_mount_paths))
        minio_index = 0
        while(minio_index < config["minio_instances"]):
            remote_mount_paths = []
            nic_index = config["nic_index"]
            if(config["different_nic"]):
                nic_index = minio_index
            if validate_minio_paths(subsytem_to_remote_mount_paths, remote_mount_paths, nic_index):
                ret = run_minio(remote_mount_paths, config, minio_index)
                if not ret:
                    print("INFO: Minio instance - {} is up ".format(minio_index + 1))
                else:
                    print("ERROR: Failed to launch Minio ")
            else:
                print("ERROR: Not able to launch Minio instance - {}".format(minio_index + 1))
            minio_index += 1
    else:
        print("ERROR: Empty ClusterMap, Auto Discovery can't be performed ")


if __name__ == "__main__":
    main()
