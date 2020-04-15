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

#ifndef DF_ITER_H
#define DF_ITER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dragonfly.h"
#include "df_device.h"

#define MAX_NR_ITER   16
#define MAX_DEV_PER_POOL    256

#define ITER_LIST_ALIGN  4
#define ITER_LIST_ALIGN_MASK  (0xFFFC)

typedef uint8_t kvs_iterator_handle;

typedef enum {
	KVS_ITERATOR_OPEN = 0x1,
	KVS_ITERATOR_CLOSE = 0x2,
	KVS_ITERATOR_KEY = 0x4,           // [DEFAULT] iterator command gets only key entries without value
	KVS_ITERATOR_KEY_VALUE = 0x08,    // [OPTION] iterator command gets key and value pairs
	KVS_ITERATOR_WITH_DELETE = 0x10,  // [OPTION] iterator command gets key and delete

} kvs_iterator_type;

#define KVS_ITERATOR_READ_EOF	0x20


typedef enum {
	KVS_ITERATOR_STATUS_UNKNOWN = 0,  // initial status, not used.
	KVS_ITERATOR_STATUS_CLOSE = 1,	// iter_close completed from all devices.
	KVS_ITERATOR_STATUS_CLOSING = 2,  // iter_close is dispatched and incompleted from all devices.
	KVS_ITERATOR_STATUS_OPENING = 3,  // iter_open is dispatched and incompleted from all devices.
	KVS_ITERATOR_STATUS_OPEN = 4,     // iter_open completed from all devices.
	KVS_ITERATOR_STATUS_READING = 5,     // iter_read is pending from all devices.
	KVS_ITERATOR_STATUS_READ = 6,     // all iter_read return from the devices.

} kvs_iterator_status;

#define DFLY_ITER_IO_PENDING	0x0000
#define DFLY_ITER_IO_FAIL		0x0001


/**
   kvs_iterator_list
   kvs_iterator_list represents an iterator group entries.  it is used for retrieved iterator entries as a return value for  kvs_interator_next() operation. nit specifies how many entries in the returned iterator list(it_list). it_ \
   list has the nit number of <key_length, key> entries when iterator is set with KV_ITERATOR_OPT_KEY and the nit number of <key_length, key, value_length, value> entries when iterator is set with KV_ITERATOR_OPT_KV.
*/
typedef struct {
	uint32_t num_entries;   /*!< the number of iterator entries in the list */
	uint32_t size;          /*!< buffer size */
	int      end;           /*!< represent if there are more keys to iterate (end = 0) or not (end = 1) */
	void    *it_list;       /*!< iterator list buffer */
} kvs_iterator_list;

typedef struct {
	kvs_iterator_handle iter_handle;   /*!< kvssd_iterator */
	uint8_t reserved;
	uint16_t nr_clt;
	uint32_t it_buff_cap;
	void *it_buff;
	uint16_t it_data_sz;
	uint16_t read_pos;
	uint16_t is_eof;			/* status code 0x93 indicates the end */
	uint32_t dev_pool_index; /* idx of device array in pool */
	void *iter_ctx;    /* point to the pool level iter info */
	void *io_device;   /* point to the kvssd device struct dfly_io_device_s */
} dev_iterator_info;

typedef uint8_t dfly_iterator_handle; // to be extended to 16 bit or 32 bit.
typedef struct {
	dfly_iterator_handle dfly_iter_handle;
	int	 pool_fh;
	uint8_t status;                     /*!< iterator status: 1(opened), 0(closed) */
	uint8_t type;                       /*!< iterator type */
	uint8_t keyspace_id;                /*!< KSID that the iterate handle deals with */
	uint32_t bit_pattern;               /*!< bit pattern for condition */
	uint32_t bitmask;                   /*!< bit mask for bit pattern to use */
	uint8_t is_eof;                     /*!< 1 (The iterate is finished), 0 (The iterate is not finished) */
	uint8_t nr_dev;
	uint8_t nr_pending_dev;
	dev_iterator_info dev_iter_info[MAX_DEV_PER_POOL];
	void *dfly_req;
} dfly_iterator_info;

#define iter_log(fmt, args...)\
		DFLY_INFOLOG(DFLY_LOG_ITER, fmt, ##args)

int iter_io(struct dfly_subsystem *pool, struct dfly_request *req, dfly_iterator_info *iter_info);

#ifdef __cplusplus
}
#endif

#endif
