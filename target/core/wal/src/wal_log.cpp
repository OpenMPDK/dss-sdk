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
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include <wal_lib.h>
#include <wal_module.h>

//#define DFLY_WAL_FLUSH_QOS_ENABLED

void wal_module_load_done_cb(struct dfly_subsystem *pool, void *arg/*Not used*/);

int wal_get_pool_id(struct dfly_subsystem *pool);
void wal_log_sb_init(wal_sb_t *sb);
int wal_log_format(wal_device_info_t *dev_info, int zone_cnt, int sz_mb);

wal_context_t g_wal_ctx = {{NULL}, NULL, PTHREAD_MUTEX_INITIALIZER, 0, NULL, NULL, NULL};

static wal_sb_t *__log_sb[WAL_MAX_LOG_DEV] = {NULL};
static int __log_nr_pools = 1;
static char __wal_log_dev_name[256] = {0};
static char __wal_nqn_name[256] = {0};

static wal_device_info_t __log_dev[WAL_MAX_LOG_DEV] = {0};
static wal_device_info_t __cache_dev = {0, 0, 0, 0, 0, {0}, -1, NULL, NULL, NULL, NULL};
static int __cache_nr_zone = 0;
void *__wal_cache_buffer[MAX_ZONE] = {0};

bool __periodic_flush = false;
bool __log_enabled = false;
wal_log_device_init_ctx_t __wal_log_init_ctx;

int __wal_io_op_cnt = 0;
#define RECOVER_DATA_SZ  (8*MB)

struct timespec wal_log_startup;

int wal_cache_io_priority = WAL_CACHE_IO_REGULAR;
int nr_watermark_high	= 0;

wal_conf_t g_wal_conf = {
	WAL_CACHE_ENABLE_DEFAULT,
	WAL_LOG_ENABLE_DEFAULT,
	WAL_NR_CORES_DEFAULT,
	WAL_NR_ZONE_PER_POOL_DEFAULT,
	WAL_ZONE_SZ_MB_BY_DEFAULT,
	WAL_CACHE_WATERMARK_H,
	WAL_CACHE_WATERMARK_L,
	WAL_CACHE_FLUSH_THRESHOLD_BY_CNT,
	WAL_CACHE_FLUSH_PERIOD_MS,
	WAL_OPEN_FORMAT,
	WAL_CACHE_OBJECT_SIZE_LIMIT_KB_DEFAULT,
	WAL_LOG_BATCH_NR_OBJ,
	WAL_LOG_BATCH_TIMEOUT_US,
	WAL_NR_LOG_DEV_DEFAULT,
	WAL_LOG_CRASH_TEST,
	WAL_NQN_NAME,
};

dfly_io_module_handler_t wal_io_handlers = {
	wal_handle_store_op,
	wal_handle_retrieve_op,
	wal_handle_delete_op,
	NULL,   //iter_ctrl_handler
	NULL,   //iter_read_handler
	NULL,   //fuse_f1_handler
	NULL,   //fuse_f2_handler
};

static dfly_io_module_context_t wal_module_ctx =
{PTHREAD_MUTEX_INITIALIZER, &g_wal_conf, NULL, &wal_io_handlers};

void *wal_set_zone_module_ctx(int ssid, int zone_idx, void *set_ctx)
{
	wal_zone_t *zone = __cache_dev.zones[zone_idx];

	zone->module_instance_ctx = set_ctx;

	return zone;
}

static inline wal_get_full_record_size(wal_object_t *obj)
{
	int record_sz = obj->key->length + obj->val->length +
			sizeof(wal_obj_hdr_t) + sizeof(wal_obj_checksum_t);
	record_sz += (WAL_ALIGN - 1);
	record_sz &= (WAL_ALIGN_MASK);
	return record_sz;
}

int __wal_obj_hdr_sz = sizeof(wal_obj_hdr_t);
int	__wal_obj_chksum_sz = sizeof(wal_obj_checksum_t);
int __buffer_alignment = WAL_ALIGN;

int wal_log_get_object_size(wal_object_t *obj)
{
	return wal_get_full_record_size(obj);
}

int wal_cache_get_object_size(wal_object_t *obj)
{
	int record_sz = wal_get_full_record_size(obj);
	if (!g_wal_conf.wal_log_enabled)
		record_sz -= sizeof(wal_obj_checksum_t);

	return record_sz;
}

int wal_prepare_large_key(struct dfly_key *large_key, wal_map_item_t *item);
bool log_verify_object_hdr(wal_obj_hdr_t *obj_hdr);
int wal_log_read_object(wal_buffer_t *buffer, wal_object_t *obj,
			int64_t src_addr, int sz, int is_data_read);

int wal_cache_read_object(wal_buffer_t *buffer, wal_object_t *obj,
			  int64_t src_addr, int sz, int is_data_read);

int wal_log_map_read(void *context, wal_map_item_t *map_item, wal_object_t *obj);
int wal_cache_map_read(void *context, wal_map_item_t *map_item, wal_object_t *obj);

wal_zone_t *wal_log_get_object_zone(wal_object_t *obj);
wal_zone_t *wal_cache_get_object_zone2(wal_subsystem_t *pool, wal_object_t *obj);
int wal_log_init(wal_log_device_init_ctx_t *log_dev_info_ctx);
int wal_log_show_sb(wal_sb_t *sb);
void wal_log_recover_zones(wal_device_info_t *dev_info, wal_sb_t *sb, int zone_idx);
void log_complete_zone_recover(wal_device_info_t *dev_info, wal_sb_t *sb, int zone_idx);
void wal_log_complete_batch(struct df_dev_response_s resp, void *arg);
void wal_log_complete_single(struct df_dev_response_s resp, void *arg);
void wal_cache_flush_zone(wal_zone_t *zone);

int pmem_bdev_write(int handle, const void *buff, uint64_t offset, uint64_t nbytes,
		    void *cb, void *cb_arg)
{
	return pwrite(handle, buff, nbytes, offset);
}

int pmem_bdev_read(int handle, void *buff, uint64_t offset, uint64_t nbytes,
		   void *cb, void *cb_arg)
{
	return pread(handle, buff, nbytes, offset);
}

inline void *get_cache_buffer(int handle)
{
	return __wal_cache_buffer[handle];
}


int mem_cache_write(int handle, const void *buff, uint64_t offset, uint64_t nbytes,
		    void *cb, void *cb_arg)
{
	//void * cache_buffer = get_cache_buffer(handle);
	void *cache_buffer = (void *)offset;
	memcpy(cache_buffer, buff, nbytes);
	return nbytes;
}

int mem_cache_read(int handle, void *buff, uint64_t offset, uint64_t nbytes,
		   void *cb, void *cb_arg)
{
	//void * cache_buffer = get_cache_buffer(handle);
	void *cache_buffer = (void *)offset;
	memcpy(buff, cache_buffer, nbytes);
	return nbytes;
}

#ifdef WAL_DFLY_BDEV
inline int wal_dfly_bdev_open(const char *pathname, int flags)
{
	return dfly_device_open(pathname, flags, true);
}

inline int wal_dfly_bdev_close(int dev_handle)
{
	dfly_device_close(dev_handle);
	return 0;
}

static int dfly_bdev_write_wrap(int handle, const void *buff, uint64_t offset, uint64_t nbytes,
				void *cb, void *cb_arg)
{
	int dfly_rc = dfly_device_write(handle, buff, offset, nbytes, cb, cb_arg);
	if (dfly_rc == 0)
		return nbytes;
	else
		return -WAL_ERROR_BDEV;
}

static int dfly_bdev_read_wrap(int handle, const void *buff, uint64_t offset, uint64_t nbytes,
			       void *cb, void *cb_arg)
{
	int dfly_rc = dfly_device_read(handle, buff, offset, nbytes, cb, cb_arg);
	if (dfly_rc == 0)
		return nbytes;
	else
		return -WAL_ERROR_BDEV;
}

wal_device_io_ops_t wal_log_bdev_io = {
	wal_dfly_bdev_open,
	wal_dfly_bdev_close,
	dfly_bdev_read_wrap,
	dfly_bdev_write_wrap,
};
#else //PMEM
inline int wal_pmem_open(const char *pathname, int flags)
{
	return open(pathname, flags);
}

inline int wal_pmem_close(int dev_handle)
{
	close(dev_handle);
	return 0;
}

wal_device_io_ops_t wal_log_bdev_io = {
	wal_pmem_open,
	wal_pmem_close,
	pmem_bdev_read,
	pmem_bdev_write,
};
#endif

wal_device_io_ops_t wal_cache_dev_io = {
#ifdef WAL_DFLY_BDEV
	wal_dfly_bdev_open,
	wal_dfly_bdev_close,
#else
	NULL,
	NULL,
#endif
	mem_cache_read,
	mem_cache_write,
};

wal_map_ops_t wal_log_map_ops = {
	hash_sdbm,
	wal_key_compare,
	wal_log_insert_object,
	wal_log_map_read,
	wal_map_invalidate_item,
};

wal_map_ops_t wal_cache_map_ops = {
	hash_sdbm,
	wal_key_compare,
	wal_cache_insert_object,
	wal_cache_map_read,
	wal_map_invalidate_item,
};

wal_buffer_ops_t wal_log_buffer_ops = {
	&wal_log_bdev_io,
	wal_log_read_object,
	wal_log_get_object_size,
};

wal_buffer_ops_t wal_cache_buffer_ops = {
	&wal_cache_dev_io,
	wal_cache_read_object,
	wal_cache_get_object_size,
};

int wal_log_map_read(void *context, wal_map_item_t *map_item, wal_object_t *obj)
{
	wal_buffer_t *buffer = (wal_buffer_t *)context;
	return wal_log_read_object(buffer, obj, map_item->addr, 0, 1);
}

int wal_cache_map_read(void *context, wal_map_item_t *map_item, wal_object_t *obj)
{
	wal_buffer_t *buffer = (wal_buffer_t *)context;
	int rd_sz = MIN(map_item->val_size, obj->val->length);
	assert(map_item->key->length == obj->key->length);
	assert(obj->val->length);
	void *val_addr = map_item->addr;
	if (__log_enabled) {
		wal_obj_hdr_t *obj_hdr = (void *)map_item->addr;
		val_addr = (void *)obj_hdr + __wal_obj_hdr_sz + obj_hdr->key_sz;
	}
	return wal_cache_read_object(buffer, obj, (int64_t)val_addr, rd_sz, 1);
}

void log_init_continue()
{
	//__wal_log_init_ctx.nr_record_recovered += __wal_log_init_ctx.log_dev_info->recover_stats.nr_obj_record;
	ATOMIC_ADD(__wal_log_init_ctx.nr_record_recovered,
		   __wal_log_init_ctx.log_dev_info->dev_log_recovery_stats.nr_obj_record);
	if (++__wal_log_init_ctx.curr_device_idx >= __wal_log_init_ctx.nr_log_devices) {
		struct dfly_subsystem *subsystem = __wal_log_init_ctx.pool;
		subsystem->wal_init_status = WAL_INIT_DONE;
		wal_module_load_done_cb(subsystem, NULL);
		__periodic_flush =  true;
		long long wal_log_starup_time_ns = elapsed_time(wal_log_startup);
		printf("Done the wal log initial of %ld records in %lld ms\n",
		       __wal_log_init_ctx.nr_record_recovered, wal_log_starup_time_ns >> 20);
	} else {
		__wal_log_init_ctx.log_dev_info = NULL;
		__wal_log_init_ctx.sb = NULL;
		__wal_log_init_ctx.device_name = NULL;
		int rc = wal_log_init(&__wal_log_init_ctx);
		printf("log_init_continue: wal_log_init %s rc %x\n",
		       __wal_log_init_ctx.device_name, rc);
	}
}

//log layout write complete cb for (sb, zone buffer hdr)
void log_format_io_write_comp(struct df_dev_response_s resp, void *arg)
{
	log_cb_ctx_t *ctx = (log_cb_ctx_t *)arg;
	//printf("log_format_io_write_comp success = %d io_type 0x%x data %p\n",
	//    success, ctx->io_type, ctx->data);

	if (ctx->io_type == LOG_IO_TYPE_SB_WR) {
		wal_log_show_sb(*__wal_log_init_ctx.sb);
		log_init_continue();
	}
}

void log_recover_sb_read_comp(struct df_dev_response_s resp, void *arg)
{
	log_cb_ctx_t *ctx = (log_cb_ctx_t *)arg;
	//printf("log_recover_sb_read_comp success = %d io_type 0x%x data %p\n",
	//    success, ctx->io_type, ctx->data);

	assert(ctx->io_type == LOG_IO_TYPE_SB_RD);

	wal_device_info_t *log_dev_info = __wal_log_init_ctx.log_dev_info;
	wal_sb_t *sb = (wal_sb_t *)ctx->data;
	if (sb->magic != WAL_MAGIC) {

		//unformatted device. do the format.
		wal_log_sb_init(sb);
		log_dev_info->zones = (wal_zone_t **)df_calloc(MAX_ZONE, sizeof(wal_zone_t *));
		log_dev_info->maps = (wal_map_t **)df_calloc(MAX_ZONE * 2, sizeof(wal_map_t *));
		log_dev_info->sb = sb;
		wal_log_format(log_dev_info,
			       __wal_log_init_ctx.nr_zones_per_device, __wal_log_init_ctx.zone_size_mb);

		//continue the log recovery.
		//log_init_continue();
		return;
	}

	wal_log_show_sb(sb);
	log_dev_info->sb = sb;

	//recovery the sb and zone struct and related buffer maps.
	if (g_wal_conf.wal_open_flag == WAL_OPEN_RECOVER_SEQ)
		wal_log_recover_zones(log_dev_info, sb, 0);
	else if (g_wal_conf.wal_open_flag == WAL_OPEN_RECOVER_PAR)
		for (int i = 0; i < sb->nr_zone; i++)
			wal_log_recover_zones(log_dev_info, sb, i);
	else
		assert(0);

	//return 0;

}

