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
#include "apis/dss_io_task_apis.h"
#include "spdk/bdev.h"

struct dss_device_s {
    char *dev_name;
    dss_device_type_t dev_type;
    struct spdk_bdev *bdev;
};

dss_io_dev_status_t  dss_io_device_open(char *dev_name, dss_device_type_t type, dss_device_t **device)
{
    dss_device_t *d;
    dss_io_dev_status_t rc;

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
        //TODO: Claim Device
        if(!d) {//Device not found
            *device = NULL;
            rc = DSS_IO_DEV_INVALID_DEVICE;
        } else {
            *device = d;
            rc = DSS_IO_DEV_STATUS_SUCCESS;
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

    DSS_RELEASE_ASSERT(device != NULL);
    DSS_RELEASE_ASSERT(device->dev_name);
    free(device->dev_name);
    switch (device->dev_type)
    {
    case DSS_BLOCK_DEVICE:
        //TODO: Release bdev
        rc = DSS_IO_DEV_STATUS_SUCCESS;
        break;
    default:
        rc = DSS_IO_DEV_INVALID_TYPE;
        break;
    }
    return rc;
}

dss_io_task_status_t dss_io_task_submit(dss_io_task_t *task)
{
    return DSS_IO_TASK_STATUS_ERROR;
}