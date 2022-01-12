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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>

#include <../../inc/list.h>

#define ATOMIC_ADD(counter, number)                 __sync_fetch_and_add(&(counter), (number))
#define ATOMIC_INC(counter)                         __sync_fetch_and_add(&(counter), 1)
#define ATOMIC_READ(counter)                        __sync_fetch_and_add(&(counter), 0)
#define ATOMIC_INC_FETCH(counter)                   __sync_add_and_fetch(&(counter), 1)
#define ATOMIC_DEC_FETCH(counter)                   __sync_sub_and_fetch(&(counter), 1)
#define ATOMIC_RESET(counter)                       __sync_and_and_fetch(&(counter), 0)
#define ATOMIC_BOOL_COMP_CHX(val, old_val, new_val) __sync_bool_compare_and_swap(&(val), (old_val), (new_val))

#define KB                  (1024)
#define MB                  (1048576)

#define WAL_DEV_NAME_SZ	128

#define WAL_DEV_TYPE_DRAM	0x0
#define WAL_DEV_TYPE_BLK	0x1

#define	WAL_INIT_FH	-1

#define WAL_MAX_ALLOC_MB		2048
#define WAL_MAX_BUCKET			1048576
#define MAX_ZONE				256

#define WAL_MAGIC	0x20180116
#define WAL_VER		0x01
#define WAL_SB_CLEAN	0x00
#define WAL_SB_DIRTY	0x01

//WAL device open flags
#define WAL_OPEN_FORMAT			0x0001
#define WAL_OPEN_RECOVER		0x0002
#define WAL_OPEN_DEVINFO		0x0004

//object alignment 1KB
#define WAL_ALIGN				1024
#define WAL_ALIGN_OFFSET		10
#define WAL_ALIGN_MASK			0xFFFFFFFFFFFFFC00

//zone alignment 1MB
#define WAL_ZONE_ALIGN			MB
#define WAL_ZONE_OFFSET			20
#define WAL_ZONE_MASK_32		0xFFF00000
#define WAL_ZONE_MASK_64		0xFFFFFFFFFFF00000

#define WAL_ERROR_HANDLE		0x1001
#define WAL_ERROR_SIGNATURE		0x1003
#define WAL_ERROR_INSERT_OBJ	0x1004
#define WAL_ERROR_BUFFER_ROLE	0x1005
#define WAL_ERROR_BDEV			0x1006


#define WAL_ERROR_TOO_MANY_ZONE		0x2001
#define WAL_ERROR_NO_SPACE			0x2002

#define WAL_ERROR_BAD_ADDR			0x3001
#define WAL_ERROR_BAD_CHKSUM		0x3002
#define WAL_ERROR_RD_LESS			0x3003
#define WAL_ERROR_WR_LESS			0x3004
#define WAL_ERROR_INVALID_SEQ		0x3005
#define WAL_ERROR_RECORD_SZ			0x3006


#define WAL_SB_OFFSET			0x0
#define WAL_BUFFER_ROLE_BIT			0x1
#define WAL_BUFFER_TYPE_BIT			0x2

#define WAL_BUFFER_ROLE_LOG			0x0
#define WAL_BUFFER_ROLE_FLUSH		0x1
#define WAL_BUFFER_ROLE_DRAM		0x2

#define WAL_ITEM_VALID			0x0	//
#define WAL_ITEM_INVALID		0x1	//VALID -> INVALID
#define WAL_ITEM_FLUSHING		0x2	//VALID -> FLUSHING
#define WAL_ITEM_FLUSHED		0x3 //FLUSHING -> FLUSHED

#define WAL_FLUSH_STATE_LOG_FULL		0x1
#define WAL_FLUSH_STATE_FLUSH_READY		0x2
#define WAL_FLUSH_STATE_FLUSH_EXIT		0x8

#define WAL_POLL_FLUSH_DONE				0x1
#define WAL_POLL_FLUSH_DOING			0x2
#define WAL_POLL_FLUSH_EXIT				0x4


