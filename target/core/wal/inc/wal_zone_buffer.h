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


#ifndef __WAL_ZONE_BUFFER_H
#define __WAL_ZONE_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <wal_map.h>
#include <wal_def.h>

struct wal_zone_s;

#define BUFFER_HDR_PADDING_SZ 936

#define DUMP_NOT_READY		0x00
#define DUMP_BATCH			0x01
#define DUMP_SINGLE			0x02

#define DUMP_IN_PROGRESS	0x04
#define DUMP_DONE			0x08

#define WAL_MAX_DUMP_REQ	350
typedef struct wal_dump_req_list_s {
	int64_t nr_reqs;
	void *req_list;
} wal_dump_req_list_t;

typedef struct wal_dump_hdr_s {
	uint32_t nr_batch_reqs;				//nr of batch dump object
	uint32_t reserved;
	void *req_list[WAL_MAX_DUMP_REQ];	//record the req of the object to be dump in batch way
} wal_dump_hdr_t;

#define WAL_BUFFER_HDR_SZ	1024
#define WAL_DUMP_HDR_SZ		3072


//the data buffer is consisted of array, dump obj group .
//each dump obj group consists of multiple objects to be dumpped in log blk device.
//dump_hdr points to current active dump obj group in cache memory


typedef struct wal_dump_info_s {
	off64_t dump_addr;	//start addr of dump obj group in cache.
	int32_t	dump_blk;	//nr of blk (1k size) to be dumped.
	int32_t	dump_flags;	//mark the dumping status, NOT_READY, BATCH, SINGLE, IN_PROGRESS
	struct timespec dump_ts;
	struct timespec idle_ts;
} wal_dump_info_t;

//ensure the sizeof(wal_buffer_hdr_s) as 1024 bytes for aligment
typedef struct wal_buffer_hdr_s {
	off64_t start_addr;	//start addr of the buffer, include the hdr
	off64_t	end_addr;	//end addr of the buffer, aligned to WAL_ALIGN
	off64_t	curr_pos;	//current addr of data region.
	int32_t role;		//log/flush
	int32_t sequence;	//sequence of this buffer for this operation round, increment during buffer switch,
	//the object recorded in this buffer should checksum with this sequence num.
	int64_t reserved;
	wal_dump_info_t dump_info;	//the state of the current active dump obj group,
	//the dump_addr points to the start of current active dump obj group.
	//the dump_blk is the nr of 1k block of current active dump obj group.
	//wal_dump_hdr_t * dump_hdr = (wal_dump_hdr_t *)dump_info.dump_addr;
	//to access the dump area. the
	char padding[BUFFER_HDR_PADDING_SZ];
} wal_buffer_hdr_t;

struct wal_buffer_s;

//typedef ssize_t (* read_func_t)(int fd, void *buf, size_t sz, off_t offset);
//typedef ssize_t (* write_func_t)(int fd, const void *buf, size_t sz, off_t offset);

typedef int (* read_object_t)(struct wal_buffer_s *buffer, wal_object_t *obj, int64_t src_addr,
			      int sz, int is_data_read);
typedef int (*get_object_size_t)(wal_object_t *obj);

typedef int (* wal_bdev_open_t)(const char *pathname, int flags);
typedef int (* wal_bdev_close_t)(int fd);
typedef int (* wal_bdev_read_t)(int handle, void *buff, uint64_t offset, uint64_t nbytes,
				void *cb, void *cb_arg);
typedef int (* wal_bdev_write_t)(int handle, const void *buff, uint64_t offset, uint64_t nbytes,
				 void *cb, void *cb_arg);

typedef struct wal_device_io_ops {
	wal_bdev_open_t open;
	wal_bdev_close_t close;
	wal_bdev_read_t read;
	wal_bdev_write_t write;
} wal_device_io_ops_t;

typedef struct buffer_ops {
	wal_device_io_ops_t *dev_io;
	read_object_t object_read;
	get_object_size_t get_object_size;
} wal_buffer_ops_t;

#define LOG_IO_TYPE_SB_RD	0x6000
#define LOG_IO_TYPE_SB_WR	0x6001

#define LOG_IO_TYPE_BUFF_HDR_RD	0x6002
#define LOG_IO_TYPE_BUFF_HDR_WR	0x6003

#define LOG_IO_TYPE_DATA_RD	0x6004	// key and value
#define LOG_IO_TYPE_DATA_WR	0x6005	// key and value


typedef struct log_cb_ctx_s {
	int io_type;
	int io_size;
	void *data;
} log_cb_ctx_t;

typedef struct log_recovery_stats_s {
	int nr_obj_record;
	int nr_obj_overwritted;
	int nr_obj_deleted;
} log_recovery_stats_t;

