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

#include <module_hash.h>

uint32_t hash_sdbm(const char *data, int sz)
{
	uint32_t hash = 0;
	int count = 0;
	while (count < sz) {
		hash = data[count] + (hash << 6) + (hash << 16) - hash;
		count++;
	}
	return hash;
}

uint32_t hash_djb2(const char *data, int sz)
{

	uint32_t hash = 5381;
	int count = 0;
	while (count < sz) {
		hash = ((hash << 5) + hash) + data[count];
		count++;
	}
	return hash;
}

uint32_t hash_adler32(const char *data, int sz)
{
	const uint8_t *buffer = (const uint8_t *)data;

	uint32_t s1 = 1;
	uint32_t s2 = 0;
	size_t n = 0 ;
	for (n = 0; n < sz; n++) {
		s1 = (s1 + buffer[n]) % 65521;
		s2 = (s2 + s1) % 65521;
	}
	return (s2 << 16) | s1;
}


#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

uint32_t hash_lookup3(
	const void *key,
	size_t      length,
	uint32_t    initval
)
{
	uint32_t  a, b, c;
	const uint8_t  *k;
	const uint32_t *data32Bit;

	data32Bit = (const uint32_t *)key;
	a = b = c = 0xdeadbeef + (((uint32_t)length) << 2) + initval;

	while (length > 12) {
		a += *(data32Bit++);
		b += *(data32Bit++);
		c += *(data32Bit++);
		mix(a, b, c);
		length -= 12;
	}

	k = (const uint8_t *)data32Bit;
	switch (length) {
	case 12:
		c += ((uint32_t)k[11]) << 24;
	case 11:
		c += ((uint32_t)k[10]) << 16;
	case 10:
		c += ((uint32_t)k[9]) << 8;
	case 9 :
		c += k[8];
	case 8 :
		b += ((uint32_t)k[7]) << 24;
	case 7 :
		b += ((uint32_t)k[6]) << 16;
	case 6 :
		b += ((uint32_t)k[5]) << 8;
	case 5 :
		b += k[4];
	case 4 :
		a += ((uint32_t)k[3]) << 24;
	case 3 :
		a += ((uint32_t)k[2]) << 16;
	case 2 :
		a += ((uint32_t)k[1]) << 8;
	case 1 :
		a += k[0];
		break;
	case 0 :
		return c;
	}
	final(a, b, c);
	return c;
}

