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

#ifndef DSS_MODULE_H
#define DSS_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>

typedef struct dss_request_s dss_request_t;//TODO: Move to another applicable header file
typedef enum dss_module_type_e {
	DSS_MODULE_START_INIT = 0,
	//Modules in the order of initialization
	DSS_MODULE_LOCK = 1,
	DSS_MODULE_FUSE,//2,
	DSS_MODULE_NET ,//3
	DSS_MODULE_IO  ,//4,
	DSS_MODULE_KVTRANS, //5,
	DSS_MODULE_LIST,//6,
	DSS_MODULE_WAL ,//7,
	DSS_MODULE_END  //8
} dss_module_type_t;

typedef enum dss_module_status_e {
	DSS_MODULE_STATUS_SUCCESS = 0,
	DSS_MODULE_STATUS_MOD_INST_NOT_CLEARED = 1,
	DSS_MODULE_STATUS_MOD_CREATED,
	DSS_MODULE_STATUS_INITIALIZED,
	DSS_MODULE_STATUS_INIT_PENDING,
	DSS_MODULE_STATUS_MOD_DESTROYED,
	DSS_MODULE_STATUS_MOD_DESTROY_PENDING,
	/* Add new error code here*/
	DSS_MODULE_STATUS_ERROR = -1
} dss_module_status_t;

typedef struct dss_module_config_s {
    uint32_t id;
    int num_cores;
    int numa_node;
    bool async_load_enabled;
} dss_module_config_t;

//TODO: typedef to enable deprecating old names
typedef struct df_module_event_complete_s dss_module_cmpl_evt_t;
typedef struct dfly_module_poller_instance_s dss_module_instance_t;
typedef struct dfly_module_s dss_module_t;
typedef struct dfly_module_ops dss_module_ops_t;
typedef struct dfly_subsytem dss_subsystem_t;

typedef enum dss_request_opc_e {
	DSS_NVMF_IO_OPC_NO_OP = 0,
	DSS_NVMF_KV_IO_OPC_STORE,
	DSS_NVMF_KV_IO_OPC_RETRIEVE,
	DSS_NVMF_KV_IO_OPC_DELETE,
	DSS_NVMF_KV_IO_OPC_LIST,
	DSS_NVMF_BLK_IO_OPC_READ,
	DSS_NVMF_BLK_IO_OPC_WRITE,
	DSS_NVMF_KV_IO_OPC_LIST_READ,
    DSS_NVMF_IO_PT,
    DSS_INTERNAL_IO
} dss_request_opc_t;

typedef enum dss_request_rc_e {
    DSS_REQ_STATUS_SUCCESS = 0,
    DSS_REQ_STATUS_KEY_NOT_FOUND,
    /* Add new error codes here */
    DSS_REQ_STATUS_ERROR = -1
} dss_request_rc_t;

//Network structs
typedef enum dss_net_request_state_e {
    DSS_NET_REQUEST_FREE = 0,
    DSS_NET_REQUEST_INIT,
	DSS_NET_REQUEST_SUBMITTED,
    DSS_NET_REQUEST_COMPLETE
} dss_net_request_state_t;

typedef struct dss_net_req_ctx_s {
    dss_net_request_state_t state;
	void *nvmf_req;//struct spdk_nvmf_request
} dss_net_req_ctx_t;
//End - Network structs

//IO Task Common
typedef struct dss_device_s dss_device_t;
typedef struct dss_io_task_s dss_io_task_t;
typedef struct dss_io_task_module_s dss_io_task_module_t;
//End - IO task Common

//KV Trans Module

typedef uint32_t key_size_t;
typedef struct kvtrans_ctx_s kvtrans_ctx_t;
typedef struct kvtrans_req kvtrans_req_t;

typedef enum dss_kvt_state_e {
    DSS_KVT_LOADING_SUPERBLOCK = 0,
    DSS_KVT_LOAD_SUPERBLOCK_COMPLETE,
    DSS_KVT_LOADING_BA_META,
    DSS_KVT_LOADING_DC_HT,
    DSS_KVT_INITIALIZED
} dss_kvt_state_t;

