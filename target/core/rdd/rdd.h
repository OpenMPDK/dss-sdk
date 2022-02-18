/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2021 Samsung Electronics Co., Ltd.
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

#ifndef RDD_H
#define RDD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

#include <rdma/rdma_cma.h>
#include "spdk/string.h"

#include "dragonfly.h"
#include "rdd_api.h"


#define RDD_DEFAULT_LISTEN_BACKLOG (10)
#define RDD_DEFAULT_CM_POLL_PERIOD_IN_US (10ULL)

#define RDD_MAX_CHANDLE_CNT (65535)
#define RDD_DEFAULT_MIN_CHANDLE_COUNT (1024)

struct rdd_rdma_listener_s {
    struct addrinfo *ai;

    char *listen_ip;
    char *listen_port;

    union {
        struct {
            struct rdma_event_channel *ev_channel;
            struct rdma_cm_id *cm_id;
        }rdma;
    }tr;//transport
    union {
        struct {
            struct spdk_thread *cm_thread;
            struct spdk_poller *cm_poller;
            struct pollfd cm_poll_fd;
        } spdk;
    } th;//Thread
	struct rdd_ctx_s *prctx;//Parent Context
    TAILQ_HEAD(, rdd_rdma_queue_s) queues;
    TAILQ_ENTRY(rdd_rdma_listener_s) link;
};

struct rdd_ctx_s {
	struct {
		pthread_rwlock_t rwlock;
		uint32_t nuse;
		uint32_t nhandles;
		struct rdd_rdma_queue_s **handle_arr;
		uint16_t hmask;
	} handle_ctx;

    TAILQ_HEAD(, rdd_rdma_listener_s) listeners;
};

enum rdd_queue_state_e {
    RDD_QUEUE_CONNECTING,
    RDD_QUEUE_LIVE
};

struct rdd_rdma_queue_s {
    rdd_ctx_t *ctx;
    uint32_t host_qid;
    uint32_t recv_qd;
    uint32_t send_qd;
	uint32_t outstanding_qd;
    //Internal
    uint16_t qhandle;
    enum rdd_queue_state_e state;
    rdd_rdma_cmd_t *cmds;
    struct rdd_rsp_s *rsps;
    struct rdd_req_s *reqs;
    TAILQ_HEAD(, dfly_request) pending_reqs;
    TAILQ_HEAD(, rdd_req_s) free_reqs;
    TAILQ_HEAD(, rdd_req_s) outstanding_reqs;
    union {
        struct {
            struct spdk_thread *cq_thread;
            struct spdk_poller *cq_poller;
        } spdk;
    } th;//Completion thread
    //IBV
    struct rdma_cm_id *cm_id;
    struct ibv_pd *pd;
    struct ibv_mr *cmd_mr;
    struct ibv_mr *rsp_mr;

    struct spdk_mem_map *map;
	struct spdk_ring *req_submit_ring;

    struct ibv_comp_channel *comp_ch;
    struct ibv_qp *qp;
    struct ibv_cq *cq;
    int ncqe;

    TAILQ_ENTRY(rdd_rdma_queue_s) link;

	//Debug
	uint64_t submit_latency;
	uint64_t submit_count;
};

enum rdd_wr_type_e {
    RDD_WR_TYPE_REQ_SEND = 0,
    RDD_WR_TYPE_RSP_RECV,
	RDD_WR_TYPE_DATA_WRITE,
};

struct rdd_wr_s {
    enum rdd_wr_type_e type;
};

typedef enum rdd_req_state_e {
    RDD_REQ_FREE = 0,
    RDD_REQ_SENT,
	RDD_REQ_DATA_PENDING,
    RDD_REQ_DONE,
} rdd_req_state_t;

struct rdd_rsp_s {
    rdd_rdma_rsp_t rsp;
    struct rdd_wr_s rdd_wr;
   	struct ibv_recv_wr	recv_wr;
    struct ibv_sge		recv_sge;
    uint16_t idx;
    struct rdd_rdma_queue_s *q;
};

struct rdd_req_s {
    struct {
        struct rdd_wr_s rdd_wr;
        struct ibv_send_wr	send_wr;
	    struct ibv_sge		send_sge;
    } req;
    struct {
        struct rdd_wr_s rdd_wr;
     	struct ibv_send_wr	data_wr;
	    struct ibv_sge		data_sge;
    } data;
    uint16_t rsp_idx;
    uint16_t id;
    rdd_rdma_cmd_t *rdd_cmd;
    
    rdd_req_state_t state;
    struct rdd_rdma_queue_s *q;
	void *ctx;
    // struct ibv_mr *data_mr;
	uint64_t start_tick;
    TAILQ_ENTRY(rdd_req_s) link;
};

int rdd_post_cmd_host_read(struct rdd_rdma_queue_s *q, void *local_val, void *remote_val, uint32_t vlen, void *ctx);

void rdd_dss_submit_request(void * arg);
int rdd_post_req2queue(rdd_ctx_t *ctx, uint16_t client_handle, dfly_request_t *req);

#ifdef __cplusplus
}
#endif

#endif // RDD_H
