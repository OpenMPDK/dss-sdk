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
#ifdef MEM_BACKEND

#ifndef DSS_BUILD_CUNIT_TEST
bool g_disk_as_data_store = true;
bool g_disk_as_meta_store = true;
#else
bool g_disk_as_data_store = false;
bool g_disk_as_meta_store = false;
#endif

#include "kvtrans_mem_backend.h"

void set_kvtrans_disk_data_store(bool val) {
    g_disk_as_data_store = val;
}

void set_kvtrans_disk_meta_store(bool val) {
    g_disk_as_meta_store = val;
}

#else
void set_kvtrans_disk_data_store(bool val) {
    return;
}

void set_kvtrans_disk_meta_store(bool val) {
    return;
}
#endif

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
dss_kvtrans_set_blk_state(kvtrans_ctx_t *ctx, uint64_t index, 
                            uint64_t blk_num, blk_state_t state)
{
    dss_blk_allocator_status_t rc;
    if (state==EMPTY) {
       rc = dss_blk_allocator_clear_blocks(ctx->blk_alloc_ctx, index, blk_num);
    } else {
        DSS_ASSERT(blk_num==1);
        rc = dss_blk_allocator_set_blocks_state(ctx->blk_alloc_ctx, index, blk_num, state);
    }
    if (rc) {
        // TODO: error handling
        return KVTRANS_STATUS_SET_BLK_STATE_ERROR;
    }
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
        return KVTRANS_STATUS_ERROR;
    }
    blk_ctx->state = (blk_state_t) blk_state;
    return KVTRANS_STATUS_SUCCESS;
}

dss_kvtrans_status_t
dss_kvtrans_get_free_blk_ctx(kvtrans_ctx_t *ctx, blk_ctx_t **blk_ctx)
{
    dss_mallocator_status_t rc;
    rc = dss_mallocator_get(ctx->blk_ctx_allocator, 0, (dss_mallocator_item_t **)blk_ctx);
    if (blk_ctx==NULL) return KVTRANS_STATUS_ERROR;
    if (rc==DSS_MALLOC_NEW_ALLOCATION || (*blk_ctx)->blk==NULL) {
        // alloc blk 
#ifndef DSS_BUILD_CUNIT_TEST
        (*blk_ctx)->blk = (ondisk_meta_t *)spdk_dma_zmalloc(BLOCK_SIZE, BLK_ALIGN, NULL);
#else
        (*blk_ctx)->blk = (ondisk_meta_t *) calloc(1, BLOCK_SIZE);
#endif
        if ((*blk_ctx)->blk==NULL) {
            return KVTRANS_STATUS_ERROR;
        }
    }
    return KVTRANS_STATUS_SUCCESS;
}

dss_kvtrans_status_t
dss_kvtrans_put_free_blk_ctx(kvtrans_ctx_t *ctx,
                            blk_ctx_t *blk_ctx,
                            bool free_blk)
{
    dss_mallocator_status_t rc;
    
    ondisk_meta_t *tmp = blk_ctx->blk;
    if (tmp && free_blk) {
#ifndef DSS_BUILD_CUNIT_TEST
        spdk_free(tmp);
#else
        free(tmp);
#endif
        tmp = NULL;
    }
    memset(blk_ctx, 0, sizeof(blk_ctx_t));
    blk_ctx->blk = tmp;
    if (blk_ctx->blk) {
        memset(blk_ctx->blk, 0, sizeof(ondisk_meta_t));
    }
    if (blk_ctx==NULL){
        printf("Error: blk_ctx to put is NULL\n");
        DSS_ASSERT(0);
        return KVTRANS_STATUS_ERROR;
    }
    rc = dss_mallocator_put(ctx->blk_ctx_allocator, 0, (dss_mallocator_item_t *)blk_ctx);
    if (rc==DSS_MALLOC_ERROR) {
        return KVTRANS_STATUS_ERROR;
    }
    return KVTRANS_STATUS_SUCCESS;
}

dss_kvtrans_status_t dss_kvtrans_dc_table_exist(kvtrans_ctx_t *ctx, const uint64_t index) {
    Word_t *entry;
    entry = (Word_t *) JudyLGet(ctx->dc_tbl, (Word_t) index, PJE0);
    if (entry) {
        return KVTRANS_STATUS_SUCCESS;
    }
    return KVTRANS_STATUS_ERROR;
}

dss_kvtrans_status_t dss_kvtrans_dc_table_lookup(kvtrans_ctx_t *ctx, const uint64_t dc_index, uint64_t *mdc_index) {
    Word_t *entry;
    dc_item_t *it;

    entry = (Word_t *) JudyLGet(ctx->dc_tbl, (Word_t) dc_index, PJE0);
    if (entry) {
        it = (dc_item_t *) *entry;
        memcpy(mdc_index, &it->mdc_index, sizeof(uint64_t));
        return KVTRANS_STATUS_SUCCESS;
    }
    return KVTRANS_STATUS_ERROR;
}

dss_kvtrans_status_t dss_kvtrans_dc_table_update(kvtrans_ctx_t *ctx, const uint64_t dc_index, blk_state_t ori_state) {
    Word_t *entry;
    dc_item_t *it;

    DSS_ASSERT(ori_state==DATA || ori_state==COLLISION_EXTENSION || ori_state==EMPTY);
    entry = (Word_t *) JudyLGet(ctx->dc_tbl, (Word_t) dc_index, PJE0);
    if (entry) {
        it = (dc_item_t *) *entry;
        it->ori_state = ori_state;
        return KVTRANS_STATUS_SUCCESS;
    }
    return KVTRANS_STATUS_ERROR;
}


