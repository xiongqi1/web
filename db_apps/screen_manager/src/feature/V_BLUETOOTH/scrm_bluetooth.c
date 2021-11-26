/*
 * scrm_bluetooth.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <linux/input.h>

#include <rdb_ops.h>
#include <ngt.h>
#include <scrm.h>
#include <scrm_ops.h>

#include "scrm_bluetooth.h"

struct rdb_session *scrm_bt_rdb_s;
static ngt_event_t *rdb_event;
static ngt_event_t *g_timer_event;
static void *g_bt_menu_screen_handle;
static ngt_list_t *g_bt_menu_widget;
static scrm_bt_toggle_var_t g_bt_enable =
    { RDB_BT_CONF_ENABLE_VAR, _("Bluetooth"), false };
static scrm_bt_toggle_var_t g_pairable_enable =
    { RDB_BT_CONF_PAIRABLE_VAR, _("Pairable"), false };
static int g_discoverable_time;

/* Forward declarations. */
static int scrm_bt_menu_update(bool create);

/*
 * percent decode the encoded string in place
 * Params:
 *   encoded: percent encoded string (null terminated)
 * Return:
 *   always return encoded for ease of function call chaining
 */
char *
scrm_bt_percent_decode (char * encoded)
{
    int i = 0, j = 0;
    char tmp[3] = {0, 0, 0};
    char * eptr;
    long c;
    while ( encoded[i] ) {
        if ( encoded[i] == '%' && encoded[i+1] ) {
            /* encoded char */
            tmp[0] = encoded[i+1];
            tmp[1] = encoded[i+2];
            c = strtol(tmp, &eptr, 16);
            if ( eptr == tmp ) {
                /* % is followed by non-hex chars, pass through */
                encoded[j++] = encoded[i++];
            } else if ( eptr == tmp + 1 ) {
                /* 1 char has been converted */
                encoded[j++] = (char)c;
                i += 2;
            } else {
                /* 2 chars have been converted */
                encoded[j++] = (char)c;
                i += 3;
            }
        } else {
            /* normal char. % followed by null is treated as a normal char */
            encoded[j++] = encoded[i++];
        }
    }
    encoded[j] = '\0';
    return encoded;
}

/*
 * Toggle the enable status of one of the BT conf variables and write
 * it to RDB.
 */
static int
scrm_bt_conf_val_toggle (scrm_bt_toggle_var_t *toggle_var)
{
    int rval = 0;

    if (!toggle_var) {
        return -EINVAL;
    }

    /* Commit toggled value to RDB. */
    rval = rdb_set_string(scrm_bt_rdb_s, toggle_var->rdb_name,
                          toggle_var->val ? "0" : "1");

    /*
     * RDB_BT_CONF_ENABLE_VAR is the template trigger variable which causes
     * btmgr to apply the BT config. If the toggle_var is not the ENABLE var
     * then set the ENABLE var now to do the trigger (otherwise already
     * triggered by above set).
     */
    if (!rval && (toggle_var != &g_bt_enable)) {
        rval = rdb_set_string(scrm_bt_rdb_s, RDB_BT_CONF_ENABLE_VAR,
                              g_bt_enable.val ? "1" : "0");
    }

    return rval;
}

/*
 * Handler for an OK button. Just closes and destroys the screen that the
 * button is part of.
 */
static int
scrm_bt_ok_handler (void *screen_handle, ngt_widget_t *widget, void *arg)
{
    UNUSED(widget);

    void **screen_handle_p = arg;

    if (!screen_handle) {
        return -EINVAL;
    }

    scrm_screen_close(screen_handle);
    scrm_screen_destroy(&screen_handle);

    if (screen_handle_p) {
        *screen_handle_p = NULL;
    }

    return 0;
}

/*
 * Handler for menu selections which toggle a config value.
 */
