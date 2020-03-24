#include "redfish.h"
#include "cluster_map.h"
//#include "nkv_utils.h"

// LinkStatus based on REdfish API spec.
std::unordered_map<std::string, uint32_t> link_status = { {"LinkUp", 1 },
                                                          {"LinkDown", 0},
                                                          {"NoLink",-1} };

/* Function Name: process_redfish_api()
 * Args         : None
 * Return       : None
 * Description  : Process the redfish api from root endpoint "/redfish/v1/Systems"
 *                and generate complete object model of target cluster.
 */
void Redfish::process_redfish_api()
{
  std::string systems;
  std::string uri = host+endpoint;
  RESTful(systems, uri);

  bool is_subsystem_added = false;
  ptree root_pt;
  try{
    std::istringstream systems_is (systems);
    read_json(systems_is, root_pt);
  
    // Process Root
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, root_pt.get_child("Members")) {
      assert(value.first.empty());
      boost::property_tree::ptree subsystem_pt = value.second;
      std::string subsystem_url;
      for(auto it: subsystem_pt ){
        subsystem_url = it.second.data();
      }
      // Determine if the URL is subsystem 
      if (! is_subsystem(subsystem_url)) {
        continue;
      }
      // Add subsystem
      Subsystem* subsystem = new Subsystem(host, subsystem_url);
      subsystem->process_subsystem();
      std::string subsystem_nqn = subsystem->get_nqn();
      subsystemsMap[subsystem_nqn] = subsystem;
      is_subsystem_added = true;

      // Just for DEBUG
      //ptree subsystem_info_pt;
      //subsystem->get_subsystem_info(subsystem_info_pt);
      //boost::property_tree::write_json(std::cout, subsystem_info_pt);
    }
  }
  catch( exception& e )
  {
    smg_error(logger,"Exception:-%s- %s", __func__, e.what());
  }
  // Add warning message
  if (! is_subsystem_added ) {
    smg_error(logger, "Redfish API - no subsytem has been found ... ");
  }
}

/* Function Name: is_subsystem
 * Input Args   : <std::string&> subsystem endpoint
 * Return       : <bool> = 1/0, Success/Failure
 * Description  : Check the input member endpoint is subsystem or target?
 */
bool Redfish::is_subsystem(std::string& url)
{
  std::vector<std::string> result_url;
  std::vector<std::string> result_sub;
  boost::split(result_url, url, boost::is_any_of("/"));
  boost::split(result_sub, result_url[result_url.size() -1], boost::is_any_of("."));

  if ( result_sub.size() == 2 ) {
    return true;
  }
  return false;
}

/* Function Name: get_clustermap
 * Args         : <ptree&> - Output - cluster_map parse tree
 * Return       : <bool> - 1/0 , Success/Failure
 * Description  : Generate the cluster map based on NKV host requirement. 
 */
bool Redfish::get_clustermap(ptree& cluster_map)
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
    subsystem.put("target_server_name", s_pt.get<std::string>("ServerName"));
    subsystem.put("subsystem_nqn_nsid", s_pt.get<int32_t>("NSID"));
    subsystem.put("subsystem_nqn", s_pt.get<std::string>("NQN"));
    subsystem.put("subsystem_nqn_id",s_pt.get<std::string>("Id"));
    subsystem.put("subsystem_avail_percent",s_pt.get<double>("PercentAvailable"));

    ptree interfaces;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, s_pt.get_child("Interfaces")) {
      assert(value.first.empty());
      boost::property_tree::ptree interface_pt = value.second;
      ptree interface;
      interface.put("subsystem_interface_numa_aligned", s_ptr->is_subsystem_aligned());
      interface.put("subsystem_interface_speed",interface_pt.get<uint32_t>("InterfaceSpeed"));
      interface.put("subsystem_type",get_nkv_transport_value(interface_pt.get<std::string>("TransportType")));
      interface.put("subsystem_port",interface_pt.get<uint32_t>("Port"));
      interface.put("subsystem_address",interface_pt.get<std::string>("Address"));
      interface.put("subsystem_interface_status",interface_pt.get<uint32_t>("Status"));
      interface.put("subsystem_addr_fam",interface_pt.get<uint32_t>("AddressFamily"));

      interfaces.push_back(std::make_pair("", interface));
    }
    subsystem.add_child("subsystem_transport", interfaces);
    subsystems_pt.push_back(std::make_pair("", subsystem));
  }
  cluster_map.add_child("subsystem_maps", subsystems_pt);
  
  return true;
}

/* Function Name: process_subsystem
 * Args         : None
 * Return       : None
 * Description  : Process subsystem along with interfaces and storage.
 */
