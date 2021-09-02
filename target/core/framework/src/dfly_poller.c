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


#include "spdk/nvmf.h"

#include "atomic.h"
#include "df_poller.h"
#include "dragonfly.h"

#define DFLY_RTAG(req)    (req)->qos_tags[DFLY_QOS_RESV]
#define DFLY_LTAG(req)    (req)->qos_tags[DFLY_QOS_LIM]
#define DFLY_PTAG(req)    (req)->qos_tags[DFLY_QOS_PROP]

typedef struct spdk_nvmf_rdma_poller dss_rdma_poler;

typedef struct dfly_poller {
	TAILQ_HEAD(qos_q, dfly_request)    	po_qos_q[DFLY_QOS_ATTRS];
	uint32_t                            	po_qos_qc;
	void					*po_base;
} dfly_poller_t;

static inline void
dfly_poller_insert_queue(dfly_request_t *r, dfly_poller_t *poller,
			 dfly_qos_attr attr)
{
	dfly_request_t *req;
	struct qos_q  *q = &poller->po_qos_q[attr];
	/*Uncomment until a better logging is avail */
#if 0
	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Qos enqueue req %jx\n", (uint64_t)req);
#endif
	if (TAILQ_FIRST(q)) {
		TAILQ_FOREACH_REVERSE(req, q, qos_q, qos_links[attr]) {
			uint64_t tag = req->qos_tags[attr];
			if (tag <= r->qos_tags[attr]) {
				TAILQ_INSERT_AFTER(q, req, r, qos_links[attr]);
				break;
			}
		}
	} else {
		TAILQ_INSERT_HEAD(q, r, qos_links[attr]);
	}
}

static inline uint64_t
dfly_poller_get_tag(dfly_session_t *ses, dfly_qos_attr attr, uint64_t nops)
{
	uint64_t     otag, iotag, ntag, *t;
	uint64_t     tick  = spdk_get_ticks();
	dfly_prof_t  *prof = ses->dfs_host_prof;

	t = &ses->dfs_curr_tags[attr];

	do {
		otag = *t;
		iotag = MAX(tick, otag + prof->dfp_credits[attr]);
		ntag = (nops == 1)?iotag: iotag + ((nops - 1) * prof->dfp_credits[attr]);
	} while (atomic_cas_64(t, otag, ntag) != otag);

	return (iotag);
}

static inline void
dfly_poller_update_tag_nops(dfly_session_t *ses, dfly_qos_attr attr, uint64_t nops)
{
	uint64_t     otag, ntag, *t;
	dfly_prof_t  *prof = ses->dfs_host_prof;

	t = &ses->dfs_curr_tags[attr];

	do {
		otag = *t;
		ntag = otag + ((nops - 1) * prof->dfp_credits[attr]);
	} while (atomic_cas_64(t, otag, ntag) != otag);

	return;
}

void dfly_qos_update_nops(dfly_request_t *req,
			void *tr_p,
			dfly_ctrl_t *ctrl)
{
	dfly_poller_t *poller;

	uint64_t nops = 1;

	assert(tr_p);
	if (!ctrl || !g_dragonfly->df_qos_enable)
		return;

	poller = CONTAINEROF(tr_p, dfly_poller_t, po_base);

	if (!ctrl->ct_session || !ctrl->ct_session->dfs_host_prof) {
		DFLY_ERRLOG("ctrl %p doesn't belong to a dfly_session "
			    "or doesn't have a profile\n", ctrl);
		return;
	}

	dfly_session_t *ses  = ctrl->ct_session;
	dfly_qos_attr   attr;

	uint32_t cmd_opc = dfly_req_get_command(req);
	if(cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE) {
		nops = dfly_resp_get_cdw0(req) / 4096;// Normalize to 4K IOPS
		DFLY_ASSERT(nops);

		for (attr = DFLY_QOS_RESV; attr < DFLY_QOS_ATTRS; attr++)
			dfly_poller_update_tag_nops(ses, attr, nops);
	}
	return;
}

int
dfly_poller_qos_recv(dfly_request_t *req,
		     void *tr_p,
		     dfly_ctrl_t *ctrl)
{
	uint64_t ot, nt, *p;
	uint64_t tick  = spdk_get_ticks();
	uint64_t nops = 1;

	dfly_poller_t *poller;

	if (!ctrl || !g_dragonfly->df_qos_enable)
		return (0);

	assert(tr_p);
	poller = CONTAINEROF(tr_p, dfly_poller_t, po_base);

	if (!ctrl->ct_session || !ctrl->ct_session->dfs_host_prof) {
		DFLY_ERRLOG("ctrl %p doesn't belong to a dfly_session "
			    "or doesn't have a profile\n", ctrl);
		return (0);
	}

	dfly_session_t *ses  = ctrl->ct_session;
	dfly_qos_attr   attr;

	//Update nops according to IO value size
	uint32_t cmd_opc = dfly_req_get_command(req);
	if(cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE) {
		nops = req->req_value.length / 4096;//Normalize to 4K IOPS
	}

	for (attr = DFLY_QOS_RESV; attr < DFLY_QOS_ATTRS; attr++)
		req->qos_tags[attr] = dfly_poller_get_tag(ses, attr, nops);

	dfly_poller_insert_queue(req, poller, DFLY_QOS_RESV);
	dfly_poller_insert_queue(req, poller, DFLY_QOS_PROP);
	poller->po_qos_qc++;

#if 0
	dfly_ses_stats_record(ses, req);
	uint32_t    core  = spdk_env_get_current_core();
	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Process new req %p on core %u tick %jx "
		      "resv t %jx%c lim t %jx%c prop t %jx%c\n",
		      req, core, tick,
		      DFLY_RTAG(req), (DFLY_RTAG(req) > tick) ? '>' : '<',
		      DFLY_LTAG(req), (DFLY_LTAG(req) > tick) ? '>' : '<',
		      DFLY_PTAG(req), (DFLY_PTAG(req) > tick) ? '>' : '<');
