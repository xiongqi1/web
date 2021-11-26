/*
 * ookla speed test driver.
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

#include <syslog.h>

#include "OoklaOutputType.h"
#include "SpeedTest.h"
#include "SpeedTestTypes.h"

#include <ext/stdio_filebuf.h>
#include <jsoncons/json.hpp>

namespace speedtest
{

    SpeedTest::SpeedTest(SpeedTestData &speedTestData) : success(false),
                                                         data(speedTestData)
    {
    }

    void SpeedTest::execute()
    {
        try
        {
            std::string cmd = binary + " --configurl=" + url;
            if (caCertificate != "") {
                cmd += " --ca-certificate=" + caCertificate;
            }
            cmd += " -f jsonl 2>&1";

            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
            if (!pipe)
            {
                throw std::runtime_error("popen() failed!");
            }

            __gnu_cxx::stdio_filebuf<char> filebuf(pipe.get(), std::ios_base::in);
            std::istream stream(&filebuf);

            int errorCount = 0;
            std::string ooklaTypeValue;
            for (std::string line; std::getline(stream, line);)
            {
                if (line.length() <= 0 || line[0] != '{')
                    continue;

                try
                {
                    jsoncons::json ooklaJson = jsoncons::json::parse(line);
                    if (ooklaJson.contains("type") == false)
                        continue;

                    ooklaTypeValue = ooklaJson["type"].as<std::string>();
                    OoklaOutputType ooklaType = ValueToOoklaOutput(ooklaTypeValue);

                    switch (ooklaType)
                    {

                        case OoklaOutputType::testStart:
                        {
                            if (ooklaJson.contains("isp"))
                            {
                                data.isp = ooklaJson["isp"].as<std::string>();
                            }

                            if (ooklaJson.contains("server"))
                            {
                                const jsoncons::json &serverJson = ooklaJson["server"];
                                data.name = serverJson["name"].as<std::string>();
                                data.location = serverJson["location"].as<std::string>();
                                data.country = serverJson["country"].as<std::string>();
                                data.host = serverJson["host"].as<std::string>();
                                data.serverIdentifier = serverJson["id"].as<int>();
                                data.serverIp = serverJson["ip"].as<std::string>();
                            }

                            if (ooklaJson.contains("interface"))
                            {
                                const jsoncons::json &interfaceJson = ooklaJson["interface"];
                                data.clientIp = interfaceJson["externalIp"].as<std::string>();
                            }
                        }
                        break;

                        case OoklaOutputType::log:
                        {
                            if (ooklaJson.contains("level") && ooklaJson.contains("message"))
                            {
                                std::string level = ooklaJson["level"].as<std::string>();
                                std::string msg = ooklaJson["message"].as<std::string>();

                                if (level == "error")
                                {
                                    errorCount++;
                                    syslog(LOG_ERR, "Ookla error message: %s\n", msg.c_str());
                                    data.testErrorMsgs.push_back(msg);
                                }
                                else if (level == "warning")
                                {
                                    syslog(LOG_WARNING, "Ookla warning message: %s\n", msg.c_str());
                                }
                            }
                        }
                        break;

                        case OoklaOutputType::ping:
                        {
                            if (ooklaJson.contains("ping"))
                            {
                                const jsoncons::json &pingJson = ooklaJson["ping"];
                                data.ping.progress = pingJson["progress"].as<float>() * 100;
                                data.ping.jitter = pingJson["jitter"].as<float>();
                                data.ping.latency = pingJson["latency"].as<float>();
                            }
                        }
                        break;

                        case OoklaOutputType::download:
                        {
                            if (ooklaJson.contains("download"))
                            {
                                const jsoncons::json &downloadJson = ooklaJson["download"];
                                data.download.progress = downloadJson["progress"].as<float>() * 100;
                                data.download.bandwidth = downloadJson["bandwidth"].as<int>();
                                data.download.bytes = downloadJson["bytes"].as<int>();
                                data.download.elapsed = downloadJson["elapsed"].as<int>();
                            }
                        }
                        break;

                        case OoklaOutputType::upload:
                        {
                            if (ooklaJson.contains("upload"))
                            {
                                const jsoncons::json &uploadJson = ooklaJson["upload"];
                                data.upload.progress = uploadJson["progress"].as<float>() * 100;
                                data.upload.bandwidth = uploadJson["bandwidth"].as<int>();
                                data.upload.bytes = uploadJson["bytes"].as<int>();
                                data.upload.elapsed = uploadJson["elapsed"].as<int>();
                            }
                        }
                        break;

                        case OoklaOutputType::serverSelection:
                        {
                        }
                        break;

                        case OoklaOutputType::result:
                        {
                            if (ooklaJson.contains("result"))
                            {
                                const jsoncons::json &resultJson = ooklaJson["result"];
                                data.resultId = resultJson["id"].as<int>();
                            }
                        }
                        break;

                        case OoklaOutputType::unknown:
                            syslog(LOG_WARNING, "Unknown ookla JSON output type: %s\n", line.c_str());
                            break;
                    }
                }
                catch (const std::exception &e)
                {
                    syslog(LOG_WARNING, "Unable to handle ookla JSON output [%s]: %s\n", line.c_str(), e.what());
                }
            }

            if (errorCount == 0)
                success = true;
        }
        catch (const std::exception &e)
        {
            syslog(LOG_WARNING, "Unknow error during ookla initialization: %s\n", e.what());
        }

        eventFinish.Signal();
    }

} // namespace speedtest
