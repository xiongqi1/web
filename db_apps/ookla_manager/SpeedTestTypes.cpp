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

#include <string>
#include <sstream>
#include <ios>
#include <iomanip>
#include <cmath>
#include <erdb.hpp>

#include "SpeedTestTypes.h"
#include "Calculation.h"

namespace speedtest
{

    std::string SpeedTestProgressToString(SpeedTestProgress value)
    {
        switch (value)
        {
            case SpeedTestProgress::Error:
                return "error";
            case SpeedTestProgress::InProgress:
                return "inprogress";
            case SpeedTestProgress::Finished:
                return "finished";
            case SpeedTestProgress::NotStarted:
                return "notstarted";
            default:
                break;
        }
        return "unknown";
    }

    std::string SpeedTestExperienceToString(SpeedTestExperience value)
    {
        switch (value)
        {
            case SpeedTestExperience::Poor:
                return "Poor";
            case SpeedTestExperience::Fair:
                return "Fair";
            case SpeedTestExperience::Good:
                return "Good";
            case SpeedTestExperience::VeryGood:
                return "Very Good";
            case SpeedTestExperience::Excellent:
                return "Excellent";
            default:
                break;
        }
        return "NotRated";
    }

    SpeedTestExperience StringToSpeedTestExperience(std::string value)
    {
        if( value == "Poor") return SpeedTestExperience::Poor;
        if( value == "Fair") return SpeedTestExperience::Fair;
        if( value == "Good") return SpeedTestExperience::Good;
        if( value == "Very Good") return SpeedTestExperience::VeryGood;
        if( value == "Excellent") return SpeedTestExperience::Excellent;
        return SpeedTestExperience::NotRated;
    }


    SpeedTestSummary::SpeedTestSummary(erdb::Rdb &rdb) : rdbTestSummaryStorage(rdb),
                                                         currentState(SpeedTestProgress::NotStarted),
                                                         downloadBandwidthMbps(0),
                                                         downloadExperience(SpeedTestExperience::NotRated),
                                                         uploadBandwidthMbps(0),
                                                         uploadExperience(SpeedTestExperience::NotRated),
                                                         pingLatency(0),
                                                         cycle(0), lastResultUpdated(time(0)),
                                                         serverCountry(""), serverHost(""), serverIsp(""),
                                                         serverLocation(""), serverName(""), testResultId(0),
                                                         serverIdentifier(0), serverIp(""), clientIp("")
    {
        rdbTestSummaryStorage.addChildren({"current_state",
                                "download_bandwidthMbps",
                                "download_experience",
                                "upload_bandwidthMbps",
                                "upload_experience",
                                "ping_latency",
                                "cycle",
                                "last_result_updated",
                                "country",
                                "host",
                                "isp",
                                "location",
                                "name",
                                "result_id",
                                "test_errors",
                                "server_identifier",
                                "server_ip",
                                "client_ip"
        });
        //lastResultUpdated = time(0);
    }

    void SpeedTestSummary::Save()
    {
        std::ostringstream osDl, osUl;
        osDl << std::fixed << std::setprecision(1) << downloadBandwidthMbps;
        osUl << std::fixed << std::setprecision(1) << uploadBandwidthMbps;

        rdbTestSummaryStorage.setChildren({{"current_state", SpeedTestProgressToString(currentState)},
                                {"download_bandwidthMbps", osDl.str()},
                                {"download_experience", SpeedTestExperienceToString(downloadExperience)},
                                {"upload_bandwidthMbps", osUl.str()},
                                {"upload_experience", SpeedTestExperienceToString(uploadExperience)},
                                {"ping_latency", pingLatency},
                                {"cycle", cycle},
                                {"last_result_updated", lastResultUpdated},
                                {"country", serverCountry},
                                {"host", serverHost},
                                {"isp", serverIsp},
                                {"location", serverLocation},
                                {"name", serverName},
                                {"result_id", testResultId},
                                {"test_errors", testErrorMsgs.to_string()},
                                {"server_identifier", serverIdentifier},
                                {"server_ip", serverIp},
                                {"client_ip", clientIp}
        });
    }

