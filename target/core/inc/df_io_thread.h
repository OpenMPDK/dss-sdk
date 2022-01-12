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


typedef int (*spdk_io_callback_fn)(struct spdk_nvmf_request *req);
typedef int (*spdk_io_passtru_cb_fn)(struct spdk_bdev *bdev, struct spdk_bdev_desc *desc,
				     struct spdk_io_channel *ch, struct spdk_nvmf_request *req);

typedef struct dfly_spdk_nvmf_io_ops_s {
	spdk_io_callback_fn    req_process;
	spdk_io_callback_fn    req_complete;
	spdk_io_passtru_cb_fn  req_passthru;
} dfly_spdk_nvmf_io_ops_t;

struct io_thread_ctx_s {
	struct dfly_subsystem *dfly_subsys;
	spdk_io_callback_fn spdk_nvmf_io_req_process_cb;
	spdk_io_callback_fn spdk_nvmf_io_req_completion_cb;
	dfly_spdk_nvmf_io_ops_t *io_ops;
	uint32_t num_threads;
};

struct io_thread_inst_ctx_s {
	struct io_thread_ctx_s *module_ctx;
	void *module_inst_ctx;
	int   module_inst_index;
	void *private_data;
	struct spdk_io_channel *io_chann_parr[0];
};

int dfly_io_req_process(struct dfly_request *req);
int dfly_io_module_init(int ssid, spdk_io_callback_fn cb, spdk_io_callback_fn req_completion_cb);
void dfly_io_complete_cb(struct dfly_request *req);
void dfly_nvmf_complete(struct dfly_request *req);

