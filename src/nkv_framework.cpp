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

#include <string>
#include "nkv_framework.h"
#include "nkv_const.h"

std::atomic<bool> nkv_stopping (false);
std::atomic<uint64_t> nkv_pending_calls (0);
std::atomic<bool> is_kvs_initialized (false);
std::atomic<uint32_t> nkv_async_path_max_qd(512);
std::atomic<uint64_t> nkv_num_async_submission (0);
std::atomic<uint64_t> nkv_num_async_completion (0);
c_smglogger* logger = NULL;
NKVContainerList* nkv_cnt_list = NULL;
int32_t core_pinning_required = 0;
int32_t queue_depth_monitor_required = 0;
int32_t queue_depth_threshold = 0;
int32_t listing_with_cached_keys = 0;
int32_t num_path_per_container_to_iterate = 0;
int32_t nkv_is_on_local_kv  = 1;
int32_t nkv_iter_max_key_threshold  = 1000;

thread_local int32_t core_running_driver_thread = -1;
std::string iter_prefix = "0000";
std::string key_default_delimiter = "/";
int32_t MAX_DIR_ENTRIES = 8;
int32_t nkv_listing_wait_till_cache_init  = 1;
int32_t nkv_listing_need_cache_stat  = 1;
int32_t nkv_listing_cache_num_shards = 1024;
int32_t nkv_dynamic_logging = 0;
int32_t path_stat_collection = 1;

#define iter_buff (32*1024)
std::string NKV_ROOT_PREFIX = "root" + key_default_delimiter;

#ifdef SAMSUNG_API

void kvs_aio_completion (kvs_callback_context* ioctx) {
  if (!ioctx) {
    smg_error(logger, "Async IO returned NULL ioctx, ignoring..");
    return;
  }
  nkv_postprocess_function* post_fn = (nkv_postprocess_function*)ioctx->private1;
  NKVTargetPath* t_path = (NKVTargetPath*)ioctx->private2;

  if (core_pinning_required && core_running_driver_thread == -1 && t_path) {
    int32_t core_to_pin = t_path->core_to_pin;
    if (core_to_pin != -1) {
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(core_to_pin, &cpuset);
      int32_t ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      if (ret != 0) {
        smg_error(logger, "Setting driver thread affinity failed, thread_id = %u, core_id = %d",
                  pthread_self(), core_to_pin);
        assert(ret == 0);

      } else {
        smg_alert(logger, "Setting driver thread affinity successful, thread_id = %u, core_id = %d",
                  pthread_self(), core_to_pin);
        core_running_driver_thread = core_to_pin;
      }
    } else {
      smg_warn(logger, "Core not supplied to set driver thread affinity, ignoring, performance may be affected !!");
      core_running_driver_thread = 0;
    }
  }

  
  if (ioctx->result != 0) {
    if (ioctx->result != KVS_ERR_KEY_NOT_EXIST) {
      smg_error(logger, "Async IO failed: op = %d, key = %s, result = 0x%x, err = %s, dev_path = %s, ip = %s\n",
                ioctx->opcode, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result, kvs_errstr(ioctx->result),
                t_path->dev_path.c_str(), t_path->path_ip.c_str());
    } else {
      smg_info(logger, "Async IO failed: op = %d, key = %s, result = 0x%x, err = %s, dev_path = %s, ip = %s\n",
               ioctx->opcode, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result, kvs_errstr(ioctx->result),
               t_path->dev_path.c_str(), t_path->path_ip.c_str());

    }

  } else {
    smg_info(logger, "Async IO success: op = %d, key = %s, result = %s, dev_path = %s, ip = %s\n", 
             ioctx->opcode, ioctx->key? (char*)ioctx->key->key : 0 , kvs_errstr(ioctx->result),
             t_path->dev_path.c_str(), t_path->path_ip.c_str());
  }
  int32_t num_op = 1;//now always 1
  uint64_t actual_get_size = 0;
  bool safe_to_free_val_buffer = true;
  if (post_fn) {
    nkv_aio_construct aio_ctx;
    //To do:: Covert to nkv result
    aio_ctx.result = ioctx->result;
    aio_ctx.private_data_1 = post_fn->private_data_1;
    aio_ctx.private_data_2 = post_fn->private_data_2;

    switch(ioctx->opcode) {
      case IOCB_ASYNC_PUT_CMD:
        aio_ctx.opcode = 1;
        break;

      case IOCB_ASYNC_GET_CMD:
        {
          aio_ctx.opcode = 0;
          actual_get_size = ioctx->value? ioctx->value->actual_value_size: 0;
          smg_info(logger, "Async GET actual value size = %u", actual_get_size);
          if (actual_get_size == 0 && ioctx->result == 0) {
            smg_error(logger, "Async GET actual value size 0 !! op = %d, key = %s, result = 0x%x, err = %s, dev_path = %s, ip = %s\n", 
                      ioctx->opcode, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result, kvs_errstr(ioctx->result),
                      t_path->dev_path.c_str(), t_path->path_ip.c_str());
            safe_to_free_val_buffer = false;
          }
        }
        break;

      case IOCB_ASYNC_DEL_CMD:
        aio_ctx.opcode = 2;
        break;

      default:
        break; 
    }
    aio_ctx.key.key = ioctx->key? ioctx->key->key: 0;
    aio_ctx.key.length = ioctx->key? ioctx->key->length: 0;
    aio_ctx.value.value = ioctx->value? ioctx->value->value: 0;
    aio_ctx.value.length = ioctx->value? ioctx->value->length: 0;
    aio_ctx.value.actual_length = ioctx->value? ioctx->value->actual_value_size: 0;
    if(post_fn->nkv_aio_cb) {
      //nkv_num_async_completion.fetch_add(1, std::memory_order_relaxed);
      /*smg_warn(logger, "Async callback on address = 0x%x, post_fn = 0x%x, private_1 = 0x%x, private_2 = 0x%x, key = %s, op = %d, sub(%u)/comp(%u)", 
               post_fn->nkv_aio_cb, post_fn, aio_ctx.private_data_1, aio_ctx.private_data_2, ioctx->key? (char*)ioctx->key->key : 0, 
               aio_ctx.opcode, nkv_num_async_submission.load(), nkv_num_async_completion.load());*/
      //smg_alert(logger, "sub(%u)/comp(%u)", nkv_num_async_submission.load(), nkv_num_async_completion.load());
      //printf("\rsub(%u)/comp(%u)", nkv_num_async_submission.load(), nkv_num_async_completion.load());
      post_fn->nkv_aio_cb(&aio_ctx, num_op);
    } else {
      smg_error(logger, "Async IO with no application callback !: op = %d, key = %s, result = 0x%x, err = %s\n",
                ioctx->opcode, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result, kvs_errstr(ioctx->result));
    }  
  } else {
    smg_error(logger, "Async IO returned with null post_fn ! : op = %d, key = %s, result = 0x%x, err = %s\n",
              ioctx->opcode, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result, kvs_errstr(ioctx->result));
  }
  if (t_path) {
    t_path->nkv_async_path_cur_qd.fetch_sub(1, std::memory_order_relaxed);
    smg_warn(logger, "cur_qd(%u)/max_qd(%u)", t_path->nkv_async_path_cur_qd.load(), nkv_async_path_max_qd.load());
  } else {
    smg_error(logger, "Async IO returned with null path pointer ! : op = %d, key = %s, result = 0x%x, err = %s\n",
              ioctx->opcode, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result, kvs_errstr(ioctx->result));
  }
  if (ioctx->key)
    free(ioctx->key);
  if (ioctx->value && safe_to_free_val_buffer)
    free(ioctx->value);
}

#else
void kvs_aio_completion (kvs_postprocess_context* ioctx) {
  if (!ioctx) {
    smg_error(logger, "Async IO returned NULL ioctx, ignoring..");
    return;
  }
  nkv_postprocess_function* post_fn = (nkv_postprocess_function*)ioctx->private1;
  NKVTargetPath* t_path = (NKVTargetPath*)ioctx->private2;

  if (core_pinning_required && core_running_driver_thread == -1 && t_path) {
    int32_t core_to_pin = t_path->core_to_pin;
    if (core_to_pin != -1) {
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(core_to_pin, &cpuset);
      int32_t ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      if (ret != 0) {
        smg_error(logger, "Setting driver thread affinity failed, thread_id = %u, core_id = %d",
                  pthread_self(), core_to_pin);
        assert(ret == 0);

      } else {
        smg_alert(logger, "Setting driver thread affinity successful, thread_id = %u, core_id = %d",
                  pthread_self(), core_to_pin);
        core_running_driver_thread = core_to_pin;
      }
    } else {
      smg_warn(logger, "Core not supplied to set driver thread affinity, ignoring, performance may be affected !!");
      core_running_driver_thread = 0;
    }
  }


  if (ioctx->result != 0) {
    if (ioctx->result != KVS_ERR_KEY_NOT_EXIST) {
      smg_error(logger, "Async IO failed: op = %d, key = %s, result = 0x%x, dev_path = %s, ip = %s\n",
                ioctx->context, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result, 
                t_path->dev_path.c_str(), t_path->path_ip.c_str());
    } else {
      smg_info(logger, "Async IO failed: op = %d, key = %s, result = 0x%x, dev_path = %s, ip = %s\n",
               ioctx->context, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result, 
               t_path->dev_path.c_str(), t_path->path_ip.c_str());

    }

  } else {
    smg_info(logger, "Async IO success: op = %d, key = %s, result = 0x%x, dev_path = %s, ip = %s\n",
             ioctx->context, ioctx->key? (char*)ioctx->key->key : 0 , ioctx->result,
             t_path->dev_path.c_str(), t_path->path_ip.c_str());
  }
  int32_t num_op = 1;//now always 1
  uint64_t actual_get_size = 0;
  bool safe_to_free_val_buffer = true;
  if (post_fn) {
    nkv_aio_construct aio_ctx;
    //To do:: Covert to nkv result
    aio_ctx.result = ioctx->result;
    aio_ctx.private_data_1 = post_fn->private_data_1;
    aio_ctx.private_data_2 = post_fn->private_data_2;

    switch(ioctx->context) {
      case KVS_CMD_STORE:
        aio_ctx.opcode = 1;
        break;

      case KVS_CMD_RETRIEVE:
        {
          aio_ctx.opcode = 0;
          actual_get_size = ioctx->value? ioctx->value->actual_value_size: 0;
          smg_info(logger, "Async GET actual value size = %u", actual_get_size);
          if (actual_get_size == 0 && ioctx->result == 0) {
            smg_error(logger, "Async GET actual value size 0 !! op = %d, key = %s, result = 0x%x, dev_path = %s, ip = %s\n",
                      ioctx->context, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result,
                      t_path->dev_path.c_str(), t_path->path_ip.c_str());
            safe_to_free_val_buffer = false;
          }
        }
        break;

      case KVS_CMD_DELETE:
        aio_ctx.opcode = 2;
        break;

      default:
        break;
    }
    aio_ctx.key.key = ioctx->key? ioctx->key->key: 0;
    aio_ctx.key.length = ioctx->key? ioctx->key->length: 0;
    aio_ctx.value.value = ioctx->value? ioctx->value->value: 0;
    aio_ctx.value.length = ioctx->value? ioctx->value->length: 0;
    aio_ctx.value.actual_length = ioctx->value? ioctx->value->actual_value_size: 0;
    if(post_fn->nkv_aio_cb) {
      post_fn->nkv_aio_cb(&aio_ctx, num_op);
    } else {
      smg_error(logger, "Async IO with no application callback !: op = %d, key = %s, result = 0x%x\n",
                ioctx->context, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result);
    }
  } else {
    smg_error(logger, "Async IO returned with null post_fn ! : op = %d, key = %s, result = 0x%x\n",
              ioctx->context, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result);
  }
  if (t_path) {
    t_path->nkv_async_path_cur_qd.fetch_sub(1, std::memory_order_relaxed);
    smg_warn(logger, "cur_qd(%u)/max_qd(%u)", t_path->nkv_async_path_cur_qd.load(), nkv_async_path_max_qd.load());
  } else {
    smg_error(logger, "Async IO returned with null path pointer ! : op = %d, key = %s, result = 0x%x\n",
              ioctx->context, ioctx->key? (char*)ioctx->key->key : 0, ioctx->result);
  }
  if (ioctx->key)
    free(ioctx->key);
  if (ioctx->value && safe_to_free_val_buffer)
    free(ioctx->value);
}

