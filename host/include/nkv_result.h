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

#ifndef NKV_RESULT_H
#define NKV_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

// kvs_result messages;	
typedef enum {	
  // generic command status	
  NKV_SUCCESS=0, // success
  //NKV Errors starting from > 1000 err code
  NKV_ERR_UNREACHABLE = 0x001, // Not able to communicate with NKV-FM
  NKV_ERR_CONFIG = 0x002,    // Error parsing nkv config file
  NKV_ERR_NULL_INPUT= 0x003,  // config_file, app_uuid or any other required input param is NULL
  NKV_ERR_INTERNAL = 0x004,    // Error parsing nkv config file
  NKV_ERR_COMMUNICATION = 0x005, // Error communicating with devices/Paths, path open failed ?
  NKV_ERR_ALREADY_INITIALIZED = 0x006,
  NKV_ERR_WRONG_INPUT = 0x007,
  NKV_ERR_INS_STOPPING = 0x008,
  NKV_ERR_NO_CNT_FOUND = 0x009,
  NKV_ERR_HANDLE_INVALID = 0x00A,
  NKV_ERR_INVALID_START_INDEX = 0x00B,
  NKV_ERR_NO_CNT_PATH_FOUND = 0x00C,
  NKV_ERR_KEY_LENGTH = 0x00D,
  NKV_ERR_VALUE_LENGTH = 0x00E,
  NKV_ERR_IO = 0x00F,
  NKV_ERR_OPTION_ENCRYPTION_NOT_SUPPORTED = 0x010,
  NKV_ERR_OPTION_CRC_NOT_SUPPORTED = 0x011,
  NKV_ERR_BUFFER_SMALL = 0x012,
  NKV_ERR_CNT_INITIALIZED = 0x013,
  NKV_ERR_COMMAND_SUBMITTED = 0x014,
  NKV_ERR_CNT_CAPACITY = 0x015,
  NKV_ERR_KEY_EXIST = 0x016,
  NKV_ERR_KEY_INVALID = 0x017,
  NKV_ERR_KEY_NOT_EXIST = 0x018,
  NKV_ERR_CNT_BUSY = 0x019,
  NKV_ERR_CNT_IO_TIMEOUT = 0x01A,
  NKV_ERR_VALUE_LENGTH_MISALIGNED = 0x01B,
  NKV_ERR_VALUE_UPDATE_NOT_ALLOWED = 0x01C,
  NKV_ERR_PERMISSION = 0x01D,
  NKV_ERR_MAXIMUM_VALUE_SIZE_LIMIT_EXCEEDED = 0x01E,
  NKV_ITER_MORE_KEYS = 0x01F,
  NKV_ERR_LOCK_KEY_LOCKED = 0x20,
  NKV_ERR_LOCK_UUID_MISMATCH = 0x21,
  NKV_ERR_LOCK_NO_WRITER = 0x22,
  NKV_ERR_LOCK_NO_READER = 0x23,
  NKV_ERR_LOCK_EXPIRED = 0x24,
  NKV_NOT_SUPPORTED = 0x025, 
  NKV_ERR_CNT_VERIFY_FAILED = 0x026, // When the load balancer is enabled, 
	                                 // the container path hash is different 
	                                 // from the container hash nkv exposed 
  NKV_ERR_MODE_NOT_SUPPORT = 0x027,   // The feature doesn't support 
  NKV_ERR_CNT_PATH_DOWN = 0x028,   // The Container path status is down
  NKV_ERR_FM = 0x029 // FM error

} nkv_result;  

#ifdef __cplusplus
} // extern "C"
#endif

#endif
