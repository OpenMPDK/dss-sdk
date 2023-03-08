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


//#include <string.h>

#include <dragonfly.h>

typedef struct dfly_tpool_s {
	char name[MAX_MODULE_NAME_LEN];
	tpool_req_process_fn tpool_req_process;//Request Polling

	pthread_mutex_t module_lock;
	int num_threads;
	struct dfly_tpool_instance_s *active_thread_arr[MAX_CPU];
	int thread_index_arr;
	void *ctx; //Module Context info

	TAILQ_HEAD(, dfly_tpool_instance_s) active_threads;

} dss_tpool_t;

struct dfly_tpool_instance_s {
	int icore;
	void *ctx;//instance context info
	dss_tpool_t *module;
	struct dfly_module_pipe_s pipe;
	int status;
	pthread_t th_h;
	TAILQ_ENTRY(dfly_tpool_instance_s) link;// Link of the list of pollers
};

int _tpool_poller(struct dfly_tpool_instance_s *m_inst)
{
	int nprocessed = 0;
	int num_msgs, i;
	int ret;

	void *reqs[REQ_PER_POLL] = {NULL};

	//deque from ring
	num_msgs = spdk_ring_dequeue(m_inst->pipe.msg_ring, reqs, REQ_PER_POLL);
	//call registered function for each request
	for (i = 0; i < num_msgs; i++) {
		struct dfly_request *req = (struct dfly_request *)reqs[i];

		req->tgt_core = spdk_env_get_current_core();//m_inst->mpoller->lcore;

		m_inst->module->tpool_req_process(m_inst->module->ctx, req); //Returns value
		//Process request inline
		dfly_handle_request(req);
	}
	nprocessed += num_msgs;

	return nprocessed;
}

void *tpool_poller(void *ctx)
{
	struct dfly_tpool_instance_s *m_inst = (struct dfly_tpool_instance_s *)ctx;

	cpu_set_t cpuset;
	int rc;

	CPU_ZERO(&cpuset);
	CPU_SET(m_inst->icore, &cpuset);

	rc = pthread_setaffinity_np(m_inst->th_h, sizeof(cpu_set_t), &cpuset);
	if(rc != 0) {
		DFLY_NOTICELOG("Failed to bind to core %d error: %d\n", m_inst->icore, rc);
	}

	//TODO: Set thread name
    //pthread_setname_np(m_inst->th_h, name);

	while(m_inst->status == 1) {
		_tpool_poller(m_inst);
	}

	//TODO: Flush pending requests

	return ctx;
}

static inline struct dfly_tpool_instance_s *dss_tpool_get_th_inst(dss_tpool_t *module)
{
	struct dfly_tpool_instance_s *m_inst = NULL;

	int thread_index = 0;

	module->thread_index_arr++;
	module->thread_index_arr %= module->num_threads;

	thread_index = module->thread_index_arr;

	m_inst = module->active_thread_arr[thread_index];

	return (m_inst);
}


void dss_tpool_post_request(struct dfly_tpool_s *module, struct dfly_request *req)
{
	int rc;

	struct dfly_tpool_instance_s *m_inst;

	//TODO:
	//	Post to queue only if initialized

	//Default to round robin across module instance
	m_inst = dss_tpool_get_th_inst(module);

	DFLY_ASSERT(req);

	rc = spdk_ring_enqueue(m_inst->pipe.msg_ring, (void **)&req, 1, NULL);
	if (rc != 1) {
		assert(false);
	}
}

//
//void *dfly_module_get_ctx(struct dfly_tpool_s *module)
//{
//	return module->ctx;
//}
//
//int  dfly_module_get_instance_icore(void *inst)
//{
//	struct dfly_tpool_instance_s *m_inst = (struct dfly_tpool_instance_s *) inst;
//	return m_inst->icore;
//}

