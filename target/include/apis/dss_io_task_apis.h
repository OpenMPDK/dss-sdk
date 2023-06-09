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

#ifndef DSS_IO_TASK_API_H
#define DSS_IO_TASK_API_H

#include <stdint.h>
#include "apis/dss_module_apis.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dss_device_type_e {
    DSS_BLOCK_DEVICE = 1,
} dss_device_type_t;

typedef enum dss_io_dev_status_e {
    DSS_IO_DEV_STATUS_SUCCESS = 0,
    DSS_IO_DEV_INVALID_TYPE = 1,
    DSS_IO_DEV_INVALID_DEVICE,
    /*Add new errors here */
    DSS_IO_DEV_STATUS_ERROR = -1
} dss_io_dev_status_t;

typedef enum dss_io_task_module_status_e {
    DSS_IO_TASK_MODULE_STATUS_SUCCESS = 0,
    /*Add new errors here */
    DSS_IO_TASK_MODULE_STATUS_ERROR = -1
} dss_io_task_module_status_t;

typedef enum dss_io_task_status_e {
    DSS_IO_TASK_STATUS_SUCCESS = 0,
    /*Add new errors here */
    DSS_IO_TASK_STATUS_ERROR = -1
} dss_io_task_status_t;

typedef struct dss_io_task_module_opts_s {
    uint32_t max_io_tasks;
    uint32_t max_io_ops;
} dss_io_task_module_opts_t;

typedef struct dss_device_s dss_device_t;
typedef struct dss_io_task_module_s dss_io_task_module_t;
typedef struct dss_iov_s dss_iov_t;

/**
 * @brief Opens a device that can be used to submit IO tasks
 *
 * @param dev_name Identifier name string that can be use to open the device
 * @param type device type
 * @param[OUT] dev device handle pointer corresponding to the opened device
 * @return dss_io_dev_status_t DSS_IO_DEV_STATUS_SUCCESS on success, otherwise DSS_IO_DEV_STATUS_ERROR
 */
dss_io_dev_status_t  dss_io_device_open(char *dev_name, dss_device_type_t type, dss_device_t **dev);

/**
 * @brief Close the device opened by a previous call to dss_io_device_open
 *
 * @param device pointer that was returned by a previous dss_io_device_open call
 * @return dss_io_dev_status_t DSS_IO_DEV_STATUS_SUCCESS on success, otherwise DSS_IO_DEV_STATUS_ERROR
 */
dss_io_dev_status_t dss_io_device_close(dss_device_t *device);

/**
 * @brief Initializes and returns a io_task module context
 * 
 * @param io_task_opts Options for initializing io module
 * @param[OUT] module Context pointer for io task module
 * @return dss_io_task_module_status_t DSS_IO_TASK_MODULE_STATUS_SUCCESS on succes, DSS_IO_TASK_MODULE_STATUS_ERROR otherwise
 */
dss_io_task_module_status_t dss_io_task_module_init(dss_io_task_module_opts_t io_task_opts, dss_io_task_module_t **module);

/**
 * @brief Free resources of the io task module
 *
 * @param m Context pointer of io task module to be freed
 * @return dss_io_task_module_status_t DSS_IO_TASK_MODULE_STATUS_SUCCESS on succes, DSS_IO_TASK_MODULE_STATUS_ERROR otherwise
 */
dss_io_task_module_status_t dss_io_task_module_end(dss_io_task_module_t *m);

/**
 * @brief Get a new IO vector for use with IO operation
 *
 * @param m Context pointer of io task module
 * @return dss_iov_t* An empty IO vector allocated
 */
// dss_iov_t *dss_io_iov_get(dss_io_task_module_t *m);

/**
 * @brief Return provided IO vector to module io vector pool
 *
 * @param m Context pointer of io task module
 * @param iov IO vector to be returned back to io module
 * @return dss_io_task_status_t DSS_IO_TASK_STATUS_SUCCESS on succes, DSS_IO_TASK_STATUS_ERROR otherwise
 */
// dss_io_task_status_t dss_io_iov_free(dss_io_task_module_t *m, dss_iov_t *iov);

/**
 * @brief Add a data region to IO vector
 *
 * @param iov IO vector that the data region has to be added
 * @param data Pointer to the memory containing the data
 * @param len Length of the data region
 * @param offset Offset from the data* where the valid data starts
 * @return dss_io_task_status_t DSS_IO_TASK_STATUS_SUCCESS on succes, DSS_IO_TASK_STATUS_ERROR otherwise
 */
// dss_io_task_status_t dss_io_iov_add(dss_iov_t *iov, void *data, uint64_t len, uint64_t offset);

/**
 * @brief Clear data regions in the IO vector
 *
 * @param iov IO vector that the data regions need to be cleared
 */