void update_dump_group_info(wal_buffer_t *buffer)
{
	if (buffer) {
		//update the cache log dump info hdr.
		wal_dump_info_t *cache_dump_info = &(buffer->hdr.dump_info);
		cache_dump_info->dump_addr = buffer->hdr.curr_pos;
		cache_dump_info->dump_blk = 0;
		cache_dump_info->dump_flags = DUMP_NOT_READY;
		cache_dump_info->dump_ts = {0};
		start_timer(&cache_dump_info->idle_ts);
		buffer->hdr.curr_pos += WAL_DUMP_HDR_SZ;
		assert(buffer->hdr.curr_pos < buffer->hdr.end_addr);
		wal_dump_hdr_t *dump_hdr = (wal_dump_hdr_t *)cache_dump_info->dump_addr;
		dump_hdr->nr_batch_reqs = 0;
	}
}

int log_object_parse(char **pp_data_addr, int32_t size,
		     int32_t sequence, wal_object_t *obj)
{
	wal_obj_hdr_t *oh;
	wal_obj_checksum_t *ch;
	int parsed_sz = 0;
	char *data = * pp_data_addr;
	void *key, * val;
	//get object hdr
	oh = (wal_obj_hdr_t *)data;
	//verify the object header and checksum
	if (!log_verify_object_hdr(oh))
		return -WAL_ERROR_BAD_CHKSUM;

	int32_t record_sz = oh->sz_blk << WAL_ALIGN_OFFSET;
	if (size < record_sz) // incomplete data
		return -WAL_ERROR_RD_LESS;

	//checksum
	ch = (wal_obj_checksum_t *)(data + sizeof(wal_obj_hdr_t) + oh->key_sz + oh->val_sz);

	if (ch->chksum != sequence) {
		return -WAL_ERROR_BAD_CHKSUM;
	}

	//key
	obj->key->length = oh->key_sz;
	obj->key->key = data + sizeof(wal_obj_hdr_t);
	//val
	if (oh->state == WAL_ITEM_INVALID)
		obj->val->length = 0;
	else
		obj->val->length = oh->val_sz;

	if (obj->val->length)
		obj->val->value = data + sizeof(wal_obj_hdr_t) + oh->key_sz;
	else
		obj->val->value = NULL;

	data += record_sz;
	* pp_data_addr = data;

	return record_sz;

}

void log_recovery_writethrough_complete(struct df_dev_response_s resp, void *arg)
{
	void *dma_buffer = arg;
	assert(resp.rc);

	if (dma_buffer)
		dfly_put_key_buff(NULL, dma_buffer);//spdk_dma_free(dma_buffer);
}

void log_recover_data_read_comp(struct df_dev_response_s resp, void *arg)
{
	log_cb_ctx_t *ctx = (log_cb_ctx_t *)arg;
	int ctx_io_size = ctx->io_size;
	assert(ctx->io_type == LOG_IO_TYPE_DATA_RD && resp.rc);

	wal_device_info_t *log_dev_info = __wal_log_init_ctx.log_dev_info;

	wal_bdev_read_t bdev_read = log_dev_info->dev_io->read;
	wal_buffer_t *buffer = (wal_buffer_t *)ctx->data;
	wal_zone_t *zone = buffer->zone;
	wal_buffer_hdr_t *hdr = &buffer->hdr;
	char *recovery_buffer = zone->recovery_buffer;
	wal_zone_t *cache_zone = NULL;
	wal_debug("log_recover_data_read_comp: success %d zone[%d] buffer role %d data %p\n",
		  resp.rc, zone->zone_id, hdr->role, recovery_buffer);

	//parse the data go here.
	// ...
	wal_object_t obj;
	wal_key_t *regular_key = (wal_key_t *)df_calloc(1, sizeof(wal_key_t));
	obj.key = regular_key;
	obj.val = (wal_val_t *)df_calloc(1, sizeof(wal_val_t));
	obj.obj_private = 0;

	struct dfly_key large_key;
	large_key.key = NULL;

	const char *buffer_data = (const char *)recovery_buffer;
	int rc = 0, dfly_rc = 0;
	int buffer_size = ctx_io_size;
	int is_last_chunk = (ctx_io_size < RECOVER_DATA_SZ);
	int min_obj_key_sz = 16;
	int min_obj_record_sz = sizeof(wal_obj_checksum_t) + sizeof(wal_obj_hdr_t) + min_obj_key_sz;
	while ((rc = log_object_parse(&buffer_data, buffer_size, hdr->sequence, &obj)) > 0) {
		obj.key_hashed = 0;
		//populate the cache here.

		int pool_id = __wal_log_init_ctx.pool->id;
		wal_map_item_t *dummy_map_item = NULL;
		cache_zone = wal_cache_get_object_zone2(&g_wal_ctx.pool_array[pool_id], &obj);
		cache_zone->associated_zone = zone;
		zone->associated_zone = cache_zone;

		//df_lock(&cache_zone->uio_lock);
		bool is_del = (obj.val->length == 0);
		int cache_rc = wal_zone_insert_object(cache_zone, &obj, is_del, &dummy_map_item, NULL);

		//TODO: Update pool information for list update
		list_key_update(NULL, obj.key->key, obj.key->length, is_del, true);

		//recovery stats
		ATOMIC_INC(zone->zone_recovery_stats.nr_obj_record);
		if (cache_rc == WAL_ERROR_WRITE_MISS) {
			//assert(!dummy_map_item);
			printf("log_recover_data_read_comp zone[%d] poll_flush_flag %x is_del %d cache_rc %d\n",
			       cache_zone->zone_id, cache_zone->poll_flush_flag, is_del, cache_rc);

			if (obj.key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
				large_key.key = dfly_get_key_buff(NULL, SAMSUNG_KV_MAX_FABRIC_KEY_SIZE);
				assert(large_key.key && obj.key->length <= SAMSUNG_KV_MAX_FABRIC_KEY_SIZE);
				large_key.length = obj.key->length;
				obj.key = &large_key;
			}

			if (is_del)
				dfly_rc = dfly_device_delete(cache_zone->tgt_fh, obj.key,
							     log_recovery_writethrough_complete, large_key.key);
			else
				dfly_rc = dfly_device_store(cache_zone->tgt_fh, obj.key, obj.val,
							    log_recovery_writethrough_complete, large_key.key);

			obj.key = regular_key;

			assert(!dfly_rc);
		}

		if (dummy_map_item && dummy_map_item->recover_status == WAL_ITEM_RECOVER_OVERWRITE)
			zone->zone_recovery_stats.nr_obj_overwritted ++;

		if (is_del)
			zone->zone_recovery_stats.nr_obj_deleted ++;

		wal_debug("log_object_parse: flush %d log_zone[%d] key 0x%llx%llx val_size 0x%x rc %d cache_rc %d buffer_size %d nr_record %d pos 0x%llx\n",
			  zone->zone_id, cache_zone->poll_flush_flag, *(long long *)obj.key->key,
			  *(long long *)(obj.key->key + 8),
			  obj.val->length, rc, cache_rc, buffer_size, zone->zone_recovery_stats.nr_obj_record,
			  buffer->hdr.curr_pos);

		//df_unlock(&cache_zone->uio_lock);
		buffer_size -= rc;
		if (buffer_size < min_obj_record_sz) {
			rc = -WAL_ERROR_RD_LESS;
			break;
		}
	};

	df_free(regular_key);
	df_free(obj.val);

	buffer->hdr.curr_pos += (ctx_io_size - buffer_size);
	//printf("log_recover_data_read_comp: zone[%d] buffer role %d parsed records %d rc -0x%x buffer_size %d nxt_pos 0x%llx\n",
	//        zone->zone_id, hdr->role, log_dev_info->recover_stats.nr_obj_record, -rc, buffer_size, buffer->hdr.curr_pos);

	if (is_last_chunk) {
		rc = -WAL_ERROR_BAD_CHKSUM;
	}

	//check if the end of buffer data
	if (rc == -WAL_ERROR_RD_LESS) {
		int rest_buffer_space_sz = buffer->hdr.end_addr - buffer->hdr.curr_pos;
		if (rest_buffer_space_sz < min_obj_record_sz) {
			printf("log_recover_data_read_comp: zone[%d] role 0x%x reach the end of buffer\n",
			       zone->zone_id, hdr->role);
			assert(0);
		} else {
			//continue read the data for the current buffer
			buffer->hdr_io_ctx.io_type = LOG_IO_TYPE_DATA_RD;
			buffer->hdr_io_ctx.data = buffer;
			buffer->hdr_io_ctx.io_size = MIN(RECOVER_DATA_SZ, rest_buffer_space_sz);
			bdev_read(log_dev_info->fh, recovery_buffer, buffer->hdr.curr_pos, buffer->hdr_io_ctx.io_size,
				  log_recover_data_read_comp, &buffer->hdr_io_ctx);
		}
	} else if (rc == -WAL_ERROR_BAD_CHKSUM) {
		// done the current buffer read.
		// for flush, continue the log buffer
		// for the log, done the whole zone data recovery.
		printf("log_object_parse zone [%d] role %d done with rc -0x%x, parsed sz %d, total %d records\n",
		       zone->zone_id, buffer->hdr.role, -rc, (buffer->hdr.curr_pos - buffer->hdr.start_addr),
		       zone->zone_recovery_stats.nr_obj_record);
		if ((buffer->hdr.role & WAL_BUFFER_ROLE_BIT) == WAL_BUFFER_ROLE_FLUSH) {
			//read back the log buffer
			buffer = zone->log;
			off64_t pos = buffer->hdr.curr_pos;
			buffer->hdr_io_ctx.io_type = LOG_IO_TYPE_DATA_RD;
			buffer->hdr_io_ctx.io_size = RECOVER_DATA_SZ;
			buffer->hdr_io_ctx.data = buffer;
			bdev_read(log_dev_info->fh, recovery_buffer, pos, buffer->hdr_io_ctx.io_size,
				  log_recover_data_read_comp, &buffer->hdr_io_ctx);
		} else {
			//done the both flush and log buffer of this zone.
			//continue to recover the next zone
			if (cache_zone) {
				update_dump_group_info(cache_zone->log);
			}

			log_dev_info->dev_log_recovery_stats.nr_obj_record += zone->zone_recovery_stats.nr_obj_record;
			log_dev_info->dev_log_recovery_stats.nr_obj_overwritted +=
				zone->zone_recovery_stats.nr_obj_overwritted;
			log_dev_info->dev_log_recovery_stats.nr_obj_deleted += zone->zone_recovery_stats.nr_obj_deleted;

			if (g_wal_conf.wal_open_flag == WAL_OPEN_RECOVER_SEQ)
				wal_log_recover_zones(__wal_log_init_ctx.log_dev_info,
						      *__wal_log_init_ctx.sb, zone->zone_id + 1);
			else if (g_wal_conf.wal_open_flag == WAL_OPEN_RECOVER_PAR)
				log_complete_zone_recover(__wal_log_init_ctx.log_dev_info,
							  *__wal_log_init_ctx.sb, zone->zone_id + 1);
			else
				assert(0);

		}
	}

}

void log_recover_buffer_read_comp(struct df_dev_response_s resp, void *arg)
{
	log_cb_ctx_t *ctx = (log_cb_ctx_t *)arg;
	assert(ctx->io_type == LOG_IO_TYPE_BUFF_HDR_RD && resp.rc);

	wal_device_info_t *log_dev_info = __wal_log_init_ctx.log_dev_info;

	wal_bdev_read_t bdev_read = log_dev_info->dev_io->read;
	wal_buffer_t *buffer = (wal_buffer_t *)ctx->data;
	wal_zone_t *zone = buffer->zone;
	wal_buffer_hdr_t *hdr = &buffer->hdr;

	char *recovery_buffer = zone->recovery_buffer;;

	printf("log_recover_buffer_read_comp: success %d zone[%d] buffer role %d start 0x%llx cur_pos 0x%llx end_addr 0x%llx seq %d\n",
	       resp.rc, zone->zone_id, hdr->role,
	       (long long)hdr->start_addr, (long long)hdr->curr_pos, (unsigned long)hdr->end_addr, hdr->sequence);

	buffer->hdr.curr_pos = (buffer->hdr.start_addr + WAL_BUFFER_HDR_SZ + WAL_ALIGN - 1) &
			       WAL_ALIGN_MASK;

	buffer->ops = &wal_log_buffer_ops;
	buffer->map = NULL; //wal_map_create(WAL_MAX_BUCKET, &wal_log_map_ops);

	if (buffer->hdr.role != WAL_BUFFER_ROLE_LOG
	    && buffer->hdr.role != WAL_BUFFER_ROLE_FLUSH) {
		//error on log buffer hdr! TODO
		return -WAL_ERROR_BUFFER_ROLE;
	}

	if ((buffer->hdr.role & WAL_BUFFER_ROLE_BIT) == WAL_BUFFER_ROLE_LOG) {
		if (buffer->hdr.sequence < buffer->hdr.sequence)
			return -WAL_ERROR_BUFFER_ROLE; //to format the log device. TBD
		zone->log = buffer;
	} else if ((buffer->hdr.role & WAL_BUFFER_ROLE_BIT) == WAL_BUFFER_ROLE_FLUSH) {
		if (buffer->hdr.sequence > buffer->hdr.sequence)
			return -WAL_ERROR_BUFFER_ROLE; //to format the log device. TBD
		zone->flush = buffer;
	} else {
		assert(0);
	}

	//start recover the flush buffer
	if ((buffer->hdr.role & WAL_BUFFER_ROLE_BIT) == WAL_BUFFER_ROLE_FLUSH) {
		off64_t pos = buffer->hdr.curr_pos;
		buffer->hdr_io_ctx.io_type = LOG_IO_TYPE_DATA_RD;
		buffer->hdr_io_ctx.io_size = RECOVER_DATA_SZ;
		buffer->hdr_io_ctx.data = buffer;
		bdev_read(log_dev_info->fh, recovery_buffer, pos, buffer->hdr_io_ctx.io_size,
			  log_recover_data_read_comp, &buffer->hdr_io_ctx);
	}

}

