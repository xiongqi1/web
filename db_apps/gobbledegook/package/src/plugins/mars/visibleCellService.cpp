#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"

// JSON library from https://github.com/nlohmann/json
#include "json.hpp"

#include "uuids.h" // our custom UUIDs
#include "CasaData.h"
#include "erdb.hpp"
#include "pluginCommon.h"

//TODO:: temporary log message for cutomer requirement. Remove later. (Related Jira task: NEP-404)
#include "elogger.hpp"

// TODO:: Related Jira task: NEP-366.
// This is to temporarily enable NR5G Non-Standalone mode with qdiag.
// This part is supposed to be re-written if QMI NR5G cell info is implemmented.
// #define TEMP_FOR_NR5G_NSA

// for convenience
using json = nlohmann::json;

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);
    ServicePlugin();

    erdb::Rdb rdbVisibleCellList = erdb::Rdb("wwan.0.cell_measurement.cell_list");
    erdb::Rdb rdbVisibleCellReq = erdb::Rdb("service.gatt.visual_cell_request");
    erdb::Rdb rdbVisibleCellInfo = erdb::Rdb("service.gatt.visual_cell_info");
    erdb::Rdb rdbServingCellSINR = erdb::Rdb("wwan.0.signal.snr");
    erdb::Rdb rdbServingCellGCID = erdb::Rdb("wwan.0.system_network_status.CellID");

#ifdef TEMP_FOR_NR5G_NSA
    erdb::Rdb rdbNr5gUp = erdb::Rdb("wwan.0.radio_stack.nr5g.up");
    erdb::Rdb cRdbNr5g = erdb::Rdb("wwan.0.radio_stack.nr5g"); // RDB Containter object for NR5G
#endif

    estd::DefValueMap<std::string, std::string> cellRAT = {
        {"U", "WCDMA"},
        {"E", "LTE"},
        {"N", "NR5G"},
    };

    std::string getVisibleCellList();
    std::string getVisibleCellReq();
    void setVisibleCellReq(std::string);
    std::string getVisibleCellInfo();
};

DEFINE_PLUGIN(ServicePlugin, "visibleCellService", PLUGIN_NAME)

ServicePlugin::ServicePlugin(void) {
#ifdef TEMP_FOR_NR5G_NSA
    cRdbNr5g.addChildren({
            "arfcn",
            "pci",
            "rsrp",
            "rsrq",
            "snr",
            });
#else
    ; //nop
#endif
}

// get Visible Cell List
std::string ServicePlugin::getVisibleCellList() {
    /* Comma delimited list of currently visible PCIs
     *  A format of each field is [G|U|E|N]:ARFCN:PCI
     *  (G: GSM, U: WCDMA, E: LTE, N: NR5G)
     * Ex) E:2950:474,E:2950:342,E:2950:343
     */
    std::string visibleCellList = rdbVisibleCellList.get(true).toStdString();

#ifdef TEMP_FOR_NR5G_NSA
    // @TODO:: It's not possible to use toBool() method,
    //         bacause it has a segmentation fault if the rdb variable does not exist.
    //         Need to apply toBool() method, after this issue is fixed.
    // std::tuple<std::string,std::string> upOrNot = {"UP",""};
    // bool isNr5gUp = rdbNr5gUp.get(true).toBool(upOrNot, false);
    // if (isNr5gUp) {}

    if (rdbNr5gUp.get(true).toStdString() == "UP") {
        std::string nr_arfcn = cRdbNr5g.children["arfcn"].get(true).toStdString();
        std::string nr_pci = cRdbNr5g.children["pci"].get(true).toStdString();
        if (! nr_arfcn.empty() && ! nr_pci.empty()) {
            if (! visibleCellList.empty()) {
                //Add comma
                visibleCellList = estd::format("%s,", visibleCellList.c_str());
            }
            visibleCellList = estd::format("%sN:%s:%s", visibleCellList.c_str(), nr_arfcn.c_str(), nr_pci.c_str());
        }
    }
#endif

    if (visibleCellList.empty()) {
        visibleCellList = "NA";
    }

    //TODO:: temporary log message for cutomer requirement. Remove later. (Related Jira task: NEP-404)
    {
        int numOfChars = 128;
        int numOfLines = ceil(visibleCellList.length() / (double)numOfChars);
        for (int line = 0; line < numOfLines; line++) {
            log(LOG_DEBUG, "Visible Cell List: %d/%d [%s]", (line+1), numOfLines, visibleCellList.substr(line*numOfChars, numOfChars).c_str());
        }
    }
    return visibleCellList;
}

