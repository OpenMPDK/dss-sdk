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
#include "dss.h"
#include "dss_io_task.h"

#include "dragonfly.h"

#ifndef DSS_BUILD_CUNIT_TEST
#include "spdk/env.h"

static inline uint32_t __dss_env_get_curr_core(void)
{
    return spdk_env_get_current_core();
}

static inline uint32_t __dss_env_max_cores(void)
{
    return spdk_env_get_last_core();
}
#else
static inline uint32_t __dss_env_get_curr_core(void)
{
    return 0;
}

static inline uint32_t __dss_env_max_cores(void)
{
    return 1;
}
#endif //DSS_BUILD_CUNIT_TEST

dss_io_task_module_status_t dss_io_task_module_init(dss_io_task_module_opts_t io_task_opts, dss_io_task_module_t **module)
{
    dss_io_task_module_t *m;
    dss_mallocator_opts_t malloc_opts;

    DSS_RELEASE_ASSERT(module);
    m = calloc(1, sizeof(dss_io_task_module_t));
    if(!m) {
        goto err_out;
    }

    m->io_module = io_task_opts.io_module;

    malloc_opts.item_sz = sizeof(dss_io_task_t);
    malloc_opts.max_per_cache_items = io_task_opts.max_io_tasks;
    malloc_opts.num_caches = __dss_env_max_cores();
    m->io_task_allocator = dss_mallocator_init(DSS_MEM_ALLOC_MALLOC, malloc_opts);
    if(!m->io_task_allocator) {
        goto err_out;
    }

    malloc_opts.item_sz = sizeof(dss_io_op_t);
    malloc_opts.max_per_cache_items = io_task_opts.max_io_ops;
    malloc_opts.num_caches = __dss_env_max_cores();
    m->ops_allocator = dss_mallocator_init(DSS_MEM_ALLOC_MALLOC, malloc_opts);
    if(!m->ops_allocator) {
        goto err_out;
    }

    *module = m;
    return DSS_IO_TASK_MODULE_STATUS_SUCCESS;

err_out:
    if(m) {
        if(m->io_task_allocator) dss_mallocator_destroy(m->io_task_allocator);
        if(m->ops_allocator) dss_mallocator_destroy(m->ops_allocator);
        free(m);
    }
    *module = NULL;
    return DSS_IO_TASK_MODULE_STATUS_ERROR;
}

dss_io_task_module_status_t dss_io_task_module_end(dss_io_task_module_t *m)
{
    dss_mallocator_status_t rc;
    DSS_RELEASE_ASSERT(m);

    rc = dss_mallocator_destroy(m->ops_allocator);
    if(rc != DSS_MALLOC_SUCCESS) {
        return DSS_IO_TASK_MODULE_STATUS_ERROR;
    }

    rc = dss_mallocator_destroy(m->io_task_allocator);
    if(rc != DSS_MALLOC_SUCCESS) {
        return DSS_IO_TASK_MODULE_STATUS_ERROR;
    }

    return DSS_IO_TASK_MODULE_STATUS_SUCCESS;
}

dss_io_task_status_t dss_io_task_get_new(dss_io_task_module_t *m, dss_io_task_t **task)
{
    dss_io_task_t *t;
    dss_mallocator_status_t status;
    uint32_t tci;

    DSS_ASSERT(task);

    tci = __dss_env_get_curr_core();
    status = dss_mallocator_get(m->io_task_allocator, tci, (dss_mallocator_item_t **)&t);
    if(!t) {
        return DSS_IO_TASK_STATUS_ERROR;
    }

    if(status == DSS_MALLOC_NEW_ALLOCATION) {
        TAILQ_INIT(&t->op_done);
        TAILQ_INIT(&t->op_todo_list);
        TAILQ_INIT(&t->ops_in_progress);
        TAILQ_INIT(&t->failed_ops);

        t->num_total_ops = 0;
        t->num_outstanding_ops = 0;
        t->num_ops_done = 0;

        t->io_task_module = m;
        t->tci = tci;
        t->task_status = DSS_IO_TASK_STATUS_SUCCESS;
        t->in_progress = false;
        t->cb_to_cq = false;
    } else {
        DSS_ASSERT(status == DSS_MALLOC_SUCCESS);
    }

    DSS_ASSERT(t->io_task_module == m);
    DSS_ASSERT(t->tci == tci);

    *task = t;
    return DSS_IO_TASK_STATUS_SUCCESS;
}


