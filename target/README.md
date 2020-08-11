###dss_target.py

A. Creates subsystems and configuration for nvmf_tgt
    Option 1: Configure based on vlan ids
        python dss_target.py configure --vlan-ids space-separated list of vlan ids -kv_fw firmware -kv_ssc number of kv subsystems to create
            This will automatically discover the correct IPs and create the subsystems
    Option 2: Configure using IP addresses
        python dss_target.py configure -ip_addrs space-separated list of local ip addresses -kv_fw firmware -kv_ssc number of kv subsystems to create

The only difference between option 1 and option 2 is whether the IP of the interfaces on the desired vlans is discovered based on vlan ID or specified manually

    After running either Option 1 or Option 2, it will output run_nvmf_tgt.sh. Run this script to start the client

    Command line flags:
        Option 1: -vids/--vlan-ids must be specified, -ip_addrs/--ip_addresses must NOT be specified
            -vids/--vlan-ids: space delimited list of (numeric) vlan ids
        Option 1: -ip_addrs/--ip_addresses must be specified -vids/--vlan-ids must NOT be specified, 
            -ip_addrs/--ip_addresses: List of IPs
        Common:
        -c/--config_file: Name of output file for nvmf_tgt config
        -kv_fw/--kv_firmware: KV firmware version for the SSDs
        -block_fw/--block_firmware: block firmware version for the SSDs (only if using block)
        -wal/--wal: Number of block devices to handle write burst
        -tcp/--tcp: 1 to enable tcp, 0 to disable tcp
        -rdma/--rdma: 1 to enable rdma, 0 to disable rdma
        -kv_ssc/--kv_ssc: number of kv subsystems to create
