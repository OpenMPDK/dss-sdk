cmake_minimum_required(VERSION 2.8.9)
project (libnkvapi C CXX)

option(NKV_WITH_EMU "Enable NKV with Emulator support" OFF)
option(NKV_WITH_KDD "Enable NKV with Kernel Driver support" OFF)
option(NKV_WITH_SPDK "Enable NKV with SPDK Driver support" OFF)

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

if(NKV_WITH_KDD)
  set(WITH_KDD ON CACHE BOOL "Enable KDD functionality" FORCE)
  #set(WITH_LOG ON CACHE BOOL "Enable KDD functionality" FORCE)

elseif(NKV_WITH_EMU)
  set(WITH_EMU ON CACHE BOOL "Enable KDD functionality" FORCE)

elseif(NKV_WITH_SPDK)

else()
  message( FATAL_ERROR "Please specify device driver type for compilation." )

endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/core/ ${CMAKE_BINARY_DIR}/lib)
set(KVAPI_LIBS -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY} -lkvapi)
set(NKVAPI_LINK_LIBS -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY} -L${LOG_LIBRARY_DIR} -pthread -lkvapi -lnkvapi -lsmglog)

# SOURCE CODE
SET(SOURCES_NKV_KDD
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nkv_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nkv_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/remote/cluster_map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nkv_utils.cpp
)

SET(HEADERS_NKV_KDD
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nkv_api.h
)

set(NKVAPI_CXXFLAGS "-std=c++11 -MMD -MP -Wall -D_FILE_OFFSET_BITS=64 -fPIC -fpermissive -march=native")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NKVAPI_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NKVAPI_CXXFLAGS}")
set(NKVAPI_LIBS ${NKVAPI_LIBS} -pthread -Wl,--no-as-needed -fPIC -lcurl -std=c++11 -lnuma -lrt -lboost_filesystem ${KVAPI_LIBS} -march=native)

set(KVS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/core/include)
set(NKV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/include_private)
set(NKV_REMOTE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/remote/include)

add_library(nkvapi SHARED ${SOURCES_NKV_KDD} ${HEADERS_NKV_KDD})
target_include_directories(nkvapi PRIVATE ${KVS_INCLUDE_DIR} ${NKV_INCLUDE_DIR} ${NKV_REMOTE_INCLUDE_DIR})
target_link_libraries (nkvapi ${NKVAPI_LIBS})
add_dependencies(nkvapi smglog)
add_dependencies(nkvapi kvapi)

add_executable(nkv_test_cli ${CMAKE_CURRENT_SOURCE_DIR}/src/test/nkv_test_cli.cpp)
target_link_libraries(nkv_test_cli ${NKVAPI_LINK_LIBS})
add_dependencies(nkv_test_cli nkvapi)

execute_process(COMMAND date "+%Y%m%d" OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
add_custom_command(
	TARGET nkv_test_cli POST_BUILD
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/include
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/lib
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/conf
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/test
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/openmpdk_driver
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config/nkv_config.json ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/conf/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config/nkv_config_emul.json ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/conf/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config/smglogger.properties ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/scripts/disk_stats.py ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/scripts/start.sh ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/core/kvssd_emul.conf ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/include/* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/include/
	COMMAND cp -av ${CMAKE_CURRENT_BINARY_DIR}/lib/*.so* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/lib/
	#COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/lib/libsmglog/*.so ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/lib/
	#COMMAND cp -av ${CMAKE_CURRENT_BINARY_DIR}/lib/log4cpp/*.so* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/lib/
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/bin/* ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/bin/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/src/test/nkv_test_cli.cpp ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/test/
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/README_INSTALL.txt ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/
	COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/src/openmpdk/PDK/driver/PCIe/kernel_driver/kernel_v3.10.0-693-centos-7_4/ ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/openmpdk_driver/
        COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND tar cvzf nkv-sdk-bin-${DATE}.tgz nkv-sdk
        COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/nkv-sdk/
)



	




