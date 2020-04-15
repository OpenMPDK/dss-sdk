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

#include <wal_map.h>
#include <wal_lib.h>

extern bool __log_enabled;

wal_key_t *wal_key_clone(wal_key_t *src_key)
{
	wal_key_t *clone = (wal_key_t *)df_calloc(1, sizeof(wal_key_t));
	clone->length = src_key->length;
	clone->key = df_malloc(clone->length);
	memcpy(clone->key, src_key->key, clone->length);
	return clone;
}

//wal_key_t dummy_key = {"ffffffffffffffff", 16};
wal_map_item_t *wal_map_allocate_item(void)
{
	wal_map_item_t *new_item = (wal_map_item_t *)df_calloc(1, sizeof(wal_map_item_t));
	new_item->key = (wal_key_t *)df_calloc(1, sizeof(wal_key_t));
	new_item->key->key = df_malloc(SAMSUNG_KV_MAX_KEY_SIZE + 1);
	new_item->key_buff_size = SAMSUNG_KV_MAX_KEY_SIZE;
	new_item->next = NULL;
	new_item->status = WAL_ITEM_INVALID;
	new_item->large_key_buff = NULL;
	return new_item;

}
// create buffer during init, given role, space range.
wal_map_t *wal_map_create(void *ctx, int nr_buckets, wal_map_ops_t *map_ops)
{
	int i = 0;
	wal_buffer_t *buffer = (wal_buffer_t *)ctx;
	wal_map_t *map = (wal_map_t *)df_calloc(1, sizeof(wal_map_t));
	wal_bucket_t *bucket;
	map->nr_buckets = nr_buckets;
	map->table = (wal_bucket_t *)df_calloc(nr_buckets, sizeof(wal_bucket_t));
	bucket = map->table;
	TAILQ_INIT(&map->head);


	for (; i < nr_buckets; i++, bucket ++) {
		//prepare three items per buckets
		wal_map_item_t *new_item = wal_map_allocate_item();
		new_item->buffer_ptr = buffer;
		bucket->entry = new_item;
		new_item = wal_map_allocate_item();
		new_item->buffer_ptr = buffer;
		bucket->entry->next = new_item;
		new_item = wal_map_allocate_item();
		new_item->buffer_ptr = buffer;
		bucket->entry->next->next = new_item;

		bucket->total_nr_entries = 3;
		bucket->valid_nr_entries = 0;
	}
	map->ops = map_ops;
	return map;
}

int wal_map_reinit(wal_map_t *map)
{
	wal_bucket_t *bucket = NULL, * tmp;
	int bucket_cnt = 0;
//	int valid_cnt = 0;

	bucket = map->table;
	/*
		INIT_LIST_HEAD(&map->head);
		for(int i = 0; i < map->nr_buckets; i++, bucket ++){
			bucket->valid_nr_entries = 0;
			INIT_LIST_HEAD(&bucket->link);
		}*/

	TAILQ_FOREACH_SAFE(bucket, &map->head, link, tmp) {
		bucket->valid_nr_entries = 0;
		wal_map_item_t *item = bucket->entry;
		while (item) {
			item->status = WAL_ITEM_INVALID;
			item = item->next;
		}
		TAILQ_REMOVE(&map->head, bucket, link);
		bucket_cnt ++;
	}
	TAILQ_INIT(&map->head);

	/*
		list_for_each_entry_safe(bucket, tmp, &map->head, link){
			valid_cnt ++;
		}
	*/

	//printf("reinit bucket cnt %d\n", bucket_cnt);
	return bucket_cnt;

}
int wal_map_bucket_iteration(wal_map_t *map)
{
	wal_bucket_t *bucket = NULL;
	int entry_cnt = 0;

	TAILQ_FOREACH(bucket, &map->head, link) {
		int cnt = bucket->valid_nr_entries;
		wal_map_item_t *entry = bucket->entry;
		if (cnt >= 2) {
			wal_debug(" bucket cnt %d\n", cnt);
		}
		while (cnt --) {
			//printf("key: 0x%llx addr 0x%llx\t", *(long long *)entry->key->key, (long long)entry->addr);
			entry = entry->next;
			entry_cnt ++;
		}
		//printf("\n");
	}
	return entry_cnt;
}

void wal_map_bucket_list_update(wal_map_t *map, wal_bucket_t *bucket)
{
	TAILQ_INSERT_TAIL(&map->head, bucket, link);
	/*
	int cnt = 0;
	wal_bucket_t * bk = NULL;
	list_for_each_entry(bk, &map->head, link){
		cnt ++;
	}
	printf("wal_map_bucket_list_update map %p new bucket %p cnt %d\n", map, bucket, cnt);
	*/
}

