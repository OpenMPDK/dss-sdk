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

#ifndef INCLUDED_C_SMGLOGGER
#define INCLUDED_C_SMGLOGGER

/** @file csmglogger.h
    Samsung logger library file.  You should include this file in your programs
    to make use of logging facilities.
*/

/** @var typedef void c_smglogger.
    @brief Logger type definition.
*/

/** @fn c_smglogger *smg_acquire_logger(char *pszcat)
    @brief Acquires a logger instance.
   Acquires a logger instance with a given category name. The category must exist
   in the smglogger.properties file and be associated with an appender for logging to occur.
   @param pszcat The category name. The category must not be NULL or an empty string.
   @return A pointer to a c_smglogger object that can be used for logging or NULL.
*/

/** @fn void smg_release_logger(c_smglogger *plogger)
    @brief Releases a logger instance.
   Releases a logger instance acquired through a call to smg_acquire_logger.
   @param plogger Pointer to a non-NULL logger acquired through smg_acquire_logger.
 */

/** @fn smg_alert(c_smglogger *plogger, pszformat, ...)
    @brief Logs an alert.
   Logs an alert using a logger instance.
   @param plogger Pointer a non-NULL logger acquired through smg_acquire_logger.
   @param pszformat Pointer to a null-terminated string specifying how to interpret the data.
 */

/** @fn smg_debug(c_smglogger *plogger, pszformat, ...)
    @brief Logs a debug message.
   Logs a message with a debug level using a logger instance.
   @param plogger Pointer a non-NULL logger acquired through smg_acquire_logger.
   @param pszformat Pointer to a null-terminated string specifying how to interpret the data.
 */

/** @fn smg_info(c_smglogger *plogger, pszformat, ...)
    @brief Logs an informational  message.
   Logs a message with an informational level using a logger instance.
   @param plogger Pointer a non-NULL logger acquired through smg_acquire_logger.
   @param pszformat Pointer to a null-terminated string specifying how to interpret the data.
 */

/** @fn smg_warn(c_smglogger *plogger, pszformat, ...)
    @brief Logs a warning message.
   Logs a message with a warning level using a logger instance.
   @param plogger Pointer a non-NULL logger acquired through smg_acquire_logger.
   @param pszformat Pointer to a null-terminated string specifying how to interpret the data.
 */

/** @fn smg_error(c_smglogger *plogger, pszformat, ...)
    @brief Logs an error message.
   Logs a message with an error level using a logger instance.
   @param plogger Pointer a non-NULL logger acquired through smg_acquire_logger.
   @param pszformat Pointer to a null-terminated string specifying how to interpret the data.
 */

/** @fn smg_fatal(c_smglogger *plogger, pszformat, ...)
    @brief Logs a fatal message.
   Logs a message with a fatal level using a logger instance.
   @param plogger Pointer a non-NULL logger acquired through smg_acquire_logger.
   @param pszformat Pointer to a null-terminated string specifying how to interpret the data.
 */
typedef void c_smglogger;

#ifdef __cplusplus
extern "C"
{
#endif

c_smglogger *smg_acquire_logger(const char *pszcat);
void smg_release_logger(c_smglogger *logger);

void smg_alert(c_smglogger *plogger, const char* pszformat, ...);
void smg_debug(c_smglogger *plogger, const char* pszformat, ...);
void smg_info(c_smglogger *plogger, const char* pszformat, ...);
void smg_warn(c_smglogger *plogger, const char* pszformat, ...);
void smg_error(c_smglogger *plogger, const char* pszformat, ...);
void smg_fatal(c_smglogger *plogger, const char* pszformat, ...);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // INCLUDED_C_SMGLOGGER