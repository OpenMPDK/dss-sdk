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
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include "dragonfly.h"
#include <df_io_module.h>
#include <module_hash.h>
#include <list_lib.h>
#include "rocksdb/dss_kv2blk_c.h"

#include <map>
#include <vector>
#include <unordered_map>


char key_default_delim = '/';
std::string key_default_delimlist = "/";
std::string NKV_ROOT_PREFIX = "root" + key_default_delimlist;

extern uint8_t __dfly_iter_next_handle;

static char __list_nqn_name[256] = {0};
extern int list_debug_level;
list_conf_t g_list_conf = {
	LIST_ENABLE_DEFAULT,
	LIST_NR_ZONES_DEFAULT,
	LIST_NR_CORES_DEFAULT,
	LIST_DEBUG_LEVEL_DEFAULT,
	LIST_TIMEOUT_DEFAULT_MS,
	LIST_PREFIX_HEAD,
};


int list_get_pool_id(struct dfly_subsystem *pool)
{
	return pool->id;
}

#define WAL_ENABLE_DEFAULT      0
#define list_ENABLE_DEFAULT	    1
#define list_NR_CORES_DEFAULT	1
#define list_NR_zoneS_DEFAULT    10
#define list_DEBUG_LEVEL_DEFAULT 0
#define list_TIMEOUT_DEFAULT_MS 2000
#define list_NQN_NAME		"nqn.2018-01.dragonfly:test1"

int list_handle_store_op(list_zone_t *zone, void *obj, int flags);
int list_handle_delete_op(list_zone_t *zone, void *obj, int flags);
int list_handle_open_op(list_zone_t *zone, void *obj, int flags);
int list_handle_close_op(list_zone_t *zone, void *obj, int flags);
int list_handle_read_op(list_zone_t *zone, void *obj, int flags);

void list_init_load_cb(struct df_dev_response_s resp, void *args,
		       dfly_iterator_info *dfly_iter_info);

///////////////////////// list module /////////////////////
dfly_io_module_handler_t g_list_io_handlers = {
	list_handle_store_op,
	NULL,   //  retrieve_handler
	list_handle_delete_op,
	NULL,   //  iter_ctrl_handler
	NULL,   //  iter_read_handler
	NULL,   //  handle_F1_op,
	NULL,   // handle_F2_op,
	list_handle_open_op,
	list_handle_close_op,
	list_handle_read_op,
};

///////////////////////// list module end ////////////////

void list_context_deinit(struct dfly_subsystem *pool)
{
	int i = 0;
	list_zone_t *zone = NULL;
	int pool_id = pool->id;
	if (!g_list_conf.list_enabled)
		return ;

	//TODO: from context
	int nr_zones = 0;

	for (i = 0; i < nr_zones; i++) {
		//TODO: offset from 0
//		zone = __list_ctx.zones[i];
		//dfly_device_close(zone->tgt_fh);
		delete zone->listing_keys;
	}

	//__list_ctx.nr_zones -= nr_zones;

//	if (!__list_ctx.nr_zones) {
//		df_free(__list_ctx.zones);
//		__list_ctx.zones = NULL;
////		df_free(__list_ctx.pools);
////		__list_ctx.pools = NULL;
//	}

}

#define MAX_LIST_ZONE    256
extern wal_conf_t g_wal_conf;

// * return DFLY_LIST_SUCCESS: insert new prefix/entry pair. new prefix
// * return DFLY_LIST_STORE_PREFIX_EXISTED: insert prefix/entry pair, prefix existed.
int list_handle_store_op(list_zone_t *zone, void *obj, int flags)
{
	int io_rc = DFLY_LIST_STORE_CONTINUE;
	assert(zone && obj);

	//std::shared_mutex write; //c++ 14

	//pthread_rwlock_t lock_rw = PTHREAD_RWLOCK_INITIALIZER;

	list_prefix_entry_pair_t *pe = (list_prefix_entry_pair_t *)obj;
	std::string prefix(pe->prefix, pe->prefix_size);
	std::string entry(pe->entry, pe->entry_size);

	//if(!pthread_rwlock_wrlock(&lock_rw))
	if ((*zone->listing_keys).count(prefix) > 0)
		io_rc = DFLY_LIST_STORE_DONE;       //the prefix already exist, store done

	if(entry.length() != 0) {
		auto ret = (*zone->listing_keys)[prefix].emplace(entry);
		//if (!ret.second)
		//	list_log("list_handle_store_op: zone[%d] pe=%s:%d:%s:%d emplace fails\n", zone->zone_idx,
		//		 prefix.c_str(), prefix.size(),
		//		 entry.c_str(), entry.size());
		//else
		
		//list_log(" pe=%s:%d:%s:%d\n", 
        //    prefix.c_str(), prefix.size(),
        //    entry.c_str(), entry.size());
	} else {
		DFLY_ASSERT(pe->entry_size == 0);
	}

	//else
	//	list_log("list_handle_store_op: zone[%d] pe=%s:%d:%s:%d\n", zone->zone_idx,
	//		 prefix.c_str(), prefix.size(),
	//		 entry.c_str(), entry.size());

	//if(pthread_rwlock_unlock(&lock_rw))
	//    assert(0);
	return io_rc;
}

