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

#include "block_allocator.h"

namespace BlockAlloc {

void IoTaskOrderer::populate_io_ranges(dss_io_task_t* io_task,
        std::vector<std::pair<uint64_t, uint64_t>>& io_ranges,
        uint64_t& num_ranges, bool is_completion) {
    dss_io_task_status_t rc;
    dss_io_op_exec_state_t op_state_filter;
    void *tmp_it_ctx = NULL;
    dss_io_op_user_param_t op_params;

    is_completion?(op_state_filter = DSS_IO_OP_COMPLETED):(op_state_filter = DSS_IO_OP_FOR_SUBMISSION);
    num_ranges = 0;

#ifndef DSS_BUILD_CUNIT_TEST
    do {
        rc = dss_io_task_get_op_ranges(io_task, DSS_IO_OP_OWNER_BA, op_state_filter, &tmp_it_ctx, &op_params);
        DSS_ASSERT((rc == DSS_IO_TASK_STATUS_SUCCESS) || (rc == DSS_IO_TASK_STATUS_IT_END));
        if(op_params.is_params_valid) {
            io_ranges[num_ranges].first = op_params.lba;
            io_ranges[num_ranges].second = op_params.num_blocks;
            if(is_completion) {
#ifndef DSS_BUILD_CUNIT_TEST
                if(op_params.data) dss_free(op_params.data);
#else
                if(op_params.data) free(op_params.data);
#endif
            }
            num_ranges++;
            DSS_ASSERT(num_ranges <= 100);//Vector capacity is now 100
        }
    } while(rc == DSS_IO_TASK_STATUS_SUCCESS);
    //TODO: Handle if any ops failed
#else
    //Dummy stub for unit test
    num_ranges = 1;
    io_ranges[0].first = 10;
    io_ranges[0].second = 100;
#endif //DSS_BUILD_CUNIT_TEST
    return;

}

dss_blk_allocator_status_t IoTaskOrderer::mark_dirty_meta(
        uint64_t lba, uint64_t num_blocks) {

    // If there are more dirty segments per IO request than
    // the specified max_dirty_segments for IoTaskOrderer
    // assert for now.
    if (dirty_counter_ > max_dirty_segments_) {
        assert(("ERROR", false));
        //return BLK_ALLOCATOR_STATUS_ERROR;
    }

    // Add dirty data into `diry_meta` container at the position
    // of dirty_counter_
    dirty_meta_data_[dirty_counter_].first = lba;
    dirty_meta_data_[dirty_counter_].second = num_blocks;

    // Increase the dirty_counter_
    dirty_counter_++;

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

bool IoTaskOrderer::is_current_overlap(
        const uint64_t& drive_lba) const {

    Word_t *ptr_j_entry = nullptr;

    // Look up current LBA for in-flight operation
    ptr_j_entry = (Word_t *)JudyLGet(
            jarr_io_dev_guard_, (Word_t)drive_lba, PJE0);
    if (ptr_j_entry != nullptr) {
        // There is an IO entry in-flight
        return true;
    } else {
        return false;
    }
}

bool IoTaskOrderer::is_prev_neighbor_overlap(
        const uint64_t& drive_lba) const {

    Word_t *ptr_j_entry_prev = nullptr;
    Word_t prev_drive_lba = (Word_t)drive_lba;
    uint64_t prev_drive_num_blocks = 0;
    
    ptr_j_entry_prev = (Word_t *)JudyLPrev(jarr_io_dev_guard_,
            &prev_drive_lba, PJE0);
    if (ptr_j_entry_prev == nullptr) {
        //There is no previous IO in flight
        return false;
    } else {
        // Check if there is no overlap with the previous neighbor
        prev_drive_num_blocks = *(uint64_t *)ptr_j_entry_prev;

        if (prev_drive_lba + prev_drive_num_blocks < drive_lba) {
            return false;
        } else {
            return true;
        }
    }
}

bool IoTaskOrderer::is_next_neighbor_overlap(const uint64_t& drive_lba,
        const uint64_t& drive_num_blocks) const {

    Word_t *ptr_j_entry_next = nullptr;
    Word_t next_drive_lba = (Word_t)drive_lba;
    
    ptr_j_entry_next = (Word_t *)JudyLNext(jarr_io_dev_guard_,
            &next_drive_lba, PJE0);
    if (ptr_j_entry_next == nullptr) {
        //There is no next IO in flight
        return false;
    } else {
        // Check if there is no overlap with the previous neighbor
        if (drive_lba + drive_num_blocks < next_drive_lba) {
            return false;
        } else {
            return true;
        }
    }
}

bool IoTaskOrderer::is_individual_range_overlap(const uint64_t& drive_lba,
        const uint64_t& drive_num_blocks) const {

    // Check if current range overlaps with previous and next 
    // ranges in-flight
    if (is_current_overlap(drive_lba)) {
        return true;
    } else if (is_prev_neighbor_overlap(drive_lba)) {
        return true;
    } else if (is_next_neighbor_overlap(drive_lba, drive_num_blocks)) {
        return true;
    } else {
        // No overlap with current, previous or next
        return false;
    }
}

bool IoTaskOrderer::is_dev_guard_overlap(
        const std::vector<std::pair<uint64_t, uint64_t>>& io_ranges,
        const uint64_t& num_ranges) const {

    if (num_ranges == 0 || (num_ranges > max_dirty_segments_)) {
        assert(("ERROR", false));
    }

    // Check if all the ranges can be scheduled on the drive
    for (size_t i=0; i<num_ranges; i++) {

        if (this->is_individual_range_overlap(
                    io_ranges[i].first, io_ranges[i].second)) {
            return true;
        }
    }

    return false;
}

bool IoTaskOrderer::mark_in_flight(
        const std::vector<std::pair<uint64_t, uint64_t>>& io_ranges,
        const uint64_t& num_ranges) {

    Word_t *ptr_j_entry = nullptr;

    // Mark all io-ranges on `jarr_io_dev_guard_`
    for (size_t i=0; i<num_ranges; i++) {
        // Sanity check to see if range is already inserted
        ptr_j_entry  = (Word_t *)JudyLIns(
                &jarr_io_dev_guard_, (Word_t)io_ranges[i].first, PJE0);

        if (ptr_j_entry == PJERR) {
            std::cout<<"malloc error on insert io range"<<std::endl;
            assert(("ERROR", false));
        }

        if (*(Word_t *)ptr_j_entry != 0) {
            //CXX: Add debug `Failed to insert at jarr_io_dev_guard_`
            // Assert for now
            assert(("ERROR", false));
            //return false;
        } else {
            // Assign value to the location pointer by ptr_j_entry
            *ptr_j_entry = (Word_t)io_ranges[i].second;
        }

        ptr_j_entry = nullptr;
    }

    return true;
}

bool IoTaskOrderer::mark_completed(
        const std::vector<std::pair<uint64_t, uint64_t>>& io_ranges,
        const uint64_t& num_ranges) {

    int rc = 0;

    // Remove all ranges from `jarr_io_dev_guard_`
    for (size_t i=0; i<num_ranges; i++) {
        rc = JudyLDel(
                &jarr_io_dev_guard_, (Word_t)io_ranges[i].first, PJE0);

        // rc can not be anything but 1
        if(rc != 1) {
            std::cout<<"Remove from jarr_io_dev_guard_ failed"<<std::endl;
            // Assert for now
            assert(("ERROR", false));
            // retun false;
        }
    }

    return true;
}

dss_blk_allocator_status_t IoTaskOrderer::queue_sync_meta_io_tasks(
        dss_io_task_t* io_task) {

    uint64_t drive_blk_addr = 0;
    uint64_t drive_num_blocks = 0;
    void* serialized_drive_data = nullptr;
    uint64_t serialized_len = 0;
    uint64_t logical_block_size = 4096;
    dss_blk_allocator_status_t blk_status = BLK_ALLOCATOR_STATUS_ERROR;
    dss_io_task_status_t io_status = DSS_IO_TASK_STATUS_ERROR;

    
    // Get dirty data  and add to io_task
    for (size_t i=0; i<dirty_counter_; i++) {

        // Get translated meta to drive data, (specific to allocator)
        blk_status = this->translate_meta_to_drive_data(
                dirty_meta_data_[i].first,
                dirty_meta_data_[i].second,
                drive_smallest_block_size_,
                logical_block_size,
                drive_blk_addr,
                drive_num_blocks,
                &serialized_drive_data,
                serialized_len
                );

        if (blk_status != BLK_ALLOCATOR_STATUS_SUCCESS) {
            assert(("ERROR", false));
            //return status;
        }

        // Add block allocator dirty meta data to the io_task
        if (io_task == nullptr && *this->get_io_device() == nullptr) {
            // CXX: STUB for testing until the IO module API is ready
            std::cout<<
                "IO module stub for testing IO ordering"<<std::endl;
        } else {
            dss_io_opts_t io_opts = {.mod_id = DSS_IO_OP_OWNER_BA, .is_blocking = false};
            io_status = dss_io_task_add_blk_write(
                  io_task,
                  *this->get_io_device(),
                  drive_blk_addr,
                  drive_num_blocks,
                  serialized_drive_data,
                  &io_opts
                  );

            if (io_status != DSS_IO_TASK_STATUS_ERROR) {
                assert(("ERROR", false));
            }
        }
    }

    // Reset dirty_counter_ to 0, since we have add all pending
    // dirty meta data relevant to the IO
    dirty_counter_ = 0;

    // Insert into IO device guard queue
    io_dev_guard_q_.push_back(io_task);

    return BLK_ALLOCATOR_STATUS_SUCCESS;

}

dss_blk_allocator_status_t IoTaskOrderer::get_next_submit_meta_io_tasks(
        dss_io_task_t* io_task) {

    // Iterate from the front of queue to examine all possible
    // io tasks that do not have overlap and can be executed in
    // parallel
    
    if (io_dev_guard_q_.size() == 0) {
        return BLK_ALLOCATOR_STATUS_ITERATION_END;
    }

    uint64_t num_ranges = 0;

    std::vector<dss_io_task_t *>::iterator it = io_dev_guard_q_.begin();
    /*while (it != io_dev_guard_q_.end()) {
        // 1. Obtain block allocator ranges from io task
        populate_io_ranges(*it, io_ranges_, num_ranges);
        // 2. Check if all ranges have no overlap
        // check_dev_guard_overlap
        // 3. If no overlap, break, erase it, mark_in_flight
        //    and return io_task
        if(is_dev_guard_overlap(io_ranges_, num_ranges)) {

            // Just break for now and do nothing
            return BLK_ALLOCATOR_STATUS_ITERATION_END;
        } else {
            break;
        }
    }*/

    // CXX: Currently only the top item is checked, this can be iterated
    // through in a loop as demonstrated above
    populate_io_ranges(*it, io_ranges_, num_ranges, false);
    if (is_dev_guard_overlap(io_ranges_, num_ranges)) {
        return BLK_ALLOCATOR_STATUS_ITERATION_END;
    } else {
        this->mark_in_flight(io_ranges_, num_ranges);
        io_task = *it;
        io_dev_guard_q_.erase(it);

        return BLK_ALLOCATOR_STATUS_SUCCESS;
    }
}

dss_blk_allocator_status_t IoTaskOrderer::complete_meta_sync(
        dss_io_task_t* io_task) {

    uint64_t num_ranges = 0;

    // Populate io-ranges associated with io-task
    this->populate_io_ranges(io_task, io_ranges_, num_ranges, true);

    // Mark ranges completed (remove from jarr_io_dev_guard_)
    if (this->mark_completed(io_ranges_, num_ranges)) {
        return BLK_ALLOCATOR_STATUS_SUCCESS;
    } else {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

}

}// End namespace BlockAlloc