int wal_log_recover_zone_buffer(wal_device_info_t *dev_info, off64_t pos, int32_t sz_mb,
				wal_zone_t  *zone)
{
	wal_buffer_t *buffer1, * buffer2;
	wal_bdev_read_t bdev_read = dev_info->dev_io->read;
	off64_t next_pos = 0;

	int32_t buffer_sz_4k = (sizeof(wal_buffer_t) + WAL_PAGESIZE - 1) >> WAL_PAGESIZE_SHIFT;
	buffer1 = (wal_buffer_t *)spdk_dma_malloc(buffer_sz_4k << WAL_PAGESIZE_SHIFT, WAL_PAGESIZE, NULL);
	buffer2 = (wal_buffer_t *)spdk_dma_malloc(buffer_sz_4k << WAL_PAGESIZE_SHIFT, WAL_PAGESIZE, NULL);

	//buffer1->recovery_buffer = spdk_dma_malloc(MB, WAL_PAGESIZE, NULL);//for log data recovery
	//buffer2->recovery_buffer = spdk_dma_malloc(MB, WAL_PAGESIZE, NULL);//for log data recovery

	buffer1->zone = zone;
	buffer2->zone = zone;

	buffer1->fh = dev_info->fh;
	buffer2->fh = dev_info->fh;

	//recover the first buffer in the zone.
	buffer1->hdr_io_ctx.io_type = LOG_IO_TYPE_BUFF_HDR_RD;
	buffer1->hdr_io_ctx.io_size = WAL_BUFFER_HDR_SZ;
	buffer1->hdr_io_ctx.data = buffer1;
	int read_sz = bdev_read(dev_info->fh, &buffer1->hdr, pos, WAL_BUFFER_HDR_SZ,
				log_recover_buffer_read_comp, &buffer1->hdr_io_ctx);

	assert(read_sz == WAL_BUFFER_HDR_SZ);

	//recover the second buffer in the zone.
	next_pos = pos + ((sz_mb / 2) << 20);
	buffer2->hdr_io_ctx.io_type = LOG_IO_TYPE_BUFF_HDR_RD;
	buffer2->hdr_io_ctx.io_size = WAL_BUFFER_HDR_SZ;
	buffer2->hdr_io_ctx.data = buffer2;
	read_sz = bdev_read(dev_info->fh, &buffer2->hdr, next_pos, WAL_BUFFER_HDR_SZ,
			    log_recover_buffer_read_comp, &buffer2->hdr_io_ctx);

	assert(read_sz == WAL_BUFFER_HDR_SZ);

	return 0;

}

void log_complete_zone_recover(wal_device_info_t *dev_info, wal_sb_t *sb, int zone_idx)
{
	++dev_info->nr_zone_recovered;
	if (dev_info->nr_zone_recovered >= sb->nr_zone) {
		printf("wal_log_recover_zones: done device %s nr_zone %d "
		       "nr_obj_records %ld nr_obj_overwrite %ld nr_obj_deleleted %ld\n",
		       __wal_log_init_ctx.device_name, sb->nr_zone,
		       dev_info->dev_log_recovery_stats.nr_obj_record,
		       dev_info->dev_log_recovery_stats.nr_obj_overwritted,
		       dev_info->dev_log_recovery_stats.nr_obj_deleted);

		log_init_continue();
		return;
	}
}

void wal_log_recover_zones(wal_device_info_t *dev_info, wal_sb_t *sb, int zone_idx)
{
	wal_zone_t *zone = NULL;
	int32_t i = zone_idx, rc = 0;
	int nr_zone = sb->nr_zone;
	off64_t pos = sb->offset;

	//if the last zone recovery completed
	if (zone_idx >= sb->nr_zone) {
		printf("wal_log_recover_zones: done device %s nr_zone %d recovered\n"
		       "nr_obj_records %ld nr_obj_overwrite %ld nr_obj_deleleted %ld\n",
		       __wal_log_init_ctx.device_name, sb->nr_zone,
		       dev_info->dev_log_recovery_stats.nr_obj_record,
		       dev_info->dev_log_recovery_stats.nr_obj_overwritted,
		       dev_info->dev_log_recovery_stats.nr_obj_deleted);

		log_init_continue();
		return;
	}

	//allocate zones for the first time recovery
	if (zone_idx == 0)
		dev_info->zones = (wal_zone_t **)df_malloc(sizeof(wal_zone_t *) * MAX_ZONE);

	pos = sb->zone_space[i].addr_mb;
	pos <<= 20;
	zone = (wal_zone_t *)df_calloc(1, sizeof(wal_zone_t));
	zone->recovery_buffer = spdk_dma_malloc(RECOVER_DATA_SZ, WAL_PAGESIZE, NULL);//for log data recovery
	df_lock_init(&zone->uio_lock, NULL);
	zone->zone_id = i;
	dev_info->zones[i] = zone;
	rc = wal_log_recover_zone_buffer(dev_info, pos, sb->zone_space[i].size_mb, zone);

	return;
}
int wal_log_sb_read(wal_device_info_t *dev_info, wal_sb_t *sb)
{
	int sb_sz = sizeof(wal_sb_t);
	int rc = WAL_ERROR_RD_PENDING;

	if (dev_info->fh == WAL_INIT_FH)
		return -WAL_ERROR_HANDLE;

	dev_info->log_io_ctx.io_type = LOG_IO_TYPE_SB_RD;
	dev_info->log_io_ctx.data = sb;
	dev_info->log_io_ctx.io_size = sb_sz;

	if ((dev_info->dev_io->read(dev_info->fh, sb, WAL_SB_OFFSET, sb_sz,
				    log_recover_sb_read_comp, &dev_info->log_io_ctx)) < sb_sz)
		return -WAL_ERROR_IO;

	return rc;
}

uint32_t inline deleted_object_record_size(wal_obj_hdr_t *hdr)
{
	uint32_t object_size = __wal_obj_chksum_sz + __wal_obj_hdr_sz + hdr->key_sz;
	return (object_size + __buffer_alignment - 1) >> WAL_ALIGN_OFFSET;

}

uint32_t inline deleted_object_record_size(wal_object_t *obj)
{
	uint32_t object_size = __wal_obj_chksum_sz + __wal_obj_hdr_sz + obj->key->length;
	return (object_size + __buffer_alignment - 1) >> WAL_ALIGN_OFFSET;
}


int wal_map_invalidate_item(void *context,
			    wal_bucket_t *bucket, wal_map_item_t *item)
{
	wal_buffer_t *buffer = (wal_buffer_t *) context;
	//printf("wal_map_invalidate_item item %p status %x ", item, item->status);
	int b1 = ATOMIC_BOOL_COMP_CHX(item->status, WAL_ITEM_VALID, WAL_ITEM_INVALID);
	int b2 = ATOMIC_BOOL_COMP_CHX(item->status, WAL_ITEM_FLUSHED, WAL_ITEM_INVALID);
	int b3 = ATOMIC_BOOL_COMP_CHX(item->status, WAL_ITEM_DELETED, WAL_ITEM_INVALID);
	//printf("b1 %d b2 %d\n", b1, b2);
	if (b1 || b2 || b3) {
		-- bucket->valid_nr_entries;
		if (b1) {
			ATOMIC_DEC_FETCH(buffer->nr_objects_inserted);
			buffer->zone->nr_invalidate ++;
		}

		if (__log_enabled && (buffer->hdr.role & WAL_BUFFER_ROLE_DRAM)) {
			wal_obj_hdr_t *hdr = (wal_obj_hdr_t *)item->addr;
			hdr->state = WAL_ITEM_INVALID;
		}
		return 0;
	}
	return -1;//item is flushing or invalid.
}

int wal_key_compare(wal_key_t *k1, wal_key_t *k2)
{
	if (k1->length != k2->length)
		return -1;

	return memcmp(k1->key, k2->key, k1->length);
}

int wal_object_compare(wal_object_t *obj1, wal_object_t *obj2)
{
	if (obj1->key->length != obj2->key->length || obj1->val->length != obj2->val->length)
		return -1;

	if (memcmp(obj1->key->key, obj2->key->key, obj1->key->length))
		return -1;

	if (memcmp(obj1->val->value + obj1->val->offset,
		   obj2->val->value + obj2->val->offset, obj1->val->length))
		return -1;

	return 0;
}

bool log_verify_object_hdr(wal_obj_hdr_t *obj_hdr)
{
	if (obj_hdr->addr != __wal_obj_hdr_sz)
		return false;

	if (obj_hdr->sz_blk != (__wal_obj_hdr_sz + __wal_obj_chksum_sz + obj_hdr->key_sz + obj_hdr->val_sz +
				__buffer_alignment - 1) >> WAL_ALIGN_OFFSET)
		return false;

	return true;
}
int32_t wal_prepare_object_record(wal_object_t *obj,
				  wal_obj_hdr_t *obj_hdr, wal_obj_checksum_t *chksum, int32_t seq, bool is_del)
{
	int32_t record_sz = 0;
	obj_hdr->key_sz = obj->key->length;
	if (is_del)
		obj_hdr->val_sz = 0;
	else
		obj_hdr->val_sz = obj->val->length;

	//hdr.addr point to the offset of key in an object record: [hdr|key|value|chksum]
	obj_hdr->addr = __wal_obj_hdr_sz;
	if (is_del)
		obj_hdr->state = WAL_ITEM_DELETED;
	else
		obj_hdr->state = WAL_ITEM_VALID;

	record_sz = __wal_obj_hdr_sz + __wal_obj_chksum_sz + obj_hdr->key_sz + obj_hdr->val_sz;
	obj_hdr->sz_blk = (record_sz + __buffer_alignment - 1) >> WAL_ALIGN_OFFSET ;

	chksum->chksum = seq;
	chksum->padding_sz = (obj_hdr->sz_blk << WAL_ALIGN_OFFSET) - record_sz;

	return record_sz;
}

int32_t wal_cache_insert_object(void *context, wal_object_t *obj,
				wal_map_item_t *cache_map_item, wal_map_insert_t insert_type)
{
	wal_buffer_t *buffer = (wal_buffer_t *)context;
	wal_buffer_hdr_t *buff_hdr = &buffer->hdr;
	wal_obj_hdr_t obj_hdr ;
	wal_bdev_write_t buffer_write = buffer->ops->dev_io->write;
	off64_t pos = buff_hdr->curr_pos;
	wal_fh fh = buffer->fh;
	int32_t write_size = 0, record_size = 0;
	bool append = true;
	int rc = WAL_SUCCESS;
	wal_obj_checksum_t chksum;
	int old_item_status = cache_map_item->status;

	if (__log_enabled) {
		wal_zone_t *log_zone = buffer->zone->associated_zone;
		assert(log_zone);
		record_size = wal_prepare_object_record(obj, &obj_hdr, &chksum,
							log_zone->log->hdr.sequence, insert_type == WAL_MAP_OVERWRITE_DELETE);
	}

	//get the addr of object to be copied
	assert(cache_map_item);
	//check if existed object data buffer is large enough
	if (!__log_enabled && cache_map_item->val_size > obj->val->length) {
		pos = cache_map_item->addr;
		append = false;
	} else {
		//append the object (value or full object), change the item->addr
		pos = buff_hdr->curr_pos;
		cache_map_item->addr = buff_hdr->curr_pos;
	}

	if (insert_type == WAL_MAP_OVERWRITE_APPEND || insert_type == WAL_MAP_OVERWRITE_INPLACE)
		buffer->zone->nr_overwrite ++;

	//copy the object
	if (!__log_enabled) {
		//cache only record [val]
		record_size = buffer_write(fh, obj->val->value + obj->val->offset,  pos, obj->val->length, NULL,
					   NULL);
		obj_hdr.sz_blk = (record_size + __buffer_alignment - 1) >> WAL_ALIGN_OFFSET;
	} else {
		//log full record [hdr|key|val|chksum]
		write_size = buffer_write(fh, &obj_hdr, pos, __wal_obj_hdr_sz, NULL, NULL);
		pos += write_size;
		write_size = buffer_write(fh, obj->key->key, pos, obj->key->length, NULL, NULL);
		pos += write_size;
		if (obj_hdr.val_sz) {
			write_size = buffer_write(fh, obj->val->value + obj->val->offset, pos, obj->val->length, NULL,
						  NULL);
			pos += write_size;
		}
		write_size = buffer_write(fh,  &chksum, pos, __wal_obj_chksum_sz, NULL, NULL);
		pos += write_size;

#ifdef WAL_DFLY_TRACK
		if (obj->obj_private)
			((struct dfly_request *)obj->obj_private)->wal_status = 1;
#endif
	}

	wal_debug("wal_cache_insert_object: pos 0x%llx obj_hdr.sz_blk 0x%x record_size 0x%x\n",
		  cache_map_item->addr, obj_hdr.sz_blk, record_size);

	if (__log_enabled && insert_type == WAL_MAP_OVERWRITE_DELETE)
		cache_map_item->status = WAL_ITEM_DELETED;
	else
		cache_map_item->status = WAL_ITEM_VALID;

	if (wal_cache_io_priority == WAL_CACHE_IO_REGULAR) {
		buffer->utilization = 100 - (((buff_hdr->end_addr - buff_hdr->curr_pos) >> 10) * 100) /
				      buffer->total_space_KB;
		if (buffer->utilization >= g_wal_conf.wal_cache_utilization_watermark_h
		    && (buffer->zone->poll_flush_flag & WAL_POLL_FLUSH_DOING)
		    && !buffer->inc_wmk_change
		   ) {
			buffer->inc_wmk_change = 1;
			if (ATOMIC_INC_FETCH(nr_watermark_high) == __cache_nr_zone) {
				wal_cache_io_priority = WAL_CACHE_IO_PRIORITY;
				wal_debug("zone %d, raise wal_cache_io_priority %d\n", buffer->zone->zone_id,
					  wal_cache_io_priority);
			} else {
				wal_debug("zone %d, util %d nr_watermark_high %d nr_obj %d curr_pos 0x%llx end_addr 0x%llx\n",
					  buffer->zone->zone_id, buffer->utilization, nr_watermark_high, buffer->nr_objects_inserted,
					  buff_hdr->curr_pos, buff_hdr->end_addr);
			}
		}
	}


	if (append) {
		buff_hdr->curr_pos += (obj_hdr.sz_blk << WAL_ALIGN_OFFSET);
	}

	//case 1: new object
	if (insert_type == WAL_MAP_INSERT_NEW)
		ATOMIC_INC(buffer->nr_objects_inserted);

	//case 2: reuse the deleted item for new object
	if (insert_type == WAL_MAP_OVERWRITE_APPEND && old_item_status == WAL_ITEM_DELETED)
		ATOMIC_INC(buffer->nr_objects_inserted);

	if (buffer->nr_objects_inserted == 1) {
		start_timer(&buffer->zone->flush_idle_ts);
	}

	return rc;

}


