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

#include "block_allocator.h"

namespace BlockInterface {

static char block_allocator_name[] = "block_impresario";

typedef struct dss_blk_alloc_impresario_ctx_s {
    dss_blk_allocator_context_t ctx_blk_alloc;
    //Allocator Private
    BlockAlloc::BlockAllocator *impresario_instance;
} dss_blk_alloc_impresario_ctx_t;

dss_blk_allocator_context_t* block_allocator_init(
        dss_device_t *device,
        dss_blk_allocator_opts_t *config) {

    dss_blk_alloc_impresario_ctx_t *c = NULL;

    BlockAlloc::BlockAllocator *ba_i = new BlockAlloc::BlockAllocator();

    if (ba_i == NULL) {
        DSS_ERRLOG(
                "Unable to allocate memory for block impresario instance");
        return NULL;
    }

    if (!ba_i->init(device, config)) {
        DSS_ERRLOG("Block Impresario init failed");
        return NULL;
    }

    c = (dss_blk_alloc_impresario_ctx_t *)
            calloc(1, sizeof(dss_blk_alloc_impresario_ctx_t));
    if (c == NULL) {
        DSS_ERRLOG("Unable to allocate memory for allocator");
        return NULL;
    }
    c->impresario_instance = ba_i;

    DSS_NOTICELOG("Block Impresario Initialized");

    return &c->ctx_blk_alloc;
}

uint64_t get_physical_size(dss_blk_allocator_context_t *ctx) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));

    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return 0;
    }

    // Invoke respective allocator to return its specific persistence
    // size requirements

    return ba_i->allocator->get_physical_size();
}

void block_allocator_destroy(dss_blk_allocator_context_t *ctx)
{
    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));

    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return;
    }
    ba_i->allocator->destroy();
    delete ba_i;
    memset(c, 0, sizeof(dss_blk_alloc_impresario_ctx_t));
    free(c);

    return;
}

dss_blk_allocator_status_t is_block_free(
        dss_blk_allocator_context_t* ctx,
        uint64_t block_index,
        bool *is_free) {
    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));

    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    return ba_i->allocator->is_block_free(block_index, is_free);
}

dss_blk_allocator_status_t get_block_state(
        dss_blk_allocator_context_t* ctx,
        uint64_t block_index,
        uint64_t *block_state) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));

    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    return ba_i->allocator->get_block_state(block_index, block_state);
}

dss_blk_allocator_status_t check_blocks_state(
        dss_blk_allocator_context_t* ctx,
        uint64_t block_index,
        uint64_t num_blocks,
        uint64_t block_state,
        uint64_t *scanned_index) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));

    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    return ba_i->allocator->check_blocks_state(
            block_index, num_blocks, block_state, scanned_index);
}

dss_blk_allocator_status_t set_blocks_state(
        dss_blk_allocator_context_t* ctx,
        uint64_t block_index,
        uint64_t num_blocks,
        uint64_t state) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));
    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    return ba_i->allocator->set_blocks_state(
            block_index, num_blocks, state);
}

dss_blk_allocator_status_t clear_blocks(
        dss_blk_allocator_context_t *ctx,
        uint64_t block_index,
        uint64_t num_blocks) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));
    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    return ba_i->allocator->clear_blocks(block_index, num_blocks);
}

dss_blk_allocator_status_t alloc_blocks_contig(
        dss_blk_allocator_context_t *ctx,
        uint64_t state,
        uint64_t hint_block_index,
        uint64_t num_blocks,
        uint64_t *allocated_start_block) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));
    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    return ba_i->allocator->alloc_blocks_contig(
            state, hint_block_index, num_blocks, allocated_start_block);
}

dss_blk_allocator_status_t print_stats(
        dss_blk_allocator_context_t *ctx) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));
    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    return ba_i->allocator->print_stats();
}

dss_blk_allocator_status_t queue_sync_meta_io_tasks(
                     dss_blk_allocator_context_t *ctx,
                     dss_io_task_t *io_task) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));

    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    return ba_i->queue_sync_meta_io_tasks(ctx, io_task);
}

dss_blk_allocator_status_t get_next_submit_meta_io_tasks(
                     dss_blk_allocator_context_t *ctx,
                     dss_io_task_t *io_task) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));

    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    return ba_i->get_next_submit_meta_io_tasks(ctx, io_task);
}

dss_blk_allocator_status_t complete_meta_sync(
        dss_blk_allocator_context_t *ctx,
        dss_io_task_t *io_task) {

    DSS_ASSERT(!strcmp(ctx->m->name, block_allocator_name));

    dss_blk_alloc_impresario_ctx_t *c =
        (dss_blk_alloc_impresario_ctx_t *)ctx;

    BlockAlloc::BlockAllocator *ba_i = c->impresario_instance;
    if (ba_i == NULL) {
        DSS_ERRLOG("Incorrect usage before block allocator init");
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    return ba_i->complete_meta_sync(ctx, io_task);
}

} // End namespace BlockInterface

/**
 * @brief Block allocator registration with target
 */
struct dss_blk_alloc_module_s dss_block_impresario = {
    .name = BlockInterface::block_allocator_name,
    .core = {
        .blk_alloc_init = BlockInterface::block_allocator_init,
        .blk_alloc_destroy = BlockInterface::block_allocator_destroy,
        .is_block_free = BlockInterface::is_block_free,
        .get_block_state = BlockInterface::get_block_state,
        .check_blocks_state = BlockInterface::check_blocks_state,
        .set_blocks_state = BlockInterface::set_blocks_state,
        .clear_blocks = BlockInterface::clear_blocks,
        .alloc_blocks_contig = BlockInterface::alloc_blocks_contig,
        .print_stats = BlockInterface::print_stats
    },
    .disk = {
        .blk_alloc_get_physical_size =
            BlockInterface::get_physical_size,
        .blk_alloc_queue_sync_meta_io_tasks =
            BlockInterface::queue_sync_meta_io_tasks,
        .blk_alloc_get_next_submit_meta_io_tasks =
            BlockInterface::get_next_submit_meta_io_tasks,
        .blk_alloc_complete_meta_sync =
            BlockInterface::complete_meta_sync
    }
};
