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

#ifndef NKV_UFM_API_H
#define NKV_UFM_API_H
#include<iostream>
#include<memory>
#include<string>
#include<cstdint>
//#include<curl/curl.h>
#include<sys/socket.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <unordered_map>
#include <vector>
#include "nkv_utils.h"
#include "csmglogger.h"
#include "fabric_manager.h"


using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;
using namespace std;

extern c_smglogger* logger;

class Subsystem;
class Storage;
class Interface;
class Drive;


/* UnifiedFabricManager supports KV/Block SSD, Ethernet SSD. It provides target
 * clustermap information based on Redfish API. A rest API that follows redfish
 * standards. Presently it si developed based on two mixed standrads v1.9.0 and
 * v1.5.1.   
 * Features Available: Complete target system informationwhich includes subsystem
 * Interfaces, Storage and Drives.
 *
 * TODO: 
 *  - Fabric Manager cluster details.
 *  - UFM version disclose.
 */

// UnifiedFamricManager class process redfish api, and contains one/more subsystems.
class UnifiedFabricManager:public FabricManager
{
    ptree cluster_map;
    string version; // To be added by Team.
    string name;
    string description;
    unordered_map<string, Subsystem*> subsystemsMap;

    //const string endpoint; // Root endpoint /redfish/v1/Systems
    const string fm_name = "UnifiedFabricManager";
    uint32_t node_count; // Number of node in UFM cluster
  public:
    UnifiedFabricManager(const string& _host, const string& _endpoint):
                                         FabricManager(_host,_endpoint)
    {
      node_count = 1;
    }
    ~UnifiedFabricManager();
    
    bool process_clustermap();
    void generate_clustermap();
    const string get_redfish_version() { return version; }
    const string get_name() { return name; }
    const string get_description() { return description; }
    bool is_subsystem(string& url);
};

// A subsystem has a storage and one/multiple interface/s
class Subsystem
{
  string target_server_name;
  string subsystem_nqn_id;
  string subsystem_nqn;
  int32_t subsystem_nqn_nsid;
  int32_t subsystem_status;  
  double subsystem_avail_percent;
  bool subsystem_numa_aligned;
  unordered_map<string, Interface*> interfacesMap;
  Storage* storage;

  string name; 
  string description;
  const string host;
  const string endpoint;
  public:
  Subsystem(const string& _host, const string& _endpoint):host(_host),
                                                  endpoint(_endpoint)
  {
    subsystem_nqn_nsid = 1;
    subsystem_status = 1;
    subsystem_avail_percent = 100.0;
    subsystem_numa_aligned = false;
    storage = NULL;
  }
  ~Subsystem();
  void get_subsystem_info(ptree& subsystem_pt) const;
  bool process_subsystem();
  bool add_interface(string interface_endpoint);
  bool add_storage();
  const string get_nqn() { return subsystem_nqn; }
  uint32_t is_subsystem_aligned() { return subsystem_numa_aligned? 1:0; }
};

// A storage consist of multiple Drives 
class Storage
{
  uint64_t capacity_bytes;
  double  percent_available;
  unordered_map<string, Drive*> drivesMap;
  uint32_t drives_count;
  string id;
  string description;
  
  const string host;
  const string endpoint;
  const string subsystem_nqn;
  public:
  Storage(const string& _host, const string& _endpoint, const string& _nqn):
                                                                host(_host),
                                                        endpoint(_endpoint), 
                                                        subsystem_nqn(_nqn)
  {
    capacity_bytes = 100000;
    percent_available = 100.0;
    drives_count = 1;
  }
  ~Storage();
  void get_storage_info(ptree& storage_pt);
  double get_available_space() { return percent_available;}
  bool process_storage();
  bool add_drive(string drive_endpoint);
  string get_subsystem_nqn() { return subsystem_nqn; }
};

// A single Drive (SSD Disk)
class Drive
{
  uint32_t block_size_bytes; //512
  uint64_t capacity_bytes; // 3840755982336
  string id; // S3VJNA0M828821
  string manufacturer; // Samsung
  string media_type; // SSD
  string model; // "MZQLB3T8HALS-000AZ",
  string protocol; //"NVMeOverFabrics",
  string revision; //"ETA51KBW",
  string serial_number; //"S3VJNA0M828821",
  uint32_t percent_available; // 100
  string description; // Drive Information
  string name;
  
  const string host;
  const string endpoint;

  public:
    Drive(const string& _host, const string& _endpoint):host(_host),
                                                endpoint(_endpoint)
    {
      block_size_bytes = 512;
      capacity_bytes = 3840755982336;
      percent_available = 100.0;
    }
    ~Drive(){}
    void get_drive_info(ptree& drive_pt);
    bool process_drive();
    string get_id() { return id;}
};


class Interface
{
  int64_t interface_speed;
  string transport_type; // Supported protocol
  int32_t port;
  string address;
  int32_t status;
  int32_t ip_address_family;

  const string host;
  const string endpoint;
  public:
  Interface(const string& _host, const string& _endpoint):host(_host),
                                                  endpoint(_endpoint)
  {
    interface_speed = 100;
    port = 1024;
    status = 1;
    ip_address_family = AF_INET;
  }
  ~Interface() {}
  int64_t get_interface_speed() { return interface_speed; }
  int32_t get_subsystem_type() { return get_nkv_transport_value(transport_type); }
  bool get_interface_address(ptree& interface_pt);
  string get_address(){ return address;}
  void get_interface_info(ptree& interface_pt);
  bool process_interface();  
};

// Redfish return from REST call
//static bool check_redfish_rest_call_status(ptree& response, string& uri);

#endif
