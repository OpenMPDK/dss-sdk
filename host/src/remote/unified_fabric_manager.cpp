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

#include "unified_fabric_manager.h"

// LinkStatus based on REdfish API spec.
unordered_map<string, int32_t> link_status = { {"LinkUp", 1 },
                                               {"LinkDown", 0},
                                               {"NoLink",-1} };

static bool check_redfish_rest_call_status(ptree& response, string& uri);

/* Function Name: process_redfish_api()
 * Args         : None
 * Return       : None
 * Description  : Process the redfish api from root endpoint "/redfish/v1/Systems"
 *                and generate complete object model of target cluster.
 */
bool UnifiedFabricManager::process_clustermap()
{
  string systems;
  string host = get_host();
  string endpoint = get_endpoint();
  string uri = host+endpoint;
  if ( RESTful(systems, uri)) {
    ptree root_pt;
    istringstream systems_is (systems);
    read_json(systems_is, root_pt);
    if ( check_redfish_rest_call_status(root_pt, uri) ) {
      bool is_subsystem_added = false;
      try{
        name = root_pt.get<string>("Name");
        description = root_pt.get<string>("Description");
        //TODO add redfish api version and print that here.

        // Process subsystems
        BOOST_FOREACH(boost::property_tree::ptree::value_type &value, root_pt.get_child("Members")) {
          assert(value.first.empty());
          boost::property_tree::ptree subsystem_pt = value.second;
          string subsystem_url;
          for(auto it: subsystem_pt ){
            subsystem_url = it.second.data();
          }
          // Determine if the URL is subsystem 
          if (! is_subsystem(subsystem_url)) {
            continue;
          }
          // Add subsystem
          Subsystem* subsystem = new Subsystem(host, subsystem_url);
          if ( subsystem->process_subsystem() ) {
            string subsystem_nqn = subsystem->get_nqn();
            subsystemsMap[subsystem_nqn] = subsystem;
            update_subsystem_nqn_list(subsystem_nqn);
            is_subsystem_added = true;
          } else {
            delete subsystem;
            subsystem = NULL;
          }
        }
        // Add warning message
        if (is_subsystem_added ) {
          generate_clustermap();
          smg_alert(logger,"ClusterMap: Received clustermap from %s", fm_name.c_str());
          smg_info(logger, "%s: consist of %d node/s cluster", fm_name.c_str(), node_count);
        } else{
          smg_error(logger, "Redfish API - no subsytem has been found ... ");
          return false;
        }
      } catch( exception& e ) {
        smg_error(logger,"Exception:-%s- %s", __func__, e.what());
        return false;
      }
    } 
  } else {
    smg_error(logger, "%s: Bad host - %s", __func__, host.c_str());
    return false;
  }
  return true;
}

/* Function Name: is_subsystem
 * Input Args   : <string&> subsystem endpoint
 * Return       : <bool> = 1/0, Success/Failure
 * Description  : Check the input member endpoint is subsystem or target?
 */
bool UnifiedFabricManager::is_subsystem(string& url)
{
  vector<string> result_url;
  vector<string> result_sub;
  boost::split(result_url, url, boost::is_any_of("/"));
  boost::split(result_sub, result_url[result_url.size() -1], boost::is_any_of("."));

  if ( result_sub.size() == 2 ) {
    return true;
  }
  return false;
}

/* Function Name: generate_clustermap
 * Args         : None
 * Return       : None
 * Description  : Generate the cluster map based on NKV host requirement. 
 */
