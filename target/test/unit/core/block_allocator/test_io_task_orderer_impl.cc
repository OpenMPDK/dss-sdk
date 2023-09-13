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
#include "bitmap_impl.h"
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

class IoTaskOrdererTest : public CppUnit::TestFixture {
public:
    
    void setUp();
    void tearDown();

    void test_integrity();
    void test_queue_sync_meta_io_tasks();
    void test_get_next_submit_meta_io_tasks();
    void test_complete_meta_sync();

    CPPUNIT_TEST_SUITE(IoTaskOrdererTest);
    CPPUNIT_TEST(test_integrity);
    CPPUNIT_TEST(test_queue_sync_meta_io_tasks);
    CPPUNIT_TEST(test_get_next_submit_meta_io_tasks);
    CPPUNIT_TEST(test_complete_meta_sync);
    CPPUNIT_TEST_SUITE_END();
private:
    BlockAlloc::IoTaskOrdererSharedPtr io_task_orderer_;
    AllocatorType::BitMapSharedPtr bmap_;
};

void IoTaskOrdererTest::setUp() {

    // Variables for bitmap and judy seek optimizer initialization
    uint64_t total_cells = 10;
    uint64_t bits_per_cell = 4;
    uint64_t num_block_states = 5;
    uint64_t logical_start_block_offset = 0;

    // Variables for io task orderer initialization
    uint64_t drive_smallest_block_size = 4096;
    uint64_t max_dirty_segments = 3;

    // jso is created during block allocator init
    BlockAlloc::JudySeekOptimizerSharedPtr jso =
        std::make_shared<BlockAlloc::
        JudySeekOptimizer>(total_cells, logical_start_block_offset, 128);

    io_task_orderer_ =
        std::make_shared<BlockAlloc::IoTaskOrderer>(
                drive_smallest_block_size, max_dirty_segments, nullptr);

    bmap_ = std::make_shared<AllocatorType::QwordVector64Cell>(
        jso, io_task_orderer_, total_cells, bits_per_cell,
        num_block_states, logical_start_block_offset);


    io_task_orderer_->translate_meta_to_drive_data =
        [&](uint64_t meta_lba, uint64_t meta_num_blocks,
                uint64_t drive_smallest_block_size,
                uint64_t logical_block_size,
                uint64_t& drive_blk_addr,
                uint64_t& drive_num_blocks,
                void** serialized_drive_data,
                uint64_t& serialized_len) {

                     return std::dynamic_pointer_cast
                                <AllocatorType::QwordVector64Cell>
                                (bmap_)->translate_meta_to_drive_data(
                             meta_lba, meta_num_blocks,
                             drive_smallest_block_size,
                             logical_block_size,
                             drive_blk_addr,
                             drive_num_blocks,
                             serialized_drive_data,
                             serialized_len
                             );
                 };
}

void IoTaskOrdererTest::tearDown() {
    // STUB for uninitializing any test specific
    // variables
}

void IoTaskOrdererTest::test_integrity() {
    
    uint64_t meta_lba = 1;
    uint64_t meta_num_blocks = 1;
    uint64_t drive_smallest_block_size = 4096;
    uint64_t logical_block_size = 4096;
    uint64_t drive_blk_addr = 0;
    uint64_t drive_num_blocks = 0;
    dss_blk_allocator_status_t status = BLK_ALLOCATOR_STATUS_ERROR;
    void *serialized_drive_data = nullptr;
    uint64_t serialized_len = 0;

    status = io_task_orderer_->translate_meta_to_drive_data(
            meta_lba, meta_num_blocks,
            drive_smallest_block_size,
            logical_block_size,
            drive_blk_addr,
            drive_num_blocks,
            &serialized_drive_data,
            serialized_len
            );

    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    status = io_task_orderer_->mark_dirty_meta(10, 100);

    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

}

void IoTaskOrdererTest::test_queue_sync_meta_io_tasks() {

    // Create a dummy IO task reference
    dss_io_task_t *io_task = nullptr;
    dss_blk_allocator_status_t status = BLK_ALLOCATOR_STATUS_ERROR;
    uint64_t dirty_lb = 5000;
    uint64_t dirty_num_blks = 1000;

    // Mark few lbas dirty
    // (This will be invoked during alloc and free)
    status = io_task_orderer_->mark_dirty_meta(dirty_lb, dirty_num_blks);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    // Queue the IO task for persisting state to disk
    status = io_task_orderer_->queue_sync_meta_io_tasks(io_task);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

}

void IoTaskOrdererTest::test_get_next_submit_meta_io_tasks() {

    // Create a dummy IO task reference
    dss_io_task_t *io_task = nullptr;
    dss_blk_allocator_status_t status = BLK_ALLOCATOR_STATUS_ERROR;
    uint64_t dirty_lb = 5000;
    uint64_t dirty_num_blks = 1000;

    // Check for iteration end first
    status = io_task_orderer_->get_next_submit_meta_io_tasks(&io_task);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_ITERATION_END);

    // Queue some IO task
    // Mark few lbas dirty
    // (This will be invoked during alloc and free)
    status = io_task_orderer_->mark_dirty_meta(dirty_lb, dirty_num_blks);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    // Queue the IO task for persisting state to disk
    status = io_task_orderer_->queue_sync_meta_io_tasks(io_task);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    // Get the next IO task to schedule to IO module
    status = io_task_orderer_->get_next_submit_meta_io_tasks(&io_task);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);
}

void IoTaskOrdererTest::test_complete_meta_sync() {

    // Create a dummy IO task reference
    dss_io_task_t *io_task_out = nullptr;
    dss_io_task_t *io_task1 = nullptr;
    dss_io_task_t *io_task2 = nullptr;
    dss_blk_allocator_status_t status = BLK_ALLOCATOR_STATUS_ERROR;
    // NB: This is currently a stub for populate_io_ranges, that
    // sets dirty for these exact values
    uint64_t dirty_lb = 10;
    uint64_t dirty_num_blks = 100;
    // Queue first IO task
    // Mark few lbas dirty
    // (This will be invoked during alloc and free)
    status = io_task_orderer_->mark_dirty_meta(dirty_lb, dirty_num_blks);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    // Queue the first IO task for persisting state to disk
    status = io_task_orderer_->queue_sync_meta_io_tasks(io_task1);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    // Queue the second IO task for persisting state to disk
    status = io_task_orderer_->mark_dirty_meta(dirty_lb, dirty_num_blks);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    status = io_task_orderer_->queue_sync_meta_io_tasks(io_task1);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    // Now getting first IO task should be OK
    status = io_task_orderer_->get_next_submit_meta_io_tasks(&io_task2);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    // Getting the next IO task should fail due to overlapping range
    status = io_task_orderer_->get_next_submit_meta_io_tasks(&io_task_out);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_ITERATION_END);

    // Now mark IO 1 completed
    status = io_task_orderer_->complete_meta_sync(io_task1);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

    // Querying for the next IO task should succeed
    status = io_task_orderer_->get_next_submit_meta_io_tasks(&io_task_out);
    CPPUNIT_ASSERT(status == BLK_ALLOCATOR_STATUS_SUCCESS);

}

int main() {

    /**
     * Main driver for CPPUNIT framework
     */
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(IoTaskOrdererTest::suite());
	runner.run();
	return 0;

}
