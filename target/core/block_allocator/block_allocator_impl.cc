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
#include "bitmap_impl.h"

namespace BlockAlloc {

bool BlockAllocator::init(
        dss_device_t *device,
        dss_blk_allocator_opts_t *config) {

    uint64_t total_blocks = 0;
    uint64_t logical_start_block_offset = 0;
    uint64_t optimum_write_size = 0;
    uint64_t num_block_states = 0;
    uint64_t num_bits_per_block = 0;

    uint64_t drive_smallest_block_size = 0;
    // CXX TODO: This needs to be tuned appropriately
    // This represents the total number of disjoint ranges
    // that can be flushed to the disk in one io_task
    uint64_t max_dirty_segments = 4096;

    if (config == NULL) {
        return false;
    }

    // Populate config
    total_blocks = config->num_total_blocks;
    optimum_write_size = config->shard_size;
    num_block_states = config->num_block_states;
    logical_start_block_offset = config->logical_start_block_offset;

    //DSS_ASSERT(!device);//Persistence not supported by allocator
    //if(device) {
    //    return false;
    //}

    if(config->num_block_states == 0) {
        return false;
    }

    // CXX: Check if judy seek optimizer is optional
    // Check if the config indicates any other type of allocator
    BlockAlloc::JudySeekOptimizerSharedPtr jso =
        std::make_shared<BlockAlloc::JudySeekOptimizer>
        (total_blocks, logical_start_block_offset, optimum_write_size);
    if (jso == NULL) {
        return false;
    }

    // Currently only bitmap allocator is supported
    if (num_block_states <= 16) {
        // Currently only 4 bits per cell/block are represented in the bitmap
        num_bits_per_block = 4;
    }

    // num_block_states represents number of states excluding cleared state

    // Associate an io task ordering instance for disk operations
    this->io_task_orderer =
        std::make_shared<BlockAlloc::IoTaskOrderer>
        (drive_smallest_block_size, max_dirty_segments, device);
    if (io_task_orderer == nullptr) {
        return false;
    }

    // Concrete definition of the allocator
    // CXX TODO: Config should be parsed to arrive at a specific
    //           allocator type. However, only 1 type of allocator
    //           is supported for now.
    this->allocator = std::make_shared<AllocatorType::QwordVector64Cell>(
            jso, this->io_task_orderer, total_blocks, num_bits_per_block,
            num_block_states + 1, logical_start_block_offset);
    if (allocator == NULL) {
        return false;
    }

    // Bind the translator logic specific to each implementation of the
    // allocator
    this->io_task_orderer->translate_meta_to_drive_data =
        [&](uint64_t meta_lba, uint64_t meta_num_blocks,
                uint64_t drive_smallest_block_size,
                uint64_t logical_block_size,
                uint64_t& drive_blk_addr,
                uint64_t& drive_num_blocks,
                void** serialized_drive_data,
                uint64_t& serialized_len) {

            return this->allocator->translate_meta_to_drive_data(
                    meta_lba, meta_num_blocks,
                    drive_smallest_block_size,
                    logical_block_size,
                    drive_blk_addr,
                    drive_num_blocks,
                    serialized_drive_data,
                    serialized_len
                    );
        };

    DSS_ASSERT(io_task_orderer->translate_meta_to_drive_data);

    return true;
}

dss_blk_allocator_status_t BlockAllocator::queue_sync_meta_io_tasks(
                     dss_blk_allocator_context_t *ctx,
                     dss_io_task_t *io_task) {

    return this->io_task_orderer->queue_sync_meta_io_tasks(io_task);
}

dss_blk_allocator_status_t BlockAllocator::get_next_submit_meta_io_tasks(
                     dss_blk_allocator_context_t *ctx,
                     dss_io_task_t *io_task) {

    return this->io_task_orderer->get_next_submit_meta_io_tasks(io_task);
}

dss_blk_allocator_status_t BlockAllocator::complete_meta_sync(
                     dss_blk_allocator_context_t *ctx,
                     dss_io_task_t *io_task) {

    return this->io_task_orderer->complete_meta_sync(io_task);
}

} // End namespace BlockAlloc
