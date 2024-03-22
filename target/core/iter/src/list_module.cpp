/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *  	* Redistributions of source code must retain the above copyright
 *  	  notice, this list of conditions and the following disclaimer.
 *  	* Redistributions in binary form must reproduce the above copyright
 *  	  notice, this list of conditions and the following disclaimer in
 *  	  the documentation and/or other materials provided with the distribution.
 *  	* Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 *  	  contributors may be used to endorse or promote products derived from
 *  	  this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 *  BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *  BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *  FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <set>
#include <string>
#include <vector>

#include "dragonfly.h"
#include "df_io_module.h"
#include "module_hash.h"
#include "list_lib.h"

#include "utils/dss_hsl.h"

#ifdef DSS_ENABLE_ROCKSDB_KV
#include "rocksdb/dss_kv2blk_c.h"
#endif//#ifdef DSS_ENABLE_ROCKSDB_KV

//namespace std;

extern list_conf_t g_list_conf;

int list_debug_level = 1;
//std::string list_prefix = "0000";
extern std::string key_default_delimlist ;
extern std::string NKV_ROOT_PREFIX ;

void *list_find_instance_ctx(struct dfly_request *request);

dss_hsl_ctx_t * dss_get_hsl_context(struct dfly_subsystem *pool)
{
	list_context_t *ctx = (list_context_t *)dfly_module_get_ctx(pool->mlist.dfly_list_module);

	return ctx->zones[0].hsl_keys_ctx;
}

bool list_valid_prefix(struct dfly_request * req){
    struct dfly_key *key = req->ops.get_key(req);
    assert(key);
    if(!strncmp((char *)key->key, g_list_conf.list_prefix_head,
        strlen(g_list_conf.list_prefix_head)))
        return true;
    else
        return false;
}
int dfly_list_req_process(void *ctx, struct dfly_request *req)
{
    //if(list_valid_prefix(req)){       
    	int io_rc = list_io(ctx, req, g_list_conf.list_op_flag & DF_OP_MASK);
    	dfly_list_info_t *list_data = &req->list_data;
#ifdef DSS_OPEN_SOURCE_RELEASE
	if (!ATOMIC_READ(list_data->pe_cnt_tbd)
	    || io_rc == DFLY_LIST_STORE_DONE
	    || io_rc == DFLY_LIST_DEL_DONE
	    || io_rc == DFLY_LIST_READ_DONE) {
		req->next_action = DFLY_REQ_IO_LIST_DONE;
		}
#else
		if(!g_dragonfly->dss_enable_judy_listing) {
    	if (!ATOMIC_READ(list_data->pe_cnt_tbd)
    	    || io_rc == DFLY_LIST_STORE_DONE
    	    || io_rc == DFLY_LIST_DEL_DONE
    	    || io_rc == DFLY_LIST_READ_DONE) {
    		req->next_action = DFLY_REQ_IO_LIST_DONE;
			}
		} else {
			if(io_rc == DFLY_LIST_READ_PENDING) {
				return DFLY_MODULE_REQUEST_QUEUED;
			}
			req->next_action = DFLY_REQ_IO_LIST_DONE;
		}
    //}else{
    //    req->next_action = DFLY_REQ_IO_LIST_DONE;        
    //}
#endif
    
	req->state = DFLY_REQ_IO_NVMF_DONE;
	//list_log("dfly_list_req_process req %p io_rc %x pe_cnt_tbd %x, state %x, next_action %x\n",
	//	 req, io_rc, list_data->pe_cnt_tbd, req->state, req->next_action);

	if(dss_subsystem_kv_mode_enabled((dss_subsystem_t *) req->req_dfly_ss)) {
		dss_module_post_to_instance(DSS_MODULE_NET, req->common_req.module_ctx[DSS_MODULE_NET].module_instance, req);
		return DFLY_MODULE_REQUEST_QUEUED;//Request already posted to network module queue
	}
	return DFLY_MODULE_REQUEST_PROCESSED;
}

