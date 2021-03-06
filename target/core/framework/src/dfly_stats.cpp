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


#include <ustat.h>
#include <stdlib.h>

#include "nvmf_internal.h"
#include "spdk/nvmf_spec.h"
#include "spdk/log.h"
#include "df_stats.h"
#include "dragonfly.h"

const ustat_class_t ustat_class_test = {
	.usc_name = "test",
	.usc_ctor = NULL,
	.usc_dtor = NULL,
	.usc_bson = NULL,
};

const stat_counter_types_t stat_counter_types_table {
	{ "instantaneous", USTAT_TYPE_STRING, 260, NULL },
	{ "constant", USTAT_TYPE_STRING, 260, NULL },
};

const rdb_debug_counters_t rdb_debug_counters_table {
	{"rdb_mem_bw", USTAT_TYPE_UINT64, 0, NULL },
};

const stat_serial_t stat_dev_serial_table = {
	{ "c_serial", USTAT_TYPE_STRING, 260, NULL },
};

const stat_subsys_t stat_subsys_nqn_table = {
	{ "c_nqn", USTAT_TYPE_STRING, 260, NULL },
};

const stat_initiator_ip_t stat_initiator_ip_table = {
	{ "c_initiator_ip", USTAT_TYPE_STRING, INET6_ADDRSTRLEN, NULL },
};

const stat_kvio_t stat_dev_io_table = {
	{ "puts", USTAT_TYPE_UINT64, 0, NULL },
	{ "gets", USTAT_TYPE_UINT64, 0, NULL },
	{ "dels", USTAT_TYPE_UINT64, 0, NULL },
	{ "exists", USTAT_TYPE_UINT64, 0, NULL },
	{ "iters", USTAT_TYPE_UINT64, 0, NULL },
	{ "putBandwidth", USTAT_TYPE_UINT64, 0, NULL},
	{ "getBandwidth", USTAT_TYPE_UINT64, 0, NULL},
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
	{ "i_pending_reqs", USTAT_TYPE_UINT64, 0, NULL},
};

const stat_kvio_t stat_subsys_io_table = {
	{ "puts", USTAT_TYPE_UINT64, 0, NULL },
	{ "gets", USTAT_TYPE_UINT64, 0, NULL },
	{ "dels", USTAT_TYPE_UINT64, 0, NULL },
	{ "exists", USTAT_TYPE_UINT64, 0, NULL },
	{ "iters", USTAT_TYPE_UINT64, 0, NULL },
	{ "putBandwidth", USTAT_TYPE_UINT64, 0, NULL},
	{ "getBandwidth", USTAT_TYPE_UINT64, 0, NULL},
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
	{ "i_pending_reqs", USTAT_TYPE_UINT64, 0, NULL},
};

const stat_kvlist_t stat_subsys_list_table = {
       { "listDevBandwidth", USTAT_TYPE_UINT64, 0, NULL},
       { "listMemBandwidth", USTAT_TYPE_UINT64, 0, NULL},
};

const stat_rqpair_t stat_rqpair_io_table = {
	{"i_reqs", USTAT_TYPE_UINT64, 0, NULL},
	{"i_reqs_max", USTAT_TYPE_UINT64, 0, NULL},
	{"c_max_qd", USTAT_TYPE_UINT64, 0, NULL},
	{"puts", USTAT_TYPE_UINT64, 0, NULL },
	{"gets", USTAT_TYPE_UINT64, 0, NULL },
	{"dels", USTAT_TYPE_UINT64, 0, NULL },
};

const stat_module_t stat_module_req_table = {
	{"i_reqs", USTAT_TYPE_UINT64, 0, NULL},
	{"i_reqs_max", USTAT_TYPE_UINT64, 0, NULL},
};

void dfly_ustat_insert_stat_thread_table(ustat_struct_t **s, int id, const stat_module_t *table,
		const char *name);
void dfly_ustat_insert_stat_ses_rqp_table(ustat_struct_t **stat, int id, const stat_rqpair_t *table,
		const char *name);
