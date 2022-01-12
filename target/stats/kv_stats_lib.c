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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdarg.h>
#include <errno.h>

#include <sched.h>
#include <pthread.h>
#include <libconfig.h>

#include "list.h"
#include "kv_stats.h"
#include "kv_dump.h"

#define MAX_PATH_SIZE			256
#define CONF_BUFFER_MIN_SIZE	4
#define CONF_BUFFER_SIZE_UNIT	MB
int debug_lib = 1;
#define DBG(fmt, args...) if(debug_lib) fprintf(g_serv_context.debug_info_fd, fmt, ##args)

kv_stats_counters_table_t kvb_cnt_table[] = {
	//for kvbench
	{ "kvb__nr_key",  LOG_TYPE_INT64, 8 },
	{ "kvb__nr_get",  LOG_TYPE_INT64, 8 },
	{ "kvb__nr_put",  LOG_TYPE_INT64, 8 },
	{ "kvb__nr_del",  LOG_TYPE_INT64, 8 },
	{ "kvb__nr_fail", LOG_TYPE_INT64, 8 },
};

kv_stats_counters_table_t kvv_cnt_table[] = {
	//for kvv
	{ "kvv__dev_id",        LOG_TYPE_INT64, 8 },
	{ "kvv__container_id",  LOG_TYPE_INT64, 8 },
	{ "kvv__nr_sync_get",   LOG_TYPE_INT64, 8 },
	{ "kvv__nr_async_get",  LOG_TYPE_INT64, 8 },
	{ "kvv__nr_sync_put",   LOG_TYPE_INT64, 8 },
	{ "kvv__nr_async_put",  LOG_TYPE_INT64, 8 },
	{ "kvv__nr_del",        LOG_TYPE_INT64, 8 },
	{ "kvv__nr_fail",       LOG_TYPE_INT64, 8 },
};

kv_stats_counters_table_t kvs_cnt_table[] = {
	//for kvs
	{ "kvs__nr_key",  LOG_TYPE_INT64, 8 },
	{ "kvs__nr_get",  LOG_TYPE_INT64, 8 },
	{ "kvs__nr_put",  LOG_TYPE_INT64, 8 },
	{ "kvs__nr_del",  LOG_TYPE_INT64, 8 },
	{ "kvs__nr_fail", LOG_TYPE_INT64, 8 },
};

kv_stats_counters_table_t kv_nvme_cnt_table[] = {
	//for kv_nvmef
	{ "kv_nvme__device_name",    LOG_TYPE_STRING, MAX_DEV_NAME },
	{ "kv_nvme__max_qd",         LOG_TYPE_INT64, 8 },
	{ "kv_nvme__current_qd",     LOG_TYPE_INT64, 8 },
	{ "kv_nvme__nr_get",         LOG_TYPE_INT64, 8 },
	{ "kv_nvme__nr_put",         LOG_TYPE_INT64, 8 },
	{ "kv_nvme__nr_16b_less",    LOG_TYPE_INT64, 8 },
	{ "kv_nvme__nr_16k_1k",      LOG_TYPE_INT64, 8 },
	{ "kv_nvme__nr_1k_4k",       LOG_TYPE_INT64, 8 },
	{ "kv_nvme__nr_4k_32k",      LOG_TYPE_INT64, 8 },
	{ "kv_nvme__nr_32k_1m",      LOG_TYPE_INT64, 8 },
	{ "kv_nvme__other_size",     LOG_TYPE_INT64, 8 },
	{ "kv_nvme__nr_failure",     LOG_TYPE_INT64, 8 },
};

#define LOG_LEVEL_DEFAULT 3
typedef enum LOG_LEVEL {
	LOG_LEVEL_0 = 0,
	LOG_LEVEL_1 = 1,
	LOG_LEVEL_2 = 2,
	LOG_LEVEL_3 = 3,
	LOG_LEVEL_4 = 4,
	LOG_LEVEL_MAX = 5
} log_level_t;

log_level_t g_log_level = LOG_LEVEL_DEFAULT;
#define ONE_LOG_SIZE_DEFAULT	128

typedef struct stats_conf_info_s {
	int conf_buffer_size_unit ;
	int conf_sample_interval ;
	int conf_dump_file_size ;
	int conf_log_file_size;
	int conf_one_log_size;
	int conf_max_sample_per_dump;
	int conf_sample_alignment ; //the sample data alignment, power of 2 KB, 4KB, 8KB, ...
	char conf_stats_storage_path[MAX_PATH_SIZE]; //path of the root of dump file system
	char conf_debug_info[MAX_PATH_SIZE];
	char conf_log_storage_path[MAX_PATH_SIZE];
} stats_conf_info_t;

enum stat_date_type {
	STATS_YEA = 0,
	STATS_MON = 1,
	STATS_DAY = 2,
	STATS_HOU = 3,
	STATS_DATE_MAX = 4
} date_type_t;

typedef struct service_context_s {
	stats_conf_info_t conf;
	pthread_mutex_t buffer_mutex;
	pthread_cond_t cond_var;
	pthread_t th_sampling;
	pthread_t th_dumping;
	pthread_t th_log_dumping;
	int thread_terminate;
	int is_host;

	void *data_buffer[2];
	void *sampling_buffer;
	void *dumping_buffer;
	int dumper_is_ready;
	int sample_data_is_full;

	pthread_mutex_t log_mutex;
	pthread_cond_t cond_log;
	void *log_buffer[2];
	void *logging_buffer;
	void *logging_buffer_orig;
	void *log_dumper_buffer;
	int log_dumper_is_ready;
	int log_data_is_full;
	int max_nr_log;
	int cur_nr_log;

	int sampling_buffer_size;
	int cur_nr_samples;
	FILE *debug_info_fd;
	struct {
		int host_nr_cpu;
		int target_nr_cpu;
		int nr_segment;
		int dump_file_size;
		int file_hdr_size;
		file_header_t file_hdr;
		char *file_hdr_dump_ptr;
		segment_cnt_table_t segment_cnt_table[MAX_SEGMENT];
	} dump_info;

	int date[STATS_DATE_MAX]; //date[0] for year, date[1] for mon, date[2] for day, date[3] for hour
	char current_stats_dir_path[PAGE_SIZE]; //full path of current dir for dump stats files.
	char current_log_dir_path[PAGE_SIZE]; //full path of current dir for dump log files.
} service_context_t;
service_context_t g_serv_context ;
int dump_file_header(file_header_t *sample_head, char **buffer, int *psize);
int kv_stats_dump_buffer(service_context_t *ctx, const char *buffer, int size);

struct kvb_stats {
	struct kvb_counters counters;
	int chunk_cnt;
};

struct kvs_stats {
	struct kvs_counters counters;
	int chunk_cnt;
};

struct kvv_container_stats {
	struct list_head list;
	int container_id;
	struct kvv_counters counters;
};

struct kvv_stats {
	struct list_head device_list;
	int device_id;
	int	chunk_cnt;
	struct list_head container_list;
};

struct kv_nvmef_stats {
	struct list_head device_list;
	char device_name[MAX_DEV_NAME];
	int	chunk_cnt;
	struct kv_nvme_counters nvme_counters;
};

struct kv_nvme_stats {
	struct list_head device_list;
	char device_name[MAX_DEV_NAME];
	int	chunk_cnt;
	struct kv_nvme_counters nvme_counters;
};

// stats wrap up for host side stacks
struct host_stats {
	//struct list_head job_list;
	//long long job_id;
	pthread_mutex_t	mutex;
	struct kvb_stats kvb_cnt;
	struct kvv_stats kvv_cnt;
	struct kvs_stats kvs_cnt;
	struct kv_nvmef_stats kv_nvmef_cnt;
};

// stats wrap up for target side stacks
struct target_stats {
	//struct list_head job_list;
	//long long job_id;
	struct kv_nvme_stats kv_nvme_cnt;
	pthread_mutex_t	mutex;
};

typedef enum kv_segment {
	kvb_seg = 0,
	kvv_seg = 1,
	kvs_seg = 2,
	kv_nvmef = 3,
	kv_nvme = 4,
	kv_max_seg
} kv_seg_t;

