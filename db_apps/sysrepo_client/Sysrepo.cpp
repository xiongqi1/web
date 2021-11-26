#include "sysrepo_client.h"
#include "Sysrepo.h"
#include <cdcs_fileselector.h>

// The sysrepo library has two threading models - http://www.sysrepo.org/static/doc/html/subscribtions.html
// This define is used to switch between these
#define SINGLE_THREAD_SYSREPO


extern "C" {
#include <sysrepo/values.h>
#include "sysrepo/xpath.h"
}
#include <stdio.h>
#include <string.h>

extern RdbSession rdbSession;

static const char *event_toString(sr_notif_event_t ev)
{
    switch (ev) {
        case SR_EV_VERIFY:
            return "verify";
        case SR_EV_APPLY:
            return "apply";
        case SR_EV_ABORT:
            return "abort";
        default:
            return "invEvent";
    }
}

void print_current_config(Sysrepo &sr, const char *module_name)
{
    std::string select_xpath = "/";
    select_xpath += module_name;
    select_xpath += ":*//*";

    sr_val_t *values = NULL;
    size_t count = 0;
    int rc = sr_get_items(sr.getSession(), select_xpath.c_str(), &values, &count);
    if (SR_ERR_OK != rc) {
        DBG(LOG_ERR, "Error by sr_get_items: %s", sr_strerror(rc));
        return;
    }
    for (size_t i = 0; i < count; i++) {
        sr_print_val(&values[i]);
    }
    sr_free_values(values, count);
}

/**
 * @brief (Debug only function) Print changes for each node.
 */
void print_change(sr_change_oper_t op, sr_val_t *old_val, sr_val_t *new_val)
{
    switch(op) {
        case SR_OP_CREATED:
            if (NULL != new_val) {
                printf("CREATED: ");
                sr_print_val(new_val);
            }
            break;
        case SR_OP_DELETED:
            if (NULL != old_val) {
                printf("DELETED: ");
                sr_print_val(old_val);
            }
            break;
        case SR_OP_MODIFIED:
            if (NULL != old_val && NULL != new_val) {
                printf("MODIFIED: ");
                printf("old value ");
                sr_print_val(old_val);
                printf("new value ");
                sr_print_val(new_val);
            }
            break;
        case SR_OP_MOVED:
            if (NULL != new_val) {
                printf("MOVED: %s after %s", new_val->xpath, NULL != old_val ? old_val->xpath : NULL);
            }
            break;
        default:
            break;
    }
}

SrRdb::SrRdb(const char *rdbName)
{
    pRdbvar = new RdbVar(rdbSession, rdbName);
    rdbSession.addSubscriber(*pRdbvar, onRdbWriteS, this);
}

SrRdb::~SrRdb()
{
    delete pRdbvar;
}

void SrRdb::onRdbWrite(void *parm, const char *val) {};

// This is called when the RDB is changed
void SrRdb::onRdbWriteS(void *parm, const char *val)
{
//  DBG(LOG_DEBUG,"change  RDB to %s",val);
    SrRdb * _this = (SrRdb *)parm;
    _this->onRdbWrite(parm, val);
}

void SrRdb::getSrVal(const std::string &xpath, sr_val_t *v)
{
    sr_val_set_xpath(v, xpath.c_str());
    DBG(LOG_DEBUG, "Get item %s Read RDB %s -> %s", xpath.c_str(), pRdbvar->getName(), pRdbvar->getVal().c_str());
    sr_val_set_str_data(v, SR_STRING_T, pRdbvar->getVal().c_str());
}

const std::string SrRdb::getRdbVal()
{
    return pRdbvar->getVal();
}

void SrRdb::setVal(char *str)
{
    pRdbvar->setVal(str);
}

DataProvider::DataProvider(const char *_name, const char *rdbName) : SrRdb(rdbName)
{
    name = _name;
}

void DataProvider::getVal(const std::string &container, sr_val_t *v)
{
    std::string xpath = container + "/" + name;
    getSrVal(xpath, v);
}

SysrepoDataContainer::SysrepoDataContainer(const char *_xpath)
{
    xpath = _xpath;
}

SysrepoDataContainer::~SysrepoDataContainer()
{
    int cnt = data.size();
    DBG(LOG_DEBUG, "Delete %d datacontainers", cnt);
    for (int i = 0; i < cnt; i++) {
        delete data[i];
    }
}

void SysrepoDataContainer::addDataProvider(DataProvider *pDp)
{
    data.push_back(pDp);
}

ConfigItem::ConfigItem(const char *_name, const char *rdbName) : SrRdb(rdbName)
{
    name = _name;
    sysRepo = 0;
}

