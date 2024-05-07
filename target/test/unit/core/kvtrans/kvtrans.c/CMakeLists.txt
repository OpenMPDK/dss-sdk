#
#   The Clear BSD License
#
#   Copyright (c) 2023 Samsung Electronics Co., Ltd.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted (subject to the limitations in the
#   disclaimer below) provided that the following conditions are met:
#
#   	* Redistributions of source code must retain the above copyright
#   	  notice, this list of conditions and the following disclaimer.
#   	* Redistributions in binary form must reproduce the above copyright
#   	  notice, this list of conditions and the following disclaimer in
#   	  the documentation and/or other materials provided with the distribution.
#   	* Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   	  contributors may be used to endorse or promote products derived from
#   	  this software without specific prior written permission.
#
#   NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
#   BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
#   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
#   BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
#   FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
#   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
#   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
#   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
#   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

add_definitions(-DDSS_BUILD_CUNIT_DISABLE_MARK_DIRTY)
add_executable(dss_kvtrans_ut ${CMAKE_SOURCE_DIR}/core/kvtrans/kvtrans.c
                          ${CMAKE_SOURCE_DIR}/core/kvtrans/kvtrans_hash.c
                          ${CMAKE_SOURCE_DIR}/core/kvtrans/kvtrans_mem_backend.c
                          ${CMAKE_SOURCE_DIR}/core/kvtrans/kvtrans_utils.c
                          ${CMAKE_SOURCE_DIR}/utils/hash/xxhash.c
                          ${CMAKE_SOURCE_DIR}/utils/hash/sha256.c
                          ${CMAKE_SOURCE_DIR}/utils/hash/spooky.c
                          ${CMAKE_SOURCE_DIR}/core/block_allocator/dss_block_allocator.c
                          ${CMAKE_SOURCE_DIR}/utils/dss_item_cache.c
                          ${CMAKE_SOURCE_DIR}/utils/dss_mallocator.c
                          ${CMAKE_SOURCE_DIR}/core/io_task/dss_io_task.c
                          ${CMAKE_SOURCE_DIR}/core/block_allocator/dss_simbmap_allocator.c
                          ${CMAKE_SOURCE_DIR}/core/block_allocator/bitmap_allocator/bitmap_impl.cc
                          ${CMAKE_SOURCE_DIR}/core/block_allocator/seek_optimization_impl.cc
                          ${CMAKE_SOURCE_DIR}/core/block_allocator/block_allocator_interface.cc
                          ${CMAKE_SOURCE_DIR}/core/block_allocator/io_task_orderer_impl.cc
                          ${CMAKE_SOURCE_DIR}/core/block_allocator/block_allocator_impl.cc
                          ${CMAKE_SOURCE_DIR}/utils/keygen.cc
                          ${CMAKE_SOURCE_DIR}/utils/crc32.cc
                          ${CMAKE_SOURCE_DIR}/utils/dss_keygen.c
                          ${CMAKE_SOURCE_DIR}/utils/adv_random.h
                          kvtrans_ut.c)

include_directories(${CMAKE_SOURCE_DIR}/core/kvtrans)
include_directories(${CMAKE_SOURCE_DIR}/core/block_allocator)
include_directories(${CMAKE_SOURCE_DIR}/include/apis)
include_directories(${CMAKE_SOURCE_DIR}/utils)

target_include_directories(dss_kvtrans_ut PRIVATE ${CMAKE_SOURCE_DIR}/core/block_allocator
                                              ${CMAKE_SOURCE_DIR}/core/kvtrans
                                              ${CMAKE_SOURCE_DIR}/utils
                                              ${CMAKE_SOURCE_DIR}/core/io_task
                                              ${CMAKE_SOURCE_DIR}/core/block_allocator/bitmap_allocator
                                              ${CMAKE_SOURCE_DIR}/core/block_allocator/utils
                                              ${CMAKE_SOURCE_DIR}/include/apis)

# include_directories(${CMAKE_SOURCE_DIR}/oss/spdk_tcp/dpdk/config)
target_link_libraries(dss_kvtrans_ut -L${CMAKE_BINARY_DIR} -ljudy_hashmap ${UNIT_LIBS} -lJudy -lm )
target_compile_options(dss_kvtrans_ut PRIVATE -Wall -g -std=gnu++11)
add_dependencies(dss_kvtrans_ut judy_hashmap)
add_dependencies(dss_kvtrans_ut judyL)