// * return list_SUCCESS: new item, status = list_DELETE_PENDING
// * return list_ERROR_IO_RETRY if item existed with other status.
int list_handle_delete_op(list_zone_t *zone, void *obj, int flags)
{
	int io_rc = DFLY_LIST_DEL_CONTINUE;
	assert(zone && obj);

	//pthread_rwlock_t lock_rw = PTHREAD_RWLOCK_INITIALIZER;

	list_prefix_entry_pair_t *pe = (list_prefix_entry_pair_t *)obj;
	std::string prefix(pe->prefix, pe->prefix_size);
	std::string entry(pe->entry, pe->entry_size);

	//if(!pthread_rwlock_wrlock(&lock_rw))
	if ((*zone->listing_keys).count(prefix) == 0) {
		//non exist prefix.
	} else {
		(*zone->listing_keys)[prefix].erase(entry);
		if ((*zone->listing_keys)[prefix].empty()) {
			(*zone->listing_keys).erase(prefix);
			list_log("list_handle_delete_op prefix=%s entry=%s both\n", prefix.c_str(), entry.c_str());
		} else {
			io_rc = DFLY_LIST_DEL_DONE; // this prefix list is not empty, del done
			list_log("list_handle_delete_op prefix=%s entry=%s entry only\n", prefix.c_str(), entry.c_str());
		}
	}
	//if(pthread_rwlock_unlock(&lock_rw))
	//    assert(0);

	return io_rc;
}

int list_handle_open_op(list_zone_t *zone, void *obj, int flags)
{
	int io_rc = DFLY_LIST_SUCCESS;
	assert(zone && obj);

	return io_rc;
}

int list_handle_close_op(list_zone_t *zone, void *obj, int flags)
{
	int io_rc = DFLY_LIST_SUCCESS;
	assert(zone && obj);
	return io_rc;
}