// get Visible Cell Request
//
std::string ServicePlugin::getVisibleCellReq() {
    return rdbVisibleCellReq.get(true).toStdString();
}

void ServicePlugin::setVisibleCellReq(std::string setVal) {
    /*
     * Example
     * Input: value of a request is an element on the rdbVisibleCellList.
     *   E:2950:474
     * Output: The output value is set to rdbVisibleCellInfo.
     *   {"Cell ID":"0x07F1E115","Cell Technology":"LTE","Cell Type":"SERV","ARFCN":"2950","PCI":"474","RSRP":"-104","RSRQ":"-11","SINR":"12","Role":"NA"}
     *   "Cell ID": Global Cell ID
     *   "Cell Technology": Radio Access Technology ["WCDMA"|"LTE"|"NR5G"]
     *   "Cell Type": ["SERV"|"NEIG"], "SERV" -> Serving cell, "NEIG" -> Neighbour cell
     *   "ARFCN": Absolute Radio-Frequency Channel Number
     *   "PCI": Physical Cell Id
     *   "RSRP, "RSRQ", "SINR"
     *   "Role": ["PCC"|"SCC"|"NA"], "PCC" -> Primary Component Carrier, "SCC" -> Secondary Component Carrier, "NA" -> Not applicable
     */
    json jRetVal = {
        { "Cell ID", "NA" }, { "Cell Technology", "NA" }, { "Cell Type", "NA" }, { "ARFCN", "NA" },
        { "PCI", "NA" }, { "RSRP", "NA" }, { "RSRQ", "NA" }, { "SINR", "NA" }, { "Role","NA" }
    };
    const char *rdbPrefix = "wwan.0.cell_measurement.";
    int index = 0;

    if (setVal.empty()) {
        rdbVisibleCellInfo.set(jRetVal.dump(), true);
        return;
    }

#ifdef TEMP_FOR_NR5G_NSA
    if (setVal.front() == 'N') {
        if (rdbNr5gUp.get(true).toStdString() == "UP") {
            std::string nr_arfcn = cRdbNr5g.children["arfcn"].get(true).toStdString();
            std::string nr_pci = cRdbNr5g.children["pci"].get(true).toStdString();

            std::string nr_rsrp = cRdbNr5g.children["rsrp"].get(true).toStdString();
            std::string nr_rsrq = cRdbNr5g.children["rsrq"].get(true).toStdString();
            std::string nr_snr = cRdbNr5g.children["snr"].get(true).toStdString();

            std::string curr_nr5g_cell_info = estd::format("N:%s:%s", nr_arfcn.c_str(), nr_pci.c_str());
            if (curr_nr5g_cell_info == setVal) {
                jRetVal["Cell Technology"] = cellRAT.get("N", "");
                jRetVal["ARFCN"] = nr_arfcn;
                jRetVal["PCI"] = nr_pci;
                jRetVal["Cell Type"] = "SERV";
                if (! nr_rsrp.empty()) {
                    jRetVal["RSRP"] = nr_rsrp;
                }
                if (! nr_rsrq.empty()) {
                    jRetVal["RSRQ"] = nr_rsrq;
                }
                if (! nr_snr.empty()) {
                    jRetVal["SINR"] = nr_snr;
                }
            }
        }
        rdbVisibleCellInfo.set(jRetVal.dump(), true);
        return;
    }
#endif

    auto cellList = estd::split(rdbVisibleCellList.get(true).toStdString(), ',');
    for(auto &elem : cellList) {
        if (elem == setVal) {
            // @TODO:: Need to add NR5G case.
            // * LTE: 0. "E", *  1. earfcn, *  2. pci, *  3. rsrp, *  4. rsrq
            // * UMTS: 0. "U", *  1. uarfcn, *  2. psc, *  3. cpich_rscp, *  4. cpich_ecno
            auto matchedCell = estd::split(erdb::Rdb(estd::format("%s%d", rdbPrefix, index)).get(true).toStdString(), ',');
            if (matchedCell.size() >= 5) {
                std::string rat = cellRAT.get(matchedCell[0], "");
                if (!rat.empty()) {
                    // TODO:: Not applicable. Need to add later.
                    // jRetVal["Role"] = ;
                    jRetVal["Cell Technology"] = rat;
                    jRetVal["ARFCN"] = matchedCell[1];
                    jRetVal["PCI"] = matchedCell[2];
                    jRetVal["RSRP"] = matchedCell[3];
                    jRetVal["RSRQ"] = matchedCell[4];
                    if(index == 0) { // Serving cell
                        jRetVal["Cell ID"] = rdbServingCellGCID.get(true).toStdString();
                        jRetVal["Cell Type"] = "SERV";
                        jRetVal["SINR"] = rdbServingCellSINR.get(true).toStdString();
                    } else {
                        jRetVal["Cell Type"] = "NEIG";
                    }
                }
            }
            break;
        }
        index++;
    }
    rdbVisibleCellInfo.set(jRetVal.dump(), true);
}

