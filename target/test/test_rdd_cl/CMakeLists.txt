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

add_subdirectory(${CMAKE_SOURCE_DIR}/../utils/ ${CMAKE_BINARY_DIR}/utils/)

set(RDD_CL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/../utils/include)
set(RDD_CL_LIBS -L${CMAKE_BINARY_DIR}/utils/rdd_cl/ -lpthread -lrdmacm -libverbs -lrdd_cl)
set(RDD_CL_CFLAGS -g -march=native -D_GNU_SOURCE)
set(RDD_CL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rdd_client.c
)

add_executable(test_rdd_cl ${RDD_CL_SOURCES})
target_compile_options(test_rdd_cl PRIVATE ${RDD_CL_CFLAGS})
target_include_directories(test_rdd_cl PRIVATE ${RDD_CL_INCLUDE_DIR})
target_link_libraries(test_rdd_cl ${RDD_CL_LIBS})
add_dependencies(test_rdd_cl rdd_cl)

