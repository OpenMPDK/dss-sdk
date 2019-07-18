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

bool update_mount_path(boost::property_tree::ptree & pt, 
                       std::string& nqn,
                       std::string& target_node,
                       std::string& ip_address,
                       int32_t port,
                       const std::string& mount_path,
                       int32_t numa_node_attached
                       );

bool update_mount_path(boost::property_tree::ptree & pt, 
                       std::string& nqn,
                       std::string& target_node,
                       std::string& ip_address,
                       int32_t port,
                       const std::string& mount_path,
                       int32_t numa_node_attached
                       );

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


bool split_lines(const std::string& result, const char& delimitter, vector<string>& lines)
{
  std::stringstream data(result);
  if(! result.empty())    
  {
    string line;
    while(getline(data, line, delimitter))
    {
      boost::trim(line); // trim spaces
      lines.push_back(line);
    }
  }
  else
  {
    smg_warn(logger,"Empty mount path");
    return false;
  }
  return true;
}

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
                    std::unordered_map<std::string, std::vector<std::string>> discover_map)
{
  std::string nvme_discover_cmd = "nvme discover -t " + transport + " -a " + ip_address + " -s " + port;
  std::string result;
  // Run discover command
  if (! nkv_cmd_exec( nvme_discover_cmd.c_str(), result) ) {
    // Parse result
    std::vector<std::string> lines;
    if( split_lines(result, '\n', lines) ) {
      std::string address;
      std::string port;

      std::string subsystem_nqn;
            
      for (const std::string& line: lines){
        // traddr for address
        if ( line.compare(0,6, "traddr") == 0 ){
          std::vector<string> fields; // traddress: 10.1.20.1
          split_lines(line, ':', fields);
          address = fields[1];
        }
        // check trsvcid for port
        else if( line.compare(0,7, "trsvcid") == 0 ){
          std::vector<string> fields; // trsvcid: 1024
          split_lines(line, ':', fields);
          port = fields[1];
        }
        else if(line.compare(0,6, "subnqn") == 0){
          std::vector<string> fields; // subnqn:  nqn.2018-04.samsung:msl-ssg-mp03-data
          split_lines(line, ':', fields);
          subsystem_nqn = fields[1];
        }
                
        // Update data to discover map
        if(! subsystem_nqn.empty() && ! address.empty() && ! port.empty() ) {
          address = address + ":" + port; // address:port
          discover_map[subsystem_nqn].push_back(address);
          address = "";
          port    = "";
          subsystem_nqn = "";
        }
      } // End of for (const std::string& line: lines)
    }

  }// end of if (! nkv_cmd_exec())
}

/* Function Name: nvme_connect
 * Input Params : <string> Subsystem NQN
 *                <string> Transport IP Address
 *                <string> Transport PORT
 * Return       : <bool> Success/Failure
 * Description  : Perform NVME connect for a transport ip and port.
 */
bool nvme_connect(std::string subsystem_nqn,
                  std::string address,
                  int32_t port)
{
  std::string connect_cmd("nvme connect -t tcp -a " + address + " -n " + subsystem_nqn + " -s " + std::to_string(port) + " 2>&1");
  std::string error;
  std::string nqn_address_port = subsystem_nqn + ":" + address + ":" + std::to_string(port);
  if (nkv_cmd_exec(connect_cmd.c_str(), error)){
    smg_error(logger,"Auto Discovery: %s" , error.c_str());
    return false;
  }
  else{
    if( error.rfind("Failed", 0) == 0 ){
      smg_error(logger, "nvme connect FAILED for %s \n %s", nqn_address_port.c_str(), error.c_str());
      return false;
    }
    else{
      smg_alert(logger, "nvme connect SUCCESS for %s", nqn_address_port.c_str());
    }
  }
  return true;
}

