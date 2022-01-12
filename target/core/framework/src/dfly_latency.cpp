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

#include <dragonfly.h>
#include "nvmf_internal.h"


#ifdef DF_LATENCY_MEASURE_ENABLED

void df_lat_update_tick(struct dfly_request *dreq, uint32_t state) {
	dreq->lat.tick_arr[state] = spdk_get_ticks();
}

void df_print_tick(struct dfly_request *dreq)
{
	uint64_t req_ticks;
	struct spdk_nvmf_request *nvmf_req = dreq?dreq->req_ctx:NULL;

	DFLY_ASSERT(dreq->lat.tick_arr[DF_LAT_REQ_END] >= dreq->lat.tick_arr[DF_LAT_REQ_START]);

	req_ticks = dreq->lat.tick_arr[DF_LAT_REQ_END] - dreq->lat.tick_arr[DF_LAT_REQ_START];

	if(g_dragonfly->req_lat_to && req_ticks/ spdk_get_ticks_hz() > g_dragonfly->req_lat_to && nvmf_req && dreq->dqpair) {
		uint64_t ri, dio, ros, ro;
		uint64_t ticks_per_us = spdk_get_ticks_hz()/1000000;

		ri  = (dreq->lat.tick_arr[DF_LAT_READY_TO_EXECUTE] - dreq->lat.tick_arr[DF_LAT_REQ_START])/ticks_per_us;
		dio = (dreq->lat.tick_arr[DF_LAT_COMPLETED_FROM_DRIVE] - dreq->lat.tick_arr[DF_LAT_READY_TO_EXECUTE])/ticks_per_us;
		ros  = (dreq->lat.tick_arr[DF_LAT_RO_STARTED] - dreq->lat.tick_arr[DF_LAT_COMPLETED_FROM_DRIVE])/ticks_per_us;
		ro  = (dreq->lat.tick_arr[DF_LAT_REQ_END] - dreq->lat.tick_arr[DF_LAT_RO_STARTED])/ticks_per_us;
		DFLY_WARNLOG("from:%s qid:%3d cid:%3d [ri:%3d|dio:%5d|ros:%9d|ro:%9d]\n", \
				dreq->dqpair?dreq->dqpair->peer_addr:"", nvmf_req->qpair->qid, \
				nvmf_req->cmd->nvme_cmd.cid,
				ri, dio, ros, ro\
				);
	}
}

void df_update_lat_us(struct dfly_request *dreq)
{
	uint64_t req_ticks;
	uint64_t ticks_per_us = spdk_get_ticks_hz()/1000000;
	uint64_t latency_us;

	req_ticks = dreq->lat.tick_arr[DF_LAT_REQ_END] - dreq->lat.tick_arr[DF_LAT_REQ_START];

	latency_us = req_ticks/ticks_per_us;

	if(dreq->dqpair) {
#ifndef DSS_OPEN_SOURCE_RELEASE
		dss_lat_inc_count(dreq->dqpair->lat_ctx, latency_us);
#endif
	}

}

#endif
