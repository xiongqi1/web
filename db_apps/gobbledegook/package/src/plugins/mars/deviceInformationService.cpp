#include <regex>

#include "Base64.h"
#include "HexMask.h"
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
#include "pluginCommon.h"

// for convenience
using json = nlohmann::json;

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);
    ServicePlugin();

    // AurusLink(casa_cfw1314) -> 0
    // AurusPro(casa_cfw2431,beleng_cfw2352,bel_cfw2352)  -> 1
    // AurusGate -> 2 //@TODO:: need to add appropriate RDB value if it is defined.
    // AurusPro4G(casa_cfw2131, nbn_cfw2131) -> 3
    estd::DefValueMap<std::string, int> enumFamily = {
        {"casa_cfw1314", 0},
        {"casa_cfw2431", 1},
        {"2352", 1}, // beleng_cfw2352, bel_cfw2352
        {"casa_cfw2131", 3},
        {"casaeng_cfw2131", 3},
        {"nbn_cfw2131", 3}
    };

    estd::DefValueMap<std::string, std::string> devErrSysMode = {
        {"no service", "No service"},
        {"limited service", "Limited service"},
        {"limited regional service", "Limited regional service"},
        {"power save", "Power save"}
    };

    enum class enumDevState: uint8_t
    {
        STATE_BOOTING,
        STATE_SCANNING,
        STATE_REGISTERED,
        STATE_ATTACHED,
        STATE_ERROR
    };

    estd::DefValueMap<std::string, int> bandMaskWidth = {
        {"WCDMA", 16},
        {"LTE",  16}
    };

    erdb::Rdb rdbHwVer = erdb::Rdb("system.product.hwver");
    erdb::Rdb rdbSwVer = erdb::Rdb("sw.version");
    erdb::Rdb rdbProdModel = erdb::Rdb("system.product.model");
    erdb::Rdb rdbTr069AcsUrl = erdb::Rdb("tr069.server.url");
    erdb::Rdb rdbTr069ConnTime = erdb::Rdb("tr069.informStartAt");
    erdb::Rdb rdbModemRegState = erdb::Rdb("wwan.0.system_network_status.reg_stat");
    erdb::Rdb rdbModemAttState = erdb::Rdb("wwan.0.system_network_status.attached");
    erdb::Rdb rdbSysMode = erdb::Rdb("wwan.0.system_network_status.system_mode");
    erdb::Rdb rdbSimStatus = erdb::Rdb("wwan.0.sim.status.status");
    erdb::Rdb rdbSupportedBandMask = erdb::Rdb("wwan.0.module_band_list.hexmask");
    erdb::Rdb rdbFixedAvailableBandMask = erdb::Rdb("wwan.0.fixed_module_band_list.hexmask");
    erdb::Rdb rdbSelectedBandMask = erdb::Rdb("wwan.0.currentband.current_selband.hexmask");
    erdb::Rdb rdbParamSelectedBand = erdb::Rdb("wwan.0.currentband.cmd.param.band.hexmask");
    erdb::Rdb rdbCmdSelectedBand = erdb::Rdb("wwan.0.currentband.cmd.command");
    erdb::Rdb rdbDevSerialNum = erdb::Rdb("system.product.sn");
    erdb::Rdb rdbDevIMEI = erdb::Rdb("wwan.0.imei");
    erdb::Rdb rdbDevNetPLMN = erdb::Rdb("wwan.0.system_network_status.PLMN");
    erdb::Rdb rdbDevNetName = erdb::Rdb("wwan.0.system_network_status.network");
    erdb::Rdb rdbDevNetLongName = erdb::Rdb("wwan.0.system_network_status.nw_name.long");
    erdb::Rdb rdbRevertBandMode = erdb::Rdb("wwan.0.currentband.revert_selband.mode");

    // RDB Containter object for simApnCharacteristic
    erdb::Rdb cRdbLinkProfiles = erdb::Rdb("link.profile");

    ggk::Base64 base64Obj;

    std::string getVersion();
    uint8_t getDevFamily();
    std::string getDevIdentifiers();
    std::string getDevNetIdentifiers();
    std::string getTr069Status();
    enumDevState getDevState();
    std::string getDevError();
    std::string getSimApn();
    std::string getSupportedBands();
    std::string getSelectedBands();
    void setSelectedBands(std::string);

