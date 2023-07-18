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
#include <map>

void dss_req_set_from_nvme_cmd(struct dfly_request *req)
{
	struct spdk_nvme_cmd *cmd;
	if (req->flags & DFLY_REQF_NVME) {
		cmd = (struct spdk_nvme_cmd *)req->req_ctx;
	} else {
		struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)req->req_ctx;
		cmd = &((*nvmf_req->cmd).nvme_cmd);
	}

	req->nvme_opcode = cmd->opc;

	return;
}

int dfly_req_fini(struct dfly_request *req)
{

	void *key_data_buf = req->key_data_buf;//Still valid after fini
	struct dfly_subsystem *ss = dfly_get_subsystem_no_lock(req->req_ssid);

	if(req->dqpair) {
		dfly_ustat_update_rqpair_stat(req->dqpair ,1);
        TAILQ_REMOVE(&req->dqpair->qp_outstanding_reqs, req, outstanding);
	}

	if(ss->initialized == true) {
		dfly_ustat_atomic_dec_u64(ss->stat_kvio, &ss->stat_kvio->i_pending_reqs);
	}

	if (req->req_fuse_data) {
		dfly_fuse_release(req);
	}

	if(g_dragonfly->enable_latency_profiling) {
		df_update_lat_us(req);
	}

	memset(req, 0, sizeof(dfly_request_t));

	req->src_core = req->tgt_core = -1;
	req->key_data_buf = key_data_buf;//restore key_data_buf

#ifdef WAL_DFLY_TRACK
	req->cache_rc = -1;
	req->log_rc = -1;
	req->zone_flush_flags = -1;
#endif

	req->data_direct = false;

	return 0;
}

void dfly_req_init_nvme_info(struct dfly_request *req)
{
	//req_ctx should be NVME Command
	struct spdk_nvme_cmd *cmd = (struct spdk_nvme_cmd *)req->req_ctx;

	//req->flags |= DFLY_REQF_NVME;

	req->req_key.key = &cmd->cdw12; //Key
	//req->req_key.length = (cmd->cdw11 + 1) & 0xFF; //Key length

	//Already populated//req->req_value.length = (cmd->cdw10 << 2);//Value length

	//Already polulated//req->req_value.value = *((void *)(&cmd->dptr.prp1));//Value
	req->req_value.offset = cmd->mptr >> 2;//Value offset

	//req_ssid needs to be valid
	req->req_dfly_ss = dfly_get_subsystem_no_lock(req->req_ssid);

#if 0
	assert(req->req_key.length == 16);
#endif

	//Do after other init
	dss_req_set_from_nvme_cmd(req);

	return;
}

std::map<std::string, uint8_t> pref2hd;


struct kv_cdw11 {
	uint8_t cdwb1;
	uint8_t cdwb2;
	uint8_t inval_bytes:2;
	uint8_t rsvd:6;
	uint8_t cdwb4;
};
extern uint8_t __dfly_iter_next_handle ;
void dfly_req_init_nvmf_info(struct dfly_request *req)
{
	struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)req->req_ctx;
	struct spdk_nvme_cmd *cmd = &((*nvmf_req->cmd).nvme_cmd);

	struct kv_cdw11 *cdw11;

	req->req_key.key = &cmd->cdw12; //Key
	//req->req_key.length = (cmd->cdw11 + 1) & 0xFF; //Key length
	if (cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_LIST_READ) {
		req->list_data.max_keys_requested = cmd->cdw11 >> 16;
		req->list_data.start_key_offset = (cmd->cdw11 >> 8) & 0xFF;
		req->list_data.list_size = (cmd->cdw10 << 2);
		req->list_data.options = (cmd->rsvd2 & 0x3);
	}

	cdw11 =(struct kv_cdw11 *)&cmd->cdw11;
	req->req_value.length = (cmd->cdw10 << 2);//Value length
	req->req_value.length -= cdw11->inval_bytes;
	req->req_value.value = nvmf_req->data;//Value
	req->req_value.offset = cmd->mptr >> 2;//Value offset

	req->req_dfly_ss = dfly_get_subsystem_no_lock(nvmf_req->qpair->ctrlr->subsys->id);

	DFLY_ASSERT(nvmf_req->qpair->ctrlr->subsys->id == req->req_dfly_ss->id);

	dfly_ustat_update_rqpair_stat(nvmf_req->qpair->dqpair, 0);

	//Do after other init
	dss_req_set_from_nvme_cmd(req);

	return;
}

