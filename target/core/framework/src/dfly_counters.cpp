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


#include "dragonfly.h"
#include "df_counters.h"
#include "df_stats.h"
#include "nvmf_internal.h"

#include <time.h>
#include "spdk/nvme_samsung_spec.h"

int dfly_counters_timestamp(void)
{
	return (int)time(NULL);
}

// Note: The stats number may be overflow due to the size limited of uint64_t
// Target agent needs to compare the prev value with new value and make the right action.

// TODO: For GET operation, how to count the size? Send size or actual size? But it's for request. So it should be fine.
int dfly_counters_size_count(stat_kvio_t *stats, struct spdk_nvmf_request *req, int opc)
{
	if (opc != SPDK_NVME_OPC_SAMSUNG_KV_STORE && opc != SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE) {
		return 0;
	}

	struct spdk_nvme_cpl *response = &req->rsp->nvme_cpl;
	uint32_t actualGetLength = 0;
	uint32_t value_size = req->dreq->req_value.length;
	if (opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE) {
		actualGetLength = response->cdw0;
		if (value_size > actualGetLength) {
			value_size = actualGetLength;
		}

		if (value_size >= 2 * MBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->get_large_2MB);
		} else if (value_size >= 1 * MBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->get_1MB_2MB);
		} else if (value_size >= 256 * KBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->get_256KB_1MB);
		} else if (value_size >= 64 * KBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->get_64KB_256KB);
		} else if (value_size >= 16 * KBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->get_16KB_64KB);
		} else if (value_size >= 4 * KBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->get_4KB_16KB);
		} else {
			dfly_ustat_atomic_inc_u64(stats, &stats->get_less_4KB);
		}
	} else if (opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE) {
		if (value_size >= 2 * MBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->put_large_2MB);
		} else if (value_size >= 1 * MBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->put_1MB_2MB);
		} else if (value_size >= 256 * KBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->put_256KB_1MB);
		} else if (value_size >= 64 * KBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->put_64KB_256KB);
		} else if (value_size >= 16 * KBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->put_16KB_64KB);
		} else if (value_size >= 4 * KBYTE) {
			dfly_ustat_atomic_inc_u64(stats, &stats->put_4KB_16KB);
		} else {
			dfly_ustat_atomic_inc_u64(stats, &stats->put_less_4KB);
		}
	}
	return 0;

}

// Note: the first IF statement can be removed if the calling func can do the check.
int dfly_counters_bandwidth_cal(stat_kvio_t *stats, struct spdk_nvmf_request *req, int opc)
{
	if (opc != SPDK_NVME_OPC_SAMSUNG_KV_STORE && opc != SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE) {
		return 0;
	}

	struct spdk_nvme_cpl *response = &req->rsp->nvme_cpl;
	uint32_t actualGetLength = 0;
	uint32_t value_size = req->dreq->req_value.length;
	if (opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE) {
		actualGetLength = response->cdw0;
		if (value_size > actualGetLength) {
			value_size = actualGetLength;
		}
	}

	switch (opc) {
	case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
		dfly_ustat_atomic_add_u64(stats, &stats->putBandwidth, value_size);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
		dfly_ustat_atomic_add_u64(stats, &stats->getBandwidth, value_size);
		break;
	default:
		break;
	}

	return 0;
}

int dfly_counters_increment_io_count(stat_kvio_t *stats, int opc)
{
	switch (opc) {
	case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
		ustat_atomic_inc_u64(stats, &stats->puts);
		break;

	case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
		ustat_atomic_inc_u64(stats, &stats->gets);
		break;

	case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
		ustat_atomic_inc_u64(stats, &stats->dels);
		break;

	case SPDK_NVME_OPC_SAMSUNG_KV_EXIST:
		ustat_atomic_inc_u64(stats, &stats->exists);
		break;

	case SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL:
	case SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_READ:
		ustat_atomic_inc_u64(stats, &stats->iters);
		break;

	default:
		break;
	}
	return 0;
}

void dfly_counters_reset(struct dfly_subsystem *subsystem)
{
	int i;
	dfly_ctrl_t *ctrl;
	struct dfly_qpair_s *dqpair;

	dfly_ustat_reset_kvio_stat(subsystem->stat_kvio);

	for (i=0; i< subsystem->num_io_devices;i++) {
		dfly_ustat_reset_kvio_stat(subsystem->devices[i].stat_io);
		dfly_ustat_reset_block_stat(dfly_bdev_get_ustat_p(subsystem->devices[i].ns->bdev));
	}

	pthread_mutex_lock(&subsystem->ctrl_lock);//Lock Begin
	TAILQ_FOREACH(ctrl, &subsystem->df_ctrlrs, ct_link) {
		TAILQ_FOREACH(dqpair, &ctrl->df_qpairs, qp_link) {
			dfly_qp_reset_counters(dqpair->stat_qpair);
		}
	}
	pthread_mutex_unlock(&subsystem->ctrl_lock);//Release Lock
}