//global stats data share by log service and kv software stack
//#define MAX_CPU	256 //assume max 256 cpu core
int host_nr_cpu = 0; 	// number of cpu, work as host role
int target_nr_cpu = 0; 	// number of cpu work as target

//each kv instance create a host_stats instance and populate the __host_stats array given its own cpu id.
pthread_mutex_t __stats_mutex;
struct host_stats *__host_stats[MAX_CPU] = {0};  // for host side stats

//each kv instance create a target_stats instance and populate the __target_stats array given its own cpu id.
struct target_stats *__target_stats[MAX_CPU] = {0};  //for the target side stats

int kv_stats_service_init = 0;
int init_kv_stats_service(const char *conf_name);

static void *kv_stats_init(int cpu_id, int is_host, const char *conf_name)
{
	void *_stats = NULL;

	pthread_mutex_lock(&__stats_mutex);

	if (!kv_stats_service_init) {
		if (!init_kv_stats_service(conf_name))
			kv_stats_service_init = 1;
	}

	if (is_host && g_serv_context.is_host == 0) { //target role
		DBG("target role defined\n");
		goto done;
	}

	if (!is_host && g_serv_context.is_host == 1) { //host role
		DBG("host role defined\n");
		goto done;
	}

	if (cpu_id < 0 || cpu_id >= MAX_CPU) {
		DBG("invalid cpu_id %d\n", cpu_id);
		goto done;
	}

	if (is_host && __host_stats[cpu_id]) {
		_stats = __host_stats[cpu_id];
		goto done;
	} else if (!is_host && __target_stats[cpu_id]) {
		_stats = __target_stats[cpu_id];
		goto done;
	}

	if (is_host) {
		_stats = calloc(1, sizeof(struct host_stats));
		struct host_stats *h_stats = (struct host_stats *)_stats;
		h_stats->kvb_cnt.chunk_cnt = 0;
		h_stats->kvs_cnt.chunk_cnt = 0;
		h_stats->kvv_cnt.chunk_cnt = 0;
		h_stats->kv_nvmef_cnt.chunk_cnt = 0;
		INIT_LIST_HEAD(&(h_stats->kvv_cnt.device_list));
		INIT_LIST_HEAD(&(h_stats->kvv_cnt.container_list));
		INIT_LIST_HEAD(&(h_stats->kv_nvmef_cnt.device_list));
		g_serv_context.dump_info.host_nr_cpu ++;
		//host_nr_cpu ++;
	} else {
		_stats = calloc(1, sizeof(struct target_stats));
		struct target_stats *t_stats = (struct target_stats *)_stats;
		t_stats->kv_nvme_cnt.chunk_cnt = 0;
		INIT_LIST_HEAD(&(t_stats->kv_nvme_cnt.device_list));
		g_serv_context.dump_info.target_nr_cpu ++;
		//target_nr_cpu ++ ;
	}

	if (!_stats) {
		perror("calloc _stat");
		goto done;
	}

	if (is_host) {
		pthread_mutex_init(&((struct host_stats *)_stats)->mutex, NULL);
		__host_stats[cpu_id] = _stats;
		g_serv_context.is_host = 1;
	} else {
		pthread_mutex_init(&((struct target_stats *)_stats)->mutex, NULL);
		__target_stats[cpu_id] = _stats;
		g_serv_context.is_host = 0;
	}

done:
	pthread_mutex_unlock(&__stats_mutex);
	return _stats;
}

void *kv_stats_init_host(int cpu_id, const char *conf_name)
{
	return (void *) kv_stats_init(cpu_id, 1, conf_name);
}

void *kv_stats_init_target(int cpu_id, const char *conf_name)
{
	return (void *) kv_stats_init(cpu_id, 0, conf_name);
}

struct kvb_counters *kv_stats_get_kvb_counters(void *stats)
{
	if (!stats)
		return NULL;

	((struct host_stats *)stats)->kvb_cnt.chunk_cnt ++;
	return &(((struct host_stats *)stats)->kvb_cnt.counters);
}

static int get_nr_kvb_chunks(void *stats)
{
	struct host_stats *h_stats = (struct host_stats *) stats;
	if (!h_stats)
		return 0;

	return h_stats->kvb_cnt.chunk_cnt;
}

static int get_nr_kvv_chunks(void *stats)
{
	struct host_stats *h_stats = (struct host_stats *) stats;
	if (!h_stats)
		return 0;

	return h_stats->kvv_cnt.chunk_cnt;
}
static int get_nr_kvs_chunks(void *stats)
{
	struct host_stats *h_stats = (struct host_stats *) stats;
	if (!h_stats)
		return 0;

	return h_stats->kvs_cnt.chunk_cnt;
}

static int get_nr_nvmef_chunks(void *stats)
{
	struct host_stats *h_stats = (struct host_stats *) stats;
	if (!h_stats)
		return 0;

	return h_stats->kv_nvmef_cnt.chunk_cnt;
}

static int get_nr_nvme_chunks(void *stats)
{
	struct target_stats *t_stats = (struct target_stats *) stats;
	if (!t_stats)
		return 0;

	return t_stats->kv_nvme_cnt.chunk_cnt;
}

int get_nr_chunks(void *stats, kv_seg_t seg)
{
	int nr_cnts = 0;
	switch (seg) {
	case kvb_seg:
		nr_cnts = get_nr_kvb_chunks(stats);
		break;
	case kvv_seg:
		nr_cnts = get_nr_kvv_chunks(stats);
		break;
	case kvs_seg:
		nr_cnts = get_nr_kvs_chunks(stats);
		break;
	case kv_nvmef:
		nr_cnts = get_nr_nvmef_chunks(stats);
		break;
	case kv_nvme :
		nr_cnts = get_nr_nvme_chunks(stats);
		break;
	default:
		nr_cnts = -1;
	}
	return nr_cnts;
}


/* to avoid the size check, ensure to provide large enough buffer for all the dumps*/
static int dump_kvb_counters(void *stats, char *buffer, int size)
{
	char *p = buffer;
	struct host_stats *h_stats = (struct host_stats *)stats;

	if (!h_stats->kvb_cnt.chunk_cnt) {
		DBG("No kvb counters\n");
		return 0;
	}

	if (size < sizeof(struct kvb_counters))
		return 0;

	* (long long *) buffer = ATOMIC_READ(h_stats->kvb_cnt.counters.nr_keys);
	buffer += 8;
	* (long long *) buffer = ATOMIC_READ(h_stats->kvb_cnt.counters.nr_get);
	buffer += 8;
	* (long long *) buffer = ATOMIC_READ(h_stats->kvb_cnt.counters.nr_put);
	buffer += 8;
	* (long long *) buffer = ATOMIC_READ(h_stats->kvb_cnt.counters.nr_del);
	buffer += 8;
	* (long long *) buffer = ATOMIC_READ(h_stats->kvb_cnt.counters.nr_fail);
	buffer += 8;

	return (buffer - p);
}

static int dump_kvv_counters(void *stats, char *buffer, int size)
{
	struct host_stats *h_stats = (struct host_stats *) stats;
	struct kvv_stats *kvv_d_s = NULL;
	struct kvv_container_stats *kvv_c_s = NULL;
	char *p = buffer;

	int nr_cnt = get_nr_kvv_chunks(stats);
	if (!nr_cnt) {
		DBG("No kvv counters\n");
		return 0;
	}

	if (size < nr_cnt  * (sizeof(struct kvv_counters) + 16)) {
		DBG("dump_kvv_counters: not enough buffer %d min %lu\n",
		    size, nr_cnt  * (sizeof(struct kvv_counters) + 16));
		return 0;
	}

	list_for_each_entry(kvv_d_s, &h_stats->kvv_cnt.device_list, device_list) {
		list_for_each_entry(kvv_c_s, &kvv_d_s->container_list, list) {
			* (long long *) buffer = kvv_d_s->device_id;
			buffer += 8;
			* (long long *) buffer = kvv_c_s->container_id;
			buffer += 8;
			* (long long *) buffer = ATOMIC_READ(kvv_c_s->counters.nr_sync_get);
			buffer += 8;
			* (long long *) buffer = ATOMIC_READ(kvv_c_s->counters.nr_async_get);
			buffer += 8;
			* (long long *) buffer = ATOMIC_READ(kvv_c_s->counters.nr_sync_put);
			buffer += 8;
			* (long long *) buffer = ATOMIC_READ(kvv_c_s->counters.nr_async_put);
			buffer += 8;
			* (long long *) buffer = ATOMIC_READ(kvv_c_s->counters.nr_del);
			buffer += 8;
			* (long long *) buffer = ATOMIC_READ(kvv_c_s->counters.nr_fail);
			buffer += 8;
		}
	}
	return (buffer - p);
}

