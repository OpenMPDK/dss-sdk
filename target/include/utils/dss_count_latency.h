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

struct dss_lat_ctx_s;

struct dss_lat_profile_s {
	uint8_t  pVal;
	uint64_t pLat;
};

struct dss_lat_prof_arr {
	uint32_t n_part;
	struct dss_lat_profile_s prof[0];
};

struct dss_lat_ctx_s * dss_lat_new_ctx(char *name);
void dss_lat_del_ctx(struct dss_lat_ctx_s *lctx);
void dss_lat_reset_ctx(struct dss_lat_ctx_s *lctx);
void dss_lat_inc_count(struct dss_lat_ctx_s *lctx, uint64_t duration);
uint64_t dss_lat_get_nentries(struct dss_lat_ctx_s *lctx);
uint64_t dss_lat_get_mem_used(struct dss_lat_ctx_s *lctx);
int dss_lat_get_percentile(struct dss_lat_ctx_s *lctx, struct dss_lat_prof_arr **out);
void dss_lat_get_percentile_multi(struct dss_lat_ctx_s **lctx, int n_ctx, struct dss_lat_prof_arr **out);
