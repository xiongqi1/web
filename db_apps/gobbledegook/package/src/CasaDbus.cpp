
#include <iostream>
#include <sstream>
#include <thread>
#include <string.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "Gobbledegook.h"

#include "CasaDbus.h"
#include "Housekeeping.h"

namespace CasaDbus
{

enum DBusException
{
    DBUS_NO_EXCEPTION,
    DBUS_EXCEPTION
};

bool niceMode=false;

// Maximum number of retries to process execptions.
int maxRetries = 3;

// Most of these functions stolen and adapted from bluez/client (bluetoothctl) or https://leonardoce.wordpress.com/2015/03/13/dbus-tutorial-part-2/

void handle_errors(DBusError *error, const char* context)
{
    
    if (dbus_error_is_set(error))
    {
        fprintf(stderr, "%s: %s\n", context, error->message);
        dbus_error_free(error); // if set, the error needs to be freed.
        throw DBUS_EXCEPTION;
    }
}

static double extract_boolean_from_variant(DBusMessage *reply, DBusError *error) {
    DBusMessageIter iter;
    DBusMessageIter sub;
    bool result;
     
    dbus_message_iter_init(reply, &iter);
 
    if (DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(&iter)) {
        dbus_set_error_const(error, "reply_should_be_variant", "This message hasn't a variant response type");
        return 0;
    }
 
    dbus_message_iter_recurse(&iter, &sub);
 
    if (DBUS_TYPE_BOOLEAN != dbus_message_iter_get_arg_type(&sub)) {
        dbus_set_error_const(error, "variant_should_be_boolean", "This variant reply message must have boolean content");
        return 0;
    }
 
    dbus_message_iter_get_basic(&sub, &result);
    return result;
}

static DBusMessage *create_property_get_message(const char *bus_name, const char *path, const char *iface, const char *propname) {
    DBusMessage *queryMessage = NULL;
 
    queryMessage = dbus_message_new_method_call(bus_name, path, 
                            "org.freedesktop.DBus.Properties",
                            "Get");
    dbus_message_append_args(queryMessage,
                 DBUS_TYPE_STRING, &iface,
                 DBUS_TYPE_STRING, &propname,
                 DBUS_TYPE_INVALID);
 
    return queryMessage;
}

static bool get_boolean_property(DBusConnection *connection, const char *bus_name, const char *path, const char *iface, const char *propname, DBusError *error) {
    DBusError myError;
    bool result = 0;
    DBusMessage *queryMessage = NULL;
    DBusMessage *replyMessage = NULL;
 
    dbus_error_init(&myError);
     
    queryMessage = create_property_get_message(bus_name, path, iface, propname);
    replyMessage = dbus_connection_send_with_reply_and_block(connection,
                          queryMessage,
                          1000,
                          &myError);
    dbus_message_unref(queryMessage);
    if (dbus_error_is_set(&myError)) {
        dbus_move_error(&myError, error);
        return 0;
    }
 
    result = extract_boolean_from_variant(replyMessage, &myError);
    if (dbus_error_is_set(&myError)) {
        dbus_move_error(&myError, error);
        return 0;
    }
 
    dbus_message_unref(replyMessage);
     
    return result;
}

const char* dbus_introspect(const char* dbus_dest, const char* dbus_path)
{
    DBusConnection *connection = NULL;
    DBusError error;

    int cntRetries = 0;

retry:
    cntRetries++;
    try
    {
        dbus_error_init(&error);
        connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
        handle_errors(&error, "dbus_introspect dbus_bus_get");
        
        DBusMessage *msgQuery = NULL;
        DBusMessage *msgReply = NULL;
        
        msgQuery = dbus_message_new_method_call(dbus_dest, dbus_path, "org.freedesktop.DBus.Introspectable", "Introspect");
        
        msgReply = dbus_connection_send_with_reply_and_block(connection, msgQuery, 1000, &error);
        handle_errors(&error, "dbus_introspect reply-block");
        dbus_message_unref(msgQuery);
        
        const char* introspectData = NULL;
        dbus_message_get_args(msgReply, &error, DBUS_TYPE_STRING, &introspectData, DBUS_TYPE_INVALID);
        
        dbus_message_unref(msgReply);
        
        dbus_connection_unref(connection);
        return introspectData;
    }
    catch (DBusException ex)
    {
        LogWarn("Caught DBus Error - sleep and retry");
        dbus_connection_unref(connection);
        // to escape retry loop
        if (ggkGetServerRunState() >= EStopping || cntRetries > maxRetries) {
            return NULL;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        goto retry;
    }
}

static bool check_device_for_pairing(const char* device, const char* interface, bool depairCurrentDevice)
{
    char message[120];
    DBusConnection *connection = NULL;
    DBusError error;

    char path[80];
    bool connected;
    bool paired;

    int cntRetries = 0;

retry:
    cntRetries++;
    try
    {
        dbus_error_init(&error);
        connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

        sprintf(path, "/org/bluez/%s/%s", interface, device);

        connected = get_boolean_property(connection, "org.bluez",
                         path,
                         "org.bluez.Device1",
                         "Connected",
                         &error);
        handle_errors(&error, "cdfp connected");

        paired = get_boolean_property(connection, "org.bluez",
                         path,
                         "org.bluez.Device1",
                         "Paired",
                         &error);
        handle_errors(&error, "cdfp paired");

        dbus_connection_unref(connection);

        sprintf(message, "Device %s: Paired = %s Connected = %s", device, paired?"true":"false", connected?"true":"false");
        LogDebug(message);

        if (connected && depairCurrentDevice)
        {
            sprintf(message, "De-pairing device %s on %s", device, interface);
            LogWarn(message);
            deletePairingRecord(device, interface);
        }

        if (connected && !paired)
        {
            LogWarn("** Device not paired!\n");
            return true;
        }
        return false;
    }
    catch (DBusException ex)
    {
        LogWarn("Caught DBus Error - sleep and retry");
        dbus_connection_unref(connection);
        // to escape retry loop
        if (ggkGetServerRunState() >= EStopping || cntRetries > maxRetries) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        goto retry;
    }
}

static bool scan_devices_for_pairing(xmlNode * a_node, const char* interface, bool depairCurrentDevice)
{
    xmlNode *cur_node = NULL;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            xmlChar *uri;
            uri = xmlGetProp(cur_node, (const xmlChar*)"name");
            if (uri) 
            {
                if (!strncmp((const char*)uri, "dev_", 4))
                {
                    {
                        char dev[40];
                        strcpy(dev, (const char*)uri);
                        
                        if (check_device_for_pairing(dev, interface, depairCurrentDevice)) return true;
                    }
                }
            }
            xmlFree(uri);
        }
        if (scan_devices_for_pairing(cur_node->children, interface, depairCurrentDevice)) return true;  // return true if child check was true
    }
    return false;   // scanned all node parts and it was OK
}

