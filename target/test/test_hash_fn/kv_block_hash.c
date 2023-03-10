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


#include "kv_block_hash.h"
#include "adv_random.h"

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
        bits |= ( mb << idx)|(mb >> (SHA256_BLOCK_SIZE - idx));
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

uint8_t first_call(hash_ctx_t *hash_ctx)
{
    return hash_ctx->tryout == 0;
}

static void SHA256_init(hash_ctx_t *hash_ctx) 
{   
    hash_ctx->sha256_ctx = (void*) malloc(sizeof(SHA256_CTX));
    sha256_init( (SHA256_CTX*) hash_ctx->sha256_ctx);
    hash_ctx->buf = (void*)calloc(SHA256_BLOCK_SIZE, sizeof(BYTE));

    hash_ctx->initialized = 1;
}

static void SHA256_update_take_bit(char *key, hash_ctx_t *hash_ctx)
{
    if (!hash_ctx->initialized)
        SHA256_init(hash_ctx);
    if (first_call(hash_ctx)) {
        BYTE* text = (unsigned char*) key;
        sha256_update((SHA256_CTX*)hash_ctx->sha256_ctx, text, KEY_LEN);
        sha256_final((SHA256_CTX*)hash_ctx->sha256_ctx, (BYTE*)hash_ctx->buf);
    }
    hash_ctx->hashcode = take_bit((BYTE*)hash_ctx->buf, 1<<hash_ctx->tryout);
    hash_ctx->tryout++;
}

static void SHA256_update_take_byte(char *key, hash_ctx_t *hash_ctx)
{
    if (!hash_ctx->initialized)
        SHA256_init(hash_ctx);
    if (first_call(hash_ctx)) {
        BYTE* text = (unsigned char*) key;
        sha256_update((SHA256_CTX*)hash_ctx->sha256_ctx, text, KEY_LEN);
        sha256_final((SHA256_CTX*)hash_ctx->sha256_ctx, (BYTE*)hash_ctx->buf);
    }
    hash_ctx->hashcode = take_byte((BYTE*)hash_ctx->buf, hash_ctx->tryout, hash_ctx->hash_size);
    hash_ctx->tryout++;
}

static void SHA256_clean(hash_ctx_t *hash_ctx) 
{   
    hash_ctx->tryout = 0;
    memset(hash_ctx->buf, 0, SHA256_BLOCK_SIZE * sizeof(BYTE)); 
}


static void XXHASH_init(hash_ctx_t *hash_ctx) {
    hash_ctx->seed = RNG_SEED;
    hash_ctx->initialized = 1;
}

static void XXHASH_update(char *key, hash_ctx_t *hash_ctx)
{   
    int bit_shift = hash_ctx->tryout;
    if (first_call(hash_ctx)) {
        hash_ctx->hash_buf = (uint32_t) XXH32(key, KEY_LEN, hash_ctx->seed);
    }
    hash_ctx->hashcode = (uint32_t) (hash_ctx->hash_buf << bit_shift)
                        | (hash_ctx->hash_buf >> (hash_ctx->hash_size - bit_shift));
    hash_ctx->tryout++;
}

static void XXHASH_clean(hash_ctx_t *hash_ctx)
{
    hash_ctx->tryout = 0;
    hash_ctx->hash_buf = 0;
}

static void SPOOKY_init(hash_ctx_t *hash_ctx) {
    hash_ctx->seed = RNG_SEED;
    hash_ctx->initialized = 1;
}

static void SPOOKY_update(char *key, hash_ctx_t *hash_ctx)
{   
    int bit_shift = hash_ctx->tryout;
    if (first_call(hash_ctx)) {
        hash_ctx->hash_buf = (uint32_t) spooky_hash32(key, KEY_LEN, hash_ctx->seed);
    }
    hash_ctx->hashcode = (uint32_t) (hash_ctx->hash_buf << bit_shift) 
                        | (hash_ctx->hash_buf >> (hash_ctx->hash_size - bit_shift));
    hash_ctx->tryout++;
}

static void SPOOKY_clean(hash_ctx_t *hash_ctx)
{
    hash_ctx->tryout = 0;
    hash_ctx->hash_buf = 0;
}

// static void UMASH_init(hash_ctx_t *hash_ctx) {
//     hash_ctx->seed = RNG_SEED;
//     hash_ctx->initialized = 1;
// }


// static void UMASH_update(char *key, hash_ctx_t *hash_ctx)
// {   
//     int bit_shift = hash_ctx->tryout;
//     struct umash_params umash_params;
//     uint64_t hash = umash_full(&umash_params, hash_ctx->seed, /*which=*/0, key, KEY_LEN);
//     hash_ctx->hashcode = (uint32_t) hash >> bit_shift;
//     hash_ctx->tryout++;
// }


