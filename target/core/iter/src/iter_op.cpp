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

#include "df_iter.h"

dfly_iterator_info g_iter_info;

static int get_iter_list_unit(dev_iterator_info *info,
			      void **unit_ptr, int *unit_sz)
{
	int nr_unit = 0;
	int read_sz = 0;
	void *buff = info->it_buff;
	int buff_sz = info->it_data_sz;

	assert(info->read_pos >= 0 && info->read_pos < info->it_data_sz);

	if (info->read_pos == 0) {
		nr_unit = *(uint32_t *)buff;
		buff += sizeof(uint32_t);
		read_sz += sizeof(uint32_t);
	} else {
		buff += info->read_pos;
	}

	if (info->read_pos > info->it_data_sz - 4)
		return read_sz;

	int data_sz = *(uint32_t *) buff;
	data_sz = (data_sz + ITER_LIST_ALIGN - 1) & ITER_LIST_ALIGN_MASK;
	* unit_sz = data_sz + sizeof(uint32_t);
	* unit_ptr = buff;
	read_sz += (*unit_sz);

	return read_sz;

}

int iter_sg_read_data(struct dfly_request *req, dfly_iterator_info *dfly_iter_info)
{
	int i = 0;
	int req_sz = req->req_value.length;
	void *req_buf = req->req_value.value + req->req_value.offset;
	int iter_sz = 0;
	dev_iterator_info *dev_iter_info = NULL;
	uint32_t *nr_ret = (int *)req_buf;
	req_sz -= sizeof(uint32_t);
	req_buf += sizeof(uint32_t);
	char dump[4096];
	uint32_t nr_unit = 0;
	dfly_iter_info->is_eof = 1;

	for (i = 0; i < dfly_iter_info->nr_dev; i++) {
		dev_iter_info = &dfly_iter_info->dev_iter_info[i];
		if (dev_iter_info->is_eof != 0x93)
			dfly_iter_info->is_eof = 0;
	}

	for (i = 0; i < dfly_iter_info->nr_dev; i++) {
		dev_iter_info = &dfly_iter_info->dev_iter_info[i];
		iter_log("\n iter_sg_read_data dev[%d] is_eof=%x read_pos %d nr_keys %d sz %d\n",
			 i, dev_iter_info->is_eof,
			 dev_iter_info->read_pos,
			 *(int *)dev_iter_info->it_buff,
			 dev_iter_info->it_data_sz);
		int read_sz = 0;
		while (dev_iter_info->read_pos + (2 * ITER_LIST_ALIGN)
		       < dev_iter_info->it_data_sz) {
			void *pdata = NULL;
			int data_sz = 0;
			read_sz = get_iter_list_unit(dev_iter_info, &pdata, &data_sz);
			if (read_sz > 0) {
				if (data_sz < req_sz) {
					int key_sz = *(int *)pdata;
					assert(key_sz > 0 && key_sz <= 255);
					snprintf(dump, data_sz - 4, "%s", (pdata + 4));
					//if(data_sz < 36)
					//    iter_log("sz %d %d %s, ", data_sz, *(int *)pdata, dump);
					memcpy(req_buf, pdata, data_sz);
					req_buf += data_sz;
					req_sz -= data_sz;
					nr_unit ++;
					dev_iter_info->read_pos += read_sz;
				} else {
					//dfly_req return buff is full.
					iter_log("too many keys to fullfill the dfly req buff\n");
					goto done;
				}
			}

		};
		if (!read_sz) { //finish the read of this data buff

		}
	}

done:
	iter_sz = req->req_value.length - req_sz;
	if (req->flags & DFLY_REQF_NVMF)
		dfly_resp_set_cdw0(req, iter_sz);
	* nr_ret = nr_unit;
	iter_log("\n");
	iter_log("iter_sg_read_data: nr_unit %d sz %d\n", nr_unit, iter_sz);
	return nr_unit;
}

static void iter_update_status(dfly_iterator_info *d_iter_info)
{
	if (ATOMIC_BOOL_COMP_CHX(d_iter_info->status, KVS_ITERATOR_STATUS_CLOSING,
				 KVS_ITERATOR_STATUS_CLOSE))
		return;

	if (ATOMIC_BOOL_COMP_CHX(d_iter_info->status, KVS_ITERATOR_STATUS_OPENING,
				 KVS_ITERATOR_STATUS_OPEN))
		return;

	if (ATOMIC_BOOL_COMP_CHX(d_iter_info->status, KVS_ITERATOR_STATUS_READING,
				 KVS_ITERATOR_STATUS_READ))
		return;
}

static void iter_reset_info(dfly_iterator_info *d_iter_info)
{
	int i = 0;
	for (i = 0; i < d_iter_info->nr_dev; i++) {
		dev_iterator_info *dev_iter_info = &d_iter_info->dev_iter_info[i];
		dev_iter_info->is_eof = 0;
		dev_iter_info->read_pos = 0;
		dev_iter_info->it_data_sz = 0;
		dev_iter_info->iter_handle = 0;
	}

	d_iter_info->dfly_req = NULL;
	//d_iter_info->dfly_iter_handle = 0;
	d_iter_info->is_eof = 0;
	d_iter_info->bitmask = 0;
	d_iter_info->bit_pattern = 0;
	d_iter_info->status = KVS_ITERATOR_STATUS_CLOSE;
}

typedef void (*iter_cb)(struct df_dev_response_s resp, void *, dfly_iterator_info *);