void UnifiedFabricManager::generate_clustermap()
{
  // TODO Need to add UFM cluster nodes information
  ptree subsystems_pt;
  // Get subsystem level information
  for ( auto s_iter=subsystemsMap.begin(); s_iter != subsystemsMap.end(); ++s_iter )
  {
   
    ptree subsystem;
    ptree s_pt;
    Subsystem* s_ptr = s_iter->second;
    s_ptr->get_subsystem_info(s_pt);
    subsystem.put("subsystem_status", s_pt.get<uint32_t>("Status"));
    subsystem.put("target_server_name", s_pt.get<string>("ServerName"));
    subsystem.put("subsystem_nqn_nsid", s_pt.get<int32_t>("NSID"));
    subsystem.put("subsystem_nqn", s_pt.get<string>("NQN"));
    subsystem.put("subsystem_nqn_id",s_pt.get<string>("Id"));
    subsystem.put("subsystem_avail_percent",s_pt.get<double>("PercentAvailable"));

    ptree interfaces;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, s_pt.get_child("Interfaces")) {
      assert(value.first.empty());
      boost::property_tree::ptree interface_pt = value.second;
      ptree interface;
      interface.put("subsystem_interface_numa_aligned", s_ptr->is_subsystem_numa_aligned());
      interface.put("subsystem_interface_speed",interface_pt.get<uint32_t>("InterfaceSpeed"));
      interface.put("subsystem_type",get_nkv_transport_value(interface_pt.get<string>("TransportType")));
      interface.put("subsystem_port",interface_pt.get<uint32_t>("Port"));
      interface.put("subsystem_address",interface_pt.get<string>("Address"));
      interface.put("subsystem_interface_status",interface_pt.get<uint32_t>("Status"));
      interface.put("subsystem_addr_fam",interface_pt.get<uint32_t>("AddressFamily"));

      interfaces.push_back(make_pair("", interface));
    }
    subsystem.add_child("subsystem_transport", interfaces);
    subsystems_pt.push_back(make_pair("", subsystem));
  }
  target_cluster_map.add_child("subsystem_maps", subsystems_pt);  
}

/* Function Name: get_subsystem
 * Args         : <const string&> subsystem_nqn
 * Return       : <void*> = Return address of Subsystem for the specified nqn
 * Description  : Return the subsystem object for the specified nqn.
 */
void* UnifiedFabricManager::get_subsystem(const string& subsystem_nqn) const
{
  auto found = subsystemsMap.find(subsystem_nqn);
  if ( found == subsystemsMap.end() ) {
    return NULL;
  } 
  return subsystemsMap.find(subsystem_nqn)->second;
}

/* Function Name: process_subsystem
 * Args         : None
 * Return       : None
 * Description  : Process subsystem along with interfaces and storage.
 */
bool Subsystem::process_subsystem()
{
  // Rest call
  string rest_response;
  string subsystem_url = host + endpoint;
  RESTful(rest_response, subsystem_url);
  ptree pt;
  istringstream iss_subsystem (rest_response);
  read_json(iss_subsystem, pt);
  if ( !check_redfish_rest_call_status(pt, subsystem_url) ){
    smg_error(logger, "Failed to read subsystem information from %s", subsystem_url.c_str());
    return false;
  }

  bool is_subsystem_good = true;
  try {
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, pt.get_child("Identifiers")) {
      assert(value.first.empty());
      boost::property_tree::ptree identifiers = value.second;
      if ( identifiers.get<string>("DurableNameFormat") == "NQN" ) {
        subsystem_nqn = identifiers.get<string>("DurableName"); 
      }
    }
    //subsytem_nqn
    vector<string> uuids; // {target_uuid}.{subsystem_uuid}
    string subsystem_uuid  = pt.get<string>("Id", "default.default");
    boost::split(uuids, subsystem_uuid , boost::is_any_of("."));
    subsystem_nqn_id = uuids[1];

    if ( pt.get<string>("Status.State", "" ) == "Enabled" && pt.get<string>("Status.Health", "") == "OK" ) {
      subsystem_status =  0 ;
    } else {
      subsystem_status = 1 ;
    }
    subsystem_numa_aligned = pt.get<bool>("oem.NumaAligned");
    subsystem_nqn_nsid = pt.get<int32_t>("oem.NSID");
    target_server_name = pt.get<string>("oem.ServerName");
    name = pt.get<string>("Name");
    description = pt.get<string>("Description");

    // Add transporters
    string interfaces_url;
    for( auto itr: pt ) {
      if( itr.first == "@odata.id" ) {
        interfaces_url = itr.second.data() + "/EthernetInterfaces" ;
      }
    }
    // RESTful call to get all the transporters information from a subsystem.
    string interfaces;
    interfaces_url = host + interfaces_url;
    // Rest call to the interface
    RESTful(interfaces, interfaces_url);
    ptree interfaces_pt;
    istringstream is2 (interfaces);
    read_json(is2, interfaces_pt);

    bool is_interface_added =  false;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interfaces_pt.get_child("Members")) {
      assert(value.first.empty());
      boost::property_tree::ptree nic_pt = value.second;
      string interface_url;
      for( auto key: nic_pt) {
        if( key.first == "@odata.id" ) {
          interface_url = key.second.data();
        }
      }
      // Add interface
      if ( add_interface(interface_url) ){
        is_interface_added = true;
      }
    }
    // Error out if no interface is added.
    if ( ! is_interface_added ) {
      smg_error(logger, "Interface not added for subsystem - %s", subsystem_nqn.c_str());
      is_subsystem_good = false;
    }
    // Add storage
    if (! add_storage() ) {
      smg_error(logger, "Storage information not added for subsystem - %s", subsystem_nqn.c_str());
      is_subsystem_good = false;
    }
  } catch(exception& e) {
    smg_error(logger,"Exception: %s - %s",__func__, e.what());
    is_subsystem_good = false;
  }
  return is_subsystem_good;
}

