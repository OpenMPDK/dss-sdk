/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2023 Samsung Electronics Co., Ltd.
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

#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>

#include "df_log.h"
#include "dss_spdk_wrapper.h"

#define DSS_RELEASE_ASSERT(x) assert((x))

#ifdef DSS_RELEASE_BUILD
    #define DSS_ASSERT(x) {}
#else
    #define DSS_ASSERT(x) assert((x))
#endif //DSS_RELEASE_BUILD

#define dss_unlikely(cond)	__builtin_expect((cond), 0)
#define dss_likely(cond)	__builtin_expect(!!(cond), 1)

void set_kvtrans_disk_data_store(bool val);
void set_kvtrans_disk_meta_store(bool val);

#ifndef DSS_BUILD_CUNIT_TEST

#define DSS_LOG(...) DFLY_LOG(...)

#define DSS_ERRLOG(...) \
    dfly_log(DFLY_LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define DSS_NOTICELOG(...) \
    dfly_log(DFLY_LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define DSS_WARNLOG(...) \
    dfly_log(DFLY_LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef DEBUG

#define DSS_DEBUGLOG(FLAG, ...)								\
	do {											\
		extern dfly_trace_flag_t FLAG;						\
		if (FLAG.enabled) {								\
			dfly_log(DFLY_LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__);	\
		}										\
	} while (0)

#else
#define DFLY_DEBUGLOG(...) do { } while (0)
#endif

#else

#define DSS_LOG printf

#define DSS_ERRLOG printf

#define DSS_NOTICELOG printf

#define DSS_WARNLOG printf

#define DSS_DEBUGLOG(...) do { } while (0)

#endif //DSS_BUILD_CUNIT_TEST
