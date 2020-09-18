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

#include "nkv_stats.h"

int nkv_ustats_get_ename(char *ename, const char* device_name, unsigned cpu_index);

const ustat_class_t ustat_class_nkv = {
	.usc_name = "nkv",
	.usc_ctor = NULL,
	.usc_dtor = NULL,
	.usc_bson = NULL,
};

const stat_io_t stat_io_table = {
	{ "num_outstanding_small_put", USTAT_TYPE_UINT64, 0, NULL},
	{ "num_outstanding_mid_put"  , USTAT_TYPE_UINT64, 0, NULL},
	{ "num_outstanding_big_put"  , USTAT_TYPE_UINT64, 0, NULL},
	{ "num_outstanding_small_get", USTAT_TYPE_UINT64, 0, NULL},
	{ "num_outstanding_mid_get"  , USTAT_TYPE_UINT64, 0, NULL},
	{ "num_outstanding_big_get"  , USTAT_TYPE_UINT64, 0, NULL},
        { "num_outstanding_del"      , USTAT_TYPE_UINT64, 0, NULL},
};


ustat_handle_t* nkv_ustat_handle = NULL;
unordered_map<string, unordered_map<unsigned, stat_io_t*>> device_cpu_ustat_map;

/*
 * Params: category_type = device or subsystem
 *         type_name = device name(nvme1n) or subsystem nqn name
 * Return: <const char*> - ename, <application name>.<device>.<cpu>.<cpu_index>.<io>
 */
int nkv_ustats_get_ename(char *ename, const char* device_name, unsigned cpu_index)
{
  size_t c;
  if ( device_name == NULL ) {
    return (-1);
  }
  c = snprintf(ename,STAT_ENAME_LEN, "%s.%s.%s.%s.%d", STAT_ENAME_APPLICATION ,STAT_NAME_DEVICE, device_name, STAT_NAME_CPU, cpu_index);

  return c;
}

// Return ustat handler
ustat_handle_t* get_nkv_ustat_handle(void)
{
  return nkv_ustat_handle;
}

// Initialize ustat
int nkv_ustats_init(void)
{
   ustat_handle_t *handler = ustat_open_proc(USTAT_VERSION, 0, O_RDWR | O_CREAT);
   if (handler == NULL) {
     smg_error(logger,"Failed to open ustat handler");
     return -1;
   }

  nkv_ustat_handle = handler;
  return 0;
}

// Delete the ustats
void nkv_ustat_delete(ustat_struct_t *s)
{
  ustat_delete(s);
}


// Initiate stats counters for IO path 
bool nkv_init_io_stats(string& device_name, unsigned cpu_index)
{
  // Get ustat handle
  ustat_handle_t* ustat_handler = get_nkv_ustat_handle();

  if(! ustat_handler ) {
    smg_error(logger, "Received NULL ustat handler!");
    return false;
  }

  // Initialize ustat counters
  char *ename = alloca(STAT_ENAME_LEN);
  nkv_ustats_get_ename(ename, device_name.c_str(), cpu_index);

  stat_io_t* device_stat_io;
  try{
    device_stat_io = (stat_io_t *)ustat_insert(ustat_handler,
                                    ename, 
                                    STAT_GNAME_NAME,
				    &ustat_class_nkv,
				    sizeof(stat_io_table) / sizeof(ustat_named_t),
  		         	    &stat_io_table, NULL);

    device_cpu_ustat_map[device_name].insert({cpu_index,device_stat_io});

  } catch(exception& e ) {
    smg_error(logger, "EXCEPTION: stat counter initialization failed - %s", e.what());
    return false;
  }
  
  return true;
}

// Reset all counters
void nkv_ustat_reset_io_stat()
{
  smg_alert(logger, "Ustat Reset operation initiated! All counters' values will reset.");

  // device and cpu level stats reset
  for(auto cpu_stats : device_cpu_ustat_map ) {
    for(auto cpu_stat: cpu_stats.second) {
      stat_io_t * stat = cpu_stat.second;
      // Set based on nkv_path_stat_detailed
      ustat_set_u64(stat, &stat->num_outstanding_del, 0);
      ustat_set_u64(stat, &stat->num_outstanding_small_put, 0);
      ustat_set_u64(stat, &stat->num_outstanding_mid_put, 0);
      ustat_set_u64(stat, &stat->num_outstanding_big_put, 0);
      ustat_set_u64(stat, &stat->num_outstanding_small_get, 0);
      ustat_set_u64(stat, &stat->num_outstanding_mid_get, 0);
      ustat_set_u64(stat, &stat->num_outstanding_big_get, 0);
    }
  }
}

// Increment counter with specified value.
int nkv_ustat_atomic_set_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value)
{
  ustat_set_u64(stat, counter , value);
}

// Increment counter with specified value.
int nkv_ustat_atomic_add_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value)
{
  ustat_atomic_add_u64(stat, counter , value);
}

// Decrement counter with specified value.
int nkv_ustat_atomic_sub_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value)
{
  ustat_atomic_sub_u64(stat, counter , value);
}

// Increment counter with specified value.
int nkv_ustat_atomic_inc_u64(ustat_struct_t* stat, ustat_named_t* counter)
{
  ustat_atomic_inc_u64(stat, counter);
}

// Decrement counter with specified value.
int nkv_ustat_atomic_dec_u64(ustat_struct_t* stat, ustat_named_t* counter)
{
  ustat_atomic_dec_u64(stat, counter);
}

