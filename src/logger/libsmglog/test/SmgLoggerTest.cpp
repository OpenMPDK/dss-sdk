/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "csmglogger.h"
#include "gtest/gtest.h"

#include <iostream>
#include <fstream>
#include <cstdio>

using std::string;
using std::ifstream;
using std::ofstream;
using std::endl;


// The fixture for testing class Foo.
class SmgLoggerTest : public ::testing::Test {

protected:
    string nvmeof_log;
    string libs3_log;

    SmgLoggerTest() 
        : nvmeof_log{"nvmeof.log"}
        , libs3_log{"libs3.log"} 
    {
    }

    virtual ~SmgLoggerTest() {

    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        const char* props = R"xox(
log4cpp.rootCategory=INFO, rootAppender
log4cpp.category.libs3=FATAL, A1
log4cpp.category.nvmeof=DEBUG, A2

log4cpp.appender.rootAppender=FileAppender
log4cpp.appender.rootAppender.fileName=root.log
log4cpp.appender.rootAppender.layout=BasicLayout

log4cpp.appender.A1=FileAppender
log4cpp.appender.A1.fileName=libs3.log
log4cpp.appender.A1.layout=BasicLayout

log4cpp.appender.A2=FileAppender
log4cpp.appender.A2.fileName=nvmeof.log
log4cpp.appender.A2.layout=BasicLayout
)xox";

        ofstream ofs("smglogger.properties", std::ios::binary | std::ios::trunc);
        ofs << props << endl;
    }

    virtual void TearDown() {
    }

    // Objects declared here can be used by all tests in the test case for Foo.
};

// Tests that the Foo::Bar() method does Abc.
TEST_F(SmgLoggerTest, test_smg_acquire_logger_with_existing_category) {
    c_smglogger* p = smg_acquire_logger("nvmeof");
    ASSERT_TRUE(p != NULL);
    smg_release_logger(p);
}

TEST_F(SmgLoggerTest, test_smg_acquire_logger_with_non_existing_category) {
    c_smglogger* p = smg_acquire_logger("dummy");
    ASSERT_TRUE(p == NULL);
}

TEST_F(SmgLoggerTest, test_smg_acquire_logger_with_null) {
    c_smglogger* p = smg_acquire_logger(NULL);
    ASSERT_TRUE(p == NULL);
}

TEST_F(SmgLoggerTest, test_smg_acquire_logger_with_empty_string) {
    c_smglogger* p = smg_acquire_logger("");
    ASSERT_TRUE(p == NULL);
}

TEST_F(SmgLoggerTest, test_smg_alert) {
    const string substr{"This is an alert"};

    c_smglogger* p = smg_acquire_logger("nvmeof");
    smg_alert(p, substr.c_str());

    ifstream fin(nvmeof_log, std::ios::binary);
    if(!fin) {
        FAIL();
    }
    else {
        string text;
        std::getline(fin, text);
        ASSERT_TRUE(text.find(substr) != string::npos);
    }
    smg_release_logger(p);    
}

TEST_F(SmgLoggerTest, test_smg_debug) {
    const string substr{"This is a debug message"};

    c_smglogger* p = smg_acquire_logger("nvmeof");
    smg_debug(p, substr.c_str());
    smg_release_logger(p);

    ifstream fin(nvmeof_log, std::ios::binary);
    if(!fin) {
        FAIL();
        return;
    }

    for (string text; std::getline(fin, text); /*empty*/) {
        if (text.find(substr) != string::npos) {
            SUCCEED();
            return;
        }
    }

    FAIL();
}

TEST_F(SmgLoggerTest, test_smg_info) {
    const string substr{"This is an informational message"};

    c_smglogger* p = smg_acquire_logger("nvmeof");
    smg_info(p, substr.c_str());
    smg_release_logger(p);

    ifstream fin(nvmeof_log, std::ios::binary);
    if(!fin) {
        FAIL();
        return;
    }

    for (string text; std::getline(fin, text); /*empty*/) {
        if (text.find(substr) != string::npos)
            SUCCEED();
            return;
    }

    FAIL();
}

TEST_F(SmgLoggerTest, test_smg_warn) {
    const string substr{"This is a warning message"};

    c_smglogger* p = smg_acquire_logger("nvmeof");
    smg_warn(p, substr.c_str());
    smg_release_logger(p);

    ifstream fin(nvmeof_log, std::ios::binary);
    if(!fin) {
        FAIL();
        return;
    }

    for (string text; std::getline(fin, text); /*empty*/) {
        if (text.find(substr) != string::npos)
            SUCCEED();
            return;
    }

    FAIL();
}

TEST_F(SmgLoggerTest, test_smg_error) {
    const string substr{"This is an error message"};

    c_smglogger* p = smg_acquire_logger("nvmeof");
    smg_error(p, substr.c_str());
    smg_release_logger(p);

    ifstream fin(nvmeof_log, std::ios::binary);
    if(!fin) {
        FAIL();
        return;
    }

    for (string text; std::getline(fin, text); /*empty*/) {
        if (text.find(substr) != string::npos)
            SUCCEED();
            return;
    }

    FAIL();
}

TEST_F(SmgLoggerTest, test_smg_fatal) {
    const string substr{"This is a fatal message"};

    c_smglogger* p = smg_acquire_logger("nvmeof");
    smg_warn(p, substr.c_str());
    smg_release_logger(p);

    ifstream fin(nvmeof_log, std::ios::binary);
    if(!fin) {
        FAIL();
        return;
    }

    for (string text; std::getline(fin, text); /*empty*/) {
        if (text.find(substr) != string::npos)
            SUCCEED();
            return;
    }

    FAIL();
}
