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


#include <fuse_map.h>
#include <fuse_lib.h>

extern int fuse_debug;
int max_nr_entries = 1;
fuse_key_t *fuse_key_clone(fuse_key_t *src_key)
{
	fuse_key_t *clone = (fuse_key_t *)df_calloc(1, sizeof(fuse_key_t));
	clone->length = src_key->length;
	clone->key = df_malloc(clone->length);
	memcpy(clone->key, src_key->key, clone->length);
	return clone;
}

void fuse_map_cleanup(fuse_map_t **pmap)
{
	fuse_map_t *map = *pmap;
	int nr_buckets = map->nr_buckets;
	fuse_bucket_t *buckets = map->table;
	fuse_map_item_t *item = NULL, * tmp = NULL;
	df_lock(&map->map_lock);
	assert(TAILQ_EMPTY(&map->wp_delay_queue));
	assert(TAILQ_EMPTY(&map->wp_pending_queue));

	for (int i = 0; i < nr_buckets; i++) {
		TAILQ_FOREACH_SAFE(item, &buckets[i].bucket_list, item_list, tmp) {
			//get map item
			assert(TAILQ_EMPTY(&item->fuse_waiting_head));
			TAILQ_REMOVE(&buckets[i].bucket_list, item, item_list);
			df_free(item);
		}
	}

	df_free(map->table);
	df_unlock(&map->map_lock);

	df_free(map);
	*pmap = NULL;
}

// create buffer during init, given role, space range.
fuse_map_t *fuse_map_create(int nr_buckets, fuse_map_ops_t *map_ops)
{
	int i = 0, j = 0;
	fuse_map_t *map = (fuse_map_t *)df_calloc(1, sizeof(fuse_map_t));
	fuse_bucket_t *buckets;
	map->nr_buckets = nr_buckets;
	fuse_map_item_t *item ;
	buckets = (fuse_bucket_t *)df_calloc(nr_buckets, sizeof(fuse_bucket_t));
	for (i = 0; i < nr_buckets; i++) {
		TAILQ_INIT(&buckets[i].bucket_list);
		//preinstall 4 clean item per bucket
		for (j = 0; j < 4; j++) {
			item = (fuse_map_item_t *)df_calloc(1, sizeof(fuse_map_item_t));
			item->map = map;
			item->bucket = &buckets[i];
			item->status = FUSE_STATUS_CLEAN;
			item->fuse_req = NULL;
			TAILQ_INIT(&item->fuse_waiting_head);
			TAILQ_INSERT_TAIL(&buckets[i].bucket_list, item, item_list);
			ATOMIC_INC(buckets[i].nr_entries);
			item->reserved = -1;
		}

	}
	TAILQ_INIT(&map->free_list);
	map->table = buckets;
	map->ops = map_ops;
	TAILQ_INIT(&map->wp_delay_queue);
	TAILQ_INIT(&map->wp_pending_queue);
	TAILQ_INIT(&map->io_pending_queue);

	df_lock_init(&map->map_lock, NULL);

	return map;
}

fuse_map_item_t *fuse_map_get_item(fuse_map_t *map)
{
	fuse_map_item_t *item = NULL, * temp;

	TAILQ_FOREACH_SAFE(item, &map->free_list, item_list, temp) {
		assert(item->status == FUSE_STATUS_CLEAN);
		TAILQ_REMOVE(&map->free_list, item, item_list);
		ATOMIC_DEC_FETCH(map->nr_free_item);
		fuse_log("fuse_map_get_item from map[%d] freelist %p, nr_free_item %d\n",
			 map->map_idx, item, map->nr_free_item);
		return item;
	}

	item = (fuse_map_item_t *)df_calloc(1, sizeof(fuse_map_item_t));
	item->map = map;
	fuse_log("fuse_map_get_item from map[%d] calloc %p, nr_free_item %d\n",
		 map->map_idx, item, map->nr_free_item);
	return item;

}

void fuse_init_item(fuse_map_item_t *item, fuse_object_t *obj)
{
	assert(item && obj);
	if (!item->key) {
		item->key = fuse_key_clone(obj->key);
	} else {
		if (item->key->length < obj->key->length) {
			free(item->key->key);
			item->key->key = df_malloc(obj->key->length);
		}
		memcpy(item->key->key, obj->key->key, obj->key->length);
		item->key->length = obj->key->length;
	}
	//item->session_id = obj->session_id;
	item->ref_cnt = 0;
	item->fuse_req = NULL;
	item->status = FUSE_STATUS_CLEAN;
}

//find the object with same key, insert new one if not existed.
uint32_t fuse_map_lookup(fuse_map_t *map, fuse_object_t *obj,
			 uint32_t lookup_type, fuse_map_item_t **ret_item)
{
	int bucket_idx = obj->key_hash % map->nr_buckets;
	fuse_bucket_t *bucket = &map->table[bucket_idx];
	int rc = FUSE_SUCCESS;
	//int nr_items = bucket->nr_entries;

	fuse_map_item_t *item = NULL, * avail_item = NULL;
	int found = 0;
	int cnt  = 0;

	TAILQ_FOREACH(item, &bucket->bucket_list, item_list) {
		cnt ++;
		//assert(bucket->nr_entries);

		if (!avail_item &&
		    !ATOMIC_READ(item->ref_cnt)
		    && !(item->fuse_req)
		    && TAILQ_EMPTY(&item->fuse_waiting_head)) {
			avail_item = item;
		} else {
			//(ref_cnt > 0 or fuse wait queue is not empty or fuse op is in progress), and the same key, found
			if ((ATOMIC_READ(item->ref_cnt) || !TAILQ_EMPTY(&item->fuse_waiting_head) || item->fuse_req)
			    && !map->ops->key_comp(obj->key, item->key)) {
				//found the obj
				found = 1;
				if (avail_item) {
					avail_item = NULL;
				}
				break;
			}
		}

	}

	if (!found) {
		if (!avail_item) {
			item = (fuse_map_item_t *)df_calloc(1, sizeof(fuse_map_item_t));
			item->map = map;
			item->bucket = bucket;
			TAILQ_INIT(&item->fuse_waiting_head);
			TAILQ_INSERT_TAIL(&bucket->bucket_list, item, item_list);
			ATOMIC_INC(bucket->nr_entries);
			item->reserved = -1;
			if (bucket->nr_entries > max_nr_entries) {
				max_nr_entries = bucket->nr_entries;
				printf("max_nr_entries %d\n", max_nr_entries);
			}
		} else {
			item = avail_item;
			assert(item->ref_cnt == 0);
			fuse_log("fuse_map_lookup: map[%d] bucket[%d] nr_entries %d, cnt %d, found %d, avail_item %p\n",
				 map->map_idx, bucket_idx, bucket->nr_entries, cnt, found, avail_item);
		}
		fuse_init_item(item, obj);
	} else {

		fuse_log("fuse_map_lookup: map[%d] bucket[%d] nr_entries %d, cnt %d, found_item %p, avail_item %p\n",
			 map->map_idx, bucket_idx, bucket->nr_entries, cnt, item, avail_item);
	}

	* ret_item = item;
	return rc;
}

