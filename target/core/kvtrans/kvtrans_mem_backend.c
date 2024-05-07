#include "kvtrans_mem_backend.h"
#include "kvtrans.h"
#include "kvtrans_utils.h"

void insert_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index, ondisk_meta_t *meta) {
    assert(meta_ctx->inited);
    store_elm(meta_ctx->meta_mem, index, (void *)meta);
}

val_t load_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index) {
    assert(meta_ctx->inited);
    val_t val;
    val = (val_t) get_elm(meta_ctx->meta_mem, index);
    return val;
}

int found_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index) {
    assert(meta_ctx->inited);
    return find_elm(meta_ctx->meta_mem, index);
}


int delete_meta(ondisk_meta_ctx_t* meta_ctx, idx_t index) {
    assert(meta_ctx->inited);
    int rc;

    rc = delete_elm(meta_ctx->meta_mem, index);
    return rc;
}

void init_meta_ctx(ondisk_meta_ctx_t* meta_ctx, uint64_t meta_pool_size) {
    char name[32] = {"META_MEMORY_BACKEND"};
    meta_ctx->meta_mem = init_cache_tbl(name, meta_pool_size, sizeof(ondisk_meta_t), 1);
    if (!meta_ctx->meta_mem) {
        printf("ERROR: malloc g_metas failed. \n");
        return;
    }
    meta_ctx->inited = true;
}

void free_meta_ctx(ondisk_meta_ctx_t* meta_ctx) {
    if (meta_ctx->meta_mem) 
        free_cache_tbl(meta_ctx->meta_mem);
    else 
        return;
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

void reset_data_ctx(ondisk_data_ctx_t *data_ctx) {
    memset(data_ctx->data_buff_start_addr, 0, data_ctx->data_buff_size_in_byte);
    data_ctx->used_num = 0;
}