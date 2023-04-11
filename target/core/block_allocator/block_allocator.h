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

#pragma once
#include "dss.h"
#include "dss_block_allocator.h"
#include <iostream>
#include <cstring>
#include <memory>

/**
 * This namespace includes all block allocator operations
 */

namespace BlockAlloc {

/**
 * Abstract class to interface with API in `dss_block_allocator.h`
 */
class Allocator{
public:

    // Default destructor
    virtual ~Allocator() = default;

    /**
     * @brief Destroys and frees any allocated resource for block allocator
     *        context
     *
     * @param ctx context to deallocate
     */
    virtual void destroy(dss_blk_allocator_context_t *ctx)=0;

    /**
     * @brief Checks if the block state at specified index is free
     *
     * @param ctx block allocator context
     * @param block_index index of the block to be looked up
     * @param[OUT] is_free true if state of block at block_index is zero
     *             false otherwise
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    virtual dss_blk_allocator_status_t is_block_free(
            dss_blk_allocator_context_t *ctx,
            uint64_t block_index,
            bool *is_free)=0;

    /**
     * @brief Retrieves the state of the block at block_index
     *
     * @param ctx block allocator context
     * @param block_index index of block whose state should be retrieved
     * @param[OUT] block_state state of the block from zero to
     *             num_block_states
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    virtual dss_blk_allocator_status_t get_block_state(
            dss_blk_allocator_context_t* ctx,
            uint64_t block_index,
            uint64_t *block_state)=0;
    /**
     * @brief Checks if range of blocks are in the given state
     * 
     * @param ctx block allocator context
     * @param block_index index of start block whose state should be verified
     * @param num_blocks number of blocks the state needs to be verified
     *        starting at block_index
     * @param block_state state of the block from zero to num_block_states
     * @param[OUT] scanned_index block_index where `block_state` was seen
     *             last. `(block_index - 1)` if the state at block_index
     *             does not match.
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    virtual dss_blk_allocator_status_t check_blocks_state(
            dss_blk_allocator_context_t *ctx,
            uint64_t block_index,
            uint64_t num_blocks,
            uint64_t block_state,
            uint64_t *scanned_index)=0;

    /**
     * @brief Sets the range of given blocks with state if range is free
     *
     * @param ctx block allocator context
     * @param block_index index of start block whose state should be set
     * @param num_blocks number of blocks the state needs to be set 
     *        starting at block_index
     * @param state state to set for blocks from 1 to num_block_states.
     *        0 is not a valid state to set.
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS
     *         on success or error code otherwise
     */
    virtual dss_blk_allocator_status_t set_blocks_state(
            dss_blk_allocator_context_t* ctx,
            uint64_t block_index,
            uint64_t num_blocks,
            uint64_t state)=0;
    /**
     * @brief Clears the state of the given range of blocks to 
     *        DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE
     *
     * @param ctx block allocator context
     * @param block_index index of start block whose state should be cleared
     * @param num_blocks number of blocks the state needs to be cleared
     *        starting at block_index
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    virtual dss_blk_allocator_status_t clear_blocks(
            dss_blk_allocator_context_t *ctx,
            uint64_t block_index,
            uint64_t num_blocks)=0;
    /**
     * @brief Allocate the given number of contiguous blocks if possible
     *        starting from hint block index
     *
     * @param ctx block allocator context
     * @param state state to set for blocks from 1 to num_block_states.
     *        0 is not a valid state to set.
     * @param hint_block_index hint for sequential allocation or allocate
     *        within segment
     * @param num_blocks number of contiguos blocks that needs to be
     *        allocated
     * @param[out] allocated_start_block Return allocated start block. If
     *             set to NULL fail allocation if not able to allocate at
     *             hint block index.
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS 
     *         on success or error code otherwise
     */
    virtual dss_blk_allocator_status_t alloc_blocks_contig(
            dss_blk_allocator_context_t *ctx,
            uint64_t state,
            uint64_t hint_block_index,
            uint64_t num_blocks,
            uint64_t *allocated_start_block)=0;
};

using AllocatorSharedPtr = std::shared_ptr<Allocator>;

class BlockAllocator{
public:
    // Constructor of the class
    explicit BlockAllocator(){}
    /**
     * @brief Initializes and returns a new block allocator context
     *
     * @param device Device handle on which the block allocator needs to be
     *        initialized
     * @param config Options struct with input coniguration options set
     * @return dss_blk_allocator_context_t* allocated and initialized
     *         context pointer
     */
    dss_blk_allocator_context_t* init(
            dss_device_t *device,
            dss_blk_allocator_opts_t *config);

    /**
     * @brief Sets default parameters for block allocator options
     *
     * @param device The device block allocator will be configured for
     *        Default parameters use the entire disk
     * @param[OUT] opts Pointer to the options struct whose parameters 
     *        will be set
     */
    void set_default_config(
            dss_device_t *device,
            dss_blk_allocator_opts_t *opts);


    /**
     * @brief
     *
     */
    dss_blk_allocator_status_t load_opts_from_disk_data(
            uint8_t *serialized_data,
            uint64_t serialized_data_len,
            dss_blk_allocator_opts_t *opts);
    //On-Disk APIs

    /**
     * @brief Generate a list of meta io tasks corresponding to the dirty
     *        segments and update the given io_task
     *
     * @param ctx block allocator context
     * @param[OUT] io_task io task that needs to be populated to be
     *             forwarded to io module
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    dss_blk_allocator_status_t get_sync_meta_io_tasks(
            dss_blk_allocator_context_t *ctx,
            dss_io_task_t **io_task);

    /**
     * @brief Update block allocator of meta IO completion
     *
     * @param ctx block allocator context
     * @param io_task completing io task corresponding to previously
     *        generated meta disk IO
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    dss_blk_allocator_status_t complete_meta_sync(
            dss_blk_allocator_context_t *ctx,
            dss_io_task_t *io_task);

    /**
     * @brief Setup io tasks to erase super block for block allocator
     *        instance
     *
     * @param ctx block allocator context
     * @param io_task io task that needs to be populated to be forwarded to
     *        io module
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    dss_blk_allocator_status_t setup_erase_super_block_io_tasks(
            dss_blk_allocator_context_t *ctx,
            dss_io_task_t **io_task);

    /**
     * @brief Update block allocator of super block erase completion
     *
     * @param ctx block allocator context
     * @param io_task completing io task corresponding to previously
     *        generated super block erase disk IO
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    dss_blk_allocator_status_t complete_erase_super_block(
            dss_blk_allocator_context_t *ctx,
            dss_io_task_t *io_task);

    AllocatorSharedPtr allocator;

};

using BlockAllocatorSharedPtr = std::shared_ptr<BlockAllocator>;

} // end BlockAlloc Namespace
