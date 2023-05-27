#
#   The Clear BSD License
#
#   Copyright (c) 2022 Samsung Electronics Co., Ltd.
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
cmake_minimum_required(VERSION 2.6)
project(nkv-target)
enable_language(C)
enable_language(CXX)

option(WITH_COVERAGE "Generate binaries with coverage enabled." OFF)
if(WITH_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(SPDK_ENABLE_COVERAGE --enable-coverage)
endif(WITH_COVERAGE)

option(WITH_ROCKSDB_KV "Support KV translation with rocksdb engine" OFF)

#--Judy--

set(JUDY_INC_DIRS -I${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyL -I${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/ -I${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon)
set(JUDY_COMPILE_FLAGS -DJUDYL -DJUDYSL -DJU_64BIT -D__LIN__ -D__USER__ -DX86_64__)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/judyL.lnk
	COMMAND mkdir -p ${CMAKE_BINARY_DIR}/JudyL
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyByCount.c          ${CMAKE_BINARY_DIR}/JudyL/JudyLByCount.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyCascade.c          ${CMAKE_BINARY_DIR}/JudyL/JudyLCascade.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyCount.c            ${CMAKE_BINARY_DIR}/JudyL/JudyLCount.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyCreateBranch.c     ${CMAKE_BINARY_DIR}/JudyL/JudyLCreateBranch.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyDecascade.c        ${CMAKE_BINARY_DIR}/JudyL/JudyLDecascade.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyDel.c              ${CMAKE_BINARY_DIR}/JudyL/JudyLDel.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyFirst.c            ${CMAKE_BINARY_DIR}/JudyL/JudyLFirst.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyFreeArray.c        ${CMAKE_BINARY_DIR}/JudyL/JudyLFreeArray.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyGet.c              ${CMAKE_BINARY_DIR}/JudyL/JudyLGet.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyGet.c              ${CMAKE_BINARY_DIR}/JudyL/j__udyLGet.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyInsArray.c         ${CMAKE_BINARY_DIR}/JudyL/JudyLInsArray.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyIns.c              ${CMAKE_BINARY_DIR}/JudyL/JudyLIns.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyInsertBranch.c     ${CMAKE_BINARY_DIR}/JudyL/JudyLInsertBranch.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyMallocIF.c         ${CMAKE_BINARY_DIR}/JudyL/JudyLMallocIF.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyMemActive.c        ${CMAKE_BINARY_DIR}/JudyL/JudyLMemActive.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyMemUsed.c          ${CMAKE_BINARY_DIR}/JudyL/JudyLMemUsed.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyPrevNext.c         ${CMAKE_BINARY_DIR}/JudyL/JudyLNext.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyPrevNext.c         ${CMAKE_BINARY_DIR}/JudyL/JudyLPrev.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyPrevNextEmpty.c    ${CMAKE_BINARY_DIR}/JudyL/JudyLNextEmpty.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyPrevNextEmpty.c    ${CMAKE_BINARY_DIR}/JudyL/JudyLPrevEmpty.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyTables.c           ${CMAKE_BINARY_DIR}/JudyL/JudyLTablesGen.c
	COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudySL/JudySL.c                   ${CMAKE_BINARY_DIR}/JudyL/JudyLSL.c
	COMMAND touch ${CMAKE_BINARY_DIR}/judyL.lnk
)

add_custom_target(cr_judyL_links
	DEPENDS ${CMAKE_BINARY_DIR}/judyL.lnk
)

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/judyL.gtables
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/JudyL
	DEPENDS ${CMAKE_BINARY_DIR}/judyL.lnk
	COMMAND gcc -fPIC  ${JUDY_INC_DIRS} ${JUDY_COMPILE_FLAGS} ${CMAKE_BINARY_DIR}/JudyL/JudyLTablesGen.c -o ${CMAKE_BINARY_DIR}/JudyL/JudyLTablesGen
	COMMAND ./JudyLTablesGen
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} ${CMAKE_BINARY_DIR}/JudyL/JudyLTables.c
	COMMAND touch ${CMAKE_BINARY_DIR}/judyL.gtables
)

add_custom_target(cr_judyL_gtables
	DEPENDS ${CMAKE_BINARY_DIR}/judyL.gtables
)

#--- Compile the main line JudyL modules
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/judyL.compile
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/JudyL
	DEPENDS ${CMAKE_BINARY_DIR}/judyL.gtables
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/oss/libjudy/src/JudyCommon/JudyMalloc.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLTables.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLGet.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} -DJUDYGETINLINE j__udyLGet.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLIns.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLIns.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLDel.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLFirst.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} -DJUDYNEXT JudyLNext.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} -DJUDYPREV JudyLPrev.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} -DJUDYNEXT JudyLNextEmpty.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} -DJUDYPREV JudyLPrevEmpty.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLCount.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLByCount.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLFreeArray.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLMemUsed.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLMemActive.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLCascade.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLDecascade.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLCreateBranch.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLInsertBranch.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLMallocIF.c
	COMMAND gcc -fPIC ${JUDY_INC_DIRS} -c ${JUDY_COMPILE_FLAGS} JudyLSL.c
	COMMAND touch ${CMAKE_BINARY_DIR}/judyL.compile
)

