#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"

#include "uuids.h" // our custom UUIDs
#include "erdb.hpp"

// JSON library from https://github.com/nlohmann/json
#include "json.hpp"

#include "Housekeeping.h"
#include "Logger.h"

#include <iostream>     // std::fixed
#include <iomanip>      // std::setprecision
#include <sstream>

using json = nlohmann::json;

namespace ggk {

const char *rdbPathRoot = "service.speedtest";
const char *rdbPathCurrentState = "service.speedtest.current_state";

// Test Result Summary
const char *rdbPathDownloadBandwidthMbps = "service.speedtest.download_bandwidthMbps";
const char *rdbPathDownloadExperience = "service.speedtest.download_experience";

const char *rdbPathUploadBandwidthMbps = "service.speedtest.upload_bandwidthMbps";
const char *rdbPathUploadExperience = "service.speedtest.upload_experience";

const char *rdbPathPingLatency = "service.speedtest.ping_latency";
const char *rdbPathResultId = "service.speedtest.result_id";

const char *rdbPathServerName = "service.speedtest.name";
const char *rdbPathServerLocation = "service.speedtest.location";
const char *rdbPathServerIdentifier = "service.speedtest.server_identifier";
const char *rdbPathServerIp = "service.speedtest.server_ip";
const char *rdbPathClientIp = "service.speedtest.client_ip";
const char *rdbPathIsp = "service.speedtest.isp";

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);
    ServicePlugin();

    erdb::Rdb rdbSpeedTestRdbRoot = erdb::Rdb(rdbPathRoot, PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbCurrentState = erdb::Rdb(rdbPathCurrentState);
    erdb::Rdb rdbDownloadBandwidthMbps = erdb::Rdb(rdbPathDownloadBandwidthMbps);
    erdb::Rdb rdbDownloadExperience = erdb::Rdb(rdbPathDownloadExperience);
    erdb::Rdb rdbUploadBandwidthMbps = erdb::Rdb(rdbPathUploadBandwidthMbps);
    erdb::Rdb rdbPingLatency = erdb::Rdb(rdbPathPingLatency);
    erdb::Rdb rdbResultId = erdb::Rdb(rdbPathResultId);
    erdb::Rdb rdbServerName = erdb::Rdb(rdbPathServerName);
    erdb::Rdb rdbServerLocation = erdb::Rdb(rdbPathServerLocation);
    erdb::Rdb rdbServerIdentifier = erdb::Rdb(rdbPathServerIdentifier);
    erdb::Rdb rdbServerIp = erdb::Rdb(rdbPathServerIp);
    erdb::Rdb rdbClientIp = erdb::Rdb(rdbPathClientIp);
    erdb::Rdb rdbIsp = erdb::Rdb(rdbPathIsp);

    void startTest();
    uint8_t getTestStatus();
    std::string getTestResult();
};

DEFINE_PLUGIN(ServicePlugin, "speedTestService", "saturn")

ServicePlugin::ServicePlugin(void) {
    rdbSpeedTestRdbRoot.addChildren({"trigger"});
}

void ServicePlugin::startTest()
{
    rdbSpeedTestRdbRoot.setChildren({{"trigger", 1}});
}

uint8_t ServicePlugin::getTestStatus()
{
    std::string state = rdbCurrentState.get(true).toStdString();
    if (state == "inprogress" || state == "notstarted")
        return 1;
    else
        return 0;
}

std::string ServicePlugin::getTestResult()
{
    json j;
    std::stringstream dStr, uStr;
    double downstreamMbps = 0.0;
    double upstreamMbps = 0.0;
    uint32_t latency = 0;
    std::string experience = "";
    std::string state = rdbCurrentState.get(true).toStdString("unknown");
    int resultId = 0; // the original type on ookla_manager process is "int".
    std::string serverName = "";
    std::string serverLocation = "";
    int serverIdentifier = 0;
    std::string serverIp = "";
    std::string clientIp = "";
    std::string isp = "";

    if (state == "finished") {
        try {
            downstreamMbps = std::stod(rdbDownloadBandwidthMbps.get(true).toStdString("0.0"));
        }
        catch (...) {
            downstreamMbps = 0.0;
        }

        try {
            upstreamMbps = std::stod(rdbUploadBandwidthMbps.get(true).toStdString("0.0"));
        }
        catch (...) {
            upstreamMbps = 0.0;
        }

        try {
            latency = std::stoi(rdbPingLatency.get(true).toStdString("0"));
        }
        catch (...) {
            latency = 0;
        }

        try {
            resultId = std::stoi(rdbResultId.get(true).toStdString("0"));
        }
        catch (...) {
            resultId = 0;
        }
        experience = rdbDownloadExperience.get(true).toStdString("Poor");

        serverName = rdbServerName.get(true).toStdString("");
        serverLocation = rdbServerLocation.get(true).toStdString("");
        serverIp = rdbServerIp.get(true).toStdString("");
        clientIp = rdbClientIp.get(true).toStdString("");
        isp = rdbIsp.get(true).toStdString("");

        try {
            serverIdentifier = std::stoi(rdbServerIdentifier.get(true).toStdString("0"));
        }
        catch (...) {
            serverIdentifier = 0;
        }
    }

    dStr << std::fixed << std::setprecision(1) << downstreamMbps;
    uStr << std::fixed << std::setprecision(1) << upstreamMbps;
    j["Downstream (Mbps)"] = dStr.str();
    j["Upstream (Mbps)"] = uStr.str();
    j["Latency"] = std::to_string(latency);
    j["Experience"] = experience;
    j["State"] = state;
    j["Result ID"] = resultId;
    j["Server Name"] = serverName;
    j["Server Location"] = serverLocation;
    j["Server ID"] = serverIdentifier;
    j["Server IP"] = serverIp;
    j["Client IP"] = clientIp;
    j["ISP"] = isp;
    return j.dump();
}

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("speedTestService", SPEEDTEST_UUID)
        .gattCharacteristicBegin("speedTestRequestCharacteristic", SPEEDTEST_REQUEST_UUID, {"encrypt-read", "encrypt-write"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getTestStatus(), true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("speedTestService/speedTestRequestCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                // Any value as in spec.
                PLUGIN(ServicePlugin)->startTest();
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getTestStatus());
                return true;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Speed Test Request";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("speedTestResultCharacteristic", SPEEDTEST_RESULT_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getTestResult(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                std::string state = PLUGIN(ServicePlugin)->rdbCurrentState.get(true).toStdString();
                if (state != "notstarted") {
                    self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getTestResult());
                }
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbCurrentState);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbCurrentState);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Speed Test Result";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
