execute_process (
    COMMAND rm ${CMAKE_CURRENT_BINARY_DIR}/JudyLTables.c
    RESULT_VARIABLE rm_result
    ERROR_QUIET
)
if (NOT rm_result EQUAL 0)
	message (STATUS "JudyLTables.c not found")
else ()
	message (STATUS "removed JudyLTables.c")
endif ()

execute_process ( 
    COMMAND rm ${CMAKE_CURRENT_BINARY_DIR}/JudyLTables_64.c
    RESULT_VARIABLE rm_result
    ERROR_QUIET
)
if (NOT rm_result EQUAL 0)
	message (STATUS "JudyLTables_64.c not found")
else ()
	message (STATUS "removed JudyLTables_64.c")
endif ()

execute_process ( 
    COMMAND rm ${CMAKE_CURRENT_BINARY_DIR}/JudyLTablesGen
    RESULT_VARIABLE rm_result
    ERROR_QUIET
)
if (NOT rm_result EQUAL 0)
	message (STATUS "JudyLTablesGen not found")
else ()
	message (STATUS "Removed JudyLTableGen")
endif ()

message (STATUS "Generating JudyLTableGen")
execute_process ( 
		COMMAND gcc
		-I${CMAKE_CURRENT_SOURCE_DIR}/../
		-I${CMAKE_CURRENT_SOURCE_DIR}/../JudyCommon
		-m64 -D__X86_64__ -D__LIN__ -D__USER__ -DJUDYL
		${CMAKE_CURRENT_SOURCE_DIR}/JudyLTablesGen.c -o
		${CMAKE_CURRENT_BINARY_DIR}/JudyLTablesGen
		RESULT_VARIABLE gcc_result
)
if (NOT gcc_result EQUAL 0)
	message (FATAL_ERROR "Failed to generate JudyLTableGen")
else ()
	message (STATUS "Created JudyLTableGen")
endif ()
message (STATUS "Generating JudyLTables_64.c")
execute_process ( 
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/JudyLTablesGen
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		RESULT_VARIABLE exec_result
)
if (NOT exec_result EQUAL 0)
	message (FATAL_ERROR "Failed to generate JudyLTables_64.c")
else ()
	message (STATUS "Created JudyLTables_64.c")
endif ()


