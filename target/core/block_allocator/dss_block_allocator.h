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

#ifndef DSS_BLOCK_ALLOCATOR_H
#define DSS_BLOCK_ALLOCATOR_H

#include <sys/queue.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "apis/dss_block_allocator_apis.h"

//typedef for block allocator APIs to support multiple implementations
typedef dss_blk_allocator_context_t*(*blk_alloc_init_fn)(dss_device_t *device, dss_blk_allocator_opts_t *config);
typedef dss_blk_allocator_status_t (*is_block_free_fn)(dss_blk_allocator_context_t* ctx, uint64_t block_index, bool *is_free);
typedef dss_blk_allocator_status_t (*get_block_state_fn)(dss_blk_allocator_context_t* ctx, uint64_t block_index, uint64_t *block_state);
typedef dss_blk_allocator_status_t (*check_blocks_state_fn)(dss_blk_allocator_context_t *ctx, uint64_t block_index, uint64_t num_blocks, uint64_t block_state, uint64_t *scanned_index);
typedef dss_blk_allocator_status_t (*set_blocks_state_fn)(dss_blk_allocator_context_t* ctx, uint64_t block_index, uint64_t num_blocks,  uint64_t state);
typedef dss_blk_allocator_status_t (*clear_blocks_fn)(dss_blk_allocator_context_t *ctx, uint64_t block_index, uint64_t num_blocks);
typedef dss_blk_allocator_status_t (*alloc_blocks_contig_fn)(dss_blk_allocator_context_t *ctx, uint64_t state, uint64_t hint_block_index, uint64_t num_blocks, uint64_t *allocated_start_block);
typedef dss_blk_allocator_status_t (*print_stats_fn)(dss_blk_allocator_context_t *ctx);
typedef void (*blk_alloc_destroy_fn)(dss_blk_allocator_context_t *ctx);

/**
 * @brief Core in-memory operations needed for block allocator implementation
 * 
 */
typedef struct dss_blk_alloc_core_ops_s {
        blk_alloc_init_fn blk_alloc_init;
        blk_alloc_destroy_fn blk_alloc_destroy;
        is_block_free_fn is_block_free;
        get_block_state_fn get_block_state;
        check_blocks_state_fn check_blocks_state; //Optional to implement in allocator
        set_blocks_state_fn set_blocks_state;
        clear_blocks_fn clear_blocks;
        alloc_blocks_contig_fn alloc_blocks_contig;
        print_stats_fn print_stats;
} dss_blk_alloc_core_ops_t;

typedef dss_blk_allocator_status_t (*blk_alloc_queue_sync_meta_io_tasks_fn)(dss_blk_allocator_context_t *ctx, dss_io_task_t *io_task);
typedef dss_blk_allocator_status_t (*blk_alloc_get_next_submit_meta_io_tasks_fn)(dss_blk_allocator_context_t *ctx, dss_io_task_t **io_task);
typedef dss_blk_allocator_status_t (*blk_alloc_complete_meta_sync_fn)(dss_blk_allocator_context_t *ctx, dss_io_task_t *io_task);
typedef uint64_t (*blk_alloc_get_physical_size_fn)(dss_blk_allocator_opts_t *config);

/**
 * @brief disk operation implementaions needed to support persisting block allocator state
 * 
 */
typedef struct dss_blk_alloc_disk_ops_s {
    blk_alloc_get_physical_size_fn blk_alloc_get_physical_size;
    blk_alloc_queue_sync_meta_io_tasks_fn blk_alloc_queue_sync_meta_io_tasks;
    blk_alloc_get_next_submit_meta_io_tasks_fn blk_alloc_get_next_submit_meta_io_tasks;
    blk_alloc_complete_meta_sync_fn blk_alloc_complete_meta_sync;
} dss_blk_alloc_disk_ops_t;

/**
 * @brief Information provided by block allocator to save provided serialized data on disk
 * 
 */
struct dss_blk_allocator_serialized_s {
	uint8_t *serialized_data_p; //Pointer to data that will be  written to disk
	int64_t  serialized_data_len; //Length of serialized data in bytes
    uint64_t serialized_start_lba; //Actual disk LBA where the serialized data needs to be written
    uint64_t serialized_num_disk_blocks; //Number of disk blocks of serialized data
};

struct dss_blk_alloc_module_s {
    const char *name;
    dss_blk_alloc_core_ops_t core;
    dss_blk_alloc_disk_ops_t disk;

    TAILQ_ENTRY(dss_blk_alloc_module_s) module_list_link;
};
typedef struct dss_blk_alloc_module_s dss_blk_alloc_module_t;

struct dss_block_allocator_context_s {
    dss_blk_allocator_opts_t blk_alloc_opts;
    dss_blk_alloc_module_t *m;
    void *ctx;
};

void dss_block_allocator_add_module(dss_blk_alloc_module_t *m);

dss_blk_alloc_module_t *dss_block_allocator_find_module(const char *name);

#define DSS_BLOCK_ALLOCATOR_REGISTER(name, module) \
void __attribute__((constructor)) _dss_block_allocator_register_##name(void) \
{ \
        dss_block_allocator_add_module(module); \
}

#ifdef __cplusplus
}
#endif

#endif //DSS_BLOCK_ALLOCATOR_H
