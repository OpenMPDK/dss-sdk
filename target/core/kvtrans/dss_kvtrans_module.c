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
#include "dss_kvtrans_module.h"

void set_default_kvtrans_params(kvtrans_params_t *params) {
    // C(params);
    params->thread_num = 1;
    params->name = "DSS_KVTRANS";
    params->hash_type = spooky;
    params->blk_alloc_name = DEFAULT_BLK_ALLOC_NAME;
    params->meta_blk_num = DEFAULT_META_NUM;
    params->total_blk_num = BLK_NUM;
    params->hash_size = 32;
    params->dev = NULL;

    return;
}

void dss_kvtrans_setup_request(dss_request_t *req, kvtrans_ctx_t *kvt_ctx)
{
    dss_io_task_status_t iot_rc;
    kvtrans_req_t *kreq = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt;
    dss_module_instance_t *kvt_mi = req->module_ctx[DSS_MODULE_KVTRANS].module_instance;

    //req->common_req.module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt.
    //req->common_req.module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt.
    TAILQ_INIT(&kreq->meta_chain);
    iot_rc = dss_io_task_get_new(kvt_ctx->kvt_iotm, &kreq->io_tasks);
    if(iot_rc != DSS_IO_TASK_STATUS_SUCCESS) {
        DSS_RELEASE_ASSERT(0);//Should always succed
    }

    dss_io_task_setup(kreq->io_tasks, req, kvt_mi, req);

    return;
}

#define DSS_KVT_SUPERBLOCK_LBA (0)
#define DSS_KVT_SUPERBLOCK_NUM_BLOCKS (1)

dss_module_status_t dss_kvtrans_initiate_loading(kvtrans_ctx_t **new_kvt_ctx, dss_module_t *m, dss_module_instance_t *kvt_m_thrd_inst, kvtrans_params_t *p)
{
    dss_request_t *req = (dss_request_t *)calloc(1, sizeof(dss_request_t));
    dss_kvt_init_ctx_t *init_req_ctx;
    dss_io_task_t *iot = NULL;
    dss_io_task_status_t iot_rc;

    DSS_ASSERT(req);
    DSS_ASSERT(*new_kvt_ctx == NULL);

    req->opc = DSS_INTERNAL_IO;

    req->module_ctx[DSS_MODULE_KVTRANS].module = m;
    req->module_ctx[DSS_MODULE_KVTRANS].module_instance = kvt_m_thrd_inst;

    init_req_ctx = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt_init;

    init_req_ctx->dev = p->dev;
    init_req_ctx->iotm = p->iotm;
    init_req_ctx->kvt_ctx = new_kvt_ctx;

    init_req_ctx->state = DSS_KVT_LOADING_SUPERBLOCK;

    //TODO: Block allocator meta init might work better with bigger buffer
    init_req_ctx->data = spdk_dma_zmalloc(BLOCK_SIZE, 4096, NULL);
    DSS_ASSERT(init_req_ctx->data != NULL);
    init_req_ctx->data_len = BLOCK_SIZE;

    dss_io_dev_set_user_blk_sz(p->dev, BLOCK_SIZE);

    iot_rc = dss_io_task_get_new(init_req_ctx->iotm, &iot);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
    DSS_ASSERT(iot != NULL);

    iot_rc = dss_io_task_setup(iot, req, kvt_m_thrd_inst, req);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);

    DSS_ASSERT(init_req_ctx->data_len >= dss_io_dev_get_user_blk_sz(init_req_ctx->dev) * DSS_KVT_SUPERBLOCK_NUM_BLOCKS);

    iot_rc = dss_io_task_add_blk_read(iot, init_req_ctx->dev, DSS_KVT_SUPERBLOCK_LBA, DSS_KVT_SUPERBLOCK_NUM_BLOCKS, \
                                    init_req_ctx->data, NULL);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);

    dss_module_inc_async_pending_task(m);

    iot_rc = dss_io_task_submit(iot);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);

    return DSS_MODULE_STATUS_INIT_PENDING;
}