#endif
	return (1); // bailout state machine
}

size_t
dfly_poller_qos_sched(void *poller,
		      void **shuttle,
		      size_t max_seats)
{
	dfly_request_t *req, *req_tmp;
	dfly_poller_t  *p;
	size_t seat = 0;

	assert(shuttle);

	p = CONTAINEROF(poller, dfly_poller_t, po_base);

	if (!g_dragonfly->df_qos_enable || !p->po_qos_qc)
		return (0);

	uint64_t tick = spdk_get_ticks();
	struct qos_q *resv_q = &p->po_qos_q[DFLY_QOS_RESV];
	struct qos_q *prop_q = &p->po_qos_q[DFLY_QOS_PROP];


	TAILQ_FOREACH_SAFE(req, resv_q, qos_links[DFLY_QOS_RESV], req_tmp) {
		if ((DFLY_RTAG(req) <= tick) && (seat < max_seats)) {
			TAILQ_REMOVE(resv_q, req, qos_links[DFLY_QOS_RESV]);
			TAILQ_REMOVE(prop_q, req, qos_links[DFLY_QOS_PROP]);
			p->po_qos_qc--;
			shuttle[seat++] = req;
		} else {
			break;
		}
	}
#if 0
	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Resv flush %u reqs on core %u qc %u\n", seat,
		      spdk_env_get_current_core(),
		      p->po_qos_qc);
#endif
	if (seat)
		return (seat);

	tick = spdk_get_ticks();
	TAILQ_FOREACH_SAFE(req, prop_q, qos_links[DFLY_QOS_PROP], req_tmp) {
		if (seat >= max_seats)
			return (seat);
		if (DFLY_LTAG(req) <= tick) {
			TAILQ_REMOVE(resv_q, req, qos_links[DFLY_QOS_RESV]);
			TAILQ_REMOVE(prop_q, req, qos_links[DFLY_QOS_PROP]);
			p->po_qos_qc--;
			shuttle[seat++] = req;
		}
	}
#if 0
	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Lim flush %u reqs on core %u\n", seat, spdk_env_get_current_core());
#endif
	return (seat);
}

void
dfly_poller_fini(void *base)
{
	dfly_poller_t *p = CONTAINEROF(base, dfly_poller_t, po_base);

	/* Sanity check */
	if (p->po_qos_qc)
		DFLY_WARNLOG("dfly poller %p still has %u pending request(s)\n",
			     base, p->po_qos_qc);

	free(p);
}

void *
dfly_poller_init(size_t bsize)
{
	dfly_poller_t *p = (dfly_poller_t *)calloc(1, sizeof(*p) + bsize);
	if (!p)
		return (NULL);

	TAILQ_INIT(&p->po_qos_q[DFLY_QOS_RESV]);
	TAILQ_INIT(&p->po_qos_q[DFLY_QOS_PROP]);

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "dfly poller %p initialized\n", &p->po_base);

	return (&p->po_base);
}

typedef int (*rmda_poller_ops_qos_recv)(struct dfly_request *r,  void *rp, void *c);
typedef size_t (*rmda_poller_ops_qos_sched)(struct spdk_nvmf_rdma_poller *poller, void **shuttle, size_t max_seats);

typedef struct spdk_rdma_poller_ops {
	void *(*init)(size_t bsize);
	void (*fini)(void *base);
	int (*qos_recv)(struct dfly_request *r,
			void *rp,
			void *c);
	size_t (*qos_sched)(struct spdk_nvmf_rdma_poller *poller,
			    void **shuttle,
			    size_t max_seats);
	void (*qos_flush)(void *(*cb)(void *));
} spdk_rdma_poller_ops_t;

struct spdk_rdma_poller_ops poller_qos_ops = {
	.init      = dfly_poller_init,
	.fini      = dfly_poller_fini,
	.qos_recv  = (rmda_poller_ops_qos_recv)dfly_poller_qos_recv,
	.qos_sched = (rmda_poller_ops_qos_sched)dfly_poller_qos_sched,
	.qos_flush = NULL,
};

dfly_qos_request_ops_t qos_request_ops = {
	.init      = dfly_poller_init,
	.fini      = dfly_poller_fini,
	.qos_recv  = (rmda_poller_ops_qos_recv)dfly_poller_qos_recv,
	.qos_sched = dfly_poller_qos_sched,
	.qos_flush = NULL,
};
