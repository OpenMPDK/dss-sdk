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

#include "dragonfly.h"

#include "dss.h"
#include "dss_spdk_wrapper.h"
#include "apis/dss_module_apis.h"
#include "apis/dss_net_module.h"

void dss_net_setup_request(dss_request_t *req, dss_module_instance_t *m_inst, void *nvmf_req)
{
    dfly_req_init_nvmf_value(req);
    req->status = DSS_REQ_STATUS_SUCCESS;
    req->module_ctx[DSS_MODULE_NET].module_instance = m_inst;
    req->module_ctx[DSS_MODULE_NET].module = m_inst->module;
    DSS_ASSERT(req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state == DSS_NET_REQUEST_FREE);
    req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_INIT;
    req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.nvmf_req = nvmf_req;
    req->ss = dss_req_get_subsystem(req);
    req->opc = dss_nvmf_get_dss_opc(nvmf_req);

    return;
}

void dss_net_teardown_request(dss_request_t *req)
{
    req->status = DSS_REQ_STATUS_ERROR;
    req->module_ctx[DSS_MODULE_NET].module_instance = NULL;
    req->module_ctx[DSS_MODULE_NET].module = NULL;
    DSS_ASSERT(req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state == DSS_NET_REQUEST_COMPLETE);
    req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_FREE;

    return;
}

static inline void dss_net_request_complete(dss_subsystem_t *ss, dss_request_t *req)
{
    dfly_nvmf_request_complete((struct spdk_nvmf_request *)req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.nvmf_req);
    return;
}

void dss_net_request_setup_blk_io_task(dss_request_t *req)
{
    dss_io_task_status_t iot_rc;
    dss_io_task_t *io_task = NULL;

    uint64_t lba;
    uint64_t nblocks;
    uint64_t len;
    uint64_t off = 0;
    bool is_blocking = false;
    void *d;

    dss_module_instance_t *net_mi = dss_req_get_net_module_instance(req);

    struct dfly_request *dreq = (struct dfly_request *)req;

    if(!dss_subsystem_use_io_task(req->ss)) {
        DSS_ASSERT(0);
    }

    if(req->opc == DSS_NVMF_IO_PT) {
        //TODO: Pass through IO support
        DSS_ASSERT(0);
    }

    iot_rc = dss_io_task_get_new(dss_subsytem_get_iotm_ctx(req->ss), &io_task);
    if(iot_rc != DSS_IO_TASK_STATUS_SUCCESS) {
        DSS_ASSERT(0);//Should always succed
    }
    DSS_ASSERT(io_task != NULL);

    dss_io_task_setup(io_task, req, net_mi, req);

    dss_nvmf_get_rw_params(req, &lba, &nblocks);
    //TODO: Support IOV
    d = dreq->req_value.value;
    len = dreq->req_value.length;

    if(req->opc == DSS_NVMF_BLK_IO_OPC_READ) {
        dss_io_task_add_blk_read(req->io_task, req->io_device, lba, nblocks, d, len, off, is_blocking);
    } else if(req->opc == DSS_NVMF_BLK_IO_OPC_WRITE) {
        dss_io_task_add_blk_write(req->io_task, req->io_device, lba, nblocks, d, len, off, is_blocking);
    }
    return;
}

void dss_net_request_process(dss_request_t *req)
{
    dss_net_request_state_t prev_state;
    do {
        prev_state = req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state;
        switch (req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state)
        {
        case DSS_NET_REQUEST_INIT:
            if (g_dragonfly->test_nic_bw) {
                dss_nvmf_process_as_no_op(req);
                req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_COMPLETE;
            } else if (dss_subsystem_kv_mode_enabled(req->ss)) {
                if(req->opc == DSS_NVMF_BLK_IO_OPC_READ) {
                    dss_nvmf_process_as_no_op(req);
                    req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_COMPLETE;
                } else {
                    dss_setup_kvtrans_req(req, dss_req_get_key(req), dss_req_get_value(req));
                    req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_SUBMITTED;
                    dfly_module_post_request(dss_module_get_subsys_ctx(DSS_MODULE_KVTRANS, req->ss), req);
                    return;
                }
            } else {
                dss_net_request_setup_blk_io_task(req);
                dss_io_task_submit(req->io_task);
                req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_SUBMITTED;
                return;
            }
            break;
        case DSS_NET_REQUEST_SUBMITTED:
            req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_COMPLETE;
            break;
        case DSS_NET_REQUEST_COMPLETE:
            dss_net_request_complete(req->ss, req);
            dss_net_teardown_request(req);
            break;
        case DSS_NET_REQUEST_FREE:
            break;
        default:
            DSS_RELEASE_ASSERT(0);
        }
    } while(req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state != prev_state);
}
