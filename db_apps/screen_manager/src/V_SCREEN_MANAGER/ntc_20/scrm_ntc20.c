/*
 * scrm_ntc20.c
 *    NTC20 screen manager support.
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
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <linux/input.h>

#include <rdb_ops.h>
#include <scrm_ops.h>
#include "scrm_ntc20.h"

static struct rdb_session *rdb_s;
static ngt_event_t *rdb_event;
static ngt_event_t *timer_event;

/*
 * Status and control bar widgets. There is only one copy of each of these
 * widgets. The same instance of each widget is placed into the status/control
 * bar for *every* screen. This not only reduces resource usage but also means
 * that the status of the widget (e.g. battery bars) can be updated once and
 * will show correctly no matter what screen is currently being displayed.
 */
static ngt_image_t *battery_widget;
static ngt_image_t *sim_signal_widget;
static ngt_label_t *bluetooth_status_widget;
static ngt_label_t *wifi_status_widget;

static int battery_update(void);
static int sim_signal_update(void);
static int bluetooth_status_update(void);
static int wifi_status_update(void);

/* Images for different battery levels */
static const char *battery_image_file[] = {
    SCRM_NTC20_IMG_BATT_0_PCENT,
    SCRM_NTC20_IMG_BATT_20_PCENT,
    SCRM_NTC20_IMG_BATT_40_PCENT,
    SCRM_NTC20_IMG_BATT_60_PCENT,
    SCRM_NTC20_IMG_BATT_80_PCENT,
    SCRM_NTC20_IMG_BATT_100_PCENT,
};

/*
 * Map 3G signal strength to an image showing the corresponsing number
 * of bars. The conversion threshold values used here are the same as
 * in the web UI.
 */
struct sim_signal_image_mapping {
    int dbm;
    const char *image;
} threeg_sig_to_image[] = {
    { -78, SCRM_NTC20_IMG_SIM_SIG_5_BAR },
    { -87, SCRM_NTC20_IMG_SIM_SIG_4_BAR },
    { -93, SCRM_NTC20_IMG_SIM_SIG_3_BAR },
    { -102, SCRM_NTC20_IMG_SIM_SIG_2_BAR },
    { -109, SCRM_NTC20_IMG_SIM_SIG_1_BAR },
};

/*
 * Handler for RDB events. Updates all widgets that are triggered off RDB.
 */
static int
rdb_event_handler (void *arg)
{
    char name_buf[RDB_GETNAME_BUF_LEN];
    int name_len = sizeof(name_buf);
    char *save_ptr;
    char *name_ptr;
    int do_battery_update = 0;
    int rval;

    UNUSED(arg);

    rval = rdb_getnames(rdb_s, "", name_buf, &name_len, TRIGGERED);
    if (rval) {
        return NGT_CONTINUE;
    }

    name_ptr = strtok_r(name_buf, RDB_GETNAMES_DELIMITER_STR, &save_ptr);
    while (name_ptr) {
        if (!strncmp(name_ptr, RDB_VAR_BATTERY, strlen(RDB_VAR_BATTERY))) {
            do_battery_update = 1;
        } else if (!strcmp(name_ptr, RDB_VAR_BLUETOOTH_ENABLE)) {
            bluetooth_status_update();
        } else if (!strcmp(name_ptr, RDB_VAR_WIFI_RADIO_VAR) ||
                   !strcmp(name_ptr, RDB_VAR_WIFI_CLIENT_RADIO_VAR)) {
            wifi_status_update();
        }
        name_ptr = strtok_r(NULL, RDB_GETNAMES_DELIMITER_STR, &save_ptr);
    }

    if (do_battery_update) {
        battery_update();
    }

    return NGT_CONTINUE;
}

/*
 * Handles timer events.
 */
static int
timer_event_handler (void *arg)
{
    UNUSED(arg);

    sim_signal_update();
    battery_update();

    return NGT_CONTINUE;
}

/*
 * Update the battery widget based on the battery RDB values.
 */
