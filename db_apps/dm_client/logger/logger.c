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

#define _POSIX_SOURCE

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "logger.h"

#define SMALL_BUFFER_LENGTH 2048
#define LARGE_BUFFER_LENGTH 32768

static logger_opts __opts = {
    .use_syslog  = false,
    .use_console = false,
    .no_colors   = false,
    .no_date     = false,
    .no_source   = false,
    .min_level   = LOGGER_INFO
};

static void print_console(int lvl, const char* file, int line, const char* msg);
static void print_syslog(int lvl, const char* msg);
static void build_hexdump(logger_buf* buf, const char* desc, const void* ptr, int len);

void logger_initialise(logger_opts* opts, const char* name)
{
    if (opts) {
        memcpy(&__opts, opts, sizeof(logger_opts));
    }
    if (__opts.use_syslog) {
        openlog(name, LOG_PID, 0);
    }
}

bool logger_willprint(int level, bool no_syslog)
{
    if (level < __opts.min_level) {
        return false;
    }
    if (__opts.use_console) {
        return true;
    }
    if (__opts.use_syslog && !no_syslog && level > LOGGER_TRACE) {
        return true;
    }
    return false;
}

void __logger(int lvl, const char* file, int line, bool no_syslog, const char* msg)
{
    if (logger_willprint(lvl, no_syslog)) {
        print_console(lvl, file, line, msg);
        print_syslog(lvl, msg);
    }
}

void __loggerv(int lvl, const char* file, int line, bool no_syslog, const char* fmt, ...)
{
    if (logger_willprint(lvl, no_syslog)) {
        va_list args;
        va_start(args, fmt);

        char msg[SMALL_BUFFER_LENGTH];
        vsnprintf(msg, sizeof(msg), fmt, args);
        print_console(lvl, file, line, msg);
        print_syslog(lvl, msg);

        va_end(args);
    }
}

void __loggerh(int lvl, const char* file, int line, const char* desc, const void* ptr, int len)
{
    if (logger_willprint(lvl, true)) {
        char* msg = malloc(LARGE_BUFFER_LENGTH);
        if (msg) {
            logger_buf buf = LOGGER_BINIT(msg, LARGE_BUFFER_LENGTH);
            build_hexdump(&buf, desc, ptr, len);
            logger_bfinish(&buf);
            print_console(lvl, file, line, msg);
            free(msg);
        }
    }
}

void logger_binit(logger_buf* buf, char* ptr, int len)
{
    buf->buffer = ptr;
    buf->written = 0;
    buf->remaining = len - 1;
}

void logger_bwrite(logger_buf* buf, const char* str, int len)
{
    if (len > buf->remaining) {
        len = buf->remaining;
    }
    memcpy(buf->buffer + buf->written, str, len);
    buf->written += len;
    buf->remaining -= len;
    buf->buffer[buf->written] = 0;
}

void logger_bzwrite(logger_buf* buf, const char* str)
{
    while (*str && buf->remaining > 0) {
        buf->buffer[buf->written++] = *(str++);
        buf->remaining--;
    }
}

void logger_bfwrite(logger_buf* buf, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int count = vsnprintf(buf->buffer + buf->written, buf->remaining, fmt, args);
    if (count > 0) {
        if (count > buf->remaining) {
            count = buf->remaining;
        }
        buf->written += count;
        buf->remaining -= count;
    }
    va_end(args);
}

void logger_bcwrite(logger_buf* buf, char c)
{
    if (buf->remaining > 0) {
        buf->buffer[buf->written++] = c;
        buf->remaining--;
    }
}

void logger_bfinish(logger_buf* buf)
{
    buf->buffer[buf->written] = 0;
}

bool logger_bcheck(logger_buf* buf, int needed)
{
    return buf->remaining >= needed;
}