int list_handle_read_op(list_zone_t *zone, void *obj, int flags)
{
	int io_rc = DFLY_LIST_READ_DONE;
	assert(zone && obj);
	struct dfly_request *req = (struct dfly_request *) obj;
	assert(zone->zone_idx == req->list_data.list_zone_idx);

	struct dfly_key *key = req->ops.get_key(req);
	struct dfly_value *val = req->ops.get_value(req);
	if (!val->value || val->length < sizeof(uint32_t)  * 3) {
		list_log("invalid val size %d or val buffer %p\n", val->length, val->value);
		dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD,
				     SPDK_NVME_SC_KV_INVALID_VALUE_SIZE);
		return io_rc;
	}

	int offset = req->list_data.start_key_offset;
	int payload_sz = 0;
	std::string prefix = NKV_ROOT_PREFIX;
	std::string start_key;
	std::set<std::string>::iterator iter;

	uint32_t *value_buffer_nr_key = (uint32_t *)((void *)val->value + val->offset);
	uint32_t *key_sz = (uint32_t *)((void *)val->value + val->offset + sizeof(uint32_t));
	void *key_ptr = key_sz + 1;
	int list_buffer_sz = req->list_data.list_size - sizeof(*value_buffer_nr_key);
	uint32_t nr_keys = 0;

	bool list_from_begining = true;
	payload_sz = key->length;

	if (offset && offset < payload_sz) {
		req->list_data.options = DFLY_LIST_OPTION_PREFIX_FROM_START_KEY;
		list_from_begining = false;
		prefix = std::string((const char *)(key->key), offset);
		start_key = std::string((const char *)(key->key + offset), payload_sz - offset);
		//list_log("option: prefix '%s' start_key '%s'\n", prefix.c_str(), start_key.c_str());
	} else if (offset && offset == payload_sz) {
		req->list_data.options = DFLY_LIST_OPTION_PREFIX_FROM_BEGIN;
		prefix = std::string((const char *)(key->key), payload_sz);
		//list_log("option: prefix '%s' from begin\n", prefix.c_str());
	} else if (!offset && payload_sz) {
		req->list_data.options = DFLY_LIST_OPTION_ROOT_FROM_START_KEY;
		list_from_begining = false;
		start_key = std::string((const char *)(key->key), payload_sz);
		//list_log("option: 'root/' start_key '%s'\n", start_key.c_str());
	} else if (!offset && !payload_sz) {
		req->list_data.options = DFLY_LIST_OPTION_ROOT_FROM_BEGIN;
		//list_log("option: 'root/' from begin\n");
	} else {
		dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD,
				     SPDK_NVME_SC_KV_LIST_CMD_UNSUPPORTED_OPTION);
		//list_log("unsupported list option: \n");
		goto done_list_read;
	}

	//list_log("list_handle_read_op zone[%d] req %p \nlist_data info: "
	//	 "list_zone_idx=%d list_size=%d val->length %d max_keys_requested=%d start_key_offset=%d "
	//	 "prefix=%s:%d start_key=%s:%d\n",
	//	 zone->zone_idx, req,
	//	 req->list_data.list_zone_idx, val->length,
	//	 req->list_data.list_size,
	//	 req->list_data.max_keys_requested,
	//	 offset,
	//	 prefix.c_str(), prefix.size(),
	//	 start_key.c_str(), start_key.size());

	if (zone->listing_keys->find(prefix) == zone->listing_keys->end()) {
		list_log("no prefix=%s sz=%d found\n", prefix.c_str(), prefix.size());
		dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD,
				     SPDK_NVME_SC_KV_LIST_CMD_NONEXIST_PREFIX);
		goto done_list_read;
	}

	assert(req->list_data.list_size >= val->length);

	iter = (*zone->listing_keys)[prefix].begin();
	if (!list_from_begining) {
		iter = (*zone->listing_keys)[prefix].find(start_key);
		if (iter == (*zone->listing_keys)[prefix].end()) {
			list_log("no prefix %s with start_key %s found\n", prefix.c_str(), start_key.c_str());
			dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD,
					     SPDK_NVME_SC_KV_LIST_CMD_NONEXIST_STARTKEY);
			goto done_list_read;
		}
		iter ++;
	}

	list_log("ss %s Found %d keys for prefix %s:%d start %s:%d\n",
        req->req_dfly_ss->name, 
        std::distance(iter, (*zone->listing_keys)[prefix].end()),
		 prefix.c_str(), prefix.size(),
		 start_key.c_str(), start_key.size());

	while (iter != (*zone->listing_keys)[prefix].end()) {
		std::string k = (*iter);
		int k_dw = (k.size() + 4 - 1) / 4 + 1;

		if (list_buffer_sz < k_dw * 4)
			break;

		//list_log("%s:%d\n", k.c_str(), k_dw);
		memcpy(key_ptr, k.c_str(), k.size());
		*key_sz = k.size();
		DFLY_ASSERT(*key_sz != 0);
		key_sz += k_dw;
		key_ptr = key_sz + 1;
		nr_keys ++;
		list_buffer_sz -= (k_dw * 4);
		if (nr_keys >= req->list_data.max_keys_requested) {
			break;
		}
		iter ++;
	}

	if (nr_keys == 0 && iter == (*zone->listing_keys)[prefix].end()) {
		dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD,
				     SPDK_NVME_SC_KV_LIST_CMD_END_OF_LIST);
	}

	list_log("list: max_keys_requested %d Returing %d keys data_sz %d\n",
		 req->list_data.max_keys_requested, nr_keys, req->list_data.list_size - list_buffer_sz);

done_list_read:
	* value_buffer_nr_key = nr_keys;
	dfly_resp_set_cdw0(req, req->list_data.list_size - list_buffer_sz);

	return io_rc;
}