static int
scrm_bt_toggle_handler (ngt_widget_t *widget, void *arg)
{
    int rval = 0;
    char buf[SCRM_BT_BUF_SIZE];
    void *enable_result_screen_handle = NULL;
    const char *enable_str;
    scrm_status_t status;
    scrm_bt_toggle_var_t *toggle_var = arg;

    scrm_button_request_t button_req[] = {
        { BTN_1, OK_LABEL, scrm_bt_ok_handler, NULL }
    };

    UNUSED(widget);

    if (!toggle_var) {
        return -EINVAL;
    }

    /* Toggle the conf variable enable status */
    rval = scrm_bt_conf_val_toggle(toggle_var);
    if (rval) {
        enable_str = toggle_var->val ? _("disable") : _("enable");
        status = SCRM_STATUS_FAIL;
        snprintf(buf, sizeof(buf), _("Failed to %s %s"), enable_str,
                 toggle_var->property_name);
    } else {
        enable_str = toggle_var->val ? _("disabled") : _("enabled");
        status = SCRM_STATUS_SUCCESS;
        snprintf(buf, sizeof(buf), _("%s %s"), toggle_var->property_name,
                 enable_str);
    }

    /* Show a result screen */
    INVOKE_CHK(scrm_message_screen_create(buf,
                                          button_req,
                                          sizeof(button_req) /
                                          sizeof(scrm_button_request_t),
                                          &enable_result_screen_handle),
               "Unable to create enable result screen");

    INVOKE_CHK(scrm_screen_show(enable_result_screen_handle, status),
               "Unable to show BT menu screen");

 done:
    if (rval) {
        if (enable_result_screen_handle) {
            scrm_screen_destroy(&enable_result_screen_handle);
        }
    }
    return rval;
}

/*
 * Timer handler for polling the Discoverable timeout.
 */
static int
scrm_bt_timer_event_handler (void *arg)
{
    int rval = 0;

    UNUSED(arg);

    /* Get remaining time from the btmgr RPC service. */
    INVOKE_CHK(scrm_bt_discoverable_status(&g_discoverable_time),
               "Unable to get discoverable status");

    /* Update the menu with the updated time. */
    INVOKE_CHK(scrm_bt_menu_update(false), "Unable to update BT menu");

 done:
    if (rval || (g_discoverable_time <= 0)) {
        /*
         * Stop the timer if the countdown has reached zero or no timeout set.
         */
        return NGT_STOP;
    } else {
        return NGT_CONTINUE;
    }
}

/*
 * Start/stop the Discoverable timer. When enabled, the timer is used to update
 * The Discoverable timeout countdown in the menu item.
 */
#define DISCOVERABLE_POLL_TIME_MS 1000
static int
discoverable_timer_set (bool start)
{
    int rval = 0;

    if (start) {
        assert(!g_timer_event);
        g_timer_event =
            ngt_run_loop_add_timer_event(DISCOVERABLE_POLL_TIME_MS,
                                         scrm_bt_timer_event_handler,
                                         NULL);

        if (!g_timer_event) {
            rval = -1;
        }
    } else if (g_timer_event) {
        rval = ngt_run_loop_delete_event(g_timer_event);
        g_timer_event = NULL;
        g_discoverable_time = 0;
    }

    return rval;
}

/*
 * Handler for the Discoverable menu option.
 */
static int
scrm_bt_discoverable_handler (ngt_widget_t *widget, void *arg)
{
    char buf[SCRM_BT_BUF_SIZE];
    bool enable;
    int rval;
    const char *enable_str;
    void *result_screen_handle = NULL;
    scrm_status_t status;

    scrm_button_request_t button_req[] = {
        { BTN_1, OK_LABEL, scrm_bt_ok_handler, NULL }
    };

    UNUSED(widget);
    UNUSED(arg);

    /* Enable if Discoverable not already counting down. */
    enable = (g_discoverable_time == 0);

    /* Send it to the btmgr RPC service. */
    rval = scrm_bt_discoverable(enable);

    if (rval) {
        enable_str =  enable ? _("enable") : _("disable");
        status = SCRM_STATUS_FAIL;
        snprintf(buf, sizeof(buf), _("Failed to %s %s"), enable_str,
                 _("Discoverable"));
    } else {
        enable_str =  enable ? _("enabled") : _("disabled");
        status = SCRM_STATUS_SUCCESS;
        snprintf(buf, sizeof(buf), _("%s %s"), _("Discoverable"), enable_str);

        discoverable_timer_set(enable);
        INVOKE_CHK(scrm_bt_menu_update(false), "Unable to update BT menu");
    }

    /* Show a result screen */
    INVOKE_CHK(scrm_message_screen_create(buf,
                                          button_req,
                                          sizeof(button_req) /
                                          sizeof(scrm_button_request_t),
                                          &result_screen_handle),
               "Unable to create result screen");

    INVOKE_CHK(scrm_screen_show_with_timeout(result_screen_handle, status,
                                             -1, NULL, NULL),
               "Unable to show result screen");

 done:
    if (rval) {
        if (result_screen_handle) {
            scrm_screen_destroy(&result_screen_handle);
        }
    }
    return rval;
}

