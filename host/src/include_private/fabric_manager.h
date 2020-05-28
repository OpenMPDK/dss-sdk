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

#ifndef NKV_FM_H
#define NKV_FM_H

#include<iostream>
#include<memory>
#include<string>
#include<cstdint>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include "csmglogger.h"
#include "nkv_utils.h"

using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;
using namespace std;

class FabricManager;
extern c_smglogger* logger;
extern FabricManager* fm;

class FabricManager
{
  const string host;
  const string endpoint; // May be we don't need in future. 

  //TODO
  //const string name;  // FM Name
  //const string version; // FM Version
  vector<string> subsystem_nqn_list;

  public:
  //FabricManager(){};
  FabricManager(const string& _host, const string& _endpoint):
                               host(_host),endpoint(_endpoint)
  {
  }
  virtual ~FabricManager(){};

  ptree target_cluster_map; // Target Cluster Map in JSON format.
  const string& get_host() { return host; }
  const string& get_endpoint() { return endpoint; }
  void update_subsystem_nqn_list(const string& nqn)
  {
    string subsystem_nqn(nqn);
    subsystem_nqn_list.push_back(subsystem_nqn); 
  }
  const vector<string>& get_subsystem_nqn_list() const { return subsystem_nqn_list; }

  virtual bool process_clustermap() = 0;
  virtual void* get_subsystem(const string& subsystem_nqn) const { return NULL;}
  const string get_rest_url() { return host+endpoint; }

  bool get_clustermap(ptree& dss_config) {
    try { 
      BOOST_FOREACH(boost::property_tree::ptree::value_type &v, target_cluster_map) {
        boost::optional< ptree& > is_node_exist = dss_config.get_child_optional(v.first.data());
        if(is_node_exist) {
          dss_config.erase(v.first.data());
        }
        dss_config.add_child(v.first.data(), v.second);
      }
      smg_info(logger, "Updated Subsystem and Cluster information to the NKV configuration ...");
      //boost::property_tree::write_json(cout, dss_config);
    } catch(exception const& e) {
      smg_error(logger, "%s: %u",__func__, e.what());
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  
};

#endif






