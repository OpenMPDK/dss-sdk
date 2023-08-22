#ifndef KVTRANS_UTILS_H
#define KVTANS_UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <Judy.h>
#include <assert.h>

#define DEBUG_ENABLED

#define MAX_CACHE_TBL_SIZE 107374182400 //100GB
#define MIN_ELM_NUM 1024
#define SCALE_CONST 2
#define SCALE_DOWN(num) 2 * num / 3

typedef struct cache_tbl_s cache_tbl_t;

typedef struct cache_tbl_s {
    char name[32];
    size_t elm_size;
    uint64_t init_elm_num;
    uint64_t elm_num;
    uint64_t free_num;
    // if tbl size is adjustable
    bool dynamic;
    // Judy1 array to represent free index
    Pvoid_t free_array;
    // JudyL array to map user passed integer index to index of elements
    Pvoid_t mem_array;
    // memory pool
    void *start_addr;
} cache_tbl_t;

cache_tbl_t *init_cache_tbl(char *name, uint64_t init_elm_num, size_t elm_size, bool dynamic);
void free_cache_tbl(cache_tbl_t *cache_tbl);
int _extend_cache_tbl(cache_tbl_t *cache_tbl);
int _shrink_cache_tbl(cache_tbl_t *cache_tbl);
int _pop_free_index(cache_tbl_t *cache_tbl, uint64_t *free_index);
int _put_free_index(cache_tbl_t *cache_tbl, uint64_t index);

int store_elm(cache_tbl_t *cache_tbl, uint64_t kidx, void *data);
void *get_elm(cache_tbl_t *cache_tbl, uint64_t kidx);
int delete_elm(cache_tbl_t *cache_tbl, uint64_t kidx);

#endif