/*
 * Adds a status label text line to a widgets list. Ensures that there
 * is always a NULL sentinel at the end of the list.
 */
static int
add_status_line (ngt_widget_t *widgets[], unsigned int widgets_len,
                 unsigned int num_widgets, const char *label_text)
{
    ngt_label_t *label;

    if (num_widgets >= widgets_len - 1) {
        assert(0);
    } else {
        label = ngt_label_new(label_text);
        if (label) {
            widgets[num_widgets] = NGT_WIDGET(label);
            ngt_widget_set_font(widgets[num_widgets], NULL,
                                SCRM_DEFAULT_TEXT_SIZE);
            ngt_widget_set_margin(widgets[num_widgets], NGT_MARGIN_LEFT,
                                  SCRM_DEFAULT_MARGIN);

            num_widgets++;
            widgets[num_widgets] = NULL;
        }
    }

    return num_widgets;
}

/*
 * Updates the status screen. Used both to construct the initial status screen
 * and to update the status screen in response to rdb variable changes.
 * If the init argument is true then the status screen is created if not
 * already created. Otherwise the status screen is only updated if it
 * is already being shown.
 */
static int
status_screen_update (bool init)
{
    static void *status_screen_handle = NULL;
    ngt_widget_t *widgets[SCRM_BT_STATUS_WIDGETS_LEN];
    char buf[SCRM_MAX_NAME_LEN];
    char *buf_p;
    int rval = 0;
    unsigned int num_widgets = 0;
    int int_val;

    scrm_button_request_t button_req[] = {
        { BTN_0, SCROLL_LABEL, NULL, NULL },
        { BTN_1, OK_LABEL, scrm_bt_ok_handler, &status_screen_handle }
    };

    if (!status_screen_handle && !init) {
        /*
         * Status screen not currently being shown and this is not a
         * request to show it. So nothing to update and can just exit.
         */
        return 0;
    }

    /* BT Enable status. */
    snprintf(buf, sizeof(buf), "%s: %s", _("Bluetooth"),
             g_bt_enable.val ? _("ON") : _("OFF"));
    num_widgets = add_status_line(widgets, SCRM_BT_STATUS_WIDGETS_LEN,
                                  num_widgets, buf);

    /* BT name. */
    buf_p = buf;
    buf_p += snprintf(buf, sizeof(buf), "%s: ", _("Name"));
    rval = rdb_get_string(scrm_bt_rdb_s, RDB_BT_CONF_NAME_VAR, buf_p,
                          sizeof(buf) - (buf_p - buf));
    if (!rval) {
        num_widgets = add_status_line(widgets, SCRM_BT_STATUS_WIDGETS_LEN,
                                      num_widgets, buf);
    }

    /* BT Pairable status. */
    rval = rdb_get_int(scrm_bt_rdb_s, RDB_BT_CONF_PAIRABLE_VAR, &int_val);
    if (!rval) {
        snprintf(buf, sizeof(buf), "%s: %s", _("Pairable"),
                 int_val ? _("ON") : _("OFF"));
        num_widgets = add_status_line(widgets, SCRM_BT_STATUS_WIDGETS_LEN,
                                      num_widgets, buf);
    }

    /* BT Discoverable Timeout status. */
    rval = rdb_get_int(scrm_bt_rdb_s, RDB_BT_CONF_DISC_TIMEOUT_VAR,
                       &int_val);
    if (!rval) {
        /* Convert the rdb seconds value to minutes. */
        int_val /= 60;
        if (int_val > 0) {
            snprintf(buf, sizeof(buf), "%s: %d %s", _("Disc.Timeout"), int_val,
                     _("min"));
        } else {
            snprintf(buf, sizeof(buf), "%s: %s", _("Disc.Timeout"), _("Never"));
        }
        num_widgets = add_status_line(widgets, SCRM_BT_STATUS_WIDGETS_LEN,
                                      num_widgets, buf);
    }

    if (status_screen_handle) {
        /*
         * Status screen currently being shown. Easiest way to update it
         * is to close/destroy the current one and show a new one in its place.
         */
        scrm_screen_close(status_screen_handle);
        scrm_screen_destroy(&status_screen_handle);
    }

    /* Show the initial or updated status screen */
    INVOKE_CHK(scrm_scroll_screen_create(_("Bluetooth Status"),
                                         widgets,
                                         button_req,
                                         sizeof(button_req) /
                                         sizeof(scrm_button_request_t),
                                         &status_screen_handle),
               "Unable to create status screen");

    INVOKE_CHK(scrm_screen_show(status_screen_handle, SCRM_STATUS_NONE),
               "Unable to show status screen");

 done:
    if (rval) {
        if (status_screen_handle) {
            scrm_screen_destroy(&status_screen_handle);
        }
    }
    return rval;
}

