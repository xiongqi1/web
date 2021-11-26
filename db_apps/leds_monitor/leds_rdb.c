/**
 * @file leds_rdb.c
 *
 * Implementation for LED RDB interface.
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
#include "logger.h"
#include "leds_rdb.h"

#include <rdb_ops.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <poll.h>

/*
 * Constants
 */
static const size_t RDB_VALUE_MAX_LEN = 256;

/*
 * Statics
 */
static size_t s_rdbNamesLen = 0;
static struct rdb_session* s_rdbSession = NULL;

static char s_valueBuffer[RDB_VALUE_MAX_LEN];
static size_t s_nameBufferLen = 0;
static char *s_nameBuffer = NULL;
static char *s_nameBufferSavePtr = NULL;


/*
 * initRdb
 */
void initRdb()
{
    if (!s_rdbSession) {
        if ((rdb_open(NULL, &s_rdbSession) < 0) || !s_rdbSession) {
            logError("Could not open RDB session - error %d (%s)", errno, strerror(errno));
        }
    }
}

/*
 * releaseRdb
 */
void releaseRdb()
{
    if (s_nameBuffer) {
        free(s_nameBuffer);
        s_nameBuffer = NULL;
        s_nameBufferLen = 0;
    }
    s_nameBufferSavePtr = NULL;
    if (s_rdbSession) {
        rdb_close(&s_rdbSession);
    }
}

/*
 * getRdb
 */
const char *getRdb(const char *rdbName)
{
    if (!s_rdbSession) {
        logError("RDB session not initialised");
        return NULL;
    }
    int valueLen = (int)(sizeof(s_valueBuffer) - 1);
    int result = rdb_get(s_rdbSession, rdbName, s_valueBuffer, &valueLen);
    if (result == 0) {
        s_valueBuffer[valueLen] = '\0';
        return s_valueBuffer;
    }
    errno = -result;
    return NULL;
}

/*
 * subscribeRdb
 */
bool subscribeRdb(const char *rdbName)
{
    if (!s_rdbSession) {
        logError("RDB session not initialised");
        return false;
    }

    // Try creating RDB first
    int result = rdb_create(s_rdbSession, rdbName, "", 0, 0, 0);
    if ((result != 0) && (result != -EEXIST)) {
        // could not create RDB
        errno = -result;
        return false;
    }

    // Then subscribe
    result = rdb_subscribe(s_rdbSession, rdbName);
    if (result != 0) {
        // could not subscribe
        errno = -result;
        return false;
    }

    // Adjust next s_nameBuffer length
    s_rdbNamesLen += strlen(rdbName) + 1;

    return true;
}

/*
 * unsubscribeRdb
 */
bool unsubscribeRdb(const char *rdbName)
{
    if (!s_rdbSession) {
        logError("RDB session not initialised");
        return false;
    }

    // Unsubscribe
    int result = rdb_unsubscribe(s_rdbSession, rdbName);
    if (result != 0) {
        // could not unsubscribe
        errno = -result;
        return false;
    }

    // Adjust next s_nameBuffer length
    size_t nameLen = strlen(rdbName) + 1;
    assert(nameLen <= s_rdbNamesLen);
    if (nameLen <= s_rdbNamesLen) {
        s_rdbNamesLen -= nameLen;
    }

    return true;
}

/*
 * waitForRdb
 */
const char *waitForRdb(uint32_t timeoutMsec)
{
    if (!s_rdbSession) {
        logError("RDB session not initialised");
        return NULL;
    }

    // Any more names in current buffer?
    if (s_nameBufferSavePtr) {
        const char *nextToken = strtok_r(NULL, RDB_GETNAMES_DELIMITER_STR, &s_nameBufferSavePtr);
        if (nextToken) {
            return nextToken;
        } else {
            s_nameBufferSavePtr = NULL;
        }
    }

    // Resize buffer if needed for newly added subscriptions
    if (s_nameBufferLen != s_rdbNamesLen) {
        free(s_nameBuffer);
        s_nameBuffer = NULL;
        s_nameBufferLen = s_rdbNamesLen;
        if (s_nameBufferLen > 0) {
            s_nameBuffer = (char *)malloc(s_nameBufferLen + 1);
            if (!s_nameBuffer) {
                logError("Could not allocate name buffer (size %u)", s_nameBufferLen + 1);
                return NULL;
            }
            s_nameBuffer[0] = '\0';
        }
    }
    if (!s_nameBuffer) {
        logError("waitForRdb called with no RDB variables subscribed");
        return NULL;
    }

    // Wait for an update
    struct pollfd rdbPoll = {rdb_fd(s_rdbSession), POLLIN, 0};
    int result = poll(&rdbPoll, 1, (int)timeoutMsec);
    if (result < 0) {
        // Interrupt or error
        if (errno != EINTR) {
            logError("waitForRdb poll returned error %d (%s)", errno, strerror(errno));
        }
        logDebug("waitForRdb poll call interrupted");
        return NULL;
    }
    else if (result == 0) {
        // Timeout
        logDebug("waitForRdb timed out");
        return NULL;
    }

    // Get triggered names
    int bufferLen = (int)s_nameBufferLen;
    if (rdb_getnames(s_rdbSession, "", s_nameBuffer, &bufferLen, TRIGGERED) == 0) {
        s_nameBuffer[bufferLen] = '\0';
        logDebug("waitForRdb rdb_getnames returned \"%s\"", s_nameBuffer);
        // Return first name
        return strtok_r(s_nameBuffer, RDB_GETNAMES_DELIMITER_STR, &s_nameBufferSavePtr);
    }

    // Nothing (unexpectedly) pending
    logDebug("waitForRdb call to rdb_getnames failed");
    return NULL;
}

