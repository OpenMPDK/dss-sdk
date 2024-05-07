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

/**
 * @file dss_item_cache.c
 * @brief The cache APIs in this file does not use locking
 *        The caller needs to make sure the context is not called simultaneously
 * @version 0.1
 *  
 */

#include <stdlib.h>

#include "utils/dss_item_cache.h"

#ifndef DSS_BUILD_CUNIT_TEST
#include "df_log.h"
#define dss_item_cache_errlog DFLY_ERRLOG
#define dss_item_cache_noticelog DFLY_NOTICELOG
#else
#include <stdio.h>

#define dss_item_cache_errlog printf
#define dss_item_cache_noticelog printf
#endif //DSS_BUILD_CUNIT_TEST

//TAILQ Implementation vs Array based implementation
//---------------------------------------------------
//TAILQ implementation will need a link element in the item
//                     will probably have lesser lines of code
//Array implementation might use more memory than required but should be okay
//                     needs to be validated initially for corner cases with care

struct dss_item_cache_context_s {
    int nitems;
    int item_index;
    int cap;
    void **items;
};

dss_item_cache_context_t *dss_item_cache_init(int capacity)
{
    dss_item_cache_context_t *c;

    //TODO: Limit capacity to be powers of 2
    if(capacity <= 0) {
        dss_item_cache_errlog("Failing since zero or negative capacity provided\n");
        return NULL;
    }

    c = (dss_item_cache_context_t *)calloc(1, sizeof(dss_item_cache_context_t) + (capacity * sizeof(void**)));
    if(!c) {
        return NULL;
    }

    c->nitems = 0;
    c->cap = capacity;
    c->items = (void **)(c + 1);
    c->item_index = -1;

    return c;
}

int dss_item_cache_free(dss_item_cache_context_t *c)
{
    if(c && (c->nitems == 0)) {
        free((void *)c);
        return 0;
    } 

    return -1;
}

int dss_item_cache_put_item(dss_item_cache_context_t *cctx, void *item)
{
    int index;

    if(cctx->nitems == cctx->cap) {
        //Cache reached capacity
        return -1;
    }

    //Unlikely
    if(item == NULL) {
        dss_item_cache_noticelog("NULL item insert not supported\n");
        return -1;
    }

    if(cctx->item_index == -1) {
        //Cache Empty scenario
        cctx->item_index = 0;
        cctx->items[cctx->item_index] = item;
        cctx->nitems++;
        return 0;
    }

    index = (cctx->item_index + cctx->nitems) % cctx->cap;
    cctx->items[index] = item;
    cctx->nitems++;

    return 0;
}

void *dss_item_cache_get_item(dss_item_cache_context_t *cctx)
{
    void * item;

    if(cctx->nitems == 0) {
        return NULL;
    }

    item = cctx->items[cctx->item_index];

    cctx->nitems--;
    cctx->items[cctx->item_index] = NULL;
    if(cctx->nitems == 0) {
        cctx->item_index = -1;
    } else {
        cctx->item_index = (cctx->item_index + 1) % cctx->cap;
    }

    return item;

}

void dss_item_cache_print_info(dss_item_cache_context_t *cctx)
{
    dss_item_cache_noticelog("Cache %p usage (%d/%d)\n", cctx, cctx->nitems, cctx->cap);
    return;
}

int dss_item_cache_get_item_number(dss_item_cache_context_t *cctx)
{
    return cctx->nitems;
}