void dfly_ustat_insert_stat_subsys_table(ustat_struct_t **stat, int id, const stat_subsys_t *table);
void dfly_ustat_insert_stat_rdma_table(ustat_struct_t **stat, int id, const stat_rdma_t *table);
void dfly_ustat_insert_stat_subsys_kvio(void **stat, int id, const stat_kvio_t *table);
void dfly_ustat_insert_stat_subsys_kvlist(void **stat, int id, const stat_kvlist_t *table);

int
dfly_ustats_get_ename(const char *ename, int id, char *buf, size_t len)
{
	size_t c;

	if (!ename || !buf)
		return (-1);

	c = snprintf(buf, len, "%s%d", ename, id);

	return (c);
}

ustat_handle_t *
dfly_ustats_get_handle()
{
	return g_dragonfly->s_handle;
}

int
dfly_ustats_init()
{
	ustat_handle_t *h = ustat_open_proc(USTAT_VERSION, 0, O_RDWR | O_CREAT);
	if (h == NULL) {
		DFLY_ERRLOG("Failed to open ustat handler\n");
		return (-1);
	}

	g_dragonfly->s_handle = h;

	stat_counter_types_t *counter_types = NULL;
	rdb_debug_counters_t *rdb_debug_counters = NULL;

	counter_types = (stat_counter_types_t *)ustat_insert(h, "target", STAT_GNAME_COUNTERS,
			&ustat_class_test,
			sizeof(stat_counter_types_table) / sizeof(ustat_named_t),
			&stat_counter_types_table, NULL);
	if (!counter_types) {
		DFLY_ERRLOG("Failed to initialize counter types ustat entry\n");
		return (-1);
	}

	dfly_ustat_set_string(counter_types, &counter_types->icounters, "[i_pending_reqs, i_reqs, i_reqs_max]");
	dfly_ustat_set_string(counter_types, &counter_types->ccounters, "[c_serial, c_nqn, c_max_qd, c_initiator_ip]");

	if(g_dragonfly->rdb_io_debug_level > 0) {
		rdb_debug_counters = (rdb_debug_counters_t *)ustat_insert(h, "target",
		STAT_GNAME_DEBUG, &ustat_class_test,
		sizeof(rdb_debug_counters_table)/sizeof(ustat_named_t),
		&rdb_debug_counters_table, NULL);

		dfly_ustat_set_u64(rdb_debug_counters,
								&rdb_debug_counters->rdb_mem_bw, 0);

		g_dragonfly->ustat_rdb_debug_counters = rdb_debug_counters;
	}

	return (0);
}

int
dfly_ustat_init_dev_stat(uint32_t subsys_id, const char *dev_name, void *dev)
{

	ustat_handle_t *h = dfly_ustats_get_handle();
	struct dfly_io_device_s *io_dev = (struct dfly_io_device_s *) dev;

	if (!h) {
		DFLY_ERRLOG("Failed to obtain ustat handler\n");
		return (-1);
	}

	stat_serial_t *st_serial = NULL;
	stat_kvio_t *st_io = NULL;

	char *buf = alloca(STAT_ENAME_LEN);
	(void)dfly_ustats_get_ename(STAT_ENAME_SUBSYS, subsys_id, buf, STAT_ENAME_LEN);

	char temp[STAT_ENAME_LEN];
	sprintf(temp, "%s.%s_%d", buf, STAT_NAME_DRIVE, io_dev->ns->nsid);
	st_serial = (stat_serial_t *)ustat_insert(h, temp, STAT_GNAME_NAME,
			&ustat_class_test,
			sizeof(stat_dev_serial_table) / sizeof(ustat_named_t),
			&stat_dev_serial_table, NULL);
	if (!st_serial) {
		DFLY_ERRLOG("Failed to initialize serial ustat entry for namespace\n");
		return (-1);
	}

	st_io = (stat_kvio_t *)ustat_insert(h, temp, STAT_GNAME_KVIO,
					    &ustat_class_test,
					    sizeof(stat_dev_io_table) / sizeof(ustat_named_t),
					    &stat_dev_io_table, NULL);

	if (!st_io) {
		DFLY_ERRLOG("Failed to initialize io ustat entry for namespace\n");
		return (-1);
	}

	dfly_ustat_set_string(st_serial, &st_serial->name, dev_name);
	io_dev->stat_serial = st_serial;
	io_dev->stat_io = st_io;

	return (0);
}