int32_t wal_log_dump_objs(wal_zone_t *log_zone, wal_dump_info_t *dump_info)
{
	/*
	wal_dump_info_t * new_cache_dump_info = cache_zone->log->hdr.curr_pos;
	new_cache_dump_info->dump_addr = cache_zone->log->hdr.curr_pos;
	new_cache_dump_info->dump_blk = 0;
	new_cache_dump_info->dump_flags = DUMP_NOT_READY;
	new_cache_dump_info->dump_ts = {0};
	cache_zone->log->hdr.curr_pos += WAL_DUMP_HDR_SZ;
	*/
	int rc = wal_log_insert_object(log_zone, NULL, NULL, 0, dump_info);
	return rc;
}

int32_t wal_log_insert_object(void *context, wal_object_t *obj,
			      wal_map_item_t *cache_map_item, wal_map_insert_t insert_type, wal_dump_info_t *p_dump_info)
{
	wal_zone_t *zone = (wal_zone_t *) context;
	assert(context);
	int rc = WAL_SUCCESS;

	wal_buffer_t *cache_buffer = (wal_buffer_t *)zone->log;
	wal_buffer_hdr_t *cache_buff_hdr = &cache_buffer->hdr;
	if (p_dump_info->dump_flags == DUMP_NOT_READY)
		return rc;

	int record_sz = 0;
	if (p_dump_info->dump_flags & DUMP_BATCH) {
		record_sz += p_dump_info->dump_blk << WAL_ALIGN_OFFSET;;
	}
	if (p_dump_info->dump_flags & DUMP_SINGLE) {
		int obj_sz = (insert_type == WAL_MAP_OVERWRITE_DELETE) ? wal_deletion_object_size(obj)
			     : zone->log->ops->get_object_size(obj);
		record_sz += obj_sz;
	}

	if (zone->log->hdr.curr_pos + record_sz >= zone->log->hdr.end_addr) {
		wal_debug("wal log %d: wal_zone_switch_buffer with nr_record %d\n",
			  zone->zone_id, zone->log->nr_objects_inserted);
		wal_zone_switch_buffer(zone);
	}

	wal_buffer_t *log_buffer = zone->log;
	wal_buffer_hdr_t *log_buff_hdr = &log_buffer->hdr;
	wal_bdev_write_t buffer_write = log_buffer->ops->dev_io->write;
	off64_t pos = log_buff_hdr->curr_pos;
	uint32_t    total_log_size = 0;
	wal_fh fh = log_buffer->fh;

	//wal_obj_checksum_t * chksum = (wal_obj_checksum_t *)(cache_map_item->addr +
	//    __wal_obj_hdr_sz + obj->key->length +  + obj->val->length);

	if (p_dump_info->dump_flags & DUMP_BATCH) {
		wal_dump_hdr_t *dump_hdr = (wal_dump_hdr_t *)p_dump_info->dump_addr;
		void *dump_data_addr = ((off64_t)dump_hdr + WAL_DUMP_HDR_SZ + WAL_ALIGN - 1) & WAL_ALIGN_MASK;
		uint32_t dump_data_size = p_dump_info->dump_blk << WAL_ALIGN_OFFSET;

		void *src_addr = dump_data_addr;
		wal_obj_hdr_t *oh = src_addr;
		int nr_updated = 0;
		while (nr_updated < dump_hdr->nr_batch_reqs) {
			wal_obj_checksum_t *ch =
				(wal_obj_checksum_t *)(src_addr + sizeof(wal_obj_hdr_t) + oh->key_sz + oh->val_sz);

			ch->chksum = zone->log->hdr.sequence;
			src_addr += (oh->sz_blk << WAL_ALIGN_OFFSET);
			oh = src_addr;
			struct dfly_request *req = (struct dfly_request *) dump_hdr->req_list[nr_updated];
			nr_updated ++;
#ifdef WAL_DFLY_TRACK
			req->wal_status = 2;
#endif

		}

		if (dump_hdr->nr_batch_reqs) {
			rc = buffer_write(fh, dump_data_addr, pos, dump_data_size,
					  wal_log_complete_batch, p_dump_info->dump_addr);
			assert(rc == dump_data_size);
			//printf("wal_log_insert_object zone[%d] pos 0x%llx size 0x%x\n",
			//    log_buffer->zone->zone_id, pos, dump_data_size);

			pos += rc;
			total_log_size += dump_data_size;
			log_buffer->nr_objects_inserted += dump_hdr->nr_batch_reqs;
		}

		p_dump_info->dump_addr = ((off64_t)dump_data_addr + dump_data_size + WAL_ALIGN - 1) &
					 WAL_ALIGN_MASK;
		p_dump_info->dump_blk = 0;
	}

	if (p_dump_info->dump_flags & DUMP_SINGLE) {
		assert(cache_map_item);
		wal_obj_hdr_t *oh = (wal_obj_hdr_t *)cache_map_item->addr;
		void *src_addr = (void *)oh;
		uint32_t cache_item_size = oh->sz_blk << WAL_ALIGN_OFFSET;

		wal_obj_checksum_t *ch =
			(wal_obj_checksum_t *)(src_addr + sizeof(wal_obj_hdr_t) + oh->key_sz + oh->val_sz);

		ch->chksum = zone->log->hdr.sequence;

		rc = buffer_write(fh, src_addr, pos, cache_item_size, wal_log_complete_single, obj->obj_private);
		assert(rc == cache_item_size);
		total_log_size += cache_item_size;
		if (oh->state == WAL_ITEM_VALID)
			log_buffer->nr_objects_inserted ++;

		if (((struct dfly_request *)obj->obj_private)->retry_count)
			printf("wal_log_insert_object: single dump pos 0x%llx sz %d oh->state %d req %p\n",
			       pos, cache_item_size, oh->state,  obj->obj_private);
	}

	p_dump_info->dump_flags = DUMP_NOT_READY;

	if (rc < 0) {
		printf("wal_cache_insert_object: failed with rc %d\n", rc);
		return rc;
	} else {

		//ATOMIC_INC(log_buffer->nr_objects_inserted);
		/*
		printf("wal_log_insert_object: req %p at zone[%d] pos 0x%llx "
		     "sz %d key 0x%llx%llx chksum %d padding %d\n",
		     req, log_buffer->zone->zone_id, pos, log_size,
		     *(long long *)obj->key->key, *(long long *)(obj->key->key+8),
		     chksum->chksum, chksum->padding_sz);
		*/
		rc = WAL_SUCCESS;
	}

	log_buff_hdr->curr_pos += total_log_size;

	return rc;

}

//zero dereference crash on purpose for test.
void crash_test(int completed_cnt)
{
	//zero dereference crash on purpose for test.
	__wal_io_op_cnt += completed_cnt;
	if (g_wal_conf.wal_log_crash_test && __wal_io_op_cnt >= g_wal_conf.wal_log_crash_test) {
		printf("crash_test with %d completed ops\n", __wal_io_op_cnt);
		char *dummy_ptr = 0;
		char c = *dummy_ptr;
	}

}
static void wal_log_complete_single(struct df_dev_response_s resp, void *arg)
{
	struct dfly_request *req = (struct dfly_request *) arg;
	struct dfly_request *parent_req = req->parent;
	int fuse_internal_req = 0;
#ifdef WAL_DFLY_TRACK
	req->wal_status = 3;
#endif

	if (!req->ops.validate_req(req)) {
		printf("wal_log_complete_single: deprecated req %p parent %p state %d, req_private %p, req_fuse_data %p\n",
		       req, parent_req, req->state, req->req_private, req->req_fuse_data);
		if (req->req_fuse_data) {
			dfly_fuse_release(req);
		}
		dfly_io_put_req(NULL, req); //recycle the req.
		goto log_complete_single_done;
	}

	//void * buff = req->req_private;
	wal_debug("wal_log_complete_single: req %p parent %p state %d, req_private %p, req_fuse_data %p\n",
		  req, parent_req, req->state, req->req_private, req->req_fuse_data);


	assert(req->state == DFLY_REQ_IO_SUBMITTED_TO_WAL ||
	       req->state == DFLY_REQ_IO_INTERNAL_SUBMITTED_TO_WAL ||
	       req->state == DFLY_REQ_IO_WRITE_MISS_SUBMITTED_TO_LOG);

	if (req->state == DFLY_REQ_IO_INTERNAL_SUBMITTED_TO_WAL) {
#ifdef WAL_DFLY_TRACK
		wal_zone_t *zone = wal_get_zone_from_req(req);
		TAILQ_REMOVE_INIT(&zone->debug_queue, req, wal_debuging);
#endif

		dfly_io_put_req(NULL, req); //recycle the req.

		assert(parent_req);
		fuse_internal_req = 1;
		wal_debug("wal_log_complete parent %p, state %d, opc %d, req_fuse_data %p\n",
			  parent_req, parent_req->state,
			  parent_req->ops.get_command(parent_req), parent_req->req_fuse_data);

		if (parent_req->state == DFLY_REQ_IO_SUBMITTED_TO_FUSE)
			parent_req->state = DFLY_REQ_IO_SUBMITTED_TO_WAL;

		req = parent_req;
	}

	if (req->state == DFLY_REQ_IO_WRITE_MISS_SUBMITTED_TO_LOG) {

		struct dfly_key *key = req->ops.get_key(req);
		wal_debug("wal_log_complete_single write_miss req %p, state %d, key: 0x%llx%llx\n",
			  req, req->state, *(long long *)key->key, *(long long *)(key->key + 8));

		req->state = DFLY_REQ_IO_SUBMITTED_TO_WAL;
		req->next_action = DFLY_FORWARD_TO_IO_THREAD;
		goto dfly_handling;
	}

	if (req->ops.get_command(req) == SPDK_NVME_OPC_SAMSUNG_KV_DELETE) {
		req->next_action = DFLY_FORWARD_TO_IO_THREAD;
		if (req->retry_count) {
			printf("wal_log_complete_single delete req %p req_state %d retry_cnt %d\n", req, req->state,
			       req->retry_count);
			//req->retry_count = 0;
		}
	} else {
		if (req->ops.get_command(req) == SPDK_NVME_OPC_SAMSUNG_KV_STORE
		    && req->req_value.length >= (g_wal_conf.wal_cache_object_size_limit_kb * 1024)) {
			req->next_action = DFLY_FORWARD_TO_IO_THREAD;
		} else {
			req->next_action = DFLY_COMPLETE_NVMF;
		}
	}

dfly_handling:

#ifdef WAL_DFLY_TRACK
	if (!fuse_internal_req
	    && (req->next_action == DFLY_FORWARD_TO_IO_THREAD
		|| req->next_action == DFLY_COMPLETE_NVMF)) {
		wal_zone_t *zone = wal_get_zone_from_req(req);
		TAILQ_REMOVE_INIT(&zone->debug_queue, req, wal_debuging);
	}
#endif

	dfly_handle_request(req);

log_complete_single_done:
	wal_debug("wal_log_complete_single: req %p done\n", req);

}

static void wal_log_complete_batch(struct df_dev_response_s resp, void *arg)
{
	wal_dump_hdr_t *dump_hdr = (wal_dump_hdr_t *)arg;
	wal_debug("wal_log_complete_batch entry %d reqs at %p\n",
		  dump_hdr->nr_batch_reqs, dump_hdr);
	assert(resp.rc);
	int32_t nr_dump_req = dump_hdr->nr_batch_reqs;
	int i = 0;
	while (nr_dump_req--) {
		struct dfly_request *req = (struct dfly_request *) dump_hdr->req_list[i++];
		assert(req);
		wal_log_complete_single(resp, req);
	}
	wal_debug("wal_log_complete_batch done %d reqs\n", dump_hdr->nr_batch_reqs);

	crash_test(dump_hdr->nr_batch_reqs);

	dump_hdr->nr_batch_reqs = 0;
}

struct dfly_request dummy_req[256];

