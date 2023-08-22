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
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "CUnit/Basic.h"
#include "kvtrans.h"
#include "keygen.h"

#define KEY_LEN 1024
#define VAL_LEN 4096 * 3
#define KEY_BATCH 10000
#define KEY_NUM 100000
#define DEFAULT_DEV 0x34acb239
#define DEFAULT_IOTM 0xffffffff
#undef BLK_NUM
#define BLK_NUM 4000000 / 8

typedef struct key_generator_s key_generator_t;
typedef struct obj_key_s obj_key_t;

struct kvtrans_ut {
    int hash_type;
    int hash_size;
    kvtrans_ctx_t *ctx;
    key_generator_t *kg;
    req_t req_list[KEY_BATCH];
    kvtrans_params_t *params;
} g_kvtrans_ut;

typedef struct obj_key_s {
    int key_len;
    char key[KEY_LEN];
    uint32_t loop_cnt;
    uint32_t lba;
    uint8_t rejected;
} obj_key_t;

typedef struct key_generator_s {
    uint64_t key_cnt;
    uint64_t key_fail_cnt;
    uint64_t key_inserted;
    int num_key;
    int key_batch;
    obj_key_t *obj_key_list;
    void (*run)(key_generator_t *, int);
} key_generator_t;

extern bool g_disk_as_data_store;

void object_key_generate(key_generator_t *kg, int key_len)
{   
    int i;
    for (i=0;  i<kg->key_batch; i++) {
        dss_keygen_next_key(kg->obj_key_list[i].key);
        if (kg->obj_key_list[i].key)
        {   
           kg->key_cnt++;
        } else {
            kg->key_fail_cnt++;
        }
    }
}

key_generator_t *init_key_generator(int num_key, int key_batch) {
    key_generator_t *kg;
    kg = (key_generator_t *) malloc (sizeof (key_generator_t));
    if (!kg) {
        printf("ERROR: malloc kg failed.\n");
        exit(1);
    }
    kg->key_cnt = 0;
    kg->key_fail_cnt = 0;
    kg->key_inserted = 0;
    kg->num_key = num_key;
    kg->key_batch = key_batch;
    kg->run = &object_key_generate;
    kg->obj_key_list = (obj_key_t *) malloc (key_batch * sizeof(obj_key_t));
    if (!kg->obj_key_list) {
        printf("ERROR: malloc kg->obj_key_list failed.\n");
        exit(1);
    }
    memset(kg->obj_key_list, 0, key_batch * sizeof(obj_key_t));
    dss_keygen_init(KEY_LEN);
    return kg;
}

void reset_key_generator(key_generator_t *kg) {
    kg->key_cnt = 0;
    kg->key_fail_cnt = 0;
    kg->key_inserted = 0;
    memset(kg->obj_key_list, 0, kg->key_batch * sizeof(obj_key_t));
}

void free_key_generator(key_generator_t *kg) {
    free(kg->obj_key_list);
    free(kg);
}

void init_params(kvtrans_params_t *params) {
    params->id = 0;
    params->thread_num = 1;
    params->name = "kvtrans_ut";
    params->hash_type = spooky;
    params->hash_size = 0;
    params->total_blk_num = BLK_NUM;
    params->meta_blk_num = KEY_NUM;
    params->blk_alloc_name = "block_impresario";
    params->dev = DEFAULT_DEV;
    params->iotm = DEFAULT_IOTM;
}

void init_test_ctx() {
    g_kvtrans_ut.params = (kvtrans_params_t *) malloc (sizeof(kvtrans_params_t));
    if (!g_kvtrans_ut.params) {
        printf("ERROR: malloc g_kvtrans_ut.params failed.\n");
        exit(1);
    }
    init_params(g_kvtrans_ut.params);
    g_kvtrans_ut.kg = init_key_generator(KEY_NUM, KEY_BATCH);
}

void free_test_ctx() {
    free(g_kvtrans_ut.params);
    free_key_generator(g_kvtrans_ut.kg);
}

void testInitCase(void)
{
    g_kvtrans_ut.ctx = init_kvtrans_ctx(NULL);
    kvtrans_ctx_t *ctx = g_kvtrans_ut.ctx;
    CU_ASSERT(ctx!=NULL);
    CU_ASSERT(ctx->hash_fn_ctx!=NULL);
    CU_ASSERT(ctx->blk_alloc_ctx!=NULL);
}

void testInitParaCase(void)
{
    if (!g_kvtrans_ut.params) {
        g_kvtrans_ut.params = (kvtrans_params_t *) malloc (sizeof(kvtrans_params_t));
        if (!g_kvtrans_ut.params) {
            printf("ERROR: malloc g_kvtrans_ut.params failed.\n");
            exit(1);
        }
        init_params(g_kvtrans_ut.params);
    }
    g_kvtrans_ut.ctx = init_kvtrans_ctx(g_kvtrans_ut.params);
    kvtrans_ctx_t *ctx = g_kvtrans_ut.ctx;
    CU_ASSERT(ctx!=NULL);
    CU_ASSERT(ctx->hash_fn_ctx!=NULL);
    CU_ASSERT(ctx->blk_alloc_ctx!=NULL);
}


