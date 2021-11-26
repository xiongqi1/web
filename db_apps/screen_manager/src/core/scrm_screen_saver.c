/*
 * scrm_screen_saver.c
 *    Screen UI screen saver support. The display is turned off after a
 *    certain amount of idle time and is turned back on when activity occurs.
 *    If enabled, a lock screen is shown when the display is turned back on.
 *    The lock screen requires an unlock code to be entered. The lock screen
 *    is intended as a safety feature (e.g. to keep out curious children and
 *    unintended handbag button presses) and not as a security feature. Hence
 *    the unlock code is displayed on the lock screen.
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
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <linux/input.h>

#include <ngt.h>
#include <scrm.h>
#include <scrm_ops.h>
#include <rdb_ops.h>

#define UNLOCK_CODE_MAX_LEN 10
#define SCRM_SCREEN_SAVER_BUF_SIZE 32

#define SCREEN_LOCK_MENU_FMT_STR _("%s screen lock")

static int g_screen_saver_menu_id = -1;
static void *g_lock_screen_handle;

/* Whether screen lock is currently enabled or not. */
static bool g_lock_enable;

/*
 * The unlock code. A string containing sequence of only 0s and 1s
 * (e.g. "0110"). Read from RDB.
 */
static char g_unlock_code_str[UNLOCK_CODE_MAX_LEN + 1];

/* The code entered by the user. Is updated as each code char is entered. */
static char g_entered_code_str[UNLOCK_CODE_MAX_LEN + 1];

/*
 * Label for the user entered code. Its text is updated as the code is
 * entered to give visual feedback to the user.
 */
static ngt_label_t *g_code_entered_label;

/* Forward declarations */
static int lock_screen_label_create(ngt_label_t **label_p, const char *text,
                                    ngt_linear_layout_t *linear_layout);
static int lock_press_0_handler(void *screen_handle, ngt_widget_t *widget,
                                    void *arg);
static int lock_press_1_handler(void *screen_handle, ngt_widget_t *widget,
                                    void *arg);
static void lock_state_reset(void);

/*
 * Creates the lock screen. Can be safely called even if the lock screen
 * already exists. Does not show the screen. The lock screen contains:
 *    - A message prompting user to enter the unlock code. The unlock code
 *      itself is part of the message (lock is for safety not security).
 *    - A line that updates with the user's input as it is entered.
 *    - Button labels ("0" and "1"). Corresponds to the valid code characters.
 */
static int
lock_screen_create (void)
{
    int rval = 0;
    char lock_msg[64];
    ngt_linear_layout_t *linear_layout;
    static ngt_label_t *lock_msg_label;

    scrm_button_request_t button_req[] = {
        { BTN_0, "0", lock_press_0_handler, NULL },
        { BTN_1, "1", lock_press_1_handler, NULL }
    };

    if (g_lock_screen_handle) {
        /* Lock screen already created */
        return 0;
    }

    INVOKE_CHK(scrm_screen_create(SCRM_BAR_FLAG_NONE,
                                  SCRM_BAR_FLAG_CREATE | SCRM_BAR_FLAG_FILL,
                                  button_req,
                                  sizeof(button_req) /
                                  sizeof(scrm_button_request_t),
                                  &g_lock_screen_handle),
               "Unable to create lock screen");

    /* Create a centered vertical linear layout */
    linear_layout = ngt_linear_layout_new(NGT_VERTICAL);
    if (!linear_layout) {
        rval = -1;
        goto done;
    }
    INVOKE_CHK(ngt_widget_set_vertical_alignment(NGT_WIDGET(linear_layout),
                                                 NGT_ALIGN_CENTER),
               "Unable to set linear layout vertical alignment");
    INVOKE_CHK(ngt_widget_set_horizontal_alignment(NGT_WIDGET(linear_layout),
                                                   NGT_ALIGN_CENTER),
               "Unable to set linear layout horiztonal alignment");

    /* Add a label that prompts user to enter the unlock code. */
    snprintf(lock_msg, sizeof(lock_msg),
             _("Screen locked. Enter code %s to unlock."),
             g_unlock_code_str);
    INVOKE_CHK(lock_screen_label_create(&lock_msg_label, lock_msg,
                                        linear_layout),
               "Unable to create lock message");

    /* Add a label that echos the code being entered by the user. */
    INVOKE_CHK(lock_screen_label_create(&g_code_entered_label, "",
                                        linear_layout),
               "Unable to create lock sequence label");

    INVOKE_CHK(scrm_screen_set_main_content(g_lock_screen_handle,
                                            NGT_WIDGET(linear_layout)),
               "Failed to set main content");
 done:
    if (rval) {
        if (g_lock_screen_handle) {
            scrm_screen_destroy(&g_lock_screen_handle);
        }
    }

    return rval;
}