#define WAL_IO_SDPK_BUFFER
#undef WAL_IO_SDPK_BUFFER
void wal_flush_complete(struct df_dev_response_s resp, void *arg)
{
	wal_map_item_t *item = (wal_map_item_t *)arg;
	wal_buffer_t *buffer = item->buffer_ptr;
	if (item->key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		dfly_put_key_buff(NULL, item->large_key_buff);
		item->large_key_buff = NULL;
	}
	ATOMIC_DEC_FETCH(buffer->nr_pending_io);
	ATOMIC_INC(buffer->zone->poll_flush_completed);
	if (!ATOMIC_BOOL_COMP_CHX(item->status, WAL_ITEM_FLUSHING, WAL_ITEM_FLUSHED)) {
		wal_debug("wal_flush_complete item not in flushing status");
		assert(0);
	}

	wal_debug("flushed item %p key 0x%llx%llx\n", item,
		  *(long long *)item->key->key, *(long long *)(item->key->key + 8));

	if (!ATOMIC_DEC_FETCH(buffer->nr_objects_inserted)) {
		printf("zone %d flush %lld completed write_miss %lld overwrite %lld invalidate %lld\n",
		       buffer->zone->zone_id, buffer->zone->poll_flush_completed,
		       buffer->zone->write_miss,
		       buffer->zone->nr_overwrite,
		       buffer->zone->nr_invalidate);
#ifdef WAL_USE_PTHREAD
		buffer->zone->flush_state |= WAL_FLUSH_STATE_FLUSH_READY;
		buffer->zone->flush_state &= ~WAL_FLUSH_STATE_LOG_FULL;
#else
		buffer->zone->poll_flush_flag &= ~WAL_POLL_FLUSH_DOING;
		buffer->zone->poll_flush_flag |= WAL_POLL_FLUSH_DONE;
		start_timer(&buffer->zone->flush_idle_ts);
#endif

		if (wal_cache_io_priority == WAL_CACHE_IO_PRIORITY) {
			if (!ATOMIC_DEC_FETCH(nr_watermark_high)) {
				wal_cache_io_priority = WAL_CACHE_IO_REGULAR;
				wal_debug("zone %d, dec wal_cache_io_priority %d\n", buffer->zone->zone_id, wal_cache_io_priority);
			} else {
				wal_debug("zone %d, dec nr_watermark_high %d\n", buffer->zone->zone_id, nr_watermark_high);
			}
		}

	} else {
		//printf("zone %d flush buffer %p unfinished cnt %d, pending %d\n",
		//	buffer->zone->zone_id, buffer, buffer->nr_objects_inserted, buffer->nr_pending_io);
	}

}

int wal_cache_read_object(wal_buffer_t *buffer, wal_object_t *obj,
			  int64_t src_addr, int sz,  int is_data_read)
{
	off64_t addr = (off64_t)src_addr;
	wal_bdev_read_t buffer_read = buffer->ops->dev_io->read;
	int rc = 0;

	if (is_data_read) {
		rc = buffer_read(buffer->fh, (void *)obj->val->value + obj->val->offset, addr, sz, NULL, NULL);
	} else {
		obj->val->value = addr;
	}

	//printf("v_size %d v:0x%llx\n", rc, *(long long *)obj->val->value);
	if (!(ATOMIC_INC_FETCH(buffer->zone->read_hit) % 10000))
		wal_debug("zone %d read_hit %d\n", buffer->zone->zone_id, buffer->zone->read_hit);

	return rc;
}

int wal_log_read_object(wal_buffer_t *buffer, wal_object_t *obj,
			int64_t src_addr, int sz, int is_data_read)
{
	wal_buffer_hdr_t *hdr = &buffer->hdr;
	off64_t addr = (off64_t)src_addr;
	off64_t addr2 = 0;
	wal_bdev_read_t buffer_read = buffer->ops->dev_io->read;
	wal_obj_hdr_t oh;
	wal_obj_checksum_t ch;
	int rc = 0;

	if ((rc = buffer_read(buffer->fh, (void *)&oh, addr, sizeof(wal_obj_hdr_t), NULL, NULL))
	    < sizeof(wal_obj_hdr_t))
		return - WAL_ERROR_RD_LESS;

	addr += rc;

	//verify the object.
	addr2 = addr + oh.key_sz + oh.val_sz;
	if (addr2 >= buffer->hdr.end_addr) {
		return - WAL_ERROR_BAD_ADDR;
	}

	rc = buffer_read(buffer->fh, (void *)&ch, addr2, sizeof(wal_obj_checksum_t), NULL, NULL);
	if (rc < sizeof(wal_obj_checksum_t))
		return -WAL_ERROR_RD_LESS;

	addr2 += rc;

	if (ch.chksum != hdr->sequence) {
		return -WAL_ERROR_BAD_CHKSUM;
	}

	//allocate key and data buffer if not enough.
	if (obj->key->length < oh.key_sz) {
		if (obj->key->key)
			df_free(obj->key->key);
		obj->key->key = df_malloc(oh.key_sz);
		obj->key->length = oh.key_sz;
	}

	if (is_data_read) {
		if (obj->val->length < oh.val_sz) {
			if (obj->val->value)
				df_free(obj->val->value);
			obj->val->value = df_malloc(oh.val_sz);
			obj->val->offset = 0;
			obj->val->length = oh.val_sz;
		}
	}

	//read key
	rc = buffer_read(buffer->fh, (void *)obj->key->key, addr, oh.key_sz, NULL, NULL);
	if (rc < oh.key_sz)
		return -WAL_ERROR_RD_LESS;
	/*
	wal_debug("hdr role %d s_addr 0x%llx key 0x%llx addr 0x%llx seq %ld\n",
				hdr->role, hdr->start_addr, *(long long *)obj->key->key, oh.addr, ch.chksum);
	*/
	//read data
	if (is_data_read) {
		addr += rc;
		rc = buffer_read(buffer->fh, (void *)obj->val->value, addr, oh.val_sz, NULL, NULL);
		if (rc < oh.val_sz)
			return -WAL_ERROR_RD_LESS;

		obj->val->length = oh.val_sz;
		addr += oh.val_sz;
	} else {
		addr += (rc + oh.val_sz);
	}

	rc = addr - src_addr;
	if ((rc + WAL_ALIGN - 1) / __buffer_alignment != oh.sz_blk)
		return -WAL_ERROR_RECORD_SZ;

	obj->key->length = oh.key_sz;

	return oh.sz_blk * __buffer_alignment;

}

void wal_log_sb_init(wal_sb_t *sb)
{
	time_t t;
	sb->version = WAL_VER;
	sb->magic = WAL_MAGIC;
	sb->status = WAL_SB_DIRTY;
	sb->alignment = WAL_ZONE_ALIGN;
	sb->nr_zone = 0;
	time(&t);
	sb->init_timestamp = t;
	sb->reserved[0] = sb->reserved[1] = 0;
	sb->size = sizeof(wal_sb_t);
	sb->offset = ((sb->size & WAL_ZONE_MASK_32) >> WAL_ZONE_OFFSET);
	sb->offset += 1;
}

int wal_log_sb_write(wal_device_info_t *dev_info)
{
	//char layout[1024];
	int rc = WAL_ERROR_WR_PENDING;
	size_t sz = sizeof(wal_sb_t);
	if (dev_info->fh == WAL_INIT_FH)
		return -WAL_ERROR_HANDLE;

	//sprintf(dev_info->log_io_ctx.io_type, "%s", "log_sb\0");
	dev_info->log_io_ctx.io_type = LOG_IO_TYPE_SB_WR;
	dev_info->log_io_ctx.data = dev_info->pool;
	sz = dev_info->dev_io->write(dev_info->fh, (const char *)dev_info->sb,
				     WAL_SB_OFFSET, sz, log_format_io_write_comp, &dev_info->log_io_ctx);
	if (sz != sizeof(wal_sb_t))
		rc = - WAL_ERROR_WR_LESS;

	return rc;
}
int wal_log_show_sb(wal_sb_t *sb)
{
	int i = 0;
	printf("version: %d\nmagic: 0x%lx\nstatus: %d\n",
	       sb->version, (unsigned long)sb->magic, sb->status);
	printf("init_time: %d\nalignment: %d Bytes\ndata offset: 0x%lx MB\n",
	       sb->init_timestamp, sb->alignment, (unsigned long)sb->offset);
	printf("nr_zone: %d\n", sb->nr_zone);
	for (; i < sb->nr_zone; i++)
		printf("addr:	0x%lx\tsize:	0x%lx mb\n",
		       (unsigned long)sb->zone_space[i].addr_mb,
		       (unsigned long)sb->zone_space[i].size_mb);

	return 0;
}

//return 0 for success retrieve a object
/*
typedef struct wal_record_s{
	wal_obj_hdr_t * 		oh;
	wal_obj_checksum_t * 	oc;
	wal_object_t * 			key;
	wal_object_t * 			val;
}wal_record_t;
*/

/*
wal_zone_t * wal_log_get_object_zone(wal_object_t * obj)
{
	int key_int = * (int*)obj->key->key;
	int zone_idx = key_int % __log_sb->nr_zone;
	return __log_dev.zones[zone_idx];
}
*/

wal_zone_t *wal_log_get_object_zone2(wal_subsystem_t *pool, wal_object_t *obj)
{
	if (!obj->key_hashed) {
		obj->key_hash = hash_sdbm(obj->key->key, obj->key->length);
		obj->key_hashed = 1;
	}
	//* (long long *)(obj->key->key + 8);
	int zone_idx = (obj->key_hash % pool->nr_zones) + pool->zone_idx;
	int log_dev_idx = zone_idx % g_wal_conf.wal_nr_log_dev;
	//printf("wal_log_get_object_zone2 [%d][%d]\n", log_dev_idx, zone_idx);
	return __log_dev[log_dev_idx].zones[zone_idx];
}

wal_zone_t *wal_cache_get_object_zone2(wal_subsystem_t *pool, wal_object_t *obj)
{
	if (!obj->key_hashed) {
		obj->key_hash = hash_sdbm(obj->key->key, obj->key->length);
		obj->key_hashed = 1;
	}
	//* (long long *)(obj->key->key + 8);
	int zone_idx = (obj->key_hash % pool->nr_zones) + pool->zone_idx;
	return __cache_dev.zones[zone_idx];
}

inline wal_zone_t *wal_get_zone_from_req(struct dfly_request *req)
{
	int pool_id = wal_get_pool_id(req->req_dfly_ss);

	wal_object_t obj;
	wal_zone_t *zone;

	obj.key = req->ops.get_key(req);
	obj.val = req->ops.get_value(req);
	obj.obj_private = req;
	obj.key_hashed = 0;

	zone = wal_cache_get_object_zone2(&g_wal_ctx.pool_array[pool_id], &obj);
	return zone;
}

void *wal_get_dest_zone_module_ctx(struct dfly_request *req)
{
	wal_zone_t *zone = wal_get_zone_from_req(req);
	return zone->module_instance_ctx;
}

//format given nr of zones
int wal_log_format2(wal_device_info_t *dev_info, int zone_cnt)
{
	int i = dev_info->sb->nr_zone;
	int j = 0;
	int sz_mb = (dev_info->size_mb - 2) / zone_cnt;
	for (; j < zone_cnt; j++) {
		dev_info->zones[i++] = wal_log_zone_create(dev_info, sz_mb,
				       &wal_log_map_ops, &wal_log_buffer_ops);
	}

	if (!wal_log_sb_write(dev_info))
		return WAL_LOG_INIT_FORMATTED;
	else
		return WAL_LOG_INIT_FAILED;
}

//format given nr of zones and size of each.
int wal_log_format(wal_device_info_t *dev_info, int zone_cnt, int sz_mb)
{
	int i = dev_info->sb->nr_zone;
	int j = 0;
	for (; j < zone_cnt; j++) {
		dev_info->zones[i++] = wal_log_zone_create(dev_info, sz_mb,
				       &wal_log_map_ops, &wal_log_buffer_ops);
	}

	if (wal_log_sb_write(dev_info) == WAL_ERROR_WR_PENDING)
		return WAL_LOG_INIT_FORMATTING;
	else
		return WAL_LOG_INIT_FAILED;

}

int wal_cache_shutdown(wal_device_info_t *dev_info)
{
	int nr_zone = __cache_nr_zone;
	int i = nr_zone - 1 ;
	wal_zone_t *zone = NULL;
	for (; i >= 0; i--) {
		zone = dev_info->zones[i];
		//flush the data first
		wal_debug("wal_cache_shutdown __cache_nr_zone %d zone %d\n", __cache_nr_zone, i);
		//wal_zone_notify_flush(zone, WAL_FLUSH_STATE_FLUSH_EXIT);
		zone->poll_flush_flag |= WAL_POLL_FLUSH_EXIT;
#ifdef WAL_USE_PTHREAD
		pthread_join(zone->flush_th, NULL);
#endif
		wal_cache_flush_zone(zone);
		wal_buffer_deinit(zone->log);
		zone->log = NULL;
		wal_buffer_deinit(zone->flush);
		zone->flush = NULL;
		df_free(zone->recovery_buffer);
		df_free(zone);
		zone = NULL;
#ifdef WAL_USE_PTHREAD
		df_free(__wal_cache_buffer[i]);
#else
		spdk_dma_free(__wal_cache_buffer[i]);
#endif
		__wal_cache_buffer[i] = NULL;
	}
	return 0;
}
int wal_log_shutdown(wal_device_info_t *dev_info)
{
	wal_sb_t *sb = dev_info->sb;
	int nr_zone = sb->nr_zone;
	int i = nr_zone - 1 ;
	for (; i > 0; i--) {
		wal_buffer_write_hdr(dev_info->zones[i]->log);
		wal_buffer_deinit(dev_info->zones[i]->log);
		dev_info->zones[i]->log = NULL;
		wal_buffer_write_hdr(dev_info->zones[i]->flush);
		wal_buffer_deinit(dev_info->zones[i]->flush);
		dev_info->zones[i]->flush = NULL;
		df_free(dev_info->zones[i]);
		dev_info->zones[i] = NULL;
	}

	sb->status = WAL_SB_CLEAN;
	wal_log_sb_write(dev_info);
#ifdef WAL_DFLY_BDEV
	spdk_dma_free(__log_sb);
#else
	df_free(__log_sb);
#endif
	return 0;

}

