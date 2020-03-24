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
#include "redfish.h"

long REST_CALL_TIMEOUT = 10;

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
    //is_good = redfish_api(cluster_map_json, host, endpoint);
    Redfish* redfish = new Redfish(host, endpoint);
    redfish->process_redfish_api();
    is_good = redfish->get_clustermap(cluster_map_json);

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