#define WAL_CACHE_IO_REGULAR	0x0
#define WAL_CACHE_IO_PRIORITY	0x1

#define WAL_DBG(fmt, args...) if(__debug_wal) fprintf(__wal_dbg_fd, fmt, ##args)

typedef struct wal_key_s {
	void     *key;
	uint16_t length;
} wal_key_t;

typedef struct wal_val_s {
	void	 *value;
	uint32_t length;
	int32_t  offset;
} wal_val_t;

typedef struct wal_object_s {
	wal_key_t *key;
	wal_val_t *val;
	uint32_t	key_hash;
	int			key_hashed;
	void 		*obj_private;
} wal_object_t;

typedef struct wal_object_hdr {
	int16_t sz_blk; //size in count of 4k block
	int16_t key_sz;
	int32_t val_sz;
	int32_t state;
	off64_t addr;
} wal_obj_hdr_t;

typedef struct wal_obj_checksum {
	int32_t chksum;
	int32_t padding_sz;
} wal_obj_checksum_t;

#define BUFFER_HDR_PADDING_SZ 984
typedef struct wal_buffer_hdr_s {
	off64_t start_addr;	//start addr of the buffer, include the hdr
	off64_t	end_addr;	//end addr of the buffer, aligned to WAL_ALIGN
	off64_t	curr_pos;	//current addr of data region.
	int32_t	role; 		//log/flush
	int32_t sequence;	//sequence of this buffer for this operation round, increment during buffer switch,
	//the object recorded in this buffer should checksum with this sequence num.
	int32_t reserved[2];
	char padding[BUFFER_HDR_PADDING_SZ];
} wal_buffer_hdr_t;

typedef struct wal_map_item_s {
	wal_key_t 	 *key;
	uint32_t		key_buff_size;
	uint32_t		val_size;
	off64_t 	addr;
	int32_t 	status;
	void		 *buffer_ptr;
	struct wal_map_item_s *next;
} wal_map_item_t;

typedef struct wal_bucket_s {
	struct list_head link;
	wal_map_item_t *entry;
	int32_t valid_nr_entries;
	int32_t total_nr_entries;
} wal_bucket_t;

typedef struct wal_map_s {
	wal_bucket_t   	*table;
	wal_bucket_t *flush_head;
	int					nr_buckets;
} wal_map_t;

struct wal_zone_s;

typedef struct wal_buffer_s {
	wal_buffer_hdr_t hdr;
	int			 fh;		//fh of the WAL device.
	wal_map_t *map;
	char 			*record_buffer;
	struct wal_zone_s *zone;
	long			nr_objects_inserted;
	long 			nr_pending_io;

	long			utilization;
	long			total_space_KB;
	long			inc_wmk_change;
	long			dec_wmk_change;
} wal_buffer_t;

typedef struct wal_zone_s {
	int32_t			zone_id; //the index of __wal_zones arrays
	wal_buffer_t *log;
	wal_buffer_t *flush;
} wal_zone_t;


typedef struct wal_space_s {
	int32_t addr_mb;
	int32_t size_mb;
} wal_space_t;

//ensure the size of the log sb size is 4k even.
//To introduce or remove field(s), adjust the padding size accordingly!!!
#define LOG_SB_PADDING_SIZE	2008
typedef struct wal_superblock {
	int32_t size;
	int32_t version;
	uint32_t magic;
	int32_t status;//TBD, clean, dirty.
	int32_t init_timestamp;
	int32_t	alignment;
	int32_t offset;	//data offset
	int32_t	reserved[2];
	int32_t nr_zone;
	wal_space_t zone_space[MAX_ZONE];
	char 	padding[LOG_SB_PADDING_SIZE];
} wal_sb_t;

typedef struct wal_device_info {
	char		dev_name[WAL_DEV_NAME_SZ];
	int		fh;
	wal_sb_t 	*sb;
	wal_zone_t	 **zones;
	wal_map_t **maps;
} wal_device_info_t;

