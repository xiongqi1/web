#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"

#include "uuids.h" // our custom UUIDs

// JSON library from https://github.com/nlohmann/json
#include "json.hpp"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <sys/ioctl.h>

/* Unofficial value, might still change (from: bluez-5.51/tools/hcitool.c)*/
#define LE_LINK     (0x80)
/* The maximum number of connected peer */
#define NUM_CONN    (1)

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);

    bool getConnectedBdAddr(bdaddr_t *conAddr);
    std::string getBleRssi();

  private:
    int hciDevId = -1;
};

DEFINE_PLUGIN(ServicePlugin, "rssiService", "saturn")

// get Bluetooth device address which is connected to GATT server.
//
// conAddr: pointer to store the address.
//
// return: success 0, failure -1
bool ServicePlugin::getConnectedBdAddr(bdaddr_t *conAddr) {
    struct hci_conn_list_req *cl = nullptr;
    struct hci_conn_info *ci;
    int sk = -1;

    if (hciDevId < 0) {
        hciDevId = hci_devid("hci0");
    }

    sk = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (sk < 0 || hciDevId < 0) {
        return -1;
    }

    if ((cl = (hci_conn_list_req *) ::operator new(NUM_CONN * sizeof(*ci) + sizeof(*cl))) == nullptr) {
        goto error;
    }

    cl->dev_id = hciDevId;
    cl->conn_num = NUM_CONN;
    ci = cl->conn_info;

    if (ioctl(sk, HCIGETCONNLIST, (void *) cl)) {
        goto error;
    }

    if (cl->conn_num != 1 || ci->type != LE_LINK){
        goto error;
    }

    bacpy(conAddr, &ci->bdaddr);
    delete cl;
    close(sk);
    return 0;

error:
    free(cl);
    if (sk >= 0) {
        close(sk);
    }
    return -1;

}

// get RSSI of connectd bluetooth device.
//
// return: json formatted BLE peer RSSI. (Ex: {"RSSI":"-85 dBm"} or {"RSSI":""})
std::string ServicePlugin::getBleRssi() {
    bdaddr_t connectedAddr;
    struct hci_conn_info_req *cr = nullptr;
    int dd = -1;
    int8_t rssi;
    nlohmann::json retJson;
    retJson["RSSI"] = "";

    if(getConnectedBdAddr(&connectedAddr) != 0 or hciDevId < 0) {
        goto finish;
    }

    dd = hci_open_dev(hciDevId);
    if (dd < 0) {
        goto finish;
    }

    if ((cr = (hci_conn_info_req *) ::operator new(sizeof(*cr) + sizeof(struct hci_conn_info), std::nothrow)) == nullptr) {
        goto finish;
    }

    bacpy(&cr->bdaddr, &connectedAddr);
    cr->type = LE_LINK;
    if (ioctl(dd, HCIGETCONNINFO, (unsigned long) cr) < 0) {
        goto finish;
    }

    if (hci_read_rssi(dd, htobs(cr->conn_info->handle), &rssi, 1000) < 0) {
        goto finish;
    }

    retJson["RSSI"] = std::to_string(rssi) + " dBm";

finish:
    delete cr;
    if ( dd >= 0) {
        close(dd);
    }
    return retJson.dump();
}

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("rssiService", RSSI_UUID)
        .gattCharacteristicBegin("rssiCharacteristic", BLE_RSSI_UUID, {"encrypt-read"}) // see GattService.h for available permissions
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getBleRssi(), true);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "BLE Signal Strength";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()
    .gattServiceEnd();
    return true;
}

}; // namespace ggk
