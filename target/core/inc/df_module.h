/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
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
#ifndef DF_MODULE_H
#define DF_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "df_stats.h"

#define MAX_MODULE_NAME_LEN (64)
#define REQ_PER_POLL (8)
#define MAX_CPU (256)

#define DFLY_MODULE_MSG_MP_SC

typedef enum dfly_module_request_status_s {
	/// @brief Note: Do not use DFLY_MODULE_REQUEST_PROCESSED in new code. This will be deprecated
	DFLY_MODULE_REQUEST_PROCESSED = 0,
	DFLY_MODULE_REQUEST_PROCESSED_INLINE,
	DFLY_MODULE_REQUEST_QUEUED,
} dfly_module_request_status_t;

struct dfly_module_pipe_s {
	struct spdk_ring *msg_ring;//To the module
	struct spdk_ring *cmpl_ring;//Request Completion to module
};

typedef struct dfly_module_s {
	char name[MAX_MODULE_NAME_LEN];
	dss_module_type_t mtype;
	struct dfly_module_ops *ops;

	pthread_mutex_t module_lock;
#if defined DFLY_MODULE_MSG_MP_SC
	int num_threads;
	struct dfly_module_poller_instance_s *active_thread_arr[MAX_CPU];
	int thread_index_arr[MAX_CPU];
#endif
	void *ctx; //Module Context info

	TAILQ_HEAD(, dfly_module_poller_instance_s) active_threads;

} dfly_module_t;

struct dfly_module_poller_instance_s {
	int icore;
	struct spdk_poller *mpoller; //pollers
	void *ctx;//instance context info
	dfly_module_t *module;
#if defined DFLY_MODULE_MSG_MP_SC
	struct dfly_module_pipe_s pipe;
#endif
	int status;
	stat_module_t *stat_module;
	TAILQ_ENTRY(dfly_module_poller_instance_s) link;// Link of the list of pollers
};

typedef int (*req_poller_fn)(void *ctx, struct dfly_request *req);
typedef int (*gen_poller_fn)(void *ctx);

typedef void (*df_module_event_complete_cb)(void *cb_event, void *dummy_arg);

struct df_module_event_complete_s {
	uint32_t src_core;
	df_module_event_complete_cb cb_fn;
	void *arg1;
	void *arg2;
};

struct dfly_module_s *dfly_module_start(const char *name, int id, dss_module_type_t mtype, struct dfly_module_ops *mops,
					void *ctx,
					int num_cores, int numa_node, df_module_event_complete_cb cb, void *cb_arg);
void dfly_module_stop(struct dfly_module_s *module, df_module_event_complete_cb cb, void *cb_arg, void *cb_private);
void dfly_module_post_request(struct dfly_module_s *module, struct dfly_request *req);
void dfly_module_complete_request(struct dfly_module_s *module, struct dfly_request *req);
void *dfly_module_get_ctx(struct dfly_module_s *module);

struct dfly_module_ops {
	void *(*module_init_instance_context)(void *module_ctx, void *ctx/*Module instance */,
					      int inst_index); /* When the  thread starts find the zone corresponding to the thread_index and copy the ctx pointer to be returned by find context based on request(find_instance_context) */
	//Poller callback function for processing task
	int (*module_rpoll)(void *ctx, struct dfly_request *req);//Request Polling
	int (*module_cpoll)(void *ctx, struct dfly_request *req);//Completion Polling
	//Poller for other non request tasks should return as soon as possible
	int (*module_gpoll)(void *ctx);//Generic polling should return in finite time
	void *(*find_instance_context)(struct dfly_request
				       *req);//	Find the module instance context based on the request
	void *(*module_instance_destroy)(void *mctx, void *inst_ctx);
};

#ifdef __cplusplus
}
#endif

#endif // DF_MODULE_H
