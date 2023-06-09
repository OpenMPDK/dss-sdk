#ifndef KVTRANS_MEM_BACKEND_H
#define KVTRANS_MEM_BACKEND_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include<Judy.h>
#include "kvtrans.h"

#define INIT_FREE_INDEX_SIZE 1024

// typedef struct ondisk_meta_s ondisk_meta_t;
typedef ondisk_meta_t* val_t;
typedef uint64_t idx_t;
void insert_meta(idx_t index, ondisk_meta_t *meta);
val_t load_meta(idx_t index);
int found_meta(idx_t index);
int delete_meta(idx_t index);
val_t get_first_meta();
int free_metas();
void log_metas(char *file_path);

bool insert_data(idx_t index, uint64_t num_blk, void *buff);
bool retrieve_data(idx_t index, uint64_t num_blk, void *buff);
bool delete_data(idx_t index, uint64_t num_blk);

void init_mem_backend(kvtrans_ctx_t  *ctx, uint64_t meta_pool_size, uint64_t data_pool_size);
void reset_mem_backend(kvtrans_ctx_t  *ctx);
void free_mem_backend();

static struct ondisk_meta_ctx_s {
    uint64_t num;
    uint64_t free_num;
	Pvoid_t Parray;
    uint64_t *free_index;
    ondisk_meta_t *pool;
    uint64_t pool_size;
    bool inited;
} g_metas;

static struct ondisk_data_ctx_s {
    void *data_buff_start_addr;
    uint64_t data_buff_size_in_byte;
    uint64_t blk_num;
    uint64_t used_num;
    bool inited;
} g_data;


void insert_meta(idx_t index, ondisk_meta_t *meta) {
    assert(g_metas.inited);
    Word_t *new_entry;
    uint64_t pool_idx;
    new_entry = (Word_t *)JudyLGet(g_metas.Parray, (Word_t)index, PJE0);
    if (new_entry==NULL) {
        if (g_metas.free_num == 0) {
            pool_idx = g_metas.num;
            g_metas.num++;
        } else {
            pool_idx = g_metas.free_index[g_metas.free_num-1];
            g_metas.free_num--;
        }
        memcpy(&g_metas.pool[pool_idx], meta, sizeof(ondisk_meta_t));
        new_entry = (Word_t *)JudyLIns(&g_metas.Parray, (Word_t)index, PJE0);
        *new_entry = (Word_t) &g_metas.pool[pool_idx];
    } else {
        ondisk_meta_t *ondisk_meta;
        ondisk_meta = (ondisk_meta_t *) *new_entry;
        memcpy(ondisk_meta, meta, sizeof(ondisk_meta_t)); 
    }
}

val_t load_meta(idx_t index) {
    assert(g_metas.inited);
    Word_t *new_entry;
    val_t val;
    new_entry = (Word_t *)JudyLGet(g_metas.Parray, (Word_t)index, PJE0);
    val = (val_t) *new_entry;
    return val;
}

int found_meta(idx_t index) {
    assert(g_metas.inited);
    Word_t *new_entry;
    new_entry = (Word_t *)JudyLGet(g_metas.Parray, (Word_t)index, PJE0);
    return new_entry!=NULL;
}


int delete_meta(idx_t index) {
    assert(g_metas.inited);
    int rc;
    uint64_t pool_idx;
    // val_t val;
    // val = load_meta(index);
    // assert(val);
     
    // pool_idx = val - g_metas.pool;

    // if (pool_idx==g_metas.num-1) {
    //     g_metas.num--;
    // } else {
    //     g_metas.free_index[g_metas.free_num] = pool_idx;
    //     g_metas.free_num++;
    // }

    rc = JudyLDel(&g_metas.Parray, (Word_t)index, PJE0);
    if (rc==0) {
        printf("index not present\n");
        return KVTRANS_STATUS_ERROR;
    } else if (rc==JERR) {
        printf("malloc failed\n");
        return KVTRANS_STATUS_ERROR;
    } 
    g_metas.num--;
    return rc;
}


