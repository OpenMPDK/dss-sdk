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

thread_local int32_t core_running_driver_thread = -1;
std::string iter_prefix = "0000";
#define iter_buff (32*1024)
//#define ITER_PREFIX "meta"

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
  //cache keys for listing (till iterator issue is fixed)
  /*if (listing_with_cached_keys) {
    std::string key_str ((char*) n_key->key, n_key->length);
    std::size_t found = key_str.find(iter_prefix);
    if (found != std::string::npos && found == 0) {
      std::lock_guard<std::mutex> lck (cache_mtx);
      cached_keys.insert(key_str);
    }
  }*/

  kvs_store_context put_ctx = {option, 0, 0};
  if (!post_fn) {
    const kvs_key  kvskey = { n_key->key, (kvs_key_t)n_key->length};
    const kvs_value kvsvalue = { n_value->value, (uint32_t)n_value->length, 0, 0};

    int ret = kvs_store_tuple(path_cont_handle, &kvskey, &kvsvalue, &put_ctx);    
    if(ret != KVS_SUCCESS ) {
      smg_error(logger, "store tuple failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s", 
                ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
      return map_kvs_err_code_to_nkv_err_code(ret);
    }
    //cache keys for listing (till iterator issue is fixed)
    if (listing_with_cached_keys) {
      std::string key_str ((char*) n_key->key, n_key->length);
      std::size_t found = key_str.find(iter_prefix);
      if (found != std::string::npos && found == 0) {
        std::lock_guard<std::mutex> lck (cache_mtx);
        cached_keys.insert(key_str);
      }
    }

  } else { //Async
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
  if (!post_fn) {
    const kvs_key  kvskey = { n_key->key, (kvs_key_t)n_key->length};
    kvs_value kvsvalue = { n_value->value, (uint32_t)n_value->length, 0, 0};

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

    n_value->actual_length = kvsvalue.actual_value_size;
    if (n_value->actual_length == 0) {
      smg_error(logger, "Retrieve tuple Success with actual length = 0, Retrying once !! key = %s, dev_path = %s, ip = %s",
                n_key->key, dev_path.c_str(), path_ip.c_str());

      ret = kvs_retrieve_tuple(path_cont_handle, &kvskey, &kvsvalue, &ret_ctx);
      if(ret != KVS_SUCCESS ) {
        smg_error(logger, "Retrieve tuple failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s",
                  ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
        return map_kvs_err_code_to_nkv_err_code(ret);
      }

      n_value->actual_length = kvsvalue.actual_value_size;
      if (n_value->actual_length == 0) {
        smg_error(logger, "Retrieve tuple Success with actual length = 0, done Retrying !! key = %s, dev_path = %s, ip = %s",
                  n_key->key, dev_path.c_str(), path_ip.c_str());

      }
    }
    assert(NULL != n_value->value);
  } else {//Async
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

  }

  return NKV_SUCCESS;
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

  const kvs_key  kvskey = { n_key->key, (kvs_key_t)n_key->length};
  kvs_delete_context del_ctx = { {false}, 0, 0};

  if (!post_fn) {
    int ret = kvs_delete_tuple(path_cont_handle, &kvskey, &del_ctx);
    if(ret != KVS_SUCCESS ) {
      smg_error(logger, "Delete tuple failed with error 0x%x - %s, key = %s, dev_path = %s, ip = %s", 
                ret, kvs_errstr(ret), n_key->key, dev_path.c_str(), path_ip.c_str());
      return map_kvs_err_code_to_nkv_err_code(ret);
    }
    if (listing_with_cached_keys && (cached_keys.size() != 0)) {
      std::string key_str ((char*) n_key->key, n_key->length);
      std::lock_guard<std::mutex> lck (cache_mtx);
      cached_keys.erase(key_str);
    }

  } else {

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
    uint32_t number_cached_keys = 0;
    if (0 == iter_info->network_path_hash_iterating) {
      {
        std::lock_guard<std::mutex> lck (cache_mtx);
        if (cached_keys.size() != 0) {
          iter_info->dup_chached_key_set = new std::unordered_set<std::string>();
          //cached_keys.swap(*(iter_info->dup_chached_key_set));
          *(iter_info->dup_chached_key_set) = cached_keys;
          iter_info->cached_key_iter = iter_info->dup_chached_key_set->cbegin();
          iter_info->network_path_hash_iterating = path_hash;
        } else {
          iter_info->visited_path.insert(path_hash);
          return stat;
        }
      }
      number_cached_keys = iter_info->dup_chached_key_set->size();
      smg_info(logger, "Successfully opened Cache Iterator context on dev_path = %s, ip = %s, network_path_hash_iterating = %u, total keys = %u",
               dev_path.c_str(), path_ip.c_str(), iter_info->network_path_hash_iterating, number_cached_keys);  
    } else {
      number_cached_keys = iter_info->dup_chached_key_set->size();
      smg_info(logger, "Cache Iterator context is already opened on dev_path = %s, ip = %s, network_path_hash_iterating = %u, total keys = %u",
               dev_path.c_str(), path_ip.c_str(), iter_info->network_path_hash_iterating, number_cached_keys);
      //iter_info->cached_key_iter++;
    }
    //Iterate over cahched keys
    for ( ; iter_info->cached_key_iter != iter_info->dup_chached_key_set->cend(); iter_info->cached_key_iter++) {
      if ((*max_keys - *num_keys_iterted) == 0) {
        smg_warn(logger,"Not enough out buffer space to accomodate next cached key for dev_path = %s, ip = %s, remaining key space = %u, total keys = %u",
                 dev_path.c_str(), path_ip.c_str(), (*max_keys - *num_keys_iterted), number_cached_keys);
        *max_keys = *num_keys_iterted;
        stat = NKV_ITER_MORE_KEYS;
        break;
      }
      
      filter_and_populate_keys_from_path (max_keys, keys, (char*)(*(iter_info->cached_key_iter)).c_str(), (*(iter_info->cached_key_iter)).length(), 
                                         num_keys_iterted, prefix, delimiter, iter_info, true);
    }

    if (iter_info->cached_key_iter == iter_info->dup_chached_key_set->cend()) {
      iter_info->visited_path.insert(path_hash);
      iter_info->network_path_hash_iterating = 0;
      if (iter_info->dup_chached_key_set) {
        delete iter_info->dup_chached_key_set;
        iter_info->dup_chached_key_set = NULL;
      }
      smg_info(logger, "Done with all keys on dev_path = %s, ip = %s. Total: %u",
                dev_path.c_str(), path_ip.c_str(), *num_keys_iterted);
    }
    return stat;  
  }

  if (0 == iter_info->network_path_hash_iterating) {
    smg_info(logger, "Opening iterator and creating handle on dev_path = %s, ip = %s", dev_path.c_str(), path_ip.c_str());

    kvs_iterator_context iter_ctx_open;

    iter_ctx_open.bitmask = 0xffffffff;
    //iter_ctx_open.bitmask = 0xffff0000;
    //char prefix_str[5] = iter_prefix.c_str();
    //char prefix_str[5] = "0000";
    unsigned int PREFIX_KV = 0;
    for (int i = 0; i < 4; i++){
      //PREFIX_KV |= (prefix_str[i] << i*8);
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
    //*num_keys_iterted += iter_info->iter_list.num_entries;

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
  return stat;

}
