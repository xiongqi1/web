/**
 * Declare common C defines or structures for the project
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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

#ifndef __D_H__
#define __D_H__

/* debug flags for testing purposes */

//#define DEBUG_VERBOSE
//#define DEBUG_PACKET_DUMP
//#define TEST_MODE_MODULE
//#define TEST_MODE_SHORT_BEARER_RDB_FLUSH
//#define TEST_MODE_BEARER_REJECT
//#define TEST_MODE_BROKEN_BEARER_INTEGRITY

// #ifndef NR5G
// #define NR5G
// #endif

#include <stdio.h>
#include <syslog.h>
#include <time.h>

/* log defines */
//#define LOG(level,m,...) printf("LOG %s:%d " m "\n",__func__,__LINE__, ##__VA_ARGS__)
#define LOG(level, m, ...) syslog(level, "%s:%d " m, __func__, __LINE__, ##__VA_ARGS__)
#define DEBUG(m, ...) LOG(LOG_DEBUG, m, ##__VA_ARGS__)
#define INFO(m, ...) LOG(LOG_INFO, m, ##__VA_ARGS__)
#define NOTICE(m, ...) LOG(LOG_NOTICE, m, ##__VA_ARGS__)
#define WARN(m, ...) LOG(LOG_WARNING, m, ##__VA_ARGS__)
#define ERR(m, ...) LOG(LOG_ERR, m, ##__VA_ARGS__)

#ifdef DEBUG_VERBOSE
#define VERBOSE(m, ...) LOG(LOG_DEBUG, m, ##__VA_ARGS__)
#else
#define VERBOSE(m, ...) \
    do { \
    } while (0)
#endif

/* peripheral defines */
#define __get_member_offset(s, m) ((unsigned int)&(((typeof(s))(0))->m))
#define __is_in_boundary(p, b, l) (((char *)(b) <= (char *)(p)) && (((char *)(p)) < ((char *)(b)) + (l)))
#define __is_expired(s, n, t) (!((n) - (s) < (t)))
#define __min(a, b) (((a) < (b)) ? (a) : (b))
#define __countof(x) (sizeof(x) / sizeof(x[0]))
#define __round(x) ((int)((x) < 0 ? (x)-0.5 : (x) + 0.5))
#define __offset(struc, member) ((int)(&(((struc *)0)->member)))
#define __strncpy(d, dn, s, sn) \
    do { \
        int l = __min(dn - 1, sn); \
        memcpy(d, s, l); \
        d[l] = 0; \
    } while (0)
#define __in_array(v, a) ((v) >= 0 && ((v) < __countof(a)))
#define __noop() \
    do { \
    } while (0)

#define __unused(v) (void)v

#define __for_each(i, a, p) for (i = 0, p = &a[i]; i < __countof(a); (i)++, p = &a[i])

time_t get_monotonic_ms();
unsigned long long get_ms_from_ts(const unsigned long long *ts);
time_t get_ctime_from_ts(unsigned long long *ts);

#endif
