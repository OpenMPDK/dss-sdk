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
#include "judy_hashmap.h"
#include <iostream>
#include <cstring>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This namespace includes all block allocator operations
 */

namespace BlockAlloc {

/**
 * Concrete class implementation for counter manager
 */
class CounterManager {
public:
    // Constructor
    CounterManager(uint64_t total_blocks)
        : total_blocks_(total_blocks),
          total_allocated_blocks_(0),
          total_free_blocks_(total_blocks)
    {}

    // Default destructor
    ~CounterManager() = default;

    // API to get allocated blocks on the allocator
    uint64_t get_allocated_blocks() const { return total_allocated_blocks_; }
    // API to get free blocks on the allocator
    uint64_t get_free_blocks() const { return total_free_blocks_; }
    // API to get total blocks managed by allocator
    uint64_t get_total_blocks() const { return total_blocks_; }

    // API only available in the inherited class
protected:
    /**
     * @brief Invoked by inheriting allocator to keep track of allocated
     *        blocks
     * @param allocated_len, length to be accounted
     */
    virtual void record_allocated_blocks(const uint64_t& allocated_len)=0;

    /**
     * @brief Invoked by inheriting allocator to keep track of freed blocks
     *        blocks
     * @param allocated_len, length to be accounted
     */
    virtual void record_freed_blocks(const uint64_t& freed_len)=0;

    uint64_t total_blocks_;
    uint64_t total_allocated_blocks_;
    uint64_t total_free_blocks_;

};

/**
 * Concrete class implementation for Judy Seek Optimization
 */
class JudySeekOptimizer : public CounterManager {
public:
    // Constructor
    JudySeekOptimizer(uint64_t total_blocks,
            uint64_t start_block_offset, uint64_t optimal_ssd_write_kb)
        : CounterManager(total_blocks),
          total_blocks_(total_blocks),
          start_block_offset_(start_block_offset),
          optimal_ssd_write_kb_(optimal_ssd_write_kb),
          jarr_free_lb_(NULL),
          jarr_free_contig_len_(std::make_shared<Utilities::JudyHashMap>())
    {
        // Perform setup operation based on the input to constructor
        if (!init()) {
            // CXX log failed JudySeekOptimizer creation
        }
    }

    // Default destructor
    ~JudySeekOptimizer() {

        int free_bytes = 0;
        // Free judy array `jarr_free_lb_` only if it is not null
        if (jarr_free_lb_ != NULL) {
            free_bytes = JudyLFreeArray(&jarr_free_lb_, PJE0);
            //CXX: Log free bytes freed
            std::cout<<"Seek optimizer free bytes = "
                <<free_bytes<<std::endl;
        }
        // Free judy hash map, `jarr_free_contig_len_`
        //jarr_free_contig_len_->delete_hashmap();
    }

    /**
     * @brief Initializes jarr_free_lb_ and jarr_free_contig_len_
     *        judy array based structures
     *        Invoked from the constructor
     */
    bool init();

    /**
     * @brief Tries to allocate total blocks at an hint lba
     * @param hint_lb, start lb to allocate num_blocks
     * @param num_blocks, total blocks requested to be allocated
     *        at hint_lb (including hint_lb)
     * @param[OUT] allocated_lb, actually lb where the num_blocks 
     *             are allocated
     * @return boolean, `allocated_lb` is only meaningful when the
     *         return is `true`
     */
    bool allocate_lb(const uint64_t& hint_lb,
            const uint64_t& num_blocks, uint64_t& allocated_lb);

    /**
     * @brief Clears a range of lbas, beginning from lb
     * @param lb, start lb to de-allocate num_blocks
     * @param num_blocks, total blocks requested to be deallocated
     *        from lb
     */
    bool free_lb(const uint64_t& lb, const uint64_t& num_blocks);

    /**
     * Debug API to look at jarr_free_lb_ and jarr_free_contig_len_
     */
    void print_map() const;

private:

