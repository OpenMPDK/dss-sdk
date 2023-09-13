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

#include <limits.h>
#include "dragonfly.h"
#include "spdk/string.h"

extern int DFLY_LOG_LEVEL;

static uint64_t DFLY_ONE_GRANT = 1000;
static uint64_t DFLY_ONE_MILL = DFLY_ONE_GRANT * DFLY_ONE_GRANT;
static uint64_t DFLY_ONE_BILL = DFLY_ONE_GRANT * DFLY_ONE_MILL;

//Config Additional helper function
int
dfly_spdk_conf_section_get_intval_default(struct spdk_conf_section *sp, const char *key,
		int default_val)
{
	const char *v;
	int value;

	v = spdk_conf_section_get_nval(sp, key, 0);
	if (v == NULL) {
		return default_val;
	}

	value = (int)strtol(v, NULL, 10);
	return value;
}
//END - Config Additional helper function

static inline uint32_t
dfly_qos_param_conv(uint32_t v, bool k)
{
	assert(v);
	if (k)
		return (spdk_get_ticks_hz() / (DFLY_ONE_GRANT * v));
	else
		return (spdk_get_ticks_hz() / (DFLY_ONE_MILL * v));
}

static int
dfly_qos_parse_host(dfly_prof_t *hp, char *s)
{
	char *h, *t, c;
	uint32_t v;

	assert(hp);

	while (s) {
		h = strchr(s, ':');
		if (!h)
			break;
		if (h - s != 1) {
			DFLY_ERRLOG("Unnamed Qos param %s h = \'%c\'\n", s, *h);
			goto ERROR;
		}
		c = *s;
		s = h + 1;
		if (h = strchr(s, ' ')) {
			v = strtol(s, &t, 10);
			if (s == t) {
				DFLY_ERRLOG("Qos param has to be set\n");
				goto ERROR;
			} else if ((*t != 'M') && (*t != 'K')) {
				DFLY_ERRLOG("Qos unit has to be either \'K\' or \'M\'\n");
				goto ERROR;
			}
			v = v ? dfly_qos_param_conv(v, (*t == 'K')) : 0;
		} else if (h = strchr(s, '\0')) {
			v = strtol(s, &t, 10);
			if (s == t) {
				DFLY_ERRLOG("Qos param has to be set\n");
				goto ERROR;
			}
			v = dfly_qos_param_conv(v, true);
		} else {
			DFLY_ERRLOG("Qos param %c has no value\n", c);
			goto ERROR;
		}

		switch (c) {
		case 'R':
			hp->dfp_credits[DFLY_QOS_RESV] = v;
			break;
		case 'L':
			hp->dfp_credits[DFLY_QOS_LIM] = v;
			break;
		case 'P':
			hp->dfp_credits[DFLY_QOS_PROP] = v;
			break;
		default:
			DFLY_ERRLOG("Illegal QoS param: %c\n", *s);
			goto ERROR;
		}

		while (isspace(*h))
			h++;

		if (*h == '\0')
			break;
		s = h;
	}

	return 0;
ERROR:
	return -1;
}

static void
dfly_qos_init(struct spdk_conf_section *sp)
{
	size_t i = 0;
	bool default_pro = false;

	while (1) {
		/* TODO: There isn't a good way to identify NVMf host for now. hostid
		   could be a good option but needs client coordination. I am using
		   a dummy host name for now. Don't confuse Host
		   here with Subsystem Host.  */
		char *id_name = spdk_conf_section_get_nmval(sp, "Qos_host", i, 0);
		if (!id_name) {
			if (default_pro) {
				break;
			} else {
				DFLY_ERRLOG("Qos host default profile is missing\n");
				return;
			}
		}

		if (!strncmp(id_name, DFLY_PROF_DEF_NAME, strlen(DFLY_PROF_DEF_NAME)))
			default_pro = true;

		/* Cast is really redundent for std C */
		dfly_prof_t *p = (dfly_prof_t *)calloc(1, sizeof(dfly_prof_t));
		if (!p) {
			DFLY_ERRLOG("Failed to allocate for Qos_host\n");
			goto ERROR;
		}

		char *s = spdk_conf_section_get_nmval(sp, "Qos_host", i, 1);
		if (dfly_qos_parse_host(p, s)) {
			free(p);
			goto ERROR;
		}

		p->dfp_nqn = strdup(id_name);
		if (!p->dfp_nqn) {
			DFLY_ERRLOG("Failed to allocate for Qos_host name\n");
			free(p);
		}

		TAILQ_INSERT_HEAD(&g_dragonfly->df_profs, p, dfp_link);
		i++;

		DFLY_NOTICELOG("host: \"%s\" resv %x lim %x prop %x\n",
			       p->dfp_nqn,
			       p->dfp_credits[DFLY_QOS_RESV],
			       p->dfp_credits[DFLY_QOS_LIM],
			       p->dfp_credits[DFLY_QOS_PROP]);
	}

	if (!default_pro)
		DFLY_ERRLOG("Missing Qos default profile\n");
ERROR:
	return;
}

