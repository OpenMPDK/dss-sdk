/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2023 Samsung Electronics Co., Ltd.
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

#include "kvtrans.h"
#include "kvtrans_hash.h"
#include "dss_block_allocator.h"

#ifndef DSS_BUILD_CUNIT_TEST
#include "dss_spdk_wrapper.h"

#define TRACE_KVTRANS_WRITE_REQ_INITIALIZED         SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x0)
#define TRACE_KVTRANS_WRITE_QUEUE_TO_LOAD_ENTRY     SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x1)
#define TRACE_KVTRANS_WRITE_ENTRY_LOADING_DONE      SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x2)
#define TRACE_KVTRANS_WRITE_QUEUE_TO_LOAD_COL       SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x3)
#define TRACE_KVTRANS_WRITE_COL_LOADING_DONE        SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x4)
#define TRACE_KVTRANS_WRITE_QUEUE_TO_LOAD_COL_EXT   SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x5)
#define TRACE_KVTRANS_WRITE_QUEUE_TO_START_IO       SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x6)
#define TRACE_KVTRANS_WRITE_IO_CMPL                 SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x7)
#define TRACE_KVTRANS_WRITE_REQ_CMPL                SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x8)

#define TRACE_KVTRANS_READ_REQ_INITIALIZED          SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0x9)
#define TRACE_KVTRANS_READ_ENTRY_LOADING_DONE       SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0xa)
#define TRACE_KVTRANS_READ_QUEUE_TO_START_IO        SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0xb)
#define TRACE_KVTRANS_READ_IO_CMPL                  SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0xc)
#define TRACE_KVTRANS_READ_REQ_CMPL                 SPDK_TPOINT_ID(TRACE_GROUP_KVT, 0xd)


