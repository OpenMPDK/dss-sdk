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

#include "cluster_map.h"


long REST_CALL_TIMEOUT = 10;

// LinkStatus based on REdfish API spec.
std::unordered_map<std::string, uint32_t> link_status = { {"LinkUp", 1 },
                                                          {"LinkDown", 0},
                                                          {"NoLink",-1} };

/*
Description: Get cluster map information from REST api.
Maximum size of data:
  - Maximum data to be transferred is 16K
  - Max header data 100k
*/

namespace{
  /* ptr   = <points to the delivered data>
  size  = 1 , always 1
  nmemb = <number of byte received>
  userdata = should set with CURLOPT_WRITEDATA
  */
  std::size_t write_callback(char *ptr, size_t size, size_t nmemb, std::string *userdata)
  {
    const std::size_t totalBytes(size * nmemb);
    userdata->append(ptr, totalBytes);
    return totalBytes;
  }
}



/*
Function Name: get_rest_url
Description  : Return the rest URL used to read/write data
Return       : <const std:: string&> , rest URL
*/
const std::string ClusterMap::get_rest_url() const
{
  return host + endpoint;
}

/*
Function Name: get_response
Description  : Get the complete JSON response from rest api
Params       : <std::string>  Rest JSON response to be stored into a string.
Return       : <bool> , 0/1 -> Success/Failure
*/
bool ClusterMap::process_clustermap()
{
  bool is_good = false;
  if( redfish_compliant ) {
    is_good = redfish_api(cluster_map_json, host, endpoint);
    if (! is_good ) {
      smg_error(logger,"ClusterMap: %s Redfish API processing failed from \"%s%s\"",
                         __func__, host.c_str(), endpoint.c_str());
    }
  } else {
    std::string response;
    std::string cluster_map_url(host+endpoint);
    is_good = RESTful(response, cluster_map_url);
    if (is_good) {
      cluster_map = response;
      smg_info(logger, "ClusterMap: Received a successful response from Fabric Manager...");
    }
  }
  return is_good;
}

/*
Function Name: get_clustermap
Description  : Get Only Subsystems Maps from JSON response.
Params       : <boost::property_tree::ptree>  nkv config datastructure
Return       : <bool> 0/1 -> Success /Failure
*/
bool ClusterMap::get_clustermap(ptree&  nkv_config)
{
  try
  {
    ptree clustermap_pt;
    if (! redfish_compliant ) {
      std::istringstream is (cluster_map);
      read_json(is, clustermap_pt);
    } else {
      clustermap_pt = cluster_map_json;
    }
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, clustermap_pt)
    {
      boost::optional< ptree& > is_node_exist = nkv_config.get_child_optional(v.first.data());
      if(is_node_exist){
        nkv_config.erase(v.first.data());
      }
      nkv_config.add_child(v.first.data(), v.second);
    }
    smg_info(logger, "Updated Subsystem and Cluster information to the NKV configuration ...");
    //boost::property_tree::write_json(std::cout, nkv_config); 
  }
  catch(std::exception const& e) {
    smg_error(logger, "ClusterMap: %u", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/*
Function Name: RESTful
Description  : Get the JSON response from a RESTful call.
Params       : <std::string>  Rest JSON response to be stored into a string.
Return       : <bool> , 0/1 -> Success/Failure
*/
bool RESTful(std::string& response, std::string& URL)
{
  bool good_response = false;
  smg_info(logger, "ClusterMap: Calling REST API %s", URL.c_str());
  // Initialize a curl session
  CURL* curl = curl_easy_init();
    
  if (curl) {
    CURLcode res;
    // Setup remote URL
    curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

    // Resolve host name using IPv4-names only.
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    // Timeout connection after TIMEOUT seconds
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, REST_CALL_TIMEOUT);

    // Response code
    long httpCode(0);
    std::unique_ptr<std::string> http_data(new std::string());

    // By default libcurl dumps the output to the default file handler stdout and error to stderr
    // Use a write_callback function to write to a buffer, CURLOPT_WRITEFUNCTION should be used with CURLOPT_WRITEDATA
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    // http_data gets passed to the write_callback as 4th arguments which is userdata.
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, http_data.get());

    // Provide a buffer to store error
    char error_buffer[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer );

    // Follow HTTP redirect if necessary
    //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L)
    // Perform transfer as specified in the options. Its a blocking function.
    res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
      // Get return code
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

      // Check the response code
      if (httpCode == HTTP_SUCCESS) {
        response = *http_data.get();
        good_response = true;
      } else {
        smg_error(logger, "ClusterMap: failed to connect, response code %d", httpCode);
        std::string error_message(*http_data.get());
        smg_error(logger, "ClusterMap: %s", error_message.c_str());
      }
    } else {
      // Error handling
      smg_error(logger, "ClusterMap: %s", curl_easy_strerror(res));
    }
    // Clean up session
    curl_easy_cleanup(curl);
  } else {
    smg_error(logger, "ClusterMap: Couldn't initialize curl session ...");
  }
  return good_response;
}

