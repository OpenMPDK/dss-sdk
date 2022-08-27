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


#include <string.h>

#include <dragonfly.h>

//Only one of (DFLY_MODULE_MSG_MP_SC/DFLY_MODULE_MSG_SP_SC)
//should be defined
//#define DFLY_MODULE_MSG_MP_SC
//#define DFLY_MODULE_MSG_SP_SC
//

int module_poller(void *ctx)
{
	struct dfly_module_poller_instance_s *m_inst = (struct dfly_module_poller_instance_s *)ctx;

	int nprocessed = 0;
	int num_msgs, i;
	int ret;

	void *reqs[REQ_PER_POLL] = {NULL};

	if(m_inst->status != 1) {
		return 0;
	}

	//deque from ring
#if defined DFLY_MODULE_MSG_MP_SC
	num_msgs = spdk_ring_dequeue(m_inst->pipe.msg_ring, reqs, REQ_PER_POLL);
#endif
	dfly_ustat_update_module_inst_stat(m_inst, 1, num_msgs);
	//call registered function for each request
	for (i = 0; i < num_msgs; i++) {
		struct dfly_request *req = (struct dfly_request *)reqs[i];

		req->tgt_core = spdk_env_get_current_core();//m_inst->mpoller->lcore;

		ret = m_inst->module->ops->module_rpoll(m_inst->ctx, req);
		if (ret == DFLY_MODULE_REQUEST_PROCESSED) {
			dfly_handle_request(req);
		}
	}
	nprocessed += num_msgs;

	if (m_inst->module->ops->module_cpoll) {
		//deque from ring
#if	defined DFLY_MODULE_MSG_MP_SC
		num_msgs = spdk_ring_dequeue(m_inst->pipe.cmpl_ring, reqs, REQ_PER_POLL);
#endif
		//call registered function for each request
		for (i = 0; i < num_msgs; i++) {
			struct dfly_request *req = (struct dfly_request *)reqs[i];

			req->tgt_core = spdk_env_get_current_core();//m_inst->mpoller->lcore;

			ret = m_inst->module->ops->module_cpoll(m_inst->ctx, req);
			if (ret == DFLY_MODULE_REQUEST_PROCESSED) {
				//forward if there are other modules
				//TODO: dfly_complete_request(req);
			}
		}
		nprocessed += num_msgs;
	}

	if (m_inst->module->ops->module_gpoll) {
		m_inst->module->ops->module_gpoll(m_inst->ctx);
	}

	return nprocessed;
}

#if defined DFLY_MODULE_MSG_MP_SC
static inline struct dfly_module_poller_instance_s *dfly_get_module_instance(dfly_module_t *module)
{
	struct dfly_module_poller_instance_s *m_inst = NULL;

	int core = spdk_env_get_current_core();
	int thread_index = 0;

	assert(core < MAX_CPU);
	thread_index = module->thread_index_arr[core];

	module->thread_index_arr[core]++;
	module->thread_index_arr[core] %= module->num_threads;

	m_inst = module->active_thread_arr[thread_index];

	return (m_inst);
}
#endif


void dfly_module_post_request(struct dfly_module_s *module, struct dfly_request *req)
{
	int rc;

#if defined DFLY_MODULE_MSG_MP_SC
	struct dfly_module_poller_instance_s *m_inst;

	if (module->ops->find_instance_context) {
		m_inst = (struct dfly_module_poller_instance_s *)module->ops->find_instance_context(req);
	} else {
		//Default to round robin across module instance
		m_inst = dfly_get_module_instance(module);
	}

	rc = spdk_ring_enqueue(m_inst->pipe.msg_ring, (void **)&req, 1, NULL);
	if (rc != 1) {
		assert(false);
	}
	dfly_ustat_update_module_inst_stat(m_inst, 0, 1);
#endif
}

void dfly_module_complete_request(struct dfly_module_s *module, struct dfly_request *req)
{
	int rc;

#if defined DFLY_MODULE_MSG_MP_SC
	struct dfly_module_poller_instance_s *m_inst;

	if (module->ops->find_instance_context) {
		m_inst = (struct dfly_module_poller_instance_s *)module->ops->find_instance_context(req);
	} else {
		//Default to round robin across module instance
		m_inst = dfly_get_module_instance(module);
	}

	rc = spdk_ring_enqueue(m_inst->pipe.cmpl_ring, (void **)&req, 1, NULL);
	if (rc != 1) {
		assert(false);
	}
#endif

}