int sync_bdev_write(int handle, const void *buff, uint64_t offset, uint64_t nbytes,
		    void *cb, void *cb_arg)
{
	return pwrite(handle, buff, nbytes, offset);
}

int sync_bdev_read(int handle, void *buff, uint64_t offset, uint64_t nbytes,
		   void *cb, void *cb_arg)
{
	return pread(handle, buff, nbytes, offset);
}

inline int sync_bdev_open(const char *pathname, int flags)
{
	return open(pathname, flags);
}

inline int sync_bdev_close(int dev_handle)
{
	close(dev_handle);
	return 0;
}

int wal_log_show_sb(wal_sb_t *sb)
{
	int i = 0;
	printf("version: %d\nmagic: 0x%lx\nstatus: %d\n",
	       sb->version, (unsigned long)sb->magic, sb->status);
	printf("init_time: %d\nalignment: %d Bytes\ndata offset: 0x%lx MB\n",
	       sb->init_timestamp, sb->alignment, (unsigned long)sb->offset);
	printf("nr_zone: %d\n", sb->nr_zone);
	for (; i < sb->nr_zone; i++)
		printf("addr:	0x%lx\tsize:	0x%lx mb\n",
		       (unsigned long)sb->zone_space[i].addr_mb, (unsigned long)sb->zone_space[i].size_mb);

	return 0;
}

typedef int (* read_object_t)(struct wal_buffer_s *buffer, wal_object_t *obj, int64_t src_addr,
			      int sz, int is_data_read);
typedef int (*get_object_size_t)(wal_object_t *obj);

int wal_log_get_object_size(wal_object_t *obj)
{
	int record_sz = obj->key->length + obj->val->length +
			sizeof(wal_obj_hdr_t) + sizeof(wal_obj_checksum_t);
	record_sz += (WAL_ALIGN - 1);
	record_sz &= (WAL_ALIGN_MASK);
	return record_sz;
}

//1. allocate space in log buffer
//2. write the object in the log buffer
int __wal_obj_hdr_sz = sizeof(wal_obj_hdr_t);
int	__wal_obj_chksum_sz = sizeof(wal_obj_checksum_t);
int __buffer_alignment = WAL_ALIGN;

