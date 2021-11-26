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

    erdb::Rdb rdbFactoryResetLevel = erdb::Rdb("service.system.factory.level");
    erdb::Rdb rdbFactoryResetTrigger = erdb::Rdb("service.system.factory");

    std::vector<std::string> factoryResetVector = {"factory", "carrier", "installer"};

    bool factoryResetTrigger(const std::string &);
};

DEFINE_PLUGIN(ServicePlugin, "systemService", "saturn")

bool ServicePlugin::factoryResetTrigger(const std::string &setVal) {
    if (std::find(factoryResetVector.begin(), factoryResetVector.end(), setVal) != factoryResetVector.end()) {
        rdbFactoryResetLevel.set(setVal, true);
        rdbFactoryResetTrigger.set("1", true);
        return true;
    }
    return false;
}

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("systemService", SYSTEM_SERVICE_UUID)

        .gattCharacteristicBegin("systemConfigurationResetCharacteristic", SYSTEM_CONFIGURATION_RESET_UUID, {"encrypt-write"})
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                if (PLUGIN(ServicePlugin)->factoryResetTrigger(Utils::stringFromGVariantByteArray(pAyBuffer))) {
                    self.methodReturnVariant(pInvocation, NULL);
                } else {
                    g_dbus_method_invocation_return_error(pInvocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Write Error");
                }
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "System Configuration Reset";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

    .gattServiceEnd();
    return true;
}

}; // namespace ggk
