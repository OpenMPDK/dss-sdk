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

#include <stdio.h>
#include <string.h>
#include "keygen.h"

struct key_gen_ctx_s {
	struct keygen keygen;
	uint32_t gen_index;
} g_keygen_ctx;


void dss_keygen_init(uint32_t klen)
{
	struct rndinfo rnd_len[1];
	struct rndinfo rnd_dist[1];
	struct keygen_option opt;

	// For key generate
	rnd_len[0].type = RND_UNIFORM;
	rnd_len[0].a = klen;
	rnd_len[0].b = klen;

	rnd_dist[0].type = RND_NORMAL;
	rnd_dist[0].a = 0;
	rnd_dist[0].b = 0xfffffffffffffff;

	opt.abt_only = 1;
	opt.delimiter = 1;

	memset(&g_keygen_ctx.keygen, 0, sizeof(g_keygen_ctx.keygen));

	keygen_init(&g_keygen_ctx.keygen, 1, rnd_len, rnd_dist, &opt);

	g_keygen_ctx.gen_index = 0;

	return;
}

void dss_keygen_next_key(char *key)
{
	uint32_t  crc;


	crc = keygen_idx2crc(g_keygen_ctx.gen_index, 0);

	BDR_RNG_VARS_SET(crc);
	keygen_seed2key(&g_keygen_ctx.keygen, g_keygen_ctx.gen_index, key);
	BDR_RNG_NEXTPAIR;

	g_keygen_ctx.gen_index++;

	return;
}
