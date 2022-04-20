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

#include "nkv_api.h"
#include <cstdlib>
#include <string>
#include "nkv_framework.h"
#include "dss_version.h"
#include "native_fabric_manager.h"
#include "unified_fabric_manager.h"
#include "auto_discovery.h"
#include "event_handler.h"
#include "nkv_stats.h"
#include <pthread.h>
#include <sstream>
#include <queue>

thread_local int32_t core_running_app_thread = -1;
std::atomic<int32_t> nkv_app_thread_count(0);
std::atomic<uint64_t> nkv_app_put_count(0);
std::atomic<uint64_t> nkv_app_get_count(0);
std::atomic<uint64_t> nkv_app_del_count(0);
std::atomic<uint64_t> nkv_app_list_count(0);

int32_t core_to_pin = -1;
std::thread nkv_thread;
std::thread nkv_event_thread;
int32_t nkv_event_polling_interval_in_sec;
int32_t nkv_stat_thread_polling_interval;
int32_t nkv_stat_thread_needed;
int32_t nkv_dummy_path_stat;
int32_t nkv_event_handler = 0;
int32_t nkv_check_alignment = 0;
std::condition_variable cv_global;
std::mutex mtx_stat;
std::string config_path;
std::queue<std::string> event_queue;
FabricManager* fm = NULL;
int32_t connect_fm = 0;

void event_handler_thread(std::string event_subscribe_channel, 
                          std::string mq_address,
                          int32_t nkv_event_polling_interval,
                          uint64_t nkv_handle) {
    int rc = pthread_setname_np(pthread_self(), "nkv_event_thr");
    if (rc != 0) {
        smg_error(logger, "Error on setting thread name on nkv_handle = %u , rc=%d", nkv_handle,rc);
    }
    // Get channels
    std::vector<std::string> channels;
    boost::split(channels, event_subscribe_channel , boost::is_any_of(","));

    try{
        // Populate event map and start event receiver function.
        if ( event_mapping() ) {
          receive_events(event_queue, mq_address, channels, nkv_event_polling_interval, nkv_handle);
        }
        else {
          smg_error(logger,"Event Handler initiation FAILED.");
        }
    }
    catch (std::exception& e) {
        smg_error(logger, "Event Receiver Failed- %s", e.what());
    }

}