static int
battery_update (void)
{
    int batt_online;
    int batt_percent;
    const char *image_file;
    int ix;
    int percent_granuality;
    int rval = 0;

    assert(rdb_s);

    INVOKE_CHK(rdb_get_int(rdb_s, RDB_VAR_BATTERY_ONLINE, &batt_online),
               "Unable to get battery online");

    /*
     * Always read batt percent even if offline. To clear any possible
     * pending RDB trigger.
     */
    INVOKE_CHK(rdb_get_int(rdb_s, RDB_VAR_BATTERY_PERCENT, &batt_percent),
               "Unable to get battery percent");

    /* Sanity checks. Should never happen - but just in case. */
    if (batt_percent < 0) {
        batt_percent = 0;
    } else if (batt_percent > 100) {
        batt_percent = 100;
    }

    if (!batt_online) {
        image_file = NULL;
    } else {
        assert(SCRM_NTC20_BATT_ID_100_PCENT > 0);

        /* Convert the battery percentage to an index */
        percent_granuality = 100 / SCRM_NTC20_BATT_ID_100_PCENT;
        ix = (batt_percent + percent_granuality - 1) / percent_granuality;
        assert(ix < SCRM_NTC20_BATT_ID_MAX);
        image_file = battery_image_file[ix];
    }

    INVOKE_CHK(ngt_image_set_image(battery_widget, image_file),
               "Unable to set battery image");

 done:
    return rval;
}

/*
 * Initialise battery widgets and monitoring.
 */
static int
battery_init (void)
{
    int rval = 0;

    /*
     * Create an image widget. The actual image will be set during battery
     * update.
     */
    battery_widget = ngt_image_new(NULL);
    if (!battery_widget) {
        errp("Unable to create battery widget\n");
        rval = -EINVAL;
        goto done;
    }
    ngt_widget_ref_inc(NGT_WIDGET(battery_widget));

    /*
     * Watch for battery online/offline. Unlike online/offline, the
     * battery percent does not need to be updated in real time. So
     * battery percent is not subscribed to be rather updated
     * periodically (timer based).
     */
    INVOKE_CHK(rdb_subscribe(rdb_s, RDB_VAR_BATTERY_ONLINE),
               "Unable to subscribe to %s", RDB_VAR_BATTERY_ONLINE);

    /* First update to get the intial screen populated */
    INVOKE_CHK(battery_update(), "Unable to update battery");

 done:
    return rval;
}

/* Big enough to hold a string of format "AP"|"C"|"AP/C"|"OFF" */
#define WIFI_STATUS_BUF_LEN 16

/*
 * Update the wifi status widget based on the wifi RDB values.
 */
static int
wifi_status_update (void)
{
    int rval;
    int ap_enabled;
    int client_enabled;
    char status_buf[WIFI_STATUS_BUF_LEN];
    char *buf_p = status_buf;
    unsigned int len;

    INVOKE_CHK(rdb_get_int(rdb_s, RDB_VAR_WIFI_RADIO_VAR, &ap_enabled),
               "Unable to get rdb var %s", RDB_VAR_WIFI_RADIO_VAR);

    INVOKE_CHK(rdb_get_int(rdb_s, RDB_VAR_WIFI_CLIENT_RADIO_VAR,
                           &client_enabled),
               "Unable to get rdb var %s", RDB_VAR_WIFI_CLIENT_RADIO_VAR);

    if (!ap_enabled && !client_enabled) {
        len = snprintf(status_buf, sizeof(status_buf), _("OFF"));
        assert(len < sizeof(status_buf));
    } else {
        buf_p = status_buf;
        len = 0;
        if (ap_enabled) {
            len = snprintf(buf_p, sizeof(status_buf), "%s%s", _("AP"),
                           client_enabled ? "/" : "");
            assert(len < sizeof(status_buf));
            buf_p += len;
        }
        if (client_enabled) {
            len += snprintf(buf_p, sizeof(status_buf) - len, _("C"));
            assert(len < sizeof(status_buf));
        }
    }

    INVOKE_CHK(ngt_label_set_text(wifi_status_widget, status_buf),
               "Unable to set wifi status label");

 done:
    return rval;
}

/*
 * Initialise wifi status widget
 */
