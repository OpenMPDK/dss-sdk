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

#include "dss.h"
#include "dss_block_allocator.h"

extern dss_blk_alloc_module_t dss_block_impresario;
DSS_BLOCK_ALLOCATOR_REGISTER(block_impresario, &dss_block_impresario);

#define BLK_ALLOCATOR_DEFAULT_TYPE "simbmap_allocator"
#define BLK_ALLOCATOR_DEFAULT_NUM_BLOCK_STATES (4)
#define BLK_ALLOCATOR_DEFAULT_ALLOCATOR_BLOCK_SIZE (4096)
#define BLK_ALLOCATOR_DEFAULT_SHARD_SIZE (131072)
#define BLK_ALLOCATOR_DEFAULT_LOGICAL_START_BLOCK_OFFSET (0)
#define BLK_ALLOCATOR_DEFAULT_SB_INDEX (0)
#define BLK_ALLOCATOR_DEFAULT_NUM_SUPER_BLOCKS (8192)
#define BLK_ALLOCATOR_DEFAULT_DISK_BLOCK_SIZE (4096)
#define BLK_ALLOCATOR_DEFAULT_RESVD_BLOCKS (0)
#define BLK_ALLOCATOR_DEFAULT_RSVD_START_BLOCK_INDEX (-1)


struct dss_blk_alloc_mgr_s {
    TAILQ_HEAD(,dss_blk_alloc_module_s) blk_alloc_modules;
};

static struct dss_blk_alloc_mgr_s g_blk_alloc_mgr = {
    .blk_alloc_modules = TAILQ_HEAD_INITIALIZER(g_blk_alloc_mgr.blk_alloc_modules)
};

dss_blk_alloc_module_t *dss_block_allocator_find_module(const char *name)
{
    dss_blk_alloc_module_t *m = NULL;

    TAILQ_FOREACH(m, &g_blk_alloc_mgr.blk_alloc_modules, module_list_link) {
        if(strcmp(name, m->name) == 0) {
            break;
        }
    }
    return m;
}

void dss_block_allocator_add_module(dss_blk_alloc_module_t *m)
{
    if(dss_block_allocator_find_module(m->name)) {
        DSS_ERRLOG("Block allocator type %s is already registered\n", m->name);
        DSS_RELEASE_ASSERT(0);//Cannot add duplicate module
    }

    //In-Memory functions
    DSS_ASSERT(m->core.blk_alloc_init);
    DSS_ASSERT(m->core.blk_alloc_destroy);
    DSS_ASSERT(m->core.is_block_free);
    DSS_ASSERT(m->core.get_block_state);
    //Optional to implement: m->core.check_blocks_state
    DSS_ASSERT(m->core.set_blocks_state);
    DSS_ASSERT(m->core.alloc_blocks_contig);
    //Optional to implement: m->core.print_stats
    DSS_ASSERT(m->core.clear_blocks);

    //On-Disk functions
    DSS_ASSERT(m->disk.blk_alloc_get_physical_size);
    DSS_ASSERT(m->disk.blk_alloc_get_sync_meta_io_tasks);
    DSS_ASSERT(m->disk.blk_alloc_complete_meta_sync);

    TAILQ_INSERT_TAIL(&g_blk_alloc_mgr.blk_alloc_modules, m, module_list_link);

    return;
}

static inline bool dss_block_allocator_is_block_index_valid(dss_blk_allocator_context_t*c, uint64_t bindex)
{
    return (((bindex >= c->blk_alloc_opts.logical_start_block_offset) && \
            (bindex < c->blk_alloc_opts.num_total_blocks + c->blk_alloc_opts.logical_start_block_offset))? true : false);
}

static inline bool dss_block_allocator_is_block_range_valid(dss_blk_allocator_context_t*c, uint64_t bindex, uint64_t num_blocks)
{
    uint64_t req_last_lb = bindex + num_blocks - 1;
    uint64_t blk_allocator_last_lb = c->blk_alloc_opts.num_total_blocks + c->blk_alloc_opts.logical_start_block_offset - 1;

    return (((bindex >= c->blk_alloc_opts.logical_start_block_offset) && \
                (bindex <= blk_allocator_last_lb) && \
                (req_last_lb <= blk_allocator_last_lb) && \
                (bindex  < (bindex + num_blocks)))? true : false);
}

