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

#ifndef NKV_NFM_H
#define NKV_NFM_H

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
#include "fabric_manager.h"


using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;
using namespace std;

extern c_smglogger* logger;

class FabricManagerNode;

//TODO to be used later
#define CLUSTERMAP_API "/api/v1/cm/getclustermap"
#define CLUSTER_API "/api/v1/cluster"
#define HEALTH_API "/api/v1/health"

/* The NativeFabricManager is developed based on RESTful API. It provides Targte
 * ClusterMap information along with FM node details. The target clustermap
 * contains all Subsystems information. FM expose clustermap details through
 * "/api/v1/cm/getclustermap" api.
 */


class NativeFabricManager:public FabricManager
{
  string cluster_map; // rest json response in string
  uint32_t node_count; // Number of node in FM cluster 
  unordered_map<string,FabricManagerNode*> fm_nodesMap;

  const string fm_name = "NativeFabricManager";

  public:
  NativeFabricManager(const string& _host,
                      const string& _endpoint):FabricManager(_host,_endpoint)
  {
    node_count = 0;
  }
  ~NativeFabricManager();
  bool process_clustermap();
  bool add_fm_node(ptree& fm_nodes_pt);
  const std::string get_rest_url() const;

  //TODO
  // FM Cluster Information:(GET): http://10.1.51.207:8080/api/v1/cluster 
  // const std::string& get_cluster_status();
  // Cluster Health:(GET): http://10.1.51.207:8080/api/v1/health
  // Cluster Downgrade:(POST): http://127.0.0.1:8080/api/v1/ops/clusterdowngrade
  // Cluster Downgrade Progress:(POST): http://127.0.0.1:8080/api/v1/ops/clusterdowngradeprogress
  // Cluster Downgrade:(POST): http://127.0.0.1:8080/api/v1/ops/clusterupgrade
  // Target Config:(POST)
  // Target Config:(GET)
  //
};

// Node contains details of FM node based on FM API.
class FabricManagerNode
{
  const string fm_id;
  const string fm_server_address;
  uint32_t fm_instance_mode = 0;
  uint32_t fm_instance_health = 0;
  const string fm_server_name;  
  public:
    FabricManagerNode() {}
    FabricManagerNode(const string& _id,
                      const string _address,
                      uint32_t _mode,
                      uint32_t _health,
                      const string& _name):
                      fm_id(_id),
                      fm_server_address(_address),
                      fm_instance_mode(_mode),
                      fm_instance_health(_health),
                      fm_server_name(_name)
    {
    }
    
    ~FabricManagerNode() {}
};

#endif






