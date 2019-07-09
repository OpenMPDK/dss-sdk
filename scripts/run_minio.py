import os,sys
import argparse
from config import Config
from cluster_map import ClusterMap
from auto_discovery import AutoDiscovery
from utility import exception, exec_cmd


def setup_minio():
    os.environ["LD_LIBRARY_PATH"]  = "/home/somnath.s/work/opennkv_out/nkv-sdk/lib"
    os.environ["MINIO_NKV_CONFIG"] = "/home/somnath.s/work/opennkv/config/nkv_ad_cfg.json"
    os.environ["MINIO_ACCESS_KEY"] =  "minio"
    os.environ["MINIO_SECRET_KEY"] =  "minio123"
    os.environ["MINIO_STORAGE_CLASS_STANDARD"] =  "EC:2"
    os.environ["MINIO_NKV_MAX_VALUE_SIZE"] = "2097152"
    os.environ["MINIO_NKV_TIMEOUT"] = "20"
    os.environ["MINIO_NKV_SYNC"] = "1"


@exception
def  run_minio(remote_mount_path):
    # Update environment variables
    setup_minio()

    # Set ulimit
    ulimit = "ulimit -n 65535; ulimit -c unlimited; "


    minio_run_cmd = "LD_PRELOAD=/lib64/libjemalloc.so.1  /home/somnath.s/work/Minio/minio_nkv_jun18.1  server "
    for mount_path in remote_mount_path:
        minio_run_cmd += " " + mount_path

    print("INFO: Minio run command {}".format(minio_run_cmd))

    os.system(ulimit + minio_run_cmd)


def  process_command_line():
    """
    Process command line arguments.
    :return: <dict> return parameters and corresponding values.
    """
    parser = argparse.ArgumentParser(description='')
    parser.add_argument("--config", "-c", type=str, required=False, help='Specify configuration file path')
    parser.add_argument("--transport", "-t", type=str, required=False, help='Specify transport such as tcp,rdma')
    options = parser.parse_args()

    return vars(options)


def main():

    #Process Command Line arguments
    params =  process_command_line()
    # get config
    config_obj  = Config(params)
    config =  config_obj.get_config()

    # URL
    fm_url = "http://" + config.get("fm_address") + config.get("fm_endpoint")

    # Cluster Map
    cm =ClusterMap(fm_url)


    #print(cm.get_cluster_map())
    cluster_map = cm.get_cluster_map()
    if cluster_map:
        auto_discovery = AutoDiscovery(cluster_map)
        #print("DEBUG: {}".format(auto_discovery.nqn_ip_port_to_remote_nvme))
        remote_mount_paths = auto_discovery.get_remote_mount_paths()
        #print("DEBUG: {}".format(remote_mount_paths))
        # Lunch Minio with mount paths
        run_minio(remote_mount_paths)
    else:
        print("ERROR: Empty ClusterMap, Auto Discovery can't be performed ")








if __name__ == "__main__":
     main()