static int
wifi_status_init (void)
{
    int rval;

    wifi_status_widget = ngt_label_new(_(""));
    if (!wifi_status_widget) {
        errp("Unable to create wifi status widget\n");
        return -EINVAL;
    }
    ngt_widget_ref_inc(NGT_WIDGET(wifi_status_widget));

    INVOKE_CHK(wifi_status_update(), "Unable to update wifi status");

    INVOKE_CHK(rdb_subscribe(rdb_s, RDB_VAR_WIFI_RADIO_VAR),
               "Unable to subscribe to %s", RDB_VAR_WIFI_RADIO_VAR);
    INVOKE_CHK(rdb_subscribe(rdb_s, RDB_VAR_WIFI_CLIENT_RADIO_VAR),
               "Unable to subscribe to %s", RDB_VAR_WIFI_CLIENT_RADIO_VAR);

 done:
    return rval;
}

/*
 * Update the sim signal widget based on the signal strength RDB values.
 */
static int
sim_signal_update (void)
{
    const char *image_file = NULL;
    int rval = 0;
    int len;
    char buf[SCRM_NTC20_RDB_BUF_LEN];
    char *dbm;
    int dbm_val;
    unsigned int ix;

    len = sizeof(buf);
    INVOKE_CHK(rdb_get_string(rdb_s, RDB_VAR_SIM_STATUS, buf, len),
               "Unable to get rdb var %s", RDB_VAR_SIM_STATUS);

    if (!strcmp(buf, RDB_VAL_SIM_STATUS_OK)) {
        /* Read the signal strength DBM value from RDB */
        INVOKE_CHK(rdb_get_string(rdb_s, RDB_VAR_SIM_SIGNAL_STRENGTH, buf, len),
                   "Unable to get sim signal strength");

        /* Strip the trailing text to leave just the numberic dbm value */
        dbm = strstr(buf, "dBm");
        if (!dbm) {
            goto done;
        }
        *dbm = '\0';

        /*
         * Convert the dbm value to number of bars.
         */
        dbm_val = atoi(buf);
        image_file = SCRM_NTC20_IMG_SIM_SIG_0_BAR;
        if (dbm_val != 0) {
            for (ix = 0; ix < sizeof(threeg_sig_to_image) /
                     sizeof(struct sim_signal_image_mapping); ix++) {
                if (dbm_val >= threeg_sig_to_image[ix].dbm) {
                    image_file = threeg_sig_to_image[ix].image;
                    break;
                }
            }
        }
    }

    INVOKE_CHK(ngt_image_set_image(sim_signal_widget, image_file),
               "Unable to set sim signal image");

 done:
    return rval;
}

/*
 * Initialise sim signal widget.
 */
static int
sim_signal_init (void)
{
    int rval = 0;

    /*
     * Create an image widget. The actual image will be set during battery
     * update.
     */
    sim_signal_widget = ngt_image_new(NULL);
    if (!sim_signal_widget) {
        errp("Unable to create battery widget\n");
        return -EINVAL;
    }
    ngt_widget_ref_inc(NGT_WIDGET(sim_signal_widget));

    /* First update to get the intial screen populated */
    INVOKE_CHK(sim_signal_update(), "Unable to update sim signal");

 done:
    return rval;
}

/*
 * Update the bluetooth widget status.
 */
static int
bluetooth_status_update (void)
{
    int rval;
    int enabled;
    const char *status;

    INVOKE_CHK(rdb_get_int(rdb_s, RDB_VAR_BLUETOOTH_ENABLE, &enabled),
               "Unable to get rdb var %s", RDB_VAR_BLUETOOTH_ENABLE);

    status = enabled ? _("ON") : _("OFF");

    INVOKE_CHK(ngt_label_set_text(bluetooth_status_widget, status),
               "Unable to set bluetooth status label");

 done:
    return rval;
}

/*
 * Initialise bluetooth status widget
 */
static int
bluetooth_status_init (void)
{
    int rval;

    bluetooth_status_widget = ngt_label_new(_(""));
    if (!bluetooth_status_widget) {
        errp("Unable to create bluetooth status widget\n");
        return -EINVAL;
    }
    ngt_widget_ref_inc(NGT_WIDGET(bluetooth_status_widget));

    INVOKE_CHK(bluetooth_status_update(), "Unable to update bluetooth status");

    INVOKE_CHK(rdb_subscribe(rdb_s, RDB_VAR_BLUETOOTH_ENABLE),
               "Unable to subscribe to %s", RDB_VAR_BLUETOOTH_ENABLE);

 done:
    return rval;
}

