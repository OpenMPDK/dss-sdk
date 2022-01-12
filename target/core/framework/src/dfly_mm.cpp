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

#include "spdk/env.h"
#include "spdk/bdev.h"
#include "spdk_internal/log.h"
#include "nvmf_internal.h"

#define DFLY_MM_LARGE_KEY_POOL_SIZE (1024)

typedef union dfly_spdk_request_u {
	struct spdk_nvmf_request nvmf_req;
	struct spdk_nvme_cmd     nvme_cmd;
} dfly_spdk_request_t;

struct mm_s {
	struct spdk_mempool *mm_io_buff_pool;
	struct spdk_mempool *mm_req_pool;
	struct spdk_mempool *mm_large_key_pool;
};

struct mm_s g_mm_ctx;

dfly_mempool *dfly_mempool_create(char *name, size_t sz, size_t cnt)
{
	return spdk_mempool_create(name, cnt, sz, \
				   SIZE_MAX,
				   SPDK_ENV_SOCKET_ID_ANY);
}

void dfly_mempool_destroy(dfly_mempool *mp, size_t cnt)
{
	// TODO: Add check for spdk_mempool_count. Use cnt
	spdk_mempool_free(mp);
}

void *dfly_mempool_get(dfly_mempool *mp)
{
	return spdk_mempool_get(mp);
}

void dfly_mempool_put(dfly_mempool *mp, void *item)
{
	spdk_mempool_put(mp, item);
}

void *dfly_mm_init(void)
{
	int cache_size;

	cache_size = g_dragonfly->mm_buff_count / (2 * spdk_env_get_core_count());

	if(g_wal_conf.wal_cache_enabled || g_fuse_conf.fuse_enabled) {
		g_mm_ctx.mm_io_buff_pool = spdk_mempool_create("dfly_mm_io_buff_pool",
					   g_dragonfly->mm_buff_count,
					   DFLY_BDEV_BUF_MAX_SIZE + 512,
					   cache_size,
					   SPDK_ENV_SOCKET_ID_ANY);
		if (!g_mm_ctx.mm_io_buff_pool) {
			DFLY_ERRLOG("create memory manager io buff pool failed\n");
			return NULL;
		}
	}

	g_mm_ctx.mm_req_pool = spdk_mempool_create("dfly_mm_pool",
			       g_dragonfly->mm_buff_count,
			       sizeof(struct dfly_request) + sizeof(dfly_spdk_request_t),
			       cache_size,
			       SPDK_ENV_SOCKET_ID_ANY);
	if (!g_mm_ctx.mm_req_pool) {
		DFLY_ERRLOG("create memory manager request pool failed\n");

		if(g_mm_ctx.mm_io_buff_pool)spdk_mempool_free(g_mm_ctx.mm_io_buff_pool);
		g_mm_ctx.mm_io_buff_pool = NULL;

		return NULL;
	}

	if(g_wal_conf.wal_cache_enabled || g_fuse_conf.fuse_enabled) {
		g_mm_ctx.mm_large_key_pool = spdk_mempool_create("dfly_mm_large_key_pool",
						 DFLY_MM_LARGE_KEY_POOL_SIZE,
						 SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1,
						 cache_size,
						 SPDK_ENV_SOCKET_ID_ANY);
		if (!g_mm_ctx.mm_large_key_pool) {
			DFLY_ERRLOG("create memory manager large key pool failed\n");

			if(g_mm_ctx.mm_io_buff_pool)spdk_mempool_free(g_mm_ctx.mm_io_buff_pool);
			g_mm_ctx.mm_io_buff_pool = NULL;

			spdk_mempool_free(g_mm_ctx.mm_req_pool);
			g_mm_ctx.mm_req_pool = NULL;

			return NULL;
		}
	}


	return &g_mm_ctx;
}

void dfly_mm_deinit(void)
{
	if(g_mm_ctx.mm_io_buff_pool) {
		if (spdk_mempool_count(g_mm_ctx.mm_io_buff_pool) != g_dragonfly->mm_buff_count) {
			DFLY_ERRLOG("DFLY IO  buffer pool count is %zu but should be %u\n",
					spdk_mempool_count(g_mm_ctx.mm_io_buff_pool),
					g_dragonfly->mm_buff_count);
			assert(false);
		}

		spdk_mempool_free(g_mm_ctx.mm_io_buff_pool);

		g_mm_ctx.mm_io_buff_pool = NULL;
	}

	if (spdk_mempool_count(g_mm_ctx.mm_req_pool) != g_dragonfly->mm_buff_count) {
		DFLY_ERRLOG("DFLY IO  buffer pool count is %zu but should be %u\n",
			    spdk_mempool_count(g_mm_ctx.mm_req_pool),
			    g_dragonfly->mm_buff_count);
		assert(false);
	}

	spdk_mempool_free(g_mm_ctx.mm_req_pool);

	g_mm_ctx.mm_req_pool = NULL;

	if(g_mm_ctx.mm_large_key_pool) {
		if (spdk_mempool_count(g_mm_ctx.mm_large_key_pool) != DFLY_MM_LARGE_KEY_POOL_SIZE) {
			DFLY_ERRLOG("DFLY large key buffer pool count is %zu but should be %u\n",
					spdk_mempool_count(g_mm_ctx.mm_large_key_pool),
					DFLY_MM_LARGE_KEY_POOL_SIZE);
			assert(false);
		}

		spdk_mempool_free(g_mm_ctx.mm_large_key_pool);

		g_mm_ctx.mm_large_key_pool = NULL;
	}

	return;

}

void *dfly_get_key_buff(void *vctx, uint64_t size)
{
	void *buff = NULL;

	assert(vctx == NULL);

	if (size <= SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1) {
		buff = spdk_mempool_get(g_mm_ctx.mm_large_key_pool);
	}

	return buff;
}

void dfly_put_key_buff(void *vctx, void *buff)
{

	assert(vctx == NULL);
	assert(buff);

	spdk_mempool_put(g_mm_ctx.mm_large_key_pool, buff);

	return;
}


void *dfly_io_get_buff(void *vctx, uint64_t size)
{
	void *buff = NULL;

	assert(vctx == NULL);

	if (size < DFLY_BDEV_BUF_MAX_SIZE) {
		buff = spdk_mempool_get(g_mm_ctx.mm_io_buff_pool);
	}

	return buff;
}

void dfly_io_put_buff(void *vctx, void *buff)
{

	assert(vctx == NULL);
	assert(buff);

	spdk_mempool_put(g_mm_ctx.mm_io_buff_pool, buff);

	return;
}

struct dfly_request *dfly_io_get_req(void *vctx)
{
	struct dfly_request *req = NULL;

	assert(vctx == NULL);

	req = (struct dfly_request *)spdk_mempool_get(g_mm_ctx.mm_req_pool);

	req->req_ctx = req + 1;
	req->io_device = NULL;
	return req;
}

void dfly_io_put_req(void *vctx, void *req)
{

	assert(vctx == NULL);
	assert(req);

	memset(req, 0, sizeof(struct dfly_request) + sizeof(dfly_spdk_request_t));
	spdk_mempool_put(g_mm_ctx.mm_req_pool, req);

	return;
}
