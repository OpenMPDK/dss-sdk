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
#include "nvmf_internal.h"

#ifdef DSS_ENABLE_ROCKSDB_KV
#include "rocksdb/dss_kv2blk_c.h"
#endif//#ifdef DSS_ENABLE_ROCKSDB_KV

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

struct spdk_nvme_cmd *dfly_nvmf_setup_cmd_key(struct spdk_nvmf_request *req,
		struct spdk_nvme_cmd *in_cmd);

void
dfly_nvmf_bdev_ctrlr_complete_cmd(struct spdk_bdev_io *bdev_io, bool success,
				  void *cb_arg)
{
	struct spdk_nvmf_request	*req = (struct spdk_nvmf_request *)cb_arg;
	struct spdk_nvme_cpl		*response = &req->rsp->nvme_cpl;
	int				sc, sct;
	uint32_t        cdw0;

	struct dfly_request *dreq;
	struct dfly_io_device_s *io_device;

	spdk_bdev_io_get_nvme_status(bdev_io, &cdw0, &sct, &sc);
	response->status.sc = sc;
	response->status.sct = sct;
	response->cdw0 = cdw0;


	dreq = req->dreq;
	assert(dreq != NULL);

	assert(dreq->state == DFLY_REQ_IO_SUBMITTED_TO_DEVICE);

	io_device = (struct dfly_io_device_s *) req->dreq->io_device;
	DFLY_ASSERT(io_device);

	dfly_ustat_atomic_dec_u64(io_device->stat_io, &io_device->stat_io->i_pending_reqs);

	dreq->status = success;

	dreq->state = DFLY_REQ_IO_COMPLETED_FROM_DEVICE;
	dreq->next_action = DFLY_COMPLETE_IO;

	spdk_bdev_free_io(bdev_io);
	dfly_handle_request(dreq);

	return;
}

int
_dfly_nvmf_ctrlr_process_io_cmd(struct io_thread_inst_ctx_s *thrd_inst,
				struct spdk_nvmf_request *req)
{
	uint32_t nsid;
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *desc;
	struct spdk_io_channel *ch;
	struct spdk_nvmf_poll_group *group = req->qpair->group;
	struct spdk_nvmf_ctrlr *ctrlr = req->qpair->ctrlr;
	struct spdk_nvme_cmd *cmd = NULL;
	struct spdk_nvme_cmd tmp_cmd;
	struct spdk_nvme_cpl *response = &req->rsp->nvme_cpl;
	struct spdk_nvmf_subsystem  *subsys = ctrlr->subsys;
	struct dfly_io_device_s *io_device;
	int rc;

	if(!g_dragonfly->blk_map) {
		cmd = dfly_nvmf_setup_cmd_key(req, &tmp_cmd);
		assert(cmd);
	} else {
		cmd = &req->cmd->nvme_cmd;
	}

	/* pre-set response details for this command */
	response->status.sc = SPDK_NVME_SC_SUCCESS;
	nsid = cmd->nsid;

	if (spdk_unlikely(ctrlr == NULL)) {
		SPDK_ERRLOG("I/O command sent before CONNECT\n");
		response->status.sct = SPDK_NVME_SCT_GENERIC;
		response->status.sc = SPDK_NVME_SC_COMMAND_SEQUENCE_ERROR;
		return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
	}

	if (spdk_unlikely(ctrlr->vcprop.cc.bits.en != 1)) {
		SPDK_ERRLOG("I/O command sent to disabled controller\n");
		response->status.sct = SPDK_NVME_SCT_GENERIC;
		response->status.sc = SPDK_NVME_SC_COMMAND_SEQUENCE_ERROR;
		return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
	}

	io_device = (struct dfly_io_device_s *) req->dreq->io_device;
	DFLY_ASSERT(io_device);

	bdev = io_device->ns->bdev;
	desc = io_device->ns->desc;
	ch   = thrd_inst->io_chann_parr[io_device->index];

