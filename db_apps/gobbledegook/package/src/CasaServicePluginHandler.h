#include "CasaServicePlugin.h"
#include <dlfcn.h>
#include <iostream>

namespace CasaPlugin
{

class CasaServicePluginHandler {
  std::shared_ptr<CasaServicePlugin> (*_load)();
  void* handle;
  char* (*_get_ServiceName)();
  char* (*_get_PluginName)();

  std::shared_ptr<CasaServicePlugin> instance = nullptr;
  public:

  CasaServicePluginHandler(std::string name) {
    handle = dlopen(name.c_str(), RTLD_LAZY);
    if (!handle)
        std::cerr << "Error!! dlopen Failed. : " << dlerror() << std::endl;
    _load = (std::shared_ptr<CasaServicePlugin> (*)())dlsym(handle, "load");
    _get_ServiceName = (char* (*)())dlsym(handle, "getServiceName");
    _get_PluginName = (char* (*)())dlsym(handle, "getPluginName");
  }

  std::string get_ServiceName() {
    return std::string(_get_ServiceName());
  }

  std::string get_PluginName() {
    return std::string(_get_PluginName());
  }

  std::shared_ptr<CasaServicePlugin> load() {
    if(!instance)
      instance = _load();
    return instance;
  }

};

}; // namespace CasaPlugin
