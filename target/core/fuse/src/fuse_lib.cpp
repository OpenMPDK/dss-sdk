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

#include <df_io_module.h>
#include <fuse_lib.h>
#include <fuse_module.h>
#include <map>

static char __fuse_nqn_name[256] = {0};

int fuse_debug_level = 1;

int fuse_get_pool_id(struct dfly_subsystem *pool);
int do_fuse_F1_op(fuse_map_item_t *item, int flags);
int do_fuse_F2_op(fuse_map_item_t *item, int flags);

int fuse_get_pool_id(struct dfly_subsystem *pool)
{
	return pool->id;
}

#define WAL_ENABLE_DEFAULT      0
#define FUSE_ENABLE_DEFAULT	    1
#define FUSE_NR_CORES_DEFAULT	1
#define FUSE_NR_MAPS_DEFAULT    10
#define FUSE_DEBUG_LEVEL_DEFAULT 0
#define FUSE_TIMEOUT_DEFAULT_MS 2000
#define FUSE_NQN_NAME		"nqn.2018-01.dragonfly:test1"

fuse_conf_t g_fuse_conf = {
	FUSE_ENABLE_DEFAULT,
	WAL_ENABLE_DEFAULT,
	FUSE_NR_MAPS_DEFAULT,
	FUSE_NR_CORES_DEFAULT,
	DF_OP_LOCKLESS,
	FUSE_DEBUG_LEVEL_DEFAULT,
	FUSE_TIMEOUT_DEFAULT_MS,
	FUSE_NQN_NAME
};

///////////////////////// fuse module /////////////////////
dfly_io_module_handler_t fuse_io_handlers = {
	(dfly_io_mod_store)fuse_handle_store_op,
	NULL,   //  retrieve_handler
	(dfly_io_mod_delete) fuse_handle_delete_op,
	NULL,   //  iter_ctrl_handler
	NULL,   //  iter_read_handler
	(dfly_io_mod_fuse_f1) fuse_handle_F1_op,
	(dfly_io_mod_fuse_f2) fuse_handle_F2_op,
};

static fuse_context_t __fuse_ctx = {0, NULL, 0, NULL, 0};
dfly_io_module_context_t fuse_module_ctx =
{PTHREAD_MUTEX_INITIALIZER, &g_fuse_conf, &__fuse_ctx, &fuse_io_handlers, {0}};
///////////////////////// fuse module end ////////////////

fuse_map_ops_t fuse_map_ops = {
	(hash_func_t) hash_sdbm,
	(lookup_func_t) fuse_map_lookup,
	(hash_key_comp_t) fuse_key_compare,
	NULL,
	NULL,
	(cleanup_func_t) fuse_map_cleanup
};

int fuse_key_compare(fuse_key_t *k1, fuse_key_t *k2)
{
	if (k1->length != k2->length)
		return -1;

	return memcmp(k1->key, k2->key, k1->length);
}

int fuse_object_compare(fuse_object_t *obj1, fuse_object_t *obj2)
{
	if (obj1->key->length != obj2->key->length || obj1->val->length != obj2->val->length)
		return -1;

	if (memcmp(obj1->key->key, obj2->key->key, obj1->key->length))
		return -1;

	if (memcmp((void *)((char *)obj1->val->value + obj1->val->offset),
		   (void *)((char *)obj2->val->value + obj2->val->offset), obj1->val->length))
		return -1;

	return 0;
}

fuse_map_t *fuse_get_object_map(int pool_id, fuse_object_t *obj)
{
	if (!obj->key_hashed) {
		obj->key_hash = hash_sdbm((const char*)obj->key->key, obj->key->length);
		obj->key_hashed = 1;
	}
	dfly_io_module_pool_t *fuse_pool = __fuse_ctx.pools[pool_id];
	int map_idx = (obj->key_hash % fuse_pool->nr_zones) + fuse_pool->zone_idx;
	return __fuse_ctx.maps[map_idx];
}

void *fuse_get_map_module_ctx(struct dfly_request *req)
{
	fuse_object_t obj;
	obj.key = req->ops.get_key(req);
	obj.key_hashed = 0;

	fuse_map_t *map = fuse_get_object_map(req->req_dfly_ss->id, &obj);
	//fuse_log("fuse_get_map_module_ctx map %p, m_inst %p\n", map, map->module_instance_ctx);

	return map->module_instance_ctx;
}

void fuse_context_deinit(struct dfly_subsystem *pool)
{
	int i = 0;
	fuse_map_t *map = NULL;
	int pool_id = pool->id;
	dfly_io_module_pool_t *fuse_pool = NULL;
	if (!g_fuse_conf.fuse_enabled || !__fuse_ctx.pools)
		return ;

	fuse_pool = __fuse_ctx.pools[pool->id];
	if (!fuse_pool)
		return;

	int nr_maps = fuse_pool->nr_zones;
	int map_idx = fuse_pool->zone_idx;

	df_lock(&fuse_module_ctx.ctx_lock);

	for (i = 0; i < nr_maps; i++) {
		map = __fuse_ctx.maps[i + map_idx];
		dfly_device_close(map->tgt_fh);
		map->ops->clean_up((void **)&map);
	}

	df_free(fuse_pool);
	__fuse_ctx.pools[pool->id] = NULL;
	__fuse_ctx.nr_maps -= nr_maps;

	if (!__fuse_ctx.nr_maps) {
		df_free(__fuse_ctx.maps);
		__fuse_ctx.maps = NULL;
		df_free(__fuse_ctx.pools);
		__fuse_ctx.pools = NULL;
	}

	df_unlock(&fuse_module_ctx.ctx_lock);
}

#define MAX_FUSE_MAP    256
#define FUSE_MAX_BUCKET 1048576

int fuse_context_init(fuse_context_t *fuse_ctx,
		      struct dfly_subsystem *pool, int nr_maps, int fuse_flags)
{
	int i = 0;
	int rc = FUSE_SUCCESS;
	fuse_map_t *map = NULL;
	int map_idx = fuse_ctx->nr_maps;
	int pool_id = pool->id;


	if (nr_maps < 0 || nr_maps > MAX_FUSE_MAP || fuse_ctx->nr_maps + nr_maps > MAX_FUSE_MAP) {
		fuse_log("fuse_context_init failed: nr_maps %d requested nr_maps %d existed max %d\n",
			 nr_maps, fuse_ctx->nr_maps, MAX_FUSE_MAP);
		rc = -FUSE_ERROR_PARAM_INVALID;
		goto fail;
	}

