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

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "dss.h"
#include "dss_block_allocator.h"

static char simbmap_allocator_name[] = "simbmap_allocator";

#define DSS_SIMBMAP_MAX_SUPPORTED_BLOCK_STATES_BITS (64)
#define BITS_PER_BYTE (8)
#define BYTES_PER_STRIDE (8)

typedef uint64_t __dss_simbmap_bits_per_block_t;//For stride size of 64 bit
#define BYTES_PER_BLOCK (sizeof(__dss_simbmap_bits_per_block_t))

typedef struct dss_blk_alloc_simbmap_ctx_s {
    dss_blk_allocator_context_t ctx_blk_alloc;
    //Allocator Private
    uint64_t *bmap_ptr;
    uint64_t  bmap_sz;//In bytes
    uint64_t  bits_per_block;
    uint64_t  blocks_per_stride;
} dss_blk_alloc_simbmap_ctx_t;

uint64_t dss_simbmap_num_states_to_bits_per_block(uint64_t num_block_states)
{
    uint64_t bits_per_block_state;
    uint64_t mask;

    bits_per_block_state = __builtin_clzl(num_block_states);//Fist occurence of set bit from MSB
    bits_per_block_state = (BYTES_PER_BLOCK * BITS_PER_BYTE) - bits_per_block_state;
    bits_per_block_state = pow(2, (ceil(log(bits_per_block_state)/log(2))));//Round up to next power of 2

    return bits_per_block_state;
}

static inline void dss_simbmap_update_block(dss_blk_alloc_simbmap_ctx_t *c, uint64_t blk_index, uint64_t *state, bool set)
{
    uint64_t stride_index;
    uint64_t block_stride_offset;
    uint64_t block_index_val;
    uint64_t stride_data, stride_state;
    uint64_t mask, shift_index;

    stride_index = blk_index / c->blocks_per_stride;//index into the 64bit region in `bmap_ptr` containing `blk_index`
    block_stride_offset = blk_index % c->blocks_per_stride;//block index offset within the 64 bit region

    // DSS_NOTICELOG("ALL: st index [%d], bso [%d]\n", stride_index, block_stride_offset);

    stride_data = *(c->bmap_ptr + stride_index);//Original data in the 64 bit region

    shift_index = c->bits_per_block * (c->blocks_per_stride - block_stride_offset - 1);
    mask = (~(UINT64_MAX << c->bits_per_block)) << shift_index;

    // DSS_NOTICELOG("ALL: mask [%016llx], shift index [%d]\n", mask, shift_index);
    if(set == true) {//Update `state` for `blk_index`
        stride_state = (*state) << shift_index;//Read 64 bit region
        stride_data = (stride_data & ~mask) | (mask & stride_state);//Update state to blk_index
        *(c->bmap_ptr + stride_index) = stride_data;//Write back the 64 bit region
        // DSS_NOTICELOG("SET: stride state %llx stride data [%llx] bso [%d] stride index [%d]\n", stride_state, stride_data, block_stride_offset, stride_index);
    } else {//Retrieve `state` for `blk_index`
        block_index_val = (stride_data & mask) >> shift_index;//Retrieve state information for `blk_index`
        // DSS_NOTICELOG("GET: stride data [%llx] bso [%d] stride_index [%d]\n", stride_data, block_stride_offset, stride_index);
        *state = block_index_val;
    }
    return;
}

