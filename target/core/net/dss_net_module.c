/**
 * The Clear BSD License
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 * BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dragonfly.h"

#include "dss.h"
#include "apis/dss_module_apis.h"
#include "apis/dss_net_module.h"


struct dss_net_module_s {
    dss_module_t *module;//Used only to retrieve module pointer during init
    char *dev_name;
    char *ip;
    uint64_t ref_cnt;
    pthread_mutex_t net_module_lock;
    TAILQ_ENTRY(dss_net_module_s) mgr_mlist;
};

struct dss_net_module_ss_node_s {
    dss_module_t *module;
    TAILQ_ENTRY(dss_net_module_ss_node_s) subsys_mlist;
};

typedef struct dss_net_module_subsys_s {
    TAILQ_HEAD(,dss_net_module_ss_node_s) module_list;
} dss_net_module_subsys_t;

typedef struct dss_net_mod_mgr_s {
    pthread_mutex_t net_module_mgr_lock;
    TAILQ_HEAD(, dss_net_module_s) net_mod_list;
} dss_net_mod_mgr_t;

dss_net_mod_mgr_t g_net_mod_mgr = {
    .net_module_mgr_lock = PTHREAD_MUTEX_INITIALIZER,
    .net_mod_list = TAILQ_HEAD_INITIALIZER(g_net_mod_mgr.net_mod_list)
};

//dss_net_module_config_t
void *dss_net_module_thread_instance_init(void *mctx, void *inst_ctx, int inst_index)
{
    return NULL;
}

void *dfly_net_module_thread_instance_destroy(void *mctx, void *inst_ctx)
{
    return NULL;
}
int dss_net_module_req_process(void *ctx, dss_request_t *req)
{
    dss_net_request_process(req);
    return DFLY_MODULE_REQUEST_QUEUED;
}

dss_module_ops_t g_net_module_ops = {
    .module_init_instance_context = dss_net_module_thread_instance_init,
    .module_rpoll = dss_net_module_req_process,
    .module_cpoll = NULL,
    .module_gpoll = NULL,
    .find_instance_context = NULL,
    .module_instance_destroy = dfly_net_module_thread_instance_destroy
};

/**
 * @brief Increment ref count for caller holding lock
 *
 * @param m Network module context
 */
void _dss_net_module_get(dss_net_module_t *m)
{
    m->ref_cnt++;
}

/**
 * @brief Decrement ref count for caller holding lock
 *
 * @param m Network module context
 * @return true if ref count is zero after decrememt
 * @return false if ref count is non-zero after decrement
 */
bool _dss_net_module_put(dss_net_module_t *m)
{
    DSS_ASSERT(m->ref_cnt);
    m->ref_cnt--;
    return (m->ref_cnt)?false:true;
}

dss_module_status_t dss_net_module_init(char *net_dev_name, char *ip, dss_net_module_t **net_module)
{
    int rc;
    dss_module_status_t status = DSS_MODULE_STATUS_SUCCESS;
    dss_net_module_t *nm;
    pthread_mutex_lock(&g_net_mod_mgr.net_module_mgr_lock);

    nm = TAILQ_FIRST(&g_net_mod_mgr.net_mod_list);

    while(nm) {
        if(!strcmp(nm->dev_name, net_dev_name)) {
            break;
        }
        nm = TAILQ_NEXT(nm, mgr_mlist);
    }

    if(!nm) {
        nm = (dss_net_module_t *)calloc(1, sizeof(dss_net_module_t));
        if(!nm) {
            DSS_ERRLOG("Net module init failed\n");
            pthread_mutex_unlock(&g_net_mod_mgr.net_module_mgr_lock);
            return DSS_MODULE_STATUS_ERROR;
        }

        nm->dev_name = strdup(net_dev_name);
        if(!nm->dev_name) {
            DSS_ERRLOG("Net module init failed\n");
            free(nm);
            pthread_mutex_unlock(&g_net_mod_mgr.net_module_mgr_lock);
            return DSS_MODULE_STATUS_ERROR;
        }

        nm->ip = strdup(ip);
        if(!nm->ip) {
            DSS_ERRLOG("Net module init failed\n");
            free(nm->dev_name);
            free(nm);
            pthread_mutex_unlock(&g_net_mod_mgr.net_module_mgr_lock);
            return DSS_MODULE_STATUS_ERROR;
        }

        rc = pthread_mutex_init(&nm->net_module_lock, NULL);
        if(rc != 0) {
            DSS_ERRLOG("Net module init failed\n");
            free(nm);
            nm = NULL;
            return DSS_MODULE_STATUS_ERROR;
        }

        TAILQ_INSERT_TAIL(&g_net_mod_mgr.net_mod_list, nm, mgr_mlist);

        status = DSS_MODULE_STATUS_MOD_CREATED;
    }

    DSS_ASSERT(nm != NULL);
    _dss_net_module_get(nm);
    *net_module = nm;

    pthread_mutex_unlock(&g_net_mod_mgr.net_module_mgr_lock);

    return status;
}

