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
#include "apis/dss_io_task_apis.h"

#include "judy_hashmap.h"
#include <iostream>
#include <cstring>
#include <memory>
#include <functional>
#include <vector>

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
            uint64_t logical_start_block_offset,
            uint64_t optimal_ssd_write_kb)
        : CounterManager(total_blocks),
          total_blocks_(total_blocks),
          logical_start_block_offset_(logical_start_block_offset),
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

    // CounterManager API
    void record_allocated_blocks(const uint64_t& allocated_len) override;
    void record_freed_blocks(const uint64_t& freed_len) override;

    uint64_t total_blocks_;
    uint64_t logical_start_block_offset_;
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
     * @brief - Query block allocator to obtain the size of persistent
     *          memory required for managing operations.
     *        - This is the on-disk size of all persistent data-structures
     *          used by block allocator.
     *        - Ideally invoked after setting up block allocator (init)
     *          and required for persistence.
     *        - Since this size changes with the type of allocator, this is
     *          also virtual and needs to be supported by any allocator
     * 
     * @return physical size in bytes, required to be persisted
     */
    virtual uint64_t get_physical_size()=0;

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
    /**
     * @brief Translate allocated meta lba to drive data for block
     *        allocator meta-data
     *
     * @param meta_lba lba allocated by block allocator, which is associated
     *        some meta data on the block allocator
     * @param meta_num_blocks total number of blocks associated with
     *        meta_lba
     * @param drive_smallest_block_size Drive block size in bytes, that is
     *        needed for translating meta_lb to drive_blk_addr, since start
     *        block offset is already available
     * @param[out] drive_blk_addr Actual drive lba associated with block
     *             allocator meta-data corresponding to meta_lba
     * @param[out] drive_num_blocks total number of blocks associated with
     *             drive_blk_addr
     * @param[out] serialized_drive_data Actual block allocator meta-data
     *             persisted to disk
     * @param[out] serialized_len, length of serialized_drive_data in bytes
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS 
     *         on success or error code otherwise
     */
    virtual dss_blk_allocator_status_t translate_meta_to_drive_data(
            uint64_t meta_lba,
            uint64_t meta_num_blocks,
            uint64_t drive_smallest_block_size,
            uint64_t logical_block_size,
            uint64_t& drive_blk_lba,
            uint64_t& drive_num_blocks,
            void** serialized_drive_data,
            uint64_t& serialized_len)=0;
    /**
     * @brief print block allocator stats to standard out
     */
    virtual dss_blk_allocator_status_t print_stats()=0;
};

using AllocatorSharedPtr = std::shared_ptr<Allocator>;

/**
 * Concrete class for ordering Bitmap meta-data ordering
 */
class IoTaskOrderer {
public:
    // Constructor of the class
    explicit IoTaskOrderer(
            uint64_t drive_smallest_block_size,
            uint64_t max_dirty_segments,
            dss_device_t *device)
        : translate_meta_to_drive_data(nullptr),
          drive_smallest_block_size_(drive_smallest_block_size),
          max_dirty_segments_(max_dirty_segments),
          io_device_(device),
          dirty_meta_data_(max_dirty_segments_, std::make_pair(0,0)),
          io_ranges_(max_dirty_segments_, std::make_pair(0,0)),
          dirty_counter_(0),
          jarr_io_dev_guard_(nullptr)
    {}

    // Default destructor
    ~IoTaskOrderer() = default;

    dss_blk_allocator_status_t mark_dirty_meta(
            uint64_t lba, uint64_t num_blocks);

    /**
     * Allocator specific translator implementation
     */
    std::function<dss_blk_allocator_status_t(
            uint64_t meta_lba,
            uint64_t meta_num_blocks,
            uint64_t drive_smallest_block_size,
            uint64_t logical_block_size,
            uint64_t& drive_blk_lba,
            uint64_t& drive_num_blocks,
            void** serialized_drive_data,
            uint64_t& serialized_len)> 
        translate_meta_to_drive_data;

    /**
     * @brief API to interface with KV-Translator layer to queue
     *        IO requests to synchronize with block allocator
     *        meta-data
     */
    dss_blk_allocator_status_t queue_sync_meta_io_tasks(
            dss_io_task_t* io_task);
    /**
     * @brief - API to interface with KV-Translator layer to acquire
     *          the next IO request that can be scheduled to IO module
     *        - This IO task ensures that there is no overlap of bitmap
     *          meta-data associated with the request
     */
    dss_blk_allocator_status_t get_next_submit_meta_io_tasks(
            dss_io_task_t* io_task);

    /**
     * @brief - API to interface with KV-Translator layer to mark
     *          completed IO.
     *        - Issued when IO module invokes completion callback on
     *          KV-Translator
     */
    dss_blk_allocator_status_t complete_meta_sync(
            dss_io_task_t* io_task);

    // Getter for io device reference
    dss_device_t** get_io_device() {
        return &io_device_;
    }

private:

    /**
     * @brief API to populate IO ranges associated with an IO task
     *
     * @param io_task, task associated with dirty block allocator meta-data
     * @param[out] io_ranges, lb-len tuple or dirty meta-data to be added
     *             to io_task
     * @param[out] num_ranges, total dirty tuples 
     */
    void populate_io_ranges(dss_io_task_t* io_task,
            std::vector<std::pair<uint64_t,uint64_t>>& io_ranges,
            uint64_t& num_ranges, bool is_completion);