typedef struct wal_buffer_s {
	wal_buffer_hdr_t hdr;
	log_cb_ctx_t	hdr_io_ctx;
	wal_fh			fh;		//fh of the WAL device.
	wal_map_t 	 	 *map;
	wal_buffer_ops_t *ops;
	struct wal_zone_s	 *zone;
	long			nr_objects_inserted;
	long 			nr_pending_io;
	long 			nr_dump_group;

	long			utilization;
	long			total_space_KB;
	long			inc_wmk_change;
	long			dec_wmk_change;
	wal_object_t	temp_obj;
} wal_buffer_t;

typedef struct wal_zone_s {
	int32_t			zone_id; //the index of __wal_zones arrays
	pthread_t 		flush_th;
	df_lock_t		uio_lock;
	df_lock_t		flush_lock;
	df_cond_t 		flush_cond;
	int				flush_state; //log_is_full BIT0, flusher_is_ready BIT1
	wal_fh			tgt_fh;
	void           *poll_qos_client_ctx;
	void           *poll_qos_request_ctx;

	void           *module_instance_ctx;
	TAILQ_HEAD(wal_pending_queue, dfly_request) wp_queue;
#ifdef WAL_DFLY_TRACK
	TAILQ_HEAD(wal_debug_queue, dfly_request) debug_queue;
#endif
	int				poll_flush_max_cnt;//max object to be flushed per poll execution.
	int				poll_flush_flag;//0x1 = flush_done, 0x2 = flushing, 0x4 = exit
	int 			poll_flush_submission;
	long long		poll_flush_completed;
	long long 		write_miss;
	long long		read_miss;
	long long 		read_hit;
	long long 		nr_invalidate;
	long long		nr_overwrite;
	wal_buffer_t *log;
	wal_buffer_t *flush;
	struct wal_zone_s *associated_zone;
	int dump_timeout_us;
	int log_batch_nr_obj;
	struct timespec flush_idle_ts;

	char 			*recovery_buffer;
	log_recovery_stats_t zone_recovery_stats;
} wal_zone_t;

typedef struct wal_space_s {
	int32_t addr_mb;
	int32_t size_mb;
} wal_space_t;

//ensure the size of the log sb size is 4k even.
//To introduce or remove field(s), adjust the padding size accordingly!!!
#define LOG_SB_PADDING_SIZE	2008
typedef struct wal_superblock {
	int32_t size;
	int32_t version;
	uint32_t magic;
	int32_t status;//TBD, clean, dirty.
	int32_t init_timestamp;
	int32_t	alignment;
	int32_t offset;	//data offset
	int32_t	reserved[2];
	int32_t nr_zone;
	wal_space_t zone_space[MAX_ZONE];
	char 	padding[LOG_SB_PADDING_SIZE];
} wal_sb_t;

typedef struct wal_device_info {
	int32_t 	bsz;
	int32_t		ssz;
	int32_t		size_mb;
	int32_t		reserved;
	int32_t		dev_type;
	char		dev_name[WAL_DEV_NAME_SZ];
	wal_fh		fh;
	wal_device_io_ops_t *dev_io;
	wal_sb_t 	*sb;
	log_cb_ctx_t	log_io_ctx;
	wal_zone_t	 **zones;
	wal_map_t **maps;
	void *pool;
	int nr_zone_recovered;
	log_recovery_stats_t dev_log_recovery_stats;
} wal_device_info_t;

wal_zone_t *wal_log_zone_create(wal_device_info_t *dev_info,  int32_t size_mb,
				wal_map_ops_t *map_ops, wal_buffer_ops_t *buffer_ops);

int wal_zone_insert_object(wal_zone_t *zone, wal_object_t *obj,
			   bool for_del, wal_map_item_t **pp_map_item, wal_dump_info_t *return_dump_info);

int wal_zone_object_iteration(wal_zone_t *zone);

wal_buffer_t *wal_buffer_create(wal_device_info_t *dev_info,
				int32_t start_pos_mb, int32_t role, int32_t sz_mb,
				wal_map_ops_t *map_ops, wal_buffer_ops_t *buffer_ops);
int wal_buffer_deinit(wal_buffer_t *buffer);

int wal_buffer_write_hdr(wal_buffer_t *buffer);
int wal_log_iterate_object(wal_buffer_t *buffer, off64_t *pos,
			   wal_object_t *obj, off64_t *val_addr, int is_data_read);
void log_format_io_write_comp(struct df_dev_response_s resp, void *arg);
void log_recover_io_read_comp(struct df_dev_response_s resp, void *arg);

int wal_deletion_object_size(wal_object_t *obj);
int wal_zone_switch_buffer(wal_zone_t *zone);

long long wal_device_object_iterate(wal_device_info_t *dev, int nr_zone);
int wal_zone_notify_flush(wal_zone_t *zone, int event);

#ifdef __cplusplus
}
#endif

#endif //____WAL_ZONE_BUFFER_H