void nkv_thread_func (uint64_t nkv_handle) {
  int rc = pthread_setname_np(pthread_self(), "nkv_stat_thr");
  if (rc != 0) {
    smg_error(logger, "Error on setting thread name on nkv_handle = %u", nkv_handle);
  }
  while (1) {
    nkv_pending_calls.fetch_add(1, std::memory_order_relaxed);

    if (!nkv_cnt_list) {
      smg_error(logger, "nkv_cnt_list is NULL, bailing out, nkv_handle = %u", nkv_handle);
      nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
      break;
    }

    if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
      smg_error(logger, "Wrong nkv handle provided, aborting stat thread, given handle = %u !!", nkv_handle);
      nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
      break;
    }

    if (nkv_stopping) {
      //nkv_cnt_list->collect_nkv_stat();
      smg_warn(logger, "Stopping the nkv_stat_thread, nkv_handle = %u", nkv_handle);
      nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
      break;
    }
    smg_alert(logger, "### NKV Library version in use = %s ###", dssVersion.c_str());
    boost::property_tree::ptree pt;
    try {
      std::unique_lock<std::mutex> lck(mtx_global);
      boost::property_tree::read_json(config_path, pt);
    }
    catch (std::exception& e) {
      smg_error(logger, "%s%s", "Error reading config file and building ptree! Error = ", e.what());
    }
    // Event Handler - Action Manager function
    if(nkv_event_handler) {
      action_manager(event_queue);
    }

    // Update storage information
    if ( connect_fm ) {
      if ( fm ) {
        const vector<string> subsystem_nqn_list = fm->get_subsystem_nqn_list();
        for( unsigned index = 0; index < subsystem_nqn_list.size(); index++ ) {
          Subsystem* subsystem = static_cast<Subsystem*>(fm->get_subsystem(subsystem_nqn_list[index]));
          if ( subsystem && subsystem->get_status() ) {
            Storage* const storage = subsystem->get_storage();
            if ( storage && storage->update_storage()) {
              smg_info(logger, "Updated subsystem storage information, nqn=%s\n\
                      \t\t\tPath Capacity        = %lld Bytes,\n\
                      \t\t\tPath Usage           = %lld Bytes,\n\
                      \t\t\tPath Util Percentage = %6.2f",
                      (subsystem->get_nqn()).c_str(), 
                      storage->get_capacity_bytes(),
                      storage->get_used_bytes(), 
                      storage->get_available_space());
            } else {
              smg_error(logger, "Failed to update storage for subsystem nqn = %s", (subsystem->get_nqn()).c_str());
            }
          }
        } // subsystem iteration
      } 
    } // connect_fm

    try {
      int32_t nkv_dynamic_logging_old = nkv_dynamic_logging;
      nkv_dynamic_logging = pt.get<int>("nkv_enable_debugging", 0);
      nkv_stat_thread_polling_interval = pt.get<int>("nkv_stat_thread_polling_interval_in_sec", 10);
      int32_t prev_path_stat_collection = get_path_stat_collection();
      set_path_stat_collection(pt.get<int>("nkv_need_path_stat", 1));

      if( prev_path_stat_collection && get_path_stat_collection() == 0 ) {
        // Remove operation 
        nkv_cnt_list->remove_nkv_ustat(true, false);
      }
      if( prev_path_stat_collection == 0 && get_path_stat_collection() ) {
        nkv_cnt_list->initiate_nkv_ustat(true, false);
        smg_alert(logger, "Path level Ustat is enabled now!");
      }

      int32_t prev_path_stat_detailed = get_path_stat_detailed();
      set_path_stat_detailed(pt.get<int>("nkv_need_detailed_path_stat", 0));
      if( prev_path_stat_detailed && get_path_stat_detailed() == 0) {
        nkv_cnt_list->remove_nkv_ustat(false, true);
      }

      if( prev_path_stat_detailed == 0 && get_path_stat_detailed()) {
        nkv_cnt_list->initiate_nkv_ustat(false, true);
        smg_alert(logger, "CPU level ustat is enabled now!");
      }

      if (nkv_dynamic_logging_old != nkv_dynamic_logging) {
        nkv_app_put_count = 0;
        nkv_app_get_count = 0;
        nkv_app_del_count = 0;
        nkv_app_list_count = 0;
        nkv_num_read_cache_miss = 0;
      }

      if (nkv_dynamic_logging) 
        smg_alert(logger, "## NKV debugging is ON ##");
      else
        smg_alert(logger, "## NKV debugging is OFF ##");
    }
    catch (std::exception& e) {
      smg_error(logger, "%s%s", "Error reading config file property, Error = %s", e.what());
    }
    if (nkv_dynamic_logging) {
      smg_alert(logger, "Cache based listing = %d, number of cache shards = %d, Num PUTs = %u, Num GETs = %u, Num LISTs = %u, Num DELs = %u, Num Misses = %u", 
                listing_with_cached_keys, nkv_listing_cache_num_shards, nkv_app_put_count.load(), nkv_app_get_count.load(), 
                nkv_app_list_count.load(), nkv_app_del_count.load(), nkv_num_read_cache_miss.load());
    } else {
      if (nkv_remote_listing) {
        /* To Do, some stats ?? */
      } else {
        
        /*smg_alert(logger, "Cache based listing = %d, number of listing cache shards = %d, total number of listing cached keys = %u, total number of listing cached prefixes = %u", 
                listing_with_cached_keys, nkv_listing_cache_num_shards, nkv_num_keys.load(), nkv_num_key_prefixes.load());*/
        nkv_cnt_list->show_listing_stat();

      }
    }

    //nkv_cnt_list->collect_nkv_stat();
    nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
    
    std::unique_lock<std::mutex> lck(mtx_global);
    cv_global.wait_for(lck,std::chrono::seconds(nkv_stat_thread_polling_interval));
    //std::this_thread::sleep_for (std::chrono::seconds(nkv_stat_thread_polling_interval));

  }

}
/* nkv_open API definition */
nkv_result nkv_open(const char *config_file, const char* app_uuid, const char* host_name_ip, uint32_t host_port,
                    uint64_t *instance_uuid, uint64_t* nkv_handle) {

  if (!config_file || !host_name_ip || (host_port <= 0))
    return NKV_ERR_NULL_INPUT;
 
  logger = smg_acquire_logger("libnkv");  
  assert(logger != NULL);

  if (is_kvs_initialized.load()) {
    smg_info(logger, "NKV already intialized for app = %s", app_uuid);
    return NKV_ERR_ALREADY_INITIALIZED;  
  }

  config_path =  config_file;

  if (NULL == config_file) {
    config_path = NKV_CONFIG_FILE;
   
    if(const char* env_p = std::getenv("NKV_CONFIG_FILE")) {
      config_path = env_p;
    }
  }
  smg_info(logger, "NKV config file = %s", config_path.c_str());
  smg_alert(logger, "### NKV Library version in use = %s ###", dssVersion.c_str());
  std::string nkv_unique_instance_name = std::string(app_uuid) + "-" + std::string(host_name_ip) + "-" + std::to_string(host_port);

  *nkv_handle = std::hash<std::string>{}(app_uuid);

  *instance_uuid  = std::hash<std::string>{}(nkv_unique_instance_name);

  smg_info(logger, "Generated NKV handle = %u , NKV instance uuid = %u for app uuid = %s",
          *nkv_handle, *instance_uuid, app_uuid);

  boost::property_tree::ptree pt;

  try {
    boost::property_tree::read_json(config_path, pt);
  }
  catch (std::exception& e) {
    smg_error(logger, "%s%s", "Error reading config file and building ptree! Error = %s", e.what()); 
    return NKV_ERR_CONFIG;
  }

  std::string fm_address;
  std::string fm_endpoint;
  int32_t nkv_transport = 0;
  int32_t min_container = 0;
  int32_t min_container_path = 0;
  int32_t nkv_container_path_qd = 32;
  int32_t nkv_app_thread_core = -1;
  std::string event_subscribe_channel;
  std::string mq_address;
  
  try {
    //0 = Local KV, 1 = nvmeOverTCPKernel, 2 = nvmeOverTCPSPDK, 3 = nvmeOverRDMAKernel, 4 = nvmeOverRDMASPDK
    nkv_transport = pt.get<int>("nkv_transport");
    min_container = pt.get<int>("min_container_required");
    min_container_path = pt.get<int>("min_container_path_required");
    nkv_container_path_qd = pt.get<int>("nkv_container_path_qd");
    core_pinning_required = pt.get<int>("nkv_core_pinning_required");
    if (core_pinning_required)
      nkv_app_thread_core = pt.get<int>("nkv_app_thread_core");
    queue_depth_monitor_required = pt.get<int>("nkv_queue_depth_monitor_required");
    if (queue_depth_monitor_required)
      queue_depth_threshold = pt.get<int>("nkv_queue_depth_threshold_per_path");

    int32_t iter_feature_required = pt.get<int>("drive_iter_support_required");
    if (iter_feature_required) {
      iter_prefix = pt.get<std::string>("iter_prefix_to_filter");
      listing_with_cached_keys = pt.get<int>("nkv_listing_with_cached_keys");
      if (listing_with_cached_keys) {
        MAX_DIR_ENTRIES = pt.get<int>("nkv_listing_num_directories", 8);
        nkv_listing_wait_till_cache_init = pt.get<int>("nkv_listing_wait_iter_done_on_init", 1);
        key_default_delimiter = pt.get<std::string>("nkv_key_default_delimiter", "/"); 
        nkv_listing_need_cache_stat = pt.get<int>("nkv_listing_need_cache_stat", 1);
        nkv_listing_cache_num_shards = pt.get<int>("nkv_listing_cache_num_shards", 1024);
      }
      num_path_per_container_to_iterate = pt.get<int>("nkv_num_path_per_container_to_iterate");
      nkv_stat_thread_polling_interval = pt.get<int>("nkv_stat_thread_polling_interval_in_sec", 100);
      nkv_stat_thread_needed = pt.get<int>("nkv_stat_thread_needed", 1);
      set_path_stat_collection(pt.get<int>("nkv_need_path_stat", 0));
      set_path_stat_detailed(pt.get<int>("nkv_need_detailed_path_stat", 0));
      nkv_dummy_path_stat = pt.get<int>("nkv_dummy_path_stat", 0);
      nkv_use_read_cache = pt.get<int>("nkv_use_read_cache", 0);
      nkv_read_cache_size = pt.get<int>("nkv_read_cache_size", 1024);
      nkv_read_cache_shard_size = pt.get<int>("nkv_read_cache_shard_size", 1024);
      nkv_data_cache_size_threshold = pt.get<int>("nkv_data_size_threshold", 4096);
      nkv_use_data_cache = pt.get<int>("nkv_use_data_cache", 0);
    }
    nkv_is_on_local_kv = pt.get<int>("nkv_is_on_local_kv");
    nkv_remote_listing = pt.get<int>("nkv_remote_listing", 0);
    nkv_max_key_length = (uint32_t)pt.get<int>("nkv_max_key_length", NKV_MAX_KEY_LENGTH);
    nkv_max_value_length = (uint32_t)pt.get<int>("nkv_max_value_length", NKV_MAX_VALUE_LENGTH);
    nkv_in_memory_exec = pt.get<int>("nkv_in_memory_exec", 0);
    nkv_check_alignment = pt.get<int>("nkv_check_alignment", 0);
    nkv_device_support_iter = pt.get<int>("nkv_device_support_iterator", 1);
    if (nkv_remote_listing) {

      transient_prefix = pt.get<std::string>("transient_prefix_to_filter", "meta/.minio.sys/tmp/" );
    }
    if(!nkv_is_on_local_kv) {
      nic_load_balance = pt.get<int>("nkv_load_balancer", 0);
    }
    if (nic_load_balance) {
      nic_load_balance_policy = pt.get<int>("nkv_load_balancer_policy", 0); 
    }
    // switches for auto discovery and events, not applicable for local KV
    if (! nkv_is_on_local_kv ) { 
      connect_fm = pt.get<int>("contact_fm", 0);
      if (! connect_fm ) {
        smg_alert(logger, "Reading cluster map information from %s", config_path.c_str());
      }
      fm_address  = pt.get<std::string>("fm_address");
      fm_endpoint = pt.get<std::string>("fm_endpoint");
      nkv_event_handler = pt.get<int>("nkv_event_handler", 1);
      event_subscribe_channel = pt.get<std::string>("event_subscribe_channel");
      mq_address = pt.get<std::string>("mq_address");
      nkv_event_polling_interval_in_sec = pt.get<int32_t>("nkv_event_polling_interval_in_sec", 60);
      nvme_connect_delay_in_mili_sec =  pt.get<uint32_t>("nvme_connect_delay_in_mili_sec", 2000);
    }

    if (!nkv_is_on_local_kv) {
      set_path_stat_collection(0);
    }
  }
  catch (std::exception& e) {
    smg_error(logger, "%s%s", "Error reading config file property, Error = %s", e.what());
    return NKV_ERR_CONFIG;
  }
  smg_alert(logger,"contact_fm = %d, nkv_transport = %d, min_container_required = %d, min_container_path_required = %d, container_path_qd = %d, is_local = %d", 
          connect_fm, nkv_transport, min_container, min_container_path, nkv_container_path_qd, nkv_is_on_local_kv);
  smg_alert(logger, "core_pinning_required = %d, app_thread_core = %d, nkv_queue_depth_monitor_required = %d, nkv_queue_depth_threshold_per_path = %d",
            core_pinning_required, nkv_app_thread_core, queue_depth_monitor_required, queue_depth_threshold);

  if (nkv_app_thread_core != -1)
    core_to_pin = nkv_app_thread_core;
  nkv_async_path_max_qd = nkv_container_path_qd;
  nkv_cnt_list = new NKVContainerList(0, app_uuid, *instance_uuid, *nkv_handle);
  assert(nkv_cnt_list != NULL);

  if (nkv_is_on_local_kv) {
    if (nkv_cnt_list->add_local_container_and_path(host_name_ip, host_port, pt))
      return NKV_ERR_CONFIG;
  } else {
    // Receive taregt cluster_map information from FabricManager.
    if (connect_fm) {
      if ( pt.get<long>("fm_connection_timeout", 0 ) ) {
        REST_CALL_TIMEOUT = pt.get<long>("fm_connection_timeout");
      }
      int32_t fm_redfish_compliant = pt.get<long>("fm_redfish_compliant", 1 );
      if ( fm_redfish_compliant ) {
        fm = new UnifiedFabricManager(fm_address, fm_endpoint);
      } else {
        fm = new NativeFabricManager(fm_address, fm_endpoint);
      }
      bool ret = fm->process_clustermap();

      if (ret){
        if ( fm->get_clustermap(pt)) {
          smg_error(logger, "NKV: Falied to receive cluster map information ... ");
          return NKV_ERR_FM;
        }
      } else{
        smg_error(logger, "NKV: Falied to receive cluster map from Fabric Manager ...");
        return NKV_ERR_FM;
      }
    } 
    smg_alert(logger, "NKV API: Adding NKV remote mount paths ...");
    // Connect remote mount paths through Auto Discovery
    if (! add_remote_mount_path(pt) ){
      smg_error(logger, "Auto Discovery failed to retrieve the remote mount path");
      return NKV_ERR_CONFIG;
    }
    // Update nkv container_list with new containers or subsystem for remote KV
    if (nkv_cnt_list->parse_add_container(pt)) 
      return NKV_ERR_CONFIG;
  }



  if (!nkv_is_on_local_kv && nkv_cnt_list->parse_add_path_mount_point(pt))
    return NKV_ERR_CONFIG;

  if (*nkv_handle != 0 && *instance_uuid != 0) {
    // All good, start initializing open_mpdk stuff
    #ifdef SAMSUNG_API
      kvs_init_options options;
      kvs_init_env_opts(&options);
    #endif

    if ((NKV_TRANSPORT_LOCAL_KERNEL == nkv_transport) || (NKV_TRANSPORT_NVMF_TCP_KERNEL == nkv_transport
         || NKV_TRANSPORT_NVMF_RDMA_KERNEL == nkv_transport)) {
      smg_info(logger,"**NKV transport is Over kernel**");
      #ifdef SAMSUNG_API
        options.aio.iocoremask = 0;
        options.memory.use_dpdk = 0;
        options.aio.queuedepth = nkv_container_path_qd;
        const char *emulconfigfile = "../kvssd_emul.conf";
        options.emul_config_file =  emulconfigfile;
      #endif

    } else if (NKV_TRANSPORT_NVMF_TCP_SPDK == nkv_transport) {
      //To do, add spdk related kvs options 
    } else {

    }
    #ifdef SAMSUNG_API
      kvs_init_env(&options);
      smg_info(logger, "Setting environment for open mpdk is successful for app = %s", app_uuid);
    #endif

    if(!nkv_cnt_list->open_container_paths())
      return NKV_ERR_COMMUNICATION;

    // Minimum path topology verification
    if (!nkv_cnt_list->verify_min_topology_exists(min_container, min_container_path)) {
      return NKV_ERR_CONFIG;
    } else {
      smg_info(logger, "Min subsystem/path topology verification is satisfied!");
    }
  } else {
    smg_error(logger, "Either NKV handle or NKV instance handle generated is zero !");
    return NKV_ERR_INTERNAL; 
  }

  if (nkv_stat_thread_needed || nkv_event_handler) {
    smg_info(logger, "Creating stat thread for nkv, app = %s", app_uuid);
    nkv_thread = std::thread(nkv_thread_func, *nkv_handle); 
  }

  // Device stat initialization
  if( get_path_stat_collection()) {
    nkv_cnt_list->initiate_nkv_ustat(true, false);
  }
  // CPU stat initialization 
  if( get_path_stat_detailed()) {
    nkv_cnt_list->initiate_nkv_ustat(false, true);
  }

  // Add event_handler_thread
  if (nkv_event_handler) {
      smg_alert(logger,"Creating event handler thread for nkv");
      nkv_event_thread = std::thread(event_handler_thread, 
                                     event_subscribe_channel,
                                     mq_address,
                                     nkv_event_polling_interval_in_sec,
                                     *nkv_handle
                                    );
  }

  if (listing_with_cached_keys && nkv_device_support_iter && !nkv_remote_listing && !nkv_in_memory_exec) {
    bool will_wait = nkv_listing_wait_till_cache_init ? true:false;
    auto start = std::chrono::steady_clock::now();
    nkv_cnt_list->wait_or_detach_thread (will_wait);
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start);
    smg_alert(logger, "NKV index cache building took %u seconds", (uint32_t)(((long double)elapsed.count())/1000000.0) );     
  } else {
    smg_alert(logger,"### Not going for iteration and building the index ###");
  }
  smg_info(logger, "NKV open is successful for app = %s", app_uuid);
  pt.clear();
  is_kvs_initialized.store(true);
  return NKV_SUCCESS;

}

