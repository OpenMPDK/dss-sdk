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

bool JudySeekOptimizer::init() {

    Word_t *ptr_j_entry = NULL;

    /**
     * Prepare proper datastructures
     * 1. Configure judy free lb array
     * - This judy array will consist of a single element
     * - Start `lb` and total size
     */

    // Insert `total_blocks_` at `start_block_offset_` on jarr free lb
    ptr_j_entry = (Word_t *)JudyLIns(
            &jarr_free_lb_, (Word_t)start_block_offset_, PJE0);
    if (*(Word_t *)ptr_j_entry != 0) {
        //CXX: Add debug `Failed to insert at jarr_free_lb_`
        return false;
    } else {
        // Assign value to the location pointer by ptr_j_entry
        *ptr_j_entry = (Word_t)total_blocks_;
    }

    /**
     * 2. Configure judy free contiguos length hash map
     * - This will also contain a single elemente
     * - Total free contiguous size and the starting lb
     */

    // Insert `start_block_offset_` at `total_blocks_` on jarr contig len 
    if (!jarr_free_contig_len_->insert_element(total_blocks_,
            start_block_offset_, &start_block_offset_)) {
        //CXX: Add debug `Failed to insert at jarr_free_contig_len_`
        return false;
    }

    // Relevant structures are initialized, return true
    return true;
}

void JudySeekOptimizer::remove_element(
    const uint64_t& request_lb, const uint64_t& request_len) {
    // CXX: Log remove element `request_lb` and `request_len`

    int rc = 0;
    // Remove from both structures

    // 1. Delete from judy free lb array
    rc = JudyLDel(
            &jarr_free_lb_, (Word_t)request_lb, PJE0);
    // rc can not be anything but 1
    if(rc != 1) {
        std::cout<<"Remove from jarr_free_lb failed"<<std::endl;
        assert(("ERROR", false));
    }

    // 2. Delete from judy free contig len array
    if (!jarr_free_contig_len_->
            delete_element(request_len, request_lb)) {
        // CXX: If this happens abort
        std::cout<<"Remove from jarr_free_contig_len_ failed"<<std::endl;
        assert(("ERROR", false));
    }
    return;
}

bool JudySeekOptimizer::add_element(
    const uint64_t& request_lb, const uint64_t& request_len) {
    // CXX: Log add element `request_lb` and `request_len`

    Word_t *ptr_j_entry = NULL;
    // Add to both structures

    // 1. Add to judy free lb array
    ptr_j_entry = (Word_t *)JudyLIns(
            &jarr_free_lb_, (Word_t)request_lb, PJE0);
    if (*(Word_t *)ptr_j_entry != 0) {
        //CXX: Add debug `Failed to insert at jarr_free_lb_`
        std::cout<<"Failed to insert id: "<<request_lb<<
            "on jarr_free_lb_"<<std::endl;
        assert(("ERROR", false));   return false;
        return false;
    } else {
        // Assign value to the location pointer by ptr_j_entry
        *ptr_j_entry = (Word_t)request_len;
    }

    // 2. Add to jarr contig len map
    if (!jarr_free_contig_len_->insert_element(
                request_len, request_lb, &request_lb)) {
        //CXX: Add debug `Failed to insert at jarr_free_contig_len_`
        std::cout<<"Failed to insert id: "<<request_len<<
            "on jarr_free_contig_len_"<<std::endl;
        assert(("ERROR", false));
        return false;
    }

    // Added to relevant structures, return true
    return true;
}