/*
 * Clean up resources.
 */
static void
scrm_ntc20_destroy (void)
{
    if (battery_widget) {
        ngt_widget_dispose(NGT_WIDGET(battery_widget));
        battery_widget = NULL;
    }

    if (sim_signal_widget) {
        ngt_widget_dispose(NGT_WIDGET(sim_signal_widget));
        sim_signal_widget = NULL;
    }

    if (wifi_status_widget) {
        ngt_widget_dispose(NGT_WIDGET(wifi_status_widget));
        wifi_status_widget = NULL;
    }

    if (bluetooth_status_widget) {
        ngt_widget_dispose(NGT_WIDGET(bluetooth_status_widget));
        bluetooth_status_widget = NULL;
    }

    if (rdb_event) {
        ngt_run_loop_delete_event(rdb_event);
        rdb_event = NULL;
    }

    if (timer_event) {
        ngt_run_loop_delete_event(timer_event);
        timer_event = NULL;
    }

    if (rdb_s) {
        rdb_close(&rdb_s);
    }
}

/*
 * Screen manager init for NTC20.
 */
static int
scrm_ntc20_init (void)
{
    int rval = 0;

    INVOKE_CHK(rdb_open(NULL, &rdb_s), "Unable to open RDB");

    INVOKE_CHK(battery_init(), "Unable to init battery widget");

    INVOKE_CHK(wifi_status_init(), "Unable to init wifi widget");

    INVOKE_CHK(sim_signal_init(), "Unable to init sim signal widget");

    INVOKE_CHK(bluetooth_status_init(), "Unable to init bluetooth widget");

    /* Hook RDB notifications and timer events into the NGT run loop */
    rdb_event = ngt_run_loop_add_fd_event(rdb_fd(rdb_s), rdb_event_handler,
                                          NULL);

    timer_event = ngt_run_loop_add_timer_event(SCRM_NTC20_POLL_TIME_MSEC,
                                               timer_event_handler, NULL);
    if (!rdb_event || !timer_event) {
        rval = -EINVAL;
        errp("Unable to add rdb and timer events to run loop");
    }

 done:
    if (rval) {
        scrm_ntc20_destroy();
    }
    return rval;
}

/*
 * Destroy a screen. Frees all associated resources.
 */
static void
screen_destroy (void **screen_handle)
{
    scrm_ntc20_screen_t *screen;

    if (!screen_handle || !(*screen_handle)) {
        return;
    }

    screen = *screen_handle;
    if (screen->screen) {
        ngt_widget_dispose(NGT_WIDGET(screen->screen));
        screen->screen = NULL;
    }

    free(screen);
    *screen_handle = NULL;
}

/*
 * Create a status bar and attach it to the given screen. The status bar
 * is only created and filled with widgets as requested in status_bar_flags.
 */
