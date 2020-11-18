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
#include "nkv_framework.h"

int nkv_ustats_get_ename(char *ename, const char* device_name, unsigned cpu_index);

const ustat_class_t ustat_class_nkv = {
	.usc_name = "nkv",
	.usc_ctor = NULL,
	.usc_dtor = NULL,
	.usc_bson = NULL,
};

const stat_io_t stat_io_table = {
	{ "put_less_4KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "put_4KB_16KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "put_64KB_2MB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_less_4KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_4KB_64KB", USTAT_TYPE_UINT64, 0, NULL},
	{ "get_64KB_2MB", USTAT_TYPE_UINT64, 0, NULL},
        { "put"         , USTAT_TYPE_UINT64, 0, NULL},
        { "get"         , USTAT_TYPE_UINT64, 0, NULL},
        { "del"         , USTAT_TYPE_UINT64, 0, NULL},
};


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
  c = snprintf(ename,STAT_ENAME_LEN, "%s.%s.%s.%s.%d.%s", STAT_ENAME_APPLICATION ,STAT_NAME_DEVICE, device_name, STAT_NAME_CPU, cpu_index,STAT_NAME_IO);

  return c;
}

// Initialize ustat
int nkv_ustats_init(void)
{
   ustat_handle_t *handler = ustat_open_proc(USTAT_VERSION, 0, O_RDWR | O_CREAT);
   if (handler == NULL) {
     smg_error(logger,"Failed to open ustat handler");
     return -1;
   }

  nkv_cnt_list->set_nkv_ustat_handle(handler);
  smg_alert(logger, "Initialized nkv Ustat handler! ");
  return 0;
}

// Delete the ustats
void nkv_ustat_delete(ustat_struct_t *s)
{
  if(s) {
    ustat_delete(s);
  }
}


ustat_struct_t* nkv_register_application_counter(const char* app_module_name, ustat_named_t* app_counter) {

  ustat_handle_t* ustat_handler = nkv_cnt_list->get_nkv_ustat_handle();

  if(! ustat_handler ) {
    smg_error(logger, "Received NULL ustat handler!");
    return NULL;
  }

  return (ustat_insert(ustat_handler, app_module_name, STAT_GNAME_NAME,
            &ustat_class_nkv, 1, app_counter, NULL));


}

// Initiate stats counters for IO path 
stat_io_t* nkv_init_path_io_stats(string& device_name, bool cpu_stat, unsigned cpu_index)
{
  // Get ustat handle
  ustat_handle_t* ustat_handler = nkv_cnt_list->get_nkv_ustat_handle();

  if(! ustat_handler ) {
    smg_error(logger, "Received NULL ustat handler!");
    return NULL;
  }
  
  // Initialize ustat counters
  char *ename = alloca(STAT_ENAME_LEN);
  if( cpu_stat ) {
    snprintf(ename,STAT_ENAME_LEN, "%s.%s.%s.%s.%d.%s", STAT_ENAME_APPLICATION ,STAT_NAME_DEVICE, device_name.c_str(), STAT_NAME_CPU, cpu_index,STAT_NAME_IO);
  } else {
    snprintf(ename,STAT_ENAME_LEN, "%s.%s.%s.%s", STAT_ENAME_APPLICATION ,STAT_NAME_DEVICE, device_name.c_str(),STAT_NAME_IO);
  }
  
  stat_io_t* device_stat_io = NULL;
  try{
    device_stat_io = (stat_io_t *)ustat_insert(ustat_handler,
                                    ename, 
                                    STAT_GNAME_NAME,
				    &ustat_class_nkv,
				    sizeof(stat_io_table) / sizeof(ustat_named_t),
  		         	    &stat_io_table, NULL);

  } catch(exception& e ) {
    smg_error(logger, "EXCEPTION: stat counter initialization failed - %s", e.what());
  }

  return device_stat_io;
}

// Reset all counters
void nkv_ustat_reset_io_stat(stat_io_t* stat)
{
  ustat_atomic_set_u64(stat, &stat->put_less_4KB, 0);
  ustat_atomic_set_u64(stat, &stat->put_4KB_64KB, 0);
  ustat_atomic_set_u64(stat, &stat->put_64KB_2MB, 0);
  ustat_atomic_set_u64(stat, &stat->get_less_4KB, 0);
  ustat_atomic_set_u64(stat, &stat->get_4KB_64KB, 0);
  ustat_atomic_set_u64(stat, &stat->get_64KB_2MB, 0);
  ustat_atomic_set_u64(stat, &stat->put, 0);
  ustat_atomic_set_u64(stat, &stat->get, 0);
  ustat_atomic_set_u64(stat, &stat->del, 0);
}

// Increment counter with specified value.
void nkv_ustat_atomic_set_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value)
{
  if( stat ) {
    ustat_atomic_set_u64(stat, counter , value);
  }
}

// Increment counter with specified value.
void nkv_ustat_atomic_add_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value)
{
  if( stat ) {
    ustat_atomic_add_u64(stat, counter , value);
  }
}

// Decrement counter with specified value.
void nkv_ustat_atomic_sub_u64(ustat_struct_t* stat, ustat_named_t* counter, uint64_t value)
{
  if( stat ) {
    ustat_atomic_sub_u64(stat, counter , value);
  }
}

// Increment counter with specified value.
void nkv_ustat_atomic_inc_u64(ustat_struct_t* stat, ustat_named_t* counter)
{
  if( stat ) {
    ustat_atomic_inc_u64(stat, counter);
  }
}

// Decrement counter with specified value.
void nkv_ustat_atomic_dec_u64(ustat_struct_t* stat, ustat_named_t* counter)
{
  if( stat ) {
    if(nkv_ustat_get_u64(stat,counter)) {
      ustat_atomic_dec_u64(stat, counter);
    }
  }
}

// Return counter value
uint64_t nkv_ustat_get_u64(ustat_struct_t* stat, ustat_named_t* counter)
{
  return ustat_get_u64(stat, counter);
}
