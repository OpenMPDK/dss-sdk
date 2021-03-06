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


#ifndef DF_STATS_H
#define DF_STATS_H

#include <ustat.h>
#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Ustat has a two part naming scheme including entity name and group
 * name. Please run dssd/cmd/ustat utility to see the current output
 * format before you make changes. If we can keep the naming scheme
 * clear and simple, that would make dev life a lot easier
 */

#define STAT_NAME_SUBSYS	"subsystem"
#define STAT_NAME_SES		"session"
#define STAT_NAME_DRIVE		"drive"
#define STAT_NAME_THREAD	"thread"

#define STAT_ENAME_LEN		64
#define STAT_ENAME_TARGET	"target."
#define STAT_ENAME_SUBSYS	STAT_ENAME_TARGET STAT_NAME_SUBSYS
#define STAT_ENAME_SES		STAT_ENAME_TARGET STAT_NAME_SES
#define STAT_ENAME_DRIVE	STAT_ENAME_TARGET STAT_NAME_DRIVE
#define STAT_ENAME_THREAD	STAT_ENAME_TARGET STAT_NAME_THREAD


#define STAT_GNAME_KVIO	"kvio"
#define STAT_GNAME_KVLIST   "kvlist"
#define STAT_GNAME_NAME	"id"
#define STAT_GNAME_RDMA "rdma"

#define STAT_GNAME_COUNTERS "counters"
#define STAT_GNAME_DEBUG "debug"

typedef struct stat_counter_types_s {
	ustat_named_t icounters;
	ustat_named_t ccounters;
} stat_counter_types_t;

typedef struct rdb_debug_counters_s {
	ustat_named_t rdb_mem_bw;
} rdb_debug_counters_t;

typedef struct stat_serial {
	ustat_named_t name;
} stat_serial_t;

typedef struct stat_subsys {
	ustat_named_t   name;
} stat_subsys_t;

typedef struct stat_initiator_ip {
	ustat_named_t   name;
} stat_initiator_ip_t;

typedef struct stat_kvio {
	ustat_named_t puts;
	ustat_named_t gets;
	ustat_named_t dels;
	ustat_named_t exists;
	ustat_named_t iters;
	ustat_named_t putBandwidth;
	ustat_named_t getBandwidth;
	ustat_named_t put_less_4KB;
	ustat_named_t put_4KB_16KB;
	ustat_named_t put_16KB_64KB;
	ustat_named_t put_64KB_256KB;
	ustat_named_t put_256KB_1MB;
	ustat_named_t put_1MB_2MB;
	ustat_named_t put_large_2MB;
	ustat_named_t get_less_4KB;
	ustat_named_t get_4KB_16KB;
	ustat_named_t get_16KB_64KB;
	ustat_named_t get_64KB_256KB;
	ustat_named_t get_256KB_1MB;
	ustat_named_t get_1MB_2MB;
	ustat_named_t get_large_2MB;
	ustat_named_t i_pending_reqs;
} stat_kvio_t;

typedef struct stat_kvlist {
       ustat_named_t listDevBandwidth;
       ustat_named_t listMemBandwidth;
} stat_kvlist_t;

typedef struct stat_rqpair {
	ustat_named_t i_reqs;
	ustat_named_t i_reqs_max;
	ustat_named_t c_max_qd;
	ustat_named_t puts;
	ustat_named_t gets;
	ustat_named_t dels;
} stat_rqpair_t;

typedef struct stat_module {
	ustat_named_t i_reqs;
	ustat_named_t i_reqs_max;
} stat_module_t;

typedef struct stat_rdma {
	ustat_named_t rdma_rs;
	ustat_named_t rdma_r_reads;
	ustat_named_t rdma_r_writes;
	ustat_named_t rdma_r_sends;
	ustat_named_t rdma_r_recvs;
	ustat_named_t rdma_r_time;
} stat_rdma_t;

typedef struct stat_thread {
	ustat_named_t i_reqs;
	ustat_named_t i_reqs_max;
} stat_thread_t;

typedef struct stat_blk_io stat_block_io_t;

extern const ustat_class_t ustat_class_test;

extern const stat_serial_t stat_dev_serial_table;
extern const stat_serial_t stat_subsys_serial_table;
extern const stat_kvio_t stat_subsys_io_table;
extern const stat_kvio_t stat_dev_io_table;

extern int dfly_ustats_get_ename(const char *ename, int id, char *buf, size_t len);
extern ustat_handle_t *dfly_ustats_get_handle(void);
extern int dfly_ustats_init(void);
extern int dfly_qp_counters_inc_io_count(stat_rqpair_t *stats, int opc);

int dfly_ustat_init_dev_stat(uint32_t subsys_id, const char *dev_name, void *dev);
void dfly_ustat_remove_dev_stat(void *dev);
int dfly_ustat_init_subsys_stat(void *subsys, const char *nqn);
void dfly_ustat_remove_subsys_stat(void *subsys);
int dfly_ustat_init_qpair_stat(void *);
void dfly_ustat_remove_qpair_stat(void *);
int dfly_ustat_init_module_inst_stat(void *, char *, int);
void dfly_ustat_remove_module_inst_stat(void *);

// ops 0 is add & ops 1 is sub
void dfly_ustat_update_rqpair_stat(void *qpair, int ops);
void dfly_ustat_update_module_inst_stat(void *module_inst, int ops, uint64_t num_reqs);

void dfly_ustat_atomic_inc_u64(ustat_struct_t *s, ustat_named_t *n);
void dfly_ustat_atomic_dec_u64(ustat_struct_t *s, ustat_named_t *n);
void dfly_ustat_delete(ustat_struct_t *s);
void dfly_ustat_atomic_sub_u64(ustat_struct_t *s, ustat_named_t *n, uint64_t v);
void dfly_ustat_atomic_add_u64(ustat_struct_t *s, ustat_named_t *n, uint64_t v);
uint64_t dfly_ustat_get_u64(ustat_struct_t *s, ustat_named_t *n);
void dfly_ustat_set_u64(ustat_struct_t *s, ustat_named_t *n, uint64_t v);
void dfly_ustat_set_string(ustat_struct_t *s, ustat_named_t *n, const char *str);

void dfly_ustat_reset_kvio_stat(stat_kvio_t *stat);
void dfly_qp_reset_counters(stat_rqpair_t *stats);
void dfly_ustat_reset_block_stat(stat_block_io_t *stat);

#ifdef  __cplusplus
}
#endif

#endif  // DF_STATS_H
