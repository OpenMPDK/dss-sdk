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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <sched.h>
#include <pthread.h>

#include <kv_stats.h>

#define CONF_NAME	"kv_stats.conf"

struct test_context {
	int nr_cpu;
	int interval_sec;
	int iteration;
} ;

void *stats_log_proc(void *param)
{
	struct test_context *ctx = (struct test_context *) param;

	int nr_cpu = ctx->nr_cpu;
	int iter = ctx->iteration;
	int interval = ctx->interval_sec;

	int nr_kvv_dev = 36;
	int nr_containers = 2;
	int nr_nvme_dev = 36;
	int nr_total_cpu = 72;
	int i = 0, j = 0;
	int cpu_id[nr_cpu] ;

	cpu_id[0] = sched_getcpu();
	for (i = 0; i < nr_cpu; i++) {
		cpu_id[i] = (cpu_id[0] + i) % nr_total_cpu ;
		printf("cpu_id %u\n", cpu_id[i]);
	}

	struct kvv_counters *kvv_cnt[nr_kvv_dev][nr_containers];
	struct kv_nvme_counters *kv_nvmef_cnt[nr_nvme_dev];

	// get host stats context
	void *h_stats[nr_cpu] ;
	for (i = 0; i < nr_cpu; i++) {
		h_stats[i] = kv_stats_init_host(cpu_id[i], CONF_NAME);
	}

	//get kvb counters
	//struct kvb_counters * kvb_cnt = kv_stats_get_kvb_counters(h_stats[0]);

	// get kvv counters
	for (i = 0; i < nr_kvv_dev; i++)
		for (j = 0; j < nr_containers ; j ++)
			kvv_cnt[i][j] = kv_stats_get_kvv_counters(h_stats[i % nr_cpu], i, j);

	// get kv_nvme counters
	for (i = 0 ; i < nr_nvme_dev; i++) {
		char dev_name[32];
		int sz = sprintf(dev_name, "kv_nvmef_device_%d", i);
		dev_name[sz] = 0;
		kv_nvmef_cnt[i] = kv_stats_get_kv_nvmef_counters(h_stats[i % nr_cpu], dev_name, strlen(dev_name));
		ATOMIC_ADD(kv_nvmef_cnt[i]->nr_put, 10);
		ATOMIC_ADD(kv_nvmef_cnt[i]->nr_get, 20);
	}

	while (iter--) {

		time_t tm;
		time(&tm);

		srandom((unsigned int)tm);
		//initial kvv counters
		for (i = 0; i < nr_kvv_dev; i++)
			for (j = 0; j < nr_containers; j++) {

				/*timestamp,1498254380.12345:__func__,get_key:key,00FBCA:status,1:cpu_id,72:dev_id,5:container_id,0: */
				int probe_status = 0; //0 for enter, 1 for exit
				long key = random();

				KV_LOG(0, "f,%s:k,%llx:s,%d:p,%d\n",
				       __FUNCTION__, key, probe_status, cpu_id[j % nr_cpu]);

				KV_LOG(0, "f,%s:k,%llx:s,%d:p,%d\n",
				       "kvb_foo", key, probe_status, cpu_id[j % nr_cpu]);

				KV_LOG(0, "f,%s:k,%llx:s,%d:p,%d:d,%d:c,%d\n",
				       "kvv_foo", key, probe_status, cpu_id[j % nr_cpu], i, j);

				KV_LOG(0, "f,%s:k,%llx:s,%d:p,%d\n",
				       "kvs_foo", key, probe_status, cpu_id[j % nr_cpu]);

				probe_status = 1;

				KV_LOG(0, "f,%s:k,%llx:s,%d:p,%d\n",
				       "kvs_foo", key, probe_status, cpu_id[j % nr_cpu]);

				KV_LOG(0, "f,%s:k,%llx:s,%d:p,%d:d,%d:c,%d\n",
				       "kvv_foo", key, probe_status, cpu_id[j % nr_cpu], i, j);

				KV_LOG(0, "f,%s:k,%llx:s,%d:p,%d\n",
				       "kvb_foo", key, probe_status, cpu_id[j % nr_cpu]);

				KV_LOG(0, "f,%s:k,%llx:s,%d:p,%d\n",
				       __FUNCTION__, key, probe_status, cpu_id[j % nr_cpu]);

				ATOMIC_ADD(kvv_cnt[i][j]->nr_del, 10);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_fail, 1);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_async_get, 1000);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_sync_get, 100);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_sync_put, 100);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_async_put, 1100);

				//inc some
				ATOMIC_INC(kvv_cnt[i][j]->nr_async_get);
				ATOMIC_INC(kvv_cnt[i][j]->nr_sync_get);
				ATOMIC_INC(kvv_cnt[i][j]->nr_sync_put);
				ATOMIC_INC(kvv_cnt[i][j]->nr_async_put);

				ATOMIC_ADD(kvv_cnt[i][j]->nr_del, 8);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_fail, 0);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_async_get, 2000);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_sync_get, 100);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_sync_put, 100);
				ATOMIC_ADD(kvv_cnt[i][j]->nr_async_put, 2100);

			}
		sleep(interval);
	}

	return ctx;
}

int main(int argc, char **argv)
{

	int nr_cpu = 2;
	int nr_kvv_dev = 36;
	int nr_containers = 2;
	int nr_nvme_dev = 36;
	int nr_total_cpu = 72;
	int i = 0, j = 0;

	pthread_t th_1, th_2;

	struct test_context ctx;

	// get cpu id
	ctx.nr_cpu = atoi(argv[1]);
	ctx.iteration = atoi(argv[2]);
	ctx.interval_sec = 1;

	init_kv_stats_service(CONF_NAME);

	pthread_create(&th_1, NULL, stats_log_proc, (void *) &ctx);
	pthread_create(&th_2, NULL, stats_log_proc, (void *) &ctx);

	pthread_join(th_1, NULL);
	pthread_join(th_2, NULL);

	deinit_kv_stats_service();

	return 0;
}
