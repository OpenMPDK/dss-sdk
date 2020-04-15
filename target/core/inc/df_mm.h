/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _DFLY_MM_H_
#define _DFLY_MM_H_
#define SPDK_BDEV_SMALL_BUF_MAX_SIZE 8192
#define SPDK_BDEV_LARGE_BUF_MAX_SIZE (64 * 1024)
#define DFLY_BDEV_BUF_MAX_SIZE SPDK_BDEV_LARGE_BUF_MAX_SIZE

void *dfly_mm_init(void);
void dfly_mm_deinit(void);

/*
 * @brief Get a IO buffer from pool.
 *
 * @param vctx :Context pointer for pointing to source. Use NULL for global context
 * @param size :Size of IO buffer needed in bytes
 *
 * @return Pointer to buffer if success or NULL on failure
 */
void *dfly_io_get_buff(void *vctx, uint64_t size);

/*
 * @brief Put a IO buffer back to pool.
 *
 * @param vctx :Context pointer for pointing to source. Use NULL for global context
 * @param buff :Pointer to buffet to be returned
 *
 * @return void
 */
void dfly_io_put_buff(void *vctx, void *buff);

struct dfly_request *dfly_io_get_req(void *vctx);
void dfly_io_put_req(void *vctx, void *req);

void *dfly_get_key_buff(void *vctx, uint64_t size);
void dfly_put_key_buff(void *vctx, void *buff);


#endif // _DFLY_MM_H_