bool JudySeekOptimizer::is_prev_neighbor_allocable(
        const uint64_t& request_lb,
        const uint64_t& request_len,
        uint64_t& neighbor_lb,
        uint64_t& neighbor_len
        ) const {

    Word_t *ptr_j_entry = NULL;
    uint64_t neighbor_last_lb = 0;
    uint64_t request_last_lb = 0;
    bool is_req_overlap = false;
    neighbor_lb = request_lb;

    // Get previous neighbor lb
    ptr_j_entry = (Word_t *)JudyLPrev(
            jarr_free_lb_, &neighbor_lb, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: No previous neighbor found
        neighbor_lb = 0;
        return false;
    } else {
        // Previous neighbor found
        neighbor_len = *(uint64_t *)ptr_j_entry;

        // Obtain neighbor last lb
        neighbor_last_lb = (neighbor_lb + neighbor_len) - 1;

        // Check if request_lb is within neighbor range
        if (neighbor_last_lb > request_lb) {
            is_req_overlap = true;
            request_last_lb = (request_lb + request_len) - 1;
        }

        if (is_req_overlap) {
            // Explicit check to catch request is right aligned
            if (request_last_lb <= neighbor_last_lb){
                return true;
            } else {
                return false;
            }
        } else {
            // Check if allocable
            if (request_len <= neighbor_len) {
                return true;
            } else {
                return false;
            }
        }
    }
    // Unallocable
    return false;
}

bool JudySeekOptimizer::is_next_neighbor_allocable(
        const uint64_t& request_lb,
        const uint64_t& request_len,
        uint64_t& neighbor_lb,
        uint64_t& neighbor_len
        ) const {

    Word_t *ptr_j_entry = NULL;
    neighbor_lb = request_lb;

    // Get next neighbor lb
    ptr_j_entry = (Word_t *)JudyLNext(
            jarr_free_lb_, &neighbor_lb, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: No next neighbor found
        neighbor_lb = 0;
        return false;
    } else {
        // Next neighbor found
        neighbor_len = *(uint64_t *)ptr_j_entry;

        // Check if allocable
        if (request_len <= neighbor_len) {
            return true;
        }
    }
    // Unallocable
    return false;
}

bool JudySeekOptimizer::split_curr_chunk(
        const uint64_t& curr_lb,
        const uint64_t& curr_len,
        const uint64_t& request_lb,
        const uint64_t& request_len,
        uint64_t& allocated_lb
        ) {

    // Variable declarations
    uint64_t chunk_last_lb = 0;
    uint64_t request_last_lb = 0;
    uint64_t left_chunk_len = 0;
    uint64_t left_chunk_lb = 0;
    uint64_t right_chunk_len = 0;
    uint64_t right_chunk_lb = 0;

    if (request_lb < curr_lb) {
        // API can not be used without the above check
        allocated_lb = 0;
        return false;
    }

    if (request_len > curr_len) {
        // API can not be used without the above check
        allocated_lb = 0;
        return false;
    }

    // Length is accounted including the first and last lbs
    chunk_last_lb = (curr_lb + curr_len) - 1;
    request_last_lb = (request_lb + request_len) - 1;

    // Allocate request exactly for curr chunk
    if (curr_lb == request_lb && curr_len == request_len) {

        // Remove from both jarr_free_lb_ & jarr_free_contig_len_
        remove_element(curr_lb, curr_len);

        // Return hint lb
        allocated_lb = request_lb;
        return true;
    }

    // Allocate request for chunk aligned to `chunk_last_lb (right aligned)
    if (chunk_last_lb == request_last_lb) {
        // Remove from both jarr_free_lb_ & jarr_free_contig_len_
        remove_element(curr_lb, curr_len);

        // Split curr chunk into left chunk
        left_chunk_lb = curr_lb;
        left_chunk_len = request_lb - curr_lb;

        // Add to respective values to both jarr_free_lb_
        // & jarr_free_contig_len_

        // Add left chunk
        // CXX: Log left_chunk_len and left_chunk_lb
        if(!add_element(left_chunk_lb, left_chunk_len)) {
            // Logic error
            assert(("ERROR", false));
        }
        // Assign allocated lb and return true
        allocated_lb = request_lb;
        return true;
    }

    // Allocate request for chunk aligned to `curr_lb` (left aligned)
    if (request_lb == curr_lb) {
        // Remove from both jarr_free_lb_ & jarr_free_contig_len_
        remove_element(curr_lb, curr_len);

        // Split curr chunk into right chunk
        right_chunk_lb = request_lb + request_len;
        right_chunk_len = curr_len - request_len;

        // Add to respective values to both jarr_free_lb_
        // & jarr_free_contig_len_

        // Add right chunk
        // CXX: Log right chunk lb and right chunk lb for req lb == curr lb
        if(!add_element(right_chunk_lb, right_chunk_len)) {
            // Logic error
            assert(("ERROR", false));
        }
        // Assign allocated lb and return true
        allocated_lb = request_lb;
        return true;
    }

    // Allocate request in between curr chunk
    // CXX: Explicit check to validate API usage
    if (request_lb > curr_lb &&
            request_last_lb < chunk_last_lb) {
        // Remove from both jarr_free_lb_ & jarr_free_contig_len_
        remove_element(curr_lb, curr_len);
        // CXX: Log request len

        // Split curr chunk into left and right chunks
        left_chunk_lb = curr_lb;
        left_chunk_len = request_lb - curr_lb;
        right_chunk_lb = request_lb + request_len;
        right_chunk_len = curr_len - (left_chunk_len + request_len);

        // Add to respective values to both jarr_free_lb_
        // & jarr_free_contig_len_

        // Add left chunk
        // CXX: Log left chunk lb and left chunk len, for in between curr chunk
        if (!add_element(left_chunk_lb, left_chunk_len)) {
            // Logic error
            assert(("ERROR", false));
        }
        // CXX: Log right chunk lb and right chunk len, for in between curr chunk
        // Add right chunk
        if (!add_element(right_chunk_lb, right_chunk_len)) {
            // Logic error
            assert(("ERROR", false));
        }
        // Assign allocated lb and return true
        allocated_lb = request_lb;
        return true;
    }

    // Any other case needs to be understood
    assert(("ERROR", false));

    return false;
}

