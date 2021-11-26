/*
 * bluez_support.h
 *    Defines for Bluez support.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef __BLUEZ_SUPPORT_H__
#define __BLUEZ_SUPPORT_H__

#include <glib.h>
#include <gio/gio.h>

#define BLUEZ_BUS_NAME "org.bluez"

#define BLUEZ_DEVICE_INTF "org.bluez.Device1"
#define BLUEZ_PROP_DEV_ADDRESS "Address"
#define BLUEZ_PROP_DEV_NAME "Name"
#define BLUEZ_PROP_DEV_PAIRED "Paired"

#define BLUEZ_ADAPTER_INTF "org.bluez.Adapter1"
#define BLUEZ_ADAPTER_PROP_POWERED "Powered"
#define BLUEZ_ADAPTER_PROP_ALIAS "Alias"
#define BLUEZ_ADAPTER_PROP_PAIRABLE "Pairable"
#define BLUEZ_ADAPTER_PROP_DISC_TIMEOUT  "DiscoverableTimeout"
#define BLUEZ_ADAPTER_PROP_DISCOVERABLE "Discoverable"

#define BTMGR_AGENT_CAPABILITY "DisplayYesNo"

#define BLUEZ_AGENT_PATH "/org/bluez/agent"
#define BLUEZ_AGENT_INTF "org.bluez.Agent1"
#define BLUEZ_AGENT_METHOD_REQ_CONF "RequestConfirmation"
#define BLUEZ_AGENT_METHOD_CANCEL "Cancel"

#define BLUEZ_AGENTMGR_PATH "/org/bluez"
#define BLUEZ_AGENTMGR_INTF "org.bluez.AgentManager1"

#define OBJ_MGR_INTF "org.freedesktop.DBus.ObjectManager"
#define PROPERTIES_INTF "org.freedesktop.DBus.Properties"

/*
 * Device property indexes.
 * NOTE: The order of these indexes must match the order of the entries
 * in the BZ_DEVICE_PROPERTIES array.
 */
typedef enum bz_device_prop_idx_ {
    PROP_DEV_ADDRESS = 0,
    PROP_DEV_NAME,
    PROP_DEV_PAIRED,
    PROP_DEV_MAX,
} bz_device_prop_idx_t;

typedef enum bz_pairing_state_ {
    /* No active pairing in progress. */
    PAIRING_NONE,
    /* Active pairing in progress. */
    PAIRING_INPROGRESS,
    /* Passkey confirmation request received. */
    PAIRING_CONFIRM_PASSKEY,
    /* Passkey confirmation sent. */
    PAIRING_PASSKEY_CONFIRMED,
    /* Remote device cancelled pairing request. */
    PAIRING_CANCEL,
    /* Last pairing operation succeeded. */
    PAIRING_SUCCESS,
    /* Las pairing operation failed. */
    PAIRING_FAIL,
} bz_pairing_state_t;

/* Large enough to store a BT/MAC address as a string */
#define BT_ADDR_LEN 20

#define BZ_BUF_LEN 128
#define BZ_DEV_FREELIST_MAX_SIZE 10

typedef struct bz_device_ {
    char dbus_path[BZ_BUF_LEN];
    char properties[PROP_DEV_MAX][BZ_BUF_LEN];
    int property_watch_id;
    struct bz_device_ *next;
} bz_device_t;

typedef struct bz_adapter_ {
    char address[BZ_BUF_LEN];
    char dbus_path[BZ_BUF_LEN];
} bz_adapter_t;

typedef struct bz_client_ {
    void *user_data;
    void (*ready)(void *user_data);
    void (*device_added)(bz_device_t *device, void *user_data);
    void (*device_removed)(bz_device_t *device, void *user_data);
} bz_client_t;

#define BZ_MAX_ERR_LEN 64
#define BZ_MAX_STATUS_LEN 256

typedef struct bz_pairing_operation_ {
    bz_pairing_state_t state;
    bz_device_t *device;
    unsigned int passkey;
    GDBusMethodInvocation *pending_invocation;
    char last_error[BZ_MAX_ERR_LEN];
} bz_pairing_operation_t;

typedef struct bz_device_free_list_ {
    int count;
    bz_device_t *free_list;
} bz_device_free_list_t;

extern char *BZ_DEVICE_PROPERTIES[];
extern int bz_init(bz_client_t *client);
extern int bz_run(void);
extern void bz_stop(void);
extern bz_device_t *bz_get_device_list(void);
extern int bz_set_adapter_property(char *prop_name, char *prop_value);
extern int bz_scan_start(void);
extern int bz_scan_stop(void);
extern int bz_pair(char *address);
extern int bz_get_pairing_status_string(char *status_buf, int len);
extern int bz_pairing_in_progress(void);
extern int bz_passkey_confirm(char *address, unsigned int passkey, int confirm);
extern int bz_unpair(char *address);

#endif /* __BLUEZ_SUPPORT_H__ */
