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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <mutex>
#include <atomic>
#include <queue>
#include "nkv_api.h"
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include "csmglogger.h"
#include <thread>

c_smglogger* logger = NULL;
std::atomic<int> submitted(0);
std::atomic<int> completed(0);
//std::atomic<int> cur_qdepth[32];
const char* S3_DELIMITER = "/";
#define NKV_TEST_META_VAL_LEN 4096
#define NKV_TEST_META_KEY_LEN 100

#define NKV_TEST_OP_LOCK_UNLOCK (6)

struct nkv_thread_args{
  int id;
  uint32_t klen;
  uint32_t vlen;
  int count;
  int op_type;
  char* key_prefix;
  nkv_io_context* ioctx;
  uint32_t ioctx_cnt;
  uint64_t nkv_handle;
  nkv_store_option* s_option;
  nkv_retrieve_option* r_option;
  nkv_lock_option* lock_option;
  nkv_unlock_option* unlock_option;
  int check_integrity;
  int hex_dump;
  int is_mixed;
  int alignment;
  void* app_ustat_ctx;
};

void memHexDump (void *addr, int len) {
    int i;
    unsigned char buff[17];       
    unsigned char *pc = (unsigned char *)addr;
     
    for (i = 0; i < len; i++) {
      if ((i % 16) == 0) {
        if (i != 0)
          printf ("  %s\n", buff);

        printf ("  %04x ", i);
      }
      printf (" %02x", pc[i]);

      if ((pc[i] < 0x20) || (pc[i] > 0x7e))
        buff[i % 16] = '.';
      else
        buff[i % 16] = pc[i];
      buff[(i % 16) + 1] = '\0';
    }

    
    while ((i % 16) != 0) {
      printf ("   ");
      i++;
    }

    printf ("  %s\n", buff);
}

void nkv_aio_complete (nkv_aio_construct* op_data, int32_t num_op) {

  if (!op_data) {
    smg_error(logger, "NKV Async IO returned NULL op_Data, ignoring..");
    return;
  }
  if (op_data->result != 0) {
    smg_error(logger, "NKV Async IO failed: op = %d, key = %s, result = 0x%x\n",
              op_data->opcode, op_data->key.key? (char*)op_data->key.key: 0 , op_data->result);

  } else {
    smg_info(logger, "NKV Async IO Succeeded: op = %d, key = %s, result = 0x%x\n",
              op_data->opcode, op_data->key.key? (char*)op_data->key.key: 0 , op_data->result);

    if (op_data->opcode == 0) { //GET
      smg_info(logger, "NKV Async GET returned actual data length = %u, key = %s", op_data->value.actual_length,
               op_data->key.key? (char*)op_data->key.key: 0);

      /*if (op_data->value.actual_length == 0)
        smg_error(logger, "NKV Async GET returned 0 length buffer !!, Aborting..");

      assert(op_data->value.actual_length != 0);*/
    }

  }
  nkv_postprocess_function* post_fn = (nkv_postprocess_function*)op_data->private_data_2;
  /*std::atomic<int>* path_cur_qd = (std::atomic<int>*) op_data->private_data_1;
  if (!path_cur_qd) {
    smg_error(logger, "NKV Async IO returned with NULL private data: op = %d, key = %s, result = 0x%x\n",
              op_data->opcode, op_data->key.key? (char*)op_data->key.key: 0 , op_data->result);

  } else {
    (*path_cur_qd).fetch_sub(1, std::memory_order_relaxed);
  }*/
  if (op_data->private_data_1) {
    int32_t* is_done = (int32_t*) op_data->private_data_1;
    smg_info(logger, "NKV Async IO returned with private data 1: op = %d, key = %s, data = %d\n",
              op_data->opcode, op_data->key.key? (char*)op_data->key.key: 0 , *is_done);
    
    *is_done = 1;
  }
  completed.fetch_add(1, std::memory_order_relaxed);

  if (op_data->key.key) {
    nkv_free(op_data->key.key);
  } 
  if (op_data->value.value) {
    nkv_free(op_data->value.value);
  }

  if (post_fn)
    free(post_fn);


}

void print(boost::property_tree::ptree & pt)
{
    std::string subsystem_nqn_id = pt.get<std::string>("subsystem_nqn_id");
    smg_info(logger, subsystem_nqn_id.c_str());
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("subsystem_transport")) {
      assert(v.first.empty());
      boost::property_tree::ptree pc = v.second;
      std::string subsystem_address = pc.get<std::string>("subsystem_address");
      int32_t numa_aligned = pc.get<bool>("subsystem_interface_numa_aligned");
      smg_info(logger, subsystem_address.c_str());
      smg_info(logger, "numa_aligned = %d", numa_aligned);
        
    }

    return;

    /*std::string subsystem_nqn_id = pt.get<std::string>("mount_point");
    smg_info(logger, subsystem_nqn_id.c_str());
    return;*/

    using boost::property_tree::ptree;
    ptree::const_iterator end = pt.end();
    for (ptree::const_iterator it = pt.begin(); it != end; ++it) {
        if (!it->second.get_value<std::string>().empty()) {
          std::stringstream ss;
          ss<< it->first << ": " << it->second.get_value<std::string>() << std::endl;
          smg_info(logger, ss.str().c_str());
        } else {
          //smg_info(logger, "\n\n");
        }
        //print(it->second);
    }
}

void *stat_thread(void *args)
{
  nkv_thread_args *targs = (nkv_thread_args *)args;
  std::string th_name = "nkv_stat_" + std::to_string(targs->id);
  pthread_setname_np(pthread_self(), th_name.c_str());
  for (uint32_t cnt_iter = 0; cnt_iter < targs->ioctx_cnt; cnt_iter++) {
    nkv_mgmt_context mg_ctx = {0};
    mg_ctx.is_pass_through = 1;

    mg_ctx.container_hash = targs->ioctx[cnt_iter].container_hash;
    mg_ctx.network_path_hash = targs->ioctx[cnt_iter].network_path_hash;
    nkv_path_stat p_stat = {0};
    nkv_result stat = nkv_get_path_stat (targs->nkv_handle, &mg_ctx, &p_stat);

    if (stat == NKV_SUCCESS) {
      smg_alert(logger, "NKV stat call succeeded, thread_name = %s", th_name.c_str());
      smg_alert(logger, "NKV path mount = %s, path capacity = %lld Bytes, path usage = %lld Bytes, path util percentage = %f, thread_name = %s",
              p_stat.path_mount_point, (long long)p_stat.path_storage_capacity_in_bytes, (long long)p_stat.path_storage_usage_in_bytes,
              p_stat.path_storage_util_percentage, th_name.c_str());
    } else {
      smg_error(logger, "NKV stat call failed, error = %d, thread_name = %s", stat, th_name.c_str());
    }
  }
  return 0;
}