bool JudySeekOptimizer::split_prev_chunk(
        const uint64_t& prev_lb,
        const uint64_t& prev_len,
        const uint64_t& request_len,
        uint64_t& allocated_lb
        ) {

    // Variable declarations
    uint64_t left_chunk_len = 0;
    uint64_t left_chunk_lb = 0;

    if (request_len > prev_len) {
        // API can not be used without the above check
        allocated_lb = 0;
        return false;
    }

    // Allocate request exactly for previous chunk
    if (prev_len == request_len) {

        // Remove from both jarr_free_lb_ & jarr_free_contig_len_
        remove_element(prev_lb, prev_len);

        // Return allocated lb
        allocated_lb = prev_lb;
        return true;
    } else {
        // This is always aligned to right of the previous chunk
        // Remove from both jarr_free_lb_ & jarr_free_contig_len_
        remove_element(prev_lb, prev_len);

        // Split curr chunk into left chunk
        left_chunk_lb = prev_lb;
        left_chunk_len = prev_len - request_len;

        // Add to respective values to both jarr_free_lb_
        // & jarr_free_contig_len_

        // Add right chunk
        // CXX: Log left chunk lb and left chunk len for split prev neighbor
        if(!add_element(left_chunk_lb, left_chunk_len)) {
            // Logic error
            assert(("ERROR", false));
        }
        // Assign allocated lb and return true
        allocated_lb = prev_lb + left_chunk_len;
        return true;
    }

    return false;
}

bool JudySeekOptimizer::split_next_chunk(
        const uint64_t& next_lb,
        const uint64_t& next_len,
        const uint64_t& request_len,
        uint64_t& allocated_lb
        ) {

    // Variable declarations
    uint64_t right_chunk_len = 0;
    uint64_t right_chunk_lb = 0;

    if (request_len > next_len) {
        // API can not be used without the above check
        allocated_lb = 0;
        return false;
    }

    // Allocate request exactly for next chunk
    if (next_len == request_len) {

        // Remove from both jarr_free_lb_ & jarr_free_contig_len_
        remove_element(next_lb, next_len);

        // Return allocated lb
        allocated_lb = next_lb;
        return true;
    } else {
        // This is always aligned to left of the next chunk
        // Remove from both jarr_free_lb_ & jarr_free_contig_len_
        remove_element(next_lb, next_len);

        // Split curr chunk into right chunk
        right_chunk_lb = next_lb + request_len;
        right_chunk_len = next_len - request_len;

        // Add to respective values to both jarr_free_lb_
        // & jarr_free_contig_len_

        // Add right chunk
        // CXX: Log right chunk lb and right chunk len for split next chunk
        if(!add_element(right_chunk_lb, right_chunk_len)) {
            // Logic error
            assert(("ERROR", false));
        }
        // Assign allocated lb and return true
        allocated_lb = next_lb;
        return true;
    }

    return false;
}

