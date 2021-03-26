/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <dragonfly.h>
#include "nvmf_internal.h"

uint32_t df_qpair_susbsys_enabled(struct spdk_nvmf_qpair *nvmf_qpair, struct spdk_nvmf_request *req)
{
	if(nvmf_qpair_is_admin_queue(nvmf_qpair)) {
		return 0;//Not enabled
	}

	if(req && (!req->dreq || !req->dreq->dqpair)) {
			return 0;//Not Enabled
	}

	if(!nvmf_qpair->ctrlr) {
			return 0;//Not Enabled
	}

	if(nvmf_qpair->state != SPDK_NVMF_QPAIR_ACTIVE) {
		return 0;//qpair not active
	}

	return df_subsystem_enabled(nvmf_qpair->ctrlr->subsys->id);
}

/*
 * Assumptions:
 *     - req_arr is a contiguous array of size transport_request struct
 *     * req_arr is valid for max_reqs of above size
 *     * First element of transport_request struct is spdk_nvmf_request
 *
 */

int dfly_qpair_init(struct spdk_nvmf_qpair *nvmf_qpair)
{

	struct dfly_qpair_s *dqpair = nvmf_qpair->dqpair;

	int rc;
	struct spdk_nvme_transport_id trid;

	if (!dqpair) {
		dqpair = (struct dfly_qpair_s *) calloc(1, sizeof(struct dfly_qpair_s));
		if (!dqpair) {
			return -1;
		}

		pthread_mutex_init(&dqpair->qp_lock, NULL);
		dqpair->parent_qpair = nvmf_qpair;
		dqpair->io_counter = 0;
		dqpair->curr_qd = 0;
		dqpair->qid = -1;
		dqpair->dss_enabled = false;
		dqpair->df_poller = dfly_poller_init(0);

		nvmf_qpair->dqpair = dqpair;

		rc = spdk_nvmf_qpair_get_local_trid(nvmf_qpair, &trid);
		if (rc) {
			SPDK_ERRLOG("Invalid host transport Id.\n");
			assert(0);
		} else {
			strncpy(nvmf_qpair->dqpair->listen_addr, trid.traddr, INET6_ADDRSTRLEN);
		}

		rc = spdk_nvmf_qpair_get_peer_trid(nvmf_qpair, &trid);
		if (rc) {
			SPDK_ERRLOG("Invalid host transport Id.\n");
			assert(0);
		} else {
			strncpy(nvmf_qpair->dqpair->peer_addr, trid.traddr, INET6_ADDRSTRLEN);
		}

		dqpair->lat_ctx = dss_lat_new_ctx(nvmf_qpair->dqpair->peer_addr);

		DFLY_DEBUGLOG(DFLY_LOG_QOS, "dqpair %p initialized\n", dqpair);
	} else {
		DFLY_ASSERT(0);
		//DFLY_DEBUGLOG(DLFY_LOG_CORE, "Allocating extra %d request for qpair %p\n", max_reqs, nvmf_qpair);
		//dfly_req = (struct dfly_request *) calloc(1, max_reqs * sizeof(struct dfly_request));
	}

	return 0;
}

int dfly_qpair_init_reqs(struct spdk_nvmf_qpair *nvmf_qpair, char *req_arr, int req_size, int max_reqs)
{

	struct dfly_request *dfly_req = NULL;
	struct spdk_nvmf_request *nvmf_req = NULL;
	int i;

	dfly_req = (struct dfly_request *) calloc(1, (max_reqs * sizeof(struct dfly_request)));
	if (!dfly_req) {
		return -1;
	}

	nvmf_qpair->dqpair->nreqs = max_reqs;
	nvmf_qpair->dqpair->reqs = (struct dfly_request *)(dfly_req);

	for (i = 0; i < max_reqs; i++) {
		nvmf_req = (struct spdk_nvmf_request *)(req_arr + (i * req_size));
		nvmf_req->dreq = dfly_req + i;
	}

	return 0;
}

int dfly_qpair_destroy(struct dfly_qpair_s *dqpair)
{
	struct dss_lat_prof_arr *tmp = NULL;

	pthread_mutex_lock(&dqpair->qp_lock);

	dfly_ustat_remove_qpair_stat(dqpair);
	if(g_dragonfly->enable_latency_profiling && dqpair->parent_qpair->qid != 0) {
		dss_lat_get_percentile(dqpair->lat_ctx, &tmp);
		DFLY_DEBUGLOG(DLFY_LOG_CORE, "nReqs:%lu ,num Percentiles:%d [", nsamples, (tmp)->n_part);
		int i;
		for(i=0;i < tmp->n_part;i++) {
			DFLY_DEBUGLOG(DLFY_LOG_CORE, "%d:%lu%s", tmp->prof[i].pVal,  tmp->prof[i].pLat, (i < tmp->n_part -1)?"|":"");
		}
		DFLY_DEBUGLOG(DLFY_LOG_CORE, "]\n");
	}
	dqpair->qid = -1;

	if(dqpair->lat_ctx) {
		dss_lat_del_ctx(dqpair->lat_ctx);
		dqpair->lat_ctx = NULL;
	}

	if(dqpair->df_poller) {
		dfly_poller_fini(dqpair->df_poller);
		dqpair->df_poller = NULL;
	}
	if(dqpair->reqs) {
		free(dqpair->reqs);
		dqpair->reqs = NULL;
	}
	pthread_mutex_unlock(&dqpair->qp_lock);
	free(dqpair);
	return 0;
}

struct dfly_qpair_s* df_get_dqpair(dfly_ctrl_t *ctrlr, uint16_t qid)
{
	struct dfly_qpair_s *dqpair;

	if(qid == -1) {
		return NULL;
	}

	TAILQ_FOREACH(dqpair, &ctrlr->df_qpairs, qp_link) {
		if(dqpair->qid == qid) {
			return dqpair;
		}
	}

	return NULL;
}
