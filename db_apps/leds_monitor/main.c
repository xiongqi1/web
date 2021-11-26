/**
 * @file main.c
 *
 * Main file for process/daemon that maps RDB variables to LED settings.
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
#include "leds_config.h"
#include "leds_handler.h"
#include "leds_rdb.h"
#include "leds_sysfs.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>

/*
 * File statics
 */
bool s_terminate = false;  ///< Termination flag for use in monitoring mode


/**
 * Show usage text
 * 
 * @param name Name of the program to use in the usage text.
 */
static void showUsage(const char *name)
{
    printf("%s - sets /sys/class/leds based on configured RDB values.\n\n", name);
    printf("Usage:\n");
    printf("  %s [option] monitor\n", name);
    printf("  %s [option] update\n", name);
    printf("  %s [option] set <rdbName> <rdbValue>\n", name);
    printf("  %s [option] reset [rdbName]\n", name);
    printf("  %s [option] list\n\n", name);
    printf("Commands:\n"
            "  monitor\n"
            "    Keep running and do not exit, monitoring RDB values and updating their LEDs as needed.\n"
            "    This is the default if no other command is given.\n"
            "  update\n"
            "    Check known RDB values, update the corresponding LEDs as needed, then exit.\n"
            "  set <rdbName> <rdbValue>\n"
            "    Set the LED corresponding to the given rdbName using the given rdbValue (ignores current RDB) then exit\n"
            "  reset [rdbName]\n"
            "    Reset the LEDs for the given RDB (or all LEDs if no rdbName given) to its default/initial value (ignores current RDB) then exit\n"
            "  list\n"
            "    Show known RDB values and their LEDs to stdout, then exit\n\n");
    printf("Options\n"
            "  --help\n"
            "    Show this usage information and exit without acting\n"
            "  --nosyslog\n"
            "    Log output to stdout instead of syslog\n\n");
}

/**
 * Update LEDs based on given RDB name
 *
 * @param rdbName Name of RDB variable to perform update on.
 * @return true if update was successful, false otherwise.
 */
static bool updateByName(const char *rdbName)
{
    const char *rdbValue = getRdb(rdbName);
    if (rdbValue) {
        return doLedUpdate(rdbName, rdbValue);
    }
    return false;
}

/**
 * Start monitoring a given RDB, and update the LED value for it
 *
 * @param rdbName Name of RDB variable to start monitoring.
 * @return true if monitoring was started successfully, false otherwise.
 */
static bool startMonitorByName(const char *rdbName)
{
    const char *rdbValue = getRdb(rdbName);
    if (subscribeRdb(rdbName)) {
        if (rdbValue) {
            return doLedUpdate(rdbName, rdbValue);
        }
        return doLedReset(rdbName);
    }
    logError("Could not subscribe to RDB %s - error %d (%s)", rdbName, errno, strerror(errno));
    return false;
}

/**
 * Stop monitoring a given RDB
 *
 * @param rdbName Name of RDB variable to stop monitoring.
 * @return true always
 * @return true if update was successful, false otherwise.
 */
static bool stopMonitorByName(const char *rdbName)
{
    if (unsubscribeRdb(rdbName)) {
        return true;
    }
    logError("Could not unsubscribe from RDB %s - error %d (%s)", rdbName, errno, strerror(errno));
    return false;
}

/**
 * Reset LEDs based on given RDB name
 *
 * @param rdbName Name of RDB variable to perform reset on.
 * @return true if reset was successful, false otherwise.
 */
static bool resetByName(const char *rdbName)
{
    return doLedReset(rdbName);
}

/**
 * List LEDs based on given RDB name
 *
 * @param rdbName Name of RDB variable to perform listing of.
 * @return true if RDB is known, false otherwise.
 */
static bool listByName(const char *rdbName)
{
    const ledMap_t *p_ledMap = getLedMapByName(rdbName);
    if (!p_ledMap) {
        logError("RDB name \"%s\" not found", rdbName);
        return false;
    }

    printf("%s\n", p_ledMap->name);
    const char *rdbValue = getRdb(rdbName);
    if (rdbValue) {
        printf("  value = \"%s\"\n", rdbValue);
    }
#ifdef FAKE_SYSFS
    const char *present = "fake";
#else
    const char *present = "present";
#endif
    if (p_ledMap->reset && (*p_ledMap->reset != '\0')) {
        printf("  reset = \"%s\"\n", p_ledMap->reset);
    }
    if (p_ledMap->ledR && (*p_ledMap->ledR != '\0')) {
        printf("  ledR @ %s/%s (%s)\n", SYSFS_PATH_LEDS, p_ledMap->ledR,
                checkSysLed(p_ledMap->ledR) ? present : "missing");
    }
    if (p_ledMap->ledG && (*p_ledMap->ledG != '\0')) {
        printf("  ledG @ %s/%s (%s)\n", SYSFS_PATH_LEDS, p_ledMap->ledG,
                checkSysLed(p_ledMap->ledG) ? present : "missing");
    }
    if (p_ledMap->ledB && (*p_ledMap->ledB != '\0')) {
        printf("  ledB @ %s/%s (%s)\n", SYSFS_PATH_LEDS, p_ledMap->ledB,
                checkSysLed(p_ledMap->ledB) ? present : "missing");
    }
    return true;
}

