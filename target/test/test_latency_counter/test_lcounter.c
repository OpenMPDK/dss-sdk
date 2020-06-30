#include "spdk/stdinc.h"

#include "spdk/env.h"

#include "utils/dss_count_latency.h"

static void test_dss_lat(void)
{

#define MAX_TICK (1000000)
#define MAX_DURATION_VARIATION (1000)
#define NUM_ITERATIONS (20)

	struct dss_lat_ctx_s *lctx;
	struct dss_lat_ctx_s *lctx_multi[2];
	int i,j;

	uint64_t stck, etck;
	float run_time;

	uint64_t nentries, mem_used;

	struct dss_lat_prof_arr *lprof;

	lctx = dss_lat_new_ctx("test_counters");

	stck = spdk_get_ticks();

	for(i=0; i < NUM_ITERATIONS; i++) {
		for(j=0; j < MAX_TICK; j++) {
			dss_lat_inc_count(lctx, j);
		}
	}
	etck = spdk_get_ticks();

	nentries = dss_lat_get_nentries(lctx);
	mem_used = dss_lat_get_mem_used(lctx);

	run_time = (float)(etck - stck)/(float)spdk_get_ticks_hz();

	printf("Lookup count (%ld) entries %d times in %.6f seconds\n", nentries, NUM_ITERATIONS, run_time);

	printf("Mem Used: %ld Bytes\n", mem_used);
	printf("Found(%ld) entries: %s\n", nentries, (nentries == MAX_TICK)?"PASS":"FAIL");

	lprof = NULL;
	dss_lat_get_percentile(lctx, &lprof);

	uint32_t passed = 1;
	const uint64_t lat_check_const = (MAX_TICK/100);
AGAIN:
	for(i=0; i<lprof->n_part; i++) {
		if(lprof->prof[i].pLat/lprof->prof[i].pVal != lat_check_const) {
			passed = 0;
			printf("Failed latency at index %d w/ (%lu/%u) = %lu\n", i, lprof->prof[i].pLat, lprof->prof[i].pVal, lprof->prof[i].pLat/lprof->prof[i].pVal);
			break;
		}
	} free(lprof);
	printf("Latecy percentile check: %s\n", passed?"PASS":"FAIL");
	if(passed == 2 || passed == 0) {
		goto END;
	}

	lctx_multi[0] = lctx;
	lctx_multi[1] = lctx;

	lprof = NULL;
	dss_lat_get_percentile_multi(lctx_multi, 2, &lprof);
	passed = 2;goto AGAIN;

END:
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
