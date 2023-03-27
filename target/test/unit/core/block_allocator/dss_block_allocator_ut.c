/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
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
#include <sys/queue.h>
#include "CUnit/Basic.h"
#include "dss_block_allocator_apis.h"

#include <assert.h>

struct ba_tc_entry_s {
    char *tc_name;
    CU_pTest cu_fn;
    TAILQ_ENTRY(ba_tc_entry_s) entry_link;
};

struct block_allocator_ut {
    char *ba_type;
    dss_blk_allocator_opts_t opts;
    dss_blk_allocator_context_t *ctx;
    TAILQ_HEAD(, ba_tc_entry_s) tc_entries;
} g_ba_ut;

struct tc_list_arr_s {
    const char *tc_name;
    CU_TestFunc tc_func;
};

extern struct tc_list_arr_s g_ba_test_cases[];

// Test case start

#define DSS_BA_UT_DEFAULT_BLOCK_INDEX (0)
#define DSS_BA_UT_NEXT_STATE(x) (x + 1)

#define DSS_BA_UT_FIRST_BLOCK (0)

void testInitFailure(void)
{
    dss_blk_allocator_context_t *c;
    dss_blk_allocator_opts_t config;

    dss_blk_allocator_set_default_config(NULL, &config);

    config.blk_allocator_type = NULL;
    c = dss_blk_allocator_init(NULL, &config);
    CU_ASSERT(c == NULL);

    config.blk_allocator_type = "Test-error-type";
    c = dss_blk_allocator_init(NULL, &config);
    CU_ASSERT(c == NULL);

    dss_blk_allocator_set_default_config(NULL, &config);
    if(strcmp(config.blk_allocator_type, "simbmap_allocator")) {
        //Check not null device
        c = dss_blk_allocator_init((dss_device_t *)-1, &config);
        CU_ASSERT(c == NULL);
    }
    return;
}

void testAllStates(void)
{
    uint64_t state_to_set;
    uint64_t state_retrieved;
    dss_blk_allocator_status_t rc;
    uint64_t test_blk_index = DSS_BA_UT_DEFAULT_BLOCK_INDEX;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;
    bool is_free;

    rc = dss_blk_allocator_clear_blocks(c, test_blk_index, 1);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

    for(state_to_set = DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE); state_to_set <= g_ba_ut.opts.num_block_states; state_to_set++)
    {
        rc = dss_blk_allocator_set_blocks_state(c, test_blk_index, 1, state_to_set);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

        rc = dss_blk_allocator_get_block_state(c, test_blk_index, &state_retrieved);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

        CU_ASSERT(state_retrieved == state_to_set);
    }

    rc = dss_blk_allocator_set_blocks_state(c, test_blk_index, 1, state_to_set);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_INVALID_BLOCK_STATE);

    rc = dss_blk_allocator_clear_blocks(c, test_blk_index, 1);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

    rc = dss_blk_allocator_is_block_free(c, test_blk_index, &is_free);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
    CU_ASSERT(is_free == true);

}

void testSetAllBlockState(void)
{
    uint64_t block_index;
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;
    uint64_t state_to_set = DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE);
    uint64_t state_to_check = (DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE);

    for(block_index = DSS_BA_UT_FIRST_BLOCK; block_index < g_ba_ut.opts.num_total_blocks; block_index++) {
        rc =dss_blk_allocator_set_blocks_state(c, block_index, 1, state_to_set);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
    }

    return;
}

void testCheckAllBlockState(void)
{
    uint64_t block_index;
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;
    uint64_t state_to_check = DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE;

    for(block_index = DSS_BA_UT_FIRST_BLOCK; block_index < g_ba_ut.opts.num_total_blocks; block_index++)
    {
        rc = dss_blk_allocator_get_block_state(c, block_index, &state_to_check);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
        CU_ASSERT(state_to_check == DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE));
    }

    return;
}

void testClearAllBlocks(void)
{
    uint64_t block_index;
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;

    for(block_index = DSS_BA_UT_FIRST_BLOCK; block_index < g_ba_ut.opts.num_total_blocks; block_index++)
    {
        rc = dss_blk_allocator_clear_blocks(c, block_index, 1);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
    }

    return;
}

