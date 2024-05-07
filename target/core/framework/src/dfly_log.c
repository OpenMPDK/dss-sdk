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


#include <time.h>
#include <stdarg.h>
#include <stdio.h>

#include "df_log.h"

/* Temp log unitlity. To be removed once we get somehting better */

DFLY_LOG_REGISTER_COMPONENT("dfly_ns_precision", DFLY_LOG_NS_PRECISION)

DFLY_LOG_REGISTER_COMPONENT("dfly_qos", DFLY_LOG_QOS)
DFLY_LOG_REGISTER_COMPONENT("dfly_module", DFLY_LOG_MODULE)
DFLY_LOG_REGISTER_COMPONENT("dfly_subsystem", DFLY_LOG_SUBSYS)
DFLY_LOG_REGISTER_COMPONENT("dfly_io", DFLY_LOG_IO)
DFLY_LOG_REGISTER_COMPONENT("dfly_fuse", DFLY_LOG_FUSE)
DFLY_LOG_REGISTER_COMPONENT("dfly_numa", DFLY_LOG_NUMA)
DFLY_LOG_REGISTER_COMPONENT("dfly_wal", DFLY_LOG_WAL)
DFLY_LOG_REGISTER_COMPONENT("dfly_list", DFLY_LOG_LIST)
DFLY_LOG_REGISTER_COMPONENT("dfly_iter", DFLY_LOG_ITER)
DFLY_LOG_REGISTER_COMPONENT("dfly_lock_service", DFLY_LOCK_SVC);
DFLY_LOG_REGISTER_COMPONENT("dss_rdd", DSS_RDD);
DFLY_LOG_REGISTER_COMPONENT("dss_kvtrans", DSS_KVTRANS);
DFLY_LOG_REGISTER_COMPONENT("dss_iotask", DSS_IO_TASK);


int DFLY_LOG_LEVEL;


static int
timespec2str(char *buf, size_t len, struct timespec *ts)
{
	size_t l = 0;
	struct tm t;

	localtime_r(&ts->tv_sec, &t);

	l = strftime(buf, len, "%T", &t);
	snprintf(buf + l, len, ".%09ld", ts->tv_nsec);
	return (l + 9);
}

void dfly_log(dfly_log_level_e level, const char *file, const int line, const char *func,
	      const char *format, ...)
{
	va_list ap;
	char buf[LOGBUF_SZ];
	char ts[50];
	struct timespec tp;

	va_start(ap, format);

	ts[0] = '\0';

	if (DFLY_LOG_NS_PRECISION.enabled) {
		clock_gettime(CLOCK_REALTIME, &tp);
		timespec2str(ts, 50, &tp);
	}

	vsnprintf(buf, sizeof(buf), format, ap);

	spdk_log(level, file, line, func, "%s:%s", ts, buf);

	va_end(ap);

	return;
}

void dfly_trace_dump(FILE *fp, const char *label, const void *buf, size_t len)
{
	return;
}

