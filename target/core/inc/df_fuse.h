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


#ifndef DRAGONFLY_FUSE_H
#define DRAGONFLY_FUSE_H


/**
 * \file
 * dragonfly FUSE definitions
 */


#ifdef __cplusplus
extern "C" {
#endif

#include "spdk/stdinc.h"
#include "spdk/assert.h"

#include "df_def.h"
#include "limits.h"

#define	FUSE_SUCCESS					0x0000	//general rc for fuse operation.

//rc for fuse_io
#define FUSE_IO_RC_WRITE_SUCCESS		0x0001	//bookkeeped for write, next: wal_module or io_module 
#define FUSE_IO_RC_DELETE_SUCCESS		0x0002	//bookkeeped for delete, next: wal_module or io module
#define FUSE_IO_RC_F2_COMPLETE			0x0003	//done the fuse (r+w). next: complete 
#define FUSE_IO_RC_WAIT_FOR_F2			0x0004	//F1 queued, wait F2 form a linked request;
#define FUSE_IO_RC_WAIT_FOR_F1			0x0005	//F1 queued, wait F2 form a linked request;

#define FUSE_IO_RC_FUSE_IN_PROGRESS		0x0006	//F1/F2 in progress.
#define FUSE_IO_RC_F1_FAIL				0x0007	//F1 compare mismatch, next, abort 
#define FUSE_IO_RC_F1_MATCH				0x0008	//F1 compare match. next F2

#define	FUSE_IO_RC_RETRY				0x0010	//retry when Fuse in progress, or object update in progress on Fuse.


#define	FUSE_IO_RC_PASS_THROUGH			0x0000	// cache done, log might fail, write through is expected
// stat machine to change the request states on complete cb
#define	FUSE_ERROR_DELETE_SUCCESS 0x0008	//deletion done on cache and log, delete through is expected
#define	FUSE_ERROR_DELETE_PENDING 0x0009	//deletion done on cache and log, record in log
#define	FUSE_ERROR_DELETE_FAIL	 0x0010	//deletion failed.

#define	FUSE_ERROR_IO_RETRY		0x0010	//retry when Fuse in progress, or object update in progress on Fuse.
#define	FUSE_ERROR_F1_PENDING	0x0010

#define	FUSE_ERROR_IO			0x0100	//uncertain io error
#define	FUSE_ERROR_NO_SUPPORT	0x0201	//KV operation that FUSE does not supported
#define FUSE_ERROR_PARAM_INVALID	0x0202	//any request from dfly with invalid parameters.

typedef struct fuse_conf_s {
	int fuse_enabled ;
	int wal_enabled;
	int	nr_maps_per_pool;
	int fuse_nr_cores ;
	int fuse_debug_level;
	int fuse_op_flag;
	long long fuse_timeout_ms;
	char fuse_nqn_name[256];
} fuse_conf_t;

extern fuse_conf_t g_fuse_conf;

int fuse_init(struct dfly_subsystem *pool, int nr_maps,
	      int nr_of_cores, int fuse_flag, void *cb, void *cb_arg);
int fuse_init_by_conf(struct dfly_subsystem *pool, void *conf, void *cb, void *cb_arg);

int fuse_finish(struct dfly_subsystem *pool);
int fuse_io(struct dfly_request *req, int fuse_op_flags);

#define fuse_log(fmt, args...)\
	DFLY_INFOLOG(DFLY_LOG_FUSE, fmt, ##args);

#ifdef __cplusplus
}
#endif

#endif // DRAGONFLY_FUSE_H