int wal_log_read_object(wal_buffer_t *buffer, wal_object_t *obj,
			int64_t src_addr, int sz, int is_data_read)
{
	wal_buffer_hdr_t *hdr = &buffer->hdr;
	off64_t addr = (off64_t)src_addr;
	off64_t addr2 = 0;
	wal_obj_hdr_t oh;
	wal_obj_checksum_t ch;
	int rc = 0;

	if ((rc = sync_bdev_read(buffer->fh, (void *)&oh, addr, sizeof(wal_obj_hdr_t), NULL, NULL))
	    < sizeof(wal_obj_hdr_t)) {
		rc = - WAL_ERROR_RD_LESS;
		goto done;
	}

	addr += rc;

	//verify the object.
	addr2 = addr + oh.key_sz + oh.val_sz;
	if (addr2 >= buffer->hdr.end_addr) {
		rc = - WAL_ERROR_BAD_ADDR;
		goto done;
	}

	rc = sync_bdev_read(buffer->fh, (void *)&ch, addr2, sizeof(wal_obj_checksum_t), NULL, NULL);
	if (rc < sizeof(wal_obj_checksum_t)) {
		rc = -WAL_ERROR_RD_LESS;
		goto done;
	}

	addr2 += rc;

	if (ch.chksum != hdr->sequence) {
		rc = -WAL_ERROR_BAD_CHKSUM;
		goto done;
	}

	//allocate key and data buffer if not enough.
	if (obj->key->length < oh.key_sz) {
		if (obj->key->key)
			free(obj->key->key);
		obj->key->key = malloc(oh.key_sz);
		obj->key->length = oh.key_sz;
	}

	if (is_data_read) {
		if (obj->val->length < oh.val_sz) {
			if (obj->val->value)
				free(obj->val->value);
			obj->val->value = malloc(oh.val_sz);
			obj->val->offset = 0;
			obj->val->length = oh.val_sz;
		}
	} else {
		obj->val->length = oh.val_sz;
	}

	//read key
	rc = sync_bdev_read(buffer->fh, (void *)obj->key->key, addr, oh.key_sz, NULL, NULL);
	if (rc < oh.key_sz) {
		rc = -WAL_ERROR_RD_LESS;
		goto done;
	}

	//read data
	if (is_data_read && oh.val_sz) {
		addr += rc;
		rc = sync_bdev_read(buffer->fh, (void *)obj->val->value, addr, oh.val_sz, NULL, NULL);
		if (rc < oh.val_sz) {
			rc = -WAL_ERROR_RD_LESS;
			goto done;
		}

		obj->val->length = oh.val_sz;
		addr += oh.val_sz;
	} else {
		addr += (rc + oh.val_sz);
	}

	rc = addr - src_addr;
	if ((rc + WAL_ALIGN - 1) / __buffer_alignment != oh.sz_blk) {
		rc = -WAL_ERROR_RECORD_SZ;
		goto done;
	}

	obj->key->length = oh.key_sz;
	rc = oh.sz_blk * __buffer_alignment;

done:
	printf("hdr role %d s_addr 0x%llx key 0x%llx oh.state 0x%x val_size %d seq %ld rc %d\n",
	       hdr->role, hdr->start_addr, *(long long *)obj->key->key, oh.state, oh.val_sz, ch.chksum, rc);
	return rc;

}

wal_device_info_t dev_info;
wal_sb_t sb;

int wal_log_iterate_object(wal_buffer_t *buffer,
			   wal_object_t *obj, off64_t *log_pos, int is_data_read)
{
	wal_buffer_hdr_t *hdr = &buffer->hdr;
	int rc = 0;
	off64_t addr = * log_pos;

	if (!hdr->sequence) {
		return -WAL_ERROR_INVALID_SEQ;
	}

	if (addr == hdr->start_addr) { // for the first object
		addr = hdr->start_addr + sizeof(wal_buffer_hdr_t) + WAL_ALIGN - 1;
		addr = addr & WAL_ALIGN_MASK;
	}

	* log_pos = addr;
	rc = wal_log_read_object(buffer, obj, addr, 0, is_data_read);
	if (rc > 0)
		* log_pos += rc;

	return rc;

}

int wal_key_compare(wal_key_t *k1, wal_key_t *k2)
{
	if (k1->length != k2->length)
		return -1;

	return memcmp(k1->key, k2->key, k1->length);
}

typedef enum wal_map_insert_type_s {
	WAL_MAP_INSERT_NEW = 0,		//new object
	WAL_MAP_OVERWRITE_APPEND,	//update by append
	WAL_MAP_OVERWRITE_INPLACE,	//update by inplace if possible
	WAL_MAP_OVERWRITE_DELETE,	//insert object by mark it deleted (for append only log devices)
	WAL_MAP_INSERT_UNKNOWN
} wal_map_insert_t;

typedef enum wal_map_lookup_type_s {
	WAL_MAP_LOOKUP_WRITE_APPEND,	//for device log
	WAL_MAP_LOOKUP_WRITE_INPLACE,	//for in mem cache
	WAL_MAP_LOOKUP_DELETE,			//invalid the object in memory as well as log devices.
	WAL_MAP_LOOKUP_READ,			//for lookup only
	WAL_MAP_LOOKUP_INVALIDATE,		//invalid the object in memory.

	WAL_MAP_LOOKUP_WRITE_INPLACE_TRY_BEST,	//for inplace overwrite on cache not ready condition (flushing and cache full)
	// only overwrite existed valid object inplace or return retry code
	WAL_MAP_LOOKUP_UNKNOWN
} wal_map_lookup_type_t;