SPDK_TRACE_REGISTER_FN(kvtrans_trace, "kvtrans", TRACE_GROUP_KVT)
{
    spdk_trace_register_object(OBJECT_KVTRANS_IO, 'k');
    
    spdk_trace_register_description("KVT_KEY_REQ_INITIALIZED", TRACE_KVTRANS_WRITE_REQ_INITIALIZED,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 1, 1, "dreq_ptr:    ");
    spdk_trace_register_description("KVT_KEY_QUEUE_ENTRY", TRACE_KVTRANS_WRITE_QUEUE_TO_LOAD_ENTRY,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:     ");
    spdk_trace_register_description("KVT_KEY_ENTRY_DONE", TRACE_KVTRANS_WRITE_ENTRY_LOADING_DONE,
                                        OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:     ");
    spdk_trace_register_description("KVT_KEY_QUEUE_COL", TRACE_KVTRANS_WRITE_QUEUE_TO_LOAD_COL,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:     ");
    spdk_trace_register_description("KVT_KEY_COL_DONE", TRACE_KVTRANS_WRITE_COL_LOADING_DONE,
                                        OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:     ");
    spdk_trace_register_description("KVT_KEY_QUEUE_IO", TRACE_KVTRANS_WRITE_QUEUE_TO_START_IO,
                                        OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:     ");
    spdk_trace_register_description("KVT_KEY_IO_CMPL", TRACE_KVTRANS_WRITE_IO_CMPL,
                                        OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:   ");
    spdk_trace_register_description("KVT_KEY_REQ_CMPL", TRACE_KVTRANS_WRITE_REQ_CMPL,
                                        OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:   ");
    
    spdk_trace_register_description("KVT_VAL_REQ_INITIALIZED", TRACE_KVTRANS_READ_REQ_INITIALIZED,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 1, 1, "dreq_ptr:    ");
    spdk_trace_register_description("KVT_VAL_ENTRY_DONE", TRACE_KVTRANS_READ_ENTRY_LOADING_DONE,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:    ");
    spdk_trace_register_description("KVT_VAL_QUEUE_IO", TRACE_KVTRANS_READ_QUEUE_TO_START_IO,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:    ");
    spdk_trace_register_description("KVT_VAL_IO_CMPL", TRACE_KVTRANS_READ_IO_CMPL,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:    ");
    spdk_trace_register_description("KVT_VAL_REQ_CMPL", TRACE_KVTRANS_READ_REQ_CMPL,
                                    OWNER_NONE, OBJECT_KVTRANS_IO, 0, 1, "dreq_ptr:    ");
}
#endif

#ifdef MEM_BACKEND

#ifndef DSS_BUILD_CUNIT_TEST
bool g_disk_as_data_store = true;
bool g_disk_as_meta_store = true;
#else
bool g_disk_as_data_store = false;
bool g_disk_as_meta_store = false;
#endif

bool g_ba_enable_meta_sync = false;
bool g_dump_mem_meta = false;

#include "kvtrans_mem_backend.h"

void set_kvtrans_disk_data_store(bool val) {
    g_disk_as_data_store = val;
}

void set_kvtrans_disk_meta_store(bool val) {
    g_disk_as_meta_store = val;
}

void set_kvtrans_ba_meta_sync_enabled(bool val) {
    g_ba_enable_meta_sync = val;
}

void set_kvtrans_dump_mem_meta_enabled(bool val) {
    g_dump_mem_meta = val;
}
#else
void set_kvtrans_disk_data_store(bool val) {
    DSS_NOTICELOG("kvtrans disk data store config not available\n");
    return;
}

void set_kvtrans_disk_meta_store(bool val) {
        DSS_NOTICELOG("kvtrans disk meta store config not available\n");
    return;
}
#endif

// TODO: use macro to convert states to strings
const char *stateNames[] = { "Empty", "Meta", "Data", "Collision", "DC", "MDC", "MDC_Entry", "CE", "DC_Empty", "DC_CE"};

// util functions to get time ticks.
// tmp use for benchmarking kvtrans 
// TODO: use spdk_get_ticks
double get_time() {
    struct timeval t;
    // struct timezone tzp;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec*1e-6;
}

void update_ticks(tick_t *tp) {
    *tp = (tick_t) get_time();
}

dss_kvtrans_status_t
dss_kvtrans_alloc_contig(kvtrans_ctx_t *ctx,
                        kvtrans_req_t *kreq,
                        uint64_t state,
                        uint64_t hint_block_index,
                        uint64_t num_blocks,
                        uint64_t *allocated_start_block)
{
    dss_blk_allocator_status_t rc;
    rc = dss_blk_allocator_alloc_blocks_contig(ctx->blk_alloc_ctx, state, 
                hint_block_index, num_blocks, allocated_start_block);
    if (rc) {
        // TODO: error handling
        DSS_ERRLOG("Alloc contig [%d] blks for blk_ctx [%zu] failed\n",
                    num_blocks, hint_block_index);

        return KVTRANS_STATUS_ALLOC_CONTIG_ERROR;
    }

    DSS_DEBUGLOG(DSS_KVTRANS, "Set blk [ %zu, %zu] from state [ Empty ] to [ %s ] for key [ %s ]\n",
        *allocated_start_block, *allocated_start_block + num_blocks - 1, stateNames[state], kreq->req.req_key.key);

    kreq->ba_meta_updated = true;
    return KVTRANS_STATUS_SUCCESS;
}

dss_kvtrans_status_t 
dss_kvtrans_set_blk_state(kvtrans_ctx_t *ctx, blk_ctx_t *blk_ctx, uint64_t index,
                            uint64_t blk_num, blk_state_t state)
{
    dss_blk_allocator_status_t rc;
    uint64_t idx_state, blk_state;
    
    blk_state = DEFAULT_BLOCK_STATE_NUM;

    rc = dss_blk_allocator_get_block_state(ctx->blk_alloc_ctx, 
                                            index,
                                            &blk_state);
    if (rc) {
        // TODO: error handling
        DSS_ERRLOG("Fail to get blk [%d] state\n", blk_ctx->index);
        return KVTRANS_STATUS_ERROR;
    }

    if (state==EMPTY) {   
        rc = dss_blk_allocator_clear_blocks(ctx->blk_alloc_ctx, index, blk_num);
    } else {
        DSS_ASSERT(blk_num==1);
        rc = dss_blk_allocator_set_blocks_state(ctx->blk_alloc_ctx, index, blk_num, state);
    }
    if (rc) {
        // TODO: error handling
        DSS_ERRLOG("Set state failed for [%d] blk_ctx [%zu] from state [%s] to state [%s]\n",
                    blk_num, index, stateNames[blk_ctx->state], stateNames[state]);

        return KVTRANS_STATUS_SET_BLK_STATE_ERROR;
    }
    if(blk_ctx) blk_ctx->kreq->ba_meta_updated = true;
    return KVTRANS_STATUS_SUCCESS;
}

dss_kvtrans_status_t 
dss_kvtrans_set_blks_state(kvtrans_ctx_t *ctx, blk_ctx_t *blk_ctx, uint64_t index,
                            uint64_t blk_num, blk_state_t state)
{
    dss_blk_allocator_status_t rc;
    uint64_t offset; 

    if (state==EMPTY || blk_num==1) {
       return dss_kvtrans_set_blk_state(ctx, blk_ctx, index, blk_num, state);
    } else {
        for (offset=0; offset<blk_num; offset++) {
            rc = dss_blk_allocator_set_blocks_state(ctx->blk_alloc_ctx, index + offset, 1, state);
            if (rc) {
                // TODO: error handling
                DSS_ERRLOG("Set state failed for [%d] blk_ctx [%zu] from state [%s] to state [%s]",
                            blk_num, index, stateNames[blk_ctx->state], stateNames[state]);

                return KVTRANS_STATUS_SET_BLK_STATE_ERROR;
            }
        }
    }

    if(blk_ctx) blk_ctx->kreq->ba_meta_updated = true;
    return KVTRANS_STATUS_SUCCESS;
}

dss_kvtrans_status_t 
dss_kvtrans_get_blk_state(kvtrans_ctx_t *ctx, blk_ctx_t *blk_ctx)
{
    dss_blk_allocator_status_t rc;
    uint64_t blk_state = DEFAULT_BLOCK_STATE_NUM;

    rc = dss_blk_allocator_get_block_state(ctx->blk_alloc_ctx, 
                                            blk_ctx->index,
                                            &blk_state);
    if (rc) {
        // TODO: error handling
        DSS_ERRLOG("Fail to get blk [%d] state\n", blk_ctx->index);
        return KVTRANS_STATUS_ERROR;
    }
    blk_ctx->state = (blk_state_t) blk_state;
    return KVTRANS_STATUS_SUCCESS;
}

void 
dss_kvtrans_check_all_empty(kvtrans_ctx_t *ctx)
{
    dss_blk_allocator_status_t rc;
    uint64_t blk_state = DEFAULT_BLOCK_STATE_NUM;

    uint64_t i;
    bool printed = false;
    for (i = ctx->blk_offset; i < ctx->blk_offset + ctx->blk_num; i++)
    {
        rc = dss_blk_allocator_get_block_state(ctx->blk_alloc_ctx, 
                                            i,
                                            &blk_state);
        if (rc) {
            // TODO: error handling
            DSS_ERRLOG("Fail to get blk [%zu] state\n", i);
            DSS_ASSERT(0);
        }
        if (blk_state != EMPTY && !printed) {
            DSS_ERRLOG("index [%zu] is [%s]\n", i, stateNames[blk_state]);
            printed = 1;
        }
    }
}

dss_kvtrans_status_t
dss_kvtrans_get_free_blk_ctx(kvtrans_ctx_t *ctx, blk_ctx_t **blk_ctx)
{
    dss_mallocator_status_t rc;

    DSS_ASSERT(blk_ctx != NULL);
    DSS_ASSERT(*blk_ctx == NULL);

    rc = dss_mallocator_get(ctx->blk_ctx_mallocator, 0, (dss_mallocator_item_t **)blk_ctx);
    if (*blk_ctx==NULL) return KVTRANS_STATUS_ERROR;
    if (rc==DSS_MALLOC_NEW_ALLOCATION) {
        // alloc blk 
        if ((*blk_ctx)->blk==NULL) {
            return KVTRANS_STATUS_ERROR;
        }
    }
    
    return KVTRANS_STATUS_SUCCESS;
}

void reset_blk_ctx(blk_ctx_t *blk_ctx) {
    if (!blk_ctx)
    {
        DSS_DEBUGLOG(DSS_KVTRANS, "Blk_ctx is none.\n");
        return;
    }
    ondisk_meta_t *blk = blk_ctx->blk;
    if (blk) {
        memset(blk, 0, sizeof(ondisk_meta_t));
    }

    memset(blk_ctx, 0, sizeof(blk_ctx_t));
    
    blk_ctx->blk = blk;

    return;
}

dss_kvtrans_status_t
dss_kvtrans_put_free_blk_ctx(kvtrans_ctx_t *ctx,
                            blk_ctx_t *blk_ctx)
{
    dss_mallocator_status_t rc;
    
    // free all memory space of blk_ctx
    reset_blk_ctx(blk_ctx);
    if (blk_ctx==NULL){
        DSS_ERRLOG("blk_ctx to put is NULL\n");
        DSS_ASSERT(0);
        return KVTRANS_STATUS_ERROR;
    }

    rc = dss_mallocator_put(ctx->blk_ctx_mallocator, 0, (dss_mallocator_item_t *)blk_ctx);
    if (rc==DSS_MALLOC_ERROR) {
        return KVTRANS_STATUS_ERROR;
    }
    return KVTRANS_STATUS_SUCCESS;
}

dss_kvtrans_status_t
dss_kvtrans_dc_table_exist(kvtrans_ctx_t *ctx, 
                            const uint64_t index) {
    if (find_elm(ctx->dc_cache_tbl, index)) {
        return KVTRANS_STATUS_SUCCESS;
    }
    return KVTRANS_STATUS_ERROR;
}

dss_kvtrans_status_t
dss_kvtrans_dc_table_lookup(kvtrans_ctx_t *ctx,
                            const uint64_t dc_index,
                            uint64_t *mdc_index) {
    dc_item_t *it;

    it = (dc_item_t *) get_elm(ctx->dc_cache_tbl, dc_index);
    if (it) {
        *mdc_index = it->mdc_index;
        return KVTRANS_STATUS_SUCCESS;
    }
    return KVTRANS_STATUS_ERROR;
}

dss_kvtrans_status_t
dss_kvtrans_dc_table_update(kvtrans_ctx_t *ctx,
                            blk_ctx_t *blk_ctx,
                            const uint64_t dc_index,
                            blk_state_t ori_state) {

    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    dc_item_t *it;
    blk_state_t state;

    DSS_ASSERT(ori_state==DATA || ori_state==COLLISION_EXTENSION || ori_state==EMPTY);

    switch (ori_state)
    {
    case DATA:
        state = DATA_COLLISION;
        break;
    case COLLISION_EXTENSION:
        state = DATA_COLLISION_CE;
        break;
    case EMPTY:
        state = DATA_COLLISION_EMPTY;
        break;
    default:
        assert(0);
    }

    rc = dss_kvtrans_set_blk_state(ctx, blk_ctx, dc_index, 1, state);
    if (rc) {
        DSS_ERRLOG("Failed to update blk state for [%u] from state [%s] to [%s]\n", dc_index, stateNames[blk_ctx->state], stateNames[state]);
        return rc;
    }

    it = (dc_item_t *) get_elm(ctx->dc_cache_tbl, dc_index);
    if (it) {
        it->ori_state = ori_state;
        if (!store_elm(ctx->dc_cache_tbl, dc_index, (void *)it)) {
            return KVTRANS_STATUS_SUCCESS;
        }
    }
    DSS_ERRLOG("update dc tbl failed for blk [%zu] with original state [%s]\n", dc_index, stateNames[ori_state]);
    return KVTRANS_STATUS_ERROR;
}


dss_kvtrans_status_t
dss_kvtrans_dc_table_insert(kvtrans_ctx_t *ctx,
                            blk_ctx_t *blk_ctx,
                            const uint64_t dc_index,
                            const uint64_t mdc_index,
                            blk_state_t ori_state) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    Word_t *entry;
    dc_item_t *it;
    blk_state_t state;

    if (dss_kvtrans_dc_table_exist(ctx, dc_index)==KVTRANS_STATUS_SUCCESS) {
        printf("ERROR: dc %zu is existent\n", dc_index);
        return KVTRANS_STATUS_ERROR;
    }
    DSS_ASSERT(ori_state==DATA || ori_state==COLLISION_EXTENSION);
    state = ori_state==DATA ? DATA_COLLISION : DATA_COLLISION_CE;

    rc = dss_kvtrans_set_blk_state(ctx, blk_ctx, dc_index, 1, state);
    if (!rc) {
        // the first mdc in the dc chain should be marked as
        // META_DATA_COLLISION_ENTRY for dc_tbl construction in a reboot.
        rc = dss_kvtrans_set_blk_state(ctx, blk_ctx, mdc_index, 1, META_DATA_COLLISION_ENTRY);
        if (rc) {
            // roll back to DATA
            rc = dss_kvtrans_set_blk_state(ctx, blk_ctx, dc_index, 1, DATA);
            if (!rc) {
                return KVTRANS_STATUS_SET_DC_STATE_ERROR;
            }
        }
    }

    it = malloc(sizeof(dc_item_t));
    it->mdc_index = mdc_index;
    it->ori_state = ori_state;

    if(!store_elm(ctx->dc_cache_tbl, dc_index, (void *)it)) {
        return rc;
    }

    DSS_ERRLOG("insert dc tbl failed for data blk [%zu] to [%zu] with original state [%s]\n", dc_index, mdc_index, stateNames[ori_state]);

    return KVTRANS_STATUS_ERROR;
}

dss_kvtrans_status_t
dss_kvtrans_dc_table_delete(kvtrans_ctx_t  *ctx,
                            blk_ctx_t *blk_ctx, 
                            const uint64_t dc_index, 
                            const uint64_t mdc_index) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    Word_t *entry;
    dc_item_t *it;
    int cleaned_bytes;

    it = (dc_item_t *) get_elm(ctx->dc_cache_tbl, dc_index);
    if (!it) {
        return KVTRANS_STATUS_NOT_FOUND;
    }

    if (it->mdc_index!=mdc_index) {
        return KVTRANS_STATUS_NOT_FOUND;
    }
    rc = dss_kvtrans_set_blk_state(ctx, blk_ctx, dc_index, 1, it->ori_state);

    if (!delete_elm(ctx->dc_cache_tbl, dc_index)) {
        ctx->stat.dc--;
        return rc;
    }
    DSS_ERRLOG("delete dc tbl failed for data blk [%zu]\n", dc_index);

    return KVTRANS_STATUS_ERROR;
}

static void write_line(uint64_t dc_idx, void * item, void *file) {
    FILE *out_file = (FILE *) file;
    dc_item_t *dc_item = (dc_item_t *) item;
    fprintf(file, "%zu %zu %d\n",dc_idx, dc_item->mdc_index, dc_item->ori_state);
    return;
}

int
dss_kvtrans_dump_dc_tbl(kvtrans_ctx_t  *ctx, const char* file_path) {
    int rc;

    FILE *out_file = fopen(file_path, "w");
    if (out_file == NULL) {
        assert(0);
    }
    rc = for_each_elm_fn(ctx->dc_cache_tbl, write_line, (void *)out_file);
    fclose(out_file);
    return rc;
}


#ifndef DSS_BUILD_CUNIT_TEST
bool _is_lba_dirty(kvtrans_meta_sync_ctx_t *meta_sync_ctx, uint64_t blk_idx) {
    DSS_ASSERT(meta_sync_ctx);
    return Judy1Test(meta_sync_ctx->dirty_meta_lba, blk_idx, PJE0);
}

bool
_modify_meta(const kvtrans_req_t *kreq) {
    if (!kreq) {
        DSS_ERRLOG("Invalid kreq\n");
        return false;
    }
    switch (kreq->req.opc)
    {
    case KVTRANS_OPC_STORE:
        return true;
    case KVTRANS_OPC_DELETE:
        return true;
    case KVTRANS_OPC_RETRIEVE:
        return false;
    case KVTRANS_OPC_EXIST:
        return false;
    default:
        DSS_ERRLOG("Invalid opc\n");
        return false;
    }
}

dss_kvtrans_status_t
_set_lba_dirty(kvtrans_meta_sync_ctx_t *meta_sync_ctx, uint64_t blk_idx) {
    DSS_ASSERT(meta_sync_ctx);
    if (Judy1Set(&meta_sync_ctx->dirty_meta_lba, blk_idx, PJE0)) {
        // 1 if Index's bit was previously unset (successful)
        DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: setting lb[%u] dirty succeeded.\n", meta_sync_ctx->kvtrans_ctx, blk_idx);
        return KVTRANS_STATUS_SUCCESS;
    }
        DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: setting lb[%u] dirty failed.\n", meta_sync_ctx->kvtrans_ctx, blk_idx);
    return KVTRANS_STATUS_ERROR;
}

dss_kvtrans_status_t
_set_lba_free(kvtrans_meta_sync_ctx_t *meta_sync_ctx, uint64_t blk_idx) {
    DSS_ASSERT(meta_sync_ctx);
    if (Judy1Unset(&meta_sync_ctx->dirty_meta_lba, blk_idx, PJE0)) {
        // 1 if Index's bit was previously set (successful)
        DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: freeing dirty lb[%u] succeeded.\n", meta_sync_ctx->kvtrans_ctx, blk_idx);
        return KVTRANS_STATUS_SUCCESS;
    }
        DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: freeing dirty lb[%u] failed.\n", meta_sync_ctx->kvtrans_ctx, blk_idx);
    return KVTRANS_STATUS_ERROR;
}

dss_kvtrans_status_t
_sync_meta_blk_to_queue(kvtrans_meta_sync_ctx_t *meta_sync_ctx, blk_ctx_t *blk_ctx) {
    dss_kvtrans_status_t rc;

    DSS_ASSERT(meta_sync_ctx);
    kvtrans_ctx_t *kvtrans_ctx = blk_ctx->kreq->kvtrans_ctx;
    if (_is_lba_dirty(meta_sync_ctx, blk_ctx->index)) {
        STAILQ_INSERT_TAIL(&meta_sync_ctx->kv_req_queue, blk_ctx->kreq, meta_sync_link);
        meta_sync_ctx->queue_length ++;
        // TODO: Add counter for queue length
        DSS_NOTICELOG("KVTRANS[%p]: kreq with key [%s] queue on blk [%zu] [%d]. meta sync ctx queue length is %d\n", 
                        kvtrans_ctx, blk_ctx->kreq->req.req_key.key, blk_ctx->index, blk_ctx->state, meta_sync_ctx->queue_length);
        return KVTRANS_META_SYNC_TRUE;
    }

    if (_modify_meta(blk_ctx->kreq)) {
        // lock lba at blk_ctx->index only if kreq is going to modify meta block possibly
        rc = _set_lba_dirty(meta_sync_ctx, blk_ctx->index);
        if (rc) return rc;
    }
    return KVTRANS_META_SYNC_FALSE;
}

dss_kvtrans_status_t
_pop_meta_blk_from_queue(kvtrans_meta_sync_ctx_t *meta_sync_ctx, blk_ctx_t *in_flight_blk) {
    dss_io_task_status_t iot_rc;
    dss_kvtrans_status_t rc;
    kvtrans_req_t *kreq;
    blk_ctx_t *blk_ctx;
    int num_blks;
    int i = 0;
    uint64_t blk_idx = in_flight_blk->index;

    DSS_ASSERT(meta_sync_ctx);

    // check if the lba was first queued
    if (!_is_lba_dirty(meta_sync_ctx, in_flight_blk->index)) {
        DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS[%p]: Trying to pop non-dirty lb[%u]\n", 
                meta_sync_ctx->kvtrans_ctx, in_flight_blk->index);
        return KVTRANS_META_SYNC_FALSE;
    }

    // DSS_ASSERT(!_is_lba_dirty(meta_sync_ctx, blk_idx));
    rc = _set_lba_free(meta_sync_ctx, in_flight_blk->index);
    if (rc) {
        DSS_ERRLOG("KVTRANS[%p]: failed to free dirty lb[%u]\n", 
                meta_sync_ctx->kvtrans_ctx, in_flight_blk->index);
        return KVTRANS_STATUS_ERROR;
    }

    // Iterate all kvreq in queue to see any one dependent on blk_idx
    // Outer loop searches for the completed kreq in the in-flight kreq queue
    STAILQ_FOREACH(kreq, &meta_sync_ctx->kv_req_queue, meta_sync_link) {
        num_blks = kreq->num_meta_blk;
        // Inner loop searches for meta-blocks related to kreq that can be
        // scheduled to drive
        TAILQ_FOREACH(blk_ctx, &kreq->meta_chain, blk_link) {
            i++;
            if (blk_ctx->index == blk_idx) {
                DSS_ASSERT(i == num_blks);
                DSS_ASSERT(meta_sync_ctx->queue_length > 0);
                STAILQ_REMOVE(&meta_sync_ctx->kv_req_queue, kreq, kvtrans_req, meta_sync_link);
                meta_sync_ctx->queue_length --;
                if (_modify_meta(blk_ctx->kreq)) {
                    rc = _set_lba_dirty(meta_sync_ctx, blk_idx);
                    if (rc) return rc;
                }
                // it's possbile the state has been changed during queuing
                rc = dss_kvtrans_get_blk_state(kreq->kvtrans_ctx, blk_ctx);
                DSS_ASSERT(rc == KVTRANS_STATUS_SUCCESS);
                // TODO: optimize for empty blocks. abort io_task, call _kvtrans_key_ops directly

                iot_rc = dss_io_task_submit(kreq->io_tasks);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                // TODO: Add counter for queue length
                DSS_NOTICELOG( "KVTRANS[%p]: kreq with key [%s] queue on blk [%zu] [%d]. meta sync ctx queue length is %d\n", 
                        meta_sync_ctx->kvtrans_ctx, kreq->req.req_key.key, blk_ctx->index, in_flight_blk->state, meta_sync_ctx->queue_length);
                return KVTRANS_META_SYNC_TRUE;
            }
        }
        // Reset iterator as a different kreq will have its own meta-blocks
        // or blk_ctx
        i = 0;
    }

    return KVTRANS_META_SYNC_FALSE;
}
#endif


// Loads meta block from drive to memory
dss_kvtrans_status_t
dss_kvtrans_queue_load_ondisk_blk(blk_ctx_t *blk_ctx,
                                    kvtrans_req_t *kreq)
{
    dss_io_task_status_t iot_rc;
    dss_io_opts_t io_opts = {.mod_id = DSS_IO_OP_OWNER_KVTRANS, .is_blocking = true};
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    // Set dirty here ?
    iot_rc = dss_io_task_add_blk_read(kreq->io_tasks, 
                                    kvtrans_ctx->target_dev,
                                    blk_ctx->index,
                                    1, 
                                    (void *) blk_ctx->blk, 
                                    &io_opts);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
    kreq->io_to_queue = true;
    return KVTRANS_IO_QUEUED;
}

dss_kvtrans_status_t 
dss_kvtrans_load_ondisk_blk(blk_ctx_t *blk_ctx,
                            kvtrans_req_t *kreq,
                            bool submit_for_disk_io) {
    DSS_ASSERT(blk_ctx->index!=0);
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
#ifdef MEM_BACKEND

#ifndef DSS_BUILD_CUNIT_TEST
    if (g_disk_as_meta_store == true) {
        dss_io_task_status_t iot_rc;
        rc = dss_kvtrans_queue_load_ondisk_blk(blk_ctx, kreq);
        if (submit_for_disk_io) {
            kreq->io_to_queue = false;
            if(_sync_meta_blk_to_queue(kreq->kvtrans_ctx->meta_sync_ctx, blk_ctx) == KVTRANS_META_SYNC_FALSE) {
                iot_rc = dss_io_task_submit(kreq->io_tasks);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
            }
            // IO is submitted to either io task module or meta_sync_queue
            rc = KVTRANS_IO_SUBMITTED;
        }
        return rc;
    } else {
        val_t val;
        val = load_meta(kreq->kvtrans_ctx->meta_ctx, blk_ctx->index);
        if(val) {
            memcpy(blk_ctx->blk, val, sizeof(ondisk_meta_t));
            return rc;
        } else {
            return KVTRANS_STATUS_ERROR;
        }
    }
#else
    val_t val;
    val = load_meta(kreq->kvtrans_ctx->meta_ctx, blk_ctx->index);
    if(val) {
        memcpy(blk_ctx->blk, val, sizeof(ondisk_meta_t));
        return rc;
    } else {
        return KVTRANS_STATUS_ERROR;
    }
#endif
#else
    dss_io_task_status_t iot_rc;
    rc = dss_kvtrans_queue_load_ondisk_blk(blk_ctx, kreq);
    if (submit_for_disk_io) {
        iot_rc = dss_io_task_submit(kreq->io_tasks);
        kreq->io_to_queue = false;
        DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
        rc = KVTRANS_IO_SUBMITTED;
    }
    return rc;
#endif
}

dss_kvtrans_status_t
dss_kvtrans_load_ondisk_data(blk_ctx_t *blk_ctx, 
                            kvtrans_req_t *kreq,
                            bool submit_for_disk_io) 
{
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    ondisk_meta_t *blk = blk_ctx->blk;
    req_t *req = &kreq->req;
    int i;
    uint64_t offset = 0;
    dss_io_task_status_t iot_rc;

#ifdef MEM_BACKEND
    DSS_RELEASE_ASSERT(blk->value_location != INLINE);
    for (i=0; i<blk->num_valid_place_value_entry; i++) {
#ifndef DSS_BUILD_CUNIT_TEST
        kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
        if (g_disk_as_data_store == true) {
            DSS_DEBUGLOG(DSS_KVTRANS, "Key [%s] LBA [%x] nBlks [%x] value [%p] offset [%x] blk_sz [%d] io_index [%d]\n", \
                            kreq->req.req_key.key, blk->place_value[i].value_index, blk->place_value[i].num_chunks, \
                            kreq->req.req_value.value, offset, kreq->kvtrans_ctx->blk_size,  i);
            iot_rc = dss_io_task_add_blk_read(kreq->io_tasks, \
                                        kvtrans_ctx->target_dev, \
                                        blk->place_value[i].value_index, \
                                        blk->place_value[i].num_chunks, \
                                        (void *)((uint64_t )kreq->req.req_value.value + offset), \
                                        NULL);
            DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
            kreq->io_to_queue = true;
         } else
#endif
            if (!retrieve_data(kreq->kvtrans_ctx->data_ctx, 
                                blk->place_value[i].value_index, 
                                blk->place_value[i].num_chunks, 
                                (void *)((char *)req->req_value.value + offset))) {
                    rc = KVTRANS_STATUS_IO_ERROR;
                    break;
            }
            rc = KVTRANS_STATUS_SUCCESS;
        offset += blk->place_value[i].num_chunks * kreq->kvtrans_ctx->blk_size;
    }
#ifndef DSS_BUILD_CUNIT_TEST
    if(submit_for_disk_io == true) {
        kreq->state = QUEUED_FOR_DATA_IO;
        iot_rc = dss_io_task_submit(kreq->io_tasks);
        DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
        kreq->io_to_queue = false;
        rc = KVTRANS_STATUS_SUCCESS;
    } else
#endif
    kreq->state = REQ_CMPL;
    return rc;
#else
    uint64_t blk_num;
    uint64_t index;

    // TODO: integrate io_task
    DSS_ASSERT(blk->num_valid_place_value_entry>0);
    offset = 0;
    for (int i=0; i<blk->num_valid_place_value_entry; i++) {
        index = blk->place_value[i].value_index;
        blk_num = blk->place_value[i].num_chunks;
        if(dss_io_task_add_blk_read(kreq->io_tasks, kvtrans_ctx->target_dev,
            index, blk_num, kreq->req->req_value.value, blk_num * kreq->kvtrans_ctx->blk_size, offset, false)) {
                return KVTRANS_STATUS_ERROR;
            }
        offset += blk_num * kreq->kvtrans_ctx->blk_size;
    }
    kreq->req->req_value.length = offset;
    kreq->req->req_value.offset = 0;

    kreq->state = QUEUED_FOR_DATA_IO;
    iot_rc = dss_io_task_submit(kreq->io_tasks);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
    return KVTRANS_STATUS_SUCCESS;
#endif
}

dss_kvtrans_status_t
dss_kvtrans_queue_write_ondisk_blk(blk_ctx_t *blk_ctx,
                                    kvtrans_req_t *kreq) 
{
    dss_io_task_status_t iot_rc;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    dss_io_opts_t io_opts = {.mod_id = DSS_IO_OP_OWNER_KVTRANS,
                             .is_blocking = false};


    iot_rc = dss_io_task_add_blk_write(kreq->io_tasks, 
                                    kvtrans_ctx->target_dev,
                                    blk_ctx->index,
                                    1, 
                                    (void *) blk_ctx->blk, 
                                    &io_opts);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
    kreq->io_to_queue = true;
#ifndef DSS_BUILD_CUNIT_TEST
    if(!_is_lba_dirty(kreq->kvtrans_ctx->meta_sync_ctx, blk_ctx->index)) {
        _set_lba_dirty(kreq->kvtrans_ctx->meta_sync_ctx, blk_ctx->index);
    }
#endif
    return KVTRANS_IO_QUEUED;
}

dss_kvtrans_status_t 
dss_kvtrans_write_ondisk_blk(blk_ctx_t *blk_ctx, 
                            kvtrans_req_t *kreq, 
                            bool submit_for_disk_io) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
#ifdef MEM_BACKEND
#ifndef DSS_BUILD_CUNIT_TEST
        if (g_disk_as_meta_store == true) {
            dss_io_task_status_t iot_rc;
            rc = dss_kvtrans_queue_write_ondisk_blk(blk_ctx, kreq);
            if (submit_for_disk_io) {
                iot_rc = dss_io_task_submit(kreq->io_tasks);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                rc = KVTRANS_IO_SUBMITTED;
                kreq->io_to_queue = false;
            }
            return rc;
        } else {
            insert_meta(kreq->kvtrans_ctx->meta_ctx, 
                blk_ctx->index,
                blk_ctx->blk);
        }
#else
    insert_meta(kreq->kvtrans_ctx->meta_ctx, 
                blk_ctx->index,
                blk_ctx->blk);
    return rc;
#endif
#else
    dss_io_task_status_t iot_rc;
    rc = dss_kvtrans_queue_write_ondisk_blk(blk_ctx, kreq);
    if (submit_for_disk_io) {
        iot_rc = dss_io_task_submit(kreq->io_tasks);
        DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
        rc = KVTRANS_IO_SUBMITTED;
        kreq->io_to_queue = false;
    }
    return rc;
#endif
}

dss_kvtrans_status_t
dss_kvtrans_delete_ondisk_blk(blk_ctx_t *blk_ctx, 
                                kvtrans_req_t *kreq)
{
    blk_ctx->state = EMPTY;
#ifdef MEM_BACKEND
#ifndef  DSS_BUILD_CUNIT_TEST
    if (g_disk_as_meta_store) {
        return KVTRANS_STATUS_SUCCESS;
    } else {
        delete_meta(kreq->kvtrans_ctx->meta_ctx, 
                blk_ctx->index);
        return KVTRANS_STATUS_SUCCESS;
    }
#else
    delete_meta(kreq->kvtrans_ctx->meta_ctx, 
                blk_ctx->index);
    return KVTRANS_STATUS_SUCCESS;
#endif
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    return KVTRANS_STATUS_SUCCESS;
#else
    return KVTRANS_STATUS_SUCCESS;
#endif
}

// put meta and data in one buffer
void *_serialize_data(void *buf1, int len1, void *buf2, int len2) {
    // TODO: use scatter gather list
    void * buff;
    buff = malloc(len1+len2);
    memcpy(buff, buf1, len1);
    memcpy(buff+len1, buf2, len2);
    return buff;
}

dss_kvtrans_status_t
dss_kvtrans_write_ondisk_data(blk_ctx_t *blk_ctx,
                            kvtrans_req_t *kreq,
                            bool submit_for_disk_io) 
{
    dss_kvtrans_status_t rc;
    ondisk_meta_t *blk = blk_ctx->blk;
    req_t *req = &kreq->req;
    dss_io_opts_t io_opts = {.mod_id = DSS_IO_OP_OWNER_KVTRANS,
                             .is_blocking = false};

    int i;
    int offset = 0;
    dss_io_task_status_t iot_rc;

#ifdef MEM_BACKEND
    if (blk->value_location!=INLINE) {
        for (i=0; i<blk->num_valid_place_value_entry; i++) {
#ifndef DSS_BUILD_CUNIT_TEST
            if (g_disk_as_data_store == true) {
                DSS_DEBUGLOG(DSS_KVTRANS, "Key [%s] LBA [%x] nBlks [%x] value [%p] offset [%x] blk_sz [%d] io_index [%d]\n", \
                            kreq->req.req_key.key, blk->place_value[i].value_index, blk->place_value[i].num_chunks, \
                            kreq->req.req_value.value, offset, kreq->kvtrans_ctx->blk_size,  i);
                iot_rc = dss_io_task_add_blk_write(kreq->io_tasks, \
                                        kreq->kvtrans_ctx->target_dev, \
                                        blk->place_value[i].value_index, \
                                        blk->place_value[i].num_chunks, \
                                        (void *)((uint64_t )kreq->req.req_value.value + offset), \
                                        &io_opts);
                DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
                kreq->io_to_queue = true;
            } else
#endif
                if (!insert_data(kreq->kvtrans_ctx->data_ctx, 
                                    blk->place_value[i].value_index, 
                                    blk->place_value[i].num_chunks,
                                    (void *)((char *)req->req_value.value+offset))) {
                       rc = KVTRANS_STATUS_IO_ERROR;
                }
            rc = KVTRANS_STATUS_SUCCESS;
            offset += blk_ctx->blk->place_value[i].num_chunks * kreq->kvtrans_ctx->blk_size;
        }
    }
#ifndef DSS_BUILD_CUNIT_TEST
    if(submit_for_disk_io == true) {
        kreq->state = QUEUED_FOR_DATA_IO;
        if(kreq->kvtrans_ctx->is_ba_meta_sync_enabled) {
            dss_blk_allocator_status_t ba_rc;
            DSS_DEBUGLOG(DSS_KVTRANS, "Queued [%p] task\n", kreq->io_tasks);
            ba_rc = dss_blk_allocator_queue_sync_meta_io_tasks(kreq->kvtrans_ctx->blk_alloc_ctx, kreq->io_tasks);
            DSS_ASSERT(ba_rc == BLK_ALLOCATOR_STATUS_SUCCESS);
        } else {
            iot_rc = dss_io_task_submit(kreq->io_tasks);
            DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
            //Will be queued in block allocator
        }
        rc = KVTRANS_IO_SUBMITTED;
        kreq->io_to_queue = false;
    } else {
        rc = KVTRANS_IO_QUEUED;
    }
#endif
    return rc;
#else
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;

    void *buff;
    dss_device_t *target_dev = kvtrans_ctx->target_dev;


    // TODO: integrate io_task
    switch (blk->value_location) 
    {
    case INLINE:
        dss_kvtrans_write_ondisk_blk(blk_ctx, kreq);
        break;
    case CONTIG:
        buff = _serialize_data((void *)blk_ctx->blk, BLOCK_SIZE, req->req_value.value, (int)req->req_value.length);
        dss_io_task_add_blk_write(kreq->io_tasks, target_dev, blk_ctx->index,
            1+blk_ctx->vctx.value_blocks, buff, BLOCK_SIZE+kreq->req->req_value.length, 0, false);
        break;
    case REMOTE:
        dss_io_task_add_blk_write(kreq->io_tasks, target_dev, blk_ctx->index, 1, (void *)blk, BLOCK_SIZE, 0, false);
        dss_io_task_add_blk_write(kreq->io_tasks, target_dev, blk_ctx->index,
            blk_ctx->vctx.value_blocks, req->req_value.value, kreq->req->req_value.length, 0, false);
        break;
    case HYBIRD:
        buff = _serialize_data((void *)blk_ctx->blk, BLOCK_SIZE, req->req_value.value, blk_ctx->blk->place_value[0].num_chunks * BLOCK_SIZE);
        dss_io_task_add_blk_write(kreq->io_tasks, target_dev, blk_ctx->index, 1+blk_ctx->blk->place_value[0].num_chunks, buff, (blk_ctx->blk->place_value[0].num_chunks+1)*BLOCK_SIZE, 0, false);
        offset = blk_ctx->blk->place_value[0].num_chunks * BLOCK_SIZE;
        for (int i=1; i<blk->num_valid_place_value_entry; i++) {
            dss_io_task_add_blk_write(kreq->io_tasks, target_dev, blk_ctx->blk->place_value[i].value_index, blk_ctx->blk->place_value[i].num_chunks, req->req_value.value, (blk_ctx->blk->place_value[i].num_chunks)*BLOCK_SIZE, offset, false);
            offset += blk_ctx->blk->place_value[i].num_chunks * BLOCK_SIZE;
        }
        break;
    default:
        break;
    }
#endif
}


static uint16_t _find_min_hash_size_for_device(int blk_num) {
    uint16_t hash_size = MIN_HASH_SIZE;
    if (blk_num < ((uint64_t)1<<hash_size)) {
        printf("Warning: blkment num is too small!");
        return hash_size;
    }
    while (blk_num > ((uint64_t)1<<hash_size)) {
        hash_size *= 2;
        blk_num >>= 1;
    }
    return hash_size;
}

/* transfer decimal number to binary and count the bits*/
uint32_t dec_2_bit(uint32_t dec_num) {
    uint32_t bits = 0;
    while (dec_num>0) {
        bits++;
        dec_num >>= 1;
    }
    return bits;
}

/* make sure index is less than the number of blocks */
static void kv_assign_block(uint64_t *index, kvtrans_ctx_t *ctx) {
    uint16_t bit_shift;
    uint64_t blk_offset;

    // calculate the useless bit number
    bit_shift = ctx->hash_fn_ctx->hash_size - ctx->hash_bit_in_use;

    // the index of first allocable blk
    blk_offset = ctx->blk_offset;
    
    // avoid allocating index less than blk_offset
    *index = ((*index >> bit_shift) % (ctx->kvtrans_params.logi_blk_num - blk_offset)) + blk_offset;
}

hash_fn_ctx_t *init_hash_fn_ctx(kvtrans_params_t *params) 
{
    hash_fn_ctx_t *hash_fn_ctx;

    hash_fn_ctx = (hash_fn_ctx_t *)malloc(sizeof(hash_fn_ctx_t));
    if (!hash_fn_ctx) {
        printf("ERROR: malloc hash_fn_ctx failed.\n");
        return NULL;
    }

    hash_fn_ctx->hashcode = 0;
    hash_fn_ctx->hash_buf = 0;
    hash_fn_ctx->tryout = 0;
    hash_fn_ctx->hash_size = params->hash_size;
    hash_fn_ctx->max_tryout = hash_fn_ctx->hash_size;
    hash_fn_ctx->initialized = 0;

    switch (params->hash_type) {
        case sha256_take_bit:
            hash_fn_ctx->hash_type = sha256_take_bit;
            hash_fn_ctx->init = SHA256_init;
            hash_fn_ctx->update = SHA256_update_take_bit;
            hash_fn_ctx->clean = SHA256_clean;
            break;
        case sha256_take_byte:
            hash_fn_ctx->hash_type = sha256_take_byte;
            hash_fn_ctx->init = SHA256_init;
            hash_fn_ctx->update = SHA256_update_take_byte;
            hash_fn_ctx->clean = SHA256_clean;
            break;
        case xxhash:
            hash_fn_ctx->hash_type = xxhash;
            hash_fn_ctx->init = XXHASH_init;
            hash_fn_ctx->update = XXHASH_update;
            hash_fn_ctx->clean = XXHASH_clean;
            break;
        case spooky:
            hash_fn_ctx->hash_type = spooky;
            hash_fn_ctx->init = SPOOKY_init;
            hash_fn_ctx->update = SPOOKY_update;
            hash_fn_ctx->clean = SPOOKY_clean;
            break;
        default:
            printf("ERROR: unknown hash function");
    }
    hash_fn_ctx->init(hash_fn_ctx);
    return hash_fn_ctx;
}

void free_hash_fn_ctx(hash_fn_ctx_t *hash_fn_ctx) {
    if (!hash_fn_ctx)
        return;
    if (hash_fn_ctx->hash_type==sha256_take_bit ||
        hash_fn_ctx->hash_type==sha256_take_byte) {
            free(hash_fn_ctx->buf);
        }
    free(hash_fn_ctx);
}

void init_dc_table(kvtrans_ctx_t *ctx) {
    char name[32] = {"DATA_COL_TBL"};
    ctx->dc_cache_tbl = init_cache_tbl(name, MAX_DC_NUM, sizeof(dc_item_t), 1);
    if (!ctx->dc_cache_tbl ) {
        printf("ERROR: malloc data collision table failed. \n");
        return;
    }
}

void free_dc_table(kvtrans_ctx_t  *ctx) {
    if(!ctx || !ctx->dc_cache_tbl) 
        return;
    free_cache_tbl(ctx->dc_cache_tbl);
}

kvtrans_params_t set_default_params() {
    kvtrans_params_t params = {};
    params.hash_size = 32;
    params.hash_type = spooky;
    params.id = 0;
    params.name = "dss_kvtrans";
    params.thread_num = 1;
    params.meta_blk_num = 1024;
    params.logi_blk_num = BLK_NUM;
    params.blk_alloc_name = "simbmap_allocator";
    params.blk_offset = 1;
    params.logi_blk_size = BLOCK_SIZE;
    params.state_num = DEFAULT_BLOCK_STATE_NUM;
    return params;    
}

void dss_kvtrans_blk_ctx_ctor(void *ctx, void *item)
{
    kvtrans_ctx_t *kvt_ctx = (kvtrans_ctx_t *)ctx;
    blk_ctx_t *blk_ctx = (blk_ctx_t *)item;

    DSS_ASSERT(ctx != NULL);

    //TODO: BLOCK_SIZE and BLK_ALIGN should be replaced from kvtrans ctx
#ifndef DSS_BUILD_CUNIT_TEST
    blk_ctx->blk = (ondisk_meta_t *)spdk_dma_zmalloc(kvt_ctx->blk_size, BLK_ALIGN, NULL);
#else
    blk_ctx->blk = (ondisk_meta_t *) calloc(1, kvt_ctx->blk_size);
#endif

    return;
}

void dss_kvtrans_blk_ctx_dtor(void *ctx, void *item)
{
    kvtrans_ctx_t *kvt_ctx = (kvtrans_ctx_t *)ctx;
    blk_ctx_t *blk_ctx = (blk_ctx_t *)item;

    DSS_ASSERT(ctx != NULL);

    DSS_ASSERT(blk_ctx->blk != NULL);
#ifndef DSS_BUILD_CUNIT_TEST
        spdk_free(blk_ctx->blk);
#else
        free(blk_ctx->blk);
#endif
    blk_ctx->blk = NULL;
    return;
}

dss_kvtrans_status_t
dss_kvtrans_init_meta_sync_ctx(kvtrans_ctx_t *kvtrans_ctx) {
    kvtrans_ctx->meta_sync_ctx = (kvtrans_meta_sync_ctx_t *) calloc(1, sizeof(kvtrans_meta_sync_ctx_t));
    if (!kvtrans_ctx->meta_sync_ctx) {
        return KVTRANS_MALLOC_ERROR;
    }
    kvtrans_ctx->meta_sync_ctx->queue_length = 0;
    // Assign the parent back as reference for trackability
    kvtrans_ctx->meta_sync_ctx->kvtrans_ctx = kvtrans_ctx;
    STAILQ_INIT(&kvtrans_ctx->meta_sync_ctx->kv_req_queue);
    return KVTRANS_STATUS_SUCCESS;
}

void
dss_kvtrans_free_meta_sync_ctx(kvtrans_meta_sync_ctx_t *meta_sync_ctx) {
    if (meta_sync_ctx) {
        Judy1FreeArray(&meta_sync_ctx->dirty_meta_lba, PJE0);
        free(meta_sync_ctx);
    }
}

kvtrans_ctx_t *init_kvtrans_ctx(kvtrans_params_t *params) 
{
    kvtrans_ctx_t *ctx;
    dss_blk_allocator_opts_t config;
    
    ctx = (kvtrans_ctx_t *) calloc(1, sizeof(kvtrans_ctx_t));
    if (!ctx) {
        DSS_ERRLOG("calloc for kvtrans_ctx failed\n");
       goto failure_handle;
    }

    if (params!=NULL) {
        DSS_ASSERT(params->dev);
        DSS_ASSERT(params->iotm);
        ctx->kvtrans_params = *params;
        ctx->target_dev = params->dev;
        ctx->kvt_iotm = params->iotm;
        ctx->blk_size = params->logi_blk_size;
        ctx->blk_offset = params->blk_offset;
        ctx->state_num = params->state_num;
        DSS_ASSERT(ctx->state_num>0);
        ctx->blk_num = params->logi_blk_num;
    } else {
        DSS_NOTICELOG("Using default parameters for initialization\n");
        ctx->kvtrans_params = set_default_params();
    }

    ctx->is_ba_meta_sync_enabled = g_ba_enable_meta_sync;
    ctx->dump_mem_meta = g_dump_mem_meta;


    dss_blk_allocator_set_default_config(ctx->kvtrans_params.dev, &config);
    //dss_blk_allocator_set_default_config(NULL, &config);

    if (*ctx->kvtrans_params.blk_alloc_name=='\0') {
        ctx->kvtrans_params.blk_alloc_name = DEFAULT_BLK_ALLOC_NAME;
    }
    //TODO: Check for valid block alloc name and set
    config.blk_allocator_type = ctx->kvtrans_params.blk_alloc_name;
    config.num_total_blocks = ctx->kvtrans_params.logi_blk_num;
    config.block_alloc_meta_start_offset =
        ctx->kvtrans_params.blk_alloc_meta_start_offset;
    config.logical_start_block_offset = ctx->kvtrans_params.blk_offset;
    config.allocator_block_size = ctx->blk_size;
    // exclude empty state
    config.num_block_states = ctx->state_num - 1;

    config.enable_ba_meta_sync = ctx->is_ba_meta_sync_enabled;

    ctx->blk_alloc_ctx = dss_blk_allocator_init(ctx->kvtrans_params.dev, &config);
    //ctx->blk_alloc_ctx = dss_blk_allocator_init(NULL, &config);
    if (!ctx->blk_alloc_ctx) {
        DSS_ERRLOG("blk_allocator init failed\n");
         goto failure_handle;
    }

    // calcualte block offset if it's not specified in params
    if (ctx->is_ba_meta_sync_enabled && ctx->blk_offset<=1) {
        ctx->blk_offset = dss_blk_allocator_get_physical_size(&config) / ctx->blk_size + 1;
        DSS_NOTICELOG("Updating block offset to %d\n", ctx->blk_offset);
    }

    if (ctx->kvtrans_params.hash_size==0) {
        ctx->kvtrans_params.hash_size = _find_min_hash_size_for_device(config.num_total_blocks);
    }

    ctx->task_num = 0;
    ctx->hash_fn_ctx = init_hash_fn_ctx(&ctx->kvtrans_params);
    if (!ctx->hash_fn_ctx) {
        printf("ERROR: hash_fn_ctx init failed\n");
         goto failure_handle;
    }

    ctx->hash_bit_in_use = dec_2_bit(config.num_total_blocks);
    ctx->kv_assign_block = &kv_assign_block;
    
    dss_mallocator_opts_t blk_ma_opts;
    blk_ma_opts.item_sz = sizeof(blk_ctx_t);
    blk_ma_opts.max_per_cache_items = DEFAULT_BLK_CTX_CACHE; 
    blk_ma_opts.num_caches = 1;

    DSS_ASSERT(ctx != NULL);

    ctx->blk_ctx_mallocator = dss_mallocator_init_with_cb(DSS_MEM_ALLOC_MALLOC, blk_ma_opts, dss_kvtrans_blk_ctx_ctor, dss_kvtrans_blk_ctx_dtor, ctx);
    DSS_ASSERT(ctx->blk_ctx_mallocator);

    init_dc_table(ctx);
    if (!ctx->dc_cache_tbl) {
        DSS_ERRLOG("ERROR: dc_table init failed\n");
        goto failure_handle;
    }

    if(dss_kvtrans_init_meta_sync_ctx(ctx)!=KVTRANS_STATUS_SUCCESS) {
        DSS_ERRLOG("Create meta sync ctx failed\n");
        goto failure_handle;
    }

#ifdef MEM_BACKEND
    init_mem_backend(ctx, ctx->kvtrans_params.meta_blk_num, ctx->kvtrans_params.logi_blk_num);
    if ((!g_disk_as_meta_store && !ctx->meta_ctx) || (!g_disk_as_data_store && !ctx->data_ctx)) {
        printf("ERROR: mem_backend init failed\n");
        goto failure_handle;
    }
    DSS_DEBUGLOG(DSS_KVTRANS, "Data backend [%p] and Meta backend [%p] created for kvtrans [%p]\n", ctx->data_ctx, ctx->meta_ctx, ctx);
#endif
#ifdef DSS_BUILD_CUNIT_TEST
    STAILQ_INIT(&ctx->req_head);
#endif

    return ctx;

failure_handle:
    free_kvtrans_ctx(ctx);
    return NULL;
}

void free_kvtrans_ctx(kvtrans_ctx_t *ctx) 
{
    DSS_ASSERT(ctx != NULL);
    if (!ctx) return;

    if (ctx->blk_alloc_ctx) {
        // NB 1: There is KV-Trans module created per drive; though
        //       having such an option while not partitioning the drive
        //       into sub-drives might not have any effect; this feature
        //       is left as is for future extensions.
        // NB 2: This check makes sure if the KVT instance is valid before
        //       free
        // NB 3: There are as many kv submodules as there are sub-sytems
        // NB 4: There are as many threads per sub module, as many as 
        //       namespaces there are by default
        if (ctx->blk_alloc_ctx->blk_alloc_opts.blk_allocator_type != NULL) {
            dss_blk_allocator_destroy(ctx->blk_alloc_ctx);
        }
    }

    //kvtrans_ctx might be used since it is registered in cb_arg
    //So this is called first
    if (ctx->blk_ctx_mallocator)  dss_mallocator_destroy(ctx->blk_ctx_mallocator);

    if (ctx->hash_fn_ctx) free_hash_fn_ctx(ctx->hash_fn_ctx);
    free_dc_table(ctx);

    if (ctx->meta_sync_ctx) dss_kvtrans_free_meta_sync_ctx(ctx->meta_sync_ctx);

#ifdef MEM_BACKEND
    free_mem_backend(ctx);
#endif
    free(ctx);
}


kvtrans_req_t *init_kvtrans_req(kvtrans_ctx_t *kvtrans_ctx, req_t *req, kvtrans_req_t *preallocated_req)
{
    dss_kvtrans_status_t rc;
    kvtrans_req_t *kreq;
    blk_ctx_t *blk_ctx = NULL;

    if(preallocated_req) {
        kreq = preallocated_req;
        kreq->req_allocated = false;
    } else {
        kreq = (kvtrans_req_t *) calloc(1, sizeof(kvtrans_req_t));
        if (!kreq) {
            DSS_ERRLOG("malloc kreq failed.\n");
            goto failure_handle;
        }
        kreq->req_allocated = true;
        TAILQ_INIT(&kreq->meta_chain);
    }

    if (TAILQ_EMPTY(&kreq->meta_chain)) {
        DSS_DEBUGLOG(DSS_KVTRANS, "allocating blk_ctx for kreq %p\n", kreq);
        rc = dss_kvtrans_get_free_blk_ctx(kvtrans_ctx, &blk_ctx);
        if (rc) {
            DSS_ERRLOG("blk_ctx allocator returns false.\n");
            goto failure_handle;
        }
        
        if (!kreq->meta_chain.tqh_last) {
            // TAILQ not initialized
            TAILQ_INIT(&kreq->meta_chain);
        }

        TAILQ_INSERT_TAIL(&kreq->meta_chain, blk_ctx, blk_link);
        kreq->num_meta_blk = 1;
        blk_ctx->kreq = kreq;
    }
    
    
    if (!req) {
        DSS_ERRLOG("req is null.\n");
        goto failure_handle;
    }

    kreq->req.req_key = req->req_key;
    // In case we need this later
    // if (kreq->req.req_key.length < KEY_LEN) {
    //     // memset junk bytes to 0
    //     memset(&kreq->req.req_key.key[kreq->req.req_key.length], 0, KEY_LEN - kreq->req.req_key.length);
    // }
    kreq->req.req_value = req->req_value;
    kreq->req.opc = req->opc;
    kreq->req.req_ts = req->req_ts;
    kreq->id = kvtrans_ctx->task_num++;
    kreq->kvtrans_ctx = kvtrans_ctx;
#ifdef DSS_BUILD_CUNIT_TEST
    STAILQ_INSERT_TAIL(&kvtrans_ctx->req_head, kreq, req_link);
#endif
    kreq->ba_meta_updated = false;
    kreq->io_to_queue = false;
    kreq->state = REQ_INITIALIZED;
    kreq->initialized = true;

    return kreq;

failure_handle:
    free_kvtrans_req(kreq);
    DSS_ASSERT(0);//TODO: This case is not handled
    return NULL;
}

void free_kvtrans_req(kvtrans_req_t *kreq)
{
    dss_io_task_status_t iot_rc;
    dss_kvtrans_status_t rc;

    struct blk_ctx *b1;
    struct blk_ctx *b2;

    DSS_ASSERT(kreq != NULL);
    if (!kreq) {
        return;
    }

    if(kreq->io_tasks && kreq->initialized) {
        iot_rc = dss_io_task_put(kreq->io_tasks);
        DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
        kreq->io_tasks = NULL;
        //TODO: Error handling
    }

    b1 = TAILQ_FIRST(&kreq->meta_chain);
    DSS_ASSERT(b1 != NULL);//Atleast one blk ctx in kreq
    //Skip freeing first blk_ctx for preallocated requests
    b1 = TAILQ_NEXT(b1, blk_link);
    while(b1) {
        b2 = TAILQ_NEXT(b1, blk_link);
        TAILQ_REMOVE(&kreq->meta_chain, b1, blk_link);
        rc = dss_kvtrans_put_free_blk_ctx(kreq->kvtrans_ctx, b1);
        if (rc) DSS_ERRLOG("free blk_ctx failed\n");
        b1 = b2;
    }

    DSS_ASSERT(TAILQ_FIRST(&kreq->meta_chain) != NULL);

    if (kreq->req_allocated) {
        DSS_DEBUGLOG(DSS_KVTRANS, "Kreq %p freed\n", kreq);
        b1 = TAILQ_FIRST(&kreq->meta_chain);
        TAILQ_REMOVE(&kreq->meta_chain, b1, blk_link);
        rc = dss_kvtrans_put_free_blk_ctx(kreq->kvtrans_ctx, b1);
        if (rc) DSS_DEBUGLOG(DSS_KVTRANS, "free blk_ctx failed\n");
        free(kreq);
        kreq = NULL;
    }

    if(kreq) {
        b1 = TAILQ_FIRST(&kreq->meta_chain);
        DSS_ASSERT(TAILQ_NEXT(b1, blk_link) == NULL);
        // keep b1's link to kreq
        
        kreq->ba_meta_updated = false;
        kreq->dreq = NULL;
        kreq->id = -1;
        kreq->io_to_queue = false;
        kreq->kvtrans_ctx = NULL;
        kreq->initialized = false;
        kreq->num_meta_blk = 1;
        TAILQ_INIT(&kreq->meta_chain);
        // we keep one blk_ctx
        reset_blk_ctx(b1);
        TAILQ_INSERT_TAIL(&kreq->meta_chain, b1, blk_link);
        b1->kreq = kreq;
    }

    return;
}

int dss_kvtrans_handle_request(kvtrans_ctx_t *ctx, req_t *req) {
    DSS_ASSERT(req);
    kvtrans_req_t *kreq;
    kreq = init_kvtrans_req(ctx, req, NULL);
    DSS_ASSERT(kreq);
    kreq->io_tasks = NULL;
    return 0;
}

bool iscb_valid(void *cb) {
    DSS_ASSERT(cb);
    if (memcmp(cb, (char *)cb+1, sizeof(blk_ctx_t)-1)==0 && (char *)cb == 0) {
    // all bits are zero
        return true;
    }
    return false;
}

bool iskeynull(char *key) {
    DSS_ASSERT(key);
    if (*key=='\0') {
        // all bits are zero
        return true;
    }
    return false;
}

bool iskeysame(char *k1, key_size_t k1_len, char *k2, key_size_t k2_len) {
    if (!k1 || !k2) {
        printf("ERROR: key is null\n");
        return false;
    }
    if (k1_len!=k2_len) return false;
    return !memcmp(k1, k2, k2_len);
}

bool is_entry_match(col_entry_t *col_entry, char *key, key_size_t key_len) {
    char *entry_key = col_entry->key;
    // TODO: get entry_key length in an efficient way
    return iskeysame(entry_key, strnlen(entry_key, KEY_LEN), key, strnlen(key, KEY_LEN));
}

dss_kvtrans_status_t 
_update_kreq_stat_after_io(kvtrans_req_t *kreq,
                    dss_kvtrans_status_t rc,
                    enum kvtrans_req_e success_stat,
                    enum kvtrans_req_e queue_stat) {
    if (rc==KVTRANS_STATUS_SUCCESS) {
        kreq->state = success_stat;
    } else if (rc == KVTRANS_IO_SUBMITTED ) {
        kreq->state = queue_stat;
        rc = KVTRANS_STATUS_SUCCESS;
    }
    return rc;
}

bool _lba_in_range(kvtrans_ctx_t *ctx, uint64_t blk_idx) {
    return blk_idx >= ctx->blk_offset && blk_idx < ctx->blk_offset + ctx->blk_num;
}

dss_kvtrans_status_t
_alloc_entry_block(kvtrans_ctx_t *ctx, 
                    kvtrans_req_t *kreq,
                    blk_ctx_t *blk_ctx)
{
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;

    hash_fn_ctx_t *hash_fn_ctx = ctx->hash_fn_ctx;
    dss_blk_allocator_context_t *blk_alloc_ctx = ctx->blk_alloc_ctx;
    req_t *req = &kreq->req;

    // TODO: it's possbile key is all zero.
    DSS_ASSERT(!iskeynull(req->req_key.key));
    hash_fn_ctx->clean(hash_fn_ctx);
    hash_fn_ctx->update(req->req_key.key, req->req_key.length, hash_fn_ctx);
    blk_ctx->index = hash_fn_ctx->hashcode;
    // ensure index is within [1, blk_alloc_opts.num_total_blocks - 1]
    ctx->kv_assign_block(&blk_ctx->index, ctx);

    DSS_ASSERT(_lba_in_range(ctx, blk_ctx->index));
    rc = dss_kvtrans_get_blk_state(ctx, blk_ctx);

    DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: Allocate block at index [%u] with state [%s] for key [%s].\n", ctx, blk_ctx->index, stateNames[blk_ctx->state], req->req_key.key);

    switch (blk_ctx->state) {
    case EMPTY:
        // no need to load blk
        // reset blk_ctx for a new kvtrans request
        blk_ctx->kctx.dc_index = 0;
        blk_ctx->kctx.pindex = 0;
        blk_ctx->nothash = false;
        kreq->state = ENTRY_LOADING_DONE;
        break;
    case COLLISION_EXTENSION:
    case DATA:
        // find an EMPTY blk TODO: rehash or not?
        blk_ctx->kctx.dc_index = blk_ctx->index;
        kreq->state = ENTRY_LOADING_DONE;
        break;
    case DATA_COLLISION_CE:
    case DATA_COLLISION_EMPTY:
    case DATA_COLLISION:
        // find MDC at index
        blk_ctx->kctx.dc_index = blk_ctx->index;
        rc = dss_kvtrans_dc_table_lookup(ctx, blk_ctx->kctx.dc_index, &blk_ctx->index);
        DSS_ASSERT(rc==KVTRANS_STATUS_SUCCESS);
        DSS_ASSERT(_lba_in_range(ctx, blk_ctx->index));
        blk_ctx->state = META_DATA_COLLISION_ENTRY;
        // continue to load ondisk blk
    case META:
    case COLLISION:
    case META_DATA_COLLISION:
    case META_DATA_COLLISION_ENTRY:
        rc = dss_kvtrans_load_ondisk_blk(blk_ctx, kreq, true);
        rc = _update_kreq_stat_after_io(kreq, rc, ENTRY_LOADING_DONE, QUEUE_TO_LOAD_ENTRY);
        break;
    default:
        DSS_ASSERT(0);
        break;
    }
    return rc;
}

dss_kvtrans_status_t _col_tbl_remove_entry(ondisk_meta_t *blk, int entry_idx) {
    DSS_ASSERT(entry_idx<=MAX_COL_TBL_SIZE-1 && blk);
    
    if (blk->collision_tbl[entry_idx].state == DATA_COL_ENTRY) {
        blk->num_valid_dc_col_entry --;
    }

    blk->collision_tbl[entry_idx].state = DELETED;
    // memset(&blk->collision_tbl[entry_idx], 0, sizeof(col_entry_t));

    blk->num_valid_col_entry--;
    
    return KVTRANS_STATUS_SUCCESS;
}

bool _col_tbl_entry_isvalid(col_entry_t *entry){
    return (entry->state!=DELETED && entry->state!=INVALID);
}

int _col_tbl_get_first_entry(ondisk_meta_t *blk, bool isvalid) {
    int i = -1;
    for(i=0; i<MAX_COL_TBL_SIZE; i++) {
        if (_col_tbl_entry_isvalid(&blk->collision_tbl[i]) == isvalid) {
            return i;
        }
    }
    return -1;
}

blk_ctx_t *_get_next_blk_ctx(kvtrans_ctx_t *ctx, blk_ctx_t *blk_ctx) {
    dss_kvtrans_status_t rc;
    blk_ctx_t *col_blk_ctx = TAILQ_NEXT(blk_ctx, blk_link);
    if (col_blk_ctx == NULL) {
        DSS_DEBUGLOG(DSS_KVTRANS, "allocating blk_ctx for kreq [%p]\n", blk_ctx->kreq);
        rc = dss_kvtrans_get_free_blk_ctx(ctx, &col_blk_ctx);
        DSS_ASSERT(blk_ctx->kreq);
        col_blk_ctx->kreq = blk_ctx->kreq;
        blk_ctx->kreq->num_meta_blk++;
        if (rc) return NULL;
        TAILQ_INSERT_TAIL(&blk_ctx->kreq->meta_chain, col_blk_ctx, blk_link);
    }
    return col_blk_ctx;
}

uint64_t _get_num_blocks_required_for_value(req_t *req, uint64_t block_size) {
    if (req->req_value.length < MAX_INLINE_VALUE) {
        return 0;
    }
    return CEILING(req->req_value.length, block_size);
}

/* find a contiguous or scatter of DATA blocks */
dss_kvtrans_status_t
find_data_blocks(blk_ctx_t *blk_ctx, kvtrans_ctx_t *ctx, uint64_t num_blocks) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    if (num_blocks==0) {
        return rc;
    } 
    uint64_t allocated_start_block = 0;

    rc = dss_kvtrans_alloc_contig(ctx, blk_ctx->kreq, DATA, 
        blk_ctx->index+1, num_blocks, &allocated_start_block);
    if(rc == KVTRANS_STATUS_SUCCESS) {
        DSS_ASSERT(_lba_in_range(ctx, allocated_start_block));
        blk_ctx->blk->place_value[blk_ctx->blk->num_valid_place_value_entry].num_chunks = num_blocks;
        blk_ctx->blk->place_value[blk_ctx->blk->num_valid_place_value_entry].value_index = allocated_start_block;

        if (allocated_start_block!=blk_ctx->index+1) {
            blk_ctx->vctx.remote_val_blocks++;
        }
        blk_ctx->blk->num_valid_place_value_entry++;
        //TODO: handle this case
        DSS_ASSERT(blk_ctx->blk->num_valid_place_value_entry!=MAX_DATA_COL_TBL_SIZE);
        return rc;
    }

    uint64_t lb = num_blocks/2;
    uint64_t rb = num_blocks - lb;

    if (lb+rb==1 && num_blocks==1) {
        printf("ERROR: failed to find 1 block at index %zu\n", blk_ctx->index+1);
        return KVTRANS_STATUS_ERROR;
    }

    if(find_data_blocks(blk_ctx, ctx, lb)==KVTRANS_STATUS_SUCCESS &&
       find_data_blocks(blk_ctx, ctx, rb)==KVTRANS_STATUS_SUCCESS) {
        return KVTRANS_STATUS_SUCCESS;
    }
    return KVTRANS_STATUS_ERROR;
}

// intialize value blocks for META blkment in ctx->blk
dss_kvtrans_status_t _blk_init_value(void *ctx) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    ondisk_meta_t *blk = blk_ctx->blk; 
    kvtrans_req_t *kreq = blk_ctx->kreq;
    req_t *req = &kreq->req;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    dss_blk_allocator_context_t *blk_alloc = kvtrans_ctx->blk_alloc_ctx;

    blk->value_size = req->req_value.length;
    if (blk->value_size<MAX_INLINE_VALUE) {
        blk->value_location = INLINE;
        memset(blk->value_buffer, 0, MAX_INLINE_VALUE);
        memcpy(blk->value_buffer, req->req_value.value, req->req_value.length);
    } else {
        memset(&blk->place_value, 0, sizeof(value_loc_t) * blk->num_valid_place_value_entry);
        if (blk_ctx->vctx.iscontig) {
            // data has been allocated with meta
            blk_ctx->vctx.remote_val_blocks = 0;
            blk->value_location = CONTIG;
            blk->num_valid_place_value_entry = 1;
            blk->place_value[0].num_chunks = blk_ctx->vctx.value_blocks;
            blk->place_value[0].value_index = blk_ctx->index+1;

            return KVTRANS_STATUS_SUCCESS;
        }
        blk->num_valid_place_value_entry = 0;
        blk_ctx->vctx.remote_val_blocks = 0;
        rc = find_data_blocks(blk_ctx, kvtrans_ctx, blk_ctx->vctx.value_blocks);
        if (rc!=KVTRANS_STATUS_SUCCESS) {
            return KVTRANS_STATUS_ALLOC_CONTIG_ERROR;
        }
        DSS_ASSERT(blk->num_valid_place_value_entry>0);
        if (blk_ctx->vctx.remote_val_blocks==0) {
            blk->value_location = CONTIG;
        } else if (blk_ctx->vctx.remote_val_blocks == blk_ctx->blk->num_valid_place_value_entry) {
            kvtrans_ctx->stat.data_scatter++;
            blk->value_location = REMOTE;
         } else {
            kvtrans_ctx->stat.data_scatter++;
            blk->value_location = HYBIRD;
        }
    }
    return rc;
}


dss_kvtrans_status_t _blk_load_value(void *ctx) {
    dss_kvtrans_status_t rc;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    ondisk_meta_t *blk = blk_ctx->blk; 
    kvtrans_req_t *kreq = blk_ctx->kreq;
    req_t *req = &kreq->req;

    // 0 = Along with Meta, 1 = next adjacent blkment, 
    // 2 = remote, 3 = some adjacent and  some remote
    if (blk->value_location == INLINE) {
        // blk is in memory
        memcpy(req->req_value.value, blk->value_buffer, blk->value_size); 
        kreq->state = REQ_CMPL;
        rc = KVTRANS_STATUS_SUCCESS;
    } else {
        rc = dss_kvtrans_load_ondisk_data(blk_ctx, kreq, true);
        DSS_RELEASE_ASSERT(rc == KVTRANS_STATUS_SUCCESS);
    }
    req->req_value.length = blk->value_size; 

    return rc;
}


dss_kvtrans_status_t _blk_del_value(void *ctx) {
    dss_kvtrans_status_t rc;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    ondisk_meta_t *blk = blk_ctx->blk; 
    kvtrans_req_t *kreq = blk_ctx->kreq;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    uint64_t index, blk_num;
    uint64_t s_idx, e_idx;

    rc = KVTRANS_STATUS_SUCCESS;
    // 0 = Along with Meta, 1 = next adjacent blkment, 
    // 2 = remote, 3 = some adjacent and  some remote
    if (blk->value_location != INLINE) {
        int i;
        for (i = 0; i < blk->num_valid_place_value_entry; i++) {
            index = blk->place_value[i].value_index;
            blk_num = blk->place_value[i].num_chunks;
            s_idx = e_idx = index;
            while (e_idx < index + blk_num) {
                if(dss_kvtrans_dc_table_exist(kvtrans_ctx, e_idx)==KVTRANS_STATUS_SUCCESS) {
                    // e_idx is a DATA COLLISION blk
                    if (s_idx!=e_idx) {
                        rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, s_idx, e_idx-s_idx, EMPTY);
                        if(rc) return rc;
                    }
                    // the ori_state of e_idx is EMPTY
                    rc = dss_kvtrans_dc_table_update(kvtrans_ctx, blk_ctx, e_idx, EMPTY);
                    // TODO: error handling
                    s_idx = e_idx + 1;
                } else if (e_idx == s_idx && e_idx == (index+blk_num-1)) {
                    // if the last blk is not a data collision
                    rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, e_idx, 1, EMPTY);
                    if(rc) return rc;
                    break;
                }
                e_idx++;
            }
            if (s_idx!=e_idx) {
                // handle cases if the rest of blks are not data collision blks
                rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, s_idx, e_idx-s_idx, EMPTY);
                if (rc) return rc;
            }
        }
        memset(&blk->place_value, 0, sizeof(value_loc_t)*blk->num_valid_place_value_entry);
    } else {
        memset(&blk->value_buffer, 0, MAX_INLINE_VALUE);
    }
    blk->value_size = 0;
    blk->value_location = 0;
    blk->num_valid_place_value_entry = 0;

    return rc;
}

dss_kvtrans_status_t _blk_update_value(void *ctx) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    kvtrans_req_t *kreq = blk_ctx->kreq;
    req_t *req = &kreq->req;

    // if (blk->value_size >= req->req_value.length) {
    //     // just overwrite values by change value_size
    //     blk->value_size = req->req_value.length;
    // } else {
    // TODO: consider transaction cost
    rc = _blk_del_value(ctx);
    if (rc) {
        DSS_ERRLOG("update blk [%zu] failed in value deletion\n", blk_ctx->index);
        return rc;
    }

    blk_ctx->vctx.value_blocks = _get_num_blocks_required_for_value(req, kreq->kvtrans_ctx->blk_size);
    blk_ctx->vctx.iscontig = false;
    blk_ctx->vctx.remote_val_blocks = 0;

    rc = _blk_init_value(ctx);
    if (rc) {
        // TODO: error handling
        DSS_ERRLOG("update blk [%zu] failed in value init\n", blk_ctx->index);
    }
    return rc;
}

