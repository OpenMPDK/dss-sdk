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


#ifndef DF_CTRL_H
#define DF_CTRL_H

#include <ustat.h>
#include <df_stats.h>
#include "spdk/queue.h"

#define  MAX_DFLY_HOST_NAME     SPDK_NVMF_NQN_MAX_LEN
#define  DFLY_PROF_DEF_NAME     "default_host"

typedef struct stat_ses {
	ustat_named_t name;
	ustat_named_t addr;
} stat_ses_t;

typedef struct dfly_prof {
	char                            *dfp_nqn;
	uint32_t                        dfp_credits[DFLY_QOS_ATTRS];

	TAILQ_ENTRY(dfly_prof)          dfp_link;
} dfly_prof_t;

typedef struct dfly_ses_id {
	uint16_t			dfsi_num;
	char				*dfsi_name;
	char				*dfsi_addr;
} dfly_ses_id_t;

typedef struct dfly_session {
	dfly_ses_id_t			dfs_id;
	dfly_prof_t                     *dfs_host_prof;
	stat_kvio_t			*dfs_stats_io;
	stat_ses_t			*dfs_stats_ses;
	uint64_t                        dfs_curr_tags[DFLY_QOS_ATTRS];

	uint32_t                        dfs_ctrlc;
	TAILQ_ENTRY(dfly_session)       dfs_link;
} dfly_session_t;

typedef struct dfly_ctrl {
	dfly_session_t                  *ct_session;
	uint32_t                         ct_cntlid;
	TAILQ_ENTRY(dfly_ctrl)           ct_link; // Subsystem
	void                            *ct_base[0];
} dfly_ctrl_t;

struct spdk_nvmf_qpair;
typedef struct dfly_qos_client_ops {/*ctrlr ops - abstraction */
	void *(*create)(char *name, struct spdk_nvmf_qpair *p/*dummy*/, size_t bsize);
	void (*destroy)(void *base);
} dfly_qos_client_ops_t;

extern dfly_qos_client_ops_t qos_client_ops;

#endif