    SpeedTestData::SpeedTestData(int cycle, erdb::Rdb &rdb) : rdbTestDataStorage(rdb.addChild(cycle)),
                                                              m_cycle(cycle)
    {
        ping = {};
        download = {};
        upload = {};

        rdbTestDataStorage.addChildren({"name",
                                "location",
                                "country",
                                "isp",
                                "host",
                                "resultId",
                                "server_identifier",
                                "server_ip",
                                "client_ip",
                                "ping.progress",
                                "ping.jitter",
                                "ping.latency",
                                "download.progress",
                                "download.bandwidth",
                                "download.bytes",
                                "download.elapsed",
                                "upload.progress",
                                "upload.bandwidth",
                                "upload.bytes",
                                "upload.elapsed"});

        /* Set non-persist */
        rdbTestDataStorage.setChildrenRdbFlags(0).setChildren("");
    }

    void SpeedTestData::Save()
    {
        rdbTestDataStorage.setChildren({{"name", name},
                                {"location", location},
                                {"country", country},
                                {"isp", isp},
                                {"host", host},
                                {"resultId", resultId},
                                {"server_identifier", serverIdentifier},
                                {"server_ip", serverIp},
                                {"client_ip", clientIp},
                                {"ping.progress", ping.progress},
                                {"ping.jitter", std::to_string(ping.jitter)},
                                {"ping.latency", std::to_string(ping.latency)},
                                {"download.progress", download.progress},
                                {"download.bandwidth", download.bandwidth},
                                {"download.bytes", download.bytes},
                                {"download.elapsed", download.elapsed},
                                {"upload.progress", upload.progress},
                                {"upload.bandwidth", upload.bandwidth},
                                {"upload.bytes", upload.bytes},
                                {"upload.elapsed", upload.elapsed}});
    }

    void AggregateTest(ExperienceType type, std::vector<SpeedTestData> &data, SpeedTestSummary &summary)
    {
        SpeedTestExperience experience = SpeedTestExperience::NotRated;
        double bandwidthMbps = 0;

        do {
            if (data.size() == 0)
                break;

            for (const auto &item : data)
            {
                if (type == ExperienceType::Download)
                    bandwidthMbps += ((double)(item.download.bandwidth) / 125000);
                else
                    bandwidthMbps += ((double)(item.upload.bandwidth) / 125000);
            }
            bandwidthMbps = bandwidthMbps / data.size();

            if (type == ExperienceType::Download)
                experience = CalculateDownloadExperience(bandwidthMbps);
            else
                experience = CalculateUploadExperience(bandwidthMbps);

        } while (false);

        if (type == ExperienceType::Download)
        {
            summary.downloadExperience = experience;
            summary.downloadBandwidthMbps = bandwidthMbps;
        }
        else
        {
            summary.uploadExperience = experience;
            summary.uploadBandwidthMbps = bandwidthMbps;
        }
    }

    void aggregateErrors(std::vector<SpeedTestData> &data, SpeedTestSummary &summary)
    {
        for (const auto &item : data) {
            summary.testErrorMsgs.push_back(item.testErrorMsgs);
        }
    }

    void AggregateOverall(std::vector<SpeedTestData> &data, SpeedTestSummary &summary)
    {
        float pingLatency = 0;
        do {
            if (data.size() == 0)
                break;

            for (const auto &item : data)
            {
                pingLatency += item.ping.latency;

                /* set server info with the last test result */
                summary.serverCountry = item.country;
                summary.serverHost = item.host;
                summary.serverIsp = item.isp;
                summary.serverLocation = item.location;
                summary.serverName = item.name;
                summary.testResultId = item.resultId;
                summary.serverIdentifier = item.serverIdentifier;
                summary.serverIp = item.serverIp;
                summary.clientIp = item.clientIp;
            }
            pingLatency = pingLatency / data.size();

        } while (false);

        aggregateErrors(data, summary);

        summary.pingLatency = std::round(pingLatency);
    }

} // namespace speedtest
