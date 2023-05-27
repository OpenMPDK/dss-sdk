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
#include <assert.h>
#include <chrono>

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


class BitmapTest : public CppUnit::TestFixture {
public:
    BitmapTest() {
        // jso is created during block allocator init
        BlockAlloc::JudySeekOptimizerSharedPtr jso =
            std::make_shared<BlockAlloc::
            JudySeekOptimizer>(16348, 0, 128);

        total_cells = 10;
        perf_total_cells = 65536;
        bits_per_cell = 4;
        num_block_states = 5;
        bmap = std::make_shared<AllocatorType::QwordVector64Cell>(
            jso, total_cells, bits_per_cell, num_block_states); 
        perf_bmap = std::make_shared<AllocatorType::QwordVector64Cell>(
            jso, perf_total_cells, bits_per_cell, num_block_states); 
    }
    void setUp();
    void tearDown();

    void test_bitmap_integrity();
    void test_set_get();
    void test_set_get_32bit_offset();
    void test_set_get_all();
    void test_bitmap_empty_range();
    void test_perf_bitmap();

    CPPUNIT_TEST_SUITE(BitmapTest);
    CPPUNIT_TEST(test_bitmap_integrity);
    CPPUNIT_TEST(test_set_get);
    CPPUNIT_TEST(test_set_get_32bit_offset);
    CPPUNIT_TEST(test_set_get_all);
    CPPUNIT_TEST(test_bitmap_empty_range);
    CPPUNIT_TEST(test_perf_bitmap);
    CPPUNIT_TEST_SUITE_END();
private:
    AllocatorType::BitMapSharedPtr bmap;
    AllocatorType::BitMapSharedPtr perf_bmap;
    BlockAlloc::JudySeekOptimizerSharedPtr jso_;
    uint64_t total_cells;
    uint64_t perf_total_cells;
    int bits_per_cell;
    uint64_t num_block_states;

};

void BitmapTest::setUp() {
    // STUB for initializing any test specific
    // variables
}

void BitmapTest::tearDown() {
    // STUB for uninitializing any test specific
    // variables
}

/**
 * Tests if the bitmap is constructed per configuration
 */
void BitmapTest::test_bitmap_integrity() {
    CPPUNIT_ASSERT(bmap->total_cells() == total_cells);

    // Total bits required
    int total_bits = total_cells * bits_per_cell;
    // Total uint64_t required
    int total_ints = total_bits/bmap->word_size();
    if (total_bits % bmap->word_size() != 0) {
        total_ints = total_ints + 1;
    }
    CPPUNIT_ASSERT(bmap->total_words() == total_ints);
}

/**
 * - Tests set and get operation on bitmap
 * - Tests visual validation of print_data and print_range API
 */
void BitmapTest::test_set_get() {

    int cell_index = bmap->total_cells() - 5;
    int cell_value = 2;

    bmap->set_cell(cell_index, cell_value);
    int val = bmap->get_cell_value(cell_index);
    CPPUNIT_ASSERT(val == cell_value);

    // Debug hooks
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_data();
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_range(0
                , bmap->total_cells()-1);

    // Clean bitmap by unsetting value for next experiment
    bmap->set_cell(cell_index, 0);
}

/**
 * - Tests set and get operation on bitmap
 * - Tests visual validation of print_data and print_range API
 */
void BitmapTest::test_set_get_32bit_offset() {

    int cell_index = 8;
    int cell_value = 1;
    int cell_index_9 = 9;
    int value = 0;

    bmap->set_cell(cell_index, cell_value);
    // Print bitmap after setting
    // Debug hooks
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_data();
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_range(0
                , bmap->total_cells()-1);
    value = bmap->get_cell_value(cell_index);
    CPPUNIT_ASSERT(value == cell_value);

    // Clean bitmap by unsetting value for next experiment
    bmap->set_cell(cell_index, 0);
    // Print bitmap after unsetting
    // Debug hooks
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_data();
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_range(0
                , bmap->total_cells()-1);

    bmap->set_cell(cell_index_9, cell_value);
    // Print bitmap after setting
    // Debug hooks
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_data();
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_range(0
                , bmap->total_cells()-1);
    value = bmap->get_cell_value(cell_index_9);
    CPPUNIT_ASSERT(value == cell_value);

    // Clean bitmap by unsetting value for next experiment
    bmap->set_cell(cell_index, 0);
    // Print bitmap after unsetting
    // Debug hooks
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_data();
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_range(0
                , bmap->total_cells()-1);
}

/**
 * - Tests set and get all cells operation on bitmap
 * - Tests visual validation of print_data and print_range API
 */
void BitmapTest::test_set_get_all() {

    int value = 0;
    int set_val = 1;
    int clean_val = 0;

    for (uint64_t i=0; i<bmap->total_cells(); i++) {
        bmap->set_cell(i, set_val);
    }

    // Print bitmap after setting it.
    // Debug hooks
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_data();
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_range(0
                , bmap->total_cells()-1);

    // Get all set values
    for (uint64_t i=0; i<bmap->total_cells(); i++) {
        value = bmap->get_cell_value(i);
        CPPUNIT_ASSERT(value == set_val);
    }


    // Clean bitmap by unsetting value for next experiment
    for (uint64_t i=0; i<bmap->total_cells(); i++) {
        bmap->set_cell(i, clean_val);
        value = bmap->get_cell_value(i);
        CPPUNIT_ASSERT(value == clean_val);
    }

    // Print bitmap after unsetting it.
    // Debug hooks
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_data();
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_range(0
                , bmap->total_cells()-1);
}

/**
 * - Tests a particular range is full or empty
 * - This does a linear search
 */
void BitmapTest::test_bitmap_empty_range() {

    int cell_index = 4;
    uint8_t value = 1;

    // Set at arbitrary range a value of 1
    bmap->set_cell(cell_index, value);
    // Seek a range before the set range, should return true
    assert(bmap->seek_empty_cell_range(0, bmap->total_cells()/4));
    // Seek total range, should return false
    assert(!bmap->seek_empty_cell_range(0, bmap->total_cells()));

    // Debug hooks
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_data();
    std::dynamic_pointer_cast
        <AllocatorType::QwordVector64Cell>(bmap)->print_range(0
                , bmap->total_cells()-1);

    // Clean bitmap by unsetting value for next experiment
    bmap->set_cell(cell_index, 0);

}

/**
 * - Tests to perform a linear search of the entire bitmap
 * - This does a scan on the bitmap containing 65536 cells
 *   with 4 bits per cell.
 * - This can be configured on perf_bitmap of the TestSuite
 */
void BitmapTest::test_perf_bitmap() {

	std::cout<<"Time for 1 run to scan -"<<std::endl;
    std::chrono::steady_clock::time_point scan_begin =
        std::chrono::steady_clock::now();
    CPPUNIT_ASSERT(perf_bmap->seek_empty_cell_range(0,bmap->total_cells()));
    std::chrono::steady_clock::time_point scan_end =
        std::chrono::steady_clock::now();
    std::cout << "Time difference for bitmap seek= " <<
        std::chrono::duration_cast<std::chrono::microseconds>
        (scan_end - scan_begin).count() << "[µs]" << std::endl;
    std::cout << "Time difference for bitmap seek= " <<
        std::chrono::duration_cast<std::chrono::nanoseconds>
        (scan_end - scan_begin).count() << "[ns]" << std::endl;
}

int main() {

    /**
     * Main driver for CPPUNIT framework
     */
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(BitmapTest::suite());
	runner.run();
	return 0;

}