/*
 * Handler for the BT Status menu option.
 */
static int
scrm_bt_status_handler (ngt_widget_t *widget, void *arg)
{
    UNUSED(widget);
    UNUSED(arg);

    return status_screen_update(true);
}

/*
 * Clean up all resources used by the BT screens.
 * This is called by the scrm core when the world ends and also when
 * the top level BT menu screen is closed.
 */
static void
scrm_bt_destroy (void)
{
    if (rdb_event) {
        ngt_run_loop_delete_event(rdb_event);
        rdb_event = NULL;
    }

    discoverable_timer_set(false);

    if (scrm_bt_rdb_s) {
        rdb_close(&scrm_bt_rdb_s);
    }
}

/*
 * Close and destroy the top level BT menu screen.
 */
static int
scrm_bt_screen_close (ngt_widget_t *widget, void *arg)
{
    void **screen_handle_p = arg;

    if (!screen_handle_p || !*screen_handle_p) {
        assert(0);
        return -EINVAL;
    }

    discoverable_timer_set(false);

    scrm_menu_screen_close(widget, *screen_handle_p);
    scrm_screen_destroy(screen_handle_p);

    return 0;
}

/*
 * Update the BT menu items. When BT is disabled some of the menu
 * items are not shown. If the create arg is true then the menu will
 * be created and shown (ie create, update and show).  Otherwise the
 * menu MUST have already been created and shown (ie, update only).
 */
