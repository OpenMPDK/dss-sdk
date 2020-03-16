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

#ifndef NKV_CLUSTER_MAP_H
#define NKV_CLUSTER_MAP_H

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
#include "csmglogger.h"
#include "nkv_utils.h"

#define HTTP_SUCCESS 200

using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;
using namespace std;

extern c_smglogger* logger;
extern long REST_CALL_TIMEOUT;

class ClusterMap
{
  const std::string host;
  const std::string endpoint; // root endpoint 
  bool redfish_compliant;
  std::string cluster_map; // rest json response in string
  ptree cluster_map_json; // ClusterMap in JSON format.
  public:
  ClusterMap(){};
  ClusterMap(std::string host, std::string endpoint, bool redfish_compliant ):
             host(host),endpoint(endpoint), redfish_compliant(redfish_compliant) {}
  ~ClusterMap(){};
  bool process_clustermap();
  bool get_clustermap(ptree& clustermap);
  const std::string get_rest_url() const;

  //const std::string& get_cluster_status();


};

bool RESTful(std::string& response, std::string& URL);

// Functions related to REDfish api
bool redfish_api(ptree& cluster_map_json, const std::string& host, const std::string& endpoint);
bool add_transporter(ptree& transporter, const std::string& host, std::string& interface_url);
bool add_subsystem(ptree& subsystem, const std::string& host, std::string& url);
bool is_subsystem(std::string& url);
double get_subsystem_storage(std::string& url);
bool get_transporter_address(ptree& transporter, ptree& interface_pt);



#endif






