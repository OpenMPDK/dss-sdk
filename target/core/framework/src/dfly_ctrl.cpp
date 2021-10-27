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


#include <netinet/in.h>

#include "spdk/nvmf.h"
#include "nvmf_internal.h"
//#include "nvmf_tgt.h"

#include "p2.h"

#include "atomic.h"
#include "dragonfly.h"
#include "df_ctrl.h"

/*
 * In general, this code makes assumption that client will create/destroy qpair in serial.
 * Multiple clients can connect target at the same time and single client can create
 * multiple controllers at the same time. Those cases are handled by this code but the same
 * cannot be said to spdk as of today. If client destroys qpair or disconnects while
 * creation is still being handled by target, then we are in trouble.
 * */

//extern char *
//spdk_nvmf_rdma_qpair_peer_addr(struct spdk_nvmf_qpair *qp, char *addr, size_t len);

#define BMAP_SIZE  4
static uint64_t bmap[BMAP_SIZE] = {0};

const stat_ses_t stat_ses_table = {
	{ "nqn", USTAT_TYPE_STRING, SPDK_NVMF_NQN_MAX_LEN, NULL },
	{ "ip", USTAT_TYPE_STRING, INET6_ADDRSTRLEN, NULL },
};

const stat_kvio_t stat_ses_io_table = {
	{ "puts", USTAT_TYPE_UINT64, 0, NULL },
	{ "gets", USTAT_TYPE_UINT64, 0, NULL },
	{ "dels", USTAT_TYPE_UINT64, 0, NULL },
	{ "exists", USTAT_TYPE_UINT64, 0, NULL },
	{ "iters", USTAT_TYPE_UINT64, 0, NULL },
	{ "putBandwidth", USTAT_TYPE_UINT64, 0, NULL },
	{ "getBandwidth", USTAT_TYPE_UINT64, 0, NULL },
	{ "put_less_4KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "put_4KB_16KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "put_16KB_64KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "put_64KB_256KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "put_256KB_1MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "put_1MB_2MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "put_large_2MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_less_4KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_4KB_16KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_16KB_64KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_64KB_256KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_256KB_1MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_1MB_2MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_large_2MB", USTAT_TYPE_UINT64, 0, NULL},
};

static size_t
dfly_ses_id_alloc()
{
	size_t i;
	uint64_t *block, mask;
	static const size_t bsize = sizeof(bmap[0]) * BMAP_SIZE * 8;

	for (i = 0; i < bsize; i++) {
		block = &bmap[i >> 6];
		mask = 1 << P2PHASE(i, 64);
		if (!(*block & mask)) {
			*block |= mask;
			return (i);
		}
	}

	DFLY_ERRLOG("Only support max %u clients now\n", bsize);
	assert(!i);

	/* Never reach here */
	return (-1);
}

static void
dfly_ses_id_free(size_t id)
{
	uint64_t *block, mask;
	static const size_t bsize = sizeof(bmap[0]) * BMAP_SIZE * 8;

	if (id >= bsize) {
		DFLY_ERRLOG("session id too big %u >= %u\n", id, bsize);
		return;
	}

	block = &bmap[id >> 6];
	mask = 1 << P2PHASE(id, 64);
	if (*block & mask) {
		*block &= ~mask;
		return;
	}

	DFLY_ERRLOG("session id %u is NOT allocated\n", id);

	return;
}

static int
dfly_ses_id_compare(dfly_ses_id_t *a, dfly_ses_id_t *b)
{
	int ret = 0;

	assert(a);
	assert(b);

	ret |= (a->dfsi_name && b->dfsi_name) ? strcmp(a->dfsi_name, b->dfsi_name) : 0;
	ret |= (a->dfsi_addr && b->dfsi_addr) ? strcmp(a->dfsi_addr, b->dfsi_addr) : 0;

	return (ret);
}

static dfly_prof_t *
dfly_prof_lookup(const char *h)
{
	dfly_prof_t *p, *dp = NULL;

	/* TODO: to be updated with real host identifier */
	TAILQ_FOREACH(p, &g_dragonfly->df_profs, dfp_link) {
		if (!strncmp(p->dfp_nqn, h, strlen(p->dfp_nqn)))
			return (p);
		else if (!strncmp(p->dfp_nqn, DFLY_PROF_DEF_NAME,
				  strlen(p->dfp_nqn)))
			dp = p;
	}

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Using \"%s\" profile for %s\n",
		      DFLY_PROF_DEF_NAME, h);

	return (dp);
}