// initialize a meta or meta_data_collision from an empty block
static dss_kvtrans_status_t init_meta_blk(void *ctx)
{
    dss_kvtrans_status_t rc;
    blk_state_t state;
    int i;
    uint64_t allocated_start_lba;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    ondisk_meta_t *blk = blk_ctx->blk; 
    kvtrans_req_t *kreq = blk_ctx->kreq;
    req_t *req = &kreq->req;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;

    memset(blk, 0, sizeof(ondisk_meta_t));
    memcpy(blk->key, req->req_key.key, req->req_key.length);
    blk->key_len = req->req_key.length;
    // ctx->entry_blk->checksum = {''};
    blk->creation_time = req->req_ts;

    if (blk_ctx->kctx.dc_index > 0) {
        blk->data_collision_index = blk_ctx->kctx.dc_index;
        state = META_DATA_COLLISION;
    } else {
        state = META;
    }
    
    DSS_DEBUGLOG(DSS_KVTRANS, "Block [%u] state changes from [%s] to [%s] for key [%s].\n", blk_ctx->index, stateNames[blk_ctx->state], stateNames[state], req->req_key.key);

    if (!blk_ctx->nothash) {
        // META blk is located by hashing. We need to alloc value blocks seperately.
        blk_ctx->vctx.value_blocks = _get_num_blocks_required_for_value(req, kvtrans_ctx->blk_size);
        // TODO: change allocated_start_lba to NULL
        rc = dss_kvtrans_alloc_contig(kvtrans_ctx, kreq, state, 
                blk_ctx->index, 1, &allocated_start_lba);
        if (rc) return rc;
        DSS_ASSERT(allocated_start_lba==blk_ctx->index);
    }
    
    blk_ctx->blk->isvalid = true;

    rc = _blk_init_value(ctx);
    if (rc) {
        goto roll_back;
    }

    rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
    if (rc == KVTRANS_STATUS_IO_ERROR) goto roll_back;
    DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);

    rc = dss_kvtrans_write_ondisk_data(blk_ctx, kreq, false);
    if (rc == KVTRANS_STATUS_IO_ERROR) {
        goto roll_back;
    }
    DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
    rc = KVTRANS_STATUS_SUCCESS;

    // rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx->index, 1, state);
    // if (rc) return rc;
 
    if (state==META) kvtrans_ctx->stat.meta++; 
    else kvtrans_ctx->stat.mdc++;

    return KVTRANS_STATUS_SUCCESS;

