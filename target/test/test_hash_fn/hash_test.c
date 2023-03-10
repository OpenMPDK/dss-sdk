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

void start_generate_keys(key_generator_t *kg, int key_batch)
{
    obj_key_t *obj_key;
    obj_key_t *obj_key_list;
    int rc;
    int key_cnt = 0;

    while (kg->key_cnt<kg->num_key && key_cnt<key_batch)
    {
       rc = object_key_generate(KEY_LEN, &kg->obj_key_list[key_cnt]);
       if(rc) {
            if (++kg->key_fail_cnt == key_batch) {
                printf("Key generation failed at max tryout.");
                return;
            }
            continue;
       }
       kg->key_cnt++;
       key_cnt++;
    }
}



// uint32_t hash_key()
// {
//     char key[] = {"abc"};
//     // char key[KEY_LEN];
//     // object_key_generate(key, KEY_LEN);
//     printf("%s\n", key);

//     BYTE sha256[SHA256_BLOCK_SIZE];
//     SHA256(key, sha256);
//     for (int i=0; i<SHA256_BLOCK_SIZE; i++)
//         printf("%x, ", sha256[i]);
//     printf("\n");

//     uint32_t rehashed_s;
//     rehashed_s = rehash1(sha256, 0x00000001);
//     printf("%x\n", rehashed_s);
//     return rehashed_s;
// }

static void help() {
    printf("usage:\n");
    printf("\t\t./hash_test_fn <key_num> <preallocation ratio> <hash function type>\n");
    printf("arguments:\n");
    printf("\t\t -key_num: the number of keys to be inserted.\n");
    printf("\t\t -preallocation ratio: the percentage of segments that are preallocated.\n");
    printf("\t\t -hash function type: 0: sha256_take_bit; 1: sha256_take_byte; 2: xxhash; 3: spooky.\n");
}

int main(int arc, char **argv)
{   
    ssd_t *ssd;
    block_allocator_t *blk_alloc;
    key_generator_t *kg;
    hash_ctx_t *hash_ctx;
    int k_idx;
    int b;
    uint32_t seg_idx;
    uint32_t collision_sum, loop_sum;
    double ave_loop, std_loop;
    int key_batch = BATCH_SIZE;

    if (arc!=4) {
        printf("Input error\n");
        help();
        return 1;
    }
    
    uint32_t num_key =  atoi(argv[1]);
    double steady_state_percil = (double) atoi(argv[2])/100;
    int hash_type = atoi(argv[3]);

    if (num_key<0) {
        printf("Numer of key out of range\n");
        help();
        return 1;
    }

    if (steady_state_percil<0 || steady_state_percil>=1) {
        printf("Preallocation ratio out of range\n");
        help();
        return 1;
    }

    if (hash_type<0 || hash_type>3) {
        printf("Hash function type out of range\n");
        help();
        return 1;
    }

    kg = (key_generator_t *) malloc (sizeof(key_generator_t ));
    init_key_generator(kg, num_key, key_batch);

    ssd = (ssd_t*) malloc(sizeof(ssd_t));
    init_ssd(ssd, steady_state_percil);

    blk_alloc = (block_allocator_t*) malloc (sizeof(block_allocator_t));
    register_blk_alloc(blk_alloc, ssd, hash_type);

    hash_ctx = blk_alloc->hash_ctx;

    per_batch_counter_t bc = {0};

    for(b=0;b<num_key/key_batch;b++) {
        memset(&bc, 0, sizeof(per_batch_counter_t));
        start_generate_keys(kg, key_batch);
        clock_t before = clock();
        for(k_idx=0;k_idx<key_batch;k_idx++) {
            kg->obj_key_list[k_idx].lba = blk_alloc->ops.lba_alloc_fn(&kg->obj_key_list[k_idx], ssd, hash_ctx);
            if (kg->obj_key_list[k_idx].rejected) {
                bc.rejection++;
                continue;
            }
            kg->key_inserted++;
        }
        clock_t difference = clock() - before;

        update_bench_counter(&bc, kg, difference);
        log_bench_counter(&bc, kg);
    }

    collision_sum = 0; 
    for(seg_idx=0;seg_idx<SEG_NUM;seg_idx++){
        collision_sum += ssd->segs[seg_idx].collision_cnt;
    }
    printf("Overall Collisions: %zu\n", collision_sum);

free:
    free_key_generator(kg);
    unregister_blk_alloc(blk_alloc);
    free_ssd(ssd);
}