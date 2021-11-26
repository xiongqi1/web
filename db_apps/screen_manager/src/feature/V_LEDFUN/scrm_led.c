/*
 * scrm_led.c
 *    Screen UI LED support. Provides the LED top level menu as well as the
 *    LED vtable. The LED vtbale exports LED functionality to the
 *    other components of the screen manager via a controlled API. Internally
 *    the LED vtable implementation uses dispd.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or
 * object forms) without the expressed written consent of NetComm Wireless Pty.
 * Ltd Copyright laws and International Treaties protect the contents of this
 * file. Unauthorized use is prohibited.
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
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <linux/input.h>

#include <scrm.h>
#include <scrm_ops.h>
#include <rdb_ops.h>
#include <ngt.h>

static bool g_led_enable;
static int g_led_menu_id = -1;
static struct rdb_session *g_rdb_s;
static ngt_event_t *g_rdb_event;

/* Dispd rdb variables */
#define RDB_SCRM_DISPD_VAR "dispd"
#define RDB_SCRM_DISPD_DISABLE_VAR RDB_SCRM_DISPD_VAR".disable"
#define RDB_SCRM_DISPD_PATTERN_VAR RDB_SCRM_DISPD_VAR".pattern.type"

/* dispd LED patterns */
#define DISPD_PATTERN_NONE ""
#define DISPD_PATTERN_ALERT "msgalert"
#define DISPD_PATTERN_SUCCESS "success"
#define DISPD_PATTERN_FAIL "error"

/* Maps scrm status values to dispd LED patterns */
static struct {
    scrm_status_t status;
    const char *dispd_pattern;
} g_led_pattern_map[] = {
    { SCRM_STATUS_NONE, DISPD_PATTERN_NONE },
    { SCRM_STATUS_ALERT, DISPD_PATTERN_ALERT },
    { SCRM_STATUS_SUCCESS, DISPD_PATTERN_SUCCESS },
    { SCRM_STATUS_FAIL, DISPD_PATTERN_FAIL },
};

#define SCRM_LED_BUF_SIZE 32

/*
 * Process the LED enable state.
 */
static void
process_led_enable (void)
{
    const char *enable_str;
    char buf[SCRM_LED_BUF_SIZE];

    assert(g_led_menu_id >= 0);

    /*
     * Set the LED menu item text. The menu item toggles the state so the text
     * string is opposite to current enable value.
     */
    if (g_led_enable) {
        enable_str = _("Disable");
    } else {
        enable_str = _("Enable");
    }
    snprintf(buf, sizeof(buf),  _("%s LED"), enable_str);
    scrm_set_top_menu_item_text(g_led_menu_id, buf);

    /* Enable/disable dispd. */
    rdb_set_string(g_rdb_s, RDB_SCRM_DISPD_DISABLE_VAR, g_led_enable ? "0" :
                   "1");
}

/*
 * Event handler for LED rdb notifications.
 */
static int
led_rdb_event_handler (void *arg)
{
    UNUSED(arg);

    /*
     * No need to call rdb_getnames. Only one subscribed rdb variable
     * so just process it.
     */
    process_led_enable();

    return NGT_CONTINUE;
}

/*
 * Handler for the OK button press on the LED enabe/disable result
 * screen. Just closes and destroys the result screen.
 */
static int
result_ok_handler (void *screen_handle, ngt_widget_t *widget, void *arg)
{
    UNUSED(widget);
    UNUSED(arg);

    scrm_screen_close(screen_handle);
    scrm_screen_destroy(&screen_handle);

    return 0;
}

/*
 * LED enable/disable menu option select handler.
 */