static int dump_kvs_counters(void *stats, char *buffer, int size)
{
	char *p = buffer;
	struct host_stats *h_stats = (struct host_stats *)stats;

	if (!h_stats->kvs_cnt.chunk_cnt) {
		DBG("No kvs counters\n");
		return 0;
	}

	if (size < sizeof(struct kvs_counters)) {
		DBG("dump_kvs_counters: not enough buffer %d min %lu\n",
		    size, sizeof(struct kvs_counters));
		return 0;
	}

	* (long long *) buffer = ATOMIC_READ(h_stats->kvs_cnt.counters.nr_keys);
	buffer += 8;
	* (long long *) buffer = ATOMIC_READ(h_stats->kvs_cnt.counters.nr_get);
	buffer += 8;
	* (long long *) buffer = ATOMIC_READ(h_stats->kvs_cnt.counters.nr_put);
	buffer += 8;
	* (long long *) buffer = ATOMIC_READ(h_stats->kvs_cnt.counters.nr_del);
	buffer += 8;
	* (long long *) buffer = ATOMIC_READ(h_stats->kvs_cnt.counters.nr_fail);
	buffer += 8;

	return (buffer - p);
}

static int dump_nvmef_counters(void *stats, char *buffer, int size)
{
	struct host_stats *h_stats = (struct host_stats *) stats;
	struct kv_nvmef_stats *nvmef = NULL;
	char *p = buffer ;

	int cnt = get_nr_nvmef_chunks(stats);
	if (!cnt) {
		DBG("No nvmef counters\n");
		return 0;
	}

	if (size < (sizeof(struct kv_nvme_counters) + MAX_DEV_NAME)* cnt) {
		DBG("dump_nvmef_counters: not enough buffer %d min %lu\n",
		    size, (sizeof(struct kv_nvme_counters) + MAX_DEV_NAME) * cnt);
		return 0;
	}

	list_for_each_entry(nvmef, &h_stats->kv_nvmef_cnt.device_list, device_list) {
		memcpy(buffer, nvmef->device_name, MAX_DEV_NAME);
		buffer += MAX_DEV_NAME;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.max_qd);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.current_qd);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.nr_get);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.nr_put);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.nr_16b_less);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.nr_16k_1k);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.nr_1k_4k);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.nr_4k_32k);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.nr_32k_1m);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.other_size);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvmef->nvme_counters.nr_failure);
		buffer += 8;

	}

	return (buffer - p);
}

static int dump_nvme_counters(void *stats, char *buffer, int size)
{
	struct target_stats *t_stats = (struct target_stats *) stats;
	struct kv_nvme_stats *nvme = NULL;
	char *p = buffer ;

	int cnt = get_nr_nvme_chunks(stats);
	if (!cnt) {
		DBG("No nvme counters\n");
		return 0;
	}

	if (size < (sizeof(struct kv_nvme_counters) + MAX_DEV_NAME)* cnt) {
		DBG("dump_nvme_counters: not enough buffer %d min %lu\n",
		    size, (sizeof(struct kv_nvme_counters) + MAX_DEV_NAME) * cnt);
		return 0;
	}

	list_for_each_entry(nvme, &t_stats->kv_nvme_cnt.device_list, device_list) {
		memcpy(buffer, nvme->device_name, MAX_DEV_NAME);
		buffer += MAX_DEV_NAME;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.max_qd);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.current_qd);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.nr_get);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.nr_put);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.nr_16b_less);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.nr_16k_1k);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.nr_1k_4k);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.nr_4k_32k);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.nr_32k_1m);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.other_size);
		buffer += 8;
		* (long long *) buffer = ATOMIC_READ(nvme->nvme_counters.nr_failure);
		buffer += 8;

	}

	return (buffer - p);
}

int dump_counters(void *stats, kv_seg_t seg, char *buffer, int size)
{
	int sz = 0;

	if (seg >= kv_max_seg) {
		DBG("invalid segment %d\n", seg);
		return 0;
	}

	switch (seg) {
	case kvb_seg:
		sz = dump_kvb_counters(stats, buffer, size);
		break;
	case kvv_seg:
		sz = dump_kvv_counters(stats, buffer, size);
		break;
	case kvs_seg:
		sz = dump_kvs_counters(stats, buffer, size);
		break;
	case kv_nvmef:
		sz = dump_nvmef_counters(stats, buffer, size);
		break;
	case kv_nvme :
		sz = dump_nvme_counters(stats, buffer, size);
		break;
	default:
		sz = 0;
	}

	return sz;
}

struct kvv_counters *kv_stats_get_kvv_counters(void *stats, int dev_id, int container_id)
{
	if (!stats)
		return NULL;

	struct host_stats *h_stats = (struct host_stats *) stats;
	struct kvv_counters *ret = NULL;
	struct kvv_container_stats *kvv_c_s = NULL;
	struct kvv_stats *kvv_d_s = &h_stats->kvv_cnt;
	struct list_head *d_list = NULL, * c_list = NULL;
	int dev_found = 0;
	//int container_found = 0;

	pthread_mutex_lock(&h_stats->mutex);

	list_for_each(d_list, &h_stats->kvv_cnt.device_list) {
		struct kvv_stats *kvv_d_s = container_of(d_list, struct kvv_stats, device_list);
		if (kvv_d_s->device_id == dev_id) {
			dev_found = 1;
			list_for_each(c_list, &kvv_d_s->container_list) {
				kvv_c_s = container_of(c_list, struct kvv_container_stats, list);
				if (kvv_c_s->container_id == container_id) {
					//container_found = 1;
					ret = &kvv_c_s->counters;
					goto found;
				}
			}
			//new container to add
			kvv_c_s = calloc(1, sizeof(struct kvv_container_stats));
			kvv_c_s->container_id = container_id;
			list_add_tail(&kvv_c_s->list, &kvv_d_s->container_list);
			ret = &kvv_c_s->counters;
			h_stats->kvv_cnt.chunk_cnt ++;
			goto found;
		}
	}

	if (!dev_found) {
		kvv_c_s = calloc(1, sizeof(struct kvv_container_stats));
		kvv_c_s->container_id = container_id;
		kvv_d_s = calloc(1, sizeof(struct kvv_stats));
		kvv_d_s->device_id = dev_id;
		INIT_LIST_HEAD(&kvv_d_s->container_list);
		list_add_tail(&kvv_c_s->list, &kvv_d_s->container_list);
		list_add_tail(&kvv_d_s->device_list, &h_stats->kvv_cnt.device_list);

		h_stats->kvv_cnt.chunk_cnt ++;
		ret =  &kvv_c_s->counters;
	}

found:
	pthread_mutex_unlock(&h_stats->mutex);
	return ret;

}

struct kvs_counters *kv_stats_get_kvs_counters(void *stats)
{
	if (!stats)
		return NULL;

	((struct host_stats *)stats)->kvs_cnt.chunk_cnt ++;
	return &(((struct host_stats *)stats)->kvs_cnt.counters);
}

