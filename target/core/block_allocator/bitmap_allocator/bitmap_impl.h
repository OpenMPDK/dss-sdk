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

#include "block_allocator.h"
#include "allocator_type.h"

namespace AllocatorType {

#define BITS_PER_WORD 8
#define BITS_PER_QUAD_WORD 64

/* 
 * - This class represents the bitmap wherein size of each cell 
 *   in bitmap is represented by n-bits (Where n <= 8)
 * - There can be a total of 256 bits per cell; however this
 *   design limits the total number of cells per uint64_t to
 *   be 64.
 * - All the cells are held inside a uint64_t integer
 *   represented by 64 bits.
 * - Thus, there are (64/(n=size of cell)) cells per 64 bit integer 
 *   in the bitmap
 */
class QwordVector64Cell : public AllocatorType::BitMap,
                          public BlockAlloc::Allocator {
public:
    // Constructor of the class
    explicit QwordVector64Cell(
            BlockAlloc::JudySeekOptimizerSharedPtr jso,
            int total_cells, uint8_t bits_per_cell)
        : jso_(std::move(jso)),
          cells_(total_cells),
          bits_per_cell_(
            ((bits_per_cell > BITS_PER_QUAD_WORD) || (bits_per_cell == 0))?
            BITS_PER_QUAD_WORD : bits_per_cell),
          cells_per_qword_(BITS_PER_QUAD_WORD/bits_per_cell_),
          data_(
            ((total_cells / cells_per_qword_) +
            ((total_cells % cells_per_qword_) ? 1 : 0)), 0),
          read_cell_flag_(0xff >> (BITS_PER_WORD - bits_per_cell_))
          {}
    
    // Class specific functions
    /**
     * @return total number of 64 bit unsigned integers
     * in the bitmap
     */
    int total_qwords() const { return data_.size(); }
    /**
     * - Debug functions to look at the bitmap
     * - Prints bitmap per nbit cell values
     */
    void print_range(int begin, int end) const;
    /**
     * - Debug functions to look at the bitmap
     * - Prints uint64_t value per quad word of bitmap
     */
    void print_data() const;

    /**
     * Operator over-loading on object of class to return the value
     * in a given cell of the bitmap. This behaves like a get operation
     * @ return a n-bit value inside the cell represented by uint8_t
     */
    uint8_t operator[](int index) const {
        int byte_index = index / cells_per_qword_;
        int shift = (index % cells_per_qword_) * bits_per_cell_;
        return (data_[byte_index] >> shift) & read_cell_flag_;
    }


    // AllocatorType::BitMap API (concrete definitions)
    int total_cells() const override { return cells_; }
    int word_size() const override { return 64; }
    uint8_t get_cell_value(int index) const override{
        return operator[](index);
    }
    void set_cell(int index, uint8_t value) override;
    int total_words() const override { return total_qwords(); }
    void serialize_all(char* return_buf) const override;
    void deserialize_all(const char* serialized) override;
    void serialize_range(int qword_begin, int num_words, char* return_buf) const override;
    void deserialize_range(const char* serialized, int len) override;
    bool seek_empty_cell_range(int begin_cell, int len) const override;

    //BlockAlloc::Allocator API (concrete definitions)
    void destroy(dss_blk_allocator_context_t *ctx) { return; }
    dss_blk_allocator_status_t is_block_free(
            dss_blk_allocator_context_t *ctx,
            uint64_t block_index,
            bool *is_free) { return BLK_ALLOCATOR_STATUS_SUCCESS; }
    dss_blk_allocator_status_t get_block_state(
            dss_blk_allocator_context_t* ctx,
            uint64_t block_index,
            uint64_t *block_state) { return BLK_ALLOCATOR_STATUS_SUCCESS; }
    dss_blk_allocator_status_t check_blocks_state(
            dss_blk_allocator_context_t *ctx,
            uint64_t block_index,
            uint64_t num_blocks,
            uint64_t block_state,
            uint64_t *scanned_index) {return BLK_ALLOCATOR_STATUS_SUCCESS; }
    dss_blk_allocator_status_t set_blocks_state(
            dss_blk_allocator_context_t* ctx,
            uint64_t block_index,
            uint64_t num_blocks,
            uint64_t state) { return BLK_ALLOCATOR_STATUS_SUCCESS; }
    dss_blk_allocator_status_t clear_blocks(
            dss_blk_allocator_context_t *ctx,
            uint64_t block_index,
            uint64_t num_blocks) { return BLK_ALLOCATOR_STATUS_SUCCESS; }
    dss_blk_allocator_status_t alloc_blocks_contig(
            dss_blk_allocator_context_t *ctx,
            uint64_t state,
            uint64_t hint_block_index,
            uint64_t num_blocks,
            uint64_t *allocated_start_block) {
        return BLK_ALLOCATOR_STATUS_SUCCESS; }

private:
    BlockAlloc::JudySeekOptimizerSharedPtr jso_;
    int cells_;
    uint8_t bits_per_cell_;
    int cells_per_qword_;
    std::vector<uint64_t> data_;
    uint8_t read_cell_flag_;
};

} // End AllocatorType namespace