/* Function Name: add_interface
 * Input Args   : <string> - interface_endpoint
 * Return       : <bool> 1/0, Success/Failure 
 * Description  : Process an interface for a subsystem.
 */
bool Subsystem::add_interface(string interface_endpoint)
{
  bool is_interface_added = false;
  // Create Interface object
  Interface* interface = new Interface(host, interface_endpoint);
  is_interface_added = interface->process_interface();
  if ( is_interface_added ) {
    string address = interface->get_address();
    interfacesMap[address] = interface;
  } else {
    delete interface;
    interface = NULL;
  }
  return is_interface_added;
}

/* Function Name: add_storage
 * Args         : None
 * Return       : <bool> 1/0, Success/Failure 
 * Description  : Add the storage information to a subsystem.
 */
bool Subsystem::add_storage()
{
  // Get storage endpoint ...
  string rest_response;
  string storage_url = host + endpoint + "/Storage";
  RESTful(rest_response, storage_url);
  ptree storage_pt;
  istringstream is (rest_response);
  read_json(is, storage_pt);
  if ( !check_redfish_rest_call_status(storage_pt, storage_url) ){
    smg_error(logger, "Failed to read storage information from %s", storage_url.c_str());
    return false;
  }

  string storage_endpoint;
  BOOST_FOREACH(boost::property_tree::ptree::value_type &value, storage_pt.get_child("Members")) {
    assert(value.first.empty());
    boost::property_tree::ptree s_pt = value.second;
    for( auto key: s_pt) {
      if( key.first == "@odata.id" ) {
        storage_endpoint = key.second.data();
      }
    }
  }
  // Create storage object
  bool is_storage_good = false;
  if( ! storage_endpoint.empty() ) {
    storage = new Storage(host, storage_endpoint, subsystem_nqn);
    is_storage_good = storage->process_storage();
    if ( is_storage_good ) {
      subsystem_avail_percent = storage->get_available_space();
    } else {
      delete storage;
      storage = NULL;
    }    
  } else {
    smg_error(logger, "%s:Storage endpoint is not found for - %s", __func__,subsystem_nqn.c_str());
  }
  return is_storage_good;
}

/* Function Name: get_subsystem_info
 * Args         : <ptree&> - Input subsystem parse tree
 * Return       : None
 * Description  : Read subsystem information and return theough subsyetm_pt.
 */