static struct kv_nvme_counters *kv_stats_get_nvme_counters(void *stats,
		int is_host, const char *device_name, int len)
{
	if (!stats)
		return NULL;

	struct kv_nvme_counters *ret = NULL;
	//struct kv_nvmef_stats * nvmef_s = NULL;
	struct kv_nvme_stats *nvme_s = NULL;
	struct list_head *dev_head = NULL;
	struct host_stats *h_stats = (is_host) ? stats : NULL;
	struct target_stats *t_stats = (is_host) ? NULL : stats;

	if (!device_name || len >= MAX_DEV_NAME)
		return NULL;

	if (is_host) {
		pthread_mutex_lock(&h_stats->mutex);
		dev_head =  &h_stats->kv_nvmef_cnt.device_list;
	} else {
		pthread_mutex_lock(&t_stats->mutex);
		dev_head =  &t_stats->kv_nvme_cnt.device_list;
	}

	list_for_each_entry(nvme_s, dev_head, device_list) {
		if (strlen(nvme_s->device_name) == len
		    && !strncmp(nvme_s->device_name, device_name, len)) {
			//found device by name
			ret = &nvme_s->nvme_counters;
			goto found;
		}
	}

	nvme_s = calloc(1, sizeof(struct kv_nvme_stats));
	strncpy(nvme_s->device_name, device_name, len);
	list_add_tail(&nvme_s->device_list, dev_head);
	ret = &nvme_s->nvme_counters;

	if (is_host) {
		h_stats->kv_nvmef_cnt.chunk_cnt ++;
	} else {
		t_stats->kv_nvme_cnt.chunk_cnt ++;
		DBG("kv_nvme_cnt.chunk_cnt %d\n", t_stats->kv_nvme_cnt.chunk_cnt);
	}

found:

	if (is_host) {
		pthread_mutex_unlock(&h_stats->mutex);
	} else {
		pthread_mutex_unlock(&t_stats->mutex);
	}

	return ret;
}

struct kv_nvme_counters *kv_stats_get_kv_nvmef_counters(
	void *stats, const char *device_name, int len)
{
	return kv_stats_get_nvme_counters(stats, 1, device_name, len);
}


struct kv_nvme_counters *kv_stats_get_kv_nvme_counters(
	void *stats, const char *device_name, int len)
{
	return kv_stats_get_nvme_counters(stats, 0, device_name, len);
}

/* conf file info

// buffer size in unit of MB
buffer_size = 128

// sampling frequence in unit of seconds
sample_interval = 5

// dump file size in unit of KB
dump_file_size = 1024

//dump sample data alignment in unit of KB
sample_alignment = 4

//file system path for dump
dump_storage_path = "/tmp/"

*/

#define BUFFER_SIZE_DEFAULT			64	//MB
#define SAMPLE_INTERVAL_DEFAULT		5		//SEC
#define SAMPLE_ALIGNMENT_DEFAULT	4
#define DUMP_FILE_SIZE_DEFAULT		256 	//KB
#define DUMP_ROOT_PATH_DEFAULT		"/tmp/stats"
#define ONE_FILE_SIZE_DEFAULT 		64		//Bytes
#define LOG_FILE_SIZE_DEFAULT 		64		//KB
#define LOG_ROOT_PATH_DEFAULT 		"/tmp/log"

int setup_timer(service_context_t *ctx);

service_context_t *get_serv_context()
{
	return &g_serv_context;
}

int prepare_sample_header(file_header_t *file_head,
			  segment_cnt_table_t *seg_cnt_table, int nr_segment)
{
	//initial the sample head
	file_head->version = KV_STATS_VERSION;
	file_head->nr_seg = nr_segment;
	file_head->segs = (segment_info_t *)calloc(nr_segment, sizeof(segment_info_t));

	//fill in the counter definitions
	int seg_idx = 0;
	int cnt_idx = 0;
	//int cpu_idx = 0;
	//int chunk_idx = 0;
	int nr_cnt = 0;
	segment_info_t *seg_info = NULL;
	counter_info_t *cnt_info = NULL;

	for (seg_idx = 0 ; seg_idx < nr_segment; seg_idx ++) {
		nr_cnt = seg_cnt_table[seg_idx].nr_cnt ;

		seg_info = &file_head->segs[seg_idx];
		seg_info->nr_cnt = nr_cnt;
		seg_info->cnt_info = (counter_info_t *)calloc(nr_cnt, sizeof(counter_info_t));

		kv_stats_counters_table_t *cnt_table = seg_cnt_table[seg_idx].cnt_table;
		DBG("seg_idx %d nr_cnt %d\n", seg_idx, nr_cnt);

		for (cnt_idx = 0; cnt_idx < nr_cnt; cnt_idx++) {
			cnt_info = &seg_info->cnt_info[cnt_idx];
			memcpy(cnt_info->name, cnt_table[cnt_idx].counter_name, 32);
			cnt_info->type = cnt_table[cnt_idx].type;
			DBG("%s %d\n", cnt_info->name, cnt_info->type);
		}
	}

	return 0;
}

int dump_sample_meta(int nr_cpu, char **pbuffer, int *psize)
{
	sample_meta_t meta;
	int sz = sizeof(sample_meta_t);
	int size = * psize;
	char *buffer = * pbuffer;

	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		perror("gettimeofday");
		return -1;
	}

	DBG("dump_sample_meta: buffer %p size %d\n", buffer, size);

	meta.timestamp_sec = tv.tv_sec;
	meta.timestamp_usec = tv.tv_usec;
	meta.nr_cpu = nr_cpu; //sysconf(_SC_NPROCESSORS_ONLN);
	meta.size = sz;
	if (size < sz) {
		DBG("less buffer for sample_meta_t %d %d", size, sz);
		return -1;
	}

	memcpy(buffer, (char *)&meta, sz);
	size -= sz;
	buffer += sz;

	* psize = size;
	* pbuffer = buffer;

	return sz;

}

int dump_sample_data(int nr_cpu, int nr_seg, char **pbuffer, int *psize, int is_host)
{
	//populate the task_meta_s
	task_meta_t one_task_meta;
	void *h_stats = NULL;
	int cpu_idx = 0;
	int cpu_cnt = 0;
	//int t_meta_sz = 0; 	// size of tast_meta
	int t_sz = 0; 		// size of the whole task counters (task_meta + counters)
	int t_sample_sz = 0;// size of one sampling data (task counters of nr_cpu)
	int size = * psize;
	char *buffer = * pbuffer;

	DBG("dump_sample_data: nr_cpu %d nr_seg %d, buffer %p size %d\n", nr_cpu, nr_seg, buffer, size);

	for (cpu_idx = 0;  cpu_idx < MAX_CPU && cpu_cnt < nr_cpu; cpu_idx ++) {

		if (is_host && !__host_stats[cpu_idx])
			continue;

		if (!is_host && !__target_stats[cpu_idx])
			continue;

		cpu_cnt ++;
		if (is_host) {
			h_stats = __host_stats[cpu_idx];
		} else {
			h_stats = __target_stats[cpu_idx];
		}

		//dump the sample meta here
		one_task_meta.cpu_id = cpu_idx;
		one_task_meta.nr_seg = nr_seg;

		t_sz = offsetof(task_meta_t, chunks_per_segment);
		int seg_idx = 0;
		for (seg_idx = 0; seg_idx < nr_seg; seg_idx++) {
			if (is_host) {
				one_task_meta.chunks_per_segment[seg_idx] = get_nr_chunks(h_stats, seg_idx);
				DBG("cpu_idx %d get_nr_chunks[%d] = %d\n", cpu_idx, seg_idx, get_nr_chunks(h_stats, seg_idx));
			} else {
				one_task_meta.chunks_per_segment[seg_idx] = get_nr_chunks(h_stats, kv_nvme);
				DBG("cpu_idx %d get_nr_chunks[%d] = %d\n", cpu_idx, seg_idx, get_nr_chunks(h_stats, kv_nvme));
			}

		}

		t_sz += seg_idx * sizeof(int);
		if (t_sz > size) {
			DBG("not enough buffer for smaple_meta_t\n");
			return -1;
		}

		//dump the task_meta
		memcpy(buffer, &one_task_meta, t_sz);

		buffer += t_sz;
		size -= t_sz;
		//dump the sample data here
		for (seg_idx = 0; seg_idx < nr_seg; seg_idx ++) {
			int sz = 0;
			if (is_host) {
				sz = dump_counters(h_stats, seg_idx, buffer, size);
			} else {
				sz = dump_counters(h_stats, kv_nvme, buffer, size);
			}

			if (sz >= 0) {
				buffer += sz;
				t_sz += sz;
				size -= sz;
			} else {
				return -2;
			}
		}

		DBG("dump_sample_data: data dumped %d\n", t_sz);
		t_sample_sz += t_sz;

	}

	* psize -= t_sample_sz;
	* pbuffer += t_sample_sz;

	return t_sample_sz ;

}

