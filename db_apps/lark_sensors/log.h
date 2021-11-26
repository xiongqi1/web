#ifndef LOG_15001104032019
#define LOG_15001104032019
/*
 * @file
 * Log mcro defintions
 *
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
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

#include <syslog.h>
#include <stdio.h>

#ifdef CLI_TEST
#define PROG_NAME      "lark_sensors_test"
#else
#define PROG_NAME      "lark_sensors"
#endif

//#############################################################################
//#############################################################################
#if defined(CLI_TEST) || defined(BHI_TEST)

#define LS_ERROR(...)    fprintf(stderr, __VA_ARGS__)
#define LS_NOTICE        printf
#define LS_INFO          printf
#define LS_DEBUG         printf

#else

#define LS_ERROR(...)    syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), __VA_ARGS__)
#define LS_NOTICE(...)   syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), __VA_ARGS__)
#define LS_INFO(...)     syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), __VA_ARGS__)
#define LS_DEBUG(...)    syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), __VA_ARGS__)

#endif

//#############################################################################
// used in lark_sensor.c
//#############################################################################
#ifdef HAS_RDB_SUPPORT

#define CHECK(expr, expected, ...) do { \
    if ((expr) != (expected)) { \
        LS_ERROR("%d: ", __LINE__); \
        LS_ERROR(__VA_ARGS__); \
        terminate_rdb_services(); \
        exit(1); \
    }; \
} while(0)

#else

#define CHECK(expr, expected, ...) do { \
    if ((expr) != (expected)) { \
        LS_ERROR("%d: ", __LINE__); \
        LS_ERROR(__VA_ARGS__); \
        exit(1); \
    }; \
} while(0)

#endif

#endif  //LOG_15001104032019