void *iothread(void *args)
{
  nkv_thread_args *targs = (nkv_thread_args *)args;
  std::string th_name = "nkvt_t_" + std::to_string(targs->id);
  pthread_setname_np(pthread_self(), th_name.c_str());
  char *cmpval   = (char*)nkv_zalloc(targs->vlen); 
  nkv_result status = NKV_SUCCESS;
  //do_io(targs->id, targs->cont_hd, targs->count, targs->klen, targs->vlen, targs->op_type);


  for(int32_t iter = 0; iter < targs->count; iter++) {
    char *key_name   = (char*)nkv_malloc(targs->klen);
    memset(key_name, 0, targs->klen);
    sprintf(key_name, "%s_%d_%u", targs->key_prefix, targs->id, iter);
    uint32_t klen = targs->klen;//strlen (key_name);
    char* val = NULL;
    
    if (!targs->alignment) {
      val   = (char*)nkv_zalloc(targs->vlen);
      //memset(val, 0, targs->vlen);
    } else {
      val   = (char*)aligned_alloc(targs->alignment, targs->vlen);
      if ((uint64_t)val % targs->alignment != 0) {
        smg_error(logger, "Non %u Byte Aligned value address = 0x%x", targs->alignment, val);
        assert(0);
      }
    }
    //memset(val, 0, targs->vlen);
    //char* val = (char*) calloc (targs->vlen, sizeof(char));
    const nkv_key  nkvkey = { (void*)key_name, klen};
    nkv_value nkvvalue = { (void*)val, targs->vlen, 0 };
    
    if (targs->app_ustat_ctx) {
      status = nkv_inc_to_counter(targs->nkv_handle, targs->app_ustat_ctx);
      if (status != NKV_SUCCESS) {
        smg_error(logger, "NKV stat inc api failed !, error = %u", status);
      }
    }

    switch(targs->op_type) {
      case 0: /*PUT*/
        {
      
          //if ((targs->is_mixed) && ( iter % 3 == 0)) {
          if (targs->is_mixed) {
            char*meta_key_1   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            char*meta_key_2   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            char*meta_key_3   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            char*meta_key_4   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            memset(meta_key_1, 0, NKV_TEST_META_KEY_LEN);
            memset(meta_key_2, 0, NKV_TEST_META_KEY_LEN);
            memset(meta_key_3, 0, NKV_TEST_META_KEY_LEN);
            memset(meta_key_4, 0, NKV_TEST_META_KEY_LEN);
            sprintf(meta_key_1, "meta/tmp/bucket_%u/%u/%s/xl.json", targs->id, iter, targs->key_prefix);
            sprintf(meta_key_2, "meta/tmp/bucket_%u/%u/%s/part1.json", targs->id, iter, targs->key_prefix);
            sprintf(meta_key_3, "meta/bucket_%u/%u/%s/xl.json", targs->id, iter, targs->key_prefix);
            sprintf(meta_key_4, "meta/bucket_%u/%u/%s/part1.json", targs->id, iter, targs->key_prefix);
            char* meta_val_1 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            char* meta_val_2 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            char* meta_val_3 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            char* meta_val_4 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            memset(meta_val_1, 0, NKV_TEST_META_VAL_LEN);
            memset(meta_val_2, 0, NKV_TEST_META_VAL_LEN);
            memset(meta_val_3, 0, NKV_TEST_META_VAL_LEN);
            memset(meta_val_4, 0, NKV_TEST_META_VAL_LEN);
            const nkv_key  nkvkeymeta1 = { (void*)meta_key_1, NKV_TEST_META_KEY_LEN};
            const nkv_key  nkvkeymeta2 = { (void*)meta_key_2, NKV_TEST_META_KEY_LEN};
            const nkv_key  nkvkeymeta3 = { (void*)meta_key_3, NKV_TEST_META_KEY_LEN};
            const nkv_key  nkvkeymeta4 = { (void*)meta_key_4, NKV_TEST_META_KEY_LEN};
            sprintf(meta_val_1, "%s_%d_%u","nkv_test_meta1_bucket_1234_xl.json", targs->id,iter);
            sprintf(meta_val_2, "%s_%d_%u","nkv_test_meta1_bucket_1234_part1.json", targs->id,iter);
            sprintf(meta_val_3, "%s_%d_%u","nkv_test_meta1_bucket_xl.json", targs->id,iter);
            sprintf(meta_val_4, "%s_%d_%u","nkv_test_meta1_bucket_part1.json", targs->id,iter);
            nkv_value nkvvaluemeta1 = { (void*)meta_val_1, NKV_TEST_META_VAL_LEN, 0 };
            nkv_value nkvvaluemeta2 = { (void*)meta_val_2, NKV_TEST_META_VAL_LEN, 0 };
            nkv_value nkvvaluemeta3 = { (void*)meta_val_3, NKV_TEST_META_VAL_LEN, 0 };
            nkv_value nkvvaluemeta4 = { (void*)meta_val_4, NKV_TEST_META_VAL_LEN, 0 };

            status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta1, targs->s_option, &nkvvaluemeta1);
            if (status != 0) {
              smg_error(logger, "NKV Store meta1 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta1.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Store meta1 successful, key = %s", (char*) nkvkeymeta1.key);
            }

            status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta2, targs->s_option, &nkvvaluemeta2);
            if (status != 0) {
              smg_error(logger, "NKV Store meta2 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Store meta2 successful, key = %s", (char*) nkvkeymeta2.key);
            }

            memset(meta_val_1, 0, NKV_TEST_META_VAL_LEN);
            memset(meta_val_2, 0, NKV_TEST_META_VAL_LEN);

            status = nkv_retrieve_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta1, targs->r_option, &nkvvaluemeta1);
            if (status != 0) {
              smg_error(logger, "NKV Retrieve meta1 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta1.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Retrieve meta1 successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkeymeta1.key,
                      (char*) nkvvaluemeta1.value, nkvvaluemeta1.length, nkvvaluemeta1.actual_length);
            }

            status = nkv_retrieve_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta2, targs->r_option, &nkvvaluemeta2);
            if (status != 0) {
              smg_error(logger, "NKV Retrieve meta2 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Retrieve successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkeymeta2.key,
                      (char*) nkvvaluemeta2.value, nkvvaluemeta2.length, nkvvaluemeta2.actual_length);
            }

            status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta3, targs->s_option, &nkvvaluemeta3);
            if (status != 0) {
              smg_error(logger, "NKV Store meta3 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta3.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Store meta3 successful, key = %s", (char*) nkvkeymeta3.key);
            }

            status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta4, targs->s_option, &nkvvaluemeta4);
            if (status != 0) {
              smg_error(logger, "NKV Store meta4 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta4.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Store meta4 successful, key = %s", (char*) nkvkeymeta4.key);
            }
            //Main put
            sprintf(val, "%0*d", klen, iter);
            status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkey, targs->s_option, &nkvvalue);
            if (status != 0) {
              smg_error(logger, "NKV Store KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Store successful, key = %s", key_name);
            }
            //End

            status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta1);
            if (status != 0) {
              smg_error(logger, "NKV Delete meta1 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta1.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Delete meta1 successful, key = %s", (char*) nkvkeymeta1.key);
            }

            status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta2);
            if (status != 0) {
              smg_error(logger, "NKV Delete meta2 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Delete meta2 successful, key = %s", (char*) nkvkeymeta2.key);
            }
            if (meta_key_1)
              nkv_free(meta_key_1);
            if (meta_key_2)
              nkv_free(meta_key_2);
            if (meta_key_3)
              nkv_free(meta_key_3);
            if (meta_key_4)
              nkv_free(meta_key_4);

            if (meta_val_1)
              nkv_free(meta_val_1);
            if (meta_val_2)
              nkv_free(meta_val_2);
            if (meta_val_3)
              nkv_free(meta_val_3);
            if (meta_val_4)
              nkv_free(meta_val_4);

          } else {

            sprintf(val, "%0*d", klen, iter);
            status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkey, targs->s_option, &nkvvalue);
            if (status != 0) {
              smg_error(logger, "NKV Store KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Store successful, key = %s", key_name);
            }
          }
        }
        break;
      case 1: /*GET*/
        {
          //if ((targs->is_mixed) && ( iter % 3 == 0)) {
          if (targs->is_mixed) {

            char*meta_key_3   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            char*meta_key_4   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            memset(meta_key_3, 0, NKV_TEST_META_KEY_LEN);
            memset(meta_key_4, 0, NKV_TEST_META_KEY_LEN);
            sprintf(meta_key_3, "meta/bucket_%u/%u/%s/xl.json", targs->id, iter, targs->key_prefix);
            sprintf(meta_key_4, "meta/bucket_%u/%u/%s/part1.json", targs->id, iter, targs->key_prefix);
            char* meta_val_3 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            char* meta_val_4 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            memset(meta_val_3, 0, NKV_TEST_META_VAL_LEN);
            memset(meta_val_4, 0, NKV_TEST_META_VAL_LEN);
            const nkv_key  nkvkeymeta3 = { (void*)meta_key_3, NKV_TEST_META_KEY_LEN};
            const nkv_key  nkvkeymeta4 = { (void*)meta_key_4, NKV_TEST_META_KEY_LEN};
            nkv_value nkvvaluemeta3 = { (void*)meta_val_3, NKV_TEST_META_VAL_LEN, 0 };
            nkv_value nkvvaluemeta4 = { (void*)meta_val_4, NKV_TEST_META_VAL_LEN, 0 };

            status = nkv_retrieve_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta3, targs->r_option, &nkvvaluemeta3);
            if (status != 0) {
              smg_error(logger, "NKV Retrieve meta3 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta3.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Retrieve meta3 successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkeymeta3.key,
                      (char*) nkvvaluemeta3.value, nkvvaluemeta3.length, nkvvaluemeta3.actual_length);
            }

            status = nkv_retrieve_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta4, targs->r_option, &nkvvaluemeta4);

            if (status != 0) {

              smg_error(logger, "NKV Retrieve meta4 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta4.key, status);
              return 0;

            } else {
              smg_info(logger, "NKV Retrieve meta4 successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkeymeta4.key,
                      (char*) nkvvaluemeta4.value, nkvvaluemeta4.length, nkvvaluemeta4.actual_length);
            }

            if (meta_key_3)
              nkv_free(meta_key_3);
            if (meta_key_4)
              nkv_free(meta_key_4);

            if (meta_val_3)
              nkv_free(meta_val_3);
            if (meta_val_4)
              nkv_free(meta_val_4);
            
          }

          status = nkv_retrieve_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkey, targs->r_option, &nkvvalue);
          if (status != 0) {
            smg_error(logger, "NKV Retrieve KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
            return 0;
          } else {
            smg_info(logger, "NKV Retrieve successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkey.key,
                    (char*) nkvvalue.value, nkvvalue.length, nkvvalue.actual_length);
          }



        }
        break;

      case 2: /*DEL*/
        {
          //if ((targs->is_mixed) && ( iter % 3 == 0)) {
          if (targs->is_mixed) {
            char*meta_key_3   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            char*meta_key_4   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            memset(meta_key_3, 0, NKV_TEST_META_KEY_LEN);
            memset(meta_key_4, 0, NKV_TEST_META_KEY_LEN);
            sprintf(meta_key_3, "meta/bucket_%u/%u/%s/xl.json", targs->id, iter, targs->key_prefix);
            sprintf(meta_key_4, "meta/bucket_%u/%u/%s/part1.json", targs->id, iter, targs->key_prefix);

            char* meta_val_3 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            char* meta_val_4 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            memset(meta_val_3, 0, NKV_TEST_META_VAL_LEN);
            memset(meta_val_4, 0, NKV_TEST_META_VAL_LEN);
            const nkv_key  nkvkeymeta3 = { (void*)meta_key_3, NKV_TEST_META_KEY_LEN};
            const nkv_key  nkvkeymeta4 = { (void*)meta_key_4, NKV_TEST_META_KEY_LEN};
            nkv_value nkvvaluemeta3 = { (void*)meta_val_3, NKV_TEST_META_VAL_LEN, 0 };
            nkv_value nkvvaluemeta4 = { (void*)meta_val_4, NKV_TEST_META_VAL_LEN, 0 };

            status = nkv_retrieve_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta3, targs->r_option, &nkvvaluemeta3);
            if (status != 0) {
              smg_error(logger, "NKV Retrieve meta3 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta3.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Retrieve meta3 successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkeymeta3.key,
                      (char*) nkvvaluemeta3.value, nkvvaluemeta3.length, nkvvaluemeta3.actual_length);
            }

            status = nkv_retrieve_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta4, targs->r_option, &nkvvaluemeta4);

            if (status != 0) {

              smg_error(logger, "NKV Retrieve meta4 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta4.key, status);
              return 0;

            } else {
              smg_info(logger, "NKV Retrieve meta4 successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkeymeta4.key,
                      (char*) nkvvaluemeta4.value, nkvvaluemeta4.length, nkvvaluemeta4.actual_length);
            }

            char*meta_key_1   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            char*meta_key_2   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
            memset(meta_key_1, 0, NKV_TEST_META_KEY_LEN);
            memset(meta_key_2, 0, NKV_TEST_META_KEY_LEN);
            sprintf(meta_key_1, "meta/bucket_%u/%u/1234/xl.json", targs->id, iter);
            sprintf(meta_key_2, "meta/bucket_%u/%u/1234/part1.json", targs->id, iter);

            char* meta_val_1 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            char* meta_val_2 = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
            memset(meta_val_1, 0, NKV_TEST_META_VAL_LEN);
            memset(meta_val_2, 0, NKV_TEST_META_VAL_LEN);
            const nkv_key  nkvkeymeta1 = { (void*)meta_key_1, NKV_TEST_META_KEY_LEN};
            const nkv_key  nkvkeymeta2 = { (void*)meta_key_2, NKV_TEST_META_KEY_LEN};
            sprintf(meta_val_1, "%s_%d_%u","nkv_test_meta1_bucket_1234_xl.json", targs->id,iter);
            sprintf(meta_val_2, "%s_%d_%u","nkv_test_meta1_bucket_1234_part1.json", targs->id,iter);

            nkv_value nkvvaluemeta1 = { (void*)meta_val_1, NKV_TEST_META_VAL_LEN, 0 };
            nkv_value nkvvaluemeta2 = { (void*)meta_val_2, NKV_TEST_META_VAL_LEN, 0 };

            status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta1, targs->s_option, &nkvvaluemeta1);
            if (status != 0) {
              smg_error(logger, "NKV Store meta1 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta1.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Store meta1 successful, key = %s", (char*) nkvkeymeta1.key);
            }

            status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta2, targs->s_option, &nkvvaluemeta2);
            if (status != 0) {
              smg_error(logger, "NKV Store meta2 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Store meta2 successful, key = %s", (char*) nkvkeymeta2.key);
            }

            status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta3);
            if (status != 0) {
              smg_error(logger, "NKV Delete meta3 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta3.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Delete meta3 successful, key = %s", (char*) nkvkeymeta1.key);
            }

            status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta4);
            if (status != 0) {
              smg_error(logger, "NKV Delete meta4 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta4.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Delete meta4 successful, key = %s", (char*) nkvkeymeta4.key);
            }
            //Main delete
            status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkey);
            if (status != 0) {
              smg_error(logger, "NKV Delete KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Delete successful, key = %s", (char*) nkvkey.key);
            }
            //end

            status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta1);
            if (status != 0) {
              smg_error(logger, "NKV Delete meta1 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta1.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Delete meta1 successful, key = %s", (char*) nkvkeymeta1.key);
            }

            status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkeymeta2);
            if (status != 0) {
              smg_error(logger, "NKV Delete meta2 KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Delete meta2 successful, key = %s", (char*) nkvkeymeta2.key);
            }

            if (meta_key_1)
              nkv_free(meta_key_1);
            if (meta_key_2)
              nkv_free(meta_key_2);
            if (meta_key_3)
              nkv_free(meta_key_3);
            if (meta_key_4)
              nkv_free(meta_key_4);

            if (meta_val_1)
              nkv_free(meta_val_1);
            if (meta_val_2)
              nkv_free(meta_val_2);
            if (meta_val_3)
              nkv_free(meta_val_3);
            if (meta_val_4)
              nkv_free(meta_val_4);


          } else {
            status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkey);
            if (status != 0) {
              smg_error(logger, "NKV Delete KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              return 0;
            } else {
              smg_info(logger, "NKV Delete successful, key = %s", (char*) nkvkey.key);
            }
          }

        }
        break;


      case 3: /*PUT, GET, DEL*/
        {
          memset(val, 0, targs->vlen);
          sprintf(val, "%0*d", klen, iter);
          
          if (targs->check_integrity) {
            memset(cmpval, 0, targs->vlen);
            strcpy(cmpval, val);
          }
          if (targs->hex_dump) {
            printf("Storing buffer:\n");
            memHexDump(nkvvalue.value, targs->vlen);
          }
          status = nkv_store_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkey, targs->s_option, &nkvvalue);
          if (status != 0) {
            smg_error(logger, "NKV Store KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
            return 0;
          }
          smg_info(logger, "NKV Store successful, key = %s", key_name);
          memset(nkvvalue.value, 0, targs->vlen);
          if (targs->hex_dump) {
            printf("Before retrieve buffer:\n");
            memHexDump(nkvvalue.value, targs->vlen);
          }
          status = nkv_retrieve_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkey, targs->r_option, &nkvvalue);
          if (status != 0) {
            smg_error(logger, "NKV Retrieve KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
            return 0;
          } else {
            if (nkvvalue.actual_length == targs->vlen) {
              smg_info(logger, "NKV Retrieve successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkey.key,
                      (char*) nkvvalue.value, nkvvalue.length, nkvvalue.actual_length);
            } else {
              smg_error(logger, "NKV Retrieve successful, but got wrong length back ! key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkey.key,
                      (char*) nkvvalue.value, nkvvalue.length, nkvvalue.actual_length);              
              if (nkvvalue.actual_length > targs->vlen)
                assert(nkvvalue.actual_length == targs->vlen);
            }

            if (targs->check_integrity) {
              //int n = memcmp(cmpval, (char*) nkvvalue.value, targs->vlen);
              int n = memcmp(cmpval, (char*) nkvvalue.value, nkvvalue.actual_length);
              if (n != 0) {
                //smg_error(logger, "Data integrity failed for key = %s, expected = %s, got = %s", key_name, cmpval, (char*) nkvvalue.value);
                smg_error(logger, "Data integrity failed for key = %s, expected = %s, got = %s, actual length = %u, mismatched byte = %d",
                          key_name, cmpval, (char*) nkvvalue.value, nkvvalue.actual_length, n);
                printf("Expected buffer:\n");
                memHexDump(cmpval, nkvvalue.actual_length);
                printf("Received buffer:\n");
                memHexDump(nkvvalue.value, nkvvalue.actual_length);                
                return 0;
              } else {
                smg_info(logger,"Data integrity check is successful for key = %s", key_name);
              }
            }
          }

          status = nkv_delete_kvp (targs->nkv_handle, &targs->ioctx[iter % targs->ioctx_cnt], &nkvkey);
          if (status != 0) {
            smg_error(logger, "NKV Delete KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
            return 0;
          } else {
            smg_info(logger, "NKV Delete successful, key = %s", (char*) nkvkey.key);
          }

        }
        break;

      case NKV_TEST_OP_LOCK_UNLOCK: //Lock Unlock
        {
          status = nkv_lock_kvp (targs->nkv_handle, \
						&targs->ioctx[iter % targs->ioctx_cnt], \
						&nkvkey, targs->lock_option);
          if (status != 0) {
            smg_error(logger, "NKV lock KVP call failed !!, key = %s,"\
						" error = %d", (char*) nkvkey.key, status);
			return 0;
          } else {
            smg_info(logger, "NKV Lock successful, key = %s", \
						(char*) nkvkey.key);
          }

          status = nkv_unlock_kvp (targs->nkv_handle, \
						&targs->ioctx[iter % targs->ioctx_cnt], \
						&nkvkey, targs->unlock_option);
          if (status != 0) {
            smg_error(logger, "NKV unlock KVP call failed !!, key = %s,"\
						" error = %d", (char*) nkvkey.key, status);
			return 0;
          } else {
            smg_info(logger, "NKV Unlock successful, key = %s", \
						(char*) nkvkey.key);
          }

        }
        break;

      default:
        smg_error(logger,"Unsupported operation provided, op = %d", \
							targs->op_type);
    }
    if (key_name)  
      nkv_free(key_name);
    if (val)
      nkv_free(val);

    if (targs->app_ustat_ctx) {
      status = nkv_dec_to_counter(targs->nkv_handle, targs->app_ustat_ctx);
      if (status != NKV_SUCCESS) {
        smg_error(logger, "NKV stat dec api failed !, error = %u", status);
      }
    }

    
  }
  if (cmpval)
    nkv_free(cmpval);
 
  return 0;
}

