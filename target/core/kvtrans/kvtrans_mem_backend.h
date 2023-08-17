#ifndef KVTRANS_MEM_BACKEND_H
#define KVTRANS_MEM_BACKEND_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include <Judy.h>

#define INIT_FREE_INDEX_SIZE 1024

typedef struct ondisk_meta_s ondisk_meta_t;
typedef ondisk_meta_t* val_t;
typedef uint64_t idx_t;

typedef struct ondisk_meta_ctx_s {
    uint64_t num;
    uint64_t free_num;
	Pvoid_t Parray;
    uint64_t *free_index;
    ondisk_meta_t *pool;
    uint64_t pool_size;
    bool inited;
} ondisk_meta_ctx_t;

typedef struct ondisk_data_ctx_s {
    void *data_buff_start_addr;
    uint64_t data_buff_size_in_byte;
    uint64_t blk_num;
    uint64_t used_num;
    bool inited;
} ondisk_data_ctx_t;

void insert_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index, ondisk_meta_t *meta);
val_t load_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index);
int found_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index);
int delete_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index);
val_t get_first_meta(ondisk_meta_ctx_t* meta_ctx);
int free_metas(ondisk_meta_ctx_t* meta_ctx);
void log_metas(ondisk_meta_ctx_t* meta_ctx, char *file_path);
void init_meta_ctx(ondisk_meta_ctx_t* meta_ctx, uint64_t meta_pool_size);
void free_meta_ctx(ondisk_meta_ctx_t* meta_ctx);

bool insert_data(ondisk_data_ctx_t* data_ctx, idx_t index, uint64_t num_blk, void *buff);
bool retrieve_data(ondisk_data_ctx_t* data_ctx, idx_t index, uint64_t num_blk, void *buff);
bool delete_data(ondisk_data_ctx_t* data_ctx, idx_t index, uint64_t num_blk);
void init_data_ctx(ondisk_data_ctx_t *data_ctx, uint64_t data_pool_size);
void free_data_ctx(ondisk_data_ctx_t *data_ctx);

#endif