	if (!fuse_ctx->maps)
		fuse_ctx->maps = (fuse_map_t **)df_calloc(MAX_FUSE_MAP, sizeof(fuse_map_t *));

	if (!fuse_ctx->pools)
		fuse_ctx->pools = (dfly_io_module_pool_t **)df_calloc(DFLY_MAX_NR_POOL,
				  sizeof(dfly_io_module_pool_t *));

	if (!fuse_ctx->pools[pool_id]) {
		fuse_ctx->pools[pool_id] = (dfly_io_module_pool_t *)df_calloc(1, sizeof(dfly_io_module_pool_t));
	} else {
		fuse_log("fuse_context_init failed: pool %p with id %d existed", pool, pool_id);
		rc = -FUSE_ERROR_PARAM_INVALID;
		goto fail;
	}

	for (i = 0; i < nr_maps; i++) {
		map = fuse_map_create(FUSE_MAX_BUCKET, &fuse_map_ops);
		map->map_idx = i;
		df_lock_init(&map->map_lock, NULL);
		fuse_ctx->maps[i + map_idx] = map;
		map->tgt_fh = dfly_device_open(__fuse_nqn_name, DFLY_DEVICE_TYPE_KV_POOL, true);
	}

	fuse_ctx->pools[pool_id]->dfly_pool = pool;
	fuse_ctx->pools[pool_id]->dfly_pool_id = pool_id;
	fuse_ctx->pools[pool_id]->zone_idx = map_idx;
	fuse_ctx->pools[pool_id]->nr_zones = nr_maps;
	fuse_ctx->nr_maps += nr_maps;

	fuse_log("fuse context : pool %d with nr_maps %d\n", pool_id, nr_maps);

fail:
	return rc;
}

int fuse_compare_value(struct dfly_value *v1, struct dfly_value *v2)
{
	assert(v1 && v2);
	assert(v1->value && v2->value);
	if (v1->length - v1->offset != v2->length - v2->offset)
		return -1;

	return (memcmp((void *)((char *)v1->value + v1->offset), (void *)((char *)v2->value + v2->offset), v1->length));
}

struct dfly_request *find_get_linked_fuse(fuse_map_item_t *item)
{
	struct dfly_request *req = NULL, * find_req = NULL;
	assert(item);
	TAILQ_FOREACH(req, &item->fuse_waiting_head, fuse_waiting) {
		if (req->next_req) {
			find_req = req;
			break;
		}
	}

	//if(find_req)
	//    TAILQ_REMOVE_INIT( &item->fuse_waiting_head, find_req, fuse_waiting);

	return find_req;

}

//return 0 if match.
int fuse_compare_on_complete(fuse_map_item_t *item)
{
	struct dfly_value *read_val = item->f1_read_val;
	struct dfly_request *f1 = item->fuse_req;
	struct dfly_value *f1_val = f1->ops.get_value(f1);

	return fuse_compare_value(read_val, f1_val);
}

static void *fuse_get_value_buff(struct dfly_value *val)
{
	void *buff = dfly_io_get_buff(NULL, val->length);

	if (!buff) {
		uint32_t dma_buff_pages = (val->length + PAGE_SIZE - 1) >> 12 ;
		buff = spdk_dma_malloc(dma_buff_pages << 12, PAGE_SIZE, NULL);
	}

	return buff;
}

static void fuse_release_value_buff(struct dfly_value *val)
{
	void *buff = dfly_io_get_buff(NULL, val->length);

	if (val->length < DFLY_BDEV_BUF_MAX_SIZE) {
		dfly_io_put_buff(NULL, val->value);
	} else {
		spdk_dma_free(val->value);
	}
}

static void f1_buffer_release(fuse_map_item_t *item)
{
	assert(item && item->f1_read_val);

	//done the F1 read
	fuse_release_value_buff(item->f1_read_val);
	//dfly_io_put_buff(NULL, item->f1_read_val->value);
	df_free(item->f1_read_val);
	item->f1_read_val = NULL;
}

static void _complete_pending_fuse_op(fuse_map_item_t *item, struct dfly_request *req)
{
	//fuse_log("_complete_pending_fuse_op item %p %p\n", item, req);

	int nr_io_pending = ATOMIC_DEC_FETCH(__fuse_ctx.nr_io_pending);

	//printf("offsetof %d \n", offsetof(struct dfly_request, pending_list));

	//printf("fuse complete req %p opc 0x%x list %p cnt first %p, last %p, nr_io_pending %d\n",
	//    req, req->ops.get_command(req), &req->pending_list,
	//    item->map->io_pending_queue.tqh_first, item->map->io_pending_queue.tqh_last, nr_io_pending);
	if (fuse_debug_level) {
		TAILQ_REMOVE_INIT(&item->map->io_pending_queue, req, fuse_pending_list);
		req->req_fuse_data = NULL;
		req->f1_req = NULL;
		req->next_req = NULL;

		struct dfly_request *rr = NULL;
		std::map<struct dfly_request *, int> req_map;
		int i = 0;
		//printf("fuse map[%d]: req ",item->map->map_idx);
		TAILQ_FOREACH(rr, &item->map->io_pending_queue, fuse_pending_list) {
			//printf("%p opc 0x%x ", rr, rr->ops.get_command(rr));
			if (req_map.find(rr) != req_map.end()) {
				assert(0);
			} else {
				req_map[rr] = i++;
			}

		}
		//printf("\n");
	}
	req->state = DFLY_REQ_IO_NVMF_DONE;
	dfly_handle_request(req);
}

void dfly_kv_pool_fuse_complete(struct df_dev_response_s resp, void *arg)
{
	fuse_map_item_t *item = (fuse_map_item_t *) arg;
	if (item->key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		assert(item->large_key_buff);
		dfly_put_key_buff(NULL, item->large_key_buff); //spdk_dma_free(item->large_key_buff);
		item->large_key_buff = NULL;
	}

	assert(item->fuse_req);
	struct dfly_request *req = item->fuse_req;

	req->req_fuse_data = item;
	req->status = resp.rc;
	fuse_log("dfly_kv_pool_fuse_complete item %p req %p, opc %d state %d, next_action %d sucess=%d\n",
		 item, req, req->ops.get_command(req), req->state, req->next_action, resp.rc);
	struct dfly_subsystem *ss = dfly_get_subsystem(req->req_ssid);
	//dfly_module_complete_request(ss->mlist.dfly_fuse_module, req);
	dfly_fuse_complete(item->fuse_req, resp.rc);
}

