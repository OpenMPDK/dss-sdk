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


#ifndef __KV_STATS_H
#define __KV_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

// Atomic data access:
// https://gcc.gnu.org/onlinedocs/gcc-4.4.3/gcc/Atomic-Builtins.html
// list operations(add/iteration/del/..), will be provided by stats lib, similar like linux kernel list_head.

#define ATOMIC_ADD(counter, number) __sync_fetch_and_add(&(counter), (number))
#define ATOMIC_INC(counter)         __sync_fetch_and_add(&(counter), 1)
#define ATOMIC_READ(counter)        __sync_fetch_and_add((&counter), 0)

#define KB                  (1024)
#define MB                  (1048576)
#define KV_STATS_VERSION    (1)
#define MAX_SEGMENT         (16)
#define CNT_NAME_SZ         (32)

#define MAX_CPU             (256) //assume max 256 cpu core
#define MAX_DEV_NAME        (256)
#define PAGE_SIZE           (4096)

#define DEFAULT_CONF_FILE   "kv_stats.conf"

typedef enum enum_counter_type {
	LOG_TYPE_INT64 = 0,
	LOG_TYPE_INT32,
	LOG_TYPE_STRING
} counter_type;

typedef struct kv_stats_counters_table_s {
	char                counter_name[CNT_NAME_SZ];
	counter_type        type;
	int                 value_size;
} kv_stats_counters_table_t;

struct kvb_counters {
	long long nr_keys;
	long long nr_get;
	long long nr_put;
	long long nr_del;
	long long nr_fail;
};

struct kvv_counters {
	long long nr_sync_get;
	long long nr_async_get;
	long long nr_sync_put;
	long long nr_async_put;
	long long nr_del;
	long long nr_fail;
};

// for the kvv to count

// * it find out the its own kvv_counters pointer by looking up the device_id and container_id for the first time.
// * If the ids is not present, kvv should create a kv_stats with the (device,container) addition in the lists.
// * afterwards, the counter could be access directly in atomic way.
// * kvv can remove kvv_stats instance given the (device_id, container_id) the list.
//
// * we can change the list to array if we know how many devices to support and how many containers per device.

struct kvs_counters {
	long long nr_keys;
	long long nr_get;
	long long nr_put;
	long long nr_del;
	long long nr_fail;
};

struct kv_nvme_counters {
	long long max_qd;
	long long current_qd;
	long long nr_get;
	long long nr_put;
	long long nr_16b_less;
	long long nr_16k_1k;
	long long nr_1k_4k;
	long long nr_4k_32k;
	long long nr_32k_1m;
	long long other_size;
	long long nr_failure;
};

// for the kv_nvmef/kv_nvme instance to count,
// * it find out the its own nvme_counters by looking up the device_name in device_list for first time.
// * If not present, kvv should add the (device_name) in the lists.
// * afterwards, the counter could be access directly in atomic way.
// * The kv_nvmef/kv_nvme might need to remove its stats from the list.

// helps for stats setup
int init_kv_stats_service(const char *conf_name);
int deinit_kv_stats_service(void);

void *kv_stats_init_host(int cpu_id, const char *conf_name);
void *kv_stats_init_target(int cpu_id, const char *conf_name);
struct kvb_counters *kv_stats_get_kvb_counters(void *stats);
struct kvv_counters *kv_stats_get_kvv_counters(void *stats, int dev_id, int container_id);
struct kvs_counters *kv_stats_get_kvs_counters(void *stats);
struct kv_nvme_counters *kv_stats_get_kv_nvmef_counters(void *stats, const char *device_name,
		int len);
struct kv_nvme_counters *kv_stats_get_kv_nvme_counters(void *stats, const char *device_name,
		int len);

void KV_LOG(int level, const char *fmt, ...);
int get_log_level(void);
int set_log_level(int level);

#ifdef DFLY_CONFIG_KVSH_LATENCY
#define MAX_CPUS 128
void *g_kvstat_cpu_ctx[MAX_CPUS];
char *g_kv_stats_conf_file = "./kv_stats.conf";

#define KVSH_KV_LOG(level, fmt, args...) \
{ \
    int cpu_id = sched_getcpu(); \
    if(!g_kvstat_cpu_ctx[cpu_id]) g_kvstat_cpu_ctx[cpu_id] = kv_stats_init_host(cpu_id, g_kv_stats_conf_file); \
    KV_LOG(level, fmt, args); \
    }
#endif //DFLY_CONFIG_KVSH_LATENCY

#ifdef __cplusplus
}
#endif
#endif //__KV_STATS_H
