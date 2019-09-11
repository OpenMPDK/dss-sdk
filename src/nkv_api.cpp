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

#include "nkv_api.h"
#include <cstdlib>
#include <string>
#include "nkv_framework.h"
#include "cluster_map.h"
#include "auto_discovery.h"
#include "event_handler.h"
#include <pthread.h>
#include <sstream>
#include<queue>

thread_local int32_t core_running_app_thread = -1;
std::atomic<int32_t> nkv_app_thread_count(0);
int32_t core_to_pin = -1;
std::thread nkv_thread;
std::thread nkv_event_thread;
int32_t nkv_event_polling_interval_in_sec;
int32_t nkv_stat_thread_polling_interval;
int32_t nkv_stat_thread_needed;
int32_t nkv_dummy_path_stat;
int32_t nkv_event_handler;
std::condition_variable cv_global;
std::mutex mtx_global;
std::mutex mtx_stat;
std::string config_path;
std::queue<std::string> event_queue;


void event_handler_thread(std::string event_subscribe_channel, 
                          std::string mq_address,
                          int32_t nkv_event_polling_interval,
                          uint64_t nkv_handle) {
    int rc = pthread_setname_np(pthread_self(), "nkv_event_thread");
    if (rc != 0) {
        smg_error(logger, "Error on setting thread name on nkv_handle ");
    }
    std::vector<std::string> channels;
    boost::split(channels, event_subscribe_channel , boost::is_any_of(","));

    try{
        receive_events(event_queue, mq_address, channels, nkv_event_polling_interval, nkv_handle);
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
      nkv_cnt_list->collect_nkv_stat();
      smg_warn(logger, "Stopping the nkv_stat_thread, nkv_handle = %u", nkv_handle);
      nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
      break;
    }
    smg_alert(logger, "### NKV Library version in use = %s ###", NKV_VERSION_INFO);
    boost::property_tree::ptree pt;
    try {
      boost::property_tree::read_json(config_path, pt);
    }
    catch (std::exception& e) {
      smg_error(logger, "%s%s", "Error reading config file and building ptree! Error = ", e.what());
    }
    // Event Handler - Action Manager function
    if( nkv_event_handler ) {
      action_manager(event_queue);
    }

    try {
      nkv_dynamic_logging = pt.get<int>("nkv_enable_debugging", 0);
      nkv_stat_thread_polling_interval = pt.get<int>("nkv_stat_thread_polling_interval_in_sec", 10);
      if (nkv_dynamic_logging) 
        smg_alert(logger, "## NKV debugging is ON ##");
      else
        smg_alert(logger, "## NKV debugging is OFF ##");
    }
    catch (std::exception& e) {
      smg_error(logger, "%s%s", "Error reading config file property, Error = ", e.what());
    }
    smg_alert(logger, "Cache based listing = %d, number of cache shards = %d", listing_with_cached_keys, nkv_listing_cache_num_shards);
    nkv_cnt_list->collect_nkv_stat();
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

  if (is_kvs_initialized) {
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
  smg_alert(logger, "### NKV Library version = %s ###", NKV_VERSION_INFO);

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
    smg_error(logger, "%s%s", "Error reading config file and building ptree! Error = ", e.what()); 
    return NKV_ERR_CONFIG;
  }

  int32_t connect_fm = 0;
  int32_t nkv_transport = 0;
  int32_t min_container = 0;
  int32_t min_container_path = 0;
  int32_t nkv_container_path_qd = 32;
  int32_t nkv_app_thread_core = -1;
  try {
    connect_fm = pt.get<int>("contact_fm");
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
      path_stat_collection = pt.get<int>("nkv_need_path_stat", 1);
      nkv_dummy_path_stat = pt.get<int>("nkv_dummy_path_stat", 0);
      nkv_event_handler = pt.get<int>("nkv_event_handler", 0);
    }
    nkv_is_on_local_kv = pt.get<int>("nkv_is_on_local_kv");
    if (!nkv_is_on_local_kv) {
      path_stat_collection = 0;
    }
  }
  catch (std::exception& e) {
    smg_error(logger, "%s%s", "Error reading config file property, Error = ", e.what());
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

  if (!connect_fm && !nkv_is_on_local_kv) {
   if (nkv_cnt_list->parse_add_container(pt)) 
     return NKV_ERR_CONFIG;
  } else {
    if (nkv_is_on_local_kv) {
      if (nkv_cnt_list->add_local_container_and_path(host_name_ip, host_port, pt))
        return NKV_ERR_CONFIG;
    } else if (connect_fm) {
        // Add logic to contact FM and then give that json ptree to the parse_add_container call
        std::string fm_address  = pt.get<std::string>("fm_address");
        std::string fm_endpoint = pt.get<std::string>("fm_endpoint");
        const long time_out     = pt.get<long>("fm_connection_timeout");
        std::string url = fm_address + fm_endpoint;

        ClusterMap* cm = new ClusterMap(url);
        std::string response("");
        bool ret = cm->get_response(response, time_out);

        if (ret){
          if ( ! cm->get_clustermap(pt)) {
            nkv_cnt_list->parse_add_container(pt);
            smg_info(logger, "NKV API: Adding NKV remote mount paths ...");
            if (! add_remote_mount_path(pt) ){
              smg_error(logger, "Auto Discovery failed to retrieve the remote mount path");
              return NKV_ERR_CONFIG;
            }

          } else{
            smg_error(logger, "NKV: Falied to receive cluster map information ... ");
            return NKV_ERR_CONFIG;
          }
        } else{
          smg_error(logger, "NKV: Response not recived from Fabric Manager ...");
          return NKV_ERR_CONFIG;
        }

    } else {
      // Should not come here !!
    }
  }

  if (!nkv_is_on_local_kv && nkv_cnt_list->parse_add_path_mount_point(pt))
    return NKV_ERR_CONFIG;

  if (!nkv_cnt_list->verify_min_topology_exists(min_container, min_container_path))
    return NKV_ERR_CONFIG; 

  if (*nkv_handle != 0 && *instance_uuid != 0) {
    // All good, start initializing open_mpdk stuff
    kvs_init_options options;
    kvs_init_env_opts(&options);

    if ((NKV_TRANSPORT_LOCAL_KERNEL == nkv_transport) || (NKV_TRANSPORT_NVMF_TCP_KERNEL == nkv_transport)) {
      smg_info(logger,"**NKV transport is Over kernel**");
      options.aio.iocoremask = 0;
      options.memory.use_dpdk = 0;
      options.aio.queuedepth = nkv_container_path_qd;
      const char *emulconfigfile = "./kvssd_emul.conf";
      options.emul_config_file =  emulconfigfile;

    } else if (NKV_TRANSPORT_NVMF_TCP_SPDK == nkv_transport) {
      //To do, add spdk related kvs options 
    } else {

    }
    kvs_init_env(&options);
    smg_info(logger, "Setting environment for open mpdk is successful for app = %s", app_uuid);

    if(!nkv_cnt_list->open_container_paths(app_uuid))
      return NKV_ERR_COMMUNICATION;
  } else {
    smg_error(logger, "Either NKV handle or NKV instance handle generated is zero !");
    return NKV_ERR_INTERNAL; 
  }
  if (nkv_stat_thread_needed) {
    smg_info(logger, "Creating stat thread for nkv, app = %s", app_uuid);
    nkv_thread = std::thread(nkv_thread_func, *nkv_handle); 
  }

  // Add event_handler_thread
  if (! nkv_is_on_local_kv && nkv_event_handler) {
      smg_alert(logger,"Creating event handler thread for nkv");
      nkv_event_thread = std::thread(event_handler_thread, 
                                     pt.get<std::string>("event_subscribe_channel"),
                                     pt.get<std::string>("mq_address"),
                                     pt.get<int32_t>("nkv_event_polling_interval_in_sec", 10),
                                     *nkv_handle
                                    );
  }

  if (listing_with_cached_keys) {
    bool will_wait = nkv_listing_wait_till_cache_init ? true:false;
    auto start = std::chrono::steady_clock::now();
    nkv_cnt_list->wait_or_detach_thread (will_wait);
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start);
    smg_alert(logger, "NKV index cache building took %u seconds", (uint32_t)(((long double)elapsed.count())/1000000.0) );     
  }
  smg_info(logger, "NKV open is successful for app = %s", app_uuid);
  pt.clear();
  is_kvs_initialized = true;
  return NKV_SUCCESS;

}

