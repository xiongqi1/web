/*
 * scrm.h
 *    Screen Manager core defines.
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
#ifndef __SCRM_H__
#define __SCRM_H__

#include <stdio.h>
#include <syslog.h>
#include <ngt.h>
#include <scrm_ops.h>
#include <scrm_features.h>

#ifdef DEBUG
#define DAEMON_LOG_LEVEL LOG_DEBUG
#define dbgp(x, ...) printf("%s:%d: " x, __func__,__LINE__, ##__VA_ARGS__)
#define errp(x, ...) printf("%s:%d: " x "\n", __func__,__LINE__, ##__VA_ARGS__)
#else
#define DAEMON_LOG_LEVEL LOG_INFO
#define errp(x, ...) syslog(LOG_ERR, x, ##__VA_ARGS__)
#define dbgp(x, ...)
#endif

#define INVOKE_CHK(invocation, err_str_fmt, ...) do {           \
        rval = (invocation);                                    \
        if (rval) {                                             \
            if (err_str_fmt) {                                  \
                errp(err_str_fmt, ##__VA_ARGS__);               \
            }                                                   \
            goto done;                                          \
        }                                                       \
    } while (0);

#define UNUSED(x) (void)(x)

/*
 * Id values for the different areas on a standard screen.
 */
typedef enum scrm_area_id_ {
    SCRM_AREA_STATUS_BAR = 0,
    SCRM_AREA_MAIN_CONTENT,
    SCRM_AREA_CONTROL_BAR,

    /* Terminating ID. Any new ids must be added before this. */
    SCRM_AREA_MAX,
} scrm_area_id_t;

#define MENU_DOWN_LABEL _("Down")
#define MENU_SELECT_LABEL _("Select")
#define MENU_CANCEL_LABEL _("Cancel")
#define MENU_CONFIRM_LABEL _("Confirm")
#define MENU_ITEM_BACK_LABEL _("<Back>")
#define OK_LABEL _("OK")
#define SCROLL_LABEL _("Scroll")

#define SCRM_DEFAULT_TEXT_SIZE 13
#define SCRM_DEFAULT_MENU_TEXT_SIZE 13
#define SCRM_DEFAULT_MARGIN 8

#define SCRM_MAX_NAME_LEN 128
#define SCRM_MAX_TEXT_LEN 32
#define SCRM_HOME_IMAGE_DEFAULT "ntclogo_only.png"

#define SCRM_SCREEN_TIMEOUT_SECS_DEFAULT 5

/* Screen manager RDB variables */
#define RDB_SCRM_VAR "screen_manager"
#define RDB_SCRM_CONF_VAR RDB_SCRM_VAR".conf"
#define RDB_SCRM_CONF_DISPLAY_TIMEOUT_VAR RDB_SCRM_CONF_VAR".display_timeout"
#define RDB_SCRM_CONF_LOCK_ENABLE_VAR RDB_SCRM_CONF_VAR".lock_enable"
#define RDB_SCRM_CONF_UNLOCK_CODE_VAR RDB_SCRM_CONF_VAR".unlock_code"
#define RDB_SCRM_CONF_LED_ENABLE_VAR RDB_SCRM_CONF_VAR".led_enable"
#define RDB_SCRM_CONF_SPKR_ENABLE_VAR RDB_SCRM_CONF_VAR".speaker_enable"
#define RDB_SCRM_CONF_SOUND_VAR RDB_SCRM_CONF_VAR".sound"
#define RDB_SCRM_CONF_SOUND_SUCCESS_VAR RDB_SCRM_CONF_SOUND_VAR".success"
#define RDB_SCRM_CONF_SOUND_FAIL_VAR RDB_SCRM_CONF_SOUND_VAR".fail"
#define RDB_SCRM_CONF_SOUND_ALERT_VAR RDB_SCRM_CONF_SOUND_VAR".alert"

/* Led rdb variables */
#define RDB_SYS_VAR "system"
#define RDB_SYS_LED_OFF_TIMER_VAR RDB_SYS_VAR".led_off_timer"
#define RDB_SYS_RESET_LED_OFF_TIMER_VAR RDB_SYS_VAR".reset_led_off_timer"

/*
 * There must be EXACTLY ONE scrm plugin defined for the entire
 * Screen Manager. The plugin must implement all the below vectors.
 */