typedef struct dss_kvt_init_ctx_s {
    dss_device_t *dev;
    dss_io_task_module_t *iotm;
    kvtrans_ctx_t **kvt_ctx;
    void *data;
    uint64_t data_len;
    dss_kvt_state_t state;
    // Block allocator specific variables
    uint64_t ba_meta_start_block;
    uint64_t ba_meta_end_block;
    uint64_t ba_meta_size;
    uint64_t ba_meta_num_blks;
    uint64_t ba_disk_read_it;
    uint64_t ba_disk_read_total_it;
    uint64_t ba_meta_num_blks_per_iter;
    uint64_t logical_block_size;
    uint64_t ba_meta_start_block_per_iter;
    // DC table specific variables
    uint64_t dss_dc_idx;
} dss_kvt_init_ctx_t;

typedef double tick_t;
#define KEY_LEN 1024

typedef struct req_key_s {
    char key[KEY_LEN];
    key_size_t length;
} req_key_t;

typedef struct req_value_s {
    void *value;
    uint64_t length;
    uint64_t offset;
} req_value_t;

typedef enum kv_op_e {
    KVTRANS_OPC_STORE = 0,
    KVTRANS_OPC_RETRIEVE,
    KVTRANS_OPC_DELETE,
    KVTRANS_OPC_EXIST
} kv_op_t;

// typedef clock_t tick_t;
// typedef double tick_t;

typedef struct req_s {
    req_key_t req_key;
    req_value_t req_value; 
    struct timespec req_ts;
    kv_op_t opc;
} req_t;

enum kvtrans_req_e {
    REQ_INITIALIZED = 0,
    QUEUE_TO_LOAD_ENTRY,
    ENTRY_LOADING_DONE,
    QUEUE_TO_LOAD_COL,
    COL_LOADING_DONE,
    QUEUE_TO_LOAD_COL_EXT,
    COL_EXT_LOADING_DONE,
    COL_EXT_LOADING_CONTIG,
    QUEUE_TO_START_IO,
    QUEUED_FOR_DATA_IO,
    IO_CMPL,
    REQ_CMPL
};

struct req_time_tick {
    tick_t bg;
    tick_t hash;
    tick_t keyset;
    tick_t valset;
    tick_t cmpl;
};


/**
 *  @brief kvtrans request context
 */
struct kvtrans_req{
    req_t req;
    bool initialized;
	bool req_allocated;
    uint64_t id;
    enum kvtrans_req_e state;
    kvtrans_ctx_t *kvtrans_ctx;
    dss_io_task_t *io_tasks;
    bool io_to_queue;
    bool ba_meta_updated;
    // a blk_ctx to maintain meta info
    TAILQ_HEAD(blk_elm, blk_ctx) meta_chain;
    int32_t num_meta_blk;

    struct req_time_tick time_tick;
    dss_request_t *dreq;
#ifdef DSS_BUILD_CUNIT_TEST
    STAILQ_ENTRY(kvtrans_req) req_link;
#endif

};

/**
 * @brief Initialize kvtrans context when the request is allocated first time
 * 
 * @param req dss request that needs to be initialized
 */
void dss_kvtrans_init_req_on_alloc(dss_request_t *req);

//End - DSS KV Trans module


typedef struct dss_module_req_ctx_s {
	/// @brief Module pointer.
	dss_module_t *module;
	/// @brief Module instance context for processing current request
	dss_module_instance_t *module_instance;
	union {
		dss_net_req_ctx_t net;
		kvtrans_req_t kvt;
        dss_kvt_init_ctx_t kvt_init;
	} mreq_ctx;//Module request context
} dss_module_req_ctx_t;

