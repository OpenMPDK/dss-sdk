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
#include <assert.h>
#include <chrono>
#include <random>
#include <fstream>

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

class SeekOptimizationTest : public CppUnit::TestFixture {
public:
    
    void setUp();
    void tearDown();

    void test_integrity();
    void test_alloc_free();
    void test_alloc_out_of_bounds();
    void test_alloc_duplicate_lb();
    void test_multiple_alloc_free();

    CPPUNIT_TEST_SUITE(SeekOptimizationTest);
    CPPUNIT_TEST(test_integrity);
    CPPUNIT_TEST(test_alloc_free);
    CPPUNIT_TEST(test_alloc_out_of_bounds);
    CPPUNIT_TEST(test_alloc_duplicate_lb);
    CPPUNIT_TEST(test_multiple_alloc_free);
    CPPUNIT_TEST_SUITE_END();
private:
    BlockAlloc::JudySeekOptimizerSharedPtr jso_;
    uint64_t test_total_blocks_;
    uint64_t test_start_block_offset_;
    uint64_t test_optimal_block_size_;
};

void SeekOptimizationTest::setUp() {

    uint64_t total_blocks = 65536;
    uint64_t start_block_offset = 0;
    uint64_t optimal_block_size = 128;
    // jso is created during block allocator init
    BlockAlloc::JudySeekOptimizerSharedPtr jso =
        std::make_shared<BlockAlloc::
        JudySeekOptimizer>(
                total_blocks, start_block_offset, optimal_block_size);
    if (jso) {
        jso_ = std::move(jso);
        test_total_blocks_ = total_blocks;
        test_start_block_offset_ = start_block_offset;
        test_optimal_block_size_ = optimal_block_size;
    }
}

void SeekOptimizationTest::tearDown() {
    // STUB
}

void SeekOptimizationTest::test_integrity() {
    // Validate initialization of the structures based
    // on setUp

    CPPUNIT_ASSERT(jso_->get_total_blocks() == test_total_blocks_);
    CPPUNIT_ASSERT(jso_->get_free_blocks() == test_total_blocks_);
    CPPUNIT_ASSERT(jso_->get_allocated_blocks() == 0);

    std::cout<<"Debug only: test_integrity"<<std::endl;
    jso_->print_map();
}

void SeekOptimizationTest::test_alloc_free() {

    // Allocate 400 blocks at index 5000
    uint64_t hint_lb = 5000;
    uint64_t num_blocks = 400;
    uint64_t allocated_lb = 0;
    CPPUNIT_ASSERT(jso_->allocate_lb(hint_lb, num_blocks, allocated_lb));

    // Debug only
    std::cout<<"Debug only: test_alloc_free"<<std::endl;
    jso_->print_map();

    // Since nothing is allocated this should be true
    CPPUNIT_ASSERT(allocated_lb == hint_lb);

    // Get total allocated, this should be same as num_blocks
    CPPUNIT_ASSERT(jso_->get_allocated_blocks() == num_blocks);

    // Free allocated blocks
    jso_->free_lb(hint_lb, num_blocks);
    CPPUNIT_ASSERT(jso_->get_allocated_blocks() == 0);
    // Total free blocks should equal all the blocks
    CPPUNIT_ASSERT(jso_->get_free_blocks() == jso_->get_total_blocks());

}

void SeekOptimizationTest::test_alloc_out_of_bounds() {

    // Allocate 100 blocks at the last index
    uint64_t hint_lb = test_total_blocks_;
    uint64_t num_blocks = 100;
    uint64_t allocated_lb = 0;

    // Allocation should fail
    CPPUNIT_ASSERT(!jso_->allocate_lb(hint_lb, num_blocks, allocated_lb));
    CPPUNIT_ASSERT(allocated_lb == 0);

    // Allocate test_start_blocks_offset_+1 at any lb
    hint_lb = test_start_block_offset_;
    num_blocks = test_total_blocks_ + 1;

    // Allocation should fail
    CPPUNIT_ASSERT(!jso_->allocate_lb(hint_lb, num_blocks, allocated_lb));
    CPPUNIT_ASSERT(allocated_lb == 0);
}

void SeekOptimizationTest::test_alloc_duplicate_lb() {

    // Alloc range first
    // Allocate 400 blocks at index 5000
    uint64_t hint_lb = 5000;
    uint64_t num_blocks = 400;
    uint64_t allocated_lb = 0;
    CPPUNIT_ASSERT(jso_->allocate_lb(hint_lb, num_blocks, allocated_lb));

    // Debug only
    std::cout<<"Debug only: test_alloc_duplicate_lb"<<std::endl;
    jso_->print_map();

    // Since nothing is allocated this should be true
    CPPUNIT_ASSERT(allocated_lb == hint_lb);

    // Allocate lb within the allocated range
    // This allocation must succeed
    CPPUNIT_ASSERT(jso_->allocate_lb(hint_lb, num_blocks, allocated_lb));
    // Allocated lb can not be the same hint_lb
    CPPUNIT_ASSERT(allocated_lb != hint_lb);
}

void SeekOptimizationTest::test_multiple_alloc_free() {

    // NB: The alloc and free pattern is logged to `multiple_alloc_free.log`
    //     This way, any pattern generating errors can be studied

    std::ofstream log_file;
    log_file.open("multiple_alloc_free.log");

    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(
            test_start_block_offset_, test_total_blocks_); // define range
    std::vector<std::pair<uint64_t, uint64_t>> allocated_blocks;

    // Allocate 100 blocks of fixed size
    uint64_t block_size = test_total_blocks_/10000; //(65536/10000)
    uint64_t lb = 0; // This will be generated randomly
    uint64_t allocated_lb = 0;

    log_file<<"Trying to allocate the following distribution\n";

    for (int i=0; i<100; i++) {
        //lb = distr(gen);
        // All allocations should succeed
        lb = distr(gen);
        log_file<<"lb: "<<lb<<"  len: "<<block_size<<"\n";
        CPPUNIT_ASSERT(jso_->allocate_lb(lb, block_size, allocated_lb));
        allocated_blocks.push_back(std::make_pair(allocated_lb, block_size));
        lb = 0;
        allocated_lb = 0;
        //CXX: DEBUG- Use jso_->print_map(); to see whats in the map
    }

    // After 100 allocations allocated chunk size should be validated
    CPPUNIT_ASSERT(jso_->get_allocated_blocks() == (100*block_size));

    // Free the allocated memory
    log_file<<"\n Trying to free the allocated distribution\n";
    for (size_t i=0; i<allocated_blocks.size(); i++) {
        log_file<<"lb: "<<allocated_blocks[i].first
            <<"  len: "<<allocated_blocks[i].second<<"\n";
        jso_->free_lb(allocated_blocks[i].first, allocated_blocks[i].second);
    }

    // Since all allocated blocks are freed, total blocks should be equal to total
    // free blocks
    CPPUNIT_ASSERT(jso_->get_free_blocks() == jso_->get_total_blocks());
    CPPUNIT_ASSERT(jso_->get_allocated_blocks() == 0);

    log_file<<"Allocation and Free successful\n";
    log_file.close();
}


int main() {

    /**
     * Main driver for CPPUNIT framework
     */
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(SeekOptimizationTest::suite());
	runner.run();
	return 0;

}
