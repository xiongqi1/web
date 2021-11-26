#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"

#include "uuids.h" // our custom UUIDs
#include "CasaData.h"

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);
};

DEFINE_PLUGIN(ServicePlugin, "visibleCellService", "test")

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("visibleCellService", VISIBLE_CELL_UUID)
        .gattCharacteristicBegin("visibleCellListCharacteristic", VISIBLE_CELL_LIST_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("visibleCellService/visibleCellListCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("visibleCellService/visibleCellListCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("visibleCellService/visibleCellListCharacteristic", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Visible cell list";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("visibleCellRequestCharacteristic", VISIBLE_CELL_REQUEST_UUID, {"encrypt-read", "notify", "encrypt-write"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("visibleCellService/visibleCellRequestCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("visibleCellService/visibleCellRequestCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("visibleCellService/visibleCellRequestCharacteristic", "");
                self.sendChangeNotificationValue(pConnection, pTextString);
                return true;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Visible cell info request";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("visibleCellInfoCharacteristic", VISIBLE_CELL_INFO_UUID, {"encrypt-read", "notify"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                const char *pTextString = self.getDataPointer<const char *>("visibleCellService/visibleCellInfoCharacteristic", ""); // dataGetter uses 1st parameter as pName
                self.methodReturnValue(pInvocation, pTextString, true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                self.setDataPointer("visibleCellService/visibleCellInfoCharacteristic", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                self.callOnUpdatedValue(pConnection, pUserData);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
            {
                std::string notiCont = getVisibleCellInfoNoti();
                self.sendChangeNotificationValue(pConnection, notiCont.c_str());
                clearVisibleCellInfoNoti();
                return true;
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Visible cell info";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
