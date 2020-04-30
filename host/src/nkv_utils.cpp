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



#include "nkv_utils.h"
#include "unified_fabric_manager.h"

long REST_CALL_TIMEOUT = 10;

int32_t nkv_cmd_exec(const char* cmd, std::string& result) {
  std::array<char, 512> buffer;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    return -1;
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
  }
  return 0;
}

nkv_result nkv_get_path_stat_util (const std::string& p_mount, nkv_path_stat* p_stat) {
  std::string cmd_str = NKV_DEFAULT_STAT_FILE;
  if(const char* env_p = std::getenv("NKV_STAT_SCRIPT")) {
    cmd_str = env_p;
  }
  smg_info(logger, "NKV stat file = %s", cmd_str.c_str());

  cmd_str += " ";
  cmd_str += p_mount;
  smg_info(logger, "NKV stat command = %s", cmd_str.c_str());
  std::string result;
  int32_t rc = nkv_cmd_exec(cmd_str.c_str(), result);
  if (rc == 0 && !result.empty()) {
    smg_info(logger, "NKV stat data = %s", result.c_str());
    boost::property_tree::ptree pt;
    try {
      std::istringstream is (result);
      boost::property_tree::read_json(is, pt);
    }
    catch (std::exception& e) {
      smg_error(logger, "Error reading NKV stat output, cmd = %s, buffer = %s ! Error = %s",
               cmd_str.c_str(), result.c_str(), e.what());
      return NKV_ERR_INTERNAL;
    }
    p_mount.copy(p_stat->path_mount_point, p_mount.length());
    try {
      BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt) {
        assert(v.first.empty());
        boost::property_tree::ptree pr = v.second;
        std::string d_cap_str = p_mount + ".DiskCapacityInBytes";
        std::string d_usage_str = p_mount + ".DiskUtilizationInBytes";
        std::string d_usage_percent_str = p_mount + ".DiskUtilizationPercentage";
        p_stat->path_storage_capacity_in_bytes = pr.get<uint64_t>(d_cap_str, 0);
        p_stat->path_storage_usage_in_bytes = pr.get<uint64_t>(d_usage_str, 0);
        p_stat->path_storage_util_percentage = pr.get<double>(d_usage_percent_str, 0);
      }
    }
    catch (std::exception& e) {
      smg_error(logger, "Error reading NKV stat output, cmd = %s, buffer = %s ! Error = %s",
               cmd_str.c_str(), result.c_str(), e.what());
      return NKV_ERR_INTERNAL;
    }
  } else {
    smg_error(logger, "NKV stat script execution failed, cmd = %s !!", cmd_str.c_str());
    return NKV_ERR_INTERNAL;
  }
  return NKV_SUCCESS;
}

/* Function Name: nkv_get_remote_path_stat
 * Input Agrs   : <const FabricManager*> = Reference to FM created during initialization of NKV
 *                <const std::string&> = nqn of subsystem
 *                <nkv_path_stat*> = NKV path stat structure
 * Return       : <nkv_result> = a enum values of nkv_result NKV_SUCCESS/NKV_FM_ERR 
 * Description  : Retrive the following subsystem stats from FabricManager.
 *                Available Space in a Subsystem in %.
 *                Total storage capacity in bytes.
 *                Total used bytes. 
 */
nkv_result nkv_get_remote_path_stat(const FabricManager* fm, const string& subsystem_nqn, nkv_path_stat* stat)
{
  Subsystem* subsystem = static_cast<Subsystem*>(fm->get_subsystem(subsystem_nqn));
  if ( subsystem ) {
    Storage* storage =const_cast<Storage*>(subsystem->get_storage());
    if ( storage ) {
      stat->path_storage_capacity_in_bytes = storage->get_capacity_bytes();
      stat->path_storage_usage_in_bytes = storage->get_used_bytes();
      stat->path_storage_util_percentage = storage->get_available_space();
      smg_info(logger, "Stats for Subsystem NQN - %s", subsystem_nqn.c_str());
    } else {
      smg_error(logger, "Subsystems storage information for nqn=%s is not available", subsystem_nqn.c_str());
      return NKV_ERR_FM;
    }
  } else {
    smg_error(logger, "Subsystem for nqn %s not found", subsystem_nqn.c_str());
    return NKV_ERR_FM;
  }
  return NKV_SUCCESS;
}

std::string nkv_transport_mapping[TRANSPORT_PROTOCOL_SIZE] = {"tcp", "rdma"};

/* Function Name: get_nkv_transport_type
 * Input Agrs   : <int32_t> = transport value 0/1 => tcp/rdma
 *                <std::string&> = transport type such as tcp,rdma etc.
 * Return       : <bool> on success respective value is updated on transport var.
 *                On failure a false is returned.
 * Description  : Convert transport value to a corresponding string value.
 */
bool get_nkv_transport_type(int32_t transport, std::string& transport_type)
{
  if ( transport >= TRANSPORT_PROTOCOL_SIZE ) {
    smg_error(logger, "Wrong transport protocol=%d, Supported protocol tcp=0,rdma=1", transport);
    return false;
  }
  transport_type = nkv_transport_mapping[transport];
  return true;
}

/* Function Name: get_nkv_transport_value
 * Input Agrs   : <std::string&> = transport type such as tcp,rdma etc.
 * Return       : <int32_t> on success respective value for transport type.
 *                On failure -1.
 * Description  : Convert transport type to a corresponding integer value.
 */
int32_t get_nkv_transport_value(std::string transport_type)
{
  for( int32_t index = 0; index < TRANSPORT_PROTOCOL_SIZE; index++) {
    boost::to_lower(transport_type);
    if ( nkv_transport_mapping[index] == transport_type ) {
      return index;
    }
  }
  return -1;
}


// REST Interface ...
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