void construct_test_dfly_request(char *key, void *value, kv_op_t opc, req_t *req) {
    CU_ASSERT(key!=NULL);
    req->req_key.length = KEY_LEN;
    memcpy(req->req_key.key, key, req->req_key.length);
    if (value) {
        req->req_value.value = value;
        req->req_value.length = VAL_LEN;
    }
    req->opc = opc;
}

void testSuccessFlow(void)
{
    req_t req;
    char *k = "keySuccess";
    void *v = malloc(VAL_LEN);//Sample data - opaque pointer will be stored
    CU_ASSERT(v!=NULL);
    memcpy(v, k, strlen(k));
    dss_kvtrans_status_t  rc;
    construct_test_dfly_request(k, v, KVTRANS_OPC_STORE, &req);
    dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &req);
    rc = kv_process(g_kvtrans_ut.ctx);
    CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS);

    construct_test_dfly_request(k, v, KVTRANS_OPC_EXIST, &req);
    dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &req);
    rc = kv_process(g_kvtrans_ut.ctx);
    CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS);

    // memset(req.req_value.value, 0, req.req_value.length);
    // req.req_value.length = 0;
    construct_test_dfly_request(k, v, KVTRANS_OPC_RETRIEVE, &req);
    dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &req);
    rc = kv_process(g_kvtrans_ut.ctx);
    CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS);
    // CU_ASSERT(req.req_value.length == VAL_LEN);
    // CU_ASSERT(!memcmp(req.req_value.value, v, req.req_value.length));

    construct_test_dfly_request(k, v, KVTRANS_OPC_DELETE, &req);
    dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &req);
    rc = kv_process(g_kvtrans_ut.ctx);
    CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS);

    construct_test_dfly_request(k, v, KVTRANS_OPC_EXIST, &req);
    dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &req);
    rc = kv_process(g_kvtrans_ut.ctx);
    CU_ASSERT(rc == KVTRANS_STATUS_NOT_FOUND);

    reset_mem_backend(g_kvtrans_ut.ctx);
}

void testCollision(void)
{
    char *k;
    void *v = malloc(VAL_LEN);//Sample data - opaque pointer will be stored
    CU_ASSERT(v!=NULL);
    dss_kvtrans_status_t  rc;
    char folder[128];
    char name[128];
    int b = 0;
    int idx = 0;
    int i;

    while (g_kvtrans_ut.kg->num_key>g_kvtrans_ut.kg->key_cnt) {
        idx = 0;
        g_kvtrans_ut.kg->run(g_kvtrans_ut.kg, KEY_LEN);
        /* store */
        for(i=0; i<g_kvtrans_ut.kg->key_batch; i++) {
            k = g_kvtrans_ut.kg->obj_key_list[i].key;
            construct_test_dfly_request(k, v, KVTRANS_OPC_STORE, &g_kvtrans_ut.req_list[i]);
            dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &g_kvtrans_ut.req_list[i]);
        }
        do {
            rc = kv_process(g_kvtrans_ut.ctx);
            assert(rc == KVTRANS_STATUS_SUCCESS || rc==KVTRANS_STATUS_FREE);
            CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS || rc==KVTRANS_STATUS_FREE);
            idx++;
        } while(rc != KVTRANS_STATUS_FREE);
        printf("batch %d finishes\n", b);
        b++;
    }
    reset_mem_backend(g_kvtrans_ut.ctx);
    reset_key_generator(g_kvtrans_ut.kg);
}

void testBatchDelete(void)
{
    char *k;
    void *v = malloc(VAL_LEN);//Sample data - opaque pointer will be stored
    CU_ASSERT(v!=NULL);
    dss_kvtrans_status_t  rc;
    char folder[128];
    char name[128];
    int b = 0;
    int idx = 0;
    int i;

    while (g_kvtrans_ut.kg->num_key>g_kvtrans_ut.kg->key_cnt) {
        idx = 0;
        g_kvtrans_ut.kg->run(g_kvtrans_ut.kg, KEY_LEN);
        /* store */
        for(i=0; i<g_kvtrans_ut.kg->key_batch; i++) {
            k = g_kvtrans_ut.kg->obj_key_list[i].key;
            construct_test_dfly_request(k, v, KVTRANS_OPC_STORE, &g_kvtrans_ut.req_list[i]);
            dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &g_kvtrans_ut.req_list[i]);
        }
        do {
            rc = kv_process(g_kvtrans_ut.ctx);
            CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS || rc==KVTRANS_STATUS_FREE);
            idx++;
        } while(rc != KVTRANS_STATUS_FREE);
        /* exist */
        for(i=0; i<g_kvtrans_ut.kg->key_batch; i++) {
            k = g_kvtrans_ut.kg->obj_key_list[i].key;
            construct_test_dfly_request(k, v, KVTRANS_OPC_EXIST, &g_kvtrans_ut.req_list[i]);
            dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &g_kvtrans_ut.req_list[i]);
        }
        do {
            rc = kv_process(g_kvtrans_ut.ctx);
            CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS || rc==KVTRANS_STATUS_FREE);
            idx++;
        } while(rc != KVTRANS_STATUS_FREE);
        /* delete */
        for(i=0; i<g_kvtrans_ut.kg->key_batch; i++) {
            k = g_kvtrans_ut.kg->obj_key_list[i].key;
            construct_test_dfly_request(k, v, KVTRANS_OPC_DELETE, &g_kvtrans_ut.req_list[i]);
            dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &g_kvtrans_ut.req_list[i]);
        }
        do {
            rc = kv_process(g_kvtrans_ut.ctx);
            CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS || rc==KVTRANS_STATUS_FREE);
            idx++;
        } while(rc != KVTRANS_STATUS_FREE);
        /* exist */
        for(i=0; i<g_kvtrans_ut.kg->key_batch; i++) {
            k = g_kvtrans_ut.kg->obj_key_list[i].key;
            construct_test_dfly_request(k, v, KVTRANS_OPC_EXIST, &g_kvtrans_ut.req_list[i]);
            dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &g_kvtrans_ut.req_list[i]);
        }
        do {
            rc = kv_process(g_kvtrans_ut.ctx);
            CU_ASSERT(rc == KVTRANS_STATUS_NOT_FOUND || rc==KVTRANS_STATUS_FREE);
            idx++;
        } while(rc != KVTRANS_STATUS_FREE);
        printf("batch %d finishes\n", b);
        b++;
    }
    reset_mem_backend(g_kvtrans_ut.ctx);
    reset_key_generator(g_kvtrans_ut.kg);
}