dss_kvtrans_status_t dss_kvtrans_dc_table_insert(kvtrans_ctx_t *ctx, const uint64_t dc_index, const uint64_t mdc_index, blk_state_t ori_state) {
    dss_kvtrans_status_t rc;
    Word_t *entry;
    dc_item_t *it;

    if (dss_kvtrans_dc_table_exist(ctx, dc_index)==KVTRANS_STATUS_SUCCESS) {
        printf("ERROR: dc %zu is existent\n", dc_index);
        return KVTRANS_STATUS_ERROR;
    }
    DSS_ASSERT(ori_state==DATA || ori_state==COLLISION_EXTENSION);
    if (ctx->dc_size == MAX_DC_NUM - 1) {
        // TODO: add error handling
        return KVTRANS_STATUS_ERROR;
    }

    rc = dss_kvtrans_set_blk_state(ctx, dc_index, 1, DATA_COLLISION);
    if (!rc) {
        rc = dss_kvtrans_set_blk_state(ctx, mdc_index, 1, META_DATA_COLLISION);
        if (rc) {
            // roll back to DATA
            rc = dss_kvtrans_set_blk_state(ctx, dc_index, 1, DATA);
            if (!rc) {
                return KVTRANS_STATUS_SET_DC_STATE_ERROR;
            }
        }
    }

    it = &ctx->dc_pool[ctx->dc_size];
    it->mdc_index = mdc_index;
    it->ori_state = ori_state;

    entry = (Word_t *) JudyLIns(&ctx->dc_tbl, (Word_t) dc_index, PJE0);
    *entry = (Word_t) it;
    ctx->dc_size++;

    return rc;
}

dss_kvtrans_status_t dss_kvtrans_dc_table_delete(kvtrans_ctx_t  *ctx, const uint64_t dc_index, const uint64_t mdc_index) {
    dss_kvtrans_status_t rc;
    Word_t *entry;
    dc_item_t *it;
    int cleaned_bytes;

    entry = (Word_t *) JudyLGet(ctx->dc_tbl, (Word_t) dc_index, PJE0);
    if (!entry) {
        return KVTRANS_STATUS_NOT_FOUND;
    }

    it = (dc_item_t *) *entry;
    if (it->mdc_index!=mdc_index) {
        return KVTRANS_STATUS_NOT_FOUND;
    }
    rc = dss_kvtrans_set_blk_state(ctx, dc_index, 1, it->ori_state);

    cleaned_bytes = JudyLDel(&ctx->dc_tbl, (Word_t) dc_index, PJE0);
    DSS_ASSERT(cleaned_bytes!=0);
    ctx->dc_size--;
    ctx->stat.dc--;
    return rc;
}

dss_kvtrans_status_t
dss_kvtrans_queue_load_ondisk_blk(blk_ctx_t *blk_ctx,
                                    kvtrans_req_t *kreq)
{
    dss_io_task_status_t iot_rc;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    iot_rc = dss_io_task_add_blk_read(kreq->io_tasks, 
                                    kvtrans_ctx->target_dev,
                                    blk_ctx->index,
                                    1, 
                                    (void *) blk_ctx->blk, 
                                    BLOCK_SIZE, 0, true);
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
            iot_rc = dss_io_task_submit(kreq->io_tasks);
            kreq->io_to_queue = false;
            DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
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
                            kreq->req.req_value.value, offset, BLOCK_SIZE,  i);
            iot_rc = dss_io_task_add_blk_read(kreq->io_tasks, \
                                        kvtrans_ctx->target_dev, \
                                        blk->place_value[i].value_index, \
                                        blk->place_value[i].num_chunks, \
                                        (void *)((uint64_t )kreq->req.req_value.value + offset), \
                                        blk->place_value[i].num_chunks * BLOCK_SIZE, \
                                        0, false);
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
        offset += blk->place_value[i].num_chunks * BLOCK_SIZE;
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
            index, blk_num, kreq->req->req_value.value, blk_num*BLOCK_SIZE, offset, false)) {
                return KVTRANS_STATUS_ERROR;
            }
        offset += blk_num*BLOCK_SIZE;
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
    // void *dma_buff = spdk_dma_malloc(sizeof(blk_ctx->blk), BLOCK_SIZE, NULL);
    iot_rc = dss_io_task_add_blk_write(kreq->io_tasks, 
                                    kvtrans_ctx->target_dev,
                                    blk_ctx->index,
                                    1, 
                                    (void *) blk_ctx->blk, 
                                    BLOCK_SIZE, 0, false);
    DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
    kreq->io_to_queue = true;
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
                            kreq->req.req_value.value, offset, BLOCK_SIZE,  i);
                iot_rc = dss_io_task_add_blk_write(kreq->io_tasks, \
                                        kreq->kvtrans_ctx->target_dev, \
                                        blk->place_value[i].value_index, \
                                        blk->place_value[i].num_chunks, \
                                        (void *)((uint64_t )kreq->req.req_value.value + offset), \
                                        blk->place_value[i].num_chunks * BLOCK_SIZE, \
                                        0, false);
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
            offset += blk_ctx->blk->place_value[i].num_chunks * BLOCK_SIZE;
        }
    }