void
dfly_ustat_remove_dev_stat(void *dev)
{
	struct dfly_io_device_s *io_dev = (struct dfly_io_device_s *) dev;
	dfly_ustat_delete(io_dev->stat_serial);
	dfly_ustat_delete(io_dev->stat_io);
	return;
}

int
dfly_ustat_init_subsys_stat(void *subsys, const char *nqn)
{

	struct dfly_subsystem *subsystem = (struct dfly_subsystem *) subsys;

	ustat_handle_t *h = dfly_ustats_get_handle();
	if (!h) {
		DFLY_ERRLOG("Failed to obtain ustat handler when initialize subsys\n");
		return (-1);
	}

	stat_subsys_t *s0;
	stat_kvio_t *s1;
	stat_kvlist_t *s2;

	dfly_ustat_insert_stat_subsys_table((ustat_struct_t **)&s0, subsystem->id, &stat_subsys_nqn_table);
	dfly_ustat_set_string(s0, &s0->name, nqn);

	dfly_ustat_insert_stat_subsys_kvio((ustat_struct_t **)&s1, subsystem->id, &stat_subsys_io_table);

	dfly_ustat_insert_stat_subsys_kvlist((ustat_struct_t **)&s2, subsystem->id, &stat_subsys_list_table);

	subsystem->stat_name = s0;
	subsystem->stat_kvio = s1;
	subsystem->stat_kvlist = s2;

	return (0);
}


/*
 * Does not guarantee accuracy if IOs are still going on
 */
void dfly_ustat_reset_kvio_stat(stat_kvio_t *stat)
{
	dfly_ustat_set_u64(stat, &stat->puts,           0);
	dfly_ustat_set_u64(stat, &stat->gets,           0);
	dfly_ustat_set_u64(stat, &stat->dels,           0);
	dfly_ustat_set_u64(stat, &stat->exists,         0);
	dfly_ustat_set_u64(stat, &stat->iters,          0);
	dfly_ustat_set_u64(stat, &stat->putBandwidth,   0);
	dfly_ustat_set_u64(stat, &stat->getBandwidth,   0);
	dfly_ustat_set_u64(stat, &stat->put_less_4KB,   0);
	dfly_ustat_set_u64(stat, &stat->put_4KB_16KB,   0);
	dfly_ustat_set_u64(stat, &stat->put_16KB_64KB,  0);
	dfly_ustat_set_u64(stat, &stat->put_64KB_256KB, 0);
	dfly_ustat_set_u64(stat, &stat->put_256KB_1MB,  0);
	dfly_ustat_set_u64(stat, &stat->put_1MB_2MB,    0);
	dfly_ustat_set_u64(stat, &stat->put_large_2MB,  0);
	dfly_ustat_set_u64(stat, &stat->get_less_4KB,   0);
	dfly_ustat_set_u64(stat, &stat->get_4KB_16KB,   0);
	dfly_ustat_set_u64(stat, &stat->get_16KB_64KB,  0);
	dfly_ustat_set_u64(stat, &stat->get_64KB_256KB, 0);
	dfly_ustat_set_u64(stat, &stat->get_256KB_1MB,  0);
	dfly_ustat_set_u64(stat, &stat->get_1MB_2MB,    0);
	dfly_ustat_set_u64(stat, &stat->get_large_2MB,  0);

	return;
}

void dfly_ustat_reset_kvlist_stat(stat_kvlist_t *stat)
{
       dfly_ustat_set_u64(stat, &stat->listDevBandwidth,    0);
       dfly_ustat_set_u64(stat, &stat->listMemBandwidth,  0);

       return;
}

void
dfly_ustat_remove_subsys_stat(void *subsys)
{
	struct dfly_subsystem *subsystem = (struct dfly_subsystem *) subsys;

	dfly_ustat_delete(subsystem->stat_name);
	dfly_ustat_delete(subsystem->stat_kvio);
	dfly_ustat_delete(subsystem->stat_kvlist);
}