dss_io_task_status_t dss_io_task_reset_ops(dss_io_task_t *io_task)
{
    dss_io_task_status_t rc = DSS_IO_TASK_STATUS_SUCCESS;
    dss_mallocator_status_t status;
    dss_io_op_t *io_op;

    uint32_t freed_ops = 0;
    uint32_t tci;

    DSS_ASSERT(io_task);

    tci = __dss_env_get_curr_core();
    DSS_ASSERT(tci == io_task->tci);
    DSS_ASSERT(io_task->in_progress == false);

    DSS_ASSERT(TAILQ_EMPTY(&io_task->op_todo_list));
    DSS_ASSERT(TAILQ_EMPTY(&io_task->ops_in_progress));
    //TODO: Handle failed ops
    DSS_ASSERT(TAILQ_EMPTY(&io_task->failed_ops));
    DSS_ASSERT(io_task->num_outstanding_ops == 0);
    DSS_ASSERT(io_task->num_ops_done == io_task->num_total_ops);

    io_op = TAILQ_FIRST(&io_task->op_done);
    while(io_op) {
        TAILQ_REMOVE(&io_task->op_done, io_op, op_next);
        memset(io_op, 0, sizeof(dss_io_op_t));
        status = dss_mallocator_put(io_task->io_task_module->ops_allocator, io_task->tci, io_op);
        if(status != DSS_MALLOC_SUCCESS) {
            rc = DSS_IO_TASK_STATUS_ERROR;
        }
        freed_ops++;
        io_op = TAILQ_FIRST(&io_task->op_done);
    }

    DSS_ASSERT(freed_ops == io_task->num_total_ops);

    io_task->num_ops_done = 0;
    io_task->num_total_ops = 0;
    io_task->task_status = DSS_IO_TASK_STATUS_SUCCESS;
    DSS_ASSERT(TAILQ_EMPTY(&io_task->op_done));

    return rc;
}

dss_io_task_status_t dss_io_task_put(dss_io_task_t *io_task)
{
    dss_io_task_status_t rc = DSS_IO_TASK_STATUS_SUCCESS;
    dss_mallocator_status_t status;

    rc = dss_io_task_reset_ops(io_task);

    status = dss_mallocator_put(io_task->io_task_module->io_task_allocator, io_task->tci, io_task);
    if(status != DSS_MALLOC_SUCCESS) {
        rc = DSS_IO_TASK_STATUS_ERROR;
    }

    return rc;
}

dss_io_task_status_t dss_io_task_setup(dss_io_task_t *io_task, dss_request_t *req, dss_module_instance_t *cb_minst, void *cb_ctx, bool cb_to_cq)
{
    DSS_ASSERT(io_task);
    DSS_ASSERT(cb_minst);
    DSS_ASSERT(cb_ctx);

    if(req) {
        io_task->dreq = req;
        DSS_ASSERT(req->io_task == NULL);
        req->io_task = io_task;
    }

    io_task->cb_minst = cb_minst;
    io_task->cb_ctx = cb_ctx;
    io_task->cb_to_cq = cb_to_cq;

    return DSS_IO_TASK_STATUS_SUCCESS;
}

