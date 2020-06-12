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

#include <malloc.h>
#include <string.h>

#include <Judy.h>

#include <utils/dss_count_latency.h>

struct dss_lat_ctx_s {
	char *name;
	void *jarr;
};

struct dss_lat_ctx_s * dss_lat_new_ctx(char *name)
{
	struct dss_lat_ctx_s *lctx = (struct dss_lat_ctx_s *)calloc(1, sizeof(struct dss_lat_ctx_s));

	if(lctx) {
		lctx->name = strdup(name);
		lctx->jarr = NULL;
		return lctx;
	} else {
		return NULL;
	}
}

void dss_lat_del_ctx(struct dss_lat_ctx_s *lctx)
{
	uint64_t mem_freed_count;

	mem_freed_count = JudyLFreeArray(&lctx->jarr, PJE0);

	free(lctx->name);
	free(lctx);

	return;
}

void dss_lat_reset_ctx(struct dss_lat_ctx_s *lctx)
{
	uint64_t mem_freed_count;

	mem_freed_count = JudyLFreeArray(&lctx->jarr, PJE0);

	return;
}

void dss_lat_inc_count(struct dss_lat_ctx_s *lctx, uint64_t duration)
{
	Word_t *value;

	value = (Word_t *)JudyLIns(&lctx->jarr, (Word_t)duration, PJE0);
	*value++;

	return;
}

uint64_t dss_lat_get_nentries(struct dss_lat_ctx_s *lctx)
{
	Word_t nentries;

	nentries = JudyLCount(lctx->jarr, 0, -1, PJE0);

	return (uint64_t)nentries;
}

uint64_t dss_lat_get_mem_used(struct dss_lat_ctx_s *lctx)
{
	Word_t mem_used;

	mem_used = JudyLMemUsed(lctx->jarr);

	return (uint64_t)mem_used;
}
