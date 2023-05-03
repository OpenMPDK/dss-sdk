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
#include "apis/dss_module_apis.h"
#include "apis/dss_net_module.h"

void dss_net_setup_request(dss_request_t *req, dss_module_instance_t *m_inst, void *nvmf_req)
{
    req->status = DSS_REQ_STATUS_SUCCESS;
    req->module_ctx[DSS_MODULE_NET].module_instance = m_inst;
    req->module_ctx[DSS_MODULE_NET].module = m_inst->module;
    DSS_ASSERT(req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state == DSS_NET_REQUEST_FREE);
    req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_INIT;
    req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.nvmf_req = nvmf_req;

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

void dss_net_request_process(dss_subsystem_t *ss, dss_request_t *req)
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
            } else if (dss_subsystem_kv_mode_enabled(ss)) {
                //TODO: KV Mode
                req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_SUBMITTED;
                return;
            } else {
                //TODO: Block mode
                req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_SUBMITTED;
                return;
            }
            break;
        case DSS_NET_REQUEST_SUBMITTED:
            req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state = DSS_NET_REQUEST_COMPLETE;
            break;
        case DSS_NET_REQUEST_COMPLETE:
            dss_net_request_complete(ss, req);
            dss_net_teardown_request(req);
            break;
        case DSS_NET_REQUEST_FREE:
            break;
        default:
            DSS_RELEASE_ASSERT(0);
        }
    } while(req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.state != prev_state);
}