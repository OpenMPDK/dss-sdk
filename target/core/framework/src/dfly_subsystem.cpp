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
#include "nvmf_internal.h"

#include "apis/dss_net_module.h"

typedef void (*df_ss_init_next_fn)(void *event);

struct df_subsys_process_event_s {
	struct dfly_subsystem *subsys;
    struct spdk_thread *src_thread;
	bool initialize;
	dss_module_type_t curr_module;
	df_subsystem_event_processed_cb cb;
	void *cb_arg;
	int cb_status;
};

struct df_ss_cb_event_s * df_ss_cb_event_allocate(struct dfly_subsystem *ss, df_module_event_complete_cb cb, void *cb_arg, void *event_private)
{
	struct df_ss_cb_event_s *ss_cb_event;

	ss_cb_event = (struct df_ss_cb_event_s *)calloc(1, sizeof(struct df_ss_cb_event_s));
	DFLY_ASSERT(ss);
	DFLY_ASSERT(ss_cb_event);

	DFLY_ASSERT(cb);
	ss_cb_event->ss = ss;
	ss_cb_event->df_ss_cb = cb;
	ss_cb_event->df_ss_cb_arg = cb_arg;
	ss_cb_event->df_ss_private = event_private;
	ss_cb_event->src_thread = spdk_get_thread();

	return ss_cb_event;
}

void df_ss_cb_event_complete(struct df_ss_cb_event_s *ss_cb_event)
{
	DFLY_ASSERT(ss_cb_event && ss_cb_event->df_ss_cb);

    spdk_thread_send_msg(ss_cb_event->src_thread, ss_cb_event->df_ss_cb, ss_cb_event->df_ss_cb_arg);

	return;
}

struct dfly_subsystem *dfly_get_subsystem_no_lock(uint32_t ssid)
{
	return &(g_dragonfly->subsystems[ssid]);
}

int dfly_get_nvmf_ssid(struct spdk_nvmf_subsystem *ss)
{
	return ss->id;
}

struct dfly_subsystem *dfly_get_subsystem(uint32_t ssid)
{
	struct dfly_subsystem *subsystem = &(g_dragonfly->subsystems[ssid]);

	DFLY_ASSERT(subsystem);
	if (subsystem->initialized) {
		pthread_mutex_lock(&subsystem->subsys_lock);//Lock Begin

		if (subsystem->shutting_down) {
			pthread_mutex_unlock(&subsystem->subsys_lock);//Release lock
			return NULL;
		}
		subsystem->ref_cnt++;

		pthread_mutex_unlock(&subsystem->subsys_lock);//Lock End
		//TODO: KD References before init. Move out and make this assert valid
		//} else {
		//	DFLY_ASSERT((*(char*)subsystem == 0) && (!memcmp(subsystem, ((char *)subsystem) + 1, (sizeof(struct dfly_subsystem) - 1))));
	}

	return subsystem;
}

struct dfly_subsystem *dfly_put_subsystem(uint32_t ssid)
{
	struct dfly_subsystem *subsystem = &(g_dragonfly->subsystems[ssid]);

	DFLY_ASSERT(subsystem);
	DFLY_ASSERT(subsystem->initialized);

	pthread_mutex_lock(&subsystem->subsys_lock);

	subsystem->ref_cnt--;

	//Process refcount zero, outstanding waiters on lock
	//Make sure lock is required only for control path

	pthread_mutex_unlock(&subsystem->subsys_lock);
}

extern fuse_conf_t g_fuse_conf;
extern wal_conf_t g_wal_conf;
extern list_conf_t g_list_conf;


typedef int (*dss_mod_init_fn)(struct dfly_subsystem *arg1, void *arg2, df_module_event_complete_cb cb, void *cb_arg);
typedef void (*dss_mod_deinit_fn)(struct dfly_subsystem *arg1, void *arg2, df_module_event_complete_cb cb, void *cb_arg);

struct df_ss_mod_init_s {
	dss_mod_init_fn mod_init_fn;
	dss_mod_deinit_fn mod_deinit_fn;
	void *arg;
	df_ss_init_next_fn cb;
	bool mod_enabled;
};