#endif

bool NKVContainerList::populate_container_info(nkv_container_info *cntlist, uint32_t *cnt_count, uint32_t s_index) {
  
  uint32_t cur_index = 0, cur_pop_index = 0;
  for (auto m_iter = cnt_list.begin(); m_iter != cnt_list.end(); m_iter++) {
    if (s_index > cur_index) {
      cur_index++;
      continue;
    }
    NKVTarget* one_cnt = m_iter->second;
    if (one_cnt) {
      cntlist[cur_pop_index].container_id = one_cnt->t_id;
      cntlist[cur_pop_index].container_hash = one_cnt->target_hash;
      cntlist[cur_pop_index].container_status = one_cnt->ss_status;
      cntlist[cur_pop_index].container_space_available_percentage = one_cnt->ss_space_avail_percent;
      one_cnt->target_uuid.copy(cntlist[cur_pop_index].container_uuid, one_cnt->target_uuid.length());
      one_cnt->target_container_name.copy(cntlist[cur_pop_index].container_name, one_cnt->target_container_name.length());
      one_cnt->target_node_name.copy(cntlist[cur_pop_index].hosting_target_name, one_cnt->target_node_name.length());
 
      //Populate paths
      cntlist[cur_pop_index].num_container_transport = one_cnt->populate_path_info(cntlist[cur_pop_index].transport_list);
      if (cntlist[cur_pop_index].num_container_transport == 0) {
        *cnt_count = 0;
        return false; 
      }
      cur_pop_index++;
      cur_index++;
    } else {
      smg_error(logger, "Got NULL container while populating container information !!");
      *cnt_count = 0;
      return false;
    }
    if ((cur_index - s_index) == NKV_MAX_ENTRIES_PER_CALL) {
      smg_info(logger, "Max number of containers populated, aborting..");
      *cnt_count = NKV_MAX_ENTRIES_PER_CALL;
      break;
    }
  }
  if (cur_pop_index == 0) {
    smg_error(logger, "No container populated !!");
    return false;
  }
  smg_info(logger, "Number of containers populated = %u", cur_pop_index);
  *cnt_count = cur_pop_index;
  return true;
}

nkv_result NKVTargetPath::map_kvs_err_code_to_nkv_err_code (int32_t kvs_code) {

#ifdef SAMSUNG_API
  switch(kvs_code) {
    case 0x001:
      return NKV_ERR_BUFFER_SMALL;
    case 0x002:
      return NKV_ERR_CNT_INITIALIZED;
    case 0x003:
      return NKV_ERR_COMMAND_SUBMITTED;
    case 0x004:
      return NKV_ERR_CNT_CAPACITY;
    case 0x005:
    case 0x006:
      return NKV_ERR_CNT_INITIALIZED;
    case 0x007:
      return NKV_ERR_NO_CNT_FOUND;
    case 0x00E:
      return NKV_ERR_KEY_EXIST;
    case 0x00F:
      return NKV_ERR_KEY_INVALID;
    case 0x010:
      return NKV_ERR_KEY_LENGTH;
    case 0x011:
      return NKV_ERR_KEY_NOT_EXIST;
    case 0x01D:
      return NKV_ERR_CNT_BUSY;
    case 0x01F:
      return NKV_ERR_CNT_IO_TIMEOUT;
    case 0x021:
      return NKV_ERR_VALUE_LENGTH;
    case 0x022:
      return NKV_ERR_VALUE_LENGTH_MISALIGNED;
    case 0x024:
      return NKV_ERR_VALUE_UPDATE_NOT_ALLOWED;
    case 0x026:
      return NKV_ERR_PERMISSION;
    case 0x20A:
      return NKV_ERR_MAXIMUM_VALUE_SIZE_LIMIT_EXCEEDED;

    default:
      return NKV_ERR_IO;
  }

#else
  switch(kvs_code) {
    case KVS_ERR_BUFFER_SMALL:
      return NKV_ERR_BUFFER_SMALL;

    case KVS_ERR_DEV_CAPAPCITY:
    case KVS_ERR_KS_CAPACITY:
      return NKV_ERR_CNT_CAPACITY;

    case KVS_ERR_DEV_NOT_EXIST:
    case KVS_ERR_KS_NOT_EXIST:
      return NKV_ERR_NO_CNT_FOUND;

    case KVS_ERR_KEY_LENGTH_INVALID:
      return NKV_ERR_KEY_LENGTH;

    case KVS_ERR_KEY_NOT_EXIST:
      return NKV_ERR_KEY_NOT_EXIST;

    case KVS_ERR_VALUE_LENGTH_INVALID:
      return NKV_ERR_VALUE_LENGTH;

    case KVS_ERR_VALUE_OFFSET_MISALIGNED:
      return NKV_ERR_VALUE_LENGTH_MISALIGNED;

    case KVS_ERR_VALUE_UPDATE_NOT_ALLOWED:
      return NKV_ERR_VALUE_UPDATE_NOT_ALLOWED;

    default:
      return NKV_ERR_IO;
  }

#endif
}

int32_t NKVTargetPath::parse_delimiter_entries(std::string& key, const char* delimiter, std::vector<std::string>& dirs, 
                                               std::vector<std::string>& prefixes, std::string& f_name) {
  assert(key.length() != 0);
  assert(delimiter != NULL);

  int32_t num_entries = 0;  
  std::size_t cur_pos = 0;
  std::size_t found = key.find(delimiter);
  while (found != std::string::npos) {
    if (cur_pos != 0)
      dirs.emplace_back(key.substr(cur_pos, (found - cur_pos) + 1));
    prefixes.emplace_back(key.substr(0, (found + 1)));
    cur_pos = found + 1;
    found = key.find(delimiter, cur_pos, strlen(delimiter));
    ++num_entries;
  }
  if (cur_pos != key.length() - 1) {
    f_name = key.substr(cur_pos);
  }
  return num_entries;
}

void NKVTargetPath::populate_iter_cache(std::string& key_prefix_p, std::string& key_prefix_val, bool need_lock) {

  if (key_prefix_val.empty()) {
    smg_warn(logger, "Empty key_prefix_val for key_prefix = %s", key_prefix_p.c_str());
    return ;
  }
  std::size_t key_prefix = std::hash<std::string>{}(key_prefix_p);
  int32_t shard_id = key_prefix % nkv_listing_cache_num_shards;
  if (need_lock) {
    int32_t r = pthread_rwlock_wrlock(&cache_rw_lock_list[shard_id]);
    if (r != 0) {
      smg_error(logger, "RW lock acquisition failed for shard_id = %d, prefix = %s", shard_id, key_prefix_p.c_str());
      assert (r == 0);
    }
  }
  auto list_iter = listing_keys[shard_id].find(key_prefix);
  if (list_iter == listing_keys[shard_id].end()) {
    //std::unordered_set<std::string> tmp_hset({key_prefix_val});
    //listing_keys.insert (std::make_pair<std::string, std::unordered_set<std::string> > (std::move(key_prefix), std::move(tmp_hset)));
    std::set<std::string> tmp_hset({key_prefix_val});
    //listing_keys.insert (std::make_pair<std::string, std::set<std::string> > (std::move(key_prefix), std::move(tmp_hset)));
    listing_keys[shard_id].insert (std::make_pair<std::size_t, std::set<std::string> > (std::move(key_prefix), std::move(tmp_hset)));
    if (nkv_listing_need_cache_stat) {
      nkv_num_key_prefixes.fetch_add(1, std::memory_order_relaxed);
      nkv_num_keys.fetch_add(1, std::memory_order_relaxed); 
    }
  } else {
    auto set_iter = listing_keys[shard_id][key_prefix].find(key_prefix_val);
    if (set_iter == listing_keys[shard_id][key_prefix].end()) {
      listing_keys[shard_id][key_prefix].emplace(key_prefix_val);
      if (nkv_listing_need_cache_stat) {
        nkv_num_keys.fetch_add(1, std::memory_order_relaxed);
      }
    }
  }
  if (need_lock) {
    int32_t r = pthread_rwlock_unlock(&cache_rw_lock_list[shard_id]);
    if (r != 0) {
      smg_error(logger, "RW lock unlock failed for shard_id = %d, prefix = %s", shard_id, key_prefix_p.c_str());
      assert (r == 0);
    }
  }
}


