#ifndef _RDB_OPS_H
#define _RDB_OPS_H
/**
 * @file rdb_ops.h
 *
 * Fake implementation of rdb_ops.h for testing without RDB.
 *
 *//*
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
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
#ifndef FAKE_RDB
#error "Wrong rdb_ops.h file included!"
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

static char RDB_GETNAMES_DELIMITER_STR[] = "&";
static int TRIGGERED = 0x0008;

static const size_t _MAX_RDBS = 32;
struct rdb_session {
    int fd;
    char *vars[_MAX_RDBS];
    size_t count;
};

static struct rdb_session s_fakeRdbSession = {1, {NULL}, 0};

static uint8_t _getRand()
{
    uint8_t result = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, &result, sizeof(result));
        close(fd);
    }
    return result;
}


static inline int rdb_open(const char *, struct rdb_session **s)
{
    if (s) {
        *s = &s_fakeRdbSession;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static inline void rdb_close(struct rdb_session **s)
{
    if (s && (*s == &s_fakeRdbSession)) {
        for (size_t index = 0; index < _MAX_RDBS ; ++index) {
            if (s_fakeRdbSession.vars[index]) {
                free(s_fakeRdbSession.vars[index]);
                s_fakeRdbSession.vars[index] = NULL;
            }
        }
        s_fakeRdbSession.count = 0;
        *s = NULL;
    }
}

static inline int rdb_getnames(struct rdb_session *s, const char* szName, char* pValue, int* pLen, int nFlags)
{
    if (s && (s == &s_fakeRdbSession) && szName && pValue && pLen) {
        // Clear data from read
        struct pollfd rdbPoll = {s_fakeRdbSession.fd, POLLIN, 0};
        while (poll(&rdbPoll, 1, 0) > 0) {
            char buffer[128];
            read(s_fakeRdbSession.fd, &buffer, sizeof(buffer));
        }
        // Randomly select names to notify
        size_t offset = 0;
        size_t maxSize = (size_t)*pLen;
        for (size_t index = 0; index < _MAX_RDBS ; ++index) {
            if (s_fakeRdbSession.vars[index]) {
                if ((_getRand() % 3) == 0) {
                    offset += snprintf(pValue + offset, maxSize - offset, offset ? "&%s" : "%s", s_fakeRdbSession.vars[index]);
                }
            }
        }
        pValue[*pLen - 1] = '\0';
        *pLen = (int)offset;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static inline int rdb_fd(struct rdb_session *s)
{
    if (s && (s == &s_fakeRdbSession)) {
        return s_fakeRdbSession.fd;
    }
    return -1;
}

static inline int rdb_subscribe(struct rdb_session *s, const char* szName)
{
    if (s && (s == &s_fakeRdbSession) && szName && (s_fakeRdbSession.count < _MAX_RDBS)) {
        for (size_t index = 0; index < _MAX_RDBS ; ++index) {
            if (!s_fakeRdbSession.vars[index]) {
                s_fakeRdbSession.vars[index] = strdup(szName);
                ++s_fakeRdbSession.count;
                return 0;
            }
        }
        return 0;
    }
    errno = EINVAL;
    return -1;
}

int rdb_unsubscribe(struct rdb_session *s, const char* szName)
{
    if (s && (s == &s_fakeRdbSession) && szName && (s_fakeRdbSession.count > 0)) {
        for (size_t index = 0; index < _MAX_RDBS ; ++index) {
            if (s_fakeRdbSession.vars[index] && (strcmp(s_fakeRdbSession.vars[index], szName) == 0)) {
                free(s_fakeRdbSession.vars[index]);
                s_fakeRdbSession.vars[index] = NULL;
                --s_fakeRdbSession.count;
                return 0;
            }
        }
    }
    errno = EINVAL;
    return -1;
}

static inline int rdb_get(struct rdb_session *s, const char* szName, char* pValue, int *nLen)
{
    if (s && (s == &s_fakeRdbSession) && szName && pValue && nLen) {
        // Randomly select a value for this RDB
        int written = 0;
        switch (_getRand() % 6) {
            case 0:
                written = snprintf(pValue, *nLen, "on");
                break;
            case 1:
                written = snprintf(pValue, *nLen, "red@01:100/1000");
                break;
            case 2:
                written = snprintf(pValue, *nLen, "ffa07a@10:100/1000");
                break;
            case 3:
                written = snprintf(pValue, *nLen, "blue@10111000:100/0");
                break;
            case 4:
                written = snprintf(pValue, *nLen, "green");
                break;
            case 5:
                written = snprintf(pValue, *nLen, "off");
                break;
            default:
                written = snprintf(pValue, *nLen, "");
                break;
        }
        pValue[*nLen - 1] = '\0';
        *nLen = written;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static inline int rdb_create(struct rdb_session *s, const char* szName, const char* szValue, int nLen, int nFlags, int nPerm)
{
    if (s && (s == &s_fakeRdbSession) && szName) {
        return 0;
    }
    errno = EINVAL;
    return -1;
}

#endif // _RDB_OPS_H
