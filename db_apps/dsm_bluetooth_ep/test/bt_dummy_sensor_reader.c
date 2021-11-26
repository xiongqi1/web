/*
 * A sample user defined executable (UDE) that interacts with the dummy sensor.
 * It simply sends a "read\n" command followed by a "bye\n" command.
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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

#include "dsm_bt_utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#define MAX_RESULT_LEN 64

/*
 * Test if a string contains all white spaces (space, tab, newline etc)
 * Return 1 for yes, and 0 for no.
 */
static int
is_empty (const char *s)
{
    while (*s) {
        if (!isspace(*s)) {
            return 0;
        }
        s++;
    }
    return 1;
}

/*
 * Read stdin until reaching a new line (inclusive) or len exceeded.
 * EAGAIN will be handled by retry with 0.1s sleep.
 * Empty lines will be ignored.
 */
#define RETRY_WAIT 100000000L
#define MAX_RETRIES 50
static int
read_a_line (char *buf, int len)
{
    int retries = 0;
    struct timespec req = { 0, RETRY_WAIT }, rem;
    do {
        if (!fgets(buf, len, stdin)) {
            if (errno == EAGAIN) {
                buf[0] = '\0';
                nanosleep(&req, &rem);
                continue;
            }
            return -errno;
        }
    } while (is_empty(buf) && ++retries < MAX_RETRIES);
    return retries < MAX_RETRIES ? 0 : -1;
}

static int
read_sensor (void)
{
    char buf[MAX_RESULT_LEN];
    int rval;

    if(fputs("read\n", stdout) < 0) {
        errp("Failed to write request\n");
        return -1;
    }

    rval = read_a_line(buf, sizeof buf);
    if (rval) {
        errp("Failed to read response: %d\n", rval);
        return rval;
    }

    syslog(LOG_INFO, "sensor read: %s", buf);
    return 0;
}

static int
disconnect_sensor (void)
{
    char buf[MAX_RESULT_LEN];
    int rval;

    if(fputs("bye\n", stdout) < 0) {
        errp("Failed to write request\n");
        return -1;
    }

    rval = read_a_line(buf, sizeof buf);
    if (rval) {
        errp("Failed to read response %d\n", rval);
        return rval;
    }
    syslog(LOG_INFO, "sensor responds: %s", buf);
    return 0;
}

int
main (void)
{
    int rval;

    dbgp("bt_dummy_sensor_reader started\n");

    /* set line buffer mode */
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

    dbgp("reading sensor\n");
    rval = read_sensor();
    if (rval) {
        errp("Failed to read sensor\n");
    }

    dbgp("disconnecting sensor\n");
    rval = disconnect_sensor();
    if (rval) {
        errp("Failed to disconnect from sensor\n");
    }

    dbgp("bt_dummy_sensor_reader ended\n");
    return rval;
}
