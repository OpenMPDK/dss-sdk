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


#include "dragonfly.h"
#include "wal_lib.h"
#include "wal_module.h"

extern wal_conf_t g_wal_conf;
extern wal_context_t g_wal_ctx;
extern bool __log_enabled ;
extern bool __periodic_flush;

struct wal_thread_inst_ctx {
	int num_zones;
	wal_zone_t *zone_arr[0];
};

void *wal_find_instance_ctx(struct dfly_request *request);

int dfly_wal_req_process(void *ctx, struct dfly_request *req)
{
	wal_zone_t *zone = wal_get_zone_from_req(req);
	int rc = wal_io(req, (g_wal_conf.wal_open_flag & DF_OP_MASK));

#ifdef WAL_DFLY_TRACK
	if (req->retry_count)
		printf("wal_io %p retry rc %d\n", req, rc);
	else
		TAILQ_INSERT_TAIL(&zone->debug_queue, req, wal_debuging);
#endif

	if (rc == WAL_SUCCESS) { //cached hit for read/write.
		req->next_action = DFLY_COMPLETE_NVMF;
		if (req->retry_count) {
			req->retry_count = 0;
		}
#ifdef WAL_DFLY_TRACK
		TAILQ_REMOVE_INIT(&zone->debug_queue, req, wal_debuging);
#endif
		return DFLY_MODULE_REQUEST_PROCESSED;
	} else if (rc == WAL_ERROR_IO_RETRY) {
		//wal_zone_t *zone = wal_get_zone_from_req(req);
		req->retry_count++;
		TAILQ_INSERT_TAIL(&zone->wp_queue, req, wal_pending);
		return DFLY_MODULE_REQUEST_QUEUED;
	} else if ((rc == WAL_ERROR_LOG_PENDING) ||
		   (rc == WAL_ERROR_DELETE_PENDING)) {
		req->next_action = DFLY_ACTION_NONE;
		return DFLY_MODULE_REQUEST_QUEUED;
	} else if ((rc == WAL_ERROR_DELETE_SUCCESS || rc == WAL_ERROR_DELETE_MISS ||
		    rc == WAL_ERROR_WRITE_MISS)
		   && req->state == DFLY_REQ_IO_INTERNAL_SUBMITTED_TO_WAL) {
		//assert(!__log_enabled);

		req->state = DFLY_REQ_IO_SUBMITTED_TO_WAL;
		req->next_action = DFLY_FORWARD_TO_IO_THREAD;

		if (req->retry_count) {
			req->retry_count = 0;
		}

#ifdef WAL_DFLY_TRACK
		TAILQ_REMOVE_INIT(&zone->debug_queue, req, wal_debuging);
#endif

		return DFLY_MODULE_REQUEST_PROCESSED;
	}

	if (req->retry_count) {
		req->retry_count = 0;
	}
	assert(req->state == DFLY_REQ_IO_SUBMITTED_TO_WAL);
	req->next_action = DFLY_FORWARD_TO_IO_THREAD;

#ifdef WAL_DFLY_TRACK
	TAILQ_REMOVE_INIT(&zone->debug_queue, req, wal_debuging);
#endif

	return DFLY_MODULE_REQUEST_PROCESSED;
}

