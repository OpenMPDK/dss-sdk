#include "kvtrans_utils.h"

cache_tbl_t *init_cache_tbl(char *name, uint64_t init_elm_num, size_t elm_size, bool dynamic) {
    cache_tbl_t *cache_tbl = (cache_tbl_t *) calloc (1, sizeof(cache_tbl_t));
    if (!cache_tbl) {
        printf("ERROR: cache_tbl init fails\n");
        exit(1);
    }
    uint64_t i;
    int rc = 0;
    cache_tbl->elm_size = elm_size;
    cache_tbl->init_elm_num = (init_elm_num < MIN_ELM_NUM) ? MIN_ELM_NUM : init_elm_num;
    cache_tbl->elm_num = cache_tbl->init_elm_num;
    cache_tbl->free_num = cache_tbl->elm_num;

    cache_tbl->start_addr = malloc(cache_tbl->elm_num * elm_size);
    memset(cache_tbl->start_addr, 0, sizeof(cache_tbl->elm_num * elm_size));
    if (!cache_tbl->start_addr) {
        printf("ERROR: memory pool init for elm_size [%zd] elm_num [%zu] fails\n", 
                cache_tbl->elm_size, cache_tbl->elm_num);
        exit(1);
    }
    
    memcpy(&cache_tbl->name, name, strnlen(name, 32));
    cache_tbl->dynamic = dynamic;

    // all elements are free
    for (i=0; i<cache_tbl->elm_num; i++) {
        rc = Judy1Set(&cache_tbl->free_array, i, PJE0);
        if (rc==JERR) goto process_malloc_failure;
        if (rc==0) {
            printf("Error: alloc memory for free_array failed at index [%zu]", i);
            goto process_malloc_failure;
        }
    }

#ifdef DEBUG_ENABLED
    cache_tbl->free_num = Judy1Count(cache_tbl->free_array, 0, cache_tbl->elm_num, PJE0);
    assert(cache_tbl->free_num==cache_tbl->elm_num);
#else
    cache_tbl->free_num = cache_tbl->elm_num;
#endif

    return cache_tbl;

process_malloc_failure:
    free(cache_tbl->start_addr);
    return NULL;
}

void free_cache_tbl(cache_tbl_t *cache_tbl) {
    Judy1FreeArray(&cache_tbl->free_array, PJE0);
    JudyLFreeArray(&cache_tbl->mem_array, PJE0);
    free(cache_tbl->start_addr);
    free(cache_tbl);
}

/**
 * @brief reset cache table
 * @param cache_tbl the cache table to reset.
*/
void reset_cache_table(cache_tbl_t *cache_tbl) {
    uint64_t i;
    int rc;
    
    Judy1FreeArray(&cache_tbl->free_array, PJE0);
    JudyLFreeArray(&cache_tbl->mem_array, PJE0);
    memset(cache_tbl->start_addr, 0, sizeof(cache_tbl->elm_num * cache_tbl->elm_size));
    cache_tbl->elm_num = cache_tbl->init_elm_num;
    cache_tbl->free_num = cache_tbl->elm_num;
    // all elements are free
    for (i=0; i<cache_tbl->elm_num; i++) {
        rc = Judy1Set(&cache_tbl->free_array, i, PJE0);
        if (rc==JERR) goto process_malloc_failure;
        if (rc==0) {
            printf("Error: alloc memory for free_array failed at index [%zu]", i);
            goto process_malloc_failure;
        }
    }
    return;

process_malloc_failure:
    free(cache_tbl->start_addr);
    return;
}

void *get_elm_addr(cache_tbl_t *cache_tbl, uint64_t index) {
    assert(index<cache_tbl->elm_num);
    return (void *) ((char *)cache_tbl->start_addr + index * cache_tbl->elm_size);
}

int _extend_cache_tbl(cache_tbl_t *cache_tbl) {
    // assert(cache_tbl->free_num == 0);
    uint64_t i;
    int rc = 0;
    void *tmp;
    uint64_t new_total = cache_tbl->elm_num * SCALE_CONST;
    if (new_total * cache_tbl->elm_size > MAX_CACHE_TBL_SIZE) {
        printf("ERROR: reach to size cap [%zu]\n", MAX_CACHE_TBL_SIZE);
        exit(1);
    }
    tmp = realloc(cache_tbl->start_addr, new_total * cache_tbl->elm_size);
    if (!tmp) {
        printf("ERROR: realloc fails for size [%zu]\n", new_total);
        goto process_malloc_failure;
    }
    cache_tbl->start_addr = tmp;

    for (i = cache_tbl->elm_num; i < new_total; i++) {
        rc = Judy1Set(&cache_tbl->free_array, i, PJE0);
        if (rc==JERR) goto process_malloc_failure;
        if (rc==0) {
            printf("Error: expand memory for free_array failed at index [%zu]\n", i);
            goto process_malloc_failure;
        }
    }

    cache_tbl->free_num += new_total - cache_tbl->elm_num;
    
#ifdef DEBUG_ENABLED
    uint64_t total_free;
    total_free = Judy1Count(cache_tbl->free_array, 0, new_total, PJE0);
    assert(total_free == cache_tbl->free_num);
#endif

    cache_tbl->elm_num = new_total;
    return 0;

process_malloc_failure:
    free_cache_tbl(cache_tbl);
    return 1;
}