private:
    int _numberOfProfiles = 6;

    // Revert default band table
    std::map<const std::string, const HexMask> mapAvailableBand;
};

DEFINE_PLUGIN(ServicePlugin, "deviceInformationService", PLUGIN_NAME)

ServicePlugin::ServicePlugin(void) {
    // Init RDB Containter object for simApnCharacteristic
    for (int i = 1; i <= _numberOfProfiles; ++i) {
        auto &profile = cRdbLinkProfiles.addChild(i);
        profile.addChildren({
                "apn",
                "status_ipv4",
                "status_ipv6",
                });
    }

    // build revert default band table
    estd::DefValueMap<std::string, bool> bandMaskKeys = {
        {"GSM",   true},
        {"WCDMA", true},
        {"LTE",   true}
    };
    std::string supportedBand = rdbSupportedBandMask.get(true).toStdString("");
    std::string fixedAvailableBand = rdbFixedAvailableBandMask.get(true).toStdString("");
    auto defaultBandList = estd::split(fixedAvailableBand == ""? supportedBand:fixedAvailableBand, ',');
    for(auto &elem : defaultBandList) {
        auto detail = estd::split(elem, ':');
        if (detail.size() == 2) {
            if (bandMaskKeys.get(detail[0], false)) {
                mapAvailableBand.insert(std::make_pair(detail[0], detail[1]));
            }
        }
    }
}

// get Device Version info
std::string ServicePlugin::getVersion() {
    return estd::format("{\"GATT\":\"%s\",\"Hardware\":\"%s\",\"Software\":\"%s\"}",
            GGK_VERSION,
            rdbHwVer.get(true).toCharString(),
            rdbSwVer.get(true).toCharString());
}

// get Device family enum
uint8_t ServicePlugin::getDevFamily() {
    return enumFamily.get(rdbProdModel.get(true).toStdString(), 2); // default value is 2
}

// get Device Identifiers
std::string ServicePlugin::getDevIdentifiers() {
    return estd::format("{\"Serial Number\":\"%s\",\"IMEI\":\"%s\"}",
            rdbDevSerialNum.get(true).toCharString(),
            rdbDevIMEI.get(true).toCharString());
}

// get Device Network Identifiers
std::string ServicePlugin::getDevNetIdentifiers() {
    json j;
    j["Current PLMN"] = rdbDevNetPLMN.get(true).toStdString();
    j["Short Network Name"] = rdbDevNetName.get(true).toStdString();
    j["Long Network Name"] = rdbDevNetLongName.get(true).toStdString();
    return j.dump();
}

// get Tr069 Status
//
// Container with ACS Server URL and last connection time in UTC.
// Example: {"ACS Server":"http://10.0.0.123/cwmp.php","Last Connect":"2019-08-24 14:40:22"}
std::string ServicePlugin::getTr069Status() {
    std::string result;
    std::smatch sm;
    std::regex regex_datetime("(\\b\\d{4}[-]\\d{2}[-]\\d{2} \\d{2}:\\d{2}:\\d{2})\\s.*");

    /* example of return value of "tr069.informStartAt" ==> "2020-03-03 04:53:14 [1 BOOT]" */
    std::string rdbConnTime = rdbTr069ConnTime.get(true).toStdString();

    if ( std::regex_match(rdbConnTime, sm, regex_datetime) ) {
        result = sm[1];
    }
    return estd::format("{\"ACS Server\":\"%s\",\"Last Connect\":\"%s\"}",
            rdbTr069AcsUrl.get(true).toCharString(),
            result.c_str()
            );
}

// get Device State
//
// Device State enum
// (STATE_BOOTING, STATE_SCANNING, STATE_REGISTERED, STATE_ATTACHED, STATE_ERROR)
// @TODO: Need to verify this function after rdb_qcmap_bridge implemented.
ServicePlugin::enumDevState ServicePlugin::getDevState() {
    // 0x00 - NOT_REGISTERED - Not registered; mobile is not currently searching for a new network to provide service
    // 0x01 - REGISTERED - Registered with a network
    // 0x02 - NOT_REGISTERED_SEARCHING - Not registered, but mobile is currently searching for a new network to provide service
    // 0x03 - REGISTRATION_DENIED - Registration denied by the visible network
    // 0x04 - REGISTRATION_UNKNOWN - Registration state is unknown
    int iRegState = 0;
    erdb::RdbValue regState = rdbModemRegState.get(true);
    erdb::RdbValue attState = rdbModemAttState.get(true);

    if (!regState.isAvail() || !regState.isSet())
        return enumDevState::STATE_BOOTING;

    if (attState.toInt<int>() == 1)
        return enumDevState::STATE_ATTACHED;

    iRegState = regState.toInt<int>();
    if (iRegState == 1)
        return enumDevState::STATE_REGISTERED;

    if (iRegState == 0 || iRegState == 2)
        return enumDevState::STATE_SCANNING;

    return enumDevState::STATE_ERROR;
}