/*
 * Close and destroy the lock screen.
 */
static void
lock_screen_destroy (void)
{
    if (g_lock_screen_handle) {

        /*
         * Close the lock screen in case it is currently open. No harm is done
         * if it is already closed.
         */
        scrm_screen_close(g_lock_screen_handle);

        scrm_screen_destroy(&g_lock_screen_handle);
    }
}

/*
 * Show the lock screen.
 */
static int
lock_screen_show (bool show)
{
    int rval = 0;

    if (show) {
        lock_state_reset();
        INVOKE_CHK(scrm_screen_show(g_lock_screen_handle, SCRM_STATUS_NONE),
                   "Uable to show lock screen");
    } else {
        INVOKE_CHK(scrm_screen_close(g_lock_screen_handle),
                   "Uable to show lock screen");
    }

 done:

    return rval;
}

/*
 * Helper function that creates a label with the given text, aligns it
 * correctly and adds it to the given linear layout.
 */
static int
lock_screen_label_create (ngt_label_t **label_p, const char *text,
                          ngt_linear_layout_t *linear_layout)
{
    int rval = 0;

    if (!label_p) {
        return -EINVAL;
    }

    *label_p = ngt_label_new(text);
    if (!(*label_p)) {
        rval = -1;
        goto done;
    }

    /* Use default NGT font but with a different font size */
    INVOKE_CHK(ngt_widget_set_font(NGT_WIDGET(*label_p), NULL,
                                   SCRM_DEFAULT_TEXT_SIZE),
               "Unable to set message font");

    /* Center the message */
    INVOKE_CHK(ngt_widget_set_horizontal_alignment(NGT_WIDGET(*label_p),
                                                   NGT_ALIGN_CENTER),
               "Unable to set horizontal alignment");

    INVOKE_CHK(ngt_widget_set_vertical_alignment(NGT_WIDGET(*label_p),
                                                 NGT_ALIGN_CENTER),
               "Unable to set vertical alignment");

    INVOKE_CHK(ngt_linear_layout_add_widget(linear_layout,
                                            NGT_WIDGET(*label_p)),
               "Unable to add label to layout");

 done:
    if (rval) {
        if (*label_p) {
            ngt_widget_dispose(NGT_WIDGET(*label_p));
            *label_p = NULL;
        }
    }

    return rval;
}

/*
 * Resets the lock state. Is called when the lock screen is created and when
 * the user has entered an incorrect code. In both cases the state is reset
 * which results in the user needing to re-enter the code from the
 * beginning.
 */
static void
lock_state_reset (void)
{
    /*
     * The state is really just the currently entered code. Reset the
     * variables to do with that.
     */
    memset(g_entered_code_str, '\0', sizeof(g_entered_code_str));
    ngt_label_set_text(g_code_entered_label, "");
}

/*
 * Update the lock state. The state tracks whether the unlock code has been
 * correctly entered or not and performs the appropriate operation. The state
 * needs to be updated every time a unlock code character is entered.
 */
static void
lock_state_update (short key_code)
{
    char seq_char;
    size_t entered_len;

    if (key_code == BTN_0) {
        seq_char = '0';
    } else if (key_code == BTN_1) {
        seq_char = '1';
    } else {
        assert(0);
        return;
    }

    /* Update the code entered by the user up to this time. */
    entered_len = strlen(g_entered_code_str);
    g_entered_code_str[entered_len++] = seq_char;
    ngt_label_set_text(g_code_entered_label, g_entered_code_str);

    /*
     * Check whether the user has entered enough characters yet.
     * If so, can check whether the entered code is correct.
     * If not, continue waiting for more input characters.
     */
    if (entered_len == strlen(g_unlock_code_str)) {
        if (!strncmp(g_entered_code_str, g_unlock_code_str, entered_len)) {
            /* Entered code is correct. Dismiss the lock screen. */
            lock_screen_show(false);
        } else {
            /*
             * Entered code is incorrect. User needs to re-enter the code
             * from the beginning.
             */
            lock_state_reset();
        }
    }
}

/*
 * Button 1 handler for the lock screen. Just updates the lock state
 * using the corresponding key code.
 */
static int
lock_press_0_handler (void *screen_handle, ngt_widget_t *widget,
                      void *arg)
{
    UNUSED(screen_handle);
    UNUSED(arg);
    UNUSED(widget);

    lock_state_update(BTN_0);

    return NGT_STOP;
}