bool JudySeekOptimizer::allocate_lb(const uint64_t& hint_lb,
        const uint64_t& num_blocks, uint64_t& allocated_lb) {

    Word_t *ptr_j_entry = NULL;
    bool is_next_free = false;
    uint64_t free_len = 0;
    uint64_t neighbor_lb = 0;
    uint64_t neighbor_len = 0;
    uint64_t neighbor_last_lb = 0;
    uint64_t free_contig_lb = 0;
    uint64_t free_contig_indexl0 = 0;
    uint64_t free_contig_indexl1 = 0;
    uint64_t free_contig_val = 0;
    bool is_allocable = false;
    uint64_t last_block_lb = total_blocks_ - start_block_offset_ - 1;

    // Sanity bounds check
    if (hint_lb > last_block_lb ||
            num_blocks > CounterManager::get_free_blocks()) {
        // Invalid request
        return false;
    }

    // Look up hint_lb on jarr_free_lb_
    ptr_j_entry = (Word_t *)JudyLGet(
        jarr_free_lb_, (Word_t)hint_lb, PJE0);
    if (ptr_j_entry != NULL) {
        // Entry found, check if len is available
        free_len = *(uint64_t *)ptr_j_entry;
        if (num_blocks <= free_len) {
            // Perform split operation
            if (split_curr_chunk(hint_lb, free_len,
                        hint_lb, num_blocks, allocated_lb)) {
                is_allocable = true;
            }
        } 
    } else {
        // Entry not found or can not allocate in the curr chunk
        // Check previous and next free chunks on jarr_free_lb_
        // (order does not matter)

        // Check previous neighbor
        if (is_prev_neighbor_allocable(
                    hint_lb, num_blocks, neighbor_lb, neighbor_len)) {
            // CXX: Log previous allocable `neighbor_lb` and `neighbor_len`

            // Check if the hint_lb and num_blocks are available to allocate
            // exactly in the previous neighbor
            neighbor_last_lb = (neighbor_lb + neighbor_len) - 1;
            if (neighbor_last_lb > hint_lb) {
                // Perform split operation treating neighbor as current chunk
                if (split_curr_chunk(neighbor_lb, neighbor_len,
                            hint_lb, num_blocks, allocated_lb)) {
                    is_allocable = true;
                }
            } else if (split_prev_chunk(neighbor_lb,
                        neighbor_len, num_blocks, allocated_lb)) {
                    is_allocable = true;
            }
        }

        if (!is_allocable) {
            // Check next neighbor
            if (is_next_neighbor_allocable(
                        hint_lb, free_len, neighbor_lb, neighbor_len)) {
                // Perform split next operation
                if (split_next_chunk(neighbor_lb,
                            neighbor_len, num_blocks, allocated_lb)) {
                    is_allocable = true;
                    
                }
            }
        }
    }

    // Still not found ?
    // Search jarr_free_contig_len_ for allocation

    if (!is_allocable) {
        // Search jarr_free_contig_len_ with num_blocks to allocate
        if (jarr_free_contig_len_->get_first_l1_element(
                    num_blocks, &free_contig_lb)) {
            // Found a contiguous block at `free_contig_lb` with num_blocks
            // Perform split next operation
            if (split_next_chunk(free_contig_lb,
                        num_blocks, num_blocks, allocated_lb)) {
                is_allocable = true;
            } else {
                // No such free contiguous chunk is found, search for the
                // next bigger chunk if any
                is_next_free = jarr_free_contig_len_->get_next_l0_element(
                                num_blocks,
                                &free_contig_val,
                                free_contig_indexl0,
                                free_contig_indexl1
                                );
                if(is_next_free) {
                    // Found a contiguous block at `free_contig_lb` whose
                    // indices are indexl0(free len) & indexl1(lb)

                    // Perform split next operation
                    if (split_next_chunk(free_contig_indexl1,
                                free_contig_indexl0, num_blocks,
                                allocated_lb)) {
                        is_allocable = true;
                    }
                }
            }
        }
    }

    if(is_allocable) {
        // Record allocated on counter manager and return
        this->record_allocated_blocks(num_blocks);
        return true;
    } else {
        return false;
    }
}