std::atomic<bool> is_kvs_closed (false);
/* nkv_close API definition */
nkv_result nkv_close (uint64_t nkv_handle, uint64_t instance_uuid) {

  smg_info(logger, "nkv_close invoked for nkv_handle = %u", nkv_handle);
  if (is_kvs_closed.load()) {
    smg_warn(logger,  "nkv_close already invoked for nkv_handle = %u", nkv_handle);
    return NKV_SUCCESS;
  }
  is_kvs_closed.store(true); 
  nkv_stopping = true;
  try {
    if (nkv_stat_thread_needed || nkv_event_handler) {
      cv_global.notify_all();
      nkv_thread.join();
    }
    if (nkv_event_handler) {
      nkv_event_thread.join();
    }
    while (nkv_pending_calls) {
      usleep(SLEEP_FOR_MICRO_SEC);
    }
    assert(nkv_pending_calls == 0);

    if (nkv_cnt_list) {
      delete nkv_cnt_list;
      nkv_cnt_list = NULL;
    }

    if ( fm ) {
      delete fm;
      fm = NULL;
    }
 
    #ifdef SAMSUNG_API 
      kvs_exit_env();
    #endif

    if (logger)
      smg_release_logger(logger);
  }
  catch (std::exception& e) {
    smg_error(logger, "%s%s", "Error during nkv_close, nkv_handle = %u, Error = %s", e.what());
    return NKV_ERR_INTERNAL;
  }

  return NKV_SUCCESS;
}

