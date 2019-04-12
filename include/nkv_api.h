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

#ifndef NKV_API_H
#define NKV_API_H

#include <memory.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

#include "nkv_result.h"
#include "nkv_const.h"
#include "nkv_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! Initialize the library
 *   
 *  This function must be called once to initialize the environment for the library for an NKV instance
 *  IN     config_file -- Absolute path of NKV config file consisting of NKV-FM information and other 
 *                        configuration related to NKV.
 *  IN     app_uuid  -- Unique app identifier to support multiple app at the same time on top of NKV.
 *                      With this appuuid, NKV generates a unique nkv_handle to isolate and protect different app data.
 *  IN     host_name_ip – Host name or IP where NKV instance will be running
 *  IN     host_port  – Host port where this instance will be running
 *  IN     instance_uuid  -- Unique NKV instance identifier. Could be generated in combination with (app_uuid, host_name_ip, host_port) 
 *  OUT    nkv_handle – A positive unique id for combination of nkv and the application(not instance). -1 in case of error.
 *      
 */

nkv_result nkv_open(const char *config_file, const char* app_uuid, const char* host_name_ip, uint32_t host_port, 
                    uint64_t *instance_uuid, uint64_t* nkv_handle);

/*! Deinitialize the library
 *  
 *  It closes all opened devices and releases any the system resources assigned by the library
 *  IN    instance_uuid  -- Unique NKV instance identifier. Could be generated in combination with (app_uuid, host_name_ip, host_port)
 *  IN    nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call 
 *             
 */

nkv_result nkv_close (uint64_t nkv_handle, uint64_t instance_uuid);


/*! Get nkv instances
 *  
 *  This API returns the metadata info related to a specific NKV instance opened
 *  IN    instance_uuid  -- Unique NKV instance identifier. Could be generated in combination with (app_uuid, host_name_ip, host_port)
 *  IN    nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN/OUT   info - nkv_instance_info structure containing meta information for a NKV instance,caller is responsible to allocate this.
 *
 */

nkv_result nkv_get_instance_info(uint64_t nkv_handle, uint64_t instance_uuid, nkv_instance_info* info);


/*! list nkv instances
 *  
 *  This API returns the metadata info related to all NKV instance opened
 *  IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN     host_name_ip – Filtered by Host name or IP, could be NULL
 *  IN     index -- Start offset index in the list, first call it will be 0 and then added with num_instances returned.
 *  IN/OUT info - Array of nkv_instance_info structure containing meta information for a NKV instance
 *  IN/OUT num_instances – Total number of pre-allocated instances as IN param and actual number of element as OUT
 *        
 */

nkv_result nkv_list_instance_info(uint64_t nkv_handle, const char* host_name_ip , uint32_t index, 
                                  nkv_instance_info* info, uint32_t* num_instances);


/*! Get version info of a nkv instances
 *  
 *  This API returns the version info related to a specific NKV instance opened
 *  IN    instance_uuid  -- Unique NKV instance identifier. Could be generated in combination with (app_uuid, host_name_ip, host_port)
 *  IN    nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  OUT   major_v – major version of this NKV library 
 *  OUT   minor_v – minor version of this NKV library
 *        
 */

nkv_result nkv_get_version_info(uint64_t nkv_handle, uint64_t instance_uuid, uint32_t* major_v, uint32_t* minor_v);


/*! Get container list for data placement
 *  
 *  This API returns the physical containers available for storing/retrieving KV pairs
 *  IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN     index - Start index of the physical container list
 *  IN/OUT cntlist – Array of physical container information, needs to be pre-allocated
 *  IN/OUT cnt_count – Number of entries preallocated as IN and number of valid entries in cntlist as OUT
 *         
 */

nkv_result nkv_physical_container_list (uint64_t nkv_handle, uint32_t index, nkv_container_info *cntlist, uint32_t *cnt_count);

/*! Allocate aligened memory 
 *  
 *  It allocates from hugepages when DPDK is enabled,
 *  otherwise allocate from the memory in the current NUMA node.
 *  IN size -  in bytes
 *  IN alignment -  optional alignment, default 4K
 *  return a memory pointer if succeeded, NULL otherwise.
 *        
 */

void* nkv_malloc(size_t size, size_t alignment = 4096);


/*! Allocate zeroed aligened memory
 *  
 *  It allocates from hugepages when DPDK is enabled,
 *  otherwise allocate from the memory in the current NUMA node.
 *  IN size -  in bytes
 *  IN alignment -  optional alignment, default 4K
 *  return a memory pointer if succeeded, NULL otherwise.
 *  
 */

void* nkv_zalloc(size_t size, size_t alignment = 4096);

/*! Allocate zeroed aligened memory
 *  
 *  It frees up memory
 *  IN buf - pointer to the memory region to be freed
 *       
 */

void nkv_free(void* buf);