void dfly_fuse_wal_complete(struct df_dev_response_s resp, void *arg)
{
	fuse_map_item_t *item = (fuse_map_item_t *) arg;
	assert(item->fuse_req);
	struct dfly_request *req = item->fuse_req;
	if (req->req_key.length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		assert(item->large_key_buff);
		dfly_put_key_buff(NULL, item->large_key_buff);
		item->large_key_buff = NULL;
	}

	fuse_log("dfly_fuse_wal_complete interal req %p on item %p success= %d\n",
		 req, item, resp.rc);

}

static int fuse_prepare_large_key(struct dfly_key *large_key, fuse_map_item_t *item)
{
	assert(!item->large_key_buff && item->key->length <= SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1);
	item->large_key_buff = dfly_get_key_buff(NULL,
			       SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1); //spdk_dma_malloc(SAMSUNG_KV_MAX_FABRIC_KEY_SIZE, 256, NULL);
	if (!item->large_key_buff)
		return -1 ;

	large_key->key = item->large_key_buff;
	large_key->length = item->key->length;
	memcpy(large_key->key, item->key->key, item->key->length);
	return 0;
}

void dfly_fuse_release(struct dfly_request *request)
{
	fuse_log("dfly_fuse_release %p with item %p\n", request, request->req_fuse_data);

	fuse_map_item_t *item = (fuse_map_item_t *)request->req_fuse_data;
	if (!item)
		return ;

	request->req_fuse_data = NULL;
	request->f1_req = NULL;
	request->next_req = NULL;
	request->opc = 0;

	if (ATOMIC_READ(item->ref_cnt)) {
		ATOMIC_DEC_FETCH(item->ref_cnt);
	}

	if (!TAILQ_UNLINKED(&request->fuse_pending_list)) {
		printf("dfly_fuse_release fuse_pending_list %p\n", request);
		TAILQ_REMOVE_INIT(&item->map->io_pending_queue, request, fuse_pending_list);
	} else {
		if (fuse_debug_level)
			printf("%p no fuse_pending_list\n", request);
	}

	if (!TAILQ_UNLINKED(&request->fuse_waiting)) {
		printf("dfly_fuse_release fuse_waiting %p\n", request);
		TAILQ_REMOVE_INIT(&item->fuse_waiting_head, request, fuse_waiting);
	}

	if (!TAILQ_UNLINKED(&request->fuse_delay)) {
		printf("dfly_fuse_release fuse_delay %p\n", request);
		TAILQ_REMOVE_INIT(&item->map->wp_delay_queue, request, fuse_delay);
	}

	if (!TAILQ_UNLINKED(&request->fuse_pending)) {
		printf("dfly_fuse_release fuse_pending %p\n", request);
		TAILQ_REMOVE_INIT(&(item->map->wp_pending_queue), request, fuse_pending);
	}

}

void dfly_fuse_complete(void *arg, bool success = true)
{
	struct dfly_request *request = (struct dfly_request *)arg;
	fuse_map_item_t *item = (fuse_map_item_t *) request->req_fuse_data;
	struct dfly_request *f1 = NULL, * f2 = NULL;
	struct dfly_request *req = NULL, * new_fues_req = NULL;
	assert(item);

	int rc = FUSE_SUCCESS;
	int io_rc = 0;
	int r_cnt = 0;
	//df_lock(&item->map->map_lock);

	assert(item->fuse_req || item->ref_cnt);

	if (ATOMIC_READ(item->ref_cnt)) {
		assert(item->fuse_req == NULL);
		r_cnt = ATOMIC_DEC_FETCH(item->ref_cnt);
		int opc = request->ops.get_command(request);
		assert(opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE ||
		       opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE);

		request->req_fuse_data = NULL;
		request->f1_req = NULL;
		request->next_req = NULL;

		int nr_io_pending = ATOMIC_DEC_FETCH(__fuse_ctx.nr_io_pending);
		if (fuse_debug_level) {
			TAILQ_REMOVE_INIT(&item->map->io_pending_queue, request, fuse_pending_list);
		}
		//printf("complete req %p item %p r_cnt %d\n", request, item, r_cnt);
		/*
		struct dfly_request *rr = NULL;
		printf("map[%d]: req ",item->map->map_idx);
		TAILQ_FOREACH(rr, &item->map->io_pending_queue, pending_list){
		    printf("%p ", rr);
		}
		printf("\n");
		*/
	}

	if (r_cnt)
		goto comp_done;

	if (item->fuse_req) {
		assert(item->ref_cnt == 0);
		//complete for a fuse in progress.
		int opc = item->fuse_req->ops.get_command(item->fuse_req);
		if (opc == SPDK_NVME_OPC_SAMSUNG_KV_CMP)
			f1 = item->fuse_req;
		else
			f2 = item->fuse_req;
	}

	if (!success) {
		if (f1) {
			//complete uio request f1, f2
			f1_buffer_release(item);

			f2 = f1->next_req;
			fuse_log("complete f1 %p f2 %p on success=%d\n", f1, f2, success);

			f1->next_action = DFLY_COMPLETE_NVMF;
			dfly_set_status_code(f1, SPDK_NVME_SCT_KV_CMD,
					     SPDK_NVME_SC_KV_FUSE_CMP_FAILURE);
			_complete_pending_fuse_op(item, f1);

			f2->next_action = DFLY_COMPLETE_NVMF;
			dfly_set_status_code(f2, SPDK_NVME_SCT_KV_CMD,
					     SPDK_NVME_SC_KV_FUSE_CMD2_ABORT);
			_complete_pending_fuse_op(item, f2);
		} else if (f2) {
			fuse_log("complete f2 %p success %d\n", f2, success);
			f2->next_action = DFLY_COMPLETE_NVMF;
			_complete_pending_fuse_op(item, f2);
		}
	} else {
		if (f1) {
			//complete for fuse F1
			fuse_log("complete f1 %p\n", f1);
			f2 = f1->next_req;
			//f1->next_action = DFLY_COMPLETE_NVMF;
			//_complete_pending_fuse_op(item, f1);

			if (!fuse_compare_on_complete(item)) {

				f1->next_action = DFLY_COMPLETE_NVMF;
				_complete_pending_fuse_op(item, f1);

				f1_buffer_release(item);
				//do F2
				fuse_log("f1 match, do f2 %p \n", f2);

				item->fuse_req = f2;
				io_rc = do_fuse_F2_op(item, 0);
				if (io_rc == FUSE_IO_RC_FUSE_IN_PROGRESS) {
					goto comp_done;
				} else if (io_rc == FUSE_IO_RC_F2_COMPLETE) {
				} else if (io_rc == FUSE_IO_RC_RETRY) {
					assert(0);
				} else {
					assert(0);
				}
			} else {
				f1->next_action = DFLY_COMPLETE_NVMF;
				dfly_set_status_code(f1, SPDK_NVME_SCT_KV_CMD,
						     SPDK_NVME_SC_KV_FUSE_CMP_FAILURE);
				_complete_pending_fuse_op(item, f1);

				f1_buffer_release(item);
				//F1 mismatch, abort the F2 and complete
				fuse_log("f1 mismatch, abort f2 %p\n", f2);
				item->fuse_req = NULL;
				f2->next_action = DFLY_COMPLETE_NVMF;
				dfly_set_status_code(f2, SPDK_NVME_SCT_KV_CMD,
						     SPDK_NVME_SC_KV_FUSE_CMD2_ABORT);
				_complete_pending_fuse_op(item, f2);

			}

		} else if (f2) {
			//complete for fuse F2
			fuse_log("complete f2 %p\n", f2);
			item->fuse_req = NULL;
			f2->next_action = DFLY_COMPLETE_NVMF;
			_complete_pending_fuse_op(item, f2);
		}
	}

	//if next linked fuse for this object
	item->fuse_req = NULL;
	while (new_fues_req = find_get_linked_fuse(item)) {
		fuse_log("find next fuse %p on map[%d] bucket %p\n", new_fues_req,
			 item->map->map_idx, item->bucket);
		item->fuse_req = new_fues_req;
		TAILQ_REMOVE_INIT(&item->fuse_waiting_head, new_fues_req, fuse_waiting);
		io_rc = do_fuse_handle_fuse_op(new_fues_req, item, 0);
		/*
		f1 return:  FUSE_IO_RC_F1_FAIL (f1, f2 completed)
		            FUSE_IO_RC_FUSE_IN_PROGRESS

		f2 return:  FUSE_IO_RC_F2_COMPLETE (f1, f2 completed)
		            FUSE_IO_RC_FUSE_IN_PROGRESS
		*/

		if (io_rc == FUSE_IO_RC_FUSE_IN_PROGRESS)
			break;
	}

comp_done:
	fuse_log("complete done with key %llx%llx r_cnt %d io_rc %d\n",
		 *(long long *)item->key->key, *(long long *)((char *)item->key->key + 8), r_cnt, io_rc);
	//df_unlock(&item->map->map_lock);

	return;
}

