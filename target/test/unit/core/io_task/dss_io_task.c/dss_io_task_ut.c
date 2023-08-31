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
#include <sys/queue.h>

#include "dss_io_task.h"

#define DSS_IO_TASK_UT_MAX_TASKS (256)
#define DSS_IO_TASK_UT_MAX_IO_OPS (512)

dss_io_task_module_t *g_io_task_module;
dss_io_task_t *g_io_task;

#define DSS_IO_TASK_UT_TEST_LBA ((uint64_t) 0x54321)
#define DSS_IO_TASK_UT_TEST_NBLOCKS ((uint64_t) 0x100)
#define DSS_IO_TASK_UT_TEST_DATAP ((void *) 0x70ABCDEF)
#define DSS_IO_TASK_UT_TEST_DEV ((dss_device_t *)0xDE51CE)

#define DSS_IO_TASK_UT_TEST_CB_INST ((dss_module_instance_t *)0x7023456)
#define DSS_IO_TASK_UT_TEST_CB_CTX  ((void *)0x7024567)

int dss_io_task_ut_init(void)
{
    dss_io_task_module_opts_t opts;
    dss_io_task_module_status_t rc;

    g_io_task_module = NULL;

    opts.max_io_tasks = DSS_IO_TASK_UT_MAX_TASKS;
    opts.max_io_ops = DSS_IO_TASK_UT_MAX_IO_OPS;

    rc = dss_io_task_module_init(opts, &g_io_task_module);
    assert(rc == DSS_IO_TASK_MODULE_STATUS_SUCCESS);
    assert(g_io_task_module != NULL);
    return 0;
}

int dss_io_task_ut_cleanup(void)
{
    dss_io_task_module_status_t rc;

    assert(g_io_task_module != NULL);
    rc = dss_io_task_module_end(g_io_task_module);
    assert(rc == DSS_IO_TASK_MODULE_STATUS_SUCCESS);

    return 0;
}

void dss_io_task_ut_sim_completion(dss_io_task_t *task)
{
    dss_io_op_t *op;

    CU_ASSERT(task->num_total_ops == 1);
    op = TAILQ_FIRST(&task->op_todo_list);
    task->num_ops_done++;

    TAILQ_REMOVE(&task->op_todo_list, op, op_next);
    TAILQ_INSERT_HEAD(&task->op_done, op, op_next);

    CU_ASSERT(task->tci == 0);
    CU_ASSERT(op->device == DSS_IO_TASK_UT_TEST_DEV);
    CU_ASSERT(op->blk_rw.data == DSS_IO_TASK_UT_TEST_DATAP);
    CU_ASSERT(op->blk_rw.lba == DSS_IO_TASK_UT_TEST_LBA);
    CU_ASSERT(op->blk_rw.nblocks == DSS_IO_TASK_UT_TEST_NBLOCKS);
    CU_ASSERT(task->cb_ctx == DSS_IO_TASK_UT_TEST_CB_CTX);
    CU_ASSERT(task->cb_minst == DSS_IO_TASK_UT_TEST_CB_INST);

    return;
}


void testIoTaskGetNew(void)
{
    dss_io_task_status_t rc;

    g_io_task = NULL;
    rc = dss_io_task_get_new(g_io_task_module, &g_io_task);

    CU_ASSERT(rc == DSS_IO_TASK_STATUS_SUCCESS);
    CU_ASSERT(g_io_task != NULL);

    rc = dss_io_task_setup(g_io_task, NULL, DSS_IO_TASK_UT_TEST_CB_INST, DSS_IO_TASK_UT_TEST_CB_CTX);
    CU_ASSERT(rc == DSS_IO_TASK_STATUS_SUCCESS);

    return;
}

void testIoTaskPut(void)
{
    dss_io_task_status_t rc;

    rc = dss_io_task_put(g_io_task);
    g_io_task = NULL;

    CU_ASSERT(rc == DSS_IO_TASK_STATUS_SUCCESS);

    return;
}

void testIOTaskReadOp(void)
{
    dss_io_task_status_t rc;
    dss_io_op_t *op;

    rc = dss_io_task_add_blk_read(g_io_task, DSS_IO_TASK_UT_TEST_DEV, \
                                             DSS_IO_TASK_UT_TEST_LBA, \
                                             DSS_IO_TASK_UT_TEST_NBLOCKS, \
                                             DSS_IO_TASK_UT_TEST_DATAP, \
                                             NULL);

    CU_ASSERT(rc == DSS_IO_TASK_STATUS_SUCCESS);
    op = TAILQ_FIRST(&g_io_task->op_todo_list);
    CU_ASSERT(op->opc == DSS_IO_BLK_READ);

    dss_io_task_ut_sim_completion(g_io_task);

    return;
}