void testRangeSetAllBlockState(void)
{
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;
    uint64_t state_to_set = DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE);
    uint64_t scanned_index;

    rc =dss_blk_allocator_set_blocks_state(c, DSS_BA_UT_FIRST_BLOCK, g_ba_ut.opts.num_total_blocks, state_to_set);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

    return;
}

void testRangeCheckAllBlockState(void)
{
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;
    uint64_t state_to_check = DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE);
    uint64_t scanned_index;

    rc = dss_blk_allocator_check_blocks_state(c, DSS_BA_UT_FIRST_BLOCK, g_ba_ut.opts.num_total_blocks, state_to_check, &scanned_index);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

    return;
}

void testRangeClearAllBlocks(void)
{
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;

    rc = dss_blk_allocator_clear_blocks(c, DSS_BA_UT_FIRST_BLOCK, g_ba_ut.opts.num_total_blocks);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

    return;
}

void testAllBlocksFree(void)
{
    uint64_t block_index;
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;
    bool is_free;

    for(block_index = DSS_BA_UT_FIRST_BLOCK; block_index < g_ba_ut.opts.num_total_blocks; block_index++)
    {
        rc = dss_blk_allocator_is_block_free(c, block_index, &is_free);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
        CU_ASSERT(is_free == true);
    }

    return;
}

void testMultiStride(void)
{
    uint64_t state_to_set = DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE);
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;
    uint64_t scanned_index;

    for(state_to_set = DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE); state_to_set < g_ba_ut.opts.num_block_states; state_to_set++) {
        rc = dss_blk_allocator_clear_blocks(c, DSS_BA_UT_FIRST_BLOCK, g_ba_ut.opts.num_total_blocks);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

        rc = dss_blk_allocator_alloc_blocks_contig(c, state_to_set, DSS_BA_UT_FIRST_BLOCK, g_ba_ut.opts.num_total_blocks, NULL);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

        rc = dss_blk_allocator_check_blocks_state(c, DSS_BA_UT_FIRST_BLOCK, g_ba_ut.opts.num_total_blocks, state_to_set, &scanned_index);
        CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);

        rc = dss_blk_allocator_alloc_blocks_contig(c, DSS_BA_UT_NEXT_STATE(state_to_set), DSS_BA_UT_FIRST_BLOCK, g_ba_ut.opts.num_total_blocks, NULL);
        CU_ASSERT(rc != BLK_ALLOCATOR_STATUS_SUCCESS);
    }

    return;
}

#define DSS_BA_ALLOC_NUM_BLOCKS (16)
#define DSS_BA_UT_START_BINDEX (8)

void testRanges(void)
{
    uint64_t allocated_block;
    uint64_t bindex;
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;

    bindex = DSS_BA_UT_START_BINDEX;
    rc = dss_blk_allocator_alloc_blocks_contig(c, DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE), bindex, DSS_BA_ALLOC_NUM_BLOCKS, &allocated_block);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
    CU_ASSERT(allocated_block == bindex);

    bindex = g_ba_ut.opts.num_total_blocks - DSS_BA_ALLOC_NUM_BLOCKS - 1;
    rc = dss_blk_allocator_alloc_blocks_contig(c, DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE), bindex, DSS_BA_ALLOC_NUM_BLOCKS, &allocated_block);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
    CU_ASSERT(allocated_block == bindex);

    bindex = g_ba_ut.opts.num_total_blocks - DSS_BA_ALLOC_NUM_BLOCKS;
    rc = dss_blk_allocator_alloc_blocks_contig(c, DSS_BA_UT_NEXT_STATE(DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE), bindex, DSS_BA_ALLOC_NUM_BLOCKS, &allocated_block);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_SUCCESS);
    CU_ASSERT(allocated_block == (DSS_BA_UT_START_BINDEX + DSS_BA_ALLOC_NUM_BLOCKS));

    return;
}

