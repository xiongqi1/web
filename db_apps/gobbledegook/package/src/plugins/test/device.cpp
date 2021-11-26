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

DEFINE_PLUGIN(ServicePlugin, "device", "test")

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("device", "180A")

        // Characteristic: Manufacturer Name String (0x2A29)
        //
        // See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.manufacturer_name_string.xml
        .gattCharacteristicBegin("mfgr_name", "2A29", {"encrypt-read"})

            // Standard characteristic "ReadValue" method call
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, "Casa Systems", true);
            })

        .gattCharacteristicEnd()

        // Characteristic: Model Number String (0x2A24)
        //
        // See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.model_number_string.xml
        .gattCharacteristicBegin("model_num", "2A24", {"encrypt-read"})

            // Standard characteristic "ReadValue" method call
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("device/model_num", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("device/model_num", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("device/model_num", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })

        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