// * return FUSE_SUCCESS: insert new item if not existed. status = FUSE_WRITE_PENDING
// * return FUSE_SUCCESS: increment the ref_cnt if existed with status as FUSE_WRITE_PENDING
// * return FUSE_ERROR_IO_RETRY if item existed with other status.
int fuse_handle_store_op(fuse_map_t *map, fuse_object_t *obj, int flags)
{
	int io_rc = FUSE_IO_RC_WRITE_SUCCESS;
	fuse_map_item_t *item = NULL;
	assert(map && obj);

	struct dfly_request *req = (struct dfly_request *)obj->obj_private;

	int rc = map->ops->lookup(map, obj, FUSE_MAP_LOOKUP_STORE, (void **)&item);
	assert(rc == FUSE_SUCCESS && item);

	if (item) {
		if (!item->fuse_req) {
			ATOMIC_INC(item->ref_cnt);
			// completion staff.
			req->req_fuse_data = item;
			item->store_del_req = req;
		} else {
			io_rc = FUSE_IO_RC_RETRY;
		}
	}

	fuse_log("fuse_handle_store_op req %p key %llx%llx, io_rc %x\n",
		 req, *(long long *)obj->key->key, *(long long *)((char *)obj->key->key + 8), io_rc);

	return io_rc;
}

// * return FUSE_SUCCESS: new item, status = FUSE_DELETE_PENDING
// * return FUSE_ERROR_IO_RETRY if item existed with other status.
int fuse_handle_delete_op(fuse_map_t *map, fuse_object_t *obj, int flags)
{
	int io_rc = FUSE_IO_RC_DELETE_SUCCESS;
	int rc = FUSE_SUCCESS;
	fuse_map_item_t *item = NULL;
	assert(map && obj);
	struct dfly_request *req = (struct dfly_request *)obj->obj_private;

	rc = map->ops->lookup(map, obj, FUSE_MAP_LOOKUP_DELETE, (void **)&item);
	assert(rc == FUSE_SUCCESS && item);

	if (item) {
		if (!item->fuse_req) {
			ATOMIC_INC(item->ref_cnt);
			// completion staff.
			req->req_fuse_data = item;
			item->store_del_req = req;
		} else {
			io_rc = FUSE_IO_RC_RETRY;
		}
	}

	fuse_log("fuse_handle_delete_op req %p key %x%x, io_rc %x\n",
		 req, *(long long *)obj->key->key, *(long long *)((char *)obj->key->key + 8), io_rc);

	return io_rc;
}

static inline int dfly_is_f1_cmd(struct dfly_request *req)
{
	assert(req && req->ops.get_command);
	return (req->ops.get_command(req) == SPDK_NVME_OPC_SAMSUNG_KV_CMP);
}

static inline int dfly_is_f2_cmd(struct dfly_request *req)
{
	assert(req && req->ops.get_command);
	return (req->ops.get_command(req) == DFLY_REQ_OPC_DELETE_FUSE2 ||
		req->ops.get_command(req) == DFLY_REQ_OPC_STORE_FUSE2);
}

//check if the two fuse request belong to same compound.
//session id, cid checking.
//TBD
bool dfly_is_fuse_pair(struct dfly_request *f1, struct dfly_request *f2)
{
	return dfly_cmd_sequential(f1, f2);
}

// check if the two fuse request belong to the same compound and duplicated.
// TBD
bool dfly_is_fuse_duplicate(struct dfly_request *req1, struct dfly_request *req2)
{
	return false;
}