/**
 * @brief Submit a request directly to a given module thread
 *
 * @param mtype Type of module used to verify module_thread_instance is correct
 * @param module_thread_instance Module instance pointer to submit the request
 * @param req Request to be submitted. This should be the one the module would expect to process.
 * @return dss_module_status_t DSS_MODULE_STATUS_SUCCESS if successfull, DSS_MODULE_STATUS_ERROR otherwise
 */
dss_module_status_t dss_module_post_to_instance(dss_module_type_t mtype, dss_module_instance_t *module_thread_instance, void *req);


/**
 * @brief Submit a request directly to a given module thread's completion queue
 *
 * @param mtype Type of module used to verify module_thread_instance is correct
 * @param module_thread_instance Module instance pointer to submit the request
 * @param req Request to be submitted. This should be the one the module would expect to process.
 * @return dss_module_status_t DSS_MODULE_STATUS_SUCCESS if successfull, DSS_MODULE_STATUS_ERROR otherwise
 */
dss_module_status_t dss_module_post_to_instance_cq(dss_module_type_t mtype, dss_module_instance_t *module_thread_instance, void *req);

/**
 * @brief Find module instance corresponding to the request
 *
 * @param module Module context for which instance need to be found
 * @param req Request to find custom instance context
 * @param[OUT] module_instance Set the module thread instance if NULL
 * @return dss_module_status_t DSS_MODULE_STATUS_MOD_INST_NOT_CLEARED if `*module_instance` was populated, DSS_MODULE_STATUS_SUCCESS if successfully set, DSS_MODULE_STATUS_ERROR otherwise
 */
dss_module_status_t dss_module_get_instance(dss_module_t *module , dss_request_t *req, dss_module_instance_t **module_instance);

/**
 * @brief
 *
 * @param module Find module instance corresponding to the core
 * @param core Target core where the instance is running
 * @param[OUT] module_instance Set the module thread instance if NULL
 * @return dss_module_status_t DSS_MODULE_STATUS_MOD_INST_NOT_CLEARED if `*module_instance` was populated, DSS_MODULE_STATUS_SUCCESS if successfully set, DSS_MODULE_STATUS_ERROR otherwise
 */
dss_module_status_t dss_module_get_instance_for_core(dss_module_t *module , uint64_t core, dss_module_instance_t **module_instance);

/**
 * @brief Populate the default module config
 *
 * @param[OUT] c Config struct that will be updated
 */
void dss_module_set_default_config(dss_module_config_t *c);

/**
 * @brief Initializes module for asyc loading
 *
 * @param m Module that needs to load asynchronously
 * @param loading Indicates whether this is loading or unloading
 * @param cmpl_evt Event to trigger module init/destroy completion
 * @return dss_module_status_t DSS_MODULE_STATUS_SUCCESS if successfull, DSS_MODULE_STATUS_ERROR otherwise
 */
dss_module_status_t dss_module_init_async_load(dss_module_t *m, bool loading, dss_module_cmpl_evt_t *cmpl_evt);

/**
 * @brief Increment reference count to add a new async task
 *
 * @param m Module to add an async task
 * @return dss_module_status_t DSS_MODULE_STATUS_SUCCESS if successfull, DSS_MODULE_STATUS_ERROR otherwise
 */
dss_module_status_t dss_module_inc_async_pending_task(dss_module_t *m);

/**
 * @brief Decrement reference corresponding to added task on completion
 *
 * @param m Module to complete an async task
 * @return dss_module_status_t DSS_MODULE_STATUS_SUCCESS if successfull, DSS_MODULE_STATUS_ERROR otherwise
 */
dss_module_status_t dss_module_dec_async_pending_task(dss_module_t *m);

void *dss_module_get_subsys_ctx( dss_module_type_t type, void *ss);

void dss_module_set_subsys_ctx( dss_module_type_t type, void *ss, void *ctx);

void dss_net_setup_nvmf_resp(dss_request_t *req, dss_request_rc_t status, uint32_t cdw0);

#ifdef __cplusplus
}
#endif

#endif // DSS_MODULE_H
