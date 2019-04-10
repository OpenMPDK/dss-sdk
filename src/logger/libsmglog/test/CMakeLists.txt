project(test-smglog CXX)

add_executable(${PROJECT_NAME} SmgLoggerTest.cpp )

target_link_libraries(${PROJECT_NAME} LINK_PUBLIC
    smglog
    gtest_main
    gtest
    -lpthread
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} --gtest_output=xml:${XML_TEST_REPORT}/${PROJECT_NAME}.xml)

install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_TEST_DIR} )

