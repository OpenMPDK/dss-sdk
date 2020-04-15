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


#ifndef __DF_KVEMUL_H_
#define __DF_KVEMUL_H_

#ifdef __cplusplus
extern "C" {
#endif

#define    KVEMUL_MAP_ITEM_SUCCESS        0
#define    KVEMUL_MAP_ITEM_CREATED        1
#define    KVEMUL_MAP_ITEM_FOUND          2
#define    KVEMUL_MAP_ITEM_NOT_FOUND      3
#define    KVEMUL_MAP_ITEM_MAPPED         4
#define    KVEMUL_MAP_COLLISION           5
#define    KVEMUL_MAP_INSERT_FAILED       6
#define    KVEMUL_MAP_FAILED              7

typedef struct kvemul_map_value_s {
	uint64_t    original_value_size;
	uint        start_lba; //in sectors
	uint        length; //in sectors
	void        *hash_key; //murmur hash, used to query cmap
} kvemul_map_value_t;

int kvemul_map_lba(void *cmap_ctx, void *orig_key, uint16_t orig_keylen, uint length,
		   uint32_t *lba);

int kvemul_map_lookup_item(void *cmap_ctx, void *tr_key, uint16_t tr_keylen,
			   kvemul_map_value_t **map_val);
int kvemul_map_insert_item(void *cmap_ctx, void *tr_key, uint16_t tr_keylen,
			   kvemul_map_value_t **map_val, uint length);
int kvemul_map_delete_item(void *cmap_ctx, void *orig_key, uint16_t orig_keylen);
void *kvemul_new_map(void);

#ifdef __cplusplus
}
#endif

#endif //__DF_KVEMUL_H_

