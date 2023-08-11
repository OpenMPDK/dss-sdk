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

#include "dss_io_task.h"
#include "dss_io_bdev.h"

#include "apis/dss_module_apis.h"

#include "spdk/likely.h"
#include "spdk/bdev_module.h"
#include "nvmf_internal.h"

/* Dummy bdev module used to to claim bdevs. */
static struct spdk_bdev_module dss_bdev_module = {
	.name	= "DSS Target",
};

void dss_io_task_submit_to_device(dss_io_task_t *task);

//static void
//_dss_nvmf_ns_hot_remove(struct spdk_nvmf_subsystem *subsystem,
//            void *cb_arg, int status)
//{
//    struct spdk_nvmf_ns *ns = cb_arg;
//    int rc;
//
//    rc = spdk_nvmf_subsystem_remove_ns(subsystem, ns->opts.nsid);
//    if (rc != 0) {
//        DSS_ERRLOG("Failed to make changes to NVME-oF subsystem with id: %u\n", subsystem->id);
//    }
//
//    spdk_nvmf_subsystem_resume(subsystem, NULL, NULL);
//}

static void
dss_nvmf_ns_hot_remove(void *remove_ctx)
{
    struct spdk_nvmf_ns *ns = remove_ctx;
    int rc;

    //TODO: Handle
    //rc = spdk_nvmf_subsystem_pause(ns->subsystem, _dss_nvmf_ns_hot_remove, ns);
    //if (rc) {
    //    DSS_ERRLOG("Unable to pause subsystem to process namespace removal!\n");
    //}
}

static void
_dss_nvmf_ns_resize(struct spdk_nvmf_subsystem *subsystem, void *cb_arg, int status)
{
    struct spdk_nvmf_ns *ns = cb_arg;

    //TODO: Handle
    //nvmf_subsystem_ns_changed(subsystem, ns->opts.nsid);
    spdk_nvmf_subsystem_resume(subsystem, NULL, NULL);
}

static void
dss_nvmf_ns_resize(void *event_ctx)
{
    struct spdk_nvmf_ns *ns = event_ctx;
    int rc;

    //TODO: Handle
    //rc = spdk_nvmf_subsystem_pause(ns->subsystem, _dss_nvmf_ns_resize, ns);
    //if (rc) {
    //    DSS_ERRLOG("Unable to pause subsystem to process namespace resize!\n");
    //}
}

static void
_dss_bdev_desc_event(enum spdk_bdev_event_type type,
	      struct spdk_bdev *bdev,
	      void *event_ctx)
{
	switch (type) {
	case SPDK_BDEV_EVENT_REMOVE:
        dss_nvmf_ns_hot_remove(event_ctx);
        break;
	case SPDK_BDEV_EVENT_RESIZE:
        dss_nvmf_ns_resize(event_ctx);
        break;
	default:
		DSS_NOTICELOG("Unsupported bdev event: type %d\n", type);
        DSS_ASSERT(0);
		break;
	}
}

dss_io_dev_status_t dss_io_device_open(const char *dev_name, dss_device_type_t type, dss_device_t **device)
{
    dss_device_t *d;
    dss_io_dev_status_t rc;
    int status;

    DSS_RELEASE_ASSERT(device != NULL);
    DSS_ASSERT(*device == NULL);

    d = calloc(1, sizeof(dss_device_t));
    if(!d) {
        *device = NULL;
        return DSS_IO_DEV_STATUS_ERROR;
    }

    d->dev_type = type;
    d->dev_name = strdup(dev_name);

    switch (type)
    {
    case DSS_BLOCK_DEVICE:
        d->bdev = spdk_bdev_get_by_name(d->dev_name);
        if(!d) {//Device not found
            *device = NULL;
            rc = DSS_IO_DEV_INVALID_DEVICE;
        } else {
            *device = d;
            rc = DSS_IO_DEV_STATUS_SUCCESS;
        }

        status = spdk_bdev_open_ext(d->dev_name, true, _dss_bdev_desc_event, d, &d->desc);
	    if (status != 0) {
		    DSS_ERRLOG("bdev %s cannot be opened, error=%d\n",
			        d->dev_name, status);
            *device = NULL;
		    return DSS_IO_DEV_STATUS_ERROR;
	    }
        DSS_ASSERT(d->desc != NULL);

        status = spdk_bdev_module_claim_bdev(d->bdev, d->desc, &dss_bdev_module);
		if (status != 0) {
			spdk_bdev_close(d->desc);
			free(d->dev_name);
			free(d);
            return DSS_IO_DEV_STATUS_ERROR;
		}

        d->n_ch = dss_env_get_spdk_max_cores();
        d->ch_arr = calloc(d->n_ch + 1, sizeof(dss_device_channel_t));
        if(!d->ch_arr) {
            *device = NULL;
            free(d);
            return DSS_IO_DEV_STATUS_ERROR;
        }
        break;
    default:
        *device = NULL;
        rc = DSS_IO_DEV_INVALID_TYPE;
        break;
    }
    return rc;
}


