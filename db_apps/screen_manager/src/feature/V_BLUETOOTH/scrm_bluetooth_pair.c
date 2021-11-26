/*
 * scrm_bluetooth_pair.c
 *    Bluetooth Screen UI support. Screen operation for pairing and unpairing
 *    BT devices.
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
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <linux/input.h>

#include <ngt.h>
#include <scrm.h>
#include <scrm_ops.h>

#include "scrm_bluetooth.h"

static int status_screen_timeout_handler(void *screen_handle, void *arg);

/*
 * Indicates whether an outgoing pairing request has been initiated
 * via the screen UI. As opposed to an outgoing pair request via
 * another UI such as the web UI or a incoming pair request.
 * If a pairing is initiated by the screen UI then the screen UI will
 * indicate its progress via progress screens. Otherwise only the passkey
 * confirm screen will be shown.
 */
static bool pair_initiated = false;

#define PAIRING_IN_PROGRESS_STR _("Pairing in progress")

/*
 * Generate a string that describes a pairing result. If error_code is NULL
 * then a success message is generated. Otherwise a failure message is
 * generated with the bluez error_code mapped to a user error string (same as
 * shown in the web UI).
 */
static void
gen_pairing_result_str (const char *error_code, char *msg_buf,
                        unsigned int msg_buf_len, bool pair)
{
    scrm_bt_str_map_t bz_error_str_map[] = {
        { "org.bluez.Error.Failed", _("Failed")},
        { "org.bluez.Error.AuthenticationFailed", _("Authentication failed")},
        { "org.bluez.Error.AuthenticationCanceled",
          _("Authentication cancelled")},
        { "org.bluez.Error.AuthenticationRejected",
          _("Authentication rejected")},
        { "org.bluez.Error.AuthenticationTimeout",
          _("Authentication timeout")},
        { "org.bluez.Error.ConnectionAttemptFailed",
          _("Connection attempt failed")},
        { "org.bluez.Error.AlreadyInProgress", _("Already in progress")},
        { "org.bluez.Error.AlreadyExists", _("Device already paired")},
        { "org.bluez.Error.InvalidDevice", _("Invalid device")},
        { "org.bluez.Error.NotPaired", _("Device not paired") }
    };

    unsigned int ix;
    const char *error_reason = _("failed");
    const char *pair_op_str = pair ? _("Pairing") : _("Unpairing");

    if (!error_code) {
        snprintf(msg_buf, msg_buf_len, "%s %s", pair_op_str, _("succeeded"));
    } else {
        for (ix = 0; ix < sizeof(bz_error_str_map) / sizeof(scrm_bt_str_map_t);
             ix++) {
            if (!strcmp(error_code, bz_error_str_map[ix].key)) {
                error_reason = bz_error_str_map[ix].data;
                break;
            }
        }

        snprintf(msg_buf, msg_buf_len, "%s %s: %s", pair_op_str, _("failed"),
                 error_reason);
    }
}

/*
 * During a pairing operation there is a single status screen that is shown.
 * The screen is updated each time the pairing state changes. The update is done
 * by closing the current status screen and displaying an updated one.
 *
 * Parameters:
 *    message    A message to display. A new status screen is created only
 *               if this parameter is not NULL.
 *    button_req    An array of button requests.
 *    num_button_req    Number of entries in the button_req array.
 *    timeout_secs    Screen timeout in seconds. 0 for no timeout. <0 for
 *                    default timeout.
 *    heartbeat    true to display a hearbeat. false otherwise.
 */
static int
update_pair_status_screen (scrm_status_t status,
                           const char *message,
                           scrm_button_request_t *button_req,
                           unsigned int num_button_req,
                           unsigned int timeout_secs,
                           bool heartbeat)
{
    static void *pair_status_screen_handle = NULL;
    scrm_screen_callback_t timeout_cb;
    int rval = 0;

    if (pair_status_screen_handle) {
        scrm_screen_close(pair_status_screen_handle);
        scrm_screen_destroy(&pair_status_screen_handle);
    }

    if (message) {
        if (heartbeat) {
            INVOKE_CHK(scrm_processing_screen_create
                       (message, button_req,
                        num_button_req,
                        &pair_status_screen_handle),
                       "Unable to create pair progress screen");
            timeout_cb = NULL;
        } else {
            INVOKE_CHK(scrm_message_screen_create(message, button_req,
                                                  num_button_req,
                                                  &pair_status_screen_handle),
                       "Unable to create pair status screen");
            timeout_cb = status_screen_timeout_handler;
        }

        INVOKE_CHK(scrm_screen_show_with_timeout(pair_status_screen_handle,
                                                 status, timeout_secs,
                                                 timeout_cb, NULL),
                   "Unable to show pair status screen");
    }

 done:
    return rval;
}

/*
 * Handler called when a pair status screen times out.
 */
static int
status_screen_timeout_handler (void *screen_handle, void *arg)
{
    UNUSED(arg);
    UNUSED(screen_handle);

    /* Close the status screen */
    update_pair_status_screen(SCRM_STATUS_NONE, NULL, NULL, 0, 0, false);

    return NGT_STOP;
}