wal_key_t *wal_key_clone(wal_key_t *src_key)
{
	wal_key_t *clone = (wal_key_t *)calloc(1, sizeof(wal_key_t));
	clone->length = src_key->length;
	clone->key = malloc(clone->length);
	memcpy(clone->key, src_key->key, clone->length);
	return clone;
}

uint32_t hash_sdbm(const char *data, int sz)
{
	uint32_t hash = 0;
	int count = 0;
	while (count < sz) {
		hash = data[count] + (hash << 6) + (hash << 16) - hash;
		count++;
	}
	return hash;
}

//need to rewrite this function: TBD to be debug!!! here
int32_t wal_map_recover_object(void *context,
			       wal_map_t *map, wal_object_t *obj, int read_data)
{
	wal_buffer_t *log_buffer = (wal_buffer_t *)context;
	wal_buffer_hdr_t *buff_hdr = &log_buffer->hdr;
	obj->key_hash = hash_sdbm((const char *)obj->key->key, obj->key->length);
	int bucket_idx = obj->key_hash % map->nr_buckets;
	wal_bucket_t *bucket = &map->table[bucket_idx];
	int nr_items = bucket->total_nr_entries;
	wal_map_item_t *item = bucket->entry;
	wal_map_item_t *pre_item = NULL;
	wal_map_item_t *invalid_item = NULL;

	off64_t pos = buff_hdr->curr_pos;

	int cache_size_kb = 0;
	int overwrite = 0;
	int val_buffer_reuse = 0;
	int rc = 0;

	while (nr_items--) {

		if (item->status == WAL_ITEM_INVALID) { //been deleted/invalidated with enough
			invalid_item = item;
			pre_item = item;
			item = item->next;
			continue;
		}

		if (wal_key_compare(obj->key, item->key)) {
			pre_item = item;
			item = item->next;
		} else {
			//found the same object
			item->status = WAL_ITEM_INVALID;
			bucket->valid_nr_entries --;
			if (!obj->val->length) { //deletion record
				ATOMIC_DEC_FETCH(log_buffer->nr_objects_inserted);
				printf("wal_map_recover_object delete zone[%d] map[%d] 0x%llx%llx\n", log_buffer->zone->zone_id,
				       bucket_idx, *(long long *)item->key->key, *(long long *)(item->key->key + 8));
				return rc;

			} else {
				printf("wal_map_recover_object overwrite zone[%d] map[%d] 0x%llx%llx\n", log_buffer->zone->zone_id,
				       bucket_idx, *(long long *)item->key->key, *(long long *)(item->key->key + 8));

			}
			overwrite = 1;
			break;
		}
	}

	if (!item && invalid_item) {
		item = invalid_item;
	}

	if (item) {
		if (!overwrite) { //new key
			if (item->key_buff_size < obj->key->length) {
				free(item->key->key);
				item->key = wal_key_clone(obj->key);
				item->key_buff_size = obj->key->length;
			} else {
				memcpy(item->key->key, obj->key->key, obj->key->length);
				item->key->length = obj->key->length;
			}
		}

		if (read_data) {
			//reuse the existed buffer if possible
			if (item->val_size >= obj->val->length) {
				pos = item->addr;
				item->val_size = obj->val->length;
				val_buffer_reuse = 1;
			} else {
				;
				//allocate new data value buffer
			}
		}
	}

	if (!item) { //need a new map item entry
		wal_map_item_t *new_item = (wal_map_item_t *)calloc(1, sizeof(wal_map_item_t));
		new_item->key = wal_key_clone(obj->key);
		new_item->key_buff_size = obj->key->length;
		new_item->next = NULL;
		new_item->buffer_ptr = context;
		if (pre_item)
			pre_item->next = new_item;
		else
			bucket->entry = new_item; // first item in the bucket

		item = new_item;

	}

	if (!overwrite)
		printf("wal_map_recover_object new zone[%d] map[%d] 0x%llx%llx val_sz %d\n",
		       log_buffer->zone->zone_id,
		       bucket_idx, *(long long *)item->key->key, *(long long *)(item->key->key + 8), obj->val->length);

	item->status = WAL_ITEM_VALID;
	bucket->valid_nr_entries ++;
	if (bucket->valid_nr_entries > bucket->total_nr_entries)
		bucket->total_nr_entries = bucket->valid_nr_entries;

	if (read_data) {
		memcpy((void *)pos, obj->val->value + obj->val->offset, obj->val->length);
		item->addr = pos;
	}

	if (!overwrite)
		ATOMIC_INC(log_buffer->nr_objects_inserted);

	if (read_data && !val_buffer_reuse) {
		//mem cache pos
		cache_size_kb = (obj->val->length + __buffer_alignment - 1) >> WAL_ALIGN_OFFSET;
		buff_hdr->curr_pos += (cache_size_kb << WAL_ALIGN_OFFSET);
	}

	return rc;

}