void ConfigItem::getVal(std::string &container, sr_val_t *v)
{
    std::string xpath = container + "/" + name;
    getSrVal(xpath, v);
}

void ConfigItem::onRdbWrite(void *parm, const char *val) {
        DBG(LOG_DEBUG,"change  RDB %s to %s", name.c_str(), val);
        pushToSysRepo(val);
};

bool ConfigItem::match(const char * xpath)
{
//        DBG(LOG_DEBUG, "Match xpath %s for event %s", xpath, name.c_str());
    return 0 == strcmp(name.c_str(), xpath);
}

bool ConfigItem::verify(sr_val_t *new_value)
{
    DBG(LOG_DEBUG, "verify ");
    return true;
}

bool ConfigItem::apply(sr_val_t *new_value)
{
    char *str = sr_val_to_str(new_value);
    DBG(LOG_DEBUG, "apply %s", str);
    setVal(str);
    free(str);
    return true;
}

void ConfigItem::setSession(Sysrepo * srepo) { sysRepo = srepo;}

void ConfigItem::pushToSysRepo(const char * value) {
    if (sysRepo) {
        bool doCommit = true;
        if (!value) {
            value = getRdbVal().c_str();
            doCommit = false;
        }
//      DBG(LOG_DEBUG, " %s to %s", name.c_str(), value);
        sysRepo->set(name.c_str(), value);
        if (doCommit)
            sysRepo->commit();
    }
}

SysrepoModule::SysrepoModule(const char * moduleName)
{
    name = moduleName;
    xpathPrefix = "/" + name + ':';
    isInitialised = false;
}

SysrepoModule::~SysrepoModule()
{
    int cnt = items.size();
    DBG(LOG_DEBUG, "Delete %d SysrepoModule", cnt);
    for (int i = 0; i < cnt; i++) {
        delete items[i];
    }
}

void SysrepoModule::addConfigItem(ConfigItem *pCfgItem)
{
    pCfgItem->name.insert(0,xpathPrefix);
    items.push_back(pCfgItem);
}

#ifdef SINGLE_THREAD_SYSREPO
// The sysrepo has a pretty ugly way of handling signle threaded. Fortunately
// this class hides all of that - reference is their example - application_fd_watcher_example.c

class SysrepoThreader {
    FileSelector *fileSelector;
public:
    SysrepoThreader(): fileSelector(0) {}

    bool init(FileSelector &_fileSelector)
    {
        int fd;
        fileSelector = &_fileSelector;
        int rc = sr_fd_watcher_init(&fd, &flush_all_events);
        if (SR_ERR_OK != rc) {
            DBG(LOG_ERR, "Error by sr_fd_watcher_init: %s", sr_strerror(rc));
            return false;
        }
        DBG(LOG_DEBUG, "file handle %d", fd);
	fileSelector->addMonitor(fd, onDataAvailable, this, true);
        return true;
    }

    ~SysrepoThreader()
    {
        DBG(LOG_DEBUG, "");
        sr_fd_watcher_cleanup();
    }

    static void onDataAvailable(void *param, void * pFm)
    {
        SysrepoThreader * _this = (SysrepoThreader*)param;
        FileMonitor *pFileMon = (FileMonitor *)pFm;
        DBG(LOG_DEBUG, "FD %d", pFileMon->fd);

        // They overload their callback. It also returns an array of file handles that get added or removed from the select/poll
        sr_fd_change_t *fd_change_set = NULL;
        size_t fd_change_set_cnt = 0;
        int rc = sr_fd_event_process(pFileMon->fd, pFileMon->forRead ? SR_FD_INPUT_READY: SR_FD_OUTPUT_READY, &fd_change_set, &fd_change_set_cnt);
        if (SR_ERR_OK != rc) {
             DBG(LOG_ERR, "Error by processing events on fd: %s\n", sr_strerror(rc));
             return;
        }

        DBG(LOG_DEBUG, "Post process %p %d", fd_change_set, fd_change_set_cnt);
        for (size_t i = 0; i < fd_change_set_cnt; i++) {
            int fd = fd_change_set[i].fd;
            int events = fd_change_set[i].events;
            sr_fd_action_t action = fd_change_set[i].action;
            DBG(LOG_DEBUG, "Post process %d FD = %d start? = %d read = %d", i, fd, SR_FD_START_WATCHING == action, SR_FD_INPUT_READY == events);
            if (SR_FD_START_WATCHING == action) {
                /* start monitoring the FD for specified event */
                _this->fileSelector->addMonitor(fd, onDataAvailable, _this, (SR_FD_INPUT_READY == events));
            }
            if (SR_FD_STOP_WATCHING == action) {
                /* stop monitoring the FD for specified event */
                _this->fileSelector->removeMonitor(fd, (SR_FD_INPUT_READY == events));
            }
        }

        free(fd_change_set);
    }

