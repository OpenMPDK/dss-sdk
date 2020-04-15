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


#ifndef DRAGONFLY_WAL_H
#define DRAGONFLY_WAL_H


/**
 * \file
 * dragonfly WAL definitions
 */


#ifdef __cplusplus
extern "C" {
#endif

#include "spdk/stdinc.h"
#include "spdk/assert.h"


#include "df_def.h"
#include "limits.h"

#define	WAL_SUCCESS					0x0000
#define	WAL_SUCCESS_PASS_THROUGH	0x0001	// cache done, log might fail, write through is expected
#define	WAL_ERROR_LOG_PENDING		0x0003	// cache done, log submitted, waiting for completion, 
// stat machine to change the request states on complete cb
#define	WAL_ERROR_READ_MISS		0x0002	//no cache hit, read through is expected
#define	WAL_ERROR_WRITE_MISS	0x0004	//no cache hit and WAL is busy/no_space, write through is expected
#define	WAL_ERROR_DELETE_MISS	0x0006	//no cache hit and WAL is busy/no_space, write through is expected
#define	WAL_ERROR_DELETE_SUCCESS 0x0008	//deletion done on cache and log, delete through is expected
#define	WAL_ERROR_DELETE_PENDING 0x0009	//deletion done on cache and log, record in log
#define	WAL_ERROR_DELETE_FAIL	 0x000A	//deletion failed.

#define	WAL_ERROR_IO_RETRY		0x0010	//retry the io due to busy (full and flushing, or other reason tbd)
#define	WAL_ERROR_IO			0x0100	//uncertain io error
#define	WAL_ERROR_NO_SUPPORT	0x0200	//KV operation that WAL does not supported

#define WAL_INIT_DONE			0x0400
#define WAL_INIT_PENDING		0x0401
#define WAL_INIT_FAILED			0x0402

#define LIST_INIT_DONE			0x0800
#define LIST_INIT_PENDING		0x0801
#define LIST_INIT_FAILED		0x0802

#define DFLY_INIT_DONE			0x1000
#define DFLY_INIT_PENDING		0x1001
#define DFLY_DEINIT_DONE		0x1002
#define DFLY_DEINIT_PENDING		0x1003


#define WAL_OPEN_FORMAT			0x0001
#define WAL_OPEN_RECOVER		0x0002
#define WAL_OPEN_DEVINFO		0x0004

#define WAL_CACHE_WATERMARK_H	80
#define WAL_CACHE_WATERMARK_L	50

#define WAL_CACHE_FLUSH_THRESHOLD_BY_CNT INT_MAX

#define WAL_NR_ZONE_PER_POOL_DEFAULT 10
#define WAL_ZONE_SZ_MB_BY_DEFAULT 1024
#define WAL_ZONE_SZ_MB_MIN 16
#define WAL_ZONE_SZ_MB_MAX 1024

#define WAL_LOG_BATCH_TIMEOUT_US 100
#define WAL_LOG_BATCH_NR_OBJ 256
#define WAL_LOG_BATCH_TIMEOUT_US_ADJUST 100
#define WAL_LOG_BATCH_NR_OBJ_ADJUST 2
#define WAL_CACHE_ZONE_GROUP_IO_CNT 10000

#define WAL_CACHE_FLUSH_PERIOD_MS 120000	// two mins timeout for cache flush.
#define WAL_LOG_CRASH_TEST	0
#define WAL_NR_LOG_DEV_DEFAULT	0
#define WAL_MAX_LOG_DEV	256

#define WAL_CACHE_OBJECT_SIZE_LIMIT_KB_DEFAULT	64

#define wal_debug(fmt, args...)\
	DFLY_INFOLOG(DFLY_LOG_WAL, fmt, ##args);
//printf(fmt, ##args)

struct dragonfly_wal_ops {
	int (*wal_pre_retrieve)(struct dfly_request *req);
	int (*wal_post_retrieve)(struct dfly_request *req);
	int (*wal_pre_store)(struct dfly_request *req);
	int (*wal_post_store)(struct dfly_request *req);
};

struct df_wal {
	struct dragonfly_wal_ops ops; /**< exported ops **/
	struct dragonfly_ops *dops; /**< expected ops **/
};

typedef struct wal_conf_s {
	int wal_cache_enabled ;
	int wal_log_enabled ;
	int wal_nr_cores;
	int wal_nr_zone_per_pool_default ;
	int wal_zone_sz_mb_default ;
	int wal_cache_utilization_watermark_h ;
	int wal_cache_utilization_watermark_l ;
	int wal_cache_flush_threshold_by_count ;
	int wal_cache_flush_period_ms ;
	int wal_open_flag ;
	int wal_cache_object_size_limit_kb ;
	int wal_log_batch_nr_obj;
	int wal_log_batch_nr_obj_adjust;
	int wal_log_batch_timeout_us;
	int wal_log_batch_timeout_us_adjust;
	int wal_nr_log_dev;
	int wal_log_crash_test;
	char wal_cache_dev_nqn_name[256];
	char wal_log_dev_name[WAL_MAX_LOG_DEV][256];
} wal_conf_t;

/* WAL inward API */
int wal_pre_retrieve(struct dfly_request *req);
int wal_post_retrieve(struct dfly_request *req);
int wal_pre_store(struct dfly_request *req);
int wal_post_store(struct dfly_request *req);

int wal_init(struct dfly_subsystem *pool, struct dragonfly_ops *dops,
	     struct dragonfly_wal_ops *ops, int no_of_zones, int zone_size, int no_of_cores, int open_flag,
	     void *cb, void *cb_arg);

int wal_init_by_conf(struct dfly_subsystem *pool, void *arg,
		     void *cb, void *cb_arg);

int wal_finish(struct dfly_subsystem *pool);
int wal_io(struct dfly_request *req, int wal_op_flags);

void *wal_set_zone_module_ctx(int ssid, int zone_idx, void *set_ctx);
void *wal_get_dest_zone_module_ctx(struct dfly_request *req);

void wal_cache_flush_spdk_proc(void *context);

#ifdef __cplusplus
}
#endif

#endif // DRAGONFLY_WAL_H