void
dfly_ustat_insert_iip(struct dfly_qpair_s *dqpair, int id,
				     const char *name, char *ip);

int
dfly_ustat_init_qpair_stat(void *qpair)
{
	struct dfly_qpair_s *dqpair = (struct dfly_qpair_s *) qpair;

	if (dqpair->parent_qpair->ctrlr->subsys->subtype == SPDK_NVMF_SUBTYPE_DISCOVERY) {
		return 0;
	}

	ustat_handle_t *h = dfly_ustats_get_handle();
	if (!h) {
		DFLY_ERRLOG("Failed to obtain ustat handle when initialize qpair ustat\n");
		return (-1);
	}

	stat_rqpair_t *st_rqpair;
	char *gname = alloca(STAT_ENAME_LEN);
	char *ctrl_gname = alloca(STAT_ENAME_LEN);

	uint32_t cid = dqpair->parent_qpair->ctrlr->cntlid;
	int id_num = dqpair->parent_qpair->ctrlr->subsys->id;

	snprintf(ctrl_gname, STAT_ENAME_LEN, "ctrlr%u", cid);
	dfly_ustat_insert_iip(dqpair, id_num, ctrl_gname, dqpair->peer_addr);

	//TODO: add session information, make the naming as sessionx.subsystemx.ctrlrx_rpairx
	snprintf(gname, STAT_ENAME_LEN, "ctrlr%u_rpair%u", cid, dqpair->parent_qpair->qid);

	dfly_ustat_insert_stat_ses_rqp_table((ustat_struct_t **)&st_rqpair, id_num, &stat_rqpair_io_table, gname);
	// Note: since when we initialize qpair. There are actually two initialize called. One is nreqs, another is 1
	// Thus, the max qd is nreqs+1
	dfly_ustat_set_u64(st_rqpair, &st_rqpair->c_max_qd, dqpair->nreqs + 1);
	assert(st_rqpair);
	dqpair->stat_qpair = st_rqpair;

	__sync_fetch_and_add(&dqpair->curr_qd, 1);
	return (0);

}

void
dfly_ustat_remove_qpair_stat(void *qpair)
{
	struct dfly_qpair_s *dqpair = (struct dfly_qpair_s *) qpair;
	dfly_ustat_delete(dqpair->stat_qpair);
	if(dqpair->stat_iip) {
		dfly_ustat_delete(dqpair->stat_iip);
		dqpair->stat_iip = NULL;
	}
}

void
dfly_ustat_update_rqpair_stat(void *qpair, int ops)
{
	struct dfly_qpair_s *dqpair = (struct dfly_qpair_s *) qpair;

	if (!dqpair->stat_qpair) {
		return;
	}

	if(dqpair->parent_qpair->state != SPDK_NVMF_QPAIR_ACTIVE) {
		return;
	}

	if (ops == 0) {
		__sync_fetch_and_add(&dqpair->curr_qd, 1);
	} else {
		__sync_fetch_and_sub(&dqpair->curr_qd, 1);
	}

	uint32_t curr_qd = dqpair->curr_qd;
	uint64_t curr_max = dfly_ustat_get_u64(dqpair->stat_qpair, &dqpair->stat_qpair->i_reqs_max);
	if (curr_qd > curr_max) {
		dfly_ustat_set_u64(dqpair->stat_qpair, &dqpair->stat_qpair->i_reqs_max, (uint64_t)curr_qd);
	}

	dfly_ustat_set_u64(dqpair->stat_qpair, &dqpair->stat_qpair->i_reqs, (uint64_t)curr_qd);
}

int dfly_qp_counters_inc_io_count(stat_rqpair_t *stats, int opc)
{
	switch (opc) {
	case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
		ustat_atomic_inc_u64(stats, &stats->puts);
		break;

	case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
		ustat_atomic_inc_u64(stats, &stats->gets);
		break;

	case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
		ustat_atomic_inc_u64(stats, &stats->dels);
		break;

	default:
		break;
	}
	return 0;
}

/*
 * Does not guarantee accuracy if IOs are still going on
 */