    /**
     * @brief API to check IO range overlap with current in-flight IO range
     * 
     * @param drive_lba, lba to check overlap
     * @return boolean,  True on overlap
     */
    bool is_current_overlap(const uint64_t& drive_lba) const;

    /**
     * @brief API to check IO range overlap with previous in-flight IO range
     * 
     * @param drive_lba, lba to check overlap
     * @return boolean,  True on overlap
     */
    bool is_prev_neighbor_overlap(const uint64_t& drive_lba) const;

    /**
     * @brief API to check IO range overlap with next in-flight IO range
     *
     * @param drive_lba, lba to check overlap
     * @param drive_num_blocks, total blocks starting with `lba` block
     * @return boolean,  True on overlap
     */
    bool is_next_neighbor_overlap(
            const uint64_t& drive_lba,
            const uint64_t& drive_num_blocks) const;

    /**
     * @brief API to check individual IO range overlap
     *
     * @param drive_lba, lba to check overlap
     * @param drive_num_blocks, total blocks starting with `lba` block
     * @return boolean,  True on overlap for a specific range
     */
    bool is_individual_range_overlap(
            const uint64_t& drive_lba,
            const uint64_t& drive_num_blocks) const;

    /**
     * @brief API to check all IO ranges overlap associated with an IO task
     *
     * @param io_ranges, list of io-ranges to be checked for overlap
     * @param num_ranges, total number of io-ranges to be checked
     * @return boolean,  True on any io-range has overlap
     */
    bool is_dev_guard_overlap(
            const std::vector<std::pair<uint64_t, uint64_t>>& io_ranges,
            const uint64_t& num_ranges) const;

    /**
     * @brief -This will indicate to block allocator, that a specific IO
     *         is in the process of being persisted
     *        -This will mark dirty meta on `jarr_io_dev_guard_`
     *
     * @param io_ranges, io-ranges associated with io-task to be marked in
     *        flight
     * @param num_ranges, total number of io-ranges associated with io-task
     * @return boolean, true on successful operation
     */
    bool mark_in_flight(
            const std::vector<std::pair<uint64_t, uint64_t>>& io_ranges,
            const uint64_t& num_ranges);

    /**
     * @brief This API will remove completed IO ranges from `io_dev_guard_q_`
     *
     * @param io_ranges, io-ranges to be removed from jarr_io_dev_guard_
     * @param num_ranges, total number of io-ranges associated with io-task
     * @return boolean, true on successful operation
     */
    bool mark_completed(
            const std::vector<std::pair<uint64_t, uint64_t>>& io_ranges,
            const uint64_t& num_ranges);

    // Class specific variables
    uint64_t drive_smallest_block_size_;
    uint64_t max_dirty_segments_;
    dss_device_t *io_device_;
    std::vector<std::pair<uint64_t, uint64_t>> dirty_meta_data_;
    std::vector<std::pair<uint64_t, uint64_t>> io_ranges_;
    uint64_t dirty_counter_;
    void *jarr_io_dev_guard_;
    std::vector<dss_io_task_t *> io_dev_guard_q_;
};

using IoTaskOrdererSharedPtr = std::shared_ptr<IoTaskOrderer>;

class BlockAllocator{
public:
    // Constructor of the class
    explicit BlockAllocator(){}
    /**
     * @brief Initializes and returns a new block allocator context
     *
     * @param device Device handle on which the block allocator needs to be
     *        initialized
     * @param config Options struct with input configuration options set
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
     * @brief load data from disk
     *
     */
    dss_blk_allocator_status_t load_opts_from_disk_data(
            uint8_t *serialized_data,
            uint64_t serialized_data_len,
            dss_blk_allocator_opts_t *opts);

    //On-Disk APIs

    /**
     * @brief - Query block allocator to obtain the size of persistent
     *          memory required for managing operations.
     *        - This is the on-disk size of all persistent data-structures
     *          used by block allocator.
     *        - Ideally invoked after setting up block allocator (init)
     *          and required for persistence.
     * 
     * @param ctx block allocator context
     * @return physical size in bytes, required to be persisted
     */
    uint64_t dss_blk_allocator_get_physical_size(
            dss_blk_allocator_context_t *ctx);

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
    dss_blk_allocator_status_t queue_sync_meta_io_tasks(
            dss_blk_allocator_context_t *ctx,
            dss_io_task_t *io_task);


    /**
     * @brief - Get IO tasks to be submitted that can be scheduled to IO
     *          module without overlap of block allocator meta-data updates
     *        - This can be invoked until there are no more tasks to be
     *          submitted to IO module
     * @param ctx block allocator context
     * @param[OUT] io_task io task that needs to be populated to be queued
     *             and forwarded to io module
     * @return dss_blk_allocator_status_t BLK_ALLOCATOR_STATUS_SUCCESS on 
     *         success or BLK_ALLOCATOR_STATUS_ITERATION_END on no available
     *         IO tasks to be scheduled to IO module or error code otherwise
     */
    dss_blk_allocator_status_t get_next_submit_meta_io_tasks(
            dss_blk_allocator_context_t *ctx,
            dss_io_task_t *io_task);

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
    IoTaskOrdererSharedPtr io_task_orderer;
};

} // end BlockAlloc Namespace