static int
led_toggle_selected (ngt_widget_t *widget, void *arg)
{
    int enable;
    int rval;
    char buf[SCRM_LED_BUF_SIZE];
    scrm_status_t status;
    void *screen_handle;
    scrm_button_request_t button_req[] = {
        { BTN_1, OK_LABEL, result_ok_handler, NULL }
    };

    UNUSED(arg);
    UNUSED(widget);

    assert(g_rdb_s);

    INVOKE_CHK(rdb_get_int(g_rdb_s, RDB_SCRM_CONF_LED_ENABLE_VAR, &enable),
               "Unable to get rdb var %s", RDB_SCRM_CONF_LED_ENABLE_VAR);

    /* toggle enable value */
    g_led_enable = enable ? false : true;

    INVOKE_CHK(rdb_set_string(g_rdb_s, RDB_SCRM_CONF_LED_ENABLE_VAR,
                              g_led_enable ? "1" : "0"),
               "Unable to set rdb var %s", RDB_SCRM_CONF_LED_ENABLE_VAR);

 done:
    if (!rval) {
        status = SCRM_STATUS_SUCCESS;
        snprintf(buf, sizeof(buf), _("LED %s."),
                 g_led_enable ? _("enabled") : _("disabled"));
    } else {
        status = SCRM_STATUS_FAIL;
        snprintf(buf, sizeof(buf), _("Failed to %s LED."),
                 g_led_enable ? _("enabled") : _("disabled"));
    }

    /* Show a result screen */
    scrm_message_screen_create(buf,
                               button_req,
                               sizeof(button_req) /
                               sizeof(scrm_button_request_t),
                               &screen_handle);

    scrm_screen_show_with_timeout(screen_handle, status, -1, NULL, NULL);

    return rval;
}

/*
 * LED plugin API implementation. Show the given dispd pattern on LEDs.
 */
static int
led_show_pattern (const char *pattern)
{
    int rval = 0;

    assert(g_rdb_s);

    if (g_led_enable) {
        if (!pattern) {
            pattern = "";
        }
        rval = rdb_set_string(g_rdb_s, RDB_SCRM_DISPD_PATTERN_VAR, pattern);
    }

    return rval;
}

/*
 * LED plugin API implementation. Renders the LED to indicate the given
 * screen manager status.
 */
static int
led_show_status (scrm_status_t status)
{
    int pattern_map_len;
    int ix;

    assert(g_rdb_s);

    if (g_led_enable) {
        pattern_map_len =
            sizeof(g_led_pattern_map) / sizeof(*g_led_pattern_map);

        for (ix = 0; ix < pattern_map_len; ix++) {
            if (g_led_pattern_map[ix].status == status) {
                return (led_show_pattern(g_led_pattern_map[ix].dispd_pattern));
            }
        }

        return -EINVAL;
    }

    return 0;
}

static scrm_led_vtable_t led_vtable = {
    led_show_pattern,
    led_show_status,
};

/*
 * Clean up.
 */
static void
led_destroy (void)
{
    if (g_rdb_event) {
        ngt_run_loop_delete_event(g_rdb_event);
        g_rdb_event = NULL;
    }

    if (g_rdb_s) {
        rdb_close(&g_rdb_s);
    }
}

/*
 * Initialisation. Called once at the beginning of time.
 */
static int
led_init (void)
{
    int rval;
    int enable;

    scrm_led_vtable = &led_vtable;

    /* Create a top level menu option to enable/disable the LEDs. */
    g_led_menu_id =
        scrm_add_top_menu_item("", led_toggle_selected, NULL);

    if (g_led_menu_id < 0) {
        errp("Unable to add LED top menu item");
        led_destroy();

        /* holds an error code in failure case. */
        return g_led_menu_id;
    }

    /* Subscribe to and hook up notifications for the LED enable variable */
    INVOKE_CHK(rdb_open(NULL, &g_rdb_s), "Unable to open rdb");

    INVOKE_CHK(rdb_get_int(g_rdb_s, RDB_SCRM_CONF_LED_ENABLE_VAR,
                           &enable),
               "Unable to get rdb var %s", RDB_SCRM_CONF_LED_ENABLE_VAR);

    g_led_enable = enable ? true : false;

    INVOKE_CHK(rdb_subscribe(g_rdb_s, RDB_SCRM_CONF_LED_ENABLE_VAR),
               "Unable to subscribe to rdb var %s",
               RDB_SCRM_CONF_LED_ENABLE_VAR);

    g_rdb_event = ngt_run_loop_add_fd_event(rdb_fd(g_rdb_s),
                                            led_rdb_event_handler,
                                            NULL);
    INVOKE_CHK((!g_rdb_event), "Unable to add fd event");

    process_led_enable();

 done:
    if (rval) {
        led_destroy();
    }
    return rval;
}

scrm_feature_plugin_t scrm_led_plugin = {
    led_init,
    led_destroy,
};

