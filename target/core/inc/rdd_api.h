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

#ifndef RDD_API_H
#define RDD_API_H

#ifdef __cplusplus
extern "C" {
#endif

//TODO: ADD for client code
//#include <stdio.h>
//#include <stdlib.h>
//#include <netdb.h>

//#include <rdma/rdma_cma.h>

#define RDD_PROTOCOL_VERSION (0xCAB00001)

#define RDD_RDMA_MAX_KEYED_SGL_LENGTH ((1u << 24u) - 1)

typedef struct rdd_ctx_s rdd_ctx_t;

typedef struct rdd_cinfo_s {
	char *ip;
	char *port;
} rdd_cinfo_t;

typedef struct rdd_cfg_s {
    int num_cq_cores_per_ip;
    int max_sgl_segs;
	int n_ip;
	rdd_cinfo_t *conn_info;
} rdd_cfg_t;

/**
 * @brief Parameters structure passed for RDMA data direct initializtion
 * 
 */
typedef struct rdd_params_s {
	uint32_t max_sgl_segs;
} rdd_params_t;

struct rdd_queue_priv_s {
    union {
        struct {
            uint32_t proto_ver;
            uint32_t qid;
	        uint32_t hsqsize;
	        uint32_t hrqsize;
        } client;//Private data from client
        struct {
            uint32_t proto_ver;
            uint16_t qhandle; //Hash identifier provided to client for direct transfer
        } server;//Private data from server
    } data;
};//RDD queue private

#define RDD_CMD_HOST_READ (0x001)
#define RDD_CMD_CTRL_WRITE (0x002)
#define RDD_CMD_CTRL_READ (0x003)

typedef struct rdd_rdma_cmd_s {
    uint8_t opc;
    uint32_t cid;
    union {
        struct {
            uint32_t len:24;
            uint64_t caddr;
            uint64_t haddr;
            uint32_t ckey;
            uint32_t rsvd;
        }hread;//Host Read
    } cmd;//Command 
} rdd_rdma_cmd_t; //32 Bytes

typedef struct rdd_rdma_rsp_s {
    uint8_t status;
    uint32_t rsvd:24;
    uint32_t cid;
} rdd_rdma_rsp_t;//8 Bytes

/**
 * @brief RDMA Data Direct initialization function
 * 
 * @param c config of multiple ip ports to listen
 * @param params 
 * @return rdd_ctx_t* Context pointer to be used in reference to request to the module
 */
rdd_ctx_t *rdd_init(rdd_cfg_t *c, rdd_params_t params);

/**
 * @brief RDMA Data Direct cleanup function
 * 
 * @param ctx Context allocated during intialization to free
 */
void rdd_destroy(rdd_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // RDD_API_H