/* nkv_close API definition */
nkv_result nkv_close (uint64_t nkv_handle, uint64_t instance_uuid) {

  smg_info(logger, "nkv_close invoked for nkv_handle = %u", nkv_handle);
  nkv_stopping = true;
  if (nkv_stat_thread_needed) {
    cv_global.notify_all();
    nkv_thread.join();
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
  
  kvs_exit_env();

  if (logger)
    smg_release_logger(logger);

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
                         int32_t which_op, nkv_postprocess_function* post_fn = NULL) {

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

  if (!value && which_op != NKV_DELETE_OP) {
    smg_error(logger, "value is NULL !!, nkv_handle = %u, op = %d", nkv_handle, which_op);
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
    stat = nkv_cnt_list->nkv_send_io(cnt_hash, cnt_path_hash, key, (void*)opt, value, which_op, post_fn); 

  } else {
    smg_error(logger, "Wrong input, nkv non-pass through mode is not supported yet, op = %d !", which_op);
    stat = NKV_ERR_WRONG_INPUT;
  }

done:
  nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
  return stat;
}

nkv_result nkv_store_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_store_option* opt, nkv_value* value) {

  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*) opt, value, NKV_STORE_OP);
  if (stat != NKV_SUCCESS)
    smg_error(logger, "NKV store operation failed for nkv_handle = %u, key = %s, key_length = %u, value_length = %u, code = %d", 
              nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value ? value->length:0, stat);
  else
    smg_info(logger, "NKV store operation is successful for nkv_handle = %u, key = %s, key_length = %u, value_length = %u", 
             nkv_handle, key ? (char*)key->key: "NULL", key ? key->length:0, value ? value->length:0);
  return stat;
}