void Subsystem::get_subsystem_info(ptree& subsystem_pt) const
{
  subsystem_pt.put("ServerName", target_server_name);
  subsystem_pt.put("Id", subsystem_nqn_id);
  subsystem_pt.put("NQN", subsystem_nqn);
  subsystem_pt.put("NSID", subsystem_nqn_nsid);
  subsystem_pt.put("Status", subsystem_status);
  subsystem_pt.put("PercentAvailable", subsystem_avail_percent);
  subsystem_pt.put("NumaAligned", subsystem_numa_aligned);
  subsystem_pt.put("Name", name);
  subsystem_pt.put("Description", description);

  // Get all interface information.
  ptree interfaces_pt;
  for ( auto i_iter = interfacesMap.begin(); i_iter != interfacesMap.end(); i_iter++ ) {
    Interface* i_ptr = i_iter->second;
    ptree interface_info;
    i_ptr->get_interface_info(interface_info);
    interfaces_pt.push_back(make_pair("", interface_info));
  }
  subsystem_pt.add_child("Interfaces",interfaces_pt);

  // Get storage information.
  ptree storage_pt;
  storage->get_storage_info(storage_pt);
  subsystem_pt.add_child("Storage", storage_pt);
}

/* Function Name: process_storage
 * Args         : None
 * Return       : None
 * Description  : Process storage for an subsystem.
 */
bool Storage::process_storage()
{
  smg_debug(logger, "Processing storage information for Subsystem - %s", (get_subsystem_nqn()).c_str());
  string rest_response;
  string storage_url = host + endpoint;
  RESTful(rest_response, storage_url);
  ptree storage_pt;
  istringstream iss_storage (rest_response);
  read_json(iss_storage, storage_pt);

  if ( !check_redfish_rest_call_status(storage_pt, storage_url) ){
    smg_error(logger, "Failed to read storage information from %s", storage_url.c_str());
    return false;
  }

  bool is_drive_added = false;
  try{
    percent_available = storage_pt.get<double>("oem.PercentAvailable");
    capacity_bytes = storage_pt.get<long uint64_t>("oem.CapacityBytes");
    used_bytes = storage_pt.get<long uint64_t>("oem.UsedBytes", 1000);
    id = storage_pt.get<string>("Id");
    description = storage_pt.get<string>("Description");
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, storage_pt.get_child("Drives")) {
      assert(value.first.empty());
      boost::property_tree::ptree s_pt = value.second;
      string drive_endpoint;
      for( auto key: s_pt) {
        if( key.first == "@odata.id" ) {
          drive_endpoint = key.second.data();
          if ( add_drive(drive_endpoint) ) {
            is_drive_added = true;
          }
        }
      }
    }
    // Check any good drive added into storage
    if(! is_drive_added) {
      smg_error(logger, "No drive has been added for the subsystem - %s", subsystem_nqn.c_str());
    }
  } catch ( exception& e ) {
    smg_error(logger,"Exception - Unable to read storage space information - %s", e.what());
    return false;
  }
  return is_drive_added;
}

/* Function Name: update_storage
 * Args         : None
 * Return       : bool
 * Description  : Update storage metrics for an subsystem. Includes drive as well.
 */
bool Storage::update_storage()
{
  // URL
  string rest_response;
  string storage_url = host + endpoint;
  RESTful(rest_response, storage_url);
  ptree storage_pt;
  istringstream iss_storage (rest_response);
  read_json(iss_storage, storage_pt);

  if ( !check_redfish_rest_call_status(storage_pt, storage_url) ){
    smg_error(logger, "Failed to read storage information from %s", storage_url.c_str());
    return false;
  }

  // Update storage information
  try {
    percent_available = storage_pt.get<double>("oem.PercentAvailable");
    capacity_bytes = storage_pt.get<long uint64_t>("oem.CapacityBytes");
    used_bytes = storage_pt.get<long uint64_t>("oem.UsedBytes", 1000);	
    // update drive
    for( auto d_iter = drivesMap.begin(); d_iter != drivesMap.end(); d_iter++) {
      Drive* drive = d_iter->second;
      drive->process_drive();
    }
  } catch ( exception& e ) {
    smg_error(logger,"Exception: %s - %s", __func__, e.what());
    return false;
  }
  return true;
}

/* Function Name: add_drive
 * Args         : <string> - drive endpoint
 * Return       : <bool> - 1/0 Success/Failure
 * Description  : Add a drive information to a storage.
 */
