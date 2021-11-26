/*
 * SwivellingScan service GATT server Plugin
 *
 * Copyright Notice:
 * Copyright (C) 2021 Casa Systems.
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

#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"

// JSON library from https://github.com/nlohmann/json
#include "json.hpp"

#include "uuids.h" // our custom UUIDs
#include "erdb.hpp"
#include "elogger.hpp"

#include "erdb_rpc_client.hpp"

// for convenience
using json = nlohmann::json;

namespace ggk {

// Class to describe GATT server Swivelling Scan Service Characteristics.
class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);

    erdb::Rdb rdbScanStatus = erdb::Rdb("swivelling_scan.status");
    erdb::Rdb rdbHomeIdx = erdb::Rdb("swivelling_scan.conf.home_position_index");

    std::string getScanStatus();
    bool invokeRdbRpc(const std::string &cmd, const std::map<std::string, std::string> &params);
    bool startSwivellingScan(uint8_t);
    bool stopSwivellingScan(uint8_t);
    bool moveToHomePosition(uint8_t);
};

DEFINE_PLUGIN(ServicePlugin, "swivellingScanService", "mrswivel")

// get Swivelling Scan Status
std::string ServicePlugin::getScanStatus() {
    // Local function to convert Index to Degree
    // Valid return range 0~359, -1
    auto cvtIdxToDeg = [](int index, int homeIdx) {
        int retDeg = -1;
        if (index >= 0 && homeIdx >= 0) {
            int homeDeg = erdb::Rdb(estd::format("swivelling_scan.conf.position.%d.coordinate", homeIdx)).get(true).toInt(-1);
            int idxDeg = erdb::Rdb(estd::format("swivelling_scan.conf.position.%d.coordinate", index)).get(true).toInt(-1);
            if (homeDeg >= 0 && idxDeg >= 0) {
                retDeg = idxDeg - homeDeg;
                if (retDeg < 0 ) {
                    retDeg += 360;
                }
            }
        }
        return retDeg;
    };

    json jStatus;
    json jRet =
    {
        {"Status",""}, {"Error","Invalid status"}, {"CurrentPositionDegree",-1},
        {"MovingToPositionDegree",-1}, {"CurrentSwivellingScanStep",0}, {"MaxSwivellingScanStep",0}
    };

    //Ex) rdbScanStatus: '{"currentPositionIndex":4,"currentSwivellingScanStep":6,"error":"","maxSwivellingScanStep":6,"movingToPositionIndex":5,"status":"swivellingScanMoving"}'
    std::string scanStatus = rdbScanStatus.get(true).toStdString("");

    if (scanStatus.empty()) {
        return jRet.dump();
    }

    try {
        jStatus = json::parse(scanStatus);
    }
    catch(const json::parse_error &ex) {
        log(LOG_ERR, "Error: Invalid JSON argument: %s", ex.what());
        return jRet.dump();
    }

    jRet["Status"] = jStatus.value("status", "");
    jRet["Error"] = jStatus.value("error", "");
    jRet["CurrentSwivellingScanStep"] = jStatus.value("currentSwivellingScanStep", 0);
    jRet["MaxSwivellingScanStep"] = jStatus.value("maxSwivellingScanStep", 0);

    int homIdx = rdbHomeIdx.get(true).toInt(-1);
    jRet["CurrentPositionDegree"] = cvtIdxToDeg(jStatus.value("currentPositionIndex", -1), homIdx);
    jRet["MovingToPositionDegree"] = cvtIdxToDeg(jStatus.value("movingToPositionIndex", -1), homIdx);

    return jRet.dump();
}

bool ServicePlugin::invokeRdbRpc(const std::string &cmd, const std::map<std::string, std::string> &params) {
    json jRet;
    // RDB RPC Client instance for SwivellingScan service
    // Do not change this variable as global.
    // Otherwise, it is possible to lose select event from select loop on rdb_rpc_client_invoke() library function.
    // Moreover, the select loop is not robust on losing select event.
    erdb::RdbRpcClient rdbRpcClient = erdb::RdbRpcClient("SwivellingScan");
    char result[128]; //response size of RDB RPC command.
    int result_len = sizeof(result);

    // Invoke with 3 seconds timeout.
    // The maximum timeout should be less than 5 seconds, which is set method timeout on lower layer(bluez).
    // So if the timeout value is greater than or equal to 5 seconds, bluez raises timeout error, anyway.
    if (rdbRpcClient.invoke( cmd, params, 3, result, &result_len)) {
        jRet = json::parse(result);
        if (jRet.value("success", false)) {
            return true;
        }
    }
    return false;
}

bool ServicePlugin::startSwivellingScan(uint8_t setVal) {
    if (setVal == 1) {
        if (invokeRdbRpc("startSwivellingScan", {{"param", "{\"force\":true}"}})) {
            return true;
        }
    }
    return false;
}

bool ServicePlugin::stopSwivellingScan(uint8_t setVal) {
    if (setVal == 1) {
        if (invokeRdbRpc("stopSwivellingScan", {})) {
            return true;
        }
    }
    return false;
}

bool ServicePlugin::moveToHomePosition(uint8_t setVal) {
    if (setVal == 1) {
        if (invokeRdbRpc("moveToHomePosition", {})) {
            return true;
        }
    }
    return false;
}

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("swivellingScanService", SWIVELLING_SCAN_SERVICE_UUID)

        // Characteristic Value: (uint8) Set to 1 to start swivelling scan.
        .gattCharacteristicBegin("startSwivellingScanCharacteristic", START_SWIVELLING_SCAN_UUID, {"encrypt-write"})
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                uint8_t setVal = *reinterpret_cast<const uint8_t *>(Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                if (PLUGIN(ServicePlugin)->startSwivellingScan(setVal)) {
                    self.methodReturnVariant(pInvocation, NULL);
                } else {
                    g_dbus_method_invocation_return_error(pInvocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Write Error");
                }
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Start Swivelling Scan";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        // Characteristic Value: (uint8) Set to 1 to stop swivelling scan.
        .gattCharacteristicBegin("stopSwivellingScanCharacteristic", STOP_SWIVELLING_SCAN_UUID, {"encrypt-write"})
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                uint8_t setVal = *reinterpret_cast<const uint8_t *>(Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                if (PLUGIN(ServicePlugin)->stopSwivellingScan(setVal)) {
                    self.methodReturnVariant(pInvocation, NULL);
                } else {
                    g_dbus_method_invocation_return_error(pInvocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Write Error");
                }
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Stop Swivelling Scan";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        // Characteristic Value: (uint8) Set to 1 to move to the home position.
        .gattCharacteristicBegin("moveToHomePositionCharacteristic", MOVE_TO_HOME_POSITION_UUID, {"encrypt-write"})
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                uint8_t setVal = *reinterpret_cast<const uint8_t *>(Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                if (PLUGIN(ServicePlugin)->moveToHomePosition(setVal)) {
                    self.methodReturnVariant(pInvocation, NULL);
                } else {
                    g_dbus_method_invocation_return_error(pInvocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Write Error");
                }
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Move To Home Position";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        // Characteristic Value: (string) JSON container:
        //   Status, Error, CurrentPositionDegree, MovingToPositionDegree,
        //   CurrentSwivellingScanStep, MaxSwivellingScanStep for running swivelling scan
        .gattCharacteristicBegin("swivellingScanStatusCharacteristic", SWIVELLING_SCAN_STATUS_UUID, {"encrypt-read", "notify"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getScanStatus(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getScanStatus());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbScanStatus);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbScanStatus);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Swivelling Scan Status";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

    .gattServiceEnd();
    return true;
}

}; // namespace ggk