dss_module_status_t dss_net_module_destroy(dss_net_module_t *net_module)
{
    dss_module_status_t status = DSS_MODULE_STATUS_SUCCESS;

    pthread_mutex_lock(&g_net_mod_mgr.net_module_mgr_lock);

    if(_dss_net_module_put(net_module)) {//Refcount down to 0
        TAILQ_REMOVE(&g_net_mod_mgr.net_mod_list, net_module, mgr_mlist);
        DSS_NOTICELOG("Stopping Network module for IP %s\n", net_module->ip);


        net_module->dev_name = NULL;
        net_module->ip = NULL;
        free(net_module->dev_name);
        free(net_module->ip);
        net_module->module = NULL;//Note: Caller should have reference to stop module

        free(net_module);

        status = DSS_MODULE_STATUS_MOD_DESTROYED;
    }

    pthread_mutex_unlock(&g_net_mod_mgr.net_module_mgr_lock);

    return status;

}

typedef struct dss_net_module_init_event_s {
    dss_subsystem_t *ss;
    dss_net_module_config_t *cfg;
    int current_index;
    dss_net_module_subsys_t *nm_subsys;
    struct df_ss_cb_event_s *cb_event;
} dss_net_module_init_event_t;

#define DSS_DEFAULT_NET_MODULE_ID (1)

void dss_net_module_free_config(dss_net_module_config_t *c)
{
    int i;
    for(i=0; i < c->count; i++) {
        free(c->dev_list[i].dev_name);
        free(c->dev_list[i].ip);
    }
}

dss_module_status_t dss_net_module_init_next(dss_net_module_init_event_t *e)
{
    dss_net_module_t *net_module;
    dss_module_status_t rc = DSS_MODULE_STATUS_SUCCESS;
    struct dss_net_module_ss_node_s *net_ss_node = NULL;
    int current_index = e->current_index;
    int nic_numa_node = -1;
    dss_module_config_t c;

	dss_module_set_default_config(&c);

    //TODO: lock ??
    if(current_index == e->cfg->count) {
        dss_module_set_subsys_ctx(DSS_MODULE_NET, (void*)e->ss ,(void *)e->nm_subsys);
        df_ss_cb_event_complete(e->cb_event);
        dss_net_module_free_config(e->cfg);
        free(e->cb_event);
        free(e->cfg);
        free(e);
        return DSS_MODULE_STATUS_INITIALIZED;
    }

    net_ss_node = calloc(1, sizeof(struct dss_net_module_ss_node_s));
    if(!net_ss_node) {
        //TODO: do callback with error code
        dss_net_module_free_config(e->cfg);
        free(e->cfg);
        free(e);
        return DSS_MODULE_STATUS_ERROR;
    }

    e->current_index++;
    nic_numa_node = dss_get_ifaddr_numa_node(e->cfg->dev_list[current_index].ip);

    rc = dss_net_module_init(e->cfg->dev_list[current_index].dev_name,
                             e->cfg->dev_list[current_index].ip,
                             &net_module);

    if(rc == DSS_MODULE_STATUS_ERROR) {
        //TODO: do callback with error code
        free(net_ss_node);
        dss_net_module_free_config(e->cfg);
        free(e->cfg);
        free(e);
        df_ss_cb_event_complete(e->cb_event);
        return rc;
    }

    if(rc == DSS_MODULE_STATUS_MOD_CREATED) {
        c.id = DSS_DEFAULT_NET_MODULE_ID;
	    c.num_cores = e->cfg->dev_list[current_index].num_nw_threads;
        c.numa_node = nic_numa_node;
        c.async_load_enabled = true;
        /*TODO: Change ip to device name e->cfg->dev_list[current_index].dev_name*/
        net_module->module = dfly_module_start(e->cfg->dev_list[current_index].ip,
                          DSS_MODULE_NET, &c, &g_net_module_ops, net_module,
                          (df_module_event_complete_cb) dss_net_module_init_next, (void *)e);

        DSS_NOTICELOG("Started Network module for IP %s\n", net_module->ip);
    }

    net_ss_node->module = net_module->module;
    TAILQ_INSERT_TAIL(&e->nm_subsys->module_list, net_ss_node, subsys_mlist);

    if(rc == DSS_MODULE_STATUS_SUCCESS) {//Sync init for module not created
        rc = dss_net_module_init_next(e);
    } else if (rc == DSS_MODULE_STATUS_MOD_CREATED) {
        //Handle created as success. Only pass thorough other errors
        rc = DSS_MODULE_STATUS_SUCCESS;
    }

    return rc;
// err_out:

}