int wal_log_dev_open_2(void *pool, wal_log_device_init_ctx_t *ctx, wal_sb_t **pp_log_sb)
{
	//struct stat stat;
	uint32_t	cnt_blk;
	int64_t 	size_bytes;
	size_t log_sb_pg = 0;
	wal_sb_t *log_sb = * pp_log_sb;
	int rc = WAL_LOG_INIT_FAILED;


	char *dev_name = ctx->device_name;
	size_t nanme_sz = strlen(ctx->device_name);
	wal_device_info_t *dev_info = ctx->log_dev_info;
	int nr_zone = ctx->nr_zones_per_device;
	int zone_sz_mb = ctx->zone_size_mb;
	int open_flags = ctx->open_flags;

	if (dev_info->fh != WAL_INIT_FH) {
		return rc;
	} else {
		dev_info->dev_io = &wal_log_bdev_io;
		dev_info->dev_type = WAL_DEV_TYPE_BLK;
	}

	if (nanme_sz >= WAL_DEV_NAME_SZ || strlen(dev_name) >= WAL_DEV_NAME_SZ)
		goto fail;

	snprintf(dev_info->dev_name, WAL_DEV_NAME_SZ, "%s", dev_name);
	if ((dev_info->fh = dev_info->dev_io->open(dev_name, DFLY_DEVICE_TYPE_BDEV)) < 0)
		return rc;

	log_sb_pg = (sizeof(wal_sb_t) + WAL_PAGESIZE - 1) >> WAL_PAGESIZE_SHIFT;
	log_sb = spdk_dma_malloc(log_sb_pg << WAL_PAGESIZE_SHIFT, WAL_PAGESIZE, NULL);
	if (!log_sb) {
		wal_debug("spdk_dma_malloc fail to alloc sb with size of %d \n", log_sb_pg << WAL_PAGESIZE_SHIFT);
		goto fail;
	}
	size_bytes = dfly_device_getsize(dev_info->fh);
	dev_info->size_mb = (size_bytes) >> 20;

	printf("log dev: name %s write_cached %d bsz %d ssz %d cnt_blk %d size_mb %d\n",
	       dev_info->dev_name, dfly_bdev_write_cached(dev_name),
	       dev_info->bsz, dev_info->ssz, cnt_blk, dev_info->size_mb);

	if (open_flags & WAL_OPEN_DEVINFO) {
		dev_info->dev_io->close(dev_info->fh);
		spdk_dma_free(log_sb);
		return WAL_LOG_INIT_FAILED;
	}

	if (open_flags == WAL_OPEN_FORMAT) {
		rc = -WAL_ERROR_SIGNATURE;
	} else if (open_flags == WAL_OPEN_RECOVER_PAR || open_flags == WAL_OPEN_RECOVER_SEQ) {
		rc = wal_log_sb_read(dev_info, log_sb);
		if (rc == WAL_ERROR_RD_PENDING)
			rc = WAL_LOG_INIT_RECOVERING;
		else
			rc = -WAL_ERROR_SIGNATURE;
	} else {
		assert(0);
	}

	if (rc == -WAL_ERROR_SIGNATURE) {
		wal_log_sb_init(log_sb);
		dev_info->zones = (wal_zone_t **)df_calloc(MAX_ZONE, sizeof(wal_zone_t *));
		dev_info->maps = (wal_map_t **)df_calloc(MAX_ZONE * 2, sizeof(wal_map_t *));
		dev_info->sb = log_sb;
		if (!zone_sz_mb) {
			rc = wal_log_format2(dev_info, nr_zone);
		} else {
			rc = wal_log_format(dev_info, nr_zone, zone_sz_mb);// 4 zone, each 1024 MB for test
		}
	}

	*pp_log_sb = log_sb;

fail:
	wal_debug("wal_log_dev_open_2 rc = %d\n", rc);

	return rc;
}

static int wal_cache_flush_buffer_sync(wal_buffer_t *buffer)
{
	wal_zone_t *zone = buffer->zone;

	wal_key_t key = {0, 0};
	wal_val_t value = {0, 0, 0};
	wal_object_t obj = {&key, &value};

	wal_bucket_t *bucket = NULL;
	wal_map_t *map = buffer->map;
	static long long total_flush_cnt = 0;

	int entry_cnt = 0;
	int rc = 0;

keep_flushing:

	bucket  = zone->flush->map->flush_head;
	//printf("wal_cache_flush_buffer_sync buffer %p nr_insert %d\n",
	//    buffer, buffer->nr_objects_inserted);
	TAILQ_FOREACH_FROM(bucket,  &zone->flush->map->head, link) {
		int cnt = bucket->valid_nr_entries;
		wal_map_item_t *entry = bucket->entry;

		if (!((ATOMIC_READ(buffer->nr_objects_inserted) > 0)
		      && ATOMIC_READ(buffer->nr_pending_io) < zone->poll_flush_max_cnt)) {
			break;
		}

		//printf("bucket %p, cnt %d\n", bucket, cnt);
		while (cnt) {
			assert(entry);
			if (ATOMIC_BOOL_COMP_CHX(entry->status, WAL_ITEM_VALID,  WAL_ITEM_FLUSHING)) {
				buffer->zone->poll_flush_flag &= ~WAL_POLL_FLUSH_DONE;
				buffer->zone->poll_flush_flag |= WAL_POLL_FLUSH_DOING;

				obj.key = entry->key;
				obj.val->length = entry->val_size;
				obj.val->offset = 0;
				obj.val->value = entry->addr;

				struct dfly_key large_key;
				if (entry->key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
					if (!wal_prepare_large_key(&large_key, entry)) {
						obj.key = &large_key;
					} else {
						DFLY_ERRLOG("fail to get dma buff for large key. will recover from during next startup.\n");
						cnt --;
						continue;
					}
				}

				rc = dfly_device_store(zone->tgt_fh, obj.key, obj.val,
						       wal_flush_complete, entry);

				if (rc < 0) {
					wal_debug("flush: wal_cache_flush_object submit store request failed %d\n", rc);
				} else {
					;//printf("flush: req %p status 0x%x submitted\n", req, entry->status);
				}
				entry_cnt ++;
				ATOMIC_INC(buffer->nr_pending_io);
				cnt --;
			} else {
				;//printf("flush: item %p status 0x%x no submittion \n", entry, entry->status);
			}

			entry = entry->next;
		}
		zone->flush->map->flush_head = TAILQ_NEXT(bucket, link);
		if (entry_cnt >= zone->poll_flush_max_cnt) {
			break;
		}

	}

	zone->poll_flush_submission += entry_cnt;
	//if(entry_cnt)
	//   wal_debug("wal_cache_flush_buffer_sync submitted %d flush_max_cnt %d, nr_objects_inserted %d\n", entry_cnt, zone->poll_flush_max_cnt, buffer->nr_objects_inserted);

	if (buffer->zone->poll_flush_flag & WAL_POLL_FLUSH_DOING) {
		goto keep_flushing;
	}

	return;
}

static void wal_cache_flush_zone(wal_zone_t *zone)
{
	wal_cache_flush_buffer_sync(zone->flush);
	zone->flush->map->flush_head = NULL;
	wal_cache_flush_buffer_sync(zone->log);
}

static int wal_prepare_large_key(struct dfly_key *large_key, wal_map_item_t *item)
{
	assert(!item->large_key_buff && item->key->length <= SAMSUNG_KV_MAX_FABRIC_KEY_SIZE);
	item->large_key_buff = dfly_get_key_buff(NULL,
			       SAMSUNG_KV_MAX_FABRIC_KEY_SIZE); //spdk_dma_malloc(SAMSUNG_KV_MAX_FABRIC_KEY_SIZE, 256, NULL);
	if (!item->large_key_buff)
		return -1;

	large_key->key = item->large_key_buff;
	large_key->length = item->key->length;
	memcpy(large_key->key, item->key->key, item->key->length);
	return 0;
}

void wal_cache_flush_spdk_proc(void *context)
{
	wal_zone_t *zone  = (wal_zone_t *) context;
	//wal_fh tgt_fd = zone->tgt_fh;

	struct dfly_request *io_request;

	if (zone->poll_flush_flag & (WAL_POLL_FLUSH_EXIT | WAL_POLL_FLUSH_DONE)) {
		return;
	}

	wal_key_t key = {0, 0};
	wal_val_t value = {0, 0, 0};
	wal_object_t obj = {&key, &value};

	wal_bucket_t *bucket = zone->flush->map->flush_head;
	wal_buffer_t *buffer = zone->flush;
	wal_map_t *map = buffer->map;
	static long long total_flush_cnt = 0;

	int entry_cnt = 0;
	int rc = 0;
	int try_next_round = 0;
	//int cur_pending_io = ATOMIC_READ(buffer->nr_pending_io);
	TAILQ_FOREACH_FROM(bucket, &map->head, link) {

		int cnt = bucket->total_nr_entries;
		wal_map_item_t *entry = bucket->entry;
		//printf("bucket %p, cnt %d\n", bucket, cnt);

		if (zone->poll_flush_flag & WAL_POLL_FLUSH_EXIT)
			break;

		if (ATOMIC_READ(buffer->nr_pending_io) < zone->poll_flush_max_cnt) {
			break;
		}

		while (cnt) {

			/*
			#define WAL_ITEM_VALID		0x0	//
			#define WAL_ITEM_INVALID		0x1	//VALID -> INVALID
			#define WAL_ITEM_FLUSHING		0x2	//VALID -> FLUSHING
			#define WAL_ITEM_FLUSHED		0x3 //FLUSHING -> FLUSHED
			#define WAL_ITEM_DELETED		0x4 //object deleted, but item points to deleted record for log.
			*/
			assert(entry);
			if (ATOMIC_BOOL_COMP_CHX(entry->status, WAL_ITEM_DELETED,  WAL_ITEM_INVALID)) {
				assert(__log_enabled);
				-- bucket->valid_nr_entries;
			} else if (ATOMIC_BOOL_COMP_CHX(entry->status, WAL_ITEM_VALID,  WAL_ITEM_FLUSHING)) {
				obj.key = entry->key;
				obj.val->length = entry->val_size;
				obj.val->offset = 0;
				obj.val->value = entry->addr;

				struct dfly_key large_key;
				if (entry->key->length > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
					if (!wal_prepare_large_key(&large_key, entry)) {
						obj.key = &large_key;
					} else { //not enough pool mem for large key. stop here.
						ATOMIC_BOOL_COMP_CHX(entry->status, WAL_ITEM_FLUSHING, WAL_ITEM_VALID);
						try_next_round = 1;
						goto next_round;
					}
				}

				if (__log_enabled) {
					void *val_addr = entry->addr;
					wal_obj_hdr_t *obj_hdr = (void *)entry->addr;
					obj.val->value = (void *)obj_hdr + __wal_obj_hdr_sz + obj_hdr->key_sz;
				}

#if 0
				io_request = NULL;
				rc = dfly_device_build_request(zone->tgt_fh, SPDK_NVME_OPC_SAMSUNG_KV_STORE,
							       obj.key, obj.val, wal_flush_complete, entry, &io_request);

				//QoS Submit
				qos_request_ops.qos_recv(io_request, zone->poll_qos_request_ctx, zone->poll_qos_client_ctx);

#else
				rc = dfly_device_store(zone->tgt_fh, obj.key, obj.val,
						       wal_flush_complete, entry);
#endif

				if (rc < 0) {
					wal_debug("flush: wal_cache_flush_object submit store request failed %d\n", rc);
				} else {
					;//printf("flush: req %p status 0x%x submitted\n", req, entry->status);
				}
				entry_cnt ++;
				ATOMIC_INC(buffer->nr_pending_io);
			} else {
				wal_debug("flush: bucket %p item %p status 0x%x no submittion \n", bucket, entry, entry->status);
			}
			-- cnt;
			entry = entry->next;
		}
next_round:
		zone->flush->map->flush_head = TAILQ_NEXT(bucket, link);
		if (try_next_round || entry_cnt >= zone->poll_flush_max_cnt) {
			break;
		}

	}

	zone->poll_flush_submission += entry_cnt;

#if defined DFLY_WAL_FLUSH_QOS_ENABLED
	int i, reaped;

	i = 0;
	struct dfly_request *w[64];
	reaped = qos_request_ops.qos_sched(zone->poll_qos_request_ctx, (void **)w, 64);
	for (i = 0; i < reaped; i++) {
		dfly_handle_request(w[i]);
	}
#endif

	//if(entry_cnt)
	//wal_debug("flush submitted %d flush_max_cnt %d, nr_objects_inserted %d\n", entry_cnt, zone->poll_flush_max_cnt, zone->flush->nr_objects_inserted);

	//printf("zone %d map %p unflushed %d flush_submitted %d nr_pending_io %d spdk_flush_pid %d cpu %d\n",
	//	zone->zone_id, map, zone->flush->nr_objects_inserted, zone->poll_flush_submission, buffer->nr_pending_io,
	//	getpid(), sched_getcpu());
	return;

}

