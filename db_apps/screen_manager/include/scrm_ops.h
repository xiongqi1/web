/*
 * scrm_ops.h
 *    Screen manager operation APIs. These APIs are provided so that
 *    the core and the features can create and manage screens in a consistent
 *    manner and without having to know the physical details of the
 *    physical screen.
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
#ifndef __SCRM_OPS_H__
#define __SCRM_OPS_H__

#include <ngt.h>

extern ngt_list_t *top_menu;

/* bit flags */
#define SCRM_BAR_FLAG_NONE   0x0
#define SCRM_BAR_FLAG_CREATE 0x1
#define SCRM_BAR_FLAG_FILL   0x2

/* screen manager status values */
typedef enum scrm_status_ {
    SCRM_STATUS_NONE,
    SCRM_STATUS_ALERT,
    SCRM_STATUS_SUCCESS,
    SCRM_STATUS_FAIL,

    /* Keep this last. Add any new status values before this. */
    SCRM_STATUS_MAX
} scrm_status_t;
/*
 * Callback function type. Used for button press callbacks.
 */
typedef int (*scrm_button_callback_t)(void *screen_handle, ngt_widget_t *widget,
                                      void *arg);

/*
 * Button request. Used to specify a button on the control bar
 * and how it is handled.
 */
typedef struct scrm_button_request_ {
    /* A linux/input.h key code that the button will respond to */
    short key_code;

    /* Label for the button */
    const char *text;

    /* Callback invoked when the button is pressed. Can be NULL. */
    scrm_button_callback_t cb;

    /* Argument passed to callback. Can be NULL. */
    void * cb_arg;
} scrm_button_request_t;

/*
 * Function callback type used for screens.
 *
 * Parameters:
 *    screen_handle    Handle of the screen that the callback was registered
 *                     with.
 *    arg    User arguement.
 *
 * Returns:
 *    A return code. The meaning of the return code is dependent on where
 *    the callback is used. See the doc for each API that uses this type for
 *    exact return code semantics.
 */
typedef int (*scrm_screen_callback_t)(void *screen_handle, void *arg);

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
 *                        will be filled with the status widgets (otherwise
 *                        the status bar space is reserved but left empty).
 *    control_bar_flags   Bitwise flags. If SCRM_BAR_FLAG_CREATE is set then
 *                        the control bar will be created. In addition, if
 *                        SCRM_BAR_FLAG_FILL is also set, the control bar
 *                        will be filled buttons/labels as specified by the
 *                        button_req and num_button_req parameters (otherwise
 *                        the control bar space is reserved but left empty).
 *    button_req    An array of button requests. See the comment for
 *                  scrm_button_request_t for details of the request fields.
 *    num_button_req    Number of entries in the button_req array.
 *    screen_handle    On success this is set to an opaque screen handle to
 *                     be used in other scrm_op APIs. NULL on error.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_screen_create(int status_bar_flags,
                              int control_bar_flags,
                              scrm_button_request_t *button_req,
                              unsigned int num_buton_req,
                              void **screen_handle);

/*
 * Show a previously created screen. This screen is shown without any timeout
 * and is thus equivalent to calling scrm_screen_show_with_timeout with a
 * 0 timeout.
 *
 * Parameters:
 *    screen_handle    Screen handle
 *    status    Screen manager status. LEDs will be set to reflect this status.
 *              Set to SCRM_STATUS_NONE to remove any previous status from the
 *              LEDs. The staus will also be removed when the screen is closed.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_screen_show(void *screen_handle, scrm_status_t status);

/*
 * Show a previously created screen with timeout.
 *
 * Parameters:
 *    screen_handle    Screen handle
 *    status    Screen manager status. LEDs will be set to reflect this status.
 *              Set to SCRM_STATUS_NONE to remove any previous status from the
 *              LEDs. The staus will also be removed when the screen is closed.
 *    timeout_secs    The timeout in seconds. If 0 then screen will never time
 *                    out (typically closed by a button handler or other event
 *                    instead). If less than 0 then the default timeout is used.
 *    timeout_cb    An optional callback to be invoked when the screen times
 *                  out. This callback should return NGT_STOP to stop the
 *                  timeout timer or NGT_CONTINUE to rearm the timer. If NULL
 *                  is passed in then the default behaviour of closing and
 *                  destroying the screen will occur at timeout.
 *    timeout_cb_arg    Argument passed to timeout_cb.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_screen_show_with_timeout(void *screen_handle,
                                         scrm_status_t status,
                                         int timeout_secs,
                                         scrm_screen_callback_t timeout_cb,
                                         void *timeout_cb_arg);

/*
 * Close a previously created screen.
 *
 * Parameters:
 *    screen_handle    Screen handle
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_screen_close(void *screen_handle);

/*
 * Destroy a previously created screen. All resources are freed.
 *
 * Parameters:
 *    screen_handle    A pointer to the handle of a previously created screen.
 *                     The pointed to handle will be NULLed out if the screen
 *                     is successfully destroyed.
 *
 * Returns:
 *    None.
 */