void dss_tpool_thread_start(void *inst)
{
	struct dfly_tpool_instance_s *m_inst = (struct dfly_tpool_instance_s *)inst;
	dss_tpool_t *module = m_inst->module;

	pthread_mutex_lock(&module->module_lock);

	DFLY_INFOLOG(DFLY_LOG_MODULE, "Launching tpool thread %p\n", m_inst);
#if defined DFLY_MODULE_MSG_MP_SC
	//Initialize spdk ring
	m_inst->pipe.msg_ring = spdk_ring_create(SPDK_RING_TYPE_MP_SC, 65536, SPDK_ENV_SOCKET_ID_ANY);
	assert(m_inst->pipe.msg_ring);

#endif

#if defined DFLY_MODULE_MSG_MP_SC
	module->active_thread_arr[module->num_threads] = m_inst;
	module->num_threads++;
#endif

	m_inst->status = 1;

	pthread_create(&m_inst->th_h, NULL, tpool_poller, (void *)m_inst);

	pthread_mutex_unlock(&module->module_lock);

	return;

}

struct dfly_tpool_s *dss_tpool_start(const char *name, int id,
					void *ctx, int num_threads,
					tpool_req_process_fn f_proc_reqs)
{

	dss_tpool_t *module;
	struct dfly_tpool_instance_s *m_inst;
	int i;

	module = (dss_tpool_t *)calloc(1, sizeof(dss_tpool_t));
	if (!module) {
		return NULL;
	}

	strncpy(module->name, name, MAX_MODULE_NAME_LEN - 1);

	module->tpool_req_process = f_proc_reqs;
	assert(module->tpool_req_process);


	TAILQ_INIT(&module->active_threads);
	module->ctx = ctx;

	m_inst = (struct dfly_tpool_instance_s *)calloc(1,
			sizeof(struct dfly_tpool_instance_s) * num_threads);
	if (!m_inst) {
		free(module);
		return NULL;
	}

	char tpool_name[MAX_MODULE_NAME_LEN];
	memset(tpool_name, 0, 64);
	sprintf(tpool_name, "%s_%d", name, id);
	DFLY_ASSERT(strlen(tpool_name) < MAX_MODULE_NAME_LEN);

	pthread_mutex_init(&module->module_lock, NULL);


	for (i = 0; i < num_threads; i++) {
		m_inst[i].ctx = ctx;
		m_inst[i].module = module;
		//m_inst[i].icore = spdk_env_get_current_core();
		m_inst[i].icore = dfly_get_next_core(tpool_name, 1, NULL, NULL);

		TAILQ_INSERT_TAIL(&module->active_threads, &m_inst[i], link);

		dss_tpool_thread_start(&m_inst[i]);
	}


	return module;
}

void dfly_tpool_thread_stop(void *ctx)
{
	struct dfly_tpool_instance_s *m_inst = (struct dfly_tpool_instance_s *)ctx;
	struct dfly_tpool_s *module = m_inst->module;
	struct dfly_tpool_instance_s *m_inst_next = NULL;

	m_inst->status = 0;

	pthread_join(m_inst->th_h, NULL);

	if (m_inst->pipe.msg_ring != NULL) {
		spdk_ring_free(m_inst->pipe.msg_ring);
		m_inst->pipe.msg_ring = NULL;
	}

	m_inst_next = TAILQ_NEXT(m_inst, link);
	memset(m_inst, 0, sizeof(struct dfly_tpool_instance_s));

	if (m_inst_next != NULL) {
		dfly_tpool_thread_stop(m_inst_next);
	} else {
		m_inst_next = TAILQ_FIRST(&module->active_threads);
		free(m_inst_next);
	}

}

void dfly_tpool_stop(struct dfly_tpool_s *module)
{
	struct dfly_tpool_instance_s *m_inst;
	struct spdk_event *event;

	pthread_mutex_lock(&module->module_lock);

	m_inst = TAILQ_FIRST(&module->active_threads);

	//TODO: Stop all threads iteratively
	dfly_tpool_thread_stop(m_inst);

	pthread_mutex_unlock(&module->module_lock);

	memset(module, 0, sizeof(dss_tpool_t));
	free(module);

}