//Call only once per thread instantiation
void *list_module_store_inst_context(void *mctx, void *inst_ctx, int inst_index)
{
	list_context_t *list_mctx = (list_context_t *)mctx;
	struct list_thread_inst_ctx *list_thrd_ctx;

	list_zone_t *zone = NULL;
	int num_cores = g_list_conf.list_nr_cores;
	int max_zones = g_list_conf.list_zone_per_pool;
	int current_zone_idx = inst_index;

	assert(max_zones >= 1);

	if (current_zone_idx > max_zones) {
		list_log("More threads %d than number of zones %d\n", current_zone_idx, max_zones);
		return NULL;
	}

	list_thrd_ctx = (struct list_thread_inst_ctx *)calloc(1, sizeof(struct list_thread_inst_ctx) + sizeof(list_zone_t *) * max_zones);
	if (!list_thrd_ctx) {
		assert(list_thrd_ctx);
	}
	list_thrd_ctx->nr_zones = 0;
	list_thrd_ctx->mctx = list_mctx;
	do {
		zone = &list_mctx->zones[current_zone_idx];
		DFLY_ASSERT(zone->module_instance_ctx == NULL);
		zone->module_instance_ctx = inst_ctx;
		list_thrd_ctx->zone_arr[list_thrd_ctx->nr_zones] = zone;
		list_log("list_module_store_inst_context zone[%d]=inst %p zone %p\n",
			 current_zone_idx, inst_ctx, zone);
		list_thrd_ctx->nr_zones ++;
		current_zone_idx += num_cores;
	} while (current_zone_idx < max_zones);

	return list_thrd_ctx;
}

int parse_delimiter_entries_pos(std::string &key, const std::string delimiter,
				std::vector<std::string> &prefixes, std::vector<std::size_t> &positions,
				std::vector<std::string> &entries)
{
	size_t cur_pos = 0;
	size_t pos = 0;
	int cnt = 0;
	//std::string prefix;
	//std::string entry;

	do {
		pos = key.find_first_of(delimiter, cur_pos);
		if (cur_pos != 0) {

			if (!cnt) {
				++cnt;
				prefixes.emplace_back(std::move(NKV_ROOT_PREFIX));
				entries.emplace_back(std::move(key.substr(0, cur_pos)));
				positions.emplace_back(0);

				//prefix = NKV_ROOT_PREFIX;
				//entry = key.substr(0, cur_pos);

				//list_log("parse_delimiter_entries_pos %*s:%d:%*s:%d pos:%d cont:%d\n",
				//       prefix.size(), prefix.c_str(), prefix.size(),
				//     entry.size(), entry.c_str(), entry.size(),
				//   0, cnt);

			}
			++cnt;
			positions.emplace_back(cur_pos);
			prefixes.emplace_back(std::move(key.substr(0, cur_pos)));
            
			//prefix = key.substr(0, cur_pos);
			if (pos != std::string::npos) {
				entries.emplace_back(std::move(key.substr(cur_pos, pos - cur_pos)));
				//entry = key.substr(cur_pos, pos - cur_pos);
			} else {
				entries.emplace_back(std::move(key.substr(cur_pos, key.size() - cur_pos)));
				//entry = key.substr(cur_pos, key.size() - cur_pos);
			}
            list_log("parse_delimiter_entries_pos pos: %d, prefixes: %s, entry: %s\n", 
                cur_pos, prefixes.back().c_str(), entries.back().c_str());
			//list_log("parse_delimiter_entries_pos %*s:%d:%*s:%d pos:%d cont:%d\n",
			//       prefix.size(), prefix.c_str(), prefix.size(),
			//     entry.size(), entry.c_str(), entry.size(),
			//   cur_pos, cnt);

		}
		cur_pos = pos + 1;

	} while (pos != std::string::npos); 
	if (!cnt) {
		DFLY_ASSERT(key.length() != 0);
		prefixes.emplace_back(NKV_ROOT_PREFIX);
		entries.emplace_back(key);
		positions.emplace_back(0);
		++cnt;
	}
	return cnt; 
}