/*
 * Handler invoked when user selects OK to close a status screen.
 */
static int
scrm_bt_status_screen_ok_handler (void *screen_handle, ngt_widget_t *widget,
                                  void *arg)
{
    UNUSED(screen_handle);
    UNUSED(widget);
    UNUSED(arg);

    /* Close and destroy the status screen. */
    update_pair_status_screen(SCRM_STATUS_NONE, NULL, NULL, 0, 0, false);

    return 0;
}

/*
 * Handle a pairing/unpairing request for a given device from the screen UI.
 */
static int
scrm_bt_do_pair (scrm_bt_dev_list_data_t *dev_list_data, bool pair)
{
    scrm_bt_device_t *dev;
    unsigned int dev_ix;
    int rval = 0;
    char result_msg[SCRM_BT_ERR_BUF_LEN];

    scrm_button_request_t button_req[] = {
        { BTN_1, OK_LABEL, scrm_bt_status_screen_ok_handler, NULL },
    };

    dbgp("\n");

    if (!dev_list_data) {
        return -EINVAL;
    }

    /*
     * Get the currently selected device. That is the one to be
     * paired/unpaired.
     */
    ngt_list_get_selected_index(dev_list_data->list_widget, &dev_ix);
    dev = dev_list_data->dev_list;
    while (dev && (dev_ix > 0)) {
        dev = dev->next;
        dev_ix--;
    }

    if (!dev) {
        return -EINVAL;
    }

    /* Send request to the btmgr */
    if (pair) {
        rval = scrm_bt_pair(dev->address);
    } else {
        rval = scrm_bt_unpair(dev->address);
    }

    dbgp("address %s\n", dev->address);

    /*
     * Close and destroy device list screen - the list will change
     * if the pairing/unpairing succeeds. Do this after the pair/unpair
     * as this call destroys the dev list which is needed for those operations.
     */
    scrm_bt_dev_list_screen_destroy(NULL, dev_list_data);

    if (!rval) {
        if (pair) {
            pair_initiated = true;
            /*
             * Display a processing screen for the duration of the
             * pair operation.
             */
            update_pair_status_screen(SCRM_STATUS_ALERT,
                                      PAIRING_IN_PROGRESS_STR, NULL, 0, 0,
                                      true);
        } else {
            gen_pairing_result_str(NULL, result_msg, sizeof(result_msg), false);

            update_pair_status_screen(SCRM_STATUS_SUCCESS,
                                      result_msg, button_req,
                                      sizeof(button_req) /
                                      sizeof(scrm_button_request_t), -1, false);
        }
    } else {
        /* Empty string as first param to get default error msg */
        gen_pairing_result_str("", result_msg, sizeof(result_msg), pair);

        update_pair_status_screen(SCRM_STATUS_FAIL, result_msg, button_req,
                                  sizeof(button_req) /
                                  sizeof(scrm_button_request_t), -1, false);
    }

    return rval;
}

/*
 * Pair button handler.
 */
int
scrm_bt_pair_handler (ngt_widget_t *widget, void *arg)
{
    scrm_bt_dev_list_data_t *dev_list_data = arg;

    dbgp("\n");

    UNUSED(widget);
    assert(dev_list_data);

    return (scrm_bt_do_pair(dev_list_data, true));
}

/*
 * Unpair button handler.
 */
int
scrm_bt_unpair_handler (ngt_widget_t *widget, void *arg)
{
    scrm_bt_dev_list_data_t *dev_list_data = arg;

    dbgp("\n");

    UNUSED(widget);
    assert(dev_list_data);

    return (scrm_bt_do_pair(dev_list_data, false));
}

/*
 * Confirm or deny a pair confirmation request.
 */
static int
scrm_bt_pair_confirm (scrm_bt_pairing_data_t *pairing_data, bool confirm)
{
    dbgp("confirm=%d\n", confirm);

    if (!pairing_data) {
        return -EINVAL;
    }

    /* Send confirm response to btmgr */
    scrm_bt_passkey_confirm(pairing_data->address, pairing_data->passkey,
                            confirm);

    free(pairing_data);

    if (pair_initiated) {
        /*
         * The confirm needs to be done on remote device as well so can take
         * some time. Display an in progress message.
         */
        update_pair_status_screen(SCRM_STATUS_NONE, PAIRING_IN_PROGRESS_STR,
                                  NULL, 0, 0, true);
    } else {
        /*
         * Passkey confirm was generated by an external (to screen UI) pair
         * request. Just dismiss the confirm screen.
         */
        update_pair_status_screen(SCRM_STATUS_NONE, NULL, NULL, 0, 0, false);
    }

    return 0;
}

/*
 * Handler for pair confirm YES button.
 */
static int
scrm_bt_pair_confirm_yes_handler (void *screen_handle, ngt_widget_t *widget,
                                  void *arg)
{
    scrm_bt_pairing_data_t *pairing_data = arg;

    UNUSED(screen_handle);
    UNUSED(widget);

    return (scrm_bt_pair_confirm(pairing_data, true));
}

/*
 * Handler for pair confirm NO button.
 */
