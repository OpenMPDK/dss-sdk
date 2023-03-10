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

#ifndef KV_BLOCK_HASH
#define KV_BLOCK_HASH

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "keygen.h"
#include "sha256.h"
#include "xxhash.h"
#include "spooky.h"
// #include "umash.h"

#define KEY_LEN 1024
#define SEG_SIZE 4096
#define SEG_NUM (3758096384 >> 4)
#define RNG_SEED SEG_NUM
#define BATCH_SIZE 100000

typedef uint64_t counter;

typedef struct obj_key_s {
    int key_len;
    char key[KEY_LEN];
    uint32_t loop_cnt;
    uint32_t lba;
    uint8_t rejected;
} obj_key_t;

enum seg_state {
    available = 0,
    meta = 1,
    data = 2,
    collision = 3,
} ;

typedef struct segment_s {
    enum seg_state state;
    uint32_t collision_cnt;
} segment_t;


typedef struct ssd_s {
    uint32_t segment_num;
    int segment_size;
    uint8_t size_in_bit;
    counter segment_occupy_cnt;
    segment_t *segs;
} ssd_t;

enum hash_type_e {
    sha256_take_bit = 0,
    sha256_take_byte = 1,
    xxhash = 2,
    spooky = 3,
    umash = 4
};

typedef struct hash_ctx_s {
    uint32_t seed;
    uint16_t hash_size;
    int tryout;
    int max_tryout;
    uint8_t initialized;
    enum hash_type_e hash_type;
    
    uint32_t hashcode;

    /* tmp buf to avoid rehash*/
    uint32_t hash_buf;
    /* SHA256 use */
    void *buf;
    void *sha256_ctx;

    void (*init)(struct hash_ctx_s *hash_ctx);
    void (*update)(char *key, struct hash_ctx_s *hash_ctx);
    void (*clean)(struct hash_ctx_s *hash_ctx);
} hash_ctx_t;


typedef hash_ctx_t sha256_hash_ctx_t, xxhash_hash_ctx_t;

typedef struct block_allocator_ops {
    uint32_t (*lba_alloc_fn)(obj_key_t*, ssd_t*, hash_ctx_t*);
} block_allocator_ops;

typedef struct block_allocator_s {
    ssd_t *ssd;
    hash_ctx_t *hash_ctx;
    block_allocator_ops ops;
} block_allocator_t;


typedef struct key_generator_s {
    counter key_cnt;
    counter key_fail_cnt;
    counter key_inserted;
    int num_key;
    int key_batch;
    obj_key_t *obj_key_list;
    int (*run)(int, obj_key_t *);
} key_generator_t;


typedef struct per_batch_counter_s {
    uint64_t collision;
    double lat;
    uint64_t rejection;
} per_batch_counter_t;


int object_key_generate( int key_len, obj_key_t *obj_key);

void init_ssd(ssd_t *ssd, double steady_state_percentil);
void free_ssd(ssd_t *ssd);

void init_hash_ctx(hash_ctx_t *hash_ctx, int hash_fn_type);
void free_hash_ctx(hash_ctx_t *hash_ctx);

void register_blk_alloc(block_allocator_t *blk_alloc, ssd_t *ssd, int hash_fn_type);
void unregister_blk_alloc(block_allocator_t *blk_alloc);

void init_key_generator(key_generator_t *kg, int num_key, int key_batch);
void free_key_generator(key_generator_t *kg);

void update_bench_counter(per_batch_counter_t *bc, const key_generator_t *kg, clock_t t);
void log_bench_counter(per_batch_counter_t *bc, const key_generator_t *kg);

#endif