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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

#include <sched.h>
#include "kv_stats.h"
#include "kv_dump.h"

typedef unsigned long long uint64_t;

extern struct host_stats *__host_stats[MAX_CPU] ;  // for host side stats
extern int host_nr_cpu ; 	// number of cpu, work as host role
extern int target_nr_cpu ; 	// number of cpu work as target

extern kv_stats_counters_table_t kvb_cnt_table[];
extern kv_stats_counters_table_t kvv_cnt_table[];
extern kv_stats_counters_table_t kvs_cnt_table[];
extern kv_stats_counters_table_t kv_nvme_cnt_table[];

int nr_segment = 0 ;
int nr_counters = 0;

void create_example_stats_for_dump()
{
	// get cpu id
	int cpu_id = sched_getcpu();
	printf("cpu_id %u \n", cpu_id);

	struct kvv_counters *kvv_cnt[2];
	struct kv_nvme_counters *kv_nvmef_cnt[2];

	// get host stats context
	void *h_stats = kv_stats_init_host(cpu_id);

	//get kvb counters
	struct kvb_counters *kvb_cnt = kv_stats_get_kvb_counters(h_stats);

	// get kvv counters
	kvv_cnt[0] = kv_stats_get_kvv_counters(h_stats, 0, 1);
	kvv_cnt[1] = kv_stats_get_kvv_counters(h_stats, 1, 2);

	// get kvs counters
	struct kvs_counters *kvs_cnt = kv_stats_get_kvs_counters(h_stats);

	// get kv_nvme counters
	kv_nvmef_cnt[0] = kv_stats_get_kv_nvmef_counters(h_stats, "kv_nvmef_device_0");
	kv_nvmef_cnt[1] = kv_stats_get_kv_nvmef_counters(h_stats, "kv_nvmef_device_1");

	// initial kvb counters

	ATOMIC_ADD(kvb_cnt->nr_del, 1);
	ATOMIC_ADD(kvb_cnt->nr_fail, 0);
	ATOMIC_ADD(kvb_cnt->nr_keys, 1000);
	ATOMIC_ADD(kvb_cnt->nr_put, 2001);
	ATOMIC_ADD(kvb_cnt->nr_get, 1000);

	//inc some
	ATOMIC_INC(kvb_cnt->nr_keys);
	ATOMIC_INC(kvb_cnt->nr_put);
	ATOMIC_INC(kvb_cnt->nr_get);

	//initial kvv counters
	ATOMIC_ADD(kvv_cnt[0]->nr_del, 10);
	ATOMIC_ADD(kvv_cnt[0]->nr_fail, 1);
	ATOMIC_ADD(kvv_cnt[0]->nr_async_get, 1000);
	ATOMIC_ADD(kvv_cnt[0]->nr_sync_get, 100);
	ATOMIC_ADD(kvv_cnt[0]->nr_sync_put, 100);
	ATOMIC_ADD(kvv_cnt[0]->nr_async_put, 1100);

	//inc some
	ATOMIC_INC(kvv_cnt[0]->nr_async_get);
	ATOMIC_INC(kvv_cnt[0]->nr_sync_get);
	ATOMIC_INC(kvv_cnt[0]->nr_sync_put);
	ATOMIC_INC(kvv_cnt[0]->nr_async_put);

	ATOMIC_ADD(kvv_cnt[1]->nr_del, 8);
	ATOMIC_ADD(kvv_cnt[1]->nr_fail, 0);
	ATOMIC_ADD(kvv_cnt[1]->nr_async_get, 2000);
	ATOMIC_ADD(kvv_cnt[1]->nr_sync_get, 100);
	ATOMIC_ADD(kvv_cnt[1]->nr_sync_put, 100);
	ATOMIC_ADD(kvv_cnt[1]->nr_async_put, 2100);

	//inc some
	ATOMIC_INC(kvv_cnt[1]->nr_async_get);
	ATOMIC_INC(kvv_cnt[1]->nr_sync_get);
	ATOMIC_INC(kvv_cnt[1]->nr_sync_put);
	ATOMIC_INC(kvv_cnt[1]->nr_async_put);


	//initial kvs counters
	ATOMIC_ADD(kvs_cnt->nr_del, 1);
	ATOMIC_ADD(kvs_cnt->nr_fail, 0);
	ATOMIC_ADD(kvs_cnt->nr_keys, 1000);
	ATOMIC_ADD(kvs_cnt->nr_put, 2001);
	ATOMIC_ADD(kvs_cnt->nr_get, 1000);

	//inc some
	ATOMIC_INC(kvs_cnt->nr_keys);
	ATOMIC_INC(kvs_cnt->nr_put);
	ATOMIC_INC(kvs_cnt->nr_get);

	// initial kv_nvmef counters
	ATOMIC_ADD(kv_nvmef_cnt[0]->nr_get, 2000);
	ATOMIC_ADD(kv_nvmef_cnt[0]->nr_put, 1000);
	ATOMIC_ADD(kv_nvmef_cnt[0]->nr_failure, 2);
	ATOMIC_ADD(kv_nvmef_cnt[0]->nr_16b_less, 500);
	ATOMIC_ADD(kv_nvmef_cnt[0]->nr_16k_1k, 200);
	ATOMIC_ADD(kv_nvmef_cnt[0]->nr_1k_4k, 300);
	ATOMIC_ADD(kv_nvmef_cnt[0]->nr_4k_32k, 100);
	ATOMIC_ADD(kv_nvmef_cnt[0]->nr_32k_1m, 1000);
	ATOMIC_ADD(kv_nvmef_cnt[0]->other_size, 0);

	//inc some
	ATOMIC_INC(kv_nvmef_cnt[0]->nr_get);
	ATOMIC_INC(kv_nvmef_cnt[0]->nr_put);

	ATOMIC_INC(kv_nvmef_cnt[0]->nr_16b_less);
	ATOMIC_INC(kv_nvmef_cnt[0]->nr_16k_1k);
	ATOMIC_INC(kv_nvmef_cnt[0]->nr_4k_32k);


	ATOMIC_ADD(kv_nvmef_cnt[1]->nr_get, 3000);
	ATOMIC_ADD(kv_nvmef_cnt[1]->nr_put, 1000);
	ATOMIC_ADD(kv_nvmef_cnt[1]->nr_failure, 2);
	ATOMIC_ADD(kv_nvmef_cnt[1]->nr_16b_less, 600);
	ATOMIC_ADD(kv_nvmef_cnt[1]->nr_16k_1k, 200);
	ATOMIC_ADD(kv_nvmef_cnt[1]->nr_1k_4k, 700);
	ATOMIC_ADD(kv_nvmef_cnt[1]->nr_4k_32k, 100);
	ATOMIC_ADD(kv_nvmef_cnt[1]->nr_32k_1m, 1000);
	ATOMIC_ADD(kv_nvmef_cnt[1]->other_size, 0);

	//inc some
	ATOMIC_INC(kv_nvmef_cnt[1]->nr_get);
	ATOMIC_INC(kv_nvmef_cnt[1]->nr_put);

	ATOMIC_INC(kv_nvmef_cnt[1]->nr_16b_less);
	ATOMIC_INC(kv_nvmef_cnt[1]->nr_16k_1k);
	ATOMIC_INC(kv_nvmef_cnt[1]->nr_4k_32k);

}

