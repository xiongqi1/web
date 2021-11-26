#include <regex>
#include <string>
#include <fstream>

#include "Base64.h"
#include "HexMask.h"
#include "Command.h"
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

// for convenience
using json = nlohmann::json;

//To enable or disable "Supported NR5G SA band for cell locking" Characteristic.
#define ENABLE_SUPPORTED_NR5G_SA_CELL_LOCK_BAND

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);
    ServicePlugin();

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
        {"LTE",  16},
        {"NR5G", 32},
        {"NR5GNSA", 32},
        {"NR5GSA", 32}
    };

    erdb::Rdb rdbHwVer = erdb::Rdb("system.product.hwver");
    erdb::Rdb rdbSwVer = erdb::Rdb("sw.version");
    erdb::Rdb rdbDevFamily = erdb::Rdb("service.gatt.dev_family");
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
    erdb::Rdb rdbDefaultBand = erdb::Rdb("wwan.0.currentband.revert_selband.factory_setting");
    erdb::Rdb rdbLastGoodBand = erdb::Rdb("wwan.0.currentband.revert_selband.last_good_band");
    erdb::Rdb rdbCustomizedBand = erdb::Rdb("wwan.0.currentband.revert_selband.customized_band_setting");
    erdb::Rdb rdbRevertBandMode = erdb::Rdb("wwan.0.currentband.revert_selband.mode");
    erdb::Rdb rdbNitConnected = erdb::Rdb("nit.connected");
    erdb::Rdb rdbNitBatteryLevel = erdb::Rdb("nit.battery.level").setRdbFlags(0).set("NA");
    erdb::Rdb rdbNitBatteryVoltage = erdb::Rdb("nit.battery.voltage").setRdbFlags(0).set("NA");
    erdb::Rdb rdbNitCompassBearing = erdb::Rdb("nit.compass.bearing_raw").setRdbFlags(0).set("NA");
    erdb::Rdb rdbNitCompassBearingCorrected = erdb::Rdb("nit.compass.bearing_corrected").setRdbFlags(0).set("NA");
    erdb::Rdb rdbNitCompassStatus = erdb::Rdb("nit.compass.status").setRdbFlags(0).set("NA");
    erdb::Rdb rdbNitDowntilt = erdb::Rdb("nit.downtilt").setRdbFlags(0).set("NA");
#ifdef ENABLE_SUPPORTED_NR5G_SA_CELL_LOCK_BAND
    erdb::Rdb rdbFixedAvailableBand = erdb::Rdb("wwan.0.fixed_module_band_list");
    erdb::Rdb rdbSupportedBand = erdb::Rdb("wwan.0.module_band_list");
#endif
    erdb::Rdb rdbCellLockListLTE = erdb::Rdb("wwan.0.modem_pci_lock_list");
    erdb::Rdb rdbCellLockListNR5G = erdb::Rdb("wwan.0.modem_pci_lock_list_5g");

    // RDB RPC for cell locking
    erdb::Rdb rdbRpcCellLockRatType = erdb::Rdb("wwan.0.cell_lock.cmd.param.rat");
    erdb::Rdb rdbRpcCellLockLockList = erdb::Rdb("wwan.0.cell_lock.cmd.param.lock_list");
    erdb::Rdb rdbRpcCellLockCmd = erdb::Rdb("wwan.0.cell_lock.cmd.command");
    erdb::Rdb rdbRpcCellLockStatus = erdb::Rdb("wwan.0.cell_lock.cmd.status");

    // RDB Containter object for simApnCharacteristic
    erdb::Rdb cRdbLinkProfiles = erdb::Rdb("link.profile");
    erdb::Rdb cRdbLinkPolicies = erdb::Rdb("link.policy");


    // RDB container object for GPS data
    erdb::Rdb cRdbSensorsGPS = erdb::Rdb("sensors.gps.0.common");

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
    std::string getGPSMagneticCharacteristic();
    std::string getBatteryCharacteristic();
    std::string getIpAddresses();
#ifdef ENABLE_SUPPORTED_NR5G_SA_CELL_LOCK_BAND
    std::string getCellLockSaBand();
#endif
    std::string getCellLockLte();
    bool setCellLockLte(std::string);
    std::string getCellLockNr5g();
    bool setCellLockNr5g(std::string);
    std::string getConfigIds();

