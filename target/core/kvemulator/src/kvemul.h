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


#ifndef __MAPPER_H_
#define __MAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct key_s {
	char key[256];
	uint16_t keylen;

	key_s() :
		keylen(0)
	{
	}

	key_s(void *key_, uint16_t keylen_)
	{
		memcpy(key, key_, keylen_);
		this->keylen = keylen_;
	}

	~key_s()
	{
	}

	bool operator==(const struct key_s &right) const
	{
		bool result = (this->keylen == right.keylen && \
			       (memcmp(this->key, right.key, this->keylen) == 0));
		//printf("[%p][%s][%d][%p][%s][%d]\n", this->key, this->key, this->keylen, right.key, right.key, right.keylen);
		return result;
	}

	bool operator!=(const struct key_s &right) const
	{
		return !(*this == right);
	}

	bool operator<(const struct key_s &right) const
	{
		const bool lt = std::make_tuple(memcmp(this->key, right.key, std::min(this->keylen, right.keylen)),
						this->keylen) < std::make_tuple(0, right.keylen);

		return lt;
	}
} map_key_t;

struct map_CMap {
	size_t hash(const map_key_t tr_key) const
	{
		size_t result = 0;
		char *p = (char *)tr_key.key;
		const size_t prime = 31;
		for (size_t i = 0; i < tr_key.keylen; i++) {
			result = p[i] + (result * prime);
		}
		return result;
	}

	bool equal(const map_key_t tr_key1, const map_key_t tr_key2) const
	{
		return (tr_key1 == tr_key2);
//        fprintf(stderr, "map_CMap[%p][%s][%d][%p][%s][%d]\n", tr_key1.key, tr_key1.key, tr_key1.keylen, tr_key2.key, tr_key2.key, tr_key2.keylen);
		if (tr_key1 == tr_key2) {
//            fprintf(stderr, "map_CMap[END-TRUE]\n");
			return true;
		} else {
//            fprintf(stderr, "map_CMap[END-FALSE]\n");
			return false;
		}
	}
};

#ifdef __cplusplus
}
#endif

#endif