/*! Store a KV Pair to the container
 *  
 *   This API stores a KV pair to the container in sync way
 *   IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *   IN     ioctx - nkv_io_context buffer required to perform IO on NKV
 *   IN     key – Key for which key value pair information will be stored
 *   IN     opt - nkv_store_option structure for specifying store option
 *   IN     value – nkv_value structure containing the value for the key. Needs to be pre-allocated by user.
 *         
 */


nkv_result nkv_store_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_store_option* opt, nkv_value* value);


/*! Retrieve a KV Pair to the container
 *  
 *  This API retrieves a KV pair to the container in sync way
 *  IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN     ioctx - nkv_io_context buffer required to perform IO on NKV
 *  IN     key – Key for which key value pair information will be stored
 *  IN     opt - nkv_store_option structure for specifying store option
 *  IN/OUT value – nkv_value structure containing the value for the key. Needs to be pre-allocated by user.
 *  
 */


nkv_result nkv_retrieve_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_retrieve_option* opt, nkv_value* value);

/*! Deletes a KV Pair to the container
 *  
 *  This API deletes a KV pair to the container in sync way
 *  IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN     ioctx - nkv_io_context buffer required to perform IO on NKV
 *  IN     key – Key for which key value pair information will be stored
 *  
 */


nkv_result nkv_delete_kvp (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key);


/*! Store a KV Pair to the container asynchronously
 *  
 *  This API stores a KV pair to the container in async way
 *  IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN     ioctx - nkv_io_context buffer required to perform IO on NKV
 *  IN     key – Key for which key value pair information will be stored
 *  IN     opt - nkv_store_option structure for specifying store option
 *  IN     value – nkv_value structure containing the value for the key. Needs to be pre-allocated by user.
 *  IN/OUT post_fn - nkv_postprocess_function data structure is used to return the result via call back function 
 *  
 */

nkv_result nkv_store_kvp_async (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_store_option* opt,
                                nkv_value* value, nkv_postprocess_function* post_fn);


/*! Retrieve a KV Pair from the container asynchronously
 *  
 *  This API retrieves a KV pair to the container in async way
 *  IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN     ioctx - nkv_io_context buffer required to perform IO on NKV
 *  IN     key – Key for which key value pair information will be stored
 *  IN     opt - nkv_retrieve_option structure for specifying store option
 *  IN     value – nkv_value structure containing the value for the key. Needs to be pre-allocated by user.
 *  IN/OUT post_fn - nkv_postprocess_function data structure is used to return the result via call back function
 *  
 */

nkv_result nkv_retrieve_kvp_async (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, const nkv_retrieve_option* opt,
                                nkv_value* value, nkv_postprocess_function* post_fn);


/*! Deletes a KV Pair to the container asynchronously
 *  
 *  This API deletes a KV pair to the container in async way
 *  IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN     ioctx - nkv_io_context buffer required to perform IO on NKV
 *  IN     key – Key for which key value pair information will be stored
 *  IN/OUT post_fn - nkv_postprocess_function data structure is used to return the result via call back function
 * 
 */


nkv_result nkv_delete_kvp_async (uint64_t nkv_handle, nkv_io_context* ioctx, const nkv_key* key, nkv_postprocess_function* post_fn);


/*! List the keys synchronously, returns NKV_ITER_MORE_KEYS if there are more keys to iterate and NKV_SUCCESS on complete.
 *  Application should call this API repeatedly with the iter_context it returned in previously till it gets a NKV_SUCCESS return value
 *  
 *  This API list all the keys for an application or optionally for a bucket
 *  IN     nkv_handle – A positive unique id for combination of nkv and the application(not instance). It is returned during nkv_open call
 *  IN     ioctx - nkv_io_context buffer required to perform IO on NKV, container hash is required, path hash is optional
 *  IN     bucket_name – Filtered by bucket name if supplied
 *  IN     prefix – Filtered by prefix name if supplied
 *  IN     delimiter – key delimiter in case of hierarchical keys if supplied
 *  IN     start_after - First key will be lexicographically after this key in case of sorted listing
 *  IN/OUT max_keys - maximum number of keys this api can pass at one shot. As out param It will send out the number of keys 
 *                    it populated in the ‘keys’ array  
 *  IN/OUT keys - Application will allocate max_keys number of empty nkv_key structures, 
 *                Library will populate less than or equal to max_keys number of keys. In buffer should be always 0 initialized.  
 *  IN/OUT iter_context - Very first time App will pass NULL, library will allocate and populate a context if needed,
 *                        For subsequent calls App should pass the same context to get the next set of keys. Once all keys are passed,
 *                        Library will deallocate this context. 
 *
 *  Returns - NKV_SUCCESS if iterator operation is successful , otherwise returns NKV_ITER_MORE_KEYS or errors. User should loop
 *            if it gets NKV_ITER_MORE_KEYS as response. 
 */


nkv_result nkv_indexing_list_keys (uint64_t nkv_handle, nkv_io_context* ioctx, const char* bucket_name, const char* prefix, 
                                   const char* delimiter, const char* start_after, uint32_t* max_keys, nkv_key* keys, void** iter_context );


#ifdef __cplusplus
} // extern "C"
#endif

#endif
