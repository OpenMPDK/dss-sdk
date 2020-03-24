#ifndef NKV_REDFISH_API_H
#define NKV_REDFISH_API_H
#include<iostream>
#include<memory>
#include<string>
#include<cstdint>
#include<curl/curl.h>
#include<sys/socket.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include<unordered_map>
#include<vector>
#include "nkv_utils.h"
#include "csmglogger.h"


using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;
using namespace std;

extern c_smglogger* logger;

class Storage;
class Subsystem;
class Interface;
class Drive;

// Redfish class process redfish api, and contains one/more subsystems.
class Redfish
{
    ptree cluster_map;
    std::string version; // To be added by Redfish team.
    std::string name;
    std::string description;
    //std::vector<Subsystem*> subsystems;
    std::unordered_map<std::string, Subsystem*> subsystemsMap;
    
    const std::string host;
    const std::string endpoint; // Root endpoint /redfish/v1/Systems
  public:
    Redfish(){}
    Redfish(std::string& _host, std::string& _endpoint):host(_host),endpoint(_endpoint){}
    Redfish(const std::string& _host, const std::string& _endpoint):host(_host),endpoint(_endpoint){}
    ~Redfish(){}
    void process_redfish_api();
    bool get_clustermap(ptree& cluster_map);
    const std::string get_redfish_version() { return version; }
    const std::string get_name() { return name; }
    const std::string get_description() { return description; }
    bool is_subsystem(std::string& url);
};

// A subsystem has a storage and one/multiple interface/s
class Subsystem
{
  std::string target_server_name;
  std::string subsystem_nqn_id;
  std::string subsystem_nqn;
  int32_t subsystem_nqn_nsid;
  int32_t subsystem_status;  
  double subsystem_avail_percent;
  bool subsystem_numa_aligned;
  std::unordered_map<uint64_t, Interface*> interfacesMap;
  Storage* storage;

  std::string name; 
  std::string description;
  const std::string host;
  const std::string endpoint;
  public:
  Subsystem(){}
  Subsystem(const std::string& host, const std::string& endpoint):host(host),endpoint(endpoint){}
  ~Subsystem(){}
  void get_subsystem_info(ptree& subsystem_pt) const;
  void process_subsystem();
  bool add_interface(std::string interface_endpoint);
  bool add_storage();
  const std::string get_nqn() { return subsystem_nqn; }
  uint32_t is_subsystem_aligned() { return subsystem_numa_aligned? 1:0; }
};

// A storage consist of multiple Drives 
class Storage
{
  uint64_t capacity_bytes;
  double  percent_available;
  std::unordered_map<uint64_t, Drive*> drivesMap;
  uint32_t drives_count;
  std::string id;
  std::string description;
  
  const std::string subsystem_nqn;
  const std::string host;
  const std::string endpoint;
  public:
  Storage(){}
  Storage(const std::string& _host, const std::string& _endpoint, const std::string& _nqn):host(_host),
                                      endpoint(_endpoint), subsystem_nqn(_nqn)  {}
  ~Storage(){}
  void get_storage_info(ptree& storage_pt);
  double get_available_space() { return percent_available;}
  void process_storage();
  bool add_drive(std::string drive_endpoint);
  std::string get_subsystem_nqn() { return subsystem_nqn; }
};

// A single Drive (SSD Disk)
class Drive
{
  uint32_t block_size_bytes; //512
  uint64_t capacity_bytes; // 3840755982336
  std::string id; // S3VJNA0M828821
  std::string manufacturer; // Samsung
  std::string media_type; // SSD
  std::string model; // "MZQLB3T8HALS-000AZ",
  std::string protocol; //"NVMeOverFabrics",
  std::string revision; //"ETA51KBW",
  std::string serial_number; //"S3VJNA0M828821",
  uint32_t percent_available; // 100
  std::string description; // Drive Information
  std::string name;
  
  const std::string host;
  const std::string endpoint;

  public:
    Drive(){}
    Drive(std::string _host, std::string _endpoint):host(_host),endpoint(_endpoint) {}
    ~Drive(){}
    void get_drive_info(ptree& drive_pt);
    void process_drive();
    std::string get_id() { return id;}
};


class Interface
{
  int64_t interface_speed;
  std::string transport_type; // Supported protocol
  int32_t port;
  std::string address;
  int32_t status;
  int32_t ip_address_family;

  const std::string host;
  const std::string endpoint;
  public:
  Interface(){}
  Interface(std::string _host, std::string _endpoint):host(_host),endpoint(_endpoint) {}
  ~Interface(){}
  int64_t get_interface_speed() { return interface_speed; }
  int32_t get_subsystem_type() { return get_nkv_transport_value(transport_type); }
  bool get_interface_address(ptree& interface_pt);
  std::string get_address(){ return address;}
  void get_interface_info(ptree& interface_pt);
  void process_interface();  
};

#endif
