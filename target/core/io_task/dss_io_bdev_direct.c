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

#include "apis/dss_module_apis.h"
#include "apis/dss_io_task_apis.h"
#include "dss_spdk_wrapper.h"
#include "dss_io_bdev.h"

#include "df_req.h"

#include "spdk/endian.h"
#include "spdk/nvme_spec.h"

#include "nvmf_internal.h"

static void
_dss_nvmf_get_rw_params(const struct spdk_nvme_cmd *cmd, uint64_t *start_lba,
                  uint64_t *num_blocks)
{
    /* SLBA: CDW10 and CDW11 */
    *start_lba = from_le64(&cmd->cdw10);

    /* NLB: CDW12 bits 15:00, 0's based */
    *num_blocks = (from_le32(&cmd->cdw12) & 0xFFFFu) + 1;
}

static bool
_dss_nvmf_bdev_lba_in_range(uint64_t bdev_num_blocks, uint64_t io_start_lba,
                 uint64_t io_num_blocks)
{
    if (io_start_lba + io_num_blocks > bdev_num_blocks ||
        io_start_lba + io_num_blocks < io_start_lba) {
        return false;
    }

    return true;
}

void dss_nvmf_bdev_ctrlr_complete_cmd(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    struct dfly_request *dreq = (struct dfly_request *)cb_arg;

    struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)dreq->req_ctx;
    //struct spdk_nvme_cmd *cmd = &nvmf_req->cmd->nvme_cmd;
    struct spdk_nvme_cpl *rsp = &nvmf_req->rsp->nvme_cpl;

    uint32_t cdw0;
    int             sc, sct;

    rsp->cdw0 = 0;
    if(success) {
        rsp->status.sct = SPDK_NVME_SCT_GENERIC;
        rsp->status.sc = SPDK_NVME_SC_SUCCESS;
    } else {
        rsp->status.sct = SPDK_NVME_SCT_GENERIC;
        rsp->status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
    }

    if(bdev_io) {
        spdk_bdev_io_get_nvme_status(bdev_io, &cdw0, &sct, &sc);
        spdk_bdev_free_io(bdev_io);

        rsp->status.sct = sc;
        rsp->status.sc = sct;
        rsp->cdw0 = cdw0;
    }

    DFLY_ASSERT(dreq->state == DFLY_REQ_IO_SUBMITTED_TO_DEVICE);
    dreq->state = DFLY_REQ_IO_COMPLETED_FROM_DEVICE;
    dreq->next_action = DFLY_COMPLETE_IO;

    dfly_handle_request(dreq);

    //COMPLETE dss request

    return;
}

int
dss_io_passthru(struct spdk_bdev_desc *desc,
				 struct spdk_io_channel *ch, struct spdk_nvmf_request *req, void *cb_arg)
{
	int rc;

	rc = spdk_bdev_nvme_io_passthru(desc, ch, &req->cmd->nvme_cmd, req->data, req->length,
					dss_nvmf_bdev_ctrlr_complete_cmd, cb_arg);
	if (spdk_unlikely(rc)) {
		if (rc == -ENOMEM) {
            //TODO: Resubmit
			// nvmf_bdev_ctrl_queue_io(req, bdev, ch, nvmf_ctrlr_process_io_cmd_resubmit, req);
			// return SPDK_NVMF_REQUEST_EXEC_STATUS_ASYNCHRONOUS;
		}
		return -1;
	}

	return 0;
}

void dss_io_submit_direct(dss_request_t *req)
{
    int rc;

    struct dfly_request *dreq = (struct dfly_request *)req;

    uint64_t start_lba;
    uint64_t num_blocks;

    dss_device_t *io_device = req->io_device;

    struct spdk_nvmf_subsystem *nvmf_ss = (struct spdk_nvmf_subsystem *)dreq->req_dfly_ss->parent_ctx;
    struct spdk_nvmf_request *nvmf_req = (struct spdk_nvmf_request *)dreq->req_ctx;
    struct spdk_nvme_cmd *cmd = &nvmf_req->cmd->nvme_cmd;
    struct spdk_nvme_cpl *rsp = &nvmf_req->rsp->nvme_cpl;

    uint64_t bdev_num_blocks = spdk_bdev_get_num_blocks(io_device->bdev);
    uint32_t block_size = spdk_bdev_get_block_size(io_device->bdev);

    uint32_t ch_arr_index = dss_env_get_current_core();

    struct spdk_io_channel *ch;

    DSS_ASSERT(ch_arr_index < io_device->n_ch);

    if(cmd->opc == SPDK_NVME_OPC_READ || cmd->opc == SPDK_NVME_OPC_WRITE) {
        _dss_nvmf_get_rw_params(cmd, &start_lba, &num_blocks);

        if (spdk_unlikely(!_dss_nvmf_bdev_lba_in_range(bdev_num_blocks, start_lba, num_blocks)))
        {
            DFLY_ERRLOG("end of media\n");
            rsp->status.sct = SPDK_NVME_SCT_GENERIC;
            rsp->status.sc = SPDK_NVME_SC_LBA_OUT_OF_RANGE;
            // Complete IO
            DSS_ASSERT(0);
        }

        if (spdk_unlikely(num_blocks * block_size > nvmf_req->length))
        {
            DFLY_ERRLOG("Read NLB %" PRIu64 " * block size %" PRIu32 " > SGL length %" PRIu32 "\n",
                        num_blocks, block_size, nvmf_req->length);
            rsp->status.sct = SPDK_NVME_SCT_GENERIC;
            rsp->status.sc = SPDK_NVME_SC_DATA_SGL_LENGTH_INVALID;
            // Complete IO
            DFLY_ASSERT(0);
        }
    }

    ch = dss_io_dev_get_channel(io_device);
    DSS_ASSERT(io_device->ch_arr[ch_arr_index].thread == dss_env_get_spdk_thread());

    dreq->state = DFLY_REQ_IO_SUBMITTED_TO_DEVICE;

    switch(cmd->opc) {
        case SPDK_NVME_OPC_READ:
            rc = spdk_bdev_read_blocks(io_device->desc, ch, dreq->req_value.value, start_lba, num_blocks, dss_nvmf_bdev_ctrlr_complete_cmd, dreq);
            break;
        case SPDK_NVME_OPC_WRITE:
            rc = spdk_bdev_write_blocks(io_device->desc, ch, dreq->req_value.value, start_lba, num_blocks, dss_nvmf_bdev_ctrlr_complete_cmd, dreq);
            break;
        case SPDK_NVME_OPC_FLUSH:
            //TODO: verify if flush supported or required
            rc = spdk_bdev_flush_blocks(io_device->desc, ch, 0, bdev_num_blocks, dss_nvmf_bdev_ctrlr_complete_cmd, dreq);
            break;
        default:
            rc = dss_io_passthru(io_device->desc, ch, nvmf_req, dreq);
    }

    if(rc) {
        dss_nvmf_bdev_ctrlr_complete_cmd(NULL, false, dreq);
    }
    return;
}