void df_subsys_update_dss_enable(uint32_t ssid, bool ss_dss_enabled)
{
	struct dfly_subsystem *df_subsys = NULL;

	df_subsys = dfly_get_subsystem_no_lock(ssid);//Preallocated structure

	DFLY_ASSERT(df_subsys);

	df_subsys->dss_enabled = ss_dss_enabled;

	//Verify input and update
	if((df_subsys->dss_enabled && (g_dragonfly->target_pool_enabled == OSS_TARGET_DISABLED)) || \
	   (!df_subsys->dss_enabled && (g_dragonfly->target_pool_enabled == OSS_TARGET_ENABLED))) {
		if(!g_dragonfly->target_pool_enabled) {
			DFLY_WARNLOG("Disabling DSS target on subsystem %d based on global config\n", ssid);
			df_subsys->dss_enabled = false;
		} else {
			DFLY_NOTICELOG("Overriding global flag to enable DSS for Subsystem %d\n", ssid);
		}
	}

	return;

}

void df_subsystem_parse_conf(struct spdk_nvmf_subsystem *subsys, struct spdk_conf_section *subsys_sp)
{

	uint32_t ssid = subsys->id;
	struct dfly_subsystem *df_subsys = NULL;
	int num_kvt_threads;

	df_subsys = dfly_get_subsystem_no_lock(ssid);//Preallocated structure

	df_subsys->iomem_dev_numa_aligned = spdk_conf_section_get_boolval(subsys_sp, "iomem_dev_numa_aligned", true);
	df_subsys->dss_enabled = spdk_conf_section_get_boolval(subsys_sp, "dss_enabled", true);
	df_subsys->dss_kv_mode = spdk_conf_section_get_boolval(subsys_sp, "dss_kv_mode", true);
	df_subsys->dss_iops_perf_mode = spdk_conf_section_get_boolval(subsys_sp, "dss_iops_perf_mode", false);
	df_subsys->use_io_task = true;//TODO: Decide if this needs to be configurable?
	// Count number of namespaces
	for (num_kvt_threads = 0;; num_kvt_threads++)
	{
		char *tmp;
		tmp = spdk_conf_section_get_nmval(subsys_sp, "Namespace", num_kvt_threads, 0);
		if (!tmp)
		{
			break;
		}
	}
	df_subsys->num_kvt_threads = dfly_spdk_conf_section_get_intval_default(subsys_sp, "num_kvt_threads", num_kvt_threads);

	df_subsys_update_dss_enable(ssid, df_subsys->dss_enabled);

	return;
}

uint32_t df_subsystem_enabled(uint32_t ssid)
{
	struct dfly_subsystem *df_subsys = NULL;

	df_subsys = dfly_get_subsystem_no_lock(ssid);//Preallocated structure

	DFLY_ASSERT(df_subsys);

	return df_subsys->dss_enabled;

}

void _dfly_subsystem_process_next(void *vctx);

//Array indices should match enum for module
static struct df_ss_mod_init_s module_initializers[DSS_MODULE_END + 1] = {
	{NULL,NULL, NULL, NULL, false},
	{dfly_lock_service_subsys_start, dfly_lock_service_subsystem_stop, NULL, _dfly_subsystem_process_next, true},//DSS_MODULE_LOCK
	{(dss_mod_init_fn)fuse_init_by_conf, NULL,  NULL, _dfly_subsystem_process_next, false},//DSS_MODULE_FUSE
	{(dss_mod_init_fn)dss_net_module_subsys_start, (dss_mod_deinit_fn)dss_net_module_subsys_stop,  NULL, _dfly_subsystem_process_next, false},//DSS_MODULE_NET
	{dfly_io_module_subsystem_start, dfly_io_module_subsystem_stop, NULL, _dfly_subsystem_process_next, true},//DSS_MODULE_IO
	{dss_kvtrans_module_subsystem_start, dss_kvtrans_module_subsystem_stop, NULL, _dfly_subsystem_process_next, true},//DSS_MODULE_KVTRANS
	{(dss_mod_init_fn)dfly_list_module_init, (dss_mod_deinit_fn)dfly_list_module_destroy, NULL, _dfly_subsystem_process_next, false},//DSS_MODULE_LIST
	{(dss_mod_init_fn)wal_init_by_conf, NULL, NULL, _dfly_subsystem_process_next, false},//DSS_MODULE_WAL
	{NULL, NULL, NULL, NULL, false}
};

int_least64_t dss_get_subsys_listener_count(void *spdk_subsys)
{
	struct spdk_nvmf_subsystem *spdk_nvmf_ss = (struct spdk_nvmf_subsystem *)spdk_subsys;
	uint32_t lcount = 0;
	struct spdk_nvmf_subsystem_listener *listener = TAILQ_FIRST(&spdk_nvmf_ss->listeners);
	while(listener) {
		lcount++;
		listener = TAILQ_NEXT(listener, link);
	}
	return lcount;
}