static uint32_t find_next_empty_seg(uint32_t lba, ssd_t *ssd)
{
    int tmp_lba = lba;
    while (lba<ssd->segment_num) {
        if (ssd->segs[lba++].state==available) 
            return lba;
    }
    lba = 0;
    while (lba<tmp_lba) {
        if (ssd->segs[lba++].state==available) 
            return lba;
    }

}

static uint32_t AssignLBA(obj_key_t *obj_key, ssd_t *ssd, hash_ctx_t* hash_ctx)
{
    uint32_t lba;
    hash_ctx->clean(hash_ctx);

next_bit_hash:
    obj_key->loop_cnt++;
    if (obj_key->loop_cnt>hash_ctx->max_tryout) {
        if (ssd->segment_occupy_cnt<ssd->segment_num) {
            lba = find_next_empty_seg(lba, ssd);
            ssd->segs[lba].state = meta;
            ssd->segment_occupy_cnt++;
            return lba;
        }
        else {
            obj_key->rejected = 1;
            return 0;
        }
    }

    hash_ctx->update(obj_key->key, hash_ctx);
    lba = hash_ctx->hashcode;

    if(lba>ssd->segment_num){
        lba = (lba >> (hash_ctx->hash_size - ssd->size_in_bit)) % ssd->segment_num;        
    }

    switch(ssd->segs[lba].state)
    {
        case available:
            ssd->segs[lba].state = meta;
            ssd->segment_occupy_cnt++;
            return lba;
        case meta:
        case data:
        case collision:
            ssd->segs[lba].collision_cnt++;
            // printf("key: %s has to rehash: %d\n", obj_key->key, obj_key->loop_cnt);
            goto next_bit_hash;
        default:
		    //Unhandled state
		    break;
    }

    //Unhandled state
    return 0;
}

uint32_t dec_2_bit(uint32_t dec_num) {
    uint32_t bits = 0;
    while (dec_num>0) {
        bits++;
        dec_num >>= 1;
    }
    return bits;
}

/* Preallocate segments to achieve a steady state.
   A percentage of segments defined in $steady_state_percentil will
   be marked as Data before executing simulation.
*/
double init_segs(segment_t *segs, double steady_state_percentil) {
    struct rndinfo ri;
    double r;
    uint32_t idx;
	BDR_RNG_VARS;
	ri.type = RND_UNIFORM;
	ri.a = 0;
	ri.b = 1;
    counter d = 0, a = 0;

    srand(time(0));
    for(idx=0;idx<SEG_NUM;idx++) {
		BDR_RNG_NEXTPAIR;
        // get a random number between 0 to 1
        r = get_random_double(&ri, rngz, rngz2);
        // each segment has a probability of $steady_state_percentil to be pre-allocated
        if (r<steady_state_percentil) {
            segs[idx].state = data;
            d++;
        }
        else {
            segs[idx].state = available;
            a++;
        }
    }
    printf("Segs init finished.\n Data seg: %lu, Empty seg: %lu, pre-allocation ratio: %f\n",
            d, a, (double)(d)/(double)(d+a));

}


void init_ssd(ssd_t *ssd, double steady_state_percentil) 
{
    int seg_idx;
    
    ssd->segment_size = SEG_SIZE;
    ssd->segment_num = SEG_NUM;
    ssd->segment_occupy_cnt = 0;

    segment_t *segs;
    segs = (segment_t*)malloc(ssd->segment_num * sizeof(segment_t));
    memset(segs, 0, ssd->segment_num * sizeof(segment_t));
    uint32_t tmp = SEG_NUM;

    if (!segs) {
        printf("Error: alloc segs failed.\n");
    }

    // if (steady_state_percentil != 0)
    init_segs(segs, steady_state_percentil);

    ssd->segs = segs;

    ssd->size_in_bit = dec_2_bit(ssd->segment_num);
    // ssd->size_in_bit = 31;

}

void free_ssd(ssd_t *ssd)
{
    free(ssd->segs);
    free(ssd);
}

uint16_t set_hash_size(int seg_num) {
    uint16_t hash_size = 8;
    if (seg_num < ((uint64_t)1<<hash_size)) {
        printf("Warning: segment num is too small!");
        return hash_size;
    }
    while (seg_num > ((uint64_t)1<<hash_size)) {
        hash_size *= 2;
        seg_num >>= 1;
    }
    return hash_size;
}

