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
    params->mb_blk_num = DEFAULT_META_NUM;
    params->mb_data_num = BLK_NUM;
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
    iot_rc = dss_io_task_get_new(kvt_ctx->kvt_iotm, &kreq->io_tasks);
    if(iot_rc != DSS_IO_TASK_STATUS_SUCCESS) {
        DSS_RELEASE_ASSERT(0);//Should always succed
    }

    dss_io_task_setup(kreq->io_tasks, req, kvt_mi, req);

    return;
}

void *dss_kvtrans_thread_instance_init(void *mctx, void *inst_ctx, int inst_index)
{
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

        dss_kvtrans_mctx->kvt_ctx_arr[i] = init_kvtrans_ctx(&params);
    }

    //Only one shard per device
    //TODO: Support multiple shards. one shard per thread
    thread_ctx->shard_index = 0;
    
    return thread_ctx;
}

void *dss_kvtrans_thread_instance_destroy(void *mctx, void *inst_ctx) {
    return NULL;
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
	.module_gpoll = NULL,
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

static int dss_kvtrans_request_handler(void *ctx, dss_request_t *req)
{
    dss_kvtrans_thread_ctx_t *kvt_thread_ctx = (dss_kvtrans_thread_ctx_t *)ctx;
    kvtrans_req_t *kreq = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt;

    kvtrans_ctx_t *kvt_ctx = kvt_thread_ctx->mctx->kvt_ctx_arr[req->io_device_index];
    dss_kvtrans_status_t rc;

    DSS_ASSERT(kreq);
    if(kreq->initialized == false) {
        dss_kvtrans_setup_request(req, kvt_ctx);
        init_kvtrans_req(kvt_ctx, &kreq->req, kreq);
    }

    DSS_ASSERT(kreq->initialized == true);
    kvt_ctx->entry_blk->kreq = kreq;

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
        // if (kreq->id%999==0) {
            //printf("[KVTRANS]: meta blks [%zu], collision blks[%zu], meta data collision blks [%zu]\n",
            //       kvt_ctx->stat.meta, kvt_ctx->stat.mc,
            //       kvt_ctx->stat.dc, kvt_ctx->stat.mdc);
        // }
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

    mctx->dss_kvtrans_module = dfly_module_start("DSS_KVTRANS", subsystem->id, DSS_MODULE_KVTRANS, &kvtrans_module_ops, 
                                        mctx, mctx->num_threads, -1, cb, cb_arg);
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