static int fuse_start_fuse(fuse_map_item_t *item,
			   struct dfly_request *f1, struct dfly_request *wait_req, int flags, int src)
{
	int io_rc = FUSE_IO_RC_FUSE_IN_PROGRESS;
	if (!item->fuse_req && !ATOMIC_READ(item->ref_cnt)) {

		item->fuse_req = f1;
		TAILQ_REMOVE_INIT(&item->fuse_waiting_head, wait_req, fuse_waiting);
		fuse_log("fuse_start_fuse_by_f%d find both f1 %p f2 %p item %p on key 0x%llx%llx\n",
			 src, f1, f1->next_req, item, *(long long *)item->key->key, *(long long *)((char *)item->key->key + 8));

		io_rc = do_fuse_handle_fuse_op(f1, item, flags);

		fuse_log("fuse_start_fuse_by_f%d f1 %p f2 %p item %p key 0x%llx%llx, io_rc %x\n",
			 src, f1, f1->next_req, item, *(long long *)item->key->key, *(long long *)((char *)item->key->key + 8),
			 io_rc);
	} else {
		fuse_log("fuse_start_fuse_by_f%d queueing both f1 %p f2 %p item %p with its fuse_req %p ref_cnt %d\n",
			 src, f1, f1->next_req, item, item->fuse_req, item->ref_cnt);
	}
	return io_rc ;
}

int fuse_handle_F1_op(fuse_map_t *map, fuse_object_t *obj, int flags)
{
	int io_rc = FUSE_IO_RC_WAIT_FOR_F2;
	int rc = FUSE_SUCCESS;
	fuse_map_item_t *item = NULL;
	assert(map && obj);

	struct dfly_request *f1 = (struct dfly_request *)obj->obj_private;
	struct dfly_request *req = NULL, * req_temp = NULL;
	struct dfly_request *found_f2 = NULL;

#ifdef FUSE_TIMEOUT
	if (g_fuse_conf.fuse_timeout_ms > 0) {
		start_timer(&f1->req_ts);
		TAILQ_INSERT_TAIL(&map->wp_pending_queue, f1, fuse_pending);
	}
#endif

	if (f1->next_req) {
		//retry fuse
		printf("fuse_handle_F1_op: retry req f1 %p f2 %p\n", f1, f1->next_req);

		assert(f1->req_fuse_data);
		item = (fuse_map_item_t *)f1->req_fuse_data;
		item->fuse_req = NULL;
		TAILQ_INSERT_TAIL(&item->fuse_waiting_head, f1, fuse_waiting);
		io_rc = fuse_start_fuse(item, f1, f1, flags, 1);
		printf("fuse_handle_F1_op: retry rc %d\n", io_rc);
		return io_rc;
	}

	rc = map->ops->lookup(map, obj, FUSE_MAP_LOOKUP_FUSE_1, (void **)&item);
	if (rc == FUSE_SUCCESS) {
		assert(item);
		f1->req_fuse_data = item;
		//search for misordered f2
		TAILQ_FOREACH_SAFE(req, &item->fuse_waiting_head, fuse_waiting, req_temp) {
			if (!req->req_ctx) {
				printf("found invalid req %p\n", req);
				TAILQ_REMOVE_INIT(&item->fuse_waiting_head, req, fuse_waiting);
				continue;
			}
			if (dfly_is_f2_cmd(req) && dfly_is_fuse_pair(f1, req)) {
				//find the f1 of the same session, link f2 as next req
				f1->next_req = req;
				found_f2 = req;
				//insert f1 and remove the found_f2 from waiting queue
				// after the f1->f2 linkage.
				TAILQ_INSERT_BEFORE(req, f1, fuse_waiting);
				TAILQ_REMOVE_INIT(&item->fuse_waiting_head, req, fuse_waiting);
				//req->req_fuse_data = NULL;
				break;
			}
			//duplicated f1 ?
			assert(!dfly_is_fuse_duplicate(req, f1));
		}

		if (!found_f2) {
			io_rc = FUSE_IO_RC_WAIT_FOR_F2;
			TAILQ_INSERT_TAIL(&item->fuse_waiting_head, f1, fuse_waiting);
			fuse_log("fuse_handle_F1_op f1 %p item %p key 0x%llx%llx, io_rc %x\n",
				 f1, item, *(long long *)item->key->key, *(long long *)((char *)item->key->key + 8), io_rc);
		} else {
			found_f2->f1_req = f1;
			io_rc = fuse_start_fuse(item, f1, f1, flags, 1);
		}
	} else {
		assert(0);
	}

	return io_rc;
}

int fuse_handle_F2_op(fuse_map_t *map, fuse_object_t *obj, int flags)
{
	int io_rc = FUSE_IO_RC_FUSE_IN_PROGRESS;
	fuse_map_item_t *item = NULL;
	assert(map && obj);

	struct dfly_request *f2 = (struct dfly_request *)obj->obj_private;
	struct dfly_request *req = NULL;
	struct dfly_request *found_f1 = NULL;

	//fuse_log("new req f2 %p\n", f2);

#ifdef FUSE_TIMEOUT
	if (g_fuse_conf.fuse_timeout_ms > 0) {
		start_timer(&f2->req_ts);
		TAILQ_INSERT_TAIL(&map->wp_pending_queue, f2, fuse_pending);
	}
#endif

	int rc = map->ops->lookup(map, obj, FUSE_MAP_LOOKUP_FUSE_2, (void **)&item);
	if (rc == FUSE_SUCCESS) {
		assert(item);
		TAILQ_FOREACH(req, &item->fuse_waiting_head, fuse_waiting) {
			if (dfly_is_f1_cmd(req) && dfly_is_fuse_pair(req, f2)) {
				//find the f1 of the same session, link f2 as next req
				req->next_req = f2;
				found_f1 = req;
				break;
			}

			//duplicated f2 ?
			assert(!dfly_is_fuse_duplicate(req, f2));
		}

		if (!found_f1) {
			//fuse_log("can not find f1 with key 0x%llx%llx\n",
			//    *(long long *)item->key->key, *(long long*)(item->key->key+8));
			io_rc = FUSE_IO_RC_WAIT_FOR_F1;
			f2->req_fuse_data = item;
			TAILQ_INSERT_TAIL(&item->fuse_waiting_head, f2, fuse_waiting);
			fuse_log("fuse_handle_F2_op f2 %p comes first, key 0x%llx%llx, io_rc %x\n",
				 f2, *(long long *)item->key->key, *(long long *)((char *)item->key->key + 8), io_rc);
		} else {
			fuse_log("fuse_handle_F2_op f1 %p item %p key 0x%llx%llx, io_rc %x\n",
				 found_f1, item, *(long long *)item->key->key, *(long long *)((char *)item->key->key + 8), io_rc);

			f2->req_fuse_data = item;
			f2->f1_req = found_f1;
			io_rc = fuse_start_fuse(item, found_f1, found_f1, flags, 2);
		}
		//assert(found_f1);

	} else {
		assert(0);
	}

	return io_rc;
}

