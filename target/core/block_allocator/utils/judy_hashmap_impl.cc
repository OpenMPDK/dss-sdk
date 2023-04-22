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

#include "judy_hashmap.h"
#include <iostream>

using namespace std;

namespace Utilities {

bool JudyHashMap::insert_element(
        const uint64_t& horizontal_index,
        const uint64_t& vertical_index,
        const uint64_t *value) {

    void **ptr_jarr_l1 = NULL;
    void *jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;

    // Check to see if there is a Judy array at the horizontal index
    ptr_j_entry = (Word_t *)JudyLIns(
            &jarr_l0_, (Word_t)horizontal_index, PJE0);

    if (*(Word_t *)ptr_j_entry != 0) {
        //CXX: Add debug `Failed to insert at L0`
        return false;
    }

    ptr_jarr_l1 = (void **)ptr_j_entry;

    ptr_j_entry = (Word_t *)JudyLIns(
            ptr_jarr_l1, (Word_t)vertical_index, PJE0);
    if (*(Word_t *)ptr_j_entry != 0) {
        //CXX: Add debug `Failed to insert at L1`
        return false;
    }

    // Assign value to the location pointed by ptr_j_entry
    *ptr_j_entry = (Word_t)*value;

    return true;
}

bool JudyHashMap::get_element(
        const uint64_t& horizontal_index,
        const uint64_t& vertical_index,
       	uint64_t* const value
        ) const {

    void *jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;

    // Get vertical jarr entry at horizontal index
    ptr_j_entry = (Word_t *)JudyLGet(
        jarr_l0_, (Word_t)horizontal_index, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect horizontal id`
        *value = 0;
        return false;
    } else {
        //CXX: Add debug `Level0 Get foudn %p\n, ptr_j_entry`
        
        // Dereference to obtain vertical judy array
        jarr_l1 = (void *)*ptr_j_entry;
        
        // Get entry at vertical index
        ptr_j_entry = (Word_t *)JudyLGet(
            jarr_l1, (Word_t)vertical_index, PJE0);
    }

    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect vertical id`
        *value = 0;
        return false;
    }

    //CXX : Add debug `Retrieved L1 item [%d] at %p\n",
    //      *ptr_j_entry, ptr_j_entry`

    // Assign and return true
    *value = *(uint64_t *)ptr_j_entry;

    return true;
}

bool JudyHashMap::get_next_l0_element(
        const uint64_t& horizontal_index,
        uint64_t* const value,
        uint64_t& next_horizontal_index,
        uint64_t& next_vertical_index
        ) const {

    void *jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;
    Word_t index_l0 = (Word_t)horizontal_index;
    Word_t index_l1 = 0; // Get first index on vertical jarr

    // Get vertical jarr entry at horizontal index
    ptr_j_entry = (Word_t *)JudyLNext(
        jarr_l0_, &index_l0, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect horizontal id`
        *value = 0;
        return false;
    } else {
        //CXX: Add debug `Found the next horizontal jarr`
        
        // Dereference to obtain vertical judy array
        jarr_l1 = (void *)*ptr_j_entry;
        
        // Get first entry at vertical index
        ptr_j_entry = (Word_t *)JudyLFirst(
            jarr_l1, &index_l1, PJE0);
    }

    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect vertical id`
        *value = 0;
        return false;
    }

    //CXX : Add debug `Retrieved L1 item [%d] at %p\n",
    //      *ptr_j_entry, ptr_j_entry`

    // Assign and return true
    next_horizontal_index = index_l0;
    next_vertical_index = index_l1;
    *value = *(uint64_t *)ptr_j_entry;

    return true;
}

bool JudyHashMap::get_prev_l0_element(
        const uint64_t& horizontal_index,
        uint64_t* const value,
        uint64_t& prev_horizontal_index,
        uint64_t& prev_vertical_index
        ) const {

    void *jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;
    Word_t index_l0 = (Word_t)horizontal_index;
    Word_t index_l1 = 0; // Get first index on vertical(l1) jarr

    // Get vertical jarr entry at horizontal(l0) index
    ptr_j_entry = (Word_t *)JudyLPrev(
        jarr_l0_, &index_l0, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect horizontal id`
        *value = 0;
        return false;
    } else {
        //CXX: Add debug `Found the next horizontal jarr`
        
        // Dereference to obtain vertical judy array
        jarr_l1 = (void *)*ptr_j_entry;
        
        // Get first entry at vertical index
        ptr_j_entry = (Word_t *)JudyLFirst(
            jarr_l1, &index_l1, PJE0);
    }

    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect vertical id`
        *value = 0;
        return false;
    }

    //CXX : Add debug `Retrieved L1 item [%d] at %p\n",
    //      *ptr_j_entry, ptr_j_entry`

    // Assign and return true
    prev_horizontal_index = index_l0;
    prev_vertical_index = index_l1;
    *value = *(uint64_t *)ptr_j_entry;

    return true;
}

bool JudyHashMap::delete_element(
        const uint64_t& horizontal_index,
        const uint64_t& vertical_index
        ) {

    void *jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;
    int del_val = 0;
    Word_t jarr_l1_mem = 0;

    // Get vertical jarr entry at horizontal index
    ptr_j_entry = (Word_t *)JudyLGet(
        jarr_l0_, (Word_t)horizontal_index, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect horizontal id`
        return false;
    } else {
        //CXX: Add debug `Level0 Get foudn %p\n, ptr_j_entry`
        
        // Dereference to obtain vertical(l1) judy array
        jarr_l1 = (void *)*ptr_j_entry;
        
        // Delete entry/value pair at vertical(l1) index
        del_val = JudyLDel(
            &jarr_l1, (Word_t)vertical_index, PJE0);
    }

    if (del_val == 1) {
        // Check to see if jarr is empty
        jarr_l1_mem = JudyLMemUsed(jarr_l1);
        //CXX: Add debug `jarr_l1_mem at vertical index is val`
        if (jarr_l1_mem == 0) {
            // Free horizontal index from horizontal(l0) judy array
            del_val = JudyLDel(
                &jarr_l0_, (Word_t)horizontal_index, PJE0);
            if (del_val != 1) {
                //CXX: Add debug `delete from horizontal on empty l1
                //     jarr failed`
                return false;
            }
        }
        return true;
    } else {
        return false;
    }
}

