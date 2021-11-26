/*
 * Program to manage the ookla speed test.
 *
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or
 * object forms) without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <syslog.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <erdb.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

#include "SpeedTest.h"
#include "SpeedTestTypes.h"

namespace st = speedtest;
namespace fs = std::filesystem;

const char *rdbPathRoot = "service.speedtest";
const char *rdbPathConfigCycles = "service.speedtest.config_cycles";

static void usage(const char *p_name)
{
    printf("CASA ookla manager\n\n");
    printf("Usage: %s [options] \n\n", p_name);
    printf(" [options] may be:\n");
    printf(
        "  -p <BIN>                : ookla binary location, default: "
        "/usr/bin/ookla \n");
    printf(
        "  -u <URL>                : config URL location e.g: "
        "http://www.speedtest.net/api/embed/trial/config \n");
    printf(
        "  -c <path>                : Path to CA certificate\n");
    printf("  -l <level>              : set syslog log level \n");
    printf("  -h                      : display this message \n");
}

int main(int argc, char *argv[])
{
    int logUpTo = LOG_INFO;

    std::string ooklaBinLocation = "/usr/bin/ookla";
    std::string ooklaConfigUrl = "";
    std::string ooklaCaCertificate = "";

    // Get Command line arguments
    int option;
    while ((option = getopt(argc, argv, "u:p:c:h")) != -1)
    {
        switch (option)
        {
            case 'l':
                if (sscanf(optarg, "%d", &logUpTo) != 1)
                {
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'u':
                ooklaConfigUrl = optarg;
                break;
            case 'c':
                ooklaCaCertificate = optarg;
                break;
            case 'p':
                ooklaBinLocation = optarg;
                break;
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                printf("Unknown parameter: %c\n", option);
                usage(argv[0]);
                break;
        }
    }

    openlog("ookla_mgr", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Start ookla manager \n");
    setlogmask(LOG_UPTO(logUpTo));

    // Check if binary exists and can be executed
    fs::path binPath = ooklaBinLocation;
    if (fs::exists(binPath) == false)
    {
        syslog(LOG_ERR, "Binary [%s] not found.\n", ooklaBinLocation.c_str());
        exit(EXIT_FAILURE);
    }
    fs::perms binPerm = fs::status(binPath).permissions();
    if ((binPerm & fs::perms::owner_exec) == fs::perms::none)
    {
        syslog(LOG_ERR, "Binary [%s] is not executable.\n",
               ooklaBinLocation.c_str());
        exit(EXIT_FAILURE);
    }

    // Initialize test storage
    erdb::Rdb speedTestRdbRoot = erdb::Rdb(rdbPathRoot, PERSIST, DEFAULT_PERM);

    // Read the number of cycles required; default to at least 1
    erdb::Rdb rdbConfigCycles = erdb::Rdb(rdbPathConfigCycles);
    uint32_t numberOfTestCycles = 1;
    try {
        numberOfTestCycles = std::stoi(rdbConfigCycles.get(true).toStdString("1"));
    }
    catch (...) {
        numberOfTestCycles = 1;
    }

    st::SpeedTestSummary speedTestSummary(speedTestRdbRoot);
    speedTestSummary.Save();

    try
    {
        std::vector<st::SpeedTestData> speedTestResult;
        for (uint32_t count = 1; count <= numberOfTestCycles; count++)
        {
            speedTestResult.push_back(st::SpeedTestData(count, speedTestRdbRoot));
        }

        for (auto &item : speedTestResult)
        {
            item.Save();
        }

        std::chrono::milliseconds refreshSleep = std::chrono::milliseconds(500);

        // Start processing
        for (auto &testData : speedTestResult)
        {
            speedTestSummary.cycle = testData.getCycle();
            speedTestSummary.currentState = st::SpeedTestProgress::InProgress;
            speedTestSummary.Save();

            st::SpeedTest *testPtr = new st::SpeedTest(testData);
            st::SpeedTest &test = *testPtr;

            test.binary = ooklaBinLocation;
            test.url = ooklaConfigUrl;
            test.caCertificate = ooklaCaCertificate;

            std::thread processingTh(&st::SpeedTest::execute, testPtr);

            while (test.eventFinish.Wait(refreshSleep) == false)
            {
                testData.Save();
            }

            processingTh.join();
            testData.Save();

            if (test.isSuccessful() == false)
            {
                aggregateErrors(speedTestResult, speedTestSummary);
                throw std::runtime_error("Test execution failure. Processing is aborted. Ookla returned error logs");
            }
        }

        AggregateTest(st::ExperienceType::Download, speedTestResult, speedTestSummary);
        AggregateTest(st::ExperienceType::Upload, speedTestResult, speedTestSummary);
        AggregateOverall(speedTestResult, speedTestSummary);
        speedTestSummary.lastResultUpdated = time(0); // last result update time in seconds since 1970-01-01 00:00:00 UTC.
    }
    catch (const std::exception &e)
    {
        speedTestSummary.currentState = st::SpeedTestProgress::Error;
        speedTestSummary.Save();

        syslog(LOG_ERR, "Program terminated. Message: %s\n", e.what());
        return EXIT_FAILURE;
    }

    // Save details before triggering finish
    speedTestSummary.Save();

    speedTestSummary.currentState = st::SpeedTestProgress::Finished;
    speedTestSummary.Save();
    return EXIT_SUCCESS;
}
