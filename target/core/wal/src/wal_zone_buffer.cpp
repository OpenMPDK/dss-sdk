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

#include <wal_zone_buffer.h>
extern wal_conf_t g_wal_conf;
extern bool __log_enabled;
extern int __wal_obj_hdr_sz;
extern int	__wal_obj_chksum_sz;

int wal_object_dump_blk(wal_object_t *obj, int is_del)
{
	int record_sz = obj->key->length + obj->val->length +
			__wal_obj_hdr_sz + __wal_obj_chksum_sz;

	if (is_del)
		record_sz -= obj->val->length;

	record_sz += (WAL_ALIGN - 1);
	record_sz &= (WAL_ALIGN_MASK);

	return (record_sz >> WAL_ALIGN_OFFSET);
}

int wal_deletion_object_size(wal_object_t *obj)
{
	int record_sz = obj->key->length + __wal_obj_hdr_sz
			+ __wal_obj_chksum_sz;
	record_sz += (WAL_ALIGN - 1);
	record_sz &= (WAL_ALIGN_MASK);
	return record_sz;
}

long long wal_device_object_iterate(wal_device_info_t *dev, int nr_zone)
{
	int i = 0;
	long long obj_cnt = 0;
	for (; i < nr_zone; i++) {
		if (dev->zones[i])
			obj_cnt += wal_zone_object_iteration(dev->zones[i]);
	}
	return obj_cnt;

}

int wal_zone_object_iteration(wal_zone_t *zone)
{
	int cnt_flush = 0, cnt_log = 0;

	cnt_flush = wal_map_bucket_iteration(zone->flush->map);
	wal_debug("zone %d: object iteration flush %d\n", zone->zone_id, cnt_flush);
	cnt_log = wal_map_bucket_iteration(zone->log->map);
	wal_debug("zone %d: object iteration log %d\n", zone->zone_id, cnt_log);
	return cnt_flush + cnt_log;
}

int wal_log_iterate_object(wal_buffer_t *buffer, off64_t *pos,
			   wal_object_t *obj, off64_t *val_addr, int is_data_read)
{
	wal_buffer_hdr_t *hdr = &buffer->hdr;
	int rc = 0;
	off64_t addr = * pos;

	if (!hdr->sequence) {
		return -WAL_ERROR_INVALID_SEQ;
	}

	if (addr == hdr->start_addr) { // for the first object
		addr = hdr->start_addr + WAL_BUFFER_HDR_SZ + WAL_ALIGN - 1;
		addr = addr & WAL_ALIGN_MASK;
	}

	* val_addr = addr;//return the record addr, not the data addr for lookup table item

	rc = buffer->ops->object_read(buffer, obj, addr, 0, is_data_read);
	if (rc < 0)
		return rc;

	*pos = addr + rc ;
	return 0;

}

