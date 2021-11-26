/*
 * utils - Miscellaneous utility functions.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
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


#include "utils.h"

#include <stdio.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>

/* buffer size for hex dump */
#define LOG_DUMP_HEX_BUF 1024


/**
 * @brief log hex codes to syslog
 *
 *
 * @param dump_name message to log
 * @param data data to log
 * @param len
 */
void log_dump_hex(const char* dump_name, const void* data, int len)
{
    char line[LOG_DUMP_HEX_BUF];
    const char* ptr = (const char*)data;
    char* buf;

    int i;

    syslog(LOG_DEBUG, "* %s (data=0x%p,len=%d)", dump_name, data, len);

    buf = line;
    for (i = 0; i < len; i++) {
        if (i && !(i % 16)) {
            syslog(LOG_DEBUG, "%04x: %s", i & ~0x0f, line);
            buf = line;
        }

        buf += sprintf(buf, "%02x ", ptr[i]);
    }

    if (i) {
        syslog(LOG_DEBUG, "%04x: %s", i & ~0x0f, line);
    }
}

/**
 * @brief get monotonic msec.
 *
 * @return msec.
 */
uint64_t get_monotonic_msec(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief get monotonic time elapsed in msec since this function is previously called.
 *
 * @return msec.
 */
uint64_t get_time_elapsed_msec(void)
{
    static uint64_t last_msec = 0;
    uint64_t now_msec = get_monotonic_msec();
    uint64_t time_elapsed;

    if (last_msec) {
        time_elapsed = now_msec - last_msec;
    } else {
        time_elapsed = 0;
    }

    last_msec = now_msec;

    return time_elapsed;
}