roll_back:
    printf("ROLL_BACK: error happens at index %zu, rollback meta and data states\n", blk_ctx->index);
    // rollback value state
    if (blk_ctx->blk->num_valid_place_value_entry>0) {
        for(i=0; i<blk_ctx->blk->num_valid_place_value_entry; i++) {
            if(dss_kvtrans_set_blk_state(kvtrans_ctx, NULL, blk->place_value[i].value_index,
                    blk->place_value[i].num_chunks, EMPTY))
                return KVTRANS_ROLL_BACK_ERROR;
        }
    }

    // rollback meta state
    if(dss_kvtrans_set_blk_state(kvtrans_ctx, NULL, blk_ctx->index, 1, EMPTY))
        return KVTRANS_ROLL_BACK_ERROR;

    //TODO: Call block allocator api to clear dirty states
    return rc;
}

// lookup an EMPTY block and init the block as META/META_DATA_COLLISION
static dss_kvtrans_status_t open_free_blk(void *ctx, uint64_t *col_index) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    blk_ctx_t *blk_ctx, *col_blk;
    kvtrans_ctx_t *kvtrans_ctx; 
    kvtrans_req_t *kreq;
    req_t *req;

    blk_ctx = (blk_ctx_t *)ctx;
    kreq = blk_ctx->kreq;
    req = &kreq->req;
    kvtrans_ctx = kreq->kvtrans_ctx;
    col_blk = _get_next_blk_ctx(kvtrans_ctx, blk_ctx);
    
    DSS_ASSERT(col_blk);
    DSS_ASSERT (blk_ctx == TAILQ_PREV(col_blk, blk_elm, blk_link));

    memset(&col_blk->vctx, 0, sizeof(blk_val_ctx_t));
    col_blk->vctx.value_blocks = _get_num_blocks_required_for_value(req, kvtrans_ctx->blk_size);
    rc = dss_kvtrans_alloc_contig(kvtrans_ctx, kreq, DATA, blk_ctx->index + 1,
        col_blk->vctx.value_blocks + 1, &col_blk->index);

    if(rc) {
        // allocate any EMPTY block for META
        rc = dss_kvtrans_alloc_contig(kvtrans_ctx, kreq, META, blk_ctx->index + 1,
                1, &col_blk->index);
        col_blk->vctx.iscontig = false;
        if (rc) {
            DSS_ERRLOG("Error: Out of spaces. Cannot find a single free blk.\n");
        }
    } else {
        // get contig blks
        col_blk->vctx.iscontig = true;
        if(dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, col_blk->index, 1, META)) {
            // falied
            if(dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, col_blk->index, col_blk->vctx.value_blocks + 1, EMPTY)) {
                return KVTRANS_ROLL_BACK_ERROR;
            }
        }
    }
    
    col_blk->state = EMPTY;
    col_blk->kctx.flag = blk_ctx->kctx.flag;
    col_blk->kctx.pindex = blk_ctx->index;
    col_blk->kctx.dc_index = blk_ctx->kctx.dc_index;
    col_blk->kreq = blk_ctx->kreq;
    col_blk->nothash = true;
    col_blk->first_insert_blk_ctx = blk_ctx->first_insert_blk_ctx;

    rc = init_meta_blk((void *)col_blk);

    *col_index = col_blk->index;

    return rc;
}