nkv_result nkv_retrieve_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_retrieve_option* opt, nkv_value* value) {

  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, (void*) opt, value, NKV_RETRIEVE_OP);
  if (stat != NKV_SUCCESS) {
    if (stat != NKV_ERR_KEY_NOT_EXIST) {
      smg_error(logger, "NKV retrieve operation failed for nkv_handle = %u, key = %s, key_length = %u, code = %d", nkv_handle, 
                 key ? (char*)key->key: "NULL", key ? key->length:0, stat);
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

nkv_result nkv_delete_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key) {

  nkv_result stat = nkv_send_kvp(nkv_handle, ioctx, key, NULL, NULL, NKV_DELETE_OP);
  if (stat != NKV_SUCCESS)
    smg_error(logger, "NKV delete operation failed for nkv_handle = %u, key = %s, key_length = %u, code = %d", nkv_handle, 
              key ? (char*)key->key: "NULL", key ? key->length:0, stat);
  else
    smg_info(logger, "NKV delete operation is successful for nkv_handle = %u, key = %s, key_length = %u", nkv_handle, 
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

  if (ioctx->is_pass_through) {
    uint64_t cnt_hash = ioctx->container_hash;
    uint64_t cnt_path_hash = ioctx->network_path_hash;
    stat = nkv_cnt_list->nkv_list_keys(cnt_hash, cnt_path_hash, max_keys, keys, *iter_context, prefix, delimiter);

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
      if (stat == NKV_SUCCESS) {
        smg_warn(logger, "NKV path mount = %s, path capacity = %lld Bytes, path usage = %lld Bytes, path util percentage = %f",
                p_stat->path_mount_point, (long long)p_stat->path_storage_capacity_in_bytes, (long long)p_stat->path_storage_usage_in_bytes, 
                p_stat->path_storage_util_percentage);
      }
      
    } else {
      smg_error(logger, "Not able to get NKV path mount point, handle = %u, cnt_hash = %u, cnt_path_hash = %u", 
               nkv_handle, cnt_hash, cnt_path_hash);
    }

  } else {
    smg_warn(logger, "nkv remote stat collection is not supported yet or dummy path collection enabled, op = nkv_get_path_stat !");
    std::string p_mount = "/dev/nvme";
    p_mount.copy(p_stat->path_mount_point, p_mount.length());
    p_stat->path_storage_capacity_in_bytes = 0;
    p_stat->path_storage_usage_in_bytes = 0;
    p_stat->path_storage_util_percentage = 0.0;

    stat = NKV_SUCCESS;
  }

done:
  nkv_pending_calls.fetch_sub(1, std::memory_order_relaxed);
  return stat;

}