/* nkv_get_instance_info API definition */
nkv_result nkv_get_instance_info(uint64_t nkv_handle, uint64_t instance_uuid, nkv_instance_info* info) {
  return NKV_SUCCESS;
}

/* nkv_list_instance_info API definition */
nkv_result nkv_list_instance_info(uint64_t nkv_handle, const char* host_name_ip , uint32_t index,
                                  nkv_instance_info* info, uint32_t* num_instances) {
  return NKV_SUCCESS;
}

/* nkv_get_version_info API definition */
nkv_result nkv_get_version_info(uint64_t nkv_handle, uint64_t instance_uuid, uint32_t* major_v, uint32_t* minor_v) {
  return NKV_SUCCESS;
}

nkv_result nkv_physical_container_list (uint64_t nkv_handle, uint32_t index, nkv_container_info *cntlist, uint32_t *cnt_count) {

  if (!cntlist) {
    smg_error(logger, "Required preallocated in/out data structure is NULL");
    return NKV_ERR_NULL_INPUT;
  }
  
  if (*cnt_count > NKV_MAX_ENTRIES_PER_CALL || *cnt_count == 0 ) {
    smg_error(logger, "Number of entries in preallocated list is more than NKV_MAX_ENTRIES_PER_CALL or zero");
    return NKV_ERR_WRONG_INPUT;
  }
  if (index < 0)
    return NKV_ERR_INVALID_START_INDEX;

  nkv_pending_calls.fetch_add(1, std::memory_order_relaxed);
  if (nkv_stopping) {
    nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed); 
    return NKV_ERR_INS_STOPPING;
  }
  if (!nkv_cnt_list) {
    nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
    return NKV_ERR_INTERNAL;
  }
  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u !!", nkv_handle);
    nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
    return NKV_ERR_HANDLE_INVALID;
  }
  if (!nkv_cnt_list->populate_container_info(cntlist, cnt_count, index)) {
    nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
    return NKV_ERR_NO_CNT_FOUND;
  }
  nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
  return NKV_SUCCESS;
}

void* nkv_malloc(size_t size) {
  return kvs_malloc(size, 4096);
}

void* nkv_zalloc(size_t size) {
  return kvs_zalloc(size, 4096);
}


void* nkv_malloc_aligned(size_t size, size_t alignment) {
  return kvs_malloc(size, alignment);
}

void* nkv_zalloc_aligned(size_t size, size_t alignment) {
  return kvs_zalloc(size, alignment);
}

void nkv_free(void* buf) {
  return kvs_free(buf);
}