/* Function Name: nvme_disconnect
 * Input Params : <string> Subsystem NQN
 *                <string> Transport IP Address
 *                <string> Transport PORT
 * Return       : <bool> Success/Failure
 * Description  : Perform NVME disconnect for a subsystem NQN.
 */
bool nvme_disconnect(std::string subsystem_nqn)
{
  // Disconnect subsystem nqn first
  std::string disconnect_cmd("nvme disconnect -n " + subsystem_nqn + " 2>&1");
  smg_debug(logger, "Disconnect command for a Subsystem NQN %s:\n%s",subsystem_nqn.c_str(), disconnect_cmd.c_str());

  std::string disconnect_result;

  if ( nkv_cmd_exec(disconnect_cmd.c_str(), disconnect_result)){
    smg_error(logger, "Auto Discovery: %s", disconnect_result.c_str());
    return false;
  }
  else{
    if( disconnect_result.rfind("Failed", 0) == 0 ){
      smg_error(logger, "NVME disconnect failed - %s", disconnect_result.c_str());
      return false;
    }
    else{
      smg_info(logger, disconnect_result.c_str());
    }
  }
  return true;
}


/* Function Name: read_file
 * Input Params : <const string> complete file path 
 *                <int32_t> , line to start retriving data
 *                <int32_t> , number of lines to read
 * Return       : <vector<string>> , List of lines
 * Description  : Read the sepcified number of lines from that file.
 */
void read_file(const std::string& file_path, int32_t start_line, int32_t line_to_read, std::vector<std::string>& lines)
{
  ifstream fh (file_path);
  int32_t index = 1;
  try{
    if (fh.is_open()) {
      while(! fh.eof() ) {
        if( index >= start_line && index < index + line_to_read ) {
          std::string line;
          getline (fh,line);
          lines.push_back(line);
          index++;
        }
      }
      fh.close();
    }
    else {
      smg_error(logger, "Auto Discovery: Couldn't open file %s", file_path.c_str());
    }
  }
  catch(std::exception & e){
    smg_error(logger, "Auto Discovery: %s", e.what());
  }
}

/* Function Name: get_address_port
 * Input Params : <const string> ,  Specify file name having address and port /sys/block/nvme0n1/device/address
 * Return Params: <bool> Success/Failure
 *                <string>, passed address gets updated
 *                <string>, passed port gets updated
 * Description  : Read address and port from nvme address file. 
 */
bool get_address_port( const std::string& file, std::string& address, std::string& port)
{
  std::string address_cmd = "head -1 " +  file  + " | cut -d ',' -f 1 | cut -d '=' -f 2";
  if(nkv_cmd_exec(address_cmd.c_str(), address)){
    return false;
  }
  boost::trim(address);
  std::string port_cmd = "head -1 " +  file  + " | cut -d ',' -f 2 | cut -d '=' -f 2";
  if(nkv_cmd_exec(port_cmd.c_str(), port)){
    return false;
  }
  boost::trim(port);
  return true;
}

/* Function Name: get_numa_node
 * Input Params : <string>, Remote nvme base path /sys/block/nvme0n1
 * Return       : <int32_t>, Return numa code
 * Description  : Read numa code from /sys/block/nvme0n1/device/numa_code
 *                Return numa code
 */
int32_t get_numa_node(std::string& remote_nvme_path)
{
  const std::string numa_node_file = remote_nvme_path + NUMA_NODE_PATH;
  int32_t numa_node_attached;
  std::vector<std::string> lines;

  read_file(numa_node_file, 1,1,lines);

  if(lines.size()) {
    numa_node_attached = std::stoi(lines[0], nullptr, 10);
  }
  else {
    smg_error(logger, "Auto Discovery: Not able to read numa_node from %s", numa_node_file.c_str());
    return -1;
  }
  return numa_node_attached;
}

/* Function Name: get_subsystem_nqn
 * Input Params : <string> , nvme nase path /sys/bolck/nvme0n1
 *                <string> , subsystem nqn to be updated in the function
 * Return       : None
 * Description  : Get subsystem nqn name from /sys/block/nvme0n1/device/subsysnqn
 */
