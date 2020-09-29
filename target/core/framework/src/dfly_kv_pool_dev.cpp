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

#include "nvmf_internal.h"
#include "spdk/nvme_samsung_spec.h"

#include "dragonfly.h"
#include <sched.h>

//extern struct nvmf_tgt g_tgt;

extern struct df_device dfly_kv_pool_device;

typedef struct dfly_kv_pool_s {
	struct df_device df_dev;
	int df_ssid;
	int id;
	uint64_t capacity;
} dfly_kv_pool_t;

void dfly_kv_pool_set_handle(void *vctx, int handle)
{
	dfly_kv_pool_t *ctx = (dfly_kv_pool_t *)vctx;

	ctx->id = handle;

	return;
}

#define PAGE_SZ_MASK (~(PAGE_SIZE -1))

static void _nvme_kv_cmd_setup_large_key(struct spdk_nvme_cmd *cmd, void *src_key, uint32_t keylen,
		void *dst_key)
{

	size_t req_size_padded;
	uint64_t phys_addr;

	assert(keylen <= SAMSUNG_KV_MAX_KEY_SIZE);

	if (keylen > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		//prp or sgl
		uint64_t *prp1, *prp2;
		char     *key_data;

		prp1 = (uint64_t *) & (cmd->cdw12);
		prp2 = (uint64_t *) & (cmd->cdw14);

		key_data = (char *)dst_key;

		key_data[255] = '\0';
		if (src_key) {
			memcpy(key_data, src_key, keylen);
		}

		phys_addr = spdk_vtophys(key_data, NULL);
		if (phys_addr == SPDK_VTOPHYS_ERROR) {
			printf("vtophys(%p) failed\n", key_data);
			assert(0);
		}

		*prp1 = phys_addr;

		if (((uint64_t)(phys_addr + keylen - 1)  & PAGE_SZ_MASK) !=
		    (((uint64_t)phys_addr & PAGE_SZ_MASK))) {
			*prp2 = ((uint64_t)(phys_addr + keylen - 1) & PAGE_SZ_MASK);
			printf("key split across two prp PRP1:%p PRP2:%p \n", *prp1, *prp2);
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


dfly_iterator_info *dfly_iter_info_setup(struct df_device *device,
		struct dfly_request *req)
{
	dfly_kv_pool_t *kv_pool_dev_ctx = (dfly_kv_pool_t *)device;
	struct spdk_nvme_cmd *cmd = dfly_get_nvme_cmd(req);
	dfly_iterator_info *iter_info = NULL;
	struct dword_bytes *cdw11 = NULL;
	assert(cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL);
	cdw11 = (struct dword_bytes *)&cmd->cdw11;

	//struct nvme_passthru_kv_cmd * nvme_pth_cmd = (struct nvme_passthru_kv_cmd *)cmd;
	//assert(nvme_pth_cmd->opcode == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL);

	iter_info = (dfly_iterator_info *)calloc(1, sizeof(dfly_iterator_info));
	iter_info->type = cdw11->cdwb2;
	iter_info->bitmask = cmd->cdw13;
	iter_info->bit_pattern = cmd->cdw12;

	iter_info->dfly_req = req;
	iter_info->is_eof = 0;
	iter_info->status = 0;
	iter_info->status = KVS_ITERATOR_STATUS_UNKNOWN;

	uint32_t nr_dev_per_pool = MAX_DEV_PER_POOL;
	void *dev_list_for_iter[MAX_DEV_PER_POOL];
	dfly_list_device(kv_pool_dev_ctx->df_ssid, dev_list_for_iter, &nr_dev_per_pool);

	for (int i = 0; i < nr_dev_per_pool; i++) {
		dev_iterator_info *dev_iter_info = &iter_info->dev_iter_info[i];
		dev_iter_info->io_device = dev_list_for_iter[i];
		dev_iter_info->dev_pool_index = i;
		dev_iter_info->iter_ctx = iter_info;

		//prepare the iter list buffer
		dev_iter_info->it_buff = spdk_dma_malloc(32 * KB, KB, NULL);
		dev_iter_info->is_eof = 0;
		dev_iter_info->it_data_sz = 0;
		dev_iter_info->it_buff_cap = 32 * KB;
		dev_iter_info->read_pos = 0;
	}

	iter_info->nr_dev = nr_dev_per_pool;

	return iter_info;
}

static void
nvme_kv_cmd_setup_bdev_iter_ctrl_request(struct spdk_nvme_cmd *cmd,
		uint32_t nsid,  dfly_iterator_info *iter_info, uint8_t dev_iter_handle)
{
	struct dword_bytes *cdw11, *cdw12, *cdw13;
	//printf("iter_info->type 0x%x handle 0x%x\n", iter_info->type, dev_iter_handle);
	cmd->opc = SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL;
	cdw11 = (struct dword_bytes *)&cmd->cdw11;
	cdw11->cdwb2 = iter_info->type; //cdwb3 and cdwd4 are reserved
	// cdwb1 for close handle
	if (iter_info->type & NVME_OPC_SAMSUNG_KV_ITERATE_OPTION_OPEN) {
		cmd->cdw12 = iter_info->bit_pattern;
		cmd->cdw13 = iter_info->bitmask;
	} else if (iter_info->type & NVME_OPC_SAMSUNG_KV_ITERATE_OPTION_CLOSE) {
		cdw11->cdwb1 = dev_iter_handle;
	} else {
		assert(0);
	}

	cmd->cdw10 = 0;
	cmd->nsid = nsid;

}

static void
nvme_kv_cmd_setup_bdev_iter_read_request(struct spdk_nvme_cmd *cmd,
		uint32_t nsid,  dev_iterator_info *dev_iter_info, int buffer_size)
{
	struct dword_bytes *cdw11, *cdw12, *cdw13;

	//printf("de_iter_info->iter_handle 0x%x\n", dev_iter_info->iter_handle);

	cmd->opc = SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ;
	cdw11 = (struct dword_bytes *)&cmd->cdw11;
	cdw11->cdwb1 = dev_iter_info->iter_handle;

	cmd->cdw10 = (buffer_size >> 2);
	cmd->nsid = nsid;

}

static void
nvme_kv_cmd_setup_bdev_store_request(struct spdk_nvme_cmd *cmd,
				     uint32_t nsid,
				     void *key, uint32_t key_size,
				     void *buffer, uint32_t buffer_size,
				     uint32_t offset,
				     uint32_t io_flags, uint32_t option)
{
	struct dword_bytes *cdw11;

	cmd->opc = SPDK_NVME_OPC_SAMSUNG_KV_STORE;

	cdw11 = (struct dword_bytes *)&cmd->cdw11;

	cdw11->cdwb1 = key_size - 1;
	cdw11->cdwb2 = option;

	cmd->cdw10 = (buffer_size >> 2);

	cmd->nsid = nsid;

	//
	// Filling key value into (cdw10-13) in case key size is small than 16.
	// If md is set, lower layer (nvme_pcie_qpair_build_contig_request())
	// will prepare PRP and fill into (cdw10-11).
	//

	if (key_size > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		//printf("Unsupported store key size:%d\n", key_size);
		_nvme_kv_cmd_setup_large_key(cmd, NULL, key_size, key);
	} else {
		memcpy(&cmd->cdw12, key, key_size);
	}

	// cdw5:
	//    [2:31] The key_size(offset) of value  in bytes
	//    [0:1] Option
	// MPTR: CDW4-5
	//    To minimize the modification to original SPDK code, still use mptr here
	cmd->mptr = (uint64_t)(offset << 2);
}

static void
nvme_kv_cmd_setup_bdev_retrieve_request(struct spdk_nvme_cmd *cmd,
					uint32_t nsid,
					void *key, uint32_t key_size,
					void *buffer, uint32_t buffer_size,
					uint32_t offset,
					uint32_t io_flags, uint32_t option)
{
	struct dword_bytes *cdw11;

	cmd->opc = SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE;

	cdw11 = (struct dword_bytes *)&cmd->cdw11;

	cdw11->cdwb1 = key_size - 1;
	cdw11->cdwb2 = option;

	cmd->cdw10 = (buffer_size >> 2);

	cmd->nsid = nsid;


	//
	// Filling key value into (cdw10-13) in case key size is small than 16.
	// If md is set, lower layer (nvme_pcie_qpair_build_contig_request())
	// will prepare PRP and fill into (cdw10-11).
	//
	if (key_size > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		//printf("Unsupported retrieve key size:%d\n", key_size);
		_nvme_kv_cmd_setup_large_key(cmd, NULL, key_size, key);
	} else {
		memcpy(&cmd->cdw12, key, key_size);
	}


	// cdw5:
	//    [2:31] The offset of value  in bytes
	//    [0:1] Option
	// MPTR: CDW4-5
	//    To minimize the modification to original SPDK code, still use mptr here
	cmd->mptr = (((uint64_t)offset) << 32);

}

static void
nvme_kv_cmd_setup_bdev_delete_request(struct spdk_nvme_cmd *cmd,
				      uint32_t nsid,
				      void *key, uint32_t key_size,
				      uint32_t io_flags, uint32_t option)
{
	struct dword_bytes *cdw11;

	cmd->opc = SPDK_NVME_OPC_SAMSUNG_KV_DELETE;

	cmd->nsid = nsid;

	cdw11 = (struct dword_bytes *)&cmd->cdw11;

	cdw11->cdwb1 = key_size - 1;
	cdw11->cdwb2 = option;

	cmd->cdw10 = 0;

	//
	// Filling key value into cdw10-13 in case key size is small than 16.
	// If md is set, lower layer (nvme_pcie_qpair_build_contig_request())
	// will prepare PRP and fill into (cdw10-11).
	//
	if (key_size > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		//printf("Unsupported delete key size:%d\n", key_size);
		_nvme_kv_cmd_setup_large_key(cmd, NULL, key_size, key);
	} else {
		memcpy(&cmd->cdw12, key, key_size);
	}
}

void dfly_set_kv_pool_handle(void *vctx, int handle)
{
	dfly_kv_pool_t *ctx = (dfly_kv_pool_t *)vctx;

	ctx->id = handle;

	return;
}

struct df_device *dfly_kv_pool_open(const char *path, bool write)
{
	dfly_kv_pool_t *kv_pool_ctx = NULL;

	struct spdk_nvmf_subsystem *subsystem = NULL;

	DFLY_ASSERT(write == true);

	kv_pool_ctx = (dfly_kv_pool_t *)calloc(1, sizeof(dfly_kv_pool_t));
	if (!kv_pool_ctx) {
		return NULL;
	}

	subsystem = spdk_nvmf_subsystem_get_first(dfly_get_g_nvmf_tgt());

	while (subsystem) {
		if (strncmp(subsystem->subnqn, path, SPDK_NVMF_NQN_MAX_LEN) == 0) {

			kv_pool_ctx->df_ssid = subsystem->id;
			break;
		}
		subsystem = spdk_nvmf_subsystem_get_next(subsystem);
	}

	if (subsystem) {
		kv_pool_ctx->df_dev = dfly_kv_pool_device;
		//TODO: Get capacity
	} else {
		free(kv_pool_ctx);
		return NULL;
	}

	return &kv_pool_ctx->df_dev;
}

void dfly_kv_pool_close(struct df_device *device)
{
	dfly_kv_pool_t *kv_pool_ctx = (dfly_kv_pool_t *)device;

	memset(kv_pool_ctx, 0, sizeof(dfly_kv_pool_t));

	free(kv_pool_ctx);
}

static void
dfly_kv_pool_io_completion_cb(struct spdk_bdev_io *bdev_io,
			      bool success,
			      void *cb_arg)
{
	struct dfly_request *req = (struct dfly_request *)cb_arg;
	struct dfly_subsystem *req_ss       = dfly_get_subsystem_no_lock(req->req_ssid);
	struct df_dev_response_s resp;

	if (!success) {
		//TODO: Fail counter
	}

	if (req) {
		req->status = success;
		if (req_ss->mlist.dfly_fuse_module
		    && (req->flags & DFLY_REQF_NVMF)) {
			req->next_action = DFLY_COMPLETE_ON_FUSE_THREAD;
			dfly_handle_request(req);
		} else {
			//assert(0);
			assert(req->ops.complete_req == wal_flush_complete
			       || req->ops.complete_req == log_recovery_writethrough_complete
			       || req->ops.complete_req == iter_ctrl_complete);

			if (req->ops.complete_req == iter_ctrl_complete) {
				spdk_bdev_io_get_nvme_status(bdev_io, &req->rsp_cdw0, &req->rsp_sct, &req->rsp_sc);
			}

			resp.rc = success;
			resp.opc = req->ops.get_command(req);
			spdk_bdev_io_get_nvme_status(bdev_io, &resp.cdw0, &resp.nvme_sct, &resp.nvme_sc);

			req->ops.complete_req(resp, req->req_private);
			dfly_io_put_req(NULL, req);
		}
	}

	spdk_bdev_free_io(bdev_io);
}

extern struct dfly_request_ops df_req_ops_inst;

int dfly_kv_execute_op(struct df_device *device, int opc,
		       struct dfly_key *key, struct dfly_value *value,
		       df_dev_io_completion_cb cb, void *cb_arg,
		       struct dfly_request **pp_req)
{
	dfly_kv_pool_t *kv_pool_dev_ctx = (dfly_kv_pool_t *)device;

	struct dfly_request *req;
	struct spdk_nvme_cmd *cmd;

	struct dfly_io_device_s *io_device;//Device

	uint32_t icore = spdk_env_get_current_core();

	void *buffer = value ? value->value : NULL;
	uint32_t buff_len = value ? value->length : 0;
	uint32_t offset   = value ? value->offset : 0;

	assert(cb);

	if (pp_req) {
		if (*pp_req) {
			req = *pp_req;
			assert(0);//Not used yet
		} else {
			req = dfly_io_get_req(NULL);
			req->ops = df_req_ops_inst;
			if (key)
				req->req_key = *key;

			if (value)
				req->req_value = *value;

			assert(req);
			*pp_req = req;
		}
	} else {
		req = dfly_io_get_req(NULL);
		assert(req);
		req->ops = df_req_ops_inst;
	}

	cmd = (struct spdk_nvme_cmd *)req->req_ctx;

	req->req_private = cb_arg;
	req->ops.complete_req = cb;

	io_device = (struct dfly_io_device_s *)dfly_kd_key_to_device(kv_pool_dev_ctx->df_ssid, key->key,
			key->length);


	switch (opc) {
	case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
		nvme_kv_cmd_setup_bdev_store_request(cmd, io_device->ns->opts.nsid, key->key, key->length,
						     buffer, buff_len, offset, 0, 0);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
		nvme_kv_cmd_setup_bdev_retrieve_request(cmd, io_device->ns->opts.nsid, key->key, key->length,
							buffer, buff_len, offset, 0, 0);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
		nvme_kv_cmd_setup_bdev_delete_request(cmd, io_device->ns->opts.nsid, key->key, key->length, 0, 0);
		break;
	default:
		assert(0);
	}

	req->flags |= DFLY_REQF_NVME;
	req->state = DFLY_REQ_BEGIN;
	req->req_ssid = kv_pool_dev_ctx->df_ssid;
	req->req_value.value = buffer;
	req->req_value.length = buff_len;

	if (!pp_req) {
		dfly_handle_request(req);
	}

	return 0;

}


void dfly_kv_submit_req(struct dfly_request *req, struct dfly_io_device_s *io_device,
			struct spdk_io_channel *ch)
{
	spdk_bdev_nvme_io_passthru(io_device->ns->desc, ch, (const struct spdk_nvme_cmd *)req->req_ctx,
				   req->req_value.value, req->req_value.length,
				   dfly_kv_pool_io_completion_cb, req);
}

int dfly_kv_execute_iter2(struct df_device *device, int opc,
			  void *dfly_iter_info,
			  df_dev_io_completion_cb cb, void *cb_arg)
{
	dfly_kv_pool_t *kv_pool_dev_ctx = (dfly_kv_pool_t *)device;
	dfly_iterator_info *iter_info = (dfly_iterator_info *)dfly_iter_info;
	struct dfly_request *req = NULL;
	struct spdk_nvme_cmd *cmd = NULL;
	uint32_t nsid;
	int i = 0;
	iter_info->nr_pending_dev = iter_info->nr_dev;
	for (i = 0; i < iter_info->nr_dev ; i++) {

		dev_iterator_info *dev_iter_info = &iter_info->dev_iter_info[i];
		assert(dev_iter_info);
		struct dfly_io_device_s *kvs_device = (struct dfly_io_device_s *)dev_iter_info->io_device;
		assert(kvs_device);

		if (opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ) {
			if (dev_iter_info->is_eof == 0x93
			    || dev_iter_info->read_pos + (2 * ITER_LIST_ALIGN) < dev_iter_info->it_data_sz) {
				ATOMIC_DEC_FETCH(iter_info->nr_pending_dev);

				continue;
			}
		}

		req = dfly_io_get_req(NULL);
		assert(req);

		nsid = kvs_device->ns->opts.nsid;
		req->opc = opc;
		req->ops = df_req_ops_inst;
		req->io_device = kvs_device;
		req->iter_data.dev_iter_info =  &iter_info->dev_iter_info[i];

		//TODO: use cb_arg
		req->req_private = dev_iter_info;
		req->ops.complete_req = cb;

		cmd = (struct spdk_nvme_cmd *)req->req_ctx;
		if (opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL) {
			nvme_kv_cmd_setup_bdev_iter_ctrl_request(cmd,
					nsid, iter_info, dev_iter_info->iter_handle);
		} else if (opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ) {
			nvme_kv_cmd_setup_bdev_iter_read_request(cmd,
					nsid, dev_iter_info, dev_iter_info->it_buff_cap);
			dev_iter_info->read_pos = 0;
			req->req_value.length = dev_iter_info->it_buff_cap;
			req->req_value.value = dev_iter_info->it_buff;
			req->req_value.offset = 0;
		} else {
			assert(0);
		}
		req->flags |= DFLY_REQF_NVME;
		req->req_ssid = kv_pool_dev_ctx->df_ssid;
		//printf("dev iter: %d of %d devices nsid %d req %p\n",
		//    i, iter_info->nr_dev, kvs_device->ns->opts.nsid, req);

		dfly_handle_request(req);
	}

	if (!iter_info->nr_pending_dev) {
		req = iter_info->dfly_req;
		dfly_resp_set_cdw0(req, 0);
		dfly_nvmf_complete(req);
		req->state = DFLY_REQ_IO_NVMF_DONE;
		dfly_handle_request(req);
		iter_info->dfly_req = NULL;

	}
	return 0;

}


uint64_t dfly_kv_pool_getsize(struct df_device *device)
{
	dfly_kv_pool_t *ctx = (dfly_kv_pool_t *)device;

	return ctx->capacity;
}

int dfly_kv_pool_iter_read(struct df_device *device, void *iter_info,
			   df_dev_io_completion_cb cb, void *cb_arg)
{
	int rc = -1;
	dfly_iterator_info *dfly_iter_info = (dfly_iterator_info *)iter_info;

	if (dfly_iter_info->nr_pending_dev ||
	    !(dfly_iter_info->type & NVME_OPC_SAMSUNG_KV_ITERATE_OPTION_OPEN))
		assert(0);

	if (ATOMIC_BOOL_COMP_CHX(dfly_iter_info->status, KVS_ITERATOR_STATUS_OPEN,
				 KVS_ITERATOR_STATUS_READING)
	    || ATOMIC_BOOL_COMP_CHX(dfly_iter_info->status, KVS_ITERATOR_STATUS_READ,
				    KVS_ITERATOR_STATUS_READING)) {
		rc =  dfly_kv_execute_iter2(device, SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ,
					    dfly_iter_info, cb, cb_arg);
	}
	return rc;
}

int dfly_kv_pool_iter_open(struct df_device *device, void *iter_info,
			   df_dev_io_completion_cb cb, void *cb_arg)
{
	int rc = -1;
	dfly_iterator_info *dfly_iter_info = (dfly_iterator_info *)iter_info;

	if (dfly_iter_info->nr_pending_dev ||
	    !(dfly_iter_info->type & NVME_OPC_SAMSUNG_KV_ITERATE_OPTION_OPEN))
		assert(0);

	if (ATOMIC_BOOL_COMP_CHX(dfly_iter_info->status, KVS_ITERATOR_STATUS_UNKNOWN,
				 KVS_ITERATOR_STATUS_OPENING)
	    || ATOMIC_BOOL_COMP_CHX(dfly_iter_info->status, KVS_ITERATOR_STATUS_CLOSE,
				    KVS_ITERATOR_STATUS_OPENING)) {
		rc =  dfly_kv_execute_iter2(device, SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL,
					    dfly_iter_info, cb, cb_arg);
	}
	return rc;
}

int dfly_kv_pool_iter_close(struct df_device *device, void *iter_info,
			    df_dev_io_completion_cb cb, void *cb_arg)
{
	int rc = -1;
	dfly_iterator_info *dfly_iter_info = (dfly_iterator_info *)iter_info;

	if (dfly_iter_info->nr_pending_dev ||
	    !(dfly_iter_info->type & NVME_OPC_SAMSUNG_KV_ITERATE_OPTION_CLOSE))
		assert(0);

	if (ATOMIC_BOOL_COMP_CHX(dfly_iter_info->status, KVS_ITERATOR_STATUS_OPEN,
				 KVS_ITERATOR_STATUS_CLOSING)
	    || ATOMIC_BOOL_COMP_CHX(dfly_iter_info->status, KVS_ITERATOR_STATUS_READ,
				    KVS_ITERATOR_STATUS_CLOSING)) {
		rc = dfly_kv_execute_iter2(device, SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL,
					   dfly_iter_info, cb, cb_arg);
	}
	return rc;
}


int dfly_kv_pool_store(struct df_device *device, struct dfly_key *key, struct dfly_value *value,
		       df_dev_io_completion_cb cb, void *cb_arg)
{
	return dfly_kv_execute_op(device, SPDK_NVME_OPC_SAMSUNG_KV_STORE, key, value, cb, cb_arg, NULL);
}

int dfly_kv_pool_retrieve(struct df_device *device, struct dfly_key *key, struct dfly_value *value,
			  df_dev_io_completion_cb cb, void *cb_arg)
{
	return dfly_kv_execute_op(device, SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE, key, value, cb, cb_arg, NULL);
}

int dfly_kv_pool_delete(struct df_device *device, struct dfly_key *key,
			df_dev_io_completion_cb cb, void *cb_arg)
{
	return dfly_kv_execute_op(device, SPDK_NVME_OPC_SAMSUNG_KV_DELETE, key, NULL, cb, cb_arg, NULL);
}

int dfly_kv_pool_admin_passthru(struct df_device *device, struct spdk_nvme_cmd *cmd,
				void *buff, uint64_t nbytes,
				df_dev_io_completion_cb cb, void *cb_arg)
{
	//TODO Implement KV POOL Passthru
	return 0;
}

struct df_device dfly_kv_pool_device = {
	.type = DFLY_DEVICE_TYPE_KV_POOL,
	.ops = {
		.open  = dfly_kv_pool_open,
		.close = dfly_kv_pool_close,
		.io_complete = NULL,
		.getsize = dfly_kv_pool_getsize,
		.admin_passthru = dfly_kv_pool_admin_passthru,
		.d = {
			.kv = {
				.iter_info_setup = dfly_iter_info_setup,
				.iter_open = dfly_kv_pool_iter_open,
				.iter_close = dfly_kv_pool_iter_close,
				.iter_read = dfly_kv_pool_iter_read,
				.store = dfly_kv_pool_store,
				.retrieve = dfly_kv_pool_retrieve,
				.key_delete = dfly_kv_pool_delete,
				.exists = NULL,
				.build_request = dfly_kv_execute_op,
			},
		},
		.set_handle = dfly_kv_pool_set_handle,
	},
};

void dfly_register_kv_pool_dev(void)
{
	dfly_register_device(&dfly_kv_pool_device);
}


//TEST
//

struct dfly_request test_req;

struct dfly_key test_key;
struct dfly_value test_value;

enum test_state {
	TEST_BEGIN = 0,
	TEST_STORE_COMPLETE,
	TEST_READ_BACK_COMPLETE,
	TEST_DELETE_COMPLETE,
	TEST_END,
} ts = TEST_BEGIN;

int test_kv_fhandle = -1;

static void
dfly_kv_pool_dev_test_completion_cb(bool success, void *arg)
{
	struct dfly_request *req = (struct dfly_request *)arg;
	//Switch to next state
	switch (ts) {
	case TEST_BEGIN:
		ts = TEST_STORE_COMPLETE;
		break;
	case TEST_STORE_COMPLETE:
		ts = TEST_READ_BACK_COMPLETE;
		DFLY_NOTICELOG("READ BUFFER:\n%s\n", test_value.value);
		DFLY_NOTICELOG("\n\n\n\nREAD BUFFER LEN:%d\n", strlen((const char *)test_value.value));
		break;
	case TEST_READ_BACK_COMPLETE:
		ts = TEST_DELETE_COMPLETE;
		break;
	case TEST_DELETE_COMPLETE:
		DFLY_NOTICELOG("READ BUFFER:\n%s\n", test_value.value);
		DFLY_NOTICELOG("\n\n\n\nREAD BUFFER LEN:%d\n", strlen((const char *)test_value.value));
		dfly_io_put_buff(NULL, test_value.value);
		test_value.value = NULL;

		dfly_device_close(test_kv_fhandle);
		test_kv_fhandle = -1;
		ts = TEST_END;
		break;
	default:
		assert(0);
	};
	//Do next Op for state
	switch (ts) {
	case TEST_STORE_COMPLETE:
	case TEST_DELETE_COMPLETE:
		//Read back
		memset(test_value.value, 0, 4095);
		dfly_device_retrieve(test_kv_fhandle, &test_key, &test_value, dfly_kv_pool_dev_test_completion_cb,
				     &test_req);
		break;
	case TEST_READ_BACK_COMPLETE:
		dfly_device_delete(test_kv_fhandle, &test_key, dfly_kv_pool_dev_test_completion_cb, &test_req);
		break;
	};
}

void dfly_kv_pool_dev_test_io(void)
{
	test_value.value = dfly_io_get_buff(NULL, 4096);
	test_value.length = 4096;
	test_value.offset = 0;

	test_key.key = (void *)"abcdef0123456789";
	test_key.length = 16;

	memset(test_value.value, 'C', 4095);

	dfly_device_store(test_kv_fhandle, &test_key, &test_value, dfly_kv_pool_dev_test_completion_cb,
			  &test_req);
	DFLY_NOTICELOG("Issued test Write IO to block0 with 4kB\n");

}

void dfly_kv_pool_dev_test(void)
{
	//Expects nvme block device in spdk config with name waltest
	test_kv_fhandle = dfly_device_open((char *)"nqn.2018-01.io.spdk:cnode1", DFLY_DEVICE_TYPE_KV_POOL,
					   true);

	dfly_kv_pool_dev_test_io();
	//DFLY_NOTICELOG("bdev size: %ld\n", dfly_device_getsize(test_kv_fhandle));
}