nkv_result nkv_send_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, void* opt, nkv_value* value, 
                         int32_t which_op, nkv_postprocess_function* post_fn = NULL, 
                         uint32_t client_rdma_key = 0, uint16_t client_rdma_qhandle = 0) {

  if (core_pinning_required && core_running_app_thread == -1 && post_fn) {
    nkv_app_thread_count.fetch_add(1, std::memory_order_relaxed);
    if (core_to_pin != -1) {
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(core_to_pin, &cpuset);
      int32_t ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      if (ret != 0) {
        smg_error(logger, "Setting App thread affinity failed, thread_id = %u, core_id = %d, total app thread = %d", 
                  pthread_self(), core_to_pin, nkv_app_thread_count.load());
        assert(ret == 0);

      } else {
        smg_alert(logger, "Setting App thread affinity successful, thread_id = %u, core_id = %d, total app thread = %d", 
                  pthread_self(), core_to_pin, nkv_app_thread_count.load());
        core_running_app_thread = core_to_pin;
      }
    } else {
      smg_warn(logger, "Core not supplied to set App thread affinity, ignoring, performance may be affected !!");
      core_running_app_thread = 0;
    }
  }

  if (!ioctx) {
    smg_error(logger, "Ioctx is NULL !!, nkv_handle = %u, op = %d", nkv_handle, which_op);
    return NKV_ERR_NULL_INPUT;
  }

  if (!key) {
    smg_error(logger, "key is NULL !!, nkv_handle = %u, op = %d", nkv_handle, which_op);
    return NKV_ERR_NULL_INPUT;
  }

  if (!opt && which_op != NKV_DELETE_OP)  {
    smg_error(logger, "opt is NULL !!, nkv_handle = %u, op = %d", nkv_handle, which_op);
    return NKV_ERR_NULL_INPUT;
  }

  if (!value && !(which_op == NKV_DELETE_OP || \
			which_op == NKV_LOCK_OP || \
			which_op == NKV_UNLOCK_OP)) {
    smg_error(logger, "value is NULL !!, nkv_handle = %u, op = %d", \
						nkv_handle, which_op);
    return NKV_ERR_NULL_INPUT;
  }

  nkv_result stat = NKV_SUCCESS;

  nkv_pending_calls.fetch_add(1, std::memory_order_relaxed);
  if (nkv_stopping) {
    stat = NKV_ERR_INS_STOPPING;
    goto done;
  }
  if (!nkv_cnt_list) {
    stat = NKV_ERR_INTERNAL;
    goto done;
  }
  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = %d !!", nkv_handle, which_op);
    stat = NKV_ERR_HANDLE_INVALID;
    goto done;
  }
  
  if (ioctx->is_pass_through) {
    uint64_t cnt_hash = ioctx->container_hash;
    uint64_t cnt_path_hash = ioctx->network_path_hash;
    if (nkv_check_alignment) {

      if ((uint64_t)value % nkv_check_alignment != 0) {
        smg_warn(logger, "Non %u Byte Aligned value address = 0x%x, op = %d, performance will be impacted !!", nkv_check_alignment, value, which_op);
      }
      
    } 
    stat = nkv_cnt_list->nkv_send_io(cnt_hash, cnt_path_hash, key, (void*)opt, value, which_op, post_fn, client_rdma_key, client_rdma_qhandle); 

  } else {
    smg_error(logger, "Wrong input, nkv non-pass through mode is not supported yet, op = %d !", which_op);
    stat = NKV_ERR_WRONG_INPUT;
  }

done:
  nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
  return stat;
}


nkv_result nkv_store_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_store_option* opt, nkv_value* value) {

  if (nkv_dynamic_logging) {
    nkv_app_put_count.fetch_add(1, std::memory_order_relaxed);
  }

  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*) opt, value, NKV_STORE_OP);
  if (stat != NKV_SUCCESS)
    smg_error(logger, "NKV store operation failed for nkv_handle = %u, key = %s, key_length = %u, value_length = %u, code = %d", 
              nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value ? value->length:0, stat);
  else
    smg_info(logger, "NKV store operation is successful for nkv_handle = %u, key = %s, key_length = %u, value_length = %u", 
             nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value ? value->length:0);
  return stat;
}

nkv_result nkv_store_kvp_rdd (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_store_option* opt, nkv_value* value,
                                 uint32_t client_rdma_key, uint16_t client_rdma_qhandle) {
  if (nkv_dynamic_logging) {
    nkv_app_get_count.fetch_add(1, std::memory_order_relaxed);
  }
  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*) opt, value, NKV_STORE_OP_RDD, NULL, client_rdma_key, client_rdma_qhandle);

  if (stat != NKV_SUCCESS)
    smg_error(logger, "NKV store direct operation failed for nkv_handle = %u, key = %s, key_length = %u, value_length = %u, code = %d",
              nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value ? value->length:0, stat);
  else
    smg_info(logger, "NKV store direct operation is successful for nkv_handle = %u, key = %s, key_length = %u, value_length = %u",
             nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value ? value->length:0);

  return stat;

}

nkv_result nkv_retrieve_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_retrieve_option* opt, nkv_value* value) {

  if (nkv_dynamic_logging) {
    nkv_app_get_count.fetch_add(1, std::memory_order_relaxed);
  }
  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*) opt, value, NKV_RETRIEVE_OP);
  if (stat != NKV_SUCCESS) {
    if (stat != NKV_ERR_KEY_NOT_EXIST) {
      smg_error(logger, "NKV retrieve operation failed for nkv_handle = %u, key = %s, key_length = %u, value_length = %u, code = %d", nkv_handle, 
                 key ? (char*)key->key: "NULL", key ? key->length:0, value ? value->length:0, stat);
    } else {
      smg_info(logger, "NKV retrieve operation failed for nkv_handle = %u, key = %s, key_length = %u, code = %d", nkv_handle, 
                key ? (char*)key->key: "NULL", key ? key->length:0, stat);
    }
  }
  else {
    smg_info(logger, "NKV retrieve operation is successful for nkv_handle = %u, key = %s, key_length = %u, supplied length = %u, actual length = %u", 
             nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value->length, value->actual_length);
    if (value->actual_length == 0)
      smg_error(logger, "NKV retrieve operation returned 0 value length object !!, nkv_handle = %u, key = %s, key_length = %u, supplied length = %u, actual length = %u",
                nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value->length, value->actual_length); 
  }
  return stat;

}