void init_hash_ctx(hash_ctx_t *hash_ctx, int hash_fn_type) 
{
    
    hash_ctx->hash_size = set_hash_size(SEG_NUM);
    hash_ctx->hashcode = 0;
    hash_ctx->hash_buf = 0;
    hash_ctx->tryout = 0;
    hash_ctx->max_tryout = hash_ctx->hash_size;
    hash_ctx->initialized = 0;

    switch (hash_fn_type) {
        case 0:
            hash_ctx->hash_type = sha256_take_bit;
            hash_ctx->init = SHA256_init;
            hash_ctx->update = SHA256_update_take_bit;
            hash_ctx->clean = SHA256_clean;
            break;
        case 1:
            hash_ctx->hash_type = sha256_take_byte;
            hash_ctx->init = SHA256_init;
            hash_ctx->update = SHA256_update_take_byte;
            hash_ctx->clean = SHA256_clean;
            break;
        case 2:
            hash_ctx->hash_type = xxhash;
            hash_ctx->init = XXHASH_init;
            hash_ctx->update = XXHASH_update;
            hash_ctx->clean = XXHASH_clean;
            break;
        case 3:
            hash_ctx->hash_type = spooky;
            hash_ctx->init = SPOOKY_init;
            hash_ctx->update = SPOOKY_update;
            hash_ctx->clean = SPOOKY_clean;
            break;
        default:
            printf("ERROR: unknown hash function");
    }
    hash_ctx->init(hash_ctx);
}

void free_hash_ctx(hash_ctx_t *hash_ctx) {
    if (hash_ctx->hash_type==sha256_take_bit ||
        hash_ctx->hash_type==sha256_take_byte) {
            free(hash_ctx->buf);
        }
}


void register_blk_alloc(block_allocator_t *blk_alloc, ssd_t *ssd, int hash_fn_type)
{   
    hash_ctx_t *hash_ctx;
    hash_ctx = (hash_ctx_t *)malloc(sizeof(hash_ctx_t));
    init_hash_ctx(hash_ctx, hash_fn_type);
    blk_alloc->hash_ctx = hash_ctx;

    blk_alloc->ssd = ssd;
    blk_alloc->ops = (block_allocator_ops) {
        .lba_alloc_fn = AssignLBA,
    };
}

void unregister_blk_alloc(block_allocator_t *blk_alloc) {
    blk_alloc->ssd = NULL;
    free(blk_alloc->hash_ctx);
    free(blk_alloc);
}

int object_key_generate( int key_len, obj_key_t *obj_key)
{   
    obj_key->key_len = key_len;
    obj_key->loop_cnt = 0;
    obj_key->rejected = 0;
    dss_keygen_next_key(obj_key->key);
    if (obj_key->key)
    {
        return 0;
    }

    return 1;
}

void init_key_generator(key_generator_t *kg, int num_key, int key_batch) {

    kg->key_cnt = 0;
    kg->key_fail_cnt = 0;
    kg->key_inserted = 0;
    kg->num_key = num_key;
    kg->key_batch = key_batch;
    kg->run = &object_key_generate;
    kg->obj_key_list = (obj_key_t *) malloc (key_batch * sizeof(obj_key_t));
    memset(kg->obj_key_list, 0, key_batch * sizeof(obj_key_t));
    dss_keygen_init(KEY_LEN);
}

void free_key_generator(key_generator_t *kg) {
    free(kg->obj_key_list);
    free(kg);
}

void update_bench_counter(per_batch_counter_t *bc, const key_generator_t *kg, clock_t t) {
    int k_idx;
    int key_batch = kg->key_batch;

    for(k_idx=0;k_idx<key_batch;k_idx++) {
        if (kg->obj_key_list[k_idx].loop_cnt>1) {
            bc->collision += (kg->obj_key_list[k_idx].loop_cnt - 1);
        }
    }
    bc->lat = (double) t / CLOCKS_PER_SEC;
}

void log_bench_counter(per_batch_counter_t *bc, const key_generator_t *kg) {
    printf("================Summary==================\n");
    printf("Num of keys generated: %d; Failed: %d\n", kg->key_inserted, kg->key_fail_cnt);
    printf("Num of keys rejected: %d; ratio: %f\n", bc->rejection, (double)bc->rejection/(double)kg->key_batch);
    printf("Collisions: %zu, ave: %f\n", bc->collision, (double)bc->collision/(double)kg->key_batch);
    printf("Hash Time: %f s, ave: %f us\n", 
            bc->lat, bc->lat * 1000000 /(double)kg->key_batch );
}