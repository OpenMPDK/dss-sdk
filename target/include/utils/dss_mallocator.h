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

#ifndef DSS_MALLOCATOR_H
#define DSS_MALLOCATOR_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dss_mallocator_ctx_s dss_mallocator_ctx_t;
typedef void dss_mallocator_item_t;

typedef enum dss_mallocator_type_e {
    DSS_MEM_ALLOC_MALLOC = 1
} dss_mallocator_type_t;

typedef enum dss_mallocator_status_e {
    DSS_MALLOC_SUCCESS = 0,
    DSS_MALLOC_NEW_ALLOCATION = 1,
    /* Add new errors here*/
    DSS_MALLOC_ERROR = -1
} dss_mallocator_status_t;

typedef struct dss_mallocator_opts_s {
    union {
        struct {
            void *dummy;//No extra param for malloc type
        } malloc; 
    } type;
    size_t item_sz;
    uint32_t num_caches;
    uint64_t max_per_cache_items;
} dss_mallocator_opts_t;

dss_mallocator_ctx_t *dss_mallocator_init(dss_mallocator_type_t allocator_type, dss_mallocator_opts_t opts);
dss_mallocator_status_t dss_mallocator_destroy(dss_mallocator_ctx_t *c);

dss_mallocator_status_t dss_mallocator_get(dss_mallocator_ctx_t *c, uint32_t cache_index, dss_mallocator_item_t **mitem);
dss_mallocator_status_t dss_mallocator_put(dss_mallocator_ctx_t *c, uint32_t cache_index, dss_mallocator_item_t *item);

dss_mallocator_status_t dss_mallocator_get_cache_size(dss_mallocator_ctx_t *c, uint32_t cache_index, int *cache_size);

#ifdef __cplusplus
}
#endif

#endif //DSS_MALLOCATOR_H