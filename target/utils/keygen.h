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


#ifndef _JSAHN_KEYGEN_H
#define _JSAHN_KEYGEN_H

#include "adv_random.h"

#ifdef __cplusplus
extern "C" {
#endif

struct keygen_option {
	uint8_t delimiter;
	uint8_t abt_only;
};

struct keygen {
	size_t nprefix;
	size_t abt_array_size;
	struct rndinfo *prefix_len;
	struct rndinfo *prefix_dist;
	struct keygen_option opt;
};

void keygen_init(
	struct keygen *keygen,
	size_t nprefix,
	struct rndinfo *prefix_len,
	struct rndinfo *prefix_dist,
	struct keygen_option *opt);

uint64_t MurmurHash64A(const void *key, int len, unsigned int seed);

void keygen_free(struct keygen *keygen);
uint32_t keygen_idx2crc(uint64_t idx, uint32_t seed);
size_t keygen_seed2key(struct keygen *keygen, uint64_t seed, char *buf);
size_t keygen_seqfill(uint64_t idx, char *buf);

#ifdef __cplusplus
}
#endif

#endif