#ifndef DSS_BUILD_CUNIT_TEST
    if(submit_for_disk_io == true) {
        kreq->state = QUEUED_FOR_DATA_IO;
        iot_rc = dss_io_task_submit(kreq->io_tasks);
        DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
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

    bit_shift = ctx->hash_fn_ctx->hash_size - ctx->hash_bit_in_use;
    
    // avoid index 0
    *index = ((*index >> bit_shift) % (ctx->kvtrans_params.total_blk_num - 1)) + 1;
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
    ctx->dc_pool = (dc_item_t*) malloc (sizeof(dc_item_t) * MAX_DC_NUM);
    if (!ctx->dc_pool) {
        printf("ERROR: malloc dc_pool failed.\n");
        return;
    }
    memset(ctx->dc_pool, 0, sizeof(dc_item_t) * MAX_DC_NUM);
}

void free_dc_table(kvtrans_ctx_t  *ctx) {
    if(!ctx || !ctx->dc_pool) 
        return;
    JudyLFreeArray(&ctx->dc_tbl, PJE0);
    free(ctx->dc_pool);
}

kvtrans_params_t set_default_params() {
    kvtrans_params_t params = {};
    params.hash_size = 32;
    params.hash_type = spooky;
    params.id = 0;
    params.name = "dss_kvtrans";
    params.thread_num = 1;
    params.meta_blk_num = 1024;
    params.total_blk_num = BLK_NUM;
    params.blk_alloc_name = "simbmap_allocator";
    return params;    
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
    } else {
        ctx->kvtrans_params = set_default_params();
    }

    dss_blk_allocator_set_default_config(ctx->kvtrans_params.dev, &config);
    //dss_blk_allocator_set_default_config(NULL, &config);

    if (*ctx->kvtrans_params.blk_alloc_name=='\0') {
        ctx->kvtrans_params.blk_alloc_name = DEFAULT_BLK_ALLOC_NAME;
    }
    //TODO: Check for valid block alloc name and set
    config.blk_allocator_type = ctx->kvtrans_params.blk_alloc_name;
    config.num_total_blocks = ctx->kvtrans_params.total_blk_num;
    // exclude empty state
    config.num_block_states = DEFAULT_BLOCK_STATE_NUM - 1;

    ctx->blk_alloc_ctx = dss_blk_allocator_init(ctx->kvtrans_params.dev, &config);
    //ctx->blk_alloc_ctx = dss_blk_allocator_init(NULL, &config);
    if (!ctx->blk_alloc_ctx) {
        printf("ERROR: blk_allocator init failed\n");
         goto failure_handle;
    }
    if(dss_kvtrans_set_blk_state(ctx, 0, 1, META)) {
        // index 0 is regared as invalid index
        printf("ERROR: set index 0 to meta failed\n");
        goto failure_handle;
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
    blk_ma_opts.max_per_cache_items = 1024; 
    blk_ma_opts.num_caches = 1;

    ctx->blk_ctx_allocator = dss_mallocator_init(DSS_MEM_ALLOC_MALLOC, blk_ma_opts);
    DSS_ASSERT(ctx->blk_ctx_allocator);

    init_dc_table(ctx);
    if (!ctx->dc_pool) {
        printf("ERROR: dc_table init failed\n");
        goto failure_handle;
    }

#ifdef MEM_BACKEND
    init_mem_backend(ctx, ctx->kvtrans_params.meta_blk_num, ctx->kvtrans_params.total_blk_num);
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
    if (!ctx) return;
    if (ctx->blk_alloc_ctx) {
        // work around to free blk pointer inside blk_ctx
        dss_mallocator_status_t dm_rc;
        int max_blk_size;
        int i;
        dss_mallocator_get_cache_size(ctx->blk_ctx_allocator, 0, &max_blk_size);
        blk_ctx_t *blk_ctx[max_blk_size];
        for (i=0; i<max_blk_size; i++) {
            dm_rc = dss_mallocator_get(ctx->blk_ctx_allocator, 0, (dss_mallocator_item_t **) &blk_ctx[i]);
            DSS_ASSERT(dm_rc != DSS_MALLOC_NEW_ALLOCATION);
            if (blk_ctx[i]->blk) free(blk_ctx[i]->blk);
        }
        for (i=0; i<max_blk_size; i++) {
            dm_rc = dss_mallocator_put(ctx->blk_ctx_allocator, 0, (dss_mallocator_item_t *) blk_ctx[i]);
        }
        dss_blk_allocator_destroy(ctx->blk_ctx_allocator);
    }
    if (ctx->hash_fn_ctx) free_hash_fn_ctx(ctx->hash_fn_ctx);
    if (ctx->blk_ctx_allocator)  dss_mallocator_destroy(ctx->blk_ctx_allocator);
    free_dc_table(ctx);
#ifdef MEM_BACKEND
    free_mem_backend(ctx);
#endif
    free(ctx);
}


