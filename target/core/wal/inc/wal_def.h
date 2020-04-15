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


#ifndef __WAL_DEF_H
#define __WAL_DEF_H

#include "dragonfly.h"
#include "df_device.h"
#include "df_list.h"
#include "df_wal.h"
#include "df_io_module.h"
#include "df_counters.h"
#include <css_latency.h>

#include <limits.h>

#define WAL_DFLY_BDEV
//#undef WAL_DFLY_BDEV

#define WAL_LOG_ENABLE_DEFAULT	1
//#undef WAL_LOG

#define WAL_CACHE_ENABLE_DEFAULT	1
//#undef WAL_CACHE

#define WAL_NR_CORES_DEFAULT	1

#define WAL_NO_MEMCPY_TEST
#undef WAL_NO_MEMCPY_TEST

#define WAL_CACHE_NO_FLUSH_TEST
#undef WAL_CACHE_NO_FLUSH_TEST

#ifdef WAL_DFLY_BDEV
#define WAL_DEV_LOG_NAME	"walbdevn1"
#define WAL_NQN_NAME		"nqn.2018-01.dragonfly:test1"
#else
#define WAL_DEV_LOG_NAME	"/dev/pmem0"
#endif

#define KB                  (1024)
#define KB_SHIFT			(10)
#define MB                  (1048576)
#define MB_SHIFT			(20)
#define WAL_PAGESIZE		(4096)
#define WAL_PAGESIZE_SHIFT	(12)
#define WAL_DEV_NAME_SZ	128

#define WAL_DEV_TYPE_DRAM	0x0
#define WAL_DEV_TYPE_BLK	0x1

#define	WAL_INIT_FH	-1

#define WAL_MAX_ALLOC_MB		2048
#define WAL_MAX_BUCKET			1048576
#define MAX_ZONE				256

#define WAL_MAGIC	0x20180116
#define WAL_VER		0x01
#define WAL_SB_CLEAN	0x00
#define WAL_SB_DIRTY	0x01

//WAL device open flags
#define WAL_OPEN_FORMAT				0x0001
#define WAL_OPEN_RECOVER_PAR		0x0002
#define WAL_OPEN_RECOVER_SEQ		0x0003
#define WAL_OPEN_DEVINFO			0x0004

//object alignment 1KB
#define WAL_ALIGN				1024
#define WAL_ALIGN_OFFSET		10
#define WAL_ALIGN_MASK			0xFFFFFFFFFFFFFC00

//zone alignment 1MB
#define WAL_ZONE_ALIGN			MB
#define WAL_ZONE_OFFSET			20
#define WAL_ZONE_MASK_32		0xFFF00000
#define WAL_ZONE_MASK_64		0xFFFFFFFFFFF00000

#define WAL_ERROR_HANDLE		0x1001
#define WAL_ERROR_SIGNATURE		0x1003
#define WAL_ERROR_INSERT_OBJ	0x1004
#define WAL_ERROR_BUFFER_ROLE	0x1005
#define WAL_ERROR_BDEV			0x1006


#define WAL_ERROR_TOO_MANY_ZONE		0x2001
#define WAL_ERROR_NO_SPACE			0x2002

#define WAL_ERROR_BAD_ADDR			0x3001
#define WAL_ERROR_BAD_CHKSUM		0x3002
#define WAL_ERROR_RD_LESS			0x3003
#define WAL_ERROR_WR_LESS			0x3004
#define WAL_ERROR_INVALID_SEQ		0x3005
#define WAL_ERROR_RECORD_SZ			0x3006

#define WAL_ERROR_RD_PENDING		0x3007
#define WAL_ERROR_WR_PENDING		0x3008


#define WAL_LOG_INIT_DONE			0x4000
#define WAL_LOG_INIT_FORMATTING		0x4001
#define WAL_LOG_INIT_FORMATTED		0x4002

#define WAL_LOG_INIT_RECOVERING		0x4003
#define WAL_LOG_INIT_RECOVERED		0x4004

#define WAL_LOG_INIT_FAILED			0x4005

#define WAL_SB_OFFSET			0x0
#define WAL_BUFFER_ROLE_BIT			0x1
#define WAL_BUFFER_TYPE_BIT			0x2

#define WAL_BUFFER_ROLE_LOG			0x0
#define WAL_BUFFER_ROLE_FLUSH		0x1
#define WAL_BUFFER_ROLE_DRAM		0x2

#define WAL_ITEM_RECOVER_NEW		0x1
#define WAL_ITEM_RECOVER_OVERWRITE	0x2
#define WAL_ITEM_RECOVER_DELETED	0x4

#define WAL_ITEM_VALID			0x0	//
#define WAL_ITEM_INVALID		0x1	//VALID -> INVALID
#define WAL_ITEM_FLUSHING		0x2	//VALID -> FLUSHING
#define WAL_ITEM_FLUSHED		0x3 //FLUSHING -> FLUSHED
#define WAL_ITEM_DELETED		0x4 //object deleted, but item points to deleted record for log.



#define WAL_FLUSH_STATE_LOG_FULL		0x1
#define WAL_FLUSH_STATE_FLUSH_READY		0x2
#define WAL_FLUSH_STATE_FLUSH_EXIT		0x8

#define WAL_POLL_FLUSH_DONE				0x1
#define WAL_POLL_FLUSH_DOING			0x2
#define WAL_POLL_FLUSH_EXIT				0x4


#define WAL_CACHE_IO_REGULAR	0x0
#define WAL_CACHE_IO_PRIORITY	0x1

#define WAL_DBG(fmt, args...) if(__debug_wal) fprintf(__wal_dbg_fd, fmt, ##args)

#endif