/*
 * Button 0 handler for the lock screen. Just updates the lock state
 * using the corresponding key code.
 */
static int
lock_press_1_handler (void *screen_handle, ngt_widget_t *widget,
                      void *arg)
{
    UNUSED(screen_handle);
    UNUSED(arg);
    UNUSED(widget);

    lock_state_update(BTN_1);

    return NGT_STOP;
}

/*
 * Idle timeout callback. Turns the screen on and off when the system
 * becomes idle or active respectively. Also tickles the system LED reset
 * as required.
 */
static void
idle_timeout_callback (ngt_idle_timeout_state_t state,
                       void *arg)
{
    ngt_display_mode_t mode = NGT_DISPLAY_MODE_ON;
    struct rdb_session *rdb_s;

    UNUSED(arg);

    switch (state) {
    case NGT_ITS_IDLE:
        mode = NGT_DISPLAY_MODE_OFF;
        break;
    case NGT_ITS_ACTIVE:
        mode = NGT_DISPLAY_MODE_ON;
        if (g_lock_enable) {
            lock_screen_show(true);
        }
        break;
    case NGT_ITS_TICKLE:
        if (!rdb_open(NULL, &rdb_s)) {
            /*
             * Tickle the led reset timer. dispd uses that to maintain its
             * dimscreen timer.
             */
            rdb_set_string(rdb_s, RDB_SYS_RESET_LED_OFF_TIMER_VAR, "1");
            rdb_close(&rdb_s);
        }
        break;
    default:
        assert(0);
        return;
    }

    if (state != NGT_ITS_TICKLE) {
        ngt_display_mode_set(mode);
    }
}

/*
 * Load the screen saver config from rdb. These rdb variables are loaded:
 *    - screen_manager.conf.display_timeout: Number of seconds before the
 *      physical display turns off.
 *    - screen_manager.conf.lock_enable: 0 for enable, 1 for disable.
 *      If enabled a lock screen will be shown when the display turns back
 *      on after timeout. Otherwise the current screen will be shown.
 *    - screen_manager.conf.unlock_code: Code that needs to be entered to
 *      close the lock screen.
 */
static int
load_config (void)
{
    int display_timeout;
    int len;
    int lock_enable;
    int rval = 0;
    char buf[SCRM_SCREEN_SAVER_BUF_SIZE];
    struct rdb_session *rdb_s = NULL;

    INVOKE_CHK(rdb_open(NULL, &rdb_s), "Error opening RDB session");

    INVOKE_CHK(rdb_get_int(rdb_s, RDB_SCRM_CONF_DISPLAY_TIMEOUT_VAR,
                           &display_timeout),
               "Unable to get rdb var %s", RDB_SCRM_CONF_DISPLAY_TIMEOUT_VAR);

    if (display_timeout < 0) {
        errp("Invalid display timeout configuration value %d", display_timeout);
        return -EINVAL;
    }

    INVOKE_CHK(rdb_get_int(rdb_s, RDB_SCRM_CONF_LOCK_ENABLE_VAR,
                           &lock_enable),
               "Unable to get rdb var %s", RDB_SCRM_CONF_LOCK_ENABLE_VAR);

    g_lock_enable = lock_enable ? true : false;

    /* Get the unlock code */
    len = sizeof(g_unlock_code_str);
    INVOKE_CHK(rdb_get(rdb_s, RDB_SCRM_CONF_UNLOCK_CODE_VAR,
                       g_unlock_code_str, &len),
               "Unable to get rdb var %s", RDB_SCRM_CONF_UNLOCK_CODE_VAR);

    /*
     * Sanity checks - should never be true.
     * Unlock sequence needs to be a string of at least 1 character long.
     */
    if ((len <= 1) || (g_unlock_code_str[len - 1] != '\0')) {
        errp("Invalid unlock code. Not a valid string.");
        return -EINVAL;
    }

    if (g_lock_enable) {
        INVOKE_CHK(lock_screen_create(), "Unable to create lock screen");
    } else {
        lock_screen_destroy();
    }

    INVOKE_CHK(ngt_idle_timeout_set(display_timeout, idle_timeout_callback,
                                    NULL), "Unable to set idle timeout");

    /*
     * Set the led timer. Set it to be 1 second longer than the
     * display timeout to ensure that the led changes after the
     * display turns off (the display timer and the led timer are
     * seperate and handled by screen manager and dispd respectively).
     */
    snprintf(buf, sizeof(buf), "%d", display_timeout + 1);
    INVOKE_CHK(rdb_set_string(rdb_s, RDB_SYS_LED_OFF_TIMER_VAR, buf),
               "Unable to set led off timer");

 done:
    if (rdb_s) {
        rdb_close(&rdb_s);
    }
    return rval;
}