void *dfly_module_get_ctx(struct dfly_module_s *module)
{
	return module->ctx;
}

int  dfly_module_get_instance_icore(void *inst)
{
	struct dfly_module_poller_instance_s *m_inst = (struct dfly_module_poller_instance_s *) inst;
	return m_inst->icore;
}

void dfly_module_event_processed(struct df_module_event_complete_s *cb_event, char *mname)
{
	//Callback function after module started
	struct spdk_event *event;
	uint32_t icore = spdk_env_get_current_core();

	if (!cb_event->cb_fn) {
		DFLY_INFOLOG(DFLY_LOG_MODULE, "No callback for module %s\n", mname);
		free(mname);
		free(cb_event);
		return;
	}

	if (icore != cb_event->src_core) {
		event = spdk_event_allocate(cb_event->src_core, cb_event->cb_fn, cb_event->arg1, cb_event->arg2);
		spdk_event_call(event);
	} else {
		cb_event->cb_fn(cb_event->arg1, cb_event->arg2);
	}

	free(mname);
	free(cb_event);
}


void dfly_module_thread_start(void *inst, void *cb_event)
{
	struct dfly_module_poller_instance_s *m_inst = (struct dfly_module_poller_instance_s *)inst;
	dfly_module_t *module = m_inst->module;

	pthread_mutex_lock(&module->module_lock);

	if (module->ops->module_init_instance_context) {
		m_inst->ctx = module->ops->module_init_instance_context(m_inst->ctx, m_inst, module->num_threads);
	}

	DFLY_ASSERT(m_inst->icore  == spdk_env_get_current_core());

	DFLY_INFOLOG(DFLY_LOG_MODULE, "Launching module thread on core %u\n", m_inst->icore);
#if defined DFLY_MODULE_MSG_MP_SC
	//Initialize spdk ring
	m_inst->pipe.msg_ring = spdk_ring_create(SPDK_RING_TYPE_MP_SC, 65536, SPDK_ENV_SOCKET_ID_ANY);
	assert(m_inst->pipe.msg_ring);

	m_inst->pipe.cmpl_ring = spdk_ring_create(SPDK_RING_TYPE_MP_SC, 65536, SPDK_ENV_SOCKET_ID_ANY);
	assert(m_inst->pipe.cmpl_ring);
#endif

#if defined DFLY_MODULE_MSG_MP_SC
	module->active_thread_arr[module->num_threads] = m_inst;
	module->num_threads++;
#endif

	m_inst->status = 1;
	m_inst->mpoller = spdk_poller_register(module_poller, m_inst, 0);
	assert(m_inst->mpoller);

	m_inst = TAILQ_NEXT(m_inst, link);/* m_inst modified*/
	if (m_inst != NULL) {
		struct spdk_event *event;
		event = spdk_event_allocate(m_inst->icore, dfly_module_thread_start, m_inst, cb_event);
		spdk_event_call(event);
	} else {
		dfly_module_event_processed((struct df_module_event_complete_s *)cb_event, strdup(module->name));
	}

	pthread_mutex_unlock(&module->module_lock);

	return;

}

//connect to module instance