    static void flush_all_events()
    {
        DBG(LOG_DEBUG, "We are single-threaded, no reason to wait for all pending events to be processed");
    }
};

SysrepoThreader sysrepoThreader;

#endif //SINGLE_THREAD_SYSREPO

Sysrepo::Sysrepo() {
    sr_conn = NULL;
    sr_sess = NULL;
}
/*
Sysrepo::Sysrepo(sr_session_ctx_t *sess)
{
    sr_conn = NULL;
    sr_sess = sess;
}

Sysrepo::Sysrepo(const Sysrepo& orig) {
}
*/
Sysrepo::~Sysrepo() {
    if (NULL != sr_conn) {
        if (NULL != sr_sess) {
            int cnt = subscriptions.size();
            for (int i = 0; i < cnt; i++) {
                sr_unsubscribe(sr_sess, subscriptions[i]);
            }
            cnt = dataContainers.size();
            for (int i = 0; i < cnt; i++) {
                delete dataContainers[i];
            }
            cnt = modules.size();
            for (int i = 0; i < cnt; i++) {
                delete modules[i];
            }
            sr_session_stop(sr_sess);
        }
        sr_disconnect(sr_conn);
    }
}

bool Sysrepo::init(FileSelector &_fileSelector) {

#ifdef SINGLE_THREAD_SYSREPO
    // This must be called first
    if(!sysrepoThreader.init(_fileSelector)) {
        DBG(LOG_ERR,"failed to init sysrepo threader");
        return false;
    }
#endif
    sr_log_stderr(SR_LL_DBG);

    /* connect to sysrepo */
    int rc = sr_connect("ntc_client", SR_CONN_DEFAULT, &sr_conn);
    if (SR_ERR_OK != rc) {
        return false;
    }

    /* start session */
    rc = sr_session_start(sr_conn, SR_DS_RUNNING, SR_SESS_DEFAULT, &sr_sess);
    if (SR_ERR_OK != rc) {
        return false;
    }
    return true;
}

bool Sysrepo::commit()
{
    int rc = sr_commit(sr_sess);
    if (SR_ERR_OK != rc) {
        DBG(LOG_ERR, "Error by sr_commit: %s", sr_strerror(rc));
        return false;
    }
    return true;
}

bool Sysrepo::set(const char *xpath, const char *str)
{
    // TODO - this needs to be sorted, the next 3 lines related to buf are needed because
    // sysrepo seems to end up with garbage without this seemingly redundant step
    DBG(LOG_ERR, "Sysrepo::set a workaround is in effect");
    char buf[256];
    snprintf(buf,sizeof(buf), "%s", str);
    str = buf;

//    sr_val_t value = { 0 };
//    value.type = SR_STRING_T;
//    value.data.string_val = (char *)str;
    DBG(LOG_DEBUG, "Sysrepo::set(%s): <%s>", xpath, str);
//    return set(xpath, &value);
    int rc = sr_set_item_str(sr_sess, xpath, str, SR_EDIT_DEFAULT);
    if (SR_ERR_OK != rc) {
        DBG(LOG_ERR, "Error by sr_set_item_str(%s): %s", xpath, sr_strerror(rc));
        return false;
    }
    return true;
}

/*
bool Sysrepo::set(const char *xpath, sr_val_t *value)
{
    int rc = sr_set_item(sr_sess, xpath, value, SR_EDIT_DEFAULT);
    if (SR_ERR_OK != rc) {
        DBG(LOG_ERR, "Error by sr_set_item(%s): %s", xpath, sr_strerror(rc));
        return false;
    }
    return true;
}
*/

int data_provider_cb(const char *xpath, sr_val_t **values, size_t *values_cnt, void *private_ctx)
{
    DBG(LOG_DEBUG, "Data for '%s' requested.", xpath );

    SysrepoDataContainer *pCntnr = (SysrepoDataContainer*)private_ctx;

    int numValues = pCntnr->data.size();
    if (numValues > 0) {

        sr_val_t *v = NULL;
        int rc = sr_new_values(numValues, &v); /* allocate space for data to return */
        if (SR_ERR_OK != rc) {
            return rc;
        }
        for (int i = 0; i < numValues; i++) {
            DataProvider * dp = pCntnr->data[i];
            dp->getVal(pCntnr->xpath, &v[i]);
        }
        *values = v;
    }
    *values_cnt = numValues;
    return SR_ERR_OK;
}

