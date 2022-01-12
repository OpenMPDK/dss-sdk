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


#ifndef __WAL_MAP_H
#define __WAL_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#include <wal_def.h>
#include <module_hash.h>

//hash table
/*
typedef struct wal_opaque{
	int32_t 	size;
	void 		* data;
}wal_opaque_t;
*/

typedef struct wal_map_item_s {
	wal_key_t 	 *key;
	uint32_t		key_buff_size;
	uint32_t		val_size;
	off64_t 	addr;
	int32_t 	status;
	int32_t		recover_status;
	void		 *buffer_ptr;
	void		 *large_key_buff;
	struct wal_map_item_s *next;
} wal_map_item_t;

typedef struct wal_bucket_s {
	TAILQ_ENTRY(wal_bucket_s) link;
	wal_map_item_t *entry;
	int32_t valid_nr_entries;
	int32_t total_nr_entries;
} wal_bucket_t;

typedef enum wal_map_lookup_type_s {
	WAL_MAP_LOOKUP_WRITE_APPEND = 0,	//for device log
	WAL_MAP_LOOKUP_WRITE_INPLACE = 1,	//for in mem cache
	WAL_MAP_LOOKUP_DELETE = 2,			//invalid the object in memory as well as log devices.
	WAL_MAP_LOOKUP_READ = 3,			//for lookup only
	WAL_MAP_LOOKUP_INVALIDATE = 4,		//invalid the object in memory.

	WAL_MAP_LOOKUP_WRITE_INPLACE_TRY_BEST = 5,	//for inplace overwrite on cache not ready condition (flushing and cache full)
	// only overwrite existed valid object inplace or return retry code
	WAL_MAP_LOOKUP_UNKNOWN = 6
} wal_map_lookup_type_t;

typedef enum wal_map_insert_type_s {
	WAL_MAP_INSERT_NEW = 0,		//new object
	WAL_MAP_OVERWRITE_APPEND,	//update by append
	WAL_MAP_OVERWRITE_INPLACE,	//update by inplace if possible
	WAL_MAP_OVERWRITE_DELETE,	//insert object by mark it deleted (for append only log devices)
	WAL_MAP_INSERT_UNKNOWN
} wal_map_insert_t;

typedef uint32_t (* hash_func_t)(const char *, int);
typedef int32_t (* insert_func_t)(void *context, wal_object_t *obj, wal_map_item_t *map_item,
				  wal_map_insert_t insert_type);
typedef int64_t (* read_func_t)(void *context, wal_map_item_t *map_item, wal_object_t *obj);

typedef int (*hash_key_comp_t)(wal_key_t *k1, wal_key_t *k2);
typedef int (*invalidate_item_t)(void *context, wal_bucket_t *bucket, wal_map_item_t *item);

typedef struct wal_map_ops {
	hash_func_t	    	hash;
	hash_key_comp_t		key_comp;
	insert_func_t		insert;
	read_func_t			read_item;
	invalidate_item_t	invalidate_item;
} wal_map_ops_t;

typedef struct wal_map_s {
	wal_bucket_t   	*table;
	TAILQ_HEAD(, wal_bucket_s) 	head;
	wal_bucket_t *flush_head;
	int					nr_buckets;
	wal_map_ops_t 		*ops;
} wal_map_t;

wal_key_t *wal_key_clone(wal_key_t *src_key);
int wal_map_reinit(wal_map_t *map);
int wal_map_bucket_iteration(wal_map_t *map);
void wal_map_bucket_list_update(wal_map_t *map, wal_bucket_t *bucket);
wal_map_item_t *wal_map_lookup(void *icontext, wal_map_t *map, wal_object_t *obj,
			       off64_t addr, wal_map_lookup_type_t lookup_type);

wal_map_t *wal_map_create(void *ctx, int nr_buckets, wal_map_ops_t *map_ops);


#ifdef __cplusplus
}
#endif

#endif //____WAL_MAP_H

