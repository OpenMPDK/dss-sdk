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
#include <fstream>

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
    DSS_ASSERT(serialized_len >= num_words * sizeof(uint64_t));
#ifndef DSS_BUILD_CUNIT_TEST
    //Serialized data should fill the entire block that is requested
    DSS_ASSERT(serialized_len == num_words * sizeof(uint64_t));
#endif
    return;
}

void QwordVector64Cell::deserialize_range(
        uint64_t qword_begin, uint64_t num_words,
        void* serialized_buf, uint64_t serialized_len) {

    char *copy_ptr = (char *)serialized_buf;

    for (uint64_t i=qword_begin; i<num_words; i++) {
        std::memcpy(&data_[i], copy_ptr, sizeof(uint64_t));
        copy_ptr = copy_ptr + sizeof(uint64_t);
    }

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
        #ifndef DSS_BUILD_CUNIT_DISABLE_MARK_DIRTY
        if(this->io_task_orderer_ != nullptr) {
            // mark dirty bitmap
            this->io_task_orderer_->mark_dirty_meta(block_index, 1);
        }
        #endif
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
    #ifndef DSS_BUILD_CUNIT_DISABLE_MARK_DIRTY
    if(this->io_task_orderer_ != nullptr) {
        // mark dirty bitmap
        this->io_task_orderer_->mark_dirty_meta(block_index, 1);
    }
    #endif

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
    #ifndef DSS_BUILD_CUNIT_DISABLE_MARK_DIRTY
    if(this->io_task_orderer_ != nullptr) {
        // mark dirty bitmap
        this->io_task_orderer_->mark_dirty_meta(block_index, num_blocks);
    }
    #endif

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
    #ifndef DSS_BUILD_CUNIT_DISABLE_MARK_DIRTY
    if(this->io_task_orderer_ != nullptr) {
        // mark dirty bitmap
        this->io_task_orderer_->mark_dirty_meta(actual_allocated_lb, num_blocks);
    }
    #endif

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t QwordVector64Cell::serialize_drive_data(
        uint64_t drive_blk_addr,
        uint64_t drive_num_blocks,
        uint64_t drive_smallest_block_size,
        void** serialized_drive_data,
        uint64_t& serialized_len
        ) {

    void *serial_buf = nullptr;
    uint64_t total_indices = 0;
    uint64_t vector_start_index = 0;
    uint64_t block_size_bits = 0;
    uint64_t dlba_with_offset = 0;
    // Represents the logical block where bitmap begins
    uint64_t block_alloc_meta_offset =
        this->block_alloc_meta_start_offset_;

#ifndef DSS_BUILD_CUNIT_TEST
    // Represents the logical block where bitmap ends
    uint64_t logical_start_block_offset =
        this->logical_start_block_offset_;
    // Make sure range within block allocator meta region
    if (drive_blk_addr < block_alloc_meta_offset) {
        assert(("ERROR", false));
    }

    if ((drive_blk_addr + drive_num_blocks) - 1 >=
            logical_start_block_offset) {
        assert(("ERROR", false));
    }
#endif

    /* NB: drive_smallest_block_size is same as logical_block_size
     *     However, this hook is in place for tuning this value in the
     *     future
     */

    // 0. Account for the block allocator meta offset before using
    //    drive_blk_addr
    dlba_with_offset = drive_blk_addr - block_alloc_meta_offset;

    // 1. Compute vector start index based on drive lba
    block_size_bits = drive_smallest_block_size * BITS_PER_BYTE;

    // dlba_with_offset lies within the bitmap region, need to align to
    // BITS_PER_WORD
    vector_start_index =
        (block_size_bits * dlba_with_offset) / BITS_PER_WORD;

    if ((block_size_bits * dlba_with_offset) % BITS_PER_WORD !=0) {
        // Remainder here will never be not 0, it will only occur
        // when drive_smallest_block_size is not a multiple of 4096
        assert(("ERROR", false));
    }

    // 2. Compute the size of serial buffer to accomodate change
    // serialized_len is accounted in bytes
    serialized_len = drive_num_blocks * drive_smallest_block_size;

    total_indices = (serialized_len * BITS_PER_BYTE) / BITS_PER_WORD;
    if ((serialized_len * BITS_PER_BYTE) % BITS_PER_WORD != 0) {
        // Remainder here will never be not 0, it will only occur
        // when drive_smallest_block_size is not a multiple of 4096
        assert(("ERROR", false));
    }


#ifndef DSS_BUILD_CUNIT_TEST
    serial_buf = dss_dma_zmalloc(serialized_len, drive_smallest_block_size);
#else
    serial_buf = malloc(serialized_len);
#endif
    DSS_ASSERT(serial_buf != NULL);

    this->serialize_range(
            vector_start_index,
            total_indices,
            serial_buf,
            serialized_len);

    *serialized_drive_data = serial_buf;

    if (*serialized_drive_data == nullptr) {
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

bool QwordVector64Cell::translate_meta_to_drive_lba(
        const uint64_t meta_lba,
        const uint64_t lb_size_in_bits,
        uint64_t& drive_blk_lba) {

    // Represents the logical block where bitmap ends
    uint64_t logical_start_block_offset =
        this->logical_start_block_offset_;
    uint64_t meta_lba_read = 0;
    // Represents the logical block where bitmap begins
    // Block allocator meta starts after super-block
    // super-block lba is `0`
    uint64_t block_alloc_meta_offset =
        this->block_alloc_meta_start_offset_;
    uint64_t lba_per_block = 0;

    // Initialize drive_lba to 0
    drive_blk_lba = 0;

    // 1. Each lba on bitmap is represented by 4 bits
    //    Determine which 64-bit integer index does lba
    //    land on.
    //    Each lba is of size logical block size in bytes
    if ((logical_start_block_offset != 0) &&
            (meta_lba < logical_start_block_offset)) {
        assert(("ERROR", false));
        return false;
    }
    lba_per_block = lb_size_in_bits / this->bits_per_cell_;

    // Make sure meta_lba is accounted for offset
    meta_lba_read = meta_lba - logical_start_block_offset;
    meta_lba_read = meta_lba_read / lba_per_block;

    // Assign drive_blk_lba
    drive_blk_lba = block_alloc_meta_offset + meta_lba_read;

#ifndef DSS_BUILD_CUNIT_TEST
    // Ignore check when there is no logical block offset
    // To catch any lba outside the block allocator meta-region
    if (drive_blk_lba >= logical_start_block_offset) {
        assert(("ERROR", false));
        return false;
    }

    if (drive_blk_lba < block_alloc_meta_offset) {
        assert(("ERROR", false));
        return false;
    }
#endif

    return true;
}

dss_blk_allocator_status_t QwordVector64Cell::translate_meta_to_drive_addr(
        uint64_t meta_lba,
        uint64_t meta_num_blocks,
        uint64_t drive_smallest_block_size,
        uint64_t logical_block_size,
        uint64_t& drive_blk_lba,
        uint64_t& drive_num_blocks) {

    uint64_t lb_size_in_bits = logical_block_size * BITS_PER_BYTE;
    uint64_t dlba_start = 0;
    uint64_t dlba_end = 0;
    uint64_t meta_lba_end = 0;
    // Initialize output variables
    drive_blk_lba = 0;
    drive_num_blocks = 0;

    if (logical_block_size != drive_smallest_block_size) {

        // CXX: Assumption that drive_smallest_block_size is equal
        //      to logical_block_size will be consistent for 
        //      immediate release
        assert(("ERROR", false));
        return BLK_ALLOCATOR_STATUS_ERROR;
    }

    // 1. Compute the start drive lba for range
    if (this->translate_meta_to_drive_lba(
                meta_lba, lb_size_in_bits, dlba_start)) {
        // Translation successful
        drive_blk_lba = dlba_start;
    } else {
        assert(("ERROR", false));
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    
    // 2. Compute total number of drive num_blocks to represent change
    // 2.1. First compute the end drive lba for range
    // Get the last meta_lba
    meta_lba_end = (meta_lba + meta_num_blocks) - 1;
    // Translate meta_lba_end to drive_lba_end (dlba_end)
    if (!this->translate_meta_to_drive_lba(
                meta_lba_end, lb_size_in_bits, dlba_end)) {
        assert(("ERROR", false));
        return BLK_ALLOCATOR_STATUS_ERROR;
    }
    // 2.2. Get drive_num_blks
    drive_num_blocks = (dlba_end - dlba_start) + 1;

    return BLK_ALLOCATOR_STATUS_SUCCESS;
}

dss_blk_allocator_status_t QwordVector64Cell::load_meta_from_disk_data(
        uint8_t *serialized_data,
        uint64_t serialized_data_len,
        uint64_t byte_offset
        ) {

    // byte_offset indicates the word alignment
    uint64_t begin_word = byte_offset / BITS_PER_WORD;
    uint64_t num_words = serialized_data_len / BITS_PER_WORD;
    
    // Acquire the allocator type and deserialize range
    this->deserialize_range(
            begin_word,
            num_words,
            serialized_data,
            serialized_data_len);

    return BLK_ALLOCATOR_STATUS_SUCCESS;
    
}

void QwordVector64Cell::write_bitmap_to_file() {
    // Currently written to /var/log/dss_bmap.data
    std::ofstream dump_file;
    dump_file.open ("/var/log/dss_bmap.data");
    for(uint64_t i=0; i<data_.size(); i++) {
        dump_file<<data_[i];
    }
    dump_file.close();
    std::cout<<"Completed writing bmap data to file"<<std::endl;

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