void Subsystem::process_subsystem()
{
  // Rest call
  std::string rest_response;
  std::string subsystem_url = host + endpoint;
  if ( ! RESTful(rest_response, subsystem_url) ){
    smg_error(logger, "Failed to read subsystem information from %s", subsystem_url.c_str());
  }
  ptree pt;
  std::istringstream iss_subsystem (rest_response);
  read_json(iss_subsystem, pt);
  
  BOOST_FOREACH(boost::property_tree::ptree::value_type &value, pt.get_child("Identifiers")) {
    assert(value.first.empty());
    boost::property_tree::ptree identifiers = value.second;
    if ( identifiers.get<std::string>("DurableNameFormat") == "NQN" ) {
      subsystem_nqn = identifiers.get<std::string>("DurableName"); 
    }
  }
  //subsytem_nqn
  std::vector<std::string> uuids; // {target_uuid}.{subsystem_uuid}
  std::string subsystem_uuid  = pt.get<std::string>("Id", "default.default");
  boost::split(uuids, subsystem_uuid , boost::is_any_of("."));
  subsystem_nqn_id = uuids[1];

  if ( pt.get<std::string>("Status.State", "" ) == "Enabled" && pt.get<std::string>("Status.Health", "") == "OK" ) {
    subsystem_status =  0 ;
  } else {
    subsystem_status = 1 ;
  }
  subsystem_numa_aligned = pt.get<bool>("oem.NumaAligned");
  subsystem_nqn_nsid = pt.get<int32_t>("oem.NSID");
  target_server_name = pt.get<std::string>("oem.ServerName");
  name = pt.get<std::string>("Name");
  description = pt.get<std::string>("Description");

  // Add transporters
  std::string interfaces_url;
  for( auto itr: pt ) {
    if( itr.first == "@odata.id" ) {
      interfaces_url = itr.second.data() + "/EthernetInterfaces" ;
    }
  }
  // RESTful call to get all the transporters information from a subsystem.
  std::string interfaces;
  interfaces_url = host + interfaces_url;
  // Rest call to the interface
  RESTful(interfaces, interfaces_url);
  ptree interfaces_pt;
  std::istringstream is2 (interfaces);
  read_json(is2, interfaces_pt);

  //bool is_transporter_added =  false;
  BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interfaces_pt.get_child("Members")) {
    assert(value.first.empty());
    boost::property_tree::ptree nic_pt = value.second;
    std::string interface_url;
    for( auto key: nic_pt) {
      if( key.first == "@odata.id" ) {
        interface_url = key.second.data();
      }
    }
    add_interface(interface_url);
  }

  // Add storage
  add_storage();
}

/* Function Name: add_interface
 * Input Args   : <std::string> - interface_endpoint
 * Return       : <bool> 1/0, Success/Failure 
 * Description  : Process an interface for a subsystem.
 */
bool Subsystem::add_interface(std::string interface_endpoint)
{
  // Create Interface object
  try
  {
  Interface* interface = new Interface(host, interface_endpoint);
  interface->process_interface();
  std::string address = interface->get_address();
  uint64_t inf_path_hash = std::hash<std::string>{}(address);
  interfacesMap[inf_path_hash] = interface;
  } catch(exception& e) {
    smg_error(logger, "Exception:%s - %s", __func__, e.what());
  }
}

/* Function Name: add_storage
 * Args         : None
 * Return       : <bool> 1/0, Success/Failure 
 * Description  : Add the storage information to a subsystem.
 */
bool Subsystem::add_storage()
{
  // Get storage endpoint ...
  std::string rest_response;
  std::string storage_url = host + endpoint + "/Storage";
  if ( ! RESTful(rest_response, storage_url) ){
    smg_error(logger, "Failed to read storage information from %s", storage_url.c_str());
    return false;
  }
  ptree storage_pt;
  std::istringstream is (rest_response);
  read_json(is, storage_pt);
  std::string storage_endpoint;

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
  if( ! storage_endpoint.empty() ) {
    storage = new Storage(host, storage_endpoint, subsystem_nqn);
    storage->process_storage();
    subsystem_avail_percent = storage->get_available_space();
  } else {
    smg_error(logger, "%s:Storage endpoint is not found for - %s", __func__,subsystem_nqn.c_str());
  }
  return true;
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
    interfaces_pt.push_back(std::make_pair("", interface_info));
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
void Storage::process_storage()
{
  smg_debug(logger, "Processing storage information for Subsystem - ", (get_subsystem_nqn()).c_str());
  std::string rest_response;
  std::string storage_url = host + endpoint;
  if ( ! RESTful(rest_response, storage_url) ){
    smg_error(logger, "Failed to read storage information from %s", storage_url.c_str());
  }
  ptree storage_pt;
  std::istringstream iss_storage (rest_response);
  read_json(iss_storage, storage_pt);

  try{
    percent_available = storage_pt.get<double>("oem.PercentAvailable");
    capacity_bytes = storage_pt.get<long uint64_t>("oem.CapacityBytes");
    id = storage_pt.get<std::string>("Id");
    description = storage_pt.get<std::string>("Description");
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, storage_pt.get_child("Drives")) {
      assert(value.first.empty());
      boost::property_tree::ptree s_pt = value.second;
      std::string drive_endpoint;
      for( auto key: s_pt) {
        if( key.first == "@odata.id" ) {
          drive_endpoint = key.second.data();
          add_drive(drive_endpoint);
        }
      }
    }
  } catch ( exception& e ) {
    smg_error(logger,"Redfish API - Unable to read storage space information ...");
  }
}