int wal_zone_switch_buffer(wal_zone_t *zone)
{
	wal_buffer_t *tmp = zone->log;
	zone->log = zone->flush;
	zone->flush = tmp;

	zone->log->hdr.role &= ~WAL_BUFFER_ROLE_BIT;
	zone->log->hdr.role |= WAL_BUFFER_ROLE_LOG;
	zone->flush->hdr.role &= ~WAL_BUFFER_ROLE_BIT;
	zone->flush->hdr.role |= WAL_BUFFER_ROLE_FLUSH;

//		zone->log->hdr.role &= 0xF0;
//		zone->flush->hdr.role |= WAL_BUFFER_ROLE_FLUSH;

	//reset the new log buffer start addr
	wal_buffer_hdr_t *log_hdr = &(zone->log->hdr);
	off64_t pos = log_hdr->start_addr;

	if (__log_enabled && zone->log->hdr.role & WAL_BUFFER_ROLE_DRAM) {
		log_hdr->dump_info.dump_addr = (pos + WAL_BUFFER_HDR_SZ + WAL_ALIGN - 1) & WAL_ALIGN_MASK;
		log_hdr->dump_info.dump_blk = 0;
		log_hdr->dump_info.dump_flags = DUMP_NOT_READY;

		wal_dump_hdr_t *dump_hdr = (wal_dump_hdr_t *)log_hdr->dump_info.dump_addr;
		//wal_debug("wal_zone_switch_buffer dump_hdr %p\n", dump_hdr);
		dump_hdr->nr_batch_reqs = 0;
		zone->log->nr_dump_group = 0;

		log_hdr->curr_pos = (log_hdr->dump_info.dump_addr + WAL_DUMP_HDR_SZ + WAL_ALIGN - 1) &
				    WAL_ALIGN_MASK;
	} else {
		log_hdr->curr_pos = (pos + WAL_BUFFER_HDR_SZ + WAL_ALIGN - 1) & WAL_ALIGN_MASK;
	}

	//update the log buffer sequence

	time((time_t *)&log_hdr->sequence);
	if (log_hdr->sequence <= zone->flush->hdr.sequence) { //in case the buffer filled in same second
		log_hdr->sequence = zone->flush->hdr.sequence + 1;
	}

	//update the buffer hdr
	if (!(zone->log->hdr.role & WAL_BUFFER_ROLE_DRAM)) {
		//only update log device necessary.
		wal_buffer_write_hdr(zone->log);
		wal_buffer_write_hdr(zone->flush);
	} else {
		zone->log->nr_objects_inserted = 0;
		zone->log->nr_pending_io = 0;
		zone->log->inc_wmk_change = 0;
		zone->log->dec_wmk_change = 0;
		zone->poll_flush_completed = 0;
		zone->flush->map->flush_head = NULL;
		wal_map_reinit(zone->log->map);
	}
	return 0;

}
int wal_zone_notify_flush(wal_zone_t *zone, int event)
{
	wal_zone_t *ctx = zone;
	unsigned int usecs = 100;
test_ready:
	df_lock(&ctx->flush_lock);
	if (ctx->flush_state & WAL_FLUSH_STATE_FLUSH_READY) {
		ctx->flush_state |= event;
		pthread_cond_signal(&ctx->flush_cond);
		wal_debug("wal_zone_notify_flush %d on event %d\n", ctx->zone_id, event);
	} else {
		//flusher is busy...
		wal_debug("wal_zone_notify_flush: zone %d is busy, wait for %d ms\n", ctx->zone_id, usecs);
		usleep(usecs);
		df_unlock(&ctx->flush_lock);
		goto test_ready;
	}
	df_unlock(&ctx->flush_lock);
	return 0;
}
int wal_zone_insert_object(wal_zone_t *zone, wal_object_t *obj,
			   bool for_del, wal_map_item_t **pp_map_item, wal_dump_info_t *return_dump_info)
{
	//printf("zone %d insert object with key 0x%llx ", zone->zone_id, *(long long *)obj->key->data);
	//check if the log if full, if yes, switch the bufffer. TBD

	int record_sz = (for_del) ? wal_deletion_object_size(obj)
			: zone->log->ops->get_object_size(obj);
	int wal_is_ready = 1;
	wal_map_item_t *cached_item = NULL;
	wal_map_item_t *invalidated_item = NULL;
	int bucket_cleaned = 0;
	int rc = WAL_SUCCESS;
	int flush_control = g_wal_conf.wal_cache_flush_threshold_by_count;//INT_MAX;
	//long long nr_objects_inserted = zone->log->nr_objects_inserted;

	assert(pp_map_item);
	wal_dump_info_t *p_dump_info = &(zone->log->hdr.dump_info);
	int buffer_switched = 0;

	if (zone->log->hdr.curr_pos + record_sz + WAL_DUMP_HDR_SZ >= zone->log->hdr.end_addr
	    || ATOMIC_READ(zone->log->nr_objects_inserted) >= flush_control
	   ) {

		if (!(zone->log->hdr.role & WAL_BUFFER_ROLE_DRAM)) {
			//for blk log
			bucket_cleaned = wal_zone_switch_buffer(zone);
			buffer_switched = 1;
		} else {
			//for cache
			wal_zone_t *ctx = zone;
			p_dump_info->dump_flags |= DUMP_BATCH;

			if (zone->poll_flush_flag == WAL_POLL_FLUSH_DONE) {
				if (for_del) {
					printf("zone[%d] to do the buffer switch curr_pos %llx end %llx nr_obj %ld, nr_dump_group %ld, dump_blk %d, dump_flags %x del=%d\n",
					       zone->zone_id, zone->log->hdr.curr_pos, zone->log->hdr.end_addr,
					       zone->log->nr_objects_inserted, zone->log->nr_dump_group, p_dump_info->dump_blk,
					       p_dump_info->dump_flags, for_del);

				}
				if (p_dump_info->dump_blk) {
					* return_dump_info = * p_dump_info;
				}
				bucket_cleaned = wal_zone_switch_buffer(zone);
				buffer_switched = 1;
				/*
				wal_bucket_t * bk = NULL;
				int cnt = 0;
				int bucket_cnt = 0;
				list_for_each_entry(bk, &zone->flush->map->head, link){
					cnt += bk->valid_nr_entries;
					bucket_cnt ++;
				wal_debug("bk %p cnt %d\n", bk, bk->valid_nr_entries);
				}
				wal_debug("after switch map %p bucket %d item %d\n", zone->flush->map, bucket_cnt, cnt);
				//printf("zone %d switch buffer with bucket cleaned %d, nr_object_inserted %lld start %llx ...curr %llx... end %llx\n",
				//			zone->zone_id, bucket_cleaned, zone->flush->nr_objects_inserted, (long long)zone->log->hdr.start_addr,
				//			(long long)zone->log->hdr.curr_pos, (long long)zone->log->hdr.end_addr);
				*/
				zone->poll_flush_flag &= (~WAL_POLL_FLUSH_DONE);
				zone->poll_flush_flag |= WAL_POLL_FLUSH_DOING;
			} else {
				//flusher is busy...
				wal_is_ready = 0 ;
				//search the key in the cache buffers, invalidate the cached item(s) and write through
				//return the map_item if found
				rc = WAL_ERROR_WRITE_MISS;
				invalidated_item = wal_map_lookup(zone->log, zone->log->map, obj, 0, WAL_MAP_LOOKUP_INVALIDATE);
				if (invalidated_item) {
					* pp_map_item = invalidated_item;
				}

				invalidated_item = wal_map_lookup(zone->flush, zone->flush->map, obj, 0, WAL_MAP_LOOKUP_INVALIDATE);
				if (invalidated_item && invalidated_item->status == WAL_ITEM_FLUSHING) {
					printf("wal_zone_insert_object: item %p is under flushing on WAL_MAP_LOOKUP_INVALIDATE\n",
					       invalidated_item);
					rc = WAL_ERROR_IO_RETRY;	//object is under flushing.rare case!!!
					wal_debug("found a io retry case for flushing object 0x%llx%llx. retry it ...\n",
						  *(long long *)obj->key->key, *(long long *)(obj->key->key + 8));
				} else {
					if (!(*pp_map_item))
						*pp_map_item = invalidated_item;
				}

				if (*pp_map_item && return_dump_info) {
					return_dump_info->dump_flags |= DUMP_SINGLE;
				}

				wal_debug("wal_zone_insert_object busy: item %p for key 0x%llx%llx\n",
					  * pp_map_item, *(long long *)obj->key->key, *(long long *)(obj->key->key + 8));

				return rc;
			}
		}

	}

	if (wal_is_ready) {
		if (for_del && (zone->log->hdr.role & WAL_BUFFER_ROLE_DRAM)) { //insert for deletion on cache
			assert(__log_enabled);
			cached_item = wal_map_lookup(zone->log, zone->log->map, obj, 0, WAL_MAP_LOOKUP_DELETE);
			//if(zone->poll_flush_flag & WAL_POLL_FLUSH_DOING)
			invalidated_item = wal_map_lookup(zone->flush, zone->flush->map, obj, 0, WAL_MAP_LOOKUP_INVALIDATE);
			if (invalidated_item && invalidated_item->status != WAL_ITEM_INVALID) {
				wal_debug("wal_zone_insert_object: item %p is status %x\n",
					  invalidated_item, invalidated_item->status);
				rc = WAL_ERROR_IO_RETRY;
			} else {
				rc = WAL_ERROR_DELETE_SUCCESS;
			}
			if (cached_item) {
				* pp_map_item = cached_item;
			} else if (invalidated_item) {
				* pp_map_item = invalidated_item;
				if (return_dump_info) {
					return_dump_info->dump_flags |= DUMP_SINGLE;
				}

				return rc;
			} else {
				rc = WAL_ERROR_DELETE_MISS;
				//assert(0);
				//printf("WAL_ERROR_DELETE_MISS %p\n", obj->obj_private);
			}

			goto zone_insert_done;
		} else { //insert for store
			if (zone->log->hdr.role & WAL_BUFFER_ROLE_DRAM && !__log_enabled)
				cached_item = wal_map_lookup(zone->log, zone->log->map, obj, 0, WAL_MAP_LOOKUP_WRITE_INPLACE);
			else
				cached_item = wal_map_lookup(zone->log, zone->log->map, obj, 0, WAL_MAP_LOOKUP_WRITE_APPEND);

			if (!cached_item) {
				assert(0);
			}
			//if(zone->log->nr_objects_inserted < 10)
			//start_timer(&zone->flush_idle_ts);
		}
	} else {

		//item = wal_map_lookup(zone->log, zone->log->map, obj, 0, WAL_MAP_LOOKUP_WRITE_INPLACE_TRY_BEST);
	}

	if (pp_map_item)
		*pp_map_item = cached_item;

	if (cached_item)
		rc = WAL_SUCCESS;
	else
		rc = WAL_ERROR_IO;

zone_insert_done:

	if (__log_enabled && *pp_map_item && return_dump_info) { //insert one object block in cache

		if (buffer_switched) {
			if (p_dump_info->dump_blk) {
				* return_dump_info = * p_dump_info;
			}
			p_dump_info = &(zone->log->hdr.dump_info);
			wal_debug("wal_zone_insert_object: buffer switch with return_dump_info flags %x blk %d\n",
				  return_dump_info->dump_flags, return_dump_info->dump_blk);

		}

		p_dump_info->dump_blk += wal_object_dump_blk(obj, for_del);
		wal_dump_hdr_t *dump_hdr = (wal_dump_hdr_t *) p_dump_info->dump_addr;
		dump_hdr->req_list[dump_hdr->nr_batch_reqs++] = obj->obj_private;
		if (1 == dump_hdr->nr_batch_reqs) {
			start_timer(&p_dump_info->dump_ts);
		}
		if (dump_hdr->nr_batch_reqs >= zone->log_batch_nr_obj) {
			p_dump_info->dump_flags |= DUMP_BATCH;
			* return_dump_info = * p_dump_info;

			wal_debug("zone[%d] timeout_us %d batch_nr %d\n",
				  zone->zone_id, zone->dump_timeout_us, zone->log_batch_nr_obj);

			zone->dump_timeout_us = MAX(zone->dump_timeout_us - g_wal_conf.wal_log_batch_timeout_us_adjust,
						    g_wal_conf.wal_log_batch_timeout_us);
			zone->log_batch_nr_obj = MIN(zone->log_batch_nr_obj + g_wal_conf.wal_log_batch_nr_obj_adjust,
						     g_wal_conf.wal_log_batch_nr_obj);

			if (!buffer_switched) {
				//prepare an new dump obj group.
				p_dump_info->dump_addr = zone->log->hdr.curr_pos;
				p_dump_info->dump_blk = 0;
				p_dump_info->dump_flags = DUMP_NOT_READY;
				p_dump_info->dump_ts = {0};
				zone->log->hdr.curr_pos += WAL_DUMP_HDR_SZ;
				dump_hdr = (wal_dump_hdr_t *)p_dump_info->dump_addr;
				dump_hdr->nr_batch_reqs = 0;
				start_timer(&p_dump_info->idle_ts);
				zone->log->nr_dump_group ++;
			}
		}

		wal_debug("return_dump: req %p nr_reqs %x, addr 0x%llx blk 0x%x flags 0x%x switch %d log_dump: addr 0x%llx"
			  " blk 0x%x flags 0x%x\n", obj->obj_private, dump_hdr->nr_batch_reqs,
			  return_dump_info->dump_addr, return_dump_info->dump_blk, return_dump_info->dump_flags,
			  buffer_switched,
			  p_dump_info->dump_addr, p_dump_info->dump_blk, p_dump_info->dump_flags);


	}

	return rc;
}