void dfly_qp_reset_counters(stat_rqpair_t *stats)
{
	dfly_ustat_set_u64(stats, &stats->puts,     0);
	dfly_ustat_set_u64(stats, &stats->gets,     0);
	dfly_ustat_set_u64(stats, &stats->dels,     0);
	dfly_ustat_set_u64(stats, &stats->i_reqs,     0);
	dfly_ustat_set_u64(stats, &stats->i_reqs_max, 0);
	//Don't reset stats->max_qd. This is constant;

	return;
}

int
dfly_ustat_init_module_inst_stat(void *poller_inst, char *name, int id)
{
	struct dfly_module_poller_instance_s *module_inst = (struct dfly_module_poller_instance_s *)
			poller_inst;
	stat_module_t *st_module;

	dfly_ustat_insert_stat_thread_table((ustat_struct_t **)&st_module, id, &stat_module_req_table, name);
	assert(st_module);
	module_inst->stat_module = st_module;

}

void
dfly_ustat_remove_module_inst_stat(void *poller_inst)
{
	struct dfly_module_poller_instance_s *module_inst = (struct dfly_module_poller_instance_s *)
			poller_inst;
	dfly_ustat_delete(module_inst->stat_module);
}

void
dfly_ustat_update_module_inst_stat(void *poller_inst, int ops, uint64_t num_reqs)
{
	struct dfly_module_poller_instance_s *module_inst = (struct dfly_module_poller_instance_s *)
			poller_inst;

	if (ops == 0) {
		dfly_ustat_atomic_add_u64(module_inst->stat_module, &module_inst->stat_module->i_reqs, num_reqs);
		uint64_t curr_req = dfly_ustat_get_u64(module_inst->stat_module, &module_inst->stat_module->i_reqs);
		uint64_t max_req = dfly_ustat_get_u64(module_inst->stat_module,
						      &module_inst->stat_module->i_reqs_max);
		if (curr_req > max_req) {
			dfly_ustat_set_u64(module_inst->stat_module, &module_inst->stat_module->i_reqs_max, curr_req);
		}
	} else {
		dfly_ustat_atomic_sub_u64(module_inst->stat_module, &module_inst->stat_module->i_reqs, num_reqs);
	}
}

void
dfly_ustat_atomic_sub_u64(ustat_struct_t *s, ustat_named_t *n, uint64_t v)
{
	ustat_atomic_sub_u64(s, n, v);
}

void
dfly_ustat_atomic_add_u64(ustat_struct_t *s, ustat_named_t *n, uint64_t v)
{
	ustat_atomic_add_u64(s, n, v);
}

void
dfly_ustat_atomic_inc_u64(ustat_struct_t *s, ustat_named_t *n)
{
	ustat_atomic_inc_u64(s, n);
}

void
dfly_ustat_atomic_dec_u64(ustat_struct_t *s, ustat_named_t *n)
{
	ustat_atomic_dec_u64(s, n);
}

void
dfly_ustat_delete(ustat_struct_t *s)
{
	ustat_delete(s);
}

uint64_t
dfly_ustat_get_u64(ustat_struct_t *s, ustat_named_t *n)
{
	return ustat_get_u64(s, n);
}

void
dfly_ustat_set_u64(ustat_struct_t *s, ustat_named_t *n, uint64_t v)
{
	ustat_set_u64(s, n, v);
}

void
dfly_ustat_set_string(ustat_struct_t *s, ustat_named_t *n, const char *str)
{
	ustat_set_string(s, n, str);
}

void
dfly_ustat_insert_stat_thread_table(ustat_struct_t **stat, int id, const stat_module_t *table,
				    const char *name)
{
	char *ename = alloca(STAT_ENAME_LEN);
	(void) dfly_ustats_get_ename(name, id, ename, STAT_ENAME_LEN);
	ustat_handle *h = dfly_ustats_get_handle();
	assert(h);
	// first, group name then entity name
	(*stat) = (stat_thread_t *)ustat_insert(h, STAT_ENAME_THREAD, ename,
						&ustat_class_test,
						sizeof(*table) / sizeof(ustat_named_t),
						table, NULL);


	return;
}