dss_blk_allocator_context_t* dss_blk_allocator_simbmap_init(dss_device_t *device, dss_blk_allocator_opts_t *config)
{
    dss_blk_alloc_simbmap_ctx_t *c = NULL;

    uint64_t bits_per_blk_state;

    DSS_ASSERT(!device);//Persistence not supported by allocator
    if(device) {
        return NULL;
    }

    if(config->num_block_states == 0) {
        return NULL;
    }

    bits_per_blk_state = dss_simbmap_num_states_to_bits_per_block(config->num_block_states);

    if(bits_per_blk_state > DSS_SIMBMAP_MAX_SUPPORTED_BLOCK_STATES_BITS) {
        DSS_ERRLOG("Unsupported num_block_states provided %u\n", config->num_block_states);
        return NULL;
    }

    //shard_size not used in in-memory allocator
    //allocator_block_size not used in in-memory allocator
    //dss_blk_allocator_disk_config_t d not used in in-memory allocator

    c = calloc(1, sizeof(dss_blk_alloc_simbmap_ctx_t));
    if(!c) {
        DSS_ERRLOG("Unable to allocate memory for allocator");
        return NULL;
    }

    c->bmap_sz = config->num_total_blocks;

    if(bits_per_blk_state < BITS_PER_BYTE) { //Cases 1,2,4
        c->bmap_sz /= (BITS_PER_BYTE/ bits_per_blk_state);
    } else { //Cases 8,16,32,64
        c->bmap_sz *= (bits_per_blk_state/BITS_PER_BYTE);
    }

    if(c->bmap_sz % sizeof(uint64_t)) {
        c->bmap_sz += (sizeof(uint64_t) - c->bmap_sz % sizeof(uint64_t));
    }

    c->bits_per_block = bits_per_blk_state;
    DSS_ASSERT(((BYTES_PER_STRIDE * BITS_PER_BYTE)%c->bits_per_block) == 0);
    c->blocks_per_stride = (BYTES_PER_STRIDE * BITS_PER_BYTE)/c->bits_per_block;

    c->bmap_ptr = malloc(c->bmap_sz);

    if(!c->bmap_ptr) {
        free(c);
        return NULL;
    }

    memset(c->bmap_ptr, 0, c->bmap_sz);//Avoid lazy allocation

    DSS_NOTICELOG("Simple In-Memory bitmap Initialized: bits_per_block [%u], blocks_per_stride [%u] Memory allocated [0x%x Bytes]\n", c->bits_per_block, c->blocks_per_stride, c->bmap_sz);
    return &c->ctx_blk_alloc;
}

void dss_blk_allocator_simbmap_destroy(dss_blk_allocator_context_t *ctx)
{
    dss_blk_alloc_simbmap_ctx_t *c = (dss_blk_alloc_simbmap_ctx_t *) ctx;

    DSS_ASSERT(!strcmp(ctx->m->name, simbmap_allocator_name));

    free(c->bmap_ptr);
    memset(c, 0, sizeof(dss_blk_alloc_simbmap_ctx_t));
    free(c);

    return;
}

