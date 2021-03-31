"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

JSONRPC_RECV_PACKET_SIZE = 4096
DEFAULT_SPDK_SOCKET_PATH = "/var/run/spdk.sock"
SPDK_CONF_NUM_OF_MD_PARAMS = 6
SPDK_RESET = False  # Upon removal and failure reset the device to nvme driver
NVMF_CONF_HEADER = ("# AUTO-GENERATED FILE - DO NOT EDIT\n"
                    "[Global]\n"
                    "LogFacility \"local7\"\n"
                    "\n"
                    "[Nvmf]\n"
                    "AcceptorCore 25\n"
                    "AcceptorPollRate 10\n"
                    "\n"
                    "[DFLY]\n"
                    "KV_PoolEnabled Yes\n"
                    "QoS            No\n"
                    "Qos_host       default_host            \"R:10K L:100M P:50\"\n"
                    "Qos_host       2014-08.org.nvmexpress  \"R:10K L:100M P:50\"\n"
                    "\n"
                    "fuse_enabled No\n"
                    "fuse_nr_maps_per_pool 4\n"
                    "fuse_nr_cores   4\n"
                    "fuse_debug_level 0\n"
                    "fuse_timeout_ms 0\n"
                    "\n"
                    "[Transport1]\n"
                    "Type TCP\n"
                    "MaxQueuesPerSession 64\n"
                    "MaxIOSize 2097152\n"
                    "IOUnitSize 2097152\n"
                    "\n"
                    "#[Transport2]\n"
                    "#Type RDMA\n"
                    "#MaxQueuesPerSession 64\n"
                    "#MaxIOSize 2097152\n"
                    "#IOUnitSize 2097152\n"
                    "\n")