void dfly_config_validate(void)
{

	//rdb_direct listing cannot be enabled with memorry based listing
	if(g_dragonfly->rdb_direct_listing == true) {
		DFLY_ASSERT(g_dragonfly->blk_map ==true);
		//DFLY_ASSERT(g_list_conf.list_enabled == false);
	}

}

void
dfly_config_read(struct spdk_conf_section *sp)
{
	char *str = NULL;
    bool val;

	g_dragonfly->target_pool_enabled = spdk_conf_section_get_boolval(sp, "KV_PoolEnabled", false);
	g_dragonfly->blk_map = spdk_conf_section_get_boolval(sp, "block_translation_enabled", false);
    	g_dragonfly->rdb_bg_core_start = dfly_spdk_conf_section_get_intval_default(sp, "block_translation_bg_core_start", 40);
    	g_dragonfly->rdb_bg_job_cnt = dfly_spdk_conf_section_get_intval_default(sp, "block_translation_bg_job_cnt", 24);
        g_dragonfly->rdb_shard_cnt = dfly_spdk_conf_section_get_intval_default(sp, "block_translation_shard_cnt", 24);
        g_dragonfly->rdb_mtable_cnt = dfly_spdk_conf_section_get_intval_default(sp, "block_translation_mtable_cnt", 10);
        g_dragonfly->rdb_min_mtable_to_merge = dfly_spdk_conf_section_get_intval_default(sp, "block_translation_min_mtable_to_merge", 4);
    	g_dragonfly->rdb_blobfs_cache_enable = spdk_conf_section_get_boolval(sp, "block_translation_blobfs_cache_enable", true);
    	g_dragonfly->rdb_blobfs_cache_sz_mb = dfly_spdk_conf_section_get_intval_default(sp, "block_translation_blobfs_cache_size", 20480);
    	g_dragonfly->num_io_threads = dfly_spdk_conf_section_get_intval_default(sp, "io_threads_per_ss", 8);
    	g_dragonfly->rdb_wal_enable = spdk_conf_section_get_boolval(sp, "rdb_wal_enable", true);
        g_dragonfly->rdb_sync_enable = spdk_conf_section_get_boolval(sp, "rdb_sync_enable", true);
        g_dragonfly->rdb_auto_compaction_enable = spdk_conf_section_get_boolval(sp, "rdb_auto_compaction_enable", false);
        g_dragonfly->rdb_io_debug_level = dfly_spdk_conf_section_get_intval_default(sp, "rdb_io_debug_level", 0);
        g_dragonfly->rdb_stats_intervals_sec = dfly_spdk_conf_section_get_intval_default(sp, "rdb_stats_intervals_sec", 10);
   		g_dragonfly->rdb_sim_timeout = dfly_spdk_conf_section_get_intval_default(sp, "rdb_sim_timeout", 0);
   		g_dragonfly->rdb_sim_io_pre_timeout = dfly_spdk_conf_section_get_intval_default(sp, "rdb_sim_io_pre_timeout", 0);
   		g_dragonfly->rdb_sim_io_post_timeout = dfly_spdk_conf_section_get_intval_default(sp, "rdb_sim_io_post_timeout", 0);

#ifdef DSS_ENABLE_ROCKSDB_KV
	g_dragonfly->rdb_direct_listing = spdk_conf_section_get_boolval(sp, "rdb_direct_listing", true);
#else
	g_dragonfly->rdb_direct_listing = spdk_conf_section_get_boolval(sp, "rdb_direct_listing", false);
#endif//#ifdef DSS_ENABLE_ROCKSDB_KV
	g_dragonfly->rdb_direct_listing_evict_levels = dfly_spdk_conf_section_get_intval_default(sp, "rdb_direct_listing_evict_levels", 2);
	g_dragonfly->rdb_direct_listing_nthreads = dfly_spdk_conf_section_get_intval_default(sp, "rdb_direct_listing_nthreads", 4);
	g_dragonfly->rdb_direct_listing_enable_tpool= spdk_conf_section_get_boolval(sp, "rdb_direct_listing_enable_tpool", false);

#ifdef DSS_OPEN_SOURCE_RELEASE
	g_dragonfly->dss_enable_judy_listing = spdk_conf_section_get_boolval(sp, "dss_enable_judy_listing", false);
	if(g_dragonfly->dss_enable_judy_listing) {
		g_dragonfly->dss_enable_judy_listing = false;
		DFLY_NOTICELOG("Judy based listing not supported for open source release\n");
	}
#else
	g_dragonfly->dss_enable_judy_listing = spdk_conf_section_get_boolval(sp, "dss_enable_judy_listing", true);
	g_dragonfly->dss_judy_listing_cache_limit_size = dfly_spdk_conf_section_get_intval_default(sp, "dss_judy_list_cache_sz_mb", DSS_LISTING_CACHE_DEFAULT_MAX_LIMIT);
#endif
        
	g_dragonfly->num_nw_threads = dfly_spdk_conf_section_get_intval_default(sp, "poll_threads_per_nic", 4);
   	g_dragonfly->mm_buff_count = dfly_spdk_conf_section_get_intval_default(sp, "mm_buff_count", 1024 * 32);
	g_dragonfly->test_nic_bw  = spdk_conf_section_get_boolval(sp, "test_nic_bw", false);
   	g_dragonfly->test_sim_io_timeout = dfly_spdk_conf_section_get_intval_default(sp, "test_sim_io_timeout", 0);
	g_dragonfly->test_sim_io_stall = spdk_conf_section_get_boolval(sp, "test_sim_io_stall", false);

	g_wal_conf.wal_cache_enabled = spdk_conf_section_get_boolval(sp, "wal_cache_enabled", false);
	g_wal_conf.wal_log_enabled = spdk_conf_section_get_boolval(sp, "wal_log_enabled", false);
	g_wal_conf.wal_nr_zone_per_pool_default = spdk_conf_section_get_intval(sp, "wal_nr_zone_per_pool");

	if (g_wal_conf.wal_nr_zone_per_pool_default == -1)
		g_wal_conf.wal_nr_zone_per_pool_default = WAL_NR_ZONE_PER_POOL_DEFAULT;

	g_wal_conf.wal_cache_flush_threshold_by_count = (sp,
			"wal_flush_threshold_by_cnt", INT_MAX);
	g_wal_conf.wal_nr_cores = dfly_spdk_conf_section_get_intval_default(sp, "wal_nr_cores", 1);

	g_wal_conf.wal_zone_sz_mb_default = dfly_spdk_conf_section_get_intval_default(sp,
					    "wal_cache_zone_sz_mb", WAL_ZONE_SZ_MB_BY_DEFAULT);
	g_wal_conf.wal_cache_utilization_watermark_h = dfly_spdk_conf_section_get_intval_default(sp,
			"wal_cache_utilization_watermark_high", WAL_CACHE_WATERMARK_H);
	g_wal_conf.wal_cache_utilization_watermark_l = dfly_spdk_conf_section_get_intval_default(sp,
			"wal_cache_utilization_watermark_low", WAL_CACHE_WATERMARK_L);

	g_wal_conf.wal_cache_object_size_limit_kb = dfly_spdk_conf_section_get_intval_default(sp,
			"wal_cache_object_size_limit_kb", WAL_CACHE_OBJECT_SIZE_LIMIT_KB_DEFAULT);

	g_wal_conf.wal_log_batch_nr_obj = dfly_spdk_conf_section_get_intval_default(sp,
					  "wal_log_batch_nr_obj", WAL_LOG_BATCH_NR_OBJ);
	g_wal_conf.wal_log_batch_timeout_us = dfly_spdk_conf_section_get_intval_default(sp,
					      "wal_log_batch_timeout_us", WAL_LOG_BATCH_TIMEOUT_US);

	g_wal_conf.wal_log_batch_nr_obj_adjust = dfly_spdk_conf_section_get_intval_default(sp,
			"wal_log_batch_nr_obj_adjust", WAL_LOG_BATCH_NR_OBJ_ADJUST);
	g_wal_conf.wal_log_batch_timeout_us_adjust = dfly_spdk_conf_section_get_intval_default(sp,
			"wal_log_batch_timeout_us_adjust", WAL_LOG_BATCH_TIMEOUT_US_ADJUST);

	g_wal_conf.wal_cache_flush_period_ms = dfly_spdk_conf_section_get_intval_default(sp,
					       "wal_cache_flush_period_ms", WAL_CACHE_FLUSH_PERIOD_MS);

	g_wal_conf.wal_open_flag = dfly_spdk_conf_section_get_intval_default(sp,
				   "wal_log_open_flag", WAL_OPEN_FORMAT);

	g_wal_conf.wal_log_crash_test = dfly_spdk_conf_section_get_intval_default(sp,
					"wal_log_crash_test", WAL_LOG_CRASH_TEST);

	g_wal_conf.wal_nr_log_dev = 0;
	for (int i = 0; i < WAL_MAX_LOG_DEV; i++) {
		char *log_name = spdk_conf_section_get_nmval(sp, "wal_log_dev_name", i, 0);
		if (!log_name)
			break;

		snprintf(g_wal_conf.wal_log_dev_name[i], strlen(log_name) + 1, "%s", log_name);
		g_wal_conf.wal_nr_log_dev ++;
		//printf("dfly_config_read: log_dev_name %s\n", g_wal_conf.wal_log_dev_name[i]);
	}

	str = spdk_conf_section_get_val(sp, "wal_cache_dev_nqn_name");
	if (str) {
		snprintf(g_wal_conf.wal_cache_dev_nqn_name, strlen(str) + 1, "%s", str);
	}

	str = spdk_conf_section_get_val(sp, "fuse_nqn_name");
	if (str) {
		snprintf(g_fuse_conf.fuse_nqn_name, strlen(str) + 1, "%s", str);
	}
	g_fuse_conf.fuse_enabled = spdk_conf_section_get_boolval(sp, "fuse_enabled", false);
	g_fuse_conf.wal_enabled = g_wal_conf.wal_cache_enabled;

	g_fuse_conf.nr_maps_per_pool = dfly_spdk_conf_section_get_intval_default(sp,
				       "fuse_nr_maps_per_pool", 1);
	g_fuse_conf.fuse_nr_cores = dfly_spdk_conf_section_get_intval_default(sp, "fuse_nr_cores", 1);
	g_fuse_conf.fuse_debug_level = dfly_spdk_conf_section_get_intval_default(sp, "fuse_debug_level",
				       SPDK_LOG_INFO);
	g_fuse_conf.fuse_timeout_ms = dfly_spdk_conf_section_get_intval_default(sp, "fuse_timeout_ms",
				      2000);

	// list service parameters.
	str = spdk_conf_section_get_val(sp, "list_prefix_head");
	if (str) {
		assert(strlen(str) <= 255);
		snprintf(g_list_conf.list_prefix_head, strlen(str) + 1, "%s", str);
		g_list_prefix_head_size = strlen(str);
	} else {
		snprintf(g_list_conf.list_prefix_head, 5, "meta");
		g_list_conf.list_prefix_head[4] = 0;
		g_list_prefix_head_size = 4;
	}

	g_list_conf.list_enabled = spdk_conf_section_get_boolval(sp, "list_enabled", true);
	g_list_conf.list_zone_per_pool = dfly_spdk_conf_section_get_intval_default(sp,
					 "list_zones_per_pool", 1);
	g_list_conf.list_nr_cores = dfly_spdk_conf_section_get_intval_default(sp, "list_nr_cores", 1);
	g_list_conf.list_debug_level = dfly_spdk_conf_section_get_intval_default(sp, "list_debug_level",
				       SPDK_LOG_INFO);
	g_list_conf.list_timeout_ms = dfly_spdk_conf_section_get_intval_default(sp, "list_timeout_ms", 0);

	g_dragonfly->req_lat_to = dfly_spdk_conf_section_get_intval_default(sp, "latency_threshold_s", 0);
	g_dragonfly->enable_latency_profiling = spdk_conf_section_get_boolval(sp, "enable_latency_profiling", false);
	g_dragonfly->df_qos_enable = spdk_conf_section_get_boolval(sp, "QoS", false);

	if (g_dragonfly->df_qos_enable) {
		pthread_mutex_init(&g_dragonfly->df_ses_lock, NULL);
		TAILQ_INIT(&g_dragonfly->df_profs);
		TAILQ_INIT(&g_dragonfly->df_sessions);
		dfly_qos_init(sp);
	}

    val = spdk_conf_section_get_boolval(sp, "kvtrans_disk_data_store", true);
    set_kvtrans_disk_data_store(val);

	val = spdk_conf_section_get_boolval(sp, "kvtrans_disk_meta_store", true);
	set_kvtrans_disk_meta_store(val);

	val = spdk_conf_section_get_boolval(sp, "kvtrans_ba_meta_sync", true);
	set_kvtrans_ba_meta_sync_enabled(val);

    return;
}