int dump_data(int fd, char *buffer, int size)
{
	//write to the dump file
	int sz = write(fd, buffer, size);
	if (sz < size) {
		DBG("write errno %d expected %d, sz %d\n", errno, size, sz);
	}

	return sz;
}

int update_file_size_for_dump(service_context_t *ctx)
{
	file_header_t *hdr = (file_header_t *)ctx->dump_info.file_hdr_dump_ptr;
	hdr->reserved[0] = ctx->dump_info.dump_file_size;
	DBG("update_file_size_for_dump: hdr %p size %d\n", hdr, hdr->reserved[0]);
	return hdr->reserved[0];
}

void exchange_log_buffer(service_context_t *ctx)
{
	void *logging_buf = NULL;
	if (ctx->log_dumper_buffer == ctx->log_buffer[0]) {
		logging_buf = ctx->log_buffer[1];
	} else {
		logging_buf = ctx->log_buffer[0];
	}

	ctx->logging_buffer_orig = ctx->log_dumper_buffer;
	ctx->logging_buffer = ctx->log_dumper_buffer;
	ctx->log_dumper_buffer = logging_buf;
	ctx->cur_nr_log = 0;
}

void exchange_buffer(service_context_t *ctx)
{
	void *sampling_buf = NULL;
	if (ctx->dumping_buffer == ctx->data_buffer[0]) {
		sampling_buf = ctx->data_buffer[1];
	} else {
		sampling_buf = ctx->data_buffer[0];
	}

	update_file_size_for_dump(ctx);

	ctx->sampling_buffer = ctx->dumping_buffer;
	ctx->dumping_buffer = sampling_buf;
	ctx->sampling_buffer_size = ctx->conf.conf_buffer_size_unit / 2;
	ctx->dump_info.file_hdr_dump_ptr = ctx->sampling_buffer;
	ctx->dump_info.dump_file_size = ctx->dump_info.file_hdr_size;

	dump_file_header(&ctx->dump_info.file_hdr,
			 (char **)&ctx->sampling_buffer, &ctx->sampling_buffer_size);

}

void one_sampling(int signo)
{
	int nr_cpu;
	int nr_seg;
	char *buffer;
	int wait_for_dumper = 0;
	static int nr_sample = 0;
	service_context_t *ctx = get_serv_context();

	if (ctx->thread_terminate) {

		//ctx->sample_data_is_full = 1;
		file_header_t *hdr = (file_header_t *)ctx->dump_info.file_hdr_dump_ptr;
		hdr->reserved[0] = ctx->dump_info.dump_file_size;

		pthread_cond_signal(&ctx->cond_var);
		return;
	}

	if (ctx->is_host == -1) //not counter access yet.
		goto done;

	int buffer_shorage = 0;
	nr_cpu = ctx->is_host ? ctx->dump_info.host_nr_cpu : ctx->dump_info.target_nr_cpu;
	nr_seg = ctx->dump_info.nr_segment;
	buffer = ctx->sampling_buffer;

	DBG("one_sampling: is_host %d nr_cpu %d nr_seg %d dump_file_size %d sampling_buffer_size %d\n",
	    ctx->is_host, nr_cpu, nr_seg, ctx->dump_info.dump_file_size, ctx->sampling_buffer_size);

x_buffer:

	//TBD: check if the buffer is full, if full, exchange buffer and trigger the dump procedure.
	if ((ctx->conf.conf_max_sample_per_dump && nr_sample >= ctx->conf.conf_max_sample_per_dump)
	    || buffer_shorage || ctx->sampling_buffer_size < (PAGE_SIZE * 128)) {

		wait_for_dumper = 0;
		pthread_mutex_lock(&ctx->buffer_mutex);
		if (ctx->dumper_is_ready) {
			exchange_buffer(ctx);
			ctx->sample_data_is_full = 1;
			pthread_cond_signal(&ctx->cond_var);
		} else {
			wait_for_dumper = 1;
			DBG("stats dump is not ready yet! thread_terminate %d\n", ctx->thread_terminate);
		}
		nr_sample = 0;
		pthread_mutex_unlock(&g_serv_context.buffer_mutex);

		if (wait_for_dumper) {
			if (!ctx->thread_terminate) {
				wait_for_dumper = 0;
				goto done;
			} else {
				return; //thread_terminate
			}
		}

	}

	int meta_sz = dump_sample_meta(nr_cpu, (char **)&ctx->sampling_buffer, &ctx->sampling_buffer_size);
	DBG("sample meta size %d\n", meta_sz);
	if (meta_sz < 0) {
		DBG("dump_sample_meta failed buffer_size %d\n", ctx->sampling_buffer_size);
		buffer_shorage = 1;
		goto x_buffer;
	}

	//dump sample meta and data
	int data_sz = dump_sample_data(nr_cpu, nr_seg, (char **)&ctx->sampling_buffer,
				       &ctx->sampling_buffer_size, ctx->is_host);
	DBG("sample data size %d\n", data_sz);
	if (data_sz < 0) {
		DBG("dump_sample_data failed buffer_size %d\n", ctx->sampling_buffer_size);
		buffer_shorage = 1;
		goto x_buffer;
	}

	nr_sample ++;
	// sample_meta.size include the whole sample ("meta:[data]+" )
	* (int *)(buffer + offsetof(sample_meta_t, size)) = meta_sz + data_sz;

	ctx->dump_info.dump_file_size += (meta_sz + data_sz);

	//check if the current dump data is enough for a single dump file
	if (ctx->dump_info.dump_file_size >= ctx->conf.conf_dump_file_size) {

		//set the dump file size in file header
		//prepare for the next dump file by aligning dump_buffer to next PAGE
		int padding = KB - ctx->dump_info.dump_file_size % KB ;

		if (padding != KB) {
			ctx->sampling_buffer += padding;
			ctx->sampling_buffer_size -= padding;
		}

		file_header_t *hdr = (file_header_t *)ctx->dump_info.file_hdr_dump_ptr;
		hdr->reserved[0] = ctx->dump_info.dump_file_size;
		DBG("one_sampling: hdr %p file_size %d padding %d\n", hdr, hdr->reserved[0], padding);

		if (ctx->sampling_buffer_size >= (PAGE_SIZE * 128)) {
			ctx->dump_info.file_hdr_dump_ptr = ctx->sampling_buffer;
			ctx->dump_info.dump_file_size = ctx->dump_info.file_hdr_size;
			dump_file_header(&ctx->dump_info.file_hdr, (char **)&ctx->sampling_buffer,
					 &ctx->sampling_buffer_size);
		}
	}

done:
	setup_timer(ctx);

	//return meta_sz + data_sz;
}

void stop_sampling()
{
	g_serv_context.thread_terminate = 1;
}

int setup_timer(service_context_t *ctx)
{
	struct itimerval tval;
	if (!ctx->thread_terminate) {

		timerclear(& tval.it_interval);	/* zero interval means no reset of timer */
		timerclear(& tval.it_value);
		tval.it_value.tv_sec = ctx->conf.conf_sample_interval;
		signal(SIGALRM, one_sampling);
		setitimer(ITIMER_REAL, &tval, NULL);
	}
	return 0;
}

static int _mkdir(const char *dir)
{
	char tmp[256];
	char *p = NULL;
	size_t len;
	int rc = 0;

	snprintf(tmp, sizeof(tmp), "%s", dir);
	len = strlen(tmp);
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;
	for (p = tmp + 1; *p; p++)
		if (*p == '/') {
			*p = 0;
			rc = mkdir(tmp, 0766);//S_IRWXU);
			*p = '/';
			if (rc && errno != EEXIST) {
				DBG("mkdir %s fail with %d\n", tmp, errno);
				return rc;
			}
		}
	rc = mkdir(tmp, 0766);
	if (rc && errno != EEXIST) {
		perror("mkdir");
		DBG("mkdir %s fail with %d\n", tmp, errno);
		return rc;
	}
	return 0;
}

