/**
 # The Clear BSD License
 #
 # Copyright (c) 2022 Samsung Electronics Co., Ltd.
 # All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted (subject to the limitations in the disclaimer
 # below) provided that the following conditions are met:
 #
 # * Redistributions of source code must retain the above copyright notice, 
 #   this list of conditions and the following disclaimer.
 # * Redistributions in binary form must reproduce the above copyright notice,
 #   this list of conditions and the following disclaimer in the documentation
 #   and/or other materials provided with the distribution.
 # * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 #   contributors may be used to endorse or promote products derived from this
 #   software without specific prior written permission.
 # NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 # THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 # CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 # NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 # PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 # OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 # WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 # OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 # ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

// Type of stats
#define STAT_TYPE_OUTSTANDING "outstanding"
#define STAT_TYPE_PENDING "pending"
#define STAT_TYPE_QD "qd"


#define STAT_ENAME_IO STAT_ENAME_APPLICATION 
#define STAT_GNAME_NAME STAT_TYPE_OUTSTANDING



typedef struct stat_io
{
  ustat_named_t put_less_4KB;
  ustat_named_t put_4KB_64KB; 
  ustat_named_t put_64KB_2MB;
  ustat_named_t get_less_4KB;
  ustat_named_t get_4KB_64KB;
  ustat_named_t get_64KB_2MB;
  ustat_named_t put;
  ustat_named_t get;
  ustat_named_t del;
} stat_io_t;

int nkv_ustats_init(void);

ustat_struct_t* nkv_register_application_counter(const char* app_module_name, ustat_named_t* app_counter);
// Reset operation 
void nkv_ustat_reset_io_stat(stat_io_t* stat);

// Delete nkv stats
void nkv_ustat_delete(ustat_struct_t *s);

// Initialize stats for IO paths ( NKVTragetPath )
stat_io_t* nkv_init_path_io_stats(string& device_name, bool cpu_stat, unsigned cpu_index=0);



// Updata ustat counters 
void nkv_ustat_atomic_add_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value);
void nkv_ustat_atomic_sub_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value);
void nkv_ustat_atomic_set_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value);
void nkv_ustat_atomic_inc_u64(ustat_struct_t* stat, ustat_named_t* counter);
void nkv_ustat_atomic_dec_u64(ustat_struct_t* stat, ustat_named_t* counter);

uint64_t nkv_ustat_get_u64(ustat_struct_t* stat, ustat_named_t* counter);

#ifdef __cplusplus
}
#endif
#endif


