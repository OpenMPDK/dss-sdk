/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2023 Samsung Electronics Co., Ltd.
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

#include "kvtrans_hash.h"

/* select the ($mask_bit)th reversely bit from each Byte of $buf, 
where $buf is a 32 Byte buffer.*/
static uint32_t take_bit(BYTE* buf, uint32_t mask_bit)
{
    int idx;
    uint32_t mb;
    uint32_t bits = 0x00000000;

    for(idx=0;idx<SHA256_BLOCK_SIZE;idx++)
    {
        mb = buf[idx] & mask_bit;
        bits |= ( mb << idx)|(mb >> ((SHA256_BLOCK_SIZE - 1) - idx));
    }
    return bits;
}

static uint32_t take_byte(BYTE* buf, uint32_t tryout, uint8_t hash_size)
{
    uint32_t bits = 0x00000000;
    int idx;
    uint32_t start = tryout;
    // take four bytes
    uint32_t end = (tryout + 4) % hash_size;

    idx = start;
    while (idx!=end) {
        bits |= buf[idx] << (idx*8);
        idx = (idx + 1) % hash_size;
    }

    return bits;
}

static uint8_t first_call(hash_fn_ctx_t *hash_fn_ctx)
{
    return hash_fn_ctx->tryout == 0;
}

void SHA256_init(hash_fn_ctx_t *hash_fn_ctx) 
{   
    assert(hash_fn_ctx);
    hash_fn_ctx->sha256_ctx = (void*) malloc(sizeof(SHA256_CTX));
    if (!hash_fn_ctx->sha256_ctx) {
        printf("ERROR: malloc sha256_ctx failed. \n");
        return;
    }

    sha256_init( (SHA256_CTX*) hash_fn_ctx->sha256_ctx);
    hash_fn_ctx->buf = (void*)calloc(SHA256_BLOCK_SIZE, sizeof(BYTE));

    hash_fn_ctx->initialized = 1;
}

void SHA256_update_take_bit(const char *key, const uint32_t klen, hash_fn_ctx_t *hash_fn_ctx)
{
    if (!hash_fn_ctx->initialized)
        SHA256_init(hash_fn_ctx);
    if (first_call(hash_fn_ctx)) {
        BYTE* text = (unsigned char*) key;
        sha256_update((SHA256_CTX*)hash_fn_ctx->sha256_ctx, text, klen);
        sha256_final((SHA256_CTX*)hash_fn_ctx->sha256_ctx, (BYTE*)hash_fn_ctx->buf);
    }
    hash_fn_ctx->hashcode = take_bit((BYTE*)hash_fn_ctx->buf, 1<<hash_fn_ctx->tryout);
    hash_fn_ctx->tryout++;
}

void SHA256_update_take_byte(const char *key, const uint32_t klen, hash_fn_ctx_t *hash_fn_ctx)
{
    if (!hash_fn_ctx->initialized)
        SHA256_init(hash_fn_ctx);
    if (first_call(hash_fn_ctx)) {
        BYTE* text = (unsigned char*) key;
        sha256_update((SHA256_CTX*)hash_fn_ctx->sha256_ctx, text, klen);
        sha256_final((SHA256_CTX*)hash_fn_ctx->sha256_ctx, (BYTE*)hash_fn_ctx->buf);
    }
    hash_fn_ctx->hashcode = take_byte((BYTE*)hash_fn_ctx->buf, hash_fn_ctx->tryout, hash_fn_ctx->hash_size);
    hash_fn_ctx->tryout++;
}

void SHA256_clean(hash_fn_ctx_t *hash_fn_ctx) 
{   
    hash_fn_ctx->tryout = 0;
    memset(hash_fn_ctx->buf, 0, SHA256_BLOCK_SIZE * sizeof(BYTE)); 
}


void XXHASH_init(hash_fn_ctx_t *hash_fn_ctx) 
{
    assert(hash_fn_ctx);
    hash_fn_ctx->seed = RNG_SEED;
    hash_fn_ctx->initialized = 1;
}

void XXHASH_update(const char *key, const uint32_t klen, hash_fn_ctx_t *hash_fn_ctx)
{   
    int bit_shift = hash_fn_ctx->tryout;
    if (first_call(hash_fn_ctx)) {
        hash_fn_ctx->hash_buf = (uint32_t) XXH32(key, klen, hash_fn_ctx->seed);
    }
    hash_fn_ctx->hashcode = (uint32_t) (hash_fn_ctx->hash_buf << bit_shift)
                        | (hash_fn_ctx->hash_buf >> (hash_fn_ctx->hash_size - bit_shift));
    hash_fn_ctx->tryout++;
}

void XXHASH_clean(hash_fn_ctx_t *hash_fn_ctx)
{
    hash_fn_ctx->tryout = 0;
    hash_fn_ctx->hash_buf = 0;
}

void SPOOKY_init(hash_fn_ctx_t *hash_fn_ctx) {
    assert(hash_fn_ctx);
    hash_fn_ctx->seed = RNG_SEED;
    hash_fn_ctx->initialized = 1;
}

void SPOOKY_update(const char *key, const uint32_t klen, hash_fn_ctx_t *hash_fn_ctx)
{   
    int bit_shift = hash_fn_ctx->tryout;
    if (first_call(hash_fn_ctx)) {
        hash_fn_ctx->hash_buf = (uint32_t) spooky_hash32(key, klen, hash_fn_ctx->seed);
    }
    hash_fn_ctx->hashcode = (uint32_t) (hash_fn_ctx->hash_buf << bit_shift) 
                        | (hash_fn_ctx->hash_buf >> (hash_fn_ctx->hash_size - bit_shift));
    hash_fn_ctx->tryout++;
}

void SPOOKY_clean(hash_fn_ctx_t *hash_fn_ctx)
{
    hash_fn_ctx->tryout = 0;
    hash_fn_ctx->hash_buf = 0;
}