/**
 # The Clear BSD License
 #
 # Copyright (c) 2022 Samsung Electronics Co., Ltd.
 # All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted (subject to the limitations in the disclaimer
 # below) provided that the following conditions are met:
 #
 # * Redistributions of source code must retain the above copyright notice, 
 #   this list of conditions and the following disclaimer.
 # * Redistributions in binary form must reproduce the above copyright notice,
 #   this list of conditions and the following disclaimer in the documentation
 #   and/or other materials provided with the distribution.
 # * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 #   contributors may be used to endorse or promote products derived from this
 #   software without specific prior written permission.
 # NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 # THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 # CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 # NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 # PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 # OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 # WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 # OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 # ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#include "native_fabric_manager.h"


/*
Function Name: process_clustermap
Description  : Process the ClusterMap from REST api only.
Input Args   : None
Return       : <bool> , 0/1 -> Success/Failure
*/
bool NativeFabricManager::process_clustermap()
{
  bool is_good = false;
  string host = get_host();
  string endpoint = get_endpoint();
  string response;
  string cluster_map_url(host+endpoint);
  is_good = RESTful(response, cluster_map_url);
  if (is_good) {
    cluster_map = response;
    ptree cluster_map_json;
    istringstream is (cluster_map);
    read_json(is, cluster_map_json);

    target_cluster_map.add_child("subsystem_maps", cluster_map_json.get_child("subsystem_maps"));
    // Get FM cluster details
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, cluster_map_json.get_child("cm_maps")) {
      assert(value.first.empty());
      ptree fm_nodes_pt = value.second;
      if ( add_fm_node(fm_nodes_pt) ) {
        node_count++;
      }
    }
    //boost::property_tree::write_json(cout, cluster_map_json);
    smg_alert(logger, "ClusterMap: Received clustermap from %s", fm_name.c_str());
    smg_info(logger, "%s: consist of %d node/s cluster", fm_name.c_str(), node_count);
  }
  return is_good;
}


/*
Function Name: add_fm_node
Description  : Add each fm node .
Input Args   : <ptree&> - Node with parameters.
Return       : <bool> , 0/1 -> Success/Failure
*/
bool NativeFabricManager::add_fm_node(ptree& fm_node_pt)
{
  bool is_node_added = false;
  // Iterate over nodes
  try {
    const string fm_id = fm_node_pt.get<string>("cm_id");
    const string fm_server_address = fm_node_pt.get<string>("cm_server_address");
    uint32_t fm_instance_mode = fm_node_pt.get<uint32_t>("cm_instance_mode");
    uint32_t fm_instance_health = fm_node_pt.get<uint32_t>("cm_instance_health");
    const string fm_server_name = fm_node_pt.get<string>("cm_server_name"); 

    FabricManagerNode* node = new FabricManagerNode(fm_id,
                                                    fm_server_address,
                                                    fm_instance_mode,
                                                    fm_instance_health,
                                                    fm_server_name);
    if ( node ) {
      fm_nodesMap[fm_id] = node;
      is_node_added = true;
    }
  } catch ( exception& e ) {
    smg_error(logger,"Exception:%s - %s",__func__, e.what());
  }
  return is_node_added;
}


NativeFabricManager::~NativeFabricManager()
{
  // Remove all Drive objects
  for( auto n_iter = fm_nodesMap.begin(); n_iter != fm_nodesMap.end(); n_iter++)
  {
    delete(n_iter->second);
  }
  fm_nodesMap.clear();
}
