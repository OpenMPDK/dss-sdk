#include "spdk/stdinc.h"

#include "spdk/env.h"

#include "utils/dss_count_latency.h"

static void test_dss_lat(void)
{

#define MAX_TICK (10000000)
#define MAX_DURATION_VARIATION (1000)
#define NUM_ITERATIONS (20)

	struct dss_lat_ctx_s *lctx;
	int i,j;

	uint64_t stick, etick;
	float run_time;

	uint64_t nentries, mem_used;

	lctx = dss_lat_new_ctx("test_counters");

	stick = spdk_get_ticks();

	for(i=0; i < NUM_ITERATIONS; i++) {
		for(j=0; j < MAX_TICK; j++) {
			dss_lat_inc_count(lctx, j);
		}
	}
	etick = spdk_get_ticks();

	nentries = dss_lat_get_nentries(lctx);
	mem_used = dss_lat_get_mem_used(lctx);

	run_time = (float)(etick - stick)/(float)spdk_get_ticks_hz();

	printf("Lookup count (%ld) entries %d times in %.6f seconds\n", nentries, NUM_ITERATIONS, run_time);
	printf("Mem Used: %ld Bytes\n", mem_used);
	printf("Found(%ld) entries: %s\n", nentries, (nentries == MAX_TICK)?"PASS":"FAIL");


	dss_lat_del_ctx(lctx);
}

int main(int argc, char **argv)
{
	struct spdk_env_opts opts;

	spdk_env_opts_init(&opts);
	opts.name = "test_lcounter";
	opts.shm_id = 0;
	if (spdk_env_init(&opts) < 0) {
		fprintf(stderr, "Unable to initialize SPDK env\n");
		return 1;
	}

	test_dss_lat();

	return 0;
}
