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

kvtrans_params_t * set_default_kvtrans_params() {
    kvtrans_params_t *params = (kvtrans_params_t *) malloc (sizeof(kvtrans_params_t));
    // C(params);
    params->thread_num = 1;
    params->name = "DSS_KVTRANS";
    params->hash_type = spooky;
    params->blk_alloc_name = DEFAULT_BLK_ALLOC_NAME;
    params->mb_blk_num = DEFAULT_META_NUM;
    params->mb_data_num = BLK_NUM;
    params->hash_size = 32;
    return params;
}

dss_kvtrans_thread_ctx_t *init_kvtrans_thread_ctx(dss_kvtrans_module_ctx_t *mctx, int inst_index, kvtrans_params_t *params) {
    dss_kvtrans_thread_ctx_t *inst_ctx = (dss_kvtrans_thread_ctx_t *) malloc (sizeof(dss_kvtrans_thread_ctx_t));
    DSS_ASSERT(inst_ctx);
    if (params) {
        inst_ctx->params = params;
    } else {
        inst_ctx->params = set_default_kvtrans_params();
    }
    inst_ctx->ctx = init_kvtrans_ctx(inst_ctx->params);
    DSS_ASSERT(inst_ctx->ctx);
    DSS_ASSERT(mctx);
    inst_ctx->mctx = mctx;
    inst_ctx->inst_index = inst_index;
    return inst_ctx;
}

void free_kvtrans_thread_ctx(dss_kvtrans_thread_ctx_t *inst_ctx) {
    if (!inst_ctx) return;
    if (inst_ctx->params) free(inst_ctx->params);
    if (inst_ctx->ctx) free_kvtrans_ctx(inst_ctx->ctx);
}

void *dss_kvtrans_thread_instance_init(void *mctx, void *inst_ctx, int inst_index) {
    struct dfly_subsystem *dfly_subsystem = NULL;
    dss_kvtrans_thread_ctx_t *thread_ctx = NULL;
    dss_kvtrans_module_ctx_t *dss_kvtrans_mctx = (dss_kvtrans_module_ctx_t *)mctx;
    int num_devices = dss_kvtrans_mctx->dfly_subsys->num_io_devices;
    int num_threads = dss_kvtrans_mctx->num_threads;

    thread_ctx = init_kvtrans_thread_ctx(dss_kvtrans_mctx, inst_index, NULL);
    DSS_ASSERT(thread_ctx);

    thread_ctx->inst_ctx = inst_ctx;
    
    if (num_devices < num_threads) {
        dss_kvtrans_mctx->num_threads = num_devices;
    }
    thread_ctx->shard_index = num_devices % num_threads;
    
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
    return NULL;
}

struct dfly_module_ops kvtrans_module_ops = {
	.module_init_instance_context = dss_kvtrans_thread_instance_init,
	.module_rpoll = dss_kvtrans_process,
	.module_cpoll = NULL,
	.module_gpoll = NULL,
	.find_instance_context = NULL,
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

static int dss_kvtrans_request_handler(void *ctx, dss_request_t *req) {
    dss_kvtrans_status_t rc;
    dss_kvtrans_thread_ctx_t *thread_ctx = (dss_kvtrans_thread_ctx_t *) ctx;
    // kvtrans_req_t *kreq = init_kvtrans_req(thread_ctx->ctx, dss_request, NULL);
    kvtrans_req_t *kreq = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt;
    DSS_ASSERT(kreq);
    init_kvtrans_req(thread_ctx->ctx, &kreq->req, kreq);
    thread_ctx->ctx->entry_blk->kreq = kreq;

    switch(kreq->req.opc) {
    case KVTRANS_OPC_STORE:
        rc = kvtrans_store(thread_ctx->ctx, kreq);
        break;
    case KVTRANS_OPC_RETRIEVE:
        rc = kvtrans_retrieve(thread_ctx->ctx, kreq);
        break;
    case KVTRANS_OPC_DELETE:
        rc = kvtrans_delete(thread_ctx->ctx, kreq);
        break;
    case KVTRANS_OPC_EXIST:
        rc = kvtrans_exist(thread_ctx->ctx, kreq);
        break;
    default:
        break;
    }
    if (rc) {
        // TODO: error log
    }

    if(kreq->state == REQ_CMPL) {
        // if (kreq->id%999==0) {
            printf("[KVTRANS]: meta blks [%zu], collision blks[%zu], meta data collision blks [%zu]\n", 
                     thread_ctx->ctx->stat.meta, thread_ctx->ctx->stat.mc,
                     thread_ctx->ctx->stat.dc, thread_ctx->ctx->stat.mdc);
        // }
        dss_kvtrans_net_request_complete(req, rc);

    } else {
        //TODO: Handling for async completion
        DSS_ASSERT(0);
    }

    return DFLY_MODULE_REQUEST_QUEUED;
}


void dss_setup_kvtrans_req(dss_request_t *req, dss_key_t *k, dss_value_t *v)
{
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
    kreq->req.req_key.key[k->length] = '\0';
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

    mctx->dfly_subsys = subsystem;
    mctx->num_threads = subsystem->num_kvt_threads;
    mctx->request_handler = dss_kvtrans_request_handler;

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
