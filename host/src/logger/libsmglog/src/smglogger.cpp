/**
 # The Clear BSD License
 #
 # Copyright (c) 2022 Samsung Electronics Co., Ltd.
 # All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted (subject to the limitations in the disclaimer
 # below) provided that the following conditions are met:
 #
 # * Redistributions of source code must retain the above copyright notice, 
 #   this list of conditions and the following disclaimer.
 # * Redistributions in binary form must reproduce the above copyright notice,
 #   this list of conditions and the following disclaimer in the documentation
 #   and/or other materials provided with the distribution.
 # * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 #   contributors may be used to endorse or promote products derived from this
 #   software without specific prior written permission.
 # NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 # THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 # CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 # NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 # PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 # OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 # WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 # OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 # ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#include "smglogger.hpp"
#include <log4cpp/Category.hh>
#include <log4cpp/Priority.hh>
#include <log4cpp/PropertyConfigurator.hh>

// #include <mutex>
#include <cassert>
#include <fstream>  // std::ifstream

using log4cpp::Category;
using log4cpp::Priority;
using log4cpp::PropertyConfigurator;
using std::string;
using std::ifstream;

static bool checkFiles = false;

// helper class to initialize the logging system from a properties file
struct property_configurator {
    // Initializes the logging system from a properties file
    property_configurator() {
        try {
            if (checkFiles) {
                return;
            }

            // Local and /etc/opt names for properties files.
            char local_props[] = "smglogger.properties";
            char tmp_props[] = "/tmp/smglogger.properties";
            char etc_props[] = "/opt/soos/configure/smglogger.properties";

            ifstream flocal(local_props);
            if (flocal) {
                // Initialize from local file
                PropertyConfigurator::configure(local_props);
                checkFiles = true;
                return;
            }

            ifstream tlocal(tmp_props);
            if (tlocal) {
                // Initialize from /tmp file
                PropertyConfigurator::configure(tmp_props);
                checkFiles = true;
                return;
            }

            ifstream fetc(etc_props);
            if (fetc) {
                // Initialize from well-known properties file
                PropertyConfigurator::configure(etc_props);
                checkFiles = true;
                return;
            }

            throw std::invalid_argument("Properties file is invalid.");
        } catch (...) {
            throw std::invalid_argument("Properties file is invalid.");
        }
    }
};

// Samsung logger implementation class
class smgloggerimpl : public property_configurator {
    public:
    // Constructs a Samsung logger implementation object from a given category
    explicit smgloggerimpl(string const &cat);

    // No copy semantics
    smgloggerimpl(smgloggerimpl const &); // = delete;

    // No assignment semantics
    smgloggerimpl &operator=(smgloggerimpl const &); // = delete;

    // Destroys a Samsung logger implementation object
    ~smgloggerimpl();

    // Logs an alert given a format and a list of arguments
    void alert(const char *pszformat, va_list va);

    // Logs a debug message given a format and a list of arguments
    void debug(const char *pszformat, va_list va);

    // Logs an informational message given a format and a list of arguments
    void info(const char *pszformat, va_list va);

    // Logs a warning message given a format and a list of arguments
    void warn(const char *pszformat, va_list va);

    // Logs an error message given a format and a list of arguments
    void error(const char *pszformat, va_list va);

    // Logs a fatal message given a format and a list of arguments
    void fatal(const char *pszformat, va_list va);

    private:
    // The logging category.
    Category *category;
};

smgloggerimpl::smgloggerimpl(string const &cat)
    : category{Category::exists(cat)} {
    if (!category) {
        throw std::invalid_argument("Invalid logging category.");
    }
}

smgloggerimpl::~smgloggerimpl() {
}

void smgloggerimpl::alert(const char *pszformat, va_list va) {
    category->logva(Priority::ALERT, pszformat, va);
}

void smgloggerimpl::debug(const char *pszformat, va_list va) {
    category->logva(Priority::DEBUG, pszformat, va);
}

void smgloggerimpl::info(const char *pszformat, va_list va) {
    category->logva(Priority::INFO, pszformat, va);
}

void smgloggerimpl::warn(const char *pszformat, va_list va) {
    category->logva(Priority::WARN, pszformat, va);
}

void smgloggerimpl::error(const char *pszformat, va_list va) {
    category->logva(Priority::ERROR, pszformat, va);
}

void smgloggerimpl::fatal(const char *pszformat, va_list va) {
    category->logva(Priority::FATAL, pszformat, va);
}

smglogger::smglogger(string const &cat)
    : pimpl{new smgloggerimpl{cat}} {
}

smglogger::~smglogger() {
}

void smglogger::alert(const char *pszformat, va_list va) {
    pimpl->alert(pszformat, va);
}

void smglogger::debug(const char *pszformat, va_list va) {
    pimpl->debug(pszformat, va);
}

void smglogger::info(const char *pszformat, va_list va) {
    pimpl->info(pszformat, va);
}

void smglogger::warn(const char *pszformat, va_list va) {
    pimpl->warn(pszformat, va);
}

void smglogger::error(const char *pszformat, va_list va) {
    pimpl->error(pszformat, va);
}

void smglogger::fatal(const char *pszformat, va_list va) {
    pimpl->fatal(pszformat, va);
}