void get_subsystem_nqn(std::string& nvme_base_path, std::string& subsystem_nqn)
{
  const std::string subsystem_nqn_file = nvme_base_path + SUBSYSTEM_NQN_PATH;
  std::vector<std::string> lines;
  read_file(subsystem_nqn_file, 1,1,lines);
  if(lines.size()) {
    subsystem_nqn = lines[0];
  }
  else {
    smg_error(logger, "Couln't find subsystem nqn from %s", subsystem_nqn_file.c_str());
  }
}

/* Function Name: get_nvme_mount_dir
 * Input Params : <string>, system block path "/sys/block"
 *                <unordered_map>, nqn_address_port mapping to nvme directory 
 * Return       : <bool> Success/Failure
 * Description  : Get a mapping of nvme directory to unique "nqn:address:port". 
 *                Read address from /sys/block/nvme0n1/device/address
 *                And nqn from  /sys/block/nvme0n1/device/subsysnqn
 */
bool get_nvme_mount_dir(const std::string& sys_block_path, 
                         std::unordered_map<std::string,std::string>& ip_to_nvme_mount_dir
                        )
{
  path p(sys_block_path);
  if ( exists(p) && is_directory(p) ) {
    // Search for nvme directories
    for( auto it = directory_iterator(p); it != directory_iterator(); it++) {
            
      if( is_directory( it->path() ) ){
        std::string nvme_dir  = it->path().filename().string(); // nvme dir such as nvme0n1
        if ( nvme_dir.compare(0,4,"nvme") == 0 ){
          // Check ip address and port from the address file for each ip transport ip address
          std::string nvme_base_path = sys_block_path + "/" + nvme_dir;
          const std::string address_file   = nvme_base_path + "/device/address";

          std::string address;
          std::string port;
          if(! get_address_port(address_file, address, port)) {
            smg_error(logger, "Auto Discovery: Unableto read address & port from %s", address_file.c_str() );
          }

          // Get subsystem nqn
          std::string sunsystem_nqn;
          get_subsystem_nqn(nvme_base_path, sunsystem_nqn);
                    
          if( ip_to_nvme_mount_dir.find(nvme_dir) == ip_to_nvme_mount_dir.end()) {
            std::string nqn_address_port = sunsystem_nqn + ":" + address + ":" + port;
            ip_to_nvme_mount_dir[nqn_address_port] = nvme_dir;
          }
        }
      } 
    } // end of for 
  }// end of if (Exist(p)
  return true;
}


// Nvme result , disconnect and connect.
/* Function Name: get_remote_mount_path
 * Input Params : <boost::property_tree::ptree> , pass parse tree to be updated
 * Return       : <bool> Success/Failure
 * Description  : Update the parse tree/ NKV configuration with remote_mount_path and associated destails.
 *                such as address, port, nqn, numa_node, driver_thread_count and target machine.
 */