// get Device Error message
//
// @TODO: Need to verify this function after rdb_qcmap_bridge implemented.
//
// Possible return from rdb variable
// * rdbSysMode
//      ["no service"|"limited service"|"limited regional service"|"power save"|specific service name]
// * rdbSimStatus
//      ["SIM not inserted"|"SIM OK"|"SIM BUSY"|"SIM PIN"|"SIM PUK"|"SIM BLOCKED"|"SIM ERR"]
std::string ServicePlugin::getDevError() {
    std::string sysMode = rdbSysMode.get(true).toStdString();
    std::string simStatus = rdbSimStatus.get(true).toStdString();

    if (simStatus == "SIM OK") {
        return devErrSysMode.get(sysMode, "");
    } else {
        return simStatus;
    }
}

// get SIM and APN info.
//
// Only list up active APN and its IP Connectivity.
std::string ServicePlugin::getSimApn() {
    json j;
    std::tuple<std::string,std::string> connectedOrNot = {"up",""};
    bool isIpV4Up = false;
    bool isIpV6Up = false;

    for (int i = 1; i <= _numberOfProfiles; ++i) {
        erdb::Rdb &rdbProfile = cRdbLinkProfiles.children[i];

        if (rdbProfile.children.contains(std::string("status_ipv4"))) {
            isIpV4Up = rdbProfile.children[std::string("status_ipv4")].get(true).toBool(connectedOrNot, false);
        } else {
            isIpV4Up = false;
        }
        if (rdbProfile.children.contains(std::string("status_ipv6"))) {
            isIpV6Up = rdbProfile.children[std::string("status_ipv6")].get(true).toBool(connectedOrNot, false);
        } else {
            isIpV6Up = false;
        }

        std::string ipConn = "IP";

        if (isIpV4Up || isIpV6Up) {
            if (isIpV4Up) {
                ipConn += "v4";
            }
            if (isIpV6Up) {
                ipConn += "v6";
            }
            j[estd::format("Prof%d",i)] = {
                {"APN",rdbProfile.children[std::string("apn")].get(true).toStdString()},
                {"IP Conn", ipConn}
            };
        }
    }

    // ["SIM not inserted"|"SIM OK"|"SIM BUSY"|"SIM PIN"|"SIM PUK"|"SIM BLOCKED"|"SIM ERR"]
    j["SIM Status"] = rdbSimStatus.get(true).toStdString();

    return j.dump();
}

std::string ServicePlugin::getSupportedBands() {
    // Supported Band list in hex format.(GSM:HexMask,WCDMA:HexMask,LTE:HexMask)
    // Ex: GSM:0x0000000000000000,WCDMA:0x100600000fc00000,LTE:0x420000a3e23b0f38df
    json j;
    int minBytes;
    std::string bandMasks = rdbSupportedBandMask.get(true).toStdString();
    auto maskList = estd::split(bandMasks, ',');

    for(auto &elem : maskList) {
        auto detail = estd::split(elem, ':');
        if (detail.size() == 2) {
            minBytes = bandMaskWidth.get(detail[0], 0); // If a name is not on the list(bandMaskWidth), just skip.
            if (minBytes != 0) {
                try {
                    j[detail[0]] = base64Obj.cvtHexStrToBase64(detail[1], minBytes);
                }
                catch(const std::invalid_argument &ex) {
                    log(LOG_INFO, "Error: Invalid argument: %s", ex.what());
                    j[detail[0]] = "";
                }
            }
        }
    }
    return j.dump();
}

