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

#ifndef _DSS_KVTRANS_MODULE_H
#define _DSS_KVTRANS_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dragonfly.h"
#include "kvtrans.h"

typedef int (*request_handler_fn)(void *ctx, void *dss_request);

typedef struct dss_kvtrans_module_ctx_s {
    struct dfly_subsystem *dfly_subsys;
    uint32_t num_threads;
    request_handler_fn request_handler;
    struct dfly_module_s *dss_kvtrans_module;
} dss_kvtrans_module_ctx_t;

typedef struct dss_kvtrans_thread_ctx_s {
    dss_kvtrans_module_ctx_t *mctx;
    // instance ctx, used in dfly_module->m_inst
    void *inst_ctx;
    int inst_index;
    kvtrans_ctx_t *ctx;
    kvtrans_params_t *params;
    // now one shard is one device;
    uint32_t shard_index;
} dss_kvtrans_thread_ctx_t;

kvtrans_params_t *set_default_kvtrans_params();
dss_kvtrans_thread_ctx_t *init_kvtrans_thread_ctx(dss_kvtrans_module_ctx_t *mctx, int inst_index, kvtrans_params_t *params);
void free_kvtrans_thread_ctx(dss_kvtrans_thread_ctx_t *inst_ctx);



#ifdef __cplusplus
}
#endif


#endif