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

#ifndef __DF_LIST_H
#define __DF_LIST_H
#ifdef __cplusplus
extern "C" {
#endif

#define DFLY_LIST_SUCCESS 				0x0
#define DFLY_LIST_IO_RC_PASS_THROUGH	0x1
#define DFLY_LIST_FAIL					0x2
#define DFLY_LIST_STORE_CONTINUE		0x4
#define DFLY_LIST_STORE_DONE			0x8

#define DFLY_LIST_DEL_CONTINUE			0x10
#define DFLY_LIST_DEL_DONE				0x20

#define DFLY_LIST_READ_DONE				0x40
#define DFLY_LIST_READ_PENDING				0x80

#define DFLY_LIST_OPTION_ROOT_FROM_BEGIN		0x0
#define DFLY_LIST_OPTION_ROOT_FROM_START_KEY	0x1
#define DFLY_LIST_OPTION_PREFIX_FROM_BEGIN		0x2
#define DFLY_LIST_OPTION_PREFIX_FROM_START_KEY	0x3

//Default 25G cache size limit in MB
#define DSS_LISTING_CACHE_DEFAULT_MAX_LIMIT (1024 * 25)

typedef struct list_conf_s {
	int list_enabled ;
	int list_zone_per_pool;
	int list_nr_cores ;
	int list_debug_level;
	int list_op_flag;
	long long list_timeout_ms;
	char list_prefix_head[256];    //prefix screen
} list_conf_t;

#define LIST_ENABLE_DEFAULT     0
#define LIST_NR_ZONES_DEFAULT   8
#define LIST_NR_CORES_DEFAULT   4
#define LIST_DEBUG_LEVEL_DEFAULT    0
#define LIST_TIMEOUT_DEFAULT_MS     0
#define LIST_PREFIX_HEAD            "/meta"

int list_finish(struct dfly_subsystem *pool);
int list_key_update(struct dfly_subsystem *pool, const char *key_str, size_t key_sz, bool is_del,
		    bool is_wal_recovery);
int dfly_list_module_init(struct dfly_subsystem *pool, void *dummy, void *cb, void *cb_arg);
void dfly_list_module_destroy(struct dfly_subsystem *pool, void *args, void *cb, void *cb_arg);

int do_list_item_process(void *ctx, const char *key, int is_leaf);

#define list_log(fmt, args...)\
		DFLY_INFOLOG(DFLY_LOG_LIST, fmt, ##args)

#ifdef __cplusplus
}
#endif

#endif // __DF_LIST_H

