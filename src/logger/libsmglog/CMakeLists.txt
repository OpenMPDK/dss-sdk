project(smglog)

add_definitions(-std=c++11)

add_library(${PROJECT_NAME} SHARED src/smglogger.cpp src/csmglogger.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# target_compile_definitions(${PROJECT_NAME} PUBLIC -std=c++11)


#target_link_libraries(${PROJECT_NAME}
#    log4cpp
#)

#install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_LIB_DIR} )
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/configure/smglogger.properties DESTINATION ${INSTALL_CONFIGURE_FILES_DIR})

# These must exist for the test in the Jenkins nightly build
ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME}
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/configure/smglogger.properties /tmp
)

#if(NOT RELEASE_BUILD)
#    add_subdirectory(test)
#    add_subdirectory(coverage)
#endif()

