cmake_minimum_required(VERSION 2.8.9)
project (libnkvapi C CXX)

add_custom_target(
  run ALL
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND ./generate_version.sh
)

option(NKV_WITH_EMU "Enable NKV with Emulator support" OFF)
option(NKV_WITH_KDD "Enable NKV with Kernel Driver support" OFF)
option(NKV_WITH_SPDK "Enable NKV with SPDK Driver support" OFF)
option(NKV_WITH_SAMSUNG_API "Enable NKV with SAMSUNG openmpdk api support" OFF)
option(NKV_WITH_REMOTE "Enable NKV with SAMSUNG openmpdk api support and REMOTE target support" OFF)

set(CMAKE_C_FLAGS "-MMD -MP -Wall -DLINUX -D_FILE_OFFSET_BITS=64 -fPIC  -march=native")
set(CMAKE_CXX_FLAGS "-O3 -g -std=c++11 -MMD -MP -Wall -DLINUX -D_FILE_OFFSET_BITS=64 -fPIC  -march=native")

#put all the libs and binaries in one place
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
set(Boost_INCLUDE_DIR /usr/include/boost/)
include_directories(${Boost_INCLUDE_DIR})

set(LOG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/logger/libsmglog/inc/ ${CMAKE_CURRENT_SOURCE_DIR}/src/logger/log4cpp/include/)
set(LOG_LIBRARY_DIR ${CMAKE_BINARY_DIR}/lib/libsmglog/)

include_directories(${LOG_INCLUDE_DIR})
link_directories(${LOG_LIBRARY_DIR})

#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/logger/ ${CMAKE_BINARY_DIR}/lib)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/logger/)
#Building zeromq
set( ZMQPP_LIBZMQ_CMAKE   true   CACHE BOOL "libzmq is build through cmake too" FORCE)
set( ZEROMQ_LIB_DIR       ${CMAKE_BINARY_DIR}/lib/      CACHE PATH "The library directory for libzmq" FORCE)
set( ZEROMQ_INCLUDE_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/src/zeromq/libzmq/include      CACHE PATH "The include directory for ZMQ" FORCE)

set( ZMQPP_BUILD_EXAMPLES false   CACHE BOOL "Build the ZMQPP examples" FORCE)
set( ZMQPP_BUILD_CLIENT   false   CACHE BOOL "Build the ZMQPP client" FORCE)
set( ZMQPP_BUILD_TESTS    false   CACHE BOOL "Build the ZMQPP tests" FORCE)

set(BUILD_TESTS OFF CACHE BOOL "Whether or not to build the tests" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/zeromq/libzmq/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/zeromq/zmqpp/)
add_dependencies(zmqpp libzmq)

# Opensource libraries
set(ZMQ_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/zeromq/zmqpp/src ${CMAKE_CURRENT_SOURCE_DIR}/src/zeromq/libzmq/include)
set(ZMQ_LIBRARY_DIR ${CMAKE_BINARY_DIR}/lib/)
include_directories(${ZMQ_INCLUDE_DIR})
link_directories(${ZMQ_LIBRARY_DIR})

set(USTAT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../target/oss/dssd/include/ ${CMAKE_CURRENT_SOURCE_DIR}/../target/oss/dssd/lib/libustat/ )
set(USTAT_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../target/oss/dssd/lib/libustat/)
include_directories(${USTAT_INCLUDE_DIR})
link_directories(${USTAT_LIBRARY_DIR})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../df_out/dssd/lib/libdssd.a DESTINATION ${CMAKE_BINARY_DIR}/lib)

if(NKV_WITH_SAMSUNG_API)
  set(SAMSUNG_API ON CACHE BOOL "Enable Samsung API functionality" FORCE)
  add_definitions(-DSAMSUNG_API)
  add_definitions(-DKVS_REMOTE)
endif()


if(NKV_WITH_REMOTE)
  set(SAMSUNG_API ON CACHE BOOL "Enable Samsung API functionality" FORCE)
  set(KVS_REMOTE ON CACHE BOOL "Enable KVS Remote API functionality" FORCE)
  add_definitions(-DSAMSUNG_API)
  add_definitions(-DNKV_REMOTE)
  add_definitions(-DKVS_REMOTE)
endif()

if(NKV_WITH_KDD)
  set(WITH_KDD ON CACHE BOOL "Enable KDD functionality" FORCE)
  #set(WITH_LOG ON CACHE BOOL "Enable KDD functionality" FORCE)

elseif(NKV_WITH_EMU)
  set(WITH_EMU ON CACHE BOOL "Enable EMUL functionality" FORCE)

elseif(NKV_WITH_SPDK)