extern struct dfly_request_ops df_req_ops_inst;

struct dfly_request *create_req_for_wal(struct dfly_key *key,
					struct dfly_value *val, uint16_t opc,
					df_dev_io_completion_cb cb, void *cb_arg)
{

	struct dfly_request *req = NULL;

	req = dfly_io_get_req(NULL);
	assert(req);
	req->ops = df_req_ops_inst;

	req->req_key = *key;
	if (val)
		req->req_value = *val;

	req->flags |= DFLY_REQF_NVME;
	struct spdk_nvme_cmd *cmd = (struct spdk_nvme_cmd *)req->req_ctx;

	cmd->opc = opc;
	req->req_private = cb_arg;
	req->ops.complete_req = cb;

	return req;
}

//retuen FUSE_IO_RC_F1_PENDING if io_module pass_through
//return FUSE_IO_RC_F1_FAIL if wal cache read and compare mismatch.
//return FUSE_IO_RC_F1_MATCH if wal cache read and compare match.

int do_fuse_F1_op(fuse_map_item_t *item, int flags)
{
	int io_rc = FUSE_IO_RC_FUSE_IN_PROGRESS;
	int rc = FUSE_SUCCESS;

	//get read buffer
	struct dfly_request *io_request = NULL;
	struct dfly_request *f1 = item->fuse_req;
	struct dfly_value *f1_val = f1->ops.get_value(f1);
	struct dfly_value *val = (struct dfly_value *) df_malloc(sizeof(struct dfly_value));
	assert(val);
	val->length = f1_val->length;
	val->offset = 0;
	void *buff = fuse_get_value_buff(val);
	assert(buff);
	val->value = buff;
	item->f1_read_val = val;

	if (g_fuse_conf.wal_enabled) {

		struct dfly_key large_key = {0, 0};
		struct dfly_key *key = item->key;
		if (key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
			if (!fuse_prepare_large_key(&large_key, item)) {
				key = &large_key;
			} else {
				DFLY_WARNLOG("fuse_prepare_large_key failed, retry f1 %p\n", f1);
				io_rc = FUSE_IO_RC_RETRY;
				goto f1_done;
			}
		}

		rc = dfly_device_build_request(item->map->tgt_fh, SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE,
					       key, val, dfly_kv_pool_fuse_complete, item, &io_request);

		/*        io_request = create_req_for_wal(item->key, val,
		                SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE,
		                dfly_kv_pool_fuse_complete, item);
		        //assert(!rc); */
		assert(!rc && io_request);
		uint32_t full_size = 0;
		io_request->req_dfly_ss = f1->req_dfly_ss;
		io_request->req_ssid = f1->req_ssid;
		io_request->req_private = item;
		io_request->req_fuse_data = &full_size;

		io_request->parent = f1;
		io_request->state = DFLY_REQ_IO_INTERNAL_SUBMITTED_TO_WAL;

		//io_request->req_fuse_data = item;
		rc = wal_io(io_request, DF_OP_LOCKLESS);

		//struct dragonfly_subsystem *ss  = dfly_subsystem_get(io_request->req_ssid);
		//dfly_module_post_request(ss->mlist.dfly_wal_module, io_request);

		if (rc == WAL_SUCCESS) {
			//cache hit read
			if (full_size != val->length || fuse_compare_value(val, f1_val)) {
				io_rc = FUSE_IO_RC_F1_FAIL;
			} else {
				io_rc = FUSE_IO_RC_F1_MATCH;
			}
			fuse_log("do_fuse_F1_op: wal_io item %p return full_size %d io_rc %d\n",
				 item, full_size, io_rc);
			//done the F1 read
			fuse_release_value_buff(val);
			//dfly_io_put_buff(NULL, val->value);
			df_free(val);
			item->f1_read_val = NULL;
			dfly_io_put_req(NULL, io_request);
			goto f1_done;
		} else {
			dfly_io_put_req(NULL, io_request);
		}
	}

	if (!g_fuse_conf.wal_enabled || rc != WAL_SUCCESS) {
		struct dfly_key large_key = {0, 0};
		struct dfly_key *key = item->key;
		if (key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
			if (!fuse_prepare_large_key(&large_key, item)) {
				key = &large_key;
			} else {
				DFLY_WARNLOG("fuse_prepare_large_key failed, retry req %p\n", f1);
				io_rc = FUSE_IO_RC_RETRY;
				goto f1_done;
			}
		}

		rc = dfly_device_retrieve(item->map->tgt_fh, key, val,
					  dfly_kv_pool_fuse_complete, item);
		if (rc) {
			//dfly_io f1 ops submission failed
			assert(0);
		} else {
			io_rc = FUSE_IO_RC_FUSE_IN_PROGRESS;
		}
	}

f1_done:
	return io_rc;
}