void wal_map_bucket_list_update(wal_map_t *map, wal_bucket_t *bucket)
{
	list_add_tail(&bucket->link, &map->head);
}

wal_map_t *wal_map_create(int nr_buckets)
{
	int i = 0;
	wal_map_t *map = (wal_map_t *)calloc(1, sizeof(wal_map_t));
	wal_bucket_t *bucket;
	map->nr_buckets = nr_buckets;
	map->table = (wal_bucket_t *)calloc(nr_buckets, sizeof(wal_bucket_t));
	bucket = map->table;
	INIT_LIST_HEAD(&map->head);
	for (; i < nr_buckets; i++, bucket ++) {
		INIT_LIST_HEAD(&bucket->link);
	}
	return map;
}

int wal_log_recover_zone_buffer(wal_device_info_t *dev_info,
				off64_t pos, int32_t sz_mb, wal_zone_t *zone)
{
	wal_buffer_t *buffer1, * buffer2;
	off64_t next_pos = 0;
	buffer1 = (wal_buffer_t *)calloc(1, sizeof(wal_buffer_t));
	buffer2 = (wal_buffer_t *)calloc(1, sizeof(wal_buffer_t));

	int read_sz = sync_bdev_read(dev_info->fh, &buffer1->hdr, pos, sizeof(wal_buffer_hdr_t), NULL,
				     NULL);
	if (read_sz < sizeof(wal_buffer_hdr_t))
		return -WAL_ERROR_RD_LESS;

	next_pos = pos + ((sz_mb / 2) << 20);
	read_sz = sync_bdev_read(dev_info->fh, &buffer2->hdr, next_pos, sizeof(wal_buffer_hdr_t), NULL,
				 NULL);
	if (read_sz < sizeof(wal_buffer_hdr_t))
		return -WAL_ERROR_RD_LESS;

	printf("wal_recover_zone_buffer: zone %d start 0x%llx pos 0x%llx sz 0x%lx mb\n",
	       zone->zone_id, (long long)buffer1->hdr.start_addr, (long long)pos, (unsigned long)sz_mb);

	buffer1->hdr.curr_pos = buffer1->hdr.start_addr;
	buffer2->hdr.curr_pos = buffer2->hdr.start_addr;

	buffer1->map = wal_map_create(WAL_MAX_BUCKET);
	buffer2->map = wal_map_create(WAL_MAX_BUCKET);

	buffer1->zone = buffer2->zone = zone;

	if (!(buffer1->hdr.role & WAL_BUFFER_ROLE_FLUSH)) {
		if (buffer1->hdr.sequence < buffer2->hdr.sequence)
			return -WAL_ERROR_BUFFER_ROLE;
		zone->log = buffer1;
		zone->flush = buffer2;
	} else {
		if (buffer1->hdr.sequence > buffer2->hdr.sequence)
			return -WAL_ERROR_BUFFER_ROLE;
		zone->log = buffer2;
		zone->flush = buffer1;
	}
	//zone->log->fh = open(dev_info->dev_name, O_RDWR|O_LARGEFILE);
	//zone->flush->fh = open(dev_info->dev_name, O_RDWR|O_LARGEFILE);
	//zone->log->fh = dup(dev_info->fh);
	//zone->flush->fh = dup(dev_info->fh);

	zone->log->fh = zone->flush->fh = dev_info->fh;

	wal_object_t obj;
	off64_t log_pos = zone->flush->hdr.start_addr;
	obj.key = (wal_key_t *)calloc(1, sizeof(wal_key_t));
	obj.key->key = malloc(4096);
	obj.key->length = 4096;
	obj.val = (wal_val_t *)calloc(1, sizeof(wal_val_t));
	while (wal_log_iterate_object(zone->flush, &obj, &log_pos, 0) >= 0) {
		//insert into map table
		wal_map_recover_object(zone->flush, zone->flush->map, &obj, 0);
		obj.key->length = 4096;
	}

	log_pos = zone->log->hdr.start_addr;
	while (wal_log_iterate_object(zone->log, &obj, &log_pos, 0) >= 0) {
		//insert into map table
		wal_map_recover_object(zone->log, zone->log->map, &obj, 0);
		obj.key->length = 4096;
	}

	printf("\tlog: t %ld s %llx c %llx e %llx\n",
	       (long)zone->log->hdr.sequence,
	       (long long)zone->log->hdr.start_addr,
	       (long long)zone->log->hdr.curr_pos,
	       (long long)zone->log->hdr.end_addr);
	printf("\tflush: t %ld s %llx c %llx e %llx\n",
	       (long)zone->flush->hdr.sequence,
	       (long long)zone->flush->hdr.start_addr,
	       (long long)zone->flush->hdr.curr_pos,
	       (long long)zone->flush->hdr.end_addr);

	return 0;

}

