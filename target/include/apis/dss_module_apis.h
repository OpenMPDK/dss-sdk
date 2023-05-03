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

typedef struct dss_request_s dss_request_t;//TODO: Move to another applicable header file
typedef enum dss_module_type_e {
	DSS_MODULE_START_INIT = 0,
	//Modules in the order of initialization
	DSS_MODULE_LOCK = 1,
	DSS_MODULE_FUSE,//2,
	DSS_MODULE_NET ,//3
	DSS_MODULE_IO  ,//4,
	DSS_MODULE_LIST,//5,
	DSS_MODULE_WAL ,//6,
	DSS_MODULE_END  //7
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

//TODO: typedef to enable deprecating old names
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
	DSS_NVMF_BLK_IO_OPC_WRITE
} dss_request_opc_t;

typedef enum dss_request_rc_e {
	DSS_REQ_STATUS_SUCCESS = 0,
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
typedef struct dss_module_req_ctx_s {
	/// @brief Module pointer.
	dss_module_t *module;
	/// @brief Module instance context for processing current request
	dss_module_instance_t *module_instance;
	union {
		dss_net_req_ctx_t net;
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

void *dss_module_get_subsys_ctx( dss_module_type_t type, void *ss);

void dss_module_set_subsys_ctx( dss_module_type_t type, void *ss, void *ctx);

#ifdef __cplusplus
}
#endif

#endif // DSS_MODULE_H