int dss_fill_dss_net_dev_info(void *spdk_subsys, dss_net_mod_dev_info_t *net_dev_info)
{
	struct spdk_nvmf_subsystem *spdk_nvmf_ss = (struct spdk_nvmf_subsystem *)spdk_subsys;
	struct spdk_nvmf_subsystem_listener *listener;
	int fill_count = 0;

	TAILQ_FOREACH(listener, &spdk_nvmf_ss->listeners, link) {
		net_dev_info[fill_count].num_nw_threads = g_dragonfly->num_nw_threads;
		net_dev_info[fill_count].ip = strdup(listener->trid->traddr);
		net_dev_info[fill_count].dev_name = strdup(listener->trid->traddr);
		// TAILQ_FOREACH(listener, spdk_nvmf_ss->listeners, link) {
		// 	//net_dev_info[fill_count].dev_name = strdup();
		// }
		fill_count++;
	}
	return fill_count;
}

void _dfly_subsystem_process_next(void *vctx)
{
	struct df_subsys_process_event_s *ss_event = (struct df_subsys_process_event_s *)vctx;
	dss_module_t **ss_module_p;
	int mod_index = 0;

	DFLY_ASSERT(ss_event->src_thread == spdk_get_thread());

	if(ss_event->curr_module == DSS_MODULE_START_INIT) {
		if (ss_event->initialize)
		{
			if (ss_event->subsys->dss_enabled)
			{
				if (dss_enable_net_module(ss_event->subsys))
				{
					module_initializers[DSS_MODULE_NET].mod_enabled = true;
					// Prepare config for net module
					int listener_count, fill_count;

					dss_net_module_config_t *c;
					listener_count = dss_get_subsys_listener_count(ss_event->subsys->parent_ctx);
					DSS_ASSERT(listener_count != 0);
					c = calloc(1, sizeof(dss_net_module_config_t) + (listener_count * sizeof(dss_net_mod_dev_info_t)));
					DSS_ASSERT(c != NULL);
					// TODO: Handle allocation failure
					c->count = listener_count;
					fill_count = dss_fill_dss_net_dev_info(ss_event->subsys->parent_ctx, c->dev_list);
					DSS_ASSERT(fill_count == listener_count);
					module_initializers[DSS_MODULE_NET].arg = (void *)c;
				}
				if (!ss_event->subsys->dss_iops_perf_mode)
				{
					module_initializers[DSS_MODULE_IO].mod_enabled = true;
				}
				else
				{
					module_initializers[DSS_MODULE_IO].mod_enabled = false;
				}
				dss_subsystem_initialize_io_devices((dss_subsystem_t *)ss_event->subsys);
				// Disable other modules
				module_initializers[DSS_MODULE_WAL].mod_enabled = false;
				module_initializers[DSS_MODULE_FUSE].mod_enabled = false;
				module_initializers[DSS_MODULE_LOCK].mod_enabled = false;
			}
			else
			{
				module_initializers[DSS_MODULE_NET].mod_enabled = false;
				// Other modules are configured already
			}
		}
		else
		{
			dss_subsystem_deinit_io_devices((dss_subsystem_t *)ss_event->subsys);
			// TODO: Deinit Config
		}
	}

	for (mod_index = ss_event->curr_module + 1; mod_index < DSS_MODULE_END; mod_index++) {
		if (module_initializers[mod_index].mod_enabled == false) {
			DFLY_INFOLOG(DFLY_LOG_SUBSYS, "Skipping module with index %d\n", mod_index);
			continue;
		}
		if(ss_event->initialize) {
			module_initializers[mod_index].mod_init_fn(ss_event->subsys,
				module_initializers[mod_index].arg, module_initializers[mod_index].cb, ss_event);
		} else {
			module_initializers[mod_index].mod_deinit_fn(ss_event->subsys,
				module_initializers[mod_index].arg, module_initializers[mod_index].cb, ss_event);
		}
		if (module_initializers[mod_index].cb) {
			DFLY_INFOLOG(DFLY_LOG_SUBSYS, "Breaking Subsystem init for cb index:%d\n", mod_index);
			break;
		}
		DFLY_INFOLOG(DFLY_LOG_SUBSYS, "Init module without cb index:%d\n", mod_index);
	}
	ss_event->curr_module = (dss_module_type_t)mod_index;
	if (mod_index == DSS_MODULE_END) {
		if(ss_event->initialize) {
			int i;
			for(i=DSS_MODULE_START_INIT; i < DSS_MODULE_END; i++) {
				//Reset arg after subsystem init to prepare for next subsys
				module_initializers[i].arg = NULL;
			}
			pthread_mutex_init(&ss_event->subsys->subsys_lock, NULL);
			ss_event->subsys->initialized = true;
		} else {
			if(ss_event->subsys->iotmod) {
				dss_io_task_module_end(ss_event->subsys->iotmod);
			}
			ss_event->subsys->initialized = false;
		}
		ss_event->cb((struct spdk_nvmf_subsystem *)ss_event->subsys->parent_ctx,
				  ss_event->cb_arg, ss_event->cb_status);
	}
	return;
}

