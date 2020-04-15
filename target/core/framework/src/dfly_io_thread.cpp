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
#include "nvmf_internal.h"

int
_dfly_nvmf_ctrlr_process_io_cmd(struct io_thread_inst_ctx_s *thrd_inst,
				struct spdk_nvmf_request *req);
int dfly_nvme_submit_io_cmd(struct io_thread_inst_ctx_s *thrd_inst, struct dfly_request *req);
int dfly_nvmf_ctrlr_process_io_cmd(struct io_thread_inst_ctx_s *thrd_inst,
				   struct spdk_nvmf_request *req);
int dfly_io_req_process(void *ctx, struct dfly_request *req)
{
	int status = 0;
	struct io_thread_inst_ctx_s *thrd_inst = (struct io_thread_inst_ctx_s *)ctx;

	if (req->flags & DFLY_REQF_NVMF) {
		status = dfly_nvmf_ctrlr_process_io_cmd(thrd_inst, (struct spdk_nvmf_request *)req->req_ctx);
	} else if (req->flags & DFLY_REQF_NVME) {
		status = dfly_nvme_submit_io_cmd(thrd_inst, req);
		DFLY_ASSERT(status == SPDK_NVMF_REQUEST_EXEC_STATUS_ASYNCHRONOUS);
	} else {
		assert(0);
	}

	if (!(req->flags & DFLY_REQF_NVME) && (status == SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE)) {
		thrd_inst->module_ctx->io_ops->req_complete((struct spdk_nvmf_request *)req->req_ctx);
		return DFLY_MODULE_REQUEST_PROCESSED;
	} else {
		return DFLY_MODULE_REQUEST_QUEUED;
	}
}

int dfly_io_request_complete(void *ctx, struct dfly_request *req)
{
	struct io_thread_inst_ctx_s *thrd_inst = (struct io_thread_inst_ctx_s *)ctx;

	thrd_inst->module_ctx->io_ops->req_complete((struct spdk_nvmf_request *)req->req_ctx);

}

extern wal_conf_t g_wal_conf;

void dfly_nvmf_complete_event_fn(void *ctx, void *arg2)
{
	struct dfly_request *dfly_req = (struct dfly_request *) ctx;;
	struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)dfly_req->req_ctx;
	struct dfly_subsystem *ss       = dfly_get_subsystem_no_lock(dfly_req->req_ssid);
	struct dfly_io_device_s *io_dev = (struct dfly_io_device_s *)(dfly_req->io_device);

	DFLY_ASSERT(nvmf_req != NULL);
	DFLY_ASSERT(dfly_req->flags & DFLY_REQF_NVMF);
	struct spdk_nvme_cmd *cmd = &nvmf_req->cmd->nvme_cmd;

	if (g_wal_conf.wal_cache_enabled) {
		io_dev = (struct dfly_io_device_s *)dfly_kd_get_device(dfly_req);
	}

	if (cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE) {
		dfly_counters_size_count(ss->stat_kvio, nvmf_req, cmd->opc);
		dfly_counters_bandwidth_cal(ss->stat_kvio, nvmf_req, cmd->opc);
		dfly_counters_size_count(io_dev->stat_io, nvmf_req, cmd->opc);
		dfly_counters_bandwidth_cal(io_dev->stat_io, nvmf_req, cmd->opc);
	}

	if(df_qpair_susbsys_enabled(nvmf_req->qpair, nvmf_req)) {
		dfly_qos_update_nops(dfly_req, \
					dfly_req->dqpair->df_poller, \
					dfly_req->dqpair->df_ctrlr);
	}

	dfly_req_fini(dfly_req);
	dfly_nvmf_request_complete(nvmf_req);

	return;
}

void dfly_nvmf_complete(struct dfly_request *req)
{
	struct spdk_event *event;
	unsigned lcore = spdk_env_get_current_core();

	if (req->src_core != lcore) {
		event = spdk_event_allocate(req->src_core, dfly_nvmf_complete_event_fn, (void *)req, NULL);
		assert(event != NULL);
		spdk_event_call(event);
	} else {
		dfly_nvmf_complete_event_fn(req, NULL);
	}
}

void dfly_io_complete_cb(struct dfly_request *req)
{
	struct dfly_subsystem *req_ss       = dfly_get_subsystem_no_lock(req->req_ssid);
	struct io_thread_ctx_s *io_thread_ctx = NULL;

	if (req_ss->mlist.dfly_fuse_module) {
		switch (req->ops.get_command(req)) {
		case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
			req->next_action = DFLY_COMPLETE_NVMF;
			break;
		default:
			if (req->req_fuse_data || req->parent) {
				req->next_action = DFLY_COMPLETE_ON_FUSE_THREAD;
			} else {
				req->next_action = DFLY_COMPLETE_NVMF;
			}
			break;
		}
		dfly_handle_request(req);
	} else {
		io_thread_ctx = (struct io_thread_ctx_s *)dfly_module_get_ctx(req_ss->mlist.dfly_io_module);
		io_thread_ctx->io_ops->req_complete((struct spdk_nvmf_request *)req->req_ctx);
	}
}

