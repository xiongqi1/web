/*
 * scrm_bluetooth_devices.c
 *    Bluetooth Screen UI support. Bluetooth device handling.
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
#include <errno.h>
#include <assert.h>
#include <linux/input.h>

#include <ngt.h>
#include <scrm.h>
#include <scrm_ops.h>

#include "scrm_bluetooth.h"

#define SCRM_BT_SCAN_SECS 10
#define SCRM_BT_SCAN_SECS_STR SCRM_BT_CONST_INT_TO_STR(SCRM_BT_SCAN_SECS)

/*
 * Called at the end of a discover operation.
 */
static int
scrm_bt_discover_end (void *screen_handle, void *arg)
{
    UNUSED(arg);

    scrm_screen_close(screen_handle);
    scrm_screen_destroy(&screen_handle);

    /*
     * Show the Available Devices screen which may have new/different
     * devices after the scan completes.
     */
    scrm_bt_available_dev_handler(NULL, NULL);

    return NGT_STOP;
}

/*
 * Called to cancel a scan operation.
 */
static int
scrm_bt_discover_cancel (void *screen_handle, ngt_widget_t *widget, void *arg)
{
    UNUSED(widget);
    UNUSED(arg);

    /* Close and destroy the processing screen to go back to previous screen */
    scrm_screen_close(screen_handle);
    scrm_screen_destroy(&screen_handle);

    return 0;
}

/*
 * Handles the "Start Discovery" BT menu option.
 */
int
scrm_bt_discover_handler (ngt_widget_t *widget, void *arg)
{
    void *screen_handle = NULL;

    scrm_button_request_t button_req[] = {
        { BTN_1, MENU_CANCEL_LABEL, scrm_bt_discover_cancel, NULL }
    };

    UNUSED(widget);
    UNUSED(arg);

    /* Start the scan */
    scrm_bt_scan(SCRM_BT_SCAN_SECS_STR);

    /* Display a processing screen for the duration of the scan */
    scrm_processing_screen_create(_("Scanning for bluetooth devices"),
                                  button_req, sizeof(button_req) /
                                  sizeof(scrm_button_request_t),
                                  &screen_handle);

    scrm_screen_show_with_timeout(screen_handle, SCRM_STATUS_ALERT,
                                  SCRM_BT_SCAN_SECS, scrm_bt_discover_end,
                                  NULL);

    return 0;
}

/*
 * Closes and destroys a device list screen.
 */
int
scrm_bt_dev_list_screen_destroy (ngt_widget_t *widget, void *arg)
{
    scrm_bt_dev_list_data_t *dev_list_data = arg;

    UNUSED(widget);

    if (dev_list_data) {
        if (dev_list_data->screen_handle) {
            /* Close and destroy the screen */
            scrm_screen_close(dev_list_data->screen_handle);
            scrm_screen_destroy(&dev_list_data->screen_handle);
        }

        /* Free the device list */
        scrm_bt_free_dev_list(&dev_list_data->dev_list);
        free(dev_list_data);
    }

    return 0;
}

/*
 * Fetch the list of BT devices and display a Device list screen.
 */
static int
scrm_bt_show_dev_list (bool paired)
{
    scrm_bt_device_t *dev;
    int rval = 0;
    scrm_bt_dev_list_data_t *dev_list_data;
    const char *header;
    const char *btn1_label;
    ngt_widget_callback_t cb;

    dev_list_data = calloc(1, sizeof(*dev_list_data));
    if (!dev_list_data) {
        return -errno;
    }

    /* Get device list from btmgr server */
    INVOKE_CHK(scrm_bt_get_devices(&(dev_list_data->dev_list), paired),
               "Unable to get BT devices");

    header = paired ? _("Paired Devices") : _("Available Devices");
    btn1_label = paired ? _("Unpair/Sel") : _("Pair/Sel");

    /* Create a menu screen to list the devices */
    INVOKE_CHK(scrm_menu_screen_create(header,
                                       &dev_list_data->screen_handle,
                                       &(dev_list_data->list_widget), NULL,
                                       btn1_label),
               "Unable to create BT available device screen");

    /*
     * Add all the BT devices to the list. Either paired or unpaired devices
     * are shown as requested. Each entry in the device will have a callback
     * registered to allow it to be unpaired or paired respectively.
     */
    dev = dev_list_data->dev_list;
    while (dev) {
        cb = paired ? scrm_bt_unpair_handler : scrm_bt_pair_handler;

        /*
         * The dev_list_data is passed to each of the callbacks.
         * It contains the data required to know which device is
         * currently selected.
         */
        INVOKE_CHK(ngt_list_add_item(dev_list_data->list_widget,
                                     dev->name, cb, dev_list_data),
                   "Unable to add BT available device item");
        dev = dev->next;
    }

    /* Add the Back option to exit this screen */
    INVOKE_CHK(ngt_list_add_item(dev_list_data->list_widget,
                                 MENU_ITEM_BACK_LABEL,
                                 scrm_bt_dev_list_screen_destroy,
                                 dev_list_data),
               "Unable to add BT available device back");

    INVOKE_CHK(scrm_screen_show(dev_list_data->screen_handle, SCRM_STATUS_NONE),
               "Unable to show screen");

 done:
    if (rval) {
        scrm_bt_dev_list_screen_destroy(NULL, dev_list_data);
    }
    return rval;
}

/*
 * Handler for the BT Paired Devices menu option.
 */
int
scrm_bt_paired_dev_handler (ngt_widget_t *widget, void *arg)
{
    UNUSED(widget);
    UNUSED(arg);

    return (scrm_bt_show_dev_list(true));
}

/*
 * Handler for the BT Available Devices menu option.
 */
int
scrm_bt_available_dev_handler (ngt_widget_t *widget, void *arg)
{
    UNUSED(widget);
    UNUSED(arg);

    return (scrm_bt_show_dev_list(false));
}
