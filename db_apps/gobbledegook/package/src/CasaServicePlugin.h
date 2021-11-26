#ifndef CASA_SERVICE_PLUGIN
#define CASA_SERVICE_PLUGIN
#include "DBusObject.h"
#include <string>
#include <memory>
namespace CasaPlugin
{

class CasaServicePlugin {
  public:
    CasaServicePlugin() {};
    virtual ~CasaServicePlugin() {};

    virtual bool append_service(ggk::DBusObject &root) {
        std::ignore = root;
        return false;
    }
};

// Macro to register rdb subscription on onStartNotify method of GattCharacteristic class.
#define RDB_SUBSCRIBE_FOR_CHANGE(rdbObj) \
  if (!PLUGIN(ServicePlugin)->rdbObj.is_subscribed()) { \
      PLUGIN(ServicePlugin)->rdbObj.subscribeForChange([&self](erdb::Rdb &rdb) { \
              ggkNofifyUpdatedCharacteristic(self.getPath().c_str()); \
              }, false); \
  }

// Macro to deregister rdb subscription on onStopNotify method of GattCharacteristic class.
#define RDB_UNSUBSCRIBE(rdbObj) \
  if (PLUGIN(ServicePlugin)->rdbObj.is_subscribed()) { \
      PLUGIN(ServicePlugin)->rdbObj.unsubscribe(); \
  }

#define DEFINE_PLUGIN(classType, serviceName, pluginName)       \
  class classType;                                              \
  static std::shared_ptr<CasaPlugin::CasaServicePlugin> service_plugin; \
  extern "C" {                                                  \
    std::shared_ptr<CasaPlugin::CasaServicePlugin> load()       \
    {                                                           \
      service_plugin = std::shared_ptr<CasaPlugin::CasaServicePlugin>(new classType()); \
      return service_plugin;                                    \
    }                                                           \
                                                                \
    const char* getServiceName()                                \
    {                                                           \
      return serviceName;                                       \
    }                                                           \
                                                                \
    const char* getPluginName()                                 \
    {                                                           \
      return pluginName;                                        \
    }                                                           \
  }

}; // namespace CasaPlugin

#define PLUGIN(classType)  \
    (dynamic_cast<classType*> (service_plugin.get()))

#endif /* ifndef CASA_SERVICE_PLUGIN */
