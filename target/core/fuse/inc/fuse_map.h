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


#ifndef __FUSE_MAP_H
#define __FUSE_MAP_H

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

#include "dragonfly.h"
#include "df_device.h"
#include "df_io_module.h"

#include <limits.h>

typedef struct dfly_key fuse_key_t;
typedef struct dfly_value fuse_val_t;

typedef enum fuse_status_s {
	FUSE_STATUS_CLEAN = 0,
	//FUSE_STATUS_LOOKUPING,
	//FUSE_STATUS_WRITE_PENDING,
	//FUSE_STATUS_DELETE_PENDING,
	FUSE_STATUS_F1_PENDING,
	FUSE_STATUS_F1_SUCCESS,
	FUSE_STATUS_F1_FAIL,
	FUSE_STATUS_F2_RETRY,
	FUSE_STATUS_F2_PENDING
} fuse_status_t;

typedef enum fuse_map_lookup_type_s {
	FUSE_MAP_LOOKUP_STORE,
	FUSE_MAP_LOOKUP_DELETE,
	FUSE_MAP_LOOKUP_FUSE,
	FUSE_MAP_LOOKUP_FUSE_1,
	FUSE_MAP_LOOKUP_FUSE_2,
	FUSE_MAP_LOOKUP_UNKNOWN
} fuse_map_lookup_type_t;

typedef uint32_t (* cleanup_func_t)(void **map);
typedef uint32_t (* hash_func_t)(const char *, int);
typedef uint32_t (* lookup_func_t)(void *map, void *obj, uint32_t lookup_type, void **ret_item);
typedef int64_t (* read_func_t)(void *context, void *item, fuse_object_t *obj);
typedef int (*hash_key_comp_t)(void *k1, void *k2);
typedef int (*invalidate_item_t)(void *context, void *bucket, void *item);

typedef struct fuse_map_ops_s {
	hash_func_t	    	hash;
	lookup_func_t		lookup;
	hash_key_comp_t		key_comp;
	read_func_t			read_item;
	invalidate_item_t	invalidate_item;
	cleanup_func_t		clean_up;
} fuse_map_ops_t;

typedef struct fuse_bucket_s {
	TAILQ_HEAD(, fuse_map_item_s) bucket_list; //link all the collision items
	int32_t nr_entries;	//reflects the nr of the items on the same bucket.
} fuse_bucket_t;

typedef struct fuse_map_s {
	df_lock_t 			map_lock;
	int					map_idx;
	int					nr_buckets;
	fuse_fh				tgt_fh;
	fuse_map_ops_t 	*ops;
	fuse_bucket_t   	*table;
	void				*module_instance_ctx;
	TAILQ_HEAD(fuse_queue, dfly_request) wp_delay_queue, wp_pending_queue, io_pending_queue;
	TAILQ_HEAD(, fuse_map_item_s) 	free_list;
	uint32_t			nr_free_item;
} fuse_map_t;

typedef struct fuse_map_item_s {
	TAILQ_ENTRY(fuse_map_item_s)
	item_list;	//link to map recycle list if clean, or link the collision list on same bucket.
	TAILQ_HEAD(, dfly_request) fuse_waiting_head; //link the waiting fuse request
	fuse_key_t 	 *key;
	fuse_map_t	 *map;
	fuse_bucket_t *bucket;
	dfly_request_t 	 *fuse_req;
	dfly_request_t   *store_del_req;
	struct dfly_value *f1_read_val;
	void 		*large_key_buff;
	int				ref_cnt;

	int				reserved;
	fuse_status_t 	status;
} fuse_map_item_t;

uint32_t fuse_map_lookup(fuse_map_t *map, fuse_object_t *obj,
			 uint32_t lookup_type, fuse_map_item_t **ret_item);
void fuse_init_item(fuse_map_item_t *item, fuse_object_t *obj);
fuse_map_item_t *fuse_map_get_item(fuse_map_t *map);
fuse_map_t *fuse_map_create(int nr_buckets, fuse_map_ops_t *map_ops);
void fuse_map_cleanup(fuse_map_t **pmap);


#ifdef __cplusplus
}
#endif

#endif //____FUSE_MAP_H