static void
dfly_ses_remove(dfly_session_t *s)
{
	pthread_mutex_lock(&g_dragonfly->df_ses_lock);
	g_dragonfly->df_sessionc--;
	TAILQ_REMOVE(&g_dragonfly->df_sessions, s, dfs_link);
	pthread_mutex_unlock(&g_dragonfly->df_ses_lock);

	//dfly_ustat_delete(s->dfs_stats_ses);
	//dfly_ustat_delete(s->dfs_stats_io);

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Free session id %u\n", s->dfs_id.dfsi_num);

	dfly_ses_id_free(s->dfs_id.dfsi_num);
	free(s->dfs_id.dfsi_name);
	if (s->dfs_id.dfsi_addr)
		free(s->dfs_id.dfsi_addr);

	free(s);
}

static void
dfly_ctrl_destroy(void *base)
{

	dfly_ctrl_t *ctrl = CONTAINEROF(base, dfly_ctrl_t, ct_base);
	dfly_session_t *s = ctrl->ct_session;
	assert(s);

	free(ctrl);

	if (!atomic_dec_32_nv(&s->dfs_ctrlc))
		dfly_ses_remove(s);

}

static size_t
dfly_ses_get_id_num(dfly_session_t *s)
{
	assert(s);
	return (s->dfs_id.dfsi_num);
}

static void
dfly_ses_insert_ctrl(dfly_session_t *s, dfly_ctrl_t *c)
{
	dfly_ctrl_t *ctrl;

	c->ct_session = s;

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "ctrl %p inserted into sesssion %p\n", c, s);

	return;
}

static dfly_session_t *
dfly_ses_lookup(dfly_ses_id_t *sid)
{
	dfly_session_t *ses;

	pthread_mutex_lock(&g_dragonfly->df_ses_lock);

	TAILQ_FOREACH(ses, &g_dragonfly->df_sessions, dfs_link) {
		if (!dfly_ses_id_compare(&ses->dfs_id, sid)) {
			pthread_mutex_unlock(&g_dragonfly->df_ses_lock);
			return (ses);
		}
	}

	pthread_mutex_unlock(&g_dragonfly->df_ses_lock);

	return (NULL);
}

static dfly_session_t *
dfly_ses_try_lookup(dfly_ses_id_t *sid)
{
	dfly_session_t *s, *ses;

	char *buf = NULL;
	stat_kvio_t *st_io;
	stat_ses_t *st_ses;

	s = (dfly_session_t *)calloc(1, sizeof(*s));
	if (!s)
		goto ERROR;

	pthread_mutex_lock(&g_dragonfly->df_ses_lock);

	/* TODO: Only match "yyyy-mm.org.nvmexpress" type prefix.
	   Hopefully we could find a better identifier for host */
	TAILQ_FOREACH(ses, &g_dragonfly->df_sessions, dfs_link) {
		if (!dfly_ses_id_compare(&ses->dfs_id, sid)) {
			atomic_inc_32(&ses->dfs_ctrlc);
			pthread_mutex_unlock(&g_dragonfly->df_ses_lock);
			DFLY_DEBUGLOG(DFLY_LOG_QOS, "Found existing ses %p for host %s\n", ses, sid->dfsi_name);
			free(s);
			return (ses);
		}
	}

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Create new ses %p for host %s at addr %s\n",
		      s, sid->dfsi_name, sid->dfsi_addr);

	s->dfs_id.dfsi_name = strdup(sid->dfsi_name);
	s->dfs_id.dfsi_addr = sid->dfsi_addr ? strdup(sid->dfsi_addr) : NULL;
	s->dfs_id.dfsi_num  = dfly_ses_id_alloc();
	s->dfs_host_prof = dfly_prof_lookup(sid->dfsi_name);
	s->dfs_ctrlc = 1;
	g_dragonfly->df_sessionc++;
	TAILQ_INSERT_TAIL(&g_dragonfly->df_sessions, s, dfs_link);
	assert(g_dragonfly->df_sessionc != 0);

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Alloc session id %u\n", s->dfs_id.dfsi_num);

	buf = alloca(STAT_ENAME_LEN);
	(void) dfly_ustats_get_ename(STAT_ENAME_SES, s->dfs_id.dfsi_num,
				     buf, STAT_ENAME_LEN);
	/*
		st_ses = ustat_insert(dfly_ustats_get_handle(), buf, STAT_GNAME_NAME,
				      &ustat_class_test,
				      sizeof (stat_ses_table)/ sizeof (ustat_named_t),
				      &stat_ses_table, NULL);
		st_io = ustat_insert(dfly_ustats_get_handle(), buf, STAT_GNAME_KVIO,
				     &ustat_class_test,
				     sizeof (stat_ses_io_table)/ sizeof (ustat_named_t),
				     &stat_ses_io_table, NULL);
	assert(st_io);
	assert(st_ses);

	dfly_ustat_set_string(st_ses, &st_ses->name, sid->dfsi_name);
	if (sid->dfsi_addr) {
		dfly_ustat_set_string(st_ses, &st_ses->addr, sid->dfsi_addr);
	}
	s->dfs_stats_io  = st_io;
	s->dfs_stats_ses = st_ses;
	*/

	pthread_mutex_unlock(&g_dragonfly->df_ses_lock);

	return (s);

