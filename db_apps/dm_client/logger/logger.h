/*
 * Configurable console/syslog interface.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __LOGGER_H_20180814__
#define __LOGGER_H_20180814__

    #include <stdbool.h>

    typedef enum {
        LOGGER_ALL,
        LOGGER_TRACE,
        LOGGER_DEBUG,
        LOGGER_INFO,
        LOGGER_NOTICE,
        LOGGER_WARNING,
        LOGGER_ERROR,
        LOGGER_NONE
    } logger_level;

    typedef struct {
        bool use_syslog;
        bool use_console;
        bool no_colors;
        bool no_date;
        bool no_source;
        int  min_level;
    } logger_opts;

    typedef struct {
        char* buffer;
        int written;
        int remaining;
    } logger_buf;

    #ifdef LOGGER_NO_SOURCE
        #define logger(lvl, ...)  __logger(lvl, NULL, 0, false, __VA_ARGS__)
        #define loggerv(lvl, ...) __loggerv(lvl, NULL, 0, false, __VA_ARGS__)
        #define loggerh(lvl, ...) __loggerh(lvl, NULL, 0, __VA_ARGS__)
    #else
        #define logger(lvl, ...)  __logger(lvl, __FILE__, __LINE__, false, __VA_ARGS__)
        #define loggerv(lvl, ...) __loggerv(lvl, __FILE__, __LINE__, false, __VA_ARGS__)
        #define loggerh(lvl, ...) __loggerh(lvl, __FILE__, __LINE__, __VA_ARGS__)
    #endif

    #ifndef LOGGER_NO_EXTRA_MACROS
        #ifdef LOGGER_NO_TRACE
            #define log_trace(fmt, ...)
            #define log_trace_hex(desc, buf, len)
        #else
            #define log_trace(fmt, ...)           loggerv(LOGGER_TRACE, fmt, ##__VA_ARGS__)
            #define log_hex_trace(desc, buf, len) loggerh(LOGGER_TRACE, desc, buf, len)
        #endif

        #define log_debug(fmt, ...)   loggerv(LOGGER_DEBUG, fmt, ##__VA_ARGS__)
        #define log_info(fmt, ...)    loggerv(LOGGER_INFO, fmt, ##__VA_ARGS__)
        #define log_notice(fmt, ...)  loggerv(LOGGER_NOTICE, fmt, ##__VA_ARGS__)
        #define log_warning(fmt, ...) loggerv(LOGGER_WARNING, fmt, ##__VA_ARGS__)
        #define log_error(fmt, ...)   loggerv(LOGGER_ERROR, fmt, ##__VA_ARGS__)

        #define log_hex_debug(desc, buf, len)   loggerh(LOGGER_DEBUG, desc, buf, len)
        #define log_hex_info(desc, buf, len)    loggerh(LOGGER_INFO, desc, buf, len)
        #define log_hex_notice(desc, buf, len)  loggerh(LOGGER_NOTICE, desc, buf, len)
        #define log_hex_warning(desc, buf, len) loggerh(LOGGER_WARNING, desc, buf, len)
        #define log_hex_error(desc, buf, len)   loggerh(LOGGER_ERROR, desc, buf, len)
    #endif

    void logger_initialise(logger_opts* opts, const char* name);
    bool logger_willprint(int lvl, bool no_syslog);

    void __logger (int lvl, const char* file, int line, bool no_syslog, const char* msg);
    void __loggerv(int lvl, const char* file, int line, bool no_syslog, const char* fmt, ...);
    void __loggerh(int lvl, const char* file, int line, const char* desc, const void* buf, int len);

    #define LOGGER_BINIT(ptr, len) {.buffer = (ptr), .written = 0, .remaining = (len) - 1}

    void logger_binit  (logger_buf* buf, char* ptr, int len);
    void logger_bwrite (logger_buf* buf, const char* str, int len);
    void logger_bzwrite(logger_buf* buf, const char* str);
    void logger_bfwrite(logger_buf* buf, const char* fmt, ...);
    void logger_bcwrite(logger_buf* buf, char c);
    void logger_bfinish(logger_buf* buf);
    bool logger_bcheck (logger_buf* buf, int needed);

#endif
