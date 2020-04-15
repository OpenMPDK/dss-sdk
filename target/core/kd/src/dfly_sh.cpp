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

#define MAX_SH_DEVICES (MAX_KD_DEVICES)
#define MAX_SH_DEV_STR (MAX_KD_DEV_STR)

struct dfly_sh_device_s {
	char  name[MAX_SH_DEV_STR];
	void *disk;
};

typedef struct dfly_sh_instance_s {
	struct dfly_kd_context_s kd_ctx;
	struct dfly_sh_device_s devices[MAX_SH_DEVICES];
	uint32_t num_devices;
} dfly_sh_instance_t;

bool dfly_sh_add_device(void *vctx, const char *dev_name, uint32_t len_dev_name, void *disk);
void *dfly_sh_find_object_disk(void *vctx, void *in, uint32_t len);

bool dfly_sh_add_device(void *vctx, const char *dev_name, uint32_t len_dev_name, void *disk)
{
	int curr_device_index = 0;
	uint32_t copy_len = 0;
	dfly_sh_instance_t *ctx = (dfly_sh_instance_t *)vctx;

	if (!ctx) {
		assert(0);//Call init first
		return false;
	}

	curr_device_index = ctx->num_devices++;

	if (curr_device_index >= MAX_SH_DEVICES) {
		return false;
	}

	assert(len_dev_name <  MAX_SH_DEV_STR);//Name will be truncated
	copy_len = (len_dev_name <  MAX_SH_DEV_STR) ? len_dev_name + 1 : (MAX_SH_DEV_STR - 1);
	memcpy(ctx->devices[curr_device_index].name, dev_name, copy_len);

	assert(disk);
	ctx->devices[curr_device_index].disk = disk;

	return true;
}


void *dfly_sh_find_object_disk(void *vctx, void *in, uint32_t len)
{
	int curr_device_index = 0;
	uint32_t dest_disk_index = 0;
	dfly_sh_instance_t *ctx = (dfly_sh_instance_t *)vctx;

	uint32_t *in_key = (uint32_t *)in;
	uint32_t key_hash = 0;

	if (!ctx) {
		assert(0);//Call init first
		return NULL;
	}

	key_hash = ((*(in_key)) ^ (*(in_key + 1)) ^ (*(in_key + 2)) ^ (*(in_key + 3)));

	dest_disk_index = (key_hash % ctx->num_devices);

	assert(ctx->devices[dest_disk_index].disk);

	return ctx->devices[dest_disk_index].disk;
}


struct dfly_kd_fn_table dfly_kd_sh_fn_table = {
	.add_device  = dfly_sh_add_device,
	.find_device = dfly_sh_find_object_disk,
};


struct dfly_kd_context_s *dfly_init_kd_sh_context(void)
{
	dfly_sh_instance_t *kd_sh_ctx;

	kd_sh_ctx = (dfly_sh_instance_t *)calloc(1, sizeof(dfly_sh_instance_t));

	if (!kd_sh_ctx) {
		return NULL;
	}

	kd_sh_ctx->kd_ctx.kd_fn_table = &dfly_kd_sh_fn_table;
}