static dss_kvtrans_status_t init_meta_data_collision_blk(void *ctx) {
    dss_kvtrans_status_t rc;
    uint64_t col_index;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    kvtrans_req_t *kreq = blk_ctx->kreq;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;

    DSS_ASSERT(blk_ctx->state==DATA || blk_ctx->state==COLLISION_EXTENSION);
    DSS_ASSERT(blk_ctx->kctx.dc_index > 0);

    // blk state will be marked as META_DATA_COLLISION
    rc = open_free_blk(ctx, &col_index);
    if (rc) return rc;

    // blk state will be marked as META_DATA_COLLISION_ENTRY
    rc = dss_kvtrans_dc_table_insert(kvtrans_ctx, blk_ctx, blk_ctx->kctx.dc_index, col_index, blk_ctx->state);

    kvtrans_ctx->stat.dc++;
    
    return rc;   
}

static dss_kvtrans_status_t update_meta_blk(void *ctx) {
    dss_kvtrans_status_t rc;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    kvtrans_req_t *kreq = blk_ctx->kreq;
    ondisk_meta_t *blk = blk_ctx->blk; 
    req_t *req = &kreq->req;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    uint64_t col_index;
    blk_state_t state;

    DSS_ASSERT(blk->isvalid && blk->num_valid_col_entry==0);

    if (!iskeysame(blk->key, blk->key_len, req->req_key.key, req->req_key.length)) {
        // META key is different to req_key
        switch (blk_ctx->kctx.flag) {
        case new_write:
            rc = open_free_blk(ctx, &col_index);
            if (rc) return rc;
            memcpy(blk->collision_tbl[0].key, req->req_key.key, req->req_key.length);
            blk->collision_tbl[0].meta_collision_index = col_index;
            blk->collision_tbl[0].state = META_COL_ENTRY;
            blk->num_valid_col_entry++;

            if (blk_ctx->kctx.dc_index > 0) {
                // no need to set state to META_DATA_COLLISION
                state = blk_ctx->state;
                DSS_ASSERT(blk_ctx->state == META_DATA_COLLISION ||
                            blk_ctx->state == META_DATA_COLLISION_ENTRY ||
                            blk_ctx->state == COLLISION_EXTENSION);
                blk->collision_tbl[0].state = DATA_COL_ENTRY;
                blk->num_valid_dc_col_entry++;
            } else {
                if (blk_ctx->state==META) {
                    state = COLLISION;
                    rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, blk_ctx->index, 1, state);
                    if (rc) return rc;
                    kvtrans_ctx->stat.meta--;
                    kvtrans_ctx->stat.mc++;
                } else if (blk_ctx->state == COLLISION_EXTENSION) {
                    rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, col_index, 1, COLLISION_EXTENSION);
                    if (rc) return rc;
                    kvtrans_ctx->stat.meta--;
                    kvtrans_ctx->stat.ce++;
                }
            }

            DSS_DEBUGLOG(DSS_KVTRANS, "Block [%u] state changes from [%s] to [%s] for key [%s].\n", blk_ctx->index, stateNames[blk_ctx->state], stateNames[state], req->req_key.key);
            // update meta blk only
            rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
            DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
            rc = KVTRANS_STATUS_SUCCESS;
            break;
        case update:
        case to_delete:
            // unhandled
            rc = KVTRANS_STATUS_NOT_FOUND;
            break;
        default:
            break;
        }
    } else {
        // key matches
        switch (blk_ctx->kctx.flag) {
        case new_write:
        case update:
            rc = _blk_update_value(ctx);
            if (rc) return rc;
            rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
            DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);

            rc = dss_kvtrans_write_ondisk_data(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
            DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
            rc = KVTRANS_STATUS_SUCCESS;
            break;
        case to_delete:
            if (blk_ctx->state==META_DATA_COLLISION || blk_ctx->state == META_DATA_COLLISION_ENTRY) {
                // META DATA COLLISION
                DSS_ASSERT(blk_ctx->kctx.dc_index==blk->data_collision_index);
                rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, blk_ctx, blk->data_collision_index, blk_ctx->index);
                if (rc==KVTRANS_STATUS_NOT_FOUND) {
                    DSS_ASSERT(blk_ctx->kctx.pindex>0);
                } else if (rc) {
                    return rc;
                }
                kvtrans_ctx->stat.mdc--;
            } else if (blk_ctx->state == COLLISION_EXTENSION || blk_ctx->state == DATA_COLLISION_CE) {
                kvtrans_ctx->stat.ce --;
                blk_ctx->blk->isvalid = 0;
            } else {
                kvtrans_ctx->stat.meta--;
            }
            rc = blk_ctx->kctx.ops.clean_blk(ctx);
            // blk is empty, no need to write it
            return rc;
        default:
            DSS_ASSERT(0);
            break;
        }
    }
    
    return rc;
}