bool Sysrepo::subscribeData(const char * xpath, SysrepoDataContainer *pCntnr)
{
    DBG(LOG_DEBUG, "Subscribe to %s", xpath);

    dataContainers.push_back(pCntnr);

    sr_subscription_ctx_t *subscription_ctx;
    int rc = sr_dp_get_items_subscribe(sr_sess, xpath, data_provider_cb, pCntnr, SR_SUBSCR_DEFAULT, &subscription_ctx);
    if (SR_ERR_OK != rc) {
        DBG(LOG_ERR, "Error failed to subscribe %s: %s", xpath, sr_strerror(rc));
        return false;
    }
    subscriptions.push_back(subscription_ctx);
    return true;
}

/**
 * @brief ietf-interfaces module change subscriber callback for sysrepo.
 * @param session Pointer to sysrepo session context
 * @param module_name refers to ietf-interfaces module (1-4)
 * @param event SR_EV_VERIFY or SR_EV_APPLY event
 * @param [out] private_ctx Application specific context - not used
 * @return Error code SR_ERR_OK on success, failure SR_ERR_[x]
 * (see sysrepo.h for the list of sysrepo specific error codes).
 */
static int module_change_cb(sr_session_ctx_t *session, const char *module_name, sr_notif_event_t event, void *private_ctx)
{
    DBG(LOG_DEBUG, "Got event: module_name:%s, event:%s", module_name, event_toString(event));

    SysrepoModule *pModule = (SysrepoModule *)private_ctx;
    sr_change_iter_t *it = NULL;
    int rc = sr_get_changes_iter(session, (pModule->xpathPrefix + '*').c_str(), &it);
    if (SR_ERR_OK != rc) {
        DBG(LOG_ERR, "Get changes iter failed for xpath %s for event %s", pModule->xpathPrefix.c_str(), event_toString(event));
        return SR_ERR_INTERNAL;
    }

    sr_change_oper_t oper;
    sr_val_t *old_value;
    sr_val_t *new_value;
    while (SR_ERR_OK == sr_get_change_next(session, it, &oper, &old_value, &new_value)) {

        print_change(oper, old_value, new_value);
        switch(oper) {
            case SR_OP_CREATED:
            case SR_OP_MODIFIED:
            case SR_OP_DELETED:
            {
                const char *xpath = NULL != new_value ? new_value->xpath : old_value->xpath;
                int cnt = pModule->items.size();
                DBG(LOG_DEBUG, "Match xpath %s for event %s in %d items", xpath, event_toString(event), cnt);
                for (int i = 0; i < cnt; i++) {
                    ConfigItem * pItem = pModule->items[i];
                    if ( pItem->match(xpath)) {
                        switch(event){
                            case SR_EV_VERIFY:
                                if (!pItem->verify(new_value)) {
                                    rc = SR_ERR_VALIDATION_FAILED;
                                }
                                break;
                            case SR_EV_APPLY:
                                if (!pItem->apply(new_value)) {
                                    rc = SR_ERR_OPERATION_FAILED;
                                }
                                break;
                            default:
                                DBG(LOG_ERR, "Unsupported event - %d", event);
                                rc = SR_ERR_UNSUPPORTED;
                                break;
                        }
                    }
                }
                break;
            }
            default:
                DBG(LOG_ERR, "Unsupported oper - %d", oper);
                rc = SR_ERR_UNSUPPORTED;
                break;
        }
        sr_free_val(old_value);
        sr_free_val(new_value);
        if (rc != SR_ERR_OK) {
            break;
        }
    }
    sr_free_change_iter(it);
    DBG(LOG_DEBUG, "return %d %d", rc, SR_ERR_OK);
    return rc;
}

bool Sysrepo::subscribeModule(SysrepoModule *pModule)
{
    modules.push_back(pModule);

    const char * xpath = pModule->name.c_str();
    DBG(LOG_DEBUG, "Subscribe to %s", xpath);
    sr_subscription_ctx_t *subscription_ctx;
    int rc = sr_module_change_subscribe(sr_sess, xpath, module_change_cb,
            pModule, 0, SR_SUBSCR_DEFAULT, &subscription_ctx);
    if (SR_ERR_OK != rc) {
        DBG(LOG_ERR, "Error failed to subscribe %s: %s", xpath, sr_strerror(rc));
        return false;
    }
    subscriptions.push_back(subscription_ctx);
    DBG(LOG_DEBUG, "Subscribed for changes:%s - OK", xpath);

    int cnt = pModule->items.size();
    for (int i = 0; i < cnt; i++) {
        ConfigItem * pItem = pModule->items[i];
        pItem->setSession(this);
        pItem->pushToSysRepo(0);
    }
    pModule->isInitialised = true;
    if (!commit()) {
        return false;
    }

    return true;
}
