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


#include <ustat.h>
#include <stdlib.h>

#include "nvmf_internal.h"
#include "spdk/nvmf_spec.h"
#include "spdk/log.h"
#include "spdk/bdev_module.h"
#include "df_stats.h"
#include "dragonfly.h"

#define STAT_GNAME_BLOCK_IO "block"

const ustat_class_t ustat_class_blk = {
	.usc_name = "block_io_stats",
	.usc_ctor = NULL,
	.usc_dtor = NULL,
	.usc_bson = NULL,
};


struct stat_blk_io {
	ustat_named_t write;
	ustat_named_t read;
	ustat_named_t write_bw;
	ustat_named_t read_bw;
	ustat_named_t write_less_4KB;
	ustat_named_t write_4KB_16KB;
	ustat_named_t write_16KB_64KB;
	ustat_named_t write_64KB_256KB;
	ustat_named_t write_256KB_1MB;
	ustat_named_t write_1MB_2MB;
	ustat_named_t write_large_2MB;
	ustat_named_t read_less_4KB;
	ustat_named_t read_4KB_16KB;
	ustat_named_t read_16KB_64KB;
	ustat_named_t read_64KB_256KB;
	ustat_named_t read_256KB_1MB;
	ustat_named_t read_1MB_2MB;
	ustat_named_t read_large_2MB;
};

const stat_block_io_t stat_bdev_io_table = {
	{ "write", USTAT_TYPE_UINT64, 0, NULL },
	{ "read", USTAT_TYPE_UINT64, 0, NULL },
	{ "write_bw", USTAT_TYPE_UINT64, 0, NULL},
	{ "read_bw", USTAT_TYPE_UINT64, 0, NULL},
	{ "write_less_4KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "write_4KB_16KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "write_16KB_64KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "write_64KB_256KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "write_256KB_1MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "write_1MB_2MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "write_large_2MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "read_less_4KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "read_4KB_16KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "read_16KB_64KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "read_64KB_256KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "read_256KB_1MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "read_1MB_2MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "read_large_2MB", USTAT_TYPE_UINT64, 0, NULL},
};

int
dfly_ustat_init_bdev_stat(const char *dev_name)
{
	struct spdk_bdev *bdev;

	ustat_handle_t *h;

	h = dfly_ustats_get_handle();

	stat_block_io_t *st_io = NULL;

	bdev = spdk_bdev_get_by_name(dev_name);
	if (bdev == NULL) {
		DFLY_ERRLOG("Could not find bdev '%s'\n", dev_name);
		return(-1);
	}

	st_io = (stat_block_io_t *)ustat_insert(h, dev_name, STAT_GNAME_BLOCK_IO,
					    &ustat_class_blk,
					    sizeof(stat_bdev_io_table) / sizeof(ustat_named_t),
					    &stat_bdev_io_table, NULL);

	if (!st_io) {
		DFLY_WARNLOG("Could not init ustat entry for %s. Statistics will not be available\n", dev_name);
		return (-1);
	}

	bdev->io_stats = st_io;

	return (0);
}

/*
 * Does not guarantee accuracy if IOs are still going on
 */
void dfly_ustat_reset_block_stat(stat_block_io_t *stat)
{

       if(!stat) return;

       dfly_ustat_set_u64(stat, &stat->write,           0);
       dfly_ustat_set_u64(stat, &stat->read,           0);
       dfly_ustat_set_u64(stat, &stat->write_bw,   0);
       dfly_ustat_set_u64(stat, &stat->read_bw,   0);
       dfly_ustat_set_u64(stat, &stat->write_less_4KB,   0);
       dfly_ustat_set_u64(stat, &stat->write_4KB_16KB,   0);
       dfly_ustat_set_u64(stat, &stat->write_16KB_64KB,  0);
       dfly_ustat_set_u64(stat, &stat->write_64KB_256KB, 0);
       dfly_ustat_set_u64(stat, &stat->write_256KB_1MB,  0);
       dfly_ustat_set_u64(stat, &stat->write_1MB_2MB,    0);
       dfly_ustat_set_u64(stat, &stat->write_large_2MB,  0);
       dfly_ustat_set_u64(stat, &stat->read_less_4KB,   0);
       dfly_ustat_set_u64(stat, &stat->read_4KB_16KB,   0);
       dfly_ustat_set_u64(stat, &stat->read_16KB_64KB,  0);
       dfly_ustat_set_u64(stat, &stat->read_64KB_256KB, 0);
       dfly_ustat_set_u64(stat, &stat->read_256KB_1MB,  0);
       dfly_ustat_set_u64(stat, &stat->read_1MB_2MB,    0);
       dfly_ustat_set_u64(stat, &stat->read_large_2MB,  0);

       return;
}

void
dfly_ustat_remove_bdev_stat(stat_block_io_t *stats)
{
	ustat_delete(stats);
	return;
}

int dfly_blk_io_count(stat_block_io_t *stats, int opc, size_t value_size)
{

	if(!stats) return 0;

	if (opc != SPDK_NVME_OPC_READ && opc != SPDK_NVME_OPC_WRITE) {
		return 0;
	}

	if (opc == SPDK_NVME_OPC_READ) {

		ustat_atomic_inc_u64(stats, &stats->read);
		ustat_atomic_add_u64(stats, &stats->read_bw, value_size);
		if (value_size >= 2 * MBYTE) {
			ustat_atomic_inc_u64(stats, &stats->read_large_2MB);
		} else if (value_size >= 1 * MBYTE) {
			ustat_atomic_inc_u64(stats, &stats->read_1MB_2MB);
		} else if (value_size >= 256 * KBYTE) {
			ustat_atomic_inc_u64(stats, &stats->read_256KB_1MB);
		} else if (value_size >= 64 * KBYTE) {
			ustat_atomic_inc_u64(stats, &stats->read_64KB_256KB);
		} else if (value_size >= 16 * KBYTE) {
			ustat_atomic_inc_u64(stats, &stats->read_16KB_64KB);
		} else if (value_size >= 4 * KBYTE) {
			ustat_atomic_inc_u64(stats, &stats->read_4KB_16KB);
		} else {
			ustat_atomic_inc_u64(stats, &stats->read_less_4KB);
		}
	} else if (opc == SPDK_NVME_OPC_WRITE) {
		ustat_atomic_inc_u64(stats, &stats->write);
		ustat_atomic_add_u64(stats, &stats->write_bw, value_size);
		if (value_size >= 2 * MBYTE) {
			ustat_atomic_inc_u64(stats, &stats->write_large_2MB);
		} else if (value_size >= 1 * MBYTE) {
			ustat_atomic_inc_u64(stats, &stats->write_1MB_2MB);
		} else if (value_size >= 256 * KBYTE) {
			ustat_atomic_inc_u64(stats, &stats->write_256KB_1MB);
		} else if (value_size >= 64 * KBYTE) {
			ustat_atomic_inc_u64(stats, &stats->write_64KB_256KB);
		} else if (value_size >= 16 * KBYTE) {
			ustat_atomic_inc_u64(stats, &stats->write_16KB_64KB);
		} else if (value_size >= 4 * KBYTE) {
			ustat_atomic_inc_u64(stats, &stats->write_4KB_16KB);
		} else {
			ustat_atomic_inc_u64(stats, &stats->write_less_4KB);
		}
	}
	return 0;
}