ERROR:
	DFLY_ERRLOG("Failed to alloc session for %s\n", sid->dfsi_name);
	return (NULL);
}

static int
dfly_ses_add_ctrl(dfly_ctrl_t *c, dfly_ses_id_t *sid)
{
	char *n;
	dfly_session_t  *s;

	s = dfly_ses_try_lookup(sid);
	if (!s) {
		DFLY_ERRLOG("Failed to add ctrl %p to session\n", c);
		return (-1);
	}

	(void) dfly_ses_insert_ctrl(s, c);

	return (0);
}

static void *
dfly_ctrl_create(char *name, struct spdk_nvmf_qpair *qp, size_t bsize)
{
	dfly_ctrl_t *ctrl;
	dfly_ses_id_t sid = {0};

	assert(name);

	ctrl = (dfly_ctrl_t *)calloc(1, sizeof(*ctrl) + bsize);
	assert(ctrl);

	sid.dfsi_name = name;
	if (qp) {
		//TODO: Fix get peer address
		//sid.dfsi_addr = alloca(INET6_ADDRSTRLEN);
		//(void) spdk_nvmf_rdma_qpair_peer_addr(qp,
		//				      sid.dfsi_addr,
		//				      INET6_ADDRSTRLEN);
	} else {
		sid.dfsi_addr = NULL;
	}

	(void) dfly_ses_add_ctrl(ctrl, &sid);

	return (&ctrl->ct_base);
}

void df_destroy_ctrl(uint32_t ssid, uint16_t cntlid)
{
	struct dfly_subsystem *df_ss = dfly_get_subsystem_no_lock(ssid);

	dfly_ctrl_t *ctrl, *tmp;
	dfly_session_t *s;

	pthread_mutex_lock(&df_ss->ctrl_lock);//Lock Begin

	TAILQ_FOREACH_SAFE(ctrl, &df_ss->df_ctrlrs, ct_link, tmp) {
		DFLY_WARNLOG("Found controller cntlid %d\n", ctrl->ct_cntlid);
		if(ctrl->ct_cntlid == cntlid) {
			TAILQ_REMOVE(&df_ss->df_ctrlrs, ctrl, ct_link);
			break;
		}
	}
	pthread_mutex_unlock(&df_ss->ctrl_lock);//Release Lock

	if(!ctrl) {
		DFLY_WARNLOG("Controller not found for ssid%d and cntlid %d\n", ssid, cntlid);
		return;
	}

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Removing df ctrlr %p\n", ctrl);

	s = ctrl->ct_session;
	assert(s);

	free(ctrl);

	if (!atomic_dec_32_nv(&s->dfs_ctrlc))
		dfly_ses_remove(s);

}

dfly_ctrl_t *df_get_ctrl(uint32_t ssid, uint16_t cntlid)
{
	struct dfly_subsystem *df_ss = dfly_get_subsystem_no_lock(ssid);

	dfly_ctrl_t *ctrl, *tmp;

	ctrl = NULL;
	pthread_mutex_lock(&df_ss->ctrl_lock);//Lock Begin

	TAILQ_FOREACH_SAFE(ctrl, &df_ss->df_ctrlrs, ct_link, tmp) {
		if(ctrl->ct_cntlid == cntlid) {
			break;
		}
	}
	pthread_mutex_unlock(&df_ss->ctrl_lock);//Release Lock

	return ctrl;
}