//return the m_inst that manage the mapped zone of the last prefix/entry pair.
void *list_get_module_ctx_on_change(struct dfly_request *req)
{
	struct dfly_key *key = req->ops.get_key(req);
	int i = 0;
	int nr_entries = 0;
	uint8_t zone_idx = 0;
	void *m_inst = NULL;
	list_context_t *list_ctx = (list_context_t *)dfly_module_get_ctx(
					   req->req_dfly_ss->mlist.dfly_list_module);

	if (!req->list_data.pe_cnt_tbd) {

		std::string key_str((const char *)(key->key), key->length);
		std::vector<std::string> prefixes;
		std::vector<std::string> entries;
		std::vector<size_t> positions;

		nr_entries = parse_delimiter_entries_pos(key_str, key_default_delimlist,
				prefixes, positions, entries);
		assert(nr_entries < (SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1) / 2);
		req->list_data.pe_total_cnt = nr_entries;
		req->list_data.pe_cnt_tbd = nr_entries;

		DFLY_ASSERT(req->req_dfly_ss->id != 0);
		for (i = 0; i < nr_entries; i++) {
			uint32_t h = hash_sdbm(prefixes[i].c_str(), prefixes[i].size());
			zone_idx = h % g_list_conf.list_zone_per_pool;
			list_log("list_get_module_ctx_on_change: zone %d i %d position %d\n", zone_idx, i, positions[i]);
			req->list_data.prefix_key_info[i * 2] = zone_idx;
			req->list_data.prefix_key_info[i * 2 + 1] = (int16_t)positions[i];
			m_inst = list_ctx->zones[zone_idx].module_instance_ctx;
		}
		req->list_data.prefix_key_info[i * 2] = req->list_data.prefix_key_info[i * 2 + 1] = -1;
	} else {
		nr_entries = req->list_data.pe_total_cnt;
		for (i = nr_entries - 1; i >= 0; i--) {
			if ((zone_idx = req->list_data.prefix_key_info[i * 2]) > 0) {
				m_inst = list_ctx->zones[zone_idx].module_instance_ctx;
				break;
			}
		}
	}

	if (m_inst) {
		req->state = DFLY_REQ_IO_LIST_FORWARD;
	} else {
		if(dss_subsystem_kv_mode_enabled((dss_subsystem_t *) req->req_dfly_ss)) {
			dss_module_post_to_instance(DSS_MODULE_NET, req->common_req.module_ctx[DSS_MODULE_NET].module_instance, req);
			return NULL;
		}
		req->next_action = DFLY_REQ_IO_LIST_DONE;
		req->state = DFLY_REQ_IO_NVMF_DONE;
		dfly_handle_request(req);
	}

	//list_log("list_get_module_ctx: zone_idx %d m_inst %p\n", zone_idx, m_inst);
	return m_inst;

}

void *list_get_module_ctx_on_read(struct dfly_request *req)
{
	//io_thread_ctx = (struct io_thread_ctx_s *)dfly_module_get_ctx(req_ss->mlist.dfly_io_module)

	struct dfly_key *key = req->ops.get_key(req);
	//assert(key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE);
	list_context_t *list_ctx = (list_context_t *)dfly_module_get_ctx(
					   req->req_dfly_ss->mlist.dfly_list_module);

	uint8_t zone_idx = 0;
	void *m_inst = NULL;
	std::string prefix = NKV_ROOT_PREFIX;
	int offset = req->list_data.start_key_offset;
	int list_option = req->list_data.options;
    int payload_sz = key->length;
 
	if (!(list_option == DFLY_LIST_OPTION_ROOT_FROM_BEGIN
	      || list_option == DFLY_LIST_OPTION_ROOT_FROM_START_KEY)) {
		if (list_option == DFLY_LIST_OPTION_PREFIX_FROM_BEGIN)
			prefix = std::string((const char *)(key->key));
		else
			prefix = std::string((const char *)(key->key), offset);
	}

	/*
    if (offset && offset < payload_sz) {
		req->list_data.options = DFLY_LIST_OPTION_PREFIX_FROM_START_KEY;
		prefix = std::string((const char *)(key->key), offset);
		//list_log("option: prefix '%s' start_key '%s'\n", prefix.c_str(), start_key.c_str());
	} else if (offset && offset == payload_sz) {
		req->list_data.options = DFLY_LIST_OPTION_PREFIX_FROM_BEGIN;
		prefix = std::string((const char *)(key->key), payload_sz);
		//list_log("option: prefix '%s' from begin\n", prefix.c_str());
	} else if (!offset && payload_sz) {
		req->list_data.options = DFLY_LIST_OPTION_ROOT_FROM_START_KEY;
		//list_log("option: 'root/' start_key '%s'\n", start_key.c_str());
	} else if (list_option == DFLY_LIST_OPTION_ROOT_FROM_BEGIN && !offset && !payload_sz) {
		req->list_data.options = DFLY_LIST_OPTION_ROOT_FROM_BEGIN;
		//list_log("option: 'root/' from begin\n");
	} else {
		DFLY_ERRLOG("list option %d does not match : offset %d, payload_sz %d, prefix %s\n",
            list_option, offset, payload_sz, prefix.c_str());
	}
	*/

	uint32_t h = hash_sdbm(prefix.c_str(), prefix.size());
	DFLY_ASSERT(req->req_dfly_ss->id != 0);
	zone_idx = h % g_list_conf.list_zone_per_pool;
	//list_log("list_get_module_ctx_on_read: prefix %s sz %d\n zone_idx %d\n", prefix.c_str(),
	//	 prefix.size(), zone_idx);

	m_inst = list_ctx->zones[zone_idx].module_instance_ctx;
	req->list_data.list_zone_idx = zone_idx;
	req->state = DFLY_REQ_IO_LIST_FORWARD;
	list_log("list_get_module_ctx_on_read: list_option %d zone_idx %d m_inst %p key %s offset %d payload_sz %d prefix %s\n", 
		 req->list_data.options, zone_idx, m_inst, key->key, offset, payload_sz, 
		 prefix.c_str());
	return m_inst;

}