dss_kvtrans_status_t
_new_write_ops(blk_ctx_t *blk_ctx, kvtrans_req_t *kreq) {
    DSS_ASSERT(blk_ctx->kctx.flag == new_write);

    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    int first_empty_index = MAX_COL_TBL_SIZE;
    ondisk_meta_t *blk = blk_ctx->blk;
    req_t *req = &kreq->req;
    uint64_t col_index;
    DSS_ASSERT(blk!=NULL);

    if (!blk->isvalid) {
        memcpy(blk->key, req->req_key.key, req->req_key.length);
        // ctx->entry_blk->checksum = {''};
        blk->key_len = req->req_key.length;
        blk->creation_time = req->req_ts;
        if (blk_ctx->kctx.dc_index!=0) {
            blk->data_collision_index = blk_ctx->kctx.dc_index;
        }
                    
        blk_ctx->vctx.value_blocks = _get_num_blocks_required_for_value(req, kreq->kvtrans_ctx->blk_size);
        blk_ctx->vctx.remote_val_blocks = 0;
        blk_ctx->vctx.iscontig = false;

        rc = _blk_init_value((void *)blk_ctx);
        if (rc) return rc;
        rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
        if (rc != KVTRANS_STATUS_IO_ERROR) {
            DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
            rc = KVTRANS_STATUS_SUCCESS;
        }
        rc = dss_kvtrans_write_ondisk_data(blk_ctx, kreq, false);
        if (rc != KVTRANS_STATUS_IO_ERROR) {
            DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
            rc = KVTRANS_STATUS_SUCCESS;
        }
        return rc;
    } else {
        first_empty_index = _col_tbl_get_first_entry(blk, false);
        rc = open_free_blk((void *)blk_ctx, &col_index);
        if (rc) return rc;
        if (first_empty_index==-1) {
            DSS_ASSERT(blk->collision_extension_index == 0);
            blk->collision_extension_index = col_index;

            DSS_DEBUGLOG(DSS_KVTRANS, "Block [%u] state changes from [%s] to [%s] for key [%s].\n", blk_ctx->index, stateNames[blk_ctx->state], stateNames[COLLISION_EXTENSION], req->req_key.key);

            rc = dss_kvtrans_set_blk_state(kreq->kvtrans_ctx, blk_ctx, col_index, 1, COLLISION_EXTENSION);
            kreq->kvtrans_ctx->stat.ce ++;
            if (rc) return rc;
        } else {
            col_entry_t *col_entry_buf = &blk->collision_tbl[first_empty_index];
            memcpy(col_entry_buf->key, req->req_key.key, KEY_LEN);
            col_entry_buf->meta_collision_index = col_index;
            col_entry_buf->state = META_COL_ENTRY;
            if (blk_ctx->kctx.dc_index>0) {
                col_entry_buf->state = DATA_COL_ENTRY;
                blk->num_valid_dc_col_entry++;
                rc = dss_kvtrans_set_blk_state(kreq->kvtrans_ctx, blk_ctx, col_index, 1, META_DATA_COLLISION);
                if (rc) return rc;
            }
            blk->num_valid_col_entry++;
        }
        // only need to update meta blk
        rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
        if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
        DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
        rc = KVTRANS_STATUS_SUCCESS;
        return rc;
    }
}

dss_kvtrans_status_t
_delete_collision_entry(blk_ctx_t *blk_ctx,
                        blk_ctx_t *col_ctx,
                        kvtrans_req_t *kreq)
{
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    ondisk_meta_t *blk = blk_ctx->blk;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    int entry_index = -1;
    int i;
    for (i=0;i<MAX_COL_TBL_SIZE;i++) {
        if (is_entry_match(&blk->collision_tbl[i], 
                            kreq->req.req_key.key, 
                            kreq->req.req_key.length)) {
            entry_index = i;
            break;
        }
    }

    if (entry_index==-1) {
        return KVTRANS_STATUS_ERROR;
    }

    rc = _col_tbl_remove_entry(blk_ctx->blk, entry_index);
    if (rc) return rc;
    // regular entries after delete
    //       -> key only            
    //          -> COLLISION        ->-> change state to meta for collsion
    //          -> MDC  / CE        ->-> no change
    //       -> no key or col  
    //          -> COLLISION        ->-> change state to empty
    //          -> MDC              ->-> remove index from dc_tbl, change state to empty
    //          -> CE               ->-> call delete_collision_extension
    if (blk->num_valid_col_entry==0 && blk->collision_extension_index==0) {
        if (blk->isvalid) {
            // no need to change state for META_DATA_COLLISION and COLLISION_ENTRY
            if (blk_ctx->state==COLLISION) {
                blk_ctx->state = META;
                rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, blk_ctx->index, 1, META); 
                kvtrans_ctx->stat.mc--;
                kvtrans_ctx->stat.meta++;
            } 
        } else {
            // no key, no col entries, no collision_extension
            if (blk_ctx->state == COLLISION_EXTENSION || blk_ctx->state == DATA_COLLISION_CE) {
                rc = clean_collision_extension(blk_ctx);
                return rc;
            }
            
            if (blk_ctx->state == META_DATA_COLLISION || blk_ctx->state == META_DATA_COLLISION_ENTRY) {
                rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, blk_ctx, blk->data_collision_index, blk_ctx->index);
                if (rc && rc != KVTRANS_STATUS_NOT_FOUND) {
                    // The dc_tbl entry has been deleted while deleting the parent blk
                    return rc;
                }
                kvtrans_ctx->stat.mdc--;
            } else {
                kvtrans_ctx->stat.mc--;
            }
            blk_ctx->state = EMPTY;
            rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, blk_ctx->index, 1, EMPTY);
            if (rc) return rc;
            rc = dss_kvtrans_delete_ondisk_blk(blk_ctx, kreq);
        }
        if (rc) return rc;
    }
    
    if ((col_ctx->state==META_DATA_COLLISION || col_ctx->state==META_DATA_COLLISION_ENTRY) && blk_ctx->blk->num_valid_col_entry>1) {
        blk_ctx->blk->num_valid_dc_col_entry--;
        if (blk_ctx->blk->num_valid_dc_col_entry==0) {
            rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, blk_ctx, col_ctx->blk->data_collision_index, col_ctx->index);
            if (rc && rc != KVTRANS_STATUS_NOT_FOUND) {
                // The dc_tbl entry has been deleted while deleting the parent blk
                return rc;
            }
            kvtrans_ctx->stat.mdc--;
            rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, blk_ctx->index, 1, COLLISION);
            kvtrans_ctx->stat.mc++;
        }
    }
    return rc;
}


dss_kvtrans_status_t 
clean_collision_extension(blk_ctx_t *ce_ctx)
{
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    blk_ctx_t *pre_ctx;
    kvtrans_ctx_t *kvtrans_ctx = ce_ctx->kreq->kvtrans_ctx;
    pre_ctx = TAILQ_PREV(ce_ctx, blk_elm, blk_link);
    DSS_ASSERT(pre_ctx!=NULL);
    // Col Extension after delete
    // col_ctx   
    //      ->-> no col entries
    //          -> key valid -> keep state.
    //          -> key invalid -> mark entry in pre_ctx invalid
    //              -> find pre of pre_ctx, repeatly check status
    //      ->-> has col entries -> keep state. 
    if (ce_ctx->blk->num_valid_col_entry== 0 && 
        !ce_ctx->blk->isvalid &&
        ce_ctx->blk->collision_extension_index == 0) {
        kvtrans_ctx->stat.ce --;
        if (pre_ctx->blk->collision_extension_index == ce_ctx->index) {
            pre_ctx->blk->collision_extension_index = 0;
        }
        if (ce_ctx->state == META_DATA_COLLISION_ENTRY) {
            DSS_ASSERT(ce_ctx->blk->data_collision_index != 0);
            rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, ce_ctx, ce_ctx->blk->data_collision_index, ce_ctx->index);
            if (rc==KVTRANS_STATUS_NOT_FOUND) {
                DSS_ASSERT(ce_ctx->kctx.pindex>0);
            } else if (rc) {
                return rc;
            }
            kvtrans_ctx->stat.mdc--;
        }
        rc = clean_blk((void *)ce_ctx);
        if (rc) return rc;
        while (pre_ctx->blk->num_valid_col_entry==0 && 
                pre_ctx->blk->collision_extension_index==0 && 
                !pre_ctx->blk->isvalid) {
            kvtrans_ctx->stat.ce --;
            if (pre_ctx->state == META_DATA_COLLISION_ENTRY) {
                DSS_ASSERT(pre_ctx->blk->data_collision_index != 0);
                rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, pre_ctx, pre_ctx->blk->data_collision_index, pre_ctx->index);
                if (rc==KVTRANS_STATUS_NOT_FOUND) {
                    DSS_ASSERT(pre_ctx->kctx.pindex>0);
                } else if (rc) {
                    return rc;
                }
                kvtrans_ctx->stat.mdc--;
            }
            rc = clean_blk((void *)pre_ctx);
            if (rc) return rc;
            pre_ctx = TAILQ_PREV(pre_ctx, blk_elm, blk_link);
            if (pre_ctx==NULL) {
                break;
            }
            pre_ctx->blk->collision_extension_index = 0;
        }
    }
    return rc;
}

static dss_kvtrans_status_t update_collision_blk(void *ctx) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    kvtrans_req_t *kreq = blk_ctx->kreq;
    ondisk_meta_t *blk = blk_ctx->blk; 
    req_t *req = &kreq->req;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    uint64_t col_index;
    int i;

    DSS_ASSERT(blk->num_valid_col_entry>0 || blk->collision_extension_index>0);
    DSS_ASSERT(blk_ctx->state == META_DATA_COLLISION ||
                blk_ctx->state == META_DATA_COLLISION_ENTRY ||
                blk_ctx->state == COLLISION ||
                blk_ctx->state == COLLISION_EXTENSION);
    if (!blk->isvalid && blk_ctx->kctx.flag!=to_delete) {
        blk_ctx->first_insert_blk_ctx = blk_ctx;
    } 
    if (iskeysame(blk->key, blk->key_len, req->req_key.key, req->req_key.length)) {
        switch (blk_ctx->kctx.flag) {
        case new_write:
        case update:
            rc = _blk_update_value(ctx);
            rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
            DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);

            rc = dss_kvtrans_write_ondisk_data(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
            DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
            rc = KVTRANS_STATUS_SUCCESS;
            break;
        case to_delete:
            rc = _blk_del_value(ctx);
            if (rc) return rc;
            blk->isvalid = false;
            // MDC after delete
            //       -> has key col only    ->-> remove index from dc_tbl
            //       -> has data col        ->-> keep the same state
            // Collision after delete
            //       -> has key col only    ->-> keep the same state
            //       -> has data col        ->-> false
            if ((blk_ctx->state==META_DATA_COLLISION || blk_ctx->state==META_DATA_COLLISION_ENTRY) &&
                    blk->num_valid_dc_col_entry==0 && 
                    blk->collision_extension_index==0) {
                // no deletion if this is a collision entry
                rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, blk_ctx, blk->data_collision_index, blk_ctx->index);
                if (rc==KVTRANS_STATUS_NOT_FOUND) {
                    DSS_ASSERT(blk_ctx->kctx.pindex>0);
                } else if (rc) {
                    return rc;
                }
                rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, blk_ctx->index, 1, COLLISION);
                if (rc) return rc;
                kvtrans_ctx->stat.mdc--;
                kvtrans_ctx->stat.mc++;
            }
            rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
            DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
            rc = KVTRANS_STATUS_SUCCESS;
            break;
        default:
            DSS_ASSERT(0);
        }
    } else {
        for (i=0;i<MAX_COL_TBL_SIZE;i++) {
            if (!_col_tbl_entry_isvalid(&blk->collision_tbl[i])
                && blk_ctx->first_insert_blk_ctx==NULL) {
                // later if no keys found, it can be written to this entry
                blk_ctx->first_insert_blk_ctx = blk_ctx;
                continue;
            } else if (is_entry_match(&blk->collision_tbl[i], 
                                        req->req_key.key, 
                                        req->req_key.length)) {
                // found matched key in col_tbl
                col_index = blk->collision_tbl[i].meta_collision_index;

                blk_ctx_t* col_blk_ctx = _get_next_blk_ctx(kvtrans_ctx, blk_ctx);
                if (col_blk_ctx==NULL) {
                    DSS_ASSERT(0);
                }
                col_blk_ctx->index = col_index;
                col_blk_ctx->kreq = kreq;
                rc = dss_kvtrans_get_blk_state(kvtrans_ctx, col_blk_ctx);
                if (rc) return rc;
                rc = dss_kvtrans_load_ondisk_blk(col_blk_ctx, kreq, true);
                rc = _update_kreq_stat_after_io(kreq, rc, COL_LOADING_DONE, QUEUE_TO_LOAD_COL);
                
                return rc;
            }
        }
        if (blk->collision_extension_index != 0) {
                // go to collision extension
                blk_ctx_t* col_blk_ctx = _get_next_blk_ctx(kvtrans_ctx, blk_ctx);
                if (col_blk_ctx==NULL) {
                    DSS_ASSERT(0);
                }
                col_blk_ctx->index = blk->collision_extension_index;
                col_blk_ctx->kreq = kreq;
                rc = dss_kvtrans_get_blk_state(kvtrans_ctx, col_blk_ctx);
                if (rc) return rc;
                rc = dss_kvtrans_load_ondisk_blk(col_blk_ctx, kreq, true);
                rc = _update_kreq_stat_after_io(kreq, rc, COL_EXT_LOADING_DONE, QUEUE_TO_LOAD_COL_EXT);
                return rc;
        } else {
            // collision_extension_index is 0
            if ((blk_ctx->first_insert_blk_ctx == blk_ctx ||
                blk_ctx->first_insert_blk_ctx == NULL) &&
                blk_ctx->kctx.flag != to_delete ) {
                // only need to write new col entry if
                // 1. the current blk_ctx is where the entry should write
                // 2. this is not a delete operation
                rc = _new_write_ops(blk_ctx, kreq);
            } else {
                rc = KVTRANS_STATUS_NOT_FOUND;
            }
        }
    }
    return rc;
}