int _shrink_cache_tbl(cache_tbl_t *cache_tbl) {
    int rc;
    uint64_t mem_pool_index = 0;
    uint64_t *mem_pool_entry;
    uint64_t new_index;
    uint64_t free_array_idx;
    int mem_pool_count;

    uint64_t new_total = cache_tbl->elm_num/SCALE_CONST;
    if ( new_total < (cache_tbl->elm_num - cache_tbl->free_num)) {
        printf("Error: fail to shrink cache tbl for elm_num [%zu] and free_num [%zu]\n", 
                cache_tbl->elm_num, cache_tbl->free_num);
        return 1;
    }

#ifdef DEBUG_ENABLED
    mem_pool_count = JudyLCount(cache_tbl->mem_array, 0, -1, PJE0);
    // printf("INFO: mem pool has %d entries\n", mem_pool_count);
    assert(mem_pool_count < new_total);
#endif

    mem_pool_entry = (uint64_t *) JudyLFirst(cache_tbl->mem_array, &mem_pool_index, PJE0);
    while (mem_pool_entry!=NULL)
    {
        if (*mem_pool_entry >= new_total) {
            rc = _pop_free_index(cache_tbl, &new_index);
            if (rc) {
                assert(0);
            }
            assert(new_index<new_total);
            memcpy(get_elm_addr(cache_tbl, new_index), \
                    get_elm_addr(cache_tbl,*mem_pool_entry), \
                    cache_tbl->elm_size);
            // printf("INFO: reassign %zu to %zu\n", *mem_pool_entry, new_index);

            *mem_pool_entry = new_index;
        }
        mem_pool_entry = (uint64_t *) JudyLNext(cache_tbl->mem_array, &mem_pool_index, PJE0);
    }

    cache_tbl->start_addr = realloc(cache_tbl->start_addr, cache_tbl->elm_size * new_total);
    assert(cache_tbl->start_addr);

    free_array_idx = new_total;
    rc = Judy1First(cache_tbl->free_array, &free_array_idx, PJE0);
    while (rc) {
        rc = Judy1Unset(&cache_tbl->free_array, free_array_idx, PJE0);
        if (rc == JERR) goto process_malloc_failure;
        if (rc == 0) {
            printf("Error: shrink memory for free_array failed at index [%zu]: no such index.\n", free_array_idx);
            goto process_malloc_failure;
        }
        cache_tbl->free_num--;
        rc = Judy1Next(cache_tbl->free_array, &free_array_idx, PJE0);
    }    

#ifdef DEBUG_ENABLED
    uint64_t total_free;
    total_free = Judy1Count(cache_tbl->free_array, 0, cache_tbl->elm_num, PJE0);
    assert(total_free == cache_tbl->free_num);
#endif
    cache_tbl->elm_num = new_total;
    return 0;

process_malloc_failure:
    free_cache_tbl(cache_tbl);
    return 1;
}

void print_cache_tbl_state(cache_tbl_t *cache_tbl) {
    printf("cache_tbl [%s]: free elm [%zu], toal elm [%zu]\n", 
            cache_tbl->name, cache_tbl->free_num, cache_tbl->elm_num);
}

// pop the smallest available index, expand memory pool if necessary.
int _pop_free_index(cache_tbl_t *cache_tbl, uint64_t *free_index) {
    int rc = 0;
    uint64_t index_to_free = 0;
    if (cache_tbl->free_num == 0) {
        if (!cache_tbl->dynamic) {
            printf("Error: no free element.\n");
            return 1;
        }
        if(_extend_cache_tbl(cache_tbl)) {
            printf("ERROR: get free failed, because no free elm. \n"); 
            return 1;
        }
    } 
    rc = Judy1First(cache_tbl->free_array, &index_to_free, PJE0);
    if (rc==0) {
        printf("ERROR: no bits set in array\n");
        return 1;
    }
    rc = Judy1Unset(&cache_tbl->free_array, index_to_free, PJE0);
    assert(rc==1);
    cache_tbl->free_num--;
    *free_index = index_to_free;
    return 0;
}