bool Storage::add_drive(string drive_endpoint)
{
  bool is_drive_good = false;
  Drive* drive = new Drive(host, drive_endpoint);
  is_drive_good = drive->process_drive();
  string drive_id = drive->get_id();
  if ( is_drive_good ) {
    drivesMap[drive_id] = drive;
  } else {
    delete drive;
    drive = NULL;
  }
  return is_drive_good;
}

/* Function Name: get_storage_info
 * Args         : <ptree&>
 * Return       : None
 * Description  : Get entire storage information for a subsystem.
 */
void Storage::get_storage_info(ptree& storage_pt) const
{
  storage_pt.put("CapacityBytes", capacity_bytes);
  storage_pt.put("PercentAvailable", percent_available);
  storage_pt.put("Id", id);
  storage_pt.put("Description", description);

  ptree drives_pt;
  // read drive data
  for ( auto p_iter = drivesMap.begin(); p_iter != drivesMap.end(); p_iter++ ) {
    Drive* d_ptr = p_iter->second;
    ptree drive_info;
    d_ptr->get_drive_info(drive_info);
    drives_pt.push_back(make_pair("", drive_info));
  }
  storage_pt.add_child("Drives", drives_pt);
}

/* Function Name: process_drive
 * Args         : None
 * Return       : None
 * Description  : Process information of drive.
 */
bool Drive::process_drive()
{
  string rest_response;
  string drive_url = host + endpoint;
  RESTful(rest_response, drive_url);
  ptree drive_pt;
  istringstream iss_drive (rest_response);
  read_json(iss_drive, drive_pt);
  if ( !check_redfish_rest_call_status(drive_pt, drive_url) ) {
    smg_error(logger, "Failed to read drive information from %s", drive_url.c_str());
    return false;
  }

  try {
    // Populate member variable:
    block_size_bytes = drive_pt.get<int64_t>("BlockSizeBytes");
    capacity_bytes = drive_pt.get<int64_t>("CapacityBytes");
    id = drive_pt.get<string>("Id");
    manufacturer = drive_pt.get<string>("Manufacturer");
    media_type = drive_pt.get<string>("MediaType");
    model = drive_pt.get<string>("Model");
    protocol = drive_pt.get<string>("Protocol");
    revision = drive_pt.get<string>("Revision");
    serial_number = drive_pt.get<string>("SerialNumber");
    percent_available = drive_pt.get<double>("oem.PercentAvailable");
    description = drive_pt.get<string>("Description");
    name = drive_pt.get<string>("Name");

    smg_info(logger, "Processed drive information for Serial Number:%s", serial_number.c_str());
  } catch (exception& e) {
    smg_error(logger, "Exception: Failed processing drive - %s", e.what());
    //return false; // May need to think if we need to bail out here or not.
  }
  return true;
}

/* Function Name: get_drive_info
 * Args         : <ptree&> - Output drive_pt parse tree
 * Return       : None
 * Description  : Get entire drive information and return through drive_pt.
 */
void Drive::get_drive_info(ptree& drive_pt)
{
  drive_pt.put("BlockSizeBytes", block_size_bytes);
  drive_pt.put("CapacityBytes", capacity_bytes);
  drive_pt.put("Id", id);
  drive_pt.put("Manufacturer", manufacturer);
  drive_pt.put("MediaType", media_type);
  drive_pt.put("Model", model);
  drive_pt.put("Protocol", protocol);
  drive_pt.put("Revision", revision);
  drive_pt.put("SerialNumber", serial_number);
  drive_pt.put("DiskSpaceAvailable", percent_available);
  drive_pt.put("Name", name);
  drive_pt.put("Description", description);
}

/* Function Name: process_interface
 * Args         : None
 * Return       : None
 * Description  : Process interface information.
 */
bool Interface::process_interface()
{

  string interface_response;
  string interface_endpoint = host + endpoint;
  RESTful(interface_response, interface_endpoint);
  ptree interface_pt;
  istringstream is (interface_response);
  read_json(is, interface_pt);

  if ( !check_redfish_rest_call_status(interface_pt, interface_endpoint) ) {
    smg_error(logger, "Failed to read interface information from %s",interface_endpoint.c_str());
    return false;
  }
  
  interface_speed = interface_pt.get<int32_t>("SpeedMbps", 100000);
  if ( !interface_pt.get<string>("LinkStatus","").empty() ) {
    status = link_status[interface_pt.get<string>("LinkStatus")];
  } else {
    status = 0; 
  }
  return get_interface_address(interface_pt);
}