void dfly_req_init_nvmf_value(struct dfly_request *req)
{
	struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)req->req_ctx;
	struct spdk_nvme_cmd *cmd = &((*nvmf_req->cmd).nvme_cmd);
	struct kv_cdw11 *cdw11;

	int io_dev_arr_index;

	struct dfly_subsystem *ss = dfly_get_subsystem_no_lock(req->req_ssid);

	req->req_value.value = nvmf_req->data;//Value

	//TODO: Deprecate dfly_request io_device
	if(ss->dss_kv_mode) {
		cdw11 =(struct kv_cdw11*)&cmd->cdw11;
		req->req_value.length = (cmd->cdw10 << 2);//Value length
		req->req_value.length -= cdw11->inval_bytes;

		req->req_value.offset = cmd->mptr >> 2;//Value offset

		dfly_qp_counters_inc_io_count(nvmf_req->qpair->dqpair->stat_qpair, cmd->opc);
		dfly_counters_increment_io_count(ss->stat_kvio, cmd->opc);
		if (ss->initialized == true)
		{
			dfly_ustat_atomic_inc_u64(ss->stat_kvio, &ss->stat_kvio->i_pending_reqs);
		}
		if (cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE)
		{
			dfly_counters_size_count(ss->stat_kvio, nvmf_req, cmd->opc);
			dfly_counters_bandwidth_cal(ss->stat_kvio, nvmf_req, cmd->opc);
		}

		req->io_device = (struct dfly_io_device_s *)dfly_kd_get_device(req);
		DFLY_ASSERT(req->io_device);
	}

	if(ss->dev_arr && (ss->num_io_devices > 0)) {
		if(ss->dss_kv_mode) {
			io_dev_arr_index = dfly_kd_get_device_index(req);
			DSS_ASSERT(io_dev_arr_index > 0);
			DSS_ASSERT(io_dev_arr_index  < ss->num_io_devices);
			req->common_req.io_device = ss->dev_arr[io_dev_arr_index];
			req->common_req.io_device_index = io_dev_arr_index;
		} else {//Block mode passthrough nsid
			uint32_t nsid;
			nsid = cmd->nsid;
			DSS_ASSERT(nsid != 0);
			DSS_ASSERT(nsid <= ss->num_io_devices);
			req->common_req.io_device = ss->dev_arr[nsid - 1];
			req->common_req.io_device_index = nsid-1;
		}
	}

	return;
}

struct spdk_nvme_cmd *dfly_get_nvme_cmd(struct dfly_request *req)
{
	struct spdk_nvme_cmd *cmd;
	if (req->flags & DFLY_REQF_NVME) {
		cmd = (struct spdk_nvme_cmd *)req->req_ctx;
	} else {
		struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)req->req_ctx;
		cmd = &((*nvmf_req->cmd).nvme_cmd);
	}

	return cmd;
}

/**
* get command for the given target
*/
uint32_t dfly_req_get_command(struct dfly_request *req)
{
	struct spdk_nvme_cmd *cmd = dfly_get_nvme_cmd(req);

	if (cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE && cmd->fuse == 0x2) {
		return DFLY_REQ_OPC_STORE_FUSE2;
	} else if (cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE && cmd->fuse == 0x2) {
		return DFLY_REQ_OPC_DELETE_FUSE2;
	}
	return cmd->opc;

}

uint32_t dfly_req_fuse_2_delete(struct dfly_request *req)
{
	struct spdk_nvme_cmd *cmd = dfly_get_nvme_cmd(req);
	if (cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE && cmd->fuse == 0x2)
		return 1;
	else
		return 0;
}

/**
* get key for the given target
*/
struct dfly_key *dfly_req_get_key(struct dfly_request *req)
{
	return &req->req_key;
}

/**---------- For consumer WAL/Cache/Flush calls --------------**/

/**
* get value for the given target
*/
struct dfly_value *dfly_req_get_value(struct dfly_request *req)
{
	return &req->req_value;
}

int dfly_req_validate(struct dfly_request *req)
{
	if (!req || !req->ops.get_command || req->state == DFLY_REQ_BEGIN)
		return 0;

	struct spdk_nvme_cmd *cmd = dfly_get_nvme_cmd(req);
	return (cmd != NULL);
}

void dfly_resp_set_cdw0(struct dfly_request *req, uint32_t value)
{

	assert(req);
	struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)req->req_ctx;
	assert(nvmf_req);
	struct spdk_nvme_cpl *rsp = &nvmf_req->rsp->nvme_cpl;

	rsp->cdw0 = value;

	return;

}

uint32_t dfly_resp_get_cdw0(struct dfly_request *req)
{

	assert(req);
	struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)req->req_ctx;
	assert(nvmf_req);
	struct spdk_nvme_cpl *rsp = &nvmf_req->rsp->nvme_cpl;

	return rsp->cdw0;
}

//Only for NVMF request
bool dfly_req_4m_same_session(struct dfly_request *req1, struct dfly_request *req2)
{
	struct spdk_nvmf_request *nvmf1, *nvmf2;
	if ((req1->flags & DFLY_REQF_NVMF) &&
	    (req2->flags & DFLY_REQF_NVMF)) {
		nvmf1 = (struct spdk_nvmf_request *)req1->req_ctx;
		nvmf2 = (struct spdk_nvmf_request *)req2->req_ctx;

		assert(nvmf1);
		assert(nvmf2);

		if (nvmf1->qpair == nvmf2->qpair) {
			return true;
		}
	}
	return false;
}