static int
scrm_bt_menu_update (bool create)
{
    int rval = 0;
    unsigned int ix;
    const char *bt_enable_text;
    const char *discoverable_enable_text;
    const char *pairable_enable_text;
    char buf[SCRM_MAX_TEXT_LEN];
    unsigned int selected_index;
    static bool prev_bt_enable;

    if (!create && !g_bt_menu_screen_handle) {
        return -EINVAL;
    }

    if (!g_bt_menu_screen_handle) {
        /* Create the BT menu screen with the BT menu options */
        INVOKE_CHK(scrm_menu_screen_create(SCRM_BT_MENU_HEADER,
                                           &g_bt_menu_screen_handle,
                                           &g_bt_menu_widget, NULL, NULL),
                   "Unable to create BT menu screen");
    }

    /* The text is the opposite of the current status as it is a toggle */
    bt_enable_text = g_bt_enable.val ? _("Disable Bluetooth") :
        _("Enable Bluetooth");
    pairable_enable_text =
        g_pairable_enable.val ? _("Disable Pairable") : _("Enable Pairable");

    if (g_discoverable_time == 0) {
        /* Discoverable currently disabled. */
        discoverable_enable_text = _("Enable Discoverable");
    } else if (g_discoverable_time > 0) {
        /* Discoverable on and counting down. */
        snprintf(buf, sizeof(buf), "%s (%d:%02d)", _("Disable Disc."),
                 g_discoverable_time / 60, g_discoverable_time % 60);
        discoverable_enable_text = buf;
    } else {
        /* Discoverable on and no timeout. */
        discoverable_enable_text = _("Disable Discoverable");
    }

    scrm_bt_menu_item_t menu_items[] = {
        { _("Status"), scrm_bt_status_handler, NULL, true },
        { bt_enable_text, scrm_bt_toggle_handler, &g_bt_enable, true },
        { discoverable_enable_text, scrm_bt_discoverable_handler,
          NULL, false },
        { pairable_enable_text, scrm_bt_toggle_handler, &g_pairable_enable,
          false },
        { _("Start discover"), scrm_bt_discover_handler, NULL, false },
        { _("Paired devices"), scrm_bt_paired_dev_handler, NULL, false },
        { _("Available devices"), scrm_bt_available_dev_handler, NULL, false },
        { MENU_ITEM_BACK_LABEL, scrm_bt_screen_close, &g_bt_menu_screen_handle,
          true },
    };

    /*
     * Save the current menu position so that the selected item remains the
     * same after the menu items are re-populated.
     */
    INVOKE_CHK(ngt_list_get_selected_index(g_bt_menu_widget, &selected_index),
               "Unable to get menu item index");

    /*
     * Clear the current menu item entries as the new list will be different to
     * the current list.
     */
    INVOKE_CHK(ngt_list_clear_items(g_bt_menu_widget),
               "Unable to clear BT menu list");

    /*
     * Add the BT options into the BT menu. If BT is currently enabled then all
     * the entries are added. Otherwise only the ones marked as show_when_disabled
     * are added.
     */
    for (ix = 0; ix < sizeof(menu_items) / sizeof(*menu_items); ix++) {
        if (g_bt_enable.val || menu_items[ix].show_when_disabled) {
            INVOKE_CHK(ngt_list_add_item(g_bt_menu_widget, menu_items[ix].label,
                                         menu_items[ix].cb, menu_items[ix].cb_arg),
                       "Unable to add BT menu item");
        }
    }

    if ((prev_bt_enable == g_bt_enable.val) && ((int)selected_index >= 0)) {
        /*
         * Move the menu back to its original position. Only done if the
         * BT enable did not change. Because toggling the enable changes the
         * list of menu items in which case the previously selected item
         * is not relevant and the menu should be positioned at the top.
         */
        INVOKE_CHK(ngt_list_set_selected_index(g_bt_menu_widget,
                                               selected_index),
                   "Unable to set menu item index %d", selected_index);
    }
    prev_bt_enable = g_bt_enable.val;

    if (create) {
        INVOKE_CHK(scrm_screen_show(g_bt_menu_screen_handle, SCRM_STATUS_NONE),
                   "Unable to show BT menu screen");
    }

 done:
    return rval;
}

/*
 * Process the BT config from RDB. The init arg is set to true
 * the first time this function is called during init time which requires
 * full processing. Otherwise full processing is only done if there is an
 * actual change in the config values.
 */
static int
scrm_bt_process_config (bool init)
{
    int rval = 0;
    bool bt_enable_prev = g_bt_enable.val;
    bool pairable_enable_prev = g_pairable_enable.val;
    int bt_enable_cur;
    int pairable_enable_cur;

    INVOKE_CHK(rdb_get_int(scrm_bt_rdb_s, RDB_BT_CONF_ENABLE_VAR, &bt_enable_cur),
               "Unable to get rdb var %s", RDB_BT_CONF_ENABLE_VAR);

    g_bt_enable.val = bt_enable_cur ? true : false;

    INVOKE_CHK(rdb_get_int(scrm_bt_rdb_s, RDB_BT_CONF_PAIRABLE_VAR,
                           &pairable_enable_cur),
               "Unable to get rdb var %s", RDB_BT_CONF_ENABLE_VAR);

    g_pairable_enable.val = pairable_enable_cur ? true : false;

    if (!init && (g_bt_enable.val == bt_enable_prev) &&
        (g_pairable_enable.val == pairable_enable_prev)) {
        /* Not init time and no change in config. Exit early. */
        return 0;
    }

    if (g_bt_enable.val) {
        /*
         * BT being enabled. This is also the trigger for starting the btmgr.
         * Need to wait for the btmgr to be started as dependent on it.
         */
        INVOKE_CHK(scrm_bt_rpc_server_wait(), "Failed waiting for btmgr");

        /*
         * RDB_BT_OP_PAIR_STATUS_VAR is created by btmgr upon start up.
         * So can only subscribe to it after btmgr has started up.
         */
        INVOKE_CHK(rdb_subscribe(scrm_bt_rdb_s, RDB_BT_OP_PAIR_STATUS_VAR),
                   "Unable to subscribe to variable %s",
                   RDB_BT_OP_PAIR_STATUS_VAR);
    } else if (g_timer_event) {
        discoverable_timer_set(false);
    }

 done:
    return rval;
}