void
dfly_ustat_insert_stat_ses_rqp_table(ustat_struct_t **stat, int id, const stat_rqpair_t *table,
				     const char *name)
{
	char *ename = alloca(STAT_ENAME_LEN);
	(void) dfly_ustats_get_ename(STAT_ENAME_SUBSYS, id, ename, STAT_ENAME_LEN);
	ustat_handle *h = dfly_ustats_get_handle();

	(*stat) = (stat_rqpair_t *)ustat_insert(h, ename, name,
						&ustat_class_test,
						sizeof(*table) / sizeof(ustat_named_t),
						table, NULL);

	return;
}

void
dfly_ustat_insert_iip(struct dfly_qpair_s *dqpair, int id,
				     const char *name, char *ip)
{
	char *ename = alloca(STAT_ENAME_LEN);
    ustat_struct_t *stat_handle = NULL;

	(void) dfly_ustats_get_ename(STAT_ENAME_SUBSYS, id, ename, STAT_ENAME_LEN);
	ustat_handle *h = dfly_ustats_get_handle();

	stat_handle = ustat_lookup_struct(h, ename, name);
	if(stat_handle) {
		dqpair->stat_iip = NULL;
	} else {
		dqpair->stat_iip = (stat_initiator_ip_t *)ustat_insert(h, ename, name,
						&ustat_class_test,
						sizeof(stat_initiator_ip_table) / sizeof(ustat_named_t),
						&stat_initiator_ip_table, NULL);

		dfly_ustat_set_string(dqpair->stat_iip, &dqpair->stat_iip->name, ip);
	}


	return;
}

void
dfly_ustat_insert_stat_subsys_table(ustat_struct_t **stat, int id, const stat_subsys_t *table)
{
	char *ename = alloca(STAT_ENAME_LEN);
	(void) dfly_ustats_get_ename(STAT_ENAME_SUBSYS, id, ename, STAT_ENAME_LEN);
	ustat_handle *h = dfly_ustats_get_handle();
	assert(h);
	(*stat) = (stat_subsys_t *)ustat_insert(h, ename, STAT_GNAME_NAME,
						&ustat_class_test,
						sizeof(*table) / sizeof(ustat_named_t),
						table, NULL);

	return;
}


void
dfly_ustat_insert_stat_rdma_table(ustat_struct_t **stat, int id, const stat_rdma_t *table)
{
	char *ename = alloca(STAT_ENAME_LEN);
	(void) dfly_ustats_get_ename(STAT_ENAME_SUBSYS, id, ename, STAT_ENAME_LEN);
	ustat_handle *h = dfly_ustats_get_handle();
	assert(h);
	(*stat) = (stat_subsys_t *)ustat_insert(h, ename, STAT_GNAME_RDMA,
						&ustat_class_test,
						sizeof(*table) / sizeof(ustat_named_t),
						table, NULL);

	return;
}

void
dfly_ustat_insert_stat_subsys_kvio(ustat_struct_t **stat, int id, const stat_kvio_t *table)
{
	char *ename = alloca(STAT_ENAME_LEN);
	(void) dfly_ustats_get_ename(STAT_ENAME_SUBSYS, id, ename, STAT_ENAME_LEN);
	ustat_handle *h = dfly_ustats_get_handle();
	assert(h);
	(*stat) = (stat_subsys_t *)ustat_insert(h, ename, STAT_GNAME_KVIO, &ustat_class_test,
						sizeof(*table) / sizeof(ustat_named_t), table, NULL);
	return;
}

void
dfly_ustat_insert_stat_subsys_kvlist(ustat_struct_t **stat, int id, const stat_kvlist_t *table)
{
       char *ename = alloca(STAT_ENAME_LEN);
       (void) dfly_ustats_get_ename(STAT_ENAME_SUBSYS, id, ename, STAT_ENAME_LEN);
       ustat_handle *h = dfly_ustats_get_handle();
       assert(h);
       (*stat) = (stat_subsys_t *)ustat_insert(h, ename, STAT_GNAME_KVLIST, &ustat_class_test,
                                               sizeof(*table) / sizeof(ustat_named_t), table, NULL);
       return;
}