int free_metas() {
    assert(g_metas.inited);
    Word_t freed_bytes;
    freed_bytes = JudyLFreeArray(&g_metas.Parray, PJE0);
    return freed_bytes;
}

val_t get_first_meta() {
    assert(g_metas.inited);
    idx_t index;
    Word_t *new_entry;
    val_t val;
    new_entry = (Word_t *) JudyLFirst(g_metas.Parray, &index, PJE0);
    val = (val_t) *new_entry;
    return val;
}

void log_metas(char *file_path) {
    assert(g_metas.inited);

    idx_t index;
    Word_t *new_entry;
    ondisk_meta_t *meta;
    FILE *fptr;
    val_t val;
    fptr = fopen(file_path, "w");
    fprintf(fptr, "index, valid_col, valid_val, value_location\n");
    new_entry = (Word_t *) JudyLFirst(g_metas.Parray, &index, PJE0);
    while (new_entry)
    {
        val = (val_t) *new_entry;
        meta = (ondisk_meta_t *)val;
        fprintf(fptr, "%zu, %zu, %zu, %zu\n", index, meta->num_valid_col_entry, meta->num_valid_place_value_entry, meta->value_location);
       new_entry = (Word_t *) JudyLNext(g_metas.Parray, &index, PJE0);
    }

}


void init_mem_backend(kvtrans_ctx_t  *ctx, uint64_t meta_pool_size, uint64_t data_pool_size) {
    assert(sizeof(val_t)==sizeof(ondisk_meta_t *));
    g_metas.pool_size = meta_pool_size;
    g_metas.num = 0;
    g_metas.free_num = 0;
    g_metas.pool = (ondisk_meta_t *) malloc (sizeof(ondisk_meta_t) * g_metas.pool_size);
    if (!g_metas.pool) {
        printf("ERROR: malloc g_metas failed. \n");
        return;
    }
    g_metas.free_index = calloc(INIT_FREE_INDEX_SIZE, sizeof(uint64_t));
    memset(g_metas.pool, 0, sizeof(ondisk_meta_t) * g_metas.pool_size);
    g_metas.inited = true;

    g_data.blk_num = data_pool_size;
    g_data.data_buff_size_in_byte = g_data.blk_num * BLOCK_SIZE;
    g_data.data_buff_start_addr = malloc(g_data.data_buff_size_in_byte);
    if (!g_data.data_buff_start_addr) {
        printf("ERROR: data memory backend init failed.\n");
        exit(1);
    }
    memset(g_data.data_buff_start_addr, 0, g_data.data_buff_size_in_byte);
    g_data.used_num = 0;
    g_data.inited = true;
}

void *get_data_addr(void *base, uint64_t offset) {
    return (void *)((char *)base + offset);
}

bool insert_data(idx_t index, uint64_t num_blk, void *buff) {
    if (!buff) {
        printf("ERROR: data buffer is null.\n");
        return false;
    }
    memcpy(get_data_addr(g_data.data_buff_start_addr, index * BLOCK_SIZE),
             buff, num_blk * BLOCK_SIZE);
    return true;
}

bool retrieve_data(idx_t index, uint64_t num_blk, void *buff) {
    memcpy(buff, get_data_addr(g_data.data_buff_start_addr, index * BLOCK_SIZE), 
            num_blk * BLOCK_SIZE);
    return true;
}

bool delete_data(idx_t index, uint64_t num_blk) {
    memset(get_data_addr(g_data.data_buff_start_addr, index * BLOCK_SIZE),
             0, num_blk * BLOCK_SIZE);
    return true;
}

void reset_mem_backend(kvtrans_ctx_t  *ctx) {
    free_metas();
    memset(g_metas.pool, 0, sizeof(ondisk_meta_t) * ctx->kvtrans_params.mb_blk_num);
    memset(g_metas.free_index, 0, sizeof(uint64_t) * INIT_FREE_INDEX_SIZE);
    g_metas.num = 0;
    g_metas.free_num = 0;
}

void free_mem_backend() {
    free_metas();
    free(g_metas.pool);
    free(g_metas.free_index);
}


#endif
