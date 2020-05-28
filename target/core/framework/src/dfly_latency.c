
#include <dragonfly.h>


#ifdef DF_LATENCY_MEASURE_ENABLED
#define LOG_SECONDS 2

void df_lat_update_tick(struct dfly_request *dreq, uint32_t state) {
	dreq->lat.tick_arr[state] = spdk_get_ticks();
}

void df_print_tick(struct dfly_request *dreq)
{
	uint64_t req_ticks;

	DFLY_ASSERT(dreq->lat.tick_arr[DF_LAT_REQ_END] >= dreq->lat.tick_arr[DF_LAT_REQ_START]);

	req_ticks = dreq->lat.tick_arr[DF_LAT_REQ_END] - dreq->lat.tick_arr[DF_LAT_REQ_START];

	if(req_ticks/ spdk_get_ticks_hz() > LOG_SECONDS) {
		DFLY_WARNLOG("from:%s [ri:%9d|dio:%9d|ro:%9d]\n", \
				dreq->dqpair?dreq->dqpair->peer_addr:"", \
				dreq->lat.tick_arr[DF_LAT_READY_TO_EXECUTE] - dreq->lat.tick_arr[DF_LAT_REQ_START], \
				dreq->lat.tick_arr[DF_LAT_COMPLETED_FROM_DRIVE] - dreq->lat.tick_arr[DF_LAT_READY_TO_EXECUTE], \
				dreq->lat.tick_arr[DF_LAT_REQ_END] - dreq->lat.tick_arr[DF_LAT_COMPLETED_FROM_DRIVE]
				);
	}
}

#endif


