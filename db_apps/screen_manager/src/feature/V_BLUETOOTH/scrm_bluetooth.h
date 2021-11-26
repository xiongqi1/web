/*
 * scrm_bluetooth.h
 *    Bluetooth Screen UI support.
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
 */
#ifndef __SCRM_BLUETOOTH_H__
#define __SCRM_BLUETOOTH_H__

#include <stdbool.h>

#define SCRM_BT_MENU_HEADER _("Bluetooth Menu")

#define RDB_BT_VAR "bluetooth"
#define RDB_BT_CONF_VAR RDB_BT_VAR".conf"
#define RDB_BT_CONF_ENABLE_VAR RDB_BT_CONF_VAR".enable"
#define RDB_BT_CONF_NAME_VAR RDB_BT_CONF_VAR".name"
#define RDB_BT_CONF_PAIRABLE_VAR RDB_BT_CONF_VAR".pairable"
#define RDB_BT_CONF_DISC_TIMEOUT_VAR RDB_BT_CONF_VAR".discoverable_timeout"
#define RDB_BT_OP_VAR RDB_BT_VAR".op"
#define RDB_BT_OP_PAIR_STATUS_VAR RDB_BT_OP_VAR".pair.status"

#define SCRM_BT_BUF_SIZE 32
#define SCRM_BT_ERR_BUF_LEN 64
#define SCRM_BT_CONFIRM_MSG_BUF_LEN 128
#define SCRM_BT_STATUS_WIDGETS_LEN 10

#define SCRM_BT_STATUS_DELIM ";"

/* Stringification of an integer constant - expansion step */
#define SCRM_BT_CONST_INT_TO_STR_EXPAND(str) #str

/* Stringification of an integer constant */
#define SCRM_BT_CONST_INT_TO_STR(str) SCRM_BT_CONST_INT_TO_STR_EXPAND(str)

enum {
    SCRM_BT_MENU_STATUS = 0,
    SCRM_BT_MENU_ENABLE,
    SCRM_BT_MENU_DISCOVER,
    SCRM_BT_MENU_PAIRED_DEVS,
    SCRM_BT_MENU_AVAILABLE_DEVS,
};

typedef struct scrm_bt_menu_item_ {
    const char *label;
    ngt_widget_callback_t cb;
    void *cb_arg;
    bool show_when_disabled;
} scrm_bt_menu_item_t;

/*
 * Node used in device lists. Describes a single BT device
 */
typedef struct scrm_bt_device_ {
    struct scrm_bt_device_ *next;
    char *address;
    char *name;
    bool paired;
} scrm_bt_device_t;

/*
 * State for a device list.
 */
typedef struct scrm_bt_dev_list_data_ {
    scrm_bt_device_t *dev_list;
    void *screen_handle;
    ngt_list_t *list_widget;
} scrm_bt_dev_list_data_t;

/*
 * Pairing data.
 */
typedef struct scrm_bt_pairing_data_ {
    char *address;
    char *passkey;
} scrm_bt_pairing_data_t;

/*
 * Used to map one string (key) to another (data).
 */
typedef struct scrm_bt_str_map_ {
    const char *key;
    const char *data;
} scrm_bt_str_map_t;

typedef struct scrm_bt_toggle_var_ {
    char *rdb_name;
    char *property_name;
    bool val;
} scrm_bt_toggle_var_t;

extern struct rdb_session *scrm_bt_rdb_s;

extern int scrm_bt_discover_handler(ngt_widget_t *widget, void *arg);
extern int scrm_bt_available_dev_handler(ngt_widget_t *widget, void *arg);
extern int scrm_bt_paired_dev_handler(ngt_widget_t *widget, void *arg);
extern int scrm_bt_pair_handler(ngt_widget_t *widget, void *arg);
extern int scrm_bt_unpair_handler(ngt_widget_t *widget, void *arg);

extern int scrm_bt_get_devices(scrm_bt_device_t **dev_list, bool paired);
extern void scrm_bt_free_dev_list(scrm_bt_device_t **dev_list);
extern int scrm_bt_dev_list_screen_destroy(ngt_widget_t *widget, void *arg);
extern int scrm_bt_scan(char *seconds);
extern int scrm_bt_pair(char *address);
extern int scrm_bt_unpair(char *address);
extern int scrm_bt_passkey_confirm(char *address, char *passkey, bool confirm);
extern int scrm_bt_discoverable(bool enable);
extern int scrm_bt_discoverable_status(int *status);
extern int scrm_bt_pair_status_update_handler(char *pair_status_str);
extern int scrm_bt_rpc_server_wait(void);

extern char * scrm_bt_percent_decode(char * encoded);

#endif /* __SCRM_BLUETOOTH_H__ */