nkv_result nkv_retrieve_kvp_rdd (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_retrieve_option* opt, nkv_value* value,
                                 uint32_t client_rdma_key, uint16_t client_rdma_qhandle) {
  if (nkv_dynamic_logging) {
    nkv_app_get_count.fetch_add(1, std::memory_order_relaxed);
  }
  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*) opt, value, NKV_RETRIEVE_OP_RDD, NULL, client_rdma_key, client_rdma_qhandle);
  if (stat != NKV_SUCCESS) {
    if (stat != NKV_ERR_KEY_NOT_EXIST) {
      smg_error(logger, "NKV retrieve direct operation failed for nkv_handle = %u, key = %s, key_length = %u, value_length = %u, code = %d", nkv_handle,
                 key ? (char*)key->key: "NULL", key ? key->length:0, value ? value->length:0, stat);
    } else {
      smg_info(logger, "NKV retrieve direct operation failed for nkv_handle = %u, key = %s, key_length = %u, code = %d", nkv_handle,
                key ? (char*)key->key: "NULL", key ? key->length:0, stat);
    }
  }
  else {
    smg_info(logger, "NKV retrieve direct operation is successful for nkv_handle = %u, key = %s, key_length = %u, supplied length = %u, actual length = %u",
             nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value->length, value->actual_length);
    if (value->actual_length == 0)
      smg_error(logger, "NKV retrieve direct operation returned 0 value length object !!, nkv_handle = %u, key = %s, key_length = %u, supplied length = %u, actual length = %u",
                nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value->length, value->actual_length);
  }
  return stat;

}

nkv_result nkv_delete_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key) {

  if (nkv_dynamic_logging) {
    nkv_app_del_count.fetch_add(1, std::memory_order_relaxed);
  }
  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, NULL, NULL, NKV_DELETE_OP);
  if (stat != NKV_SUCCESS)
    smg_error(logger, "NKV delete operation failed for nkv_handle = %u, key = %s, key_length = %u, code = %d", nkv_handle, 
              key ? (char*)key->key: "NULL", key ? key->length:0, stat);
  else
    smg_info(logger, "NKV delete operation is successful for nkv_handle = %u, key = %s, key_length = %u", nkv_handle, 
             key ? (char*)key->key: "NULL", key ? key->length:0);
  return stat;

}

nkv_result nkv_lock_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, \
				const nkv_key* key, const nkv_lock_option *opt)
{

  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*)opt, \
						NULL, NKV_LOCK_OP);
  if (stat != NKV_SUCCESS) {
    if (stat != NKV_ERR_LOCK_KEY_LOCKED) {
      smg_error(logger, \
		"NKV lock operation failed for nkv_handle = %u,"\
		" key = %s, key_length = %u, code = %d", nkv_handle, 
                 key ? (char*)key->key: "NULL", key ? key->length:0, stat);
    } else {
      smg_warn(logger, \
		"NKV lock operation failed for nkv_handle = %u, "\
		"key = %s, key_length = %u, code = %d", nkv_handle, 
                key ? (char*)key->key: "NULL", key ? key->length:0, stat);
    }
  }
  else {
    smg_info(logger, \
		"NKV lock operation is successful for nkv_handle = %u,"\
		" key = %s, key_length = %u", 
             nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0);
  }
  return stat;

}

nkv_result nkv_unlock_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, \
			const nkv_key* key, const nkv_unlock_option *opt)
{

  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, \
						(void*)opt, NULL, NKV_UNLOCK_OP);
  if (stat != NKV_SUCCESS)
    smg_error(logger, \
			"NKV unlock operation failed for nkv_handle = %u,"\
			" key = %s, key_length = %u, code = %d", nkv_handle, 
              key ? (char*)key->key: "NULL", key ? key->length:0, stat);
  else
    smg_info(logger, \
			"NKV unlock operation is successful for nkv_handle = %u,"\
			" key = %s, key_length = %u", nkv_handle, 
             key ? (char*)key->key: "NULL", key ? key->length:0);
  return stat;

}

nkv_result nkv_store_kvp_async (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_store_option* opt,
                                nkv_value* value, nkv_postprocess_function* post_fn) {

  if (!post_fn) {
    smg_error(logger, "NKV store async post_fn is NULL !!, nkv_handle = %u, op = %d", nkv_handle, NKV_STORE_OP);
    return NKV_ERR_NULL_INPUT;
  }
  if (!post_fn->nkv_aio_cb) {
    smg_error(logger, "NKV store async Call back function within post_fn is NULL !!, nkv_handle = %u, op = %d", nkv_handle, NKV_STORE_OP);
    return NKV_ERR_NULL_INPUT;
  }
  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*) opt, value, NKV_STORE_OP, post_fn);
  if (stat != NKV_SUCCESS)
    smg_error(logger, "NKV store async operation start failed for nkv_handle = %u, key = %s, value_length = %u, code = %d", 
              nkv_handle, (char*)key->key, value ? value->length:0, stat);
  else
    smg_info(logger, "NKV store async operation start is successful for nkv_handle = %u, key = %s, value_length = %u", 
             nkv_handle, (char*)key->key, value ? value->length:0);
  return stat;

}

nkv_result nkv_retrieve_kvp_async (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_retrieve_option* opt,
                                nkv_value* value, nkv_postprocess_function* post_fn) {

  if (!post_fn) {
    smg_error(logger, "NKV retrieve async post_fn is NULL !!, nkv_handle = %u, op = %d", nkv_handle, NKV_RETRIEVE_OP);
    return NKV_ERR_NULL_INPUT;
  }
  if (!post_fn->nkv_aio_cb) {
    smg_error(logger, "NKV retrieve async Call back function within post_fn is NULL !!, nkv_handle = %u, op = %d", nkv_handle, NKV_RETRIEVE_OP);
    return NKV_ERR_NULL_INPUT;
  }
  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*) opt, value, NKV_RETRIEVE_OP, post_fn);
  if (stat != NKV_SUCCESS)
    smg_error(logger, "NKV retrieve async operation start failed for nkv_handle = %u, key = %s, value_length = %u, code = %d", 
              nkv_handle, (char*)key->key, value ? value->length:0, stat);
  else
    smg_info(logger, "NKV retrieve async operation start is successful for nkv_handle = %u, key = %s, value_length = %u", 
             nkv_handle, (char*)key->key, value ? value->length:0);
  return stat;

}

