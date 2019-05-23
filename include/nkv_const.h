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

#ifndef NKV_CONST_H
#define NKV_CONST_H

#ifdef __cplusplus
extern "C" {
#endif

#define NKV_CONFIG_FILE "/etc/nkv/nkv_config.json"

#define NKV_TRANSPORT_LOCAL_KERNEL 0
#define NKV_TRANSPORT_NVMF_TCP_KERNEL 1
#define NKV_TRANSPORT_NVMF_TCP_SPDK 2
#define NKV_TRANSPORT_NVMF_RDMA_KERNEL 3
#define NKV_TRANSPORT_NVMF_RDMA_SPDK 4
#define NKV_MAX_ENTRIES_PER_CALL 16 
#define NKV_MAX_CONT_NAME_LENGTH 256 
#define NKV_MAX_CONT_TRANSPORT 32 //For NKV it will be max 4 but big number is to support LKV 
#define NKV_MAX_IP_LENGTH 32 
#define NKV_MAX_KEY_LENGTH 256 
#define NKV_MAX_MOUNT_POINT_LENGTH 16 
#define NKV_MAX_VALUE_LENGTH 2097152 //2MB value support 

#ifdef __cplusplus
} // extern "C"
#endif

#endif

