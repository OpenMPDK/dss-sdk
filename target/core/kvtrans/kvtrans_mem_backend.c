#include "kvtrans_mem_backend.h"
#include "kvtrans.h"

void insert_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index, ondisk_meta_t *meta) {
    assert(meta_ctx->inited);
    Word_t *new_entry;
    uint64_t pool_idx;
    new_entry = (Word_t *)JudyLGet(meta_ctx->Parray, (Word_t)index, PJE0);
    if (new_entry==NULL) {
        if (meta_ctx->free_num == 0) {
            pool_idx = meta_ctx->num;
            meta_ctx->num++;
        } else {
            pool_idx = meta_ctx->free_index[meta_ctx->free_num-1];
            meta_ctx->free_num--;
        }
        memcpy(&meta_ctx->pool[pool_idx], meta, sizeof(ondisk_meta_t));
        new_entry = (Word_t *)JudyLIns(&meta_ctx->Parray, (Word_t)index, PJE0);
        *new_entry = (Word_t) &meta_ctx->pool[pool_idx];
    } else {
        ondisk_meta_t *ondisk_meta;
        ondisk_meta = (ondisk_meta_t *) *new_entry;
        memcpy(ondisk_meta, meta, sizeof(ondisk_meta_t)); 
    }
}

val_t load_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index) {
    assert(meta_ctx->inited);
    Word_t *new_entry;
    val_t val;
    new_entry = (Word_t *)JudyLGet(meta_ctx->Parray, (Word_t)index, PJE0);
    val = (val_t) *new_entry;
    return val;
}

int found_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index) {
    assert(meta_ctx->inited);
    Word_t *new_entry;
    new_entry = (Word_t *)JudyLGet(meta_ctx->Parray, (Word_t)index, PJE0);
    return new_entry!=NULL;
}


int delete_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index) {
    assert(meta_ctx->inited);
    int rc;
    // val_t val;
    // val = load_meta(index);
    // assert(val);
     
    // pool_idx = val - meta_ctx->pool;

    // if (pool_idx==meta_ctx->num-1) {
    //     meta_ctx->num--;
    // } else {
    //     meta_ctx->free_index[meta_ctx->free_num] = pool_idx;
    //     meta_ctx->free_num++;
    // }

    rc = JudyLDel(&meta_ctx->Parray, (Word_t)index, PJE0);
    if (rc==0) {
        printf("index not present\n");
        return KVTRANS_STATUS_ERROR;
    } else if (rc==JERR) {
        printf("malloc failed\n");
        return KVTRANS_STATUS_ERROR;
    } 
    meta_ctx->num--;
    return rc;
}


int free_metas(ondisk_meta_ctx_t* meta_ctx) {
    assert(meta_ctx->inited);
    Word_t freed_bytes;
    freed_bytes = JudyLFreeArray(&meta_ctx->Parray, PJE0);
    return freed_bytes;
}

val_t get_first_meta(ondisk_meta_ctx_t* meta_ctx) {
    assert(meta_ctx->inited);
    idx_t index;
    Word_t *new_entry;
    val_t val;
    new_entry = (Word_t *) JudyLFirst(meta_ctx->Parray, &index, PJE0);
    val = (val_t) *new_entry;
    return val;
}

void log_metas(ondisk_meta_ctx_t* meta_ctx, char *file_path) {
    if(!meta_ctx->inited) {
        printf( "meta memory backend not initialized");
        return;
    }

    idx_t index;
    Word_t *new_entry;
    ondisk_meta_t *meta;
    FILE *fptr;
    val_t val;
    fptr = fopen(file_path, "w");
    fprintf(fptr, "index, valid_col, valid_val, value_location\n");
    new_entry = (Word_t *) JudyLFirst(meta_ctx->Parray, &index, PJE0);
    while (new_entry)
    {
        val = (val_t) *new_entry;
        meta = (ondisk_meta_t *)val;
        fprintf(fptr, "%zu, %2x, %2x, %d\n", index, meta->num_valid_col_entry, meta->num_valid_place_value_entry, meta->value_location);
       new_entry = (Word_t *) JudyLNext(meta_ctx->Parray, &index, PJE0);
    }
    fclose(fptr);
}

void init_meta_ctx(ondisk_meta_ctx_t* meta_ctx, uint64_t meta_pool_size) {
    meta_ctx->pool_size = meta_pool_size;
    meta_ctx->num = 0;
    meta_ctx->free_num = 0;
    meta_ctx->pool = (ondisk_meta_t *) malloc (sizeof(ondisk_meta_t) * meta_ctx->pool_size);
    if (!meta_ctx->pool) {
        printf("ERROR: malloc g_metas failed. \n");
        return;
    }
    meta_ctx->free_index = calloc(INIT_FREE_INDEX_SIZE, sizeof(uint64_t));
    memset(meta_ctx->pool, 0, sizeof(ondisk_meta_t) * meta_ctx->pool_size);
    meta_ctx->inited = true;
}

void free_meta_ctx(ondisk_meta_ctx_t* meta_ctx) {
    if (meta_ctx) 
        free_metas(meta_ctx);
    else 
        return;
    if (meta_ctx->pool) free(meta_ctx->pool);
    if (meta_ctx->free_index) free(meta_ctx->free_index);
    free(meta_ctx);
}

void *get_data_addr(void *base, uint64_t offset) {
    return (void *)((char *)base + offset);
}

bool insert_data(ondisk_data_ctx_t* data_ctx, idx_t index, uint64_t num_blk, void *buff) {
    if (!buff) {
        printf("ERROR: data buffer is null.\n");
        return false;
    }
    void *addr = get_data_addr(data_ctx->data_buff_start_addr, index * BLOCK_SIZE);
    memcpy(addr, buff, num_blk * BLOCK_SIZE);
    // printf("save data at [%zu], at [%p] with blk_num [%zu]\n", index, addr, num_blk);
    return true;
}

bool retrieve_data(ondisk_data_ctx_t* data_ctx, idx_t index, uint64_t num_blk, void *buff) {
    memcpy(buff, get_data_addr(data_ctx->data_buff_start_addr, index * BLOCK_SIZE), 
            num_blk * BLOCK_SIZE);
    return true;
}

bool delete_data(ondisk_data_ctx_t* data_ctx, idx_t index, uint64_t num_blk) {
    memset(get_data_addr(data_ctx->data_buff_start_addr, index * BLOCK_SIZE),
             0, num_blk * BLOCK_SIZE);
    return true;
}

void init_data_ctx(ondisk_data_ctx_t *data_ctx, uint64_t data_pool_size) {
    data_ctx->blk_num = data_pool_size;
    data_ctx->data_buff_size_in_byte = data_ctx->blk_num * BLOCK_SIZE;
    data_ctx->data_buff_start_addr = malloc(data_ctx->data_buff_size_in_byte);
    if (!data_ctx->data_buff_start_addr) {
        printf("ERROR: data memory backend init failed.\n");
        return;
    }
    memset(data_ctx->data_buff_start_addr, 0, data_ctx->data_buff_size_in_byte);
    data_ctx->used_num = 0;
    data_ctx->inited = true;
}

void free_data_ctx(ondisk_data_ctx_t *data_ctx) {
    if (data_ctx->data_buff_start_addr) {
        free(data_ctx->data_buff_start_addr);
    }
    free(data_ctx);
}