// put a index to free_array, shrink cache if necessary
int _put_free_index(cache_tbl_t *cache_tbl, uint64_t index) {
    int rc = 0;
    if (index >= cache_tbl->elm_num) {
        printf("Error: index out of range. Index [%zu], Max [%zu]\n", index, cache_tbl->elm_num);
        return 1;
    }
    rc = Judy1Set(&cache_tbl->free_array, index, PJE0);
    if (rc!=1) {
        printf("Error: put free index failed.\n");
        return 1;
    }
    cache_tbl->free_num++;
    if (cache_tbl->free_num > SCALE_DOWN(cache_tbl->elm_num) && \
        (cache_tbl->elm_num/SCALE_CONST >= cache_tbl->init_elm_num)) {
        return _shrink_cache_tbl(cache_tbl);
    }
    return 0;
}

/**
 * @brief save a memory buffer to the cache table.
 * @param cache_tbl the cache table to insert.
 * @param kidx the key index specified by user.
 * @param data the buffer to cache, the size of data must be the
 *          same as cache_tbl->elm_size.
 * @return 0 for successful, 1 for failed
*/
int store_elm(cache_tbl_t *cache_tbl, uint64_t kidx, void *data) {
    uint64_t *new_entry;
    int rc = 0;
    new_entry = (uint64_t *)JudyLGet(cache_tbl->mem_array, kidx, PJE0);
    if (new_entry==NULL) {
        new_entry = (uint64_t *)JudyLIns(&cache_tbl->mem_array, kidx, PJE0);
        rc = _pop_free_index(cache_tbl, new_entry);
    }
    memcpy(get_elm_addr(cache_tbl, *new_entry), data, cache_tbl->elm_size);
    return rc;
}

/**
 * @brief return an element by key index
 * @param cache_tbl the cache table to insert.
 * @param kidx the key index specified by user.
 * @return the address to the found element or NULL for not found.
*/
void *get_elm(cache_tbl_t *cache_tbl, uint64_t kidx) {
    uint64_t *new_entry;
    new_entry = (uint64_t *)JudyLGet(cache_tbl->mem_array, kidx, PJE0);
    if (new_entry==NULL) {
        printf("Error: index is not found\n");
        return NULL;
    }

    return get_elm_addr(cache_tbl, *new_entry);
}

/**
 * @brief lookup an element by key index
 * @param cache_tbl the cache table to insert.
 * @param kidx the key index specified by user.
 * @return the address to the found element or NULL for not found.
*/
int find_elm(cache_tbl_t *cache_tbl, uint64_t kidx) {
    uint64_t *new_entry;
    new_entry = (uint64_t *)JudyLGet(cache_tbl->mem_array, kidx, PJE0);
    if (new_entry==NULL) {
        return 0;
    }
    return 1;
}

/**
 * @brief delete an element by key index
 * @param cache_tbl the cache table to insert.
 * @param kidx the key index specified by user.
 * @return 0 for successful, 1 for failed.
*/
int delete_elm(cache_tbl_t *cache_tbl, uint64_t kidx) {
    uint64_t *new_entry;
    uint64_t freed_index;
    int rc;
    new_entry = (uint64_t *)JudyLGet(cache_tbl->mem_array, kidx, PJE0);
    if (new_entry==NULL) {
        printf("Error: index is not found\n");
        return 1;
    }

    freed_index = *new_entry;
    rc = JudyLDel(&cache_tbl->mem_array, kidx, PJE0);
    if (rc==0) assert(0);
    if (rc==JERR) {
        return 1;
    }

    rc = _put_free_index(cache_tbl, freed_index);
    return rc;
}

/**
 * @brief call function on each element in the cache table
 * @param cache_tbl the cache table to iterate.
 * @param cb_fn the key index specified by user.
 * @param cb_args the call back arguments.
 * @return number of elements in cache_tbl
*/
int for_each_elm_fn(cache_tbl_t *cache_tbl, void (*cb_fn)(uint64_t, void *, void *), void* cb_args) {
    uint64_t *new_entry;
    uint64_t kidx = 0;
    uint64_t count = 0;
    new_entry = (uint64_t *)JudyLFirst(cache_tbl->mem_array, &kidx, PJE0);
    while (new_entry!=NULL) {
        count ++;
        if (cb_fn) {
            cb_fn(kidx,  get_elm_addr(cache_tbl, *new_entry), cb_args);
        }
        new_entry = (uint64_t *)JudyLNext(cache_tbl->mem_array, &kidx, PJE0);
    }
    return count;
}