int dfly_subsystem_init(void *vctx, dfly_spdk_nvmf_io_ops_t *io_ops,
			df_subsystem_event_processed_cb cb, void *cb_arg, int cb_status)
{

	struct spdk_nvmf_subsystem *spdk_nvmf_ss = (struct spdk_nvmf_subsystem *)vctx;
	struct dfly_subsystem *dfly_subsystem = NULL;

	int rc = 0;

	struct spdk_nvmf_subsystem_listener *listener = TAILQ_FIRST(&spdk_nvmf_ss->listeners);
	rdd_params_t rdd_params;

	struct df_subsys_process_event_s *ss_mod_init_next = NULL;

	assert(spdk_nvmf_ss);

	if (df_subsystem_enabled(spdk_nvmf_ss->id) &&
	    spdk_nvmf_ss->subtype == SPDK_NVMF_SUBTYPE_NVME) {
		spdk_nvmf_ss->oss_target_enabled = OSS_TARGET_ENABLED;
	} else {
		spdk_nvmf_ss->oss_target_enabled = OSS_TARGET_DISABLED;
		return DFLY_INIT_DONE;
	}

	assert(spdk_nvmf_ss->id < MAX_SS);

	dfly_subsystem = dfly_get_subsystem(spdk_nvmf_ss->id);//Preallocated structure

	if (dfly_subsystem->initialized == true) {
		DFLY_NOTICELOG("Initialized subsystem %s\n", spdk_nvmf_ss->subnqn);
		return DFLY_INIT_DONE;
	}

	ss_mod_init_next = (struct df_subsys_process_event_s *)calloc(1,
			   sizeof(struct df_subsys_process_event_s));
	if (!ss_mod_init_next) {
		return -ENOMEM;
	}

	ss_mod_init_next->subsys = dfly_subsystem;
	ss_mod_init_next->initialize = true;
	DFLY_ASSERT(cb);
	ss_mod_init_next->cb = cb;
	ss_mod_init_next->cb_arg = cb_arg;
	ss_mod_init_next->cb_status = cb_status;
	ss_mod_init_next->curr_module = DSS_MODULE_START_INIT;
    ss_mod_init_next->src_thread = spdk_get_thread();

	module_initializers[DSS_MODULE_IO].arg = io_ops;

	DFLY_ASSERT(dfly_subsystem);
	DFLY_ASSERT(dfly_subsystem->initialized == false);

	dfly_subsystem->id = spdk_nvmf_ss->id;
	dfly_subsystem->name = spdk_nvmf_ss->subnqn;
	dfly_subsystem->parent_ctx = vctx;

	dfly_ustat_init_subsys_stat(dfly_subsystem, spdk_nvmf_ss->subnqn);

	dfly_subsystem->list_initialized_nbdev = 0;

	TAILQ_INIT(&dfly_subsystem->df_ctrlrs);
	pthread_mutex_init(&dfly_subsystem->ctrl_lock, NULL);

	if(!dss_subsystem_kv_mode_enabled((dss_subsystem_t *)dfly_subsystem)) {
		DSS_NOTICELOG("KV translation disabled by config for %s\n", spdk_nvmf_ss->subnqn);
		dfly_subsystem->dss_kv_mode = false;
		module_initializers[DSS_MODULE_KVTRANS].mod_enabled = false;
	}

	if (g_fuse_conf.fuse_enabled) {
		module_initializers[DSS_MODULE_FUSE].arg = &g_fuse_conf;
		module_initializers[DSS_MODULE_FUSE].mod_enabled = true;
		DFLY_ASSERT(spdk_nvmf_ss->subnqn);
		snprintf(g_fuse_conf.fuse_nqn_name, strlen(spdk_nvmf_ss->subnqn) + 1, "%s", spdk_nvmf_ss->subnqn);
	}

	if (g_list_conf.list_enabled) {
		module_initializers[DSS_MODULE_LIST].mod_enabled = true;
		module_initializers[DSS_MODULE_LIST].arg = &g_list_conf;
	}

	if (g_wal_conf.wal_cache_enabled) {
		module_initializers[DSS_MODULE_WAL].mod_enabled = true;
		module_initializers[DSS_MODULE_WAL].arg = &g_wal_conf;
	}

	_dfly_subsystem_process_next(ss_mod_init_next);

	return DFLY_INIT_PENDING;
}