//Only for NVMF request
bool dfly_cmd_sequential(struct dfly_request *req_first, struct dfly_request *req_next)
{

	struct spdk_nvmf_request *nvme_first, *nvme_next;
	uint16_t cid1, cid2, max_cid;

	if ((req_first->flags & DFLY_REQF_NVMF) &&
	    (req_next->flags & DFLY_REQF_NVMF)) {
		if (dfly_req_4m_same_session(req_first, req_next)) {
			if (req_next->io_seq_no == req_first->io_seq_no + 1) {
				return true;
			}
		}
	}
	return false;
}

struct dfly_request_ops df_req_ops_inst = {
	.get_command = dfly_req_get_command,
	.get_key = dfly_req_get_key,
	.get_value = dfly_req_get_value,
	.validate_req = dfly_req_validate,
};

int dfly_req_ini(struct dfly_request *req, int flags, void *ctx)
{
	assert(req);
	assert(ctx);

	req->req_ctx = ctx; /*parent nvme or rdma request*/
	req->flags = flags;

	req->src_core = req->tgt_core = spdk_env_get_current_core();

	req->src_nsid = req->tgt_nsid;

	req->xfed_len = 0; /* xfed len so far */

	req->parent = NULL; /*parent request incase of splitting*/

	req->dev = NULL;

	req->ops = df_req_ops_inst;

	return 0;
}

void dfly_nvmf_req_init(struct spdk_nvmf_request *req)
{
    struct dfly_subsystem *df_ss;

	if (req->qpair->ctrlr &&
	    req->qpair->ctrlr->subsys->oss_target_enabled == OSS_TARGET_ENABLED) {

        df_ss = dfly_get_subsystem(req->qpair->ctrlr->subsys->id);

		if(df_ss->mlist.dss_net_module && !req->qpair->dqpair->net_module_instance) {
			/// @note Ideally this should be done after qpair initialization. Adding here since SPDK qpair init does not populate required fields until much later
			dss_qpair_set_net_module_instance(req->qpair);
		}

		if ((req->cmd->nvmf_cmd.opcode != SPDK_NVME_OPC_FABRIC) &&
		    (nvmf_qpair_is_admin_queue(req->qpair) == false)) {

			dfly_req_ini(req->dreq, DFLY_REQF_NVMF | DFLY_REQF_DATA, (void *)req);
			req->dreq->req_ssid = req->qpair->ctrlr->subsys->id;

			req->dreq->io_seq_no = req->qpair->dqpair->io_counter++;
			req->dreq->abort_cmd = false;
			req->dreq->dqpair = req->qpair->dqpair;

            req->dreq->submit_tick = spdk_get_ticks();
            if(req->dreq->dqpair) {
    	        TAILQ_INSERT_TAIL(&req->qpair->dqpair->qp_outstanding_reqs, req->dreq, outstanding);
            }
		}
	}
}

void dfly_set_status_code(struct dfly_request *req, int sct, int sc)
{
	struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)req->req_ctx;
	struct spdk_nvme_cpl *rsp = &nvmf_req->rsp->nvme_cpl;

	rsp->status.sct = sct;
	rsp->status.sc = sc;
	//printf("dfly_set_status_code sct=%x, sc=%x\n", sct, sc);
}

int dfly_nvmf_qpair_init_request()
{
	struct dfly_request *dfly_req_arr = NULL;

}

int dfly_nvmf_qpair_deinit_requests(void *req_arr)
{
	free(req_arr);
	//dfly_fuse_release(req);
	//req->req_ss->nvmf_complete_cb((struct spdk_nvmf_request *)req->req_ctx);
}

void dss_set_rdd_transfer(struct dfly_request *req)
{
	req->data_direct = true;
}

uint32_t dss_req_get_val_len(dss_request_t *req)
{
	struct dfly_request *dreq = (struct dfly_request *) req;
	return dreq->req_value.length;
}

dss_key_t *dss_req_get_key(dss_request_t *req)
{
	struct dfly_request *dreq = (struct dfly_request *) req;
	return &dreq->req_key;
}

dss_value_t *dss_req_get_value(dss_request_t *req)
{
	struct dfly_request *dreq = (struct dfly_request *) req;
	return &dreq->req_value;
}

dss_subsystem_t *dss_req_get_subsystem(dss_request_t *req)
{
	struct dfly_request *dreq = (struct dfly_request *) req;
	DSS_ASSERT(dreq->req_dfly_ss);
	return (dss_subsystem_t *)dreq->req_dfly_ss;
}


