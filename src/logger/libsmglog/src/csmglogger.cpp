
#include "csmglogger.h"
#include "smglogger.hpp"

#include <cassert>

using std::string;

extern "C" {

c_smglogger *smg_acquire_logger(const char *pszcat) {
    try {
        if (pszcat == NULL) {
            return NULL;
        }

        string cat{pszcat};
        if (cat.empty()) {
            return NULL;
        }

        smglogger *plogger = new smglogger(cat);
        return reinterpret_cast<c_smglogger *>(plogger);
    } catch (...) {
        return NULL;
    }
}

void smg_release_logger(c_smglogger *pclogger) {
    if (!pclogger) {
        return;
    }

    smglogger *plogger = reinterpret_cast<smglogger *>(pclogger);
    delete plogger;
}

void smg_alert(c_smglogger *pclogger, const char *pszformat, ...) {
    if (!pclogger) {
        return;
    }

    smglogger *plogger = reinterpret_cast<smglogger *>(pclogger);
    va_list va;
    va_start(va, pszformat);
    plogger->alert(pszformat, va);
    va_end(va);
}

void smg_debug(c_smglogger *pclogger, const char *pszformat, ...) {
    if (!pclogger) {
        return;
    }

    smglogger *plogger = reinterpret_cast<smglogger *>(pclogger);
    va_list va;
    va_start(va, pszformat);
    plogger->debug(pszformat, va);
    va_end(va);
}

void smg_info(c_smglogger *pclogger, const char *pszformat, ...) {
    if (!pclogger) {
        return;
    }

    smglogger *plogger = reinterpret_cast<smglogger *>(pclogger);
    va_list va;
    va_start(va, pszformat);
    plogger->info(pszformat, va);
    va_end(va);
}

void smg_warn(c_smglogger *pclogger, const char *pszformat, ...) {
    if (!pclogger) {
        return;
    }

    smglogger *plogger = reinterpret_cast<smglogger *>(pclogger);
    va_list va;
    va_start(va, pszformat);
    plogger->warn(pszformat, va);
    va_end(va);
}

void smg_error(c_smglogger *pclogger, const char *pszformat, ...) {
    if (!pclogger) {
        return;
    }

    smglogger *plogger = reinterpret_cast<smglogger *>(pclogger);
    va_list va;
    va_start(va, pszformat);
    plogger->error(pszformat, va);
    va_end(va);
}

void smg_fatal(c_smglogger *pclogger, const char *pszformat, ...) {
    if (!pclogger) {
        return;
    }

    smglogger *plogger = reinterpret_cast<smglogger *>(pclogger);
    va_list va;
    va_start(va, pszformat);
    plogger->fatal(pszformat, va);
    va_end(va);
}

} // extern C