/* Function Name: get_interface_address
 * Args         : <ptree&> - Output - interface_pt parse tree
 * Return       : <bool> - 1/0 , processed successfuly/ failed.
 * Description  : Process address, family, port and protocol section of interface.
 *                Mandatory parameter for nvme connect.
 */
bool Interface::get_interface_address(ptree& interface_pt)
{
  try {
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interface_pt.get_child("IPv4Addresses")) {
      assert(value.first.empty());
      boost::property_tree::ptree address_pt = value.second;
      if( !(address_pt.get<string>("Address", "")).empty() ) {
        address = address_pt.get<string>("Address");
        transport_type = address_pt.get<string>("oem.SupportedProtocol");
        port = address_pt.get<int32_t>("oem.Port");
        ip_address_family = AF_INET;
      }
    }
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interface_pt.get_child("IPv6Addresses")) {
      assert(value.first.empty());
      boost::property_tree::ptree address_pt = value.second;
      if( !(address_pt.get<string>("Address", "")).empty() ) {
        address = address_pt.get<string>("Address");
        transport_type = address_pt.get<string>("oem.SupportedProtocol");
        port = address_pt.get<int32_t>("oem.Port");
        ip_address_family = AF_INET6;
      }
    }
  } catch ( exception& e ) {
    smg_error(logger, "Exception: %s - %s",__func__, e.what());
    return false;
  }
  return true;
}

/* Function Name: get_interface_info
 * Args         : <ptree&> - Output - interface_pt parse tree
 * Return       : None
 * Description  : Get interface information and store in a boost JSON format
 *                into input interface_pt. 
 */
void Interface::get_interface_info(ptree& interface_pt)
{
  interface_pt.put("Status", status);
  interface_pt.put("Address", address);
  interface_pt.put("Port", port);
  interface_pt.put("AddressFamily", ip_address_family);
  interface_pt.put("TransportType", transport_type);
  interface_pt.put("InterfaceSpeed", interface_speed);
}

//Destructors
UnifiedFabricManager::~UnifiedFabricManager() 
{
  for( auto s_iter=subsystemsMap.begin(); s_iter != subsystemsMap.end(); s_iter++)
  {
    delete(s_iter->second);
  }
  subsystemsMap.clear();
}

Subsystem::~Subsystem() 
{
  // Remove all interface objects
  for( auto i_iter=interfacesMap.begin(); i_iter != interfacesMap.end(); i_iter++)
  {
    delete(i_iter->second);
  }
  interfacesMap.clear();
  // Remove Storage object
  if (storage) {
    delete storage;
  }
}

Storage::~Storage() 
{
  // Remove all Drive objects
  for( auto d_iter=drivesMap.begin(); d_iter != drivesMap.end(); d_iter++)
  {
    delete(d_iter->second);
  }
  drivesMap.clear();
}



/* Function Name: check_redfish_rest_call_status
 * Input Args   : <ptree&> - response from redfish rest call
 *                <std:string> - URL used for REST call
 * Return       : <bool> - Return false if received bad response.
 * Description  : Function determine the REST response valid?
 * Sample Bad Response:
 *{
 *   "Status": 404,
 *   "Message": "Not Found"
 *}
 */
bool check_redfish_rest_call_status(ptree& response, string& uri)
{
  boost::optional< ptree& > is_status_exist = response.get_child_optional("Status");
  boost::optional< ptree& > is_msg_exist = response.get_child_optional("Message");

  if ( is_status_exist && is_msg_exist ) {
    string message = response.get<string>("Message");
    int32_t status = response.get<int>("Status");
    smg_error(logger, "Failed to process URL - %s", uri.c_str());
    smg_error(logger,"Received Bad redfish response, CODE-%d MESSAGE:%s",status, message.c_str());
    return false;
  }
  return true;
}