static void print_console(int lvl, const char* file, int line, const char* msg)
{
    if (__opts.use_console) {
        const char* lvl_name = "[?]";
        switch (lvl) {
            case LOGGER_ERROR: 
                lvl_name = __opts.no_colors ? "[ERROR]" : "\033[0;31m[ERROR]\033[0m";
                break;
            case LOGGER_WARNING:
                lvl_name = __opts.no_colors ? "[WARNING]" : "\033[0;33m[WARNING]\033[0m";
                break;
            case LOGGER_NOTICE:
                lvl_name = __opts.no_colors ? "[NOTICE]" : "\033[0;32m[NOTICE]\033[0m";
                break;
            case LOGGER_INFO:
                lvl_name = __opts.no_colors ? "[INFO]" : "\033[0;32m[INFO]\033[0m";
                break;
            case LOGGER_DEBUG:
                lvl_name = __opts.no_colors ? "[DEBUG]" : "\033[0;37m[DEBUG]\033[0m";
                break;
            case LOGGER_TRACE:
                lvl_name = __opts.no_colors ? "[TRACE]" : "\033[0;37m[TRACE]\033[0m";
                break;
        }
        if (!__opts.no_source && file) {
            const char* last_slash = strrchr(file, '/');
            if (last_slash) {
                file = last_slash + 1;
            }
        }
        if (__opts.no_date) {
            if (__opts.no_source || !file) {
                fprintf(stderr, "%s %s\n", lvl_name, msg);
            } else {
                fprintf(stderr, "%s (%s:%i) %s\n", lvl_name, file, line, msg);
            }
        } else {
            char now_buf[64];
            struct tm now_tm = {0};
            time_t now_time = time(NULL);
            localtime_r(&now_time, &now_tm);
            strftime(now_buf, sizeof(now_buf), "%F %T", &now_tm);
            if (__opts.no_source || !file) {
                fprintf(stderr, "%s %s %s\n", now_buf, lvl_name, msg);
            } else {
                fprintf(stderr, "%s %s (%s:%i) %s\n", now_buf, lvl_name, file, line, msg);
            }
        }
    }
}

static void print_syslog(int lvl, const char* msg)
{
    if (__opts.use_syslog && lvl > LOGGER_TRACE) {
        int priority = LOG_DEBUG;
        switch (lvl) {
            case LOGGER_ERROR:   priority = LOG_ERR; break;
            case LOGGER_WARNING: priority = LOG_WARNING; break;
            case LOGGER_NOTICE:  priority = LOG_NOTICE; break;
            case LOGGER_INFO:    priority = LOG_INFO; break;
        }
        syslog(priority, "%s", msg);
    }
}

static void build_hexdump(logger_buf* buf, const char* desc, const void* ptr, int len)
{
    #define BYTES_PER_COLUMN 8
    #define BYTES_PER_LINE   16

    logger_bfwrite(buf, "[HEX] %s, %i bytes%c", desc, len, len == 0 ? '.' : ':');

    const unsigned char* bytes = ptr;
    while (len > 0 && logger_bcheck(buf, 1)) {
        if (__opts.no_colors) {
            logger_bfwrite(buf, "\n  0x%08uX:", bytes - (unsigned char*)ptr);
        } else {
            logger_bfwrite(buf, "\n  \033[0;37m0x%08X:\033[0m", bytes - (unsigned char*)ptr);
        }
        int num = len > BYTES_PER_LINE ? BYTES_PER_LINE : len;
        for (int i = 0; i < BYTES_PER_LINE; i++) {
            if (!(i % BYTES_PER_COLUMN)) {
                logger_bzwrite(buf, " ");
            }
            if (i < num) {
                logger_bfwrite(buf, " %02X", bytes[i]);
            } else {
                logger_bzwrite(buf, "   ");
            }
        }
        for (int i = 0; i < num; i++) {
            if (!(i % BYTES_PER_COLUMN)) {
                logger_bzwrite(buf, " ");
            }
            logger_bfwrite(buf, " %c", isprint(bytes[i]) ? bytes[i] : '.');
        }
        len -= num;
        bytes += num;
    }
}
