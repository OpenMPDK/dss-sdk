
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