dfly_ctrl_t *df_init_ctrl(struct dfly_qpair_s *dqpair, uint16_t cntlid, uint32_t ssid)
{
	struct dfly_subsystem *df_ss = dfly_get_subsystem_no_lock(ssid);
	dfly_ctrl_t *ctrl;
	dfly_ses_id_t sid = {0};

	pthread_mutex_lock(&df_ss->ctrl_lock);//Lock Begin

	TAILQ_FOREACH(ctrl, &df_ss->df_ctrlrs, ct_link) {
		if(ctrl->ct_cntlid == cntlid) {
			pthread_mutex_unlock(&df_ss->ctrl_lock);//Release Lock
			DFLY_DEBUGLOG(DFLY_LOG_QOS, "Found df ctrlr %p\n", ctrl);
			return ctrl;
		}
	}

	ctrl = (dfly_ctrl_t *)calloc(1, sizeof(dfly_ctrl_t));
	DFLY_ASSERT(ctrl);

	ctrl->ct_cntlid = cntlid;
	TAILQ_INIT(&ctrl->df_qpairs);
	pthread_mutex_init(&ctrl->ct_lock, NULL);

	DFLY_ASSERT(dqpair->parent_qpair->ctrlr);

	sid.dfsi_name = dqpair->parent_qpair->ctrlr->hostnqn;
	DFLY_ASSERT(sid.dfsi_name);

	dfly_ses_add_ctrl(ctrl, &sid);

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Inserted df ctrlr %p\n", ctrl);
	TAILQ_INSERT_TAIL(&df_ss->df_ctrlrs, ctrl, ct_link);
	pthread_mutex_unlock(&df_ss->ctrl_lock);//Release Lock

	DFLY_DEBUGLOG(DFLY_LOG_QOS, "Created controller for hostnqn: %s\n", sid.dfsi_name);
	return ctrl;
}

int
dfly_ses_qpair_get_num(struct spdk_nvmf_qpair *qp)
{
	dfly_ses_id_t sid = {0};
	struct spdk_nvmf_ctrlr *c;
	dfly_ctrl_t *ctrl;
	dfly_session_t *s;

	/* Should have a ctrl by now */
	//TODO: Get spdk ctrlr
	//c = spdk_nvmf_qpair_get_ctrlr(qp);
	assert(c);

	ctrl = CONTAINEROF(c, dfly_ctrl_t, ct_base);
	s = ctrl->ct_session;
	assert(s);

	return (s->dfs_id.dfsi_num);
}

static void
dfly_ctrl_def_destroy(void *base)
{
	free(base);
}

static void *
dfly_ctrl_def_create(char *n, struct spdk_nvmf_qpair *qp, size_t bsize)
{
	return (calloc(1, bsize));
}

typedef struct spdk_nvmf_ctrlr_ops {
	void *(*create)(char *name, struct spdk_nvmf_qpair *qp, size_t bsize);
	void (*destroy)(void *base);
} spdk_nvmf_ctrlr_ops_t;

const spdk_nvmf_ctrlr_ops_t ctrl_default_ops = {
	.create    = dfly_ctrl_def_create,
	.destroy   = dfly_ctrl_def_destroy,
};

const spdk_nvmf_ctrlr_ops_t ctrl_std_ops = {
	.create   = dfly_ctrl_create,
	.destroy = dfly_ctrl_destroy,
};

dfly_qos_client_ops_t qos_client_ops = {
	.create  = dfly_ctrl_create,
	.destroy = dfly_ctrl_destroy,
};

int
dfly_ses_stats_record(dfly_session_t *s, dfly_request_t *r)
{
	stat_kvio_t *st = s->dfs_stats_io;

	switch (dfly_req_get_command(r)) {
	case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
		dfly_ustat_atomic_inc_u64(st, &st->puts);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
		dfly_ustat_atomic_inc_u64(st, &st->gets);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
		dfly_ustat_atomic_inc_u64(st, &st->dels);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_EXIST:
		dfly_ustat_atomic_inc_u64(st, &st->exists);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL:
	case SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ:
		dfly_ustat_atomic_inc_u64(st, &st->iters);
		break;
	default:
		break;
	}
}