int prepare_dump_fs(service_context_t *context, int is_log)
{
	const char *root_path = (is_log) ? context->conf.conf_log_storage_path :
				context->conf.conf_stats_storage_path;
	pid_t pid = getpid();
	char *dir_path = (is_log) ? context->current_log_dir_path : context->current_stats_dir_path;

	snprintf(dir_path, PAGE_SIZE - 1, "%s/%d/%d/%d/%d/%d/", root_path, pid,
		 context->date[STATS_YEA], context->date[STATS_MON], context->date[STATS_DAY],
		 context->date[STATS_HOU]);

	return _mkdir((const char *)dir_path);

}
int get_current_date(service_context_t *context)
{
	time_t epoch_tm;
	struct tm date_tm;
	time(&epoch_tm);

	if (!localtime_r(&epoch_tm, &date_tm))
		return -1;

	context->date[STATS_YEA] = date_tm.tm_year + 1900;
	context->date[STATS_MON] = date_tm.tm_mon + 1;
	context->date[STATS_DAY] = date_tm.tm_mday;
	context->date[STATS_HOU] = date_tm.tm_hour;

	return epoch_tm;

}

//return the current log level.
int get_log_level()
{
	return g_log_level;

}

//set the log level, return the old level if success, or -1 if wrong range.
int set_log_level(int level)
{
	int ret = g_log_level;
	if (level < LOG_LEVEL_MAX && level > LOG_LEVEL_0)
		g_log_level = level;
	else
		ret = -1;

	return ret;

}

