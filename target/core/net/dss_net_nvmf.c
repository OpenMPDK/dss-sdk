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
#include "spdk/endian.h"

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

dss_request_opc_t dss_nvmf_get_dss_opc(void *req)
{
    struct spdk_nvmf_request *nvmf_req = req;

    switch(nvmf_req->cmd->nvme_cmd.opc) {
        case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
            return DSS_NVMF_KV_IO_OPC_STORE;
        case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
            return DSS_NVMF_KV_IO_OPC_RETRIEVE;
        case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
            return DSS_NVMF_KV_IO_OPC_DELETE;
        case SPDK_NVME_OPC_READ:
            return DSS_NVMF_BLK_IO_OPC_READ;
        case SPDK_NVME_OPC_WRITE:
            return DSS_NVMF_BLK_IO_OPC_WRITE;
        default:
            return DSS_NVMF_IO_PT;
    }
}

void dss_nvmf_set_sc_success(struct spdk_nvmf_request *nvmf_req)
{
    struct spdk_nvme_cpl *rsp;

	rsp = &nvmf_req->rsp->nvme_cpl;

    rsp->status.sct = SPDK_NVME_SCT_GENERIC;
    rsp->status.sc = SPDK_NVME_SC_SUCCESS;

    return;
}

void dss_nvmf_set_sct_sc(struct spdk_nvmf_request *nvmf_req, uint16_t sct, uint16_t sc)
{
    struct spdk_nvme_cpl *rsp;

	rsp = &nvmf_req->rsp->nvme_cpl;

    rsp->status.sct = sct;
    rsp->status.sc = sc;

    return;
}

void dss_nvmf_get_rw_params(dss_request_t *req, uint64_t *start_lba,
                  uint64_t *num_blocks)
{
    struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.nvmf_req;
    struct spdk_nvme_cmd *cmd = &nvmf_req->cmd->nvme_cmd;

    /* SLBA: CDW10 and CDW11 */
    *start_lba = from_le64(&cmd->cdw10);

    /* NLB: CDW12 bits 15:00, 0's based */
    *num_blocks = (from_le32(&cmd->cdw12) & 0xFFFFu) + 1;
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
        case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
            //Nothing to set just complete as success
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

void dss_net_setup_nvmf_resp(dss_request_t *req, dss_request_rc_t status, uint32_t cdw0)
{
    struct spdk_nvmf_request *nvmf_req;
    nvmf_req = (struct spdk_nvmf_request *)req->module_ctx[DSS_MODULE_NET].mreq_ctx.net.nvmf_req;

    switch(status) {
        case DSS_REQ_STATUS_SUCCESS:
            dss_nvmf_set_sc_success(nvmf_req);
            if(req->opc == DSS_NVMF_KV_IO_OPC_RETRIEVE) {
                dss_nvmf_set_resp_cdw0(nvmf_req, cdw0);
            }
            break;
        case DSS_REQ_STATUS_KEY_NOT_FOUND:
            dss_nvmf_set_sct_sc(nvmf_req, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_KEY_NOT_EXIST);
            break;
        case DSS_REQ_STATUS_ERROR:
            DSS_ASSERT(0);
    }

    return;
}