nkv_result NKVTargetPath::do_store_io_to_path(const nkv_key* n_key, const nkv_store_option* n_opt, 
                                              nkv_value* n_value, nkv_postprocess_function* post_fn) {

  if (!n_key->key) {
    smg_error(logger, "nkv_key->key = NULL !!");
    return NKV_ERR_NULL_INPUT;
  }
  if ((n_key->length > NKV_MAX_KEY_LENGTH) || (n_key->length == 0)) {
    smg_error(logger, "Wrong key length, supplied length = %d !!", n_key->length);
    return NKV_ERR_KEY_LENGTH;
  }

  if (!n_value->value) {
    smg_error(logger, "nkv_value->value = NULL !!");
    return NKV_ERR_NULL_INPUT;
  }
 
  if ((n_value->length > NKV_MAX_VALUE_LENGTH) || (n_value->length == 0)) {
    smg_error(logger, "Wrong value length, supplied length = %d !!", n_value->length);
    return NKV_ERR_VALUE_LENGTH;
  }
  if (nkv_dynamic_logging) {
    smg_alert(logger, "NKV store request for key = %s, key_length = %u, value_length = %u, dev_path = %s, ip = %s",
             (char*) n_key->key, n_key->length, n_value->length, dev_path.c_str(), path_ip.c_str());
  }

  #ifdef SAMSUNG_API
    kvs_store_option option;
    option.st_type = KVS_STORE_POST;
    option.kvs_store_compress = false;

    smg_info(logger, "Store option:: compression = %d, encryption = %d, store crc = %d, no overwrite = %d, atomic = %d, update only = %d, append = %d, is_async = %d",
            n_opt->nkv_store_compressed, n_opt->nkv_store_ecrypted, n_opt->nkv_store_crc_in_meta, n_opt->nkv_store_no_overwrite,
            n_opt->nkv_store_atomic, n_opt->nkv_store_update_only, n_opt->nkv_store_append, post_fn? 1: 0);

    if (n_opt->nkv_store_compressed) {
      option.kvs_store_compress = true;    
    }

    if (n_opt->nkv_store_no_overwrite) {
      option.st_type = KVS_STORE_NOOVERWRITE; //Idempotent
    } else if (n_opt->nkv_store_update_only) {
      option.st_type = KVS_STORE_UPDATE_ONLY;
    } else if (n_opt->nkv_store_append) {
      option.st_type = KVS_STORE_APPEND;
    }
    if (n_opt->nkv_store_ecrypted) {
      return NKV_ERR_OPTION_ENCRYPTION_NOT_SUPPORTED;
    } 
    if (n_opt->nkv_store_crc_in_meta) {
      return NKV_ERR_OPTION_CRC_NOT_SUPPORTED;
    }

    kvs_store_context put_ctx = {option, 0, 0};
  #else
    kvs_option_store option;
    option.st_type = KVS_STORE_POST;
    option.assoc = NULL;  
  #endif

  if (!post_fn) {
    kvs_value kvsvalue = { n_value->value, (uint32_t)n_value->length, 0, 0};
    #ifdef SAMSUNG_API
      const kvs_key  kvskey = { n_key->key, (kvs_key_t)n_key->length};
      int ret = kvs_store_tuple(path_cont_handle, &kvskey, &kvsvalue, &put_ctx);    
      if(ret != KVS_SUCCESS ) {
        smg_error(logger, "store tuple failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s", 
                  ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
        return map_kvs_err_code_to_nkv_err_code(ret);
      }
    #else
      kvs_key  kvskey = { n_key->key, (uint16_t)n_key->length};
      kvs_result ret = kvs_store_kvp(path_ks_handle, &kvskey, &kvsvalue, &option);
      if(ret != KVS_SUCCESS ) {
        smg_error(logger, "store tuple failed with error 0x%x, key = %s, dev_path = %s, ip = %s",
                  ret, n_key->key, dev_path.c_str(), path_ip.c_str());
        return map_kvs_err_code_to_nkv_err_code(ret);
      }

    #endif

    //cache keys for listing (till iterator issue is fixed)
    if (listing_with_cached_keys) {
      std::string key_str ((char*) n_key->key, n_key->length);
      std::size_t found = key_str.find(iter_prefix);
      if (found != std::string::npos && found == 0) {
        std::vector<std::string> dir_entries; 
        dir_entries.reserve(MAX_DIR_ENTRIES);
        std::vector<std::string> prefix_entries; 
        prefix_entries.reserve(MAX_DIR_ENTRIES);
        std::string file_name;
        int32_t num_prefixes = parse_delimiter_entries(key_str, key_default_delimiter.c_str(), dir_entries, prefix_entries, file_name);
        //assert(!file_name.empty());
        if (file_name.empty()) {
          smg_warn(logger, "Empty file name found for key = %s, not a S3 compatible hierarchical key!", key_str.c_str());
        }
        
        std::string tmp_root = NKV_ROOT_PREFIX;
        if (num_prefixes == 0) {
          //std::lock_guard<std::mutex> lck (cache_mtx);
          populate_iter_cache(tmp_root, file_name);
        } else {
          //Got hierarchial key
          assert(dir_entries.size() == (uint32_t)(num_prefixes - 1));
          //std::lock_guard<std::mutex> lck (cache_mtx);

          for (int32_t prefix_it = 0; prefix_it < num_prefixes; ++prefix_it) {
            if (prefix_it == 0) {
              populate_iter_cache(tmp_root, prefix_entries[prefix_it]);
            }
            if (prefix_it == (num_prefixes -1)) {
              populate_iter_cache (prefix_entries[prefix_it], file_name);
            } else {
               populate_iter_cache(prefix_entries[prefix_it], dir_entries[prefix_it]);
            }
          }
        }     
      }
      
    }

  } else { //Async
    #ifdef SAMSUNG_API
      while (nkv_async_path_cur_qd > nkv_async_path_max_qd) {
        smg_warn(logger, "store tuple waiting on high qd, key = %s, dev_path = %s, ip = %s, cur_qd = %u, max_qd = %u",
                n_key->key, dev_path.c_str(), path_ip.c_str(), nkv_async_path_cur_qd.load(), nkv_async_path_max_qd.load());
        usleep(1);
      }
      if (queue_depth_monitor_required && (nkv_async_path_cur_qd < (uint32_t)queue_depth_threshold))
        smg_error(logger, "In store: outstanding QD is going below threashold = %d, key = %s, dev_path = %s, cur_qd = %u, max_qd = %u",
                  queue_depth_threshold, n_key->key, dev_path.c_str(), nkv_async_path_cur_qd.load(), nkv_async_path_max_qd.load());

      put_ctx.private1 = (void*) post_fn;
      put_ctx.private2 = (void*) this;
      kvs_key *kvskey = (kvs_key*)malloc(sizeof(kvs_key));
      kvskey->key = n_key->key;
      kvskey->length = n_key->length;
      kvs_value *kvsvalue = (kvs_value*)malloc(sizeof(kvs_value));
      kvsvalue->value = n_value->value;
      kvsvalue->length = (uint32_t)n_value->length;
      kvsvalue->actual_value_size = kvsvalue->offset = 0;
    
      int ret = kvs_store_tuple_async(path_cont_handle, kvskey, kvsvalue, &put_ctx, kvs_aio_completion);

      if(ret != KVS_SUCCESS ) {
        smg_error(logger, "store tuple async start failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s", 
                  ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
        return map_kvs_err_code_to_nkv_err_code(ret);
      }
      nkv_async_path_cur_qd.fetch_add(1, std::memory_order_relaxed);
    #else
      return NKV_NOT_SUPPORTED;
    #endif

  }

  return NKV_SUCCESS;
}

nkv_result NKVTargetPath::do_retrieve_io_from_path(const nkv_key* n_key, const nkv_retrieve_option* n_opt, nkv_value* n_value,
                                                   nkv_postprocess_function* post_fn ) {

  if (!n_key->key) {
    smg_error(logger, "nkv_key->key = NULL !!");
    return NKV_ERR_NULL_INPUT;
  }
  if ((n_key->length > NKV_MAX_KEY_LENGTH) || (n_key->length == 0)) {
    smg_error(logger, "Wrong key length, supplied length = %d !!", n_key->length);
    return NKV_ERR_KEY_LENGTH;
  }

  if (!n_value->value) {
    smg_error(logger, "nkv_value->value = NULL !!");
    return NKV_ERR_NULL_INPUT;
  }
  if ((n_value->length > NKV_MAX_VALUE_LENGTH) || (n_value->length == 0)) {
    smg_error(logger, "Wrong value length, supplied length = %d !!", n_value->length);
    return NKV_ERR_VALUE_LENGTH;
  }
  if (nkv_dynamic_logging) {
    smg_alert(logger, "NKV retrieve request for key = %s, key_length = %u, dev_path = %s, ip = %s",
             (char*) n_key->key, n_key->length, dev_path.c_str(), path_ip.c_str());
  }

  #ifdef SAMSUNG_API
    kvs_retrieve_option option;
    memset(&option, 0, sizeof(kvs_retrieve_option));
    option.kvs_retrieve_decompress = false;
    option.kvs_retrieve_delete = false;

    smg_info(logger, "Retrieve option:: decompression = %d, decryption = %d, compare crc = %d, delete = %d, is_async = %d",
            n_opt->nkv_retrieve_decompress, n_opt->nkv_retrieve_decrypt, n_opt->nkv_compare_crc, n_opt->nkv_retrieve_delete,
            post_fn? 1: 0);

    if (n_opt->nkv_retrieve_decompress) {
      option.kvs_retrieve_decompress = true;
    }
    if (n_opt->nkv_retrieve_delete) {
      option.kvs_retrieve_delete = true;
    }

    kvs_retrieve_context ret_ctx = {option, 0, 0};
  #else
    kvs_option_retrieve option;
    if (n_opt->nkv_retrieve_delete) {
      option.kvs_retrieve_delete = true;
    } else {
      option.kvs_retrieve_delete = false;
    }
  #endif

  if (!post_fn) {
    kvs_value kvsvalue = { n_value->value, (uint32_t)n_value->length, 0, 0};

    #ifdef SAMSUNG_API
      const kvs_key  kvskey = { n_key->key, (kvs_key_t)n_key->length};
      int ret = kvs_retrieve_tuple(path_cont_handle, &kvskey, &kvsvalue, &ret_ctx);
      if(ret != KVS_SUCCESS ) {
        if (ret != KVS_ERR_KEY_NOT_EXIST) {
          smg_error(logger, "Retrieve tuple failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s", 
                    ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
        } else {
          smg_info(logger, "Retrieve tuple failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s",
                    ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
        }
        return map_kvs_err_code_to_nkv_err_code(ret);
    }

    #else
      kvs_key  kvskey = { n_key->key, (uint16_t)n_key->length};
      kvs_result ret = kvs_retrieve_kvp(path_ks_handle, &kvskey, &option, &kvsvalue);
      if(ret != KVS_SUCCESS ) {
        if (ret != KVS_ERR_KEY_NOT_EXIST) {
          smg_error(logger, "Retrieve kvp failed with error 0x%x , key = %s, dev_path = %s, ip = %s",
                    ret, n_key->key, dev_path.c_str(), path_ip.c_str());
        } else {
          smg_info(logger, "Retrieve kvp failed with error 0x%x , key = %s, dev_path = %s, ip = %s",
                    ret, n_key->key, dev_path.c_str(), path_ip.c_str());
        }

        return map_kvs_err_code_to_nkv_err_code(ret);
      }

    #endif


    n_value->actual_length = kvsvalue.actual_value_size;
    if (n_value->actual_length == 0) {
      smg_error(logger, "Retrieve tuple Success with actual length = 0, Retrying once !! key = %s, dev_path = %s, ip = %s, passed_length = %u",
                n_key->key, dev_path.c_str(), path_ip.c_str(), kvsvalue.length);

      kvs_value kvsvalue_retry = { n_value->value, (uint32_t)n_value->length, 0, 0};
 
      #ifdef SAMSUNG_API
        ret = kvs_retrieve_tuple(path_cont_handle, &kvskey, &kvsvalue_retry, &ret_ctx);
        if(ret != KVS_SUCCESS ) {
          smg_error(logger, "Retrieve tuple failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s, passed_length = %u",
                    ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str(), kvsvalue_retry.length);
          return map_kvs_err_code_to_nkv_err_code(ret);
        }
      #else
        kvs_result ret = kvs_retrieve_kvp(path_ks_handle, &kvskey, &option, &kvsvalue_retry);
        if(ret != KVS_SUCCESS ) {
          smg_error(logger, "Retrieve tuple failed with error 0x%x, key = %s, dev_path = %s, ip = %s",
                    ret, n_key->key, dev_path.c_str(), path_ip.c_str());
          return map_kvs_err_code_to_nkv_err_code(ret);
        }

      #endif
      

      n_value->actual_length = kvsvalue_retry.actual_value_size;
      if (n_value->actual_length == 0) {
        smg_error(logger, "Retrieve tuple Success with actual length = 0, done Retrying !! key = %s, dev_path = %s, ip = %s",
                  n_key->key, dev_path.c_str(), path_ip.c_str());

      }
    }
    assert(NULL != n_value->value);
  } else {//Async
    #ifdef SAMSUNG_API
      while (nkv_async_path_cur_qd > nkv_async_path_max_qd) {
        smg_warn(logger, "Retrieve tuple waiting on high qd, key = %s, dev_path = %s, ip = %s, cur_qd = %u, max_qd = %u",
                  n_key->key, dev_path.c_str(), path_ip.c_str(), nkv_async_path_cur_qd.load(), nkv_async_path_max_qd.load());
        usleep(1);
      }
      if (queue_depth_monitor_required && (nkv_async_path_cur_qd < (uint32_t)queue_depth_threshold))
        smg_error(logger, "In retrieve: outstanding QD is going below threashold = %d, key = %s, dev_path = %s, cur_qd = %u, max_qd = %u",
                  queue_depth_threshold, n_key->key, dev_path.c_str(), nkv_async_path_cur_qd.load(), nkv_async_path_max_qd.load());

      ret_ctx.private1 = (void*) post_fn;
      ret_ctx.private2 = (void*) this;
      kvs_key *kvskey = (kvs_key*)malloc(sizeof(kvs_key));
      kvskey->key = n_key->key;
      kvskey->length = n_key->length;
      kvs_value *kvsvalue = (kvs_value*)malloc(sizeof(kvs_value));
      kvsvalue->value = n_value->value;
      kvsvalue->length = (uint32_t)n_value->length;
      kvsvalue->actual_value_size = kvsvalue->offset = 0;

      int ret = kvs_retrieve_tuple_async(path_cont_handle, kvskey, kvsvalue, &ret_ctx, kvs_aio_completion);

      if(ret != KVS_SUCCESS ) {
        smg_error(logger, "retrieve tuple async start failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s", 
                  ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
        return map_kvs_err_code_to_nkv_err_code(ret);
      }
      nkv_async_path_cur_qd.fetch_add(1, std::memory_order_relaxed);
    #else
      return NKV_NOT_SUPPORTED;
    #endif
  }

  return NKV_SUCCESS;
}

bool NKVTargetPath::remove_from_iter_cache(std::string& key_prefix_p, std::string& key_prefix_val, bool root_prefix) {

  if (key_prefix_val.empty()) {
    smg_warn(logger, "Empty key_prefix_val for key_prefix = %s", key_prefix_p.c_str());
    return true; //Delete empty directory without file
  }

  bool key_prefix_delete = false;
  std::size_t key_prefix = std::hash<std::string>{}(key_prefix_p);
  int32_t shard_id = key_prefix % nkv_listing_cache_num_shards;
  int32_t r = pthread_rwlock_wrlock(&cache_rw_lock_list[shard_id]);
  if (r != 0) {
    smg_error(logger, "RW lock acquisition failed for shard_id = %d, prefix = %s", shard_id, key_prefix_p.c_str());
    assert (r == 0);
  }

  auto list_iter = listing_keys[shard_id].find(key_prefix);
  if (list_iter == listing_keys[shard_id].end()) {
    smg_warn(logger, "Key prefix passed is not found !!, key_prefix = %s, shard_id = %d", key_prefix_p.c_str(), shard_id);
    r = pthread_rwlock_unlock(&cache_rw_lock_list[shard_id]);
    if (r != 0) {
      smg_error(logger, "RW lock unlock failed for shard_id = %d, prefix = %s", shard_id, key_prefix_p.c_str());
      assert (r == 0);
    }
    return key_prefix_delete;
  }

  int32_t num_keys_deleted = listing_keys[shard_id][key_prefix].erase(key_prefix_val);
  if (num_keys_deleted == 0) {
    smg_warn(logger, "Key prefix value not found while removing !!, key_prefix = %s, key_prefix_val = %s", key_prefix_p.c_str(), key_prefix_val.c_str());
  }

  if (nkv_listing_need_cache_stat && num_keys_deleted) {
    nkv_num_keys.fetch_sub(1, std::memory_order_relaxed);
  }

  if (listing_keys[shard_id][key_prefix].empty() && !root_prefix) {
    num_keys_deleted = listing_keys[shard_id].erase(key_prefix);
    if (num_keys_deleted == 0) {
      smg_warn(logger, "Key prefix not found while removing empty prefix !!, key_prefix = %s", key_prefix_p.c_str());
    }

    key_prefix_delete = true;
    if (nkv_listing_need_cache_stat && num_keys_deleted) {
      nkv_num_key_prefixes.fetch_sub(1, std::memory_order_relaxed);
    }
  }
  r = pthread_rwlock_unlock(&cache_rw_lock_list[shard_id]);
  if (r != 0) {
    smg_error(logger, "RW lock unlock failed for shard_id = %d, prefix = %s", shard_id, key_prefix_p.c_str());
    assert (r == 0);
  }
  return key_prefix_delete;
}


nkv_result NKVTargetPath::do_delete_io_from_path (const nkv_key* n_key, nkv_postprocess_function* post_fn) {

  if (!n_key->key) {
    smg_error(logger, "nkv_key->key = NULL !!");
    return NKV_ERR_NULL_INPUT;
  }
  if ((n_key->length > NKV_MAX_KEY_LENGTH) || (n_key->length == 0)) {
    smg_error(logger, "Wrong key length, supplied length = %d !!", n_key->length);
    return NKV_ERR_KEY_LENGTH;
  }
  if (nkv_dynamic_logging) {
    smg_alert(logger, "NKV delete request for key = %s, key_length = %u, dev_path = %s, ip = %s",
             (char*) n_key->key, n_key->length, dev_path.c_str(), path_ip.c_str());
  }

  #ifdef SAMSUNG_API
    const kvs_key  kvskey = { n_key->key, (kvs_key_t)n_key->length};
    kvs_delete_context del_ctx = { {false}, 0, 0};
  #else
    kvs_key  kvskey = { n_key->key, (uint16_t)n_key->length};
    kvs_option_delete option = {false};
  #endif

  if (!post_fn) {

    #ifdef SAMSUNG_API
      int ret = kvs_delete_tuple(path_cont_handle, &kvskey, &del_ctx);
      if(ret != KVS_SUCCESS ) {
        smg_error(logger, "Delete tuple failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s", 
                  ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
        return map_kvs_err_code_to_nkv_err_code(ret);
      }
    #else
      kvs_result ret = kvs_delete_kvp(path_ks_handle, &kvskey, &option);
      if(ret != KVS_SUCCESS ) {
        smg_error(logger, "Delete tuple failed with error 0x%x, key = %s, dev_path = %s, ip = %s",
                  ret, n_key->key, dev_path.c_str(), path_ip.c_str());
        return map_kvs_err_code_to_nkv_err_code(ret);
      }

    #endif

    if (listing_with_cached_keys) {
      std::string key_str ((char*) n_key->key, n_key->length);
      std::size_t found = key_str.find(iter_prefix);
      if (found != std::string::npos && found == 0) {
     
      #if 0
        int32_t num_keys_deleted = 0;
        int32_t num_cache_keys = 0;
        int32_t del_cache_keys = 0;
        {
          std::lock_guard<std::mutex> lck (cache_mtx);
          if (cached_keys.size() != 0) {
            num_keys_deleted = cached_keys.erase(key_str);
            num_cache_keys = cached_keys.size();
          }
          if (num_keys_deleted == 0) {
            deleted_cached_keys.emplace(key_str); 
            del_cache_keys = deleted_cached_keys.size();
          }
        }
        if (num_keys_deleted > 0) {
          smg_warn(logger, "deleted key = %s from the cache, cache_size = %u", key_str.c_str(), num_cache_keys);
        } else {
          smg_warn(logger, "key = %s added to the delete cache, del cache size = %u", key_str.c_str(), del_cache_keys);
        }
      #endif
        //smg_warn(logger, "NKV del success and removing from the cache, key = %s", key_str.c_str());
        std::vector<std::string> dir_entries;
        dir_entries.reserve(MAX_DIR_ENTRIES);
        std::vector<std::string> prefix_entries;
        prefix_entries.reserve(MAX_DIR_ENTRIES);
  
        std::string file_name;
        int32_t num_prefixes = parse_delimiter_entries(key_str, key_default_delimiter.c_str(), dir_entries, prefix_entries, file_name);
        //assert(!file_name.empty());
        if (file_name.empty()) {
          smg_warn(logger, "Empty file name found for key = %s", key_str.c_str());
        }
        /*std::lock_guard<std::mutex> lck (cache_mtx);
        if (listing_keys.size() == 0)
          return NKV_SUCCESS;
        */
        if (num_prefixes == 0) {
          remove_from_iter_cache(NKV_ROOT_PREFIX, file_name, true);
        } else {
          assert(dir_entries.size() == (uint32_t)(num_prefixes - 1));
          bool key_prefix_deleted = false;

          for (int32_t prefix_it = num_prefixes -1; prefix_it >= 0; --prefix_it) {
            
            if (prefix_it == (num_prefixes -1)) {
              key_prefix_deleted = remove_from_iter_cache (prefix_entries[prefix_it], file_name);

            } else {
              key_prefix_deleted = remove_from_iter_cache(prefix_entries[prefix_it], dir_entries[prefix_it]);
              if (prefix_it == 0 && key_prefix_deleted) {
                remove_from_iter_cache(NKV_ROOT_PREFIX, prefix_entries[prefix_it], true);
              }
            }
            if (!key_prefix_deleted)         
              break;
          }
        }  
      }
    }

  } else {

    #ifdef SAMSUNG_API
      while (nkv_async_path_cur_qd > nkv_async_path_max_qd) {
        smg_warn(logger, "Delete tuple waiting on high qd, key = %s, dev_path = %s, ip = %s, cur_qd = %u, max_qd = %u",
                  n_key->key, dev_path.c_str(), path_ip.c_str(), nkv_async_path_cur_qd.load(), nkv_async_path_max_qd.load());
        usleep(1);
      }
      del_ctx.private1 = (void*) post_fn;
      del_ctx.private2 = (void*) this;
      kvs_key *kvskey = (kvs_key*)malloc(sizeof(kvs_key));
      kvskey->key = n_key->key;
      kvskey->length = n_key->length;

      int ret = kvs_delete_tuple_async(path_cont_handle, kvskey, &del_ctx, kvs_aio_completion);

      if(ret != KVS_SUCCESS ) {
        smg_error(logger, "delete tuple async start failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s", 
                  ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
        return map_kvs_err_code_to_nkv_err_code(ret);
      }
      nkv_async_path_cur_qd.fetch_add(1, std::memory_order_relaxed);

    #else
      return NKV_NOT_SUPPORTED;
    #endif

  }
  
  return NKV_SUCCESS;

}

void NKVTargetPath::filter_and_populate_keys_from_path(uint32_t* max_keys, nkv_key* keys, char* disk_key_raw, uint32_t key_size, uint32_t* num_keys_iterted,
                                                  const char* prefix, const char* delimiter, iterator_info*& iter_info, bool cached_keys) {
  assert(disk_key_raw != NULL);
  uint32_t cur_index = *num_keys_iterted;
  smg_info(logger, "### Got key = %s, size = %u and we are about to filter based on prefix = %s, delimiter = %s",
           disk_key_raw, key_size, prefix, delimiter);

  if (!prefix && !delimiter) {
    if (cached_keys) {
      assert(keys[cur_index].key != NULL);
      uint32_t max_key_len = keys[cur_index].length;
      if (max_key_len < key_size) {
        smg_error(logger, "Output buffer key length supplied (%u) is less than the actual key length (%u) ! dev_path = %s, ip = %s",
                  max_key_len, key_size, dev_path.c_str(), path_ip.c_str());
      }
      assert(max_key_len >= key_size);
      memcpy(keys[cur_index].key, disk_key_raw, key_size);
      
    }
    keys[cur_index].length = key_size;
    smg_info(logger, "No prefix  and delimiter provided, Added key = %s, length = %u to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u",
             (char*) keys[cur_index].key, keys[cur_index].length, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys);
    (*num_keys_iterted)++;

  } else if (prefix && !delimiter) {
    char* p = strstr(disk_key_raw, prefix);
    if (p) {
      if (cached_keys) {
        assert(keys[cur_index].key != NULL);
        uint32_t max_key_len = keys[cur_index].length;
        if (max_key_len < key_size) {
          smg_error(logger, "Output buffer key length supplied (%u) is less than the actual key length (%u) ! dev_path = %s, ip = %s",
                    max_key_len, key_size, dev_path.c_str(), path_ip.c_str());
        }
        assert(max_key_len >= key_size);
        memcpy(keys[cur_index].key, disk_key_raw, key_size);

      }

      keys[cur_index].length = key_size;
      smg_info(logger, "No delimiter provided but Prefix matched, Added key = %s, length = %u to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, prefix = %s",
               (char*) keys[cur_index].key, keys[cur_index].length, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, prefix);
      (*num_keys_iterted)++;
    } else {
      smg_info(logger, "No delimiter provided and Prefix *not matched*, skipping key = %s, length = %u , dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, prefix = %s",
               disk_key_raw, key_size, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, prefix);
    }
  } else if (!prefix && delimiter) {
    std::string disk_key (disk_key_raw, key_size);
    std::size_t found = disk_key.find_first_of(delimiter);
    if (found == std::string::npos) {
      if (cached_keys) {
        assert(keys[cur_index].key != NULL);
        uint32_t max_key_len = keys[cur_index].length;
        if (max_key_len < key_size) {
          smg_error(logger, "Output buffer key length supplied (%u) is less than the actual key length (%u) ! dev_path = %s, ip = %s",
                    max_key_len, key_size, dev_path.c_str(), path_ip.c_str());
        }
        assert(max_key_len >= key_size);
        memcpy(keys[cur_index].key, disk_key_raw, key_size);

      }
      keys[cur_index].length = key_size;
      smg_info(logger, "No prefix provided, delimiter provided but not found in key, Added key = %s, length = %u to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, delimiter = %s",
               (char*) keys[cur_index].key, keys[cur_index].length, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, delimiter);
      (*num_keys_iterted)++;
      
    } else {
      std::string root_dir = disk_key.substr(0, found + 1);
      auto d_iter = iter_info->dir_entries_added.find(root_dir);
      if (d_iter != iter_info->dir_entries_added.end()) {
        smg_info(logger, "No prefix provided, delimiter provided, dir = %s already added, skipping key = %s, length = %u to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, delimiter = %s",
                 root_dir.c_str(), disk_key_raw, key_size, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, delimiter);        
      } else {
        //Add the dir
        assert(keys[cur_index].key != NULL);
        uint32_t max_key_len = keys[cur_index].length;
        key_size = root_dir.length();
        if (max_key_len < key_size) {
          smg_error(logger, "Output buffer key length supplied (%u) is less than the actual key length (%u) ! dev_path = %s, ip = %s",
                    max_key_len, key_size, dev_path.c_str(), path_ip.c_str());
        }
        assert(max_key_len >= key_size);
        strcpy((char*)keys[cur_index].key, root_dir.c_str());
        keys[cur_index].length = key_size;
        smg_info(logger, "No prefix provided, delimiter provided and found in key, Added dir = %s, length = %u from actual key = %s to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, delimiter = %s",
                 (char*) keys[cur_index].key, keys[cur_index].length, disk_key_raw, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, delimiter);
        (*num_keys_iterted)++;
        iter_info->dir_entries_added.insert(root_dir);
      }
      
    }
  } else {
    //Both prefix and delimitier provided
    std::string disk_key (disk_key_raw, key_size);
    std::size_t found = disk_key.find(prefix);
    if (found != std::string::npos) {
      //Now search for delimiter after prefix
      std::size_t prefix_end = found + strlen(prefix);
      found =  disk_key.find(delimiter, prefix_end, strlen(delimiter));
      if (found != std::string::npos) {
        //delimitier found , add the next dir in hierarchy
        std::string dir_after_prefix = disk_key.substr(prefix_end, (found - prefix_end) + 1);
        auto d_iter = iter_info->dir_entries_added.find(dir_after_prefix);
        if (d_iter != iter_info->dir_entries_added.end()) {
          smg_info(logger, "prefix and delimiter provided, dir = %s already added, skipping key = %s, length = %u to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, delimiter = %s",
                   dir_after_prefix.c_str(), disk_key_raw, key_size, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, delimiter);
        } else {

          assert(keys[cur_index].key != NULL);
          uint32_t max_key_len = keys[cur_index].length;
          key_size = dir_after_prefix.length();
          if (max_key_len < key_size) {
            smg_error(logger, "Output buffer key length supplied (%u) is less than the actual key length (%u) ! dev_path = %s, ip = %s",
                      max_key_len, key_size, dev_path.c_str(), path_ip.c_str());
          }
          assert(max_key_len >= key_size);
          strcpy((char*)keys[cur_index].key, dir_after_prefix.c_str());
          keys[cur_index].length = key_size;
          smg_info(logger, "prefix and delimiter provided and found in key, Added dir = %s, length = %u from actual key = %s to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, delimiter = %s",
                   (char*) keys[cur_index].key, keys[cur_index].length, disk_key_raw, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, delimiter);
          (*num_keys_iterted)++;
          iter_info->dir_entries_added.insert(dir_after_prefix);

        }
      } else {
        //No delimiter after prefix, add rest of the key to the output
        std::string filename = disk_key.substr(prefix_end);
        if (!filename.empty()) {
          assert(keys[cur_index].key != NULL);
          uint32_t max_key_len = keys[cur_index].length;
          key_size = filename.length();
          if (max_key_len < key_size) {
            smg_error(logger, "Output buffer key length supplied (%u) is less than the actual key length (%u) ! dev_path = %s, ip = %s",
                      max_key_len, key_size, dev_path.c_str(), path_ip.c_str());
          }
          assert(max_key_len >= key_size);
          strcpy((char*)keys[cur_index].key, filename.c_str());
          keys[cur_index].length = key_size;
          smg_info(logger, "prefix and delimiter provided and found filename in key, Added key = %s, length = %u from actual key = %s to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, delimiter = %s",
                 (char*) keys[cur_index].key, keys[cur_index].length, disk_key.c_str(), dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, delimiter);
          (*num_keys_iterted)++;
       } else {
         smg_warn(logger, "Empty filename found, probably prefix supplied exactly matching key, skipping the key = %s, prefix = %s, delimiter = %s",
                  disk_key.c_str(), prefix, delimiter);
       }

      }
    
    } else {
      smg_info(logger, "delimiter, prefix provided but Prefix *not matched*, skipping key = %s, length = %u , dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, prefix = %s",
               disk_key_raw, key_size, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, prefix);
    }
  }

}

nkv_result NKVTargetPath::find_keys_from_path(uint32_t* max_keys, nkv_key* keys, iterator_info*& iter_info, uint32_t* num_keys_iterted, 
                                                  const char* prefix, const char* delimiter) {
  nkv_result stat = NKV_SUCCESS;
  uint32_t key_size = 0;
  char key[256] = {0};
  uint8_t *it_buffer = (uint8_t *) iter_info->iter_list.it_list;
  assert(it_buffer != NULL);
  assert(*num_keys_iterted >= 0);
  bool no_space = false;

  for(uint32_t i = 0; i < iter_info->iter_list.num_entries; i++) {
    if (*num_keys_iterted >= *max_keys) {
      *max_keys = *num_keys_iterted;
      stat = NKV_ITER_MORE_KEYS;
      no_space = true;
    }    
    key_size = *((unsigned int*)it_buffer);
    it_buffer += sizeof(unsigned int);   
    if (!no_space) {
      uint32_t cur_index = *num_keys_iterted;
      assert(keys[cur_index].key != NULL);
      uint32_t key_len = keys[cur_index].length;
      if (key_len < key_size) {
        smg_error(logger, "Output buffer key length supplied (%u) is less than the actual key length (%u) ! dev_path = %s, ip = %s",
                  key_len, key_size, dev_path.c_str(), path_ip.c_str());
      }
      assert(key_len >= key_size);
      memcpy(keys[cur_index].key, it_buffer, key_size);
      //key_name[key_size] = '\0';
      /*if (!prefix) {
        keys[cur_index].length = key_size;
        smg_info(logger, "No prefix provided, Added key = %s, length = %u to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u",
                 (char*) keys[cur_index].key, keys[cur_index].length, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys);
        (*num_keys_iterted)++;
      } else {
        char* p = strstr((char*) keys[cur_index].key, prefix);
        if (p) {
          keys[cur_index].length = key_size;
          smg_info(logger, "Prefix matched, Added key = %s, length = %u to the output buffer, dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, prefix = %s",
                   (char*) keys[cur_index].key, keys[cur_index].length, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, prefix);
          (*num_keys_iterted)++;
        } else {
          smg_info(logger, "Prefix *not matched*, skipping key = %s, length = %u , dev_path = %s, ip = %s, added so far in this batch = %u, max_keys = %u, prefix = %s",
                   (char*) keys[cur_index].key, keys[cur_index].length, dev_path.c_str(), path_ip.c_str(), cur_index, *max_keys, prefix);
        }
      }*/
      
      filter_and_populate_keys_from_path (max_keys, keys, (char*)keys[cur_index].key, key_size, num_keys_iterted, prefix, delimiter, iter_info);
      /*(*num_keys_iterted)++;
      memcpy(key, it_buffer, key_size);
      smg_warn(logger, "Added key = %s, length = %u to nothing! dev_path = %s, ip = %s, number of keys so far = %u",
               key, key_size, dev_path.c_str(), path_ip.c_str(), *num_keys_iterted);*/
    } else {
      memcpy(key, it_buffer, key_size);
      //key[key_size] = 0; 
      iter_info->excess_keys.insert(std::string(key));
      smg_warn(logger, "Added key = %s, length = %u to the exceeded key less, output buffer is not big enough! dev_path = %s, ip = %s, number of keys = %u", 
               key, key_size, dev_path.c_str(), path_ip.c_str(), iter_info->excess_keys.size());
    }

    it_buffer += key_size;
  }
  return stat;
}

#if 0
int32_t NKVTargetPath::create_modify_iter_meta(std::string& key_prefix) {

      std::string prefix_iter_sub = key_prefix + "num_sub/";
      auto list_iter = listing_keys.find(prefix_iter_sub);
      if (list_iter == listing_keys.end()) {
        std::unordered_set<std::string> tmp_hset({"1"});
        listing_keys.insert (std::make_pair<std::string, std::unordered_set<std::string> > (std::move(prefix_iter_sub), std::move(tmp_hset)));
        ++num_sub_prefix;
      } else {
        num_sub_prefix = listing_keys[prefix_iter_on].size() + 1;
        listing_keys[prefix_iter_on].emplace(std::to_string(num_sub_prefix));
      }
      std::string prefix_sub = key_prefix + std::to_string(num_sub_prefix) + "/";
      std::unordered_set<std::string> tmp_hset;
      list_iter_main->second.swap(tmp_hset);
      listing_keys.insert (std::make_pair<std::string, std::unordered_set<std::string> > (std::move(prefix_sub), std::move(tmp_hset)));
      
    }
  }
 
}
#endif

void NKVTargetPath::nkv_path_thread_init(int32_t what_work) {

  std::vector<std::string> dir_entries;
  dir_entries.reserve(MAX_DIR_ENTRIES);
  std::vector<std::string> prefix_entries;
  prefix_entries.reserve(MAX_DIR_ENTRIES);
  std::string file_name;
  std::vector<std::string> tmp_vec;
  bool done_processing = false;

  std::string init_thread_name = "nkv_p" + dev_path.substr(5);
  int rc = pthread_setname_np(pthread_self(), init_thread_name.c_str());
  if (rc != 0) {
    smg_error(logger, "Error on setting worker thread on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
  }

  if (what_work == 0 && listing_with_cached_keys) {
    while (1) {
      {
        std::unique_lock<std::mutex> lck(cache_mtx);
        if (path_vec.empty()) {
          cv_path.wait_for(lck,std::chrono::seconds(1));
          if (nkv_path_stopping && path_vec.empty()) {
            smg_info(logger, "nkv_path_thread_init::Thread is stopping on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
            break;
          }
        }
        if (nkv_path_stopping) {
          done_processing = true;
        }

        if (!path_vec.empty()) {
          path_vec.swap(tmp_vec);
        }
      }
      while (!tmp_vec.empty()) {
        std::string key_str = tmp_vec.back();
        tmp_vec.pop_back();
        smg_info(logger, "Adding key = %s to the iterator cache", key_str.c_str());  
        int32_t num_prefixes = parse_delimiter_entries(key_str, key_default_delimiter.c_str(), dir_entries, prefix_entries, file_name);
        //assert(!file_name.empty());
        if (file_name.empty()) {
          smg_warn(logger, "Empty file name found for key = %s", key_str.c_str());
        }
        
        std::string tmp_root = NKV_ROOT_PREFIX;
        if (num_prefixes == 0) {
          populate_iter_cache(tmp_root, file_name, false);
        } else {
          assert(dir_entries.size() == (uint32_t)(num_prefixes - 1));
          for (int32_t prefix_it = 0; prefix_it < num_prefixes; ++prefix_it) {
            if (prefix_it == 0) {
              populate_iter_cache(tmp_root, prefix_entries[prefix_it], false);
            }
            if (prefix_it == (num_prefixes -1)) {
              populate_iter_cache (prefix_entries[prefix_it], file_name, false);
            } else {
              populate_iter_cache(prefix_entries[prefix_it], dir_entries[prefix_it], false);
            }
          }
        }
        dir_entries.clear();
        prefix_entries.clear();
      }
      if (done_processing) {
        smg_info(logger, "nkv_path_thread_init::Thread finished work on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
        break;
      }
    }
  } else {
    smg_error(logger, "nkv_path_thread_init::Invalid work order = %d, exiting thread", what_work);
  }

}

int32_t NKVTargetPath::initialize_iter_cache (iterator_info*& iter_info) {

  uint32_t key_size = 0;
  uint8_t *it_buffer = (uint8_t *) iter_info->iter_list.it_list;
  assert(it_buffer != NULL);
  std::lock_guard<std::mutex> lck (cache_mtx);
  for(uint32_t i = 0; i < iter_info->iter_list.num_entries; i++) {
    key_size = *((unsigned int*)it_buffer);
    it_buffer += sizeof(unsigned int);
    std::string key_str ((const char*) it_buffer, key_size);
    path_vec.emplace_back(key_str);
    it_buffer += key_size;
  }
  return 0;
}

void NKVTargetPath::nkv_path_thread_func(int32_t what_work) {

  std::string device_thread_name = "nkv_" + dev_path.substr(5);
  int rc = pthread_setname_np(pthread_self(), device_thread_name.c_str());
  if (rc != 0) {
    smg_error(logger, "Error on setting worker thread on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
  } 

  if (what_work == 0 && listing_with_cached_keys) {
    #ifdef SAMSUNG_API
      kvs_iterator_context iter_ctx_open;
      iter_ctx_open.bitmask = 0xffffffff;
      unsigned int PREFIX_KV = 0;
      for (int i = 0; i < 4; i++){    
        PREFIX_KV |= (iter_prefix[i] << i*8);
      }

      iter_ctx_open.bit_pattern = PREFIX_KV;
      smg_info(logger, "nkv_path_thread_func::Opening iterator context with iter_prefix = %s, bitmask = 0x%x, bit_pattern = 0x%x, dev_path = %s, ip = %s",
               iter_prefix.c_str(), iter_ctx_open.bitmask, iter_ctx_open.bit_pattern, dev_path.c_str(), path_ip.c_str());
      iter_ctx_open.private1 = NULL;
      iter_ctx_open.private2 = NULL;
      iter_ctx_open.option.iter_type = KVS_ITERATOR_KEY;

      iterator_info* iter_info = new iterator_info(); 
      assert(iter_info != NULL);
 
      int ret = kvs_open_iterator(path_cont_handle, &iter_ctx_open, &iter_info->iter_handle);
      if(ret != KVS_SUCCESS) {
        if (ret != KVS_ERR_ITERATOR_OPEN) {
          smg_error(logger, "nkv_path_thread_func::iterator open fails on dev_path = %s, ip = %s with error 0x%x - %s\n", dev_path.c_str(),
                    path_ip.c_str(), ret, kvs_errstr(ret));
          if (iter_info) {
            delete(iter_info);
            iter_info = NULL;
          }
          return ;
        } else {
          smg_warn(logger, "nkv_path_thread_func::Iterator context is already opened, closing and reopening on dev_path = %s, ip = %s",
                   dev_path.c_str(), path_ip.c_str());
          kvs_iterator_context iter_ctx_close;
          iter_ctx_close.private1 = NULL;
          iter_ctx_close.private2 = NULL;

          ret = kvs_close_iterator(path_cont_handle, iter_info->iter_handle, &iter_ctx_close);
          if(ret != KVS_SUCCESS) {
            smg_error(logger, "nkv_path_thread_func::Failed to close iterator on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
          }
          ret = kvs_open_iterator(path_cont_handle, &iter_ctx_open, &iter_info->iter_handle);
          if(ret != KVS_SUCCESS) {
            smg_error(logger, "nkv_path_thread_func::iterator open fails on dev_path = %s, ip = %s with error 0x%x - %s\n", dev_path.c_str(),
                      path_ip.c_str(), ret, kvs_errstr(ret));
            if (iter_info) {
              delete(iter_info);
              iter_info = NULL;
            }
            return ;

          }
        }
      }

      /* Do iteration */
      static int total_entries = 0;
      iter_info->iter_list.size = iter_buff;
      uint8_t *buffer;
      buffer =(uint8_t*) kvs_malloc(iter_buff, 4096);
      memset(buffer, 0, iter_buff);
      iter_info->iter_list.it_list = (uint8_t*) buffer;

      kvs_iterator_context iter_ctx_next;
      iter_ctx_next.private1 = iter_info;
      iter_ctx_next.private2 = NULL;

      iter_info->iter_list.end = 0;
      iter_info->iter_list.num_entries = 0;

      while(1) {
        iter_info->iter_list.size = iter_buff;
        int ret = kvs_iterator_next(path_cont_handle, iter_info->iter_handle, &iter_info->iter_list, &iter_ctx_next);
        if(ret != KVS_SUCCESS) {
          smg_error(logger, "nkv_path_thread_func::iterator next fails on dev_path = %s, ip = %s with error 0x%x - %s\n", dev_path.c_str(), path_ip.c_str(),
                    ret, kvs_errstr(ret));
          break;
        }
        total_entries += iter_info->iter_list.num_entries;
        initialize_iter_cache (iter_info); 
        smg_info(logger, "nkv_path_thread_func::iterator next successful on dev_path = %s, ip = %s. Total: %u, this batch = %u, keys populated = %u, key prefixes populated = %u",
                 dev_path.c_str(), path_ip.c_str(), total_entries, iter_info->iter_list.num_entries, nkv_num_keys.load(), nkv_num_key_prefixes.load());

        if(iter_info->iter_list.end) {
          smg_info(logger, "nkv_path_thread_func::Done with all keys on dev_path = %s, ip = %s. Total: %u, keys populated = %u, key prefixes populated = %u",
                   dev_path.c_str(), path_ip.c_str(), total_entries, nkv_num_keys.load(), nkv_num_key_prefixes.load());
          break;

        } else {
          memset(iter_info->iter_list.it_list, 0,  iter_buff);
        }
        if (nkv_path_stopping) {
          smg_info(logger, "nkv_path_thread_func::Thread is stopping on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
          break;
        }
      }

      kvs_iterator_context iter_ctx_close;
      iter_ctx_close.private1 = NULL;
      iter_ctx_close.private2 = NULL;

      ret = kvs_close_iterator(path_cont_handle, iter_info->iter_handle, &iter_ctx_close);
      if(ret != KVS_SUCCESS) {
        smg_error(logger, "nkv_path_thread_func::Failed to close iterator on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
      }   

      if(buffer) kvs_free(buffer);
      if (iter_info) {
        delete(iter_info);
        iter_info = NULL;
      }
    #else
      struct iterator_info *iter_info = (struct iterator_info *)malloc(sizeof(struct iterator_info));
      iter_info->g_iter_mode.iter_type = KVS_ITERATOR_KEY;

      kvs_result ret;
      static int total_entries = 0;

      /* Open iterator */
      kvs_key_group_filter iter_fltr;

      iter_fltr.bitmask[0] = 0xffffffff & 0xff;
      iter_fltr.bitmask[1] = (0xffffffff & 0xff00) >> 8;
      iter_fltr.bitmask[2] = (0xffffffff & 0xff0000) >> 16;
      iter_fltr.bitmask[3] = (0xffffffff & 0xff000000) >> 24;
      unsigned int PREFIX_KV = 0;
      for (int i = 0; i < 4; i++){
        PREFIX_KV |= (iter_prefix[i] << i*8);
      }

      iter_fltr.bit_pattern[0] = PREFIX_KV & 0xff;
      iter_fltr.bit_pattern[1] = (PREFIX_KV & 0xff00) >> 8;
      iter_fltr.bit_pattern[2] = (PREFIX_KV & 0xff0000) >> 16;
      iter_fltr.bit_pattern[3] = (PREFIX_KV & 0xff000000) >> 24;

      smg_warn(logger, "Creating iter with iter_prefix = %s, p_kv = 0x%x, bitmask[0] = 0x%x, bitmask[1] = 0x%x, bitmask[2] = 0x%x, bitmask[3] = 0x%x, bit_pattern[0] = 0x%x, bit_pattern[1] = 0x%x, bit_pattern[2] = 0x%x, bit_pattern[3] = 0x%x, dev_path = %s, ip = %s",
               iter_prefix.c_str(), PREFIX_KV, iter_fltr.bitmask[0], iter_fltr.bitmask[1], iter_fltr.bitmask[2], iter_fltr.bitmask[3], iter_fltr.bit_pattern[0],
               iter_fltr.bit_pattern[1], iter_fltr.bit_pattern[2], iter_fltr.bit_pattern[3], dev_path.c_str(), path_ip.c_str());

      ret = kvs_create_iterator(path_ks_handle, &(iter_info->g_iter_mode), &iter_fltr, &(iter_info->iter_handle));
      if(ret != KVS_SUCCESS) {
        smg_error(logger, "nkv_path_thread_func::iterator creation fails on dev_path = %s, ip = %s with error 0x%x \n", dev_path.c_str(),
                  path_ip.c_str(), ret);
        free(iter_info);
        return ;
      }
      /* Do iteration */
      iter_info->iter_list.size = iter_buff;
      uint8_t *buffer;
      buffer =(uint8_t*)kvs_malloc(iter_buff, 4096);
      iter_info->iter_list.it_list = (uint8_t*) buffer;

      iter_info->iter_list.end = 0;
      iter_info->iter_list.num_entries = 0;

      while(1) {
        iter_info->iter_list.size = iter_buff;
        memset(iter_info->iter_list.it_list, 0, iter_buff);
        ret = kvs_iterate_next(path_ks_handle, iter_info->iter_handle, &(iter_info->iter_list));
        if(ret != KVS_SUCCESS) {
          smg_error(logger, "nkv_path_thread_func::iterator next fails on dev_path = %s, ip = %s with error 0x%x \n", dev_path.c_str(), path_ip.c_str(),
                    ret);
          free(iter_info);
          kvs_free(buffer);
          return ;
        }
        
        total_entries += iter_info->iter_list.num_entries;
        initialize_iter_cache (iter_info); 
        smg_info(logger, "nkv_path_thread_func::iterator next successful on dev_path = %s, ip = %s. Total: %u, this batch = %u, keys populated = %u, key prefixes populated = %u",
                 dev_path.c_str(), path_ip.c_str(), total_entries, iter_info->iter_list.num_entries, nkv_num_keys.load(), nkv_num_key_prefixes.load());

        if(iter_info->iter_list.end) {
          smg_info(logger, "nkv_path_thread_func::Done with all keys on dev_path = %s, ip = %s. Total: %u, keys populated = %u, key prefixes populated = %u",
                   dev_path.c_str(), path_ip.c_str(), total_entries, nkv_num_keys.load(), nkv_num_key_prefixes.load());
          break;
        } else {
          memset(iter_info->iter_list.it_list, 0,  iter_buff);
        }
        if (nkv_path_stopping) {
          smg_info(logger, "nkv_path_thread_func::Thread is stopping on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
          break;
        }
      }

      /* Close iterator */
      ret = kvs_delete_iterator(path_ks_handle, iter_info->iter_handle);
      if(ret != KVS_SUCCESS) {
        smg_error(logger, "nkv_path_thread_func::Failed to delete iterator on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
        kvs_free(buffer);
        free(iter_info);
        return ;
      }
  
      if(buffer) kvs_free(buffer);
      if(iter_info) free(iter_info);
    #endif
    smg_info(logger, "nkv_path_thread_func::Finished iteration job for dev_path = %s, ip = %s, exiting the thread", dev_path.c_str(), path_ip.c_str());
  } else {
    smg_error(logger, "nkv_path_thread_func::Invalid work order = %d, exiting thread", what_work);
  }
 
}

nkv_result NKVTargetPath::do_list_keys_from_path(uint32_t* num_keys_iterted, iterator_info*& iter_info, uint32_t* max_keys, nkv_key* keys, const char* prefix,
                                                const char* delimiter) {
  
  nkv_result stat = NKV_SUCCESS;
  if (!iter_info->visited_path.empty()) {
    auto v_iter = iter_info->visited_path.find(path_hash);
    if (v_iter != iter_info->visited_path.end()) {
      smg_warn(logger, "This path is alrady been iterated, path = %u, dev_path = %s, ip = %s", 
                path_hash, dev_path.c_str(), path_ip.c_str());
      return NKV_SUCCESS;
    }
  }

  if (listing_with_cached_keys) {
    /*std::string key_prefix_iter (NKV_ROOT_PREFIX);
    if (prefix) {
      key_prefix_iter = prefix;
    }*/
    int32_t shard_id = 0;
    if (0 == iter_info->network_path_hash_iterating) {
      if (nkv_dynamic_logging) {
        smg_alert(logger, "NKV listing request for prefix = %s, delimiter = %s, dev_path = %s, ip = %s",
                 prefix ? prefix: "NULL", delimiter ? delimiter:"NULL", dev_path.c_str(), path_ip.c_str());
      }

      std::string key_prefix_iter (NKV_ROOT_PREFIX);
      if (prefix) {
        key_prefix_iter = prefix;
      }

      iter_info->network_path_hash_iterating = path_hash;
      std::size_t key_prefix = std::hash<std::string>{}(key_prefix_iter);
      iter_info->key_prefix_hash = key_prefix;
      shard_id = key_prefix % nkv_listing_cache_num_shards;
      int32_t r = pthread_rwlock_rdlock(&cache_rw_lock_list[shard_id]);
      if (r != 0) {
        smg_error(logger, "RW lock acquisition failed for shard_id = %d, prefix = %s", shard_id, key_prefix_iter.c_str());
        assert (r == 0);
      }

      //cache_mtx.lock();
      /*auto list_iter_main = listing_keys.find(key_prefix_iter);*/
      auto list_iter_main = listing_keys[shard_id].find(iter_info->key_prefix_hash);
      if (list_iter_main == listing_keys[shard_id].end()) {
        iter_info->visited_path.insert(path_hash);
        iter_info->network_path_hash_iterating = 0;
        r = pthread_rwlock_unlock(&cache_rw_lock_list[shard_id]);
        if (r != 0) {
          smg_error(logger, "RW lock unlock failed for shard_id = %d, prefix = %s", shard_id, key_prefix_iter.c_str());
          assert (r == 0);
        }
        //cache_mtx.unlock();
        return stat;
      }
      
      iter_info->cached_key_iter = list_iter_main->second.cbegin();
      /*
      std::string prefix_str(prefix);
      bool pending_iter = false; 
      std::lock_guard<std::mutex> lck_iter (iter_mtx);
      auto list_iter_track = listing_keys_track_iter.find(key_prefix);
      if (list_iter_track == listing_keys_track_iter.end()) {
        listing_keys_track_iter[key_prefix] = 1;
      } else {
        listing_keys_track_iter[key_prefix]++;
        pending_iter = true;
      }

      std::lock_guard<std::mutex> lck (cache_mtx);
      auto list_iter_main = listing_keys.find(key_prefix);
      if (list_iter_main == listing_keys.end()) {
        iter_info->visited_path.insert(path_hash);
        return stat;
        
      } else {
        uint32_t num_keys = list_iter_main->second.size();
        if ((num_keys > nkv_iter_max_key_threshold) ||  && !pending_iter) {
          create_modify_iter_meta(prefix_str);
        }
      }*/
      
    } else {
      shard_id = iter_info->key_prefix_hash % nkv_listing_cache_num_shards;
      int32_t r = pthread_rwlock_rdlock(&cache_rw_lock_list[shard_id]);
      if (r != 0) {
        smg_error(logger, "RW lock acquisition failed for shard_id = %d, prefix = %s", shard_id, prefix ? prefix : NKV_ROOT_PREFIX.c_str());
        assert (r == 0);
      }
      //cache_mtx.lock();
      iter_info->cached_key_iter = listing_keys[shard_id][iter_info->key_prefix_hash].find(iter_info->key_to_start_iter);
    }

    //for ( ; iter_info->cached_key_iter != listing_keys[key_prefix_iter.c_str()].cend(); iter_info->cached_key_iter++) {
    for ( ; iter_info->cached_key_iter != listing_keys[shard_id][iter_info->key_prefix_hash].cend(); iter_info->cached_key_iter++) {
      if ((*max_keys - *num_keys_iterted) == 0) {
        smg_warn(logger,"Not enough out buffer space to accomodate next cached key for dev_path = %s, ip = %s, remaining key space = %u",
                 dev_path.c_str(), path_ip.c_str(), (*max_keys - *num_keys_iterted));
        *max_keys = *num_keys_iterted;
        stat = NKV_ITER_MORE_KEYS;
        iter_info->key_to_start_iter = (*(iter_info->cached_key_iter));
        break;
      }
      std::string one_key =  (*(iter_info->cached_key_iter));
      uint32_t one_key_len = one_key.length();
      assert(one_key_len <= 255);
      smg_info(logger, "Adding key = %s for prefix = %s during iteration", one_key.c_str(), prefix ? prefix : NKV_ROOT_PREFIX.c_str());
      filter_and_populate_keys_from_path (max_keys, keys, (char*)one_key.c_str(), one_key_len, num_keys_iterted, NULL, NULL, iter_info, true);
    }

    //if (iter_info->cached_key_iter == listing_keys[key_prefix_iter.c_str()].cend()) {
    if (iter_info->cached_key_iter == listing_keys[shard_id][iter_info->key_prefix_hash].cend()) {
      iter_info->visited_path.insert(path_hash);
      iter_info->network_path_hash_iterating = 0;
      //cache_mtx.unlock();
      smg_info(logger, "Done with all keys on dev_path = %s, ip = %s. Total: %u",
                dev_path.c_str(), path_ip.c_str(), *num_keys_iterted);
    }

    int32_t r = pthread_rwlock_unlock(&cache_rw_lock_list[shard_id]);
    if (r != 0) {
      smg_error(logger, "RW lock unlock failed for shard_id = %d, prefix = %s", shard_id, prefix ? prefix : NKV_ROOT_PREFIX.c_str());
      assert (r == 0);
    }
    
    //cache_mtx.unlock();  
    return stat;
  }

#if 0
  if (listing_with_cached_keys) {
    uint32_t number_cached_keys = 0;
    uint32_t number_deleted_keys = 0;
    uint32_t number_iter_keys = 0;

    if (0 == iter_info->network_path_hash_iterating) {
      {
        std::lock_guard<std::mutex> lck (cache_mtx);
        if (cached_keys.size() != 0 || iter_key_set.size() != 0) {
          //iter_info->dup_chached_key_set = new std::unordered_set<std::string>();
          number_cached_keys = cached_keys.size();
          number_deleted_keys = deleted_cached_keys.size();

          if (iter_key_set.size() == 0 && nkv_outstanding_iter_on_path == 0) {
            //First iterator call after boot up
            cached_keys.swap(iter_key_set);
          }
          else if (((cached_keys.size() > 0) || (deleted_cached_keys.size() > 0)) &&
              (nkv_outstanding_iter_on_path == 0)) {
            
            //Update the diff, update deleted list first and then cache list
            for (auto del_iter = deleted_cached_keys.begin(); del_iter != deleted_cached_keys.end(); ++del_iter) {
               iter_key_set.erase(*del_iter);
            }
            deleted_cached_keys.clear();
            for (auto cache_iter = cached_keys.begin(); cache_iter != cached_keys.end(); ++cache_iter) {
               iter_key_set.emplace(*cache_iter);
            }
            cached_keys.clear();
          }
          if (iter_key_set.size() == 0) {
            iter_info->visited_path.insert(path_hash);
            return stat;
          }
          iter_info->dup_chached_key_set = &iter_key_set;
          iter_info->cached_key_iter = iter_info->dup_chached_key_set->cbegin();
          iter_info->network_path_hash_iterating = path_hash;
          nkv_outstanding_iter_on_path.fetch_add(1, std::memory_order_relaxed);
          number_iter_keys = iter_info->dup_chached_key_set->size();
        } else {
          iter_info->visited_path.insert(path_hash);
          return stat;
        }
      }
      assert(iter_info->dup_chached_key_set != NULL);
      //number_iter_keys = iter_info->dup_chached_key_set->size();
      //if (nkv_outstanding_iter_on_path <= 1) {
      smg_info(logger, "Opened Cache Iter context on dev_path = %s, ip = %s, net_path_hash_iterating = %u, total iter keys = %u, cached_keys = %u, deleted_keys = %u, iter_pending = %u",
                 dev_path.c_str(), path_ip.c_str(), iter_info->network_path_hash_iterating, number_iter_keys, number_cached_keys, number_deleted_keys, nkv_outstanding_iter_on_path.load());  
      //}
    } else {
      assert(iter_info->dup_chached_key_set != NULL);
      //number_iter_keys = iter_info->dup_chached_key_set->size();
      smg_info(logger, "Cache Iter context is already opened on dev_path = %s, ip = %s, network_path_hash_iterating = %u, total iter keys = %u",
               dev_path.c_str(), path_ip.c_str(), iter_info->network_path_hash_iterating, number_iter_keys);
    }
    //Iterate over cahched keys
    for ( ; iter_info->cached_key_iter != iter_info->dup_chached_key_set->cend(); iter_info->cached_key_iter++) {
      if ((*max_keys - *num_keys_iterted) == 0) {
        smg_warn(logger,"Not enough out buffer space to accomodate next cached key for dev_path = %s, ip = %s, remaining key space = %u, total iter keys = %u",
                 dev_path.c_str(), path_ip.c_str(), (*max_keys - *num_keys_iterted), number_iter_keys);
        *max_keys = *num_keys_iterted;
        stat = NKV_ITER_MORE_KEYS;
        break;
      }
      std::string one_key =  (*(iter_info->cached_key_iter));
      uint32_t one_key_len = one_key.length();
      assert(one_key_len <= 255);
      filter_and_populate_keys_from_path (max_keys, keys, (char*)one_key.c_str(), one_key_len, num_keys_iterted, prefix, delimiter, iter_info, true);
    }

    if (iter_info->cached_key_iter == iter_info->dup_chached_key_set->cend()) {
      nkv_outstanding_iter_on_path.fetch_sub(1, std::memory_order_relaxed);
      iter_info->visited_path.insert(path_hash);
      iter_info->network_path_hash_iterating = 0;
      iter_info->dup_chached_key_set = NULL;
      smg_info(logger, "Done with all keys on dev_path = %s, ip = %s. Total: %u",
                dev_path.c_str(), path_ip.c_str(), *num_keys_iterted);
    }
    return stat;  
  }
#endif
  #ifdef SAMSUNG_API
    //Disk based iteration
    if (0 == iter_info->network_path_hash_iterating) {
      smg_info(logger, "Opening iterator and creating handle on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());

      kvs_iterator_context iter_ctx_open;

      iter_ctx_open.bitmask = 0xffffffff;
      unsigned int PREFIX_KV = 0;
      for (int i = 0; i < 4; i++){
        PREFIX_KV |= (iter_prefix[i] << i*8);
      }

      iter_ctx_open.bit_pattern = PREFIX_KV;
      smg_info(logger, "Opening iterator with iter_prefix = %s, bitmask = 0x%x, bit_pattern = 0x%x", 
               iter_prefix.c_str(), iter_ctx_open.bitmask, iter_ctx_open.bit_pattern);
      iter_ctx_open.private1 = NULL;
      iter_ctx_open.private2 = NULL;

      iter_ctx_open.option.iter_type = KVS_ITERATOR_KEY;
      //Lock because we can have only one iterator open for the same prefix/bitmask at one time
      iter_mtx.lock();
      int ret = kvs_open_iterator(path_cont_handle, &iter_ctx_open, &iter_info->iter_handle);
      if(ret != KVS_SUCCESS) {
        if (ret != KVS_ERR_ITERATOR_OPEN) {
          smg_error(logger, "iterator open fails on dev_path = %s, ip = %s with error 0x%x - %s\n", dev_path.c_str(),
                    path_ip.c_str(), ret, kvs_errstr(ret));
          return map_kvs_err_code_to_nkv_err_code(ret);
        } else {
          smg_warn(logger, "Iterator context is already opened, closing and reopening on dev_path = %s, ip = %s, network_path_hash_iterating = %u",
                   dev_path.c_str(), path_ip.c_str(), iter_info->network_path_hash_iterating);
          kvs_iterator_context iter_ctx_close;
          iter_ctx_close.private1 = NULL;
          iter_ctx_close.private2 = NULL;

          ret = kvs_close_iterator(path_cont_handle, iter_info->iter_handle, &iter_ctx_close);
          if(ret != KVS_SUCCESS) {
            smg_error(logger, "Failed to close iterator on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
          }
          ret = kvs_open_iterator(path_cont_handle, &iter_ctx_open, &iter_info->iter_handle);
          if(ret != KVS_SUCCESS) {
            smg_error(logger, "iterator open fails on dev_path = %s, ip = %s with error 0x%x - %s\n", dev_path.c_str(),
                      path_ip.c_str(), ret, kvs_errstr(ret));
            return map_kvs_err_code_to_nkv_err_code(ret);

          }
        }
      }
      iter_info->network_path_hash_iterating = path_hash;
    } else {
      smg_info(logger, "Iterator context is already opened on dev_path = %s, ip = %s, network_path_hash_iterating = %u", 
               dev_path.c_str(), path_ip.c_str(), iter_info->network_path_hash_iterating);
    }

    /* Do iteration */
    static int total_entries = 0;
    iter_info->iter_list.size = iter_buff;
    uint8_t *buffer;
    buffer =(uint8_t*) kvs_malloc(iter_buff, 4096);
    memset(buffer, 0, iter_buff);
    iter_info->iter_list.it_list = (uint8_t*) buffer;

    kvs_iterator_context iter_ctx_next;
    iter_ctx_next.private1 = iter_info;
    iter_ctx_next.private2 = NULL;

    iter_info->iter_list.end = 0;
    iter_info->iter_list.num_entries = 0;  

    while(1) {
      iter_info->iter_list.size = iter_buff;
      int ret = kvs_iterator_next(path_cont_handle, iter_info->iter_handle, &iter_info->iter_list, &iter_ctx_next);
      if(ret != KVS_SUCCESS) {
        smg_error(logger, "iterator next fails on dev_path = %s, ip = %s with error 0x%x - %s\n", dev_path.c_str(), path_ip.c_str(),
                  ret, kvs_errstr(ret));
        if(buffer) kvs_free(buffer);
        return map_kvs_err_code_to_nkv_err_code(ret);
      }
      total_entries += iter_info->iter_list.num_entries;

      stat = find_keys_from_path(max_keys, keys, iter_info, num_keys_iterted, prefix, delimiter);
      smg_info(logger, "iterator next successful on dev_path = %s, ip = %s. Total: %u, this batch = %u, keys populated so far = %u",
               dev_path.c_str(), path_ip.c_str(), total_entries, iter_info->iter_list.num_entries, *num_keys_iterted);

      if(iter_info->iter_list.end) {
        smg_info(logger, "Done with all keys on dev_path = %s, ip = %s. Total: %u, this batch = %u", 
                  dev_path.c_str(), path_ip.c_str(), total_entries, *num_keys_iterted);
        /* Close iterator */
        kvs_iterator_context iter_ctx_close;
        iter_ctx_close.private1 = NULL;
        iter_ctx_close.private2 = NULL;

        int ret = kvs_close_iterator(path_cont_handle, iter_info->iter_handle, &iter_ctx_close);
        if(ret != KVS_SUCCESS) {
          smg_error(logger, "Failed to close iterator on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());
        }
        iter_mtx.unlock();
        iter_info->visited_path.insert(path_hash);
        iter_info->network_path_hash_iterating = 0;
 
        break;

      } else {
        if ((*max_keys - *num_keys_iterted) < iter_info->iter_list.num_entries) {
          smg_warn(logger,"Probably not enough out buffer space to accomodate next set of keys for dev_path = %s, ip = %s, remaining key space = %u, avg number of entries per call = %u",
                   dev_path.c_str(), path_ip.c_str(), (*max_keys - *num_keys_iterted), iter_info->iter_list.num_entries);
          *max_keys = *num_keys_iterted;
          stat = NKV_ITER_MORE_KEYS;
          break;
        }
        memset(iter_info->iter_list.it_list, 0,  iter_buff);
      }
    }
    if(buffer) kvs_free(buffer);
  #endif
  return stat;

}