void *list_find_instance_ctx(struct dfly_request *req)
{
	if (req->ops.get_command(req) == SPDK_NVME_OPC_SAMSUNG_KV_LIST_READ) {
		return list_get_module_ctx_on_read(req);
	} else {
		return list_get_module_ctx_on_change(req);
	}
}

struct dfly_module_ops list_module_ops = {
	.module_init_instance_context = list_module_store_inst_context,
	.module_rpoll = dfly_list_req_process,
	.module_cpoll = NULL,//dfly_list_req_complete,
	.module_gpoll = NULL, //dfly_list_generic_poll,
	.find_instance_context = list_find_instance_ctx,
};

extern dfly_io_module_handler_t g_list_io_handlers;

int list_init_load_by_iter(struct df_ss_cb_event_s *e);
int list_init_load_by_blk_iter(struct dfly_subsystem *pool);

//Assumes subsystems are initialized one by one
struct list_started_cb_event_s {
	df_module_event_complete_cb df_ss_cb;
	void *df_ss_cb_arg;
	uint32_t src_core;
	uint64_t start_tick;
} list_cb_event;

void list_module_started_cb(struct df_ss_cb_event_s *e);
void list_module_load_done_cb(struct df_ss_cb_event_s *e);

void list_module_started_cb(struct df_ss_cb_event_s *e)
{
    struct dfly_subsystem *pool = e->ss;
    if(g_dragonfly->blk_map){
        pool->list_init_event = e;
        list_init_load_by_blk_iter(pool);//Rocksdb loading
    } else if (dss_subsystem_kv_mode_enabled((dss_subsystem_t *)pool)) {
        //TODO: load keys from KVTrans
		DSS_WARNLOG("In-memory listing not loading keys from disks for subsystem %s\n", pool->name);
        list_module_load_done_cb(e);//Complete without loading keys
    } else {
        list_init_load_by_iter(e);//KVdrive loading
    }
    return;
}

void list_module_load_done_cb(struct df_ss_cb_event_s *e)
{
    struct dfly_subsystem *pool = e->ss;

	uint64_t start_tick, load_time;

	dss_hsl_ctx_t *hsl_ctx =  dss_get_hsl_context(pool);
    start_tick = (uint64_t)e->df_ss_private;


#ifndef DSS_OPEN_SOURCE_RELEASE
	if(g_dragonfly->dss_enable_judy_listing &&
			g_dragonfly->rdb_direct_listing) {//Evict if direct listing
		//dss_hsl_print_info(hsl_ctx);
		//Using cache threshold with LRU eviction
		//dss_hsl_evict_levels(hsl_ctx, g_dragonfly->rdb_direct_listing_evict_levels, &hsl_ctx->lnode, 0);
		//dss_hsl_print_info(hsl_ctx);
	}
#endif

	load_time = ((spdk_get_ticks() - start_tick) * SPDK_SEC_TO_USEC )/ spdk_get_ticks_hz();


	DFLY_DEBUGLOG(DFLY_LOG_LIST, "List load for pool %s, completed in %d micro seconds\n", pool->name, load_time);

    df_ss_cb_event_complete(e);
}

void list_module_load_done_blk_cb(struct df_ss_cb_event_s *e)
{
    struct dfly_subsystem *pool = e->ss;
    int rc = e->status;

    if(rc == LIST_INIT_DONE){
        if(++pool->list_initialized_nbdev == pool->num_io_devices){
            pool->list_init_status = LIST_INIT_DONE;
            list_module_load_done_cb(e);
        }
    }
}

