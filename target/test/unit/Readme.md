<!--
The Clear BSD License

Copyright (c) 2023 Samsung Electronics Co., Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->
# DSS Unit testing

This documentation provides pointers to implement new unit test cases. DSS target used CUnit to implement unit test cases. The unit test is added to CMake framework and can be run using CTest.

## Building and running unit test

The build script can be used to conviniently build and run unit test the option `--run-tests` will compile and run DSS unit test. Additionally, if `--with-coverage` option is used with the build script then the binaried built will support coverage and after running the unit tests a coverage report in sonzrqube format for the tests will be generated in reports directory `reports/sonar_qube_ut_coverage_report.xml`

    ./build.sh --run-tests
    ./build.sh --run-tests --with-coverage

## Creating unit tests

* First within the `test/unit` folder create the folder structure that micmics the source directory structure.
* In the `CMakeLists.txt` in the `test/unit` folder add top level subdirectory if it's newly created
* Add `CMakeLists.txt` file to each sub directory and add each sub directory

        add_subdirectory(subdirectory_name)

* The last sub directory is named with the file name for which the tests are to be created
* To write test cases for a file (c or cpp) `new.c` create `new_ut.c` inside the last sub directory
* In the CMakeLists.txt in the last sub direcroy add below format code(change names as appropriate)

        add_executable(new_ut ${CMAKE_SOURCE_DIR}/path/to/src/directory/new.c
                                                                        new_ut.c)
        # Add any compile options if needed
        target_link_libraries(new_ut ${UNIT_LIBS})

* Use below code block as a reference for creating new CUnit test suite.

        #include "CUnit/Basic.h"

        void testDummy1(void)
        {
            //Do Test
            CU_ASSERT(1);
        }

        void testDummy2(void)
        {
            //Do Test
            CU_ASSERT(1);
        }

        int main( )
        {
            CU_pSuite pSuite = NULL;

            if(CUE_SUCCESS != CU_initialize_registry()) {
                return CU_get_error();
            }

            pSuite = CU_add_suite("New Test", NULL, NULL);
            if(NULL == pSuite) {
                CU_cleanup_registry();
                return CU_get_error();
            }

            if(
                NULL == CU_add_test(pSuite, "testDummy1", testDummy1) ||
                NULL == CU_add_test(pSuite, "testDummy2", testDummy2)
            ) {
                CU_cleanup_registry();
                return CU_get_error();
            }

            CU_basic_set_mode(CU_BRM_VERBOSE);
            CU_basic_run_tests();
            CU_cleanup_registry();

            return CU_get_error();
        }

* Add new tests with format `void testNewTest(void)`
* Use `CU_ASSERT(/*condition*/)` as required to evaluate intermediate states
* Add test case in the if block in main function
* If there are portions of code that should not be compiled as part of unit test binary from the original source, use following `#ifndef` to not compile for unit testing

        #ifndef DSS_BUILD_CUNIT_TEST

        // Code that should not be built
        // while compiling for unit test

        #else

        // Code that should be built
        // while compiling for unit test

        #endif //DSS_BUILD_CUNIT_TEST
