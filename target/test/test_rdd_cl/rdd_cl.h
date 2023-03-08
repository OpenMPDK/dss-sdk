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

#ifndef RDD_CL_H
#define RDD_CL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <assert.h>
#include <sys/queue.h>

#include <rdma/rdma_cma.h>

#include "rdd_api.h"

/* Offset of member MEMBER in a struct of type TYPE. */
#define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)
#define containerof(ptr, type, member) ((type *)((uintptr_t)ptr - offsetof(type, member)))


#define RDD_CL_MAX_DEFAULT_QUEUEC (1)
#define RDD_CL_DEFAULT_QDEPTH (128)
#define RDD_CL_DEFAULT_SEND_WR_NUM RDD_CL_DEFAULT_QDEPTH
#define RDD_CL_DEFAULT_RECV_WR_NUM RDD_CL_DEFAULT_SEND_WR_NUM
#define RDD_CL_CQ_QDEPTH (RDD_CL_DEFAULT_SEND_WR_NUM + RDD_CL_DEFAULT_RECV_WR_NUM)

#define RDD_CL_TIMEOUT_IN_MS (500)

typedef struct rdd_cl_conn_ctx_s rdd_cl_conn_ctx_t;

enum rdd_cl_queue_state_e {
    RDD_CL_Q_INIT = 0,
    RDD_CL_Q_LIVE,
};

typedef struct rdd_cl_conn_params_s {
    const char *ip;
    const char *port;
    uint32_t qd;
} rdd_cl_conn_params_t;

typedef struct rdd_cl_dev_s {
    //TODO: ??Required??//struct ibv_pd *pd;
	//TODO: ??Required??//struct ibv_mr *mr;
	//TODO: ??Required??//struct ibv_mw *mw;
	struct ibv_device *dev;
    TAILQ_ENTRY(rdd_cl_dev_s) dev_link;
} rdd_cl_dev_t;

typedef struct rdd_cl_queue_s {
    uint32_t qid;
    rdd_cl_conn_ctx_t *conn;
    pthread_mutex_t qlock;
    enum rdd_cl_queue_state_e state;
    struct rdma_cm_id *cm_id;
    struct ibv_pd *pd;
    struct ibv_context *ibv_ctx;
    struct ibv_qp *qp;

    struct ibv_mr *cmd_mr;
    struct ibv_mr *rsp_mr;

    struct rdd_cl_req_s *reqs;
    rdd_rdma_cmd_t *cmds;
    rdd_rdma_rsp_t *rsps;
} rdd_cl_queue_t;

struct rdd_cl_conn_ctx_s {
    int conn_id;
    struct addrinfo *ai;
	uint16_t qhandle;
    uint32_t qd;
    uint32_t queuec;
    rdd_cl_queue_t *queues;
    rdd_cl_dev_t *device;
    //IBV
    struct ibv_comp_channel *ch;
    union {
        struct {
            pthread_t cq_poll_thread;
        }pt;//Pthread
    }th;//Thread

    pthread_mutex_t conn_lock;

    struct rdd_client_ctx_s *cl_ctx;
};

struct rdd_client_ctx_s {
    struct ibv_device **ibv_devs;
    struct rdma_event_channel *cm_ch;
    union {
        struct {
            pthread_t cm_thread;
        }pt;//Pthread
    }th;//Thread
    TAILQ_HEAD(, rdd_cl_dev_s) devices;
};

enum rdd_cl_wr_type_e {
    RDD_CL_WR_TYPE_REQ_RECV = 0,
    RDD_CL_WR_TYPE_RSP_SEND,
    RDD_CL_WR_TYPE_DATA_READ,
};

typedef enum rdd_cl_req_state_e {
    RDD_CL_REQ_FREE = 0,
    RDD_CL_REQ_RECV,
    RDD_CL_REQ_FINI,
} rdd_cl_req_state_t;

struct rdd_cl_wr_s {
    enum rdd_cl_wr_type_e type;
};

struct rdd_cl_req_s {
    struct {
        struct rdd_cl_wr_s rdd_cl_wr;
        struct ibv_recv_wr	recv_wr;
	    struct ibv_sge		recv_sge;
    } req;
    struct {
        struct rdd_cl_wr_s rdd_cl_wr;
     	struct ibv_send_wr	send_wr;
	    struct ibv_sge		send_sge;
    } rsp;
    struct {
        struct rdd_cl_wr_s rdd_cl_wr;
     	struct ibv_send_wr	data_wr;
	    struct ibv_sge		data_sge;
    } data;
    struct ibv_mr       *data_mr;
    void *temp_data;
    rdd_rdma_cmd_t *rdd_cl_cmd;
    rdd_rdma_rsp_t *rdd_cl_rsp;
    rdd_cl_req_state_t state;
    struct rdd_cl_queue_s *q;
    TAILQ_ENTRY(rdd_cl_req_s) link;
};

struct rdd_client_ctx_s *rdd_cl_init(void);
void rdd_cl_destroy(struct rdd_client_ctx_s *ctx);

rdd_cl_conn_ctx_t *rdd_cl_create_conn(struct rdd_client_ctx_s *cl_ctx, rdd_cl_conn_params_t params);
void rdd_cl_destroy_connection(rdd_cl_conn_ctx_t *ctx);

uint16_t rdd_cl_conn_get_qhandle(void * arg);

#ifdef __cplusplus
}
#endif

#endif // RDD_CL_H
