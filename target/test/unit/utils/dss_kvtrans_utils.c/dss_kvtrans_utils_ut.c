#include "kvtrans_utils.h"
#include "CUnit/Basic.h"

#define TEST_ITER 5
#define TEST_ELM_NUM 5000
#define DEFAULT_ELM_SIZE 1024

struct test_str {
    int i[2];
    char c[2];
};

struct g_cache_tbl {
    cache_tbl_t *ctx;
    uint64_t addr_arry[TEST_ELM_NUM];
} g_cache_tbl;

void testExpand(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);

    uint64_t elm_num, free_num;
    int i;

    for(i=0;i<TEST_ITER;i++) {
        elm_num = g_cache_tbl.ctx->elm_num;
        free_num = g_cache_tbl.ctx->free_num;
        _extend_cache_tbl(g_cache_tbl.ctx);
        CU_ASSERT(elm_num * SCALE_CONST == g_cache_tbl.ctx->elm_num);
        CU_ASSERT(free_num * SCALE_CONST == g_cache_tbl.ctx->free_num);
    }    
}

void testShrink(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);
    uint64_t elm_num, free_num;
    int i;

    for(i=0;i<TEST_ITER;i++) {
        elm_num = g_cache_tbl.ctx->elm_num;
        free_num = g_cache_tbl.ctx->free_num;
        _shrink_cache_tbl(g_cache_tbl.ctx);
        CU_ASSERT(elm_num / SCALE_CONST == g_cache_tbl.ctx->elm_num);
        CU_ASSERT(free_num / SCALE_CONST == g_cache_tbl.ctx->free_num);
    }   
}

void testStore(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);
    uint64_t elm_num, free_num;
    uint64_t i;
    int rc;

    for(i=0;i<TEST_ELM_NUM;i++) {
        struct test_str data = {{i, i+1}, "te"};
        rc = store_elm(g_cache_tbl.ctx, i, &data);
        CU_ASSERT(rc==0);
    }   

    elm_num = DEFAULT_ELM_SIZE;
    while (elm_num<TEST_ELM_NUM) {
        elm_num *= 2;
    } 
    free_num = elm_num - TEST_ELM_NUM;

    CU_ASSERT( elm_num == g_cache_tbl.ctx->elm_num);
    CU_ASSERT( free_num == g_cache_tbl.ctx->free_num);
}

void testGet(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);
    uint64_t i;
    struct test_str *data;
    for(i=0;i<TEST_ELM_NUM;i++) {
        data = (struct test_str *) get_elm(g_cache_tbl.ctx, i);
        CU_ASSERT(data!=NULL);
        CU_ASSERT(data->i[0] == i && data->i[1] == i+1);
    }
}

void testEmpty(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);
    bool empty = false;
    empty = has_no_elm(g_cache_tbl.ctx);
    CU_ASSERT(empty==true);
}

void testNotEmpty(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);
    bool empty = false;
    empty = has_no_elm(g_cache_tbl.ctx);
    CU_ASSERT(empty==false);
}

void testGetFirst(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);
    struct test_str *data;
    uint64_t i = 0;
    data = (struct test_str *) get_first_elm(g_cache_tbl.ctx, &i);
    CU_ASSERT(data!=NULL);
    CU_ASSERT(i==0);
}

void testDelete(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);
    uint64_t i;
    int rc;

    for(i=0;i<TEST_ELM_NUM;i++) {
        rc = delete_elm(g_cache_tbl.ctx, i);
        CU_ASSERT(rc==0);
    }   

    CU_ASSERT( g_cache_tbl.ctx->elm_num == DEFAULT_ELM_SIZE);
    CU_ASSERT( g_cache_tbl.ctx->free_num == g_cache_tbl.ctx->elm_num);
}

void testIterate(void) {
    CU_ASSERT(g_cache_tbl.ctx!=NULL);
    int rc;

    rc = for_each_elm_fn(g_cache_tbl.ctx, NULL, NULL);
    CU_ASSERT(rc==TEST_ELM_NUM);
}

int main (int argc, char **argv) {
    CU_pSuite pSuite = NULL;

    if(CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    char name[32] = {"test_cache_tbl"};
    g_cache_tbl.ctx = init_cache_tbl(name, DEFAULT_ELM_SIZE, sizeof(struct test_str), 1);

    pSuite = CU_add_suite("kvtrans utils", NULL, NULL);
    if(NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if(
        NULL == CU_add_test(pSuite, "testExpand" ,  testExpand)
        || NULL == CU_add_test(pSuite, "testShrink" ,  testShrink)
        || NULL == CU_add_test(pSuite, "testStore" ,  testStore)
        || NULL == CU_add_test(pSuite, "testIterate" ,  testIterate)
        || NULL == CU_add_test(pSuite, "testGet" ,  testGet)
        || NULL == CU_add_test(pSuite, "testNotEmpty" ,  testNotEmpty)
        || NULL == CU_add_test(pSuite, "testGetFirst" ,  testGetFirst)
        || NULL == CU_add_test(pSuite, "testDelete" ,  testDelete)
        || NULL == CU_add_test(pSuite, "testEmpty" ,  testEmpty)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return CU_get_error();
    
}