extern void scrm_screen_destroy(void **screen_handle_p);

/*
 * Set the main content area of a screen with a given widget (which can be a
 * layout with other widgets contained in it). If there is already a different
 * widget set in the main content area then the current widget will replace it.
 * The previous widget is not freed.
 *
 * Parameters:
 *    screen_handle    Handle to the screen.
 *    widget    The widget to set. Can be a layout.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_screen_set_main_content(void *screen_handle,
                                        ngt_widget_t *widget);

/*
 * Get the ngt_screen widget corresponding to a give screen handle.
 * This allows the caller to perform NGT operations directly on the screen
 * widget.
 *
 * Parameters:
 *    screen_handle    Handle to the screen.
 *
 * Returns:
 *    The ngt_screen widget handle or NULL on error.
 */
extern ngt_screen_t *scrm_screen_widget_get(void *screen_handle);

/*
 * Create a standard menu screen. The menu screen will be laid out as:
 *    - No status bar.
 *    - Menu widget in the main content area. The menu widget will be
 *      created with no menu items. It is returned to the caller for that
 *      purpose.
 *    - Control bar with two buttons - BTN0: Down, BTN1: Select
 *
 * Parameters:
 *    menu_header    Header for the menu. Can be NULL to request no header.
 *    menu_screen_handle    On success this is set to an opaque screen handle
 *                          to be used in other scrm_op APIs. NULL on error.
 *    menu_widget    On success this is set to the newly created menu widget.
 *                   The caller can use this to add menu items. NULL on error.
 *    btn0_label    Text for button 0 label. Can be NULL in which case a default
 *                  menu text is used.
 *    btn1_label    Text for button 1 label. Can be NULL in which case a default
 *                  menu text is used.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_menu_screen_create(const char *menu_header,
                                   void **menu_screen_handle,
                                   ngt_list_t **menu_widget,
                                   const char *btn0_label,
                                   const char *btn1_label);

/*
 * Close a menu screen. This can be used directly as the callback to
 * the menu's "Back/Exit" item or it can be called explicitly. This
 * callback resets the menu's selected item to be the first item (for
 * the next time that the menu is opened) and then closes the menu
 * screen.
 *
 * Parameters:
 *    widget    The menu widget.
 *    arg     Callback argument. Needs to be set to the scrm screen handle for
 *            the menu screen being closed.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_menu_screen_close(ngt_widget_t *widget, void *arg);

/*
 * Create a standard message screen. The message screen will be laid out as:
 *    - No status bar.
 *    - Message text centered in the main content area.
 *    - Control bar with buttons as requested in the button_req paramter.
 *
 * Parameters:
 *    msg_text    The message to display.
 *    button_req    An array of button requests. See the comment for
 *                  scrm_button_request_t for details of the request fields.
 *    num_button_req    Number of entries in the button_req array.
 *    msg_screen_handle    On success this is set to an opaque screen handle
 *                         to be used in other scrm_op APIs. NULL on error.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_message_screen_create(const char *msg_text,
                                      scrm_button_request_t *button_req,
                                      unsigned int num_button_req,
                                      void **msg_screen_handle);

/*
 * Create a standard processing screen. The intended use of this
 * screen is to indicate to the user that an operation is being
 * processed. The screen will be displayed until a given timeout
 * period has passed. The screen can also be created with one or more
 * buttons which can be used to close the screen before the timeout
 * expires (and/or for any other purpose).
 *
 * The processing screen will be laid out as:
 *    - No status bar.
 *    - Message text and hearbeat text centered in the main content area.
 *      The hearbeat text is a periodically updating series of symbols
 *      to give a visual indicator that the system is alive.
 *    - Control bar with buttons as requested in the button_req paramter.
 *
 * Parameters:
 *    msg_text    The message to display.
 *    button_req    An array of button requests. See the comment for
 *                  scrm_button_request_t for details of the request fields.
 *    num_button_req    Number of entries in the button_req array.
 *    screen_handle    On success this is set to an opaque screen handle
 *                     to be used in other scrm_op APIs. NULL on error.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_processing_screen_create(const char *msg_text,
                                         scrm_button_request_t *button_req,
                                         unsigned int num_button_req,
                                         void **screen_handle);

/*
 * Create a screen that supports vertical scrolling. The scroll action is
 * bound to BTN_0. Each scroll moves the screen down one page with wrapping
 * back to the first page from the last page.
 *
 * The processing screen will be laid out as:
 *    - No status bar.
 *    - Header line at the top. Contains a user defined string along
 *      with an indication of the current page and total number of pages.
 *      The header line does not scroll and is shown in the same spot for
 *      every page.
 *    - A list of user defined widgets stacked vertically in the area
 *      between the header line and the control bar. This section scrolls.
 *    - Control bar with buttons as requested in the button_req paramter.
 *
 * Parameters:
 *    header_text    Fixed text to be shown in the header line.
 *    widgets    An array of widgets that are shown in the scrolling portion
 *               of the screen. The array must have an sentinel NULL value
 *               to indicate the end of the widget list. The widgets in list
 *               will be managed and disposed by the API regardless of
 *               whether this call returns success or error (ie, caller should
 *               not dispose any of the the widgets unless an explicit
 *               ngt_widget_ref_inc has been called on the widget).
 *    button_req    An array of button requests. See the comment for
 *                  scrm_button_request_t for details of the request fields.
 *    num_button_req    Number of entries in the button_req array.
 *    screen_handle    On success this is set to an opaque screen handle
 *                     to be used in other scrm_op APIs. NULL on error.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_scroll_screen_create(const char *header_text,
                                     ngt_widget_t *widgets[],
                                     scrm_button_request_t *button_req,
                                     unsigned int num_button_req,
                                     void **screen_handle);

/*
 * Add an item to the top level screen manager menu. This is intended
 * primarily to be used by the feature initialisation code (though would work
 * at other times too). The menu item will be added at the end of the
 * current list of items in the menu.
 *
 * Parameters:
 *    text    The menu item text label.
 *    selected_cb    The callback to invoke when the menu item is selected.
 *    selected_cb_arg    The argument to be passed to the callback. Can be NULL.
 *
 * Returns:
 *    A non-negative menu item id on success. This id is used in other
 *    scrm_op APIs which operate on a menu item.
 *    Negative error code otherwise.
 */
