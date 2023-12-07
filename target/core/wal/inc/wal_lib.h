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


#ifndef __WAL_LIB_H
#define __WAL_LIB_H

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

#include <list.h>

#include <wal_def.h>
#include <module_hash.h>
#include <wal_map.h>
#include <wal_zone_buffer.h>

//WAL Object and record types -----------

//typedef wal_opaque_t wal_key_t;
//typedef wal_opaque_t wal_val_t;

typedef struct wal_object_hdr {
	int16_t sz_blk; //size in count of 4k block
	int16_t key_sz;
	int32_t val_sz;
	int32_t state;
	off64_t addr;	//offset of the object record's key, [hdr|key|data|chksum]
} wal_obj_hdr_t;

typedef struct wal_obj_checksum {
	int32_t chksum;
	int32_t padding_sz;
} wal_obj_checksum_t;

typedef struct wal_record_s {
	wal_obj_hdr_t 		*oh;
	wal_obj_checksum_t 	*oc;
	wal_object_t 			*key;
	wal_object_t 			*val;
} wal_record_t;

typedef struct wal_context_s {
	wal_device_info_t *log_dev[WAL_MAX_LOG_DEV];
	wal_device_info_t *cache_dev;
	df_lock_t			ctx_lock;
	int32_t				nr_pool;
	struct dragonfly_ops 		 *dops;
	struct dragonfly_wal_ops 	 *wops;
	wal_subsystem_t	 *pool_array;
} wal_context_t ;

typedef struct wal_log_device_init_ctx_s {
	int nr_log_devices;
	int curr_device_idx;
	int nr_zones_per_device;
	int zone_size_mb;
	int open_flags; // WAL_OPEN_FORMAT, WAL_OPEN_RECOVER
	const char *device_name;
	wal_device_info_t *log_dev_info;
	wal_sb_t **sb;
	struct dfly_subsystem *pool;
	long long nr_record_recovered;
    void *init_cb_event;
} wal_log_device_init_ctx_t;

// On disk data struct
// A zone is consist of dual buffers, log and flush, switch log to flush when it's full.
// The role indicates it's log or flush buffer, change the value during switch
// |buffer_hdr|record0|record1|record2|...|
// |buffer_hdr|record0|record1|record2|...|

int wal_key_compare(wal_key_t *k1, wal_key_t *k2);
int32_t wal_cache_insert_object(void *context, wal_object_t *obj,
				wal_map_item_t *cache_map_item, wal_map_insert_t insert_type);

int32_t wal_log_insert_object(void *context, wal_object_t *obj,
			      wal_map_item_t *cache_map_item, wal_map_insert_t insert_type, wal_dump_info_t *p_dump_info);

int wal_dfly_object_io(void *ctx, struct dfly_subsystem *pool,
		       wal_object_t *obj, int opc, int op_flags);
int wal_handle_store_op(struct dfly_subsystem *pool,
			wal_object_t *obj, int op_flags);
int wal_handle_delete_op(struct dfly_subsystem *pool,
			 wal_object_t *obj, int op_flags);
int wal_handle_retrieve_op(struct dfly_subsystem *pool,
			   wal_object_t *obj, int op_flags);

int wal_conf_info(const char *conf_name);
int wal_map_invalidate_item(void *context,
			    wal_bucket_t *bucket, wal_map_item_t *item);

wal_zone_t *wal_get_zone_from_req(struct dfly_request *req);

int32_t wal_log_dump_objs(wal_zone_t *cache_zone, wal_dump_info_t *dump_info);
void update_dump_group_info(wal_buffer_t *buffer);

#ifdef __cplusplus
}
#endif

#endif //___WAL_LIB_H
