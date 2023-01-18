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
#include <stdlib.h>
#include "CUnit/Basic.h"

#include <stdint.h>

void *dfly_ht_create_table(void);
bool dfly_ht_insert(void *table, char *index_ptr, uint32_t len, void *opaque_ptr);
void *dfly_ht_find(void *table, char *index_ptr, uint32_t len);
void dfly_ht_delete(void *table, char *index_ptr, uint32_t len);


void *ht;

void testSuccessCase(void)
{
    char *k = (char *)"keySuccess";
    int b = 100;//Sample data - opaque pointer will be stored
    bool rc;
    void *rptr;

    CU_ASSERT(ht != NULL);
    rc = dfly_ht_insert(ht, k, strlen(k), &b);
    CU_ASSERT(rc == true);
    rptr = dfly_ht_find(ht, k, strlen(k));
    CU_ASSERT(rptr == &b);
    dfly_ht_delete(ht, k, strlen(k));
    rptr = dfly_ht_find(ht, k, strlen(k));
    CU_ASSERT(rptr == NULL);

    return;
}


void testNegativeCase(void)
{
    char *k = (char *)"keyFail";
    int b = 100;//Sample data - opaque pointer will be stored
    bool rc;
    void *rptr;

    rptr = dfly_ht_find(ht, k, strlen(k));
    CU_ASSERT(rptr == NULL);
    rc = dfly_ht_insert(ht, k, strlen(k), &b);
    CU_ASSERT(rc == true);
    rptr = dfly_ht_find(ht, k, strlen(k));
    CU_ASSERT(rptr == &b);
    dfly_ht_delete(ht, k, strlen(k));
    rptr = dfly_ht_find(ht, k, strlen(k));
    CU_ASSERT(rptr == NULL);
    rc = dfly_ht_insert(ht, k, strlen(k), &b);
    CU_ASSERT(rc == true);
    rptr = dfly_ht_find(ht, k, strlen(k));
    CU_ASSERT(rptr == &b);

    return;
}

void testMultiInsert(void)
{
    char *k1 = (char *)"SampleKey1";
    char *k2 = (char *)"SampleKey2";
    char *k3 = (char *)"SampleKey3";
    char *k4 = (char *)"SampleKey4";
    char *k5 = (char *)"SampleKey5";
    int v1 = 1001;//Sample data - opaque pointer will be stored
    int v2 = 1002;//Sample data - opaque pointer will be stored
    int v3 = 1003;//Sample data - opaque pointer will be stored
    int v4 = 1004;//Sample data - opaque pointer will be stored
    int v5 = 1005;//Sample data - opaque pointer will be stored
    bool rc;
    void *rptr;


    rc = dfly_ht_insert(ht, k1, strlen(k1), &v1);
    CU_ASSERT(rc == true);
    rc = dfly_ht_insert(ht, k2, strlen(k1), &v2);
    CU_ASSERT(rc == true);
    rc = dfly_ht_insert(ht, k3, strlen(k1), &v3);
    CU_ASSERT(rc == true);
    rc = dfly_ht_insert(ht, k4, strlen(k1), &v4);
    CU_ASSERT(rc == true);
    rc = dfly_ht_insert(ht, k5, strlen(k1), &v5);
    CU_ASSERT(rc == true);

    rptr = dfly_ht_find(ht, k1, strlen(k1));
    CU_ASSERT(rptr == &v1);
    rptr = dfly_ht_find(ht, k2, strlen(k2));
    CU_ASSERT(rptr == &v2);
    rptr = dfly_ht_find(ht, k3, strlen(k3));
    CU_ASSERT(rptr == &v3);
    rptr = dfly_ht_find(ht, k4, strlen(k4));
    CU_ASSERT(rptr == &v4);
    rptr = dfly_ht_find(ht, k5, strlen(k5));
    CU_ASSERT(rptr == &v5);

    dfly_ht_delete(ht, k1, strlen(k1));
    dfly_ht_delete(ht, k2, strlen(k2));
    dfly_ht_delete(ht, k3, strlen(k3));
    dfly_ht_delete(ht, k4, strlen(k4));
    dfly_ht_delete(ht, k5, strlen(k5));

    rptr = dfly_ht_find(ht, k1, strlen(k1));
    CU_ASSERT(rptr == NULL);
    rptr = dfly_ht_find(ht, k2, strlen(k2));
    CU_ASSERT(rptr == NULL);
    rptr = dfly_ht_find(ht, k3, strlen(k3));
    CU_ASSERT(rptr == NULL);
    rptr = dfly_ht_find(ht, k4, strlen(k4));
    CU_ASSERT(rptr == NULL);
    rptr = dfly_ht_find(ht, k5, strlen(k5));
    CU_ASSERT(rptr == NULL);

    return;
}

int main( )
{
    CU_pSuite pSuite = NULL;

    //Setup
    ht = dfly_ht_create_table();

    if(CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    pSuite = CU_add_suite("DSS lock service htable", NULL, NULL);
    if(NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if(
        NULL == CU_add_test(pSuite, "testSuccessCase" ,  testSuccessCase) ||
        NULL == CU_add_test(pSuite, "testNegativeCase", testNegativeCase) ||
        NULL == CU_add_test(pSuite, "testMultiInsert", testMultiInsert)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return CU_get_error();
}
