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


#include <stdio.h>
#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_vector.h"
#include <string>
#include <tuple>
#include "kvemul.h"
#include "df_kvemul.h"

#define SAMSUNG_KV_BLOCK_LBA_SIZE (512)

using namespace std;
using namespace tbb;

typedef tbb::concurrent_hash_map<map_key_t, kvemul_map_value_t *, map_CMap> cmap_table_t;

typedef struct cmap_ctx_s {
	cmap_table_t *ctable;
	unsigned long  lba_index;

	cmap_ctx_s()
	{
		this->ctable = new cmap_table_t(72);
		this->lba_index = 0;
	}

	~cmap_ctx_s()
	{
		delete this->ctable;
	}
} cmap_ctx_t;


int kvemul_map_lookup_item(void *cmap_ctx, void *orig_key, uint16_t orig_keylen,
			   kvemul_map_value_t **map_val)
{
	map_key_t               *tr_key;
	cmap_table_t::accessor  afind;
	bool                    ret = false;
	int                     retval = KVEMUL_MAP_FAILED;

	tr_key = new map_key_t(orig_key, orig_keylen);
	ret = ((cmap_ctx_t *)cmap_ctx)->ctable->find(afind, *tr_key);

	if (ret) {
		retval = KVEMUL_MAP_ITEM_FOUND;
		*map_val = afind->second;
	}

#if 0
	switch (retval) {
	case KVEMUL_MAP_ITEM_CREATED:
		map_LOG(INFO, "KEYCREATED: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		break;
	case KVEMUL_MAP_ITEM_FOUND:
		map_LOG(INFO, "KEYFOUND: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		break;
	case KVEMUL_MAP_ITEM_MAPPED:
		break;
	case KVEMUL_MAP_COLLISION:
		map_LOG(INFO, "KEYCOLLISION: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		return -1;
	case KVEMUL_MAP_INSERT_FAILED:
		map_LOG(INFO, "KEYINSERTFAIL: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		return -1;
	case KVEMUL_MAP_FAILED:
		map_LOG(INFO, "KEYCMAPFAIL: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		return -1;
	default:
		return -1;
	}
#endif

	delete tr_key;

	return retval;
}


int kvemul_map_insert_item(void *cmap_ctx, void *orig_key, uint16_t orig_keylen,
			   kvemul_map_value_t **map_val, uint length)
{

	map_key_t               *tr_key;
	cmap_table_t::accessor  ainsert;
	bool                    ret = false;
	int                     retval = KVEMUL_MAP_FAILED;
	kvemul_map_value_t        *li = NULL;

	tr_key = new map_key_t(orig_key, orig_keylen);

	ret = ((cmap_ctx_t *)cmap_ctx)->ctable->insert(ainsert, *tr_key);
	if (ret) {
		retval = KVEMUL_MAP_ITEM_CREATED;
		li = (kvemul_map_value_t *)malloc(sizeof(kvemul_map_value_t));
		li->hash_key = (void *)tr_key;
		tr_key = NULL;
		li->length = length;
		li->original_value_size = length;
		li->start_lba = __sync_fetch_and_add(&((cmap_ctx_t *)cmap_ctx)->lba_index,
						     (length / SAMSUNG_KV_BLOCK_LBA_SIZE));
		if (map_val)
			*map_val = li;
		ainsert->second = li;
	} else {
		printf("kvemul_map_insert_item: failed to insert key\n");
	}

#if 0
	switch (retval) {
	case KVEMUL_MAP_ITEM_CREATED:
		map_LOG(INFO, "KEYCREATED: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		break;
	case KVEMUL_MAP_ITEM_FOUND:
		map_LOG(INFO, "KEYFOUND: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		break;
	case KVEMUL_MAP_ITEM_MAPPED:
		break;
	case KVEMUL_MAP_COLLISION:
		map_LOG(INFO, "KEYCOLLISION: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		return -1;
	case KVEMUL_MAP_INSERT_FAILED:
		map_LOG(INFO, "KEYINSERTFAIL: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		return -1;
	case KVEMUL_MAP_FAILED:
		map_LOG(INFO, "KEYCMAPFAIL: OKEY[%16s] TRKEY[%s] ENTRIES[%u]\n", (char *)orig_key, (char *)tr_key,
			cmap_ctx->ctable->size());
		return -1;
	default:
		return -1;
	}
#endif

	if (tr_key)
		delete tr_key;

	return retval;
}

int kvemul_map_lba(void *cmap_ctx, void *orig_key, uint16_t orig_keylen, uint length, uint32_t *lba)
{
	cmap_ctx_t *cmap = (cmap_ctx_t *)cmap_ctx;

	*lba = __sync_fetch_and_add(&cmap->lba_index, (length / SAMSUNG_KV_BLOCK_LBA_SIZE));

	return 0;
}

int kvemul_map_delete_item(void *cmap_ctx, void *orig_key, uint16_t orig_keylen)
{
	map_key_t               *tr_key;
	bool                    ret = false;

	tr_key = new map_key_t(orig_key, orig_keylen);

	ret = ((cmap_ctx_t *)cmap_ctx)->ctable->erase(*tr_key);

	delete tr_key;

	return ret;
}

int kvemul_map_delete_item(void *cmap_ctx, kvemul_map_value_t *map_val, unsigned int *lba)
{
	return KVEMUL_MAP_FAILED;
}

void *kvemul_new_map(void)
{
	return (void *) new cmap_ctx_t();
}
