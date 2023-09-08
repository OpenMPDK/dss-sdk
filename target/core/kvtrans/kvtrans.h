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

#ifndef KVTRANS_H
#define KVTRANS_H


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/queue.h>
#include <Judy.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MEM_BACKEND

#include "apis/dss_module_apis.h"
#include "apis/dss_block_allocator_apis.h"
#include "apis/dss_io_task_apis.h"
#include "dss.h"
#include "utils/dss_mallocator.h"
#include "dragonfly.h"

#ifdef MEM_BACKEND
#include "kvtrans_mem_backend.h"
#endif

#define MAX_DC_NUM (262144)

#define MAX_COL_TBL_SIZE (2)
#define MAX_DATA_COL_TBL_SIZE MAX_COL_TBL_SIZE
#define MAX_VALUE_SCATTER (8)
// make sure blk_ctx is 4096 Byte
#define MAX_INLINE_VALUE (1024 - 248)
#define MIN_HASH_SIZE (8)
#define DEFAULT_BLOCK_STATE_NUM (7)
#define DEFAULT_BLK_ALLOC_NAME "block_impresario"
#define DEFAULT_META_NUM (1000000)
// A 64 bit value to indicate if meta blk valid
#define META_MAGIC (0xabc0)
#define BLK_ALIGN (4096)

#define CEILING(x,y) (((x) + (y) - 1) / (y))

typedef uint64_t counter_t;
typedef double tick_t;
typedef struct hash_fn_ctx_s hash_fn_ctx_t;
typedef struct kvtrans_params_s kvtrans_params_t;
typedef struct ondisk_meta_s ondisk_meta_t;
typedef struct blk_ctx blk_ctx_t;

// typedef dfly_module_t kvtrans_t;

typedef enum dss_kvtrans_status_e {
    KVTRANS_STATUS_ERROR = -1,
    KVTRANS_STATUS_SUCCESS = 0,
    KVTRANS_STATUS_FREE = 1,
    KVTRANS_STATUS_FOUND = 2,
    KVTRANS_STATUS_NOT_FOUND = 3,
    // failed to set a block state
    KVTRANS_STATUS_SET_BLK_STATE_ERROR = 4,
    // failed to set block states for a data collision pair
    KVTRANS_STATUS_SET_DC_STATE_ERROR = 5,
    // failed to alloc contiguous blocks for request
    KVTRANS_STATUS_ALLOC_CONTIG_ERROR = 6,
    // failed to roll back in any error
    KVTRANS_ROLL_BACK_ERROR = 7,
    // failed to alloc memory
    KVTRANS_MALLOC_ERROR = 8,
    // I/O failed
    KVTRANS_STATUS_IO_ERROR = 9,
    KVTRANS_IO_QUEUED = 10,
    KVTRANS_IO_SUBMITTED = 11
} dss_kvtrans_status_t;


/* tmp use for test */
#define REHASH_MAX 32
// #define BLK_NUM (3758096384 >> 8)
#define BLK_NUM (536870912)
#define BLOCK_SIZE (4096)

/* tmp use for test end */

typedef enum blk_state_e {
    EMPTY = 0,
    META,
    DATA,
    COLLISION,
    DATA_COLLISION,
    META_DATA_COLLISION,
    COLLISION_EXTENSION
} blk_state_t;

typedef struct dc_item_s {
    uint64_t mdc_index;
    blk_state_t ori_state;
} dc_item_t;

typedef enum col_entry_state_s {
    INVALID = 0,
    DELETED,
    META_COL_ENTRY,
    DATA_COL_ENTRY,
} col_entry_state_t;

typedef struct col_entry_s {
    char key[KEY_LEN];
    uint64_t  meta_collision_index;
    col_entry_state_t state;
} col_entry_t;

typedef struct value_loc_s {
    uint64_t  value_index;     // In case not within Meta
    uint16_t   num_chunks;
} value_loc_t;

enum value_loc_e {
    INLINE = 0,
    CONTIG,
    REMOTE,
    HYBIRD
};

typedef struct ondisk_meta_s {
    uint64_t    magic;
    char        key[KEY_LEN];
    char        checksum[16];
    // char        creation_time[32];
    struct timespec creation_time;
    bool isvalid;
    key_size_t key_len;

    //value entry
    uint64_t  value_size;   // if value_size = 0, it is just holding the blkment to serve other collision keys
    enum value_loc_e value_location;   // 0 = Along with Meta, 1 = next adjacent blkment, 2 = remote, 3 = some adjacent and  some remote
    uint8_t    num_valid_place_value_entry;
    value_loc_t   place_value [MAX_VALUE_SCATTER];
    uint8_t    value_buffer[MAX_INLINE_VALUE];  //Small values we can club together with Meta

    //collision entry
    uint8_t     num_valid_col_entry;
    uint8_t     num_valid_dc_col_entry;
    col_entry_t collision_tbl[MAX_COL_TBL_SIZE];
    uint64_t    data_collision_index;
    uint64_t    collision_extension_index;
} ondisk_meta_t;