/* Function Name: add_drive
 * Args         : <std::string> - drive endpoint
 * Return       : <bool> - 1/0 Success/Failure
 * Description  : Add a drive information to a storage.
 */
bool Storage::add_drive(std::string drive_endpoint)
{
  try
  {
  Drive* drive = new Drive(host, drive_endpoint);
  drive->process_drive();
  std::string drive_id = drive->get_id();
  uint64_t inf_path_hash = std::hash<std::string>{}(drive_id);
  drivesMap[inf_path_hash] = drive;
  } catch(exception& e) {
    smg_error(logger, "Exception:%s - %s", __func__, e.what());
    return false;
  }
  return true;
}

/* Function Name: get_storage_info
 * Args         : <ptree&>
 * Return       : None
 * Description  : Get entire storage information for a subsystem.
 */
void Storage::get_storage_info(ptree& storage_pt)
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
    drives_pt.push_back(std::make_pair("", drive_info));
  }
  storage_pt.add_child("Drives", drives_pt);
}

/* Function Name: process_drive
 * Args         : None
 * Return       : None
 * Description  : Process information of drive.
 */
void Drive::process_drive()
{
  std::string rest_response;
  std::string drive_url = host + endpoint;
  if ( ! RESTful(rest_response, drive_url) ){
    smg_error(logger, "Failed to read storage information from %s", drive_url.c_str());
  }
  ptree drive_pt;
  std::istringstream iss_drive (rest_response);
  read_json(iss_drive, drive_pt);

  // Populate member variable:
  block_size_bytes = drive_pt.get<int64_t>("BlockSizeBytes");
  capacity_bytes = drive_pt.get<int64_t>("CapacityBytes");
  id = drive_pt.get<std::string>("Id");
  manufacturer = drive_pt.get<std::string>("Manufacturer");
  media_type = drive_pt.get<std::string>("MediaType");
  model = drive_pt.get<std::string>("Model");
  protocol = drive_pt.get<std::string>("Protocol");
  revision = drive_pt.get<std::string>("Revision");
  serial_number = drive_pt.get<std::string>("SerialNumber");
  percent_available = drive_pt.get<double>("oem.PercentAvailable");
  description = drive_pt.get<std::string>("Description");
  name = drive_pt.get<std::string>("Name");

  smg_info(logger, "Processed drive information for Serial Number:", serial_number.c_str());
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
void Interface::process_interface()
{

  std::string interface_response;
  std::string interface_endpoint = host + endpoint;
  if (! RESTful(interface_response, interface_endpoint)) {
    smg_error(logger, "Failed to read interface information from %s",interface_endpoint.c_str());
  }
  try {
    ptree interface_pt;
    std::istringstream is (interface_response);
    read_json(is, interface_pt);
    interface_speed = interface_pt.get<int32_t>("SpeedMbps");
    status = link_status[interface_pt.get<std::string>("LinkStatus")];

    get_interface_address(interface_pt);
  } catch ( exception& e ) {
    smg_error(logger, "EXCEPTION: % - %s",__func__, e.what());
  }
}

/* Function Name: get_interface_address
 * Args         : <ptree&> - Output - interface_pt parse tree
 * Return       : <bool> - Able to process interface address information or not
 * Description  : Process address, family, port and protocol section of interface.
 */
bool Interface::get_interface_address(ptree& interface_pt)
{
   try{
     BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interface_pt.get_child("IPv4Addresses")) {
       assert(value.first.empty());
       boost::property_tree::ptree address_pt = value.second;
       if( !(address_pt.get<std::string>("Address", "")).empty() ) {
         address = address_pt.get<std::string>("Address");
         transport_type = address_pt.get<std::string>("oem.SupportedProtocol");
         port = address_pt.get<int32_t>("oem.Port");
         ip_address_family = AF_INET;
         return true;
       }
     }
     BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interface_pt.get_child("IPv6Addresses")) {
       assert(value.first.empty());
       boost::property_tree::ptree address_pt = value.second;
       if( !(address_pt.get<std::string>("Address", "")).empty() ) {
         address = address_pt.get<std::string>("Address");
         transport_type = address_pt.get<std::string>("oem.SupportedProtocol");
         port = address_pt.get<int32_t>("oem.Port");
         ip_address_family = AF_INET6;
         return true;
       }
     }
   } catch ( exception& e ) {
     smg_error(logger, "EXCEPTION: % - %s",__func__, e.what());
   }
  return false;
}

/* Function Name: get_interface_info
 * Args         : <ptree&> - Output - interface_pt parse tree
 * Return       : None
 * Description  : Get interface information and store in a boost JSON format
 *                into input interface_pt. 
 */
void Interface::get_interface_info(ptree& interface_pt)
{
  try
  {
    interface_pt.put("Status", status);
    interface_pt.put("Address", address);
    interface_pt.put("Port", port);
    interface_pt.put("AddressFamily", ip_address_family);
    interface_pt.put("TransportType", transport_type);
    interface_pt.put("InterfaceSpeed", interface_speed);
  } catch ( exception& e ) {
    smg_error(logger, "EXCEPTION: % - %s",__func__, e.what());
  }
}