void dss_kvtrans_free_init_request(dss_request_t *req)
{
    dss_kvt_init_ctx_t *kv_init_ctx;

    kv_init_ctx = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt_init;

    spdk_free(kv_init_ctx->data);
    memset(req, 0, sizeof(dss_request_t));
    free(req);

    return;
}

void *dss_kvtrans_thread_instance_init(void *mctx, void *inst_ctx, int inst_index)
{
    dss_module_status_t m_rc;
    dss_kvtrans_module_ctx_t *dss_kvtrans_mctx = (dss_kvtrans_module_ctx_t *)mctx;
    int num_devices = dss_kvtrans_mctx->dfly_subsys->num_io_devices;
    int num_threads = dss_kvtrans_mctx->num_threads;
    int i;

    kvtrans_params_t params;
    dss_kvtrans_thread_ctx_t *thread_ctx = (dss_kvtrans_thread_ctx_t *) calloc (1, sizeof(dss_kvtrans_thread_ctx_t));
    DSS_RELEASE_ASSERT(thread_ctx);

    thread_ctx->mctx = mctx;
    thread_ctx->inst_index = inst_index;
    thread_ctx->module_inst_ctx = inst_ctx;

    dss_kvtrans_mctx->kvt_thrd_ctx_arr[inst_index] = thread_ctx;
    //Initialize devices corresponding to thread
    for(i=inst_index; i < num_devices; i= i+num_threads) {

        set_default_kvtrans_params(&params);
        params.dev =  dss_kvtrans_mctx->dfly_subsys->dev_arr[i];
        DSS_ASSERT(params.dev != NULL);
        //TODO: Setup device specific kvtrans params
        params.iotm = dss_subsytem_get_iotm_ctx(dss_kvtrans_mctx->dfly_subsys);
        DSS_ASSERT(params.iotm);
        m_rc = dss_kvtrans_initiate_loading(&dss_kvtrans_mctx->kvt_ctx_arr[i], dss_kvtrans_mctx->dss_kvtrans_module, thread_ctx->module_inst_ctx, &params);
        DSS_ASSERT(m_rc == DSS_MODULE_STATUS_INIT_PENDING);

        // dss_kvtrans_mctx->kvt_ctx_arr[i] = init_kvtrans_ctx(&params);
    }

    //Only one shard per device
    //TODO: Support multiple shards. one shard per thread
    thread_ctx->shard_index = 0;
    
    return thread_ctx;
}

void *dss_kvtrans_thread_instance_destroy(void *mctx, void *inst_ctx) {
    //TODO: Handle syncing dirty data and shutdown gracefully
    return NULL;
}

void dss_kvtrans_submit_runnable_tasks(kvtrans_ctx_t *kv_ctx)
{
    dss_io_task_t *io_task = NULL;
    dss_blk_allocator_status_t rc;

    //TODO: Can we try to sudmit multiple availabe tasks upto a defined threshold
    rc = dss_blk_allocator_get_next_submit_meta_io_tasks(kv_ctx->blk_alloc_ctx, &io_task);
    if(rc != BLK_ALLOCATOR_STATUS_SUCCESS) {
        DSS_ASSERT(rc == BLK_ALLOCATOR_STATUS_ITERATION_END);
        return;//No IO task available to submit
    }

    DSS_ASSERT(io_task != NULL);
    DSS_DEBUGLOG(DSS_KVTRANS, "submit task [%p]\n", io_task);
    dss_io_task_submit(io_task);

    return;
}

