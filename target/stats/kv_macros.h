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


#ifndef __KV_MACROS_H
#define __KV_MACROS_H

#ifdef __cplusplus
extern "C" {
#endif

#define KV_STATS_INIT_CPU_CTX(CPU_STATS_CTX_ARR, LEN) \
    { \
        int i = 0; \
        for (; i < LEN; ++i) { \
            CPU_STATS_CTX_ARR[i] = NULL; \
        } \
    }

#define KVB_COUNT(CPU_CTX, KVB_CTX, COUNTER) \
    { \
        int cpu_id = sched_getcpu(); \
        if (!CPU_CTX[cpu_id]) CPU_CTX[cpu_id] = kv_stats_init_host(cpu_id); \
        if (!KVB_CTX[cpu_id]) { \
           KVB_CTX[cpu_id] = kv_stats_get_kvb_counters(CPU_CTX[cpu_id]); \
        } \
        ATOMIC_INC(KVB_CTX[cpu_id]->COUNTER); \
    }

#define KVV_COUNT(CPU_CTX, KVV_CTX, DEV, CONTAINER, COUNTER) \
    { \
        int cpu_id = sched_getcpu(); \
        if (!CPU_CTX[cpu_id]) CPU_CTX[cpu_id] = kv_stats_init_host(cpu_id); \
        if (!KVV_CTX[cpu_id][(DEV)][(CONTAINER)]) { \
            KVV_CTX[cpu_id][(DEV)][(CONTAINER)] = \
                kv_stats_get_kvv_counters(CPU_CTX[cpu_id], (DEV), (CONTAINER)); \
        } \
        ATOMIC_INC(KVV_CTX[cpu_id][(DEV)][(CONTAINER)]->COUNTER); \
    }

#define KV_NVMEF_COUNT(CPU_CTX, KV_NVMEF_CTX, DEVICE_NAME, COUNTER) \
    { \
        int cpu_id = sched_getcpu(); \
        if (!CPU_CTX[cpu_id]) CPU_CTX[cpu_id] = kv_stats_init_host(cpu_id); \
        if (!KV_NVMEF_CTX[cpu_id]) { \
            KV_NVMEF_CTX[cpu_id] = \
                kv_stats_get_kv_nvmef_counters(CPU_CTX[cpu_id], (DEVICE_NAME), strlen((DEVICE_NAME))); \
        } \
        ATOMIC_INC(KV_NVMEF_CTX[cpu_id]->COUNTER); \
    }

#define KV_NVME_COUNT(CPU_CTX, KV_NVME_CTX, DEVICE_NAME, COUNTER) \
    { \
        int cpu_id = sched_getcpu(); \
        if (!CPU_CTX[cpu_id]) CPU_CTX[cpu_id] = kv_stats_init_target(cpu_id); \
        if (!KV_NVME_CTX[cpu_id]) { \
            KV_NVME_CTX[cpu_id] = \
                kv_stats_get_kv_nvme_counters(CPU_CTX[cpu_id], (DEVICE_NAME), strlen((DEVICE_NAME))); \
        } \
        ATOMIC_INC(KV_NVME_CTX[cpu_id]->COUNTER); \
    }

#ifdef __cplusplus
}
#endif
#endif //__KV_MACROS_H