static inline dss_io_task_status_t _dss_io_task_add_blk_op(dss_io_task_t *task, dss_device_t *target_dev, uint64_t lba, uint64_t num_blocks, void *data, bool is_write, dss_io_opts_t *opts)
{
    dss_io_op_t *io_op;
    dss_mallocator_status_t status;
    dss_io_task_status_t rc;

    bool is_blocking = false;
    dss_io_op_owner_t mod_id = DSS_IO_OP_OWNER_NONE;

    if(opts != NULL) {
        //TODO: Validate options for debug builds
        is_blocking = opts->is_blocking;
        mod_id = opts->mod_id;
    }

    DSS_ASSERT(__dss_env_get_curr_core() == task->tci);
    DSS_ASSERT(task->in_progress == false);

    status = dss_mallocator_get(task->io_task_module->ops_allocator, task->tci, (dss_mallocator_item_t **)&io_op);
    if(status == DSS_MALLOC_ERROR) {
        return DSS_IO_TASK_STATUS_ERROR;
    }

    if(is_write == true) {
        io_op->opc = DSS_IO_BLK_WRITE;
    } else {
        io_op->opc = DSS_IO_BLK_READ;
    }

    io_op->is_blocking = is_blocking;
    io_op->mod_id = mod_id;
    io_op->op_id = task->num_total_ops;
    io_op->device = target_dev;

    io_op->blk_rw.is_write = is_write;
    io_op->blk_rw.lba = lba;
    io_op->blk_rw.nblocks = num_blocks;
    io_op->blk_rw.data = data;

    io_op->parent = task;

    task->num_total_ops++;
    TAILQ_INSERT_TAIL(&task->op_todo_list, io_op, op_next);

    DSS_DEBUGLOG(DSS_IO_TASK, "task[%p] op[%d] lba[%x] nblocks[%x]\n", task, io_op->opc, io_op->blk_rw.lba, io_op->blk_rw.nblocks);

    return DSS_IO_TASK_STATUS_SUCCESS;

}

dss_io_task_status_t dss_io_task_add_blk_read(dss_io_task_t *task, dss_device_t *target_dev, uint64_t lba, uint64_t num_blocks, void *data, dss_io_opts_t *opts)
{
    return _dss_io_task_add_blk_op(task, target_dev, lba, num_blocks, data,false, opts);
}

dss_io_task_status_t dss_io_task_add_blk_write(dss_io_task_t *task, dss_device_t *target_dev, uint64_t lba, uint64_t num_blocks, void *data, dss_io_opts_t *opts)
{
    return _dss_io_task_add_blk_op(task, target_dev, lba, num_blocks, data, true, opts);
}

dss_io_task_status_t dss_io_task_get_op_ranges(dss_io_task_t *task, dss_io_op_owner_t mod_id, dss_io_op_exec_state_t op_state, void **it_ctx, dss_io_op_user_param_t *op_params)
{
    dss_io_op_t *op;
    bool found = false;

    DSS_ASSERT(op_params != NULL);
    DSS_ASSERT(it_ctx != NULL);

    //API can only be called before submit or after completion
    DSS_ASSERT((task->num_outstanding_ops == 0) ||
                (task->num_ops_done == task->num_total_ops));

    //TODO: Handle failure path
    DSS_ASSERT(task->task_status == DSS_IO_TASK_STATUS_SUCCESS);

    op_params->is_params_valid = false;

    op = (dss_io_op_t *)*it_ctx;
    *it_ctx = NULL;
    if(!op) {
        //First Call
        switch(op_state) {
            case DSS_IO_OP_FOR_SUBMISSION:
                op = TAILQ_FIRST(&task->op_todo_list);
                break;
            case DSS_IO_OP_COMPLETED:
                op = TAILQ_FIRST(&task->op_done);
                break;
            case DSS_IO_OP_FAILED:
                op = TAILQ_FIRST(&task->failed_ops);
                break;
            default:
                DSS_RELEASE_ASSERT(0);
        }
    }

    if(!op) {
        //Empty list
        return DSS_IO_TASK_STATUS_IT_END;
    }
    op_params->data = NULL;

    do {
        if(op->mod_id == mod_id) {
            if(found == false) {
               //Only write ops is expected
                DSS_ASSERT(op->opc == DSS_IO_BLK_WRITE);
                op_params->lba = op->blk_rw.lba;
                op_params->num_blocks = op->blk_rw.nblocks;
                if(op_state != DSS_IO_OP_FOR_SUBMISSION) {
                    op_params->data = op->blk_rw.data;
                }
                op_params->is_params_valid = true;
                DSS_DEBUGLOG(DSS_IO_TASK, "type[%d] lba [%x] nblocks [%x]\n", op_state, op_params->lba, op_params->num_blocks);
                found = true;
            } else {
                //OP to start processing on next iteration
                *it_ctx = op;
                break;
            }
        }
        op = TAILQ_NEXT(op, op_next);
    } while(op);

    DSS_ASSERT(found == true);

    if(!*it_ctx) {
        //Last item
        return DSS_IO_TASK_STATUS_IT_END;
    } else {
        return DSS_IO_TASK_STATUS_SUCCESS;
    }
}