int wal_cache_dev_init(wal_device_info_t *dev_info, int nr_zone, int zone_sz_mb, int open_flag)
{
	//struct stat stat;
	uint32_t	cnt_blk;
	int64_t 	size_bytes;
	int rc = 0;
	int i = 0;
	int zone_idx = __cache_nr_zone;
	int64_t addr = 0;

	dev_info->dev_io = &wal_cache_dev_io;
	dev_info->dev_type = WAL_DEV_TYPE_DRAM;

#ifdef WAL_DFLY_BDEV
	//dev_info->fh = dev_info->dev_io->open(__wal_nqn_name, DFLY_DEVICE_TYPE_KV_POOL);
	//printf("cache pool nqn name %s handle %d\n", __wal_nqn_name, dev_info->fh);
#endif

	for (i = 0; i < nr_zone; i++) {
		while (!(__wal_cache_buffer[__cache_nr_zone] = spdk_dma_malloc(zone_sz_mb * MB, 4096, NULL))) {
			wal_debug("df_malloc fail to alloc %d mb cache buffer errno %d\n", zone_sz_mb, errno);
			zone_sz_mb /= 2;
		}
		dev_info->size_mb += zone_sz_mb;
		printf("wal_cache_dev_init: spdk_dma_malloc alloc %d mb cache buffer for zone %d \n", zone_sz_mb,
		       i);
		__cache_nr_zone ++;
	}

	if (!dev_info->zones)
		dev_info->zones = (wal_zone_t **)df_calloc(MAX_ZONE, sizeof(wal_zone_t *));

	if (!dev_info->maps)
		dev_info->maps = (wal_map_t **)df_calloc(MAX_ZONE * 2, sizeof(wal_map_t *));

	for (i = 0; i < nr_zone; i++) {
		wal_zone_t *zone = (wal_zone_t *)df_malloc(sizeof(wal_zone_t));
		df_lock_init(&zone->uio_lock, NULL);
		zone->poll_flush_max_cnt = 32;
		zone->poll_flush_submission = 0;
		zone->poll_flush_flag = WAL_POLL_FLUSH_DONE;
		zone->poll_flush_completed = 0;
		zone->write_miss = 0;
		zone->read_miss = 0;
		zone->read_hit = 0;
		zone->nr_invalidate = 0;
		zone->nr_overwrite = 0;
		zone->dump_timeout_us = g_wal_conf.wal_log_batch_timeout_us;
		zone->log_batch_nr_obj = g_wal_conf.wal_log_batch_nr_obj;
		addr = (int64_t)__wal_cache_buffer[i] +  MB - 1;
		int32_t start_pos_mb = (int32_t)((addr & WAL_ZONE_MASK_64) >> WAL_ZONE_OFFSET);
		//dev_info->fh = i*2;
		zone->log = wal_buffer_create(dev_info, start_pos_mb, WAL_BUFFER_ROLE_LOG, (zone_sz_mb - 1) / 2,
					      &wal_cache_map_ops, &wal_cache_buffer_ops);
		//dev_info->fh = i*2 + 1;
		zone->flush = wal_buffer_create(dev_info, start_pos_mb + zone_sz_mb / 2, WAL_BUFFER_ROLE_FLUSH,
						(zone_sz_mb - 1) / 2, &wal_cache_map_ops, &wal_cache_buffer_ops);
		dev_info->maps[(i + zone_idx) * 2] = zone->log->map;
		dev_info->maps[(i + zone_idx) * 2 + 1] = zone->flush->map;
		zone->log->zone = zone;
		zone->flush->zone = zone;
		zone->flush->map->flush_head = NULL;
		//wal_buffer_write_hdr(zone->log);
		//wal_buffer_write_hdr(zone->flush);
		zone->zone_id = i;
		dev_info->zones[i + zone_idx] = zone;

		df_lock_init(&zone->flush_lock, NULL);
		pthread_cond_init(&zone->flush_cond, NULL);

		zone->flush_state = 0;
		zone->flush_state &= ~WAL_FLUSH_STATE_LOG_FULL;
		zone->flush_state |= WAL_FLUSH_STATE_FLUSH_READY;

		start_timer(&zone->flush_idle_ts);
		TAILQ_INIT(&zone->wp_queue);
#ifdef WAL_DFLY_TRACK
		TAILQ_INIT(&zone->debug_queue);
#endif


		zone->tgt_fh = dfly_device_open(__wal_nqn_name, DFLY_DEVICE_TYPE_KV_POOL, true);

		wal_debug("cache zone[%d] log addr %p: buffer 0x%llx curr_pos 0x%llx end 0x%llx role 0x%x\n", i,
			  __wal_cache_buffer[i],
			  zone->log->hdr.start_addr, zone->log->hdr.curr_pos, zone->log->hdr.end_addr, zone->log->hdr.role);
		wal_debug("cache zone[%d] flush addr %p: buffer 0x%llx curr_pos 0x%llx end 0x%llx role 0x%x\n", i,
			  __wal_cache_buffer[i],
			  zone->flush->hdr.start_addr, zone->flush->hdr.curr_pos, zone->flush->hdr.end_addr,
			  zone->flush->hdr.role);

	}

	printf("wal cache : nr_zone %d zone_sz_mb total cache_size %d\n", __cache_nr_zone,
	       dev_info->size_mb);

fail:
	return rc;
}


void generate_dfly_key(struct dfly_key *d_key, int sz)
{
	char *p = (char *)df_calloc(1, sz);
	if (d_key->key && d_key->length) {
		df_free(d_key->key);
		d_key->length = 0;
	}
	d_key->key = p;
	d_key->length = sz;
	int i = 0;
	while (sz >= 4) {
		*(int32_t *)(p + (i * 4))  = random();
		sz -= 4;
		i++;
	}
}

void generate_dfly_val(struct dfly_value *d_val, int sz)
{
	char *p = (char *)df_calloc(1, sz);
	if (d_val->value && d_val->length) {
		df_free(d_val->value);
		d_val->length = 0;
	}

	d_val->value  = p;
	d_val->length = sz;
	d_val->offset = 0;
	int i = 0;
	while (sz >= 4) {
		*(int32_t *)(p + (i * 4))  = random();
		sz -= 4;
		i++;
	}
}

int wal_get_pool_id(struct dfly_subsystem *pool)
{
	//return pool->num_channels;
	return pool->id;
	//return 0;
}

int wal_dfly_object_io(void *ctx,
		       struct dfly_subsystem *pool, wal_object_t *obj,
		       int opc, int op_flags)
{
	dfly_io_module_context_t *mctx = (dfly_io_module_context_t *)ctx;
	off64_t addr = 0;
	wal_map_item_t *item = NULL;
	int rc = WAL_SUCCESS;

#ifdef WAL_DFLY_TRACK
	if (!((struct dfly_request *)(obj->obj_private))->retry_count) {
		struct dfly_request *req = (struct dfly_request *)obj->obj_private;
		req->cache_rc = -1;
		req->log_rc = -1;
		req->dump_flag = 0;
		req->wal_map_item = 0;
		req->nr_objects_inserted = 0;
		req->dump_blk = 0;
		req->wal_status = 0;
		req->zone_flush_flags = 0;
	}
#endif

	switch (opc) {
	case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
		rc = mctx->io_handlers->store_handler(pool, obj, op_flags);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
		rc = mctx->io_handlers->delete_handler(pool, obj, op_flags);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
		rc = mctx->io_handlers->retrieve_handler(pool, obj, op_flags);
		break;
	default:
		rc = WAL_ERROR_NO_SUPPORT;
		break;
	}

	return rc;
}

int wal_io(struct dfly_request *req, int wal_op_flags)
{
	int opc = req->ops.get_command(req);

	if (!(opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE || opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE ||
	      opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE))
		return WAL_ERROR_NO_SUPPORT;

	wal_object_t obj;
	obj.key = req->ops.get_key(req);
	obj.val = req->ops.get_value(req);
	obj.obj_private = req;
	obj.key_hashed = 0;
	req->opc = opc;
	TAILQ_INIT_ENTRY(&req->wal_pending);
	int rc = wal_dfly_object_io(&wal_module_ctx, req->req_dfly_ss, &obj, opc, wal_op_flags);

	wal_debug("wal_io req %p opc 0x%x key: 0x%llx%llx rc %d\n",
		  req, opc, *(long long *)obj.key->key, *(long long *)(obj.key->key + 8), rc);
	return rc;

}

int wal_pre_retrieve(struct dfly_request *req)
{
	return wal_io(req, DF_OP_LOCK);
}

int wal_post_retrieve(struct dfly_request *req)
{
	return wal_io(req, DF_OP_LOCK);
}

int wal_pre_store(struct dfly_request *req)
{
	return wal_io(req, DF_OP_LOCK);
}

int wal_post_store(struct dfly_request *req)
{
	return wal_io(req, DF_OP_LOCK);
}

int wal_init_by_conf(struct dfly_subsystem *pool, void *arg/*Not used*/,
		     void *cb, void *cb_arg)
{
	if (!g_wal_conf.wal_cache_enabled)
		return WAL_INIT_DONE;

	snprintf(__wal_nqn_name, strlen(g_wal_conf.wal_cache_dev_nqn_name) + 1, "%s",
		 g_wal_conf.wal_cache_dev_nqn_name);
	return wal_init(pool, NULL, NULL,
			g_wal_conf.wal_nr_zone_per_pool_default, g_wal_conf.wal_zone_sz_mb_default, 0,
			g_wal_conf.wal_open_flag,
			cb, cb_arg);
}

//Assumes subsystems are initialized one by one
static struct wal_started_cb_event_s {
	df_module_event_complete_cb df_ss_cb;
	void *df_ss_cb_arg;
	uint32_t src_core;
} wal_cb_event;


void wal_module_load_done_cb(struct dfly_subsystem *pool, void *arg/*Not used*/)
{
	uint32_t icore = spdk_env_get_current_core();
	struct spdk_event *event;
	//wal_cb event to be populated before start
	DFLY_ASSERT(wal_cb_event.df_ss_cb);
	DFLY_ASSERT(wal_cb_event.df_ss_cb_arg);

	//Call cb started event;
	if (icore == wal_cb_event.src_core) {
		wal_cb_event.df_ss_cb(wal_cb_event.df_ss_cb_arg, NULL);
	} else {
		event = spdk_event_allocate(wal_cb_event.src_core, wal_cb_event.df_ss_cb,
					    wal_cb_event.df_ss_cb_arg, NULL);
		spdk_event_call(event);
	}

	//Reset wal cb event
	wal_cb_event.df_ss_cb = NULL;
	wal_cb_event.df_ss_cb_arg = NULL;
}

void wal_module_started_cb(struct dfly_subsystem *pool, void *arg/*Not used*/)
{
	int rc;

	__log_enabled = true;
	start_timer(&wal_log_startup);

	rc = wal_log_init(&__wal_log_init_ctx);
	if (rc == WAL_LOG_INIT_DONE) {
		rc = WAL_INIT_DONE;
		wal_module_load_done_cb(pool, arg);
	}

	if (rc == WAL_LOG_INIT_RECOVERING || rc == WAL_LOG_INIT_FORMATTING)
		rc = WAL_INIT_PENDING;

	if (rc == WAL_LOG_INIT_FAILED)
		rc = WAL_INIT_FAILED;

}

int wal_init(struct dfly_subsystem *pool, struct dragonfly_ops *dops,
	     struct dragonfly_wal_ops *ops, int nr_zones, int size_mb, int no_of_cores, int open_flag
	     , void *cb, void *cb_arg)
{
	int zone_idx = 0;
	int rc = WAL_INIT_PENDING;
	wal_subsystem_t *subsys = NULL;
	df_lock(&g_wal_ctx.ctx_lock);

	if (!g_wal_ctx.dops) {
		g_wal_ctx.dops = dops;
		g_wal_ctx.wops = ops;
		g_wal_ctx.pool_array = (wal_subsystem_t *)df_calloc(
					       DFLY_MAX_NR_POOL, sizeof(dfly_io_module_pool_t));
	}

	int pool_id = wal_get_pool_id(pool);
	if (g_wal_ctx.pool_array[pool_id].dfly_pool) {
		wal_debug("wal_init: pool %p with id %d alread inited\n", pool, pool_id);
		goto done;
	}

	if (size_mb < WAL_ZONE_SZ_MB_MIN)
		size_mb = WAL_ZONE_SZ_MB_MIN;

	if (size_mb > WAL_ZONE_SZ_MB_MAX)
		size_mb = WAL_ZONE_SZ_MB_MAX;

	if (size_mb % 2)
		size_mb += 1;

	//if(size_mb >= WAL_MAX_ALLOC_MB)
	//	size_mb = WAL_MAX_ALLOC_MB - 16;

	if (g_wal_conf.wal_cache_enabled) {
		//cache init
		if (g_wal_ctx.cache_dev != &__cache_dev) {
			g_wal_ctx.cache_dev = &__cache_dev;
			if (__wal_nqn_name[0] == 0) {
				snprintf(__wal_nqn_name, strlen(WAL_NQN_NAME) + 1, "%s", WAL_NQN_NAME);
			}
			__cache_dev.fh = WAL_INIT_FH;
			if (wal_cache_dev_init(&__cache_dev, nr_zones, size_mb, open_flag) == -1) {
				g_wal_ctx.cache_dev = NULL;
				goto done;
			}
		} else {
			zone_idx = __cache_nr_zone;
			if ((rc = wal_cache_dev_init(&__cache_dev, nr_zones, size_mb, open_flag)) == -1) {
				wal_debug("wal_init: failed with rc %d\n", rc);
				goto done;
			}
		}
	}

	g_wal_ctx.pool_array[pool_id].dfly_pool = pool;
	g_wal_ctx.pool_array[pool_id].nr_zones = nr_zones;
	g_wal_ctx.pool_array[pool_id].dfly_pool_id = pool_id;
	g_wal_ctx.pool_array[pool_id].zone_idx = zone_idx;
	g_wal_ctx.nr_pool ++;

	wal_module_ctx.ctx = &g_wal_ctx;

	if (g_wal_conf.wal_log_enabled) {
		__wal_log_init_ctx.curr_device_idx = 0;
		__wal_log_init_ctx.nr_log_devices = g_wal_conf.wal_nr_log_dev;
		__wal_log_init_ctx.open_flags = open_flag;
		__wal_log_init_ctx.nr_zones_per_device = nr_zones;
		__wal_log_init_ctx.zone_size_mb = size_mb;
		__wal_log_init_ctx.pool = pool;
		__wal_log_init_ctx.nr_record_recovered = 0;
		zone_idx = 0;
	}

	if (g_wal_conf.wal_cache_enabled) {
		if (g_wal_conf.wal_log_enabled) {
			wal_cb_event.df_ss_cb = cb;
			wal_cb_event.df_ss_cb_arg = cb_arg;
			dfly_wal_module_init(pool_id, g_wal_conf.wal_nr_cores, wal_module_started_cb,
					     pool);//Pool ID should be dfly subsystem id
		} else {
			dfly_wal_module_init(pool_id, g_wal_conf.wal_nr_cores, cb,
					     cb_arg);//Pool ID should be dfly subsystem id
		}
	}

done:
	df_unlock(&g_wal_ctx.ctx_lock);

	return WAL_INIT_PENDING;
}