int do_fuse_F2_op(fuse_map_item_t *item, int flags)
{
	int rc = FUSE_SUCCESS;
	int io_rc = FUSE_IO_RC_FUSE_IN_PROGRESS;
	int f2_delete = 0;
	//item->status = FUSE_STATUS_F2_PENDING;
	struct dfly_request *f2 = item->fuse_req;
	struct dfly_value *f2_val = NULL;

	if (dfly_req_fuse_2_delete(f2)) {
		fuse_log("do_fuse_F2_op delete\n");
		f2_delete = 1;
	} else {
		f2_val = f2->ops.get_value(f2);
		fuse_log("do_fuse_F2_op store length %d\n", f2_val->length);
	}

	if (g_fuse_conf.wal_enabled) {
		struct dfly_request *io_request = NULL;
		struct dfly_key *key = item->key;

		struct dfly_key large_key;
		if (key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
			if (!fuse_prepare_large_key(&large_key, item)) {
				key = &large_key;
			} else {
				DFLY_WARNLOG("fuse_prepare_large_key fail for item %p\n", item);
				io_rc = FUSE_IO_RC_RETRY;
				goto f2_done;
			}
		}

		if (!f2_delete) { //overwrite case
			rc = dfly_device_build_request(item->map->tgt_fh, SPDK_NVME_OPC_SAMSUNG_KV_STORE,
						       key, f2_val, dfly_fuse_wal_complete, item, &io_request);
			//io_request = create_req_for_wal(item->key, f2_val,
			//    SPDK_NVME_OPC_SAMSUNG_KV_STORE,
			//    dfly_fuse_wal_complete, item);

		} else {               //deletion case
			rc = dfly_device_build_request(item->map->tgt_fh, SPDK_NVME_OPC_SAMSUNG_KV_DELETE,
						       key, f2_val, dfly_fuse_wal_complete, item, &io_request);
			//io_request = create_req_for_wal(item->key, f2_val,
			//    SPDK_NVME_OPC_SAMSUNG_KV_DELETE,
			//    dfly_fuse_wal_complete, item);
		}
		assert(io_request);
		io_request->parent = f2;
		io_request->state = DFLY_REQ_IO_INTERNAL_SUBMITTED_TO_WAL;
		io_request->req_dfly_ss = f2->req_dfly_ss;

#if 1
		struct dfly_subsystem *ss = io_request->req_dfly_ss; //dfly_get_subsystem(io_request->req_ssid);
		dfly_module_post_request(ss->mlist.dfly_wal_module, io_request);

		return io_rc;
#else
		rc =  wal_io(io_request, DF_OP_LOCKLESS);
		if (rc == WAL_SUCCESS) {
			item->fuse_req = NULL;
			f2->next_action = DFLY_COMPLETE_NVMF;
			_complete_pending_fuse_op(item, f2);

			io_rc = FUSE_IO_RC_F2_COMPLETE;
		} else if (rc == WAL_ERROR_LOG_PENDING || rc == WAL_ERROR_DELETE_PENDING) {
			io_rc = FUSE_IO_RC_FUSE_IN_PROGRESS;
		} else if (rc == WAL_ERROR_IO_RETRY) {
			item->status = FUSE_STATUS_F2_RETRY;//wal is busy, retry write/delete
			io_rc = FUSE_IO_RC_RETRY;
		}

		//if write miss, no request submitted.
		//if(rc != WAL_ERROR_WRITE_MISS){
		//    goto f2_done;
		//}
		if (FUSE_IO_RC_FUSE_IN_PROGRESS != io_rc)
			dfly_io_put_req(NULL, io_request);

		fuse_log("do_fuse_F2_op: key %llx%llx f2_delete %d wal_io rc %d\n",
			 *(long long *)item->key->key, *(long long *)(item->key->key + 8), f2_delete, rc);
#endif
	}

	if (!g_fuse_conf.wal_enabled
	    || rc == WAL_ERROR_WRITE_MISS
	    || rc == WAL_ERROR_DELETE_SUCCESS) {

		struct dfly_key large_key = {0, 0};
		struct dfly_key *key = item->key;
		if (key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
			if (fuse_prepare_large_key(&large_key, item)) {
				key = &large_key;
			} else {
				DFLY_WARNLOG("fuse_prepare_large_key failed, retry item %p\n", item);
				io_rc = FUSE_IO_RC_RETRY;
				goto f2_done;
			}
		}

		if (!f2_delete)
			rc = dfly_device_store(item->map->tgt_fh, key, f2_val,
					       dfly_kv_pool_fuse_complete, item);
		else
			rc = dfly_device_delete(item->map->tgt_fh, key,
						dfly_kv_pool_fuse_complete, item);

		assert(!rc);
		io_rc = FUSE_IO_RC_FUSE_IN_PROGRESS;
	}

f2_done:
	return io_rc;
}

int do_fuse_handle_fuse_op(struct dfly_request *req,
			   fuse_map_item_t *item, int flags)
{
	int io_rc = FUSE_IO_RC_FUSE_IN_PROGRESS;
	int rc = FUSE_SUCCESS;
	assert(item && req);

	struct dfly_request *f1 = req;

	struct dfly_request *f2;

next_fuse:

	f2 = f1->next_req;

#ifdef FUSE_TIMEOUT
	if (g_fuse_conf.fuse_timeout_ms > 0) {
		TAILQ_REMOVE_INIT(&(item->map->wp_pending_queue), f1, fuse_pending);
		TAILQ_REMOVE_INIT(&(item->map->wp_pending_queue), f2, fuse_pending);
	}
	//printf("remove f1 %p f2 %p\n", f1, f2);
#endif

	io_rc = do_fuse_F1_op(item, flags);
	if (io_rc == FUSE_IO_RC_F1_MATCH) {

		item->fuse_req = f2;
		io_rc = do_fuse_F2_op(item, flags);
		if (FUSE_IO_RC_RETRY != io_rc) {
			f1->next_action = DFLY_COMPLETE_NVMF;
			_complete_pending_fuse_op(item, f1);
		} else {
			//assert(0);
			return io_rc;
		}
	} else if (io_rc == FUSE_IO_RC_F1_FAIL) {
		dfly_set_status_code(f1, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_FUSE_CMP_FAILURE);
		f1->next_action = DFLY_COMPLETE_NVMF;
		_complete_pending_fuse_op(item, f1);
		f2->next_action = DFLY_COMPLETE_NVMF;
		dfly_set_status_code(f2, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_FUSE_CMD2_ABORT);
		_complete_pending_fuse_op(item, f2);
		item->fuse_req = NULL;

	} else if (io_rc == FUSE_IO_RC_FUSE_IN_PROGRESS) {
		//process on complete cb
	} else {
		//F1 op is read, should not retry.
		assert(io_rc == FUSE_IO_RC_RETRY);
	}

	if (!item->fuse_req) {
		if (f1 = find_get_linked_fuse(item)) {
			fuse_log("do_fuse_handle_fuse_op: find next fuse %p on map[%d] bucket %p\n", f1,
				 item->map->map_idx, item->bucket);
			item->fuse_req = f1;
			TAILQ_REMOVE_INIT(&item->fuse_waiting_head, f1, fuse_waiting);
			goto next_fuse;
		}
	}


	return io_rc;
}