// void dss_io_iov_clear(dss_iov_t *iov);

/**
 * @brief Get an IO task from the IO module task pool
 *
 * @param m Context pointer of io task module
 * @param[OUT] task IO task context
 * @return dss_io_task_status_t DSS_IO_TASK_STATUS_SUCCESS on succes, DSS_IO_TASK_STATUS_ERROR otherwise
 */
dss_io_task_status_t dss_io_task_get_new(dss_io_task_module_t *m, dss_io_task_t **task);

/**
 * @brief Return an IO task to the IO module task pool
 *
 * @param io_task pointer to IO task context
 * @return dss_io_task_status_t DSS_IO_TASK_STATUS_SUCCESS on succes, DSS_IO_TASK_STATUS_ERROR otherwise
 * 
 */
dss_io_task_status_t dss_io_task_put(dss_io_task_t *io_task);

/**
 * @brief Setup IO task call back module instance context and callback context
 *
 * @param io_task IO task context pointer
 * @param cb_minst Module instance context to invoke on completion
 * @param cb_ctx Context passed back on completion call
 * @return dss_io_task_status_t DSS_IO_TASK_STATUS_SUCCESS on succes, DSS_IO_TASK_STATUS_ERROR otherwise
 */
dss_io_task_status_t dss_io_task_setup(dss_io_task_t *io_task, dss_module_instance_t *cb_minst, void *cb_ctx);

/**
 * @brief Add a block readv operation to the IO task
 *
 * @param task IO task context where operation needs to be added
 * @param target_dev Target IO device
 * @param lba Target LBA on disk
 * @param num_blocks Number blocks corresponding to the IO operation starting from `lba`
 * @param iov IO vector having ranges correspoing to the data
 * @param is_blocking Indicates if the current operation is blocking until completion
 * @return dss_io_task_status_t DSS_IO_TASK_STATUS_SUCCESS on succes, DSS_IO_TASK_STATUS_ERROR otherwise
 */
// dss_io_task_status_t dss_io_task_add_blk_readv(dss_io_task_t *task, dss_device_t *target_dev, uint64_t lba, uint64_t num_blocks, dss_iov_t *iov, bool is_blocking);

/**
 * @brief Add a block writev operation to the IO task
 *
 * @param task IO task context where operation needs to be added
 * @param target_dev Target IO device
 * @param lba Target LBA on disk
 * @param num_blocks Number blocks corresponding to the IO operation starting from `lba`
 * @param iov IO vector having ranges correspoing to the data
 * @param is_blocking Indicates if the current operation is blocking until completion
 * @return dss_io_task_status_t DSS_IO_TASK_STATUS_SUCCESS on succes, DSS_IO_TASK_STATUS_ERROR otherwise
 */
// dss_io_task_status_t dss_io_task_add_blk_writev(dss_io_task_t *task, dss_device_t *target_dev, uint64_t lba, uint64_t num_blocks, dss_iov_t *iov, bool is_blocking);

/**
 * @brief Add a block read operation to the IO task
 *
 * @param task IO task context where operation needs to be added
 * @param target_dev Target IO device
 * @param lba Target LBA on disk
 * @param num_blocks Number blocks corresponding to the IO operation starting from `lba`
 * @param data Pointer to data start
 * @param len Length of data
 * @param offset byte offset from LBA to read data
 * @param is_blocking
 * @return dss_io_task_status_t
 */
dss_io_task_status_t dss_io_task_add_blk_read(dss_io_task_t *task, dss_device_t *target_dev, uint64_t lba, uint64_t num_blocks, void *data, uint64_t len, uint64_t offset, bool is_blocking);

/**
 * @brief Add a block write operation to the IO task
 *
 * @param task IO task context where operation needs to be added
 * @param target_dev Target IO device
 * @param lba Target LBA on disk
 * @param num_blocks Number blocks corresponding to the IO operation starting from `lba`
 * @param data Pointer to data start
 * @param len Length of data
 * @param offset byte offset from LBA to write data
 * @param is_blocking Indicates if the current operation is blocking until completion
 * @return dss_io_task_status_t
 */
dss_io_task_status_t dss_io_task_add_blk_write(dss_io_task_t *task, dss_device_t *target_dev, uint64_t lba, uint64_t num_blocks, void *data, uint64_t len, uint64_t offset, bool is_blocking);

/**
 * @brief Submit a populated IO task to IO module
 *
 * @param task IO task to be submitted
 * @return dss_io_task_status_t DSS_IO_TASK_STATUS_SUCCESS on succes, DSS_IO_TASK_STATUS_ERROR otherwise
 */
dss_io_task_status_t dss_io_task_submit(dss_io_task_t *task);

#ifdef __cplusplus
}
#endif

#endif //DSS_IO_TASK_API_H
