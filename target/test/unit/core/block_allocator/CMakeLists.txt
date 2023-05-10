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

# Tests for simple bitmap implementation with cunit
add_executable(dss_simbmap_allocator_ut ${CMAKE_SOURCE_DIR}/core/block_allocator/dss_block_allocator.c
                                        ${CMAKE_SOURCE_DIR}/core/block_allocator/dss_simbmap_allocator.c
                                        dss_simbmap_allocator_ut.c)


target_include_directories(dss_simbmap_allocator_ut PRIVATE ${CMAKE_SOURCE_DIR}/core/block_allocator
                                                            ${CMAKE_SOURCE_DIR}/include/apis)


target_link_libraries(dss_simbmap_allocator_ut ${UNIT_LIBS} -lm)

# Tests for bitmap implementation with cppunit
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_COMPILER "g++")

add_executable(test_bitmap_impl ${CMAKE_SOURCE_DIR}/core/block_allocator/bitmap_allocator/bitmap_impl.cc
                                ${CMAKE_SOURCE_DIR}/core/block_allocator/seek_optimization_impl.cc
                                test_bitmap_impl.cc)

target_include_directories(test_bitmap_impl PRIVATE ${CMAKE_SOURCE_DIR}/core/block_allocator
                                                    ${CMAKE_SOURCE_DIR}/core/block_allocator/bitmap_allocator
                                                    ${CMAKE_SOURCE_DIR}/core/block_allocator/utils
                                                    ${CMAKE_SOURCE_DIR}/include/apis)

target_link_libraries(test_bitmap_impl -L${CMAKE_BINARY_DIR} -ljudy_hashmap -ljudyL ${UNIT_LIBS})
target_compile_options(test_bitmap_impl PRIVATE -Wall -g -std=gnu++11)
add_dependencies(test_bitmap_impl judy_hashmap)

# Tests for Judy hashmap implementation with cppunit
add_executable(test_judy_hashmap_impl test_judy_hashmap_impl.cc)

target_include_directories(test_judy_hashmap_impl PRIVATE ${CMAKE_SOURCE_DIR}/core/block_allocator/utils)

target_link_libraries(test_judy_hashmap_impl -L${CMAKE_BINARY_DIR} -ljudy_hashmap -ljudyL ${UNIT_LIBS})
target_compile_options(test_judy_hashmap_impl PRIVATE -DJUDYERROR_NOTEST -g -Wall -std=gnu++11)
add_dependencies(test_judy_hashmap_impl judy_hashmap)

# Tests for bitmap implementation with cppunit
add_executable(test_seek_optimization_impl ${CMAKE_SOURCE_DIR}/core/block_allocator/seek_optimization_impl.cc
                                test_seek_optimization_impl.cc)

target_include_directories(test_seek_optimization_impl PRIVATE ${CMAKE_SOURCE_DIR}/core/block_allocator
                                                    ${CMAKE_SOURCE_DIR}/core/block_allocator/utils
                                                    ${CMAKE_SOURCE_DIR}/include/apis)

target_link_libraries(test_seek_optimization_impl -L${CMAKE_BINARY_DIR} -ljudy_hashmap -ljudyL ${UNIT_LIBS})
target_compile_options(test_seek_optimization_impl PRIVATE -Wall -g -std=gnu++11)
add_dependencies(test_seek_optimization_impl judy_hashmap)