wal_buffer_t *wal_buffer_create(wal_device_info_t *dev_info,
				int32_t start_pos_mb, int32_t role, int32_t sz_mb,
				wal_map_ops_t *map_ops, wal_buffer_ops_t *buffer_ops)
{
	wal_buffer_t *buffer = NULL;
	off64_t pos = start_pos_mb;
	int32_t buffer_sz_4k = (sizeof(wal_buffer_t) + WAL_PAGESIZE - 1) >> WAL_PAGESIZE_SHIFT;
	//if log buffer, to be malloced by spdk_dma_malloc with 4k alignment
	if (dev_info->dev_type == WAL_DEV_TYPE_BLK) {
		//printf("wal_buffer_create spdk_dma_malloc\n");
		buffer = (wal_buffer_t *)spdk_dma_malloc(buffer_sz_4k << WAL_PAGESIZE_SHIFT, WAL_PAGESIZE, NULL);
	} else if (dev_info->dev_type == WAL_DEV_TYPE_DRAM) {
		buffer = df_calloc(1, buffer_sz_4k << WAL_PAGESIZE_SHIFT);
	} else {
		assert(0);
	}
	wal_buffer_hdr_t *buffer_hdr = (wal_buffer_hdr_t *)buffer;
	if (g_wal_conf.wal_log_enabled && dev_info->dev_type == WAL_DEV_TYPE_DRAM) {
		buffer_hdr->dump_info.dump_addr = ((pos << MB_SHIFT) + WAL_BUFFER_HDR_SZ + WAL_ALIGN - 1) &
						  WAL_ALIGN_MASK;
		buffer_hdr->dump_info.dump_blk = 0;
		buffer_hdr->dump_info.dump_flags = DUMP_NOT_READY;
		start_timer(&buffer_hdr->dump_info.idle_ts);
		wal_dump_hdr_t *dump_hdr = (wal_dump_hdr_t *)buffer_hdr->dump_info.dump_addr;
		wal_debug("wal_buffer_create dump_hdr %p\n", dump_hdr);
		dump_hdr->nr_batch_reqs = 0;
		buffer_hdr->curr_pos = (buffer_hdr->dump_info.dump_addr + WAL_DUMP_HDR_SZ + WAL_ALIGN - 1) &
				       WAL_ALIGN_MASK;
	} else {
		buffer_hdr->curr_pos = (pos << MB_SHIFT) + WAL_BUFFER_HDR_SZ + WAL_ALIGN - 1;
		buffer_hdr->curr_pos = buffer_hdr->curr_pos & WAL_ALIGN_MASK;
	}

	buffer_hdr->start_addr = pos << MB_SHIFT;
	pos = start_pos_mb + sz_mb;
	buffer_hdr->end_addr = (pos) << MB_SHIFT;
	buffer_hdr->end_addr -= WAL_ALIGN;
	buffer->total_space_KB = sz_mb << KB_SHIFT;
	buffer_hdr->role = role;
	if (dev_info->dev_type == WAL_DEV_TYPE_DRAM)
		buffer_hdr->role |= WAL_BUFFER_ROLE_DRAM;

	buffer_hdr->sequence = 0;

	buffer->fh = dev_info->fh;
	buffer->nr_objects_inserted = 0;
	buffer->nr_pending_io = 0;
	wal_debug("buffer start 0x%llx dump_addr 0x%llx curr_pos 0x%llx end 0x%llx\n",
		  (long long)buffer_hdr->start_addr,
		  (long long)buffer_hdr->dump_info.dump_addr,
		  (long long)buffer_hdr->curr_pos,
		  (long long)buffer_hdr->end_addr);

	//setup lookup map
	if (dev_info->dev_type == WAL_DEV_TYPE_DRAM)
		buffer->map = wal_map_create(buffer, WAL_MAX_BUCKET, map_ops);
	else
		buffer->map = NULL;

	buffer->ops = buffer_ops;

	return buffer;

}


