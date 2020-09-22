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


#ifndef NKV_USTAT_H
#define NKV_USTAT_H

#include<ustat.h>
#include <unistd.h>
#include <alloca.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
#include <vector>
#include "csmglogger.h"

using namespace std;
extern c_smglogger* logger;

#ifdef  __cplusplus
extern "C" {
#endif

// STAT Category IO
#define STAT_NAME_IO "io"
#define STAT_NAME_DISK "disk"

// stats for device or subsystem
#define STAT_NAME_DEVICE "device"
#define STAT_NAME_CPU "cpu"

#define STAT_ENAME_LEN 64
#define STAT_ENAME_APPLICATION "nkv"

#define STAT_ENAME_IO STAT_ENAME_APPLICATION 
#define STAT_GNAME_NAME STAT_NAME_IO



typedef struct stat_io
{
  ustat_named_t del_outstanding;
  ustat_named_t put_less_4KB;
  ustat_named_t put_4KB_64KB; 
  ustat_named_t put_64KB_2MB;
  ustat_named_t get_less_4KB;
  ustat_named_t get_4KB_64KB;
  ustat_named_t get_64KB_2MB;
} stat_io_t;



extern ustat_handle_t* nkv_ustat_handle;
extern unordered_map<string, unordered_map<unsigned, stat_io_t*>> device_cpu_ustat_map;


int nkv_ustats_init(void);
ustat_handle_t* get_nkv_ustat_handle(void); 

// Reset operation 
void nkv_ustat_reset_io_stat();

// Delete nkv stats
void nkv_ustat_delete(ustat_struct_t *s);

// Initialize stats for IO paths ( NKVTragetPath )
bool nkv_init_io_stats(string& device_name, unsigned cpu_index);

// Updata ustat counters 
int nkv_ustat_atomic_add_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value);
int nkv_ustat_atomic_sub_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value);
int nkv_ustat_atomic_set_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value);
int nkv_ustat_atomic_inc_u64(ustat_struct_t* stat, ustat_named_t* counter);
int nkv_ustat_atomic_dec_u64(ustat_struct_t* stat, ustat_named_t* counter);

#ifdef __cplusplus
}
#endif
#endif