int dss_kvtrans_process_generic(void *ctx)
{
    dss_kvtrans_thread_ctx_t *thread_ctx = (dss_kvtrans_thread_ctx_t *) ctx;
    dss_kvtrans_module_ctx_t *dss_kvtrans_mctx = thread_ctx->mctx;
    int num_devices = dss_kvtrans_mctx->dfly_subsys->num_io_devices;
    int num_threads = dss_kvtrans_mctx->num_threads;
    int i, inst_index;

    inst_index = thread_ctx->inst_index;
    for(i=inst_index; i < num_devices; i= i+num_threads) {
        if(dss_kvtrans_mctx->kvt_ctx_arr[i]->is_ba_meta_sync_enabled) {
            //TODO: Optimize to disable genric poller if no kv trans instance has ba meta sync enabled
            dss_kvtrans_submit_runnable_tasks(dss_kvtrans_mctx->kvt_ctx_arr[i]);
        }
    }
    return 0;
}

int dss_kvtrans_process(void *ctx, dss_request_t *req) {
    int rc;

    dss_kvtrans_thread_ctx_t *thread_ctx = (dss_kvtrans_thread_ctx_t *) ctx;
    
    return thread_ctx->mctx->request_handler(ctx, req);
}

int dss_kvtrans_complete(void *ctx, struct dfly_request *req) {
    dss_kvtrans_thread_ctx_t *thread_ctx = (dss_kvtrans_thread_ctx_t *) ctx;
    // TODO: handle completed req
    return 1;
}

void *dss_kvtrans_find_instance_context(struct dfly_request *req) {
    uint32_t thread_index;
    uint32_t num_threads = req->req_dfly_ss->num_kvt_threads;
    uint32_t num_devices = req->req_dfly_ss->num_io_devices;

    dfly_module_t *mctx = dss_module_get_subsys_ctx(DSS_MODULE_KVTRANS, req->req_dfly_ss);
    dss_kvtrans_module_ctx_t *dss_kvtrans_mctx = dfly_module_get_ctx(mctx);
    dss_module_instance_t *kvt_mi;

    thread_index = req->common_req.io_device_index % num_threads;

    DSS_ASSERT(thread_index < dss_kvtrans_mctx->num_threads);

    kvt_mi = dss_kvtrans_mctx->kvt_thrd_ctx_arr[thread_index]->module_inst_ctx;

    DSS_ASSERT(req->common_req.module_ctx[DSS_MODULE_KVTRANS].module_instance == NULL);
    DSS_ASSERT(req->common_req.module_ctx[DSS_MODULE_KVTRANS].module == NULL);

    req->common_req.module_ctx[DSS_MODULE_KVTRANS].module_instance = kvt_mi;
    req->common_req.module_ctx[DSS_MODULE_KVTRANS].module = kvt_mi->module;

    DSS_ASSERT(kvt_mi != NULL);
    return kvt_mi;
}

struct dfly_module_ops kvtrans_module_ops = {
	.module_init_instance_context = dss_kvtrans_thread_instance_init,
	.module_rpoll = dss_kvtrans_process,
	.module_cpoll = NULL,
	.module_gpoll = dss_kvtrans_process_generic,
	.find_instance_context = dss_kvtrans_find_instance_context,
	.module_instance_destroy = dss_kvtrans_thread_instance_destroy,
};

void dss_kvtrans_net_request_complete(dss_request_t *req, dss_kvtrans_status_t rc)
{
    kvtrans_req_t *kreq = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt;
    DSS_ASSERT(kreq);

    switch(rc) {
        case KVTRANS_STATUS_SUCCESS:
            req->status = DSS_REQ_STATUS_SUCCESS;
            break;
        case KVTRANS_STATUS_NOT_FOUND:
            req->status = DSS_REQ_STATUS_KEY_NOT_FOUND;
            break;
        default:
            DSS_ASSERT(0);
        }

        dss_net_setup_nvmf_resp(req, req->status, kreq->req.req_value.length);
        //Send back to net module
        dss_module_post_to_instance(DSS_MODULE_NET, req->module_ctx[DSS_MODULE_NET].module_instance, req);
}