add_custom_target(cr_judyL_compile
	DEPENDS ${CMAKE_BINARY_DIR}/judyL.compile
)

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/libjudyL.so
	DEPENDS cr_judyL_compile
	COMMAND ld -shared -o ${CMAKE_BINARY_DIR}/libjudyL.so ${CMAKE_BINARY_DIR}/JudyL/*.o
)

add_custom_target(judyL
	DEPENDS ${CMAKE_BINARY_DIR}/libjudyL.so
)

add_custom_command(
        TARGET judyL POST_BUILD
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/lib
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/libjudyL.so ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/lib/
)

#--Judy--

# Prohibit a common type of an in-source build.
# Note that building in a subdirectory in the source tree is still allowed
# as it can be convenient.
string (COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" in_source)
if (in_source)
    message (FATAL_ERROR
"It is not allowed to build the project in its source directory. "
"Please delete CMakeCache.txt and CMakeFiles from ${CMAKE_SOURCE_DIR} "
"if they are there and then use some other directory to build nkv-target."
)
endif ()

add_subdirectory(mk/filelists)
#add_subdirectory(common)

set(CMAKE_C_FLAGS_DEBUG "-DDEBUG -O0 -fno-omit-frame-pointer")

add_custom_target(
  GenConfig ALL
  DEPENDS dfly_config.h
)

set(DFLY_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/include/dfly_config.h)
add_custom_command ( OUTPUT dfly_config.h
                     COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/include
                     COMMAND echo "Dragonfly Config dir: `pwd`"
                     COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/genconfig.py > ${DFLY_CONFIG_FILE}
                     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/spdk/include)

# DFLY flags
set(DFLY_LIBS ${DFLY_LIBS} -lpthread -Wl,--no-as-needed -fPIC -lrt -L. -ltbb -lm -march=native)
set(DFLY_CFLAGS -std=gnu++11 -MMD -MP -D_FILE_OFFSET_BITS=64 -fPIC -fpermissive -Wwrite-strings -march=native -include include/dfly_config.h)

#
# KVEMUL library build rules (shared)
#
SET(KVEMUL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/kvemulator/src/kvemul.cpp
)
SET(KVEMUL_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/kvemulator/src/kvemul.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inc/df_kvemul.h
)
#
# Key Distribution build rules (sources)
#
SET(KD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/kd/src/murmurhash3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/kd/src/dfly_sh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/kd/src/dfly_rh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/kd/src/dfly_kd.cpp
)

SET(KD_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inc/murmurhash3.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inc/df_kd.h
)

SET(KV_CMD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/nvme_samsung_kv_cmd.c
)

# Block allocator
SET(BLK_ALLOC_SOURCES
    ${CMAKE_SOURCE_DIR}/core/block_allocator/dss_block_allocator.c
    ${CMAKE_SOURCE_DIR}/core/block_allocator/dss_simbmap_allocator.c
    ${CMAKE_SOURCE_DIR}/core/block_allocator/bitmap_allocator/bitmap_impl.cc
    ${CMAKE_SOURCE_DIR}/core/block_allocator/seek_optimization_impl.cc
    ${CMAKE_SOURCE_DIR}/core/block_allocator/utils/judy_hashmap_impl.cc
)

SET(BLK_ALLOC_HEADERS
    ${CMAKE_SOURCE_DIR}/include/apis/dss_block_allocator_apis.h
    ${CMAKE_SOURCE_DIR}/core/block_allocator/dss_block_allocator.h
    ${CMAKE_SOURCE_DIR}/core/block_allocator/block_allocator.h
    ${CMAKE_SOURCE_DIR}/core/block_allocator/allocator_type.h
    ${CMAKE_SOURCE_DIR}/core/block_allocator/bitmap_allocator/bitmap_impl.h
    ${CMAKE_SOURCE_DIR}/core/block_allocator/utils/judy_hashmap.h
)

#Io task Module
SET(IO_TASK_SOURCES
    ${CMAKE_SOURCE_DIR}/core/io_task/dss_io_bdev.c
    ${CMAKE_SOURCE_DIR}/core/io_task/dss_io_task.c
)

SET(IO_TASK_HEADERS
    ${CMAKE_SOURCE_DIR}/include/apis/dss_io_task_apis.h
    ${CMAKE_SOURCE_DIR}/core/io_task/dss_io_task.h
)

# KV Translator
SET(KVTRANS_SOURCES
    ${CMAKE_SOURCE_DIR}/core/kvtrans/kvtrans.c
    ${CMAKE_SOURCE_DIR}/core/kvtrans/kvtrans_hash.c
)

SET(KVTRANS_HEADERS
    ${CMAKE_SOURCE_DIR}/core/kvtrans/kvtrans.h
    ${CMAKE_SOURCE_DIR}/core/kvtrans/kvtrans_hash.h
)

#
# QOS library build rules (shared)
#
SET(QOS_SOURCES
#    ${CMAKE_CURRENT_SOURCE_DIR}/core/qos/src/qos.cpp
)
SET(QOS_HEADERS
#    ${CMAKE_CURRENT_SOURCE_DIR}/core/qos/src/qos.h
)

#
# WAL library build rules
#
SET(ITER_SOURCES
    ${CMAKE_SOURCE_DIR}/core/iter/src/iter_op.cpp
    ${CMAKE_SOURCE_DIR}/core/iter/src/list_op.cpp
    ${CMAKE_SOURCE_DIR}/core/iter/src/list_module.cpp
)
SET(ITER_HEADERS
    ${CMAKE_SOURCE_DIR}/core/inc/df_iter.h
    ${CMAKE_SOURCE_DIR}/core/iter/inc/list_lib.h
)


SET(WAL_SOURCES
    ${CMAKE_SOURCE_DIR}/core/wal/src/wal_hash.cpp
    ${CMAKE_SOURCE_DIR}/core/wal/src/wal_map.cpp
    ${CMAKE_SOURCE_DIR}/core/wal/src/wal_zone_buffer.cpp
    ${CMAKE_SOURCE_DIR}/core/wal/src/wal_log.cpp
    ${CMAKE_SOURCE_DIR}/core/wal/src/wal_module.cpp
)
SET(WAL_HEADERS
    ${CMAKE_SOURCE_DIR}/core/wal/inc/wal_lib.h
    ${CMAKE_SOURCE_DIR}/core/wal/inc/wal_map.h
    ${CMAKE_SOURCE_DIR}/core/wal/inc/wal_zone_buffer.h
    ${CMAKE_SOURCE_DIR}/core/inc/module_hash.h
)


# FUSE library build rules
#
SET(FUSE_SOURCES
    ${CMAKE_SOURCE_DIR}/core/fuse/src/fuse_lib.cpp
    ${CMAKE_SOURCE_DIR}/core/fuse/src/fuse_map.cpp
    ${CMAKE_SOURCE_DIR}/core/fuse/src/fuse_module.cpp
    ${CMAKE_SOURCE_DIR}/core/wal/src/wal_hash.cpp
)
SET(FUSE_HEADERS
    ${CMAKE_SOURCE_DIR}/core/fuse/inc/fuse_lib.h
    ${CMAKE_SOURCE_DIR}/core/fuse/inc/fuse_map.h
    ${CMAKE_SOURCE_DIR}/core/fuse/inc/fuse_module.h
    ${CMAKE_SOURCE_DIR}/core/inc/module_hash.h
)

SET(RDD_SOURCES
    ${CMAKE_SOURCE_DIR}/utils/dss_ref.c
    ${CMAKE_SOURCE_DIR}/core/rdd/rdd.c
)
SET(RDD_HEADERS
    ${CMAKE_SOURCE_DIR}/core/rdd/rdd.h
    ${CMAKE_SOURCE_DIR}/core/inc/rdd_api.h
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/oss/dssd DESTINATION ${CMAKE_BINARY_DIR})
ADD_CUSTOM_TARGET(dssd
    COMMENT "Building open source DSSD Library"
    COMMAND make -C dssd/lib
    COMMAND make -C dssd/cmd/ustat
    COMMAND export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CMAKE_BINARY_DIR}/dssd/lib
    COMMAND export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CMAKE_BINARY_DIR}/dssd/lib/libustat
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/include
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/lib
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/dssd/lib/libdssd.a ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/lib/
)

#
# FRAMEWORK library build rules (shared)
#
SET(FW_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_latency.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_spdk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_stats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_subsystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_conf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_namespace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_request.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_numa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_qpair.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_ctrl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_poller.c
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_dev.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_bdev.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_kv_pool_dev.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_mm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_module.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dss_tpool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_io_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_lock_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_req_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_counters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dfly_blk_stats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/framework/src/dragonfly.cpp
)
SET(FW_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inc/uthash.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inc/dragonfly.h
    ${CMAKE_CURRENT_SOURCE_DIR}/oss/dssd/lib/libustat/ustat.h
)

#
# Util sources
#
SET(UTIL_SOURCES
   ${CMAKE_SOURCE_DIR}/utils/dss_item_cache.c
   ${CMAKE_SOURCE_DIR}/utils/dss_mallocator.c
)

include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_BINARY_DIR}/oss/spdk_tcp/include)
include_directories (${CMAKE_BINARY_DIR}/oss/spdk_tcp/lib/nvmf/)
include_directories (${CMAKE_BINARY_DIR}/oss/spdk_tcp/lib/nvme/)
include_directories (${CMAKE_BINARY_DIR}/oss/spdk_tcp/app/nvmf_tgt)
include_directories (${CMAKE_SOURCE_DIR}/stats)
include_directories (${CMAKE_SOURCE_DIR}/core/inc)
include_directories (${CMAKE_SOURCE_DIR}/core/wal/inc)
include_directories (${CMAKE_SOURCE_DIR}/core/iter/inc)
include_directories (${CMAKE_SOURCE_DIR}/core/fuse/inc)
include_directories (${CMAKE_SOURCE_DIR}/oss/dssd/include/)
include_directories (${CMAKE_SOURCE_DIR}/oss/dssd/lib/libustat/)

if(WITH_ROCKSDB_KV)
include_directories (${CMAKE_SOURCE_DIR}/oss/rocksdb/include)
endif(WITH_ROCKSDB_KV)

include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_SOURCE_DIR}/core/block_allocator/)
include_directories (${CMAKE_SOURCE_DIR}/core/block_allocator/utils)
include_directories (${CMAKE_SOURCE_DIR}/utils)

option(WITH_KV_STATS "Enable KV Stats library" OFF)
option(WITH_KV_LOG   "Enable KV Logging" OFF)
if(WITH_KV_STATS)

SET(KVV_LIBS    ${KVV_LIBS}    -lkv_stats)
SET(KVSH_LIBS   ${KVSH_LIBS}   -lkv_stats)
SET(SIRIUS_LIBS ${SIRIUS_LIBS} -lkv_stats -L.)

add_definitions(-DCONFIG_KVV_STATS=y)
add_definitions(-DCONFIG_KV_STATS=y)

if(WITH_KV_LOG)
add_definitions(-DCONFIG_KVV_LOG=y)
endif(WITH_KV_LOG)

message("KV Stats is enabled")

# Make sure only either of the build types are enabled.
if(BUILD_MODE_DEBUG)
unset(BUILD_MODE_RELEASE)
endif()

if(BUILD_MODE_RELEASE)
unset(BUILD_MODE_DEBUG)
endif()

# KVV STATS flags
set(KV_STATS_LIBS -lpthread -lconfig -Wl,--no-as-needed -fPIC -lrt -L. -march=native)
set(KV_STATS_CFLAGS  -fPIC -O3 -Wall -Werror -march=native)

SET(KV_STATS_SOURCES
    ${CMAKE_SOURCE_DIR}/stats/kv_stats_lib.c
)

add_library(kv_stats SHARED ${KV_STATS_SOURCES})
target_link_libraries (kv_stats ${KV_STATS_LIBS})
target_compile_options(kv_stats PRIVATE ${KV_STATS_CFLAGS})

endif(WITH_KV_STATS)

option(WITH_DSS_JUDY_LISTING "Build with Judy for listing" ON)

#SPDK tcp
set(OSS_LIBS ${OSS_LIBS} -lpthread -Wl,--no-as-needed -fPIC -lrt -L. -ltbb -lm -march=native)
if(BUILD_MODE_RELEASE)
set(OSS_CFLAGS -std=gnu++11 -MMD -MP -D_FILE_OFFSET_BITS=64 -fPIC -fpermissive -Wwrite-strings -march=native)
else()
set(OSS_CFLAGS -g -std=gnu++11 -MMD -MP -D_FILE_OFFSET_BITS=64 -fPIC -fpermissive -Wwrite-strings -march=native)
endif()

SET(DSS_FILES
    ${FW_SOURCES}
    ${FW_HEADERS}
    ${RDD_SOURCES}
    ${RDD_HEADERS}
    ${ITER_SOURCES}
    ${ITER_HEADERS}
    ${WAL_SOURCES}
    ${WAL_HEADERS}
    ${FUSE_SOURCES}
    ${FUSE_HEADERS}
    ${QOS_SOURCES}
    ${QOS_HEADERS}
    ${KD_SOURCES}
    ${KD_HEADERS}
    ${BLK_ALLOC_SOURCES}
    ${BLK_ALLOC_HEADERS}
    ${IO_TASK_SOURCES}
    ${IO_TASK_HEADERS}
    ${UTIL_SOURCES}
    ${KVTRANS_SOURCES}
    ${KVTRANS_HEADERS}
)

add_library(oss STATIC ${DSS_FILES})
target_link_libraries (oss ${OSS_LIBS})
target_compile_options(oss PRIVATE ${OSS_CFLAGS})
add_dependencies(oss spdk_tcp_configure)
add_dependencies(oss GenConfig)

if(WITH_ROCKSDB_KV)
target_compile_definitions(oss PRIVATE -DDSS_ENABLE_ROCKSDB_KV)
endif(WITH_ROCKSDB_KV)

add_custom_command(
        TARGET oss POST_BUILD
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/lib
        COMMAND cp liboss.a ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/lib
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/build/lib/
        COMMAND export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CMAKE_BINARY_DIR}/nkv-target/lib
)

if(BUILD_MODE_RELEASE)
ADD_CUSTOM_TARGET(spdk_tcp_configure
  COMMENT "Configuring SPDK with NVMEoTCP Support"
  WORKING_DIRECTORY oss/spdk_tcp
  COMMAND ./configure ${SPDK_ENABLE_COVERAGE} --disable-tests --with-rdma
  COMMAND make include/spdk/config.h
)
else()
ADD_CUSTOM_TARGET(spdk_tcp_configure
  COMMENT "Configuring SPDK with NVMEoTCP Support"
  WORKING_DIRECTORY oss/spdk_tcp
  COMMAND ./configure ${SPDK_ENABLE_COVERAGE} --disable-tests --enable-debug --with-rdma
  COMMAND make include/spdk/config.h
)
endif()

if(WITH_ROCKSDB_KV)
set(SPDK_CONFIG_ENABLE_ROCKSDB_KV "CONFIG_DSS_ROCKSDB_KV=y")
endif(WITH_ROCKSDB_KV)

if(WITH_DSS_JUDY_LISTING)

ADD_CUSTOM_TARGET(spdk_tcp
  COMMENT "Building SPDK with NVMEoTCP Support"
  WORKING_DIRECTORY oss/spdk_tcp
  COMMAND make V=2 -j16 ${SPDK_CONFIG_ENABLE_ROCKSDB_KV}
  COMMAND make -C test/blobfs/mkfs ${SPDK_CONFIG_ENABLE_ROCKSDB_KV}
  DEPENDS dssd
  DEPENDS spdk_tcp_configure
  DEPENDS oss
  DEPENDS dss_lat
  DEPENDS dss_hsl
  DEPENDS rocksdb_lib
)

else(WITH_DSS_JUDY_LISTING)

target_compile_definitions(oss PRIVATE -DDSS_OPEN_SOURCE_RELEASE)
set(DSS_RDB_OSS_EXTRACXXFLAGS EXTRA_CXXFLAGS=-DDSS_OPEN_SOURCE_RELEASE)

ADD_CUSTOM_TARGET(spdk_tcp
  COMMENT "Building SPDK with NVMEoTCP Support"
  WORKING_DIRECTORY oss/spdk_tcp
  COMMAND make V=2 -j16 CONFIG_DSS_OSS=y ${SPDK_CONFIG_ENABLE_ROCKSDB_KV}
  COMMAND make -C test/blobfs/mkfs CONFIG_DSS_OSS=y ${SPDK_CONFIG_ENABLE_ROCKSDB_KV}
  DEPENDS dssd
  DEPENDS spdk_tcp_configure
  DEPENDS oss
  DEPENDS rocksdb_lib
)

endif(WITH_DSS_JUDY_LISTING)

add_subdirectory(test)

ADD_CUSTOM_TARGET(test_rdd
    COMMENT "Building test rdma data direct"
    WORKING_DIRECTORY test/test_rdd
    COMMAND make
    DEPENDS spdk_tcp
)

execute_process(COMMAND git describe --abbrev=4 --always --tags OUTPUT_VARIABLE DSS_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
add_custom_command(
        TARGET spdk_tcp POST_BUILD
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/bin
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/scripts
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/include
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/include/spdk
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/build/bin/nvmf_tgt ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/bin/
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/test/blobfs/mkfs/mkfs ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/bin/mkfs_blobfs
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/scripts/setup.sh ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/scripts/
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/scripts/common.sh ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/scripts/
        COMMAND cp -r ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/scripts/rpc/ ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/scripts/
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/scripts/rpc.py ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/scripts/
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/scripts/dss_rpc.py ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/scripts/
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/oss/spdk_tcp/include/spdk/pci_ids.h ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/include/spdk/
        COMMAND cp ${CMAKE_SOURCE_DIR}/dss_target.py ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/bin/
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/dssd/cmd/ustat/ustat ${CMAKE_CURRENT_BINARY_DIR}/nkv-target/bin/
        COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND tar cvzf nkv-target-${DSS_VER}.tgz nkv-target
)

if(WITH_ROCKSDB_KV)
ADD_CUSTOM_TARGET(rocksdb
  COMMENT "Building rocksdb async"
  WORKING_DIRECTORY oss/rocksdb
  COMMAND DISABLE_WARNING_AS_ERROR=1 make -j16 db_bench SPDK_DIR=../spdk_tcp/
  DEPENDS spdk_tcp
)
endif(WITH_ROCKSDB_KV)

if(WITH_DSS_JUDY_LISTING)

add_library(dss_lat STATIC	utils/dss_count_latency.c)
target_link_libraries(dss_lat -ljudyL)
target_compile_options(dss_lat PRIVATE -g -I${CMAKE_SOURCE_DIR}/oss/libjudy/src -I${CMAKE_SOURCE_DIR}/include/)
add_dependencies(dss_lat judyL)
add_custom_command(
        TARGET dss_lat PRE_BUILD
        COMMENT ${CMAKE_SOURCE_DIR}
)

ADD_CUSTOM_TARGET(test_latency_counter
    COMMENT "Building test latency counter"
    WORKING_DIRECTORY test/test_latency_counter
    COMMAND make
    DEPENDS dss_lat
)

add_library(dss_hsl STATIC	utils/dss_list_tree.c
							utils/keygen.cc
							utils/crc32.cc
							utils/dss_keygen.c)
target_link_libraries(dss_hsl -ljudyL)

if(WITH_ROCKSDB_KV)
target_compile_options(dss_hsl PRIVATE -g -I${CMAKE_SOURCE_DIR}/oss/rocksdb/include -I${CMAKE_SOURCE_DIR}/oss/libjudy/src -I${CMAKE_SOURCE_DIR}/include/)
else()
target_compile_options(dss_hsl PRIVATE -g -I${CMAKE_SOURCE_DIR}/core/inc -I${CMAKE_SOURCE_DIR}/oss/libjudy/src -I${CMAKE_SOURCE_DIR}/include/)
endif()

add_dependencies(dss_hsl judyL)

add_custom_command(
        TARGET dss_hsl PRE_BUILD
        COMMENT ${CMAKE_SOURCE_DIR}
)

ADD_CUSTOM_TARGET(trigger_build_sh
	COMMENT "Building trigger build.sh script"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMAND ./build.sh
)

ADD_CUSTOM_TARGET(test_hsl
    COMMENT "Building test hsl"
    WORKING_DIRECTORY test/test_hsl
    COMMAND make
    DEPENDS dss_hsl
)

endif(WITH_DSS_JUDY_LISTING)

if(WITH_ROCKSDB_KV)
if(BUILD_MODE_DEBUG)
ADD_CUSTOM_TARGET(rocksdb_lib
  COMMENT "Building rocksdb async static library"
  WORKING_DIRECTORY oss/rocksdb
  COMMAND make clean SPDK_DIR=../spdk_tcp
  COMMAND DEBUG_MODE_BUILD=1 DISABLE_WARNING_AS_ERROR=1 make -j16 static_lib SPDK_DIR=../spdk_tcp/ -I../../core/iter/inc/ ${DSS_RDB_OSS_EXTRACXXFLAGS}
  DEPENDS spdk_tcp_configure
)
elseif(BUILD_MODE_RELEASE)
ADD_CUSTOM_TARGET(rocksdb_lib
  COMMENT "Building rocksdb async static library"
  WORKING_DIRECTORY oss/rocksdb
  COMMAND make clean SPDK_DIR=../spdk_tcp
  COMMAND RELEASE_MODE_BUILD=1 DISABLE_WARNING_AS_ERROR=1 make -j16 static_lib SPDK_DIR=../spdk_tcp/ -I../../core/iter/inc/ ${DSS_RDB_OSS_EXTRACXXFLAGS}
  DEPENDS spdk_tcp_configure
)
else()
ADD_CUSTOM_TARGET(rocksdb_lib
  COMMENT "Building rocksdb async static library"
  WORKING_DIRECTORY oss/rocksdb
  COMMAND make clean SPDK_DIR=../spdk_tcp
  COMMAND DISABLE_WARNING_AS_ERROR=1 make -j16 static_lib SPDK_DIR=../spdk_tcp/ -I../../core/iter/inc/ ${DSS_RDB_OSS_EXTRACXXFLAGS}
  DEPENDS spdk_tcp_configure
)
endif()
else(WITH_ROCKSDB_KV)
ADD_CUSTOM_TARGET(rocksdb_lib
  COMMENT "Not building rocksdb - Disabled"
  COMMAND echo "option WITH_ROCKSDB_KV not set"
)
endif(WITH_ROCKSDB_KV)

add_library(judy_hashmap STATIC core/block_allocator/utils/judy_hashmap_impl.cc)
add_definitions(-DJUDYERROR_NOTEST=y)
target_link_libraries(judy_hashmap -ljudyL)
target_compile_options(judy_hashmap PRIVATE -g -I${CMAKE_SOURCE_DIR}/oss/libjudy/src -I${CMAKE_SOURCE_DIR}/include/ -std=gnu++11)
add_dependencies(judy_hashmap judyL)

add_test(NAME dss_lock_service_ut COMMAND dss_lock_service_ut)
add_test(NAME dss_simbmap_allocator_ut COMMAND dss_simbmap_allocator_ut)
add_test(NAME test_bitmap_impl COMMAND test_bitmap_impl)
add_test(NAME dss_item_cache_ut COMMAND dss_item_cache_ut)
add_test(NAME dss_mallocator_ut COMMAND dss_mallocator_ut)
add_test(NAME dss_io_task_ut COMMAND dss_io_task_ut)
add_test(NAME test_judy_hashmap_impl COMMAND test_judy_hashmap_impl)
add_test(NAME test_seek_optimization_impl COMMAND test_seek_optimization_impl)
add_test(NAME test_block_allocator_ut COMMAND test_block_allocator_ut)
add_test(NAME dss_kvtrans_ut COMMAND dss_kvtrans_ut)

enable_testing()

