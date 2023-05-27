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

#include <cstdint>
#include <vector>
#include <iostream>
#include <cstring>
#include <memory>

/** 
 * This namespace includes all Allocator implementations
 */

namespace AllocatorType {

/**
 * An interface that holds all bitmap representations
 * of the Allocator
 */
class BitMap {
public:
    
    virtual ~BitMap() = default;

    /**
     * @brief
     * - Cell is the basic index into the bitmap
     * - A cell can be made up of any number of bits based on
     *   bitmap implementation.
     * @ return total cells inside the bitmap
     */
    virtual uint64_t total_cells() const =0;

    /**
     * @brief
     * - Word is the number of bits in the `Data Type` used to
     *   hold cells
     * - An usual implementation of a bitmap uses a series of
     *   words represented as an array or a std::vector
     * - Returns total number of bits per word
     */
    virtual int word_size() const =0;

    /**
     * @brief returns uint8_t value in the cell identified by `index`
     */
    virtual uint8_t get_cell_value(uint64_t index) const =0;

    /**
     * @brief Sets uint8_t `value` in the cell identified by `index`
     */
    virtual void set_cell(uint64_t index, uint8_t value) =0;

    /**
     * @brief return total words in the bitmap
     */
    virtual int total_words() const =0;

    /**
     * @brief returns char* to the entire serialized bitmap
     */
    virtual void serialize_all(char* return_buf) const =0;

    /**
     * @brief Deserializes the entire bitmap represented by `serialized`
     */
    virtual void deserialize_all(const char* serialized) =0;

    /**
     * @brief returns char* to a serialized range of the bitmap
     */
    virtual void serialize_range(int word_begin, int num_words, char* return_buf) const =0;

    /**
     * @brief
     * - Deserializes a buffer `serialized` to a specific range in the bitmap
     */
    virtual void deserialize_range(const char* serialized, int len) =0;

    /**
     * @brief 
     * - Checks if a given range beginning with a cell index is empty &
     *   returns boolean (true is range is empty else false)
     */
    virtual bool seek_empty_cell_range(uint64_t begin_cell, uint64_t len) const =0;

};

using BitMapSharedPtr = std::shared_ptr<BitMap>;

} // end AllocatorType Namespace
