#ifndef SYSREPO_H
#define SYSREPO_H

extern "C" {
#include <sysrepo.h>
}
#include <cdcs_rdb.h>

#include <vector>
#include <string>

class SrRdb {
public:
    SrRdb(const char *rdbName);
    virtual ~SrRdb();

    virtual void onRdbWrite(void *parm, const char *val);
    // This is called when the RDB is changed
    static void onRdbWriteS(void *parm, const char *val);

    void getSrVal(const std::string &xpath, sr_val_t *v);

    const std::string getRdbVal();

    void setVal(char *str);

private:
    RdbVar *pRdbvar;
};

// A Dataprovider associates an xpath variable name to and RDB variable
class DataProvider: public SrRdb {
public:
    DataProvider(const char *_name, const char *rdbName);

    void getVal(const std::string &container, sr_val_t *v);

private:
    std::string name;
};

class SysrepoDataContainer {
public:
    std::vector<DataProvider *> data;
    std::string xpath;
    SysrepoDataContainer(const char *_xpath);
    virtual ~SysrepoDataContainer();
    void addDataProvider(DataProvider *pDp);
private:
};

class Sysrepo;
class ConfigItem: public SrRdb {
    Sysrepo * sysRepo;
public:
    std::string name;

    ConfigItem(const char *_name, const char *rdbName);
    void getVal(std::string &container, sr_val_t *v);

    virtual void onRdbWrite(void *parm, const char *val);

    bool match(const char * xpath);

    bool verify(sr_val_t *new_value);

    bool apply(sr_val_t *new_value);

    void setSession(Sysrepo * srepo);

    void pushToSysRepo(const char * value);
};

class SysrepoModule {
public:
    std::string name;
    std::string xpathPrefix;
    std::vector<ConfigItem *> items;
    bool isInitialised;

    SysrepoModule(const char * moduleName);

    virtual ~SysrepoModule();

    void addConfigItem(ConfigItem *pCfgItem);

private:
};

class FileSelector;

// This class encapsulates all the extern sysrepo "C" interface
class Sysrepo {
public:
    Sysrepo();
//    Sysrepo(sr_session_ctx_t *sr_sess);
//    Sysrepo(const Sysrepo& orig);
    ~Sysrepo();

    bool init(FileSelector &fileSelector);
    bool commit();

    bool set(const char *xpath, const char *str);
//    bool set(const char *xpath, sr_val_t *value);

    // This function is generated at build time by an script from the data
    // definitions. Corresponding Yang files are also produced by the script
    bool provideData();

    // adds a data provider to the list
    bool subscribeData(const char *xpath, SysrepoDataContainer *pCntnr);

    // adds a configuration module to the list
    bool subscribeModule(SysrepoModule *pModule);

    sr_session_ctx_t * getSession() {return sr_sess;}

private:
    sr_conn_ctx_t *sr_conn;
    sr_session_ctx_t *sr_sess;
    std::vector<SysrepoModule *> modules;
    std::vector<SysrepoDataContainer *> dataContainers;
    std::vector<sr_subscription_ctx_t *> subscriptions;
};

#endif /* SYSREPO_H */

