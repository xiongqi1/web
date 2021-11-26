#include "CasaServicePlugin.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "DBusObject.h"
#include "GattService.h"
#include "Gobbledegook.h"

#include "uuids.h" // our custom UUIDs
#include "erdb.hpp"
#include "elogger.hpp"
#include "pluginCommon.h"

extern int ping(std::string target);

namespace ggk {

class ServicePlugin : public CasaPlugin::CasaServicePlugin {
  public:
    virtual bool append_service(ggk::DBusObject &root);

    ServicePlugin(); // Constructor declaration
    ~ServicePlugin(); // Destructor: declaration

    std::thread pingThread; // Thread object for ping loop.
    std::string pingServer; // Ping target server
    uint8_t pingInterval = 0; // Ping interval

    void pingLoop(std::string, uint8_t); // Thread function to ping with interval.

    std::string getPingServer();
    void setPingServer(std::string);
    uint8_t getPingInterval();
    void setPingInterval(uint8_t);

  private:
    bool exitPingThread = false;
};

DEFINE_PLUGIN(ServicePlugin, "customDesignedService", PLUGIN_NAME)

ServicePlugin::ServicePlugin() {
    pingInterval = 0;
}

ServicePlugin::~ServicePlugin() {
    // Wait until ping thread terminated, if necessary.
    exitPingThread = true;
    if (pingThread.joinable()) {
        pingThread.join();
    }
}

// Thread function to ping with interval.
void ServicePlugin::pingLoop(std::string target, uint8_t interval) {
    uint8_t l_interval = interval, l_cnt = interval;

    int disconn_timeout_secs = 60;
    int disconn_count = 0;

    if (l_interval == 0) {
        return;
    }

    while ((ggkGetServerRunState() < EStopping) && pingInterval > 0 && !exitPingThread) {
        if (l_cnt++ >= l_interval) {
            ping(target); // Don't care ping result, this is just to bring up 5G.
            l_cnt = 1;
        }

        if(ggkGetActiveConnections() >= 1) {
            disconn_count=0;
        } else {
            ++disconn_count;
        }

        if (disconn_count >= disconn_timeout_secs) {
            log(LOG_ERR, "Timeout. exit pingThread");
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::string ServicePlugin::getPingServer() {
    return pingServer;
}

void ServicePlugin::setPingServer(std::string setVal) {
    pingServer = setVal;
}

uint8_t ServicePlugin::getPingInterval() {
    return pingInterval;
}

void ServicePlugin::setPingInterval(uint8_t setVal) {
    pingInterval = setVal;
    exitPingThread = true;
    if (pingThread.joinable()) {
        pingThread.join();
    }
    if (pingInterval > 0 && !pingServer.empty()) {
        exitPingThread = false;
        pingThread = std::thread(&ServicePlugin::pingLoop, this, pingServer, pingInterval);
    }
}

bool ServicePlugin::append_service(ggk::DBusObject &root) {
    root.gattServiceBegin("customDesignedService", CUSTOM_DESIGNED_PING_UUID)

        .gattCharacteristicBegin("pingServerCharacteristic", PING_SERVER_UUID, {"encrypt-read", "encrypt-write"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getPingServer(), true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                PLUGIN(ServicePlugin)->setPingServer(Utils::stringFromGVariantByteArray(pAyBuffer));
                self.methodReturnVariant(pInvocation, NULL);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Ping Server";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

        .gattCharacteristicBegin("pingIntervalCharacteristic", PING_INTERVAL_UUID, {"encrypt-read", "encrypt-write"})
            .onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                self.methodReturnValue(pInvocation, PLUGIN(ServicePlugin)->getPingInterval(), true);
            })
            .onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
            {
                GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
                uint8_t setVal = *reinterpret_cast<const uint8_t *>(Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
                PLUGIN(ServicePlugin)->setPingInterval(setVal);
                self.methodReturnVariant(pInvocation, NULL);
            })
            .gattDescriptorBegin("description", "2901", {"encrypt-read"})
                .onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
                {
                    const char *pDescription = "Ping Interval";
                    self.methodReturnValue(pInvocation, pDescription, true);
                })
            .gattDescriptorEnd()
        .gattCharacteristicEnd()

    .gattServiceEnd();
    return true;
}

}; // namespace ggk