int dfly_wal_generic_poll(void *ctx)
{
	struct wal_thread_inst_ctx *wal_thrd_ctx = (struct wal_thread_inst_ctx *)ctx;
	int i, rc;

	for (i = 0; i < wal_thrd_ctx->num_zones; i++) {
		wal_zone_t *zone = wal_thrd_ctx->zone_arr[i];
		struct dfly_request *req, *req_tmp;
		wal_dump_info_t *cache_dump_info = &(zone->log->hdr.dump_info);
		int switch_buffer = 0;
		//periodically flush if there are dirty object for a while,
		//not necessary till cache full
		if (__periodic_flush && zone->poll_flush_flag == WAL_POLL_FLUSH_DONE
		    && zone->log->nr_objects_inserted
		    && ((elapsed_time(zone->flush_idle_ts) >> 20) >= g_wal_conf.wal_cache_flush_period_ms)) {
			// two seconds idle flush
			wal_debug("zone [%d] flush period timeout with nr_req %d nr_dump_group %d conf %d\n",
				  zone->zone_id, zone->log->nr_objects_inserted,
				  zone->log->nr_dump_group,
				  g_wal_conf.wal_cache_flush_period_ms);

			if (cache_dump_info->dump_blk) {
				wal_dump_info_t dump_info = * cache_dump_info;
				dump_info.dump_flags |= DUMP_BATCH;
				int dump_rc = wal_log_dump_objs(zone->associated_zone, &dump_info);
			}
			wal_zone_switch_buffer(zone);

			switch_buffer = 1;

			zone->poll_flush_flag &= (~WAL_POLL_FLUSH_DONE);
			zone->poll_flush_flag |= WAL_POLL_FLUSH_DOING;

		}
		wal_cache_flush_spdk_proc((void *)zone);

		if (!switch_buffer && cache_dump_info->dump_blk &&
		    ((elapsed_time(cache_dump_info->dump_ts) >> 10) >= zone->dump_timeout_us)) {

			//copy the current dump group info
			wal_dump_info_t dump_info = * cache_dump_info;
			dump_info.dump_flags |= DUMP_BATCH;

			//update buffer hdr for next dump grouping.
			update_dump_group_info(zone->log);
			/*
			//prepare the nexe dump obj group area
			cache_dump_info->dump_addr = zone->log->hdr.curr_pos;
			cache_dump_info->dump_blk = 0;
			cache_dump_info->dump_flags = DUMP_NOT_READY;
			cache_dump_info->dump_ts = {0};
			start_timer(&cache_dump_info->idle_ts);
			zone->log->hdr.curr_pos += WAL_DUMP_HDR_SZ;
			wal_dump_hdr_t * dump_hdr = (wal_dump_hdr_t *)cache_dump_info->dump_addr;
			dump_hdr->nr_batch_reqs = 0;
			*/
			wal_dump_hdr_t *dump_hdr = (wal_dump_hdr_t *) dump_info.dump_addr;
			int nr_reqs_holded =  dump_hdr->nr_batch_reqs;
			void *dump_src_addr = (void *)dump_hdr;

			zone->dump_timeout_us = zone->dump_timeout_us + g_wal_conf.wal_log_batch_timeout_us_adjust;
			zone->log_batch_nr_obj = zone->log_batch_nr_obj - g_wal_conf.wal_log_batch_nr_obj_adjust;
			if (zone->log_batch_nr_obj <= 0)
				zone->log_batch_nr_obj = 1;

			assert(zone->associated_zone);
			zone->log->nr_dump_group ++;
			int dump_rc = wal_log_dump_objs(zone->associated_zone, &dump_info);

			wal_debug("zone[%d] timeout_us %d batch_nr %d dump_addr 0x%llx with %d nr_reqs time out rc %d\n",
				  zone->zone_id, zone->dump_timeout_us, zone->log_batch_nr_obj, dump_src_addr, nr_reqs_holded,
				  dump_rc);

		} else {
			if (!cache_dump_info->dump_blk &&
			    elapsed_time(cache_dump_info->idle_ts) >= zone->dump_timeout_us * 4) {
				zone->dump_timeout_us = g_wal_conf.wal_log_batch_timeout_us;
				zone->log_batch_nr_obj = g_wal_conf.wal_log_batch_nr_obj;
			}
		}


		TAILQ_FOREACH_SAFE(req, &zone->wp_queue, wal_pending, req_tmp) {
			printf("to handle retry %p in zone[%d]\n", req, zone->zone_id);
			TAILQ_REMOVE_INIT(&zone->wp_queue, req, wal_pending);
			rc = dfly_wal_req_process(ctx, req);
			printf("after handle retry %p in zone[%d] rc %d\n", req, zone->zone_id, rc);
			if (rc == DFLY_MODULE_REQUEST_PROCESSED) {
				dfly_handle_request(req);
			}

			break;
		}

	}

	return 0;
}

//Call only once per thread instantiation
void *wal_module_store_inst_context(void *mctx, void *inst_ctx, int inst_index)
{
	wal_subsystem_t *wal_subsys = (wal_subsystem_t *)mctx;

	struct wal_thread_inst_ctx *wal_thrd_ctx;

	int num_cores = g_wal_conf.wal_nr_cores;//TODO: Move as part of context struct

	int max_zones = wal_subsys->nr_zones;
	int current_zone_idx = inst_index;

	assert(max_zones >= 1);

	if (current_zone_idx > max_zones) {
		DFLY_NOTICELOG("More threads than number of zones\n");
		return NULL;
	}

	wal_thrd_ctx = (struct wal_thread_inst_ctx *) calloc(1, sizeof(struct wal_thread_inst_ctx) + sizeof(wal_zone_t *) * max_zones);
	if (!wal_thrd_ctx) {
		assert(wal_thrd_ctx);
	}

	do {
		wal_thrd_ctx->zone_arr[wal_thrd_ctx->num_zones] =
			(wal_zone_t *)wal_set_zone_module_ctx(wal_subsys->dfly_pool_id,
					current_zone_idx, inst_ctx);
		wal_thrd_ctx->num_zones++;

		current_zone_idx += num_cores;
	} while (current_zone_idx < wal_subsys->nr_zones);

	return wal_thrd_ctx;
}

void *wal_find_instance_ctx(struct dfly_request *request)
{
	return wal_get_dest_zone_module_ctx(request);
}

struct dfly_module_ops wal_module_ops = {
	.module_init_instance_context = wal_module_store_inst_context,
	.module_rpoll = dfly_wal_req_process,
	.module_cpoll = NULL,
	.module_gpoll = dfly_wal_generic_poll,
	.find_instance_context = wal_find_instance_ctx,
};

int dfly_wal_module_init(int ssid, int nr_cores, void *cb, void *cb_arg)
{

	struct dfly_subsystem *ss = dfly_get_subsystem(ssid);

	ss->mlist.dfly_wal_module = dfly_module_start("WAL", ssid, &wal_module_ops,
				    &g_wal_ctx.pool_array[ssid],
				    nr_cores, (df_module_event_complete_cb)cb, cb_arg);

	return 0;
}