//read the conf file for stat/log paramenters.
int get_conf_info(service_context_t *context, const char *conf_name)
{

	config_t cfg;
	//config_setting_t *setting;
	const char *str = NULL;
	stats_conf_info_t *conf_info = &context->conf;
	int i_val = 0;

	config_init(&cfg);

	if (! config_read_file(&cfg, conf_name)) {
		DBG("%s:%d - %s\n", config_error_file(&cfg),
		    config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	if (config_lookup_string(&cfg, "debug_info", &str)) {
		strncpy(conf_info->conf_debug_info, str, strlen(str));
		context->debug_info_fd = fopen(conf_info->conf_debug_info, "w+");
		if (context->debug_info_fd < 0) {
			context->debug_info_fd = stderr;
		}
		DBG("debug_info: %s\n\n", conf_info->conf_debug_info);
	} else {
		debug_lib = 0;
	}

	/* Get the buffer_size in mb, default 64 MB. min 4 MB */
	if (config_lookup_int(&cfg, "buffer_size", &conf_info->conf_buffer_size_unit)) {
		if (CONF_BUFFER_MIN_SIZE > conf_info->conf_buffer_size_unit) {
			DBG("buffer_size %d MB less than min %d MB\n",
			    conf_info->conf_buffer_size_unit, CONF_BUFFER_MIN_SIZE);
			exit(1);
		} else {
			DBG("buffer_size : %d MB\n", conf_info->conf_buffer_size_unit);
			conf_info->conf_buffer_size_unit *= CONF_BUFFER_SIZE_UNIT;
		}
	} else {
		DBG("can not find the buffer_size");
		conf_info->conf_buffer_size_unit = BUFFER_SIZE_DEFAULT * CONF_BUFFER_SIZE_UNIT;
	}

	/* Get the sample interval, default 5 secs */
	if (config_lookup_int(&cfg, "sample_interval", &conf_info->conf_sample_interval)) {
		DBG("sample_interval : %d sec\n", conf_info->conf_sample_interval);
	} else {
		DBG("can not find the sample_interval\n");
		conf_info->conf_sample_interval = SAMPLE_INTERVAL_DEFAULT;
	}

	/* Get the sample interval, default 5 secs */
	if (config_lookup_int(&cfg, "dump_file_size", &conf_info->conf_dump_file_size)) {
		conf_info->conf_dump_file_size *= KB;
		DBG("dump_file_size : %d Bytes\n", conf_info->conf_dump_file_size);
	} else {
		DBG("can not find the dump_file_size\n");
		conf_info->conf_dump_file_size = DUMP_FILE_SIZE_DEFAULT * KB;
	}

	/* Get the sample interval, default 5 secs */
	if (config_lookup_int(&cfg, "log_file_size", &conf_info->conf_log_file_size)) {
		conf_info->conf_log_file_size *= KB;
		DBG("log_file_size : %d Bytes\n", conf_info->conf_log_file_size);
	} else {
		DBG("can not find the log_file_size\n");
		conf_info->conf_log_file_size = LOG_FILE_SIZE_DEFAULT * KB;
	}

	/* Get the sample interval, default 5 secs */
	if (config_lookup_int(&cfg, "max_sample_per_dump", &conf_info->conf_max_sample_per_dump)) {
		DBG("max_sample_per_dump : %d Bytes\n", conf_info->conf_max_sample_per_dump);
	} else {
		DBG("can not find the max_sample_per_dump\n");
		conf_info->conf_max_sample_per_dump = 0;
	}

	/* Get the sample interval, default 5 secs */
	if (config_lookup_int(&cfg, "one_log_size", &conf_info->conf_one_log_size)) {
		DBG("one_log_size : %d Bytes\n", conf_info->conf_one_log_size);
	} else {
		DBG("can not find the one_log_size\n");
		conf_info->conf_one_log_size = ONE_LOG_SIZE_DEFAULT;
	}

	/* Get the sample interval, default 5 secs */
	if (config_lookup_int(&cfg, "log_level", &i_val)) {
		g_log_level = i_val;
		DBG("log_level : %d Bytes\n", g_log_level);
	} else {
		DBG("can not find the log level\n");
		g_log_level = LOG_LEVEL_DEFAULT;
	}

	/* Get the sample interval, default 5 secs */
	if (config_lookup_int(&cfg, "is_host", &i_val)) {
		context->is_host = i_val;
		DBG(" is host : %d \n", g_log_level);
	} else {
		DBG("can not find the role, host as default\n");
		context->is_host = 1;
	}

	if (config_lookup_string(&cfg, "log_storage_path", &str)) {
		strncpy(conf_info->conf_log_storage_path, str, strlen(str));
		DBG("Log Store path: %s\n\n", conf_info->conf_log_storage_path);
	} else {
		DBG("No 'dump_storage_path' setting in configuration file.\n");
		snprintf(conf_info->conf_log_storage_path, strlen(LOG_ROOT_PATH_DEFAULT) + 1,
			 "%s", LOG_ROOT_PATH_DEFAULT);
	}

	if (config_lookup_string(&cfg, "dump_storage_path", &str)) {
		strncpy(conf_info->conf_stats_storage_path, str, strlen(str));
		DBG("Stats Store path: %s\n\n", conf_info->conf_stats_storage_path);
	} else {
		DBG("No 'dump_storage_path' setting in configuration file.\n");
		snprintf(conf_info->conf_stats_storage_path, strlen(DUMP_ROOT_PATH_DEFAULT) + 1,
			 "%s", DUMP_ROOT_PATH_DEFAULT);
	}


	config_destroy(&cfg);
	return 0;
}

int dump_file_header(file_header_t *sample_head, char **buffer, int *psize)
{
	char *p = * buffer;

	int nr_seg = sample_head->nr_seg;
	int seg_idx = 0;
	int cnt_idx = 0;
	int size = * psize;

	segment_info_t *seg_info = NULL;
	counter_info_t *cnt_info = NULL;

	DBG("dump_file_header: buffer %p size %d\n", *buffer, size);

	int sz = offsetof(file_header_t, segs);
	if (sz > size)
		return -1;

	memcpy(p, (char *)sample_head, sz);

	//TBD: check the buffer overflow
	p += sz;
	size -= sz;
	for (seg_idx = 0; seg_idx < nr_seg; seg_idx ++) {
		seg_info = &sample_head->segs[seg_idx];
		*(int *)p = seg_info->nr_cnt;
		p += sizeof(int);

		for (cnt_idx = 0; cnt_idx < seg_info->nr_cnt;  cnt_idx ++) {
			cnt_info = &seg_info->cnt_info[cnt_idx];
			if (size < CNT_NAME_SZ)
				return -2;

			//copy the counter name
			memcpy(p, cnt_info->name, CNT_NAME_SZ);
			size -= CNT_NAME_SZ;
			p += CNT_NAME_SZ;

			//copy the counter data type
			if (size < (sizeof(int)))
				return -3;

			*(int *)p = cnt_info->type;
			p += sizeof(int);
			size -= sizeof(int);
		}
	}


	size = p - * buffer;
	* psize -= size;
	* buffer = p;

	return size;
}

void kv_stats_table_init(service_context_t *ctx)
{
	int nr_seg = 0;
	int rc = 0;
	segment_cnt_table_t *p_seg_table = ctx->dump_info.segment_cnt_table;

	if (ctx->is_host) {
		p_seg_table[nr_seg].nr_cnt = sizeof(kvb_cnt_table) / sizeof(kv_stats_counters_table_t);
		p_seg_table[nr_seg ++].cnt_table = kvb_cnt_table;
		p_seg_table[nr_seg ].nr_cnt = sizeof(kvv_cnt_table) / sizeof(kv_stats_counters_table_t);
		p_seg_table[nr_seg ++].cnt_table = kvv_cnt_table;
		p_seg_table[nr_seg ].nr_cnt = sizeof(kvs_cnt_table) / sizeof(kv_stats_counters_table_t);
		p_seg_table[nr_seg ++].cnt_table = kvs_cnt_table;
	}
	p_seg_table[nr_seg ].nr_cnt = sizeof(kv_nvme_cnt_table) / sizeof(kv_stats_counters_table_t);
	p_seg_table[nr_seg ++].cnt_table = kv_nvme_cnt_table;

	ctx->dump_info.nr_segment = nr_seg;
	//file_header_t file_header;
	prepare_sample_header(&ctx->dump_info.file_hdr, p_seg_table, nr_seg);
	ctx->dump_info.file_hdr_dump_ptr = ctx->sampling_buffer;
	rc = dump_file_header(&ctx->dump_info.file_hdr, (char **)&ctx->sampling_buffer,
			      &ctx->sampling_buffer_size);
	if (rc < 0) {
		DBG("dump_file_header faile with rc %d buffer_size %d\n", rc, ctx->sampling_buffer_size);
		//less buffer size for header
	}
	//initial the next dump file size with file hdr size
	ctx->dump_info.dump_file_size = rc;
	ctx->dump_info.file_hdr_size = rc;
	ctx->dump_info.file_hdr.reserved[0] = rc;

}

void kv_stats_table_deinit(service_context_t *ctx)
{
	file_header_t *file_head = &ctx->dump_info.file_hdr;
	int nr_segment = ctx->dump_info.nr_segment;
	int seg_idx = 0;
	segment_info_t *seg_info = NULL;

	for (seg_idx = 0; seg_idx < nr_segment; seg_idx ++) {
		seg_info = &file_head->segs[seg_idx];
		if (seg_info->cnt_info) {
			free(seg_info->cnt_info);
		}
	}

	free(file_head->segs);
	file_head->segs = NULL;
	file_head->nr_seg = 0;
}

void *kv_stats_sampling_proc(void *context)
{
	service_context_t *ctx  = (service_context_t *) context;
	setup_timer(ctx);
	while (!ctx->thread_terminate) {
		sleep(1);
	}
	DBG("kv_stats_sampling_proc exit\n");
	return ctx;
}

int kv_log_dump_buffer(service_context_t *ctx, const char *buffer, int size)
{
	int log_file_size = ctx->conf.conf_log_file_size;
	int	one_log_size = ctx->conf.conf_one_log_size;
	int msg_cnt = size / one_log_size;
	char file_path[PAGE_SIZE];
	int file_size = 0;
	int total_size = 0;
	int cur_time = get_current_date(ctx);
	cur_time --;
	FILE *fd = NULL;
	while (msg_cnt --) {

		if (!fd) {
			cur_time ++;
			prepare_dump_fs(ctx, 1);
			snprintf(file_path, PAGE_SIZE, "%s%ld", ctx->current_log_dir_path, (long)cur_time);
			DBG("log_dump_buffer: %s\n", file_path);
			fd = fopen(file_path, "a+");
			file_size = 0;
		}

		if (!fd) {
			DBG(" fail to open log file %s, rc %d\n", file_path, errno);
			sleep(1);
			msg_cnt ++;
			continue;
		}

		int rc = fprintf(fd, "%s", buffer);
		if (rc < 0) {
			DBG(" fail to write log file %s, sz %d\n", file_path, errno);
			sleep(1);
		}

		total_size += rc;
		file_size += rc;
		buffer += one_log_size;

		if (file_size >= log_file_size) {
			fclose(fd);
			fd = NULL;
		}
	}

	if (fd)
		fclose(fd);

	return total_size;

}

int kv_stats_dump_buffer(service_context_t *ctx, const char *buffer, int size)
{
	int total_dump = 0;
	int dump_buffer_size = size;
	file_header_t *hdr = (file_header_t *)buffer;
	char file_path[PAGE_SIZE];
	int file_size = hdr->reserved[0];
	DBG("kv_stats_dump working ... buffer %p size %d file_size %d \n", hdr, size, hdr->reserved[0]);
	int cur_time = get_current_date(ctx);
	cur_time --;
	while (file_size && (file_size > ctx->dump_info.file_hdr_size)) {
		//prepare timestamped filename
		cur_time ++;
		prepare_dump_fs(ctx, 0);
		snprintf(file_path, PAGE_SIZE, "%s%ld", ctx->current_stats_dir_path, (long)cur_time);
		int fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
		if (fd < 0) {
			DBG(" fail to open dumpe file %s, rc %d\n", file_path, errno);
			sleep(1);
			continue;
		}

		int sz = 0;
		int sz_to_write = file_size;
		while (sz < sz_to_write) {
			sz = write(fd, (const void *)hdr, sz_to_write);
			if (sz < 0) {
				DBG(" fail to write dumpe file %s, sz %d\n", file_path, errno);
				sleep(1);
			} else {
				sz_to_write -= sz;
				sz = 0;
			}
		}

		close(fd);

		int padding = KB - file_size % KB ;
		total_dump += (file_size + padding);

		DBG("kv_stats_dumping_buffer: one file %s total %d size %d padding %d\n",
		    file_path, total_dump, file_size, padding);
		//if we have more data to dump
		if ((dump_buffer_size - total_dump) > (PAGE_SIZE * 128)) {
			hdr = (file_header_t *)((char *)hdr + file_size + padding);
			file_size = hdr->reserved[0];
			DBG("kv_stats_dumping_buffer: next file size %d\n", file_size);
		} else {
			break;
		}
	}

	return total_dump;
}

void *kv_stats_log_dumping_proc(void *context)
{
	service_context_t *ctx  = (service_context_t *) context;
	int log_buffer_size = ctx->conf.conf_buffer_size_unit / 2;
	int total_log = 0;
	while (!ctx->thread_terminate) {

		pthread_mutex_lock(&ctx->log_mutex);
		if (!ctx->log_data_is_full) {
			pthread_cond_wait(&ctx->cond_log, &ctx->log_mutex);
		}

		DBG("log_dumping_proc: working ... \n");
		ctx->log_dumper_is_ready = 0;
		pthread_mutex_unlock(&ctx->log_mutex);

		if (ctx->thread_terminate)
			break;

		if (ctx->log_data_is_full) {
			ctx->log_data_is_full = 0;
			total_log = kv_log_dump_buffer(ctx, ctx->log_dumper_buffer, log_buffer_size);
			DBG("kv_log_dump_buffer done total_log size %d\n", total_log);
		} else {
			DBG("log_dumping_proc: log_data_is_full = 0\n");
		}

		ctx->log_dumper_is_ready = 1;
		//sleep(1);

	}
	DBG("kv_stats_log_dumping_proc exit\n");

	return ctx;

}
void *kv_stats_dumping_proc(void *context)
{
	service_context_t *ctx  = (service_context_t *) context;
	int dump_buffer_size = ctx->conf.conf_buffer_size_unit / 2;
	int total_dump = 0;
	while (!ctx->thread_terminate) {

		pthread_mutex_lock(&ctx->buffer_mutex);
		if (!ctx->sample_data_is_full) {
			pthread_cond_wait(&ctx->cond_var, &ctx->buffer_mutex);
		}

		ctx->dumper_is_ready = 0;
		pthread_mutex_unlock(&ctx->buffer_mutex);

		if (ctx->sample_data_is_full) {
			ctx->sample_data_is_full = 0;
			total_dump = kv_stats_dump_buffer(ctx, ctx->dumping_buffer, dump_buffer_size);
			DBG("kv_stats_dumping_proc done buffer_dump size %d\n", total_dump);
		}

		ctx->dumper_is_ready = 1;
		sleep(1);

	}
	DBG("kv_stats_dumping_proc exit\n");

	return ctx;
}

void KV_LOG(int level, const char *fmt, ...)
{
	service_context_t *ctx = &g_serv_context;
	va_list arg;
	int sz = 0, rc = 0;
	int log_dumper_ok = 1;
	int one_log_size = ctx->conf.conf_one_log_size;
	char *p ;
	static time_t start_time = 0;

	if (ctx->thread_terminate) {
		pthread_cond_signal(&ctx->cond_log);
		return;
	}

	if (level > g_log_level)
		return ;

	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
		perror("clock_gettime");
		return;
	}

	if (!start_time) {
		start_time = ts.tv_sec;
	}

	char *buffer = ctx->logging_buffer;
	buffer = ATOMIC_ADD(ctx->logging_buffer, one_log_size);
	//DBG("cur_nr_log %d log_full %d buffer %p, logging_buffer %p\n",
	//	ctx->cur_nr_log, ctx->log_data_is_full, buffer, ctx->logging_buffer);

	//exchange buffer if it's full
	char *old_buffer = buffer;
	if (buffer > ((char *)ctx->logging_buffer_orig + (ctx->conf.conf_buffer_size_unit / 2) -
		      PAGE_SIZE)) {
		pthread_mutex_lock(&ctx->log_mutex);
		if (ctx->log_dumper_is_ready && !ctx->log_data_is_full) {
			exchange_log_buffer(ctx);
			ctx->log_data_is_full = 1;
			pthread_cond_signal(&ctx->cond_log);
			DBG("exchange log_buffer, cur_nr_log %d logging_buffer %p\n",
			    ctx->cur_nr_log, ctx->logging_buffer);
		} else {
			log_dumper_ok = 0;
		}

		pthread_mutex_unlock(&ctx->log_mutex);
		buffer = ATOMIC_ADD(ctx->logging_buffer, one_log_size);

		if (!log_dumper_ok) {
			DBG("log_buffer full: dumper_is_ready %d, log_data_is_full %d, old %p new %p\n",
			    ctx->log_dumper_is_ready, ctx->log_data_is_full, old_buffer, buffer);
		}

	}

	p = buffer;
	if (ctx->cur_nr_log++ > (ctx->max_nr_log / 4))
		ctx->log_data_is_full = 0;

	rc = snprintf(buffer, one_log_size, "%ld.%09ld:", ts.tv_sec - start_time, ts.tv_nsec);
	if (rc > 0) {
		buffer = (char *)buffer + rc;
		sz += rc;
	}

	va_start(arg, fmt);
	rc = vsprintf(buffer, fmt, arg);
	sz += rc;
	va_end(arg);


	if (sz > one_log_size) {
		//truncate the msg
		p[one_log_size - 2] = '\n';
		p[one_log_size - 1] = 0;
		DBG("KV_LOG sz %d\n", sz);
	}

}


int init_kv_stats_service(const char *conf_name)
{

	int rc = 0;

	if (kv_stats_service_init) {
		return 0;
	} else {
		kv_stats_service_init = 1;
	}

	g_serv_context.is_host = -1; //undetermined role
	pthread_mutex_init(&__stats_mutex, NULL);

	g_serv_context.debug_info_fd = stderr;

	rc = get_conf_info(&g_serv_context, conf_name);
	rc = get_current_date(&g_serv_context);
	rc = prepare_dump_fs(&g_serv_context, 0);
	rc = prepare_dump_fs(&g_serv_context, 1);

	g_serv_context.data_buffer[0] = (char *)malloc(g_serv_context.conf.conf_buffer_size_unit / 2);
	g_serv_context.data_buffer[1] = (char *)malloc(g_serv_context.conf.conf_buffer_size_unit / 2);
	g_serv_context.sampling_buffer_size = (g_serv_context.conf.conf_buffer_size_unit) / 2;
	g_serv_context.sampling_buffer = g_serv_context.data_buffer[0];
	g_serv_context.dumping_buffer = g_serv_context.data_buffer[1];
	DBG("data_buffer[0] %p data_buffer[1] %p\n", g_serv_context.data_buffer[0],
	    g_serv_context.data_buffer[1]);

	g_serv_context.log_buffer[0] = (char *)malloc(g_serv_context.conf.conf_buffer_size_unit / 2);
	g_serv_context.log_buffer[1] = (char *)malloc(g_serv_context.conf.conf_buffer_size_unit / 2);
	g_serv_context.logging_buffer = g_serv_context.log_buffer[0];
	g_serv_context.logging_buffer_orig = g_serv_context.log_buffer[0];
	g_serv_context.log_dumper_buffer = g_serv_context.log_buffer[1];
	g_serv_context.max_nr_log = g_serv_context.conf.conf_buffer_size_unit /
				    (2 * g_serv_context.conf.conf_one_log_size);
	g_serv_context.cur_nr_log = 0;

	pthread_mutex_init(&g_serv_context.log_mutex, NULL);
	pthread_cond_init(&g_serv_context.cond_log, NULL);
	pthread_create(&g_serv_context.th_log_dumping, NULL, kv_stats_log_dumping_proc,
		       (void *) &g_serv_context);
	g_serv_context.log_dumper_is_ready = 1;
	g_serv_context.log_data_is_full = 0;
	DBG("log_buffer[0] %p log_buffer[1] %p max_nr_log %d\n",
	    g_serv_context.log_buffer[0], g_serv_context.log_buffer[1], g_serv_context.max_nr_log);

	kv_stats_table_init(&g_serv_context);

	pthread_mutex_init(&g_serv_context.buffer_mutex, NULL);
	pthread_cond_init(&g_serv_context.cond_var, NULL);
	g_serv_context.thread_terminate = 0;
	g_serv_context.dumper_is_ready = 1;
	g_serv_context.sample_data_is_full = 0;

	pthread_create(&g_serv_context.th_sampling, NULL, kv_stats_sampling_proc, (void *) &g_serv_context);
	pthread_create(&g_serv_context.th_dumping, NULL, kv_stats_dumping_proc, (void *) &g_serv_context);

	return rc;
}

int deinit_kv_stats_service(void)
{
	//flag the task termiation
	if (!kv_stats_service_init)
		return 0;

	g_serv_context.thread_terminate = 1;
	pthread_cond_signal(&g_serv_context.cond_log);

	pthread_join(g_serv_context.th_sampling, NULL);
	pthread_join(g_serv_context.th_dumping, NULL);
	pthread_join(g_serv_context.th_log_dumping, NULL);

	//dump the last stats
	update_file_size_for_dump(&g_serv_context);
	char *buffer = (g_serv_context.dumping_buffer == g_serv_context.data_buffer[0]) ?
		       g_serv_context.data_buffer[1] : g_serv_context.data_buffer[0];

	int last_dump_sz = kv_stats_dump_buffer(&g_serv_context, (const char *)buffer,
						(const char *)g_serv_context.sampling_buffer - buffer);
	DBG("last stats dump size %d\n", last_dump_sz);

	//dump the last logs
	buffer = (g_serv_context.log_dumper_buffer == g_serv_context.log_buffer[0]) ?
		 g_serv_context.log_buffer[1] : g_serv_context.log_buffer[0];

	last_dump_sz = kv_log_dump_buffer(&g_serv_context, (const char *)buffer,
					  g_serv_context.cur_nr_log * g_serv_context.conf.conf_one_log_size);
	DBG("last log dump size %d\n", last_dump_sz);

	//free the context, segment and cnt info
	kv_stats_table_deinit(&g_serv_context);

	//free the buffers
	free(g_serv_context.data_buffer[0]);
	free(g_serv_context.data_buffer[1]);
	free(g_serv_context.log_buffer[0]);
	free(g_serv_context.log_buffer[1]);

	//close the debug file handle
	if (g_serv_context.debug_info_fd != stderr) {
		fclose(g_serv_context.debug_info_fd);
	}

	kv_stats_service_init = 0;

	return 0;
}
