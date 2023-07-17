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

#include "dragonfly.h"
#include "df_io_module.h"
#include "fuse_lib.h"
#include "fuse_module.h"

extern fuse_conf_t g_fuse_conf;
extern dfly_io_module_context_t fuse_module_ctx;
extern int fuse_debug_level;

long long nr_timeout_fuse_req = 0;

struct fuse_thread_inst_ctx {
	int num_maps;
	fuse_map_t *maps_arr[0];
};

void *fuse_find_instance_ctx(struct dfly_request *request);

int dfly_fuse_req_process(void *ctx, struct dfly_request *req)
{

	/*

	return: FUSE_IO_RC_RETRY queued

	store return: FUSE_IO_RC_WRITE_SUCCESS
	delete return: FUSE_IO_RC_DELETE_SUCCESS

	f1 return:  FUSE_IO_RC_F1_FAIL (f1, f2 completed)
	            FUSE_IO_RC_FUSE_IN_PROGRESS (wait cb)

	f2 return:  FUSE_IO_RC_F2_COMPLETE (f1, f2 completed)
	            FUSE_IO_RC_FUSE_IN_PROGRESS (wait cb)
	*/


	int io_rc = fuse_io(req, g_fuse_conf.fuse_op_flag & DF_OP_MASK);
	int dfly_rc = -1;
	if (io_rc == FUSE_IO_RC_WRITE_SUCCESS || io_rc == FUSE_IO_RC_DELETE_SUCCESS) {
		if (g_fuse_conf.wal_enabled)
			req->next_action = DFLY_FORWARD_TO_WAL;
		else
			req->next_action = DFLY_FORWARD_TO_IO_THREAD;

		dfly_rc = DFLY_MODULE_REQUEST_PROCESSED;
	} else if (io_rc == FUSE_IO_RC_FUSE_IN_PROGRESS
		   || io_rc == FUSE_IO_RC_WAIT_FOR_F2
		   || io_rc == FUSE_IO_RC_WAIT_FOR_F1) {   //f1 or f2 in progress

		fuse_log("dfly_fuse_req_process: req %p io_rc = %d\n", req, io_rc);

		req->next_action = DFLY_ACTION_NONE;
		dfly_rc = DFLY_MODULE_REQUEST_QUEUED;

	} else if (io_rc == FUSE_IO_RC_F2_COMPLETE) { //f2 complete

		dfly_rc = DFLY_MODULE_REQUEST_QUEUED;

	} else if (io_rc == FUSE_IO_RC_F1_FAIL) { //f1 mismatch

		dfly_rc = DFLY_MODULE_REQUEST_QUEUED;

	} else if (io_rc == FUSE_IO_RC_RETRY) {  //op retry

		fuse_object_t obj;
		obj.key = req->ops.get_key(req);
		obj.key_hashed = 0;
		fuse_map_t *map = fuse_get_object_map(req->req_dfly_ss->id, &obj);

		printf("fuse retry req %p opc 0x%x on map %p\n", req, req->opc, map);
		if (req->opc == DFLY_REQ_OPC_STORE_FUSE2 || req->opc == DFLY_REQ_OPC_DELETE_FUSE2) {
			assert(req->f1_req);
			req = req->f1_req;
		}
		req->retry_count++;
		TAILQ_INSERT_HEAD(&map->wp_delay_queue, req, fuse_delay);
		dfly_rc = DFLY_MODULE_REQUEST_QUEUED;
	}

	if (dfly_rc != -1) { //done
		return dfly_rc;
	}

	// FUSE_IO_RC_PASS_THROUGH
	if (g_fuse_conf.wal_enabled)
		req->next_action = DFLY_FORWARD_TO_WAL;
	else
		req->next_action = DFLY_FORWARD_TO_IO_THREAD;

	return DFLY_MODULE_REQUEST_PROCESSED;
}

int dfly_fuse_req_complete(void *ctx, struct dfly_request *req)
{
	struct df_dev_response_s resp;
	int fuse_op = 0;
	if (req->flags & DFLY_REQF_NVMF) {
		int opc = req->ops.get_command(req);

		if (opc == SPDK_NVME_OPC_SAMSUNG_KV_CMP ||
		    opc == DFLY_REQ_OPC_STORE_FUSE2 ||
		    opc == DFLY_REQ_OPC_DELETE_FUSE2) {
			fuse_op = 1;
		} else {
			/*
			assert(opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE ||
					opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE ||
					opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE); */
		}
		//assert(req->req_fuse_data);
		if (req->req_fuse_data) {
			dfly_fuse_complete(req, req->status);
			req->req_fuse_data = NULL;
		}
		//printf("dfly_fuse_req_complete\n");
		if (!fuse_op) {
			req->state = DFLY_REQ_IO_NVMF_DONE;
			dfly_handle_request(req);
		}

	} else if (req->flags & DFLY_REQF_NVME) {
		struct dfly_request *parent_req = req->parent;
		if (parent_req)
			parent_req->status = req->status;

		assert(!req->req_fuse_data);
		fuse_log("dfly_fuse_req_complete: req %p, status %d, req_private %p\n",
			 req, req->status, req->req_private);

		resp.rc = req->status;
		req->ops.complete_req(resp, req->req_private);
		dfly_io_put_req(NULL, req);

		if (parent_req) {
			if (parent_req->req_fuse_data) {
				dfly_fuse_complete(parent_req, parent_req->status);
				parent_req->req_fuse_data = NULL;
			} else {
				printf("dfly_fuse_req_complete: req_fuse_data == NULL !!! req %p parent_req %p opc 0x%x\n",
				       req, parent_req, parent_req->opc);
				parent_req->state = DFLY_REQ_IO_NVMF_DONE;
				dfly_handle_request(req);
			}

		}
		//dfly_fuse_complete(req, req->status);
	} else {
		assert(0);
	}

	return DFLY_MODULE_REQUEST_PROCESSED;
}

