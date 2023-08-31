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

#include "bitmap_impl.h"
#include <stdint.h>

namespace AllocatorType {

uint8_t QwordVector64Cell::get_cell_value(uint64_t index) const {

    // Account for offset if provided
    if (index < logical_start_block_offset_) {
        assert(("ERROR", false));
    }

    index = index - logical_start_block_offset_;

    return operator[](index);

}

void QwordVector64Cell::set_cell(uint64_t index, uint8_t value) {

    // Account for offset if provided
    if (index < logical_start_block_offset_) {
        assert(("ERROR", false));
    }

    index = index - logical_start_block_offset_;

    // Cast the value to a 64 bit integer for shift operation
    uint64_t cell_value = value & read_cell_flag_;
    uint64_t qword_index = index / cells_per_qword_;
    uint64_t shift = (index % cells_per_qword_) * bits_per_cell_;
    // Clear cell to set
    data_[qword_index] &= ~(read_cell_flag_ << shift);
    // Set cell with actual value
    data_[qword_index] |= cell_value << shift;

    return;
}

void QwordVector64Cell::serialize_all(char* serialized) const {
    std::memcpy(serialized, data_.data(), data_.size() * sizeof(uint64_t));
    return;
}

void QwordVector64Cell::deserialize_all(const char* serialized) {
    std::memcpy(data_.data(), serialized, data_.size() * sizeof(uint64_t));
}

void QwordVector64Cell::serialize_range(
        uint64_t qword_begin, uint64_t num_words,
        void *serialized_buf, uint64_t serialized_len) {

    // char *alloc_buf = nullptr;
    uint64_t *data_ptr = data_.data();
    // Account for offset
    data_ptr = data_ptr + qword_begin;
    // CXX!: This memory is procured from a pre-allocated mem-pool
    //TODO: allocated DMAable aligned memory
    // alloc_buf = (char*)malloc(num_words * sizeof(uint64_t));
    std::memcpy(serialized_buf, data_ptr, num_words * sizeof(uint64_t));
    // *serialized_buf = alloc_buf;
    //Serialized data should fill the entire block that is requested
    DSS_ASSERT(serialized_len == num_words * sizeof(uint64_t));
    return;
}

void QwordVector64Cell::deserialize_range(
        uint64_t qword_begin, uint64_t num_words,
        char** serialized_buf, uint64_t& serialized_len) {

    char *copy_ptr = *serialized_buf;

    for (uint64_t i=qword_begin; i<num_words; i++) {
        std::memcpy(&data_[i], copy_ptr, sizeof(uint64_t));
        copy_ptr = copy_ptr + sizeof(uint64_t);
    }

    serialized_len = num_words * sizeof(uint64_t);

    return;
}

bool QwordVector64Cell::seek_empty_cell_range(
        uint64_t begin_cell, uint64_t len) const {

    if (begin_cell < 0 || begin_cell + len > cells_) {
        return false;  // Out of bounds
    }

    int i = begin_cell / cells_per_qword_;  // Integer index containing the start bit
    int j = (begin_cell % cells_per_qword_) * bits_per_cell_; // Position of n-bit val
    uint64_t mask = 0;
    mask |= read_cell_flag_;
    uint64_t mask_shift = 0;
    
    // Check each n-bit value in the range
    for (uint64_t k = 0; k < len; k += bits_per_cell_) { 
        mask_shift = j * bits_per_cell_;
        // Get the n-bits (cell-value) at the current position
        uint64_t bits = data_[i] & (mask << mask_shift);
        if (bits != 0) {
            // Found non-zero n-bit value, return false
            return false;
        }
        
        // Move to the next cell
        j = j + bits_per_cell_;
        if (j == cells_per_qword_) {
            i++;
            j = 0;
        }
    }

    // All cells in range are zero, retun true
    return true;
}

uint64_t QwordVector64Cell::get_physical_size() {

    // Since data_ is a vector of uint64_t the physical
    // size in bytes will be 8 * data_.size()
    // 8(BITS_PER_BYTE) being the size of uint64_t in bytes
    return (BITS_PER_BYTE * data_.size());
}

dss_blk_allocator_status_t QwordVector64Cell::is_block_free(
        uint64_t block_index,
        bool *is_free) {

    int block_value = 0;
    uint64_t bmap_last_cell_id = cells_ + logical_start_block_offset_;

    if (block_index > bmap_last_cell_id) {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    block_value = get_cell_value(block_index);

    if (block_value == DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE) {
        *is_free = true;
    } else {
        *is_free = false;
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t QwordVector64Cell::get_block_state(
        uint64_t block_index,
        uint64_t *block_state) {

    uint64_t bmap_last_cell_id = cells_ + logical_start_block_offset_;

    if (block_index > bmap_last_cell_id) {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    *block_state = get_cell_value(block_index);

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t QwordVector64Cell::check_blocks_state(
        uint64_t block_index,
        uint64_t num_blocks,
        uint64_t block_state,
        uint64_t *scanned_index) {

    uint64_t bmap_last_cell_id = cells_ + logical_start_block_offset_;
    uint64_t req_last_block_lb = block_index + num_blocks - 1;

    if ((block_index > bmap_last_cell_id)
            || (req_last_block_lb > bmap_last_cell_id)) {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    if (block_index == 0) {
        *scanned_index = 0;
    } else {
        *scanned_index = block_index - 1;
    }

    for (uint64_t i=block_index; i<num_blocks; i++) {
        if (get_cell_value(i) == block_state) {
            *scanned_index = *scanned_index + 1;
        } else {
            break;
        }
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t QwordVector64Cell::set_blocks_state(
        uint64_t block_index,
        uint64_t num_blocks,
        uint64_t state) {

    bool is_jso_allocable = false;
    uint64_t actual_allocated_lb = 0;
    int cell_value = 0;

    uint64_t bmap_last_cell_id = cells_ + logical_start_block_offset_;
    uint64_t req_last_block_lb = block_index + num_blocks - 1;

    if (num_blocks != 1) {
        // Incorrect use of API
        assert(("ERROR", false));
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    if (state == DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE) {
        assert(("ERROR", false));
    }

    if (state == 0) {
        assert(("ERROR", false));
    }

    // Make sure to set only valid states
    if (state >= num_block_states_) {
        assert(("ERROR", false));
        return BLK_ALLOCATOR_STATUS_INVALID_BLOCK_STATE;
    }

    if ((block_index > bmap_last_cell_id)
            || (req_last_block_lb > bmap_last_cell_id)) {
        assert(("ERROR", false));
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    cell_value = QwordVector64Cell::get_cell_value(block_index);

    // Check to see if the block is previously allocated
    if(cell_value != DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE) {
        // OK to change state here
        QwordVector64Cell::set_cell(block_index, state);
        return BLK_ALLOCATOR_STATUS_SUCCESS;
    }

    // If the block to change state is free, alloc and then change-state

    // Check if seek optimization is enabled
    if (jso_ != NULL) {
        // try allocating on jso
        is_jso_allocable = jso_->allocate_lb(
                block_index, num_blocks, actual_allocated_lb);
        if (!is_jso_allocable) {
            return BLK_ALLOCATOR_STATUS_ERROR;
        }
    }

    // Make sure block_index is the same as actual_allocated_lb
    if (actual_allocated_lb != block_index) {
        assert(("ERROR", false));
    }

    // Now proceed to represent state on bitmap
    QwordVector64Cell::set_cell(block_index, state);

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t QwordVector64Cell::clear_blocks(
        uint64_t block_index,
        uint64_t num_blocks) {

    uint64_t iter_blk_id = 0;
    uint64_t bmap_last_cell_id = cells_ + logical_start_block_offset_;
    uint64_t req_last_block_lb = block_index + num_blocks - 1;

    if ((block_index > bmap_last_cell_id)
            || (req_last_block_lb > bmap_last_cell_id)) {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    // Check if seek optimization is enabled
    if (jso_ != NULL) {
        // try clearing/freeing on jso
        if (!jso_->free_lb(block_index, num_blocks)) {
            // JSO returns false on clearing blocks when nothing is
            // allocated.
            // Check if nothing is allocated to support clear blocks
            if (jso_->get_allocated_blocks() == 0) {
                return BLK_ALLOCATOR_STATUS_SUCCESS;
            } else {
                assert(("ERROR", false));
                return BLK_ALLOCATOR_STATUS_ERROR;
            }
        }
    }

    // Store block index
    iter_blk_id = block_index;

    // Now proceed to represent state on bitmap
    for (uint64_t i=0; i<num_blocks; i++ ) {
        QwordVector64Cell::set_cell(
                iter_blk_id, DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE);
        iter_blk_id = iter_blk_id + 1;
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}


dss_blk_allocator_status_t QwordVector64Cell::alloc_blocks_contig(
        uint64_t state,
        uint64_t hint_block_index,
        uint64_t num_blocks,
        uint64_t *allocated_start_block) {

    uint64_t actual_allocated_lb = 0;
    uint64_t iter_blk_id = 0;
    bool is_jso_allocable = false;

    uint64_t bmap_last_cell_id = cells_ + logical_start_block_offset_;

    if ((hint_block_index > bmap_last_cell_id)
            || (num_blocks > cells_)) {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    // Check if seek optimization is enabled
    if (jso_ != NULL) {
        // try allocating on jso
        is_jso_allocable = jso_->allocate_lb(
                hint_block_index, num_blocks, actual_allocated_lb);
        if (!is_jso_allocable) {
            return BLK_ALLOCATOR_STATUS_ERROR;
        }
    }

    if (allocated_start_block == NULL) {
        // This means that actual_allocated_lb
        // should be equal to hint_block_index
        if (hint_block_index != actual_allocated_lb) {
            // Free the allocated block at a different index
            if (!jso_->free_lb(actual_allocated_lb, num_blocks)) {
                // This can not fail
                assert(("ERROR", false));
            }

            return BLK_ALLOCATOR_STATUS_ERROR;
        }
    } else {
        // Assign the allocated lb to the request lb
        *allocated_start_block = actual_allocated_lb;
    }

    iter_blk_id = actual_allocated_lb;

    // Now proceed to represent state on bitmap
    for (uint64_t i=0; i<num_blocks; i++ ) {
        QwordVector64Cell::set_cell(iter_blk_id, state);
        iter_blk_id = iter_blk_id + 1;
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t QwordVector64Cell::translate_meta_to_drive_data(
        uint64_t meta_lba,
        uint64_t meta_num_blocks,
        uint64_t drive_smallest_block_size,
        uint64_t logical_block_size,
        uint64_t& drive_blk_lba,
        uint64_t& drive_num_blocks,
        void** serialized_drive_data,
        uint64_t& serialized_len) {

    std::cout<<"Qword translate meta"<<std::endl;

    /**
     * Represents the logical block where bitmap begins
     */
    uint64_t logical_start_block_offset =
        this->logical_start_block_offset_;

    uint64_t drive_start_block_offset = 0;
    uint64_t drive_vector_start_pos = 0;
    uint64_t drive_vector_start_pos_mod = 0;
    uint64_t offset_vector = 0;
    uint64_t offset_vector_mod = 0;
    uint64_t meta_bitmap_pos = 0;
    uint64_t vector_start_index = 0;
    uint64_t meta_bitmap_num_blks = 0;
    uint64_t total_indices = 0;
    uint64_t total_indices_mod = 0;
    void *serial_buf = nullptr;

    if (logical_block_size == drive_smallest_block_size) {

        drive_start_block_offset = logical_start_block_offset;
        // CXX: Assumption that drive_smallest_block_size is equal
        //      to logical_block_size will be consistent for 
        //      immediate release
    } else {
        // CXX TODO!: Handle adjusting drive start block offset
        assert(("ERROR", false));
    }

    // 1. Each lba on bitmap is represented by 4 bits
    //    Determine which 64-bit integer index does lba
    //    land on
    if (meta_lba <= logical_start_block_offset) {
        assert(("ERROR", false));
    }
    meta_lba = meta_lba - logical_start_block_offset;
    meta_bitmap_pos = bits_per_cell_* meta_lba;
    vector_start_index = meta_bitmap_pos / BITS_PER_WORD;
    drive_blk_lba = drive_start_block_offset +
            meta_bitmap_pos / 
                (drive_smallest_block_size * BITS_PER_BYTE);

    // 1.1. Compute the vector start index aligned to drive_start_lba
    drive_vector_start_pos = vector_start_index * BITS_PER_WORD;
    // 1.2. Check the offset of vector start index
    drive_vector_start_pos_mod =
        drive_vector_start_pos % 
            (drive_smallest_block_size * BITS_PER_BYTE);
    if (drive_vector_start_pos_mod != 0) {
        // Account vector start pos to be the drive lba start
        offset_vector = drive_vector_start_pos_mod / BITS_PER_WORD;
        offset_vector_mod =
            drive_vector_start_pos_mod % BITS_PER_WORD;
        if (offset_vector_mod != 0) {
            assert(("ERROR", false));
        }

        if (vector_start_index <= offset_vector) {
            vector_start_index = 0;
        } else {
            vector_start_index =
                vector_start_index - offset_vector;
        }
    }
    // 2. Compute the end vector index based on num_blocks
    meta_bitmap_num_blks = meta_num_blocks * bits_per_cell_;
    total_indices = meta_bitmap_num_blks / BITS_PER_WORD;
    total_indices_mod = meta_bitmap_num_blks % BITS_PER_WORD;
    if (total_indices_mod != 0) {
        total_indices = total_indices + 1;
    }

    // 3. Update the total number of drive lbas required
    drive_num_blocks =
        serialized_len / (drive_smallest_block_size * BITS_PER_BYTE);
    if (serialized_len % 
            (drive_smallest_block_size * BITS_PER_BYTE) != 0) {
        drive_num_blocks = drive_num_blocks + 1;
    }

    // 4. Vector start index and total_indices will be used for
    //    serialization

#ifndef DSS_BUILD_CUNIT_TEST
    serial_buf = dss_dma_zmalloc(serialized_len, drive_smallest_block_size);
#else
    serial_buf = malloc(serialized_len);
#endif
    DSS_ASSERT(serial_buf != NULL);

    this->serialize_range(
            vector_start_index, total_indices,
            serial_buf, serialized_len);

    *serialized_drive_data = serial_buf;

    if (*serialized_drive_data == nullptr) {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t QwordVector64Cell::print_stats() {

    if (jso_ != NULL) {
        // Read stats from jso
        uint64_t total_blocks = jso_->get_total_blocks();
        uint64_t free_blocks = jso_->get_free_blocks();
        uint64_t allocated_blocks = jso_->get_allocated_blocks();

        // Print to standard out
        std::cout<<std::endl;
        std::cout<<"Total blocks = "<<total_blocks<<std::endl;
        std::cout<<"Free blocks = "<<free_blocks<<std::endl;
        std::cout<<"Allocated blocks = "<<allocated_blocks<<std::endl;
        
        return BLK_ALLOCATOR_STATUS_SUCCESS;
    } else {
        // Currently allocation without jso is not fully supported
        std::cout<<"Judy seek optimizer turned off!"
            <<" No stats to report"<<std::endl;
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
}

void QwordVector64Cell::print_range(uint64_t begin, uint64_t end) const {
    // Begin and end must account for logical_start_block_offset if present
    // As they are accounted again inside `get_cell_value` API
    begin = begin + logical_start_block_offset_;
    end = end + logical_start_block_offset_;
    uint64_t last_cell_index =
        (total_cells() - 1) + logical_start_block_offset_;
    std::cout<<"Debug only: Printing Bitmap Begin"<<std::endl;
    std::cout<<"Total cells in bitmap = "<<total_cells()<<std::endl;
    if ((end > last_cell_index) || (begin < 0)) {
        std::cout<<"Incorrect range"<<std::endl;
    }
    for (uint64_t i = begin; i <= end; i++) {
        int val = get_cell_value(i);
        std::cout<<val;
    }
    std::cout<<std::endl;
    std::cout<<"Debug only: Printing Bitmap End"<<std::endl;
}

void QwordVector64Cell::print_data() const {
    std::cout<<"Debug only: Printing Qword vector Begin"<<std::endl;
    for(uint64_t i=0;i<data_.size();i++) {
        uint64_t out = data_[i];
        std::cout<<out<<std::endl;
    }
    std::cout<<"Debug only: Printing Qword vector End"<<std::endl;
}

} // End AllocatorType namespace
