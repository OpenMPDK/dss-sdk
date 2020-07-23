cmake_minimum_required(VERSION 2.6)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/JudyCommon")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL")
add_subdirectory(src)

add_definitions(-DJUDYL)
add_definitions(-D__LIN__)
add_definitions(-D__USER__)
add_definitions(-D__X86_64__)

add_library(judyL STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLByCount.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLCascade.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLCount.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLCreateBranch.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLDecascade.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLDel.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLFirst.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLFreeArray.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLGet.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/j__udyLGet.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLInsArray.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLIns.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLInsertBranch.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLMallocIF.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLMemActive.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLMemUsed.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLNext.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLNextEmpty.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLPrev.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyL/JudyLPrevEmpty.c"
        "${CMAKE_CURRENT_BINARY_DIR}/src/JudyL/JudyLTables_64.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/JudyCommon/JudyMalloc.c"
)
