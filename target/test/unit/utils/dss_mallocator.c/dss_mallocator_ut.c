/**
 * The Clear BSD License
 * 
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 * BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "CUnit/Basic.h"

#include "utils/dss_mallocator.h"
#include "utils/dss_item_cache.h"

#include <assert.h>

#define DSS_MA_UT_DEFAULT_NUM_CACHES (1024)
#define DSS_MA_UT_DEFAULT_MAX_CACHE_ITEMS (256)
#define DSS_MA_UT_DEFAULT_ITEM_SIZE (64)
#define DSS_MA_UT_DEFAULT_CB_ARG (0x73ABCD)
#define DSS_MA_UT_DEFAULT_INIT_VAR (75432)

dss_mallocator_ctx_t *g_ma_ctx;
dss_mallocator_opts_t g_ma_opts;


void test_ma_ctor(void *cb_arg, dss_mallocator_item_t *item)
{
    uint64_t *allocated_var = item;

    CU_ASSERT(item != NULL);
    CU_ASSERT(cb_arg == DSS_MA_UT_DEFAULT_CB_ARG);
    CU_ASSERT(*allocated_var == 0);

    *allocated_var = DSS_MA_UT_DEFAULT_INIT_VAR;

    return;
}

void test_ma_dtor(void *cb_arg, dss_mallocator_item_t *item)
{
    uint64_t *allocated_var = item;

    CU_ASSERT(item != NULL);
    CU_ASSERT(cb_arg == DSS_MA_UT_DEFAULT_CB_ARG);
    CU_ASSERT(*allocated_var == DSS_MA_UT_DEFAULT_INIT_VAR);

    return;
}

void testInitAllocatorSuccess(void)
{

    g_ma_opts.item_sz = DSS_MA_UT_DEFAULT_ITEM_SIZE;
    g_ma_opts.max_per_cache_items = DSS_MA_UT_DEFAULT_MAX_CACHE_ITEMS;
    g_ma_opts.num_caches = DSS_MA_UT_DEFAULT_NUM_CACHES;

    g_ma_ctx = dss_mallocator_init(DSS_MEM_ALLOC_MALLOC, g_ma_opts);
    assert(g_ma_ctx);
    return;
}

void testInitAllocatorWithCbSuccess(void)
{

    g_ma_opts.item_sz = DSS_MA_UT_DEFAULT_ITEM_SIZE;
    g_ma_opts.max_per_cache_items = DSS_MA_UT_DEFAULT_MAX_CACHE_ITEMS;
    g_ma_opts.num_caches = DSS_MA_UT_DEFAULT_NUM_CACHES;

    g_ma_ctx = dss_mallocator_init_with_cb(DSS_MEM_ALLOC_MALLOC, g_ma_opts, test_ma_ctor, test_ma_dtor, DSS_MA_UT_DEFAULT_CB_ARG);
    assert(g_ma_ctx);
    return;
}

void testInitAllocatorFail(void)
{
    dss_mallocator_opts_t opts;

    g_ma_opts.item_sz = DSS_MA_UT_DEFAULT_ITEM_SIZE;
    g_ma_opts.max_per_cache_items = DSS_MA_UT_DEFAULT_MAX_CACHE_ITEMS;
    g_ma_opts.num_caches = DSS_MA_UT_DEFAULT_NUM_CACHES;

    g_ma_opts.num_caches = 0;
    g_ma_ctx = dss_mallocator_init(DSS_MEM_ALLOC_MALLOC, g_ma_opts);
    CU_ASSERT(g_ma_ctx == NULL);

    g_ma_opts.num_caches = DSS_MA_UT_DEFAULT_NUM_CACHES;
    g_ma_opts.item_sz = 0;
    g_ma_ctx = dss_mallocator_init(DSS_MEM_ALLOC_MALLOC, g_ma_opts);
    CU_ASSERT(g_ma_ctx == NULL);

    g_ma_opts.item_sz = DSS_MA_UT_DEFAULT_ITEM_SIZE;

    return;
}

void testAllocandFree2xMax(void)
{
    int i, j;
    dss_mallocator_status_t rc;
    void **parr[(2 * g_ma_opts.max_per_cache_items)];
    for(i=0; i < g_ma_opts.num_caches; i++) {
        for(j=0; j < 2 * g_ma_opts.max_per_cache_items; j++) {
            rc = dss_mallocator_get(g_ma_ctx, i, &parr[j]);
            CU_ASSERT(parr[j] != NULL);
            CU_ASSERT(rc == DSS_MALLOC_NEW_ALLOCATION);
        }
        for(j=0; j < 2 * g_ma_opts.max_per_cache_items; j++) {
            rc = dss_mallocator_put(g_ma_ctx, i, parr[j]);
            CU_ASSERT(rc == DSS_MALLOC_SUCCESS);
        }
    }
}

void testAlloc2xMax(void)
{
    int i, j;
    dss_mallocator_status_t rc;
    void **parr[(2 * g_ma_opts.max_per_cache_items)];
    for(i=0; i < g_ma_opts.num_caches; i++) {
        for(j=0; j < 2 * g_ma_opts.max_per_cache_items; j++) {
            rc = dss_mallocator_get(g_ma_ctx, i, &parr[j]);
            CU_ASSERT(parr[j] != NULL);
            CU_ASSERT(rc == DSS_MALLOC_NEW_ALLOCATION);
        }
    }
}

void testDestructMA(void)
{
    dss_mallocator_status_t rc;
    rc = dss_mallocator_destroy(g_ma_ctx);
    CU_ASSERT(rc == DSS_MALLOC_SUCCESS);
}

int main()
{
    CU_pSuite pSuite = NULL;

    if (CUE_SUCCESS != CU_initialize_registry())
    {
        return CU_get_error();
    }

    pSuite = CU_add_suite("New Test", NULL, NULL);
    if (NULL == pSuite)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (
        NULL == CU_add_test(pSuite, "testInitAllocatorFail", testInitAllocatorFail) ||
        NULL == CU_add_test(pSuite, "testInitAllocatorSuccess", testInitAllocatorSuccess) ||
        NULL == CU_add_test(pSuite, "testAlloc2xMax", testAlloc2xMax) ||
        NULL == CU_add_test(pSuite, "testDestructMA_withelements", testDestructMA) ||
        NULL == CU_add_test(pSuite, "testInitAllocatorSuccess1", testInitAllocatorSuccess) ||
        NULL == CU_add_test(pSuite, "testAllocandFree2xMax", testAllocandFree2xMax) ||
        NULL == CU_add_test(pSuite, "testDestructMA", testDestructMA) ||
        NULL == CU_add_test(pSuite, "testInitAllocatorWithCbSuccess", testInitAllocatorWithCbSuccess) ||
        NULL == CU_add_test(pSuite, "testAlloc2xMaxWithCb", testAlloc2xMax) ||
        NULL == CU_add_test(pSuite, "testDestructMA_withelementsWithCb", testDestructMA) ||
        NULL == CU_add_test(pSuite, "testInitAllocatorWithCbSuccess1", testInitAllocatorWithCbSuccess) ||
        NULL == CU_add_test(pSuite, "testAllocandFree2xMaxWithCb1", testAllocandFree2xMax) ||
        NULL == CU_add_test(pSuite, "testDestructMAWithCb1", testDestructMA))

    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return CU_get_error();
}
