#include <algorithm>

#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"

#include "uuids.h" // our custom UUIDs
#include "erdb.hpp"

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);

    erdb::Rdb rdbSignalStrength = erdb::Rdb("wwan.0.signal.0.rsrp");
    erdb::Rdb rdbModemRegState = erdb::Rdb("wwan.0.system_network_status.reg_stat");
    erdb::Rdb rdbModemAttState = erdb::Rdb("wwan.0.system_network_status.attached");
    erdb::Rdb rdbSysMode = erdb::Rdb("wwan.0.system_network_status.system_mode");
    erdb::Rdb rdbLastRejCause = erdb::Rdb("wwan.0.system_network_status.last_reject_cause");

    // NR5G connection status
    erdb::Rdb rdbNr5gUp = erdb::Rdb("wwan.0.radio_stack.nr5g.up");

    enum class enumSignalStrength
    {
        POOR      = -120,
        AVERAGE   = -110,
        GOOD      = -100,
        EXCELLENT = -90
    };

    enum class enumDevState: uint8_t
    {
        STATE_BOOTING,
        STATE_SCANNING,
        STATE_REGISTERED,
        STATE_ATTACHED,
        STATE_ERROR
    };

    enumDevState getDevState();
    int8_t getAntennaBars();
    std::string getConnectedRat();
    uint8_t getConnectionStatusCode();
};

DEFINE_PLUGIN(ServicePlugin, "currentConnectionService", "saturn")

// get Device State
//
// Device State enum
// (STATE_BOOTING, STATE_SCANNING, STATE_REGISTERED, STATE_ATTACHED, STATE_ERROR)
ServicePlugin::enumDevState ServicePlugin::getDevState() {
    // 0x00 - NOT_REGISTERED - Not registered; mobile is not currently searching for a new network to provide ser
    // 0x01 - REGISTERED - Registered with a network
    // 0x02 - NOT_REGISTERED_SEARCHING - Not registered, but mobile is currently searching for a new network to p
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

// derive the number of antenna bars, assuming no rdb for the bars.
// TODO : Assuming 4 bars, -1 to 4, using LTE rsrp for calculation
int8_t ServicePlugin::getAntennaBars() {
    enumDevState attached = getDevState();
    erdb::RdbValue rSignalStrength = rdbSignalStrength.get(true);
    int signalStrength = -200;

    if (rSignalStrength.isAvail() && rSignalStrength.isSet()) {
        signalStrength = rSignalStrength.toInt<int>();
    }

    if (attached == enumDevState::STATE_ATTACHED) {

        if (signalStrength < static_cast<int>(enumSignalStrength::POOR)) {
            return 0;
        }
        else if (signalStrength >= static_cast<int>(enumSignalStrength::POOR) && signalStrength < static_cast<int>(enumSignalStrength::AVERAGE)) {
            return 1;
        }
        else if (signalStrength >= static_cast<int>(enumSignalStrength::AVERAGE) && signalStrength < static_cast<int>(enumSignalStrength::GOOD)) {
            return 2;
        }
        else if (signalStrength >= static_cast<int>(enumSignalStrength::GOOD) && signalStrength < static_cast<int>(enumSignalStrength::EXCELLENT)) {
            return 3;
        }
        else { // > enumSignalStrength::EXCELLENT
            return 4;
        }
    }

    return -1; // no signal
}

// get connected Radio Access Technology(RAT)
//
// * rdbSysMode
//      ["no service"|"limited service"|"limited regional service"|"power save"|specific service name]
std::string ServicePlugin::getConnectedRat() {
    std::string sysMode = rdbSysMode.get(true).toStdString();

    if (rdbNr5gUp.get(true).toStdString() == "UP") {
        return "NR5G";
    }

    // toupper
    std::transform(sysMode.begin(), sysMode.end(), sysMode.begin(), ::toupper);
    return sysMode;
}

// get Connection Status Code
//
// return 0 -> Full Service
//        255 -> Unknown
//        others -> GMM Reject codes(Refer to Annex G in 3GPP TS 24.008)
uint8_t ServicePlugin::getConnectionStatusCode() {
    uint8_t ret = 255;
    erdb::RdbValue attState = rdbModemAttState.get(true);
    erdb::RdbValue rejCause = rdbLastRejCause.get(true);

    if (attState.toInt(0) == 1) {
        ret = 0;
    } else if (rejCause.isAvail() && rejCause.isSet()) {
        ret = rejCause.toInt(255);
    }
    return ret;
}

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("currentConnectionService", CURRENT_CONNECTION_UUID)
        .gattCharacteristicBegin("connectionParametersCharacteristic", CONNECTION_PARAMETERS_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
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
                    const char *pDescription = "Connection Parameters";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("connectedRatCharacteristic", CONNECTED_RAT_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getConnectedRat(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getConnectedRat());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbSysMode);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbNr5gUp);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbSysMode);
                RDB_UNSUBSCRIBE(rdbNr5gUp);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Connected RAT";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("connectionStatusCodeCharacteristic", CONNECTION_STATUS_CODE_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getConnectionStatusCode(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getConnectionStatusCode());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbModemAttState);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbLastRejCause);
                ;
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbModemAttState);
                RDB_UNSUBSCRIBE(rdbLastRejCause);
                ;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Connection Status Code";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("antennaBarsCharacteristic", ANTENNA_BARS_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getAntennaBars(), true);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                self.sendChangeNotificationValue(pConnection, PLUGIN(ServicePlugin)->getAntennaBars());
                return true;
            })
            .onStartNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_SUBSCRIBE_FOR_CHANGE(rdbSignalStrength);
                RDB_SUBSCRIBE_FOR_CHANGE(rdbModemAttState);
            })
            .onStopNotify(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                RDB_UNSUBSCRIBE(rdbSignalStrength);
                RDB_UNSUBSCRIBE(rdbModemAttState);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Antenna Bars";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