struct dfly_module_s *dfly_module_start(const char *name, int id,
					struct dfly_module_ops *mops, void *ctx, int num_cores,
					df_module_event_complete_cb cb, void *cb_arg)
{

	dfly_module_t *module;
	struct dfly_module_poller_instance_s *m_inst;
	int i;

	int launch_core = -1;

	char stat_name[64];
	memset(stat_name, 0, 64);
	sprintf(stat_name, "%s_%d", name, id);
	DFLY_ASSERT(strlen(stat_name) < 64);

	module = (dfly_module_t *)calloc(1, sizeof(dfly_module_t));
	if (!module) {
		return NULL;
	}

	strncpy(module->name, stat_name, MAX_MODULE_NAME_LEN - 1);

	module->ops = mops;
	assert(module->ops->module_rpoll);

	TAILQ_INIT(&module->active_threads);
	module->ctx = ctx;

	m_inst = (struct dfly_module_poller_instance_s *)calloc(1,
			sizeof(struct dfly_module_poller_instance_s) * num_cores);
	if (!m_inst) {
		free(module);
		return NULL;
	}

	pthread_mutex_init(&module->module_lock, NULL);


#if 0
	launch_core = spdk_env_get_current_core();
#endif
	for (i = 0; i < num_cores; i++) {
		m_inst[i].ctx = ctx;
		m_inst[i].module = module;
#if 0
		launch_core = spdk_env_get_next_core(launch_core);
		if (launch_core == UINT32_MAX) {
			launch_core = spdk_env_get_first_core();
		}
#else
		launch_core = dfly_get_next_core(module->name, num_cores, NULL, NULL);
		m_inst[i].icore = launch_core;
		DFLY_ASSERT(launch_core != -1);
#endif
		dfly_ustat_init_module_inst_stat(&m_inst[i], stat_name, i);

		TAILQ_INSERT_TAIL(&module->active_threads, &m_inst[i], link);
	}

	struct df_module_event_complete_s *cb_event;

	cb_event = (struct df_module_event_complete_s *) calloc(1,
			sizeof(struct df_module_event_complete_s));
	if (!cb_event) {
		free(m_inst);
		free(module);
		return NULL;
	}

	cb_event->src_core = spdk_env_get_current_core();
	cb_event->cb_fn = cb;
	cb_event->arg1 = cb_arg;

	//TODO: Return pending
	//Last init instance will give module
	struct spdk_event *event;

	//TODO: clean code: use TAILQ_FIRST instead of index
	event = spdk_event_allocate(m_inst[0].icore, dfly_module_thread_start, &m_inst[0], cb_event);
	spdk_event_call(event);

	return module;
}

void dfly_module_thread_stop(void *ctx, void *cb_event)
{
	struct dfly_module_poller_instance_s *m_inst = (struct dfly_module_poller_instance_s *)ctx;
	struct dfly_module_s *module = m_inst->module;
	struct dfly_module_poller_instance_s *m_inst_next = NULL;

	if (m_inst->module->ops->module_instance_destroy) {
		m_inst->module->ops->module_instance_destroy(m_inst->module->ctx, m_inst->ctx);
	}
#if defined DFLY_MODULE_MSG_MP_SC
	if (m_inst->pipe.msg_ring != NULL) {
		spdk_ring_free(m_inst->pipe.msg_ring);
		m_inst->pipe.msg_ring = NULL;
	}
	if (m_inst->pipe.cmpl_ring != NULL) {
		spdk_ring_free(m_inst->pipe.msg_ring);
		m_inst->pipe.cmpl_ring = NULL;
	}
#endif
	m_inst->status = 0;
	spdk_poller_unregister(&m_inst->mpoller);

	dfly_ustat_remove_module_inst_stat(m_inst);


	m_inst_next = TAILQ_NEXT(m_inst, link);
	memset(m_inst, 0, sizeof(struct dfly_module_poller_instance_s));

	if (m_inst_next != NULL) {
		struct spdk_event *event;
		event = spdk_event_allocate(m_inst_next->icore, dfly_module_thread_stop, m_inst_next, cb_event);
		spdk_event_call(event);
	} else {
		dfly_module_event_processed((struct df_module_event_complete_s *)cb_event, strdup(module->name));
		m_inst_next = TAILQ_FIRST(&module->active_threads);
		free(m_inst_next);
		memset(module, 0, sizeof(dfly_module_t));
		free(module);
	}

}

void dfly_module_stop(struct dfly_module_s *module, df_module_event_complete_cb cb, void *cb_arg, void *cb_private)
{
	struct dfly_module_poller_instance_s *m_inst;
	struct spdk_event *event;

	pthread_mutex_lock(&module->module_lock);

	m_inst = TAILQ_FIRST(&module->active_threads);

	struct df_module_event_complete_s *cb_event;

	cb_event = (struct df_module_event_complete_s *) calloc(1,
			sizeof(struct df_module_event_complete_s));
	if (!cb_event) {
		DFLY_ASSERT(0);
		return;
	}

	cb_event->src_core = spdk_env_get_current_core();
	cb_event->cb_fn = cb;
	cb_event->arg1 = cb_arg;
	cb_event->arg2 = cb_private;

	event = spdk_event_allocate(m_inst->icore, dfly_module_thread_stop, m_inst, cb_event);
	spdk_event_call(event);

	pthread_mutex_unlock(&module->module_lock);

}
