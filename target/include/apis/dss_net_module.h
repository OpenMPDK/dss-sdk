/**
 * The Clear BSD License
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 * BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef DSS_NET_MODULE_H
#define DSS_NET_MODULE_H

#include "apis/dss_module_apis.h"
#include "df_module.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dss_net_module_s dss_net_module_t;

typedef struct dss_net_mod_dev_info_s {
    char *dev_name;
    char *ip;
    uint64_t num_nw_threads;
} dss_net_mod_dev_info_t;

typedef struct dss_net_module_config_s {
    int count;
    dss_net_mod_dev_info_t dev_list[0];
} dss_net_module_config_t;

char * dss_net_module_get_name(char *ip);
dss_module_t * dss_net_module_find_module_ctx(char *ip);

// dss_module_status_t dss_net_module_subsys_start(dss_subsystem_t *ss, dss_net_module_config_t *cfg, df_module_event_complete_cb cb, void *cb_arg);

// void dss_net_module_subsys_stop(dss_subsystem_t *ss, void *arg /*Not used*/, df_module_event_complete_cb cb, void *cb_arg);

void dss_net_setup_request(dss_request_t *req, dss_module_instance_t *m_inst, void *nvmf_req);

void dss_net_teardown_request(dss_request_t *req);

void dss_nvmf_process_as_no_op(dss_request_t *req);

dss_request_opc_t dss_nvmf_get_dss_opc(void *req);

#ifdef __cplusplus
}
#endif

#endif // DSS_NET_MODULE_H
