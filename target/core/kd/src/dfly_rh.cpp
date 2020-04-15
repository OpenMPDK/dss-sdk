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


#include <string.h>
#include <cmath>
#include "assert.h"
#include "murmurhash3.h"

#include "dragonfly.h"

#define MAX_RH_DEV_STR (MAX_KD_DEV_STR)
#define MAX_RH_DEVICES (MAX_KD_DEVICES)

#define MAX_INT64 (0XFFFFFFFFFFFFFFFF)

typedef union rh_hash_u {
	unsigned char digest[17];//NULL termination first 16 bytes are used
	struct h64_s {
		uint64_t p1;
		uint64_t p2;
	} h64;
	struct h32_s {
		uint32_t p1;
		uint32_t p2;
		uint32_t p3;
		uint32_t p4;
	} h32;
} rh_hash_t;

struct dfly_rh_device_s {
	char             name[MAX_RH_DEV_STR];
	rh_hash_t        id;
	void            *disk;
};

typedef struct dfly_rh_instance_s {
	struct dfly_kd_context_s kd_ctx;
	struct dfly_rh_device_s devices[MAX_RH_DEVICES];
	uint32_t              num_devices;
} dfly_rh_instance_t;

bool dfly_rh_add_device(void *vctx, const char *dev_name, uint32_t len_dev_name, void *disk);
void *dfly_rh_find_object_disk(void *vctx, void *in, uint32_t len);

void __dfly_rh_hash_murmur3(void *in, uint32_t len, uint32_t seed, rh_hash_t *out)
{
	MurmurHash3_x64_128((const void *)in, len, seed, (void *) & (out->digest));
}

void dfly_rh_set_hash_devid(struct dfly_rh_device_s *dev)
{
	__dfly_rh_hash_murmur3((void *)dev->name, strlen(dev->name), 0, &dev->id);
}

bool dfly_rh_add_device(void *vctx, const char *dev_name, uint32_t len_dev_name, void *disk)
{


	dfly_rh_instance_t *ctx = (dfly_rh_instance_t *)vctx;

	int curr_device_index = 0;
	uint32_t copy_len = 0;

	if (!ctx) {
		assert(0);//Call init first
		return false;
	}

	curr_device_index = ctx->num_devices++;

	if (curr_device_index >= MAX_RH_DEVICES) {
		return false;
	}

	assert(len_dev_name <  MAX_RH_DEV_STR);//Name will be truncated
	copy_len = (len_dev_name <  MAX_RH_DEV_STR) ? (len_dev_name + 1) : (MAX_RH_DEV_STR - 1);
	memcpy(ctx->devices[curr_device_index].name, dev_name, copy_len);

	assert(disk);
	ctx->devices[curr_device_index].disk = disk;

	dfly_rh_set_hash_devid(&ctx->devices[curr_device_index]);

	return true;
}

static inline float rh_compute_object_node_wt(uint64_t value)
{
	uint64_t nr = (MAX_INT64 >> (64 - 53));
	float dr = (float)((uint64_t)1 << 53);
	return (1 / -logf(((float)(value & nr)) / dr));
}

void *dfly_rh_find_object_disk(void *vctx, void *in, uint32_t len)
{
	struct dfly_rh_device_s *max_rh_dev = NULL;
	float max_wt, curr_obj_node_wt;
	rh_hash_t curr_mmh3;

	dfly_rh_instance_t *ctx = (dfly_rh_instance_t *)vctx;

	int i;

	if (!ctx) {
		assert(0);//Call init first
		return NULL;
	}

	for (i = 0; i < ctx->num_devices; i++) {

		__dfly_rh_hash_murmur3(in, len, ctx->devices[i].id.h32.p2, &curr_mmh3);
		curr_obj_node_wt = rh_compute_object_node_wt(curr_mmh3.h64.p2);

		if (max_rh_dev) {
			if (curr_obj_node_wt > max_wt) {
				max_rh_dev = &ctx->devices[i];
				max_wt = curr_obj_node_wt;
			}
		} else {
			max_rh_dev = &ctx->devices[i];
			max_wt = curr_obj_node_wt;
		}

	}

	assert(max_rh_dev->disk);
	return max_rh_dev->disk;
}

void *dfly_rh_list_object_disk(void *vctx, void **dev_list, uint32_t *nr_dev)
{
	struct dfly_rh_device_s *max_rh_dev = NULL;
	float max_wt, curr_obj_node_wt;
	rh_hash_t curr_mmh3;

	dfly_rh_instance_t *ctx = (dfly_rh_instance_t *)vctx;

	int i;

	if (!ctx) {
		assert(0);//Call init first
		return NULL;
	}

	assert(ctx->num_devices <= *nr_dev);

	for (i = 0; i < ctx->num_devices; i++) {
		max_rh_dev = &ctx->devices[i];
		assert(max_rh_dev->disk);
		if (dev_list)
			dev_list[i] = max_rh_dev->disk;
	}

	* nr_dev = ctx->num_devices;
	return dev_list;
}


struct dfly_kd_fn_table dfly_kd_rh_fn_table = {
	.add_device  = dfly_rh_add_device,
	.find_device = dfly_rh_find_object_disk,
	.list_device = dfly_rh_list_object_disk,
};


struct dfly_kd_context_s *dfly_init_kd_rh_context(void)
{
	dfly_rh_instance_t *kd_rh_ctx;

	kd_rh_ctx = (dfly_rh_instance_t *)calloc(1, sizeof(dfly_rh_instance_t));

	if (!kd_rh_ctx) {
		return NULL;
	}

	kd_rh_ctx->kd_ctx.kd_fn_table = &dfly_kd_rh_fn_table;

	return (dfly_kd_context_s *)kd_rh_ctx;
}

void dfly_deinit_kd_rh_context(void *vctx)
{
	dfly_rh_instance_t *kd_rh_ctx = vctx;

	memset(kd_rh_ctx, 0, sizeof(dfly_rh_instance_t));

	free(kd_rh_ctx);

	return;
}