    /**
     * @brief Checks if a current free chunk is mergable with its
     *        neighbors
     * @param request_lb, lb of the free chunk to be merged
     * @param request_len, length of the free chunk to be merged
     * @param[OUT] prev_neighbor_lb, is the lb of the previous neighbor; if
     *             mergable. This is 0 otherwise
     * @param[OUT] prev_neighbo_len, length of the previous neighbor
     * @param[OUT] next_neighbor_lb, is the lb of the next neighbor; if
     *             mergable. This is 0 otherwise
     * @param[OUT] next_neighbor_len, length of the next neighbor
     * @return boolean, all [OUT] variables only have meaning when true
     */
    bool is_mergable(
            const uint64_t& request_lb,
            const uint64_t& request_len,
            uint64_t& prev_neighbor_lb,
            uint64_t& prev_neighbor_len,
            uint64_t& next_neighbor_lb,
            uint64_t& next_neighbor_len
            ) const;

    /**
     * @brief Removes elements from jarr_free_lb_ and jarr_free_contig_len_
     * @param request_lb, lb number to be removed from jarr_free_lb_
     * @param request_len, len associated with lb to be removed from
     *        jarr_free_contig_len_
     */
    void remove_element(
            const uint64_t& request_lb, const uint64_t& request_len);

    /**
     * @brief Add elements to jarr_free_lb_ and jarr_free_contig_len_
     * @param request_lb, lb to be added to jarr_free_lb_
     * @param request_len, len associated with lb to be added to
     *        jarr_free_contig_len_
     * @return boolean
     */
    bool add_element(
            const uint64_t& request_lb, const uint64_t& request_len);

    /**
     * @brief Checks if the previous chunk on jarr_free_lb_ is allocable
     * @param request_lb, previous lb of this `request_lb` is checked
     * @param request_len, actual len of request that needs to be allocated
     * @param[OUT] neighbor_len, if neighbor found; free len in the previous
     *             chunk
     * @param[OUT] neighbor_lb, if neighbor found; lb of the previous free
     *             chunk
     * @return boolean, neighbor_lb and neighbor_len only have meaning when
     *         return is `true`
     */
    bool is_prev_neighbor_allocable(
            const uint64_t& request_lb,
            const uint64_t& request_len,
            uint64_t& neighbor_lb,
            uint64_t& neighbor_len
            ) const;

    /**
     * @brief Checks if the next chunk on jarr_free_lb_ is allocable
     * @param request_lb, next lb of this `request_lb` is checked
     * @param request_len, actual len of request that needs to be allocated
     * @param[OUT] neighbor_len, if neighbor found; free len in the next
     *             chunk
     * @param[OUT] neighbor_lb, if neighbor found; lb of the next free
     *             chunk
     * @return boolean, neighbor_lb and neighbor_len only have meaning when
     *         return is `true`
     */
    bool is_next_neighbor_allocable(
            const uint64_t& request_lb,
            const uint64_t& request_len,
            uint64_t& neighbor_lb,
            uint64_t& neighbot_len
            ) const;

    /**
     * @brief Splits a free chunk identified by curr_lb for request_lb, len
     * @param curr_lb, lb of the current free chunk
     * @param curr_len, len of the current free chunk
     * @param request_lb, lb of the requested chunk
     * @param request_len, len of the requested chunk
     * @param[OUT] allocated_lb, actual start lb for allocated block
     * @return boolean, `allocated_lb` only has meaning when true
     */
    bool split_curr_chunk(
            const uint64_t& curr_lb,
            const uint64_t& curr_len,
            const uint64_t& request_lb,
            const uint64_t& request_len,
            uint64_t& allocated_lb
            );

    /**
     * @brief Splits a previous free chunk identified by curr_lb for
     *        request_lb, request_len
     * @param prev_lb, lb of the previous free chunk
     * @param prev_len, len of the previous free chunk
     * @param request_len, len of the requested chunk
     * @param[OUT] allocated_lb, actual start lb for allocated block
     * @return boolean, `allocated_lb` only has meaning when true
     */
    bool split_prev_chunk(
            const uint64_t& prev_lb,
            const uint64_t& prev_len,
            const uint64_t& request_len,
            uint64_t& allocated_lb
            );