kvtrans_req_t *init_kvtrans_req(kvtrans_ctx_t *kvtrans_ctx, req_t *req, kvtrans_req_t *preallocated_req) {
    dss_kvtrans_status_t rc;
    kvtrans_req_t *kreq;
    blk_ctx_t *blk_ctx;
    if(preallocated_req) {
        kreq = preallocated_req;
        kreq->req_allocated = false;
    } else {
        kreq = (kvtrans_req_t *) calloc(1, sizeof(kvtrans_req_t));
        if (!kreq) {
            printf("ERROR: malloc kreq failed.\n");
            goto failure_handle;
        }
        kreq->req_allocated = true;
    }

    if (TAILQ_EMPTY(&kreq->meta_chain)) {
        TAILQ_INIT(&kreq->meta_chain);
        rc = dss_kvtrans_get_free_blk_ctx(kvtrans_ctx, &blk_ctx);
        if (rc) {
            printf("ERROR: blk_ctx allocator returns false.\n");
            return NULL;
        }
        
        TAILQ_INSERT_HEAD(&kreq->meta_chain, blk_ctx, blk_link);
        kreq->num_meta_blk = 1;
        blk_ctx->kreq = kreq;
    }
    
    if (!req) {
        printf("ERROR: req is null.\n");
        goto failure_handle;
    }

    kreq->req.req_key = req->req_key;
    kreq->req.req_value = req->req_value;
    kreq->req.opc = req->opc;
    kreq->req.req_ts = req->req_ts;
    kreq->id = kvtrans_ctx->task_num++;
    kreq->kvtrans_ctx = kvtrans_ctx;
#ifdef DSS_BUILD_CUNIT_TEST
    STAILQ_INSERT_TAIL(&kvtrans_ctx->req_head, kreq, req_link);
#endif
    kreq->state = REQ_INITIALIZED;
    kreq->initialized = true;

    return kreq;

failure_handle:
    free_kvtrans_req(kreq);
    return NULL;
}

void free_kvtrans_req(kvtrans_req_t *kreq)
{
    dss_io_task_status_t iot_rc;
    dss_kvtrans_status_t rc;
    if (!kreq) {
        return;
    }

    if(kreq->io_tasks) {
        iot_rc = dss_io_task_put(kreq->io_tasks);
        DSS_ASSERT(iot_rc == DSS_IO_TASK_STATUS_SUCCESS);
        //TODO: Error handling
    }

    if (kreq && kreq->req_allocated) {
        struct blk_ctx *b1;
        struct blk_ctx *b2;
        b1 = TAILQ_FIRST(&kreq->meta_chain);
        while (b1!=NULL) {
            b2 = TAILQ_NEXT(b1, blk_link);
            TAILQ_REMOVE(&kreq->meta_chain, b1, blk_link);
            rc = dss_kvtrans_put_free_blk_ctx(kreq->kvtrans_ctx, b1, true);
            if (rc) DSS_DEBUGLOG(DSS_KVTRANS, "free blk_ctx failed\n");
            b1 = b2;
        }
        free(kreq);
    }
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
    hash_fn_ctx->update(req->req_key.key, hash_fn_ctx);
    blk_ctx->index = hash_fn_ctx->hashcode;
    // ensure index is within [1, blk_alloc_opts.num_total_blocks - 1]
    ctx->kv_assign_block(&blk_ctx->index, ctx);

    DSS_ASSERT(blk_ctx->index>0 && blk_ctx->index<BLK_NUM);
    rc = dss_kvtrans_get_blk_state(ctx, blk_ctx);

    switch (blk_ctx->state) {
    case EMPTY:
        // no need to load blk
        kreq->state = ENTRY_LOADING_DONE;
        break;
    case COLLISION_EXTENSION:
    case DATA:
        // find an EMPTY blk TODO: rehash or not?
        blk_ctx->kctx.dc_index = blk_ctx->index;
        kreq->state = ENTRY_LOADING_DONE;
        break;
    case DATA_COLLISION:
        // find MDC at index
        blk_ctx->kctx.dc_index = blk_ctx->index;
        rc = dss_kvtrans_dc_table_lookup(ctx, blk_ctx->kctx.dc_index, &blk_ctx->index);
        DSS_ASSERT(rc==KVTRANS_STATUS_SUCCESS);
        DSS_ASSERT(blk_ctx->index>0 && blk_ctx->index<BLK_NUM);
        blk_ctx->state = META_DATA_COLLISION;
        // continue to load ondisk blk
    case META:
    case COLLISION:
    case META_DATA_COLLISION:
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
        rc = dss_kvtrans_get_free_blk_ctx(ctx, &col_blk_ctx);
        if (rc) return NULL;
        TAILQ_INSERT_TAIL(&blk_ctx->kreq->meta_chain, col_blk_ctx, blk_link);
    }
    return col_blk_ctx;
}

uint64_t _get_num_blocks_required_for_value(req_t *req) {
    uint64_t block_size = BLOCK_SIZE;
    if (req->req_value.length < MAX_INLINE_VALUE) {
        return 0;
    }
    return CEILING(req->req_value.length, block_size);
}