else()
  message( FATAL_ERROR "Please specify device driver type for compilation." )

endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/core/ ${CMAKE_BINARY_DIR}/lib)
set(KVAPI_LIBS -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY} -lkvapi)
set(NKVAPI_LINK_LIBS -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY} -L${LOG_LIBRARY_DIR} -L${ZMQ_LIBRARY_DIR} -L${USTAT_LIBRARY_DIR} -pthread -lkvapi -lnkvapi -lsmglog -lzmq -lzmqpp)
# SOURCE CODE
SET(SOURCES_NKV_KDD
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nkv_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nkv_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/remote/native_fabric_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/remote/unified_fabric_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/remote/auto_discovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/remote/event_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nkv_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stats/nkv_stats.cpp
)

SET(HEADERS_NKV_KDD
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nkv_api.h
)

set(NKVAPI_CXXFLAGS "-std=c++11 -MMD -MP -Wall -D_FILE_OFFSET_BITS=64 -fPIC -fpermissive -march=native")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NKVAPI_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NKVAPI_CXXFLAGS}")
set(NKVAPI_LIBS ${NKVAPI_LIBS} -pthread -Wl,--no-as-needed -fPIC -lcurl -std=c++11 -lnuma -lrt -lboost_filesystem ${KVAPI_LIBS} -lsmglog -lzmq -lzmqpp -march=native  ${CMAKE_BINARY_DIR}/lib/libdssd.a)

set(KVS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/core/include)
set(NKV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/include_private)

add_library(nkvapi SHARED ${SOURCES_NKV_KDD} ${HEADERS_NKV_KDD})
target_include_directories(nkvapi PRIVATE ${KVS_INCLUDE_DIR} ${NKV_INCLUDE_DIR})
target_link_libraries (nkvapi ${NKVAPI_LIBS})
add_dependencies(nkvapi smglog zmqpp)
add_dependencies(nkvapi kvapi)

add_executable(nkv_test_cli ${CMAKE_CURRENT_SOURCE_DIR}/src/test/nkv_test_cli.cpp)
target_link_libraries(nkv_test_cli ${NKVAPI_LINK_LIBS})
add_dependencies(nkv_test_cli nkvapi)

execute_process(COMMAND date "+%Y%m%d" OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

set (git_cmd "git")
set (git_arg "describe")
set (git_arg1 "--abbrev=4")
set (git_arg2 "--dirty")
set (git_arg3 "--always")
set (git_arg4 "--tags")

#message(STATUS "git cmd: ${git_cmd} ${git_arg}")

execute_process(COMMAND ${git_cmd} ${git_arg} ${git_arg1} ${git_arg2} ${git_arg3} ${git_arg4} RESULT_VARIABLE dss_result OUTPUT_VARIABLE dss_version OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "Command = ${git_cmd} ${git_arg} , result =  ${dss_result} : ${dss_version}")

add_custom_command(
	TARGET nkv_test_cli POST_BUILD
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/include
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/lib
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/conf
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/test
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/openmpdk_driver/kernel_v5.1_nvmf
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/spec
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/spec/* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/spec
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config/nkv_config.json ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/conf/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config/nkv_config_emul.json ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/conf/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config/nkv_config_remote.json ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/conf/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config/event_mapping.json ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config/smglogger.properties ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/scripts/disk_stats.py ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/scripts/dss_host.py ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/scripts/minio_cleanup.sh ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/scripts/minio_startup_scripts ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/scripts/start.sh ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/core/kvssd_emul.conf ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/core/env_init.conf ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/include/* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/include/
	COMMAND cp -av ${CMAKE_CURRENT_BINARY_DIR}/lib/*.so* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/lib/
	#COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/lib/libsmglog/*.so ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/lib/
	#COMMAND cp -av ${CMAKE_CURRENT_BINARY_DIR}/lib/log4cpp/*.so* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/lib/
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/bin/sample_code_* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/../df_out/dssd/cmd/ustat/ustat   ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/	
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/bin/nkv_* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/src/test/nkv_test_cli.cpp ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/test/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/README_INSTALL.txt ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/
	COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/driver/PCIe/kernel_driver/kernel_v3.10.0-693-centos-7_4/ ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/openmpdk_driver/
	COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/driver/PCIe/kernel_driver/kernel_v5.1/* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/openmpdk_driver/kernel_v5.1_nvmf/
        COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}
	#COMMAND tar cvzf nkv-sdk-bin-${DATE}.tgz nkv-sdk
	COMMAND tar cvzf nkv-sdk-bin-${dss_version}.tgz nkv-sdk
        COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/
)



	