void usage(char *program)
{
  printf("==============\n");
  printf("usage: %s -c config_path -i host_name_ip -p host_port -b key_prefix [-n num_ios] [-q queue_depth] [-o op_type] [-k klen] [-v vlen] [-e is_exclusive] [-m check_integrity] \n", program);
  printf("-c      config_path     :  NKV config file location\n");
  printf("-i      host_name_ip    :  Host name or ip the nkv client instance will be running\n");
  printf("-p      host_port       :  Host port this nkv instance will bind to\n");
  printf("-b      key_prefix      :  Key name prefix to be used\n");
  printf("-n      num_ios         :  total number of ios\n");
  printf("-o      op_type         :  0: Put; 1: Get; 2: Delete; "\
					"3: Put, Get and delete (only sync); 4: listing; "\
					"5: Put and list 6: Lock&Unlock\n");
  printf("-k      klen            :  key length \n");
  printf("-v      vlen            :  value length \n");
  printf("-e      is_exclusive    :  Idempotent Put \n");
  printf("-m      check_integrity :  Data integrity check during Get, only valid for op_type = 3  \n");
  printf("-a      async_mode      :  Execution will be done in async mode  \n");
  printf("-w      working key     :  CLI will only work on the key passed  \n");
  printf("-s      mount_point     :  CLI will send io to this mount_point only  \n");
  printf("-t      threads         :  number of threads in case of sync IO  \n");
  printf("-d      mixed_io        :  small meta io before doing a big io  \n");
  printf("-x      hex_dump        :  Inspect memory dump  \n");
  printf("-r      delimiter       :  delimiter for S3 like listing  \n");
  printf("-u      path_stat       :  Collect path/disk stat for all underlying path(s)/disk(s) \n");
  printf("-l      multipath       :  0:disable 1:enable  \n");
  printf("-f      multipath policy:  0/1:RR 2:Failover 3:Least Queue Depth 4:Least Block Size  \n");
  printf("-j      alingned alloc  :  Alligned allocation for value size, give the desired alignment value here, works with multiple threads option only  \n");
  printf("-z      application stat:  Application wants it's stat to be collected via NKV ustat  \n");
  printf("==============\n");
}


