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
#include "apis/dss_net_module.h"

#include "nvmf_internal.h"

void dss_nvmf_set_resp_cdw0(struct spdk_nvmf_request *nvmf_req, uint32_t cdw_val)
{
    struct spdk_nvme_cpl *rsp;

	rsp = &nvmf_req->rsp->nvme_cpl;

	rsp->cdw0 = cdw_val;

	return;
}

uint8_t dss_nvmf_get_req_opc(struct spdk_nvmf_request *nvmf_req)
{
    return nvmf_req->cmd->nvme_cmd.opc;
}

void dss_nvmf_set_sc_success(struct spdk_nvmf_request *nvmf_req)
{
    struct spdk_nvme_cpl *rsp;

	rsp = &nvmf_req->rsp->nvme_cpl;

    rsp->status.sct = SPDK_NVME_SCT_GENERIC;
    rsp->status.sc = SPDK_NVME_SC_SUCCESS;

    return;
}

void dss_nvmf_process_as_no_op(dss_request_t *req)
{
    struct spdk_nvmf_request *nvmf_req;
    uint32_t val_len;
    uint8_t nvmf_opc;

    req->opc = DSS_NVMF_IO_OPC_NO_OP;

    nvmf_req = (struct spdk_nvmf_request *)req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.nvmf_req;
    nvmf_opc = dss_nvmf_get_req_opc(nvmf_req);

    switch(nvmf_opc) {
        case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
        case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
            val_len = dss_req_get_val_len(req);
            dss_nvmf_set_resp_cdw0(nvmf_req, val_len);
            break;
        case SPDK_NVME_OPC_READ:
        case SPDK_NVME_OPC_WRITE:
            //Nothing to set just complete as success
            break;
        default:
            //Not Handled
            DSS_ASSERT(0);
    }
    dss_nvmf_set_sc_success(nvmf_req);

    return;
}