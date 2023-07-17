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
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in
 *        the documentation and/or other materials provided with the distribution.
 *      * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "dss_block_allocator_ut.c"

#define BLOCK_IMPRESARIO_DEFAULT_NUM_BLOCKS (1024)
#define BLOCK_IMPRESARIO_DEFAULT_LOGICAL_START_BLK_OFFSET (32)

struct tc_list_arr_s g_ba_test_cases[] = {
    {"testInitFailure", testInitFailure},
    {"testAllStates", testAllStates},
    {"testSetAllBlockState", testSetAllBlockState},
    {"testCheckAllBlockState", testCheckAllBlockState},
    {"testClearAllBlocks", testClearAllBlocks},
    //<NB: Illegal>{"testRangeSetAllBlockState", testRangeSetAllBlockState},
    {"testRangeCheckAllBlockState", testRangeCheckAllBlockState},
    {"testRangeClearAllBlocks", testRangeClearAllBlocks},
    {"testAllBlocksFree", testAllBlocksFree},
    {"testMultiStride", testMultiStride},
    {"testClearAllBlocks1", testClearAllBlocks},/*Clear previous*/
    {"testRanges", testRanges},
    {"testInvalidBlockIndex", testInvalidBlockIndex},
    {"testInvalidBlockRange", testInvalidBlockRange}
};

int main(int argc, char **argv)
{
    const char *ba_type = "block_impresario";

    dss_ba_ut_set_default_params();
    g_ba_ut.opts.num_total_blocks = BLOCK_IMPRESARIO_DEFAULT_NUM_BLOCKS;
    g_ba_ut.opts.logical_start_block_offset =
        BLOCK_IMPRESARIO_DEFAULT_LOGICAL_START_BLK_OFFSET;

    if(argc == 2) {
        g_ba_ut.opts.num_block_states = atol(argv[1]);
    }

    CU_pSuite pSuite = NULL;

    if(CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    pSuite = CU_add_suite(
            "DSS Block Impresario",
            dss_ba_ut_init,
            dss_ba_ut_cleanup
            );
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if(dss_ba_ut_add_to_suite(
                pSuite, ba_type,
                g_ba_test_cases,
                sizeof(g_ba_test_cases)/sizeof(g_ba_test_cases[0]))
            ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return CU_get_error();
}
