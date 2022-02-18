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


#include "dragonfly.h"
#include "version.h"

extern "C" {
#include "spdk/blobfs.h"
}

struct dragonfly dragonfly_glob;

struct dragonfly *g_dragonfly = &dragonfly_glob;

int dragonfly_core_init(uint32_t nvmf_core)
{
	uint32_t max_core = 0, core;

	g_dragonfly->core[nvmf_core].nvmf_core =  g_dragonfly->core[nvmf_core].current_core = nvmf_core;

	return 0;
}

int dragonfly_core_finish(uint32_t nvmf_core)
{
	return 0;
}

int dragonfly_init(void)
{
	uint32_t max_core = 0, core;

	memset(g_dragonfly, 0, sizeof(struct dragonfly));

	dfly_mm_init();
	dfly_dev_init();

	//dfly_bdev_test();
	dfly_init_numa();

	SPDK_ENV_FOREACH_CORE(core) {
		g_dragonfly->max_core = spdk_max(max_core, core + 1);
	}

	g_dragonfly->disk_stat_table = NULL;
	dfly_ustats_init();
	return 0;
}

int dragonfly_finish(void)
{
	dfly_mm_deinit();
	dfly_deleteAllItems(&g_dragonfly->disk_stat_table);

	if(g_dragonfly->rdd_ctx) {
		rdd_destroy(g_dragonfly->rdd_ctx);
	}

	return 0;
}

int dfly_init(void)
{
	int rc = 0;
	rdd_params_t rdd_params;

	DFLY_NOTICELOG("DSS target software version: %s\n", OSS_TARGET_GIT_VER);

	//Parse Config
	rc = dfly_config_parse();
	if (rc < 0) {
		DFLY_ERRLOG("Config parsing failed\n");
		return -1;
	}

	//Initialize NUMA allocation code
	dfly_init_numa();

	g_dragonfly->disk_stat_table = NULL;
	dfly_ustats_init();

	if (!g_dragonfly->target_pool_enabled) {
		return 0;
	}

	//Initialize memory pool
	dfly_mm_init();

	if(g_dragonfly->blk_map && g_dragonfly->rdb_blobfs_cache_enable) {
		spdk_fs_set_cache_size(g_dragonfly->rdb_blobfs_cache_sz_mb);
	}

	//Initialize devices
	dfly_dev_init();

	if(g_dragonfly->rddcfg) {
		g_dragonfly->rdd_ctx = rdd_init(g_dragonfly->rddcfg, rdd_params);
		if(!g_dragonfly->rdd_ctx) {
			DFLY_ERRLOG("Failed to intialize RDMA direct context\n");
		}
	}

	return 0;
}

// reference: https://gist.github.com/kylef/86784/fe97567ec9baf5c0dce3c7fcbec948e21dfcce09

// THis function can be used as contain function
dict_t *dfly_getItem(dict_t **dict, char *key)
{
	dict_t *d;
	HASH_FIND_STR((*dict), key, d);
	return d;
}


void dfly_delItem(dict_t **dict, char *key)
{
	dict_t *d;
	HASH_FIND_STR((*dict), key, d);
	if (d) {
		HASH_DEL((*dict), d);
		free(d);
	}
}

void dfly_addItem(dict_t **dict, char *key, int value, char *message)
{
	dict_t *d;
	HASH_FIND_STR((*dict), key, d);
	if (d == NULL) {
		d = (dict_t *)malloc(sizeof(struct dict_t_struct));
		strcpy(d->key, key);
		d->value = value;
		strcpy(d->message, message);
		HASH_ADD_STR((*dict), key, d);
	}
}


void dfly_updateItem(dict_t **dict, char *key, int value, char *message)
{
	dict_t *d;
	HASH_FIND_STR((*dict), key, d);
	if (d) {
		d->value = value;
		memset(d->message, 0, strlen(d->message) + 1);
		strcpy(d->message, message);
	}
}

void
dfly_deleteAllItems(dict_t **disk_table)
{
	dict_t *current_item, *tmp;

	HASH_ITER(hh, (*disk_table), current_item, tmp) {
		HASH_DEL((*disk_table), current_item);
		free(current_item);
	}
}

