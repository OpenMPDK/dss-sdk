/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2019 Samsung Electronics Co., Ltd.
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
 */

#ifndef NKV_AUTO_DISCOVERY_H
#define NKV_AUTO_DISCOVERY_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/optional/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include "csmglogger.h"
#include "nkv_utils.h"
#include <unistd.h>

using namespace std;
using namespace boost::filesystem;

#ifndef LINUX_SYS_PATHS
#define SYS_BLOCK_PATH "/sys/block"
// /sys/block/nvme0n1/device/numa_node
#define NUMA_NODE_PATH "/device/numa_node"
// /sys/block/nvme0n1/device/subsysnqn
#define SUBSYSTEM_NQN_PATH "/device/subsysnqn"
#endif



extern c_smglogger* logger;
extern uint32_t nvme_connect_delay_in_mili_sec;

/************
 * Auto Discovery: Discover the remote mount path from the host machine.
 *                - One way executing a system command "nvme list | grep NKV | awk '{print $1}' "
 *                - The second one lokking at the /sys/block/nvme* path and finding out mount path
 *
 *
 *   Cluster Map
 *   NVME Discovery
 *   Search from nvme list  or /sys/block ---> check the list of connected devices.
 *   Connect if some ip is not connected.
 */

/* Function Name: update_mount_path
* Parameters   : <boost::property_tree::ptree> , NKV configuration
*                <std::string> , subsystem nqn
* Return       : <bool> Success/Failure
* Description  : Update mouunt path to the parse tree.
*/
bool update_mount_path(boost::property_tree::ptree & pt, 
                       std::string& nqn,
                       std::string& target_node,
                       std::string& ip_address,
                       int32_t port,
                       const std::string& mount_path,
                       int32_t numa_node_attached
                       );


bool split_lines(const std::string& result, char delimiter, vector<string>& lines);

/* Function Name: nvme_discovery
 * Input Params : <string> , ip address
 *                <string> , port 
 *                <string> , tranport type such as tcp, rdma
 *                <unordered_map> , Mapping of nqn to ip address and port nqn->  ["10.1.20.1:1024","10.1.20.2:1024"] 
 * Return       : None
 * Description  : Find out nqn and ip address mapping through nvme discover command.
 */
void nvme_discovery(std::string ip_address,
                    std::string port,
                    std::string transport,
                    std::unordered_map<std::string, std::vector<std::string>> discover_map);

/* Function Name: nvme_connect
 * Input Params : <string> Subsystem NQN
 *                <string> Transport IP Address
 *                <string> Transport PORT
 *                <int32_t> Transport Protocol such as 0/1 => tcp/rdma
 * Return       : <bool> Success/Failure
 * Description  : Perform NVME connect for a transport ip and port.
 */
bool nvme_connect(std::string subsystem_nqn,
                  std::string address,
                  int32_t port,
                  int32_t transport_type);

/* Function Name: nvme_disconnect
 * Input Params : <string> Subsystem NQN
 *                <string> Transport IP Address
 *                <string> Transport PORT
 * Return       : <bool> Success/Failure
 * Description  : Perform NVME disconnect for a subsystem NQN.
 */
bool nvme_disconnect(std::string subsystem_nqn);

/* Function Name: read_file
 * Input Params : <const string> complete file path 
 *                <int32_t> , line to start retriving data
 *                <int32_t> , number of lines to read
 * Return       : <vector<string>> , List of lines
 * Description  : Read the sepcified number of lines from that file.
 */
void read_file(const std::string& file_path, int32_t start_line, int32_t line_to_read, std::vector<std::string>& lines);

/* Function Name: get_address_port
 * Input Params : <const string> ,  Specify file name having address and port /sys/block/nvme0n1/device/address
 * Return Params: <bool> Success/Failure
 *                <string>, passed address gets updated
 *                <string>, passed port gets updated
 * Description  : Read address and port from nvme address file. 
 */
bool get_address_port( const std::string& file, std::string& address, std::string& port);

/* Function Name: get_numa_node
 * Input Params : <string>, Remote nvme base path /sys/block/nvme0n1
 * Return       : <int32_t>, Return numa code
 * Description  : Read numa code from /sys/block/nvme0n1/device/numa_code
 *                Return numa code
 */
int32_t get_numa_node(std::string& remote_nvme_path);

/* Function Name: get_subsystem_nqn
 * Input Params : <string> , nvme nase path /sys/bolck/nvme0n1
 *                <string> , subsystem nqn to be updated in the function
 * Return       : None
 * Description  : Get subsystem nqn name from /sys/block/nvme0n1/device/subsysnqn
 */
void get_subsystem_nqn(std::string& nvme_base_path, std::string& subsystem_nqn);

/* Function Name: get_nvme_mount_dir
 * Input Params : <unordered_map>, nqn_address_port mapping to nvme directory
 *                <string>, "<subsystem_nqn>:<ipv4_address>:<port>" 
 * Return       : <bool> Success/Failure
 * Description  : Get a mapping of nvme directory to unique "nqn:address:port". 
 *                Read address from /sys/block/nvme0n1/device/address
 *                And nqn from  /sys/block/nvme0n1/device/subsysnqn
 *                Findout the mapping of nqn:address:port and correspoding device
 *                path "/dev/nvme0n1" etc.
 */
bool get_nvme_mount_dir( std::unordered_map<std::string,std::string>& ip_to_nvme_mount_dir,
                         const std::string& subsystem_nqn_address_port
                       );

/* Function Name: get_remote_mount_path
 * Input Params : <boost::property_tree::ptree> , pass parse tree to be updated
 * Return       : <bool> Success/Failure
 * Description  : Update the parse tree/ NKV configuration with remote_mount_path and associated destails.
 *                such as address, port, nqn, numa_node, driver_thread_count and target machine.
 */

bool add_remote_mount_path(boost::property_tree::ptree & pt);

/* Function Name: device_path_exist
 * Parameters   : <std::String> - Remote device path , i.e /dev/nvme0n1
 * Returns      : bool 
 * Description  : Check if remote device path (/dev/nvme1n1 exist in the host machine?
 *                On success return true
 */

bool device_path_exist(const std::string remote_device_path );

#endif