dss_io_dev_status_t dss_io_device_close(dss_device_t *device)
{
    dss_io_dev_status_t rc; 
    int i;

    DSS_RELEASE_ASSERT(device != NULL);
    DSS_RELEASE_ASSERT(device->dev_name);
    free(device->dev_name);
    switch (device->dev_type)
    {
    case DSS_BLOCK_DEVICE:
        //TODO: Release bdev
        for(i=0; i < device->n_ch; i++) {
            if(device->ch_arr[i].ch) {
                DSS_ASSERT(device->ch_arr[i].thread != NULL);
                dss_spdk_thread_send_msg(device->ch_arr[i].thread, (void *)spdk_put_io_channel, device->ch_arr[i].ch);
            }
        }
        spdk_bdev_module_release_bdev(device->bdev);
        spdk_bdev_close(device->desc);
        free(device->ch_arr);
        free(device);
        rc = DSS_IO_DEV_STATUS_SUCCESS;
        break;
    default:
        rc = DSS_IO_DEV_INVALID_TYPE;
        break;
    }
    return rc;
}

struct spdk_io_channel *dss_io_dev_get_channel(dss_device_t *io_device)
{
    uint32_t ch_arr_index = dss_env_get_current_core();
    struct spdk_io_channel *ch;

    DSS_ASSERT(ch_arr_index < io_device->n_ch);

    if(spdk_unlikely(!io_device->ch_arr[ch_arr_index].ch)) {
        io_device->ch_arr[ch_arr_index].ch = spdk_bdev_get_io_channel(io_device->desc);
        DSS_ASSERT(io_device->ch_arr[ch_arr_index].ch);
        io_device->ch_arr[ch_arr_index].thread = dss_env_get_spdk_thread();
        DSS_ASSERT(io_device->ch_arr[ch_arr_index].thread != NULL);
    }

    ch = io_device->ch_arr[ch_arr_index].ch;
    DSS_ASSERT(ch != NULL);

    return ch;
}




void _dss_io_task_op_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    dss_io_op_t *op = (dss_io_op_t *)cb_arg;
    dss_io_task_t *task = op->parent;
    dss_io_op_t *next_op;

    DSS_ASSERT(task->num_outstanding_ops >= 0);

    task->num_ops_done++;
    task->num_outstanding_ops--;

    // TODO: Get nvme status
    // spdk_bdev_io_get_nvme_status(bdev_io, &cdw0, &sct, &sc);
    spdk_bdev_free_io(bdev_io);

    TAILQ_REMOVE(&task->ops_in_progress, op, op_next);
    if(success) {
        TAILQ_INSERT_TAIL(&task->op_done, op, op_next);
    } else {
        TAILQ_INSERT_TAIL(&task->failed_ops, op, op_next);
        //TODO: Update failure code to OP
        task->task_status = DSS_IO_TASK_STATUS_ERROR;
    }

    if(task->num_outstanding_ops != 0) {
        //Do nothing the last completing op will complete IO or trigger more
        return;
    }

    next_op = TAILQ_FIRST(&task->op_todo_list);
    if(next_op) {
        dss_io_task_submit_to_device(task);
    } else {//All operations completed
        //Note: Assumption net module is always present
        //TODO: If there are no module threads then this callback needs to happen and trigger 
        //      further processing of the request
        DSS_ASSERT(task->cb_minst);
        DSS_ASSERT(task->cb_ctx);
        dss_module_post_to_instance(DSS_MODULE_NET, task->cb_minst, task->cb_ctx);
    }


    return;
}

void dss_io_task_submit_to_device(dss_io_task_t *task)
{
    dss_io_op_t *curr_op, *tmp_op;
    dss_device_t *io_device;

    struct spdk_io_channel *ch;
    int rc;

    TAILQ_FOREACH_SAFE(curr_op, &task->op_todo_list, op_next, tmp_op) {
        //TODO: io task management
        io_device = curr_op->device;
        ch = dss_io_dev_get_channel(io_device);
        switch(curr_op->opc) {
            case DSS_IO_BLK_READ:
                rc = spdk_bdev_read_blocks(io_device->desc, ch, curr_op->rw.data, \
                                                           curr_op->rw.lba, \
                                                           curr_op->rw.nblocks,
                                                           _dss_io_task_op_complete,
                                                           curr_op);
                break;
            case DSS_IO_BLK_WRITE:
                rc = spdk_bdev_write_blocks(io_device->desc, ch, curr_op->rw.data, \
                                                            curr_op->rw.lba, \
                                                            curr_op->rw.nblocks,
                                                            _dss_io_task_op_complete,
                                                            curr_op);
                break;
            default:
                DSS_ASSERT(0);
        }
        DSS_ASSERT(rc == 0);
        //TODO: Handle IO submission failure case

        TAILQ_REMOVE(&task->op_todo_list, curr_op, op_next);
        TAILQ_INSERT_TAIL(&task->ops_in_progress, curr_op, op_next);
        task->num_outstanding_ops++;
        if (curr_op->is_blocking) {
            break;
        }
    }

    //Must have atleast one operation
    DSS_ASSERT(task->num_outstanding_ops != 0);

    return;
}

dss_io_task_status_t dss_io_task_submit(dss_io_task_t *task)
{
    DSS_ASSERT(task->dreq);
    if(task->io_task_module->io_module) {
        //Send to io module
        dfly_module_post_request(task->io_task_module->io_module, task->dreq);
    } else {
        //Send direct
        dss_io_task_submit_to_device(task);
    }
    return DSS_IO_TASK_STATUS_SUCCESS;
}
