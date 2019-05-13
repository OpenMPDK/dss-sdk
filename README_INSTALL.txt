Here is the steps to install nkv stack and test it out.

Supported OS and Kernel:
-----------------------
CentOS Linux release 7.4.1708 (Core)
3.10.0-693.el7.x86_64

Unzip nkv-sdk-bin-*.tgz and it will create a folder named 'nkv-sdk' say ~/nkv-sdk.

Build open_mpdk driver:
----------------------

 1. cd nkv-sdk/openmpdk_driver/kernel_v3.10/
 2. make clean
 3. make all
 4. ./re_insmod.sh   //It may take some seconds
 5. It should unload stock nvme driver and load open_mpdk one.
 6. Run the following command to see if you are getting similar output to make sure driver loaded properly

    [root@msl-ssg-sk01 kernel_v3.10]# lsmod | grep nvme
    nvme                   62347  0
    nvme_core              50009  0

Run open_mpdk test cli:
---------------------

Run the open_mpdk test cli to make sure nvme KV driver is working fine.

 1. Run "nvme list" command to identify the Samsung KV devices, let's say it is mounted on /dev/nvme0n1
 2. "cd ~/nkv-sdk/bin" and run the following command and check if similar output is coming or not in your setup.
 //PUT
 [root@msl-ssg-sk01 bin]# ./sample_code_sync -d /dev/nvme0n1 -n 10 -o 1 -k 16 -v 4096
 ENTER: open
 EXIT : open
 Total time 0.00 sec; Throughput 8883.03 ops/sec
 KV device is closed: fd 3

 //GET
 [root@msl-ssg-sk01 bin]# ./sample_code_sync -d /dev/nvme0n1 -n 10 -o 2 -k 16 -v 4096
 ENTER: open
 EXIT : open
 Total time 0.00 sec; Throughput 9268.52 ops/sec
 KV device is closed: fd 3

Run NKV test cli:
----------------

All good so far, let's now nkv test cli to see if nkv stack is working fine or not

 1. export LD_LIBRARY_PATH=~/nkv-sdk/lib
 2. vim ../conf/nkv_config.json
 3. nkv_config.json is the config file for NKV. It has broadly 2 sections for now, global, "nkv_local_mounts" .
    NKV api doc has the detailed explanation of all the fields. For local KV devices user only needs to change 
    the "mount_point" field under "nkv_local_mounts" to run with defaults.
    Provide the dev path (/dev/mvme*) from nvme list command like we use for running "sample_code_sync" above.
    Example config file has four mount points defined and thus four dev path, "/dev/nvme14n1" .. "/dev/nvme17n1"
    If we need to add/remove devices, we need to add/remove section under 'nkv_local_mounts'.

 4. Create log folder "/var/log/dragonfly/" if doesn't exists. This is default log location. Default log level is WARN.
    Log config options can be changed from bin/smglogger.properties.

 5. Run "./nkv_test_cli" command to find the usage information.
 6. ./nkv_test_cli -c ../conf/nkv_config.json -i msl-ssg-dl04 -p 1030  -b meta/prefix1/nkv -r / -k 128 -v 4096 -o 3 -n 100
 7. This command should generate output on console as well as /var/log/dragonfly/nkv.log 
 8. On successful run, it should generate output similar to the following. For more verbose output if INFO logging is enabled

  2019-04-23 17:10:11,103 [140136478451584] [ALERT] contact_fm = 0, nkv_transport = 0, min_container_required = 1, min_container_path_required = 1, container_path_qd = 16384
  2019-04-23 17:10:11,103 [140136478451584] [ALERT] core_pinning_required = 0, app_thread_core = -1, nkv_queue_depth_monitor_required = 0, nkv_queue_depth_threshold_per_path = 0
  2019-04-23 17:10:11,103 [140136478451584] [ALERT] Adding device path, mount point = /dev/nvme14n1, address = 101.100.10.31, port = 1023, nqn name = nqn-02, target node = msl-ssg-sk01, numa = 0, core = -1
  2019-04-23 17:10:11,103 [140136478451584] [ALERT] Adding device path, mount point = /dev/nvme15n1, address = 102.100.10.31, port = 1023, nqn name = nqn-02, target node = msl-ssg-sk01, numa = 1, core = -1
  2019-04-23 17:10:11,103 [140136478451584] [ALERT] Adding device path, mount point = /dev/nvme16n1, address = 103.100.10.31, port = 1023, nqn name = nqn-02, target node = msl-ssg-sk01, numa = 1, core = -1
  2019-04-23 17:10:11,103 [140136478451584] [ALERT] Adding device path, mount point = /dev/nvme17n1, address = 104.100.10.31, port = 1023, nqn name = nqn-02, target node = msl-ssg-sk01, numa = 1, core = -1
  2019-04-23 17:10:11,103 [140136478451584] [ALERT] Max QD per path = 4096
  ENTER: open
  EXIT : open
  ENTER: open
  EXIT : open
  ENTER: open
  EXIT : open
  ENTER: open
  EXIT : open
  2019-04-23 17:10:11,144 [140136478451584] [ALERT] TPS = 6910, Throughput = 26 MB/sec, value_size = 4096, total_num_objs = 100
  KV device is closed: fd 3
  KV device is closed: fd 6
  KV device is closed: fd 8
  KV device is closed: fd 10

 9. For more verbose output, enable INFO logging in bin/smglogger.properties by editing the following.
   log4cpp.category.libnkv=WARN, nkvAppender_rolling

 10. To get list of keys, run the following command
   ./nkv_test_cli -c ../conf/nkv_config.json -i msl-ssg-dl04 -p 1030 -b meta/minio -r /  -k 128 -o 4 -n 10000
  
   -b <prefix> - will filter on the key prefix
   -k <key-length> - Key size
   -o <op-type> - 4 is for listing
   -n <num_ios> - max number of keys at one shot

 
Building app on top of NKV:
--------------------------

 1. Header files required to build the app is present in ~/nkv-sdk/include folder
 2. NKV library and other dependent libraries are present in ~/nkv-sdk/lib
 3. nkv_test_cli code is provided as reference under ~/nkv-sdk/src/test folder 
 4. Supported api so far , nkv_open, nkv_close, nkv_physical_container_list, nkv_malloc, nkv_zalloc, nkv_free, nkv_store_kvp, nkv_retrieve_kvp, nkv_delete_kvp, nkv_indexing_list_keys

Running MINIO app :
------------------
Put the following in a script may be..Need at least 4 devices to run Minio..

 1. export LD_LIBRARY_PATH=~/nkv-sdk/lib
 2. export MINIO_NKV_CONFIG=~/nkv-sdk/conf/nkv_config.json
 3. export MINIO_ACCESS_KEY=minio
 4. export MINIO_SECRET_KEY=minio123
 5. export MINIO_NKV_MAX_VALUE_SIZE=2097152
 6. export MINIO_NKV_SYNC=1
 7. ulimit -n 65535
 8. ulimit -c unlimited

 9. cd ~/nkv-sdk/bin
 10. ./<minio-binary> server  /dev/nvme{0...5}n1
 11. Mount points above should be matching the mount points given to nkv_config.json under 'nkv_local_mounts'
 12. Distributed Minio command is:

     ./<minio-binary> server  http://nkvsmchost{1...4}/dev/nvme{0...5}n1
    Where, mkvsmchost1, to nkvsmchost4 are 4 minio node names mentioned in /etc/hosts file of each server.
    /dev/nvme{0...5}n1 are KV drives.

 13. For more detailed documentation on how to run Minio with KV stack can be found in Minio web site.
 