void iter_ctrl_complete(struct df_dev_response_s resp, void *args)
{
	//assert(success);

	dev_iterator_info *dev_iter_info = args;
	dfly_iterator_info *dfly_iter_info = dev_iter_info->iter_ctx;
	if (resp.opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL
	    && (dfly_iter_info->type & NVME_OPC_SAMSUNG_KV_ITERATE_OPTION_OPEN)) {
		dev_iter_info->iter_handle = resp.cdw0;
	}

	if (resp.opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ) {
		dev_iter_info->it_data_sz = resp.cdw0;
		dev_iter_info->is_eof = resp.nvme_sc;
		if (!dev_iter_info->it_data_sz)
			*(int *)dev_iter_info->it_buff = 0;
	}

	iter_log("iter_ctrl_complete: opc 0x%x success=%d type %x nr_pending_dev %d rsp_cdw0 %d sc %x\n",
		 resp.opc, resp.rc, dfly_iter_info->type, dfly_iter_info->nr_pending_dev,
		 resp.cdw0, resp.nvme_sc);

	if (!resp.rc && resp.nvme_sc == 0x93)
		resp.rc = true;

	if (!ATOMIC_DEC_FETCH(dfly_iter_info->nr_pending_dev)) {
		iter_update_status(dfly_iter_info);

		struct dfly_request *dfly_iter_req = dfly_iter_info->dfly_req;
		iter_log("iter_ctrl_complete done: opc 0x%x rsp_cdw0 %d dfly_iter_req %p\n",
			 resp.opc, resp.cdw0, dfly_iter_req);

		if (resp.opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL
		    && (dfly_iter_info->type & NVME_OPC_SAMSUNG_KV_ITERATE_OPTION_CLOSE)) {
			iter_reset_info(dfly_iter_info);
		}

		if (resp.opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ) {
			iter_sg_read_data(dfly_iter_req, dfly_iter_info);
		}

		if (dfly_iter_req->iter_data.internal_cb) {
			if (dfly_iter_info->is_eof) {
				iter_log("Complete the iteration!\n");
				dfly_iter_req->iter_data.iter_option |= KVS_ITERATOR_READ_EOF;
			}
			iter_cb cb = (iter_cb)(dfly_iter_req->iter_data.internal_cb);
			cb(resp, dfly_iter_req, dfly_iter_info);
		} else {
			dfly_iter_info->is_eof = 0;
			dfly_iter_req->status = DFLY_REQ_IO_NVMF_DONE;
			dfly_handle_request(dfly_iter_req);
			//dfly_nvmf_complete(dfly_iter_req);
			dfly_iter_info->dfly_req = NULL;

		}
	}

}

uint8_t __dfly_iter_next_handle = 0;
#define MAX_ITER_HANDLE 255

static int iter_handle_ctrl_op(struct dfly_subsystem *pool, struct dfly_request *req,
			       dfly_iterator_info *d_iter_info)
{
	int rc = 0;
	struct spdk_nvme_cmd *cmd = (struct spdk_nvme_cmd *)dfly_get_nvme_cmd(req);

	uint8_t iter_type = ((struct dword_bytes *)&cmd->cdw11)->cdwb2;
	if (iter_type & KVS_ITERATOR_OPEN) {
		if (!d_iter_info) {
			int iter_fh = dfly_device_open(pool->name, DFLY_DEVICE_TYPE_KV_POOL, true);
			DFLY_ASSERT(iter_fh >= 0);
			d_iter_info = (dfly_iterator_info *)dfly_device_create_iter_info(iter_fh, req);
			assert(d_iter_info);
			d_iter_info->pool_fh = iter_fh;
		}
		d_iter_info->type = iter_type;
		d_iter_info->bitmask = cmd->cdw13;
		d_iter_info->bit_pattern = cmd->cdw12;
		d_iter_info->dfly_req = req;
		d_iter_info->is_eof = 0;

		rc = dfly_device_iter_open(d_iter_info->pool_fh, d_iter_info, iter_ctrl_complete, d_iter_info);

	} else if (iter_type & KVS_ITERATOR_CLOSE) {
		struct dword_bytes *cdw11 = (struct dword_bytes *)&cmd->cdw11;
		uint8_t iter_handle = cdw11->cdwb1;
		assert(d_iter_info);
		d_iter_info->type = iter_type;
		d_iter_info->dfly_req = req;
		rc = dfly_device_iter_close(d_iter_info->pool_fh, d_iter_info,
					    iter_ctrl_complete, d_iter_info);
	} else {
		assert(0);
	}

	if (!rc)
		rc = DFLY_ITER_IO_PENDING;

	return rc;
}

static int iter_handle_read_op(struct dfly_subsystem *pool, struct dfly_request *req,
			       dfly_iterator_info *d_iter_info)
{
	int rc = -1;
	struct spdk_nvme_cmd *cmd = (struct spdk_nvme_cmd *)dfly_get_nvme_cmd(req);
	uint8_t iter_handle = ((struct dword_bytes *)&cmd->cdw11)->cdwb1;

	//d_iter_info = iter_lookup(iter_handle, false);
	assert(d_iter_info);
	d_iter_info->dfly_req = req;
	rc = dfly_device_iter_read(d_iter_info->pool_fh, d_iter_info,
				   iter_ctrl_complete, d_iter_info);
	if (!rc)
		rc = DFLY_ITER_IO_PENDING;
	return rc;
}

int iter_io(struct dfly_subsystem *pool, struct dfly_request *req, dfly_iterator_info *iter_info)
{
	int rc = 0;
	int opc = req->ops.get_command(req);
	if (opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL) {
		rc = iter_handle_ctrl_op(pool, req, iter_info);
	} else if (opc == SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ) {
		rc = iter_handle_read_op(pool, req, iter_info);
	} else {
		assert(0);
	}

	return rc;
	/*
	req->next_action = DFLY_ACTION_NONE;
	return DFLY_MODULE_REQUEST_QUEUED;
	*/
}

