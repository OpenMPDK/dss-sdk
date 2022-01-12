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
