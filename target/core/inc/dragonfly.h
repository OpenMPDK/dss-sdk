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


#ifndef DRAGONFLY_H
#define DRAGONFLY_H


/**
 * \file
 * dragonfly definitions
 */


#ifdef __cplusplus
extern "C" {
#endif

#include "spdk/stdinc.h"
#include "spdk/assert.h"
#include "spdk/util.h"
#include "spdk/queue.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/conf.h"
#include "spdk/io_channel.h"
#include "spdk/bdev.h"
#include "spdk/nvme.h"

#include "spdk_internal/log.h"

#include "df_dev.h"
#include "df_req.h"
#include "df_pool.h"
#include "df_io_pool.h"
#include "df_wal_pool.h"
#include "df_wal.h"
#include "df_fuse.h"
#include "df_iter.h"
#include "df_list.h"
#include "df_ctrl.h"

#include "df_bdev.h"
#include "df_device.h"
#include "df_mm.h"
#include "df_stats.h"
#include "df_counters.h"

#include "df_kd.h"
#include "df_module.h"
#include "df_io_thread.h"
#include "df_req_handler.h"
#include "df_numa.h"

#include "df_log.h"

#include "df_atomic.h"
#include "df_counters.h"
#include "uthash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/******************* Macro utilites **********************/


#ifndef CONTAINEROF
#define CONTAINEROF(ptr, type, member) ((type *)((uintptr_t)ptr - offsetof(type, member)))
#endif
#ifndef MAX
#define MAX(a, b) ((a)>=(b) ? (a):(b))
#endif
#ifndef MIN
#define MIN(a, b) ((a)<=(b) ? (a):(b))
#endif

#define DF_LATENCY_MEASURE_ENABLED

#define MAX_SS (64)
#define KB                  (1024)
#define KB_SHIFT			(10)
#define MB                  (1048576)
#define MB_SHIFT			(20)

#define DFLY_ASSERT(x) assert((x))

#define OSS_TARGET_ENABLED (1)
#define OSS_TARGET_DISABLED (0)

#define MAX_STRING_LEN (256)

struct dragonfly_core {
	uint32_t nvmf_core; /**< this core number */
	uint32_t current_core; /**< round robin next core */
};

struct dfly_module_list_s {
	struct dfly_module_s *dfly_io_module;
	struct dfly_module_s *dfly_wal_module;
	struct dfly_module_s *dfly_fuse_module;
	struct dfly_module_s *lock_service;
	struct dfly_module_s *dfly_list_module;
};

struct dfly_io_device_s {
	struct spdk_nvmf_ns *ns;
	uint32_t index;

	int32_t numa_node;
	stat_kvio_t *stat_io;
	stat_serial_t *stat_serial;
};

typedef struct df_subsys_conf_s {
	uint32_t oss_target_enabled;
} df_subsys_conf_t;

struct dfly_subsystem {
	df_subsys_conf_t conf;
	struct dfly_module_list_s mlist;
	struct dfly_kd_context_s *kd_ctx;

	int id;
	char *name;//spdk_nvmf_subsystem subnqn

	pthread_mutex_t subsys_lock;
	pthread_mutex_t ctrl_lock;
	unsigned ref_cnt;

	int num_io_devices;
	struct dfly_io_device_s *devices;

	void *parent_ctx;//struct spdk_nvmf_subsystem

	stat_kvio_t *stat_kvio;
	stat_subsys_t *stat_name;

	bool shutting_down;
	bool initialized;
	int wal_init_status;
	int list_init_status;
	void (*wal_init_cb)(void *subsystem, int status);
	void (*list_init_cb)(void *subsystem, int status);

	TAILQ_HEAD(, dfly_ctrl) df_ctrlrs;
};


struct df_ss_cb_event_s {
	struct dfly_subsystem *ss;
	df_module_event_complete_cb df_ss_cb;
	void *df_ss_cb_arg;
	uint32_t src_core;
	void *df_ss_private;
};

typedef void (*df_exec_on_core)(void *arg1, void *arg2);


typedef struct dict_t_struct {
	char key[60];
	int value;
	char message[256];
	UT_hash_handle hh;
} dict_t;

struct dragonfly_ops {

	void *(*dfly_iomem_get_xfer_chunk)(void); /**< get xferable memory chunk */
	int (*dfly_iomem_put_xfer_chunk)(void *xfer_chunk); /**< put/free xferable memory chunk */

	int (*dfly_core_allocate)(df_exec_on_core cb, void *arg1,
				  void *arg2); /**< mainly used by QOS/WAL */
	int (*dfly_core_release)(df_exec_on_core cb, void *arg1, void *arg2); /**< mainly used by QOS/WAL */

	int (*dfly_io_set_device_distribution)(struct dfly_request *req); /**< mainly used by QOS */

	int (*dfly_io_forward)(struct dfly_request *req); /**< mainly used by QOS/WAL */
	int (*dfly_io_complete)(struct dfly_request *req); /**< mainly used by WAL */

	int (*dfly_device_open)(char *path, devtype_t type, bool write);
	void (*dfly_device_close)(int handle);
	int (*dfly_device_read)(int handle, void *buff, uint64_t offset, uint64_t nbytes,
				df_dev_io_completion_cb cb, void *cb_arg);
	int (*dfly_device_write)(int handle, void *buff, uint64_t offset, uint64_t nbytes,
				 df_dev_io_completion_cb cb, void *cb_arg);

	int (*dfly_wal_flush)(struct dfly_key *key, struct dfly_value *val); /**< mainly used by WAL */

	int (*dfly_cm_target_data_migrate)(int src_tgt_id, int dst_tgt_id, void *prefix,
					   int prefix_len); /**< mainly used by CM, prefix are comma separated values */
	int (*dfly_cm_container_data_migrate)(int src_cnt_id, int dst_cnt_id, void *prefix,
					      int prefix_len); /**< mainly used by CM, prefix are comma separated values */
	int (*dfly_cm_target_data_remove)(int tgt_id, void *prefix,
					  int prefix_len); /**< mainly used by CM, prefix are comma separated values */
	int (*dfly_cm_container_data_remove)(int cnt_id, void *prefix,
					     int prefix_len); /**< mainly used by CM, prefix are comma separated values */
	int (*dfly_cm_job_status)(int job_id); /**< mainly used by CM*/
};

struct dragonfly {
	uint32_t max_core; /**< maimum available cores for DragonFly */

	struct dragonfly_core core[64]; /**< round robin core selection data from each core */

	bool target_pool_enabled; /**< whether want to disable dragonfly for debugging or measuring baseline performance */
	bool test_nic_bw; /** Enable simulation for short circuit without drive IO (PUT/GET) */

	uint32_t num_sgroups; /**< number of pools */

	struct dfly_subsystem
		subsystems[MAX_SS]; /**< currently there will be only two subsystems (data and meta-data pool) */
	struct dragonfly_wal_ops *wal_ops; /**< exported services from WAL */
	ustat_handle_t              *s_handle;
	bool				df_qos_enable;
	TAILQ_HEAD(, dfly_prof)   	df_profs;
	TAILQ_HEAD(, dfly_session)      df_sessions;
	uint32_t			df_sessionc;
	pthread_mutex_t			df_ses_lock;

	dict_t	 			*disk_stat_table;
	uint64_t req_lat_to;//Request latency timeout
};

struct dfly_qpair_s {
	struct spdk_nvmf_qpair *parent_qpair;
	struct dfly_request *reqs;
	int nreqs;
	char listen_addr[INET6_ADDRSTRLEN];
	char peer_addr[INET6_ADDRSTRLEN];

	/*Global counter to assist in sequencing IO requests*/
	uint16_t				io_counter;//Can cycle back to 0
	char *p_key_arr;

	dfly_ctrl_t *df_ctrlr;//dfly_ctrlr_t

	uint32_t curr_qd;
	stat_rqpair_t *stat_qpair;

	uint32_t max_pending_lock_reqs;
	uint32_t npending_lock_reqs;

	void *df_poller;
};

struct dword_bytes {
	uint8_t cdwb1;
	uint8_t cdwb2;
	uint8_t cdwb3;
	uint8_t cdwb4;
};

int dragonfly_init(void);
int dragonfly_finish(void);

int dfly_init(void);

void dfly_config_read(struct spdk_conf_section *sp);
int dfly_config_parse(void);

int dragonfly_core_init(uint32_t nvmf_core);
int dragonfly_core_finish(uint32_t nvmf_core);

struct dfly_subsystem *dfly_get_subsystem_no_lock(uint32_t ssid);
struct dfly_subsystem *dfly_get_subsystem(uint32_t ssid);
struct dfly_subsystem *dfly_put_subsystem(uint32_t ssid);

int dfly_io_module_subsystem_start(struct dfly_subsystem *subsystem,
				   dfly_spdk_nvmf_io_ops_t *io_ops, df_module_event_complete_cb cb, void *cb_arg);
void dfly_io_module_subsystem_stop(struct dfly_subsystem *subsystem, void *args/*Not Used*/,
					df_module_event_complete_cb cb, void *cb_arg);

int dfly_lock_service_subsys_start(struct dfly_subsystem *subsys, void *arg/*Not used*/,
				   df_module_event_complete_cb cb, void *cb_arg);
void dfly_lock_service_subsystem_stop(struct dfly_subsystem *subsys, void *arg/*Not used*/,
				   df_module_event_complete_cb cb, void *cb_arg);

int dfly_core_allocate(df_exec_on_core cb, void *arg1, void *arg2); /**< mainly used by QOS/WAL */
int dfly_core_release(df_exec_on_core cb, void *arg1, void *arg2); /**< mainly used by QOS/WAL */

int dfly_io_set_device_distribution(struct dfly_request *req); /**< mainly used by QOS */

int dfly_io_forward(struct dfly_request *req); /**< mainly used by QOS/WAL */
int dfly_io_complete(struct dfly_request *req); /**< mainly used by WAL */

extern struct dragonfly *g_dragonfly;

void dfly_kv_pool_dev_test(void);

void dfly_fuse_complete(void *arg, bool sccess);
uint32_t dfly_req_fuse_2_delete(struct dfly_request *req);

int  dfly_nvmf_migrate(struct spdk_nvmf_request *req);

typedef void(*df_subsystem_event_processed_cb)(struct spdk_nvmf_subsystem *subsystem, void *cb_arg,
				       int status);

struct df_ss_cb_event_s * df_ss_cb_event_allocate(struct dfly_subsystem *ss, df_module_event_complete_cb cb, void *cb_arg, void *event_private);
void df_ss_cb_event_complete(struct df_ss_cb_event_s *ss_cb_event);

int dfly_subsystem_init(void *vctx, dfly_spdk_nvmf_io_ops_t *io_ops,
			df_subsystem_event_processed_cb cb, void *cb_arg, int status);
int dfly_subsystem_destroy(void *vctx, df_subsystem_event_processed_cb cb, void *cb_arg, int cb_status);
void *dfly_subsystem_list_device(struct dfly_subsystem *ss, void **dev_list, uint32_t *nr_dev);

void df_subsys_update_dss_enable(uint32_t ssid, uint32_t ss_dss_enabled);
void df_subsystem_parse_conf(uint32_t ssid, struct spdk_conf_section *subsys_sp);
uint32_t df_subsystem_enabled(uint32_t ssid);

char *
dfly_nvmf_rdma_qpair_peer_addr(struct spdk_nvmf_qpair *qp, char *addr, size_t len);

uint32_t df_qpair_susbsys_enabled(struct spdk_nvmf_qpair *nvmf_qpair, struct spdk_nvmf_request *req);

int dfly_qpair_init(struct spdk_nvmf_qpair *nvmf_qpair, char *req_arr, int req_size, int max_reqs);
int dfly_qpair_destroy(struct dfly_qpair_s *dqpair);
int dfly_nvmf_request_complete(struct spdk_nvmf_request *req);

extern struct spdk_nvmf_tgt *g_spdk_nvmf_tgt;
void wal_flush_complete(struct df_dev_response_s resp, void *arg);
void log_recovery_writethrough_complete(struct df_dev_response_s resp, void *arg);

dict_t *dfly_getItem(dict_t **dict, char *key);


void dfly_delItem(dict_t **dict, char *key);

void dfly_addItem(dict_t **dict, char *key, int value, char *message);

void dfly_updateItem(dict_t **dict, char *key, int value, char *message);
void dfly_deleteAllItems(dict_t **disk_table);
void dfly_dump_status_info(struct spdk_json_write_ctx *w);
void nvmf_tgt_subsystem_start_continue(void *nvmf_subsystem, int status);

struct spdk_nvme_cmd *dfly_get_nvme_cmd(struct dfly_request *req);
void iter_ctrl_complete(struct df_dev_response_s resp, void *args);

typedef struct spdk_mempool dfly_mempool;

dfly_mempool *dfly_mempool_create(char *name, size_t sz, size_t cnt);
void dfly_mempool_destroy(dfly_mempool *mp, size_t cnt);
void *dfly_mempool_get(dfly_mempool *mp);
void dfly_mempool_put(dfly_mempool *mp, void *item);

//QoS
void* dfly_poller_init(size_t bsize);
void dfly_poller_fini(void *base);

void dfly_qos_update_nops(dfly_request_t *req, void *tr_p, dfly_ctrl_t *ctrl);

dfly_ctrl_t *df_init_ctrl(struct dfly_qpair_s *dqpair, uint16_t cntlid, uint32_t ssid);
void df_destroy_ctrl(uint32_t ssid, uint16_t cntlid);

struct spdk_nvme_ns_data *dfly_nvme_ns_get_data(struct spdk_nvme_ns *ns);

struct spdk_nvme_ctrlr * spdk_bdev_nvme_get_ctrlr(struct spdk_bdev *bdev);
void dfly_nvme_ctrlr_update_namespaces(struct spdk_nvme_ctrlr *ctrlr);

#ifdef DF_LATENCY_MEASURE_ENABLED
void df_lat_update_tick(struct dfly_request *dreq, uint32_t state);
void df_print_tick(struct dfly_request *dreq);

#endif

#ifdef __cplusplus
}
#endif

#endif // DRAGONFLY_H
