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


#ifndef _DF_LOG_H
#define _DF_LOG_H

#ifdef  __cplusplus
extern "C" {
#endif

#include "spdk_internal/log.h"
#include "spdk/log.h"

#define LOGBUF_SZ 1024

//DFLY wrapper for spdk logging
//
#define DFLY_LOG_REGISTER_COMPONENT SPDK_LOG_REGISTER_COMPONENT

#define DFLY_LOG_ERROR  SPDK_LOG_ERROR
#define DFLY_LOG_WARN   SPDK_LOG_WARN
#define DFLY_LOG_NOTICE SPDK_LOG_NOTICE
#define DFLY_LOG_INFO   SPDK_LOG_INFO
#define DFLY_LOG_DEBUG  SPDK_LOG_DEBUG

typedef struct spdk_log_flag dfly_trace_flag_t;
typedef enum spdk_log_level dfly_log_level_e;

#define DFLY_INFOLOG(FLAG, ...)									\
	do {											\
		extern dfly_trace_flag_t FLAG;						\
		if (FLAG.enabled) {								\
			dfly_log(DFLY_LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__);	\
		}										\
	} while (0)

#ifdef DEBUG

#define DFLY_DEBUGLOG(FLAG, ...)								\
	do {											\
		extern dfly_trace_flag_t FLAG;						\
		if (FLAG.enabled) {								\
			dfly_log(DFLY_LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__);	\
		}										\
	} while (0)

#define DFLY_TRACEDUMP(FLAG, LABEL, BUF, LEN)						\
	do {										\
		extern dfly_trace_flag_t FLAG;					\
		if ((FLAG.enabled) && (LEN)) {						\
			dfly_trace_dump1(stderr, (LABEL), (BUF), (LEN));			\
		}									\
	} while (0)

#else
#define DFLY_DEBUGLOG(...) do { } while (0)
#define DFLY_TRACEDUMP(...) do { } while (0)
#endif

#define DFLY_NOTICELOG(...) \
	dfly_log(DFLY_LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DFLY_WARNLOG(...) \
	dfly_log(DFLY_LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DFLY_ERRLOG(...) \
	dfly_log(DFLY_LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

void dfly_log(dfly_log_level_e level, const char *file, const int line, const char *func,
	      const char *format, ...) __attribute__((__format__(__printf__, 5, 6)));

void dfly_trace_dump1(FILE *fp, const char *label, const void *buf, size_t len);

#ifdef  __cplusplus
}
#endif

#endif
