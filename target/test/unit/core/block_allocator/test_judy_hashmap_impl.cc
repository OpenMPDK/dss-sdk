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
#include <assert.h>
#include <cstdlib> // For random
#include <vector>  // For std::vector 

/**
 * Cppunit headers
 */
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

class JudyHashMapTest : public CppUnit::TestFixture {
public:

    void setUp();
    void tearDown();

    void test_set_get();
    void test_multi_set_get();
    void test_set_get_next();
    void test_set_get_prev();
    void test_set_get_delete();
    void test_set_get_delete_l1_jarr();
    void test_set_get_delete_multiple_l1_jarr();
    void test_delete_all();

    CPPUNIT_TEST_SUITE(JudyHashMapTest);
    CPPUNIT_TEST(test_set_get);
    CPPUNIT_TEST(test_multi_set_get);
    CPPUNIT_TEST(test_set_get_next);
    CPPUNIT_TEST(test_set_get_prev);
    CPPUNIT_TEST(test_set_get_delete);
    CPPUNIT_TEST(test_set_get_delete_l1_jarr);
    CPPUNIT_TEST(test_set_get_delete_multiple_l1_jarr);
    CPPUNIT_TEST(test_delete_all);
    CPPUNIT_TEST_SUITE_END();

private:
    Utilities::JudyHashMapSharedPtr hash_map_;

};

void JudyHashMapTest::setUp() {
    // Create a new hash map for every new test
    hash_map_ = 
        std::make_shared<Utilities::JudyHashMap>();
}

void JudyHashMapTest::tearDown() {
    // Delete hashmap and recreate for next test
    //hash_map_->delete_hashmap();
}

/**
 * Tests set and get operation on 2D (l0, l1)
 * Judy Hashmap
 */
void JudyHashMapTest::test_set_get() {
    uint64_t horizontal_index = 10;
    uint64_t vertical_index = 20;
    uint64_t val = 50;
    uint64_t get_val = 0;

    CPPUNIT_ASSERT(hash_map_->insert_element(
                horizontal_index,
                vertical_index,
                &val)
            );

    CPPUNIT_ASSERT(hash_map_->get_element(
                horizontal_index,
                vertical_index,
                &get_val)
            );

    CPPUNIT_ASSERT(val == *(&get_val));

    hash_map_->print_map();

    return;
}

/**
 * - Tests multiple set and get operation on
 *   2d (l0,l1) Judy Hashmap
 * - This test is structured is such a way that
 *   the value being set is the sum of both
 *   horizontal_index and vertical index
 */
void JudyHashMapTest::test_multi_set_get() {
    // Initialize values for test
    uint64_t horizontal_index = 0;
    uint64_t vertical_index = 0;
    uint64_t val = 0;
    uint64_t get_val = 0;
    std::vector<std::pair<uint64_t,uint64_t>> set_pairs;

    // Generate random values for variables in test

    // Generate random seed
    srand((unsigned) time(NULL));

    // Set random 100 indices in the hashmap
    for (size_t i=0; i<100; i++) {
        horizontal_index = rand();
        vertical_index = rand();
        val = horizontal_index + vertical_index;

        CPPUNIT_ASSERT(hash_map_->insert_element(
                horizontal_index,
                vertical_index,
                &val)
            );

        // Store indices used to set for querying get
        set_pairs.push_back(
                std::make_pair(horizontal_index, vertical_index));
    }

    // Query the get operation on the same random set indices
    for(auto it=set_pairs.begin(); it!=set_pairs.end(); it++) {

        CPPUNIT_ASSERT(hash_map_->get_element(
                (*it).first,
                (*it).second,
                &get_val)
            );

        CPPUNIT_ASSERT(*(&get_val) == 
                ((*it).first +(*it).second));
        get_val = 0;
    }

    return;
}

/**
 * Tests set and get next operation on 2D (l0,l1)
 * Judy Hashmap
 */
void JudyHashMapTest::test_set_get_next() {
    uint64_t horizontal_index = 10;
    uint64_t smaller_horizontal_index = 5;
    uint64_t vertical_index = 20;
    uint64_t val = 50;
    uint64_t get_val = 0;
    uint64_t ret_l0_id = 0;
    uint64_t ret_l1_id = 0;

    // Insert an element into hash map
    CPPUNIT_ASSERT(hash_map_->insert_element(
                horizontal_index,
                vertical_index,
                &val)
            );

    // Get the inserted element by querying for a smaller
    // horizontal index
    CPPUNIT_ASSERT(hash_map_->get_next_l0_element(
                smaller_horizontal_index,
                &get_val,
                ret_l0_id,
                ret_l1_id)
            );

    CPPUNIT_ASSERT(val == *(&get_val));
    CPPUNIT_ASSERT(ret_l0_id == horizontal_index);
    CPPUNIT_ASSERT(ret_l1_id == vertical_index);

    return;
}

/**
 * Tests set and get previous operation on 2D (l0,l1)
 * Judy Hashmap
 */
