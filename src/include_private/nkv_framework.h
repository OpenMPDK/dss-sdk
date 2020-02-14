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

#ifndef NKV_FRAMEWORK_H
#define NKV_FRAMEWORK_H

/*#include <unordered_map>
#include <unordered_set>
#include <set>
#include <list>
#include <string>
#include <atomic>
#include <mutex>
#include <functional>*/

#include <thread>
#include "kvs_api.h"
#include "nkv_utils.h"
#include <condition_variable>
#include <pthread.h>

  #define SLEEP_FOR_MICRO_SEC 100
  #define NKV_STORE_OP      0
  #define NKV_RETRIEVE_OP   1
  #define NKV_DELETE_OP     2
  #define NKV_LIST_OP       3
  #define NKV_LOCK_OP       3
  #define NKV_UNLOCK_OP     4

  extern std::atomic<bool> nkv_stopping;
  extern std::atomic<uint64_t> nkv_pending_calls;
  extern std::atomic<bool> is_kvs_initialized;
  extern std::atomic<uint32_t> nkv_async_path_max_qd;
  extern std::atomic<uint64_t> nkv_num_async_submission;
  extern std::atomic<uint64_t> nkv_num_async_completion;
  extern std::atomic<uint64_t> nkv_num_read_cache_miss;
  
  //extern c_smglogger* logger;
  class NKVContainerList;
  extern NKVContainerList* nkv_cnt_list;
  extern int32_t core_pinning_required;
  extern int32_t queue_depth_monitor_required;
  extern int32_t queue_depth_threshold;
  extern int32_t listing_with_cached_keys;
  extern std::string iter_prefix;
  extern std::string transient_prefix;
  extern int32_t num_path_per_container_to_iterate;
  extern int32_t nkv_is_on_local_kv;
  extern std::string key_default_delimiter;
  extern int32_t MAX_DIR_ENTRIES;
  extern int32_t nkv_listing_wait_till_cache_init;
  extern int32_t nkv_listing_need_cache_stat;
  extern int32_t nkv_listing_cache_num_shards;
  extern int32_t nkv_dynamic_logging;
  extern int32_t path_stat_collection;
  extern int32_t nkv_use_read_cache;
  extern int32_t nkv_read_cache_size;
  extern int32_t nkv_read_cache_shard_size;
  extern int32_t nkv_data_cache_size_threshold;
  extern int32_t nkv_use_data_cache;
  extern int32_t nkv_remote_listing;
  

  typedef struct iterator_info {
    std::unordered_set<uint64_t> visited_path;
    std::unordered_set<std::string> excess_keys;
    std::unordered_set<std::string> dir_entries_added;
    kvs_iterator_handle iter_handle;
    kvs_iterator_list iter_list;
    #ifndef SAMSUNG_API
      kvs_option_iterator g_iter_mode;
    #endif
    uint64_t network_path_hash_iterating;
    int32_t all_done;
    //For cached based listing
    std::unordered_set<std::string>* dup_chached_key_set;
    //std::unordered_set<std::string>::const_iterator cached_key_iter;
    //std::list<std::string>::const_iterator cached_key_iter;
    //std::vector<std::string>::const_iterator cached_key_iter;
    std::set<std::string>::const_iterator cached_key_iter;
    std::size_t key_prefix_hash;
    std::string key_to_start_iter;
    iterator_info():network_path_hash_iterating(0), all_done(0), dup_chached_key_set(NULL) {
      excess_keys.clear();
      dir_entries_added.clear();
      visited_path.clear();
      key_prefix_hash = 0;
    }

    ~iterator_info() {
      if (dup_chached_key_set) {
        delete dup_chached_key_set;
        dup_chached_key_set = NULL;
      }
    }
  } iterator_info;

  struct nkv_value_wrapper {
    void *value;
    uint64_t length;
    uint64_t actual_length;
    nkv_value_wrapper(void* pval, uint64_t plength, uint64_t pactual_length) : value(pval),
                                                                               length(plength),
                                                                               actual_length(pactual_length) {}

    ~nkv_value_wrapper() {
      if (value) {
        free (value);
        value = NULL;
      }
      length = 0;
      actual_length = 0;
    }

    nkv_value_wrapper(const nkv_value_wrapper& other) {
      value = malloc (other.length);
      memcpy(value, other.value, other.length);
      length = other.length;
      actual_length = other.actual_length;
  }


    nkv_value_wrapper (nkv_value_wrapper&& other) : value (other.value), length(other.length), actual_length(other.actual_length) {
      other.value = NULL;
      other.length = 0;
      other.actual_length = 0;
    }

    nkv_value_wrapper& operator=(nkv_value_wrapper&& other) {
      if (this != &other) {
        value = other.value;
        length = other.length;
        actual_length = other.actual_length;
        other.value = NULL;
        other.length = 0;
        other.actual_length = 0;
      }
      return *this;
    }


  };


  //Aio call back function
  void nkv_aio_completion (nkv_aio_construct* ops, int32_t num_op);

  class NKVTargetPath {
    kvs_device_handle path_handle;
    #ifdef SAMSUNG_API
      kvs_container_context path_cont_ctx;
      kvs_container_handle path_cont_handle;
    #else
      kvs_key_space_name   path_ks_name;
      kvs_key_space_handle path_ks_handle;
    #endif
    std::thread path_thread_iter;
    std::thread path_thread_cache;
  public:
    std::string path_cont_name;
    std::string dev_path;
    std::string path_ip;
    int32_t path_port;
    int32_t addr_family;
    int32_t path_speed;
    std::atomic<int32_t> path_status;
    int32_t path_numa_aligned;
    int32_t path_type;
    int32_t path_id;
    uint64_t path_hash;
    int32_t core_to_pin;
    int32_t path_numa_node;
    std::atomic<uint32_t> nkv_async_path_cur_qd;
    std::unordered_map<std::size_t, std::set<std::string> > *listing_keys; 
    std::atomic<uint32_t> nkv_outstanding_iter_on_path;
    std::atomic<uint32_t> nkv_path_stopping;
    std::atomic<uint64_t> nkv_num_key_prefixes;
    std::atomic<uint32_t> nkv_num_keys;
    std::mutex cache_mtx;
    pthread_rwlock_t* cache_rw_lock_list;
    std::mutex iter_mtx;
    std::vector<std::string> path_vec;
    std::condition_variable cv_path;
    nkv_lruCache<std::string, nkv_value_wrapper>* cnt_cache;

  public:
    NKVTargetPath (uint64_t p_hash, int32_t p_id, std::string& p_ip, int32_t port, int32_t fam, int32_t p_speed, int32_t p_stat, 
                  int32_t numa_aligned, int32_t p_type):
                  path_ip(p_ip), path_port(port), addr_family(fam), path_speed(p_speed), path_status(p_stat),
                  path_numa_aligned(numa_aligned), path_type(p_type), path_id(p_id), path_hash(p_hash) {

      nkv_async_path_cur_qd = 0;
      core_to_pin = -1;
      path_numa_node = -1;

      nkv_outstanding_iter_on_path = 0;
      nkv_path_stopping = 0;
      nkv_num_key_prefixes = 0;
      nkv_num_keys = 0;
      cache_rw_lock_list = new pthread_rwlock_t[nkv_listing_cache_num_shards];
      
      for (int iter = 0; iter < nkv_listing_cache_num_shards; iter++) {
        pthread_rwlock_init(&cache_rw_lock_list[iter], NULL);
      }
      listing_keys = new std::unordered_map<std::size_t, std::set<std::string> > [nkv_listing_cache_num_shards];
      cnt_cache = new nkv_lruCache<std::string, nkv_value_wrapper> [nkv_read_cache_shard_size](nkv_read_cache_size);
    }

    ~NKVTargetPath() {
      if (listing_with_cached_keys) {
        nkv_path_stopping.fetch_add(1, std::memory_order_relaxed);
        wait_for_thread_completion();
      }
      kvs_result ret;
      #ifdef SAMSUNG_API
        ret = kvs_close_container(path_cont_handle);
        assert(ret == KVS_SUCCESS);
      #else
        kvs_close_key_space(path_ks_handle);
        kvs_delete_key_space(path_handle, &path_ks_name);
      #endif

      ret = kvs_close_device(path_handle);
      assert(ret == KVS_SUCCESS);
      for (int iter = 0; iter < nkv_listing_cache_num_shards; iter++) {
        pthread_rwlock_destroy(&cache_rw_lock_list[iter]);
      }
      delete[] cache_rw_lock_list;
      delete[] listing_keys;
      delete[] cnt_cache;
      smg_info(logger, "Cleanup successful for path = %s", dev_path.c_str());
    }

    void add_device_path (std::string& p_dev_path) {
      dev_path = p_dev_path;
    }
    
    int32_t get_target_path_status() {
      return path_status.load(std::memory_order_relaxed);
    }

    bool open_path(const std::string& app_name) {
      kvs_result ret = kvs_open_device((char*)dev_path.c_str(), &path_handle);
      if(ret != KVS_SUCCESS) {
        #ifdef SAMSUNG_API
          smg_error(logger, "Path open failed, path = %s, error = %s", dev_path.c_str(), kvs_errstr(ret));
        #else
          smg_error(logger, "Path open failed, path = %s, error = %d", dev_path.c_str(), ret);
        #endif
        return false;
      }
      smg_info(logger,"** Path open successful for path = %s **", dev_path.c_str());
      //For now device supports single container only..
      path_cont_name = "nkv_" + app_name;
      #ifdef SAMSUNG_API 
        ret = kvs_create_container(path_handle, path_cont_name.c_str(), 0, &path_cont_ctx);      
        if (ret != KVS_SUCCESS) {
          smg_error(logger, "Path container creation failed, path = %s, container name = %s, error = %s", 
                   dev_path.c_str(), path_cont_name.c_str(), kvs_errstr(ret));
          return false;
        }
      #else
        kvs_option_key_space option = {KVS_KEY_ORDER_ASCEND};
        path_ks_name.name = (char*)path_cont_name.c_str();
        path_ks_name.name_len = path_cont_name.length();
  
        kvs_create_key_space(path_handle, &path_ks_name, 0, option);

      #endif
      smg_info(logger,"** Path container creation successful for path = %s, container name = %s **", 
              dev_path.c_str(), path_cont_name.c_str());

      #ifdef SAMSUNG_API
        ret = kvs_open_container(path_handle, path_cont_name.c_str(), &path_cont_handle);
        if(ret != KVS_SUCCESS) {
          smg_error(logger, "Path open container failed, path = %s, container name = %s, error = %s",
                   dev_path.c_str(), path_cont_name.c_str(), kvs_errstr(ret));
          return false;
        }
      #else
        ret = kvs_open_key_space(path_handle, (char*)path_cont_name.c_str(), &path_ks_handle);
        if(ret != KVS_SUCCESS) {
          smg_error(logger, "Path open key_space failed, path = %s, container name = %s, error = %d",
                   dev_path.c_str(), path_cont_name.c_str(), ret);
          return false;
        }

      #endif
      smg_info(logger,"** Path open container successful for path = %s, container name = %s **",
              dev_path.c_str(), path_cont_name.c_str());


      return true;
    }
   
    nkv_result map_kvs_err_code_to_nkv_err_code (int32_t kvs_code);
    nkv_result do_store_io_to_path (const nkv_key* key, const nkv_store_option* opt, nkv_value* value, nkv_postprocess_function* post_fn); 
    nkv_result do_retrieve_io_from_path (const nkv_key* key, const nkv_retrieve_option* opt, nkv_value* value, nkv_postprocess_function* post_fn); 
    nkv_result do_delete_io_from_path (const nkv_key* key, nkv_postprocess_function* post_fn); 
    nkv_result do_lock_io_from_path (const nkv_key* key,
		const nkv_lock_option *opt, nkv_postprocess_function* post_fn); 
    nkv_result do_unlock_io_from_path (const nkv_key* key,
		const nkv_unlock_option *opt, nkv_postprocess_function* post_fn); 
    nkv_result do_list_keys_from_path(uint32_t* num_keys_iterted, iterator_info*& iter_info, uint32_t* max_keys, nkv_key* keys, const char* prefix,
                                     const char* delimiter, const char* start_after); 
    nkv_result find_keys_from_path(uint32_t* max_keys, nkv_key* keys, iterator_info*& iter_info, uint32_t* num_keys_iterted, const char* prefix,
                                      const char* delimiter); 
    void filter_and_populate_keys_from_path(uint32_t* max_keys, nkv_key* keys, char* disk_key, uint32_t key_size, uint32_t* num_keys_iterted,
                                            const char* prefix, const char* delimiter, iterator_info*& iter_info, bool cached_keys = false);
    nkv_result perform_remote_listing(const char* key_prefix_iter, const char* start_after, uint32_t* max_keys, 
                                      nkv_key* keys, iterator_info*& iter_info, uint32_t* num_keys_iterted);
    int32_t parse_delimiter_entries(std::string& key, const char* delimiter, std::vector<std::string>& dirs,
                                    std::vector<std::string>& prefixes, std::string& f_name);
    void populate_iter_cache(std::string& key_prefix, std::string& key_prefix_val, bool need_lock = true);
    bool remove_from_iter_cache(std::string& key_prefix, std::string& key_prefix_val, bool root_prefix = false);
    void nkv_path_thread_func(int32_t what_work);
    void nkv_path_thread_init(int32_t what_work);

    void start_thread(int32_t what_work = 0) {
      path_thread_iter = std::thread(&NKVTargetPath::nkv_path_thread_func, this, what_work);
      path_thread_cache = std::thread(&NKVTargetPath::nkv_path_thread_init, this, what_work);
      
    }
    void wait_for_thread_completion() {
      try {
        if (path_thread_iter.joinable()) {
          path_thread_iter.join();
        }
        nkv_path_stopping.fetch_add(1, std::memory_order_relaxed);
        if (path_thread_cache.joinable()) {
          cv_path.notify_one();
          path_thread_cache.join();
        }

      } catch(...) {
        smg_warn(logger,"Exception during pthread_join() on dev_path = %s, ip = %s, may be thread is not running ?",
                 dev_path.c_str(), path_ip.c_str());
      }
    }
    void detach_thread_completion() {
      path_thread_iter.detach();
      path_thread_cache.detach();
    }
    int32_t initialize_iter_cache (iterator_info*& iter_info);

  };

  class NKVTarget {
  protected:
    //<ip_address hash, path> pair
    std::unordered_map<uint64_t, NKVTargetPath*> pathMap;
  public:
    //Incremental Id
    uint32_t t_id;
    //Could be Hash of target_node_name:target_container_name if not coming from FM
    std::string target_uuid;
    std::string target_node_name;
    std::string target_container_name;
    int32_t ss_status;
    float ss_space_avail_percent;
    //Generated from target_uuid and passed to the app
    uint64_t target_hash;
    NKVTarget(uint32_t p_id, const std::string& puuid, const std::string& tgtNodeName, const std::string& tgtCntName, uint64_t t_hash) : 
             t_id(p_id), target_uuid(puuid), target_node_name(tgtNodeName), target_container_name(tgtCntName), target_hash(t_hash) {}

    ~NKVTarget() {
      /*if ( ! ss_status ) {
        for (auto m_iter = pathMap.begin(); m_iter != pathMap.end(); m_iter++) {
          delete(m_iter->second);
        }
        pathMap.clear();    
      }*/
      for (auto m_iter = pathMap.begin(); m_iter != pathMap.end(); m_iter++) {
        delete(m_iter->second);
      }
      pathMap.clear();  
    }

    std::unordered_map<uint64_t, NKVTargetPath*>& get_path_map() {
        return pathMap;
    }

    void set_ss_status (int32_t p_status) {
      ss_status = p_status;
    }

    int32_t get_ss_status () {
      return ss_status;
    } 
    
    void set_space_avail_percent (float p_space) {
      ss_space_avail_percent = p_space;
    } 

    nkv_result  send_io_to_path(uint64_t container_path_hash, const nkv_key* key, 
                                void* opt, nkv_value* value, int32_t which_op, nkv_postprocess_function* post_fn) {
      nkv_result stat = NKV_SUCCESS;
      auto p_iter = pathMap.find(container_path_hash);
      if (p_iter == pathMap.end()) {
        smg_error(logger,"No Container path found for hash = %u", container_path_hash);
        return NKV_ERR_NO_CNT_PATH_FOUND;
      }
      NKVTargetPath* one_p = p_iter->second;
      if (one_p) {
        if (!post_fn) {
          smg_info(logger, "Sending IO to dev mount = %s, container name = %s, target node = %s, path ip = %s, path port = %d, key = %s, key_length = %u, op = %d, cur_qd = %u",
                  one_p->dev_path.c_str(), target_container_name.c_str(), target_node_name.c_str(), one_p->path_ip.c_str(), 
                  one_p->path_port, (char*)key->key, key->length, which_op, one_p->nkv_async_path_cur_qd.load());
        } else {
          smg_info(logger, "Sending IO to dev mount = %s, container name = %s, target node = %s, path ip = %s, path port = %d, key = %s, key_length = %u, op = %d, cur_path_qd = %u, max_path_qd = %u",
                  one_p->dev_path.c_str(), target_container_name.c_str(), target_node_name.c_str(), one_p->path_ip.c_str(),
                  one_p->path_port, (char*)key->key, key->length, which_op, one_p->nkv_async_path_cur_qd.load(), nkv_async_path_max_qd.load());
          /*smg_warn(logger, "Sending IO to dev mount = %s, key = %s, op = %d, cur_qd = %u, cb_address = 0x%x, post_fn = 0x%x, pvt_1 = 0x%x",
                  one_p->dev_path.c_str(), (char*)key->key, which_op, one_p->nkv_async_path_cur_qd.load(), post_fn->nkv_aio_cb, post_fn, post_fn->private_data_1);*/
        }

        switch(which_op) {
          case NKV_STORE_OP:
            stat = one_p->do_store_io_to_path(key, (const nkv_store_option*)opt, value, post_fn);
            break;
          case NKV_RETRIEVE_OP:
            stat = one_p->do_retrieve_io_from_path(key, (const nkv_retrieve_option*)opt, value, post_fn);
            break;
          case NKV_DELETE_OP:
            stat = one_p->do_delete_io_from_path(key, post_fn);
            break;
          case NKV_LOCK_OP:
            stat = one_p->do_lock_io_from_path(key, \
						(const nkv_lock_option*)opt, post_fn);
            break;
          case NKV_UNLOCK_OP:
            stat = one_p->do_unlock_io_from_path(key, \
						(const nkv_unlock_option*)opt, post_fn);
            break;
          default:
            smg_error(logger, "Unknown op, op = %d", which_op);
            return NKV_ERR_WRONG_INPUT;
        }
   
      } else {
        smg_error(logger, "NULL Container path found for hash = %u!!", container_path_hash);
        return NKV_ERR_NO_CNT_PATH_FOUND;
      }
      /*if (stat == NKV_SUCCESS && post_fn)
        nkv_num_async_submission.fetch_add(1, std::memory_order_relaxed);*/
      return stat;

    }
    
    nkv_result get_path_mount_point (uint64_t container_path_hash, std::string& p_mount) {

      nkv_result stat = NKV_SUCCESS;
      auto p_iter = pathMap.find(container_path_hash);
      if (p_iter == pathMap.end()) {
        smg_error(logger,"No Container path found for hash = %u", container_path_hash);
        return NKV_ERR_NO_CNT_PATH_FOUND;
      }
      NKVTargetPath* one_p = p_iter->second;
      if (one_p) {
        p_mount = one_p->dev_path;
      } else {
        smg_error(logger, "NULL Container path found for hash = %u!!", container_path_hash);
        return NKV_ERR_NO_CNT_PATH_FOUND;
      }
      return stat;
    }

    nkv_result list_keys_from_path (uint64_t container_path_hash, uint32_t* max_keys, nkv_key* keys, void*& iter_context, const char* prefix,
                                    const char* delimiter, const char* start_after) {

      nkv_result stat = NKV_SUCCESS;
      uint64_t current_path_hash = 0;
      uint32_t num_keys_iterted = 0;

      iterator_info* iter_info = NULL;
      if (iter_context) {
        iter_info = (iterator_info*) iter_context;
        current_path_hash = iter_info->network_path_hash_iterating; 
        if (iter_info->excess_keys.size() != 0) {
          smg_info(logger, "Got some excess keys from last call, adding to output buffer, number of keys = %u, container name = %s, target node = %s",
                   iter_info->excess_keys.size(), target_container_name.c_str(), target_node_name.c_str()); 
          auto k_iter = iter_info->excess_keys.begin();
          for(; k_iter != iter_info->excess_keys.end(); k_iter++) {
            (*k_iter).copy((char*)keys[num_keys_iterted].key, (*k_iter).length());
            keys[num_keys_iterted].length = (*k_iter).length();
            num_keys_iterted++;
            if (num_keys_iterted >= *max_keys) {
              smg_warn(logger, "Output buffer full, returning the call, max_keys = %u, key_inserted = %u, container name = %s, target node = %s",
                       *max_keys, num_keys_iterted, target_container_name.c_str(), target_node_name.c_str());
              break;
            }
          }
          iter_info->excess_keys.erase(iter_info->excess_keys.begin(), k_iter);
        }
      } else {
        iter_info = new iterator_info();
        assert(iter_info != NULL);
        /*iter_info->network_path_hash_iterating = 0;
        iter_info->all_done = 0;
        iter_info->excess_keys.clear();
        iter_info->dir_entries_added.clear();
        iter_info->visited_path.clear();*/
        smg_info(logger, "Created an iterator context for NKV iteration, container name = %s, target node = %s, prefix = %s, delimiter = %s",
                 target_container_name.c_str(), target_node_name.c_str(), prefix, delimiter);

        iter_context = (void*) iter_info;
      }

      if ((current_path_hash != 0) && (num_keys_iterted < *max_keys)) {
        smg_info(logger, "Start iterating on iter_context path = %u, container name = %s, target node = %s", 
                 current_path_hash, target_container_name.c_str(), target_node_name.c_str());
        auto p_iter = pathMap.find(current_path_hash);
        if (p_iter == pathMap.end()) {
          smg_error(logger,"No Container path found for hash = %u, container name = %s, target node = %s", 
                    current_path_hash, target_container_name.c_str(), target_node_name.c_str());
          return NKV_ERR_NO_CNT_PATH_FOUND;
        }

        NKVTargetPath* one_p = p_iter->second;
        assert(one_p != NULL);
        stat = one_p->do_list_keys_from_path(&num_keys_iterted, iter_info, max_keys, keys, prefix, delimiter, start_after);
        if (stat != NKV_SUCCESS) {
          *max_keys = num_keys_iterted;
          if (stat != NKV_ITER_MORE_KEYS) {
            if (iter_info) {
              delete(iter_info);
              iter_info = NULL;
            }
            smg_error(logger,"Path iteration failed on dev mount = %s, path ip = %s , error = %x",
                     one_p->dev_path.c_str(), one_p->path_ip.c_str(), stat);
            return stat;
          } else {
            smg_warn(logger,"Out buffer exhausted on dev mount = %s, path ip = %s , error = %x",
                     one_p->dev_path.c_str(), one_p->path_ip.c_str(), stat);
            return stat;
          } 
        }
        
      }

      if ((container_path_hash != 0) && (num_keys_iterted < *max_keys)) {
        smg_info(logger, "Start iterating on container path hash= %u, container name = %s, target node = %s", 
                 container_path_hash, target_container_name.c_str(), target_node_name.c_str());
        auto p_iter = pathMap.find(container_path_hash);
        if (p_iter == pathMap.end()) {
          smg_error(logger,"No Container path found for hash = %u, container name = %s, target node = %s", 
                    container_path_hash, target_container_name.c_str(), target_node_name.c_str());
          return NKV_ERR_NO_CNT_PATH_FOUND;
        }
        
        NKVTargetPath* one_p = p_iter->second; 
        assert(one_p != NULL);
        stat = one_p->do_list_keys_from_path(&num_keys_iterted, iter_info, max_keys, keys, prefix, delimiter, start_after);
        if (stat != NKV_SUCCESS) {
          *max_keys = num_keys_iterted;
          if (stat != NKV_ITER_MORE_KEYS) {
            smg_error(logger,"Path iteration failed dev mount = %s, path ip = %s , error = %x",
                      one_p->dev_path.c_str(), one_p->path_ip.c_str(), stat);
            if (iter_info) {
              delete(iter_info);
              iter_info = NULL;
            }
            return stat;
          } else {
            smg_warn(logger,"Out buffer exhausted on dev mount = %s, path ip = %s , error = %x",
                    one_p->dev_path.c_str(), one_p->path_ip.c_str(), stat);
            return stat;
          }
        }
        iter_info->all_done = 1;
      }

      if ((num_path_per_container_to_iterate > 0) && (num_path_per_container_to_iterate == (int32_t)iter_info->visited_path.size())) {
        smg_info(logger, "Finished iterating the number of path (%d) provided via config option (%d), exiting..",
                  iter_info->visited_path.size(), num_path_per_container_to_iterate);
        iter_info->all_done = 1;
      }

      if ((num_keys_iterted < *max_keys) && (0 == iter_info->all_done)) {
        smg_info(logger, "No Path provided, listing for all path(s)/device(s), container name = %s, target node = %s",
                 target_container_name.c_str(), target_node_name.c_str());

        for (auto p_iter = pathMap.begin(); p_iter != pathMap.end(); p_iter++) {
          NKVTargetPath* one_p = p_iter->second;
          assert(one_p != NULL);
          smg_info(logger,"Start Iterating over path hash = %u, dev mount = %s, path ip = %s", one_p->path_hash,
                   one_p->dev_path.c_str(), one_p->path_ip.c_str());
          stat = one_p->do_list_keys_from_path(&num_keys_iterted, iter_info, max_keys, keys, prefix, delimiter, start_after);
          
          if (stat != NKV_SUCCESS) {
            *max_keys = num_keys_iterted;
            if (stat != NKV_ITER_MORE_KEYS) {
              if (iter_info) {
                delete(iter_info);
                iter_info = NULL;
              }
              smg_error(logger,"Path iteration failed on dev mount = %s, path ip = %s , error = %x",
                        one_p->dev_path.c_str(), one_p->path_ip.c_str(), stat);

              return stat;
            } else {
              smg_warn(logger,"Out buffer exhausted on dev mount = %s, path ip = %s , error = %x",
                       one_p->dev_path.c_str(), one_p->path_ip.c_str(), stat);
              break;
            }
          }
          if (num_keys_iterted >= *max_keys) {
            smg_warn(logger, "Output buffer full, returning the call from path iteration, max_keys = %u, key_inserted = %u, container name = %s, target node = %s",
                     *max_keys, num_keys_iterted, target_container_name.c_str(), target_node_name.c_str());
            break;
          }

          if ((num_path_per_container_to_iterate > 0) && (num_path_per_container_to_iterate == (int32_t)iter_info->visited_path.size())) {
            smg_info(logger, "Finished iterating the number of path (%d) provided via config option (%d), exiting..",
                      iter_info->visited_path.size(), num_path_per_container_to_iterate);
            iter_info->all_done = 1;  
          }
          
        }
      }
      *max_keys = num_keys_iterted;
      if ((iter_info->visited_path.size() == pathMap.size()) || (iter_info->all_done)) {
        smg_info(logger, "Iteration is successfully completed for container name = %s, target node = %s, completed_paths = %d, all_done = %d, prefix = %s, delimiter = %s",
                  target_container_name.c_str(), target_node_name.c_str(), iter_info->visited_path.size(), iter_info->all_done, prefix, delimiter);
        if (iter_info) {
          delete(iter_info);
          iter_info = NULL;
        }
        stat = NKV_SUCCESS;
      } else {
        stat = NKV_ITER_MORE_KEYS;  
      }
      
      return stat;
    }    

    void add_network_path(NKVTargetPath* path, uint64_t ip_hash) {
      if (path) {
        auto cnt_p_iter = pathMap.find(ip_hash);
        assert (cnt_p_iter == pathMap.end());
        pathMap[ip_hash] = path;
        smg_info (logger, "Network path  added, ip hash = %u, target_container_name = %s, total path so far = %d", 
                 ip_hash, target_container_name.c_str(), pathMap.size()); 
      }
    }

    void collect_nkv_path_stat() {

      for (auto p_iter = pathMap.begin(); p_iter != pathMap.end(); p_iter++) {
        NKVTargetPath* one_path = p_iter->second;
        if (one_path) {
          smg_debug(logger, "collecting stat for path with address = %s , dev_path = %s, port = %d, status = %d",
                   one_path->path_ip.c_str(), one_path->dev_path.c_str(), one_path->path_port, (one_path->path_status).load(std::memory_order_relaxed));
          nkv_path_stat p_stat = {0};
          if (!nkv_is_on_local_kv) {
            smg_alert(logger, "Path = %s, Address = %s, Cached keys = %lld, Indexes = %lld, Capacity = %lld B, Used = %lld B, Percent used = %3.2f",
                       one_path->dev_path.c_str(), one_path->path_ip.c_str(), (long long)one_path->nkv_num_keys.load(), (long long)one_path->nkv_num_key_prefixes.load(),
                       0, 0, ss_space_avail_percent);
            continue;
          }
          nkv_result stat = nkv_get_path_stat_util(one_path->dev_path, &p_stat); 
          if (stat == NKV_SUCCESS) {

            smg_alert(logger, "Path = %s, Address = %s, Cached keys = %lld, Indexes = %lld, Capacity = %lld B, Used = %lld B, Percent used = %3.2f",
                       one_path->dev_path.c_str(), one_path->path_ip.c_str(), (long long)one_path->nkv_num_keys.load(), (long long)one_path->nkv_num_key_prefixes.load(),
                       (long long)p_stat.path_storage_capacity_in_bytes, (long long)p_stat.path_storage_usage_in_bytes, p_stat.path_storage_util_percentage); 

          } else {
            smg_alert(logger, "Path = %s, Address = %s, Cache keys = %lld, Indexes = %lld, path stat collection failed !!",
                      one_path->dev_path.c_str(), one_path->path_ip.c_str(), (long long)one_path->nkv_num_keys.load(), (long long)one_path->nkv_num_key_prefixes.load());
          }
        } else {
          smg_error(logger, "NULL path found !!");
          assert(0);
        }
      }

    }

    void wait_or_detach_path_thread(bool will_wait) {

      for (auto p_iter = pathMap.begin(); p_iter != pathMap.end(); p_iter++) {
        NKVTargetPath* one_path = p_iter->second;
        if (one_path) {
          smg_info(logger, "wait_or_detach_path_thread for path with address = %s , dev_path = %s, port = %d, status = %d",
                   one_path->path_ip.c_str(), one_path->dev_path.c_str(), one_path->path_port, (one_path->path_status).load(std::memory_order_relaxed));

          if (will_wait) {
            one_path->wait_for_thread_completion();          
          } else {
            one_path->detach_thread_completion();
          }
 
        } else {
          smg_error(logger, "NULL path found !!");
          assert(0);
        }
      }

    }

    /* Function Name: verify_min_path_exists
     * Input Args   : <int32_t> = Minimum paths required for a subsystem to
     *                statisfy min path topology.
     * Return       : <bool> = 1/0 = Min Path topology statisfied/ Failed
     * Description  : Check if minimum number of paths available to satisfy 
     *                min topology subsystem paths requirement.
     */
    bool verify_min_path_exists(int32_t min_path_required) {
      if (pathMap.size() < (uint32_t) min_path_required) {
        smg_error(logger, "Not enough container path, minimum required = %d, available = %d",
                 min_path_required, pathMap.size());
        return false;
      }
      bool subsystem_status = true;
      smg_debug(logger, "Minimum subsystem paths required = %d, available = %d",
                 min_path_required, pathMap.size());

      int32_t device_path_count = 0;
      for (auto p_iter = pathMap.begin(); p_iter != pathMap.end(); p_iter++) {
        NKVTargetPath* one_path = p_iter->second;
        if (one_path) {
          smg_debug(logger, "Inspecting path with address = %s , dev_path = %s, port = %d, status = %d",
                   one_path->path_ip.c_str(), one_path->dev_path.c_str(), one_path->path_port, (one_path->path_status).load(std::memory_order_relaxed));
          // Check path_status as well
          if (! one_path->path_ip.empty() && ! one_path->dev_path.empty()) {
            device_path_count++;
          } else {
            smg_error(logger, "No IP or mount point (for TCP over KDD only) provided !");
          }
           
        } else {
          smg_error(logger, "NULL path found !!");
        }
      }
      uint32_t total_qd = nkv_async_path_max_qd.load();
      nkv_async_path_max_qd = (total_qd/pathMap.size());

      smg_alert(logger, "Max QD per path = %u", nkv_async_path_max_qd.load());

      if ( device_path_count < (uint32_t) min_path_required ) {
        subsystem_status = false;
      }
      return subsystem_status;

    }

    int32_t add_mount_point_to_path(std::string ip, std::string m_point, int32_t numa, int32_t core) {
     
      uint64_t ip_hash = std::hash<std::string>{}(ip);

      auto p_iter = pathMap.find(ip_hash);
      if (p_iter == pathMap.end()) {
        smg_error(logger,"No path found for ip = %s, node = %s, container = %s", ip.c_str(), target_node_name.c_str(),
                 target_container_name.c_str());
        return 1; 
      }
      NKVTargetPath* one_path = p_iter->second;
      if (!one_path) {
        smg_error(logger,"NULL path found for ip = %s, node = %s, container = %s", ip.c_str(), target_node_name.c_str(),
                 target_container_name.c_str());        
        return 1;
      }
      one_path->add_device_path(m_point);
      one_path->path_numa_node = numa;
      one_path->core_to_pin = core;
      return 0;
    }

    /* Function Name: open_paths
     * Input        : <const std::string> = application name 
     * Return       : <bool> Return success (true) / failure(false)
     * Description  : Iterate over all the paths in the container and open those paths.
     *                Success: if atleast one path is opend in the container.
     */ 
    bool open_paths(const std::string& app_name) {
      bool is_container_valid = false;
      for (auto p_iter = pathMap.begin(); p_iter != pathMap.end(); p_iter++) {
        NKVTargetPath* one_path = p_iter->second;
        if (one_path) {
          smg_debug(logger, "Opening path with address = %s , dev_path = %s, port = %d, status = %d",
                   one_path->path_ip.c_str(), one_path->dev_path.c_str(), one_path->path_port,(one_path->path_status).load(std::memory_order_relaxed));
 
          if (one_path->get_target_path_status()) { 
            if (one_path->open_path(app_name)) {
              if (listing_with_cached_keys && !nkv_remote_listing) {
                one_path->start_thread();
              }
              is_container_valid = true;
            }
          }

        } else {
          smg_error(logger, "NULL path found while opening path !!");
        }
      }
      return is_container_valid;
    }

    int32_t populate_path_info(nkv_container_transport  *transportlist) {

      uint32_t cur_pop_index = 0;
      for (auto p_iter = pathMap.begin(); p_iter != pathMap.end(); p_iter++) {
        NKVTargetPath* one_path = p_iter->second;
        if (one_path) {
          transportlist[cur_pop_index].network_path_id = one_path->path_id;
          transportlist[cur_pop_index].network_path_hash = one_path->path_hash;
          transportlist[cur_pop_index].port = one_path->path_port;
          transportlist[cur_pop_index].addr_family = one_path->addr_family;
          transportlist[cur_pop_index].speed = one_path->path_speed;
          transportlist[cur_pop_index].status = (one_path->path_status).load(std::memory_order_relaxed);
          one_path->path_ip.copy(transportlist[cur_pop_index].ip_addr, one_path->path_ip.length());
          one_path->dev_path.copy(transportlist[cur_pop_index].mount_point, one_path->dev_path.length());

          cur_pop_index++;
 
        } else {
          smg_error(logger, "NULL path found while opening path !!");
          return 0;
        }
      }
      if (cur_pop_index == 0) {
        smg_error(logger, "No Path found for the container = %s", target_container_name.c_str());
        return 0;
      }
      return cur_pop_index;
            
    }

    int32_t get_object_async(const char* key, uint32_t key_len,  char* buff, uint32_t buff_len, void* cb);
    int32_t put_object_async(const char* key, uint32_t key_len, char* buff, uint32_t buff_len, void* cb, bool is_idempotent);
    int32_t del_object_async(const char* key, uint32_t key_len, void* cb);

  };

  class NKVContainerList {
    std::unordered_map<uint64_t, NKVTarget*> cnt_list;
    //std::unordered_map<std::size_t, NKVTarget*> cnt_list;
    std::atomic<uint64_t> cache_version;
    std::string app_name;
    uint64_t instance_uuid;
    uint64_t nkv_handle;

  public:
    NKVContainerList(uint64_t latest_version, const char* a_uuid, uint64_t ins_uuid, uint64_t n_handle): 
                    cache_version(latest_version), app_name(a_uuid), instance_uuid(ins_uuid), 
                    nkv_handle(n_handle) {


    }
    ~NKVContainerList() {
      for (auto m_iter = cnt_list.begin(); m_iter != cnt_list.end(); m_iter++) {
        delete(m_iter->second);
      }
      cnt_list.clear();
    }

    uint64_t get_nkv_handle() {
      return nkv_handle;
    }

    int32_t get_container_count() {
      return cnt_list.size();
    }

    nkv_result nkv_send_io(uint64_t container_hash, uint64_t container_path_hash, const nkv_key* key, void* opt, 
                           nkv_value* value, int32_t which_op, nkv_postprocess_function* post_fn) {
      nkv_result stat = NKV_SUCCESS;

      auto c_iter = cnt_list.find(container_hash);
      if (c_iter == cnt_list.end()) {
        smg_error(logger,"No Container found for hash = %u, number of containers = %u", container_hash, cnt_list.size());
        return NKV_ERR_NO_CNT_FOUND;
      }

      if((which_op == NKV_LOCK_OP || which_op == NKV_UNLOCK_OP) \
				&& (((nkv_lock_option *)opt)->nkv_lock_uuid != this->instance_uuid)) {
			return NKV_ERR_LOCK_UUID_MISMATCH;
      }

      NKVTarget* one_cnt = c_iter->second;
      // A Subsystem UP is indicated by status 0
      if (one_cnt && ! one_cnt->get_ss_status()) {
        stat = one_cnt->send_io_to_path(container_path_hash, key, opt, value, which_op, post_fn);
      } else {
        smg_error(logger, "NULL Container found for hash = %u!!", container_hash);
        return NKV_ERR_NO_CNT_FOUND;
      }

      //Populate cache for meta keys
      /*if (false && (stat == NKV_SUCCESS) && (!cache_hit)) {
        std::string key_str ((char*) key->key, key->length);
        std::size_t found = key_str.find(iter_prefix);
        if (found != std::string::npos && found == 0) {
          //Meta keys, check if it is present in lru cache
          if (shard_id == -1) {
            std::size_t key_prefix = std::hash<std::string>{}(key_str);
            shard_id = key_prefix % nkv_read_cache_shard_size;
          }
          
          if (which_op != NKV_DELETE_OP) {
            if (which_op == NKV_RETRIEVE_OP) {
              void* c_buffer = malloc(value->actual_length);
              memcpy(c_buffer, value->value, value->actual_length);

              nkv_value_wrapper nkvvalue (c_buffer, value->actual_length, value->actual_length);
              cnt_cache[shard_id].put(key_str, std::move(nkvvalue));
             
            } else {
              void* c_buffer = malloc(value->length);
              memcpy(c_buffer, value->value, value->length);
              nkv_value_wrapper nkvvalue (c_buffer, value->length, value->length);
              cnt_cache[shard_id].put(key_str, std::move(nkvvalue));
            }
          } else {
            cnt_cache[shard_id].del(key_str);
          }
          
        }
          
      }*/

      return stat;
    }

    nkv_result nkv_list_keys (uint64_t container_hash, uint64_t container_path_hash, uint32_t* max_keys, 
                              nkv_key* keys, void*& iter_context, const char* prefix, const char* delimiter, const char* start_after) {

      nkv_result stat = NKV_SUCCESS;
      auto c_iter = cnt_list.find(container_hash);
      if (c_iter == cnt_list.end()) {
        smg_error(logger,"No Container found for hash = %u, number of containers = %u, op = list_keys", container_hash, cnt_list.size());
        return NKV_ERR_NO_CNT_FOUND;
      }
      NKVTarget* one_cnt = c_iter->second;
      if (one_cnt) {
        stat = one_cnt->list_keys_from_path(container_path_hash, max_keys, keys, iter_context, prefix, delimiter, start_after);
      } else {
        smg_error(logger, "NULL Container found for hash = %u, op = list_keys!!", container_hash);
        return NKV_ERR_NO_CNT_FOUND;
      }
      return stat;

    }

    nkv_result nkv_get_path_mount_point (uint64_t container_hash, uint64_t container_path_hash, std::string& p_mount) {
      nkv_result stat = NKV_SUCCESS;
      auto c_iter = cnt_list.find(container_hash);
      if (c_iter == cnt_list.end()) {
        smg_error(logger,"No Container found for hash = %u, number of containers = %u, op = nkv_get_path_mount_point", container_hash, cnt_list.size());
        return NKV_ERR_NO_CNT_FOUND;
      }
      NKVTarget* one_cnt = c_iter->second;
      if (one_cnt) {
        stat = one_cnt->get_path_mount_point(container_path_hash, p_mount);
      } else {
        smg_error(logger, "NULL Container found for hash = %u, op = nkv_get_path_mount_point!!", container_hash);
        return NKV_ERR_NO_CNT_FOUND;
      }
      return stat;

    }
      
    bool populate_container_info(nkv_container_info *cntlist, uint32_t *cnt_count, uint32_t index);

    bool open_container_paths(const std::string& app_name) {
      bool is_container_list_valid = false;
      for (auto m_iter = cnt_list.begin(); m_iter != cnt_list.end(); m_iter++) {
        NKVTarget* one_cnt = m_iter->second;
        if (one_cnt) {
          smg_alert(logger, "Opening path for target node = %s, container name = %s status=%d",
                   one_cnt->target_node_name.c_str(), one_cnt->target_container_name.c_str(),one_cnt->get_ss_status());
          if ( !one_cnt->get_ss_status() && one_cnt->open_paths(app_name)) {
            is_container_list_valid = true;
          }
        } else {
          smg_error(logger, "Got NULL container while opening paths !!");
        }
      }
      return is_container_list_valid;
    }

    bool verify_min_topology_exists (int32_t num_required_container, int32_t num_required_container_path) {
      bool min_topology_exist = true;
      if (cnt_list.size() < (uint32_t) num_required_container) {
        smg_error(logger, "Not enough containers, minimum required = %d, available = %d",
                 num_required_container, cnt_list.size());
        min_topology_exist = false;
      }
      else {
        int32_t valid_container_count = cnt_list.size(); // subsystem count
        for (auto m_iter = cnt_list.begin(); m_iter != cnt_list.end(); m_iter++) {
          NKVTarget* one_cnt = m_iter->second;
          if (one_cnt) {
            smg_info(logger, "Inspecting target id = %d, target node = %s, container name = %s", 
                     one_cnt->t_id, one_cnt->target_node_name.c_str(), one_cnt->target_container_name.c_str()); 
            if (!one_cnt->verify_min_path_exists(num_required_container_path)) {
              valid_container_count--;
              one_cnt->set_ss_status(1);
              smg_error(logger, "Minimum path doesn't exist for the container %s", one_cnt->target_container_name.c_str());
            }
          } else {
            smg_error(logger, "Got NULL container while adding path mount point !!");
            valid_container_count--;
          }
        }
        if ( valid_container_count < (uint32_t) num_required_container ) {
          min_topology_exist = false;
        }
      }
      return min_topology_exist;
    }

    void collect_nkv_stat () {
      if ( path_stat_collection ) {
        for (auto m_iter = cnt_list.begin(); m_iter != cnt_list.end(); m_iter++) {
          NKVTarget* one_cnt = m_iter->second;
          if (one_cnt) {
            smg_debug(logger, "Collecting stat for target id = %d, target node = %s, container name = %s",
                     one_cnt->t_id, one_cnt->target_node_name.c_str(), one_cnt->target_container_name.c_str());
            one_cnt->collect_nkv_path_stat();
          } else {
            smg_error(logger, "Got NULL container while collecting stat !!");
            assert(0);
          }
        }
      } else {
          smg_warn(logger, "NKV Path Stats Collection is disabled! ...");
      }
    }

    void wait_or_detach_thread (bool will_wait = true) {
      for (auto m_iter = cnt_list.begin(); m_iter != cnt_list.end(); m_iter++) {
        NKVTarget* one_cnt = m_iter->second;
        if (one_cnt) {
          smg_debug(logger, "wait_or_detach_thread for target id = %d, target node = %s, container name = %s",
                   one_cnt->t_id, one_cnt->target_node_name.c_str(), one_cnt->target_container_name.c_str());
          one_cnt->wait_or_detach_path_thread(will_wait);
        } else {
          smg_error(logger, "Got NULL container while inspecting thread !!");
          assert(0);
        }
      }
    }


    int32_t parse_add_path_mount_point(boost::property_tree::ptree & pt) {
      try {
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("nkv_remote_mounts")) {
          assert(v.first.empty());
          boost::property_tree::ptree pc = v.second;
          std::string subsystem_mount = pc.get<std::string>("mount_point");
          std::string subsystem_address = pc.get<std::string>("nqn_transport_address");
          std::string subsystem_name = pc.get<std::string>("remote_nqn_name");
          std::string subsystem_node = pc.get<std::string>("remote_target_node_name");
          int32_t subsystem_port = pc.get<int>("nqn_transport_port");
          int32_t numa_node_attached = pc.get<int>("numa_node_attached");
          int32_t driver_thread_core = -1;
          if (core_pinning_required)
            driver_thread_core = pc.get<int>("driver_thread_core");

          smg_alert(logger, "Adding device path, mount point = %s, address = %s, port = %d, nqn name = %s, target node = %s, numa = %d, core = %d",
                    subsystem_mount.c_str(), subsystem_address.c_str(), subsystem_port, subsystem_name.c_str(), subsystem_node.c_str(), 
                    numa_node_attached, driver_thread_core);
          
          bool dev_path_added = false;
 
          for (auto m_iter = cnt_list.begin(); m_iter != cnt_list.end(); m_iter++) {
            NKVTarget* one_cnt = m_iter->second;
            if (one_cnt) {
              if ((one_cnt->target_node_name == subsystem_node) && (one_cnt->target_container_name == subsystem_name)){
                int32_t stat = one_cnt->add_mount_point_to_path(subsystem_address, subsystem_mount, numa_node_attached, driver_thread_core);
                if (stat == 0) {
                  dev_path_added = true;
                  break;
                }
              }  

            } else {
              smg_error(logger, "Got NULL container while adding path mount point !!");
            }
          }
          if (!dev_path_added) {
            smg_error(logger, "Could not add Device path, mount point = %s, address = %s, port = %d, nqn name = %s, target node = %s",
                    subsystem_mount.c_str(), subsystem_address.c_str(), subsystem_port, subsystem_name.c_str(), subsystem_node.c_str());
             
          }

        }
      }
      catch (std::exception& e) {
        smg_error(logger, "%s%s", "Error reading config file while adding path mount point, Error = ", e.what());
        return 1;
      } 
      return 0; 
    }
  
    // This is for LKV based deployment
    int32_t add_local_container_and_path (const char* host_name_ip, uint32_t host_port, boost::property_tree::ptree & pt) {
      uint64_t ss_hash = std::hash<std::string>{}(host_name_ip);

      NKVTarget* one_cnt = new NKVTarget(0, "", host_name_ip, host_name_ip, ss_hash);
      assert(one_cnt != NULL);
      one_cnt->set_ss_status(1);
      one_cnt->set_space_avail_percent(100);
      int32_t path_id = 0;

      try {
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("nkv_local_mounts")) {
          assert(v.first.empty());
          boost::property_tree::ptree pc = v.second;
          std::string local_mount = pc.get<std::string>("mount_point");
          std::string local_address = "127.0.0.1";
          std::string local_node = host_name_ip;
          int32_t local_port = host_port;
          int32_t numa_node_attached = -1;
          int32_t driver_thread_core = -1;

          if (core_pinning_required) {
            numa_node_attached = pc.get<int>("numa_node_attached");
            driver_thread_core = pc.get<int>("driver_thread_core");
          }

          smg_alert(logger, "Adding local device path, mount point = %s, address = %s, host_name_ip = %s, port = %d, numa = %d, core = %d",
                    local_mount.c_str(), local_address.c_str(), host_name_ip, local_port, numa_node_attached, driver_thread_core);

          std::string h_path_str = host_name_ip + local_mount;
          uint64_t ss_p_hash = std::hash<std::string>{}(h_path_str);
          NKVTargetPath* one_path = new NKVTargetPath(ss_p_hash, path_id, local_address, local_port, -1, -1, -1, -1, -1);
          assert(one_path != NULL);
          one_path->add_device_path(local_mount);
          one_path->path_numa_node = numa_node_attached;
          one_path->core_to_pin = driver_thread_core;

          one_cnt->add_network_path(one_path, ss_p_hash);
          path_id++;

        }
      }
      catch (std::exception& e) {
        smg_error(logger, "%s%s", "Error reading config file while adding path mount point, Error = ", e.what());
        return 1;
      }
      auto cnt_iter = cnt_list.find(ss_hash);
      assert (cnt_iter == cnt_list.end());
      cnt_list[ss_hash] = one_cnt;

      smg_info (logger, "Local Container added, hash = %u, id = %u, host_name_ip = %s, container count = %d",
                ss_hash, 0, host_name_ip, cnt_list.size());

      return 0;

    }

    // This is for Network KV based deployment
    /* Function Name: parse_add_container
     * Input Args   : <boost::property_tree::ptree> = ClusterMap information
     * Return       : <int32_t> 0/1 = Success/failure
     * Description  : Create a NKV-Host container list based on "subsystem_map" informaion
     *                of ClusterMap. A container contains subsystem information.
     */
    int32_t parse_add_container(boost::property_tree::ptree & pr) {

      int32_t cnt_id = 0;  
      try {
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pr.get_child("subsystem_maps")) {
          assert(v.first.empty());
          
          boost::property_tree::ptree pt = v.second;
          std::string target_server_name = pt.get<std::string>("target_server_name");
          std::string subsystem_nqn_id = pt.get<std::string>("subsystem_nqn_id");
          std::string subsystem_nqn = pt.get<std::string>("subsystem_nqn");
          int32_t subsystem_status = pt.get<int>("subsystem_status");
          float subsystem_space_available_percent = pt.get<float>("subsystem_avail_percent");
          uint64_t ss_hash = std::hash<std::string>{}(subsystem_nqn_id);

          NKVTarget* one_cnt = new NKVTarget(cnt_id, subsystem_nqn_id, target_server_name, subsystem_nqn, ss_hash);
          assert(one_cnt != NULL);
          one_cnt->set_ss_status(subsystem_status);
          one_cnt->set_space_avail_percent(subsystem_space_available_percent);

          int32_t path_id = 0;
          BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("subsystem_transport")) {
            assert(v.first.empty());
            boost::property_tree::ptree pc = v.second;
            int32_t subsystem_np_type = pc.get<int>("subsystem_type");
            std::string subsystem_address = pc.get<std::string>("subsystem_address");
            int32_t subsystem_port = pc.get<int>("subsystem_port");
            int32_t subsystem_addr_fam = pc.get<int>("subsystem_addr_fam");
            int32_t subsystem_in_speed = pc.get<int>("subsystem_interface_speed");
            int32_t subsystem_in_status = pc.get<int>("subsystem_interface_status");
            int32_t numa_aligned = pc.get<bool>("subsystem_interface_numa_aligned");

            uint64_t ss_p_hash = std::hash<std::string>{}(subsystem_address);

            NKVTargetPath* one_path = new NKVTargetPath(ss_p_hash, path_id, subsystem_address, subsystem_port, subsystem_addr_fam, 
                                                        subsystem_in_speed, subsystem_in_status, numa_aligned, subsystem_np_type);
            assert(one_path != NULL);
            smg_info(logger, "Adding path, hash = %u, address = %s, port = %d, fam = %d, speed = %d, status = %d, numa_aligned = %d",
                    ss_p_hash, subsystem_address.c_str(), subsystem_port, subsystem_addr_fam, subsystem_in_speed, subsystem_in_status, numa_aligned);

            one_cnt->add_network_path(one_path, ss_p_hash);
            path_id++;
          }

          auto cnt_iter = cnt_list.find(ss_hash);
          assert (cnt_iter == cnt_list.end());
          cnt_list[ss_hash] = one_cnt;

          smg_info (logger, "Container added, hash = %u, id = %u, uuid = %s, Node name = %s , NQN name = %s , container count = %d",
                   ss_hash, cnt_id, subsystem_nqn_id.c_str(), target_server_name.c_str(), subsystem_nqn.c_str(), cnt_list.size());
          cnt_id++;
 
        }
      }
      catch (std::exception& e) {
        smg_error(logger, "%s%s", "Error reading config file while adding container/path, Error = ", e.what());
        return 1;
      }
      return 0;
    }

    int32_t add_container(const std::string& tuuid, NKVTarget* pcnt) {
      if (pcnt) {
        if (!tuuid.empty()) {
          uint64_t ss_hash = std::hash<std::string>{}(tuuid);

          cnt_list[ss_hash] = pcnt;
          smg_info (logger, "Container added, hash = %u, uuid = %s, container count = %d", ss_hash, tuuid.c_str(), cnt_list.size());
          return 0;
        } else {
          smg_warn (logger, "Empty uuid passed !! "); 
        }
      } else {
        smg_warn (logger, "NULL container passed !! ");
      }
      return 1;
    }
 
    
   /* Function Name: update_container
    * Params       : <string> -Address of Remote Mount Path
    *                <int32_t>-Rremote Mount Path status
    * Return       : <bool>  Updated Mount Path or Not
    * Description  : Update remote mount path status based on the address received from event.
    *                Invoked from event handler.
    */
    bool update_container(std::string category,
                          std::string node_name,
                          boost::property_tree::ptree& args,
                          int32_t remote_path_status) {

      bool is_nkv_data_structure_updated =  false;

      // Iterate container list which contain the list of subsystems 
      for (auto m_iter = cnt_list.begin(); m_iter != cnt_list.end(); m_iter++) {
        NKVTarget* target_ptr = m_iter->second;
        std::unordered_map<uint64_t, NKVTargetPath*> target_path_map = target_ptr->get_path_map(); 

        // Iterate each path of a subsystem
        for ( auto p_iter = target_path_map.begin(); p_iter != target_path_map.end(); p_iter++ ) {
          NKVTargetPath* target_path_ptr = p_iter->second;
          
          bool skip = true;
          // Target: Check only node name
          if ( category == "TARGET" ){
            if ( node_name == target_ptr->target_node_name ) {
              skip = false;
            }
          }
          else if ( category == "SUBSYSTEM" ) {
            // Subsystem: Check only nqn
            if (  target_ptr->target_container_name == args.get<std::string>("nqn", "") ) {
              skip = false;
            } 
          }
          else if ( category == "NETWORK" ) {
            if (target_path_ptr->path_ip == args.get<std::string>("address", "10.1.1.0") ) { 
              skip = false;
            }
          }  
        
          smg_debug(logger, "Remote PATH = %s , STATUS = %d , EVENT STATUS = %d",
                  (target_path_ptr->dev_path).c_str(),  (target_path_ptr->path_status).load(std::memory_order_relaxed), remote_path_status);
          // NIC: Check path if matches and update status accordingly
          if (! skip) {
            if ( (target_path_ptr->path_status).load(std::memory_order_relaxed) !=  remote_path_status ) {
                 (target_path_ptr->path_status).store(remote_path_status, std::memory_order_relaxed);
                 is_nkv_data_structure_updated = true; 
                              
              if ( remote_path_status ) {
                smg_alert(logger,"Remote mount path %s is UP for IO", (target_path_ptr->dev_path).c_str());
              }
              else {
                smg_alert(logger,"Remote mount path %s is DOWN for IO", (target_path_ptr->dev_path).c_str());
              }
            }
          } // End of checking subsystem paths
        } // End of transporter path iteration
      } // End of iteration of subsystems

      return is_nkv_data_structure_updated;
    } 

  };

#endif