	dfly_counters_increment_io_count(io_device->stat_io, cmd->opc);
	if (cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE) {
		dfly_counters_size_count(io_device->stat_io, req, cmd->opc);
		dfly_counters_bandwidth_cal(io_device->stat_io, req, cmd->opc);
	}
	dfly_ustat_atomic_inc_u64(io_device->stat_io, &io_device->stat_io->i_pending_reqs);

	if(df_subsystem_enabled(subsys->id) &&
		g_dragonfly->blk_map) {//Rocksdb block trannslation
#ifdef DSS_ENABLE_ROCKSDB_KV
		if(spdk_unlikely(g_dragonfly->rdb_sim_timeout)) {
				usleep(g_dragonfly->rdb_sim_timeout * 1000000);
		}
		switch (cmd->opc) {
			case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
				//SPDK_NOTICELOG("Rocksdb put started\n");
				if(spdk_unlikely(g_dragonfly->rdb_sim_io_pre_timeout)) {
						usleep(g_dragonfly->rdb_sim_io_pre_timeout * 1000000);
				}
				rc = dss_rocksdb_put(io_device->rdb_handle->rdb_db_handle,
										req->dreq->req_key.key, req->dreq->req_key.length,
										req->dreq->req_value.value, req->dreq->req_value.length);
				if(spdk_unlikely(g_dragonfly->rdb_sim_io_post_timeout)) {
						usleep(g_dragonfly->rdb_sim_io_post_timeout * 1000000);
				}
				if(rc == -1) {
					SPDK_ERRLOG("Rocksdb put failed\n");
					req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
					req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
				} else {
					DFLY_ASSERT(rc == 0);
					//SPDK_NOTICELOG("Rocksdb put completed\n");
					req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
					req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_SUCCESS;
				}
				return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
				break;
			case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
				//SPDK_NOTICELOG("Rocksdb get started\n");
				dss_rocksdb_get_async(thrd_inst, io_device->rdb_handle->rdb_db_handle,
										req->dreq->req_key.key, req->dreq->req_key.length,
										req->dreq->req_value.value, /*req->dreq->req_value.length*/ req->qpair->transport->opts.io_unit_size + VALUE_4KB,
										NULL, req);
				return SPDK_NVMF_REQUEST_EXEC_STATUS_ASYNCHRONOUS;
				break;
			case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
				rc = dss_rocksdb_delete(io_device->rdb_handle->rdb_db_handle,
										req->dreq->req_key.key,
										req->dreq->req_key.length);
				if(rc == -1) {
					SPDK_ERRLOG("Rocksdb delete failed\n");
					req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
					req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
				} else {
					DFLY_ASSERT(rc == 0);
					//SPDK_NOTICELOG("Rocksdb delete completed\n");
					req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
					req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_SUCCESS;
				}
				return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
				break;
			case SPDK_NVME_OPC_READ:
				req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
				req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_SUCCESS;
				return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
			default:
				DFLY_ERRLOG("Failing request with opcode %d\n", cmd->opc);
				req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
				req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INVALID_OPCODE;
				return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
				break;
		}
		DFLY_ASSERT(0);
#else
		DFLY_ASSERT(0);
#endif//DSS_ENABLE_ROCKSDB_KV
	}
	switch (cmd->opc) {
	case SPDK_NVME_OPC_READ:
	case SPDK_NVME_OPC_WRITE:
	case SPDK_NVME_OPC_WRITE_ZEROES:
	case SPDK_NVME_OPC_FLUSH:
	case SPDK_NVME_OPC_DATASET_MANAGEMENT:
	//assert(0);//Non Key value command
	//break;
	default:
		rc = spdk_bdev_nvme_io_passthru(desc, ch, cmd, req->data, req->length,
						dfly_nvmf_bdev_ctrlr_complete_cmd, req);
		if (spdk_unlikely(rc)) {
			if (rc == -ENOMEM) {
				DFLY_ERRLOG("submit io failed with no mem\n");
				req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
				req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
			} else {
				req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
				req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INVALID_OPCODE;
			}
			return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
		}
	}

	return SPDK_NVMF_REQUEST_EXEC_STATUS_ASYNCHRONOUS;
}