void *dfly_io_thread_instance_init(void *mctx, void *inst_ctx, int inst_index)
{

	struct dfly_subsystem *dfly_subsystem = NULL;
	struct io_thread_inst_ctx_s *thread_instance = NULL;
	struct io_thread_ctx_s *io_mod_ctx = (struct io_thread_ctx_s *)mctx;
	int i;

	dfly_subsystem = io_mod_ctx->dfly_subsys;

	thread_instance = (struct io_thread_inst_ctx_s *)calloc(1, sizeof(struct io_thread_inst_ctx_s)  + \
			  sizeof(struct spdk_io_channel *) * io_mod_ctx->dfly_subsys->num_io_devices);

	DFLY_ASSERT(thread_instance);

	thread_instance->module_ctx = io_mod_ctx;
	thread_instance->module_inst_ctx = inst_ctx;
	thread_instance->module_inst_index = inst_index;

	for (i = 0; i < io_mod_ctx->dfly_subsys->num_io_devices; i++) {
		DFLY_ASSERT(io_mod_ctx->dfly_subsys->devices[i].index == i);
		thread_instance->io_chann_parr[i] = spdk_bdev_get_io_channel(
				io_mod_ctx->dfly_subsys->devices[i].ns->desc);
	}

	return thread_instance;
}

void *dfly_io_thread_instance_destroy(void *mctx, void *inst_ctx)
{
	struct io_thread_ctx_s *io_mod_ctx = (struct io_thread_ctx_s *)mctx;
	struct io_thread_inst_ctx_s *thread_instance = inst_ctx;
	int i;

	for (i = 0; i < io_mod_ctx->dfly_subsys->num_io_devices; i++) {
		DFLY_ASSERT(io_mod_ctx->dfly_subsys->devices[i].index == i);
		DFLY_ASSERT(thread_instance->io_chann_parr[i]);
		if (thread_instance->io_chann_parr[i]) {
			spdk_put_io_channel(thread_instance->io_chann_parr[i]);
			thread_instance->io_chann_parr[i] = NULL;
		}
	}

	return;
}

struct dfly_module_ops io_module_ops {
	.module_init_instance_context = dfly_io_thread_instance_init,
	.module_rpoll = dfly_io_req_process,
	//.module_cpoll = dfly_io_request_complete,
	.module_cpoll = NULL,
	.module_gpoll = NULL,
	.find_instance_context = NULL,
	.module_instance_destroy = dfly_io_thread_instance_destroy,
};

int
dfly_nvme_submit_io_cmd(struct io_thread_inst_ctx_s *thrd_inst, struct dfly_request *req)
{
	struct dfly_io_device_s *io_device;
	struct spdk_io_channel *ch = NULL;

	if (!req->io_device)
		io_device = (struct dfly_io_device_s *)dfly_kd_get_device(req);
	else
		io_device = req->io_device; // for iterator

	DFLY_ASSERT(io_device);

	ch   = thrd_inst->io_chann_parr[io_device->index];
	DFLY_ASSERT(ch);

	dfly_kv_submit_req(req, io_device, ch);

	return SPDK_NVMF_REQUEST_EXEC_STATUS_ASYNCHRONOUS;
}

#define PAGE_SZ_MASK (~(PAGE_SIZE -1))

void nvme_kv_cmd_setup_key(struct spdk_nvme_cmd *cmd, void *src_key, uint32_t keylen, void *dst_key)
{

	size_t req_size_padded;
	uint64_t phys_addr;

	assert(keylen <= SAMSUNG_KV_MAX_KEY_SIZE);

	if (keylen > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		//prp or sgl
		uint64_t *prp1, *prp2;
		char     *key_data;

		prp1 = (uint64_t *)&cmd->cdw12;
		prp2 = (uint64_t *)&cmd->cdw14;

		key_data = (char *)dst_key;

		key_data[255] = '\0';
		if (src_key) {
			memcpy(key_data, src_key, keylen);
		}

		phys_addr = spdk_vtophys(key_data, NULL);
		if (phys_addr == SPDK_VTOPHYS_ERROR) {
			SPDK_ERRLOG("vtophys(%p) failed\n", key_data);
			assert(0);
		}

		*prp1 = phys_addr;

		if (((uint64_t)(phys_addr + keylen - 1)  & PAGE_SZ_MASK) !=
		    (((uint64_t)phys_addr & PAGE_SZ_MASK))) {
			*prp2 = ((uint64_t)(phys_addr + keylen - 1) & PAGE_SZ_MASK);
			SPDK_WARNLOG("key split across two prp PRP1:%p PRP2:%p \n", *prp1, *prp2);
			assert(0);
		} else {
			*prp2 = NULL;
		}
	} else {
		if (src_key) {
			memcpy(&cmd->cdw12, src_key, keylen);
		}
	}
}