int main(int argc, char **argv)
{
	int fd = -1;
	int rc = 0;
	int dump_sz = 0;
	int sample_data_sz = 0;
	int nr_sample = 0;
	int interval_sec = 0;

	char file_header_buffer[PAGE_SIZE];
	char dump_buffer[PAGE_SIZE];

	if (argc < 4) {
		fprintf(stderr, "%s dump_file nr_samples internal");
		return 0;
	}

	//emulate the sample counters
	create_example_stats_for_dump();

	fd = open(argv[1], O_TRUNC | O_CREAT | O_RDWR, 0644);
	if (fd < 0) {
		perror("open file");
		rc = -1;
		goto done;
	}

	nr_sample = atoi(argv[2]);
	interval_sec = atoi(argv[3]);

	//initial the count table
	segment_cnt_table[nr_segment ].nr_cnt = sizeof(kvb_cnt_table) / sizeof(kv_stats_counters_table_t);
	segment_cnt_table[nr_segment ++].cnt_table = kvb_cnt_table;
	segment_cnt_table[nr_segment ].nr_cnt = sizeof(kvv_cnt_table) / sizeof(kv_stats_counters_table_t);
	segment_cnt_table[nr_segment ++].cnt_table = kvv_cnt_table;
	segment_cnt_table[nr_segment ].nr_cnt = sizeof(kvs_cnt_table) / sizeof(kv_stats_counters_table_t);
	segment_cnt_table[nr_segment ++].cnt_table = kvs_cnt_table;
	segment_cnt_table[nr_segment ].nr_cnt = sizeof(kv_nvme_cnt_table) / sizeof(
			kv_stats_counters_table_t);
	segment_cnt_table[nr_segment ++].cnt_table = kv_nvme_cnt_table;

	file_header_t file_header;
	prepare_sample_header(&file_header, nr_segment);

	//dump the sample header
	int header_sz = dump_file_header(&file_header, file_header_buffer, PAGE_SIZE);
	printf("sample header size %d\n", header_sz);
	if (header_sz < 0) {
		return -1;
	}

	dump_sz = dump_data(fd, file_header_buffer, header_sz);
	if (dump_sz < header_sz) {
		printf("dump less data for header size %d %d\n", header_sz, dump_sz);
		return -2;
	}

	while (nr_sample--) {
		sample_data_sz = one_sampling(host_nr_cpu, nr_segment, dump_buffer, PAGE_SIZE);
		if (sample_data_sz > 0) {
			rc = dump_data(fd, dump_buffer, sample_data_sz);
		}
		sleep(interval_sec);
	}

done:
	if (fd >= 0) {
		close(fd);
	}

	return rc;
}
