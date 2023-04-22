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

#include "Judy.h"
#include <memory>

namespace Utilities {

/**
 * 2 dimensional implementation for Judy Array of Judy
 * Array
 *
 * JudyHashMap is an interface for interacting with the
 * 2 dimensional implementation
 */
class JudyHashMap {
public:
    JudyHashMap()
        : jarr_l0_(NULL)
    {}

    /**
     * Insert into 2D (l0, l1) Judy Hashmap
     * - @param `horizontal index` index on l0 judy array
     *   @param `vertical_index` index on l1 judy array
     *   @param `value` is the pointer to the value to be inserted
     *   @return boolean
     */
    bool insert_element(
            const uint64_t& horizontal_index,
            const uint64_t& vertical_index,
            const uint64_t *value
            );

    /**
     * Delete an element from 2D (l0, l1) Judy Hashmap
     * - @param `horizontal index` index on l0 judy array
     *   @param `vertical_index` index on l1 judy array
     *   @return boolean, `true` if delete was successful
     */
    bool delete_element(
            const uint64_t& horizontal_index,
            const uint64_t& vertical_index
            );

    /**
     * Get an element from 2D (l0, l1) Judy Hashmap
     * - @param `horizontal index` index on l0 judy array
     *   @param `vertical_index` index on l1 judy array
     *   @return `value` is the pointer to the value inside hashmap
     *   @return boolean, value can only be used when `true` is returned
     */
    bool get_element(
            const uint64_t& horizontal_index,
            const uint64_t& vertical_index,
            uint64_t* const value
            ) const;

    /**
     * Get the next element from 2D (l0, l1) Judy Hashmap
     * - @param `horizontal index` index on l0 judy array
     *           The element returned is from the next greater index on
     *           l0 and the first element in l1 at l0
     *   @return `value` is the pointer to the value inside hashmap
     *   @return `next_horizontal_index` next l0 index
     *   @return `next_vertical_index` next l1 index
     *   @return boolean, value can only be used when `true` is returned
     */
    bool get_next_l0_element(
            const uint64_t& horizontal_index,
            uint64_t* const value,
            uint64_t& next_horizontal_index,
            uint64_t& next_vertical_index
            ) const;

    /**
     * Get the previous element from 2D (l0, l1) Judy Hashmap
     * - @param `horizontal index` index on l0 judy array
     *           The element returned is from the previous smaller index on
     *           l0 and the first element in l1 at l0
     *   @return `value` is the pointer to the value inside hashmap
     *   @return `prev_horizontal_index` previous l0 index
     *   @return `prev_vertical_index` previous l1 index
     *   @return boolean, value can only be used when `true` is returned
     */
    bool get_prev_l0_element(
            const uint64_t& horizontal_index,
            uint64_t* const value,
            uint64_t& prev_horizontal_index,
            uint64_t& prev_vertical_index
            ) const;

    /**
     * Delete the l1 Judy Array from 2D (l0, l1) Judy Hashmap
     * - @param `horizontal index` index on l0 judy array
     *          This operation subsequently removes horizontal index
     *          from l0
     *   @return boolean
     */
    bool delete_l1_jarr(const uint64_t& horizontal_index);

    /**
     * Delete the entire judy hashmap
     */
    void delete_hashmap();

    /**
     * API to check if jarr_l0_ is null
     */
    bool is_l0_null();

private:
    void *jarr_l0_;
};

using JudyHashMapSharedPtr = std::shared_ptr<JudyHashMap>;

} // End namespace Utilities
