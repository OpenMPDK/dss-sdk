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

#ifndef DSS_ITEM_CACHE_H
#define DSS_ITEM_CACHE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dss_item_cache_context_s dss_item_cache_context_t;

/**
 * 
 * @brief Create and initialize a cache context to be used by single thread
 *  
 * @param capacity The maximum number of items the cache can hold
 * 
 * @return dss_item_cache_context_t* if initialization was successfull or 
 *         NULL if initialization failed
 */
dss_item_cache_context_t *dss_item_cache_init(int capacity);

/**
 * @brief Free the cache context provided. Cache should be empty;
 * 
 * @param cctx The cache to be freed
 * 
 * @return 0 if cache was freed -1 otherwise
 */
int dss_item_cache_free(dss_item_cache_context_t *cctx);

/**
 * @brief Insert an item to cache
 * 
 * @param cctx Cache context to return item to
 * @param item The item to be returned
 * 
 * @return 0 if item was pushed successfully -1 otherwise
 */
int dss_item_cache_put_item(dss_item_cache_context_t *cctx, void *item);

/**
 * @brief Get item from cache if avaliable
 * 
 * @param cctx Cache context
 * @return void* valid item pointer, NULL otherwise
 */
void *dss_item_cache_get_item(dss_item_cache_context_t *cctx);

/**
 * @brief Prints usage information of cache context
 * 
 * @param cctx Cache context
 */
void dss_item_cache_print_info(dss_item_cache_context_t *cctx);

#ifdef __cplusplus
}
#endif

#endif //DSS_ITEM_CACHE_H