int dfly_subsystem_destroy(void *vctx, df_subsystem_event_processed_cb cb, void *cb_arg, int cb_status)
{
	struct spdk_nvmf_subsystem *spdk_nvmf_ss = (struct spdk_nvmf_subsystem *)vctx;
	struct dfly_subsystem *dfly_subsystem = NULL;
	int rc = 0;

	struct df_subsys_process_event_s *ss_mod_deinit_next = NULL;

	assert(spdk_nvmf_ss);
	assert(spdk_nvmf_ss->id < MAX_SS);

	if (spdk_nvmf_ss->oss_target_enabled == OSS_TARGET_DISABLED) {
		return DFLY_DEINIT_DONE;
	}

	dfly_subsystem = dfly_get_subsystem_no_lock(spdk_nvmf_ss->id);//Preallocated structure

	assert(spdk_nvmf_ss->id < MAX_SS);
	DFLY_ASSERT(dfly_subsystem);

	if (dfly_subsystem->initialized == false) {
		DFLY_NOTICELOG("Deinitialized subsystem %s\n", spdk_nvmf_ss->subnqn);
		return DFLY_DEINIT_DONE;
	}

	ss_mod_deinit_next = (struct df_subsys_process_event_s *)calloc(1,
			   sizeof(struct df_subsys_process_event_s));
	if (!ss_mod_deinit_next) {
		DFLY_ASSERT(0);
		return DFLY_DEINIT_DONE;
	}

	ss_mod_deinit_next->subsys = dfly_subsystem;
	ss_mod_deinit_next->initialize = false;
	DFLY_ASSERT(cb);
	ss_mod_deinit_next->cb = cb;
	ss_mod_deinit_next->cb_arg = cb_arg;
	ss_mod_deinit_next->cb_status = cb_status;
	ss_mod_deinit_next->curr_module = DSS_MODULE_START_INIT;
    ss_mod_deinit_next->src_thread = spdk_get_thread();

	//TODO: Deinit FUSE
	//TODO: Deinit WAL
	dfly_ustat_remove_subsys_stat(dfly_subsystem);

	//dfly_lock_service_subsystem_stop(dfly_subsystem);

	//dfly_io_module_subsystem_stop(dfly_subsystem);

	_dfly_subsystem_process_next(ss_mod_deinit_next);
	return DFLY_DEINIT_PENDING;
	//dfly_subsystem->initialized = false;
}

void *dfly_subsystem_list_device(struct dfly_subsystem *ss, void **dev_list, uint32_t *nr_dev)
{
	return ss->kd_ctx->kd_fn_table->list_device(ss->kd_ctx, dev_list, nr_dev);

}

void *dss_module_get_subsys_ctx(dss_module_type_t type, void *ss)
{
	struct dfly_subsystem *df_ss = (struct dfly_subsystem *) ss;

	DSS_ASSERT(type > DSS_MODULE_START_INIT);
	DSS_ASSERT(type < DSS_MODULE_END);
	void **mlist = (void **)&df_ss->mlist;

	return mlist[type];
}

void dss_module_set_subsys_ctx(dss_module_type_t type, void *ss, void *ctx)
{
	struct dfly_subsystem *df_ss = (struct dfly_subsystem *) ss;

	DSS_ASSERT(type > DSS_MODULE_START_INIT);
	DSS_ASSERT(type < DSS_MODULE_END);

	void **mlist = (void **)&df_ss->mlist;

	mlist[type] = ctx;
	return;
}

