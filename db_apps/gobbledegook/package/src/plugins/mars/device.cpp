#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"

#include "uuids.h" // our custom UUIDs
#include "erdb.hpp"
#include "pluginCommon.h"

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);

    ServicePlugin(); // Constructor declaration

  private:
    std::string modelNumber = "N/A";
};

DEFINE_PLUGIN(ServicePlugin, "device", PLUGIN_NAME)

ServicePlugin::ServicePlugin(void) {
    auto vectorProduct = estd::split(erdb::Rdb("system.product").get(true).toStdString(), '_');
    if (vectorProduct.size() == 2) {
        // Model number format: [product name]-[customer name with no eng suffix]-[minor version number]
        // TODO:: The usage of minor version number is decided, yet. So it is set to "01" for now.
        modelNumber = vectorProduct[1] + "-" + estd::replace(vectorProduct[0], "eng", "") + "-" + "01";
    }
}

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
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->modelNumber, true);
            })

        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
