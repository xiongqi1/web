#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"

#include "uuids.h" // our custom UUIDs

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);
};

DEFINE_PLUGIN(ServicePlugin, "currentConnectionService", "test")

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("currentConnectionService", CURRENT_CONNECTION_UUID)
        .gattCharacteristicBegin("connectionParametersCharacteristic", CONNECTION_PARAMETERS_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("currentConnectionService/connectionParametersCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("currentConnectionService/connectionParametersCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("currentConnectionService/connectionParametersCharacteristic", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
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
                const char *pTextString = self.getDataPointer<const char *>("currentConnectionService/connectedRatCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("currentConnectionService/connectedRatCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("currentConnectionService/connectedRatCharacteristic", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
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
                const uint8_t uintValue = self.getDataValue<uint8_t >("currentConnectionService/connectionStatusCodeCharacteristic", 0); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, uintValue, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("currentConnectionService/connectionStatusCodeCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const uint8_t uintValue = self.getDataValue<uint8_t >("currentConnectionService/connectionStatusCodeCharacteristic", 0);
                self.sendChangeNotificationValue(pConnection, uintValue);
                return true;
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
                const char *pTextString = self.getDataPointer<const char *>("currentConnectionService/antennaBarsCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("currentConnectionService/antennaBarsCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("currentConnectionService/antennaBarsCharacteristic", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
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