/* blkment context */

typedef dss_kvtrans_status_t (*blk_cb_t)(void *ctx);
typedef dss_kvtrans_status_t (*blk_fn_t)(void *ctx);

enum ops_flag_e {
    // input state
    new_write=0,
    update,
    to_delete,
};

typedef struct blk_key_ops_s {
    blk_fn_t init_blk;
    // update blk states
    blk_fn_t update_blk;
    blk_fn_t clean_blk;
} blk_key_ops_t;

typedef struct blk_key_ctx_s {
    enum ops_flag_e flag;
    blk_key_ops_t ops;
    // previous blk_ctx index
    // index of collision block
    uint64_t pindex;
    // index of dc
    uint64_t dc_index;
    // dc entry ???
    col_entry_t col_entry;
} blk_key_ctx_t;

typedef struct blk_val_ctx_s {
    bool iscontig;
    // total number of value blocks to allocate
    uint64_t  value_blocks;
    // can be non zero only if iscontig is false
    uint64_t remote_val_blocks;
} blk_val_ctx_t;

/**
 *  @brief blkment context.
 *  Hold a buffer for ondisk meta blkment.
 *  Include a key context and a value context
 *  to update key and value related variables.
 */
struct blk_ctx {
    uint64_t index;
    blk_state_t state;
    ondisk_meta_t *blk;
    blk_key_ctx_t kctx;
    blk_val_ctx_t vctx;
    kvtrans_req_t *kreq;
    // two types of blk: 1. hashed; 2. lookuped
    bool nothash;

    // track the first insertable blk if a blk chain exists
    blk_ctx_t *first_insert_blk_ctx;

    // the number of meta in chain
    TAILQ_ENTRY(blk_ctx) blk_link;
} ;


/**
 *  @brief hash function type to use, hashing key string to be within a range of integers.
 * 
 */
enum hash_type_e {
    sha256_take_bit = 0,
    sha256_take_byte = 1,
    xxhash = 2,
    spooky = 3
};

/**
 *  @brief parameters to config a kvtrans_ctx instance
*/
typedef struct kvtrans_params_s {
    int id;
    int thread_num;
    char *name;
    char *blk_alloc_name;
    enum hash_type_e hash_type;
    uint16_t hash_size;
    uint64_t meta_blk_num;
    uint64_t total_blk_num;
    dss_device_t *dev;
    dss_io_task_module_t *iotm;
} kvtrans_params_t;

/**
 *  @brief hashing algorithm context
 */
typedef struct hash_fn_ctx_s {
    uint32_t seed;
    int tryout;
    int max_tryout;
    uint8_t initialized;
    enum hash_type_e hash_type;
    uint16_t hash_size;

    uint64_t hashcode;

    /* tmp buf to avoid rehash*/
    uint32_t hash_buf;
    /* SHA256 use */
    void *buf;
    void *sha256_ctx;

    void (*init)(struct hash_fn_ctx_s *hash_fn_ctx);
    void (*update)(const char *key, struct hash_fn_ctx_s *hash_fn_ctx);
    void (*clean)(struct hash_fn_ctx_s *hash_fn_ctx);
} hash_fn_ctx_t;

#if 0
enum kvtrans_req_e {
    REQ_INITIALIZED = 0,
    QUEUE_TO_LOAD_ENTRY,
    ENTRY_LOADING_DONE,
    QUEUE_TO_LOAD_COL,
    COL_LOADING_DONE,
    QUEUE_TO_LOAD_COL_EXT,
    COL_EXT_LOADING_DONE,
    QUEUE_TO_START_IO,
    IO_CMPL,
    REQ_CMPL
};

struct req_time_tick {
    tick_t bg;
    tick_t hash;
    tick_t keyset;
    tick_t valset;
    tick_t cmpl;
};


/**
 *  @brief kvtrans request context
 */
struct kvtrans_req{
    req_t *req;
    uint64_t id;
    enum kvtrans_req_e state;
    // a blk_ctx to maintain meta info 
    kvtrans_ctx_t *kvtrans_ctx;
    dss_io_task_t **io_tasks;
    void *cb_ctx;
    struct req_time_tick time_tick;
    STAILQ_ENTRY(kvtrans_req) req_link;

};
#endif

typedef struct dstat_s {
    // meta + mc + mdc = all dirty blks on disk
    counter_t meta;
    counter_t mc;
    counter_t dc;
    counter_t mdc;
    counter_t ce; // overlapped with other stats
    counter_t data_scatter;
    tick_t pre;
    tick_t hash;
    tick_t setkey;
    tick_t setval;
} dstat_t;

/**
 *  @brief kv translator context
 *  Create kv translator instance per disk.
 *  Include a hash function context, a block allocator context, and a blkment context
 *  Associate with the request pointer to process.
 *  Initialize a free list for data collision mapping.
 */