void
dfly_config_parse_rdd(struct spdk_conf_section *sp) {

	int i;
	int nips = 0;
	char *listen_info;

	char *tlip;
	char *tlhost, *tlport;

	DFLY_ASSERT(g_dragonfly->rddcfg == NULL);

	for(i=0; ;i++) {//Count Number of Listen Sections
		listen_info = spdk_conf_section_get_nmval(sp, "Listen", i, 0);
		if(!listen_info) {
			break;
		}
		nips++;
	}

	if(nips == 0) {
		//No Listen IPs in the section
		DFLY_ERRLOG("Did not find any Listen info in rdd section\n");
		return;
	}


	g_dragonfly->rddcfg = calloc(1, sizeof(rdd_cfg_t) + (nips *sizeof(rdd_cinfo_t))); 

    g_dragonfly->rddcfg->num_cq_cores_per_ip = dfly_spdk_conf_section_get_intval_default(sp, "num_cq_cores_per_ip", 1);
    g_dragonfly->rddcfg->max_sgl_segs = dfly_spdk_conf_section_get_intval_default(sp, "max_sgl_segs", 8);

	g_dragonfly->rddcfg->n_ip = 0;

	g_dragonfly->rddcfg->conn_info = (rdd_cinfo_t *)(g_dragonfly->rddcfg + 1);

	for(i=0; ;i++) {//Parse Listen IPs
		listen_info = spdk_conf_section_get_nmval(sp, "Listen", i, 0);
		if(!listen_info) {
			break;
		}

		tlip = strdup(listen_info);

		if(spdk_parse_ip_addr(tlip, &tlhost, &tlport) < 0) {
			DFLY_ERRLOG("Unable to Parse listen address %d %s\n", i, tlip);

			free(tlip);
			for(int j =0; j < i; j++) {
				free(g_dragonfly->rddcfg->conn_info[j].ip);
				free(g_dragonfly->rddcfg->conn_info[j].port);
			}
			free(g_dragonfly->rddcfg);
		}

		g_dragonfly->rddcfg->n_ip++;
		g_dragonfly->rddcfg->conn_info[i].ip = strdup(tlhost);
		g_dragonfly->rddcfg->conn_info[i].port = strdup(tlport);
		free(tlip);
	}

	return;
}

int
dfly_config_parse(void)
{
	struct spdk_conf_section *sp;

	char *s = getenv("DFLY_DEBUG");
	if (s && !strncmp(s, "on", 2))
		DFLY_LOG_LEVEL = 1;

	sp = spdk_conf_find_section(NULL, "DFLY");
	if (sp != NULL) {
		dfly_config_read(sp);
	}

	sp = spdk_conf_find_section(NULL, "RDD");
	if (sp != NULL) {
		dfly_config_parse_rdd(sp);
	}

	dfly_config_validate();

	return 0;
}