void testInvalidBlockIndex(void)
{
    uint64_t invalid_block = g_ba_ut.opts.num_total_blocks;
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;

    rc = dss_blk_allocator_is_block_free(c, invalid_block, NULL);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_INVALID_BLOCK_INDEX);

    rc = dss_blk_allocator_get_block_state(c, invalid_block, NULL);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_INVALID_BLOCK_INDEX);

    rc = dss_blk_allocator_alloc_blocks_contig(c, DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE, invalid_block, 1, NULL);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_INVALID_BLOCK_INDEX);

    return;
}

void testInvalidBlockRange(void)
{
    uint64_t invalid_block = g_ba_ut.opts.num_total_blocks - 1;
    dss_blk_allocator_status_t rc;
    dss_blk_allocator_context_t *c = g_ba_ut.ctx;
    uint64_t scanned_index;

    rc = dss_blk_allocator_check_blocks_state(c,invalid_block, 2, DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE, &scanned_index);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_INVALID_BLOCK_RANGE);
    CU_ASSERT(scanned_index == invalid_block - 1);

    rc = dss_blk_allocator_set_blocks_state(c, invalid_block, 2, DSS_BLOCK_ALLOCATOR_BLOCK_STATE_FREE);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_INVALID_BLOCK_RANGE);

    rc = dss_blk_allocator_clear_blocks(c,invalid_block, 2);
    CU_ASSERT(rc == BLK_ALLOCATOR_STATUS_INVALID_BLOCK_RANGE);

    return;
}

//Test Case End

int dss_ba_add_tc_to_suite(CU_pSuite psuite, const char *ba_type, const char *tc_name, CU_TestFunc tc_fn)
{
    char *tc_full_name = NULL;
    struct ba_tc_entry_s *tc = NULL;
    int rc;

    rc = asprintf(&tc_full_name, "%s_%s", ba_type, tc_name);
    if(rc == -1) {
        return rc;
    }

    tc = (struct ba_tc_entry_s *)calloc(1, sizeof(struct ba_tc_entry_s));
    if(!tc) {
        return -1;
    }

    tc->tc_name = tc_full_name;
    tc->cu_fn = CU_add_test(psuite, tc_full_name, tc_fn);
    if(!tc->cu_fn) {
        free(tc);
        printf("%s:%d CU Add test failed [%d]%s", __FILE__, __LINE__, CU_get_error(), CU_get_error_msg());
        return -1;
    }

    TAILQ_INSERT_TAIL(&g_ba_ut.tc_entries, tc, entry_link);
    return 0;
}

void dss_ba_ut_set_default_params(void)
{
    dss_blk_allocator_set_default_config(NULL, &g_ba_ut.opts);
    return;
}

int dss_ba_ut_init(void)
{
    g_ba_ut.opts.blk_allocator_type = g_ba_ut.ba_type;

    g_ba_ut.ctx = dss_blk_allocator_init(NULL, &g_ba_ut.opts);//NULL Device should be supported for in-memory unit tests
    assert(g_ba_ut.ctx != NULL);
    if(!g_ba_ut.ctx) {
        return -1;
    }

    return 0;
}

int dss_ba_ut_cleanup(void)
{
    struct ba_tc_entry_s *tc = NULL;

    dss_blk_allocator_destroy(g_ba_ut.ctx);

    while(tc = TAILQ_FIRST(&g_ba_ut.tc_entries)) {
        TAILQ_REMOVE(&g_ba_ut.tc_entries, tc, entry_link);
        free(tc->tc_name);
    }

    free(g_ba_ut.ba_type);

    return 0;
}

int dss_ba_ut_add_to_suite(CU_pSuite psuite, const char *ba_type, const  struct tc_list_arr_s* ba_test_cases, int tc_count)
{
    int i, rc;

    TAILQ_INIT(&g_ba_ut.tc_entries);
    g_ba_ut.ba_type = strdup(ba_type);

    for(i=0; i < tc_count; i++) {
        rc = dss_ba_add_tc_to_suite(psuite, ba_type, ba_test_cases[i].tc_name, ba_test_cases[i].tc_func);
        if(rc) {
            return -1;
        }
    }

    return 0;
}
