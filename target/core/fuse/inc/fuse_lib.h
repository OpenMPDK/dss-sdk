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

#ifndef __FUSE_LIB_H
#define __FUSE_LIB_H

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
#include <css_latency.h>

#include <df_io_module.h>
#include <df_fuse.h>
#include <module_hash.h>
#include <fuse_map.h>

#define FUSE_TIMEOUT

typedef struct fuse_context_s {
	uint32_t	nr_maps;	//total nr of valid maps, multiple maps per pool.
	fuse_map_t **maps;		//the whole maps array
	int32_t					nr_pools;	//total nr of pools
	dfly_io_module_pool_t **pools;		//pool array.
	int32_t		nr_io_pending;
} fuse_context_t;

int fuse_handle_store_op(fuse_map_t *map, fuse_object_t *obj, int flags);
int fuse_handle_delete_op(fuse_map_t *map, fuse_object_t *obj, int flags);
//int fuse_handle_fuse_op(fuse_map_t * map, fuse_object_t * obj, int flags);
int fuse_handle_F1_op(fuse_map_t *map, fuse_object_t *obj, int flags);
int fuse_handle_F2_op(fuse_map_t *map, fuse_object_t *obj, int flags);

int do_fuse_handle_fuse_op(struct dfly_request *req,
			   fuse_map_item_t *item, int flags);
fuse_map_t *fuse_get_object_map(int pool_id, fuse_object_t *obj);

int fuse_key_compare(fuse_key_t *k1, fuse_key_t *k2);
int fuse_conf_info(const char *conf_name);
void *fuse_get_map_module_ctx(struct dfly_request *req);

#ifdef __cplusplus
}
#endif

#endif //___FUSE_LIB_H