/*
 * Handler for the Bluetooth main menu option.
 */
static int
scrm_bt_selected (ngt_widget_t *widget, void *arg)
{
    int rval = 0;

    UNUSED(arg);
    UNUSED(widget);

    if (g_bt_enable.val) {
        INVOKE_CHK(scrm_bt_discoverable_status(&g_discoverable_time),
                   "Unable to get discoverable time");

        if (g_discoverable_time > 0) {
            discoverable_timer_set(true);
        }
    }

    INVOKE_CHK(scrm_bt_menu_update(true),
               "Unable to update BT menu");

 done:
    if (rval) {
        scrm_bt_destroy();
    }
    return rval;
}

/*
 * Event handler for bluetooth rdb notifications.
 */
static int
scrm_bt_rdb_event_handler (void *arg)
{
    char name_buf[MAX_NAME_LENGTH];
    int name_len = sizeof(name_buf);
    char *val_buf = NULL;
    int val_len = 0;
    int rval;
    char *save_ptr;
    char *name_ptr;

    UNUSED(arg);

    dbgp("\n");

    INVOKE_CHK(rdb_getnames(scrm_bt_rdb_s, "", name_buf, &name_len, TRIGGERED),
               "Unable to get rdb triggered vars %d", rval);

    dbgp("getnames: %s\n", name_buf);

    /*
     * Get each triggered rdb variable name, get it's value and pass that
     * value to the specific handler for that variable.
     */
    name_ptr = strtok_r(name_buf, RDB_GETNAMES_DELIMITER_STR, &save_ptr);
    while (name_ptr) {
        INVOKE_CHK(rdb_get_alloc(scrm_bt_rdb_s, name_ptr, &val_buf, &val_len),
                   "Unable to get rdb value for %s", name_ptr);

        if (!strcmp(name_ptr, RDB_BT_CONF_ENABLE_VAR) ||
            !strcmp(name_ptr, RDB_BT_CONF_PAIRABLE_VAR)) {
            scrm_bt_process_config(false);
            scrm_bt_menu_update(false);
            status_screen_update(false);
        } else if (!strcmp(name_ptr, RDB_BT_OP_PAIR_STATUS_VAR)) {
            scrm_bt_pair_status_update_handler(val_buf);
        } else {
            /* Unexpected rdb trigger */
            assert(0);
        }
        name_ptr = strtok_r(NULL, RDB_GETNAMES_DELIMITER_STR, &save_ptr);
    }

 done:
    if (val_buf) {
        free(val_buf);
    }
    return NGT_CONTINUE;
}

static int
scrm_bt_init (void)
{
    int rval;
    int menu_item_id;

    INVOKE_CHK(rdb_open(NULL, &scrm_bt_rdb_s), "Error opening RDB session");

    INVOKE_CHK(scrm_bt_process_config(true), "Unable to process BT config");

    INVOKE_CHK(rdb_subscribe(scrm_bt_rdb_s, RDB_BT_CONF_ENABLE_VAR),
               "Unable to subscribe to variable %s",
               RDB_BT_CONF_ENABLE_VAR);

    rdb_event = ngt_run_loop_add_fd_event(rdb_fd(scrm_bt_rdb_s),
                                          scrm_bt_rdb_event_handler,
                                          NULL);
    INVOKE_CHK((!rdb_event), "Unable to add fd event");

    menu_item_id = scrm_add_top_menu_item(_("Bluetooth"), scrm_bt_selected,
                                          NULL);
    if (menu_item_id < 0) {
        rval = menu_item_id;
        errp("Unable to add Bluetooth top menu item");
    }

 done:
    if (rval) {
        scrm_bt_destroy();
    }
    return rval;
}

scrm_feature_plugin_t scrm_bluetooth_plugin = {
    scrm_bt_init,
    scrm_bt_destroy,
};
