/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2021 Samsung Electronics Co., Ltd.
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

#ifndef __DSS_HSL_H
#define __DSS_HSL_H

#define DSS_LIST_DEBUG_MEM_USE

typedef struct dss_hslist_node_s {
	uint8_t leaf:1;
	uint8_t list_direct:1;
	void  *subtree;
	TAILQ_ENTRY(dss_hslist_node_s) lru_link;
} dss_hslist_node_t;

typedef int (*list_item_cb)(void *ctx, const char *item_key, int is_leaf);

typedef struct dss_hsl_ctx_s {
	list_item_cb process_listing_item;
	char *root_prefix;
	char* delim_str;
	uint32_t tree_depth;
#if defined DSS_LIST_DEBUG_MEM_USE
	uint64_t node_count;
#endif
	uint64_t mem_limit;
	uint64_t mem_usage;
	dss_hslist_node_t lnode;

	TAILQ_HEAD(lru_list_head, dss_hslist_node_s) lru_list;
	void *dev_ctx;
	struct dfly_tpool_s *dlist_mod;
} dss_hsl_ctx_t;

dss_hsl_ctx_t *dss_hsl_new_ctx(char *root_prefix, char *delim_str, list_item_cb list_cb);
int dss_hsl_insert(dss_hsl_ctx_t *hctx, const char *key);
int dss_hsl_delete(dss_hsl_ctx_t *hctx, const char *key);
int dss_hsl_list(dss_hsl_ctx_t *hctx, const char *prefix, const char *start_key, void *listing_ctx);
void dss_hsl_print_info(dss_hsl_ctx_t *hctx);

void dss_hsl_evict_levels(dss_hsl_ctx_t *hctx, int num_evict_levels, dss_hslist_node_t *node, int curr_level);

#endif //___DSS_HSL_H