bool JudySeekOptimizer::is_mergable(
        const uint64_t& request_lb,
        const uint64_t& request_len,
        uint64_t& next_neighbor_lb,
        uint64_t& next_neighbor_len,
        uint64_t& prev_neighbor_lb,
        uint64_t& prev_neighbor_len
        ) const {

    Word_t *ptr_j_entry = NULL;
    bool rc = false;
    bool check_prev_mergable = false;
    bool check_next_mergable = false;

    // Find the next neighbor lb, if any
    ptr_j_entry = (Word_t *)JudyLNext(
            jarr_free_lb_, &next_neighbor_lb, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: No next neighbor found; thus next is unmergable
        next_neighbor_lb = 0;
        next_neighbor_len = 0;
    } else {
        next_neighbor_len = *(uint64_t *)ptr_j_entry;
        check_next_mergable = true;
    }

    // Check if next is mergable
    if (check_next_mergable) {
        if (request_lb + request_len == next_neighbor_lb) {
            // Next is mergable
            rc = true;
        }
    }

    // Reset pointer
    ptr_j_entry = NULL;

    // Find the prev neighbor_lb, if any
    ptr_j_entry = (Word_t *)JudyLPrev(
            jarr_free_lb_, &prev_neighbor_lb, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: No previous neighbor found; thus prev is unmergable
        prev_neighbor_lb = 0;
        prev_neighbor_len = 0;
    } else {
        prev_neighbor_len = *(uint64_t *)ptr_j_entry;
        check_prev_mergable = true;
    }

    if (check_prev_mergable) {
        if (prev_neighbor_lb + prev_neighbor_len == request_lb) {
            // Previous is mergable
            rc = true;
        }
    }
    return rc;
}

// This API needs to be invoked only after checking `is_mergable`
void JudySeekOptimizer::merge(
        const uint64_t& request_lb,
        const uint64_t& request_len,
        const uint64_t& next_lb,
        const uint64_t& next_len,
        const uint64_t& prev_lb,
        const uint64_t& prev_len
        ) {

    uint64_t merge_chunk_lb = 0;
    uint64_t merge_chunk_len = 0;

    if (next_len == 0 && prev_len == 0) {
        // Wrong use of API
        assert(("ERROR", false));
    }

    // Check if entire range mergable
    if (next_len != 0 && prev_len !=0 ) {
        // Entire range can be merged in 1 chunk

        merge_chunk_lb = prev_lb;
        merge_chunk_len = prev_len + request_len + next_len;

        // Remove both next and prev
        remove_element(next_lb, next_len);
        remove_element(prev_lb, prev_len);
    }

    // Check if can be merged with previous
    if (prev_len != 0) {
        // Can only be merged with the previous chunk

        merge_chunk_lb = prev_lb;
        merge_chunk_len = prev_len + request_len;

        // Remove prev
        remove_element(prev_lb, prev_len);
    }

    // Check if can be merged with next
    if (next_len != 0) {
        // Can only be merged with next chunk
        
        merge_chunk_lb = request_lb;
        merge_chunk_len = request_len + next_len;

        // Remove next
        remove_element(next_lb, next_len);

    }

    // Add 1 chunk aggregated
    if(!add_element(merge_chunk_lb, merge_chunk_len)) {
        assert(("ERROR", false));
    }
    return;
}