/* Function Name: get_subsystem_storage
 * Input Args   : <std::string&> = redfish endpoint for storage
 * Return       : <double> = Return space availabel to the subsystem
 * Description  : Get the available space for a subsystem.
 */

double get_subsystem_storage(std::string& url)
{
  std::string rest_response;
  std::string storage_url = url + "/Storage";
  if ( ! RESTful(rest_response, storage_url) ){
    smg_error(logger, "Failed to read storage information from %s", url.c_str());
  }
  ptree pt;
  std::istringstream iss_subsystem (rest_response);
  read_json(iss_subsystem, pt);
  double space_available = 0.0;

  try{
    space_available = pt.get<double>("oem.PercentAvailable");
  } catch ( exception& e ) {
    smg_error(logger,"Redfish API - Unable to read storage space information ...");
  }
  return space_available;
}

/* Function Name: add_subsystem
 * Input Args   : <ptree&> = output subsystem information in boost json format.
 *                <const std::string&> host ip address with port
 *                <std::string> subsystem endpoint 
 * Description  : Update NKV configuration subsystem information in json format. 
 */
bool add_subsystem(ptree& subsystem, const std::string& host, std::string& url)
{
  // Rest call
  std::string rest_response;
  std::string subsystem_url = host + url;
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
      subsystem.put("subsystem_nqn", identifiers.get<std::string>("DurableName"));
    }
  }
  //subsystem_avail_percent, need to update
  subsystem.put("subsystem_avail_percent", get_subsystem_storage(subsystem_url) );

  //subsytem_nqn
  std::vector<std::string> uuids; // {target_uuid}.{subsystem_uuid}
  std::string subsystem_uuid  = pt.get<std::string>("Id", "default.default");
  boost::split(uuids, subsystem_uuid , boost::is_any_of("."));
  subsystem.put("subsystem_nqn_id", uuids[1]);
  if ( pt.get<std::string>("Status.State", "" ) == "Enabled" && pt.get<std::string>("Status.Health", "") == "OK" ) {
    subsystem.put("subsystem_status", 0 );
  } else {
    subsystem.put("subsystem_status", 1 );
  }
  subsystem.put("subsystem_nqn_nsid", pt.get<int32_t>("oem.NSID", 1));
  subsystem.put("target_server_name", pt.get<std::string>("oem.ServerName", "default_server"));
  // Add transporters
  std::string interfaces_url;
  for( auto itr: pt ) {
    if( itr.first == "@odata.id" ) {
      interfaces_url = itr.second.data() + "/EthernetInterfaces" ;
    }
  }
  // RESTful call to get all the transporters information from a subsystem.
  ptree transporters;
  std::string interfaces;
  interfaces_url = host + interfaces_url;
  // Rest call to the interface
  RESTful(interfaces, interfaces_url);
  ptree interfaces_pt;
  std::istringstream is2 (interfaces);
  read_json(is2, interfaces_pt);

  bool is_transporter_added =  false;
  BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interfaces_pt.get_child("Members")) {
    assert(value.first.empty());
    boost::property_tree::ptree nic_pt = value.second;
    std::string interface_url;
    for( auto key: nic_pt) {
      if( key.first == "@odata.id" ) {
        interface_url = key.second.data();
      }
    }
    ptree transporter;
    if ( add_transporter(transporter, host, interface_url) ) {
      transporter.put("subsystem_interface_numa_aligned", pt.get<bool>("oem.NumaAligned"));
      transporters.push_back(std::make_pair("", transporter));
      is_transporter_added = true;
    }
  }
  subsystem.add_child("subsystem_transport", transporters);
  return is_transporter_added;
}

/* Function Name: get_transporter_address
 * Input Args   : <ptree&> Output transporter information in json format.
 *                <ptree&> Interface information in json format retrieve from
 *                 redfish api.
 * Return       : <bool> = 1/0 Success/failure
 * Description  : Process interface address, port, and subsystem type.
 */