    /**
     * @brief Splits a next free chunk identified by curr_lb for
     *        request_lb, request_len
     * @param next_lb, lb of the next free chunk
     * @param next_len, len of the next free chunk
     * @param request_len, len of the requested chunk
     * @param[OUT] allocated_lb, actual start lb for allocated block
     * @return boolean, `allocated_lb` only has meaning when true
     */
    bool split_next_chunk(
            const uint64_t& next_lb,
            const uint64_t& next_len,
            const uint64_t& request_len,
            uint64_t& allocated_lb
            );

    /**
     * @brief Merges a free chunk identified by request_lb
     *        with its neighbors
     * @param request_lb, lb of the free chunk
     * @param request_len, length of the free chunk
     * @param next_lb, lb of the next free chunk
     * @param next_len, len of the next free chunk
     * @param prev_lb, lb of the previous chunk
     * @param prev_len, len of the next chunk
     */
    void merge(
            const uint64_t& request_lb,
            const uint64_t& request_len,
            const uint64_t& next_lb,
            const uint64_t& next_len,
            const uint64_t& prev_lb,
            const uint64_t& prev_len
            );

    // CounterManager protected API
    void record_allocated_blocks(const uint64_t& allocated_len) override;
    void record_freed_blocks(const uint64_t& freed_len) override;

    uint64_t total_blocks_;
    uint64_t start_block_offset_;
    uint64_t optimal_ssd_write_kb_;
    void *jarr_free_lb_;
    Utilities::JudyHashMapSharedPtr jarr_free_contig_len_;
};

using JudySeekOptimizerSharedPtr = std::shared_ptr<JudySeekOptimizer>;

/**
 * Abstract class to interface with API in `dss_block_allocator.h`
 */
class Allocator {
public:

    // Default destructor
    virtual ~Allocator() = default;

    /**
     * @brief Destroys and frees any allocated resource for block allocator
     *        context
     */
    virtual void destroy()=0;

    /**
     * @brief Checks if the block state at specified index is free
     *
     * @param block_index index of the block to be looked up
     * @param[OUT] is_free true if state of block at block_index is zero
     *             false otherwise
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    virtual dss_blk_allocator_status_t is_block_free(
            uint64_t block_index,
            bool *is_free)=0;

    /**
     * @brief Retrieves the state of the block at block_index
     *
     * @param block_index index of block whose state should be retrieved
     * @param[OUT] block_state state of the block from zero to
     *             num_block_states
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    virtual dss_blk_allocator_status_t get_block_state(
            uint64_t block_index,
            uint64_t *block_state)=0;
    /**
     * @brief Checks if range of blocks are in the given state
     * 
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
            uint64_t block_index,
            uint64_t num_blocks,
            uint64_t block_state,
            uint64_t *scanned_index)=0;

    /**
     * @brief Sets the range of given blocks with state if range is free
     *
     * @param block_index index of start block whose state should be set
     * @param num_blocks number of blocks the state needs to be set 
     *        starting at block_index
     * @param state state to set for blocks from 1 to num_block_states.
     *        0 is not a valid state to set.
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS
     *         on success or error code otherwise
     */
    virtual dss_blk_allocator_status_t set_blocks_state(
            uint64_t block_index,
            uint64_t num_blocks,
            uint64_t state)=0;
    /**
     * @brief Clears the state of the given range of blocks to 
     *        DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE
     *
     * @param block_index index of start block whose state should be cleared
     * @param num_blocks number of blocks the state needs to be cleared
     *        starting at block_index
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on
     *         success or error code otherwise
     */
    virtual dss_blk_allocator_status_t clear_blocks(
            uint64_t block_index,
            uint64_t num_blocks)=0;
    /**
     * @brief Allocate the given number of contiguous blocks if possible
     *        starting from hint block index
     *
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
    bool init(
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

} // end BlockAlloc Namespace
#ifdef __cplusplus
}
#endif