bool list_find_key_prefix(void *ctx, struct dfly_key *key, dfly_list_info_t *list_data,
			  list_prefix_entry_pair_t &pe, int &zone_idx)
{
	list_thread_inst_ctx_t *list_inst_ctx = (list_thread_inst_ctx_t *) ctx;
	uint16_t i = 0;

	int pe_cnt_tbd = ATOMIC_READ(list_data->pe_cnt_tbd);
	if (!pe_cnt_tbd)
		return false;

	int inst_nr_zone = list_inst_ctx->nr_zones;
	std::vector<int16_t > zone_ids(256, -1);
	for (i = 0; i < inst_nr_zone; i++) {
		zone_ids[list_inst_ctx->zone_arr[i]->zone_idx] = list_inst_ctx->zone_arr[i]->zone_idx;
	}

	i = list_data->pe_total_cnt;
	int16_t z_id = list_data->prefix_key_info[(i - 1) * 2];
	while (i && (z_id < 0)) {
		i--;
		z_id = list_data->prefix_key_info[(i - 1) * 2];
	}

	if (i && zone_ids[list_data->prefix_key_info[(i - 1) * 2]] >= 0) { //

		i--; //prefix index

		int16_t  pos = list_data->prefix_key_info[i * 2 + 1];
		int16_t  next_pos = list_data->prefix_key_info[(i + 1) * 2 + 1];
		if (next_pos < 0 || next_pos > key->length)
			next_pos = key->length;

		if (pos) {
			pe.prefix = key->key;
			pe.prefix_size = pos;
		} else {
			//NKV_ROOT_PREFIX
			pe.prefix = NKV_ROOT_PREFIX.c_str();
			pe.prefix_size = NKV_ROOT_PREFIX.size();
		}
		pe.entry = key->key + pos;
		pe.entry_size = next_pos - pos;

		zone_idx = zone_ids[list_data->prefix_key_info[i * 2]];

		list_data->prefix_key_info[i * 2] = -1; //reset the prefix info
		ATOMIC_DEC_FETCH(list_data->pe_cnt_tbd);

		list_log("i %d pos %d: prefix_size %d, next_pos %d entry_size %d key->length %d\n", 
            i, pos, pe.prefix_size, next_pos, pe.entry_size, key->length);

		return true;
	}
	//list_log("do_list_io ctx %p\n", ctx);
	return false;

}

int do_list_io(void *ctx, struct dfly_key *key, struct dfly_request *req, int opc, int op_flags)
{
	list_prefix_entry_pair_t pe;
	list_zone_t *zone = NULL;
	int zone_idx = -1;
	int rc = DFLY_LIST_IO_RC_PASS_THROUGH;
	dfly_list_info_t *list_data = &req->list_data;
	list_thread_inst_ctx_t *list_inst_ctx = (list_thread_inst_ctx_t *) ctx;
	dfly_io_module_handler_t *ops = list_inst_ctx->mctx->io_ctx.io_handlers;

	if (!g_list_conf.list_enabled)
		return rc;

	//list_log("do_list_io ctx %p\n", ctx);

	if (opc == SPDK_NVME_OPC_SAMSUNG_KV_LIST_READ) {
		zone = &list_inst_ctx->mctx->zones[req->list_data.list_zone_idx];
		rc = ops->list_read_handler(zone, req, op_flags);
	} else {
		while (list_find_key_prefix(ctx, key, list_data, pe, zone_idx)) {
			assert(zone_idx >= 0 && zone_idx < list_inst_ctx->mctx->nr_zones);
			zone = &list_inst_ctx->mctx->zones[zone_idx];
			switch (opc) {
			case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
                {
                std::string prefix(pe.prefix, pe.prefix_size);
	            std::string entry(pe.entry, pe.entry_size);
                list_log("store ss %s pe=%s:%d:%s:%d\n", 
                    req->req_dfly_ss->name, 
                    prefix.c_str(), prefix.size(),
                    entry.c_str(), entry.size());
                }
				rc = ops->store_handler(zone, &pe, op_flags);
				break;
			case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
				rc = ops->delete_handler(zone, &pe, op_flags);
				break;
			default:
				list_log("do_list_io: unsupported opc %x\n", opc);
				assert(0);
				break;
			}
			if (rc == DFLY_LIST_STORE_DONE || rc == DFLY_LIST_DEL_DONE) {
				break;
			}
		}
	}
	return rc;
}

struct dss_list_read_process_ctx_s {
	struct dfly_value *val;
	uint32_t *total_keys;
	uint32_t max_keys;
	uint32_t *key_sz;
	void *key;
	uint32_t rem_buffer_len;
	char delim;
};

