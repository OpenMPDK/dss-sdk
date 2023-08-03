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

#ifndef DSS_IO_TASK_H
#define DSS_IO_TASK_H

#include "dss.h"

#include "apis/dss_io_task_apis.h"

#include "utils/dss_mallocator.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dss_io_task_module_s {
    dss_mallocator_ctx_t *io_task_allocator;
    dss_mallocator_ctx_t *ops_allocator;
    dss_module_t *io_module;
};

// struct dss_iov_s {
//     void *data;
//     uint64_t len;
//     uint64_t offset;
// };

typedef enum dss_io_op_type_e {
    DSS_IO_BLK_READ = 0,
    DSS_IO_BLK_WRITE
} dss_io_op_type_t;

typedef struct dss_io_op_s {
    dss_io_op_type_t opc;
    union {
        // struct {
        //     uint64_t lba;
        //     uint64_t nblocks;
        //     dss_iov_t *data_iov;
        //     uint64_t length;
        //     uint64_t offset;
        //     bool is_write;
        // } rw_v;
        struct {
            uint64_t lba;
            uint64_t nblocks;
            void *data;
            uint64_t length;
            uint64_t offset;
            bool is_write;
        } rw;
    };
    bool is_blocking;
    bool data_buff_from_io_module;
    dss_device_t *device;
    dss_io_task_t *parent;
    uint64_t op_id;
    union {
        struct {
            uint32_t cdw0;
            uint16_t sc;
            uint16_t sct;
        } nvme_rsp;
    } status;
    TAILQ_ENTRY(dss_io_op_s) op_next;
} dss_io_op_t;

struct dss_io_task_s {
    dss_io_task_module_t *io_task_module;
    dss_request_t *dreq;
    dss_module_instance_t *cb_minst;
    void *cb_ctx;
    uint64_t num_total_ops;
    uint64_t num_outstanding_ops;
    uint64_t num_ops_done;
    TAILQ_HEAD(, dss_io_op_s) op_todo_list;
    TAILQ_HEAD(, dss_io_op_s) ops_in_progress;
    TAILQ_HEAD(, dss_io_op_s) op_done;
    TAILQ_HEAD(, dss_io_op_s) failed_ops;
    dss_io_task_status_t task_status;
    uint32_t tci;//Task cache index
};

#ifdef __cplusplus
}
#endif

#endif //DSS_IO_TASK_H