static int
scrm_bt_pair_confirm_no_handler (void *screen_handle, ngt_widget_t *widget,
                                  void *arg)
{
    scrm_bt_pairing_data_t *pairing_data = arg;

    UNUSED(screen_handle);
    UNUSED(widget);

    return (scrm_bt_pair_confirm(pairing_data, false));
}

/*
 * Handler for changes in the RDB pair status variable. Note that this can
 * be triggered from a user pairing request via either screen or web UI and
 * it can also be triggered by an externally generated pair request from a
 * remote device.
 */
int
scrm_bt_pair_status_update_handler (char *pair_status_str)
{
    char *status_save_ptr;
    char *status_ptr;
    scrm_bt_pairing_data_t *pairing_data;
    const char *address;
    char *name;
    const char *passkey;

    dbgp("status_str: %s\n", pair_status_str);

    /*
     * Get the pairing state string. The string is defined by btmgr and has
     * this format:
     *
     * If pair_state = confirm_passkey:
     *    confirm_passkey;<passkey>;<address>;<name>
     * If pair_state = fail:
     *    fail;<error string>;<address>;<name>
     * Else:
     *    <pair_state>;<address>;<name>
     */

    status_ptr = strtok_r(pair_status_str, SCRM_BT_STATUS_DELIM,
                          &status_save_ptr);

    if (!status_ptr) {
        assert(0);
        return -EINVAL;
    }

    if (!strcmp(status_ptr, "confirm_passkey")) {

        /* Format: confirm_passkey;<passkey>;<address>;<name> */

        char msg_buf[SCRM_BT_CONFIRM_MSG_BUF_LEN];

        /* Get the passkey */
        passkey = strtok_r(NULL, SCRM_BT_STATUS_DELIM,
                           &status_save_ptr);
        if (!passkey) {
            assert(0);
            return -EINVAL;
        }

        /* Get the device address */
        address = strtok_r(NULL, SCRM_BT_STATUS_DELIM,
                           &status_save_ptr);
        if (!address) {
            assert(0);
            return -EINVAL;
        }

        /* Get the device name */
        name = strtok_r(NULL, SCRM_BT_STATUS_DELIM,
                        &status_save_ptr);
        if (!name) {
            assert(0);
            return -EINVAL;
        }
        /*
         * percent decode since bt_name might have been percent encoded.
         * note: this should be done after parsing the name token.
         */
        scrm_bt_percent_decode(name);

        pairing_data = calloc(1, sizeof(*pairing_data));
        if (!pairing_data) {
            return -errno;
        }

        pairing_data->passkey = strdup(passkey);
        pairing_data->address = strdup(address);

        /*
         * Create a screen that prompts the user to confirm the passkey
         * for the given device. The screen has two buttons, YES and NO,
         * with corresponding handlers to confirm or deny the request.
         */
        scrm_button_request_t button_req[] = {
            { BTN_0, _("Yes"), scrm_bt_pair_confirm_yes_handler,
              pairing_data },
            { BTN_1, _("No"), scrm_bt_pair_confirm_no_handler,
              pairing_data }
        };

        snprintf(msg_buf, sizeof(msg_buf),
                 _("Confirm passkey %s to pair with %s"), passkey, name);

        update_pair_status_screen(SCRM_STATUS_ALERT, msg_buf, button_req,
                                  sizeof(button_req) /
                                  sizeof(scrm_button_request_t), 0, false);

    } else if (!strcmp(status_ptr, "passkey_confirmed") ||
               !strcmp(status_ptr, "cancel")) {
        if (!pair_initiated) {
            update_pair_status_screen(SCRM_STATUS_NONE, NULL, NULL, 0, 0,
                                      false);
        }
    } else if ((!strcmp(status_ptr, "fail") ||
                !strcmp(status_ptr, "success"))) {

        if (!pair_initiated) {
            /*
             * External pair request. Just dismiss the confirm passkey
             * screen if it is currently still displayed (that's the
             * only possible screen for external pair requests).
             */
            update_pair_status_screen(SCRM_STATUS_NONE, NULL, NULL, 0, 0,
                                      false);
        } else {

            /*
             * Success format: success;<address>;<name>
             * Fail format: fail;<error_code>;<address>;<name>
             */

            scrm_button_request_t button_req[] = {
                { BTN_1, OK_LABEL, scrm_bt_status_screen_ok_handler, NULL },
            };

            char result_msg[SCRM_BT_ERR_BUF_LEN];
            const char *error_code = NULL;
            scrm_status_t status;

            if (!strcmp(status_ptr, "fail")){
                /* On failure get the error code */
                error_code = strtok_r(NULL, SCRM_BT_STATUS_DELIM,
                                      &status_save_ptr);
                status = SCRM_STATUS_FAIL;
                assert(error_code);
            } else {
                status = SCRM_STATUS_SUCCESS;
            }

            /* Display a success or fail screen with default timeout */
            gen_pairing_result_str(error_code, result_msg, sizeof(result_msg),
                                   true);

            update_pair_status_screen(status, result_msg, button_req,
                                      sizeof(button_req) /
                                      sizeof(scrm_button_request_t), -1, false);

            pair_initiated = false;
        }
    }

    return 0;
}