int wal_log_recover_zones(wal_device_info_t *dev_info, wal_sb_t *sb)
{
	wal_zone_t *zone = NULL;
	//wal_buffer_hdr_t buff_hdr ;
	//int32_t read_sz = 0 ;
	int32_t i = 0, rc = 0;
	//wal_fh fd = dev_info->fh;
	int nr_zone = sb->nr_zone;
	off64_t pos = sb->offset;
	pos = pos << 20;
	dev_info->zones = (wal_zone_t **)malloc(sizeof(wal_zone_t *) * MAX_ZONE);
	for (; i < nr_zone; i++) {
		pos = sb->zone_space[i].addr_mb;
		pos <<= 20;
		zone = (wal_zone_t *)malloc(sizeof(wal_zone_t));
		zone->zone_id = i;
		dev_info->zones[i] = zone;
		rc = wal_log_recover_zone_buffer(dev_info, pos, sb->zone_space[i].size_mb, zone);
		if (rc)
			break;
	}

	return rc;
}

int main(int argc, char **argv)
{
	char *bdev_path = argv[1];
	int fh = sync_bdev_open(bdev_path, O_RDONLY);
	if (fh < 0) {
		printf("open %s fail with error %d\n", bdev_path, errno);
		return -1;
	}
	dev_info.fh = fh;
	sprintf(dev_info.dev_name, "%s\0", bdev_path, strlen(bdev_path));

	int32_t sb_sz = sizeof(wal_sb_t);
	int rc = sync_bdev_read(fh, &sb, WAL_SB_OFFSET, sb_sz, NULL, NULL);
	if (rc < sb_sz) {
		printf("sync_log_sb_read rc %d errno %d\n", rc, errno);
		goto exit;
	}

	dev_info.sb = &sb;
	wal_log_show_sb(&sb);

	wal_log_recover_zones(&dev_info, &sb);
exit:
	sync_bdev_close(fh);
}
