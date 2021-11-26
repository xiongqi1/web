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

DEFINE_PLUGIN(ServicePlugin, "ledService", "test")

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("ledService", LED_UUID)
        .gattCharacteristicBegin("ledColourCharacteristic", LED_COLOUR_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("ledService/ledColourCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("ledService/ledColourCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("ledService/ledColourCharacteristic", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "LED Colour";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("ledFlashCharacteristic", LED_FLASH_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("ledService/ledFlashCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("ledService/ledFlashCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("ledService/ledFlashCharacteristic", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "LED Flash";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