typedef struct scrm_plugin_ {
    /*
     * Plugin specific Screen Manager initialisation. Called once
     * during Screen Manager start up.
     *
     * Parameters:
     *    None.
     *
     * Returns:
     *    0 on success. Negative error code otherwise.
     */
    int (*init)(void);

    /*
     * Plugin specific Screen Manager clean up. Called once
     * during Screen Manager shut down.
     *
     * Parameters:
     *    None.
     *
     * Returns:
     *    None.
     */
    void (*destroy)(void);

    /*
     * Create a screen. Each screen has up to three components:
     *   1. A status bar - Optional.
     *      The status bar contains status widgets set by the plugin.
     *      The entire status bar can be left out but when present, the set of
     *      status widgets is fixed by the plugin.
     *   2. A main content area - Mandatory.
     *      Space is reserved for this area on the screen but is left
     *      empty to be filled by the caller.
     *   3. A control bar - Optional.
     *      The control bar contains a set of buttons and/or labels.
     *      The entire control bar can be left out. If present, the set of
     *      buttons and/or labels is determined by the caller. The position
     *      of the buttons/labels are set by the plugin (e.g. to match the
     *      position of hardware buttons).
      *
     * Parameters:
     *    status_bar_flags    Bitwise flags. If SCRM_BAR_FLAG_CREATE is set then
     *                        the status bar will be created. In addition, if
     *                        SCRM_BAR_FLAG_FILL is also set, the status bar
     *                        will be filled with the standard status widgets
     *                        provided by the plugin (otherwise
     *                        the status bar space is reserved but left empty).
     *    control_bar_flags   Bitwise flags. If SCRM_BAR_FLAG_CREATE is set then
     *                        the control bar will be created. In addition, if
     *                        SCRM_BAR_FLAG_FILL is also set, the control bar
     *                        will be filled buttons/labels as specified by the
     *                        button_req and num_button_req parameters
     *                        (otherwise the control bar space is reserved but
     *                        left empty).
     *    button_req    An array of button requests. See the comment for
     *                  scrm_button_request_t for details of the request fields.
     *    num_button_req    Number of entries in the button_req array.
     *    screen_handle    On success this is set to an opaque screen handle to
     *                     be used in other scrm plugin APIs. NULL on error.
     *
     * Returns:
     *    0 on success. Negative error code otherwise.
     */
    int (*screen_create)(int control_bar_flags,
                         int status_bar_flags,
                         scrm_button_request_t *button_req,
                         unsigned int num_button_req,
                         void **screen_handle);

    /*
     * Destroy a previously created screen. All resources are freed.
     *
     * Parameters:
     *    screen_handle    A pointer to the handle of a previously created
     *                     screen. The pointed to handle will be NULLed out if
     *                     the screen is successfully destroyed.
     *
     * Returns:
     *    None.
     */
    void (*screen_destroy)(void **screen_handle);

    /*
     * Set user data to be associated with a screen.
     *
     * Parameters:
     *    screen_handle    A pointer to the handle of a previously created
     *                     screen.
     *    user_data    The user data.
     *
     * Returns:
     *    None.
     */
    int (*screen_set_user_data)(void *screen_handle, void *user_data);

    /*
     * Get user data associated with a screen.
     *
     * Parameters:
     *    screen_handle    A pointer to the handle of a previously created
     *                     screen.
     *    user_data_p    A pointer for storing the user data.
     *
     * Returns:
     *    None.
     */
    int (*screen_get_user_data)(void *screen_handle, void **user_data_p);

    /*
     * Set the main content area of a screen with a given widget
     * (which can be a layout with other widgets contained in it). If
     * there is already a different widget set in the main content
     * area then the current widget will replace it.  The previous
     * widget is not freed.
     *
     * Parameters:
     *    screen_handle    Handle to the screen.
     *    widget    The widget to set. Can be a layout.
     *
     * Returns:
     *    0 on success. Negative error code otherwise.
     */
    int (*screen_set_main_content)(void *screen_handle, ngt_widget_t *widget);

    /*
     * Get the ngt_screen widget corresponding to a given screen handle.
     * This allows the caller to perform NGT operations directly on the screen
     * widget.
     *
     * Parameters:
     *    screen_handle    Handle to the screen.
     *
     * Returns:
     *    The ngt_screen widget handle or NULL on error.
     */
    ngt_screen_t *(*screen_widget_get)(void *screen_handle);
} scrm_plugin_t;

/* The definition for this is provided by the plugin code */
extern scrm_plugin_t scrm_plugin;

/*
 * Core scrm features. These features are always built into scrm.
 */
extern scrm_feature_plugin_t scrm_screen_saver_plugin;
extern scrm_feature_plugin_t scrm_user_menu_plugin;

#endif /* __SCRM_H__ */