dss_blk_allocator_status_t dss_blk_allocator_simbmap_is_block_free(dss_blk_allocator_context_t* ctx, uint64_t block_index, bool *is_free)
{
    dss_blk_alloc_simbmap_ctx_t *c = (dss_blk_alloc_simbmap_ctx_t *) ctx;
    uint64_t state;

    dss_simbmap_update_block(c, block_index, &state, false);

    if(state == DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE) {
        *is_free = true;
    } else {
        *is_free = false;
    }
    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t dss_blk_allocator_simbmap_get_block_state(dss_blk_allocator_context_t* ctx, uint64_t block_index, uint64_t *block_state)
{
    dss_blk_alloc_simbmap_ctx_t *c = (dss_blk_alloc_simbmap_ctx_t *) ctx;

    DSS_ASSERT(block_state);

    dss_simbmap_update_block(c, block_index, block_state, false);

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t dss_blk_allocator_simbmap_set_blocks_state(dss_blk_allocator_context_t* ctx, uint64_t block_index, uint64_t num_blocks,  uint64_t state)
{
    dss_blk_alloc_simbmap_ctx_t *c = (dss_blk_alloc_simbmap_ctx_t *) ctx;
    int i;

    for(i=0; i < num_blocks; i++) {
        dss_simbmap_update_block(c, block_index + i, &state, true);
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t dss_blk_allocator_simbmap_clear_blocks(dss_blk_allocator_context_t *ctx, uint64_t block_index, uint64_t num_blocks)
{
    dss_blk_alloc_simbmap_ctx_t *c = (dss_blk_alloc_simbmap_ctx_t *) ctx;
    int i;
    uint64_t cleared_state = DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE;

    for(i=0; i < num_blocks; i++) {
        dss_simbmap_update_block(c, block_index + i, &cleared_state, true);
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

static inline bool __dss_simbmap_check_range_free(dss_blk_alloc_simbmap_ctx_t *c, uint64_t block_index, uint64_t num_blocks, uint64_t *failed_index)
{
    int i;
    uint64_t block_state;
    dss_blk_allocator_status_t rc = BLK_ALLOCATOR_STATUS_SUCCESS;

    for(i=0; i < num_blocks; i++) {
        if(block_index + i >= c->ctx_blk_alloc.blk_alloc_opts.num_total_blocks) {
            return false;
        }
        dss_simbmap_update_block(c, block_index + i, &block_state, false);
        if(block_state == DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE) {
            continue;
        } else {
            if(failed_index) {
                *failed_index = block_index + i;
            }
            return false;
        }
    }
    return true;//All blocks in range are free
}

dss_blk_allocator_status_t dss_blk_allocator_simbmap_alloc_blocks_contig(dss_blk_allocator_context_t *ctx, uint64_t state, uint64_t hint_block_index,
                                             uint64_t num_blocks, uint64_t *allocated_start_block)
{
    dss_blk_alloc_simbmap_ctx_t *c = (dss_blk_alloc_simbmap_ctx_t *) ctx;
    bool range_free;
    uint64_t curr_block_index;
    dss_blk_allocator_status_t rc;

    if(!allocated_start_block) {
        range_free = __dss_simbmap_check_range_free(c, hint_block_index, num_blocks, NULL);
        if(range_free) {
            rc = dss_blk_allocator_simbmap_set_blocks_state(ctx, hint_block_index, num_blocks, state);
            DSS_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
            return rc;
        } else {
            return BLK_ALLOCATOR_STATUS_ERROR;
        }
    }

    curr_block_index = hint_block_index;
    while (curr_block_index < c->ctx_blk_alloc.blk_alloc_opts.num_total_blocks) {
        if(curr_block_index + num_blocks >= c->ctx_blk_alloc.blk_alloc_opts.num_total_blocks) {
            break;
        }
        range_free = __dss_simbmap_check_range_free(c, curr_block_index, num_blocks, &curr_block_index);
        if(range_free == true) {
            *allocated_start_block = curr_block_index;
            goto found_range;
        } else {
            curr_block_index++;
        }
    }

    //Wrap around
    curr_block_index = 0;
    while(curr_block_index < hint_block_index) {
        range_free = __dss_simbmap_check_range_free(c, curr_block_index, num_blocks, &curr_block_index);
        if(range_free == true) {
            *allocated_start_block = curr_block_index;
            goto found_range;
        } else {
            curr_block_index++;
        }
    }

    //Failed to find range
    return BLK_ALLOCATOR_STATUS_ERROR;

found_range:
    rc = dss_blk_allocator_simbmap_set_blocks_state(ctx, *allocated_start_block, num_blocks, state);
    DSS_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
    return rc;
}

//On-disk implementations return Success always. No need to sync anything for in-memory implementation

uint64_t dss_blk_allocator_simbmap_get_physical_size(dss_blk_allocator_context_t *ctx)
{
    dss_blk_alloc_simbmap_ctx_t *c = (dss_blk_alloc_simbmap_ctx_t *) ctx;

    return c->bmap_sz;

}

dss_blk_allocator_status_t dss_blk_allocator_simbmap_queue_sync_meta_io_tasks(dss_blk_allocator_context_t *ctx, dss_io_task_t **io_task)
{
    if(io_task) {
        DSS_ASSERT(!io_task);
        *io_task = NULL;
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t dss_blk_allocator_simbamp_complete_meta_sync(dss_blk_allocator_context_t *ctx, dss_io_task_t **io_task)
{
    DSS_ASSERT(!io_task);

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

/**
 * @brief Simple In Memory BitMAP ALLOCATOR (simbmap_allocator)
 * 
 */
static struct dss_blk_alloc_module_s dss_simbmap_allocator = {
    .name = simbmap_allocator_name,
    .core = {
        .blk_alloc_init = dss_blk_allocator_simbmap_init,
        .blk_alloc_destroy = dss_blk_allocator_simbmap_destroy,
        .is_block_free = dss_blk_allocator_simbmap_is_block_free,
        .get_block_state = dss_blk_allocator_simbmap_get_block_state,
        .check_blocks_state = NULL,
        .set_blocks_state = dss_blk_allocator_simbmap_set_blocks_state,
        .clear_blocks = dss_blk_allocator_simbmap_clear_blocks,
        .alloc_blocks_contig = dss_blk_allocator_simbmap_alloc_blocks_contig,
        .print_stats = NULL
    },
    .disk = {
        .blk_alloc_get_physical_size = dss_blk_allocator_simbmap_get_physical_size,
        .blk_alloc_queue_sync_meta_io_tasks = dss_blk_allocator_simbmap_queue_sync_meta_io_tasks,
        .blk_alloc_get_next_submit_meta_io_tasks = NULL,
        .blk_alloc_complete_meta_sync = dss_blk_allocator_simbamp_complete_meta_sync
    }
};

DSS_BLOCK_ALLOCATOR_REGISTER(simbmap_allocator, &dss_simbmap_allocator);