int do_list_item_process(void *ctx, const char *key, int is_leaf)
{
	struct dss_list_read_process_ctx_s *lp_ctx = (struct dss_list_read_process_ctx_s *) ctx;
	uint32_t keylen = strlen(key);

	int k_dw;

	if(!is_leaf) {
		keylen++;
	}

	k_dw = (keylen + 4 - 1) / 4 + 1;

	if(lp_ctx->rem_buffer_len < k_dw * 4 ) {
		return -1;//No room for more keys
	}

	if(is_leaf) {
		memcpy(lp_ctx->key, key, keylen);
	} else {
		memcpy(lp_ctx->key, key, keylen - 1);
		((char *)lp_ctx->key)[keylen - 1] = lp_ctx->delim;
	}

	*lp_ctx->key_sz = keylen;

	lp_ctx->key_sz += k_dw;
	lp_ctx->key = lp_ctx->key_sz + 1;// int size inc after key sz is start of next key

	lp_ctx->rem_buffer_len -= (k_dw * 4);

	(*lp_ctx->total_keys)++;

	if(*lp_ctx->total_keys == lp_ctx->max_keys) {
		return -1;
	}

	return 0;
}

int do_list_io_judy(void *ctx, struct dfly_request *req)
{
	int opc = req->ops.get_command(req);
	struct dfly_key *key = req->ops.get_key(req);
	list_thread_inst_ctx_t *list_inst_ctx = (list_thread_inst_ctx_t *) ctx;

	dss_hsl_ctx_t *hsl_ctx = NULL;
	struct dss_list_read_process_ctx_s lp_ctx;

	std::string key_str((char *)key->key, (size_t)key->length);
	int rc = DFLY_LIST_IO_RC_PASS_THROUGH;

	if (!g_list_conf.list_enabled)
		return rc;


	hsl_ctx = list_inst_ctx->mctx->zones[0].hsl_keys_ctx;

	switch(opc) {
		case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
			//DFLY_NOTICELOG("hsl insert key: %s\n", key_str.c_str());
			dss_hsl_insert(hsl_ctx, key_str.c_str());
			rc = DFLY_LIST_STORE_DONE;
			break;
		case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
			//DFLY_NOTICELOG("hsl delete key: %s\n", key_str.c_str());
			dss_hsl_delete(hsl_ctx, key_str.c_str());
			rc = DFLY_LIST_DEL_DONE;
			break;
		case SPDK_NVME_OPC_SAMSUNG_KV_LIST_READ:
			int offset = req->list_data.start_key_offset;
			std::string prefix((char *)key->key, (size_t)offset);

			lp_ctx.val = req->ops.get_value(req);;
			lp_ctx.max_keys = req->list_data.max_keys_requested;
			lp_ctx.delim = hsl_ctx->delim_str[0];

			lp_ctx.rem_buffer_len = lp_ctx.val->length;
			if(lp_ctx.rem_buffer_len <= 2 * sizeof(uint32_t)) {
				//Not enough value buffer to do listing
				//Minimum to hold total keys plus one key length
				//Return error code
			}

			lp_ctx.total_keys = (uint32_t *)((char *)lp_ctx.val->value + lp_ctx.val->offset);
			lp_ctx.rem_buffer_len -= sizeof(uint32_t);

			lp_ctx.key_sz = (uint32_t *)((void *)lp_ctx.val->value + lp_ctx.val->offset + sizeof(uint32_t));
			//lp_ctx.rem_buffer_len -= sizeof(uint32_t); // include for calculation for next key write

			lp_ctx.key    = lp_ctx.key_sz + 1;//Advance by int pointer

			*lp_ctx.total_keys = 0;
			*lp_ctx.key_sz = 0;

			if(!offset) {
				//Root listing not required
				DFLY_NOTICELOG("hsl read list: root listing not supported\n");
				dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD,
				     SPDK_NVME_SC_KV_LIST_CMD_UNSUPPORTED_OPTION);
			} else if (offset && offset < key->length) {
				std::string start_key;
				if(((char *)(key->key))[key->length - 1] == lp_ctx.delim) {
					start_key = std::string((char *)(key->key + offset), key->length - offset - 1);
				} else {
					start_key = std::string((char *)(key->key + offset), key->length - offset);
				}
				dss_hsl_list(hsl_ctx, prefix.c_str(), start_key.c_str(), &lp_ctx);
			} else {
				dss_hsl_list(hsl_ctx, key_str.c_str(), NULL, &lp_ctx);
			}
			//DFLY_NOTICELOG("hsl read list: %s\n", key_str.c_str());
			if (*lp_ctx.total_keys == 0) {
				dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD,
						     SPDK_NVME_SC_KV_LIST_CMD_END_OF_LIST);
			}
			dfly_resp_set_cdw0(req, lp_ctx.val->length - lp_ctx.rem_buffer_len);
			//TODO: List Read
			rc = DFLY_LIST_READ_DONE;
			break;
		default:
			list_log("do_list_io_judy: unsupported opc %x\n", opc);
			assert(0);
			break;
	}

	return rc;
}