bool JudyHashMap::delete_l1_jarr(const uint64_t& horizontal_index) {

    void *jarr_l1 = NULL; Word_t *ptr_j_entry = NULL;
    int del_val = 0;
    int free_bytes = 0;

    // Get vertical jarr entry at horizontal(l0) index
    ptr_j_entry = (Word_t *)JudyLGet(
        jarr_l0_, (Word_t)horizontal_index, PJE0);
    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect horizontal id`
        return false;
    } else {
        //CXX: Add debug `Level0 Get foudn %p\n, ptr_j_entry`
        
        // Dereference to obtain vertical(l1) judy array
        jarr_l1 = (void *)*ptr_j_entry;

        // Free vertical(l1) judy array
        free_bytes = JudyLFreeArray(&jarr_l1, PJE0);

        //CXX: Add debug `total bytes freed`
    }

    // Delete entry/value pair at horizontal(l0) index on l0 jarr
    del_val = JudyLDel(
        &jarr_l0_, (Word_t)horizontal_index, PJE0);
    if (del_val == 1) {
        return true;
    } else {
        return false;
    }
}

void JudyHashMap::delete_hashmap() {

    if (jarr_l0_ == NULL)
        return;

    int free_bytes = 0;
    void *jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;
    Word_t index_l0 = 0;
    Word_t index_l0_curr = 0;

    // Iterate through l0 judy array to free all l1 judy
    // Get first entry at horizontal index
    ptr_j_entry = (Word_t *)JudyLFirst(
        jarr_l0_, &index_l0, PJE0);

    // Check if there is something inserted in l0
    if (ptr_j_entry == NULL) {
    //free_bytes = JudyLFreeArray(&jarr_l0_, PJE0);
        return;
    }

    while(ptr_j_entry != NULL) {
        //CXX: Add debug `free judy array at index_l0_curr`
        index_l0_curr = index_l0;

        //if (!delete_l1_jarr(index_l0_curr))
        //    return;
        //CXX: Add debug `Level0 Get foudn %p\n, ptr_j_entry`
        
        // Dereference to obtain vertical(l1) judy array
        jarr_l1 = (void *)*ptr_j_entry;
        if (jarr_l1 != NULL) {
            // Free vertical(l1) judy array
            free_bytes = JudyLFreeArray(&jarr_l1, PJE0);
        }

        ptr_j_entry = (Word_t *)JudyLNext(
            jarr_l0_, &index_l0, PJE0);
    }

    // Delete l0
    free_bytes = JudyLFreeArray(&jarr_l0_, PJE0);
    
    //CXX: Add debug `total bytes freed`
    return;
}

bool JudyHashMap::is_l0_null() {

    if (jarr_l0_ == NULL) {
        return true;
    } else {
        return false;
    }
}

} // End namespace Utilities