/**
 * Signal handler
 *
 * @param signum Signal ID of the received signal.
 */
static void signalHandler(int signum)
{
    if ((signum == SIGTERM) || (signum == SIGINT) || (signum == SIGQUIT)) {
        logNotice("Caught termination signal %d", signum);
        s_terminate = true;
    }
    else {
        logNotice("Ignoring signal %d", signum);
    }
}

/**
 * Install signal handler
 */
static void installSignalHandler()
{
    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGTERM, &action, NULL) != 0) {
        logError("Could not install SIGTERM handler - error %d (%s)", errno, strerror(errno));
    }
    if (sigaction(SIGINT, &action, NULL) != 0) {
        logError("Could not install SIGINT handler - error %d (%s)", errno, strerror(errno));
    }
    if (sigaction(SIGQUIT, &action, NULL) != 0) {
        logError("Could not install SIGQUIT handler - error %d (%s)", errno, strerror(errno));
    }
}


/**
 * Program commands
 */
typedef enum {
    COMMAND_UPDATE,         ///< Update LEDs based on current RDB values, then exit
    COMMAND_MONITOR,        ///< Keep watching RDB values and update LEDs as needed (does not exit)
    COMMAND_SET,            ///< Ignore RDB, set per command line (run and exit)
    COMMAND_RESET,          ///< Ignore RDB, reset per defaults (run and exit)
    COMMAND_LIST            ///< List known RDBs and LEDs (run and exit)
} command_e;

/**
 * Program main
 */
int main(int argc, char **argv)
{
    command_e command = COMMAND_MONITOR;
    const char *rdbName = NULL;
    const char *rdbValue = NULL;

    const char *baseName = strrchr(argv[0], '/');
    if (baseName) {
        ++baseName;
    }
    else {
        baseName = argv[0];
    }

    int index = 1;
    while (index < argc) {
        if (strcmp(argv[index], "monitor") == 0) {
            command = COMMAND_MONITOR;
        }
        else if (strcmp(argv[index], "update") == 0) {
            command = COMMAND_UPDATE;
        }
        else if (strcmp(argv[index], "set") == 0) {
            command = COMMAND_SET;
            if ((index + 2) < argc) {
                ++index;
                rdbName = argv[index];
                ++index;
                rdbValue = argv[index];
            }
            else {
                fprintf(stderr, "Command \"%s\" requires two arguments\n", argv[index]);
                exit(1);
            }
        }
        else if (strcmp(argv[index], "reset") == 0) {
            command = COMMAND_RESET;
            if ((index + 1) < argc) {
                ++index;
                rdbName = argv[index];
            }
        }
        else if (strcmp(argv[index], "list") == 0) {
            command = COMMAND_LIST;
        }
        else if (strcmp(argv[index], "--nosyslog") == 0) {
            loggerConfig(LOG_OUTPUT_STDOUT);
        }
        else if (strcmp(argv[index], "--help") == 0) {
            showUsage(baseName);
            exit(0);
        }
        else {
            fprintf(stderr, "Unknown command \"%s\"\n", argv[index]);
            showUsage(baseName);
            exit(1);
        }
        ++index;
    }

    // Initialise
    initRdb();

    if (command == COMMAND_UPDATE) {
        iterateLedMapByName(updateByName);
    }
    else if (command == COMMAND_MONITOR) {
        iterateLedMapByName(startMonitorByName);
    }
    else if (command == COMMAND_SET) {
        doLedUpdate(rdbName, rdbValue);
    }
    else if (command == COMMAND_RESET) {
        if (rdbName) {
            if (!doLedReset(rdbName)) {
                logError("RDB name \"%s\" not found", rdbName);
            }
        }
        else {
            iterateLedMapByName(resetByName);
        }
    }
    else if (command == COMMAND_LIST) {
        iterateLedMapByName(listByName);
    }
    else {
        logError("Unhandled command %d", command);
    }

    if (command == COMMAND_MONITOR) {
        // Catch signals for graceful termination
        installSignalHandler();

        logDebug("Starting monitoring");
        while (!s_terminate) {
            const char *rdbName = waitForRdb();
            if (rdbName) {
                logDebug("waitForRdb returned \"%s\"", rdbName);
                updateByName(rdbName);
            }
            else {
                logDebug("waitForRdb returned NULL");
                sched_yield();
            }
        }

        // Stop monitoring
        logDebug("Stopping monitoring");
        iterateLedMapByName(stopMonitorByName);
    }

    // Done
    releaseRdb();
    return 0;
}

