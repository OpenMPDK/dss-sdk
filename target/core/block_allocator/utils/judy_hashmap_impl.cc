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

constexpr std::uint8_t JLAP_MASK{ 0x07  }; // hex for 0000 1111
constexpr std::uint8_t CHECK_JUDY_MASK{ 0x07  }; // hex for 0000 0111
constexpr std::uint8_t VALID_JUDY{ 0x00 }; // hex for 0000 0000

bool JudyHashMap::insert_element(
        const uint64_t& horizontal_index,
        const uint64_t& vertical_index,
        const uint64_t *value) {

    void **ptr_jarr_l1 = NULL;
    //void *jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;

    // Check to see if there is a Judy array at the horizontal index
    ptr_j_entry = (Word_t *)JudyLIns(
            &jarr_l0_, (Word_t)horizontal_index, PJE0);

    if (ptr_j_entry == PJERR) {
        std::cout<<"malloc error on insert horizontal index"<<std::endl;
        assert(("ERROR", false));
    }

    ptr_jarr_l1 = (void **)ptr_j_entry;

    ptr_j_entry = (Word_t *)JudyLIns(
            ptr_jarr_l1, (Word_t)vertical_index, PJE0);
    if (ptr_j_entry == PJERR) {
        std::cout<<"malloc error on insert vertical index"<<std::endl;
        assert(("ERROR", false));
    }
    if (*(Word_t *)ptr_j_entry != 0) {
        //CXX: Add debug `Failed to insert at L1`
        std::cout<<"failed to insert at l1 on map"<<std::endl;
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
        if(ptr_j_entry == PJERR) {
            std::cout<<"PJERR on get"<<std::endl;
            std::cout<<"going to oom handling"<<std::endl;
            assert(("ERROR", false));
        }
        //CXX: Add debug `Level0 Get foudn %p\n, ptr_j_entry`
        if (*ptr_j_entry & CHECK_JUDY_MASK != VALID_JUDY) {
            std::cout<<"INVALID JUDY_ARRAY"<<std::endl;
            assert(("ERROR", false));
        }
        
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

    if (ptr_j_entry == PJERR) {
        std::cout<<"PJERR on get"<<std::endl;
        std::cout<<"going to oom handling"<<std::endl;
        assert(("ERROR", false));
    }

    //CXX : Add debug `Retrieved L1 item [%d] at %p\n",
    //      *ptr_j_entry, ptr_j_entry`

    // Assign and return true
    *value = *(uint64_t *)ptr_j_entry;

    return true;
}

bool JudyHashMap::get_first_l1_element(
        const uint64_t& horizontal_index,
       	uint64_t* const value
        ) const {

    void *jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;
    Word_t index_l1 = 0; // Get first index on vertical jarr

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

    void **jarr_l1 = NULL;
    Word_t *ptr_j_entry = NULL;
    int rc = 0;
    Word_t jarr_l1_mem = 0;
    Word_t jarr_l1_count = 0;
    int free_bytes = 0;

    // Get vertical jarr entry at horizontal index
    ptr_j_entry = (Word_t *)JudyLGet(
        jarr_l0_, (Word_t)horizontal_index, PJE0);


    if (ptr_j_entry == NULL) {
        //CXX: Add debug `value not found incorrect horizontal id`
        std::cout<<"Index "<<horizontal_index<<" not found incorrect"
            <<" horizontal_index id"<<std::endl;
        return false;
    } else {
        if(ptr_j_entry == PJERR) {
            std::cout<<"PJERR on get on delete"<<std::endl;
            assert(("ERROR", false));
        }

        if (*ptr_j_entry & JLAP_INVALID) {
            std::cout<<"invalid judy array on get for delete "<<std::endl;
            assert(("ERROR", false));
        }
        //CXX: Add debug `Level0 Get found %p\n, ptr_j_entry`
        
        // Dereference to obtain vertical(l1) judy array
        jarr_l1 = (void **)ptr_j_entry;
        // Delete entry/value pair at vertical(l1) index
        rc = JudyLDel(
            jarr_l1, (Word_t)vertical_index, PJE0);

        if (rc == JERR) {
            std::cout<<"JERR on delete"<<std::endl;
            assert(("ERROR", false));
        }
    }

    if (rc == 1) {
        // Check to see if jarr is empty
        jarr_l1_count = JudyLCount(*jarr_l1, 0, -1, PJE0);
        //CXX: Add debug `jarr_l1_mem at vertical index is val`
        if (jarr_l1_count == 0) {
            // CXX: Log jarr l1 elements are 0 
            // Free horizontal index from horizontal(l0) judy array
            // free jarr_l1 (JudyLFreeArray)
            free_bytes = JudyLFreeArray(jarr_l1, PJE0);
            // CXX: Log free_bytes, to understand the memory freed
            // This should be `0` in a happy path
            rc = JudyLDel(
                &jarr_l0_, (Word_t)horizontal_index, PJE0);
            if (rc != 1) {
                //CXX: Add debug `delete from horizontal on empty l1
                //     jarr failed`
                std::cout<<"Delete from "<<horizontal_index<<" on empty l1"
                    <<" jarr failed"<<std::endl;
                return false;
            }
        }
        return true;
    } else {

        if (rc == JERR) {
            std::cout<<"malloc bug"<<std::endl;
        }
        std::cout<<"Delete on "<<vertical_index<<" on l1"
            <<" jarr failed"<<std::endl;
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
    int rc = 0;

    // Iterate through l0 judy array to free all l1 judy
    // Get first entry at horizontal index
    ptr_j_entry = (Word_t *)JudyLFirst(
        jarr_l0_, &index_l0, PJE0);

    // Check if there is something inserted in l0
    if (ptr_j_entry == NULL) {
        return;
    }

    while(ptr_j_entry != NULL) {
        // Iterate only over pointers that are judy arrays
        if ((*ptr_j_entry & JLAP_MASK) != JLAP_INVALID) {
            //CXX: Add debug `free judy array at index_l0_curr`
            index_l0_curr = index_l0;

            // Dereference to obtain vertical(l1) judy array
            jarr_l1 = (void *)*ptr_j_entry;
            if (jarr_l1 != NULL) {
                // Free vertical(l1) judy array
                free_bytes = JudyLFreeArray(&jarr_l1, PJE0);
                // Remove key value from jarr_l0_
                rc = JudyLDel(
                        &jarr_l0_, index_l0_curr, PJE0);
                if (rc != 1) {
                    // CXX: Add debug `delete from l0 on empty 
                    // l1 jarr failed`
                    std::cout<<"delete from l0 on empty l1 jarr failed"
                        <<std::endl;
                    assert(("ERROR", false));
                }
            }
        } else {
            //
            // Remove key value from jarr_l0_
           /* rc = JudyLDel(
                    &jarr_l0_, index_l0_curr, PJE0);
            if (rc != 1) {
                // CXX: Add debug `delete from l0 on empty 
                // l1 jarr failed`
                std::cout<<"delete from l0 on empty l1 jarr failed"
                    <<std::endl;
                assert(("ERROR", false));
            }*/
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

void JudyHashMap::print_map() {

    if (jarr_l0_ == NULL) {
        std::cout<<"Hashmap is empty"<<std::endl;
        return;
    }

    void *jarr_l1 = NULL;
    Word_t *ptr_j_entry_l0 = NULL;
    Word_t *ptr_j_entry_l1 = NULL;
    Word_t index_l0 = 0;
    Word_t index_l0_curr = 0;
    Word_t index_l1 = 0;
    Word_t index_l1_curr = 0;
    uint64_t get_val = 0;

    // Iterate through l0 judy array to free all l1 judy
    // Get first entry at horizontal index
    ptr_j_entry_l0 = (Word_t *)JudyLFirst(
        jarr_l0_, &index_l0, PJE0);


    // Check if there is something inserted in l0
    if (ptr_j_entry_l0 == NULL) {
        std::cout<<"Hashmap is empty"<<std::endl;
        return;
    }

    cout<<"Printing Judy of Judy map"<<std::endl;

    if (ptr_j_entry_l0 == PJERR) {
        std::cout<<"PJERR on JLF for jarr l0"<<std::endl;
        assert(("ERROR", false));
    }

    while(ptr_j_entry_l0 != NULL) {
        // Print judy array at index_l0_curr
        index_l0_curr = index_l0;
        index_l1_curr = 0;
        index_l1 = 0;

        std::cout<<"Index on l0: "<<index_l0_curr<<"\t"<<"l1 Elements:";
        if (*ptr_j_entry_l0 & JLAP_INVALID) {
            std::cout<<"Invalid judy array at horizontal id "<<index_l0<<std::endl;
            ptr_j_entry_l0 = (Word_t *)JudyLNext(
                jarr_l0_, &index_l0, PJE0);
            continue;
        }

        //CXX: Add debug `Level0 Get found %p\n, ptr_j_entry`
        
        // Dereference to obtain vertical(l1) judy array
        jarr_l1 = (void *)*ptr_j_entry_l0;
        if (jarr_l1 != NULL) {
            if (*ptr_j_entry_l0 & JLAP_INVALID) {
                std::cout<<"this is not a judy array"<<std::endl;
                assert(("ERROR", false));
            }
            // Iterate and print vertical(l1) judy array
            ptr_j_entry_l1 = (Word_t *)JudyLFirst(
                    jarr_l1, &index_l1, PJE0);
            // ptr_j_entry_l1 can not be null
            if (ptr_j_entry_l1 != NULL) {
                while(ptr_j_entry_l1 != NULL) {
                    if (ptr_j_entry_l1 == PJERR) {
                        std::cout<<"PJERR on JLF for jarr l1"<<std::endl;
                        assert(("ERROR", false));
                    } 
                    index_l1_curr = index_l1;
                    std::cout<<"id: "<<index_l1_curr<<" ,";
                    std::cout<<"val: "<<*(uint64_t *)ptr_j_entry_l1;
                    std::cout<<"\t";

                    ptr_j_entry_l1 = (Word_t *)JudyLNext(
                            jarr_l1, &index_l1, PJE0);
                }
            }
        }
        std::cout<<endl;
        std::cout<<endl;

        ptr_j_entry_l0 = (Word_t *)JudyLNext(
            jarr_l0_, &index_l0, PJE0);
    }
    return;
}

} // End namespace Utilities
