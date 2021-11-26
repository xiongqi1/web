#ifndef NC_UTIL_H_11020009122019
#define NC_UTIL_H_11020009122019
/**
 * @file nc_util.h
 * @brief Provides useful macros to help program more simple
 *
 * Copyright Notice:
 * Copyright (C) 2015 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <syslog.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#define MAX(x,y) (((x) > (y)) ? (x) : (y))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define ROUND_UP(x, y) ( \
{ \
    const typeof(y) __y = (y); \
    (((x) + (__y - 1)) / __y) * __y; \
} \
)

#define ROUND_DOWN(x, y) ( \
{ \
    typeof(x) __x = (x); \
    __x - (__x % (y)); \
} \
)

#define _TO_STR_INTERNAL(s) #s
#define TO_STR(s) _TO_STR_INTERNAL(s)


#ifdef CLI_DEBUG
// log to stdout/stderr
#define BLOG_ERR(...) fprintf(stderr, __VA_ARGS__)
#define BLOG_WARNING(...) printf( __VA_ARGS__)
#define BLOG_NOTICE(...) printf( __VA_ARGS__)
#define BLOG_INFO(...) printf( __VA_ARGS__)
#define BLOG_DEBUG(...) printf(__VA_ARGS__)

#else
// log to syslog
#define BLOG_ERR(...) syslog(LOG_ERR, __VA_ARGS__)
#define BLOG_WARNING(...) syslog(LOG_WARNING, __VA_ARGS__)
#define BLOG_NOTICE(...) syslog(LOG_NOTICE, __VA_ARGS__)
#define BLOG_INFO(...) syslog(LOG_INFO, __VA_ARGS__)
#define BLOG_DEBUG(...) syslog(LOG_DEBUG, __VA_ARGS__)

#endif

#endif