int list_io(void *ctx, struct dfly_request *req, int list_op_flags)
{
	int pool_id = list_get_pool_id(req->req_dfly_ss);
	int opc = req->ops.get_command(req);

	int io_rc;

	if (!(opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE
	      || opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE
	      || opc == SPDK_NVME_OPC_SAMSUNG_KV_LIST_OPEN
	      || opc == SPDK_NVME_OPC_SAMSUNG_KV_LIST_CLOSE
	      || opc == SPDK_NVME_OPC_SAMSUNG_KV_LIST_READ))
		return DFLY_LIST_IO_RC_PASS_THROUGH;

	struct dfly_key *key = req->ops.get_key(req);

	if(g_dragonfly->dss_enable_judy_listing) {
		return do_list_io_judy(ctx, req);
	}

	io_rc = do_list_io(ctx, key, req, opc, list_op_flags);

	return io_rc;
}

void list_module_load_done_cb(struct dfly_subsystem *pool, void *arg/*Not used*/);
int list_init_load_done(struct dfly_request *req)
{
	struct dfly_subsystem *subsystem = req->req_dfly_ss;
	//clean up the resource
	if (req->req_value.value) {
		free(req->req_value.value);
	}
	dfly_io_put_req(NULL, req);
	printf("list_init_load_done!!!");
	//cb to continue the starup init
	subsystem->list_init_status = LIST_INIT_DONE;
	list_module_load_done_cb(subsystem, NULL);
}

int list_send_iter_cmd(struct dfly_request *req, int opc, dfly_iterator_info *iter_info)
{
	struct spdk_nvme_cmd *cmd = (struct spdk_nvme_cmd *)req->req_ctx;
	struct dword_bytes *cdw11 = (struct dword_bytes *)&cmd->cdw11;

	unsigned int PREFIX_KV = 0;
	switch (opc) {
	case KVS_ITERATOR_OPEN:
		cmd->opc = SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL;
		cmd->nsid = 1;                              //nsid;        // 1
		cdw11->cdwb2 = (KVS_ITERATOR_OPEN | KVS_ITERATOR_KEY);
		//cmd->cdw12 = 0x6174656d ; //PREFIX_KV;     //iter_handle->prefix;   // "0000" for init_iter_prefix //0x6174656d -> meta

		if (strlen(g_list_conf.list_prefix_head)) {
			for (int i = 0; i < 4; i++) {
				PREFIX_KV |= (g_list_conf.list_prefix_head[i] << i * 8);
			}
			cmd->cdw12 = PREFIX_KV;
			cmd->cdw13 = 0xFFFFFFFF;
		} else {
			cmd->cdw12 = 0x0; //0x6174656d;    //PREFIX_KV;
			cmd->cdw13 = 0x0; //0xFFFFFFFF;    //iter_handle->bitmask;   //0xFFFFFFFF -> -1
		}
		req->iter_data.iter_option = cdw11->cdwb2;
		break;
	case KVS_ITERATOR_CLOSE:
		cmd->opc = SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL;
		cmd->nsid = 1; //nsid;        // 1
		cdw11->cdwb2 = KVS_ITERATOR_CLOSE;
		cdw11->cdwb1 = __dfly_iter_next_handle % 255;
		req->iter_data.iter_option = cdw11->cdwb2;
		break;
	case KVS_ITERATOR_KEY:
		cmd->opc = SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ;
		cmd->nsid = 1;  //nsid;        // 1
		cdw11->cdwb1 = 1;
		break;
	default:
		assert(0);
		break;

	}

	//req->iter_data.iter_option = cdw11->cdwb2;
	req->iter_data.internal_cb = list_init_load_cb;

	return iter_io(req->req_dfly_ss, req, iter_info);

}



