# DSS-Formatter
SPDK format application

## Introduction
The goal of this application is to write super block specific for target.

## Setup
```
* Before trying to run the application following things need to be done:
1. Generating a `spdk_conf` for the application
    1.a. This process tells spdk application about pcie devices on machine
        - run `nvme list` and `lspci | grep NVM` to select the drive of choice
        - Use pcie slot address in building the file
        - Example `spdk_conf` file would look like below:
        [Nvme]
          TransportID "trtype:PCIe traddr:0000:1d:00.0" "Nvme1"
        - Here the fields are indented by two spaces
        - `traddr` is procured from `lspci | grep NVM` command
        - The last field is arbitrary name provided to the device for app use
    1.b. Once generated place this file with the same name `spdk_conf` at  
         location of binary `dss_formatter`. This is mandatory.

2. Generating `conf-file` for the application. This step is not required for using command line options.
    2.a. This is used by dss-formatter stack to generate super-block
        - This is a tab separated file.
        - First field indicates the name of the device, this is the same name
          used in `spdk_conf` file for device prepended with `n1`
        - Second field represents the user defined logical block size
        - Third field represents the number of block states
        - Fourth field is the name of the block allocator type
        - Fifth field indicates whether the application needs to be run in debug mode
          - If run in debug mode, the application will try reading and printing the
            written super-block
        - Example `<conf-file>` file would look like below:
        Nvme1n1<\TAB>4096<\TAB>6<\TAB>block_impresario<\TAB>true
    2.b. This can be placed anywhere however path provided must be accessible.

3. Moving devices from kernel space to user space for application use
    3.a. Once `spdk_conf` and `conf-file` are generated we can move devices to
         user space using following command:
        - `./nkv-target/scripts/setup.sh`
        - This command needs to be run from `df_out` directory
        - This command is available after issuing `build.sh` from target directory
    3.b. To move the drives back into kernel space run the following command
        - `./nkv-target/scripts/setup.sh reset`
```

## Usage
```
./dss_formatter [options]
options:
 -c, --config <config>     config file (default ./spdk_conf)
     --json <config>       JSON config file (default none)
     --json-ignore-init-errors
                           don't exit on invalid config entry
 -d, --limit-coredump      do not set max coredump size to RLIM_INFINITY
 -g, --single-file-segments
                           force creating just one hugetlbfs file
 -h, --help                show this usage
 -i, --shm-id <id>         shared memory ID (optional)
 -m, --cpumask <mask>      core mask for DPDK
 -n, --mem-channels <num>  channel number of memory channels used for DPDK
 -p, --master-core <id>    master (primary) core for DPDK
 -r, --rpc-socket <path>   RPC listen address (default /var/tmp/spdk.sock)
 -s, --mem-size <size>     memory size in MB for DPDK (default: 0MB)
     --silence-noticelog   disable notice level logging to stderr
 -u, --no-pci              disable PCI access
     --wait-for-rpc        wait for RPCs to initialize subsystems
     --max-delay <num>     maximum reactor delay (in microseconds)
 -B, --pci-blacklist <bdf>
                           pci addr to blacklist (can be used more than once)
 -R, --huge-unlink         unlink huge files after initialization
 -v, --version             print SPDK version
 -W, --pci-whitelist <bdf>
                           pci addr to whitelist (-B and -W cannot be used at the same time)
      --huge-dir <path>    use a specific hugetlbfs mount to reserve memory from
      --iova-mode <pa/va>  set IOVA mode ('pa' for IOVA_PA and 'va' for IOVA_VA)
      --base-virtaddr <addr>      the base virtual address for DPDK (default: 0x200000000000)
      --num-trace-entries <num>   number of trace entries for each core, must be power of 2. (default 32768)
 -L, --logflag <flag>    enable debug log flag (all, accel_ioat, aio, app_config, bdev, bdev_ftl, bdev_malloc, bdev_null, bdev_nvme, bdev_ocssd, bdev_raid, bdev_raid0, blob, blob_rw, blobfs, blobfs_rw, dfly_fuse, dfly_io, dfly_iter, dfly_list, dfly_lock_service, dfly_module, dfly_ns_precision, dfly_numa, dfly_qos, dfly_subsystem, dfly_wal, dss_iotask, dss_kvtrans, dss_rdd, ftl_core, ftl_init, gpt_parse, ioat, json_util, log, lvol, lvolrpc, notify_rpc, nvme, nvmf, nvmf_tcp, opal, raidrpc, rdma, reactor, rpc, rpc_client, thread, trace, vbdev_delay, vbdev_gpt, vbdev_lvol, vbdev_opal, vbdev_passthru, vbdev_split, vbdev_zone_block, virtio, virtio_blk, virtio_dev, virtio_pci, virtio_user, vmd)
 -e, --tpoint-group-mask <mask>
                           tracepoint group mask for spdk trace buffers (default 0x0, bdev 0x8, nvmf_rdma 0x10, nvmf_tcp 0x20, ftl 0x40, blobfs 0x80, kvtrans 0x800, kvtrans_module 0x1000, dss_net 0x2000, dss_io_task 0x4000, all 0xffff)
-------------DSS Formatter Usage------------
 --dev_name <device name>. Required option, specify device file name configured. Usually 'n1' needs to be appended to spdk device name.
 --block_size <block size>. Optional, Defaults to 4096
 --num_block_states <total block states>. Optional, Defaults to 6
 --block_allocator_type <type string>. Optional, defaults to 'block_impresario'
 --no_verify. Optional, verifies formatted info on disk by default

```