static int
screen_status_bar_create (scrm_ntc20_screen_t *screen, int status_bar_flags)
{
    unsigned int sbar_row_defn[] = STATUS_BAR_ROW_DEFN;
    unsigned int sbar_column_defn[] = STATUS_BAR_COL_DEFN;
    int rval = 0;

    assert(screen);

    if (!(status_bar_flags & SCRM_BAR_FLAG_CREATE)) {
        /* Caller doesn't want the status bar */
        return 0;
    }

    /* Include status bar. Create it now */
    screen->status_bar =
        ngt_grid_layout_new(sizeof(sbar_row_defn) / sizeof(unsigned int),
                            sbar_row_defn,
                            sizeof(sbar_column_defn) / sizeof(unsigned int),
                            sbar_column_defn, NGT_UNIT_PROPORTIONAL);

    if (!screen->status_bar) {
        return -EINVAL;
    }

    /* Fill up status bar with default widgets */
    if (status_bar_flags & SCRM_BAR_FLAG_FILL) {

        /* Add wifi status widget */
        assert(wifi_status_widget);
        INVOKE_CHK(ngt_grid_layout_add_widget(screen->status_bar,
                                              NGT_WIDGET(wifi_status_widget),
                                              0, SCRM_NTC20_SBAR_WIFI),
                   "Unable to add wifi widget");

        INVOKE_CHK(ngt_widget_set_horizontal_alignment
                   (NGT_WIDGET(wifi_status_widget), NGT_ALIGN_LEFT),
                   "Unable to set wifi widget halign");

        /* Add sim signal widget */
        assert(sim_signal_widget);
        INVOKE_CHK(ngt_grid_layout_add_widget(screen->status_bar,
                                              NGT_WIDGET(sim_signal_widget),
                                              0, SCRM_NTC20_SBAR_SIM),
                   "Unable to add sim signal widget");

        INVOKE_CHK(ngt_widget_set_horizontal_alignment
                   (NGT_WIDGET(sim_signal_widget), NGT_ALIGN_CENTER),
                   "Unable to set sim signal widget halign");

         /* Add bluetooth status widget */
        assert(wifi_status_widget);
        INVOKE_CHK(ngt_grid_layout_add_widget(screen->status_bar,
                                              NGT_WIDGET
                                              (bluetooth_status_widget),
                                              0, SCRM_NTC20_SBAR_BLUETOOTH),
                   "Unable to add bluetooth widget");

        INVOKE_CHK(ngt_widget_set_horizontal_alignment
                   (NGT_WIDGET(bluetooth_status_widget), NGT_ALIGN_RIGHT),
                   "Unable to set bluetooth widget halign");
    }

    /* Add status bar to main layout */
    INVOKE_CHK(ngt_grid_layout_add_widget(screen->top_level_grid,
                                          NGT_WIDGET(screen->status_bar),
                                          screen->top_level_row_numbers
                                          [STATUS_BAR_ROW], 0),
               "Unable to add status bar to screen");

 done:
    return rval;
}

/*
 * Callback for when a button is pressed on a particular screen.
 */
static int
button_pressed_handler (ngt_widget_t *widget, void *arg)
{
    scrm_ntc20_screen_t *screen = arg;
    scrm_button_t *button;
    unsigned int ix;

    if (!screen) {
        return -EINVAL;
    }

    /*
     * Find the user callback for this button (if any) and invoke it.
     */
    for (ix = 0; ix < screen->num_control_buttons; ix++) {
        button = &(screen->control_buttons[ix]);
        if (button->button == widget) {
            if (button->cb) {
                button->cb(screen, widget, button->cb_arg);
            }
            break;
        }
    }

    return 0;
}

/*
 * Create a control bar and attach it to the given screen. The control bar
 * is only created and filled with widgets as requested in control_bar_flags.
 */