inline
bool do_op_list_map(std::string& prefix, std::string& entry, list_context_t* list_ctx, bool is_del) {
  
		uint32_t h = hash_sdbm(prefix.c_str(), prefix.size());
		int zone_idx = h % g_list_conf.list_zone_per_pool;
		//printf("list_load_iter_read_keys: prefix %s entry %s zone_idx %d\n",
		//    prefixes[i].c_str(), entries[i].c_str(), zone_idx);
		list_zone_t *zone = &list_ctx->zones[zone_idx];
		assert(zone);

		DFLY_ASSERT(!entry.empty());
		if (!is_del) { //for insert
			if ((*zone->listing_keys).count(prefix)) {
				(*zone->listing_keys)[prefix].emplace(std::move(entry));
				return true;
			}
			(*zone->listing_keys)[prefix].emplace(std::move(entry));
		} else { //for deletion
			bool prefix_is_empty = true;
			if ((*zone->listing_keys).count(prefix) == 0) {
				//non exist prefix.
				return true;;
			} else {
				(*zone->listing_keys)[prefix].erase(entry);
				if ((*zone->listing_keys)[prefix].empty()) {
					(*zone->listing_keys).erase(prefix);
				} else {
					prefix_is_empty = false;
				}
				if (!prefix_is_empty) {
					return false;
				}
			}
		}
		
		return false;
}


int g_list_prefix_head_size = 0;
//return 0 on success
int list_key_update_helper(struct dfly_subsystem *pool, const char *key_str, size_t key_sz, bool is_del,
		    bool is_wal_recovery, std::vector<std::string>& prefixes, std::vector<std::string>& entries, std::vector<size_t>& positions)
{
	//check if prefix match
	if (key_sz < g_list_prefix_head_size)
		return -1;

	if (g_list_conf.list_prefix_head[0]) {
		if (memcmp(g_list_conf.list_prefix_head, key_str, g_list_prefix_head_size)){
			return -1;
		}
	}

	if (!pool) {
		return -1;
	}

	std::string key(key_str, key_sz);
	int key_unit_sz = (key_sz + ITER_LIST_ALIGN - 1) & ITER_LIST_ALIGN_MASK;

	list_context_t *list_ctx = (list_context_t *)dfly_module_get_ctx(pool->mlist.dfly_list_module);

	if (is_wal_recovery) {
	}

	size_t pos_end = std::string::npos;
	size_t pos = 0;
	std::string prefix;
	std::string entry;
	bool dup_flag = false;	

	pos = key.find_last_of(key_default_delimlist, pos_end);
	if(pos != std::string::npos) {
		entry = key.substr(pos + 1, key.size() - pos);
		prefix = key.substr(0, pos+1);
		if(entry.length() != 0 ) {
			dup_flag = do_op_list_map(prefix, entry, list_ctx, is_del);
		}
		pos_end = pos - 1;
		pos = key.find_last_of(key_default_delimlist, pos_end);
	}
	
	while(!dup_flag && pos != std::string::npos) {
		entry = key.substr(pos + 1, pos_end - pos + 1);
		prefix = key.substr(0, pos+1);
		DFLY_ASSERT(entry.length() != 0);
		dup_flag = do_op_list_map(prefix, entry, list_ctx, is_del);
		
		pos_end = pos - 1;
		pos = key.find_last_of(key_default_delimlist, pos_end); 
	}

	if(!dup_flag) {
		pos = 0;
		if(pos_end == std::string::npos) {
			entry = key.substr(pos, key.size());
		} else {
			entry = key.substr(pos, pos_end + 1);
		}
	
		DFLY_ASSERT(entry.length() != 0);
		(void)do_op_list_map(NKV_ROOT_PREFIX, entry, list_ctx, is_del);
	}
	
	return 0;

}


int list_key_update(struct dfly_subsystem *pool, const char *key_str, size_t key_sz, bool is_del,
		    bool is_wal_recovery)
{
	std::vector<std::string> prefixes;
	std::vector<std::string> entries;
	std::vector<size_t> positions;
	
	prefixes.reserve(16);
	entries.reserve(16);
	positions.reserve(16);

	return list_key_update_helper(pool, key_str, key_sz, is_del, is_wal_recovery, prefixes, entries, positions);	
}

