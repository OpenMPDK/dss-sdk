/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
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

#include <pthread.h>

#include "dss.h"
#include "utils/dss_mallocator.h"
#include "utils/dss_item_cache.h"

struct dss_mallocator_ctx_s {
    dss_mallocator_type_t type;
    dss_mallocator_opts_t opts;
    //Private
    uint64_t allocation_size;//sizeof(struct dss_mallocator_item_id_s) + opts.item_sz
    uint32_t n_c_ctx;
    dss_item_cache_context_t **c_ctx;
    dss_mallocator_obj_cb_fn ctor;
    dss_mallocator_obj_cb_fn dtor;
    void *cb_arg;
    pthread_mutex_t allocator_lock;
};


static inline dss_mallocator_item_t *__dss_mallocator_alloc(dss_mallocator_ctx_t *c) {
    //TODO: support multiple types -- function impl
    dss_mallocator_item_t *item;
    item = malloc(c->allocation_size);
    if(!item) {
        return NULL;
    }
    if(c->ctor) {
        c->ctor(c->cb_arg, item);
    }
    return item;
}

static inline dss_mallocator_status_t __dss_mallocator_free(dss_mallocator_ctx_t *c, dss_mallocator_item_t *item) {
        //TODO: support multiple types -- use function impl
        if(c->dtor) {
            c->dtor(c->cb_arg, item);
        }
        free(item);
        return DSS_MALLOC_SUCCESS;
}

dss_mallocator_ctx_t *dss_mallocator_init_with_cb(dss_mallocator_type_t allocator_type, dss_mallocator_opts_t opts, dss_mallocator_obj_cb_fn ctor, dss_mallocator_obj_cb_fn dtor, void *cb_arg)
{
    struct dss_mallocator_ctx_s *c;
    int rc;

    if((opts.num_caches == 0) ||
        (opts.item_sz == 0)) {
        return NULL;
    }

    DSS_ASSERT(allocator_type == DSS_MEM_ALLOC_MALLOC);
    c = calloc(1, sizeof(struct dss_mallocator_ctx_s));
    if(!c) {
        return NULL;
    }

    c->opts = opts;
    c->n_c_ctx = c->opts.num_caches;
    c->allocation_size = c->opts.item_sz;

    c->ctor = ctor;
    c->dtor = dtor;
    c->cb_arg = cb_arg;
    
    rc = pthread_mutex_init(&c->allocator_lock, NULL);
    if(rc == -1) {
        free(c);
        return NULL;
    }

    c->c_ctx = calloc(c->n_c_ctx, sizeof(dss_item_cache_context_t *));
    if(!c->c_ctx) {
        free(c->c_ctx);
        free(c);
        return NULL;
    }

    return c;
}

dss_mallocator_ctx_t *dss_mallocator_init(dss_mallocator_type_t allocator_type, dss_mallocator_opts_t opts)
{
    return dss_mallocator_init_with_cb(allocator_type, opts, NULL, NULL, NULL);
}

dss_mallocator_status_t dss_mallocator_destroy(dss_mallocator_ctx_t *c)
{
    int cache_index = 0;
    dss_item_cache_context_t *cache_ctx;
    dss_mallocator_item_t *item;
    dss_mallocator_status_t rc;
    dss_mallocator_status_t status = DSS_MALLOC_SUCCESS;

    while(cache_index < c->n_c_ctx) {
        cache_ctx = c->c_ctx[cache_index];
        cache_index++;
        if(!cache_ctx) {
            continue;
        }
        while(item = dss_item_cache_get_item(cache_ctx)) {
            rc = __dss_mallocator_free(c, item);
            if(rc != 0) {
                status = DSS_MALLOC_ERROR;
            }
        }
    }
    if(c->c_ctx) free(c->c_ctx);
    if(c) free(c);

    return status;
}

dss_mallocator_status_t dss_mallocator_get(dss_mallocator_ctx_t *c, uint32_t cache_index, dss_mallocator_item_t **mitem)
{
    dss_item_cache_context_t *cache_ctx;
    dss_mallocator_item_t *item = NULL;
    dss_mallocator_status_t rc = DSS_MALLOC_SUCCESS;

    DSS_ASSERT(cache_index < c->opts.num_caches);
    *mitem = NULL;
    if(cache_index >= c->opts.num_caches) {
        return DSS_MALLOC_ERROR;
    }

    if(!c->c_ctx[cache_index]) {//Unlikely
        pthread_mutex_lock(&c->allocator_lock);
        if(!c->c_ctx[cache_index]) {//Check again after locking
            c->c_ctx[cache_index] = dss_item_cache_init(c->opts.max_per_cache_items);
            if(!c->c_ctx[cache_index]) {
                pthread_mutex_unlock((&c->allocator_lock));
                DSS_ASSERT(c->c_ctx[cache_index]);
                return DSS_MALLOC_ERROR;//Fail on cache initialization failure
            }
        }
        pthread_mutex_unlock((&c->allocator_lock));
    }

    cache_ctx = c->c_ctx[cache_index];

    item = dss_item_cache_get_item(cache_ctx);
    if(!item) {
        item = __dss_mallocator_alloc(c);
        rc = DSS_MALLOC_NEW_ALLOCATION;
    }

    *mitem = item;
    return rc;
}

dss_mallocator_status_t dss_mallocator_put(dss_mallocator_ctx_t *c, uint32_t cache_index, dss_mallocator_item_t *item)
{
    int rc;

    if(!item) {
        return DSS_MALLOC_ERROR;
    }

    if(cache_index >= c->n_c_ctx) {
        return DSS_MALLOC_ERROR;
    }

    if(!c->c_ctx[cache_index]) {
        return DSS_MALLOC_ERROR;
    }

    rc = dss_item_cache_put_item(c->c_ctx[cache_index], item);
    if(rc != 0) {
        __dss_mallocator_free(c, item);
    }

    return DSS_MALLOC_SUCCESS;
}