/*
 * Save the screen saver config. At this time only the lock enable can be
 * user configured so only that value is saved. The other config values
 * are set at build time and are not user modifiable (they may be at some
 * time in the future).
 */
static int
save_config (void)
{
    int rval = 0;
    struct rdb_session *rdb_s = NULL;

    INVOKE_CHK(rdb_open(NULL, &rdb_s), "Error opening RDB session");

    INVOKE_CHK(rdb_set_string(rdb_s, RDB_SCRM_CONF_LOCK_ENABLE_VAR,
                              g_lock_enable ? "1" : "0"),
               "Unable to set rdb var %s", RDB_SCRM_CONF_LOCK_ENABLE_VAR);

 done:
    if (rdb_s) {
        rdb_close(&rdb_s);
    }
    return rval;
}

/*
 * Handler for the OK button press on the screen lock enabe/disable result
 * screen. Just closes and destroys the result screen.
 */
static int
lock_result_ok_handler (void *screen_handle, ngt_widget_t *widget, void *arg)
{
    UNUSED(widget);
    UNUSED(arg);

    scrm_screen_close(screen_handle);
    scrm_screen_destroy(&screen_handle);

    return 0;
}

/*
 * Set the text for the screen lock top level menu option. That menu option
 * toggles the screen lock on or off so the text changes depending on the
 * screen lock enable state.
 */
static void
set_lock_menu_text (void)
{
    const char *enable_str;
    char buf[SCRM_SCREEN_SAVER_BUF_SIZE];

    assert(g_screen_saver_menu_id >= 0);

    /* Menu item toggles so string is opposite to current enable value. */
    if (g_lock_enable) {
        enable_str = _("Disable");
    } else {
        enable_str = _("Enable");
    }

    snprintf(buf, sizeof(buf), SCREEN_LOCK_MENU_FMT_STR, enable_str);

    scrm_set_top_menu_item_text(g_screen_saver_menu_id, buf);
}

/*
 * Screen lock enable/disable menu option select handler.
 */
static int
screen_lock_selected (ngt_widget_t *widget, void *arg)
{
    int rval = 0;
    void *screen_handle;
    char buf[SCRM_SCREEN_SAVER_BUF_SIZE];
    const char *enable_str;
    const char *result_str;
    scrm_status_t status;

    scrm_button_request_t button_req[] = {
        { BTN_1, OK_LABEL, lock_result_ok_handler, NULL }
    };

    UNUSED(arg);
    UNUSED(widget);

    g_lock_enable = !g_lock_enable;
    enable_str =  g_lock_enable ? _("enable") : _("disable");

    /* Create or destroy the lock screen based on the new enable state. */
    if (g_lock_enable) {
        rval = lock_screen_create();
    } else {
        lock_screen_destroy();
    }

    /* Save the update lock enable state to the config. */
    if (!rval) {
        rval = save_config();
        if (rval) {
            /* Error - undo the above operations. */
            g_lock_enable = !g_lock_enable;
            if (!g_lock_enable) {
                lock_screen_destroy();
            }
        }
    }

    if (rval) {
        result_str = _("failed");
        status = SCRM_STATUS_FAIL;
    } else {
        set_lock_menu_text();
        result_str = _("succeeded");
        status = SCRM_STATUS_SUCCESS;
    }

    snprintf(buf, sizeof(buf), _("Screen lock %s %s."), enable_str, result_str);

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
 * Clean up screen saver resources.
 */
static void
screen_saver_destroy (void)
{
    lock_screen_destroy();
}

/*
 * Initialise screen saver feature.
 */
static int
screen_saver_init (void)
{
    int rval;

    rval = load_config();
    if (rval) {
        return rval;
    }

    /* Create a top level menu option to enable/disable the screen lock. */
    g_screen_saver_menu_id =
        scrm_add_top_menu_item("", screen_lock_selected, NULL);

    if (g_screen_saver_menu_id < 0) {
        errp("Unable to add Screen Lock top menu item");
        screen_saver_destroy();

        /* g_screen_saver_menu_id holds an error code in failure case. */
        return g_screen_saver_menu_id;
    } else {
        set_lock_menu_text();
        return 0;
    }
}

scrm_feature_plugin_t scrm_screen_saver_plugin = {
    screen_saver_init,
    screen_saver_destroy,
};
