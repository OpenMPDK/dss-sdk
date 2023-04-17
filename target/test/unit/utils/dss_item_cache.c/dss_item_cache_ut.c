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

#include <stdlib.h>
#include "CUnit/Basic.h"

#include "utils/dss_item_cache.h"

dss_item_cache_context_t *item_cache;
int capacity = 1;


void testCreateCacheCapNeg(void)
{
    item_cache = dss_item_cache_init(-1);
    CU_ASSERT(NULL == item_cache);
    return;
}

void testCreateCacheCap0(void)
{
    item_cache = dss_item_cache_init(0);
    CU_ASSERT(NULL == item_cache);
    return;
}

void testCreateCap1(void)
{
    capacity = 1;
    item_cache = dss_item_cache_init(capacity);
    CU_ASSERT(NULL != item_cache);
}

void testFreeNULLCache(void)
{
    int ret;
    ret = dss_item_cache_free(NULL);
    CU_ASSERT(-1 == ret);
    return;
}

void testFreeCache(void)
{
    int ret;
    ret = dss_item_cache_free(item_cache);
    CU_ASSERT(0 == ret);
    return;
}

void testOneItem(void)
{
    int a[1] = {1};
    int *item, ret;
    item = (int *) dss_item_cache_get_item(item_cache);
    CU_ASSERT(NULL == item);
    ret = dss_item_cache_put_item(item_cache, &a);
    CU_ASSERT(0 == ret);
    item = (int *)dss_item_cache_get_item(item_cache);
    CU_ASSERT(a == item);
    CU_ASSERT(1 == *item);
    ret = dss_item_cache_put_item(item_cache, &a);
    CU_ASSERT(0 == ret);
    ret = dss_item_cache_put_item(item_cache, &a);
    CU_ASSERT(-1 == ret);
    item = (int *)dss_item_cache_get_item(item_cache);
    CU_ASSERT(a == item);
    CU_ASSERT(1 == *item);
    item = (int *) dss_item_cache_get_item(item_cache);
    CU_ASSERT(NULL == item);

    return;
}

int arr[10];// Create max capacity used in test

void testInsertItem1(void)
{
    int ret;
    arr[0] = 1;
    ret = dss_item_cache_put_item(item_cache, &arr);
    CU_ASSERT(0 == ret);
}

void testFreeNonEmpty(void)
{
    testFreeNULLCache();//Reuse but use different function for clarity 
}

void testPopItem1(void)
{
    int ret, *item;
    item = (int *)dss_item_cache_get_item(item_cache);
    CU_ASSERT(arr == item);
    CU_ASSERT(1 == *item);

    return;
}

void testNULLItem(void)
{
    int ret;
    ret = dss_item_cache_put_item(item_cache, NULL);
    CU_ASSERT(-1 == ret);
}

int main( )
{
    CU_pSuite pSuite = NULL;

    if(CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    pSuite = CU_add_suite("DSS MP Cache", NULL, NULL);
    if(NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if(
        NULL == CU_add_test(pSuite, "testCreateCacheCapNeg", testCreateCacheCapNeg) ||
        NULL == CU_add_test(pSuite, "testCreateCacheCap0", testCreateCacheCap0) ||
        NULL == CU_add_test(pSuite, "testFreeNULLCache", testFreeNULLCache) ||
        NULL == CU_add_test(pSuite, "testCreateCap1", testCreateCap1) ||
        NULL == CU_add_test(pSuite, "testOneItem", testOneItem) ||
        NULL == CU_add_test(pSuite, "testNULLItem", testNULLItem) ||
        NULL == CU_add_test(pSuite, "testInsertItem1", testInsertItem1) ||
        NULL == CU_add_test(pSuite, "testFreeNonEmpty", testFreeNonEmpty) ||
        NULL == CU_add_test(pSuite, "testPopItem1", testPopItem1) ||
        NULL == CU_add_test(pSuite, "testFreeCache", testFreeCache)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return CU_get_error();
}