static inline bool dss_block_allocator_is_block_state_valid(dss_blk_allocator_context_t*c, uint64_t state)
{
    if(state > c->blk_alloc_opts.num_block_states) {
        return false;
    }
    return true;
}

dss_blk_allocator_context_t* dss_blk_allocator_init(dss_device_t *device, dss_blk_allocator_opts_t *config)
{
    dss_blk_allocator_context_t *c = NULL;
    dss_blk_alloc_module_t *m;

    if(!config->blk_allocator_type) {
        DSS_ERRLOG("Block allocator type name not provided\n");
        return NULL;
    }

    m = dss_block_allocator_find_module(config->blk_allocator_type);
    if(!m) {
        DSS_ERRLOG("Block allocator type --[%s]-- not found\n", config->blk_allocator_type);
        return NULL;
    }

    //TODO: Validate Config
    //TODO: Use *device

    c = m->core.blk_alloc_init(device, config);
    if(!c) {
        DSS_ERRLOG("Failed to initialize block allocator\n");
        return NULL;
    }

    c->m = m;
    c->blk_alloc_opts = *config;

    return c;
}

void dss_blk_allocator_set_default_config(dss_device_t *device, dss_blk_allocator_opts_t *opts)
{
    opts->blk_allocator_type   = BLK_ALLOCATOR_DEFAULT_TYPE;
    opts->num_block_states     = BLK_ALLOCATOR_DEFAULT_NUM_BLOCK_STATES;
    opts->allocator_block_size = BLK_ALLOCATOR_DEFAULT_ALLOCATOR_BLOCK_SIZE;
    opts->shard_size           = BLK_ALLOCATOR_DEFAULT_SHARD_SIZE;
    opts->logical_start_block_offset = BLK_ALLOCATOR_DEFAULT_LOGICAL_START_BLOCK_OFFSET;
    //TODO: Set default total blocks querying disk
    // opts->num_total_blocks =
    opts->d.disk_block_size   = BLK_ALLOCATOR_DEFAULT_DISK_BLOCK_SIZE;//TODO: Query from disk
    opts->d.super_disk_block_index = BLK_ALLOCATOR_DEFAULT_SB_INDEX;
    opts->d.num_super_blocks = BLK_ALLOCATOR_DEFAULT_NUM_SUPER_BLOCKS;//TODO: Calulate this value
    //TODO: Calculate default allocatable disk block based on disk size
    // opts->d.total_allocatable_disk_blocks =
    //TODO: Calculate this value
    // opts->d.allocatable_start_disk_block = 
    opts->d.reserved_data_blocks_start_index = BLK_ALLOCATOR_DEFAULT_RSVD_START_BLOCK_INDEX;
    opts->d.reserved_data_blocks = BLK_ALLOCATOR_DEFAULT_RESVD_BLOCKS;

    return;
}

void dss_blk_allocator_destroy(dss_blk_allocator_context_t *ctx)
{
    DSS_ASSERT(ctx->m->core.blk_alloc_destroy);

    ctx->m->core.blk_alloc_destroy(ctx);

    return;
}