void testFullDelete(void)
{
    char *k;
    void *v = malloc(VAL_LEN);//Sample data - opaque pointer will be stored
    CU_ASSERT(v!=NULL);
    dss_kvtrans_status_t  rc;
    obj_key_t *obj_keys;
    req_t *reqs;
    char folder[128];
    char name[128];
    int b = 0;
    int idx = 0;
    int i;

    obj_keys = (obj_key_t *) malloc(sizeof(obj_key_t) * KEY_NUM);
    reqs = (req_t *) malloc(sizeof(req_t) * KEY_NUM);
    CU_ASSERT(obj_keys!=NULL && reqs!=NULL);
    while (g_kvtrans_ut.kg->num_key>g_kvtrans_ut.kg->key_cnt) {
        idx = 0;
        g_kvtrans_ut.kg->run(g_kvtrans_ut.kg, KEY_LEN);
        // memcpy(&reqs[b*KEY_BATCH], g_kvtrans_ut.kg->obj_key_list, sizeof(req_t) * KEY_BATCH);
        /* store */
        for(i=0; i<g_kvtrans_ut.kg->key_batch; i++) {
            k = g_kvtrans_ut.kg->obj_key_list[i].key;
            obj_keys[b*KEY_BATCH+i] = g_kvtrans_ut.kg->obj_key_list[i];
            construct_test_dfly_request(k, v, KVTRANS_OPC_STORE, &g_kvtrans_ut.req_list[i]);
            dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &g_kvtrans_ut.req_list[i]);
        }
        do {
            rc = kv_process(g_kvtrans_ut.ctx);
            CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS || rc==KVTRANS_STATUS_FREE);
            idx++;
        } while(rc != KVTRANS_STATUS_FREE);
        printf("batch %d finishes\n", b);
        b++;
    }

    
    idx = 0;
    for(i=0; i<KEY_NUM; i++) {
        k = obj_keys[i].key;
        construct_test_dfly_request(k, v, KVTRANS_OPC_DELETE, &reqs[i]);
        dss_kvtrans_handle_request(g_kvtrans_ut.ctx, &reqs[i]);
    }
    do {
        rc = kv_process(g_kvtrans_ut.ctx);
        assert(rc == KVTRANS_STATUS_SUCCESS || rc==KVTRANS_STATUS_FREE);
        CU_ASSERT(rc == KVTRANS_STATUS_SUCCESS || rc==KVTRANS_STATUS_FREE);
        idx++;
    } while(rc != KVTRANS_STATUS_FREE);

    free(reqs);
    free(obj_keys);
    reset_mem_backend(g_kvtrans_ut.ctx);
    reset_key_generator(g_kvtrans_ut.kg);
}


int main( )
{
    CU_pSuite pSuite = NULL;

    if(CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }
    g_disk_as_data_store = false;
    init_test_ctx();

    pSuite = CU_add_suite("DSS kv translator", NULL, NULL);
    if(NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if(
        // NULL == CU_add_test(pSuite, "testInitCase" ,  testInitCase)
        NULL == CU_add_test(pSuite, "testInitParaCase" ,  testInitParaCase)
        || NULL == CU_add_test(pSuite, "testSuccessFlow" ,  testSuccessFlow)
        // || NULL == CU_add_test(pSuite, "testCollision" ,  testCollision) // included in testFullDelete
        || NULL == CU_add_test(pSuite, "testBatchDelete" ,  testBatchDelete)
        || NULL == CU_add_test(pSuite, "testFullDelete" ,  testFullDelete)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    free_test_ctx();
    return CU_get_error();
}