extern int scrm_add_top_menu_item(const char *text,
                                  ngt_widget_callback_t selected_cb,
                                  void *selected_cb_arg);

/*
 * Set the text for a top level menu item. Intended to be used by features
 * which require the top level option text to change dynamically.
 *
 * Parameters:
 *    menu_item_id    An id as returned from scrm_add_top_menu_item.
 *    text    The menu item text label.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_set_top_menu_item_text(int menu_item_id, const char *text);

/*
 * Show a given dispd LED pattern. This overrides any system level LED
 * behaviour until the pattern is unset.
 *
 * Parameters:
 *    pattern    The dispd pattern to show. Pass either an empty string or
 *               NULL to unset the pattern and revert to system level LED
 *               behaviour.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_led_show_pattern (const char *pattern);

/*
 * Indicate a given screen manager status via the LED. This overrides any
 * system level LED behaviour until the status is set to SCRM_STATUS_NONE.
 *
 * Parameters:
 *    status    The screen manager status to show. Set to SCRM_STATUS_NONE
 *              to stop currently shown status and revert to system level LED
 *              behaviour.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_led_show_status (scrm_status_t status);

/*
 * Play a given sound file.
 *
 * Parameters:
 *    file_name    Sound file name to play. Assumed to be a full path
 *                 if file_name begins eith '/'. Otherwise the file_name
 *                 is prepended with the system sound path.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_speaker_play_sound (const char *file_name);

/*
 * Indicate a given screen manager status via the speaker by playing a sound
 * corresponding to the status.
 *
 * Parameters:
 *    status    The screen manager status to indicate.
 *
 * Returns:
 *    0 on success. Negative error code otherwise.
 */
extern int scrm_speaker_play_status (scrm_status_t status);

#define _(String) (String)
#define N_(String) String

#endif /* __SCRM_OPS_H__ */
