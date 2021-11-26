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

DEFINE_PLUGIN(ServicePlugin, "ledService", "saturn")

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("ledService", LED_UUID)
        .gattCharacteristicBegin("ledColourCharacteristic", LED_COLOUR_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
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
                    const char *pDescription = "LED Colour";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("ledFlashCharacteristic", LED_FLASH_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
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
                    const char *pDescription = "LED Flashing state";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
