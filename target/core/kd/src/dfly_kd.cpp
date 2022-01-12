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

int dfly_init_kd_context(uint32_t ssid, kd_type_t type)
{

	struct dfly_subsystem *ss = NULL;

	int32_t ret = 0;

	ss = dfly_get_subsystem(ssid);
	assert(ss);
	if (!ss) {
		//No Dragonfly Subsystem
		return -ENOMEM;
	}

	if (ss->kd_ctx) {
		DFLY_ASSERT(0);
		return 0;
	}

	switch (type) {
	case DFLY_KD_SIMPLE_HASH://Simple XOR hashing
		ss->kd_ctx = dfly_init_kd_sh_context();
		break;
	case DFLY_KD_RH_MURMUR3://Rendezvous Hashing
		ss->kd_ctx  = dfly_init_kd_rh_context();
		break;
	case DFLY_KD_CH_MURMUR3://Consitent Hashing with virtual nodes
		/** TODO */
		break;
	default:
		ss->kd_ctx = NULL;
	}

	if (!ss->kd_ctx) {
		return -ENOMEM;
	}

	return 0;
}

void dfly_deinit_kd_context(uint32_t ssid, kd_type_t type)
{

	struct dfly_subsystem *ss = NULL;

	int32_t ret = 0;

	ss = dfly_get_subsystem(ssid);
	assert(ss);
	if (!ss) {
		//No Dragonfly Subsystem
		DFLY_ASSERT(0);
	}

	if (!ss->kd_ctx) {
		DFLY_ASSERT(0);
	}

	switch (type) {
	case DFLY_KD_SIMPLE_HASH://Simple XOR hashing
		DFLY_ASSERT(0);
		break;
	case DFLY_KD_RH_MURMUR3://Rendezvous Hashing
		dfly_deinit_kd_rh_context(ss->kd_ctx);
		ss->kd_ctx = NULL;
		break;
	case DFLY_KD_CH_MURMUR3://Consitent Hashing with virtual nodes
		/** TODO */
		DFLY_ASSERT(0);
		break;
	default:
		ss->kd_ctx = NULL;
	}
	return;
}

bool dfly_kd_add_device(uint32_t ssid, const char *device_name, uint32_t len, void *disk)
{
	struct dfly_subsystem *ss = NULL;
	struct dfly_kd_context_s *ctx = NULL;

	ss = dfly_get_subsystem(ssid);
	assert(ss);
	if (!ss) {
		//No Dragonfly Subsystem
		return -ENOMEM;
	}

	return ss->kd_ctx->kd_fn_table->add_device(ss->kd_ctx, device_name, len, disk);
}

void *dfly_kd_get_device(struct dfly_request *req)
{
	//TODO
	return req->req_dfly_ss->kd_ctx->kd_fn_table->find_device(req->req_dfly_ss->kd_ctx,
			req->req_key.key, req->req_key.length);
}

void *dfly_kd_key_to_device(uint32_t ssid, void *key, uint32_t keylen)
{
	struct dfly_subsystem *ss = NULL;

	ss = dfly_get_subsystem_no_lock(ssid);
	assert(ss);

	return ss->kd_ctx->kd_fn_table->find_device(ss->kd_ctx, key, keylen);


}

void *dfly_list_device(uint32_t ssid, void **dev_list, uint32_t *nr_dev)
{
	struct dfly_subsystem *ss = NULL;

	ss = dfly_get_subsystem_no_lock(ssid);
	assert(ss);

	return ss->kd_ctx->kd_fn_table->list_device(ss->kd_ctx, dev_list, nr_dev);

}

