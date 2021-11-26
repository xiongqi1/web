// Copyright 2017-2019 Paul Nettle
//
// This file is part of Gobbledegook.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is an example single-file stand-alone application that runs a Gobbledegook server.
//
// >>
// >>>  DISCUSSION
// >>
//
// Very little is required ("MUST") by a stand-alone application to instantiate a valid Gobbledegook server. There are also some
// things that are reocommended ("SHOULD").
//
// * A stand-alone application MUST:
//
//     * Start the server via a call to `ggkStart()`.
//
//         Once started the server will run on its own thread.
//
//         Two of the parameters to `ggkStart()` are delegates responsible for providing data accessors for the server, a
//         `GGKServerDataGetter` delegate and a 'GGKServerDataSetter' delegate. The getter method simply receives a string name (for
//         example, "battery/level") and returns a void pointer to that data (for example: `(void *)&batteryLevel`). The setter does
//         the same only in reverse.
//
//         While the server is running, you will likely need to update the data being served. This is done by calling
//         `ggkNofifyUpdatedCharacteristic()` or `ggkNofifyUpdatedDescriptor()` with the full path to the characteristic or delegate
//         whose data has been updated. This will trigger your server's `onUpdatedValue()` method, which can perform whatever
//         actions are needed such as sending out a change notification (or in BlueZ parlance, a "PropertiesChanged" signal.)
//
// * A stand-alone application SHOULD:
//
//     * Shutdown the server before termination
//
//         Triggering the server to begin shutting down is done via a call to `ggkTriggerShutdown()`. This is a non-blocking method
//         that begins the asynchronous shutdown process.
//
//         Before your application terminates, it should wait for the server to be completely stopped. This is done via a call to
//         `ggkWait()`. If the server has not yet reached the `EStopped` state when `ggkWait()` is called, it will block until the
//         server has done so.
//
//         To avoid the blocking behavior of `ggkWait()`, ensure that the server has stopped before calling it. This can be done
//         by ensuring `ggkGetServerRunState() == EStopped`. Even if the server has stopped, it is recommended to call `ggkWait()`
//         to ensure the server has cleaned up all threads and other internals.
//
//         If you want to keep things simple, there is a method `ggkShutdownAndWait()` which will trigger the shutdown and then
//         block until the server has stopped.
//
//     * Implement signal handling to provide a clean shut-down
//
//         This is done by calling `ggkTriggerShutdown()` from any signal received that can terminate your application. For an
//         example of this, search for all occurrences of the string "signalHandler" in the code below.
//
//     * Register a custom logging mechanism with the server
//
//         This is done by calling each of the log registeration methods:
//
//             `ggkLogRegisterDebug()`
//             `ggkLogRegisterInfo()`
//             `ggkLogRegisterStatus()`
//             `ggkLogRegisterWarn()`
//             `ggkLogRegisterError()`
//             `ggkLogRegisterFatal()`
//             `ggkLogRegisterAlways()`
//             `ggkLogRegisterTrace()`
//
//         Each registration method manages a different log level. For a full description of these levels, see the header comment
//         in Logger.cpp.
//
//         The code below includes a simple logging mechanism that logs to stdout and filters logs based on a few command-line
//         options to specify the level of verbosity.
//
// >>
// >>>  Building with GOBBLEDEGOOK
// >>
//
// The Gobbledegook distribution includes this file as part of the Gobbledegook files with everything compiling to a single, stand-
// alone binary. It is built this way because Gobbledegook is not intended to be a generic library. You will need to make your
// custom modifications to it. Don't worry, a lot of work went into Gobbledegook to make it almost trivial to customize
// (see Server.cpp).
//
// If it is important to you or your build process that Gobbledegook exist as a library, you are welcome to do so. Just configure
// your build process to build the Gobbledegook files (minus this file) as a library and link against that instead. All that is
// required by applications linking to a Gobbledegook library is to include `include/Gobbledegook.h`.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <sys/inotify.h>

#include "Gobbledegook.h"

#include <string.h>
#include <unistd.h>
#include <glib.h>

#include "Housekeeping.h"
#include "Logger.h"

#include "CasaData.h" // our data setters/getters and randomiser
#include "CasaDbus.h" // our custom DBUS queries
#include "uuids.h" // our custom UUIDs

// JSON library from https://github.com/nlohmann/json
#include "json.hpp"

#include "elogger.hpp"
#include "erdb.hpp"
#include "erdbsession.hpp"

// for convenience
using json = nlohmann::json;

// TODO:: The definition is on the source code for tight firmware release schedule.
//        This should be moved to configuration on build system.
// Feature to enable or disable Bonding timeout.
#define CONFIGURE_BONDING_TIMEOUT

//
// Entry point
//