int dfly_fuse_generic_poll(void *ctx)
{
	struct fuse_thread_inst_ctx *fuse_thrd_ctx = (struct fuse_thread_inst_ctx *)ctx;
	int i, rc;

	//printf("dfly_fuse_generic_poll nr_maps %d\n ", fuse_thrd_ctx->num_maps);
	for (i = 0; i < fuse_thrd_ctx->num_maps; i++) {
		fuse_map_t *map = fuse_thrd_ctx->maps_arr[i];
		struct dfly_request *req, *req_tmp;

#ifdef FUSE_TIMEOUT
		TAILQ_FOREACH_SAFE(req, &map->wp_pending_queue, fuse_pending, req_tmp) {
			if ((elapsed_time(req->req_ts) >> 10) >= g_fuse_conf.fuse_timeout_ms) {
				TAILQ_REMOVE_INIT(&map->wp_pending_queue, req, fuse_pending);
				nr_timeout_fuse_req ++;
				//struct dfly_request * next_req = req->next_req;
				fuse_map_item_t *item = (fuse_map_item_t *)req->req_fuse_data;
				if (item) {
					assert(item->fuse_req != req);
					TAILQ_REMOVE_INIT(&item->fuse_waiting_head, req, fuse_waiting);
				}

				fuse_log("fuse %p timeout %lld ms cnt %lld\n",
					 req, elapsed_time(req->req_ts) >> 10, nr_timeout_fuse_req);

				//complete f1 as timeout
				req->next_action = DFLY_COMPLETE_NVMF;
				dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD,
						     SPDK_NVME_SC_KV_FUSE_CMD_MISSED);
				req->state = DFLY_REQ_IO_NVMF_DONE;
				dfly_handle_request(req);

			} else {
				break;
			}

		}
#endif

		TAILQ_FOREACH_SAFE(req, &map->wp_delay_queue, fuse_delay, req_tmp) {
			printf("do delayed req %p\n", req);
			TAILQ_REMOVE_INIT(&map->wp_delay_queue, req, fuse_delay);
			if (fuse_debug_level) {
				TAILQ_REMOVE_INIT(&map->io_pending_queue, req, fuse_pending_list);
			}
			rc = dfly_fuse_req_process(ctx, req);
			if (rc == DFLY_MODULE_REQUEST_PROCESSED)
				printf("done delayed req %p\n", req);

			break;
		}

	}

	return 0;
}

//Call only once per thread instantiation
void *fuse_module_store_inst_context(void *mctx, void *inst_ctx, int inst_index)
{
	dfly_io_module_context_t *fuse_mctx = (dfly_io_module_context_t *)mctx;
	fuse_context_t *fuse_ctx = (fuse_context_t *)fuse_mctx->ctx ;
	struct fuse_thread_inst_ctx *fuse_thrd_ctx;

	fuse_map_t *map = NULL;
	int num_cores = g_fuse_conf.fuse_nr_cores;
	int max_zones = fuse_ctx->nr_maps;
	int current_map_idx = inst_index;

	assert(max_zones >= 1);

	if (current_map_idx > max_zones) {
		printf("More threads %d than number of zones %d\n", current_map_idx, max_zones);
		return NULL;
	}

	fuse_thrd_ctx = (struct fuse_thread_inst_ctx *)calloc(1, sizeof(struct fuse_thread_inst_ctx) + sizeof(fuse_map_t *) * max_zones);
	if (!fuse_thrd_ctx) {
		assert(fuse_thrd_ctx);
	}

	do {
		map = fuse_ctx->maps[current_map_idx];
		map->module_instance_ctx = inst_ctx;
		fuse_thrd_ctx->maps_arr[fuse_thrd_ctx->num_maps] = map;
		fuse_log("fuse_module_store_inst_context map[%d]=inst %p map %p\n",
			 current_map_idx, inst_ctx, map);
		fuse_thrd_ctx->num_maps ++;
		current_map_idx += num_cores;
	} while (current_map_idx < fuse_ctx->nr_maps);

	return fuse_thrd_ctx;
}

void *fuse_find_instance_ctx(struct dfly_request *request)
{
	return fuse_get_map_module_ctx(request);
}

struct dfly_module_ops fuse_module_ops = {
	.module_init_instance_context = fuse_module_store_inst_context,
	.module_rpoll = dfly_fuse_req_process,
	.module_cpoll = dfly_fuse_req_complete,
	.module_gpoll = dfly_fuse_generic_poll,
	.find_instance_context = fuse_find_instance_ctx,
};

int dfly_fuse_module_init(int ssid, int nr_cores, df_module_event_complete_cb cb, void *cb_arg)
{
	//assume one pool per system, ssid is not used so far. multiple (pool -> maps) TBD
	struct dfly_subsystem *ss = dfly_get_subsystem(ssid);
	ss->mlist.dfly_fuse_module = dfly_module_start("FUSE", ssid, DSS_MODULE_FUSE, &fuse_module_ops,
				     &fuse_module_ctx, nr_cores, -1, cb, cb_arg);

	return 0;
}