int do_fuse_io(int pool_id, fuse_object_t *obj, int opc, int op_flags)
{
	fuse_map_item_t *item = NULL;
	int rc = FUSE_IO_RC_PASS_THROUGH;

	if (!g_fuse_conf.fuse_enabled)
		return rc;

	int nr_io_pending = ATOMIC_INC_FETCH(__fuse_ctx.nr_io_pending);

	fuse_map_t *map = fuse_get_object_map(pool_id, obj);

	if (fuse_debug_level) {
		struct dfly_request *rr = NULL, *tmp_req;
		std::map<struct dfly_request *, int> req_map;
		int i = 0;
		//printf("fuse insert map[%d]: req ",map->map_idx);
		TAILQ_FOREACH_SAFE(rr, &map->io_pending_queue, fuse_pending_list, tmp_req) {
			if (!rr->req_ctx) {
				assert(0);
				printf("do_fuse_io: found invalid req %p\n", rr);
				TAILQ_REMOVE_INIT(&map->io_pending_queue, rr, fuse_pending_list);
				continue;
			}
			//printf("%p opc 0x%x ", rr, rr->ops.get_command(rr));
			if (req_map.find(rr) != req_map.end()) {
				assert(0);
			} else {
				req_map[rr] = i++;
			}

		}
		//printf("\n");
		assert(req_map.find((struct dfly_request *)obj->obj_private) == req_map.end());
		TAILQ_INSERT_HEAD(&map->io_pending_queue, ((struct dfly_request *)obj->obj_private),
				  fuse_pending_list);
		//printf("do new req %p cnt %d\n", obj->obj_private, nr_io_pending);
	}

	dfly_io_module_handler_t *ops = fuse_module_ctx.io_handlers;
	if (op_flags & DF_OP_LOCK) df_lock(&map->map_lock);

#if 0
	if (!(opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE
	      || opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE)) { //record only the fuse ops
		struct dfly_request *req = (struct dfly_request *)obj->obj_private;
		start_timer(&req->req_ts);
		TAILQ_INSERT_TAIL(&map->wp_pending_queue, req, fuse_pending);
	}
#endif

	switch (opc) {
	case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
		rc = ops->store_handler(map, obj, op_flags);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
		rc = ops->delete_handler(map, obj, op_flags);
		break;
	//case SPDK_NVME_OPC_SAMSUNG_KV_FUSE:
	//    rc = ops->fuse_handler(map, obj, op_flags);
	//    break;
	case SPDK_NVME_OPC_SAMSUNG_KV_CMP:
		rc = ops->fuse_f1_handler(map, obj, op_flags);
		break;
	case DFLY_REQ_OPC_STORE_FUSE2:
	case DFLY_REQ_OPC_DELETE_FUSE2:
		rc = ops->fuse_f2_handler(map, obj, op_flags);
		break;
	default:
		printf("do_fuse_io: supported opc %x\n", opc);
		break;
	}

	if (op_flags & DF_OP_LOCK) df_unlock(&map->map_lock);
	return rc;
}

int fuse_io(struct dfly_request *req, int fuse_op_flags)
{
	int pool_id = fuse_get_pool_id(req->req_dfly_ss);
	int opc = req->ops.get_command(req);

	if (!(opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE
	      || opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE
	      || opc == SPDK_NVME_OPC_SAMSUNG_KV_CMP
	      || opc == DFLY_REQ_OPC_STORE_FUSE2
	      || opc == DFLY_REQ_OPC_DELETE_FUSE2))
		return FUSE_IO_RC_PASS_THROUGH;

	fuse_object_t obj;
	obj.key_hashed = 0;
	obj.key = req->ops.get_key(req);
	obj.val = req->ops.get_value(req);
	obj.obj_private = req;
	TAILQ_INIT_ENTRY(&req->fuse_pending_list);
	TAILQ_INIT_ENTRY(&req->fuse_delay); /**< request pool linkage for retry */
	//TAILQ_INIT_ENTRY(req->fuse_pending); /**< request linkage in process, to be unlinked on completion */
	//TAILQ_INIT_ENTRY(req->fuse_waiting); /**< F1 request waiting linkage for F2 */

	if (req->req_fuse_data) {
		//retry request.
		printf("get retry req %p opc 0x%x\n", req, req->opc);
	} else {
		req->req_fuse_data = NULL;
		req->next_req = NULL;
		req->opc = opc;
	}

	//fuse_log("fuse_io opc 0x%x key: size %d 0x%llx%llx\n",
	//  opc, d_key->length, *(long long *)d_key->key, *(long long *)(d_key->key + 8));
	int io_rc = do_fuse_io(pool_id, &obj, opc, fuse_op_flags);
	if (0 && !fuse_module_ctx.s.total_cnt % 10000) {
		fuse_log("fuse_io wt %lld, dl %lld, fs %lld\n",
			 fuse_module_ctx.s.write_cnt,
			 fuse_module_ctx.s.delete_cnt,
			 fuse_module_ctx.s.fuse_cnt);
	}
	return io_rc;
}

int fuse_init_by_conf(struct dfly_subsystem *pool, void *conf,
		      void *cb, void *cb_arg)
{
	fuse_conf_t *fconf = (fuse_conf_t *)conf;

	if (g_fuse_conf.fuse_enabled) {
		snprintf(__fuse_nqn_name, strlen(g_fuse_conf.fuse_nqn_name) + 1, "%s", g_fuse_conf.fuse_nqn_name);
		return fuse_init(pool, fconf->nr_maps_per_pool, fconf->fuse_nr_cores, 0, (void *)cb, cb_arg);
	}
	return 0;
}
int fuse_init(struct dfly_subsystem *pool, int nr_maps, int nr_of_cores, int fuse_flag,
	      void *cb, void *cb_arg)
{
	//int open_flag = WAL_OPEN_FORMAT;
	int pool_exist = 0;
	int zone_idx = 0;
	int rc = FUSE_SUCCESS;

	fuse_debug_level = g_fuse_conf.fuse_debug_level;
	printf("fuse_init: %s nr_maps %d nr_of_cores %d\n",
	       g_fuse_conf.fuse_nqn_name, nr_maps, nr_of_cores);
	df_lock(&fuse_module_ctx.ctx_lock);

	rc = fuse_context_init(&__fuse_ctx, pool, nr_maps, fuse_flag);

	if (rc == FUSE_SUCCESS) {
		dfly_fuse_module_init(pool->id, g_fuse_conf.fuse_nr_cores, (df_module_event_complete_cb)cb, cb_arg);
	}

	df_unlock(&fuse_module_ctx.ctx_lock);

	return 0;
}

int fuse_finish(struct dfly_subsystem *pool)
{
	fuse_context_deinit(pool);
	return 0;
}