void dss_kvtrans_process_internal_io(dss_request_t *req)
{
    kvtrans_params_t params;
    dss_kvt_init_ctx_t *kv_init_ctx = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt_init;
    dss_kvt_state_t prev_state;


    DSS_ASSERT(req->opc == DSS_INTERNAL_IO);
    do {
        prev_state = kv_init_ctx->state;
        switch(kv_init_ctx->state) {
            case DSS_KVT_LOADING_SUPERBLOCK:
                set_default_kvtrans_params(&params);
                params.dev = kv_init_ctx->dev;
                DSS_ASSERT(params.dev != NULL);
                params.iotm = kv_init_ctx->iotm;
                DSS_ASSERT(params.iotm != NULL);
                //TODO: Process super block
                //TODO: init path if not loading from superblock
                //TODO: Setup device specific kvtrans params
                DSS_ASSERT(*kv_init_ctx->kvt_ctx == NULL);
                DSS_NOTICELOG("Read kv trans superblock from device %p\n", params.dev);
                *kv_init_ctx->kvt_ctx = init_kvtrans_ctx(&params);
                dss_module_dec_async_pending_task(req->module_ctx[DSS_MODULE_KVTRANS].module);//TODO: This need to be moved to after all loading is completed
                //TODO: Continue to load block allocator meta
                kv_init_ctx->state = DSS_KVT_INITIALIZED;
                break;
            case DSS_KVT_LOADING_BA_META:
                DSS_ASSERT(0);
                //TODO: Add state and continue to load dc table
                break;
            case DSS_KVT_INITIALIZED:
                dss_kvtrans_free_init_request(req);
                return;
            default:
                DSS_ASSERT(0);
        }
    } while (kv_init_ctx->state != prev_state);
    return;
}

static int dss_kvtrans_request_handler(void *ctx, dss_request_t *req)
{
    dss_kvtrans_thread_ctx_t *kvt_thread_ctx = (dss_kvtrans_thread_ctx_t *)ctx;
    kvtrans_req_t *kreq = NULL;

    kvtrans_ctx_t *kvt_ctx = NULL;
    dss_kvtrans_status_t rc;

    if(dss_unlikely(req->opc == DSS_INTERNAL_IO)) {
        dss_kvtrans_process_internal_io(req);
        return DFLY_MODULE_REQUEST_QUEUED;
    }

    kreq = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt;
    kvt_ctx = kvt_thread_ctx->mctx->kvt_ctx_arr[req->io_device_index];
    kreq->dreq = req;

    DSS_ASSERT(kreq);
    if(kreq->initialized == false) {
        dss_kvtrans_setup_request(req, kvt_ctx);
        init_kvtrans_req(kvt_ctx, &kreq->req, kreq);
    }

    DSS_ASSERT(kreq->initialized == true);

    switch(kreq->req.opc) {
    case KVTRANS_OPC_STORE:
        rc = kvtrans_store(kvt_ctx, kreq);
        break;
    case KVTRANS_OPC_RETRIEVE:
        rc = kvtrans_retrieve(kvt_ctx, kreq);
        break;
    case KVTRANS_OPC_DELETE:
        rc = kvtrans_delete(kvt_ctx, kreq);
        break;
    case KVTRANS_OPC_EXIST:
        rc = kvtrans_exist(kvt_ctx, kreq);
        break;
    default:
        break;
    }
    if (rc) {
        // TODO: error log
    }

    if(kreq->state == REQ_CMPL) {
        DSS_DEBUGLOG(DSS_KVTRANS, "[KVTRANS]: meta blks [%zu], collision blks[%zu], meta data collision blks [%zu]\n",
            kvt_ctx->stat.meta, kvt_ctx->stat.mc,
            kvt_ctx->stat.dc, kvt_ctx->stat.mdc);
        dss_kvtrans_net_request_complete(req, rc);
    }

    return DFLY_MODULE_REQUEST_QUEUED;
}


