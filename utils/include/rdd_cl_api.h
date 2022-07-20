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

#ifndef RDD_CL_API_H
#define RDD_CL_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rdma/rdma_cma.h>

typedef struct rdd_cl_conn_ctx_s rdd_cl_conn_ctx_t;

typedef enum {
	RDD_PD_GLOBAL = 0, // One Global Protection Domain per Client context
	RDD_PD_CONN, // Each RDMA Direct queue gets it's own protection Domain
} rdd_cl_pd_type_e;

typedef struct {
	rdd_cl_pd_type_e pd_type;//RDMA protection domain type for client context
} rdd_cl_ctx_params_t;

typedef struct rdd_cl_conn_params_s {
    const char *ip;
    const char *port;
    uint32_t qd;
} rdd_cl_conn_params_t;

struct rdd_client_ctx_s;

struct rdd_client_ctx_s *rdd_cl_init(rdd_cl_ctx_params_t params);
void rdd_cl_destroy(struct rdd_client_ctx_s *ctx);

rdd_cl_conn_ctx_t *rdd_cl_create_conn(struct rdd_client_ctx_s *cl_ctx, rdd_cl_conn_params_t params);
void rdd_cl_destroy_connection(rdd_cl_conn_ctx_t *ctx);

uint16_t rdd_cl_conn_get_qhandle(void * arg);
struct ibv_mr *rdd_cl_conn_get_mr(void *ctx, void *addr, size_t len);
void rdd_cl_conn_put_mr(struct ibv_mr *mr);

#ifdef __cplusplus
}
#endif

#endif // RDD_CL_API_H

