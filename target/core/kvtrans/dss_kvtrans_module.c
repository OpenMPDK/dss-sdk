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
#include "dss_spdk_wrapper.h"
#include "apis/dss_superblock_apis.h"
#include "dss_block_allocator.h"

#define TRACE_KVS_GET_NEW            SPDK_TPOINT_ID(TRACE_GROUP_DSS_KVTRANS, 0x1)
#define TRACE_KVS_PUSH_CPL           SPDK_TPOINT_ID(TRACE_GROUP_DSS_KVTRANS, 0x2)
SPDK_TRACE_REGISTER_FN(kvm_trace, "kvtrans_module", TRACE_GROUP_DSS_KVTRANS)
{
    spdk_trace_register_description("KVT_GET_NEW", TRACE_KVS_GET_NEW,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:    ");
    spdk_trace_register_description("KVT_PUSH_CPL", TRACE_KVS_PUSH_CPL,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:    ");
}

void set_default_kvtrans_params(kvtrans_params_t *params) {
    // C(params);
    params->thread_num = 1;
    params->name = "DSS_KVTRANS";
    params->hash_type = spooky;
    params->blk_alloc_name = DEFAULT_BLK_ALLOC_NAME;
    params->meta_blk_num = DEFAULT_META_NUM;
    params->logi_blk_num = BLK_NUM;
    params->hash_size = 32;
    params->dev = NULL;
    params->blk_offset = 1;
    params->state_num = DEFAULT_BLOCK_STATE_NUM;
    params->logi_blk_size = BLOCK_SIZE;

    return;
}

void dss_kvtrans_init_req_on_alloc(dss_request_t *req)
{
    kvtrans_req_t *kreq;

    kreq = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt;
	TAILQ_INIT(&kreq->meta_chain);

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

    dss_io_task_setup(kreq->io_tasks, req, kvt_mi, req, true);

    return;
}

#define DSS_KVT_SUPERBLOCK_LBA (0)
#define DSS_KVT_SUPERBLOCK_NUM_BLOCKS (1)
#define DEFAULT_MDC_IO_DEPTH (128)

dss_module_status_t dss_kvtrans_initiate_loading(kvtrans_ctx_t **new_kvt_ctx, dss_module_t *m, dss_module_instance_t *kvt_m_thrd_inst, kvtrans_params_t *p)
{
    dss_kvtrans_module_ctx_t *dss_kvtrans_mctx = (dss_kvtrans_module_ctx_t *)m->ctx;
    dss_request_t *req = (dss_request_t *)calloc(1, sizeof(dss_request_t));
    dss_kvt_init_ctx_t *init_req_ctx;
    dss_io_task_t *iot = NULL;
    dss_io_task_status_t iot_rc;

    DSS_ASSERT(req);
    DSS_ASSERT(*new_kvt_ctx == NULL);

    req->opc = DSS_INTERNAL_IO;

    req->module_ctx[DSS_MODULE_KVTRANS].module = m;
    req->module_ctx[DSS_MODULE_KVTRANS].module_instance = kvt_m_thrd_inst;
    req->ss = dss_kvtrans_mctx->dfly_subsys;

    init_req_ctx = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt_init;

    init_req_ctx->dev = p->dev;
    init_req_ctx->iotm = p->iotm;
    init_req_ctx->kvt_ctx = new_kvt_ctx;

    init_req_ctx->state = DSS_KVT_LOADING_SUPERBLOCK;

    //TODO: Block allocator meta init might work better with bigger buffer
    init_req_ctx->data = spdk_dma_zmalloc(BLOCK_SIZE, 4096, NULL);
    DSS_ASSERT(init_req_ctx->data != NULL);
    init_req_ctx->data_len = BLOCK_SIZE;

    //TODO: This cannot be hard coded.
    // But for this to work the logical block size
    // should be read from the first drive LBA and reload super block
    dss_io_dev_set_user_blk_sz(p->dev, BLOCK_SIZE);

    iot_rc = dss_io_task_get_new(init_req_ctx->iotm, &iot);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
    DSS_ASSERT(iot != NULL);

    iot_rc = dss_io_task_setup(iot, req, kvt_m_thrd_inst, req, false);
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
    if (kv_init_ctx->dc_entries) free(kv_init_ctx->dc_entries);
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

void *dss_kvtrans_thread_instance_destroy(void *mctx, void *inst_ctx)
{
    dss_module_status_t m_rc;
    dss_kvtrans_module_ctx_t *dss_kvtrans_mctx = (dss_kvtrans_module_ctx_t *)mctx;
    dss_kvtrans_thread_ctx_t *thread_ctx = (dss_kvtrans_thread_ctx_t *) inst_ctx;
    int num_devices = dss_kvtrans_mctx->dfly_subsys->num_io_devices;
    int num_threads = dss_kvtrans_mctx->num_threads;
    int i, inst_index;
    kvtrans_ctx_t *kvt_ctx;

    inst_index = thread_ctx->inst_index;
    for(i=inst_index; i < num_devices; i= i+num_threads) {
        kvt_ctx = dss_kvtrans_mctx->kvt_ctx_arr[i];
        DSS_ASSERT(kvt_ctx->blk_alloc_ctx);
        if (kvt_ctx->dump_mem_meta) {
            dss_kvtrans_dump_in_memory_meta(kvt_ctx);
        }
        DSS_NOTICELOG("Destructor bitmap dumping\n");
        free_kvtrans_ctx(kvt_ctx);
    }

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
    dss_trace_record(TRACE_KVS_GET_NEW, 0, 0, 0, (uintptr_t)req);
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
	.module_cpoll = dss_kvtrans_process,//Completion still process request but it is from a different queue
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
        dss_trace_record(TRACE_KVS_PUSH_CPL, 0, 0, 0, (uintptr_t)req);


        if(g_list_conf.list_enabled == true) {
            ((struct dfly_request *)req)->state = DFLY_REQ_IO_LIST_FORWARD;
            dfly_module_post_request(dss_module_get_subsys_ctx(DSS_MODULE_LIST, req->ss), req);
        } else {
            dss_module_post_to_instance(DSS_MODULE_NET, req->module_ctx[DSS_MODULE_NET].module_instance, req);
        }
}

int boot_dc_get_next_dmc_blk(dss_kvt_init_ctx_t *kv_init_ctx) {

    dss_blk_allocator_status_t rc;
    kvtrans_ctx_t *kvt_ctx = *kv_init_ctx->kvt_ctx;
    blk_state_t state;
    
    while (kv_init_ctx->dss_mdc_lba < kvt_ctx->blk_offset + kvt_ctx->blk_num - 1)
    {
        // starts from kvt_ctx->blk_offset - 1;
        kv_init_ctx->dss_mdc_lba ++;
        rc = dss_blk_allocator_get_block_state(kvt_ctx->blk_alloc_ctx, kv_init_ctx->dss_mdc_lba, &state);
        if (rc) {
            // TODO: error handling
            DSS_ERRLOG("Fail to get blk [%d] state\n", kv_init_ctx->dss_mdc_lba);
            return 1;
        }

        switch (state)
        {
        case EMPTY:
            break;
        case META:
            kvt_ctx->stat.meta ++;
        case COLLISION:
            kvt_ctx->stat.mc ++;
        case META_DATA_COLLISION_ENTRY:
        case META_DATA_COLLISION:
            kvt_ctx->stat.mdc ++;
            break; 
        default:
            break;
        }

        if (state==META_DATA_COLLISION_ENTRY) {
            break;
        }
    }

    return 0;
}

int boot_dc_insert_elm(kvtrans_ctx_t *kvt_ctx, dc_item_t *it, void *data) {

    dss_blk_allocator_status_t rc;
    uint64_t dc_idx;
    blk_state_t dc_state;

    ondisk_meta_t *blk = (ondisk_meta_t *)data;

    // DSS_ASSERT(blk->magic == META_MAGIC);

    dc_idx = blk->data_collision_index;

    if (find_elm(kvt_ctx->dc_cache_tbl, dc_idx)) {
        // handle duplicated elm
    }

    rc = dss_blk_allocator_get_block_state(kvt_ctx->blk_alloc_ctx, dc_idx, &dc_state);
    if (rc) {
        // TODO: error handling
        DSS_ERRLOG("Fail to get blk [%d] state\n", it->mdc_index);
        return 1;
    }

    switch(dc_state) {
        case DATA_COLLISION:
            it->ori_state = DATA;
            break;
        case DATA_COLLISION_EMPTY:
            it->ori_state = EMPTY;
            break;
        case DATA_COLLISION_CE:
            it->ori_state = COLLISION_EXTENSION;
            break;
        default:
            assert(0);
    }

    if(store_elm(kvt_ctx->dc_cache_tbl, dc_idx, (void *)it)) {
        DSS_ERRLOG("Insert element failed in dc boot\n");
        return 1;
    }
    return 0;
}

uint64_t _dss_calculate_default_ba_meta_sz_blocks(uint64_t total_blocks)
{
    dss_blk_allocator_opts_t ba_config;
    uint64_t ba_meta_sz_blocks;

    // Create Dummy ba instance
    dss_blk_allocator_set_default_config(NULL, &ba_config);
    ba_config.num_total_blocks = total_blocks;
    ba_config.blk_allocator_type = DEFAULT_BLK_ALLOC_NAME;
    ba_config.block_alloc_meta_start_offset = 0;
    ba_config.logical_start_block_offset = 0;
    ba_config.allocator_block_size = BLOCK_SIZE;
    // exclude empty state
    ba_config.num_block_states = DEFAULT_BLOCK_STATE_NUM - 1;
    ba_config.enable_ba_meta_sync = false;

    ba_meta_sz_blocks = dss_blk_allocator_get_physical_size(&ba_config);
    //Convert from bytes to blocks
    ba_meta_sz_blocks = (ba_meta_sz_blocks / BLOCK_SIZE) + ((ba_meta_sz_blocks%BLOCK_SIZE)?1:0);

    return ba_meta_sz_blocks;
}

#define DSS_DEFAULT_KV_META_START_OFFSET (2)

void dss_kvtrans_process_internal_io(dss_request_t *req)
{
    kvtrans_params_t params;
    dss_kvt_init_ctx_t *kv_init_ctx = &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt_init;
    kvtrans_ctx_t *kvt_ctx = *kv_init_ctx->kvt_ctx;
    dss_kvt_state_t prev_state;
    dss_super_block_t *super_block = NULL;
    dss_io_task_status_t iot_rc = DSS_IO_TASK_STATUS_ERROR;
    dss_blk_allocator_context_t *blk_alloc_ctx = NULL;
    uint64_t usable_start_block = 0;
    uint64_t usable_end_block = 0;
    int mdc_io_idx = 0;
    void *mdc_buffer;
    dc_item_t *dc_entry;

    DSS_ASSERT(req->opc == DSS_INTERNAL_IO);
    do {
        prev_state = kv_init_ctx->state;
        switch(kv_init_ctx->state) {
            case DSS_KVT_LOADING_SUPERBLOCK:
                // Initialize BA-Meta specific variables
                kv_init_ctx->ba_meta_start_block = 0;
                kv_init_ctx->ba_meta_end_block = 0;
                kv_init_ctx->ba_meta_size = 0;
                kv_init_ctx->ba_meta_num_blks = 0;
                kv_init_ctx->ba_disk_read_it = 0;
                kv_init_ctx->ba_disk_read_total_it = 0;
                kv_init_ctx->ba_meta_num_blks_per_iter = 0;
                kv_init_ctx->logical_block_size = 0;
                kv_init_ctx->ba_meta_start_block_per_iter = 0;

                set_default_kvtrans_params(&params);
                params.dev = kv_init_ctx->dev;
                DSS_ASSERT(params.dev != NULL);
                params.iotm = kv_init_ctx->iotm;
                DSS_ASSERT(params.iotm != NULL);
                if(!dss_subsystem_kv_persistence_disabled(req->ss)) {//Load from super block
                    super_block = (dss_super_block_t *)kv_init_ctx->data;
                    DSS_NOTICELOG("phy_sz : %d\n",super_block->phy_blk_size_in_bytes);
                    kv_init_ctx->logical_block_size =
                        super_block->logi_blk_size_in_bytes;
                    /* TODO!: Super block provides physical addresses and we
                              need logical block address.
                       This translation is done temporarily here and needs to be
                       refactored
                    */
                    params.logi_blk_size =
                        super_block->logi_blk_size_in_bytes;
                    // Procure total number of usable blocks
                    usable_start_block =
                        super_block->logi_usable_blk_start_addr;
                    usable_end_block = super_block->logi_usable_blk_end_addr;
                    params.logi_blk_num =
                        usable_end_block - usable_start_block + 1;
                    // Procure block allocator meta start block offset
                    params.blk_alloc_meta_start_offset =
                        super_block->logi_blk_alloc_meta_start_blk;
                    // Procure logical start addr or the offset
                    params.blk_offset = usable_start_block;

                    //TODO: init path if not loading from superblock
                    //TODO: Setup device specific kvtrans params
                    DSS_ASSERT(*kv_init_ctx->kvt_ctx == NULL);
                    DSS_NOTICELOG("Read kv trans superblock from device %p\n", params.dev);
                    *kv_init_ctx->kvt_ctx = init_kvtrans_ctx(&params);
                    if ((*kv_init_ctx->kvt_ctx)->is_ba_meta_sync_enabled == false) {
                        //Skip loading bitmap when ba_meta_sync is disabled
                        kv_init_ctx->state = DSS_KVT_INITIALIZED;
                        break;
                    }
                    DSS_NOTICELOG("BA loading bitmap starts\n");
                    // Figure out the first and last logical block to read from
                    kv_init_ctx->ba_meta_start_block =
                        super_block->logi_blk_alloc_meta_start_blk;
                    kv_init_ctx->ba_meta_end_block =
                        super_block->logi_blk_alloc_meta_end_blk;
                    kv_init_ctx->ba_meta_num_blks =
                        kv_init_ctx->ba_meta_end_block -
                        kv_init_ctx->ba_meta_start_block + 1;
                    kv_init_ctx->ba_meta_size =
                        kv_init_ctx->ba_meta_num_blks *
                        super_block->logi_blk_size_in_bytes;
                    // BA meta read multiple times instead in a single shot
                    kv_init_ctx->ba_disk_read_total_it =
                        kv_init_ctx->ba_meta_size / BA_META_DISK_READ_SZ_MB;
                    if (kv_init_ctx->ba_meta_size %
                            BA_META_DISK_READ_SZ_MB != 0) {
                        kv_init_ctx->ba_disk_read_total_it++;
                    }
                    kv_init_ctx->ba_meta_num_blks_per_iter =
                        BA_META_DISK_READ_SZ_MB /
                        kv_init_ctx->logical_block_size;

                    spdk_dma_free(kv_init_ctx->data);
                    // Free and assign memory appropriately
                    kv_init_ctx->data =
                        spdk_dma_zmalloc(
                                BA_META_DISK_READ_SZ_MB, 4096, NULL);
                    DSS_ASSERT(kv_init_ctx->data != NULL);
                    kv_init_ctx->data_len = BA_META_DISK_READ_SZ_MB;
                    // Reset current IO task, since super block has been read
                    iot_rc = dss_io_task_reset_ops(req->io_task);
                    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                    DSS_ASSERT(req->io_task != NULL);
                    DSS_ASSERT(kv_init_ctx->data_len >=
                            dss_io_dev_get_user_blk_sz(kv_init_ctx->dev)
                                * DSS_KVT_SUPERBLOCK_NUM_BLOCKS);
                    // Complete reading super-block
                    kv_init_ctx->state = DSS_KVT_LOAD_SUPERBLOCK_COMPLETE;
                } else {
                    uint64_t total_blocks;
                    uint64_t ba_meta_sz_blocks;

                    DSS_ASSERT(BLOCK_SIZE%dss_io_dev_get_disk_blk_sz(params.dev) == 0);
                    total_blocks = dss_io_dev_get_disk_num_blks(params.dev)/(BLOCK_SIZE / dss_io_dev_get_disk_blk_sz(params.dev));
                    ba_meta_sz_blocks = _dss_calculate_default_ba_meta_sz_blocks(total_blocks);
                    DSS_ASSERT(ba_meta_sz_blocks + DSS_DEFAULT_KV_META_START_OFFSET < total_blocks);

                    spdk_dma_free(kv_init_ctx->data);
                    kv_init_ctx->data = NULL;

                    kv_init_ctx->logical_block_size = BLOCK_SIZE;
                    params.logi_blk_size = BLOCK_SIZE;
                    params.logi_blk_num = total_blocks - ba_meta_sz_blocks - DSS_DEFAULT_KV_META_START_OFFSET;
                    params.blk_alloc_meta_start_offset = DSS_DEFAULT_KV_META_START_OFFSET;
                    params.blk_offset = ba_meta_sz_blocks + DSS_DEFAULT_KV_META_START_OFFSET;
                    params.blk_alloc_name = DEFAULT_BLK_ALLOC_NAME;

                    *kv_init_ctx->kvt_ctx = init_kvtrans_ctx(&params);
                    kv_init_ctx->state = DSS_KVT_INITIALIZED;
                }
                break;
            case DSS_KVT_LOAD_SUPERBLOCK_COMPLETE:
                // Issue read IO operation for block allocator meta-block
                //kv_init_ctx->state = DSS_KVT_LOADING_BA_META;
                iot_rc = dss_io_task_add_blk_read(
                        req->io_task,
                        kv_init_ctx->dev,
                        kv_init_ctx->ba_meta_start_block,
                        kv_init_ctx->ba_meta_num_blks_per_iter, 
                        kv_init_ctx->data,
                        NULL);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                iot_rc = dss_io_task_submit(req->io_task);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                kv_init_ctx->ba_meta_start_block_per_iter =
                    kv_init_ctx->ba_meta_start_block +
                    kv_init_ctx->ba_meta_num_blks_per_iter;
                break;
            case DSS_KVT_LOADING_BA_META:
                // Read into BA thru
                // dss_blk_allocator_load_meta_from_disk_data
                blk_alloc_ctx = (*kv_init_ctx->kvt_ctx)->blk_alloc_ctx;
                DSS_ASSERT(blk_alloc_ctx != NULL);
                dss_blk_allocator_load_meta_from_disk_data(
                        blk_alloc_ctx,
                        kv_init_ctx->data,
                        BA_META_DISK_READ_SZ_MB,
                        kv_init_ctx->ba_disk_read_it * BA_META_DISK_READ_SZ_MB);
                kv_init_ctx->ba_disk_read_it++;
                if (kv_init_ctx->ba_disk_read_it == 
                        kv_init_ctx->ba_disk_read_total_it + 1) {
                    // Initialize reading DC hash table, proceed to next
                    // phase
                    kv_init_ctx->state = DSS_KVT_LOADING_DC_HT;
                    DSS_ASSERT(kv_init_ctx->dss_mdc_lba == 0);
                    DSS_NOTICELOG("BA loading bitmap completed\n");
                    break;
                }
                // Reset current IO task, since super block has been read
                iot_rc = dss_io_task_reset_ops(req->io_task);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                DSS_ASSERT(req->io_task != NULL);
                DSS_ASSERT(kv_init_ctx->data_len >=
                        dss_io_dev_get_user_blk_sz(kv_init_ctx->dev)
                            * DSS_KVT_SUPERBLOCK_NUM_BLOCKS);
                // Issue read IO operation for block allocator meta-block
                //kv_init_ctx->state = DSS_KVT_LOADING_BA_META;
                iot_rc = dss_io_task_add_blk_read(
                        req->io_task,
                        kv_init_ctx->dev,
                        kv_init_ctx->ba_meta_start_block_per_iter,
                        kv_init_ctx->ba_meta_num_blks_per_iter, 
                        kv_init_ctx->data,
                        NULL);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                iot_rc = dss_io_task_submit(req->io_task);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                kv_init_ctx->ba_meta_start_block_per_iter =
                    kv_init_ctx->ba_meta_start_block_per_iter +
                    kv_init_ctx->ba_meta_num_blks_per_iter;
                break;
             case DSS_KVT_LOADING_DC_HT:
                if (kv_init_ctx->dss_mdc_lba == 0) {
                    // init varible for dc table construction
                    DSS_NOTICELOG("DC table construction starts\n");
                    kv_init_ctx->mdc_io_depth = DEFAULT_MDC_IO_DEPTH;
                    kv_init_ctx->last_batch = false;
                    kv_init_ctx->mdc_io_count = 0;
                    kv_init_ctx->dc_ent_sz = sizeof(dc_item_t);
                    kv_init_ctx->data = spdk_dma_zmalloc(kvt_ctx->blk_size * kv_init_ctx->mdc_io_depth, 4096, NULL);
                    kv_init_ctx->dc_entries = calloc(kv_init_ctx->mdc_io_depth, kv_init_ctx->dc_ent_sz);
                    kv_init_ctx->dss_mdc_lba = kvt_ctx->blk_offset - 1;
                    iot_rc = dss_io_task_reset_ops(req->io_task);
                    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                } else {
                    // iterate over all buffers of outstanding mdc IOs.
                    for (mdc_io_idx = 0; mdc_io_idx<kv_init_ctx->mdc_io_count; mdc_io_idx++) {
                        mdc_buffer = (void *) (mdc_io_idx * kvt_ctx->blk_size + (char *) kv_init_ctx->data);
                        dc_entry = (dc_item_t *) (mdc_io_idx * kv_init_ctx->dc_ent_sz + (char *) kv_init_ctx->dc_entries);
                        if (boot_dc_insert_elm(kvt_ctx, dc_entry, mdc_buffer)) {
                            DSS_ERRLOG("Insert element to dc table in boot failed for index [%u]\n", kv_init_ctx->dss_mdc_lba);
                            assert(0);
                        }
                    }

                    DSS_DEBUGLOG(DSS_KVTRANS, "Insert %d dc entries to dc_tbl.\n", mdc_io_idx);

                    memset(kv_init_ctx->data, 0, kvt_ctx->blk_size * kv_init_ctx->mdc_io_depth);
                    memset(kv_init_ctx->dc_entries, 0, kv_init_ctx->dc_ent_sz * kv_init_ctx->mdc_io_depth);
                    iot_rc = dss_io_task_reset_ops(req->io_task);
                    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                }

                for (mdc_io_idx = 0 ; mdc_io_idx<kv_init_ctx->mdc_io_depth; mdc_io_idx++) {
                    mdc_buffer = (void *) (mdc_io_idx * kvt_ctx->blk_size + (char *) kv_init_ctx->data);
                    dc_entry = (dc_item_t *) (mdc_io_idx * kv_init_ctx->dc_ent_sz + (char *) kv_init_ctx->dc_entries);
                    DSS_ASSERT(mdc_buffer);
                    if (boot_dc_get_next_dmc_blk(kv_init_ctx)) {
                        DSS_ERRLOG("DC Construction failed\n");
                        assert(0);
                    }
                    if (kv_init_ctx->dss_mdc_lba == kvt_ctx->blk_offset + kvt_ctx->blk_num - 1) {
                        kv_init_ctx->last_batch = true;
                        kv_init_ctx->mdc_io_count = mdc_io_idx;
                        break;
                    }
                    iot_rc = dss_io_task_add_blk_read(
                                        req->io_task,
                                        kv_init_ctx->dev,
                                        kv_init_ctx->dss_mdc_lba,
                                        1, 
                                        mdc_buffer,
                                        NULL);
                    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                    dc_entry->mdc_index = kv_init_ctx->dss_mdc_lba;
                }

                if (mdc_io_idx>0) {
                    kv_init_ctx->mdc_io_count = mdc_io_idx;
                    iot_rc = dss_io_task_submit(req->io_task);
                    DSS_DEBUGLOG(DSS_KVTRANS, "Submit %d mdc I/Os.\n", mdc_io_idx);
                }

                if (mdc_io_idx<kv_init_ctx->last_batch && kv_init_ctx->mdc_io_count==0) {
                    // reach to the last blk && no outstanding IOs
                    DSS_NOTICELOG("DC table construction finished\n");
                    if ((*kv_init_ctx->kvt_ctx)->dump_mem_meta) {
                        dss_kvtrans_dump_in_memory_meta(*kv_init_ctx->kvt_ctx);
                        DSS_NOTICELOG("In-memory data dumping finished\n");
                    }
                    kv_init_ctx->state = DSS_KVT_INITIALIZED;
                }
                break;
                
            case DSS_KVT_INITIALIZED:
                dss_module_dec_async_pending_task(req->module_ctx[DSS_MODULE_KVTRANS].module);
                
                if (has_no_elm((*kv_init_ctx->kvt_ctx)->dc_cache_tbl)) {
                    DSS_NOTICELOG("Loading done. DC table is empty\n");
                }
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

        //Exclusive handling of DSS_KVT_LOADING_BA_META state
        dss_kvt_init_ctx_t *kv_init_ctx =
            &req->module_ctx[DSS_MODULE_KVTRANS].mreq_ctx.kvt_init;
        if (kv_init_ctx->state == DSS_KVT_LOAD_SUPERBLOCK_COMPLETE) {
            /* There will be only 1 since IO issue with the same state
             * indicating that the super block has been loaded.
             * When The IO is completed, its payload will contain the
             * first chunk of BA meta-data, thus we switch the state
             * here
             */
            kv_init_ctx->state = DSS_KVT_LOADING_BA_META;
        }

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
        // key not found is handled in dss_kvtrans_net_request_complete function
        if (!(rc == KVTRANS_STATUS_NOT_FOUND && kreq->state == REQ_CMPL)) {
            DSS_ERRLOG("kvt_ctx [%p] returns code [%d] for key [%s]\n", kvt_ctx, rc, kreq->req.req_key.key);
            kreq->state = REQ_CMPL;
        }
    }

    if (kreq->state == REQ_CMPL) {
        DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: meta blks [%zu], collision blks[%zu], meta data collision blks [%zu]\n",
            kvt_ctx,
            kvt_ctx->stat.meta, kvt_ctx->stat.mc,
            kvt_ctx->stat.dc, kvt_ctx->stat.mdc);
        free_kvtrans_req(kreq);
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
            if (v->length != 0) {
                // This check is in place to support
                // writing zero byte keys
                DSS_ASSERT(v->value);
            }
            break;
        case DSS_NVMF_KV_IO_OPC_RETRIEVE:
            kreq->req.opc = KVTRANS_OPC_RETRIEVE;
            if (v->length != 0) {
                // This check is in place to support
                // reading zero byte keys
                DSS_ASSERT(v->value);
            }
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
    dfly_module_stop(subsystem->mlist.dss_kv_trans_module, cb, cb_arg);
    return;
}
