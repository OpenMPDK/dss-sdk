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

#include <malloc.h>
#include <string.h>
#include <pthread.h>

#include <Judy.h>

#include <utils/dss_count_latency.h>

struct dss_lat_ctx_s {
	char *name;
	void *jarr;
	uint64_t nsamples;
	int initialized;
	pthread_mutex_t lat_ctx_lock;
};

#define DEFAULT_PARR_COUNT (15)
uint8_t DEFAULT_PARR[] =  {0,1,5,10,20,30,40,50,60,70,80,90,95,99,100};

struct dss_lat_ctx_s * dss_lat_new_ctx(char *name)
{
	struct dss_lat_ctx_s *lctx = (struct dss_lat_ctx_s *)calloc(1, sizeof(struct dss_lat_ctx_s));

	if(lctx) {
		pthread_mutex_init(&lctx->lat_ctx_lock, NULL);
		lctx->name = strdup(name);
		lctx->jarr = NULL;
		lctx->nsamples = 0;
		lctx->initialized = 1;
		return lctx;
	} else {
		return NULL;
	}
}

void dss_lat_del_ctx(struct dss_lat_ctx_s *lctx)
{
	uint64_t mem_freed_count;

	pthread_mutex_lock(&lctx->lat_ctx_lock);
	if(lctx->jarr) {
		mem_freed_count = JudyLFreeArray(&lctx->jarr, PJE0);
		lctx->nsamples = 0;
		lctx->jarr = NULL;
	}
	if(lctx->initialized) {
		free(lctx->name);
		lctx->name = NULL;
		lctx->initialized = 0;
		free(lctx);
	}
	pthread_mutex_unlock(&lctx->lat_ctx_lock);

	return;
}

void dss_lat_reset_ctx(struct dss_lat_ctx_s *lctx)
{
	uint64_t mem_freed_count;

	pthread_mutex_lock(&lctx->lat_ctx_lock);

	if(lctx->jarr) {
		mem_freed_count = JudyLFreeArray(&lctx->jarr, PJE0);
		lctx->jarr = NULL;
	}
	lctx->nsamples = 0;

	pthread_mutex_unlock(&lctx->lat_ctx_lock);

	return;
}

void dss_lat_inc_count(struct dss_lat_ctx_s *lctx, uint64_t duration)
{
	Word_t *value;

	value = (Word_t *)JudyLIns(&lctx->jarr, (Word_t)duration, PJE0);
	(*value)++;
	(lctx->nsamples)++;

	return;
}

uint64_t dss_lat_get_nentries(struct dss_lat_ctx_s *lctx)
{
	Word_t nentries;

	nentries = JudyLCount(lctx->jarr, 0, -1, PJE0);

	return (uint64_t)nentries;
}

uint64_t dss_lat_get_mem_used(struct dss_lat_ctx_s *lctx)
{
	Word_t mem_used;

	mem_used = JudyLMemUsed(lctx->jarr);

	return (uint64_t)mem_used;
}

int _dss_lat_get_percentile(void *jarr, uint64_t nsamples, struct dss_lat_prof_arr **out)
{
	uint64_t curr_latency = 0;
	uint64_t *curr_samples;
	uint64_t cum_samp_count = 0;
	uint64_t next_sample_cnt;
	uint8_t next_prof_index = 0;

	next_sample_cnt = nsamples * (*out)->prof[next_prof_index].pVal/100;

	assert((*out)->prof[next_prof_index].pVal < 100);
	curr_latency = 0;
	curr_samples = (uint64_t *)JudyLFirst(jarr, &curr_latency, PJE0);
	do {
		if(curr_samples) {
			cum_samp_count += *curr_samples;
			if(cum_samp_count >= next_sample_cnt) {
				(*out)->prof[next_prof_index++].pLat = curr_latency;
				if(next_prof_index >= (*out)->n_part) {
					break;
				}

				assert((*out)->prof[next_prof_index].pLat <
						(*out)->prof[next_prof_index - 1].pLat);
				next_sample_cnt = nsamples * (*out)->prof[next_prof_index].pVal/100;
				if(cum_samp_count >= next_sample_cnt) {
					continue;
				}
			}
		} else {
			return -1;
		}

		curr_samples = (uint64_t *)JudyLNext(jarr, &curr_latency, PJE0);
	} while(curr_samples);

	return 0 ;
}

void dss_lat_alloc_profile(struct dss_lat_prof_arr **out)
{
	int i;
	*out = calloc(1, sizeof(struct dss_lat_prof_arr) + DEFAULT_PARR_COUNT * sizeof(struct dss_lat_profile_s));
	(*out)->n_part = DEFAULT_PARR_COUNT;
	for(i=0; i<DEFAULT_PARR_COUNT; i++) {
		(*out)->prof[i].pVal = DEFAULT_PARR[i];
	}
}

int dss_lat_get_percentile(struct dss_lat_ctx_s *lctx, struct dss_lat_prof_arr **out)
{
	if(!out) return;

	if(*out == NULL) {
		dss_lat_alloc_profile(out);

		if(!*out) return -1;
	}

	return _dss_lat_get_percentile(lctx->jarr, lctx->nsamples, out);
}

void dss_lat_get_percentile_multi(struct dss_lat_ctx_s **lctx, int n_ctx, struct dss_lat_prof_arr **out)
{

	void *jarr = NULL;
	int i;

	Word_t *new_arr_entry;
	uint64_t  *lat_entry;
	uint64_t lat;
	uint64_t total_samples = 0;

	if(!out) return;

	if(*out == NULL) {
		dss_lat_alloc_profile(out);
	}

	for(i=0; i<n_ctx; i++) {
		lat = 0;
		lat_entry = (uint64_t *)JudyLFirst(lctx[i]->jarr, &lat, PJE0);

		while(lat_entry) {
			new_arr_entry = (Word_t *)JudyLIns(&jarr, (Word_t)lat, PJE0);

			*new_arr_entry = (*new_arr_entry) + (*lat_entry);
			total_samples += *lat_entry;

			lat_entry = (uint64_t *)JudyLNext(lctx[i]->jarr, &lat, PJE0);
		}
	}

	_dss_lat_get_percentile(jarr, total_samples, out);

	JudyLFreeArray(&jarr, PJE0);

	return;
}