int bucket_revalidate(wal_map_t *map, wal_object_t *obj)
{
	int bucket_idx = obj->key_hash % map->nr_buckets;
	wal_bucket_t *bucket = &map->table[bucket_idx];

	int ve = bucket->valid_nr_entries;
	int te = bucket->total_nr_entries;

	wal_map_item_t *item = bucket->entry;

	int ive = 0;
	while (item) {
		if (item->status == WAL_ITEM_INVALID)
			ive ++;
		item = item->next;
	}

	if (ive != (te - ve)) {
		printf("bucket_revalidate: bucket %p te %d ve %d ive %d\n",
		       bucket, te, ve, ive);
		bucket->valid_nr_entries = bucket->total_nr_entries - ive;
	}

	return 0;
}
wal_map_item_t *wal_map_lookup(void *context, wal_map_t *map, wal_object_t *obj,
			       off64_t addr, wal_map_lookup_type_t lookup_type)
{
	//int bucket_idx = map->ops->hash((const char*)obj->key->key, obj->key->length) % map->nr_buckets ;
	int bucket_idx = obj->key_hash % map->nr_buckets;
	wal_bucket_t *bucket = &map->table[bucket_idx];
	int nr_items = bucket->valid_nr_entries;
	wal_map_item_t *item = bucket->entry;
	wal_map_item_t *pre_item = NULL, * invalid_item = NULL;
	int32_t log_size = 0 ;
	int is_read = (lookup_type == WAL_MAP_LOOKUP_READ);
	wal_map_insert_t map_insert_type = WAL_MAP_INSERT_NEW;
	int insert_rc = WAL_ERROR_IO;
	int old_vne = bucket->valid_nr_entries;
	int old_tne = bucket->total_nr_entries;
	wal_map_item_t *old_entry = bucket->entry;



	if (nr_items) {
		//check conflict
		while (nr_items) {

			if (item->status == WAL_ITEM_INVALID) {
				invalid_item = item;
				pre_item = item;
				item = item->next;
				if (!item && nr_items) {
					//assert(0);
					bucket_revalidate(map, obj);
					nr_items = bucket->valid_nr_entries;
					item = bucket->entry;
					pre_item = NULL;
					invalid_item = NULL;

				}

				continue;
			}

			-- nr_items;

			if (map->ops->key_comp(obj->key, item->key)) {
				pre_item = item;
				item = item->next;
			} else {
				//found the valid object with the same key, update
				//update the item->addr with new addr in wal
				if (lookup_type != WAL_MAP_LOOKUP_READ) {
					int rc_modify = 0;
					//printf("update of the same key object: ");
					if (lookup_type == WAL_MAP_LOOKUP_INVALIDATE || lookup_type == WAL_MAP_LOOKUP_DELETE) {
						if (lookup_type == WAL_MAP_LOOKUP_DELETE && item->status == WAL_ITEM_DELETED) {
							//the object already marked as deleted.
							return NULL;
						}
						rc_modify = map->ops->invalidate_item(context, bucket, item);
						if (lookup_type == WAL_MAP_LOOKUP_INVALIDATE) {
							//printf("invalid bkt %p item %p status %x nr_entries %d\n", bucket, item,
							//	item->status, bucket->valid_nr_entries);
							return item;
						} else {
							//for WAL_MAP_LOOKUP_DELETE, mark as deleted, but keep the item. LOG SERVICE ONLY
							if (lookup_type == WAL_MAP_LOOKUP_DELETE && !rc_modify) {
								assert(__log_enabled);
								bucket->valid_nr_entries ++; //due to the invalidat_item reduce the valid cnt.
							}
						}
					}

					if (!addr) {
						if (lookup_type == WAL_MAP_LOOKUP_WRITE_APPEND)
							map_insert_type = WAL_MAP_OVERWRITE_APPEND;
						else if (lookup_type == WAL_MAP_LOOKUP_DELETE)
							map_insert_type = WAL_MAP_OVERWRITE_DELETE;
						else
							map_insert_type = WAL_MAP_OVERWRITE_INPLACE;

						insert_rc = map->ops->insert(context, obj, item, map_insert_type);
						wal_debug("wal_map_lookup obj map %p [%d] bucket %p 0x%llx%llx insert_type=%d, insert_rc %d\n",
							  map, bucket_idx, bucket, *(long long *)obj->key->key, *(long long *)(obj->key->key + 8),
							  map_insert_type, insert_rc);

						if (WAL_SUCCESS == insert_rc) {
							if (map_insert_type == WAL_MAP_OVERWRITE_DELETE) {
								item->status = WAL_ITEM_DELETED;
								item->val_size = 0;
								item->recover_status = WAL_ITEM_RECOVER_DELETED;
							} else {
								item->val_size = obj->val->length;
								item->recover_status = WAL_ITEM_RECOVER_OVERWRITE;
							}
						} else {
							if (map_insert_type == WAL_MAP_OVERWRITE_DELETE) {
								wal_debug("deletion item %p status %x  key 0x%llx%llx failed\n", item, item->status,
									  *(long long *)obj->key->key, *(long long *)(obj->key->key + 8));
								assert(0);
								return WAL_ERROR_DELETE_FAIL;
							} else {
								return NULL;
							}
						}
					} else {
						item->addr = addr;
					}
				} else {
					//for read, return the valid item
					if (!(item->status == WAL_ITEM_INVALID) && !(item->status == WAL_ITEM_DELETED)
					    && map->ops->read_item(context, item, obj)) {
						return item;
					} else {
						assert(item);
						wal_debug("invalid/deleted item %p status %d\n", item, item->status);
						return NULL;
					}
				}
				//printf("exit wal_map_lookup map %p[%d] for item %p\n", map, bucket_idx, item);
				return item;
			}
		}
		//not found same key in the bucket.


	}

	//not find the object
	if (lookup_type == WAL_MAP_LOOKUP_INVALIDATE || lookup_type == WAL_MAP_LOOKUP_DELETE) {
		return NULL;
	}

	//insert new object
	if (!is_read) {
		if (!bucket->total_nr_entries || bucket->valid_nr_entries == bucket->total_nr_entries) {
			//create new item here.
			wal_map_item_t *new_item = (wal_map_item_t *)df_calloc(1, sizeof(wal_map_item_t));
			new_item->key = wal_key_clone(obj->key);
			new_item->key_buff_size = obj->key->length;
			new_item->next = NULL;
			new_item->status = WAL_ITEM_VALID;
			new_item->recover_status = WAL_ITEM_RECOVER_NEW;
			new_item->buffer_ptr = context;
			item = new_item;
			if (!addr) {
				if ((insert_rc = map->ops->insert(context, obj, item, map_insert_type))
				    == WAL_SUCCESS) {
					item->val_size = obj->val->length;
				} else {
					assert(0);
					return NULL;
				}
				wal_debug("wal_map_lookup obj map[%d] bucket %p 0x%llx%llx insert_type=%d insert_rc %d\n",
					  bucket_idx, bucket, *(long long *)obj->key->key, *(long long *)(obj->key->key + 8), map_insert_type,
					  insert_rc);
			} else {
				item->addr = addr;
			}
			assert(old_vne == bucket->valid_nr_entries && old_tne == bucket->total_nr_entries);

			if (!bucket->total_nr_entries) { //the first item in the bucket
				bucket->entry = item;
				//bucket link list update
				wal_map_bucket_list_update(map, bucket);
			} else {
				pre_item->next = new_item;	//add new item for the same bucket.
			}
		} else {
			if (bucket->valid_nr_entries < bucket->total_nr_entries) {
				//reuse the same map item with different key
				//printf("1 pre_item 0x%p item %p .. %d .. %d\t", pre_item, item, bucket->valid_nr_entries, bucket->total_nr_entries);
				if (bucket->valid_nr_entries) {
					//printf("2 pre_item 0x%p item %p .. %d .. %d\n", pre_item, item, bucket->valid_nr_entries, bucket->total_nr_entries);

					assert(item || invalid_item); 	//reuse recycled item
				}

				if (invalid_item)	//reuse invalid item entry.
					item = invalid_item;

				item->status = WAL_ITEM_VALID;
				item->recover_status = WAL_ITEM_RECOVER_NEW;
				if (item->key_buff_size < obj->key->length) {
					df_free(item->key);
					item->key = wal_key_clone(obj->key);
					item->key_buff_size = obj->key->length;
				} else {
					item->key->length = obj->key->length;
					memcpy(item->key->key, obj->key->key, obj->key->length);
				}

				if (!addr) {
					if (WAL_SUCCESS == map->ops->insert(context, obj, item, map_insert_type)) {
						item->val_size = obj->val->length;
					} else {
						assert(0);
						return NULL;
					}
				} else {
					item->addr = addr;
				}

				//if(!bucket->valid_nr_entries){//first item for reused item list
				if (TAILQ_NEXT(bucket, link) == NULL) {
					wal_map_bucket_list_update(map, bucket);
				}
				//}

			} else {
				assert(0);
			}
		}
		//update the item list count
		bucket->valid_nr_entries ++;
		if (bucket->total_nr_entries < bucket->valid_nr_entries)
			bucket->total_nr_entries = bucket->valid_nr_entries;

	} else {
		//found nothing for get
		return NULL;
	}
	//printf("exit wal_map_lookup map %p[%d] for item %p\n", map, bucket_idx, item);
	return item;
}

