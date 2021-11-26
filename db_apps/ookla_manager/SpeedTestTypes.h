/*
 * ookla speed test types and storage.
 *
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
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

#pragma once

#include <erdb.hpp>
#include <string>
#include <vector>
#include <time.h>
#include <jsoncons/json.hpp>

namespace speedtest
{

    enum class SpeedTestProgress
    {
        NotStarted,
        InProgress,
        Finished,
        Error
    };
    enum class SpeedTestExperience
    {
        NotRated,
        Poor,
        Fair,
        Good,
        VeryGood,
        Excellent
    };
    enum class ExperienceType
    {
        Download,
        Upload
    };

    std::string SpeedTestProgressToString(SpeedTestProgress value);
    std::string SpeedTestExperienceToString(SpeedTestExperience value);
    SpeedTestExperience StringToSpeedTestExperience(std::string value);

    struct SpeedTestPing
    {

        int progress;
        float jitter;
        float latency;
    };

    struct SpeedTestThroughput
    {

        int progress;
        int bandwidth;
        int bytes;
        int elapsed;
    };

    class SpeedTestSummary
    {

    private:
        erdb::Rdb &rdbTestSummaryStorage;

    public:
        SpeedTestProgress currentState;
        double downloadBandwidthMbps;
        SpeedTestExperience downloadExperience;
        double uploadBandwidthMbps;
        SpeedTestExperience uploadExperience;
        uint32_t pingLatency;
        uint32_t cycle;
        time_t lastResultUpdated; // in seconds since 1970-01-01 00:00:00 UTC
        std::string serverCountry;
        std::string serverHost;
        std::string serverIsp;
        std::string serverLocation;
        std::string serverName;
        int serverIdentifier;
        std::string serverIp;
        std::string clientIp;
        int testResultId;
        jsoncons::json testErrorMsgs{jsoncons::json_array_arg};

    public:
        SpeedTestSummary(erdb::Rdb &rdb);
        void Save();
    };

    class SpeedTestData
    {

    private:
        erdb::Rdb &rdbTestDataStorage;
        int m_cycle;

    public:
        int resultId;
        std::string name;
        std::string location;
        std::string country;
        std::string isp;
        std::string host;
        int serverIdentifier;
        std::string serverIp;
        std::string clientIp;

        SpeedTestPing ping;
        SpeedTestThroughput download;
        SpeedTestThroughput upload;

        int getCycle() const { return m_cycle; }

        jsoncons::json testErrorMsgs{jsoncons::json_array_arg};

    public:
        SpeedTestData(int cycle, erdb::Rdb &rdb);
        void Save();
    };

    void AggregateTest(ExperienceType type, std::vector<SpeedTestData> &data, SpeedTestSummary &summary);
    void AggregateOverall(std::vector<SpeedTestData> &data, SpeedTestSummary &summary);
    void aggregateErrors(std::vector<SpeedTestData> &data, SpeedTestSummary &summary);
} // namespace speedtest