void dss_setup_kvtrans_req(dss_request_t *req, dss_key_t *k, dss_value_t *v)
{
    DSS_ASSERT(k!=NULL && k->length<=KEY_LEN);
    kvtrans_req_t *kreq = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt;

    switch(req->opc) {
        case DSS_NVMF_KV_IO_OPC_STORE:
            kreq->req.opc = KVTRANS_OPC_STORE;
            DSS_ASSERT(v->value);
            break;
        case DSS_NVMF_KV_IO_OPC_RETRIEVE:
            kreq->req.opc = KVTRANS_OPC_RETRIEVE;
            DSS_ASSERT(v->value);
            break;
        case DSS_NVMF_KV_IO_OPC_DELETE:
            kreq->req.opc = KVTRANS_OPC_DELETE;
            break;
        default:
            DSS_ASSERT(0);
    }

    strncpy(kreq->req.req_key.key, k->key, k->length);//TODO: Copy pointer
    if (k->length<KEY_LEN) {
        kreq->req.req_key.key[k->length] = '\0';
    }
    kreq->req.req_key.length = k->length;

    kreq->req.req_value.value = v->value;
    kreq->req.req_value.length = v->length;
    kreq->req.req_value.offset = v->offset;

    return;
}

int dss_kvtrans_module_subsystem_start(struct dfly_subsystem *subsystem, 
                                        void *arg /*Not used */, df_module_event_complete_cb cb, void *cb_arg)
{
    int rc = 0;

    dss_kvtrans_module_ctx_t *mctx = (dss_kvtrans_module_ctx_t *) calloc (1, sizeof(dss_kvtrans_module_ctx_t));
	dss_module_config_t c;

	dss_module_set_default_config(&c);
	c.id = subsystem->id;
    c.async_load_enabled = true;

    DSS_ASSERT(mctx);
    DSS_ASSERT(subsystem->num_kvt_threads > 0);

    mctx->dfly_subsys = subsystem;
    mctx->num_threads = subsystem->num_kvt_threads;
    mctx->request_handler = dss_kvtrans_request_handler;

    mctx->num_kvts = subsystem->num_io_devices;
    mctx->kvt_ctx_arr = (kvtrans_ctx_t **)calloc(mctx->num_kvts, sizeof(kvtrans_ctx_t *));
    DSS_RELEASE_ASSERT(mctx->kvt_ctx_arr);

    if (mctx->num_kvts < mctx->num_threads) {
        DSS_NOTICELOG("Reduced kv trans num threads to %d from %d\n", mctx->num_kvts, mctx->num_threads);
        mctx->num_threads = mctx->num_kvts;
        subsystem->num_kvt_threads = mctx->num_kvts;
    }

    mctx->kvt_thrd_ctx_arr = (dss_kvtrans_thread_ctx_t **)calloc(mctx->num_threads, sizeof(dss_kvtrans_thread_ctx_t *));
    DSS_RELEASE_ASSERT(mctx->kvt_thrd_ctx_arr != NULL);

    c.num_cores = mctx->num_threads;
    mctx->dss_kvtrans_module = dfly_module_start("DSS_KVTRANS", DSS_MODULE_KVTRANS, &c, &kvtrans_module_ops,
                                        mctx, cb, cb_arg);
    DSS_ASSERT(mctx->dss_kvtrans_module);
    dss_module_set_subsys_ctx(DSS_MODULE_KVTRANS, (void *)subsystem, (void *)mctx->dss_kvtrans_module);
    DSS_NOTICELOG("Kvtrans module %p started\n", mctx->dss_kvtrans_module);
    
    return 0;
}

void dss_kvtrans_module_subsystem_stop(struct dfly_subsystem *subsystem, void *arg /*Not used*/, df_module_event_complete_cb cb, void *cb_arg)
{
    dfly_module_stop(subsystem->mlist.dss_kv_trans_module, cb, cb_arg, NULL);
    return;
}
