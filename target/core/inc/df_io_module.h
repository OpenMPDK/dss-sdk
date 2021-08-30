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


#ifndef __DF_IO_MODULE_H
#define __DF_IO_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <df_req.h>

#define df_lock_t 		pthread_mutex_t
#define df_cond_t		pthread_cond_t
#define df_malloc		malloc
#define df_calloc		calloc
#define df_free			free

#define df_lock_init	pthread_mutex_init
#define df_lock		pthread_mutex_lock
#define df_trylock		pthread_mutex_trylock
#define df_unlock		pthread_mutex_unlock


#define DFLY_MAX_NR_POOL 256

//DF io modeule op flags
#define DF_OP_LOCKLESS		0x0010
#define DF_OP_LOCK			0x0020
#define DF_OP_MASK			0x00F0


typedef struct dfly_key wal_key_t, fuse_key_t;
typedef struct dfly_value wal_val_t, fuse_val_t;

typedef int wal_fh, fuse_fh;

struct dfly_io_module_object_s {
	struct dfly_key *key;
	struct dfly_value *val;
	uint32_t	key_hash;
	int			key_hashed;
	void 		*obj_private;
};

typedef struct dfly_io_module_object_s wal_object_t;
typedef struct dfly_io_module_object_s fuse_object_t;

typedef int (* dfly_io_mod_store)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_retrieve)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_delete)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_iter_ctrl)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_iter_read)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_fuse_f1)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_fuse_f2)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_list_open)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_list_close)(void *ctx, void *obj, int flags);
typedef int (* dfly_io_mod_list_read)(void *ctx, void *obj, int flags);

typedef struct dfly_io_module_handler_s {
	dfly_io_mod_store store_handler        ;
	dfly_io_mod_retrieve retrieve_handler     ;
	dfly_io_mod_delete delete_handler       ;
	dfly_io_mod_iter_ctrl iter_ctrl_handler    ;
	dfly_io_mod_iter_read iter_read_handler    ;
	dfly_io_mod_fuse_f1 fuse_f1_handler      ;
	dfly_io_mod_fuse_f2 fuse_f2_handler      ;
	dfly_io_mod_list_open list_open_handler    ;
	dfly_io_mod_list_close list_close_handler   ;
	dfly_io_mod_list_read list_read_handler    ;
} dfly_io_module_handler_t;

typedef struct dfly_io_module_stats_s {
	uint64_t	read_cnt;
	uint64_t	write_cnt;
	uint64_t 	delete_cnt;
	uint64_t	fuse_cnt;
	uint64_t 	total_cnt;
} dfly_io_module_stats_t;
typedef struct dfly_io_module_context_s {
	df_lock_t						ctx_lock;
	void							 *conf;
	void 							 *ctx;
	dfly_io_module_handler_t 		 *io_handlers;
	dfly_io_module_stats_t			s;
	int ssid;
} dfly_io_module_context_t ;

typedef struct dfly_io_module_pool_s {
	struct dfly_subsystem 	*dfly_pool;
	int32_t	dfly_pool_id;
	int32_t nr_zones;	//nr of zones per pool for wal. Or nr of maps for fuse
	int32_t zone_idx;	//idx of zone/map accross the whole zone/maps array for this pool.
} dfly_io_module_pool_t, wal_subsystem_t;


#ifdef __cplusplus
}
#endif

#endif