/*
		wal_format format the wal log device given zone space information.
			  	- wal_create_zone, with space information, determines the space range and split the into log and flush dual buffers
					- wal_create_buffer populate the buffer hdr info and create associated table.
						- get fh of the wal device.
						- wal_create_lookup_table create the lookup table per buffer
						- wal_write_buffer_hdr init buffer hdr on wal device.
					- assign zone id for this zone.
				- wal_sb_write to write sb info on wal device.
*/

wal_zone_t *wal_log_zone_create(wal_device_info_t *dev_info,  int32_t size_mb,
				wal_map_ops_t *map_ops, wal_buffer_ops_t *buffer_ops)
{
	wal_zone_t *zone = 0;
	wal_space_t *space;
	wal_sb_t *sb = dev_info->sb;
	int32_t  off_s = sb->offset;
	//wal_buffer_hdr_t * buff_hdr = 0;

	// be conservative of log size, set twice size of the cache size per zone
	// in case of flush invalidate and cache full (we need more log space to
	// record the deletion object, it's write miss, worst case)
	size_mb += size_mb;

	if (sb->nr_zone >= MAX_ZONE) {
		return NULL;
	}

	if (sb->nr_zone) {
		space = &sb->zone_space[sb->nr_zone - 1];
		off_s = space->addr_mb + space->size_mb;
	}

	if ((off_s + size_mb) > (dev_info->size_mb - 1)) {
		wal_debug("space shortage fro the zone (%llx:%lx mb)\n", (long long)off_s, (unsigned long)size_mb);
		return NULL;
	}

	sb->zone_space[sb->nr_zone].addr_mb = off_s;
	sb->zone_space[sb->nr_zone].size_mb = size_mb;

	zone = (wal_zone_t *)df_malloc(sizeof(wal_zone_t));

	df_lock_init(&zone->uio_lock, NULL);

//	zone->poll_flush_max_cnt = 32;
//	zone->poll_flush_submission = 0;
	zone->poll_flush_flag = WAL_POLL_FLUSH_DONE;

	zone->log = wal_buffer_create(dev_info, off_s, WAL_BUFFER_ROLE_LOG, size_mb / 2, map_ops,
				      buffer_ops);
	zone->flush = wal_buffer_create(dev_info, off_s + size_mb / 2, WAL_BUFFER_ROLE_FLUSH, size_mb / 2,
					map_ops, buffer_ops);
	zone->log->zone = zone;
	zone->flush->zone = zone;
	zone->nr_invalidate = 0;
	zone->nr_overwrite = 0;
	time((time_t *)&zone->flush->hdr.sequence);
	zone->log->hdr.sequence = zone->flush->hdr.sequence + 1;
	dev_info->maps[sb->nr_zone * 2] = zone->log->map;
	dev_info->maps[sb->nr_zone * 2 + 1] = zone->flush->map;
	zone->zone_id = sb->nr_zone;
	sb->nr_zone ++;
	wal_buffer_write_hdr(zone->log);
	wal_buffer_write_hdr(zone->flush);
	return zone;
}



// write or update the on disk buffer header
int wal_buffer_write_hdr(wal_buffer_t *buffer)
{
	buffer->hdr_io_ctx.io_type = LOG_IO_TYPE_BUFF_HDR_WR;
	size_t sz = buffer->ops->dev_io->write(buffer->fh, &buffer->hdr,
					       buffer->hdr.start_addr, WAL_BUFFER_HDR_SZ, log_format_io_write_comp, &buffer->hdr_io_ctx);
	wal_debug("wal_buffer_write_hdr zone id %d pos 0x%llx sz %d\n",
		  buffer->zone->zone_id, buffer->hdr.start_addr, sz);
	return (sz == WAL_BUFFER_HDR_SZ) ? 0 : -1;
}

int wal_buffer_deinit(wal_buffer_t *buffer)
{
	if (buffer) {
		if (buffer->map) {
			if (buffer->map->table) {
				//iterate the hash table to free the entry list.. TBD
				df_free(buffer->map->table);
			}
			df_free(buffer->map);
		}
		if (buffer->fh != WAL_INIT_FH) {
			close(buffer->fh);
		}
		spdk_dma_free(buffer);
	}
	return 0;
}