void list_load_iter_read_keys(struct dfly_request *req)
{
	assert(req);
	struct dfly_value *val = req->ops.get_value(req);
	assert(val && val->value);
	void *data = val->value + val->offset;
	int sz = val->length;
	int nr_keys_tbd = *(uint32_t *)data;
	int nr_keys = nr_keys_tbd;
	int key_sz = 0;
	int key_unit_sz = 0;
	static long long nr_keys_itered = 0;
	static long long nr_keys_parsed = 0;
	static long long nr_keys_updated = 0;
	list_log("list_parse_init_keys buffer_sz %d nr_key %d, iter_option 0x%x\n",
		 sz, nr_keys, req->iter_data.iter_option);

	nr_keys_itered += nr_keys;

	data += sizeof(uint32_t);

	std::vector<std::string> prefixes;
	std::vector<std::string> entries;
	std::vector<size_t> positions;
	while (nr_keys && sz >= 0) {
		key_sz = *(int *)data;
		assert(key_sz);
		data += sizeof(uint32_t);
		std::string key((const char *)data, key_sz);
		key_unit_sz = (key_sz + ITER_LIST_ALIGN - 1) & ITER_LIST_ALIGN_MASK;
		sz -= (sizeof(uint32_t) + key_unit_sz);

		if (!list_key_update_helper(req->req_dfly_ss, data, key_sz, false, false, prefixes, entries, positions))
			nr_keys_updated ++;


		data += key_unit_sz;
		nr_keys --;
		nr_keys_parsed ++;
	}
	
	if (nr_keys_tbd)
	//	printf("list_load_iter_read_keys:  %lld keys updated\n", nr_keys_updated);

	//  printf("list_load_iter_read_keys: insert nr_keys %d total itered %lld, parsed %lld\n",
	//      nr_keys_tbd, nr_keys_itered, nr_keys_parsed);

	assert(!nr_keys);


}
void list_init_load_cb(struct df_dev_response_s resp, void *args,
		       dfly_iterator_info *dfly_iter_info)
{
	assert(resp.rc);
	struct dfly_request *req = (struct dfly_request *)args;
	int opc = req->ops.get_command(req);
	list_log("list_init_load_cb: opc = %x\n", opc);
	if (opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL) {

		if (req->iter_data.iter_option & KVS_ITERATOR_OPEN) {
			//prepare the read command
			list_send_iter_cmd(req, KVS_ITERATOR_KEY, dfly_iter_info);
		} else if (req->iter_data.iter_option & KVS_ITERATOR_CLOSE) {
			//done the iter for close
			list_init_load_done(req);
		} else {
			assert(0);
		}

	} else if (opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ) {
		// parse the data
		list_load_iter_read_keys(req);
		if (!(req->iter_data.iter_option & KVS_ITERATOR_READ_EOF))
			list_send_iter_cmd(req, KVS_ITERATOR_KEY, dfly_iter_info); //for next
		else
			list_send_iter_cmd(req, KVS_ITERATOR_CLOSE, dfly_iter_info);    //EOF, close the iter

	} else {
		assert(0);
	}
}

extern struct dfly_request_ops df_req_ops_inst;
int list_init_load_by_iter(struct dfly_subsystem *pool)
{
	int rc = LIST_INIT_PENDING; //LIST_INIT_PENDING
	struct dfly_request *req = dfly_io_get_req(NULL);
	assert(req);
	req->flags |= DFLY_REQF_NVME;
	req->req_dfly_ss = pool;
	req->ops = df_req_ops_inst;
	req->req_value.offset = 0;
	req->req_value.length = 2048 * 1024;
	req->req_value.value = malloc(req->req_value.length);

	rc = list_send_iter_cmd(req, KVS_ITERATOR_OPEN, NULL);
	if (rc == DFLY_ITER_IO_PENDING)
		rc = LIST_INIT_PENDING;

	return rc;
}

void list_module_load_done_blk_cb(struct dfly_subsystem *pool, int rc);
typedef void (*list_done_cb)(void * ctx, int rc);
int dss_rocksdb_list_key(void *ctx, void * pool, void * prefix, size_t prefix_size, list_done_cb list_cb);
int list_init_load_by_blk_iter(struct dfly_subsystem *pool)
{
    int rc = LIST_INIT_PENDING;
    char prefix[SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1] = {0};
    size_t prefix_size = SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1;
    if(g_list_prefix_head_size && g_list_prefix_head_size< prefix_size && 
        g_list_conf.list_prefix_head){
        memcpy(prefix, g_list_conf.list_prefix_head, g_list_prefix_head_size);
        prefix_size = g_list_prefix_head_size;
    } 
    for(int i = 0; i< pool->num_io_devices; i++){
     printf("list_init_load_by_blk_iter: nr_dev %d dev %p\n", pool->num_io_devices, &pool->devices[i]);
     rc = dss_rocksdb_list_key(&pool->devices[i], prefix, prefix_size, list_module_load_done_blk_cb);
     printf("list_init_load_by_blk_iter [%d] rc = %d\n", i, rc);
    }
    
	return rc;
}

int list_finish(struct dfly_subsystem *pool)
{
	list_context_deinit(pool);
	return 0;
}

