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

namespace AllocatorType {

void QwordVector64Cell::set_cell(int index, uint8_t value) {
    int byte_index = index / cells_per_qword_;
    int shift = (index % cells_per_qword_) * bits_per_cell_;
    data_[byte_index] &= ~(read_cell_flag_ << shift);
    data_[byte_index] |= (value & read_cell_flag_) << shift;
}

void QwordVector64Cell::serialize_all(char* serialized) const {
    std::memcpy(serialized, data_.data(), data_.size() * sizeof(uint64_t));
    return;
}

void QwordVector64Cell::deserialize_all(const char* serialized) {
    std::memcpy(data_.data(), serialized, data_.size() * sizeof(uint64_t));
}

void QwordVector64Cell::serialize_range(int qword_begin, 
    int num_words, char* return_buf) const {
    // CXX !TODO: This requires knowledge about the on-disk structure
    return;
}

void QwordVector64Cell::deserialize_range(const char* serialized, int len) {
    // CXX !TODO: This requires knowledge about the on-disk structure
    return;
}

bool QwordVector64Cell::seek_empty_cell_range(int begin_cell, int len) const {

    if (begin_cell < 0 || begin_cell + len > cells_) {
        return false;  // Out of bounds
    }

    int i = begin_cell / cells_per_qword_;  // Integer index containing the start bit
    int j = (begin_cell % cells_per_qword_) * bits_per_cell_; // Position of n-bit val
    uint64_t mask = 0;
    mask |= read_cell_flag_;
    uint64_t mask_shift = 0;
    
    // Check each n-bit value in the range
    for (int k = 0; k < len; k += bits_per_cell_) { 
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

void QwordVector64Cell::print_range(int begin, int end) const {
    std::cout<<"Debug only: Printing Bitmap Begin"<<std::endl;
    if ((end >= total_cells()) || (begin < 0)) {
        std::cout<<"Incorrect range"<<std::endl;
    }
    for (int i = begin; i <= end; ++i) {
        std::cout << static_cast<int>(operator[](i));
    }
    std::cout<<std::endl;
    std::cout<<"Debug only: Printing Bitmap End"<<std::endl;
}
void QwordVector64Cell::print_data() const {
    std::cout<<"Debug only: Printing Qword vector Begin"<<std::endl;
    for(int i=0;i<data_.size();i++) {
        std::cout<<data_[i]<<std::endl;
    }
    std::cout<<"Debug only: Printing Qword vector End"<<std::endl;
}

} // End AllocatorType namespace