static int
screen_control_bar_create (scrm_ntc20_screen_t *screen, int control_bar_flags,
                           scrm_button_request_t *button_req,
                           unsigned int num_button_req)
{
    int rval = 0;
    unsigned int cbar_row_defn[] = CONTROL_BAR_ROW_DEFN;
    unsigned int cbar_column_defn[] = CONTROL_BAR_COL_DEFN;
    unsigned int ix;

    assert(screen);

    if (!(control_bar_flags & SCRM_BAR_FLAG_CREATE)) {
        /* Caller doesn't want the control bar */
        return 0;
    }

    /* Include control bar. Create it now */
    screen->control_bar =
        ngt_grid_layout_new(sizeof(cbar_row_defn) / sizeof(unsigned int),
                            cbar_row_defn,
                            sizeof(cbar_column_defn) / sizeof(unsigned int),
                            cbar_column_defn, NGT_UNIT_PROPORTIONAL);
    if (!screen->control_bar) {
        rval = -1;
        goto done;
    }

    /* Fill up control bar with default widgets */
    if (control_bar_flags & SCRM_BAR_FLAG_FILL) {

        /* Create requested button or label widget labels */
        screen->control_buttons = calloc(num_button_req,
                                         sizeof(*screen->control_buttons));
        if (!screen->control_buttons) {
            rval = -1;
            goto done;
        }

        screen->num_control_buttons = num_button_req;
        for (ix = 0; ix < num_button_req; ix++) {
            if (button_req[ix].cb) {
                /* Callback given, create button widget */
                screen->control_buttons[ix].button =
                    NGT_WIDGET(ngt_button_new(button_req[ix].key_code,
                                              button_req[ix].text,
                                              button_pressed_handler, screen));

                screen->control_buttons[ix].key_code =
                    button_req[ix].key_code;
                screen->control_buttons[ix].cb = button_req[ix].cb;
                screen->control_buttons[ix].cb_arg = button_req[ix].cb_arg;
            } else {
                /* No callback give, a label widget will suffice */
                screen->control_buttons[ix].button =
                    NGT_WIDGET(ngt_label_new(button_req[ix].text));
                screen->control_buttons[ix].key_code = -1; /* Invalid */
            }

            if (!screen->control_buttons[ix].button) {
                rval = -1;
                goto done;
            }

            /* Place the buttons into the right part of the control bar */
            switch (button_req[ix].key_code) {
            case BTN_0:
                INVOKE_CHK(
                           ngt_grid_layout_add_widget(screen->control_bar,
                                                      NGT_WIDGET
                                                      (screen->
                                                       control_buttons[ix].
                                                       button), 0,
                                                      SCRM_NTC20_CBAR_BTN0),
                           "Unable to create button 0\n");
                break;
            case BTN_1:
                INVOKE_CHK
                    (ngt_grid_layout_add_widget(screen->control_bar,
                                                NGT_WIDGET
                                                (screen->
                                                 control_buttons[ix].
                                                 button), 0,
                                                SCRM_NTC20_CBAR_BTN1),
                     "Unable to create button 1\n");
                INVOKE_CHK
                    (ngt_widget_set_horizontal_alignment
                     (NGT_WIDGET(screen->control_buttons[ix].button),
                      NGT_ALIGN_RIGHT),
                     "Unable to set button halign\n");
                break;
            default:
                break;
            }
        }

        /* Add battery widget to control bar */
        assert(battery_widget);
        INVOKE_CHK(ngt_grid_layout_add_widget(screen->control_bar,
                                              NGT_WIDGET(battery_widget),
                                              0, SCRM_NTC20_CBAR_BATTERY),
                   "Unable to add battery widget\n");
        INVOKE_CHK(ngt_widget_set_horizontal_alignment
                   (NGT_WIDGET(battery_widget), NGT_ALIGN_CENTER),
                   "Unable to set battery widget halign\n");
    }

    /* Add control bar to main layout */
    INVOKE_CHK
        (ngt_grid_layout_add_widget(screen->top_level_grid,
                                    NGT_WIDGET(screen->control_bar),
                                    screen->top_level_row_numbers
                                    [CONTROL_BAR_ROW],
                                    0),
         "Unable to add control bar to screen");

 done:
    return rval;
}

/*
 * Create a screen with the NTC20 layout.
 */