struct spdk_nvme_cmd *dfly_nvmf_setup_cmd_key(struct spdk_nvmf_request *req,
		struct spdk_nvme_cmd *in_cmd)
{
	struct spdk_nvme_cmd *cmd;
	uint32_t key_len;

	switch (req->cmd->nvme_cmd.opc) {
	case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
	case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
	case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
		key_len = (req->cmd->nvme_cmd.cdw11 & 0xFF) + 1;
		cmd = in_cmd;
		*in_cmd = req->cmd->nvme_cmd;
		nvme_kv_cmd_setup_key(cmd, NULL, key_len, req->dreq->key_data_buf);
		break;
	default:
		cmd = &req->cmd->nvme_cmd;
		break;
	}
	return cmd;
}

int
dfly_nvmf_ctrlr_process_io_cmd(struct io_thread_inst_ctx_s *thrd_inst,
			       struct spdk_nvmf_request *req)
{
	return _dfly_nvmf_ctrlr_process_io_cmd(thrd_inst, req);
}

int dfly_io_module_init_spdk_devices(struct dfly_subsystem *subsystem,
				     struct spdk_nvmf_subsystem *nvmf_subsys)
{
	int i;

	if(subsystem->devices) {
		free(subsystem->devices);
		subsystem->devices = NULL;
		subsystem->num_io_devices = 0;
	}

	subsystem->devices = (struct dfly_io_device_s *)calloc(nvmf_subsys->max_nsid,
			     sizeof(struct dfly_io_device_s));
	subsystem->num_io_devices = nvmf_subsys->max_nsid;

	DFLY_ASSERT(subsystem->devices);

	if (dfly_init_kd_context(subsystem->id, DFLY_KD_RH_MURMUR3)) {
		DFLY_ERRLOG("DFLY Key Distribution init failed\n");
		return -1;
	}

	for (i = 0; i < nvmf_subsys->max_nsid; i++) {
		const char *bdev_name = spdk_bdev_get_name(nvmf_subsys->ns[i]->bdev);
		subsystem->devices[i].ns = nvmf_subsys->ns[i];
		subsystem->devices[i].index = i;

		dfly_ustat_init_dev_stat(subsystem->id, bdev_name, &subsystem->devices[i]);
		dfly_kd_add_device(subsystem->id, bdev_name, strlen(bdev_name), &subsystem->devices[i]);
	}

	return 0;
}

int dfly_io_module_deinit_spdk_devices(struct dfly_subsystem *subsystem)
{
	int i;

	dfly_deinit_kd_context(subsystem->id, DFLY_KD_RH_MURMUR3);

	for (i = 0; i < subsystem->num_io_devices; i++) {
		dfly_ustat_remove_dev_stat(&subsystem->devices[i]);
	}

	free(subsystem->devices);
	subsystem->devices = NULL;
	subsystem->num_io_devices = 0;

}

int dfly_io_module_subsystem_start(struct dfly_subsystem *subsystem,
				   dfly_spdk_nvmf_io_ops_t *io_ops, df_module_event_complete_cb cb, void *cb_arg)
{

	int rc = 0;
	struct io_thread_ctx_s *io_thrd_ctx;

	io_thrd_ctx = (struct io_thread_ctx_s *)calloc(1, sizeof(struct io_thread_ctx_s));
	assert(io_thrd_ctx);
	if (io_thrd_ctx == NULL) {
		return -1;
	}

	io_thrd_ctx->dfly_subsys = subsystem;
	io_thrd_ctx->io_ops = io_ops;

	rc = dfly_io_module_init_spdk_devices(subsystem,
					      (struct spdk_nvmf_subsystem *)subsystem->parent_ctx);
	if (rc) {
		DFLY_ASSERT(0);
	}

	subsystem->mlist.dfly_io_module = dfly_module_start("DFLY_IO", subsystem->id, &io_module_ops,
					  io_thrd_ctx, 4, cb, cb_arg);

	return 0;
}

void _dfly_io_module_stop(void *event, void *dummy)
{
	struct df_ss_cb_event_s *io_mod_cb_event = event;

	struct dfly_subsystem *subsystem = io_mod_cb_event->ss;

	subsystem->mlist.dfly_io_module = NULL;

	dfly_io_module_deinit_spdk_devices(subsystem);

	df_ss_cb_event_complete(io_mod_cb_event);
	return;
}

void dfly_io_module_subsystem_stop(struct dfly_subsystem *subsystem, void *args/*Not Used*/,
					df_module_event_complete_cb cb, void *cb_arg)
{

	struct df_ss_cb_event_s *io_mod_cb_event = df_ss_cb_event_allocate(subsystem, cb, cb_arg, args);
	dfly_module_stop(subsystem->mlist.dfly_io_module, _dfly_io_module_stop, io_mod_cb_event, NULL);

}