static bool find_hci_interface(xmlNode * a_node, char *dst)
{
    xmlNode *cur_node = NULL;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            xmlChar *uri;
            uri = xmlGetProp(cur_node, (const xmlChar*)"name");
            if (uri) 
            {
                if (!strncmp((const char*)uri, "hci", 3))
                {
                    strcpy(dst, (const char*)uri);
                    xmlFree(uri);
                    xmlUnlinkNode(cur_node);
                    xmlFreeNode(cur_node);
                    return true;
                }
            }
            xmlFree(uri);
        }
        if (find_hci_interface(cur_node->children, dst)) return true;
    }
    xmlUnlinkNode(cur_node);
    xmlFreeNode(cur_node);
    return false;
}

static bool findFirstInterface(xmlDocPtr doc, char *dst)
{
    xmlNode *root_element = NULL;
    
    root_element = xmlDocGetRootElement(doc);
    bool result = find_hci_interface(root_element, dst);
    xmlUnlinkNode(root_element);
    xmlFreeNode(root_element);
    return result;
}

bool scanDeviceList(xmlDocPtr doc, const char* interface, bool depairCurrentDevice)
{
    xmlNode *root_element = NULL;
    
    root_element = xmlDocGetRootElement(doc);
    bool result = scan_devices_for_pairing(root_element, interface, depairCurrentDevice);
    xmlUnlinkNode(root_element);
    xmlFreeNode(root_element);
    return result;
}

//
// This function needs to check all known devices to find the connected one, and make sure it's paired.
// If it's not paired, we boot it off (by setting DASBOOT flag or just immediately forcing the software "powered" setting off).
//
// NOTE: Devices connect and present the "Pair" prompt to users during which time they appear to be unpaired.  We need to be lenient
// in terms of letting this condition persist for several seconds so the user can push the button.
//

bool checkConnectionsForPairing(bool depairCurrentDevice)
{
    const char* result = dbus_introspect("org.bluez", "/org/bluez");

    if (result == NULL) {
        return false;
    }

    xmlDocPtr doc = NULL;
    doc = xmlParseDoc((const unsigned char*)result);
    
    char interface[8];
    if (findFirstInterface(doc, interface))
    {
        char dest[80];
        sprintf(dest, "/org/bluez/%s", interface);
        const char* deviceList = dbus_introspect("org.bluez", dest);

        if (deviceList == NULL) {
            return false;
        }
        xmlDocPtr deviceDoc = NULL;
        deviceDoc = xmlParseDoc((const unsigned char*)deviceList);
        
        bool result = scanDeviceList(deviceDoc, interface, depairCurrentDevice);
        xmlFreeDoc(deviceDoc);
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return result;
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return false;
}

// In the situation where a connected device generates Authentication Denied errors, it is likely that the user forgot the pairing
// from the phone.  We still see it as paired but it generates EAuthenticationFailedEvent messages.  So, let's tidy up our end.
void deletePairingRecord(const char* device, const char* interface)
{
    // call path /org/bluez/hci0 interface org.bluez.Adapter1 function RemoveDevice with arg of device path
    DBusConnection *connection = NULL;
    DBusError error;

    int cntRetries = 0;

retry:
    cntRetries++;
    try
    {
        dbus_error_init(&error);
        connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
        handle_errors(&error, "deletePairingRecord dbus_bus_get");
        
        DBusMessage *msgQuery = NULL;
        
        char path[80];
        sprintf(path, "/org/bluez/%s", interface);
        
        msgQuery = dbus_message_new_method_call("org.bluez", path, "org.bluez.Adapter1", "RemoveDevice");
        
        char parameter[80];
        sprintf(parameter, "/org/bluez/%s/%s", interface, device);
        
        const char* pptr = parameter;   // we have to do it this way - see the warning at
                                        // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#ga591f3aab5dd2c87e56e05423c2a671d9
        
        dbus_message_append_args (msgQuery, DBUS_TYPE_OBJECT_PATH, &pptr, DBUS_TYPE_INVALID);
        dbus_connection_send(connection, msgQuery, NULL);
        handle_errors(&error, "deletePairingRecord send");
        dbus_message_unref(msgQuery);
        dbus_connection_unref(connection);
        
        ggkSetDasBootFlag(); // force a disconnect
    }
    catch (DBusException ex)
    {
        LogWarn("Caught DBus Error - sleep and retry");
        dbus_connection_unref(connection);
        // to escape retry loop
        if (ggkGetServerRunState() >= EStopping || cntRetries > maxRetries) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        goto retry;
    }
}

}; // namespace Casa
