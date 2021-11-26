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

DEFINE_PLUGIN(ServicePlugin, "speedTestService", "test")

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("speedTestService", SPEEDTEST_UUID)
        .gattCharacteristicBegin("speedTestRequestCharacteristic", SPEEDTEST_REQUEST_UUID, {"encrypt-read", "notify", "encrypt-write"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                const uint8_t uintValue = self.getDataValue<uint8_t >("speedTestService/speedTestRequestCharacteristic", 0); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, uintValue, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("speedTestService/speedTestRequestCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const uint8_t uintValue = self.getDataValue<uint8_t >("speedTestService/speedTestRequestCharacteristic", 0);
                self.sendChangeNotificationValue(pConnection, uintValue);
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
                const char *pTextString = self.getDataPointer<const char *>("speedTestService/speedTestResultCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("speedTestService/speedTestResultCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("speedTestService/speedTestResultCharacteristic", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
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
