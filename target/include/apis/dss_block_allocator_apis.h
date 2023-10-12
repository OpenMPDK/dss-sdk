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

#ifndef DSS_BLOCK_ALLOCATOR_APIS_H
#define DSS_BLOCK_ALLOCATOR_APIS_H

#include <stdint.h>
#include <stdbool.h>
#include "dss_io_task_apis.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE (0x0)

typedef struct dss_block_allocator_context_s dss_blk_allocator_context_t;
typedef struct dss_blk_allocator_opts_s dss_blk_allocator_opts_t;

struct dss_blk_allocator_disk_config_s {
    uint64_t super_disk_block_index;  //Start LBA for allocator meta on disk
    uint64_t num_super_blocks; //Number of disk blocks used for allocator meta info
    uint64_t disk_block_size; //Disk block size in bytes
                              //This corresponds to the minumum
                              //  block size that can be written to the disk.
    uint64_t allocatable_start_disk_block; //Start offset for blocks managed by the allocator
    uint64_t total_allocatable_disk_blocks; //Total number of blocks managed by the allocator
    uint64_t reserved_data_blocks_start_index; //Start index of reserved data blocks when reserved data block is valid
    uint64_t reserved_data_blocks; //Total count of reserved data/allocatable blocks
};

typedef struct dss_blk_allocator_disk_config_s dss_blk_allocator_disk_config_t;

typedef struct dss_blk_allocator_serialized_s dss_blk_allocator_serialized_t;

typedef enum dss_blk_allocator_status_e {
    BLK_ALLOCATOR_STATUS_ERROR = -1,
    BLK_ALLOCATOR_STATUS_SUCCESS = 0,
    BLK_ALLOCATOR_STATUS_INVALID_BLOCK_INDEX = 1,
    BLK_ALLOCATOR_STATUS_INVALID_BLOCK_RANGE = 2,
    BLK_ALLOCATOR_STATUS_INVALID_BLOCK_STATE = 3,
    BLK_ALLOCATOR_STATUS_ITERATION_END = 4
} dss_blk_allocator_status_t;

/**
 * @brief Configurable options for block allocator
 *
 */
struct dss_blk_allocator_opts_s {
    char *blk_allocator_type; // one of registered block allocator names
    uint64_t num_total_blocks; // - Number of blocks managed by allocator
                               // - This is number of total allocatable block
                               // - Each block is of size allocator_block_size
    uint64_t num_block_states; // - Number of states each block can take excluding cleared state.
                               // - This will correspond to the number of bit required for each block
    uint64_t shard_size; // Optimal write size for high throughput
    uint64_t allocator_block_size;// Size of each block allocated in bytes
    uint64_t logical_start_block_offset; // - Logical block number from which the block_allocator
                                         //   needs to manage `num_total_blocks`
                                         // - This offset will include the size for superblock
                                         //   and size for persistent block allocator data in blocks
                                         //   at the minimum
    dss_blk_allocator_disk_config_t d; // On disk configuration for block allocator
    bool enable_ba_meta_sync; //Enable or disable block allocation meta sync interface to disk
};

/**
 * @brief Initializes and returns a new block allocator context
 *
 * @param device Device handle on which the block allocator needs to be initialized
 * @param config Options struct with input configuration options set
 * @return dss_blk_allocator_context_t* allocated and initialized context pointer
 */
dss_blk_allocator_context_t* dss_blk_allocator_init(dss_device_t *device, dss_blk_allocator_opts_t *config);

/**
 * @brief Sets default parameters for block allocator options
 *
 * @param device The device block allocator will be configured for. Default parameters use the entire disk
 * @param[OUT] opts Pointer to the options struct whose parameters will be set
 */
void dss_blk_allocator_set_default_config(dss_device_t *device, dss_blk_allocator_opts_t *opts);

/**
 * @brief Destroys and frees any allocated resource for block allocator context
 *
 * @param ctx context to deallocate
 */
void dss_blk_allocator_destroy(dss_blk_allocator_context_t *ctx);