typedef struct kvtrans_ctx_s {
    kvtrans_params_t kvtrans_params;
    // kvtrans_t *kvtrans_module;
    hash_fn_ctx_t *hash_fn_ctx;
    dss_blk_allocator_context_t *blk_alloc_ctx;

    dss_io_task_module_t *kvt_iotm;

    // a blk_ctx to maintain hashed entry.
    // blk_ctx_t *entry_blk;
    
    // each kvtrans handles one target
    dss_device_t *target_dev;
#ifdef DSS_BUILD_CUNIT_TEST
    // to mimic request queue
    STAILQ_HEAD(, kvtrans_req) req_head;
#endif
    uint64_t task_num;
    uint64_t task_done;
    uint64_t task_failed;

    // the digit number of hex number of allocable blocks
    uint16_t hash_bit_in_use;   

    Pvoid_t dc_tbl;
    uint64_t dc_size;
    // TODO: pool size should be dynamic
    dc_item_t *dc_pool;

    dss_mallocator_ctx_t *blk_ctx_allocator;

    void (*kv_assign_block)(uint64_t *, kvtrans_ctx_t *);

#ifdef MEM_BACKEND
    ondisk_meta_ctx_t *meta_ctx;
    ondisk_data_ctx_t *data_ctx;
#endif

    dstat_t stat;
} kvtrans_ctx_t;


hash_fn_ctx_t *init_hash_fn_ctx(kvtrans_params_t *params);
void free_hash_fn_ctx(hash_fn_ctx_t *ctx);

blk_ctx_t *init_blk_ctx();
void free_blk_ctx(blk_ctx_t *ctx);

void init_dc_table(kvtrans_ctx_t  *ctx);
void free_dc_table(kvtrans_ctx_t  *ctx);

kvtrans_ctx_t *init_kvtrans_ctx(kvtrans_params_t *params);
void free_kvtrans_ctx(kvtrans_ctx_t *ctx);

kvtrans_req_t *init_kvtrans_req(kvtrans_ctx_t *kvtrans_ctx, req_t *req, kvtrans_req_t *preallocated_req);
void free_kvtrans_req(kvtrans_req_t *kreq);

kvtrans_params_t set_default_params();

dss_kvtrans_status_t dss_kvtrans_dc_table_exist(kvtrans_ctx_t *ctx, const uint64_t index);
dss_kvtrans_status_t dss_kvtrans_dc_table_lookup(kvtrans_ctx_t *ctx, const uint64_t dc_index, uint64_t *mdc_index);
dss_kvtrans_status_t dss_kvtrans_dc_table_insert(kvtrans_ctx_t *ctx, const uint64_t dc_index, const uint64_t mdc_index, blk_state_t ori_stat);
dss_kvtrans_status_t dss_kvtrans_dc_table_update(kvtrans_ctx_t *ctx, const uint64_t dc_index, blk_state_t ori_state);
dss_kvtrans_status_t dss_kvtrans_dc_table_delete(kvtrans_ctx_t  *ctx, const uint64_t dc_index,  const uint64_t mdc_index);


dss_kvtrans_status_t dss_kvtrans_load_ondisk_blk(blk_ctx_t *blk_ctx, kvtrans_req_t *kreq, bool submit_for_disk_io);
dss_kvtrans_status_t dss_kvtrans_load_ondisk_data(blk_ctx_t *blk_ctx, kvtrans_req_t *kreq, bool submit_for_disk_io);
dss_kvtrans_status_t dss_kvtrans_write_ondisk_blk(blk_ctx_t *blk_ctx, kvtrans_req_t *kreq, bool submit_for_disk_io);
dss_kvtrans_status_t dss_kvtrans_write_ondisk_data(blk_ctx_t *blk_ctx, kvtrans_req_t *kreq, bool submit_for_disk_io);


typedef dss_kvtrans_status_t (async_kvtrans_fn)(kvtrans_ctx_t *ctx, kvtrans_req_t *kreq);

async_kvtrans_fn kvtrans_store;
async_kvtrans_fn kvtrans_retrieve;
async_kvtrans_fn kvtrans_delete;
async_kvtrans_fn kvtrans_exist;
dss_kvtrans_status_t kv_process(kvtrans_ctx_t *ctx);

int dss_kvtrans_handle_request(kvtrans_ctx_t *ctx, req_t *req);

// void dss_setup_kvtrans_req(dss_request_t *req, dss_key_t *k, dss_value_t *v);

#ifdef MEM_BACKEND
void init_mem_backend(kvtrans_ctx_t  *ctx, uint64_t meta_pool_size, uint64_t data_pool_size);
void reset_mem_backend(kvtrans_ctx_t  *ctx);
void free_mem_backend(kvtrans_ctx_t  *ctx);
#endif

#ifdef __cplusplus
}
#endif


#endif