void JudyHashMapTest::test_set_get_prev() {
    uint64_t horizontal_index = 10;
    uint64_t greater_horizontal_index = 20;
    uint64_t vertical_index = 20;
    uint64_t val = 50;
    uint64_t get_val = 0;
    uint64_t ret_l0_id = 0;
    uint64_t ret_l1_id = 0;

    // Insert an element into hash map
    CPPUNIT_ASSERT(hash_map_->insert_element(
                horizontal_index,
                vertical_index,
                &val)
            );

    // Get the inserted element by querying for a greater
    // horizontal index
    CPPUNIT_ASSERT(hash_map_->get_prev_l0_element(
                greater_horizontal_index,
                &get_val,
                ret_l0_id,
                ret_l1_id)
            );

    CPPUNIT_ASSERT(val == *(&get_val));
    CPPUNIT_ASSERT(ret_l0_id == horizontal_index);
    CPPUNIT_ASSERT(ret_l1_id == vertical_index);

    return;
}

/**
 * Tests set, get & delete operations on 2D (l0, l1)
 * Judy Hashmap
 */
void JudyHashMapTest::test_set_get_delete() {
    uint64_t horizontal_index = 10;
    uint64_t vertical_index = 20;
    uint64_t val = 50;
    uint64_t get_val = 0;

    // First set an element in the hashmap
    CPPUNIT_ASSERT(hash_map_->insert_element(
                horizontal_index,
                vertical_index,
                &val)
            );

    // Get the element set for sanity check
    CPPUNIT_ASSERT(hash_map_->get_element(
                horizontal_index,
                vertical_index,
                &get_val)
            );

    CPPUNIT_ASSERT(val == *(&get_val));

    // Delete the set element - (Delete success)
    CPPUNIT_ASSERT(hash_map_->delete_element(
                horizontal_index,
                vertical_index
                )
            );
    
    // Delete the same element - (Delete failure)
    CPPUNIT_ASSERT(!hash_map_->delete_element(
                horizontal_index,
                vertical_index
                )
            );

    return;
}

/**
 * Tests set, get, delete(l1) jarr operation on 2D (l0, l1)
 * Judy Hashmap
 */
void JudyHashMapTest::test_set_get_delete_l1_jarr() {
    uint64_t horizontal_index = 10;
    uint64_t vertical_index = 20;
    uint64_t val = 50;
    uint64_t get_val = 0;

    // First set an element in the hashmap
    CPPUNIT_ASSERT(hash_map_->insert_element(
                horizontal_index,
                vertical_index,
                &val)
            );

    // Get the element set for sanity check
    CPPUNIT_ASSERT(hash_map_->get_element(
                horizontal_index,
                vertical_index,
                &get_val)
            );

    CPPUNIT_ASSERT(val == *(&get_val));

    // Delete the horizontal jarr - (Delete Jarr success)
    CPPUNIT_ASSERT(hash_map_->delete_l1_jarr(horizontal_index));

    // Get the same element - (Get failure)
    CPPUNIT_ASSERT(!hash_map_->get_element(
                horizontal_index,
                vertical_index,
                &get_val)
            );

    return;
}

/**
 * Tests delete all operation on 2D (l0, l1)
 * Judy Hashmap
 */
void JudyHashMapTest::test_delete_all() {

    hash_map_->delete_hashmap();

    CPPUNIT_ASSERT(hash_map_->is_l0_null());

}

/**
 * Tests adding, retrieving and deleting vertical elements
 * on 2D Judy Hashmap
 */
void JudyHashMapTest::test_set_get_delete_multiple_l1_jarr() {
    // Initialize values for test
    uint64_t horizontal_index = 0;
    uint64_t vertical_index = 0;
    uint64_t val = 0;
    uint64_t get_val = 0;
    std::set<uint64_t> vertical_indices;
    std::vector<uint64_t> l1_val;

    // Generate random values for variables in test

    // Generate random seed
    srand((unsigned) time(NULL));

    // Generate a random horizontal index
    horizontal_index = rand();

    // Set random 10 unique vertical indices at horizontal index
    for (size_t i=0; i<10; i++) {
        vertical_index = rand();
        auto it = vertical_indices.insert(vertical_index);
        if (it.second) {
            // Unique vertical index, insert into map
            val = vertical_index;
            CPPUNIT_ASSERT(hash_map_->insert_element(
                    horizontal_index,
                    vertical_index,
                    &val)
                );
            l1_val.push_back(val);
        }
    }

    // Print map for debug
    hash_map_->print_map();

    // Delete few elements of the set
    for (size_t i=0;i<3;i++) {
        CPPUNIT_ASSERT(hash_map_->delete_element(
                    horizontal_index,
                    l1_val[i]
                    )
                );
        hash_map_->print_map();
    }

    // Get remaining elements from l1 jarr
    for (int i=3; i<10; i++) {
        vertical_index = i;
        CPPUNIT_ASSERT(hash_map_->get_element(
                horizontal_index,
                l1_val[i],
                &get_val)
            );

         CPPUNIT_ASSERT(l1_val[i] == get_val);
         get_val = 0;
    }

    return;
}

int main() {

    /**
     * Main driver for CPPUNIT framework
     */
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(JudyHashMapTest::suite());
	runner.run();
	return 0;

}