int main(int argc, char *argv[]) {
  char* config_path = NULL;
  char* host_name_ip = NULL;
  char* key_to_work = NULL;
  char* subsystem_mp = NULL;
  int32_t port = -1;
  uint64_t num_ios = 10;
  //int qdepth = 64;
  int op_type = 3;
  int parent_op_type = -1;
  uint32_t vlen = 4096;
  uint32_t klen = 16;
  char* key_beginning = NULL;
  char* key_delimiter = NULL;
  int is_exclusive = 0;
  int check_integrity = 1;
  int is_async = 0;
  int c;
  int32_t num_threads = 0;
  int is_mixed = 0;
  int hex_dump = 0;
  bool get_stat = false;
  int app_ustat = 0;
  int multipath_lb = 0;
  int multipath_lb_policy = 0;
  int alignment = 0;
  nkv_feature_list feature_list = {0, 0};

  logger = smg_acquire_logger("libnkv");
  if (!logger) {
    std::cout << "Logger is NULL ! " << std::endl;
    exit(1);
  }
 
  while ((c = getopt(argc, argv, "c:i:p:n:q:o:k:v:b:e:m:a:w:s:t:d:x:r:u:h:l:f:j:z:")) != -1) {
    switch(c) {

    case 'c':
      config_path = optarg;
      break;
    case 'i':
      host_name_ip = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'n':
      num_ios = atoi(optarg);
      break;
    /*case 'q':
      qdepth = atoi(optarg);
      break;*/
    case 'o':
      op_type = atoi(optarg);
      break;
    case 'k':
      klen = atoi(optarg);
      break;
    case 'v':
      vlen = atoi(optarg);
      break;
    case 'e':
      is_exclusive = atoi(optarg);
      break;
    case 'm':
      check_integrity = atoi(optarg);
      break;
    case 'b':
      key_beginning = optarg;
      break;
    case 'a':
      is_async = atoi(optarg);
      break;
    case 'w':
      key_to_work = optarg;
      break;
    case 's':
      subsystem_mp = optarg;
      break;
    case 't':
      num_threads = atoi(optarg);
      break;
    case 'd':
      is_mixed = atoi(optarg);
      break;
    case 'x':
      hex_dump = atoi(optarg);
      break;
    case 'r':
      key_delimiter = optarg;
      break;
    case 'u':
      get_stat = true;
      break;
    case 'j':
      alignment = atoi(optarg);
      break;

    case 'h':
      usage(argv[0]);
      smg_release_logger(logger);
      exit(0);
      break;
    case 'l':
      multipath_lb = atoi(optarg);
      feature_list.nic_load_balance = multipath_lb;
      break;
    case 'f':
      multipath_lb_policy = atoi(optarg);
      feature_list.nic_load_balance_policy = multipath_lb_policy;
      break;

    case 'z':
      app_ustat = atoi(optarg);
      break;

    default:
      usage(argv[0]);
      smg_release_logger(logger);
      exit(1);
    }
  }

  if(host_name_ip == NULL) {
    smg_error(logger,"Please specify host name or ip");
    usage(argv[0]);
    exit(1);
  }

  if(port == -1) {
    smg_error(logger,"Please specify host port");
    usage(argv[0]);
    exit(1);
  }

  if (!key_beginning && !key_to_work && (op_type != 4) && (!get_stat)) {
    smg_error(logger, "Please provide a key prefix to work on..");
    usage(argv[0]);
    exit(1);
  }
  if (key_to_work != NULL) {
    if (!subsystem_mp) {
      smg_error(logger,"Please provide a mount_point to send the io");
      usage(argv[0]);
      exit(1);

    }
  }
  if (is_async && num_threads > 0) {
    smg_error(logger, "Async mode can't run in muktithreaded environment..");
    usage(argv[0]);
    exit(1);
  }
  if (klen > vlen) {
    smg_error(logger, "key length can't be greater than value length, not an API restriction but specific to test CLI requirement, sorry..");
    usage(argv[0]);
    exit(1);  
  }

  uint64_t instance_uuid, nkv_handle = 0; 
  nkv_result status = nkv_open(config_path, "nkv_test_cli", host_name_ip, port, &instance_uuid, &nkv_handle);
  if (status != 0) {
    smg_error(logger, "NKV open failed !!, error = %d", status);
    exit(1);
  }
  smg_info(logger, "NKV open successful, instance uuid = %u, nkv handle = %u", instance_uuid, nkv_handle);
  void* stat_ctx = NULL;
  if (app_ustat) {
    nkv_stat_counter cli_cnt;
    cli_cnt.counter_name = "cli_qd";
    cli_cnt.counter_type = STAT_TYPE_UINT64;
  
    status = nkv_register_stat_counter (nkv_handle, "test_cli", &cli_cnt, &stat_ctx);
    if (status != NKV_SUCCESS) {
      smg_error(logger, "NKV stat counter registration failed, counter_name = %s, error = %u", cli_cnt.counter_name, status);
    } else {
      smg_alert(logger, "## NKV stat counter registration successful, counter_name = %s, stat_ctx = 0x%x", cli_cnt.counter_name, stat_ctx);
    }
  }

do {
  if (op_type == 5) {
    parent_op_type = 5;
    op_type = 0;
  }
  uint32_t index = 0;
  uint32_t cnt_count = NKV_MAX_ENTRIES_PER_CALL;
  nkv_container_info *cntlist = new nkv_container_info[NKV_MAX_ENTRIES_PER_CALL];
  memset(cntlist, 0, sizeof(nkv_container_info) * NKV_MAX_ENTRIES_PER_CALL);

  for (int i = 0; i < NKV_MAX_ENTRIES_PER_CALL; i++) {
    cntlist[i].num_container_transport = NKV_MAX_CONT_TRANSPORT;
    cntlist[i].transport_list = new nkv_container_transport[NKV_MAX_CONT_TRANSPORT];
    memset(cntlist[i].transport_list, 0, sizeof(nkv_container_transport)*NKV_MAX_CONT_TRANSPORT);
  }

  status = nkv_set_supported_feature_list(nkv_handle, &feature_list);
  status = nkv_get_supported_feature_list(nkv_handle, &feature_list);
  smg_info(logger, "Load Balancer is %d. (1 enable, 0 disable). Policy is %d "
   	   "(0 = round robin policy, 1 = failover policy, 2 = least queue depth,"
     	   "3 = least queue size", feature_list.nic_load_balance,
	   feature_list.nic_load_balance_policy);  

  status = nkv_physical_container_list (nkv_handle, index, cntlist, &cnt_count); 
  if (status != 0) {
    smg_error(logger, "NKV getting physical container list failed !!, error = %d", status);
    nkv_close (nkv_handle, instance_uuid);
    exit(1);
  }
  smg_info(logger, "Got container list, count = %u", cnt_count);
  nkv_io_context io_ctx[32];
  memset(io_ctx, 0, sizeof(nkv_io_context) * 32);
  uint32_t io_ctx_cnt = 0;

  for (uint32_t i = 0; i < cnt_count; i++) {
    smg_info(logger, "Container Information :: hash = %u, id = %u, uuid = %s, name = %s, target node = %s, status = %u, space available pcnt = %u",
             cntlist[i].container_hash, cntlist[i].container_id, cntlist[i].container_uuid, cntlist[i].container_name, cntlist[i].hosting_target_name,
             cntlist[i].container_status, cntlist[i].container_space_available_percentage);

    smg_info(logger,"\n");
    io_ctx[io_ctx_cnt].container_hash = cntlist[i].container_hash;

    smg_info(logger,"Number of Container transport = %d", cntlist[i].num_container_transport);
    for (int p = 0; p < cntlist[i].num_container_transport; p++) {
      smg_info(logger,"Transport information :: hash = %u, id = %d, address = %s, port = %d, family = %d, speed = %d, status = %d, numa_node = %d, mount_point = %s",
              cntlist[i].transport_list[p].network_path_hash, cntlist[i].transport_list[p].network_path_id, cntlist[i].transport_list[p].ip_addr, 
              cntlist[i].transport_list[p].port, cntlist[i].transport_list[p].addr_family, cntlist[i].transport_list[p].speed, 
              cntlist[i].transport_list[p].status, cntlist[i].transport_list[p].numa_node, cntlist[i].transport_list[p].mount_point);

      if (subsystem_mp && (0 != strcmp(cntlist[i].transport_list[p].mount_point, subsystem_mp)))
        continue;
      io_ctx[io_ctx_cnt].is_pass_through = 1;
      io_ctx[io_ctx_cnt].container_hash = cntlist[i].container_hash;
      if (op_type != 4) { //not listing
        io_ctx[io_ctx_cnt].network_path_hash = cntlist[i].transport_list[p].network_path_hash;
      }
      io_ctx_cnt++;           
      if (op_type == 4)
        break;   
    } 
  }
  //Path stat request
  if (get_stat) {
    nkv_mgmt_context mg_ctx = {0};
    mg_ctx.is_pass_through = 1;

    if (num_threads > 0) {
      nkv_thread_args args[num_threads];
      std::thread nkv_test_threads[num_threads];

      for(int i = 0; i < num_threads; i++){
        args[i].id = i;
        args[i].klen = klen;
        args[i].vlen = vlen;
        args[i].count = num_ios;
        args[i].op_type = op_type;
        args[i].key_prefix = key_beginning;
        args[i].ioctx = io_ctx;
        args[i].ioctx_cnt = io_ctx_cnt;
        args[i].nkv_handle = nkv_handle;
        //args[i].s_option = &s_option;
        //args[i].r_option = &r_option;
        args[i].check_integrity = check_integrity;
        args[i].hex_dump = hex_dump;

        nkv_test_threads[i] = std::thread(stat_thread, &args[i]);
      }

      for(int i = 0; i < num_threads; i++) {
        nkv_test_threads[i].join();
      }

      nkv_close (nkv_handle, instance_uuid);
      return 0;

    }


    for (uint32_t cnt_iter = 0; cnt_iter < io_ctx_cnt; cnt_iter++) {
      mg_ctx.container_hash = io_ctx[cnt_iter].container_hash;
      mg_ctx.network_path_hash = io_ctx[cnt_iter].network_path_hash;
      nkv_path_stat p_stat = {0};
      nkv_result stat = nkv_get_path_stat (nkv_handle, &mg_ctx, &p_stat);

      if (stat == NKV_SUCCESS) {
        smg_alert(logger, "NKV stat call succeeded");
        smg_alert(logger, "NKV path mount = %s, path capacity = %lld Bytes, path usage = %lld Bytes, path util percentage = %f",
                p_stat.path_mount_point, (long long)p_stat.path_storage_capacity_in_bytes, (long long)p_stat.path_storage_usage_in_bytes,
                p_stat.path_storage_util_percentage);
      } else {
        smg_error(logger, "NKV stat call failed, error = %d", stat);
      }
    }
    nkv_close (nkv_handle, instance_uuid);
    return 0;    
  }

  if (is_async) {
    /*for (uint32_t iter = 0; iter < io_ctx_cnt; iter++) {
      cur_qdepth[iter] = 0;      
    }*/
  }

  smg_info(logger, "\nLet's send some IO now....");

  nkv_store_option s_option = {0};
  if (is_exclusive)
    s_option.nkv_store_no_overwrite = 1;

  nkv_retrieve_option r_option = {0};

  nkv_lock_option lock_option;
  nkv_unlock_option unlock_option;

  //Lock options
  lock_option.nkv_lock_priority = 0;
  lock_option.nkv_lock_writer = 1;
  lock_option.nkv_lock_blocking = 0;
  lock_option.nkv_lock_duration = 100;
  lock_option.nkv_lock_uuid = instance_uuid;

  //Unlock options
  unlock_option.nkv_lock_priority = 0;
  unlock_option.nkv_lock_writer = 1;
  unlock_option.nkv_lock_blocking = 0;
  unlock_option.nkv_lock_duration = 100;
  unlock_option.nkv_lock_uuid = instance_uuid;

  if (key_to_work != NULL) {
    smg_info(logger, "CLI will only work on key = %s, op = %d", key_to_work, op_type);
    assert(io_ctx_cnt == 1);
    char* key_name = NULL;
    if (klen != 16) {
      key_name   = (char*)nkv_malloc(klen);
      memset(key_name, 0, klen);
      sprintf(key_name, "%s", key_to_work);
    } else {

      klen = strlen (key_to_work);
      key_name = key_to_work;
    }
    char *val   = (char*)nkv_zalloc(vlen);
    memset(val, 0, vlen);
    const nkv_key  nkvkey = { (void*)key_name, klen};
    nkv_value nkvvalue = { (void*)val, vlen, 0 };
    
    switch(op_type) {
      case 0: //PUT
        {
          sprintf(val, "%0*d", klen, 1980);
          
          status = nkv_store_kvp (nkv_handle, &io_ctx[0], &nkvkey, &s_option, &nkvvalue);
          if (status != 0) {
            smg_error(logger, "NKV Store KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
            nkv_close (nkv_handle, instance_uuid);
            exit(1);
          }
          smg_info(logger, "NKV Store successful, key = %s", (char*) nkvkey.key);
        }
        break;
      case 1: //GET
        {
          status = nkv_retrieve_kvp (nkv_handle, &io_ctx[0], &nkvkey, &r_option, &nkvvalue);
          if (status != 0) {
            smg_error(logger, "NKV Retrieve KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
            nkv_close (nkv_handle, instance_uuid);
            exit(1);
          } else {
            smg_info(logger, "NKV Retrieve successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkey.key,
                    (char*) nkvvalue.value, nkvvalue.length, nkvvalue.actual_length);
          }

        }
        break;
      case 2: //DEL
        {
          status = nkv_delete_kvp (nkv_handle, &io_ctx[0], &nkvkey);
          if (status != 0) {
            smg_error(logger, "NKV Delete KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
            nkv_close (nkv_handle, instance_uuid);
            exit(1);
          } else {
            smg_info(logger, "NKV Delete successful, key = %s", (char*) nkvkey.key);
          }

        }
        break;
    }
    if (val)
      nkv_free(val);
    nkv_close (nkv_handle, instance_uuid);
    exit(0);
  }

  if (op_type != 4) {
    std::string key_start = key_beginning;
    std::string num_io_len = std::to_string(num_ios);
    if (klen <= num_io_len.length()) {
      smg_error(logger, "key length provided is not big enough compare to number of IO request to perform !");
      nkv_close (nkv_handle, instance_uuid);
      exit(1);
    }

    if (key_start.length() + 4 > (klen - num_io_len.length())) {
      smg_error(logger, "Key prefix provided should be less than %d characters", (klen - num_io_len.length()));
      nkv_close (nkv_handle, instance_uuid);
      exit(1);
    }
  }

  if (num_threads > 0) {

    nkv_thread_args args[num_threads];
    std::thread nkv_test_threads[num_threads];
    /*pthread_t tid[num_threads];
    pthread_attr_t attrs[num_threads];*/

    /*struct timespec t1, t2;
    clock_gettime(CLOCK_REALTIME, &t1);*/
    auto start = std::chrono::steady_clock::now();
 
    for(int i = 0; i < num_threads; i++){
      args[i].id = i;
      args[i].klen = klen;
      args[i].vlen = vlen;
      args[i].count = num_ios;
      args[i].op_type = op_type;
      args[i].key_prefix = key_beginning;
      args[i].ioctx = io_ctx;
      args[i].ioctx_cnt = io_ctx_cnt;
      args[i].nkv_handle = nkv_handle;
      args[i].s_option = &s_option;
      args[i].r_option = &r_option;
      args[i].lock_option = &lock_option;
      args[i].unlock_option = &unlock_option;
      args[i].check_integrity = check_integrity;
      args[i].hex_dump = hex_dump;
      args[i].is_mixed = is_mixed;
      args[i].alignment = alignment;
      args[i].app_ustat_ctx = stat_ctx;
    

      /*cpu_set_t cpus;
      pthread_attr_init(attr);
      CPU_ZERO(&cpus);
      CPU_SET(0, &cpus); // CPU 0
      pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpus);

      ret = pthread_create(&tid[i], attr, iothread, &args[i]);*/
      nkv_test_threads[i] = std::thread(iothread, &args[i]);
    }

    for(int i = 0; i < num_threads; i++) {
      nkv_test_threads[i].join();
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start); 
    int64_t total_num_objs = num_ios * num_threads;
    auto tps = ((long double)((long double)total_num_objs/(long double)elapsed.count())) * 1000000;
    uint32_t rounded_tps = round(tps);
    uint32_t vlen_kb = (vlen/1024);
    uint32_t t_put_in_mb = (rounded_tps * vlen_kb)/1024;
    smg_alert (logger, "TPS = %u, Throughput = %u MB/sec, value_size = %u, total_num_objs = %u", rounded_tps,
             t_put_in_mb, vlen, total_num_objs);

    /*std::cout << "Press any key to quit.." << std::endl;
    char input;
    std::cin >> input;*/

    if (parent_op_type == 5) {
      std::cout << "Enter op_type:" << std::endl;
      std::cin>> op_type;
      if (op_type == -1)
        break;

      std::cout << "Enter num_ios:" << std::endl;
      std::cin>> num_ios;

      std::cout << "Enter key prefix:" << std::endl;
      std::cin>> key_beginning;

      if (key_delimiter) {
        std::cout << "Enter key delimiter:" << std::endl;
        std::cin>> key_delimiter;
      }

      smg_alert (logger, "Running with op_type = %d, num_ios = %u, key_beginning = %s, key_delimiter = %s", op_type, num_ios, key_beginning, key_delimiter);
      continue;

    } else {
      nkv_close (nkv_handle, instance_uuid);
      return 0;
    }
  
  }

  if (op_type == 4) { //Listing
   
    smg_info(logger, "Max number of keys to be iterated is %u", num_ios);
    assert(num_ios != 0);
    num_ios += 1;
    uint32_t max_keys = num_ios;
    nkv_result status = NKV_SUCCESS;
    uint32_t total_keys = 0;
    //Allocate out buffer
    nkv_key* keys_out = (nkv_key*) malloc (sizeof(nkv_key) * num_ios);
    memset(keys_out, 0, (sizeof(nkv_key) * num_ios));
    for (uint32_t iter = 0; iter < num_ios; iter++) {
      keys_out[iter].key = malloc (256);
      assert(keys_out[iter].key != NULL);
      memset(keys_out[iter].key, 0, 256);
      keys_out[iter].length = 256;
    }
    auto start = std::chrono::steady_clock::now();

    // Checking key_prefix, and delimiter
    uint32_t prefix_length = strlen(key_beginning);
    if ( key_delimiter ) { 
      if ( key_delimiter[0] != key_beginning[prefix_length -1] ) {
        strncat(key_beginning, key_delimiter, 1);
      }
    } else {
      if (key_beginning[prefix_length -1] !=  S3_DELIMITER[0]) {
        strncat(key_beginning, S3_DELIMITER, 1);
      }
    }

    for (uint32_t cnt_iter = 0; cnt_iter < io_ctx_cnt; cnt_iter++) {
      smg_info(logger, "Iterating for container hash = %u, prefix = %s, delimiter = %s", io_ctx[cnt_iter].container_hash, key_beginning, key_delimiter);
      void* iter_context = NULL;
      do {
        max_keys = num_ios;
        status = nkv_indexing_list_keys(nkv_handle, &io_ctx[cnt_iter], NULL, key_beginning, key_delimiter, NULL, &max_keys, keys_out, &iter_context);
        if ((status == NKV_ITER_MORE_KEYS) || (status == NKV_SUCCESS)) {
          smg_alert(logger, "Looks like we got some valid keys, number of keys got in this batch = %u, status = %x", max_keys, status);
          total_keys += max_keys;
          for (uint32_t k_iter = 0; k_iter < max_keys; k_iter++) {
            smg_warn(logger, "key_%u = %s", k_iter, keys_out[k_iter].key);
          }
        }
        for (uint32_t iter = 0; iter < num_ios; iter++) {
          assert(keys_out[iter].key != NULL);
          memset(keys_out[iter].key, 0, 256);
          keys_out[iter].length = 256;
        }

        
      } while (status == NKV_ITER_MORE_KEYS);
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start);
    long double tps = ((long double)((long double)total_keys/(long double)elapsed.count())) * 1000000;
    uint32_t rounded_tps = round(tps);

    for (uint32_t iter = 0; iter < num_ios; iter++) {
      if (keys_out[iter].key) {
        free(keys_out[iter].key);
        keys_out[iter].key = NULL;
      }
      keys_out[iter].length = 0;
    }
    if (keys_out)
      free (keys_out);

    smg_alert (logger, "TPS = %u, total_num_keys = %u", rounded_tps, total_keys);
    if (parent_op_type != 5) {
      nkv_close (nkv_handle, instance_uuid);
      return 0;
    }
    else {
      std::cout << "Enter op_type:" << std::endl;
      std::cin>> op_type;
      if (op_type == -1)
        break;

      std::cout << "Enter num_ios:" << std::endl;
      std::cin>> num_ios;

      std::cout << "Enter key prefix:" << std::endl;
      std::cin>> key_beginning;

      if (key_delimiter) {
        std::cout << "Enter key delimiter:" << std::endl;
        std::cin>> key_delimiter;
      }

      smg_alert (logger, "Running with op_type = %d, num_ios = %u, key_beginning = %s, key_delimiter = %s", op_type, num_ios, key_beginning, key_delimiter);
      continue;
    } 
  } 

  smg_info(logger, "num_threads = 0, going without thread creation..");
 
  char *cmpval   = (char*)nkv_zalloc(vlen);
  char* meta_key_1 = NULL;
  char* meta_key_2 = NULL;
  char* meta_val = NULL;
  uint32_t meta1_klen = 0;
  uint32_t meta2_klen = 0;

  auto start = std::chrono::steady_clock::now();
  
  for(uint32_t iter = 0; iter < num_ios; iter++) {
    char *key_name   = (char*)nkv_malloc(klen);
    memset(key_name, 0, klen);

    if (is_async && is_mixed) {
      meta_key_2   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
      memset(meta_key_2, 0, NKV_TEST_META_KEY_LEN);
      sprintf(meta_key_2, "meta_2_%u", iter);
      meta2_klen = NKV_TEST_META_KEY_LEN; //strlen(meta_key_2);
    }
    sprintf(key_name, "%s_%u", key_beginning, iter);
    //klen = strlen (key_name);
    const nkv_key  nkvkey = { (void*)key_name, klen};
    nkv_postprocess_function* p_fn = NULL;
    char *val = NULL;

    if (is_async) {
      p_fn = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
      p_fn->nkv_aio_cb = nkv_aio_complete;
      //p_fn->private_data_1 = (void*) &cur_qdepth[iter % io_ctx_cnt];
      p_fn->private_data_1 = NULL;
      p_fn->private_data_2 = (void*) p_fn;
    }

    switch(op_type) {
      case 0: //PUT
        {
          val   = (char*)nkv_zalloc(vlen);
          memset(val, 0, vlen);
          nkv_value nkvvalue = { (void*)val, vlen, 0 };
          sprintf(val, "%0*d", klen, iter);

          if (!is_async) {
            status = nkv_store_kvp (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey, &s_option, &nkvvalue);
            if (status != 0) {
              smg_error(logger, "NKV Store KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            }
            smg_info(logger, "NKV Store successful, key = %s, value size = %u", key_name, vlen);
          } else {
            //Async PUT
            if (is_mixed) {
              {
                meta_key_1   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
                memset(meta_key_1, 0, NKV_TEST_META_KEY_LEN);
                sprintf(meta_key_1, "meta_1_%u", iter);
                meta1_klen = NKV_TEST_META_KEY_LEN;//strlen(meta_key_1);
                meta_val = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
                memset(meta_val, 0, NKV_TEST_META_VAL_LEN);
                const nkv_key  nkvkeymeta1 = { (void*)meta_key_1, meta1_klen};
                sprintf(meta_val, "%s_%d","nkv_test_meta1_", iter); 
                nkv_value nkvvaluemeta = { (void*)meta_val, NKV_TEST_META_VAL_LEN, 0 };
                int32_t is_done = 0;

                nkv_postprocess_function* p_fn2 = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
                p_fn2->nkv_aio_cb = nkv_aio_complete;
                p_fn2->private_data_1 = (void*) &is_done;
                p_fn2->private_data_2 = (void*) p_fn2;
           
                //PUT Meta 1
                status = nkv_store_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta1, &s_option, &nkvvaluemeta, p_fn2);
                if (status != 0) {
                  smg_error(logger, "NKV Store Meta KVP start failed !!, key = %s, error = %d", (char*) nkvkeymeta1.key, status);
                  nkv_close (nkv_handle, instance_uuid);
                  exit(1);
                }
                submitted.fetch_add(1, std::memory_order_relaxed);
                while (!is_done) {
                  usleep(1);
                }

              }
       
              {
                meta_key_1   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
                memset(meta_key_1, 0, NKV_TEST_META_KEY_LEN);
                sprintf(meta_key_1, "meta_1_%u", iter);
                meta1_klen = strlen(meta_key_1);
                meta_val = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
                memset(meta_val, 0, NKV_TEST_META_VAL_LEN);
                const nkv_key  nkvkeymeta1 = { (void*)meta_key_1, meta1_klen};
                nkv_value nkvvaluemeta = { (void*)meta_val, NKV_TEST_META_VAL_LEN, 0 };
                int32_t is_done = 0;

                nkv_postprocess_function* p_fn2 = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
                p_fn2->nkv_aio_cb = nkv_aio_complete;
                p_fn2->private_data_1 = (void*) &is_done;
                p_fn2->private_data_2 = (void*) p_fn2;

                //GET Meta 1
                status = nkv_retrieve_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta1, &r_option, &nkvvaluemeta, p_fn2);
                if (status != 0) {
                  smg_error(logger, "NKV Retrieve Meta KVP call failed !!, key = %s, error = %d", (char*) nkvkeymeta1.key, status);
                } else {
                  smg_info(logger, "NKV Retrieve Meta start successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkeymeta1.key,
                          (char*) nkvvaluemeta.value, nkvvaluemeta.length, nkvvaluemeta.actual_length);
                }
                submitted.fetch_add(1, std::memory_order_relaxed);
                while (!is_done) {
                  usleep(1);
                }

              }
            
              {
                meta_key_1   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
                memset(meta_key_1, 0, NKV_TEST_META_KEY_LEN);
                sprintf(meta_key_1, "meta_1_%u", iter);
                meta1_klen = strlen(meta_key_1);
                const nkv_key  nkvkeymeta1 = { (void*)meta_key_1, meta1_klen};

                nkv_postprocess_function* p_fn1 = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
                p_fn1->nkv_aio_cb = nkv_aio_complete;
                p_fn1->private_data_1 = NULL;
                p_fn1->private_data_2 = (void*) p_fn1;

                //Del staged meta1
                status = nkv_delete_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta1, p_fn1);
                if (status != 0) {
                  smg_error(logger, "NKV Delete Meta KVP Async start failed !!, key = %s, error = %d", (char*) nkvkeymeta1.key, status);
                  nkv_close (nkv_handle, instance_uuid);
                  exit(1);
                }
                submitted.fetch_add(1, std::memory_order_relaxed);
              }

              {
                meta_val = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
                memset(meta_val, 0, NKV_TEST_META_VAL_LEN);
                sprintf(meta_val, "%s_%d","nkv_test_meta2_", iter);
                nkv_value nkvvaluemeta = { (void*)meta_val, NKV_TEST_META_VAL_LEN, 0 };
                const nkv_key  nkvkeymeta2 = { (void*)meta_key_2, meta2_klen};

                nkv_postprocess_function* p_fn2 = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
                p_fn2->nkv_aio_cb = nkv_aio_complete;
                p_fn2->private_data_1 = NULL;
                p_fn2->private_data_2 = (void*) p_fn2;

                //Write Meta2  
                status = nkv_store_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta2, &s_option, &nkvvaluemeta, p_fn2);
                //status = nkv_store_kvp (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta2, &s_option, &nkvvaluemeta);
                if (status != 0) {
                  smg_error(logger, "NKV Store KVP Async start failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
                  nkv_close (nkv_handle, instance_uuid);
                  exit(1);
                }
                submitted.fetch_add(1, std::memory_order_relaxed);
              }

            }
            //PUT payload
            status = nkv_store_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey, &s_option, &nkvvalue, p_fn);
            if (status != 0) {
              smg_error(logger, "NKV Store KVP Async start failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            }
            submitted.fetch_add(1, std::memory_order_relaxed);

            smg_info(logger, "NKV Store async IO done starting, Submitted = %u, completed = %u", 
                     submitted.load(), completed.load());
          }
        }
        break;

      case 1: //GET
        {
          val   = (char*)nkv_zalloc(vlen);
          memset(val, 0, vlen);
          nkv_value nkvvalue = { (void*)val, vlen, 0 };

          if (!is_async) {
            status = nkv_retrieve_kvp (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey, &r_option, &nkvvalue);
            if (status != 0) {
              smg_error(logger, "NKV Retrieve KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            } else {
              smg_info(logger, "NKV Retrieve successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkey.key, 
                      (char*) nkvvalue.value, nkvvalue.length, nkvvalue.actual_length);
            }
          } else {
            //Async GET
            if (is_mixed) {
              {
                meta_val = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
                memset(meta_val, 0, NKV_TEST_META_VAL_LEN);
                const nkv_key  nkvkeymeta2 = { (void*)meta_key_2, meta2_klen};
                nkv_value nkvvaluemeta = { (void*)meta_val, NKV_TEST_META_VAL_LEN, 0 };

                nkv_postprocess_function* p_fn1 = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
                p_fn1->nkv_aio_cb = nkv_aio_complete;
                p_fn1->private_data_1 = NULL;
                p_fn1->private_data_2 = (void*) p_fn1;
              
                //Meta read 1
                status = nkv_retrieve_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta2, &r_option, &nkvvaluemeta, p_fn1);
                if (status != 0) {
                  smg_error(logger, "NKV Retrieve Meta1 KVP Async start failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
                  nkv_close (nkv_handle, instance_uuid);
                  exit(1);
                }
                submitted.fetch_add(1, std::memory_order_relaxed);
              }
              {
                meta_key_2   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
                memset(meta_key_2, 0, NKV_TEST_META_KEY_LEN);
                sprintf(meta_key_2, "meta_2_%u", iter);
                meta_val = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
                memset(meta_val, 0, NKV_TEST_META_VAL_LEN);
                const nkv_key  nkvkeymeta2 = { (void*)meta_key_2, meta2_klen};
                nkv_value nkvvaluemeta = { (void*)meta_val, NKV_TEST_META_VAL_LEN, 0 };

                nkv_postprocess_function* p_fn2 = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
                p_fn2->nkv_aio_cb = nkv_aio_complete;
                p_fn2->private_data_1 = NULL;
                p_fn2->private_data_2 = (void*) p_fn2;

                //Meta read 2
                status = nkv_retrieve_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta2, &r_option, &nkvvaluemeta, p_fn2);
                if (status != 0) {
                  smg_error(logger, "NKV Retrieve Meta2 KVP Async start failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
                  nkv_close (nkv_handle, instance_uuid);
                  exit(1);
                }

                submitted.fetch_add(1, std::memory_order_relaxed);
              }
              {
                meta_key_2   = (char*)nkv_malloc(NKV_TEST_META_KEY_LEN);
                memset(meta_key_2, 0, NKV_TEST_META_KEY_LEN);
                sprintf(meta_key_2, "meta_2_%u", iter);
                meta_val = (char*)nkv_zalloc(NKV_TEST_META_VAL_LEN);
                memset(meta_val, 0, NKV_TEST_META_VAL_LEN);
                const nkv_key  nkvkeymeta2 = { (void*)meta_key_2, meta2_klen};
                nkv_value nkvvaluemeta = { (void*)meta_val, NKV_TEST_META_VAL_LEN, 0 };

                nkv_postprocess_function* p_fn3 = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
                p_fn3->nkv_aio_cb = nkv_aio_complete;
                p_fn3->private_data_1 = NULL; 
                p_fn3->private_data_2 = (void*) p_fn3;

                //Meta read 3
                status = nkv_retrieve_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta2, &r_option, &nkvvaluemeta, p_fn3);
                if (status != 0) {
                  smg_error(logger, "NKV Retrieve Meta3 KVP Async start failed !!, key = %s, error = %d", (char*) nkvkeymeta2.key, status);
                  nkv_close (nkv_handle, instance_uuid);
                  exit(1);
                }
                submitted.fetch_add(1, std::memory_order_relaxed);
              }
            }            
            status = nkv_retrieve_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey, &r_option, &nkvvalue, p_fn);
            if (status != 0) {
              smg_error(logger, "NKV Retrieve KVP Async start failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            }
            submitted.fetch_add(1, std::memory_order_relaxed);

            smg_info(logger, "NKV Retrieve async IO done starting, Submitted = %u, completed = %u",
                     submitted.load(), completed.load());

          }

        }
        break;

      case 2: /*DEL*/
        {
          if (!is_async) {
            status = nkv_delete_kvp (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey);
            if (status != 0) {
              smg_error(logger, "NKV Delete KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            } else {
              smg_info(logger, "NKV Delete successful, key = %s", (char*) nkvkey.key);
            }
          } else {
            /*Async DEL*/
            if (is_mixed) {
              const nkv_key  nkvkeymeta2 = { (void*)meta_key_2, meta2_klen};

              nkv_postprocess_function* p_fn1 = (nkv_postprocess_function*) malloc (sizeof(nkv_postprocess_function));
              p_fn1->nkv_aio_cb = nkv_aio_complete;
              p_fn1->private_data_1 = NULL;
              p_fn1->private_data_2 = (void*) p_fn1;

              status = nkv_delete_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkeymeta2, p_fn1);
              if (status != 0) {
                smg_error(logger, "NKV Delete KVP Async start failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
                nkv_close (nkv_handle, instance_uuid);
                exit(1);
              }
              submitted.fetch_add(1, std::memory_order_relaxed);

            }
            status = nkv_delete_kvp_async (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey, p_fn);
            if (status != 0) {
              smg_error(logger, "NKV Delete KVP Async start failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            }
            submitted.fetch_add(1, std::memory_order_relaxed);

            smg_info(logger, "NKV Delete async IO done starting, Submitted = %u, completed = %u",
                     submitted.load(), completed.load());

          }

        }
        break;


      case 3: //PUT, GET, DEL
        {
          val   = (char*)nkv_zalloc(vlen);
          memset(val, 0, vlen);
          nkv_value nkvvalue = { (void*)val, vlen, 0 };

          sprintf(val, "%0*d", klen, iter);
          if (!is_async) {
            //const nkv_key  nkvkey = { (void*)key_name, klen};
            //nkv_value nkvvalue = { (void*)val, vlen, 0 };

            if (check_integrity) {
              memset(cmpval, 0, vlen);
              strcpy(cmpval, (char*)nkvvalue.value);
            }
            if (hex_dump) {
              printf("Storing buffer:\n");
              memHexDump(nkvvalue.value, vlen);
            }

            status = nkv_store_kvp (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey, &s_option, &nkvvalue);
            if (status != 0) {
              smg_error(logger, "NKV Store KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            }
            smg_info(logger, "NKV Store successful, key = %s", key_name);
            memset(nkvvalue.value, 0, vlen);
            if (hex_dump) {
              printf("Before retrieve buffer:\n");
              memHexDump(nkvvalue.value, vlen);
            }

            status = nkv_retrieve_kvp (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey, &r_option, &nkvvalue);
            if (status != 0) {
              smg_error(logger, "NKV Retrieve KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            } else {
              smg_info(logger, "NKV Retrieve successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkey.key,
                      (char*) nkvvalue.value, nkvvalue.length, nkvvalue.actual_length);
              if (hex_dump) {
                printf("After retrieve  buffer:\n");
                memHexDump(nkvvalue.value, vlen);
              }

              if (check_integrity) {
                int n = memcmp(cmpval, (char*) nkvvalue.value, nkvvalue.actual_length);
                if (n != 0) {
                  //smg_error(logger, "Data integrity failed for key = %s", key_name);
                  smg_error(logger, "Data integrity failed for key = %s, expected = %s, got = %s, actual length = %u, mismatched byte = %d", 
                            key_name, cmpval, (char*) nkvvalue.value, nkvvalue.actual_length, n);
                  smg_error(logger, "Retrying once..");
                  memset(nkvvalue.value, 0, vlen);
                  status = nkv_retrieve_kvp (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey, &r_option, &nkvvalue);
                  if (status != 0) {
                    smg_error(logger, "NKV Retrieve retry KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
                    nkv_close (nkv_handle, instance_uuid);
                    exit(1);
                  } else {
                    smg_info(logger, "NKV Retrieve retry successful, key = %s, value = %s, len = %u, got actual length = %u", (char*) nkvkey.key,
                            (char*) nkvvalue.value, nkvvalue.length, nkvvalue.actual_length);                  
                    int n = memcmp(cmpval, (char*) nkvvalue.value, nkvvalue.actual_length);
                    if (n !=0 ) {
                      smg_error(logger, "Data integrity failed even after retry for key = %s, expected = %s, got = %s, actual length = %u, mismatched byte = %d",
                                key_name, cmpval, (char*) nkvvalue.value, nkvvalue.actual_length, n);                  
                      printf("Expected buffer:\n");
                      memHexDump(cmpval, nkvvalue.actual_length);
                      printf("Received buffer:\n");
                      memHexDump(nkvvalue.value, nkvvalue.actual_length);
                      nkv_close (nkv_handle, instance_uuid);
                      exit(1);
                    } else {
                      smg_info(logger,"Data integrity check is successful after retrying once for key = %s", key_name);
                    }
                  }
                } else {
                  smg_info(logger,"Data integrity check is successful for key = %s", key_name);
                }
              }
            }

            status = nkv_delete_kvp (nkv_handle, &io_ctx[iter % io_ctx_cnt], &nkvkey);
            if (status != 0) {
              smg_error(logger, "NKV Delete KVP call failed !!, key = %s, error = %d", (char*) nkvkey.key, status);
              nkv_close (nkv_handle, instance_uuid);
              exit(1);
            } else {
              smg_info(logger, "NKV Delete successful, key = %s", (char*) nkvkey.key);
            }
          } else {
            //Async PUT, GET, DEL
            smg_error(logger, "This operation is not supported for async !!");
            nkv_close (nkv_handle, instance_uuid);
            exit(1);
          }

        }
        break;

      default:
        smg_error(logger,"Unsupported operation provided, op = %d", op_type);
    }
    if (!is_async) {
      if (key_name) {
        nkv_free(key_name);
        key_name = NULL;
      }
      if (val) {
        nkv_free(val);
        val = NULL;
      }
    }
  }
  if (is_async) {
    smg_info(logger, "Waiting for all IO to finish, Submitted = %u, completed = %u", submitted.load(), completed.load());
    while(completed < submitted) {
      usleep(1);
    }
    smg_info(logger, "All async IO completed successfully, Submitted = %u, completed = %u", submitted.load(), completed.load());
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start);
  uint64_t total_num_objs = num_ios;
  long double tps = ((long double)((long double)total_num_objs/(long double)elapsed.count())) * 1000000;
  uint32_t rounded_tps = round(tps);
  uint32_t vlen_kb = (vlen/1024);
  uint32_t t_put_in_mb = (rounded_tps * vlen_kb)/1024;
  smg_alert (logger, "TPS = %u, Throughput = %u MB/sec, value_size = %u, total_num_objs = %u", rounded_tps,
             t_put_in_mb, vlen, total_num_objs);

  if (parent_op_type == 5) {
    std::cout << "Enter op_type:" << std::endl;
    std::cin>> op_type;
    if (op_type == -1)
      break;  
  
    std::cout << "Enter num_ios:" << std::endl;
    std::cin>> num_ios;

    std::cout << "Enter key prefix:" << std::endl;
    std::cin>> key_beginning; 

    if (key_delimiter) {
      std::cout << "Enter key delimiter:" << std::endl;
      std::cin>> key_delimiter;
    }

    smg_alert (logger, "Running with op_type = %d, num_ios = %u, key_beginning = %s, key_delimiter = %s", op_type, num_ios, key_beginning, key_delimiter);

  } else
    op_type = -1;

} while (op_type != -1);
  /*std::cout << "Press any key to quit.." << std::endl;
  char input;
  std::cin >> input;*/
  
  nkv_close (nkv_handle, instance_uuid);
  return 0;
}