#define GATT_SHORT_ADV_NAME_LEN 10 // Length of short advertising name
#define PAIRING_GRACE_TIME  20  // When bonding window is open, how many seconds to allow unpaired connections (waiting for user to click "Paired")
#define BONDING_WINDOW_TIME 600 // How long after startup to keep bonding window open - production value should be 600 seconds

#define INOTIFY_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

int main(int argc, char **ppArgv)
{
    unsigned int timerCounter=0;   // seconds counter for doing stuff (eg regen test data every x seconds)
    bool testMode=false;   // for generating random test data
    std::chrono::steady_clock::time_point bondingTime, graceTime, currentTime;
    int startupBondingTimer= 0;
    int graceTimer = 0;
    char message[120];

    // Set test data with file(inotify)
    int inotifyFd = -1; // inotify File descriptor
    int inotifyWd = -1; // inotify Watch descriptor
    const char* inotifyConf = nullptr;
    char inotifyBuf[INOTIFY_LEN];
    int numRead;
    struct inotify_event *event;
    bool enConfData = false;

    int sessionFd, rc;
    struct timeval tv;
    bool isBondable = true;
    int syslogLevel = LOG_ERR;
#ifdef CONFIGURE_BONDING_TIMEOUT
    bool disabledBindingTimeout = false; // flag is true, disable BONDING_WINDOW_TIME timer.
#endif

    int optc;

    /* parse parameters */
    while (((optc = getopt(argc, ppArgv, "qvdtnj:l:"))) != -1) {
        switch(optc) {
            case 'q':
                logLevel = ErrorsOnly;
                break;
            case 'v':
                logLevel = Verbose;
                break;
            case 'd':
                logLevel = Debug;
                break;
            case 't':
                LogWarn("Starting in TEST MODE");
                testMode = true;
                break;
            case 'n':
                LogWarn("Disabling DAS BOOT");
                CasaDbus::niceMode = true;
                break;
            case 'j':
                LogWarn("Configure test data with a json file");
                inotifyConf = optarg;
                break;
            case 'l':
                try {
                    syslogLevel = std::stoi(optarg);
                    if (syslogLevel < 0) {
                        syslogLevel = 0;
                    } else if (syslogLevel > 4) {
                        syslogLevel = 4;
                    }
                }
                catch(std::exception &ex){
                    syslogLevel = 0;
                }
                syslogLevel += LOG_ERR;
                break;
            case 'h':
            default:
                LogFatal("");
                LogFatal("Usage: standalone [-q | -v | -d | -t | -n | -j]");
                LogFatal("\t-q: Error only log level");
                LogFatal("\t-v: Verbose log level");
                LogFatal("\t-d: Debug log level");
                LogFatal("\t-t: Start TEST MODE");
                LogFatal("\t-n: Disable DAS BOOT");
                LogFatal("\t-j file: Set test data with json file.(Default: randomized data)");
                LogFatal("\t-l number: Set syslog verbosity)");
                return -1;
        }
    }

    /*
    if (testMode == false)   // TODO - remove this branch when real data is available
    {
        LogFatal("You forgot to run in test mode\n");
        return -1;
    }
    */

    // change log level
    estd::Logger::getInstance().setup(syslogLevel);

    // Setup our signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Register our loggers
    ggkLogRegisterDebug(LogDebug);
    ggkLogRegisterInfo(LogInfo);
    ggkLogRegisterStatus(LogStatus);
    ggkLogRegisterWarn(LogWarn);
    ggkLogRegisterError(LogError);
    ggkLogRegisterFatal(LogFatal);
    ggkLogRegisterAlways(LogAlways);
    ggkLogRegisterTrace(LogTrace);

    log(LOG_DEBUG, "initiate RDB session");
    erdb::Session &rdbSess = erdb::Session::getInstance();

    // Start the server's ascync processing
    //
    // This starts the server on a thread and begins the initialization process
    //
    // !!!IMPORTANT!!!
    //
    //     This first parameter (the service name) must match tha name configured in the D-Bus permissions. See the Readme.md file
    //     for more information.
    //
    // Raspberry Pi has issues with long name, so we're sticking to short advertisements only for now (10 characters)
    const char *advName;
    char hostname[GATT_SHORT_ADV_NAME_LEN + 1];
    std::string rdbAdvName = erdb::Rdb("service.gatt.adv_name").get(true);

    if (testMode == true || rdbAdvName.empty()) {
        gethostname(hostname, GATT_SHORT_ADV_NAME_LEN);
        hostname[GATT_SHORT_ADV_NAME_LEN] = 0;   // null terminate!
        advName = hostname;
    } else {
        rdbAdvName.resize(GATT_SHORT_ADV_NAME_LEN);
        advName = rdbAdvName.data();
    }
    sprintf(message, "My hostname is %s", advName);
    LogInfo(message);
    if (!ggkStart("gobbledegook", advName, advName, dataGetter, dataSetter, kMaxAsyncInitTimeoutMS))
    {
        return -1;
    }

    if (testMode == true)
    {
        if (inotifyConf != nullptr) {
            inotifyFd = inotify_init1(IN_NONBLOCK); // Set the O_NONBLOCK file status flag
            if (inotifyFd >= 0) {
                inotifyWd = inotify_add_watch(inotifyFd, inotifyConf, IN_MODIFY);
                if (inotifyWd >= 0) {
                    if(setDataWithFile(inotifyConf)) {
                        enConfData = true;
                    }
                }
            }
        }
        if (enConfData)
            LogError("Set test data for the characteristics with a json data file.");
        else
            LogError("Set test data for the characteristics at random.");
    }

    graceTime = bondingTime = std::chrono::steady_clock::now();

#ifdef CONFIGURE_BONDING_TIMEOUT
    erdb::Rdb rdbTest = erdb::Rdb("nit.connected");
    rdbTest.subscribeForChange([&disabledBindingTimeout](erdb::Rdb &rdb) {
        if (rdb.get(true).toBool()) {
            log(LOG_ERR, "Install toool is attached");
            disabledBindingTimeout = true;
        } else {
            log(LOG_ERR, "Install toool is detached");
            disabledBindingTimeout = false;
        }
    }, false);
    if ( rdbTest.get(true).toBool() ) {
        disabledBindingTimeout = true;
    }
#endif

    // Do stuff
    while (ggkGetServerRunState() < EStopping)
    {
        if (testMode)
        {
            if (enConfData) {
                numRead = read(inotifyFd, inotifyBuf, INOTIFY_LEN);
                if (numRead > 0) {
                    for (char *ptr = inotifyBuf; ptr < inotifyBuf+ numRead; ) {
                        event = (struct inotify_event *) ptr;
                        if (event->wd == inotifyWd && ((event->mask & IN_MODIFY) || (event->mask & IN_IGNORED))) {
                            if(!setDataWithFile(inotifyConf)) {
                                LogError("Failed to parse json data file. Ignored");
                            }
                            if (event->mask & IN_IGNORED) { // some of editors(ex. vim) trigger IN_IGNORED event instead of IN_MODIFY
                                inotify_rm_watch(inotifyFd, inotifyWd);
                                inotifyWd = inotify_add_watch(inotifyFd, inotifyConf, IN_MODIFY);
                            }
                        }
                        ptr += sizeof(struct inotify_event) + event->len;
                    }
                }

            } else {
                if (!(timerCounter % 30)) generateRandomData();   // regen every 30 seconds
            }
            if (strlen(getVisibleCellRequest()))
            {
                sprintf(message, "Visible cell detail request for PCI %s, generating data", getVisibleCellRequest());
                LogStatus(message);
                // check for validity
                std::stringstream ss(getVisibleCellList());
                std::vector<int> cellList;
                std::string notiCont = "";
                int reqPci = atoi(getVisibleCellRequest());
                while (ss.good())
                {
                    std::string substr;
                    getline(ss, substr, ',');
                    cellList.push_back(atoi(substr.c_str()));
                }
                if (std::find(cellList.begin(), cellList.end(), reqPci) < cellList.end())
                {
                    int position = std::distance(cellList.begin(), std::find(cellList.begin(), cellList.end(), reqPci));
                    sprintf(message, "Found PCI at position %d\n", position);
                    LogStatus(message);
                    if (enConfData) {
                        std::string cellInfo = getVisibleCellDetails();
                        json jCellInfo = json::parse(cellInfo);
                        if (jCellInfo.is_array()) {
                            for(auto elem: jCellInfo) {
                                if(std::stoi(elem["PCI"].dump()) == reqPci) {
                                    notiCont = elem.dump();
                                    break;
                                }
                            }
                        } else {
                            if(std::stoi(jCellInfo["PCI"].dump()) == reqPci)
                                notiCont = cellInfo;
                        }
                    } else {
                        static const std::string roles[] = { "P", "S", "N" };
                        json j;
                        j["Cell Type"] = "??";
                        j["Cell Technology"] = "??";
                        j["Cell ID"] = random() % 65535;
                        j["PCI"] = reqPci;
                        j["EARFCN"] = random() % 65535;
                        j["RSRP"] = (int)(random() % 80) - 40;
                        j["RSRQ"] = (int)(random() % 12) - 8;
                        j["SINR"] = (int)(random() % 45) - 5;
                        j["Role"] = roles[position % (sizeof(roles)/sizeof(roles[0]))];
                        notiCont = j.dump();
                    }
                }
                setVisibleCellInfoNoti(notiCont);
                ggkNofifyUpdatedCharacteristic("/com/gobbledegook/visibleCellService/visibleCellInfoCharacteristic");
                // TODO - invalid requests keep outputting the "generating data" message
                clearVisibleCellRequest();
            }
            if (checkSpeedTestRequest() && (!(timerCounter % 10)))   // speed test response within 10 seconds
            {
                LogStatus("Speed test requested - generating results");
                if (enConfData) { // just notify with current data.
                    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/speedTestService/speedTestResultCharacteristic");
                    clearspeedTestRequest();
                } else {
                    const std::string experience[] = { "Excellent", "Good", "Average", "Not great" };
                    setSpeedTestResult(random() % 1400 + 400, random() % 600 + 200, random() % 20 + 20, \
                            experience[random() % (sizeof(experience)/sizeof(experience[0]))]);
                }
            }
        }
        else
        {
            // TODO - NTC/CASA real stuff goes here!
            ::fd_set rfds;

            FD_ZERO(&rfds);
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            sessionFd = rdbSess.setfdSet(rfds);
            rc = ::select(sessionFd + 1, &rfds, NULL, NULL, &tv);
            if (rc > 0)
            {
                try {
                    rdbSess.processSubscription(rfds);
                }
                catch(std::exception &ex){
                    log(LOG_DEBUG, "Error: processSubscription: %s", ex.what());
                }
            }
        }

        // This is a check to prevent accidental/nuisance connections from tying up the peripheral.
        if (!CasaDbus::niceMode) ggkCheckDasBoot();

        currentTime = std::chrono::steady_clock::now();

#ifdef CONFIGURE_BONDING_TIMEOUT
        if (disabledBindingTimeout) {
            bondingTime = currentTime; // reset bondingTime
        }
#endif

        startupBondingTimer = std::chrono::duration_cast<std::chrono::duration<int>>(currentTime - bondingTime).count();

        if ( startupBondingTimer < BONDING_WINDOW_TIME)
        {
            if (!isBondable) {
                log(LOG_NOTICE, "Enable BT Connectable/Bonding");
                ggkSetConnectable(true);
                ggkSetBonding(true);
                isBondable = true;
            }
            if (!ggkGetActiveConnections())
            {
                graceTime = currentTime;
                LogInfo("No active connections, bonding open, resetting grace timer");
            }
        }
        else
        {
            if (isBondable) {
                log(LOG_ERR, "BONDING_WINDOW_TIMER(%d seconds) expired, device binding is not allowed.", BONDING_WINDOW_TIME);
                log(LOG_NOTICE, "Disable BT Connectable/Bonding");
                ggkSetBonding(false); // good idea to keep disabling it, in case someone else fiddles with the knob
                ggkSetConnectable(false); // Disable connectable. Otherwise, a peer keeps retrying to connect a GATT server.
                isBondable = false;
            }
            graceTime = currentTime - std::chrono::seconds(PAIRING_GRACE_TIME + 1);
        }

        graceTimer = std::chrono::duration_cast<std::chrono::duration<int>>(currentTime - graceTime).count();
        sprintf(message, "Server running, active connections: %d, counter=%u, bondable=%s grace=%d", ggkGetActiveConnections(),\
                timerCounter, startupBondingTimer < BONDING_WINDOW_TIME ? "true" : "false", graceTimer);
        LogInfo(message);

        if (!CasaDbus::niceMode && CasaDbus::checkConnectionsForPairing(false))
        {
            if (!ggkGetActiveConnections())
            {
                LogWarn("unpaired connection detected but no connections..  this is weird, power cycle time!");
                ggkSetDasBootFlag();
            }
            if (graceTimer >= PAIRING_GRACE_TIME)
            {
                if ( startupBondingTimer < BONDING_WINDOW_TIME) {
                    log(LOG_ERR, "PAIRING_GRACE_TIMER(%d seconds) expired, terminated a session.", PAIRING_GRACE_TIME);
                } else {
                    log(LOG_ERR, "BONDING_WINDOW_TIMER(%d seconds) expired, connection request is refused.", BONDING_WINDOW_TIME);
                }
                ggkSetDasBootFlag();
            }
        }
        else
        {
            graceTime = currentTime;
        }

        if (testMode) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            timerCounter++;
        }
    }

    if (testMode) {
        if (inotifyFd >= 0) {
            /* remove wd */
            if (inotifyWd >= 0) {
                inotify_rm_watch(inotifyFd, inotifyWd);
            }
            /* close inotify */
            close(inotifyFd);
        }
    }

    // Wait for the server to come to a complete stop (CTRL-C from the command line)
    if (!ggkWait())
    {
        return -1;
    }

    // Return the final server health status as a success (0) or error (-1)
      return ggkGetServerHealth() == EOk ? 0 : 1;
}