// get Visible Cell result requested
std::string ServicePlugin::getVisibleCellInfo() {
    //TODO:: temporary log message for cutomer requirement. Remove later. (Related Jira task: NEP-404)
    log(LOG_ERR, "Get Visible Cell Info reguired: [%s]", rdbVisibleCellInfo.get(true).toCharString());
    return rdbVisibleCellInfo.get(true).toStdString();
}

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("visibleCellService", VISIBLE_CELL_UUID)
        .gattCharacteristicBegin("visibleCellListCharacteristic", VISIBLE_CELL_LIST_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getVisibleCellList(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getVisibleCellList());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbVisibleCellList);
#ifdef TEMP_FOR_NR5G_NSA
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNr5gUp);
#endif
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbVisibleCellList);
#ifdef TEMP_FOR_NR5G_NSA
                RDB_UNSUBSCRIBE(rdbNr5gUp);
#endif
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Visible Cell list";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("visibleCellRequestCharacteristic", VISIBLE_CELL_REQUEST_UUID, {"encrypt-read", "notify", "encrypt-write"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getVisibleCellReq(), true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                //TODO:: temporary log message for cutomer requirement. Remove later. (Related Jira task: NEP-404)
                log(LOG_ERR, "Received Visible Cell Request: [%s]", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                PLUGIN(ServicePlugin)->setVisibleCellReq(Utils::stringFromGVariantByteArray(pAyBuffer));
                ggkNofifyUpdatedCharacteristic("/com/gobbledegook/visibleCellService/visibleCellInfoCharacteristic");
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getVisibleCellReq());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbVisibleCellReq);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbVisibleCellReq);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Visible Cell request";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("visibleCellInfoCharacteristic", VISIBLE_CELL_INFO_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getVisibleCellInfo(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getVisibleCellInfo());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                ; // Notification is triggered via visibleCellRequestCharacteristic. Don't need to subscribe rdb variable.
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                ; // Notification is triggered via visibleCellRequestCharacteristic. Don't need to subscribe rdb variable.
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Requested Cell information";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