/* find a contiguous or scatter of DATA blocks */
dss_kvtrans_status_t find_data_blocks(blk_ctx_t *ctx, dss_blk_allocator_context_t *blk_alloc, uint64_t num_blocks) {
    if (num_blocks==0) {
        return KVTRANS_STATUS_SUCCESS;
    } 
    uint64_t allocated_start_block = 0;

    if(dss_blk_allocator_alloc_blocks_contig(blk_alloc, DATA, 
        ctx->index+1, num_blocks, &allocated_start_block)==BLK_ALLOCATOR_STATUS_SUCCESS) {
        DSS_ASSERT(allocated_start_block>0 && allocated_start_block<BLK_NUM);
        ctx->blk->place_value[ctx->blk->num_valid_place_value_entry].num_chunks = num_blocks;
        ctx->blk->place_value[ctx->blk->num_valid_place_value_entry].value_index = allocated_start_block;

        if (allocated_start_block!=ctx->index+1) {
            ctx->vctx.remote_val_blocks++;
        }
        ctx->blk->num_valid_place_value_entry++;
        //TODO: handle this case
        DSS_ASSERT(ctx->blk->num_valid_place_value_entry!=MAX_DATA_COL_TBL_SIZE);
        return KVTRANS_STATUS_SUCCESS;
    }

    uint64_t lb = num_blocks/2;
    uint64_t rb = num_blocks - lb;

    if (lb+rb==1 && num_blocks==1) {
        printf("ERROR: failed to find 1 block at index %zu\n", ctx->index+1);
        return KVTRANS_STATUS_ERROR;
    }

    if(find_data_blocks(ctx, blk_alloc, lb)==KVTRANS_STATUS_SUCCESS &&
       find_data_blocks(ctx, blk_alloc, rb)==KVTRANS_STATUS_SUCCESS) {
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
        rc = find_data_blocks(blk_ctx, blk_alloc, blk_ctx->vctx.value_blocks);
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
                        rc = dss_kvtrans_set_blk_state(kvtrans_ctx, s_idx, e_idx-s_idx, EMPTY);
                        if(rc) return rc;
                    }
                    // the ori_state of e_idx is EMPTY
                    rc = dss_kvtrans_dc_table_update(kvtrans_ctx, e_idx, EMPTY);
                    s_idx = e_idx + 1;
                } else if (e_idx == s_idx && e_idx == (index+blk_num-1)) {
                    // if the last blk is not a data collision
                    rc = dss_kvtrans_set_blk_state(kvtrans_ctx, e_idx, 1, EMPTY);
                    if(rc) return rc;
                    break;
                }
                e_idx++;
            }
            if (s_idx!=e_idx) {
                // handle cases if the rest of blks are not data collision blks
                rc = dss_kvtrans_set_blk_state(kvtrans_ctx, s_idx, e_idx-s_idx, EMPTY);
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
    if (rc) return rc;

    blk_ctx->vctx.value_blocks = _get_num_blocks_required_for_value(req);
    blk_ctx->vctx.iscontig = false;
    blk_ctx->vctx.remote_val_blocks = 0;

    rc = _blk_init_value(ctx);
    
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
    
    if (!blk_ctx->nothash) {
        // META blk is located by hashing. We need to alloc value blocks seperately.
        blk_ctx->vctx.value_blocks = _get_num_blocks_required_for_value(req);
        // TODO: change allocated_start_lba to NULL
        if (dss_blk_allocator_alloc_blocks_contig(kvtrans_ctx->blk_alloc_ctx, state, 
                blk_ctx->index, 1, &allocated_start_lba)) {
            return KVTRANS_STATUS_ALLOC_CONTIG_ERROR;
        }
        DSS_ASSERT(allocated_start_lba==blk_ctx->index);
    }
    
    blk_ctx->blk->isvalid = true;

    rc = _blk_init_value(ctx);
    if (rc) {
        goto roll_back;
    }

    rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
    if (rc == KVTRANS_STATUS_IO_ERROR) goto roll_back;
    rc = dss_kvtrans_write_ondisk_data(blk_ctx, kreq, false);
    if (rc == KVTRANS_STATUS_IO_ERROR) {
        goto roll_back;
    }
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
            if(dss_kvtrans_set_blk_state(kvtrans_ctx, blk->place_value[i].value_index, 
                    blk->place_value[i].num_chunks, EMPTY))
                return KVTRANS_ROLL_BACK_ERROR;
        }
    }

    // rollback meta state
    if(dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx->index, 1, EMPTY))
        return KVTRANS_ROLL_BACK_ERROR;
    return rc;
}