private:
    int _numberOfProfiles = 6;

    // Revert default band table
    std::map<const std::string, const HexMask> mapAvailableBand;
};

DEFINE_PLUGIN(ServicePlugin, "deviceInformationService", "saturn")

ServicePlugin::ServicePlugin(void) {
    // Init RDB Containter object for simApnCharacteristic
    for (int i = 1; i <= _numberOfProfiles; ++i) {
        auto &profile = cRdbLinkProfiles.addChild(i);
        auto &policy = cRdbLinkPolicies.addChild(i);

        profile.addChildren({
                "apn",
                "apn_type",
                });
        policy.addChildren({
                "connect_progress_v4",
                "connect_progress_v6",
                "enable",
                "iplocal",
                "ipv6_ipaddr",
                });
    }

    cRdbSensorsGPS.addChildren({"altitude", "latitude_degrees", "longitude_degrees", "valid", "vertical_uncertainty", "horizontal_uncertainty"});

    // build revert default band table
    estd::DefValueMap<std::string, bool> bandMaskKeys = {
        {"GSM",   true},
        {"WCDMA", true},
        {"LTE",   true},
        {"NR5G",  true},
        {"NR5GNSA",  true},
        {"NR5GSA",  true}
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
    return rdbDevFamily.get(true).toInt(255);
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

    for (int i = 1; i <= _numberOfProfiles; ++i) {
        erdb::Rdb &rdbProfile = cRdbLinkProfiles.children[i];
        erdb::Rdb &rdbPolicy = cRdbLinkPolicies.children[i];

        std::tuple<std::string,std::string> connectedOrNot = {"connected",""};
        bool isIpV4Up = rdbPolicy.children["connect_progress_v4"].get(true).toBool(connectedOrNot, false);
        bool isIpV6Up = rdbPolicy.children["connect_progress_v6"].get(true).toBool(connectedOrNot, false);

        std::string ipConn = "IP";

        if (isIpV4Up || isIpV6Up) {
            if (isIpV4Up) {
                ipConn += "v4";
            }
            if (isIpV6Up) {
                ipConn += "v6";
            }
            j[estd::format("Prof%d",i)] = {
                {"APN",rdbProfile.children["apn"].get(true).toStdString()},
                {"IP Conn", ipConn}
            };
        }
    }

    // ["SIM not inserted"|"SIM OK"|"SIM BUSY"|"SIM PIN"|"SIM PUK"|"SIM BLOCKED"|"SIM ERR"]
    j["SIM Status"] = rdbSimStatus.get(true).toStdString();

    return j.dump();
}

std::string ServicePlugin::getSupportedBands() {
    // Supported Band list in hex format.(GSM:HexMask,WCDMA:HexMask,LTE:HexMask,NR5G:HexMask)
    // Ex: GSM:0x0000000000000000,WCDMA:0x100600000fc00000,LTE:0x420000a3e23b0f38df,NR5G:0x00000000010f38df
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
    // Selected Band list in hex format.(GSM:HexMask,WCDMA:HexMask,LTE:HexMask,NR5G:HexMask)
    // Ex: GSM:0x0000000000000000,WCDMA:0x100600000fc00000,LTE:0x420000a3e23b0f38df,NR5G:0x0000000000000000
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

    j["Non-Persist"] = rdbRevertBandMode.get(true).toStdString() == "customized_band_setting" ? "1" : "0";

    return j.dump();
}

void ServicePlugin::setSelectedBands(std::string setVal) {
    /*
     * Example
     * Input:
     *   {"WCDMA":"AAAAAAAAAAAQBgAAD8AAAA==","LTE":"AAAAAAAAAEIAAKPiOw843w==","NR5G":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=","Non-Persist":"1"}
     * Output:
     *    WCDMA:0x0000000000000000100600000FC00000,LTE:0x00000000000000420000A3E23B0F38DF,NR5G:0x0000000000000000000000000000000000000000000000000000000000000000
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

    std::string revertBandMode = rdbRevertBandMode.get(true).toStdString();
    if (non_persist && revertBandMode != "customized_band_setting") {
        std::string defaultBand = rdbDefaultBand.get(true).toStdString();
        std::string lastGoodBand = rdbLastGoodBand.get(true).toStdString();
        rdbCustomizedBand.set((lastGoodBand != "") ? lastGoodBand : defaultBand, true);
        rdbRevertBandMode.set("customized_band_setting", true);
    } else if (!non_persist && revertBandMode != "no_change") {
        rdbRevertBandMode.set("no_change", true);
        rdbCustomizedBand.set("", true);
    }

    if (!rdbSetVal.empty()) {
        rdbParamSelectedBand.set(rdbSetVal, true);
        rdbCmdSelectedBand.set("set_hexmask", true);
    }
}

std::string ServicePlugin::getGPSMagneticCharacteristic() {
    json j;

    if (!rdbNitConnected.get(true).toBool()) {
        // Only write to RDB database if different, to avoid too many callbacks to this function
        rdbNitCompassBearing.set("NA", true, true);
        rdbNitCompassBearingCorrected.set("NA", true, true);
        rdbNitDowntilt.set("NA", true, true);
        rdbNitCompassStatus.set("NA", true, true);
    }

    // Use true north corrected compass bearing when status is 0
    // (lark sensors calibrated, gps available)
    auto rdb_compass_status = rdbNitCompassStatus.get(true);
    int status = rdb_compass_status.toInt(-1);
    if (status == 0) {
        j["Antenna Azimuth"] = rdbNitCompassBearingCorrected.get(true).toStdString();
    } else {
        j["Antenna Azimuth"] = rdbNitCompassBearing.get(true).toStdString();
    }
    j["Antenna Downtilt"] = rdbNitDowntilt.get(true).toStdString();
    j["MagneticStatus"] = rdb_compass_status.toStdString();

    std::string height = "NA";
    std::string latitude = "NA";
    std::string longitude = "NA";
    std::string verUncertainty = "NA";
    std::string horUncertainty = "NA";
    if (cRdbSensorsGPS.children["valid"].get(true).toStdString() == "valid") {
        // convert to double in string that has 6 digits after decimal point.
        try {
            height = std::to_string(std::stod(cRdbSensorsGPS.children["altitude"].get(true).toStdString()));
        } catch(...) {}
        try {
            latitude = std::to_string(std::stod(cRdbSensorsGPS.children["latitude_degrees"].get(true).toStdString()));
        } catch(...) {}
        try {
            longitude = std::to_string(std::stod(cRdbSensorsGPS.children["longitude_degrees"].get(true).toStdString()));
        } catch(...) {}
        try {
            verUncertainty = std::to_string(std::stod(cRdbSensorsGPS.children["vertical_uncertainty"].get(true).toStdString()));
        } catch(...) {}
        try {
            horUncertainty = std::to_string(std::stod(cRdbSensorsGPS.children["horizontal_uncertainty"].get(true).toStdString()));
        } catch(...) {}
    }
    j["Height"] = height;
    j["Latitude"] = latitude;
    j["Longitude"] = longitude;
    j["VerticalUncertainty"] = verUncertainty;
    j["HorizontalUncertainty"] = horUncertainty;

    return j.dump();
}

std::string ServicePlugin::getBatteryCharacteristic() {
    if (!rdbNitConnected.get(true).toBool()) {
        // Only write to RDB database if different, to avoid too many callbacks to this function
        rdbNitBatteryLevel.set("NA", true, true);
        rdbNitBatteryVoltage.set("NA", true, true);
    }

    return estd::format("{\"Battery Level\":\"%s\",\"Battery Voltage\":\"%s\"}",
            rdbNitBatteryLevel.get(true).toCharString(),
            rdbNitBatteryVoltage.get(true).toCharString()
            );
}


// get IP addresses and LAN port connection status.
std::string ServicePlugin::getIpAddresses() {
    json j;
    std::string etherStatus = "NA";
    std::string wanIpv4Addr = "NA";
    std::string wanIpv6Addr = "NA";
    std::ifstream ethernetStatusFile("/sys/class/net/eth0/operstate");
    std::string lineCont;

    // set WAN IP addresses
    for (int i = 1; i <= _numberOfProfiles; ++i) {
        erdb::Rdb &rdbProfile = cRdbLinkProfiles.children[i];
        erdb::Rdb &rdbPolicy = cRdbLinkPolicies.children[i];

        if (rdbPolicy.children["enable"].get(true).toBool()
            && rdbProfile.children["apn_type"].get(true).toStdString().find("default") != std::string::npos) {
            std::tuple<std::string,std::string> connectedOrNot = {"connected",""};
            bool isIpV4Up = rdbPolicy.children["connect_progress_v4"].get(true).toBool(connectedOrNot, false);
            bool isIpV6Up = rdbPolicy.children["connect_progress_v6"].get(true).toBool(connectedOrNot, false);

            if (isIpV4Up) {
                wanIpv4Addr = rdbPolicy.children["iplocal"].get(true).toStdString("NA");
            }
            if (isIpV6Up) {
                wanIpv6Addr = rdbPolicy.children["ipv6_ipaddr"].get(true).toStdString("NA");
            }
            break;
        }
    }

    // Set LAN port status
    if (ethernetStatusFile.is_open()) {
        while (getline (ethernetStatusFile, lineCont)) {
            if (lineCont == "up" || lineCont == "down") {
                etherStatus = lineCont;
                break;
            }
        }
        ethernetStatusFile.close();
    }

    j["LAN IP"] = erdb::Rdb("link.profile.0.address").get(true).toStdString("NA");
    j["LAN Port Status"] = etherStatus;
    j["WAN IPv4"] = wanIpv4Addr;
    j["WAN IPv6"] = wanIpv6Addr;
    return j.dump();
}

#ifdef ENABLE_SUPPORTED_NR5G_SA_CELL_LOCK_BAND
std::string ServicePlugin::getCellLockSaBand() {
    std::string supportedBand = rdbSupportedBand.get(true).toStdString("");
    std::string fixedAvailableBand = rdbFixedAvailableBand.get(true).toStdString("");
    auto supportedBandList = estd::split(fixedAvailableBand == ""? supportedBand:fixedAvailableBand, '&');

    std::regex rgx(".*,NR5G SA BAND (\\d+)");
    std::smatch match;
    std::vector< int > vecBandNum;

    for(auto &elem : supportedBandList) {
        if (std::regex_search(elem, match, rgx)) {
            vecBandNum.push_back(std::stoi(match[1]));
        }
    }
    if (vecBandNum.size() > 0){
        json j(vecBandNum);
        return j.dump();
    }

    return "NA";
}
#endif

std::string ServicePlugin::getCellLockLte() {
    return rdbCellLockListLTE.get(true).toStdString("");
}

bool ServicePlugin::setCellLockLte(std::string setVal) {
    int rdbRpcTimeout = 3;
    int cnt = 0;

    std::string rpcStatus;
    rdbRpcCellLockRatType.set("lte", true);
    rdbRpcCellLockLockList.set(setVal, true);
    rdbRpcCellLockCmd.set("set", true);

    do {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        rpcStatus = rdbRpcCellLockStatus.get(true).toStdString();
        cnt++;
    } while(rpcStatus != "[done]" && rpcStatus != "[error]" && cnt < rdbRpcTimeout);

    if (rpcStatus == "[done]") {
        return true;
    }
    return false;
}

std::string ServicePlugin::getCellLockNr5g() {
    return rdbCellLockListNR5G.get(true).toStdString("");
}

bool ServicePlugin::setCellLockNr5g(std::string setVal) {
    int rdbRpcTimeout = 3;
    int cnt = 0;

    std::string rpcStatus;
    rdbRpcCellLockRatType.set("5g", true);
    rdbRpcCellLockLockList.set(setVal, true);
    rdbRpcCellLockCmd.set("set", true);

    do {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        rpcStatus = rdbRpcCellLockStatus.get(true).toStdString();
        cnt++;
    } while(rpcStatus != "[done]" && rpcStatus != "[error]" && cnt < rdbRpcTimeout);

    if (rpcStatus == "[done]") {
        return true;
    }
    return false;
}

std::string ServicePlugin::getConfigIds() {
    json j;
    j["CONFIG_RDB"] = json::array({"NA"});
    j["CONFIG_CERT"] = json::array({"NA"});
    j["CONFIG_MBN"] = json::array({"NA"});
    j["CONFIG_EFS"] = json::array({"NA"});

    std::smatch match;
    std::regex patt ("\\b(CONFIG_CERT|CONFIG_MBN|CONFIG_EFS|CONFIG_RDB)=([^\n]*)\n");
    try {
        CommandResult cmdResult = Command::exec("environment 2>/dev/null");
        if (cmdResult.exitStatus == 0) {
            std::string cmdOutput = cmdResult.output;
            while (std::regex_search (cmdOutput, match, patt)) {
                j[match[1]] = json::array();
                //Convert config id list in string to JSON array.
                auto idList = estd::split(std::string(match[2]), ',');
                for(auto &elem : idList) {
                    if (elem.size() > 0) {
                        j[match[1]].push_back(elem);
                    }
                }
                cmdOutput = match.suffix().str();
            }
        }
    }
    catch (...) {
        j["CONFIG_RDB"] = json::array({"NA"});
        j["CONFIG_CERT"] = json::array({"NA"});
        j["CONFIG_MBN"] = json::array({"NA"});
        j["CONFIG_EFS"] = json::array({"NA"});
    }
    return j.dump();
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
                PLUGIN(ServicePlugin)->cRdbLinkPolicies.runForChildren([&self](erdb::Rdb &rdb) {
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
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getIpAddresses(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getIpAddresses());
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
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getGPSMagneticCharacteristic(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getGPSMagneticCharacteristic());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNitConnected);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNitCompassBearing);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNitCompassBearingCorrected);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNitDowntilt);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbNitConnected);
                RDB_UNSUBSCRIBE(rdbNitCompassBearing);
                RDB_UNSUBSCRIBE(rdbNitCompassBearingCorrected);
                RDB_UNSUBSCRIBE(rdbNitDowntilt);
            })
            .registerNotifyTrigger(CHARACTERISTIC_REGISTER_NOTIFY_LAMBDA
            {
                PLUGIN(ServicePlugin)->cRdbSensorsGPS.runForChildren([&self](erdb::Rdb &rdb) {
                        rdb.subscribeForChange([&self](erdb::Rdb &rdb) {
                                ggkNofifyUpdatedCharacteristic(self.getPath().c_str());
                                }, false);
                        });
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
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getBatteryCharacteristic(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getBatteryCharacteristic());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNitConnected);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNitBatteryLevel);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNitBatteryVoltage);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbNitConnected);
                RDB_UNSUBSCRIBE(rdbNitBatteryLevel);
                RDB_UNSUBSCRIBE(rdbNitBatteryVoltage);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Battery Data";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

#ifdef ENABLE_SUPPORTED_NR5G_SA_CELL_LOCK_BAND
        .gattCharacteristicBegin("cellLockBandCharacteristic", CELL_LOCK_SA_BAND_UUID, {"encrypt-read"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getCellLockSaBand(), true);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Supported NR5G SA band for cell locking";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
#endif

        .gattCharacteristicBegin("cellLockLteCharacteristic", CELL_LOCK_LTE_UUID, {"encrypt-read", "encrypt-write", "notify"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getCellLockLte(), true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                if (PLUGIN(ServicePlugin)->setCellLockLte(Utils::stringFromGVariantByteArray(pAyBuffer))) {
                    self.methodReturnVariant(pInvocation, NULL);
                } else {
                    g_dbus_method_invocation_return_error(pInvocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Write Error");
                }
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getCellLockLte());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbCellLockListLTE);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbCellLockListLTE);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Cell locking on LTE";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("cellLockNr5gCharacteristic", CELL_LOCK_NR5G_UUID, {"encrypt-read", "encrypt-write", "notify"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getCellLockNr5g(), true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                if (PLUGIN(ServicePlugin)->setCellLockNr5g(Utils::stringFromGVariantByteArray(pAyBuffer))) {
                    self.methodReturnVariant(pInvocation, NULL);
                } else {
                    g_dbus_method_invocation_return_error(pInvocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Write Error");
                }
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getCellLockNr5g());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbCellLockListNR5G);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbCellLockListNR5G);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Cell locking on NR5G";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("configIdsCharacteristic", CONFIG_IDS_UUID, {"encrypt-read"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getConfigIds(), true);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Config IDs";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

    .gattServiceEnd();
    return true;
}

}; // namespace ggk