bool add_remote_mount_path(boost::property_tree::ptree & pt)
{
  try
  {
    // Get /sys/block base path
    const std::string sys_base_path = SYS_BLOCK_PATH;

    // Get nvme mount information.
    std::unordered_map<std::string, std::string> ip_to_nvme;
    get_nvme_mount_dir(sys_base_path, ip_to_nvme);

    // Remove nkv_remote_mounts if exist
    boost::optional< ptree& > is_node_exist = pt.get_child_optional("nkv_remote_mounts");
    if(is_node_exist) {
      pt.erase("nkv_remote_mounts");
    }


    // Find remote mount path for each subsytem NQN
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("subsystem_maps")) {
      assert(v.first.empty());
      boost::property_tree::ptree subsystem_map = v.second;
      std::string target_server_name = subsystem_map.get<std::string>("target_server_name");
      std::string subsystem_nqn      = subsystem_map.get<std::string>("subsystem_nqn");

      // For each address in the transport, look for corresponding remote host path.
      BOOST_FOREACH(boost::property_tree::ptree::value_type &v, subsystem_map.get_child("subsystem_transport")) {
        assert(v.first.empty());
        boost::property_tree::ptree subsystem_transport = v.second;
        std::string subsystem_address = subsystem_transport.get<std::string>("subsystem_address");
        int32_t subsystem_port = subsystem_transport.get<int>("subsystem_port");

        // Get nvme remote mount path
        std::string remote_mount_path;
        std::string remote_nvme_dir; // nvme0n1 etc.

        std::string subsystem_nqn_address_port = subsystem_nqn + ":" + subsystem_address + ":" + std::to_string(subsystem_port);
                
        bool is_remote_mount_exist = true;
        // Check if mount path exist for that ip_address_port 
        if(  ip_to_nvme.count(subsystem_nqn_address_port) == 0 ) {
                    
          // When subsystem_ip_port from cluster map is not mounted, connect using nvme connect
          smg_info(logger, "Auto Discovery: %s is not mounted",subsystem_nqn_address_port.c_str() );
          if (nvme_connect(subsystem_nqn, subsystem_address, subsystem_port)) {
            usleep(1000 * 10); // Add a sleep for connection to complete.
            get_nvme_mount_dir(sys_base_path, ip_to_nvme); // Update ip_to_nvme mapping with new mounted remote disk.
          }
          else {  // In case nvme connect failed
            is_remote_mount_exist = false;
          }
        }
                  
        // Remote nvme mount paths
        if( is_remote_mount_exist) {
          remote_mount_path = "/dev/" + ip_to_nvme[subsystem_nqn_address_port];  // /dev/nvme01n1
          std::string remote_nvme_path = sys_base_path + "/" + ip_to_nvme[subsystem_nqn_address_port];  // /sys/block/nvme0n1
        
          int32_t numa_node_attached = get_numa_node(remote_nvme_path);
          smg_info(logger, "numa_node for %s is %d",subsystem_nqn_address_port.c_str(), numa_node_attached );
          update_mount_path(pt, subsystem_nqn,target_server_name,subsystem_address, subsystem_port, remote_mount_path, numa_node_attached);
        }

      } // End of iteration of trasporter
    }// Update into the property tree
  }
  catch ( std::exception & e) {
    smg_error(logger, "Auto Discovery: %s", e.what());
    return false;
  }
  return true;
}

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
                       )
{
  try
  {

    boost::optional< ptree& > is_node_exist = pt.get_child_optional("nkv_remote_mounts");
    
    // Create a new child for mount path
    boost::property_tree::ptree nkv_remote_element;
    nkv_remote_element.put("mount_point", mount_path);
    nkv_remote_element.put("remote_nqn_name", nqn);
    nkv_remote_element.put("remote_target_node_name", target_node);
    nkv_remote_element.put("nqn_transport_address", ip_address);
    nkv_remote_element.put("nqn_transport_port", port);
    nkv_remote_element.put("numa_node_attached", numa_node_attached);
    nkv_remote_element.put("driver_thread_core", 26);

       
    if ( is_node_exist ) {
      boost::property_tree::ptree& nkv_remote_mounts_tree  = pt.get_child("nkv_remote_mounts");
      nkv_remote_mounts_tree.push_back(std::make_pair("",nkv_remote_element));
    }
    else {
      boost::property_tree::ptree nkv_remote_mount;
      nkv_remote_mount.push_back(std::make_pair("",nkv_remote_element));
      pt.add_child("nkv_remote_mounts", nkv_remote_mount);
    }
    //boost::property_tree::write_json(std::cout, nkv_remote_mount_tree);
  }
  catch(exception& e) {
    smg_error(logger, "Auto Discovery: %s", e.what());
    return false;
  }
    
  return true;
} 

