project(coverage-smglog CXX)


set(TEST_COVERAGE_NAME cov_smglog)

add_executable(${TEST_COVERAGE_NAME}
    ../test/SmgLoggerTest.cpp
)

target_link_libraries(${TEST_COVERAGE_NAME} LINK_PUBLIC
    smglog
    gtest_main
    gtest
    -lpthread
)

set_target_properties(
    ${TEST_COVERAGE_NAME}
    PROPERTIES LINK_FLAGS "-O0 -g -lgcov -ftest-coverage"
)

include(${root_source_dir}/coverage.cmake)

SETUP_TARGET_FOR_COVERAGE(
  ${PROJECT_NAME}
  ${CMAKE_CURRENT_BINARY_DIR}/${TEST_COVERAGE_NAME}
  ${HTML_LCOV_REPORT}/${TEST_COVERAGE_NAME}
)