bool get_transporter_address(ptree& transporter, ptree& interface_pt)
{
   std::string address;
   BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interface_pt.get_child("IPv4Addresses")) {
      assert(value.first.empty());
      boost::property_tree::ptree address_pt = value.second;
      if( !(address_pt.get<std::string>("Address", "")).empty() ) {
        transporter.put("subsystem_address",address_pt.get<std::string>("Address") );
        //subsystem_type
        transporter.put("subsystem_type", get_nkv_transport_value(address_pt.get<std::string>("oem.SupportedProtocol")));
        transporter.put("subsystem_port", address_pt.get<int32_t>("oem.Port"));
        return true;
      }
    }
   BOOST_FOREACH(boost::property_tree::ptree::value_type &value, interface_pt.get_child("IPv6Addresses")) {
      assert(value.first.empty());
      boost::property_tree::ptree address_pt = value.second;
      if( !(address_pt.get<std::string>("Address", "")).empty() ) {
        transporter.put("subsystem_address",address_pt.get<std::string>("Address") );
        transporter.put("subsystem_type", get_nkv_transport_value(address_pt.get<std::string>("oem.SupportedProtocol")));
        transporter.put("subsystem_port", address_pt.get<int32_t>("oem.Port"));
        return true;
      }
    }

  return false;
}

/* Function Name: add_transporter
 * Input Args   : <ptree&> = Output transporter infromation in JSON format.
 *                <const std::string&> = host ip address
 *                <std::string&> Interface endpoint
 * Return       : <bool> = 1/0, Success/Failure
 * Description  : Get transporter information from redfish api and update the same
 *                into transporter input object.
 */
bool add_transporter(ptree& transporter, const std::string& host, std::string& interface_url)
{
  std::string interface;
  interface_url = host + interface_url;
  if (! RESTful(interface, interface_url)) {
    smg_error(logger, "Failed to read interface information from %s", interface_url.c_str());
    return false;
  }
  try {
    ptree interface_pt;
    std::istringstream is (interface);
    read_json(is, interface_pt);
    //subsystem_interface_speed
    transporter.put("subsystem_interface_speed", interface_pt.get<int32_t>("SpeedMbps"));
    //subsystem_addr_fam
    transporter.put("subsystem_addr_fam", AF_INET); // WORK determine address family
    get_transporter_address(transporter, interface_pt);
    //subsystem_interface_numa_aligned
    transporter.put("subsystem_interface_status", link_status[interface_pt.get<std::string>("LinkStatus")]);
    } catch ( exception& e ) {
      smg_error(logger, "Exception:Redfish API -%s- %s", __func__, e.what());
      return false;
    }
  return true;
}

/* Function Name: is_subsystem
 * Input Args   : <std::string&> subsystem endpoint
 * Return       : <bool> = 1/0, Success/Failure
 * Description  : Check the input member endpoint is subsystem or target?
 */
bool is_subsystem(std::string& url)
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

/* Function Name: redfish_api
 * Input Args   : <ptree&> = Output clustermap in json format
 *                <const std::string&> = host ip address with port
 *                <const std::string&> = root endpoint for redfish api
 *                   <ip address>:5000/redfish/v1/Systems
 * Return       : <bool> = 1/0, Success/Failure
 * Description  : Process redfish api from UFM and return clustermap in json format.                  
 */

bool redfish_api(ptree& cluster_map, const std::string& host, const std::string& endpoint)
{
  std::string systems;
  std::string uri = host+endpoint;
  RESTful(systems, uri);

  bool is_subsystem_added = false;
  ptree root_pt;
  try{
    std::istringstream systems_is (systems);
    read_json(systems_is, root_pt);
  
    ptree subsystems;
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

      ptree subsystem;
      if ( add_subsystem(subsystem, host, subsystem_url) ) {
        subsystems.push_back(std::make_pair("",subsystem));
        is_subsystem_added = true;
      }
    }
    cluster_map.add_child("subsystem_maps", subsystems);
  }
  catch( exception& e )
  {
    smg_error(logger,"Exception:Redfish API -%s- %s", __func__, e.what());
    return false;
  }
  
  // Add warning message
  if (! is_subsystem_added ) {
    smg_error(logger, "Redfish API - no subsytem has been found ... ");
  }
  return is_subsystem_added;
}