/**
 * @brief Update the block allocator opts from serialized data read from disk
 *
 * @param serialized_data Data buffer read from the disk containing block allocator options
 * @param serialized_data_len length of valid serialized data
 * @param[OUT] opts  Pointer to the options struct whose parameters will be set
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_load_opts_from_disk_data(uint8_t *serialized_data, uint64_t serialized_data_len, dss_blk_allocator_opts_t *opts);

//In-Memory APIs

/**
 * @brief Checks if the block state at specified index is free
 *
 * @param ctx block allocator context
 * @param block_index index of the block to be looked up
 * @param[OUT] is_free true if state of block at block_index is zero false otherwise
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_is_block_free(dss_blk_allocator_context_t* ctx, uint64_t block_index, bool *is_free);

/**
 * @brief Retrieves the state of the block at block_index
 *
 * @param ctx block allocator context
 * @param block_index index of block whose state should be retrieved
 * @param[OUT] block_state state of the block from zero to num_block_states
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_get_block_state(dss_blk_allocator_context_t* ctx, uint64_t block_index, uint64_t *block_state);

/**
 * @brief Checks if range of blocks are in the given state
 * 
 * @param ctx block allocator context
 * @param block_index index of start block whose state should be verified
 * @param num_blocks number of blocks the state needs to be verified starting at block_index
 * @param block_state state of the block from zero to num_block_states
 * @param[OUT] scanned_index block_index where `block_state` was seen last. `(block_index - 1)` if the state at block_index does not match.
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_check_blocks_state(dss_blk_allocator_context_t *ctx, uint64_t block_index, uint64_t num_blocks, uint64_t block_state, uint64_t *scanned_index);

/**
 * @brief Sets the range of given blocks with state if range is free
 *
 * @param ctx block allocator context
 * @param block_index index of start block whose state should be set
 * @param num_blocks number of blocks the state needs to be set starting at block_index
 * @param state state to set for blocks from 1 to num_block_states. 0 is not a valid state to set.
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_set_blocks_state(dss_blk_allocator_context_t* ctx, uint64_t block_index, uint64_t num_blocks,  uint64_t state);

/**
 * @brief Clears the state of the given range of blocks to DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE
 *
 * @param ctx block allocator context
 * @param block_index index of start block whose state should be cleared
 * @param num_blocks number of blocks the state needs to be cleared starting at block_index
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_clear_blocks(dss_blk_allocator_context_t *ctx, uint64_t block_index, uint64_t num_blocks);

/**
 * @brief Allocate the given number of contiguous blocks if possible starting from hint block index
 *
 * @param ctx block allocator context
 * @param state state to set for blocks from 1 to num_block_states. 0 is not a valid state to set.
 * @param hint_block_index hint for sequential allocation or allocate within segment
 * @param num_blocks number of contiguos blocks that needs to be allocated
 * @param[out] allocated_start_block Return allocated start block. If set to NULL fail allocation if not able to allocate at hint block index.
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_alloc_blocks_contig(dss_blk_allocator_context_t *ctx, uint64_t state, uint64_t hint_block_index,
                                             uint64_t num_blocks, uint64_t *allocated_start_block);

/**
 * @brief Print block allocator statistics to standard out
 *
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_print_stats();

//On-Disk APIs

/**
 * @brief Query block allocator to obtain the size of persistent memory required for managing operations.
 *        This is the on-disk size of all persistent data-structures used by block allocator.
 *        Ideally invoked after setting up block allocator (init) and required for persistence.
 * 
 * @param config Options struct with input configuration options set
 * @return physical size in bytes, required to be persisted
 */
uint64_t dss_blk_allocator_get_physical_size(dss_blk_allocator_opts_t *config);


/**
 * @brief Generate a list of meta io tasks corresponding to the dirty segments and queue the given io_task
 *
 * @param ctx block allocator context
 * @param[OUT] io_task io task that needs to be populated to be queued and forwarded to io module
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_queue_sync_meta_io_tasks(dss_blk_allocator_context_t *ctx, dss_io_task_t *io_task);

/**
 * @brief -Get IO tasks to be submitted that can be scheduled to IO module without overlap of block allocator
 *         meta-data updates
 *        - This can be invoked until there are no more tasks to be submitted to IO module
 * @param ctx block allocator context
 * @param[OUT] io_task io task that needs to be populated to be queued and forwarded to io module
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or BLK_ALLOCATOR_STATUS_ITERATION_END on no available
 *         IO tasks to be scheduled to IO module or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_get_next_submit_meta_io_tasks(dss_blk_allocator_context_t *ctx, dss_io_task_t **io_task);

/**
 * @brief Update block allocator of meta IO completion
 *
 * @param ctx block allocator context
 * @param io_task completing io task corresponding to previously generated meta disk IO
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_complete_meta_sync(dss_blk_allocator_context_t *ctx, dss_io_task_t *io_task);

/**
 * @brief Setup io tasks to erase super block for block allocator instance
 *
 * @param ctx block allocator context
 * @param io_task io task that needs to be populated to be forwarded to io module
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_setup_erase_super_block_io_tasks(dss_blk_allocator_context_t *ctx, dss_io_task_t **io_task);

/**
 * @brief Update block allocator of super block erase completion
 *
 * @param ctx block allocator context
 * @param io_task completing io task corresponding to previously generated super block erase disk IO
 * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on success or error code otherwise
 */
dss_blk_allocator_status_t dss_blk_allocator_complete_erase_super_block(dss_blk_allocator_context_t *ctx, dss_io_task_t *io_task);

#ifdef __cplusplus
}
#endif

#endif //DSS_BLOCK_ALLOCATOR_APIS_H