dss_module_status_t dss_net_module_subsys_start(dss_subsystem_t *ss, void *arg, df_module_event_complete_cb cb, void *cb_arg)
{
    dss_module_status_t rc = DSS_MODULE_STATUS_SUCCESS;
    dss_net_module_config_t *cfg = (dss_net_module_config_t *)arg;
    struct df_ss_cb_event_s *net_init_event;

    dss_net_module_init_event_t *e = calloc(1, sizeof(dss_net_module_init_event_t));
    dss_net_module_subsys_t *nm_ss = calloc(1, sizeof(dss_net_module_subsys_t));

    DSS_ASSERT(cfg != NULL);

    if(e && nm_ss) {
        net_init_event =  df_ss_cb_event_allocate(ss, cb, cb_arg, NULL);
        e->cb_event = net_init_event;
        if(!net_init_event) {
            DSS_ERRLOG("Failed to allocate net event\v");
            goto err_out;
        }
        TAILQ_INIT(&nm_ss->module_list);
        e->nm_subsys = nm_ss;
        e->ss = ss;
        e->current_index = 0;
        e->cfg = cfg;

        rc = dss_net_module_init_next(e);
        if (rc == DSS_MODULE_STATUS_SUCCESS ||
            rc == DSS_MODULE_STATUS_INITIALIZED) {
            return DSS_MODULE_STATUS_SUCCESS;
        } else {
            DSS_ERRLOG("Failed to initialize net module");
            goto err_out;
        }
    } else {
        DSS_ERRLOG("Failed to allocated memory\n");
        goto err_out;
    }
err_out:
    //TODO: Destroy initialized NICs before destory
    if(e) free(e);
    if(nm_ss) free(nm_ss);

    return DSS_MODULE_STATUS_ERROR;
}

typedef struct dss_net_module_destroy_event_s {
    dss_net_module_subsys_t *nm_subsys;
    struct df_ss_cb_event_s *cb_event;
} dss_net_module_destroy_event_t;

dss_module_status_t dss_net_module_destroy_next(dss_net_module_destroy_event_t *e)
{
    dss_module_status_t rc;
    struct dss_net_module_ss_node_s *net_ss_node;
    dss_net_module_t *nm;
    dss_module_t *m;

    net_ss_node = TAILQ_FIRST(&e->nm_subsys->module_list);

    if(net_ss_node == NULL) {
        df_ss_cb_event_complete(e->cb_event);
        free(e->nm_subsys);
        e->nm_subsys = NULL;
        free(e);
        return DSS_MODULE_STATUS_SUCCESS;
    }

    TAILQ_REMOVE(&e->nm_subsys->module_list, net_ss_node, subsys_mlist);

    m = net_ss_node->module;
    nm = dfly_module_get_ctx(m);
    DSS_RELEASE_ASSERT(nm != NULL);
    rc = dss_net_module_destroy(nm);
    if(rc == DSS_MODULE_STATUS_MOD_DESTROYED) {
        //Stop module
        dfly_module_stop(m, (df_module_event_complete_cb)dss_net_module_destroy_next, e);
        return DSS_MODULE_STATUS_MOD_DESTROY_PENDING;
    }

    if(rc == DSS_MODULE_STATUS_SUCCESS) {
        rc = dss_net_module_destroy_next(e);
    }

    return rc;
}

void dss_net_module_subsys_stop(dss_subsystem_t *ss, void *arg /*Not used*/, df_module_event_complete_cb cb, void *cb_arg)
{
    dss_module_status_t rc;
    struct df_ss_cb_event_s *net_destroy_event;
    dss_net_module_destroy_event_t *e = calloc(1, sizeof(dss_net_module_destroy_event_t));
    if(!e) {
        DSS_ASSERT(0);
        return;
    }
    net_destroy_event =  df_ss_cb_event_allocate(ss, cb, cb_arg, NULL);
    e->cb_event = net_destroy_event;
    if(!net_destroy_event) {
        DSS_ERRLOG("Failed to allocate net event\v");
        DSS_ASSERT(0);
        return;
    }
    e->nm_subsys = (dss_net_module_subsys_t *)dss_module_get_subsys_ctx(DSS_MODULE_NET, (void *)ss);

    rc = dss_net_module_destroy_next(e);
    if(rc != DSS_MODULE_STATUS_SUCCESS && rc != DSS_MODULE_STATUS_MOD_DESTROY_PENDING) {
        DSS_ERRLOG("Subsys %p module stop failed with error code %d", ss, rc);
    }
}

char * dss_net_module_get_name(char *ip)
{
    struct dss_net_module_s *m;

    TAILQ_FOREACH(m, &g_net_mod_mgr.net_mod_list, mgr_mlist) {
		if(!strcmp(m->ip, ip)) {
			return m->module->name;
		}
	}

    return NULL;
}

dss_module_t * dss_net_module_find_module_ctx(char *ip)
{
    struct dss_net_module_s *m;

    TAILQ_FOREACH(m, &g_net_mod_mgr.net_mod_list, mgr_mlist) {
		if(!strcmp(m->ip, ip)) {
			return m->module;
		}
	}

    return NULL;
}