bool dss_subsystem_kv_mode_enabled(dss_subsystem_t *ss)
{
	struct dfly_subsystem *df_ss = (struct dfly_subsystem *) ss;
	return df_ss->dss_kv_mode;

}

bool dss_subsystem_use_io_task(dss_subsystem_t *ss)
{
	struct dfly_subsystem *df_ss = (struct dfly_subsystem *) ss;
	return df_ss->use_io_task;

}

dss_io_task_module_t *dss_subsytem_get_iotm_ctx(dss_subsystem_t *ss)
{
	struct dfly_subsystem *df_ss = (struct dfly_subsystem *) ss;
	return df_ss->iotmod;

}

dss_io_dev_status_t dss_subsystem_initialize_io_devices(dss_subsystem_t *ss)
{
	struct dfly_subsystem *df_ss = (struct dfly_subsystem *) ss;
	struct spdk_nvmf_subsystem *nvmf_subsys = (struct spdk_nvmf_subsystem *)df_ss->parent_ctx;
	int i;
	dss_io_dev_status_t rc;

	df_ss->num_io_devices = nvmf_subsys->max_nsid;
	df_ss->dev_arr = (dss_device_t **)calloc(nvmf_subsys->max_nsid, sizeof(dss_device_t *));
	DSS_ASSERT(df_ss->dev_arr);

	for (i = 0; i < nvmf_subsys->max_nsid; i++) {
		const char *bdev_name = spdk_bdev_get_name(nvmf_subsys->ns[i]->bdev);
		DSS_ASSERT(bdev_name);
		rc = dss_io_device_open(bdev_name, DSS_BLOCK_DEVICE, &df_ss->dev_arr[i]);
		if(rc != DSS_IO_DEV_STATUS_SUCCESS) {
			dss_subsystem_deinit_io_devices(ss);
			return rc;
		}
		DSS_ASSERT(df_ss->dev_arr[i]);
	}
	return DSS_IO_DEV_STATUS_SUCCESS;
}

void dss_subsystem_deinit_io_devices(dss_subsystem_t *ss)
{
	struct dfly_subsystem *df_ss = (struct dfly_subsystem *) ss;
	struct spdk_nvmf_subsystem *nvmf_subsys = (struct spdk_nvmf_subsystem *)df_ss->parent_ctx;
	int i;

	for (i = 0; i < nvmf_subsys->max_nsid; i++) {
		if(df_ss->dev_arr[i]) {
			dss_io_device_close(df_ss->dev_arr[i]);
		}
	}
	free(df_ss->dev_arr);
	df_ss->dev_arr = NULL;
	df_ss->num_io_devices = 0;

	return;
}

#define DSS_SUBSYS_DEFAULT_MAX_REQS (8192)

//TODO: This can changes if transports can be added dynamically to subsystem. Handle
uint64_t dss_subystem_get_max_inflight_requests(dss_subsystem_t *ss)
{
	uint64_t max_requests = DSS_SUBSYS_DEFAULT_MAX_REQS;
	struct dfly_subsystem *df_ss = (struct dfly_subsystem *) ss;
	struct spdk_nvmf_subsystem *nvmf_subsys = (struct spdk_nvmf_subsystem *)df_ss->parent_ctx;

	struct spdk_nvmf_transport *tgt_transport;
	struct spdk_nvmf_subsystem_listener *ss_listener;

	tgt_transport = spdk_nvmf_transport_get_first(nvmf_subsys->tgt);
	if(!tgt_transport) {
		DSS_NOTICELOG("No transport found for subsystem id:[%d] nqn:[%s]\n", nvmf_subsys->id, nvmf_subsys->subnqn);
		return max_requests;
	}

	//Reset max requests
	max_requests = 0;

	do {
		TAILQ_FOREACH(ss_listener, &nvmf_subsys->listeners, link) {
			if(ss_listener->transport == tgt_transport) {//TODO: Check assumption that Pointer should be same?
				DSS_NOTICELOG("Accounting %s transport for %s nqn max_requests\n", tgt_transport->ops->name, nvmf_subsys->subnqn);
				max_requests += tgt_transport->opts.num_shared_buffers;
				break;
			}
		}
	} while((tgt_transport = spdk_nvmf_transport_get_next(tgt_transport)) != NULL);

	//Note: It is possible for max_requests value to be higher that the required requets.
	//		it is okay since we dont want to run out of resources during IO path
	//		This will avoid need to queue requests inside DSS and handle at nvmf_tgt layer
	return max_requests;
}