std::string ServicePlugin::getSelectedBands() {
    // Selected Band list in hex format.(GSM:HexMask,WCDMA:HexMask,LTE:HexMask)
    // Ex: GSM:0x0000000000000000,WCDMA:0x100600000fc00000,LTE:0x420000a3e23b0f38df
    json j;
    int minBytes;
    std::string bandMasks = rdbSelectedBandMask.get(true).toStdString();
    auto maskList = estd::split(bandMasks, ',');

    for(auto &elem : maskList) {
        auto detail = estd::split(elem, ':');
        if (detail.size() == 2) {
            minBytes = bandMaskWidth.get(detail[0], 0); // If a name is not on the list(bandMaskWidth), just skip.
            if (minBytes != 0) {
                try {
                    j[detail[0]] = base64Obj.cvtHexStrToBase64(detail[1], minBytes);
                }
                catch(const std::invalid_argument &ex) {
                    log(LOG_INFO, "Error: Invalid argument: %s", ex.what());
                    j[detail[0]] = "";
                }
            }
        }
    }

    j["Non-Persist"] = rdbRevertBandMode.get(true).toStdString() == "factory_setting" ? "1" : "0";

    return j.dump();
}

void ServicePlugin::setSelectedBands(std::string setVal) {
    /*
     * Example
     * Input:
     *   {"WCDMA":"AAAAAAAAAAAQBgAAD8AAAA==","LTE":"AAAAAAAAAEIAAKPiOw843w==","Non-Persist":"1"}
     * Output:
     *    WCDMA:0x0000000000000000100600000FC00000,LTE:0x00000000000000420000A3E23B0F38DF
     */
    int minBytes;
    json newV = json::parse(setVal);
    std::string rdbSetVal, base64Val;
    bool non_persist = false;

    for(auto &elem : newV.items()) {
        std::string name = elem.key(), value = elem.value();
        if (name == "Non-Persist") {
            if(value == "1") {
                non_persist = true;
            }
            continue;
        }

        minBytes = bandMaskWidth.get(name, 0); // If a name is not on the list(bandMaskWidth), just skip.
        if (minBytes > 0) {
            try {
                base64Val = base64Obj.cvtBase64ToHexStr(value);
            }
            catch(const std::invalid_argument &ex) {
                log(LOG_INFO, "Error: Invalid argument: %s", ex.what());
                continue;
            }

            auto searchDefault = mapAvailableBand.find(name);
            if (searchDefault != mapAvailableBand.end()) { // the band has default bandmask.
                try {
                    base64Val = searchDefault->second & base64Val;
                }
                catch(const std::invalid_argument &ex) {
                    log(LOG_INFO, "Error: Invalid argument: %s", ex.what());
                    continue;
                }
            }

            if (rdbSetVal.empty()) {
                rdbSetVal = name + ":0x" + base64Val;
            } else {
                rdbSetVal = rdbSetVal + "," + name + ":0x" + base64Val;
            }
        }
    }

    if (non_persist && rdbRevertBandMode.get(true).toStdString() != "factory_setting") {
        rdbRevertBandMode.set("factory_setting", true);
    } else if (!non_persist && rdbRevertBandMode.get(true).toStdString() != "no_change") {
        rdbRevertBandMode.set("no_change", true);
    }

    if (!rdbSetVal.empty()) {
        rdbParamSelectedBand.set(rdbSetVal, true);
        rdbCmdSelectedBand.set("set_hexmask", true);
    }
}

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("deviceInformationService", DEVICE_INFORMATION_UUID)
        .gattCharacteristicBegin("deviceVersionCharacteristic", DEVICE_VERSION_UUID, {"encrypt-read"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getVersion(), true);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Device Version";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("deviceFamilyCharacteristic", DEVICE_FAMILY_UUID, {"encrypt-read"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getDevFamily(), true);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Device Family";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("deviceIdentifiersCharacteristic", DEVICE_IDENTIFIERS_UUID, {"encrypt-read"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getDevIdentifiers(), true);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Device Identifiers";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("deviceNetworkIdentifiersCharacteristic", DEVICE_NETWORK_IDENTIFIERS_UUID, {"encrypt-read", "notify"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getDevNetIdentifiers(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getDevNetIdentifiers());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbDevNetPLMN);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbDevNetName);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbDevNetLongName);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbDevNetPLMN);
                RDB_UNSUBSCRIBE(rdbDevNetName);
                RDB_UNSUBSCRIBE(rdbDevNetLongName);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Device Network Identifiers";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("stateCharacteristic", STATE_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, (uint8_t)PLUGIN(ServicePlugin)->getDevState(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, (uint8_t)PLUGIN(ServicePlugin)->getDevState());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbModemRegState);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbModemAttState);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbModemRegState);
                RDB_UNSUBSCRIBE(rdbModemAttState);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Device State";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("errorCharacteristic", ERROR_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getDevError(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getDevError());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbSysMode);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbSimStatus);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbSysMode);
                RDB_UNSUBSCRIBE(rdbSimStatus);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Error Message";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("simApnCharacteristic", SIM_APN_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getSimApn(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getSimApn());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // Always on, because this could not handle RDB Containter.
                // This callback still needs to process StartNotify method.
                ;
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // Always on, because this could not handle RDB Containter.
                // This callback still needs to process StartNotify method.
                ;
            })
            .registerNotifyTrigger(CHARACTERISTIC_REGISTER_NOTIFY_LAMBDA
            {
                PLUGIN(ServicePlugin)->cRdbLinkProfiles.runForChildren([&self](erdb::Rdb &rdb) {
                        rdb.subscribeForChange([&self](erdb::Rdb &rdb) {
                                ggkNofifyUpdatedCharacteristic(self.getPath().c_str());
                                }, false);
                        });
                PLUGIN(ServicePlugin)->rdbSimStatus.subscribeForChange([&self](erdb::Rdb &rdb) {
                        ggkNofifyUpdatedCharacteristic(self.getPath().c_str());
                        }, false);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "SIM and APN Status";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("connectivityCharacteristic", CONNECTIVITY_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporarily set to NA.
                const char *pTextString = "NA";
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                // @TODO:: Temporarily set to NA.
                const char *pTextString = "NA";
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporarily set to NA.
                ;
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporarily set to NA.
                ;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Connectivity";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("ipAddressesCharacteristic", IP_ADDRESSES_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporarily set to NA.
                const char *pTextString = "NA";
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                // @TODO:: Temporarily set to NA.
                const char *pTextString = "NA";
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporarily set to NA.
                ;
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporarily set to NA.
                ;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "IP Addresses";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("tr069StatusCharacteristic", TR069_STATUS_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getTr069Status(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getTr069Status());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbTr069AcsUrl);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbTr069ConnTime);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbTr069AcsUrl);
                RDB_UNSUBSCRIBE(rdbTr069ConnTime);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "TR-069 Status";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("supportedBandsCharacteristic", SUPPORTED_BANDS_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getSupportedBands(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getSupportedBands());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbSupportedBandMask);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbSupportedBandMask);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Supported Bands";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("bandLockCharacteristic", BANDLOCK_UUID, {"encrypt-read", "notify", "encrypt-write"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getSelectedBands(), true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                PLUGIN(ServicePlugin)->setSelectedBands(Utils::stringFromGVariantByteArray(pAyBuffer));
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getSelectedBands());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbSelectedBandMask);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbSelectedBandMask);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Band lock bitfields";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("gpsMagneticCharacteristic", GPS_MAGNETIC_DATA_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporary data for release. Need to fix later.
                const char *pTextString = "{\"Antenna Azimuth\":\"NA\",\"Antenna Downtilt\":\"NA\",\"Height\":\"NA\",\"Latitude\":\"NA\",\"Longitude\":\"NA\",\"MagneticStatus\":\"NA\"}";
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                // @TODO:: Temporary data for release. Need to fix later.
                const char *pTextString = "{\"Antenna Azimuth\":\"NA\",\"Antenna Downtilt\":\"NA\",\"Height\":\"NA\",\"Latitude\":\"NA\",\"Longitude\":\"NA\",\"MagneticStatus\":\"NA\"}";
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporary data for release. Need to fix later.
                ;
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporary data for release. Need to fix later.
                ;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "GPS, Magnetic Data";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("batteryCharacteristic", BATTERY_DATA_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporary data for release. Need to fix later.
                const char *pTextString = "{\"Battery Level\":\"NA\",\"Battery Voltage\":\"NA\"}";
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                // @TODO:: Temporary data for release. Need to fix later.
                const char *pTextString = "{\"Battery Level\":\"NA\",\"Battery Voltage\":\"NA\"}";
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporary data for release. Need to fix later.
                ;
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                // @TODO:: Temporary data for release. Need to fix later.
                ;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Battery Data";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