void JudySeekOptimizer::free_lb(
        const uint64_t& lb, const uint64_t& num_blocks) {

    Word_t *ptr_j_entry = NULL;
    uint64_t next_neighbor_lb = 0;
    uint64_t next_neighbor_len = 0;
    uint64_t prev_neighbor_lb = 0;
    uint64_t prev_neighbor_len = 0;

    // Make sure to check if the `free` request is valid

    // Check no occurence of `lb` on jarr_free_lb_ since lb must be
    // allocated first to be freed
    
    ptr_j_entry = (Word_t *)JudyLGet(
            jarr_free_lb_, (Word_t)lb, PJE0);
    if (ptr_j_entry != NULL) {
        // Invalid free_lb request
        return;
    }

    // Check if the lb and chunk to be freed is mergable
    if (is_mergable(
                lb,
                num_blocks,
                next_neighbor_lb,
                next_neighbor_len,
                prev_neighbor_lb,
                prev_neighbor_len
                )) {
        // Is mergable true
        merge(
            lb,
            num_blocks,
            next_neighbor_lb,
            next_neighbor_len,
            prev_neighbor_lb,
            prev_neighbor_len
            );

    } else {
        // Is mergable false
        // Insert lb, num_blocks into the data-structures
        if (!add_element(lb, num_blocks)) {
            // Logic error
            assert(("ERROR", false));
        }
    }

    // Record freed blocks
    this->record_freed_blocks(num_blocks);

    return;
}

void JudySeekOptimizer::record_allocated_blocks(
        const uint64_t& allocated_len) {

    // Increment total_allocated_blocks_ and decrement total_free_blocks_
    total_allocated_blocks_ = total_allocated_blocks_ + allocated_len;
    // Logic check
    if (CounterManager::total_allocated_blocks_ > 
            CounterManager::total_blocks_ || 
            allocated_len > CounterManager::total_free_blocks_) {
        assert(("ERROR", false));
    }
    CounterManager::total_free_blocks_ =
        CounterManager::total_free_blocks_ - allocated_len;

    return;
}

void JudySeekOptimizer::record_freed_blocks(
        const uint64_t& freed_len) {

    // Increment total_free_blocks_ and decrement total_allocated_blocks_
    CounterManager::total_free_blocks_ =
        CounterManager::total_free_blocks_ + freed_len;
    // Logic check
    if (CounterManager::total_free_blocks_ > CounterManager::total_blocks_ ||
            freed_len > CounterManager::total_allocated_blocks_) {
        assert(("ERROR", false));
    }
    CounterManager::total_allocated_blocks_ = 
        CounterManager::total_allocated_blocks_ - freed_len;

    return;

}

void JudySeekOptimizer::print_map() const {

    if (jarr_free_lb_ == NULL) {
        std::cout<<"Structures empty"<<std::endl;
        return;
    }

    Word_t *ptr_j_entry = NULL;
    Word_t index_l0 = 0;
    Word_t index_l0_curr = 0;

    // Print jarr_free_lb_
    std::cout<<"Printing jarr_free_lb"<<std::endl;

    // Iterate through l0 judy array
    // Get first entry
    ptr_j_entry = (Word_t *)JudyLFirst(
        jarr_free_lb_, &index_l0, PJE0);

    // Check if there is something inserted in l0
    if (ptr_j_entry == NULL) {
        return;
    }

    while(ptr_j_entry != NULL) {
        //CXX: Add debug `free judy array at index_l0_curr`
        index_l0_curr = index_l0;
        std::cout<<"Id(lb): "<<(uint64_t)index_l0_curr<<"\t";
        std::cout<<"Val(len): "<<(uint64_t)*ptr_j_entry;
        std::cout<<std::endl;

        ptr_j_entry = (Word_t *)JudyLNext(
            jarr_free_lb_, &index_l0, PJE0);
    }

    // Print jarr_free_contig_len_
    std::cout<<"Printing jarr_free_contig_len map"<<std::endl;
    jarr_free_contig_len_->print_map();
}

}// End namespace BlockAlloc
