<!--
The Clear BSD License

Copyright (c) 2022 Samsung Electronics Co., Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in
	  the documentation and/or other materials provided with the distribution.
	* Neither the name of Samsung Electronics Co., Ltd. nor the names of its
	  contributors may be used to endorse or promote products derived from
	  this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->

# Building DSS Target

After installing required dependencies the 'build.sh' script can be run to build all packages.

To build with the default options run the following,

    ./build.sh

To build binaries with gcov coverage support run with following option,

    ./build.sh --with-coverage

The binaries and packages are created in the output folder '../df_out'

### dss_target.py

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
