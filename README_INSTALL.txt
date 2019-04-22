Here is the steps to install nkv stack and test it out.

Supported OS and Kernel:
-----------------------
CentOS Linux release 7.6.1810 (Core)
3.10.0-514.el7.x86_64

Build open_mpdk driver:
----------------------

 1. cd open_mpdk/PDK/driver/PCIe/kernel_driver/kernel_v3.10/
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

 1. Run "nvme list" command to identify the SamSung KV devices, let's say it is mounted on /dev/nvme0n1
 2. "cd <root-package>/bin" and run the following command and check if similar output is coming or not in your setup.
 //PUT
 [root@msl-ssg-sk01 bin]# ./sample_code_sync -d /dev/nvme0n1 -n 10 -o 1 -k 16 -v 4096
 Total time 0.00 sec; Throughput 12093.16 ops/sec

 //GET
 [root@msl-ssg-sk01 bin]# ./sample_code_sync -d /dev/nvme0n1 -n 10 -o 2 -k 16 -v 4096
 retrieve tuple 000000000000000 with value = 000000000000010, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000001 with value = 000000000000011, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000002 with value = 000000000000012, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000003 with value = 000000000000013, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000004 with value = 000000000000014, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000005 with value = 000000000000015, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000006 with value = 000000000000016, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000007 with value = 000000000000017, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000008 with value = 000000000000018, vlen = 4096, actual vlen = 4096
 retrieve tuple 000000000000009 with value = 000000000000019, vlen = 4096, actual vlen = 4096
 Total time 0.00 sec; Throughput 8200.93 ops/sec

Run NKV test cli:
----------------

All good so far, let's now nkv test cli to see if nkv stack is working fine or not

 1. export LD_LIBRARY_PATH=<package-root>/lib, for ex: "export LD_LIBRARY_PATH=/root/som/nkv_minio_package/lib"
 2. vim ../config/nkv_config.json
 3. nkv_config.json is the config file for NKV. It has broadly 3 section for now, global, "nkv_mounts" and "subsystem_maps".
    Config file is mostly designed for remote NVMEoF targets and thus the term subsystem,nqn etc. NKV api doc has the detailed
    explanation of all the fields. For local KV devices user only needs to change the "mount_point" field under "nkv_mounts".
    Provide the dev path (/dev/mvme*) from nvme list command like we use for running "sample_code_sync" above.
    Example config file has two mount points defined and thus two dev path, "/dev/nvme4n1" and "/dev/nvme5n1"
    Other fields under "nkv_mounts" need not be changed for local devices. If we need to add more devices, we need to add new section
    under 'subsystem_transport' and new section with same ip/port/nqn under 'nkv_mounts'.

 4. Create log folder "/var/log/dragonfly/" if doesn't exists.
 5. Run "./nkv_test_cli" command to find the usage information.
 6. ./nkv_test_cli -c /root/som/nkv_minio_package/config/nkv_config.json -i msl-ssg-sk01 -p 1030 -b minio_nkv -o 3
 7. This command should generate output on console as well as /var/log/dragonfly/nkv.log 
 8. On successful run, it should generate output similar to the following.

    2019-01-21 11:00:09,801 [139853549025152] [INFO] NKV Store successful, key = minio_nkv_9
    2019-01-21 11:00:09,801 [139853549025152] [INFO] Sending IO to dev mount = /dev/nvme4n1, container name = nqn-02, target node = msl-ssg-sk01, path ip = 101.100.10.31, path port = 1023
    2019-01-21 11:00:09,801 [139853549025152] [INFO] Retrieve option:: decompression = 0, decryption = 0, compare crc = 0, delete = 0
    2019-01-21 11:00:09,801 [139853549025152] [INFO] NKV retrieve operation is successful for key = minio_nkv_9
    2019-01-21 11:00:09,801 [139853549025152] [INFO] NKV Retrieve successful, key = minio_nkv_9, value = 0000000000000009, len = 4096, got actual length = 0
    2019-01-21 11:00:09,801 [139853549025152] [INFO] Data integrity check is successful for key = minio_nkv_9

9. To get list of keys, run the following command
   ./nkv_test_cli -c /root/som/dragonfly/adapter/nkv-client/package/nkv_minio_package/config/nkv_config.json -i msl-ssg-dl04 -p 1030 -b meta/root/som/samsung  -k 128 -o 4 -n 10000
  
   -b <prefix> - will filter on the key prefix
   -k <key-length> - Key size
   -o <op-type> - 4 is for listing
   -n <num_ios> - max number of keys at one shot

 
Building app on top of NKV:
--------------------------

 1. Header files required to build the app is present in <root-package>/include folder
 2. NKV library and other dependent libraries are present in <root-package>/lib
 3. nkv_test_cli code is provided as reference under <root-package>/test folder 
 4. Supported api so far , nkv_open, nkv_close, nkv_physical_container_list, nkv_malloc, nkv_zalloc, nkv_free, nkv_store_kvp, nkv_retrieve_kvp, nkv_delete_kvp

Running MINIO app :
------------------

 1. export LD_LIBRARY_PATH=<package-root>/lib
 2. export MINIO_NKV_CONFIG=<package-root>/config/nkv_config.json
 3. cd <package-root>/bin
 4. ./minio_nkv_02_21 server  /ip/101.100.10.31 /ip/102.100.10.31 /ip/103.100.10.31 /ip/104.100.10.31
 5. IPs above should be matching the IPs given to nkv_config.json under 'subsystem_transport' and 'nkv_mounts'
 