static dss_kvtrans_status_t update_meta_data_collision_blk(void *ctx) {
    dss_kvtrans_status_t rc;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    ondisk_meta_t *blk = blk_ctx->blk; 

    // blk must be initialized
    DSS_ASSERT(!iskeynull(blk->key) || blk->num_valid_col_entry>0);

    if (blk->num_valid_col_entry==0 && blk->collision_extension_index==0) {
        rc = update_meta_blk(ctx);
    } else {
        rc = update_collision_blk(ctx);
    }
    return rc;
}



dss_kvtrans_status_t clean_blk(void *ctx) {
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    kvtrans_req_t *kreq = blk_ctx->kreq;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;

    if (blk_ctx->blk->value_location != INLINE) {
        rc = _blk_del_value(ctx);
        if (rc) return rc;
    }
    
    rc = dss_kvtrans_dc_table_exist(kvtrans_ctx, blk_ctx->index);
    if (rc == KVTRANS_STATUS_SUCCESS) {
        rc = dss_kvtrans_dc_table_update(kvtrans_ctx, blk_ctx, blk_ctx->index, EMPTY);
    } else {
        rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx, blk_ctx->index, 1, EMPTY);
    }

    if (rc) return rc;

    rc = dss_kvtrans_delete_ondisk_blk(blk_ctx, kreq);
    memset(blk_ctx->blk, 0, sizeof(ondisk_meta_t));
    return rc;
}

static dss_kvtrans_status_t update_collision_extension_blk(void *ctx);

static blk_key_ops_t g_blk_register[DEFAULT_BLOCK_STATE_NUM] = {
    // EMPTY is the initial state
    { &init_meta_blk, NULL, NULL},
    // META
    { NULL, &update_meta_blk, &clean_blk},
    // DATA
    { &init_meta_data_collision_blk, NULL, NULL},
    // COLLISION
    { NULL, &update_collision_blk, NULL},
    // DATA_COLLISION,
    { NULL, &update_meta_data_collision_blk, NULL},
    // META_DATA_COLLISION
    { NULL, &update_meta_data_collision_blk, &clean_blk},
    // META_DATA_COLLISION_ENTRY
    { NULL, &update_meta_data_collision_blk, &clean_blk},
    // COLLISION_EXTENSION
    { &init_meta_data_collision_blk, &update_collision_extension_blk, &clean_collision_extension},
    // DATA_COLLISION_EMPTY,
    { NULL, &update_meta_data_collision_blk, NULL},
    // DATA_COLLISION_CE
    { NULL, &update_meta_data_collision_blk, NULL},
};

static dss_kvtrans_status_t update_collision_extension_blk(void *ctx) {
    dss_kvtrans_status_t rc;
    blk_ctx_t *blk_ctx = (blk_ctx_t *) ctx;
    ondisk_meta_t *blk = blk_ctx->blk; 
    blk_ctx_t *blk_next;

    DSS_ASSERT(blk_ctx->state == COLLISION_EXTENSION ||
               blk_ctx->state == DATA_COLLISION_CE ||
               blk_ctx->state == DATA_COLLISION_EMPTY);
    if (blk_ctx->blk->data_collision_index > 0) {
        blk_ctx->kctx.dc_index = blk_ctx->blk->data_collision_index;
        rc = g_blk_register[META_DATA_COLLISION].update_blk(ctx);
    } else if (blk_ctx->blk->num_valid_col_entry==0 && blk_ctx->blk->isvalid) {
        rc = g_blk_register[META].update_blk(ctx);
    } else {
        rc = g_blk_register[COLLISION].update_blk(ctx);
    }

    // blk_next = TAILQ_NEXT(blk_ctx, blk_link);
    // if (blk_next && blk_ctx->kctx.flag != to_delete) {
    //     if (blk_next->state != COLLISION_EXTENSION ||
    //         blk_next->state != DATA_COLLISION_CE)
    //     dss_kvtrans_set_blks_state(blk_ctx->kreq->kvtrans_ctx, blk_next, blk_next->index, 1, COLLISION_EXTENSION);
    // }

    return rc;
}

// key_ops is the call-back from io_task on write completion
dss_kvtrans_status_t _kvtrans_key_ops(kvtrans_ctx_t *ctx, kvtrans_req_t *kreq)
{
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;

    req_t *req = &kreq->req;
    blk_ctx_t *blk_ctx, *col_ctx, *meta_blk_ctx, *pre_ctx;
    enum kvtrans_req_e prev_state = -1;
    dss_io_task_status_t iot_rc;
    dss_blk_allocator_status_t ba_rc;

    do {
        DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: Req[%p] with opc [%d] prev state [%d] current_state [%d] for key [%s]\n",
                        kreq->kvtrans_ctx, kreq, kreq->req.opc, prev_state, kreq->state, req->req_key.key);
        prev_state = kreq->state;
        switch (kreq->state) {
        case REQ_INITIALIZED:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_WRITE_REQ_INITIALIZED, 0, 0, 0, (uintptr_t)kreq->dreq);
#endif
            // only one blk_ctx in the meta_chain
            blk_ctx = TAILQ_FIRST(&kreq->meta_chain);
            DSS_ASSERT(blk_ctx);
            blk_ctx->vctx.iscontig = false;
            if (kreq->req.opc==KVTRANS_OPC_STORE) {
                blk_ctx->kctx.flag = new_write;
            } else if (kreq->req.opc==KVTRANS_OPC_DELETE) {
                blk_ctx->kctx.flag = to_delete;
            }
            rc = _alloc_entry_block(ctx, kreq, blk_ctx);
            if (rc) {
                DSS_ERRLOG("Failed to locate entry index for kreq [%p].\n", kreq);
                return rc;
            }
#ifndef DSS_BUILD_CUNIT_TEST
            if (kreq->state == QUEUE_TO_LOAD_ENTRY) {
                dss_trace_record(TRACE_KVTRANS_WRITE_QUEUE_TO_LOAD_ENTRY, 0, 0, 0, (uintptr_t)kreq->dreq);
            }
#endif
            break;
        case QUEUE_TO_LOAD_ENTRY:
            //External code should continue progres
            break;
        case ENTRY_LOADING_DONE:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_WRITE_ENTRY_LOADING_DONE, 0, 0, 0, (uintptr_t)kreq->dreq);
#endif
            blk_ctx = TAILQ_FIRST(&kreq->meta_chain);
            DSS_ASSERT(blk_ctx);
            blk_ctx->kctx.ops = g_blk_register[blk_ctx->state];
            if (blk_ctx->state==EMPTY || 
                blk_ctx->state==DATA || 
                blk_ctx->state==COLLISION_EXTENSION) {
                if (blk_ctx->kctx.flag==to_delete) {
                    rc = KVTRANS_STATUS_NOT_FOUND;
                    goto req_terminate;
                }
                rc = blk_ctx->kctx.ops.init_blk((void *)blk_ctx);
            } else {
                rc = blk_ctx->kctx.ops.update_blk((void *)blk_ctx);
            }
            
            if (rc) {
                DSS_ERRLOG("rc [%d]: Failed to process blk [%zu] with state [%s] for kreq [%p].\n", rc, blk_ctx->index, stateNames[blk_ctx->state], kreq);
                goto req_terminate;
            }

            if (kreq->state==ENTRY_LOADING_DONE) {
                // dss_blk_allocator_get_sync_meta_io_tasks(ctx->blk_alloc_ctx, kreq->io_tasks);
                // dss_io_task_submit(*kreq->io_tasks);
                kreq->state = QUEUE_TO_START_IO;
            }
            break;

        case QUEUE_TO_LOAD_COL_EXT:
        case QUEUE_TO_LOAD_COL:
            //External code should continue progres
            break;
        case COL_LOADING_DONE:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_WRITE_COL_LOADING_DONE, 0, 0, 0, (uintptr_t)kreq->dreq);
#endif
            col_ctx = TAILQ_LAST(&kreq->meta_chain, blk_elm);
            DSS_ASSERT(col_ctx!=NULL);
            blk_ctx = TAILQ_PREV(col_ctx, blk_elm, blk_link);
            DSS_ASSERT(blk_ctx->blk!=NULL && col_ctx!=blk_ctx);
            col_ctx->kreq = kreq;
            col_ctx->kctx.pindex = blk_ctx->index;
            col_ctx->kctx.flag = blk_ctx->kctx.flag;
            col_ctx->vctx.iscontig = false;
            col_ctx->vctx.value_blocks = 0;
            col_ctx->vctx.remote_val_blocks = 0;

            DSS_ASSERT(col_ctx->state!=META_DATA_COLLISION_ENTRY);
            if (col_ctx->state==META_DATA_COLLISION) {
                DSS_ASSERT(col_ctx->kctx.dc_index = col_ctx->blk->data_collision_index);
            } else if (col_ctx->state == META) {
                DSS_ASSERT(col_ctx->blk->num_valid_col_entry==0);
            } else if (col_ctx->state == COLLISION) {
                DSS_ASSERT(col_ctx->blk->num_valid_col_entry>0);
            } else if (col_ctx->state == COLLISION_EXTENSION ||
                        col_ctx->state == DATA_COLLISION_CE) {
                col_ctx->state = COLLISION_EXTENSION;
            }

            col_ctx->kctx.ops = g_blk_register[col_ctx->state];

            rc = col_ctx->kctx.ops.update_blk((void *)col_ctx);
            if (rc) {
                DSS_ERRLOG("Failed to process collision blk [%zu] with state [%s] for kreq [%p].\n", blk_ctx->index, stateNames[blk_ctx->state], kreq);
                goto req_terminate;
            }

            if (blk_ctx->kctx.flag == to_delete) {
                rc = _delete_collision_entry(blk_ctx, col_ctx, kreq);
                if (rc) {
                    DSS_ERRLOG("Failed to delete collision entry [%d] for blk [%zu] with state [%s] for kreq [%p].\n", col_ctx->index, blk_ctx->index, stateNames[blk_ctx->state], kreq);
                    return rc;
                }
            }
            
            // states of col_ctx has been updated in kctx.ops
            // need to update blk_ctx if it's not empty.
            // TODO: avoid update if blk_ctx has not been modified.
            if (blk_ctx->state!=EMPTY) {
                rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
                if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
                DSS_ASSERT(rc == KVTRANS_IO_QUEUED || rc == KVTRANS_IO_SUBMITTED || rc == KVTRANS_STATUS_SUCCESS);
                rc = KVTRANS_STATUS_SUCCESS;
            }

            if (kreq->state==COL_LOADING_DONE) {
                kreq->state = QUEUE_TO_START_IO;
            }
            break;
        case COL_EXT_LOADING_CONTIG:
            kreq->state = COL_EXT_LOADING_DONE;
            break;
        case COL_EXT_LOADING_DONE:
            blk_ctx = TAILQ_FIRST(&kreq->meta_chain);
            DSS_ASSERT(blk_ctx->blk!=NULL);
            col_ctx = TAILQ_LAST(&kreq->meta_chain, blk_elm);
            DSS_ASSERT(col_ctx!=NULL && col_ctx!=blk_ctx);
            col_ctx->kreq = kreq;
            col_ctx->kctx.pindex = blk_ctx->index;
            col_ctx->kctx.flag = blk_ctx->kctx.flag;
            col_ctx->vctx.iscontig = false;
            col_ctx->vctx.value_blocks = 0;
            col_ctx->vctx.remote_val_blocks = 0;
            col_ctx->state = COLLISION_EXTENSION;

            col_ctx->kctx.ops = g_blk_register[col_ctx->state];

            rc = col_ctx->kctx.ops.update_blk((void *)col_ctx);
            if (rc == KVTRANS_STATUS_NOT_FOUND &&
                col_ctx->first_insert_blk_ctx!=NULL &&
                blk_ctx->kctx.flag != to_delete) {
                // write new col entry to col_ctx->first_insert_blk_ctx
                // make sure this is not a delete operation
                rc = _new_write_ops(col_ctx->first_insert_blk_ctx, kreq);
                if (rc) {
                    DSS_ERRLOG("Failed to write new blk [%zu] with state [%s] for kreq [%p].\n", 
                                col_ctx->first_insert_blk_ctx->index, stateNames[col_ctx->state], kreq);

                    goto req_terminate;
                }
            } else if (rc) {
                DSS_ERRLOG("Failed to process collision extension blk [%zu] with state [%s] for kreq [%p].\n", 
                                col_ctx->index, stateNames[col_ctx->state], kreq);
                goto req_terminate;
            }
            if (kreq->state == COL_EXT_LOADING_DONE) {
                if (TAILQ_NEXT(col_ctx, blk_link)!=NULL && col_ctx->blk->collision_extension_index != 0) {
                    // continue to load next col extension
                    kreq->state = COL_EXT_LOADING_CONTIG;
                    break;
                }
                // if (col_ctx->kctx.flag==to_delete) {
                //     rc =  clean_collision_extension(col_ctx);
                //     if (rc) return rc;
                // }
                kreq->state = QUEUE_TO_START_IO;
            }
            break;
        case QUEUE_TO_START_IO:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_WRITE_QUEUE_TO_START_IO, 0, 0, (uintptr_t)kreq->id, (uintptr_t)kreq);
            if(ctx->is_ba_meta_sync_enabled == true && kreq->ba_meta_updated == true) {
                ba_rc = dss_blk_allocator_queue_sync_meta_io_tasks(ctx->blk_alloc_ctx, kreq->io_tasks);
                DSS_ASSERT(ba_rc == BLK_ALLOCATOR_STATUS_SUCCESS);
                break;
            } else if (kreq->io_to_queue) {
                // No BA changes, thus submit to io_task
                iot_rc = dss_io_task_submit(kreq->io_tasks);
                DSS_ASSERT(iot_rc==DSS_IO_TASK_STATUS_SUCCESS);
                break;
            }
#endif
            // Memory backend case
            kreq->state = IO_CMPL;
            break;
        case QUEUED_FOR_DATA_IO:
            //External code should continue progres
            break;
        case IO_CMPL:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_WRITE_IO_CMPL, 0, 0, 0, (uintptr_t)kreq->dreq);
#endif
            kreq->state = REQ_CMPL;
            if(ctx->is_ba_meta_sync_enabled && kreq->ba_meta_updated) {
                ba_rc = dss_blk_allocator_complete_meta_sync(ctx->blk_alloc_ctx, kreq->io_tasks);
                DSS_ASSERT(ba_rc == BLK_ALLOCATOR_STATUS_SUCCESS);
                DSS_DEBUGLOG(DSS_KVTRANS, "kreq [%p] completes BA meta sync with iotask [%p]\n", kreq, kreq->io_tasks);
            }
            break;
        case REQ_CMPL:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_WRITE_REQ_CMPL, 0, 0, 0, (uintptr_t)kreq->dreq);
            TAILQ_FOREACH(blk_ctx, &kreq->meta_chain, blk_link) {
                DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS[%p]: kreq write [%p] for lba[%u] completed [%p] by io_task\n", ctx, kreq, blk_ctx->index, kreq->io_tasks);
                rc = _pop_meta_blk_from_queue(ctx->meta_sync_ctx, blk_ctx);
                if (rc == KVTRANS_STATUS_ERROR) {
                    return rc;
                } else {
                    rc = KVTRANS_STATUS_SUCCESS;
                }
            }