//int dfly_list_module_init(int ssid, int nr_cores, void *cb, void *cb_arg)
int dfly_list_module_init(struct dfly_subsystem *pool, void *dummy, void *cb, void *cb_arg)
{
	list_context_t  *list_mctx;
	int nr_zones;
	int i;
	dss_module_config_t c;
    struct df_ss_cb_event_s *list_mod_event;

	dss_module_set_default_config(&c);
	c.id = pool->id;
	c.num_cores = g_list_conf.list_nr_cores;

	nr_zones = g_list_conf.list_zone_per_pool;

	list_debug_level = g_list_conf.list_debug_level;

	list_mctx = (list_context_t  *)calloc(1, sizeof(list_context_t) + (nr_zones * sizeof(list_zone_t)));
	if (!list_mctx) {
		DFLY_ERRLOG("Failed to allocate module ctx memory\n");
		return -1;
	}

	for (i = 0; i < nr_zones; i++) {
		list_zone_t *zone = NULL;
		zone = &list_mctx->zones[i];
		assert(zone);
		zone->zone_idx = i;
#ifdef DSS_OPEN_SOURCE_RELEASE
		zone->listing_keys = new std::unordered_map<std::string, std::set<std::string>>();
		(*zone->listing_keys).reserve(1048576);
#else
		if(g_dragonfly->dss_enable_judy_listing) {
			DFLY_ASSERT(zone->zone_idx == 0);
			zone->hsl_keys_ctx = dss_hsl_new_ctx(g_list_conf.list_prefix_head, (char *)key_default_delimlist.c_str(), do_list_item_process);
			zone->hsl_keys_ctx->dev_ctx = pool;
			if(g_dragonfly->rdb_direct_listing) {
#ifdef DSS_ENABLE_ROCKSDB_KV
				if(g_dragonfly->rdb_direct_listing_enable_tpool) {
					zone->hsl_keys_ctx->dlist_mod = dss_tpool_start("list_tpool", pool->id, zone->hsl_keys_ctx,
						g_dragonfly->rdb_direct_listing_nthreads, dss_rocksdb_direct_iter);
				}
#else
DFLY_ASSERT(0);
#endif//#ifdef DSS_ENABLE_ROCKSDB_KV
			}
		} else {
			zone->listing_keys = new std::unordered_map<std::string, std::set<std::string>>();
			(*zone->listing_keys).reserve(1048576);
		}
#endif
	}

	pthread_mutex_init(&list_mctx->io_ctx.ctx_lock, NULL);
	list_mctx->io_ctx.conf = &g_list_conf;
	list_mctx->io_ctx.ctx = list_mctx;
	list_mctx->io_ctx.io_handlers = &g_list_io_handlers;
	list_mctx->nr_zones = nr_zones;

	//Initialize subsystem call backs
    list_mod_event = df_ss_cb_event_allocate(pool ,cb, cb_arg, (void *)spdk_get_ticks());

	pool->mlist.dfly_list_module = dfly_module_start("list", DSS_MODULE_LIST, &c, &list_module_ops,
				       list_mctx, (df_module_event_complete_cb)list_module_started_cb, list_mod_event);

	DFLY_DEBUGLOG(DFLY_LOG_LIST, "dfly_list_module_init ss %p ssid %d\n", pool, pool->id);
	assert(pool->mlist.dfly_list_module);
	return 0;
}

void dfly_list_module_destroy_compelete(void *event, void *list_arg)
{
	struct df_ss_cb_event_s *list_cb_event = (struct df_ss_cb_event_s *)event;

	struct dfly_subsystem *ss = list_cb_event->ss;

	list_context_t  *list_mctx;

	DFLY_ASSERT(ss);
	list_mctx = (list_context_t  *)ss->mlist.dfly_list_module->ctx;

	free(list_mctx);

	df_ss_cb_event_complete(list_cb_event);
	return;
}

void dfly_list_module_destroy(struct dfly_subsystem *pool, void *args, void *cb, void *cb_arg)
{

	struct df_ss_cb_event_s *list_cb_event = df_ss_cb_event_allocate(pool, (df_module_event_complete_cb)cb, cb_arg, args);

	dfly_module_stop(pool->mlist.dfly_list_module, dfly_list_module_destroy_compelete, list_cb_event);

	return;
}