nkv_result nkv_delete_kvp_async (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, nkv_postprocess_function* post_fn) {

  if (!post_fn) {
    smg_error(logger, "NKV delete async post_fn is NULL !!, nkv_handle = %u, op = %d", nkv_handle, NKV_DELETE_OP);
    return NKV_ERR_NULL_INPUT;
  }
  if (!post_fn->nkv_aio_cb) {
    smg_error(logger, "NKV delete async Call back function within post_fn is NULL !!, nkv_handle = %u, op = %d", nkv_handle, NKV_DELETE_OP);
    return NKV_ERR_NULL_INPUT;
  }
  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, NULL, NULL, NKV_DELETE_OP, post_fn);
  if (stat != NKV_SUCCESS)
    smg_error(logger, "NKV delete async operation start failed for nkv_handle = %u, key = %s, code = %d", nkv_handle, (char*)key->key, stat);
  else
    smg_info(logger, "NKV delete async operation start is successful for nkv_handle = %u, key = %s", nkv_handle, (char*)key->key);
  return stat;

}

nkv_result nkv_indexing_list_keys (uint64_t nkv_handle, nkv_io_context* ioctx, const char* bucket_name, const char* prefix,
                                   const char* delimiter, const char* start_after, uint32_t* max_keys, nkv_key* keys, void** iter_context ) {

  if (!ioctx) {
    smg_error(logger, "Ioctx is NULL !!, nkv_handle = %u, op = list_keys", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  if (!keys || (*max_keys <= 0)) {
    smg_error(logger, "Input keys buffer is NULL or max_keys is invalid!!, nkv_handle = %u, op = list_keys", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  nkv_result stat = NKV_SUCCESS;

  nkv_pending_calls.fetch_add(1, std::memory_order_relaxed);
  if (nkv_stopping) {
    stat = NKV_ERR_INS_STOPPING;
    goto done;
  }
  if (!nkv_cnt_list) {
    stat = NKV_ERR_INTERNAL;
    goto done;
  }
  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = list_keys !!", nkv_handle);
    stat = NKV_ERR_HANDLE_INVALID;
    goto done;
  }

  if (nkv_dynamic_logging) {
    nkv_app_list_count.fetch_add(1, std::memory_order_relaxed);
  }

  if (ioctx->is_pass_through) {
    uint64_t cnt_hash = ioctx->container_hash;
    uint64_t cnt_path_hash = ioctx->network_path_hash;
    stat = nkv_cnt_list->nkv_list_keys(cnt_hash, cnt_path_hash, max_keys, keys, *iter_context, prefix, delimiter, start_after);

  } else {
    smg_error(logger, "Wrong input, nkv non-pass through mode is not supported yet, op = list_keys !");
    stat = NKV_ERR_WRONG_INPUT;
  }

done:
  nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
  return stat;

}

nkv_result nkv_get_path_stat (uint64_t nkv_handle, nkv_mgmt_context* mgmtctx, nkv_path_stat* p_stat) {

  if (!mgmtctx) {
    smg_error(logger, "mgmtctx is NULL !!, nkv_handle = %u, op = nkv_get_path_stat", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  if (!p_stat) {
    smg_error(logger, "Input stat structure is NULL!!, nkv_handle = %u, op = nkv_get_path_stat", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  nkv_result stat = NKV_SUCCESS;

  nkv_pending_calls.fetch_add(1, std::memory_order_relaxed);
  if (nkv_stopping) {
    stat = NKV_ERR_INS_STOPPING;
    goto done;
  }
  if (!nkv_cnt_list) {
    stat = NKV_ERR_INTERNAL;
    goto done;
  }
  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_get_path_stat !!", nkv_handle);
    stat = NKV_ERR_HANDLE_INVALID;
    goto done;
  }

  if (mgmtctx->is_pass_through && nkv_is_on_local_kv && !nkv_dummy_path_stat) {
    uint64_t cnt_hash = mgmtctx->container_hash;
    uint64_t cnt_path_hash = mgmtctx->network_path_hash;
    std::string p_mount;
    stat = nkv_cnt_list->nkv_get_path_mount_point(cnt_hash, cnt_path_hash, p_mount);
    if (stat == NKV_SUCCESS) {
      {
        std::unique_lock<std::mutex> lck(mtx_stat);
        stat = nkv_get_path_stat_util(p_mount, p_stat);
      }
    } else {
      smg_error(logger, "Not able to get NKV path mount point, handle = %u, cnt_hash = %u, cnt_path_hash = %u", 
               nkv_handle, cnt_hash, cnt_path_hash);
    }

  } else {
    uint64_t cnt_hash = mgmtctx->container_hash;
    uint64_t cnt_path_hash = mgmtctx->network_path_hash;
    std::string subsystem_nqn;
    std::string p_mount;
    stat = nkv_cnt_list->nkv_get_target_container_name(cnt_hash, cnt_path_hash, subsystem_nqn, p_mount);

    if (stat == NKV_SUCCESS) {
      p_mount.copy(p_stat->path_mount_point, p_mount.length());
      if(connect_fm){
        stat = nkv_get_remote_path_stat(fm, subsystem_nqn, p_stat );
      } else {
        smg_alert(logger, "NKV is not connected to the FabricManager, Skipping remote stat collection.");
        smg_warn(logger, "Continue with default stat values");
        p_stat->path_storage_capacity_in_bytes = 0;
        p_stat->path_storage_usage_in_bytes = 0;
        p_stat->path_storage_util_percentage = 0;
      }
    }
  }
  
  if (stat == NKV_SUCCESS) {
    smg_info(logger, "NKV path mount = %s,\n\
                      \t\t\tpath capacity = %lld Bytes,\n\
                      \t\t\tpath usage = %lld Bytes,\n\
                      \t\t\tpath util percentage = %6.2f",
                      p_stat->path_mount_point, 
                      (long long)p_stat->path_storage_capacity_in_bytes,
                      (long long)p_stat->path_storage_usage_in_bytes, 
                      p_stat->path_storage_util_percentage);
  }

done:
  nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
  return stat;

}

nkv_result nkv_get_supported_feature_list(uint64_t nkv_handle, nkv_feature_list *features) {
  if (!features) {
    smg_error(logger, "feature list is NULL !!, nkv_handle = %u, op = nkv_get_supported_feature_list", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  if(nkv_is_on_local_kv) {
    smg_error(logger, "nkv_get_supported_feature_list is Not Supported in local NKV mode");
	return NKV_ERR_MODE_NOT_SUPPORT;
  }

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_get_supported_feature_list !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  nkv_pending_calls.fetch_add(1, std::memory_order_relaxed);
  features->nic_load_balance = nic_load_balance;
  features->nic_load_balance_policy = nic_load_balance_policy;
  nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
	
  return NKV_SUCCESS;
}


nkv_result nkv_set_supported_feature_list(uint64_t nkv_handle, nkv_feature_list *features) {
  if (!features) {
    smg_error(logger, "feature list is NULL !!, nkv_handle = %u, op = nkv_get_supported_feature_list", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }
  
  if(nkv_is_on_local_kv) {
    smg_error(logger, "nkv_set_supported_feature_list is Not Supported in local NKV mode");
	return NKV_ERR_MODE_NOT_SUPPORT;
  }

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_set_supported_feature_list !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  nkv_pending_calls.fetch_add(1, std::memory_order_relaxed);
  nic_load_balance = features->nic_load_balance;
  nic_load_balance_policy = features->nic_load_balance_policy;
  nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
  
  return NKV_SUCCESS;
}

nkv_result nkv_register_stat_counter(uint64_t nkv_handle, const char* module_name, nkv_stat_counter* stat_cnt, void **statctx) {

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  if (!module_name) {
    smg_error(logger, "NULL module_name provided, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  if (!stat_cnt) {
    smg_error(logger, "NULL stat_cnt provided, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  if (*statctx) {
    smg_error(logger, "Non-NULL statctx provided, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    return NKV_ERR_WRONG_INPUT;
  }

  if (stat_cnt->counter_name == NULL) {
    smg_error(logger, "NULL counter_name provided, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  if( !get_path_stat_collection()) {
    smg_error(logger, "NKV side stat collection is not enabled, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    return NKV_ERR_INTERNAL;  
  }

  ustat_named_t* ustat_counter = (ustat_named_t*) malloc (sizeof(ustat_named_t));
  assert(ustat_counter != NULL);
  memset(ustat_counter, 0, sizeof(ustat_named_t));
  ustat_counter->usn_name = (char*) malloc (strlen(stat_cnt->counter_name) + 1);
  
  strcpy((char*)ustat_counter->usn_name, stat_cnt->counter_name);
  
  switch (stat_cnt->counter_type) {

    case STAT_TYPE_INT8:
      ustat_counter->usn_type = USTAT_TYPE_INT8;
      break;

    case STAT_TYPE_INT16:
      ustat_counter->usn_type = USTAT_TYPE_INT16;
      break;

    case STAT_TYPE_INT32:
      ustat_counter->usn_type = USTAT_TYPE_INT32;
      break;

    case STAT_TYPE_INT64:
      ustat_counter->usn_type = USTAT_TYPE_INT64;
      break;

    case STAT_TYPE_UINT8:
      ustat_counter->usn_type = USTAT_TYPE_UINT8;
      break;

    case STAT_TYPE_UINT16:
      ustat_counter->usn_type = USTAT_TYPE_UINT16;
      break;

    case STAT_TYPE_UINT32:
      ustat_counter->usn_type = USTAT_TYPE_UINT32;
      break;

    case STAT_TYPE_UINT64:
      ustat_counter->usn_type = USTAT_TYPE_UINT64;
      break;

    case STAT_TYPE_SIZE:
      ustat_counter->usn_type = USTAT_TYPE_SIZE;
      break;
    
    default:
      free((void*)ustat_counter->usn_name);
      free (ustat_counter);
      smg_error(logger, "Non-NULL statctx provided, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
      return NKV_ERR_WRONG_INPUT;
  }
  
  *statctx = nkv_register_application_counter(module_name, ustat_counter);
  if (*statctx == NULL) {
    smg_error(logger, "Getting NULL statctx, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    assert (0);
  }
  return NKV_SUCCESS; 
}

nkv_result nkv_unregister_stat_counter(uint64_t nkv_handle, void *statctx) {

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  if (!statctx) {
    smg_error(logger, "NULL statctx provided, aborting, given handle = %u, op = nkv_register_stat_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }
  
  ustat_named_t* ustat_counter = (ustat_named_t*) statctx;
  nkv_ustat_delete(ustat_counter);
  return NKV_SUCCESS;
}

nkv_result nkv_set_stat_counter(uint64_t nkv_handle, uint64_t value, void *statctx) {

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_set_stat_counter !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  if (!statctx) {
    smg_error(logger, "NULL statctx provided, aborting, given handle = %u, op = nkv_set_stat_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  ustat_named_t* ustat_counter = (ustat_named_t*) statctx;

  nkv_ustat_atomic_set_u64(statctx, ustat_counter, value);
  return NKV_SUCCESS;
}

nkv_result nkv_add_to_counter(uint64_t nkv_handle, uint64_t value, void *statctx) {

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_add_to_counter !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  if (!statctx) {
    smg_error(logger, "NULL statctx provided, aborting, given handle = %u, op = nkv_add_to_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  ustat_named_t* ustat_counter = (ustat_named_t*) statctx;

  nkv_ustat_atomic_add_u64(statctx, ustat_counter, value);
  return NKV_SUCCESS;
}

nkv_result nkv_sub_from_counter(uint64_t nkv_handle, uint64_t value, void *statctx) {

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_sub_from_counter !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  if (!statctx) {
    smg_error(logger, "NULL statctx provided, aborting, given handle = %u, op = nkv_sub_from_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  ustat_named_t* ustat_counter = (ustat_named_t*) statctx;

  nkv_ustat_atomic_sub_u64(statctx, ustat_counter, value);
  return NKV_SUCCESS;
}

nkv_result nkv_inc_to_counter(uint64_t nkv_handle, void *statctx) {

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_inc_to_counter !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  if (!statctx) {
    smg_error(logger, "NULL statctx provided, aborting, given handle = %u, op = nkv_inc_to_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  ustat_named_t* ustat_counter = (ustat_named_t*) statctx;
  nkv_ustat_atomic_inc_u64(statctx, ustat_counter);

  return NKV_SUCCESS;
}

nkv_result nkv_dec_to_counter(uint64_t nkv_handle, void *statctx) {

  if (nkv_handle != nkv_cnt_list->get_nkv_handle()) {
    smg_error(logger, "Wrong nkv handle provided, aborting, given handle = %u, op = nkv_dec_to_counter !!", nkv_handle);
    return NKV_ERR_HANDLE_INVALID;
  }

  if (!statctx) {
    smg_error(logger, "NULL statctx provided, aborting, given handle = %u, op = nkv_dec_to_counter !!", nkv_handle);
    return NKV_ERR_NULL_INPUT;
  }

  ustat_named_t* ustat_counter = (ustat_named_t*) statctx;

  nkv_ustat_atomic_dec_u64(statctx, ustat_counter);
  return NKV_SUCCESS;
}