#endif
            ctx->task_done++;
            return rc;
        default:
            break;
        }
    } while( kreq->state != prev_state);
#ifdef DSS_BUILD_CUNIT_TEST
    STAILQ_INSERT_TAIL(&kreq->kvtrans_ctx->req_head, kreq, req_link);
#endif

    return rc;

req_terminate:
    kreq->state = REQ_CMPL;

#ifndef DSS_BUILD_CUNIT_TEST
    // handle errors properly. otherwise, dirty LBAs become zombies
    // new requests would be queued by zombie dirty LBAs
    dss_trace_record(TRACE_KVTRANS_WRITE_REQ_CMPL, 0, 0, 0, (uintptr_t)kreq->dreq);
    TAILQ_FOREACH(blk_ctx, &kreq->meta_chain, blk_link) {
        rc = _pop_meta_blk_from_queue(ctx->meta_sync_ctx, blk_ctx);
        if (rc == KVTRANS_STATUS_ERROR) {
            return rc;
        } else {
            rc = KVTRANS_STATUS_SUCCESS;
        }
    }
#endif

    ctx->task_failed++;
    return rc;
}

dss_kvtrans_status_t
kvtrans_handle_kreq_state(kvtrans_req_t *kreq) {
    DSS_ASSERT(kreq);
    // TODO: check return status from io_task
    switch (kreq->state)
    {
    case QUEUE_TO_LOAD_ENTRY:
        kreq->state = ENTRY_LOADING_DONE;
        // dss_io_task_put(kreq->io_tasks);
        // dss_io_task_get_new(&kreq->io_tasks);
        dss_io_task_reset_ops( kreq->io_tasks);
        break;
    case QUEUE_TO_LOAD_COL:
        kreq->state = COL_LOADING_DONE;
        dss_io_task_reset_ops( kreq->io_tasks);
        break;
    case QUEUE_TO_LOAD_COL_EXT:
        kreq->state = COL_EXT_LOADING_DONE;
        dss_io_task_reset_ops( kreq->io_tasks);
        break;
    case QUEUED_FOR_DATA_IO:
        kreq->state = IO_CMPL;
        break;
    case QUEUE_TO_START_IO:
        kreq->state = IO_CMPL;
        break;
    default:
        break;
    }
    return KVTRANS_STATUS_SUCCESS;
}

dss_kvtrans_status_t kvtrans_store(kvtrans_ctx_t *ctx, kvtrans_req_t *kreq) {
    dss_kvtrans_status_t rc;
    kvtrans_handle_kreq_state(kreq);
    rc = _kvtrans_key_ops(ctx, kreq);
    return rc;
}

dss_kvtrans_status_t  kvtrans_delete(kvtrans_ctx_t *ctx, kvtrans_req_t *kreq) {
    dss_kvtrans_status_t rc;
    DSS_RELEASE_ASSERT(kreq->state!=QUEUED_FOR_DATA_IO);
    kvtrans_handle_kreq_state(kreq);
    rc = _kvtrans_key_ops(ctx, kreq);
    return rc;
}

dss_kvtrans_status_t _kvtrans_val_ops(kvtrans_ctx_t *ctx, kvtrans_req_t *kreq, blk_cb_t cb)
{
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    dss_io_task_status_t iot_rc;
    blk_ctx_t *blk_ctx;
    req_t *req = &kreq->req;

    enum kvtrans_req_e prev_state = -1;

    do {
        DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: Req[%p] with opc [%d] prev state [%s] current_state [%s] for key [%s]\n", 
                            kreq->kvtrans_ctx, kreq, kreq->req.opc, stateNames[prev_state], stateNames[kreq->state], req->req_key.key);
        prev_state = kreq->state;
        switch (kreq->state) {
        case REQ_INITIALIZED:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_READ_REQ_INITIALIZED, 0, 0, 0, (uintptr_t)kreq->dreq);
#endif
            // only one blk_ctx in the meta_chain
            blk_ctx = TAILQ_FIRST(&kreq->meta_chain);
            // TODO: init blk_ctx with 0
            rc = _alloc_entry_block(ctx, kreq, blk_ctx);
            break;
        case QUEUE_TO_LOAD_ENTRY:
            break;
        case ENTRY_LOADING_DONE:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_READ_ENTRY_LOADING_DONE, 0, 0, 0, (uintptr_t)kreq->dreq);
#endif
            blk_ctx = TAILQ_FIRST(&kreq->meta_chain);
            DSS_DEBUGLOG(DSS_KVTRANS, "KVTRANS [%p]: ENTRY LOADING DONE with blk_ctx->state [%s]\n", kreq->kvtrans_ctx, stateNames[blk_ctx->state]);
            switch(blk_ctx->state) {
                case EMPTY:
                case DATA:
                    // DATA will become DATA COLLISION in write
                    rc = KVTRANS_STATUS_NOT_FOUND;
                    kreq->state = REQ_CMPL;
                    break;
                case META:
                    if (blk_ctx->blk->isvalid && 
                            iskeysame(blk_ctx->blk->key, blk_ctx->blk->key_len, req->req_key.key, req->req_key.length)) {
                        rc = KVTRANS_STATUS_SUCCESS;
                        if (cb) {
                            rc = cb((void *)blk_ctx);
                            DSS_ASSERT(rc == KVTRANS_STATUS_SUCCESS);
                            //State should be updated by cb
                        } else {
                            kreq->state = REQ_CMPL;
                        }
                    } else {
                        rc = KVTRANS_STATUS_NOT_FOUND;
                        kreq->state = REQ_CMPL;
                    }
                    break;
                case DATA_COLLISION_EMPTY:
                case DATA_COLLISION_CE:
                case DATA_COLLISION:
                    DSS_ASSERT(0); // will be mdc
                    break;
                case COLLISION:
                case META_DATA_COLLISION:
                case META_DATA_COLLISION_ENTRY:
                    if (blk_ctx->blk->isvalid && 
                        iskeysame(blk_ctx->blk->key, blk_ctx->blk->key_len, req->req_key.key, req->req_key.length)) {
                        rc = KVTRANS_STATUS_SUCCESS;
                        if (cb) {
                            rc = cb((void *)blk_ctx);
                            //State should be updated by cb
                        } else {
                            kreq->state = REQ_CMPL;
                        }
                        break;
                    }
                    // a COLLISION META to update
                    int i;
                    for (i=0;i<MAX_COL_TBL_SIZE;i++) {
                        if (is_entry_match(&blk_ctx->blk->collision_tbl[i], req->req_key.key, req->req_key.length) &&
                            _col_tbl_entry_isvalid(&blk_ctx->blk->collision_tbl[i])) {
                            rc = KVTRANS_STATUS_SUCCESS;
                            if (cb) {
                                blk_ctx->index = blk_ctx->blk->collision_tbl[i].meta_collision_index;
                                rc = dss_kvtrans_load_ondisk_blk(blk_ctx, kreq, true);
                                rc = _update_kreq_stat_after_io(kreq, rc, ENTRY_LOADING_DONE, QUEUE_TO_LOAD_ENTRY);
                            } else {
                                kreq->state = REQ_CMPL;
                            }
                            break;
                        } else {
                            rc = KVTRANS_STATUS_NOT_FOUND;
                            continue;
                        }
                    }
                    if (rc == KVTRANS_STATUS_NOT_FOUND) {
                        if (blk_ctx->blk->collision_extension_index>0) {
                            blk_ctx->index = blk_ctx->blk->collision_extension_index;
                            rc = dss_kvtrans_load_ondisk_blk(blk_ctx, kreq, true);
                            rc = _update_kreq_stat_after_io(kreq, rc, ENTRY_LOADING_DONE, QUEUE_TO_LOAD_ENTRY);
                        } else {
                            rc = KVTRANS_STATUS_NOT_FOUND;
                            kreq->state = REQ_CMPL;
                        }
                    } else if (rc) return rc;
                    break;
                default:
                    DSS_RELEASE_ASSERT(0);
                    break;
            }
            break;
        case QUEUE_TO_START_IO:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_READ_QUEUE_TO_START_IO, 0, 0, 0, (uintptr_t)kreq->dreq);
            DSS_ASSERT(cb!=NULL);
            iot_rc = dss_io_task_submit(kreq->io_tasks);
            DSS_ASSERT(iot_rc==DSS_IO_TASK_STATUS_SUCCESS);
            break;
#endif
            kreq->state = IO_CMPL;
            break;
        case QUEUED_FOR_DATA_IO:
            //External code should continue progres
            break;
        case IO_CMPL:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_READ_IO_CMPL, 0, 0, 0, (uintptr_t)kreq->dreq);
#endif
            kreq->state = REQ_CMPL;
            // TODO: execute cb function?
            break;
        case REQ_CMPL:
#ifndef DSS_BUILD_CUNIT_TEST
            dss_trace_record(TRACE_KVTRANS_READ_REQ_CMPL, 0, 0, 0, (uintptr_t)kreq->dreq);
#endif
            ctx->task_done++;
            return rc;
        default:
            DSS_ASSERT(0);
        }
    } while (kreq->state != prev_state);
#ifdef DSS_BUILD_CUNIT_TEST
    STAILQ_INSERT_TAIL(&kreq->kvtrans_ctx->req_head, kreq, req_link);
#endif
    return rc;
}

dss_kvtrans_status_t kvtrans_retrieve(kvtrans_ctx_t *ctx, kvtrans_req_t *kreq) {
    dss_kvtrans_status_t rc;
    kvtrans_ctx_t *kvtrans_ctx = (kvtrans_ctx_t *)ctx;

    kvtrans_handle_kreq_state(kreq);
    rc = _kvtrans_val_ops(kvtrans_ctx , kreq, &_blk_load_value);
    return rc;
}

dss_kvtrans_status_t kvtrans_exist(kvtrans_ctx_t *ctx, kvtrans_req_t *kreq) {
    dss_kvtrans_status_t rc;
    kvtrans_ctx_t *kvtrans_ctx = (kvtrans_ctx_t *)ctx;

    DSS_RELEASE_ASSERT(kreq->state != QUEUED_FOR_DATA_IO);

    kvtrans_handle_kreq_state(kreq);
    rc = _kvtrans_val_ops(kvtrans_ctx , kreq, NULL);
    return rc;
}

bool iskreq_healthy(kvtrans_req_t *kreq) {
    req_t req = kreq->req;
    if (!kreq->kvtrans_ctx) return false;
    // if (kreq->req.req_key.key) return false;
    // for test only
    if (iskeynull(req.req_key.key)) return false;
    return true;
}

#ifdef DSS_BUILD_CUNIT_TEST
dss_kvtrans_status_t kv_process(kvtrans_ctx_t *kvtrans_ctx) {
    dss_kvtrans_status_t rc;
    kvtrans_req_t *kreq;
    req_t *req;
    blk_ctx_t *blk_ctx;

    kreq = STAILQ_FIRST(&kvtrans_ctx->req_head);
    if (!kreq) {
        return KVTRANS_STATUS_FREE;
    }
    STAILQ_REMOVE_HEAD(&kvtrans_ctx->req_head, req_link);

    if (!iskreq_healthy(kreq)) {
        printf("ERROR: kreq %zu is damaged\n", kreq->id);
        return KVTRANS_STATUS_ERROR;
    }

    req = &kreq->req;
    blk_ctx = TAILQ_FIRST(&kreq->meta_chain);

    // printf("receive kreq %zu, opc: %d\n", kreq->id, req->opc);
    switch(req->opc) {
    case KVTRANS_OPC_STORE:
        rc = kvtrans_store(kvtrans_ctx, kreq);
        break;
    case KVTRANS_OPC_RETRIEVE:
        rc = kvtrans_retrieve(kvtrans_ctx, kreq);
        break;
    case KVTRANS_OPC_DELETE:
        rc = kvtrans_delete(kvtrans_ctx, kreq);
        break;
    case KVTRANS_OPC_EXIST:
        rc = kvtrans_exist(kvtrans_ctx, kreq);
        break;
    default:
        break;
    }   
    // update_ticks(&req->cmpl);

    // kvtrans_ctx->stat.pre += (double) (req->hash - req->bg);
    // kvtrans_ctx->stat.hash += (double) (req->keyset - req->hash);
    // kvtrans_ctx->stat.setkey += (double) (req->valset - req->keyset);
    // kvtrans_ctx->stat.setval += (double) (req->cmpl - req->valset);

    return rc;
}
#endif

void dump_blk_ctx(blk_ctx_t *blk_ctx) {
    if(!blk_ctx) {
        printf("Seg_ctx is null.\n");
        return;
    }

    printf("===============\n");
    printf("index: %zu\n", blk_ctx->index);
    printf("kctx: \n");
    printf("    dc_index: %zu\n", blk_ctx->kctx.dc_index);
    printf("    flag: %d\n", blk_ctx->kctx.flag);
    printf("    state: %s\n", stateNames[blk_ctx->state]);
    printf("vctx: \n");
    printf("    iscontig: %d\n", blk_ctx->vctx.iscontig);
    printf("    remote_val_blocks: %zu\n", blk_ctx->vctx.remote_val_blocks);
    printf("    value_blocks: %zu\n", blk_ctx->vctx.value_blocks);
    if (!blk_ctx->kreq) printf("kreq is null.\n");
    else {
        printf("kreq: \n");
        printf("    id: %zu\n", blk_ctx->kreq->id);
        printf("    state: %s\n", stateNames[blk_ctx->kreq->state]);
        printf("    key: %s\n", blk_ctx->kreq->req.req_key.key);
    }


    printf("blk: \n");
    if (!iskeynull(blk_ctx->blk->key)) printf("    key: %s\n", blk_ctx->blk->key);
    else printf("    key: 0\n");
    printf("    num_valid_col_entry: %2x\n", blk_ctx->blk->num_valid_col_entry);
    printf("    value_location: %2x\n", blk_ctx->blk->value_location);
    printf("    value_size: %zu\n", blk_ctx->blk->value_size);
    printf("    data_collision_index: %zu\n", blk_ctx->blk->data_collision_index);   
    
}

void dss_kvtrans_dump_in_memory_meta(kvtrans_ctx_t *kvt_ctx) {
    dss_blk_allocator_status_t ba_rc;
    int rc;
    char dc_path[256];

    ba_rc = dss_blk_allocator_write_meta_to_file(kvt_ctx->blk_alloc_ctx);
    DSS_ASSERT(ba_rc == BLK_ALLOCATOR_STATUS_SUCCESS);

    snprintf(dc_path, 255, "/var/log/dss_bitmap.%s.txt", kvt_ctx->kvtrans_params.name);
    rc = dss_kvtrans_dump_dc_tbl(kvt_ctx, dc_path);
    DSS_DEBUGLOG(DSS_KVTRANS, "write %d entries to %s\n", rc, dc_path);
}

#ifdef MEM_BACKEND

void init_mem_backend(kvtrans_ctx_t  *ctx, uint64_t meta_pool_size, uint64_t data_pool_size) {
    if (!ctx) {
        printf("kvtrans_ctx is not initialized\n");
        return;
    }

    if (g_disk_as_meta_store) {
        return;
    }

    ctx->meta_ctx = (ondisk_meta_ctx_t *) calloc(1, sizeof(ondisk_meta_ctx_t));
    if (!ctx->meta_ctx) {
        printf("meta_ctx init failed\n");
        return;
    }

    init_meta_ctx(ctx->meta_ctx, meta_pool_size);

    if (g_disk_as_data_store) {
        return;
    }
    
    ctx->data_ctx = (ondisk_data_ctx_t *) calloc(1, sizeof(ondisk_data_ctx_t));
    if (!ctx->data_ctx) {
        printf("data_ctx init failed\n");
        return;
    }

    init_data_ctx(ctx->data_ctx, data_pool_size);
}


void reset_mem_backend(kvtrans_ctx_t  *ctx) {
    if (!ctx) {
        printf("kvtrans_ctx is not initialized\n");
        return;
    }  
    reset_cache_table(ctx->meta_ctx->meta_mem);
    reset_data_ctx(ctx->data_ctx);
}

void free_mem_backend(kvtrans_ctx_t  *ctx) {
    if (!ctx) {
        printf("kvtrans_ctx is not initialized\n");
        return;
    }
    if (ctx->meta_ctx) free_meta_ctx(ctx->meta_ctx);
    if (ctx->data_ctx) free_data_ctx(ctx->data_ctx);
}
#endif