void testIOTaskWriteOP(void)
{
    dss_io_task_status_t rc;
    dss_io_op_t *op;

    void *tmp_ctx = NULL;
    dss_io_op_user_param_t op_params = {};

    rc = dss_io_task_add_blk_write(g_io_task, DSS_IO_TASK_UT_TEST_DEV, \
                                              DSS_IO_TASK_UT_TEST_LBA, \
                                              DSS_IO_TASK_UT_TEST_NBLOCKS, \
                                              DSS_IO_TASK_UT_TEST_DATAP, \
                                              NULL);

    CU_ASSERT(rc == DSS_IO_TASK_STATUS_SUCCESS);
    op = TAILQ_FIRST(&g_io_task->op_todo_list);
    CU_ASSERT(op->opc == DSS_IO_BLK_WRITE);

    rc = dss_io_task_get_op_ranges(g_io_task, DSS_IO_OP_OWNER_NONE, DSS_IO_OP_FOR_SUBMISSION, &tmp_ctx, &op_params);
    CU_ASSERT(rc == DSS_IO_TASK_STATUS_IT_END);
    CU_ASSERT(tmp_ctx == NULL);
    CU_ASSERT(op_params.is_params_valid == true);
    CU_ASSERT(op_params.data == NULL);
    CU_ASSERT(op_params.lba == DSS_IO_TASK_UT_TEST_LBA);
    CU_ASSERT(op_params.num_blocks == DSS_IO_TASK_UT_TEST_NBLOCKS)

    rc = dss_io_task_get_op_ranges(g_io_task, DSS_IO_OP_OWNER_NONE, DSS_IO_OP_COMPLETED, &tmp_ctx, &op_params);
    CU_ASSERT(rc == DSS_IO_TASK_STATUS_IT_END);
    CU_ASSERT(tmp_ctx == NULL);
    CU_ASSERT(op_params.is_params_valid == false);

    rc = dss_io_task_get_op_ranges(g_io_task, DSS_IO_OP_OWNER_NONE, DSS_IO_OP_FAILED, &tmp_ctx, &op_params);
    CU_ASSERT(rc == DSS_IO_TASK_STATUS_IT_END);
    CU_ASSERT(tmp_ctx == NULL);
    CU_ASSERT(op_params.is_params_valid == false);


    dss_io_task_ut_sim_completion(g_io_task);

    rc = dss_io_task_get_op_ranges(g_io_task, DSS_IO_OP_OWNER_NONE, DSS_IO_OP_COMPLETED, &tmp_ctx, &op_params);
    CU_ASSERT(rc == DSS_IO_TASK_STATUS_IT_END);
    CU_ASSERT(tmp_ctx == NULL);
    CU_ASSERT(op_params.is_params_valid == true);
    CU_ASSERT(op_params.data == DSS_IO_TASK_UT_TEST_DATAP);
    CU_ASSERT(op_params.lba == DSS_IO_TASK_UT_TEST_LBA);
    CU_ASSERT(op_params.num_blocks == DSS_IO_TASK_UT_TEST_NBLOCKS)

    rc = dss_io_task_get_op_ranges(g_io_task, DSS_IO_OP_OWNER_NONE, DSS_IO_OP_FOR_SUBMISSION, &tmp_ctx, &op_params);
    CU_ASSERT(rc == DSS_IO_TASK_STATUS_IT_END);
    CU_ASSERT(tmp_ctx == NULL);
    CU_ASSERT(op_params.is_params_valid == false);

    rc = dss_io_task_get_op_ranges(g_io_task, DSS_IO_OP_OWNER_NONE, DSS_IO_OP_FAILED, &tmp_ctx, &op_params);
    CU_ASSERT(rc == DSS_IO_TASK_STATUS_IT_END);
    CU_ASSERT(tmp_ctx == NULL);
    CU_ASSERT(op_params.is_params_valid == false);

    return;
}

int main()
{
    CU_pSuite pSuite = NULL;

    if (CUE_SUCCESS != CU_initialize_registry())
    {
        return CU_get_error();
    }

    pSuite = CU_add_suite("DSS IO Task module", dss_io_task_ut_init, dss_io_task_ut_cleanup);
    if (NULL == pSuite)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    //TODO: Add test cases
    //          * Different Module ID for OPs
    //          * Test adding multiple ops to io task
    if (
        NULL == CU_add_test(pSuite, "testIoTaskGetNewMalloc", testIoTaskGetNew) ||
        NULL == CU_add_test(pSuite, "testIOTaskReadOp", testIOTaskReadOp) ||
        NULL == CU_add_test(pSuite, "testIoTaskPut1", testIoTaskPut) ||
        NULL == CU_add_test(pSuite, "testIoTaskGetNewCacheAlloc", testIoTaskGetNew) ||
        NULL == CU_add_test(pSuite, "testIOTaskWriteOP", testIOTaskWriteOP) ||
        NULL == CU_add_test(pSuite, "testIoTaskPut2", testIoTaskPut))
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return CU_get_error();
}