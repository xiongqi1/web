/**
 * @file leds_handler.c
 *
 * Implementation for LED sysfs interfaces.
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
#include "leds_sysfs.h"

#include <assert.h>
#include <stdio.h>

#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

/*
 * Constants
 */
static const size_t SYSFS_PATH_MAX = 128;

/**
 * Open a SYSFS entry for writing
 *
 * @param ledName Name of the LED in the /sys/class/leds file system.
 * @param entry Name of the entry to open for writing in the /sys/class/leds/<ledName> directory.
 * @param timeoutMsec Timeout in msec to wait for the entry to be writeable.
 * @return file descriptor for the opened file, or -1 if timed out while waiting.
 */
static int openWrite(const char *ledName, const char *entry, uint32_t timeoutMsec)
{
    char path[SYSFS_PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/%s/%s", SYSFS_PATH_LEDS, ledName, entry) >= (ssize_t)sizeof(path)) {
        logWarning("path length %u exceeded for %s/%s/%s", sizeof(path), SYSFS_PATH_LEDS, ledName, entry);
        path[sizeof(path) - 1] = '\0';
    }
#ifdef FAKE_SYSFS
    int fd = open("/dev/null", O_WRONLY);
#else
    int fd = open(path, O_WRONLY);
#endif
    if (fd < 0) {
        if (timeoutMsec > 0) {
            struct timespec start, now;
            uint32_t elapsedMsec = 0;

            clock_gettime(CLOCK_MONOTONIC_COARSE, &start);
            do {
                sched_yield();

                fd = open(path, O_WRONLY);
                if (fd < 0) {
                    clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
                    elapsedMsec = (uint32_t)((now.tv_sec - start.tv_sec)*1000 + (now.tv_nsec - start.tv_nsec)/1000000);
                }
                else {
                    break;
                }
            }
            while (elapsedMsec < timeoutMsec);
        }
    }
    return fd;
}


/*
 * setSysLed (for string)
 */
bool setSysLed(const char *ledName, const char *entry, const char *value, uint32_t timeoutMsec)
{
    assert(ledName);
    assert(entry);

    int fd = openWrite(ledName, entry, timeoutMsec);
    if (fd >= 0) {
        // assume all bytes will be written (ignore return value)
        dprintf(fd, "%s\n", value);
        close(fd);
        return true;
    }
    logWarning("Could not open %s/%s/%s for writing", SYSFS_PATH_LEDS, ledName, entry);
    return false;
}

/*
 * setSysLed (for int)
 */
bool setSysLed(const char *ledName, const char *entry, int value, uint32_t timeoutMsec)
{
    assert(ledName);
    assert(entry);

    int fd = openWrite(ledName, entry, timeoutMsec);
    if (fd >= 0) {
        // assume all bytes will be written (ignore return value)
        dprintf(fd, "%d\n", value);
        close(fd);
        return true;
    }
    logWarning("Could not open %s/%s/%s for writing", SYSFS_PATH_LEDS, ledName, entry);
    return false;
}

/*
 * checkSysLed
 */
bool checkSysLed(const char *ledName, const char *entry, uint32_t timeoutMsec)
{
    assert(ledName);
    assert(entry);

    int fd = openWrite(ledName, entry, timeoutMsec);
    if (fd >= 0) {
        close(fd);
        return true;
    }
    return false;
}