int wal_log_init(wal_log_device_init_ctx_t *log_dev_info_ctx)
{
	wal_log_device_init_ctx_t *ctx = log_dev_info_ctx;
	struct dfly_subsystem *pool = ctx->pool;
	int log_init_rc = WAL_LOG_INIT_RECOVERING;
	int i = ctx->curr_device_idx;

	if (i >= ctx->nr_log_devices)
		return WAL_LOG_INIT_DONE;

	for (; i < ctx->nr_log_devices; i++) {
		g_wal_ctx.log_dev[i] = &__log_dev[i];
		g_wal_ctx.log_dev[i]->fh = WAL_INIT_FH;
		g_wal_ctx.log_dev[i]->pool = pool;
		ctx->log_dev_info = g_wal_ctx.log_dev[i];
		ctx->log_dev_info->nr_zone_recovered = 0;
		ctx->log_dev_info->dev_log_recovery_stats = {0, 0, 0};
		ctx->sb = &__log_sb[i];

		ctx->device_name = g_wal_conf.wal_log_dev_name[i];

		log_init_rc = wal_log_dev_open_2(pool, ctx, &__log_sb[i]);

		printf("wal_log_init %s rc %x\n", ctx->device_name, log_init_rc);

		if (log_init_rc == WAL_LOG_INIT_FORMATTED) {
			ctx->curr_device_idx ++;
			ctx->log_dev_info = NULL;
			ctx->sb = NULL;
			ctx->device_name = NULL;
			continue;
		} else {
			break;
		}
	}

	if (log_init_rc == WAL_LOG_INIT_FORMATTED)
		log_init_rc = WAL_LOG_INIT_DONE;


	return log_init_rc;

}

int wal_finish(struct dfly_subsystem *pool)
{
	wal_subsystem_t *subsys = NULL, * temp = NULL;
	int pool_exist = 0;
	assert(pool);
	if (!g_wal_ctx.nr_pool)
		return 0;

	df_lock(&g_wal_ctx.ctx_lock);
	int pool_id = wal_get_pool_id(pool);
	if (pool_id < 0 || pool_id >= DFLY_MAX_NR_POOL) {
		wal_debug("wal_finish: invalid pool_id %d from pool %p, skip\n", pool_id, pool);
		goto finish_skip;
	}

	if (!g_wal_ctx.pool_array[pool_id].dfly_pool || !g_wal_ctx.nr_pool) {
		wal_debug("wal_finish: saved pool=%p, nr_pool=%d, skip\n",
			  g_wal_ctx.pool_array[pool_id].dfly_pool, g_wal_ctx.nr_pool);
		goto finish_skip;
	} else {
		wal_debug("wal_finish: pool_id %d from pool %p\n", pool_id, pool);
	}

	g_wal_ctx.pool_array[pool_id].dfly_pool = NULL;
	g_wal_ctx.nr_pool --;

	if (g_wal_conf.wal_cache_enabled && !g_wal_ctx.nr_pool) {
		wal_cache_shutdown(g_wal_ctx.cache_dev);
	}

	if (__log_enabled && !g_wal_ctx.nr_pool) {
		for (int i = 0; i < g_wal_conf.wal_nr_log_dev; i++) {
			wal_log_shutdown(g_wal_ctx.log_dev[i]);
			g_wal_ctx.log_dev[i]->dev_io->close(g_wal_ctx.log_dev[i]->fh);
			g_wal_ctx.log_dev[i] = NULL;
		}
	}

	if (!g_wal_ctx.nr_pool)
		wal_module_ctx.ctx = NULL;

finish_skip:
	df_unlock(&g_wal_ctx.ctx_lock);
	return 0;
}

void inline wal_lookup_zone(int pool_id, wal_object_t *obj, wal_zone_t **pp_cache_zone,
			    wal_zone_t  **pp_log_zone)
{
	wal_context_t *wal_ctx = wal_module_ctx.ctx;

	if (__log_enabled) {
		*pp_log_zone = wal_log_get_object_zone2(&wal_ctx->pool_array[pool_id], obj);
		assert(*pp_log_zone);
	}

	*pp_cache_zone = wal_cache_get_object_zone2(&wal_ctx->pool_array[pool_id], obj);
	assert(*pp_cache_zone);
	(*pp_cache_zone)->associated_zone = *pp_log_zone;

}

int wal_handle_store_op(struct dfly_subsystem *pool,
			wal_object_t *obj, int op_flags)
{

	off64_t addr = 0;
	wal_map_item_t *cached_map_item = NULL;
	int cache_rc = WAL_SUCCESS, log_rc = WAL_SUCCESS, rc;
	int pool_id = pool->id;
	wal_conf_t *wal_conf = wal_module_ctx.conf;
	wal_zone_t *cache_zone = NULL, * log_zone = NULL;
	wal_dump_info_t  dump_info = {0, 0, DUMP_NOT_READY};

	wal_lookup_zone(pool_id, obj, &cache_zone, &log_zone);
	wal_debug("key %s\n", obj->key->key);

	//no cache/log of big object.
	if (obj->val->length >= (wal_conf->wal_cache_object_size_limit_kb * 1024)) {
		return wal_handle_delete_op(pool, obj, op_flags);
	}

	//cache io first
	if (wal_conf->wal_cache_enabled) {
		//cache the object
		addr = 0;
		if (op_flags & DF_OP_LOCK) df_lock(&cache_zone->uio_lock);

		cache_rc = wal_zone_insert_object(cache_zone, obj, false, &cached_map_item, &dump_info);
#ifdef WAL_DFLY_TRACK
		((struct dfly_request *)(obj->obj_private))->cache_rc = cache_rc;
#endif
		if (cache_rc == WAL_ERROR_WRITE_MISS)
			cache_zone->write_miss ++;

		if (op_flags & DF_OP_LOCK) df_unlock(&cache_zone->uio_lock);
	}

	rc = cache_rc;

	if (!__log_enabled)
		goto store_done;

	if (rc == WAL_SUCCESS && dump_info.dump_flags == DUMP_NOT_READY) {
		rc = WAL_ERROR_LOG_PENDING;
		goto store_done;
	}

	//log io if cache success
	if (__log_enabled && cached_map_item && cache_rc != WAL_ERROR_IO_RETRY) {
		//rc = WAL_SUCCESS_PASS_THROUGH;
		//log the object
		if (op_flags & DF_OP_LOCK) df_lock(&log_zone->uio_lock);

		if (cache_rc == WAL_ERROR_WRITE_MISS) {
			struct dfly_request *req = (struct dfly_request *)obj->obj_private;
			req->state = DFLY_REQ_IO_WRITE_MISS_SUBMITTED_TO_LOG;
			wal_debug("wal_handle_store_op: cache write_miss on item %p, req %p state %d next_action %d\n",
				  cached_map_item, req, req->state, req->next_action);
		}

		if (dump_info.dump_flags & DUMP_SINGLE) {
			wal_obj_hdr_t *oh = (wal_obj_hdr_t *)cached_map_item->addr;
			wal_debug("wal_handle_store_op dump_single with zone[%d] flush_flags %x oh.state %x\n",
				  cache_zone->zone_id, cache_zone->poll_flush_flag, oh->state);
		}

		log_rc = wal_log_insert_object(log_zone, obj, cached_map_item, WAL_MAP_OVERWRITE_APPEND,
					       &dump_info);
		//rc = wal_zone_insert_object(log_zone, obj, false, &cache_map_item);
#ifdef WAL_DFLY_TRACK
		((struct dfly_request *)(obj->obj_private))->log_rc = log_rc;
#endif
		if (op_flags & DF_OP_LOCK) df_unlock(&log_zone->uio_lock);

		if (log_rc != WAL_SUCCESS) {
			wal_conf->wal_log_enabled = false;
			rc = WAL_SUCCESS_PASS_THROUGH;
		} else {
			if (cache_rc == WAL_SUCCESS || cache_rc == WAL_ERROR_WRITE_MISS) {
				//cache done and no problem of log, completion cb to change the status
				rc = WAL_ERROR_LOG_PENDING;
			} else {
				//some problem in log, disable the log and write through
				rc = cache_rc;
			}
		}

	}

store_done:
	wal_debug("map_item %p key: 0x%llx%llx  cache_rc %d log_rc %d rc %d\n",
		  cached_map_item,
		  *(long long *)obj->key->key, *(long long *)(obj->key->key + 8),
		  cache_rc, log_rc, rc);
	return rc;
}

int wal_handle_delete_op(struct dfly_subsystem *pool,
			 wal_object_t *obj, int op_flags)
{
	int pool_id = pool->id;
	wal_map_item_t *cached_map_item = NULL, * log_map_item = NULL;
	int rc = WAL_SUCCESS, cache_rc = WAL_SUCCESS;
	struct dfly_request *req = obj->obj_private;
	wal_conf_t *wal_conf = wal_module_ctx.conf;
	wal_zone_t *cache_zone = NULL, * log_zone = NULL;
	wal_dump_info_t  dump_info = {0, 0, DUMP_NOT_READY};

	wal_lookup_zone(pool_id, obj, &cache_zone, &log_zone);

	if (wal_conf->wal_cache_enabled) {
		if (op_flags & DF_OP_LOCK) df_lock(&cache_zone->uio_lock);
		if (__log_enabled) {
			rc = wal_zone_insert_object(cache_zone, obj, true, &cached_map_item, &dump_info);

#ifdef WAL_DFLY_TRACK
			req->cache_rc = rc;
			req->wal_map_item = cached_map_item;
			req->dump_flag = dump_info.dump_flags;
			req->nr_objects_inserted = cache_zone->log->nr_objects_inserted;
			req->dump_blk = dump_info.dump_blk;
			req->zone_flush_flags = cache_zone->poll_flush_flag;
#endif

			if (rc == WAL_ERROR_WRITE_MISS) { //invalidated in cache
				printf("wal_handle_delete_op req %p write_miss cached_map_item %p\n",
				       req, cached_map_item);
				rc = WAL_ERROR_DELETE_SUCCESS;
			}

			if (rc == WAL_ERROR_DELETE_MISS) {
				cache_rc = rc;
			}
		} else {
			cached_map_item = wal_map_lookup(cache_zone->log, cache_zone->log->map, obj, 0,
							 WAL_MAP_LOOKUP_INVALIDATE);
			cached_map_item = wal_map_lookup(cache_zone->flush, cache_zone->flush->map, obj, 0,
							 WAL_MAP_LOOKUP_INVALIDATE);
			if (cached_map_item && cached_map_item->status != WAL_ITEM_INVALID) {
				wal_debug("wal_handle_delete_op: item %p is status %x\n", cached_map_item, cached_map_item->status);
				rc = WAL_ERROR_IO_RETRY;
			} else {
				rc = WAL_ERROR_DELETE_SUCCESS;
			}
		}
		if (op_flags & DF_OP_LOCK) df_unlock(&cache_zone->uio_lock);

	}


	if (!__log_enabled)
		goto delete_done;

	if (rc == WAL_SUCCESS && dump_info.dump_flags == DUMP_NOT_READY) {
		rc = WAL_ERROR_DELETE_PENDING;
		goto delete_done;
	}

	if (dump_info.dump_flags & DUMP_SINGLE) {
		wal_obj_hdr_t *oh = (wal_obj_hdr_t *)cached_map_item->addr;
		if ( //cache_zone->poll_flush_flag == WAL_POLL_FLUSH_DOING
			// &&
			oh->state != WAL_ITEM_INVALID) {
			printf("wal_handle_delete_op dump_single req %p zone[%d] flush_flags %x oh.state %x rc 0x%x\n",
			       obj->obj_private, cache_zone->zone_id, cache_zone->poll_flush_flag, oh->state, rc);
		}
	}

	//log io if cache success
	if (__log_enabled && rc != WAL_ERROR_IO_RETRY
	    && (cached_map_item || dump_info.dump_flags & DUMP_BATCH)) {

		//log the object
		if (op_flags & DF_OP_LOCK) df_lock(&log_zone->uio_lock);

		//delete logged obj
		rc = wal_log_insert_object(log_zone, obj, cached_map_item, WAL_MAP_OVERWRITE_DELETE, &dump_info);
#ifdef WAL_DFLY_TRACK
		req->log_rc = rc;
#endif
		if (rc == WAL_SUCCESS)
			rc = WAL_ERROR_DELETE_PENDING;
		else
			rc = WAL_ERROR_DELETE_FAIL;

		if (op_flags & DF_OP_LOCK) df_unlock(&log_zone->uio_lock);

		if (rc == WAL_ERROR_DELETE_FAIL) {
			//some problem in log, disable the log and write through
			wal_conf->wal_log_enabled = false;
			__log_enabled = false;
			rc = WAL_SUCCESS_PASS_THROUGH;
		}

	}

delete_done:
	if (cache_rc == WAL_ERROR_DELETE_MISS)
		return cache_rc;
	else
		return rc;
}

int wal_handle_retrieve_op(struct dfly_subsystem *pool,
			   wal_object_t *obj, int op_flags)
{

	wal_map_item_t *item = NULL;
	int rc = WAL_ERROR_READ_MISS;
	wal_context_t *wal_ctx = wal_module_ctx.ctx;
	wal_conf_t *wal_conf = wal_module_ctx.conf;

	//cache io first
	if (wal_conf->wal_cache_enabled) {
		assert(wal_ctx->cache_dev);
		wal_zone_t *cache_zone = wal_cache_get_object_zone2(&wal_ctx->pool_array[pool->id], obj);
		if (op_flags & DF_OP_LOCK) df_lock(&cache_zone->uio_lock);

		wal_map_item_t *map_item = wal_map_lookup(cache_zone->log, cache_zone->log->map, obj, 0,
					   WAL_MAP_LOOKUP_READ);
		if (!map_item) {
			map_item = wal_map_lookup(cache_zone->flush, cache_zone->flush->map, obj, 0, WAL_MAP_LOOKUP_READ);
		}

		if (!map_item) {
			cache_zone->read_miss ++;
		} else {
			rc = WAL_SUCCESS;
			struct dfly_request *req = (struct dfly_request *)obj->obj_private;
			if (req->req_fuse_data) {
				*(uint32_t *)req->req_fuse_data = map_item->val_size;
			} else {
				dfly_resp_set_cdw0(req, map_item->val_size);
			}
			//printf("wal_handle_retrieve_op: cache read OK with key 0x%llx%llx val_size %d\n",
			//    *(long long *)obj->key->key, *(long long *)(obj->key->key+8), obj->val->length);
		}

		if (op_flags & DF_OP_LOCK) df_unlock(&cache_zone->uio_lock);
	}

	return rc;
}