dss_blk_allocator_status_t dss_blk_allocator_load_opts_from_disk_data(uint8_t *serialized_data, uint64_t serialized_data_len, dss_blk_allocator_opts_t *opts)
{
    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t dss_blk_allocator_is_block_free(dss_blk_allocator_context_t* ctx, uint64_t block_index, bool *is_free)
{
    DSS_ASSERT(ctx->m->core.is_block_free);

    if(!dss_block_allocator_is_block_index_valid(ctx, block_index)) {
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_INDEX;
    }

    return ctx->m->core.is_block_free(ctx, block_index, is_free);
}

dss_blk_allocator_status_t dss_blk_allocator_get_block_state(dss_blk_allocator_context_t* ctx, uint64_t block_index, uint64_t *block_state)
{
    DSS_ASSERT(ctx->m->core.get_block_state);

    if(!dss_block_allocator_is_block_index_valid(ctx, block_index)) {
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_INDEX;
    }

    return ctx->m->core.get_block_state(ctx, block_index, block_state);
}

dss_blk_allocator_status_t dss_blk_allocator_check_blocks_state(dss_blk_allocator_context_t *ctx, uint64_t block_index, uint64_t num_blocks, uint64_t block_state, uint64_t *scanned_index)
{
    int i;
    uint64_t ret_blk_state;
    dss_blk_allocator_status_t rc;

    DSS_ASSERT(scanned_index);

    if(!dss_block_allocator_is_block_range_valid(ctx, block_index, num_blocks)) {
        *scanned_index = block_index - 1;
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_RANGE;
    }

    if(!dss_block_allocator_is_block_state_valid(ctx, block_state)) {
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_STATE;
    }

    if(ctx->m->core.check_blocks_state) {
        return ctx->m->core.check_blocks_state(ctx, block_index, num_blocks, block_state, scanned_index);
    } else {
        DSS_ASSERT(ctx->m->core.get_block_state);
        *scanned_index = block_index - 1;
        for(i=0; i < num_blocks; i++) {
            rc = ctx->m->core.get_block_state(ctx, (block_index + i), &ret_blk_state);
            if(rc == BLK_ALLOCATOR_STATUS_SUCCESS && (ret_blk_state == block_state)) {
                *scanned_index = *scanned_index + 1;
                continue;
            } else {
                rc = BLK_ALLOCATOR_STATUS_ERROR;
                break;
            }
        }
        return rc;
    }
}

dss_blk_allocator_status_t dss_blk_allocator_set_blocks_state(dss_blk_allocator_context_t* ctx, uint64_t block_index, uint64_t num_blocks,  uint64_t state)
{
    DSS_ASSERT(ctx->m->core.set_blocks_state);

    if(!dss_block_allocator_is_block_range_valid(ctx, block_index, num_blocks)) {
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_RANGE;
    }

    if(state == DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE) {
        //API cannot clear state using set
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_STATE;
    }

    if(!dss_block_allocator_is_block_state_valid(ctx, state)) {
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_STATE;
    }

    return ctx->m->core.set_blocks_state(ctx, block_index, num_blocks, state);
}

dss_blk_allocator_status_t dss_blk_allocator_clear_blocks(dss_blk_allocator_context_t *ctx, uint64_t block_index, uint64_t num_blocks)
{
    DSS_ASSERT(ctx->m->core.clear_blocks);

    if(!dss_block_allocator_is_block_range_valid(ctx, block_index, num_blocks)) {
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_RANGE;
    }

    return ctx->m->core.clear_blocks(ctx, block_index, num_blocks);
}

dss_blk_allocator_status_t dss_blk_allocator_alloc_blocks_contig(dss_blk_allocator_context_t *ctx, uint64_t state, uint64_t hint_block_index,
                                             uint64_t num_blocks, uint64_t *allocated_start_block)
{
    DSS_ASSERT(ctx->m->core.alloc_blocks_contig);

    if(!dss_block_allocator_is_block_index_valid(ctx, hint_block_index)) {
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_INDEX;
    }

    return ctx->m->core.alloc_blocks_contig(ctx, state, hint_block_index, num_blocks, allocated_start_block);
}

dss_blk_allocator_status_t dss_blk_allocator_print_stats(dss_blk_allocator_context_t *ctx)
{
    // This could be optional
    if (ctx->m->core.print_stats != NULL) {
        return ctx->m->core.print_stats(ctx);
    } else {
        return BLK_ALLOCATOR_STATUS_ERROR; 
    }
}

uint64_t
dss_blk_allocator_get_physical_size(dss_blk_allocator_context_t *ctx) {

    DSS_ASSERT(ctx->m->disk.blk_alloc_get_physical_size);

    return ctx->m->disk.blk_alloc_get_physical_size(ctx);

}

dss_blk_allocator_status_t dss_blk_allocator_get_sync_meta_io_tasks(dss_blk_allocator_context_t *ctx, dss_io_task_t **io_task)
{
    DSS_ASSERT(ctx->m->disk.blk_alloc_get_sync_meta_io_tasks);

    if(!io_task) {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    return ctx->m->disk.blk_alloc_get_sync_meta_io_tasks(ctx, io_task);
}

dss_blk_allocator_status_t dss_blk_allocator_complete_meta_sync(dss_blk_allocator_context_t *ctx, dss_io_task_t *io_task)
{
    DSS_ASSERT(ctx->m->disk.blk_alloc_complete_meta_sync);

    return ctx->m->disk.blk_alloc_complete_meta_sync(ctx, io_task);
}