// lookup an EMPTY block and init the block as META/META_DATA_COLLISION
static dss_kvtrans_status_t open_free_blk(void *ctx, uint64_t *col_index) {
    dss_kvtrans_status_t rc;
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

    memset(&col_blk->vctx, 0, sizeof(blk_val_ctx_t));
    col_blk->vctx.value_blocks = _get_num_blocks_required_for_value(req);

    // if(dss_blk_allocator_alloc_blocks_contig(kvtrans_ctx->blk_alloc_ctx, EMPTY, blk_ctx->index + 1,
    //     col_blk->vctx.value_blocks + 1, &col_blk->index) == BLK_ALLOCATOR_STATUS_ERROR) {
    //     // TODO: handle error
    //     return KVTRANS_STATUS_ALLOC_CONTIG_ERROR;
    // }
    
    if(dss_blk_allocator_alloc_blocks_contig(kvtrans_ctx->blk_alloc_ctx, DATA, blk_ctx->index + 1,
        col_blk->vctx.value_blocks + 1, &col_blk->index) == BLK_ALLOCATOR_STATUS_ERROR) {
        // allocate any EMPTY block for META
        if (dss_blk_allocator_alloc_blocks_contig(kvtrans_ctx->blk_alloc_ctx, META, blk_ctx->index + 1,
        1, &col_blk->index) == BLK_ALLOCATOR_STATUS_ERROR) {
            printf("Error: out of spaces");
            exit(1);
        }
    } else {
        // get 4 blocks as DATA
        col_blk->vctx.iscontig = true;
        if(dss_kvtrans_set_blk_state(kvtrans_ctx, col_blk->index, 1, META)) {
            // falied
            if(dss_kvtrans_set_blk_state(kvtrans_ctx, col_blk->index, col_blk->vctx.value_blocks + 1, EMPTY)) {
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

    rc = open_free_blk(ctx, &col_index);
    if (rc) return rc;

    rc = dss_kvtrans_dc_table_insert(kvtrans_ctx, blk_ctx->kctx.dc_index, col_index, blk_ctx->state);

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
                DSS_ASSERT(blk_ctx->state==META_DATA_COLLISION);
                blk->collision_tbl[0].state = DATA_COL_ENTRY;
                blk->num_valid_dc_col_entry++;
            } else {
                if (blk_ctx->state==META) {
                    rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx->index, 1, COLLISION);
                    if (rc) return rc;
                    kvtrans_ctx->stat.meta--;
                    kvtrans_ctx->stat.mc++;
                }
            }
            // update meta blk only
            rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
            rc = KVTRANS_STATUS_SUCCESS;
            break;
        case update:
        case to_delete:
            // unhandled
            DSS_ASSERT(0);
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
            rc = dss_kvtrans_write_ondisk_data(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
            rc = KVTRANS_STATUS_SUCCESS;
            break;
        case to_delete:
            if (blk_ctx->state==META_DATA_COLLISION) {
                // META DATA COLLISION
                DSS_ASSERT(blk_ctx->kctx.dc_index==blk->data_collision_index);
                rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, blk->data_collision_index, blk_ctx->index);
                if (rc==KVTRANS_STATUS_NOT_FOUND) {
                    DSS_ASSERT(blk_ctx->kctx.pindex>0);
                } else if (rc) {
                    return rc;
                }
                kvtrans_ctx->stat.mdc--;
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
                    
        blk_ctx->vctx.value_blocks = _get_num_blocks_required_for_value(req);
        
        rc = _blk_init_value((void *)blk_ctx);
        if (rc) return rc;
        rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
        if (rc != KVTRANS_STATUS_IO_ERROR) rc = KVTRANS_STATUS_SUCCESS;
        rc = dss_kvtrans_write_ondisk_data(blk_ctx, kreq, false);
        if (rc != KVTRANS_STATUS_IO_ERROR) rc = KVTRANS_STATUS_SUCCESS;
        return rc;
    } else {
        first_empty_index = _col_tbl_get_first_entry(blk, false);
        rc = open_free_blk((void *)blk_ctx, &col_index);
        if (rc) return rc;
        if (first_empty_index==-1) {
            DSS_ASSERT(blk->collision_extension_index == 0);
            blk->collision_extension_index = col_index;
            rc = dss_kvtrans_set_blk_state(kreq->kvtrans_ctx, col_index, 1, COLLISION_EXTENSION);
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
            }
            blk->num_valid_col_entry++;
        }
        // only need to update meta blk
        rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
        if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
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
    //          -> MDC              ->-> no change
    //       -> no key or col  
    //          -> COLLISION        ->-> change state to empty
    //          -> MDC              ->-> remove index from dc_tbl, change state to empty
    if (blk->num_valid_col_entry==0 && blk->collision_extension_index==0) {
        if (blk->isvalid) {
            // no need to change state for META_DATA_COLLISION
            if (blk_ctx->state==COLLISION) {
                blk_ctx->state = META;
                rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx->index, 1, META); 
                kvtrans_ctx->stat.mc--;
                kvtrans_ctx->stat.meta++;
            } 
        } else {
            // no key, no col entries, no collision_extension
            if (blk_ctx->state == META_DATA_COLLISION) {
                rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, blk->data_collision_index, blk_ctx->index);
                if (rc==KVTRANS_STATUS_NOT_FOUND) {
                    // The dc_tbl entry has been deleted while deleting the parent blk->
                    DSS_ASSERT(blk_ctx->kctx.pindex>0);
                } else if (rc) {
                    return rc;
                }
                kvtrans_ctx->stat.mdc--;
            } else {
                kvtrans_ctx->stat.mc--;
            }
            blk_ctx->state = EMPTY;
            rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx->index, 1, EMPTY);
            if (rc) return rc;
            rc = dss_kvtrans_delete_ondisk_blk(blk_ctx, kreq);
        }
        if (rc) return rc;
    }
    
    if (col_ctx->state==META_DATA_COLLISION && blk_ctx->blk->num_valid_col_entry>1) {
        blk_ctx->blk->num_valid_dc_col_entry--;
        if (blk_ctx->blk->num_valid_dc_col_entry==0) {
            rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, col_ctx->blk->data_collision_index, col_ctx->index);
            if (rc==KVTRANS_STATUS_NOT_FOUND) {
            // The dc_tbl entry has been deleted while deleting the parent blk->
                DSS_ASSERT(col_ctx->kctx.pindex>0);
            } else if (rc) {
                return rc;
            }
            kvtrans_ctx->stat.mdc--;
            rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx->index, 1, COLLISION);
            kvtrans_ctx->stat.mc++;
        }
    }
    return rc;
}