static int
screen_create (int status_bar_flags,
               int control_bar_flags,
               scrm_button_request_t *button_req,
               unsigned int num_button_req,
               void **screen_handle)
{
    unsigned int main_full_row_defn[] = MAIN_GRID_ROW_DEFN;
    unsigned int *main_row_defn;
    unsigned int main_column_defn[] = MAIN_GRID_COL_DEFN;
    ngt_grid_layout_t *main_grid;
    unsigned int ix;
    int rval = 0;
    scrm_ntc20_screen_t *screen = NULL;

    if ((!screen_handle) || (num_button_req > MAX_NUM_BUTTONS)) {
        return -EINVAL;
    }

    screen = calloc(1, sizeof(*screen));
    if (!screen) {
        rval = -errno;
        goto done;
    }

    /*
     * A single column grid is used to layout the main grid. The
     * number of rows in the grid depends on which optional screen
     * components the caller has requested. Start off assuming all rows
     * (status, main, control) will be present.  After that the
     * row_numbers and num_rows will be adjusted depending on whether
     * or not the caller wants to include the status bar and/or
     * control bar.
     */
    for (ix = 0; ix < MAIN_GRID_MAX_ROWS; ix++) {
        screen->top_level_row_numbers[ix] = ix;
    }

    /* Main content area is mandatory */
    screen->top_level_num_rows = 1;

    if (status_bar_flags & SCRM_BAR_FLAG_CREATE) {
        /* Include status bar */
        screen->top_level_num_rows++;
        main_row_defn = main_full_row_defn;
    } else {
        /* Don't include the status bar - increase main content area
         * to use the status bar space.
         */
        main_full_row_defn[MAIN_CONTENT_ROW] +=
            main_full_row_defn[STATUS_BAR_ROW];

        /* Skip the status bar */
        main_row_defn = &main_full_row_defn[MAIN_CONTENT_ROW];

        /* Shift the remaining rows up one row */
        screen->top_level_row_numbers[MAIN_CONTENT_ROW]--;
        screen->top_level_row_numbers[CONTROL_BAR_ROW]--;
    }

    if (control_bar_flags & SCRM_BAR_FLAG_CREATE) {
        /* Include control bar */
        screen->top_level_num_rows++;
    } else {
        /*
         * Don't include the control bar - increase main content area
         * to use the control bar space.
         */
        main_full_row_defn[MAIN_CONTENT_ROW] +=
            main_full_row_defn[CONTROL_BAR_ROW];
    }

    screen->screen = ngt_screen_new();
    if (!screen->screen) {
        rval = -EINVAL;
        goto done;
    }

    /*
     * Create the main grid with the above determined configuration based on
     * which of the screen components were requested.
     */
    main_grid = ngt_grid_layout_new(screen->top_level_num_rows, main_row_defn,
                                    sizeof(main_column_defn) /
                                    sizeof(unsigned int),
                                    main_column_defn, NGT_UNIT_PROPORTIONAL);
    if (!main_grid) {
        rval = -EINVAL;
        goto done;
    }

    screen->top_level_grid = main_grid;

    INVOKE_CHK(ngt_screen_set_layout(screen->screen, NGT_LAYOUT(main_grid)),
               "Unable to set screen layout");

    /* Create and fill the status bar (if requested) */
    INVOKE_CHK(screen_status_bar_create(screen, status_bar_flags),
               "Unable to create status bar");

    /* Create and fill the control bar (if requested) */
    INVOKE_CHK(screen_control_bar_create(screen, control_bar_flags, button_req,
                                         num_button_req),
               "Unable to create status bar");

 done:
    if (rval) {
        screen_destroy((void **)&screen);
    }

    *screen_handle = screen;
    return rval;
}

static int
screen_set_user_data (void *screen_handle, void *user_data)
{
    scrm_ntc20_screen_t *screen = screen_handle;

    if (!screen) {
        return -EINVAL;
    }

    screen->user_data = user_data;

    return 0;
}

static int
screen_get_user_data (void *screen_handle, void **user_data_p)
{
    scrm_ntc20_screen_t *screen = screen_handle;

    if (!screen || !user_data_p) {
        return -EINVAL;
    }

    *user_data_p = screen->user_data;

    return 0;
}

static int
screen_set_main_content (void *screen_handle, ngt_widget_t *widget)
{
    scrm_ntc20_screen_t *screen = screen_handle;
    ngt_grid_layout_t *main_grid;
    int rval;

    if (!screen || !screen->top_level_grid) {
        return -EINVAL;
    }

    main_grid = screen->top_level_grid;
    rval = ngt_grid_layout_add_widget(main_grid, widget,
                                      screen->top_level_row_numbers
                                      [MAIN_CONTENT_ROW],
                                      0);

    if (!rval) {
        screen->main_content = widget;
    }

    return rval;
}

static ngt_screen_t *
screen_widget_get (void *screen_handle)
{
    scrm_ntc20_screen_t *screen = screen_handle;

    if (!screen) {
        return NULL;
    }

    return (screen->screen);
}

scrm_plugin_t scrm_plugin = {
    scrm_ntc20_init,
    scrm_ntc20_destroy,
    screen_create,
    screen_destroy,
    screen_set_user_data,
    screen_get_user_data,
    screen_set_main_content,
    screen_widget_get,
};