dss_kvtrans_status_t 
_delete_collision_extension(blk_ctx_t *blk_ctx, 
                            blk_ctx_t *last_ce_ctx,
                            kvtrans_req_t *kreq)
{
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;
    blk_ctx_t *pre_ctx;
    kvtrans_ctx_t *kvtrans_ctx = kreq->kvtrans_ctx;
    if (rc) return rc;
    pre_ctx = TAILQ_PREV(last_ce_ctx, blk_elm, blk_link);
    DSS_ASSERT(pre_ctx!=NULL);
    if (rc) return rc;
    // Col Extension after delete
    // col_ctx   
    //      ->-> no col entries
    //          -> key valid -> keep state.
    //          -> key invalid -> mark entry in pre_ctx invalid
    //              -> find pre of pre_ctx, repeatly check status
    //      ->-> has col entries -> keep state. 
    if (last_ce_ctx->blk->num_valid_col_entry==0 && !last_ce_ctx->blk->isvalid) {
        kvtrans_ctx->stat.ce --;
        pre_ctx->blk->collision_extension_index = 0;
        while (pre_ctx->blk->num_valid_col_entry==0 && 
                pre_ctx->blk->collision_extension_index==0 && 
                !pre_ctx->blk->isvalid) {
            kvtrans_ctx->stat.ce --;
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
    DSS_ASSERT(blk_ctx->state == META_DATA_COLLISION || blk_ctx->state == COLLISION);
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
            rc = dss_kvtrans_write_ondisk_data(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
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
            if (blk_ctx->state==META_DATA_COLLISION &&
                    blk->num_valid_dc_col_entry==0 && 
                    blk->collision_extension_index==0) {
                // no deletion if this is a collision entry
                rc = dss_kvtrans_dc_table_delete(kvtrans_ctx, blk->data_collision_index, blk_ctx->index);
                if (rc==KVTRANS_STATUS_NOT_FOUND) {
                    DSS_ASSERT(blk_ctx->kctx.pindex>0);
                } else if (rc) {
                    return rc;
                }
                rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx->index, 1, COLLISION);
                if (rc) return rc;
                kvtrans_ctx->stat.mdc--;
                kvtrans_ctx->stat.mc++;
            }
            rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
            if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
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
                rc = dss_kvtrans_get_blk_state(kvtrans_ctx, col_blk_ctx);
                if (rc) return rc;
                rc = dss_kvtrans_load_ondisk_blk(col_blk_ctx, kreq, true);
                if (rc) return rc;
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
                rc = dss_kvtrans_load_ondisk_blk(col_blk_ctx, kreq, true);
                rc = _update_kreq_stat_after_io(kreq, rc, COL_EXT_LOADING_DONE, QUEUE_TO_LOAD_COL_EXT);
                return rc;
        } else {
            // collision_extension_index is 0
            if (blk_ctx->first_insert_blk_ctx == blk_ctx ||
                blk_ctx->first_insert_blk_ctx == NULL) {
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

    if (blk_ctx->blk->isvalid && blk_ctx->blk->value_location != INLINE) {
        rc = _blk_del_value(ctx);
        if (rc) return rc;
    }
    
    rc = dss_kvtrans_dc_table_exist(kvtrans_ctx, blk_ctx->index);
    if (rc == KVTRANS_STATUS_SUCCESS) {
        rc = dss_kvtrans_dc_table_update(kvtrans_ctx, blk_ctx->index, EMPTY);
    } else {
        rc = dss_kvtrans_set_blk_state(kvtrans_ctx, blk_ctx->index, 1, EMPTY);
    }

    if (rc) return rc;

    rc = dss_kvtrans_delete_ondisk_blk(blk_ctx, kreq);
    memset(blk_ctx->blk, 0, sizeof(ondisk_meta_t));
    return rc;
}

void _copy_key_and_value(ondisk_meta_t *des, ondisk_meta_t *src) {
    memcpy(des, src, 2006);
}

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
    // COLLISION_EXTENSION
    { &init_meta_data_collision_blk, NULL, NULL},
};

dss_kvtrans_status_t _kvtrans_key_ops(kvtrans_ctx_t *ctx, kvtrans_req_t *kreq)
{
    dss_kvtrans_status_t rc = KVTRANS_STATUS_SUCCESS;

    req_t *req = &kreq->req;
    blk_ctx_t *blk_ctx, *col_ctx, *meta_blk_ctx, *pre_ctx;
    enum kvtrans_req_e prev_state = -1;
    dss_io_task_status_t iot_rc;

    do {
        DSS_DEBUGLOG(DSS_KVTRANS, "Req[%p] prev state [%d] current_state [%d]\n", kreq, prev_state, kreq->state);
        prev_state = kreq->state;
        switch (kreq->state) {
        case REQ_INITIALIZED:
            // only one blk_ctx in the meta_chain
            blk_ctx = TAILQ_FIRST(&kreq->meta_chain);
            DSS_ASSERT(blk_ctx);
            if (kreq->req.opc==KVTRANS_OPC_STORE) {
                blk_ctx->kctx.flag = new_write;
            } else if (kreq->req.opc==KVTRANS_OPC_DELETE) {
                blk_ctx->kctx.flag = to_delete;
            }
            rc = _alloc_entry_block(ctx, kreq, blk_ctx);
            if (rc) return rc;
            break;
        case QUEUE_TO_LOAD_ENTRY:
            //External code should continue progres
            break;
        case ENTRY_LOADING_DONE:
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
            
            if (rc) goto req_terminate;

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
            col_ctx = TAILQ_LAST(&kreq->meta_chain, blk_elm);
            DSS_ASSERT(col_ctx!=NULL && col_ctx!=blk_ctx);
            blk_ctx = TAILQ_PREV(col_ctx, blk_elm, blk_link);
            DSS_ASSERT(blk_ctx->blk!=NULL);
            col_ctx->kreq = kreq;
            col_ctx->kctx.pindex = blk_ctx->index;
            col_ctx->kctx.flag = blk_ctx->kctx.flag;
            col_ctx->vctx.iscontig = false;
            col_ctx->vctx.value_blocks = 0;
            col_ctx->vctx.remote_val_blocks = 0;
            col_ctx->kctx.dc_index = blk_ctx->kctx.dc_index;

            if (col_ctx->state==META_DATA_COLLISION) {
                DSS_ASSERT(col_ctx->kctx.dc_index = col_ctx->blk->data_collision_index);
            } else if (col_ctx->state == META) {
                DSS_ASSERT(col_ctx->blk->num_valid_col_entry==0);
            } else if (col_ctx->state == COLLISION) {
                DSS_ASSERT(col_ctx->blk->num_valid_col_entry>0);
            }

            col_ctx->kctx.ops = g_blk_register[col_ctx->state];

            rc = col_ctx->kctx.ops.update_blk((void *)col_ctx);
            if (rc) goto req_terminate;

            if (blk_ctx->kctx.flag==to_delete) {
                rc = _delete_collision_entry(blk_ctx, col_ctx, kreq);
                if (rc) return rc;
            }
            
            // states of col_ctx has been updated in kctx.ops
            // need to update blk_ctx if it's not empty.
            // TODO: avoid update if blk_ctx has not been modified.
            if (blk_ctx->state!=EMPTY) {
                rc = dss_kvtrans_write_ondisk_blk(blk_ctx, kreq, false);
                if (rc == KVTRANS_STATUS_IO_ERROR) return rc;
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
            col_ctx->kctx.dc_index = blk_ctx->kctx.dc_index;

            if (col_ctx->blk->data_collision_index>0) {
                col_ctx->kctx.dc_index = col_ctx->blk->data_collision_index;
                col_ctx->state = META_DATA_COLLISION;
            } else if (col_ctx->blk->num_valid_col_entry==0) {
                col_ctx->state = META;
            } else {
                col_ctx->state = COLLISION;
            }

            col_ctx->kctx.ops = g_blk_register[col_ctx->state];

            rc = col_ctx->kctx.ops.update_blk((void *)col_ctx);
            if (rc == KVTRANS_STATUS_NOT_FOUND && col_ctx->first_insert_blk_ctx!=NULL) {
                rc = _new_write_ops(col_ctx->first_insert_blk_ctx, kreq);
                if (rc) goto req_terminate;
            } else if (rc) goto req_terminate;
            if (kreq->state == COL_EXT_LOADING_DONE) {
                if (TAILQ_NEXT(col_ctx, blk_link)!=NULL) {
                    // continue to load next col extension
                    kreq->state = COL_EXT_LOADING_CONTIG;
                    break;
                }
                if (col_ctx->kctx.flag==to_delete) {
                    rc =  _delete_collision_extension(blk_ctx, col_ctx, kreq);
                    if (rc) return rc;
                }
                kreq->state = QUEUE_TO_START_IO;
            }
            break;
        case QUEUE_TO_START_IO:
#ifndef DSS_BUILD_CUNIT_TEST
            if (kreq->io_to_queue) {
                iot_rc = dss_io_task_submit(kreq->io_tasks);
                DSS_ASSERT(iot_rc==DSS_IO_TASK_STATUS_SUCCESS);
                break;
            }
#endif
            kreq->state = IO_CMPL;
            break;
        case QUEUED_FOR_DATA_IO:
            //External code should continue progres
            break;
        case IO_CMPL:
            kreq->state = REQ_CMPL;
            // dss_blk_allocator_complete_meta_sync(ctx->blk_alloc_ctx, *kreq->io_tasks);
            break;
        case REQ_CMPL:
            free_kvtrans_req(kreq);
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
    free_kvtrans_req(kreq);
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
        //Assume second call is after IO completion
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
        DSS_DEBUGLOG(DSS_KVTRANS, "Req[%p] prev state [%d] current_state [%d]\n", kreq, prev_state, kreq->state);
        prev_state = kreq->state;
        switch (kreq->state) {
        case REQ_INITIALIZED:
            // only one blk_ctx in the meta_chain
            blk_ctx = TAILQ_FIRST(&kreq->meta_chain);
            // TODO: init blk_ctx with 0
            rc = _alloc_entry_block(ctx, kreq, blk_ctx);
            break;
        case QUEUE_TO_LOAD_ENTRY:
            break;
        case ENTRY_LOADING_DONE:
            blk_ctx = TAILQ_FIRST(&kreq->meta_chain);
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
                case DATA_COLLISION:
                    DSS_ASSERT(0); // will be mdc
                    break;
                case COLLISION:
                case META_DATA_COLLISION:
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
            kreq->state = REQ_CMPL;
            // TODO: execute cb function?
            break;
        case REQ_CMPL:
            free_kvtrans_req(kreq);
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
    printf("    state: %d\n", blk_ctx->state);
    printf("vctx: \n");
    printf("    iscontig: %d\n", blk_ctx->vctx.iscontig);
    printf("    remote_val_blocks: %zu\n", blk_ctx->vctx.remote_val_blocks);
    printf("    value_blocks: %zu\n", blk_ctx->vctx.value_blocks);
    if (!blk_ctx->kreq) printf("kreq is null.\n");
    else {
        printf("kreq: \n");
        printf("    id: %zu\n", blk_ctx->kreq->id);
        printf("    state: %d\n", blk_ctx->kreq->state);
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
