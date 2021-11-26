/*
 * scrm_ops.c
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <linux/input.h>

#include <ngt.h>
#include <scrm.h>
#include <scrm_ops.h>
#include <scrm_ops_priv.h>

/*
 * Handler that is invoked when a screen times out.
 */
static int
screen_timeout_handler (void *screen_handle)
{
    scrm_screen_t *scrm_screen = NULL;
    int rval = NGT_STOP;

    if (!screen_handle) {
        return NGT_STOP;
    }

    scrm_plugin.screen_get_user_data(screen_handle,
                                     (void **)&scrm_screen);

    assert(scrm_screen);
    if (scrm_screen) {
        if (scrm_screen->timeout_cb) {
            /* Timeout callback present. Invoke the callback. */
            rval = scrm_screen->timeout_cb(scrm_screen->screen_handle,
                                           scrm_screen->timeout_cb_arg);
        } else {
            /* No callback. Do default timeout behaviour. */
            scrm_screen_close(screen_handle);
            scrm_screen_destroy(&screen_handle);
        }
    }

    return rval;
}

/*
 * Internal worker function for creating a screen. Performs common work
 * required to create a screen. Specific screen behaviour may be implemented
 * in each of the screen type specific create APIs.
 */
static int
scrm_screen_create_internal (scrm_screen_type_t type,
                             int status_bar_flags,
                             int control_bar_flags,
                             scrm_button_request_t *button_req,
                             unsigned int num_button_req,
                             void **screen_handle,
                             scrm_screen_t **scrm_screen)
{
    int rval = 0;

    if (!screen_handle || !scrm_screen) {
        return -EINVAL;
    }

    *screen_handle = NULL;
    *scrm_screen = calloc(1, sizeof(scrm_screen_t));

    (*scrm_screen)->type = type;
    INVOKE_CHK(scrm_plugin.screen_create(status_bar_flags,
                                         control_bar_flags,
                                         button_req, num_button_req,
                                         screen_handle),
               "Unable to create screen");

 done:
    if (rval) {
        if (*scrm_screen) {
            free(*scrm_screen);
            *scrm_screen = NULL;
        }
        scrm_screen_destroy(screen_handle);
    } else {
        (*scrm_screen)->screen_handle = *screen_handle;
        scrm_plugin.screen_set_user_data(*screen_handle, *scrm_screen);
    }

    return rval;
}

/*
 * Processing screen specific clean up.
 */
static int
scrm_processing_screen_destroy (scrm_screen_t *scrm_screen)
{
    scrm_processing_screen_state_t *state;

    if (!scrm_screen) {
        return -EINVAL;
    }

    state = &(scrm_screen->state.processing);
    if (!state) {
        return -EINVAL;
    }

    if (state->heartbeat_timer) {
        ngt_run_loop_delete_event(state->heartbeat_timer);
    }

    return 0;
}

/*
 * See scrm_ops.h
 */
int
scrm_screen_create (int status_bar_flags,
                    int control_bar_flags,
                    scrm_button_request_t *button_req,
                    unsigned int num_button_req,
                    void **screen_handle)
{
    scrm_screen_t *scrm_screen;
    int rval;

    rval = scrm_screen_create_internal(SCRM_SCREEN_BASE, status_bar_flags,
                                       control_bar_flags,
                                       button_req, num_button_req,
                                       screen_handle, &scrm_screen);

    return rval;
}

/*
 * See scrm_ops.h
 */
void
scrm_screen_destroy (void **screen_handle_p)
{
    scrm_screen_t *scrm_screen = NULL;

    if (!screen_handle_p || !(*screen_handle_p)) {
        return;
    }

    scrm_plugin.screen_get_user_data(*screen_handle_p,
                                     (void **)&scrm_screen);

    if (!scrm_screen) {
        assert(0);
        return;
    }

    /* Some screen types have specific clean ups that need to be done */
    switch (scrm_screen->type) {
    case SCRM_SCREEN_PROCESSING:
        scrm_processing_screen_destroy(scrm_screen);
        break;
    default:
        /* All other screen types just need common clean up code */
        break;
    }

    if (scrm_screen->timeout_timer) {
        ngt_run_loop_delete_event(scrm_screen->timeout_timer);
    }

    scrm_plugin.screen_destroy(screen_handle_p);
    free(scrm_screen);
}

/*
 * See scrm_ops.h
 */
ngt_screen_t *
scrm_screen_widget_get (void *screen_handle)
{
    return (scrm_plugin.screen_widget_get(screen_handle));
}

/*
 * See scrm_ops.h
 */
int
scrm_screen_show_with_timeout (void *screen_handle, scrm_status_t status,
                               int timeout_secs,
                               scrm_screen_callback_t timeout_cb,
                               void *timeout_cb_arg)
{
    scrm_screen_t *scrm_screen = NULL;
    ngt_screen_t *screen;

    if (!screen_handle) {
        return -EINVAL;
    }

    screen = scrm_screen_widget_get(screen_handle);
    if (!screen) {
        return -EINVAL;
    }

    scrm_plugin.screen_get_user_data(screen_handle,
                                     (void **)&scrm_screen);
    if (!scrm_screen) {
        assert(0);
        return -EINVAL;
    }

    /*
     * Handle corner case where screen was previously shown with a timeout
     * and is now being shown again before that timeout expired.
     * Just stop the the previoulsy started timeout timer. It will be restarted
     * again below if required.
     */
    if (scrm_screen->timeout_timer) {
        ngt_run_loop_delete_event(scrm_screen->timeout_timer);
        scrm_screen->timeout_timer = NULL;
    }

    /* If screen timeout requested then create the timer */
    if (timeout_secs != 0) {

        if (timeout_secs < 0) {
            timeout_secs = SCRM_SCREEN_TIMEOUT_SECS_DEFAULT;
        }

        scrm_screen->timeout_timer =
            ngt_run_loop_add_timer_event(timeout_secs * 1000,
                                         screen_timeout_handler,
                                         screen_handle);

        if (!scrm_screen->timeout_timer) {
            return -EINVAL;
        }

        scrm_screen->timeout_cb = timeout_cb;
        scrm_screen->timeout_cb_arg = timeout_cb_arg;
    }

    scrm_led_show_status(status);
    scrm_speaker_play_status(status);

    return (ngt_widget_show(NGT_WIDGET(screen)));
}

/*
 * See scrm_ops.h
 */
int
scrm_screen_show (void *screen_handle, scrm_status_t status)
{
    return (scrm_screen_show_with_timeout(screen_handle, status, 0, NULL,
                                          NULL));
}

/*
 * See scrm_ops.h
 */
int
scrm_screen_close (void *screen_handle)
{
    ngt_screen_t *screen;
    scrm_screen_t *scrm_screen = NULL;

    if (!screen_handle) {
        return -EINVAL;
    }

    screen = scrm_screen_widget_get(screen_handle);
    if (!screen) {
        return -EINVAL;
    }

    scrm_plugin.screen_get_user_data(screen_handle, (void **)&scrm_screen);
    if (!scrm_screen) {
        assert(0);
        return -EINVAL;
    }

    if (scrm_screen->timeout_timer) {
        ngt_run_loop_delete_event(scrm_screen->timeout_timer);
        scrm_screen->timeout_timer = NULL;
    }

    scrm_led_show_status(SCRM_STATUS_NONE);

    return (ngt_screen_close(screen));
}

/*
 * See scrm_ops.h
 */
int
scrm_screen_set_main_content (void *screen_handle, ngt_widget_t *widget)
{
    return (scrm_plugin.screen_set_main_content(screen_handle,
                                                         widget));
}

/*
 * See scrm_ops.h
 */
int
scrm_menu_screen_create (const char *menu_header,
                         void **menu_screen_handle,
                         ngt_list_t **menu_widget,
                         const char *btn0_label,
                         const char *btn1_label)
{
    int rval = 0;
    void *screen_handle = NULL;
    ngt_list_t *menu = NULL;
    char header[SCRM_MAX_TEXT_LEN];
    const char *label0;
    const char *label1;
    scrm_screen_t *scrm_screen = NULL;

    label0 = btn0_label ? btn0_label : MENU_DOWN_LABEL;
    label1 = btn1_label ? btn1_label : MENU_SELECT_LABEL;

    scrm_button_request_t button_req[] = {
        { BTN_0, label0, NULL, NULL },
        { BTN_1, label1, NULL, NULL }
    };

    if (!menu_screen_handle || !menu_widget) {
        return -EINVAL;
    }

    /* Create the screen */
    INVOKE_CHK(scrm_screen_create_internal(SCRM_SCREEN_MENU,
                                           SCRM_BAR_FLAG_NONE,
                                           SCRM_BAR_FLAG_CREATE |
                                           SCRM_BAR_FLAG_FILL,
                                           button_req,
                                           sizeof(button_req) /
                                           sizeof(scrm_button_request_t),
                                           &screen_handle,
                                           &scrm_screen),
               "Unable to create screen");

    /* Create a list for the menu and set it as the screen main content */
    snprintf(header, sizeof(header), "[%s]", menu_header);

    menu = ngt_list_new(header);
    if (!menu) {
        errp("Error creating menu\n");
        rval = -1;
        goto done;
    }

    /* Use default NGT font but with a different font size */
    INVOKE_CHK(ngt_widget_set_font(NGT_WIDGET(menu), NULL,
                                   SCRM_DEFAULT_MENU_TEXT_SIZE),
               "Unable to set menu font");

    INVOKE_CHK(scrm_screen_set_main_content(screen_handle, NGT_WIDGET(menu)),
               "Failed to set main content");

 done:
    if (rval) {
        if (menu) {
            ngt_widget_dispose(NGT_WIDGET(menu));
            menu = NULL;
        }

        if (screen_handle) {
            scrm_screen_set_main_content(screen_handle, NULL);
            scrm_screen_destroy(&screen_handle);
            screen_handle = NULL;
        }
    }

    *menu_screen_handle = screen_handle;
    *menu_widget = menu;
    return rval;
}

/*
 * See scrm_ops.h
 */
int
scrm_menu_screen_close (ngt_widget_t *widget, void *arg)
{
    ngt_list_t *menu = NGT_LIST(widget);
    void *screen_handle = arg;

    assert(widget && menu);

    if (!screen_handle) {
        return -EINVAL;
    }

    /*
     * Reset the menu selection so that it starts from the top the next time it
     * is opened.
     */
    ngt_list_set_selected_index(menu, 0);

    return (scrm_screen_close(screen_handle));
}

/*
 * See scrm_ops.h
 */
int
scrm_message_screen_create (const char *msg_text,
                            scrm_button_request_t *button_req,
                            unsigned int num_button_req,
                            void **msg_screen_handle)
{
    int rval = 0;
    ngt_label_t *msg_widget = NULL;
    scrm_screen_t *scrm_screen = NULL;

    if (!msg_screen_handle) {
        return -EINVAL;
    }

    *msg_screen_handle = NULL;

    INVOKE_CHK(scrm_screen_create_internal(SCRM_SCREEN_MESSAGE,
                                           SCRM_BAR_FLAG_NONE,
                                           SCRM_BAR_FLAG_CREATE |
                                           SCRM_BAR_FLAG_FILL,
                                           button_req, num_button_req,
                                           msg_screen_handle, &scrm_screen),
               "Unable to create screen");

    msg_widget = ngt_label_new(msg_text);
    if (!msg_widget) {
        rval = -1;
        goto done;
    }

    /* Use default NGT font but with a different font size */
    INVOKE_CHK(ngt_widget_set_font(NGT_WIDGET(msg_widget), NULL,
                                   SCRM_DEFAULT_TEXT_SIZE),
               "Unable to set message font");

    /* Center the message */
    INVOKE_CHK(ngt_widget_set_horizontal_alignment(NGT_WIDGET(msg_widget),
                                                   NGT_ALIGN_CENTER),
               "Unable to set horizontal alignment for message screen");

    INVOKE_CHK(ngt_widget_set_vertical_alignment(NGT_WIDGET(msg_widget),
                                                 NGT_ALIGN_CENTER),
               "Unable to set vertical alignment for message screen");

    INVOKE_CHK(scrm_screen_set_main_content(*msg_screen_handle,
                                            NGT_WIDGET(msg_widget)),
               "Failed to set main content");

 done:
    if (rval) {
        if (msg_widget) {
            ngt_widget_dispose(NGT_WIDGET(msg_widget));
        }
        if (*msg_screen_handle) {
            scrm_screen_set_main_content(*msg_screen_handle, NULL);
            scrm_screen_destroy(msg_screen_handle);
        }
    }

    return rval;
}

/*
 * Called for each tick of a processing screen's heartbeat.
 */
static int
heartbeat_handler (void *arg)
{
    scrm_screen_t *scrm_screen = arg;
    scrm_processing_screen_state_t *state;
    int num_heartbeat_chars;

    if (!scrm_screen) {
        assert(0);
        return NGT_STOP;
    }

    state = &(scrm_screen->state.processing);

    num_heartbeat_chars =
        (state->counter % (sizeof(state->heartbeat_text) - 1)) + 1;

    /* Set the indicator characters */
    memset(state->heartbeat_text, SCRM_HEARTBEAT_CHAR, num_heartbeat_chars);

    /*
     * Set the remaining characters as blank. This is to ensure
     * the label is always the same width so that it remains
     * displayed at the same centered location.
     */
    memset(&(state->heartbeat_text[num_heartbeat_chars]), ' ',
           sizeof(state->heartbeat_text) - 1 - num_heartbeat_chars);

    state->heartbeat_text[sizeof(state->heartbeat_text) - 1] = '\0';

    ngt_label_set_text(state->heartbeat_label, state->heartbeat_text);
    state->counter++;

    return NGT_CONTINUE;
}

/*
 * See scrm_ops.h
 */
int
scrm_processing_screen_create (const char *msg_text,
                               scrm_button_request_t *button_req,
                               unsigned int num_button_req,
                               void **screen_handle)
{
    int rval = 0;
    ngt_linear_layout_t *linear_layout;
    ngt_label_t *msg_widget = NULL;
    scrm_screen_t *scrm_screen = NULL;
    scrm_processing_screen_state_t *state = NULL;

    if (!screen_handle) {
        return -EINVAL;
    }

    *screen_handle = NULL;
    INVOKE_CHK(scrm_screen_create_internal(SCRM_SCREEN_PROCESSING,
                                           SCRM_BAR_FLAG_NONE,
                                           SCRM_BAR_FLAG_CREATE |
                                           SCRM_BAR_FLAG_FILL,
                                           button_req, num_button_req,
                                           screen_handle, &scrm_screen),
               "Unable to create screen");

    state = &(scrm_screen->state.processing);
    linear_layout = ngt_linear_layout_new(NGT_VERTICAL);

    msg_widget = ngt_label_new(msg_text);
    if (!msg_widget) {
        rval = -1;
        goto done;
    }

    state->heartbeat_label = ngt_label_new("");
    if (!state->heartbeat_label) {
        rval = -1;
        goto done;
    }

    /* Use default NGT font but with a different font size */
    INVOKE_CHK(ngt_widget_set_font(NGT_WIDGET(msg_widget), NULL,
                                   SCRM_DEFAULT_TEXT_SIZE),
               "Unable to set message font");

    /* Center the message */
    INVOKE_CHK(ngt_widget_set_horizontal_alignment(NGT_WIDGET(msg_widget),
                                                   NGT_ALIGN_CENTER),
               "Unable to set horizontal alignment for message");

    /* Use default NGT font but with a different font size */
    INVOKE_CHK(ngt_widget_set_font(NGT_WIDGET(state->heartbeat_label), NULL,
                                   SCRM_DEFAULT_TEXT_SIZE),
               "Unable to set message font");

    /* Center the heartbeat label */
    INVOKE_CHK(ngt_widget_set_horizontal_alignment
               (NGT_WIDGET(state->heartbeat_label), NGT_ALIGN_CENTER),
               "Unable to set horizontal alignment for heartbeat label");

    /* Add the msg and hearbeat label stacked vertically */
    INVOKE_CHK(ngt_linear_layout_add_widget(linear_layout,
                                            NGT_WIDGET(msg_widget)),
               "Unable to add msg widget");
    INVOKE_CHK(ngt_linear_layout_add_widget(linear_layout,
                                            NGT_WIDGET(state->heartbeat_label)),
               "Unable to add heartbeat label widget");

    INVOKE_CHK(ngt_widget_set_horizontal_alignment
               (NGT_WIDGET(linear_layout), NGT_ALIGN_CENTER),
               "Unable to set horizontal alignment for");

    INVOKE_CHK(ngt_widget_set_vertical_alignment
               (NGT_WIDGET(linear_layout), NGT_ALIGN_CENTER),
               "Unable to set horizontal alignment");


    INVOKE_CHK(scrm_screen_set_main_content(*screen_handle,
                                            NGT_WIDGET(linear_layout)),
               "Failed to set main content");

    /* Kick of a periodic timer to update the heartbeat label */
    state->counter = 0;
    state->heartbeat_timer =
        ngt_run_loop_add_timer_event(SCRM_HEARTBEAT_TICK_MSEC,
                                     heartbeat_handler, scrm_screen);

 done:
    if (rval) {
        if (scrm_screen) {
            state = &(scrm_screen->state.processing);
            if (state->heartbeat_timer) {
                ngt_run_loop_delete_event(state->heartbeat_timer);
            }
            if (state->heartbeat_label) {
                ngt_widget_dispose(NGT_WIDGET(state->heartbeat_label));
            }
            if (msg_widget) {
                ngt_widget_dispose(NGT_WIDGET(msg_widget));
            }
            if (*screen_handle) {
                scrm_screen_set_main_content(*screen_handle, NULL);
                scrm_screen_destroy(screen_handle);
            }
        }
    }

    return rval;
}

/*
 * Updates the header text for a scroll screen. If the header_fixed_text arg
 * is non-NULL then that string is first added to the header label followed
 * by the page info. If that arg is NULL then the header_fixed_text is
 * assumed to already be present and only the page info is updated.
 */
static int
update_scroll_screen_header (ngt_layout_t *layout,
                             scrm_screen_t *scrm_screen,
                             const char *header_fixed_text)
{
    scrm_scroll_screen_state_t *state;
    unsigned int num_pages;
    unsigned int current_page;
    int remaining = 0;
    char *header_p = NULL;
    int len;
    int rval= 0;

    if (!scrm_screen || !scrm_screen->state.scroll.header_label) {
        assert(0);
        return -EINVAL;
    }
    state = &(scrm_screen->state.scroll);

    if (header_fixed_text) {
        len = snprintf(state->header_text, sizeof(state->header_text), "[%s ",
                       header_fixed_text);
        if (len < 0) {
            return -errno;
        } else if (len < (int)sizeof(state->header_text)) {
            header_p = state->header_text + len;
            remaining = sizeof(state->header_text) - len;
        }
    } else {
        /*
         * header_text has this format: [$fixed_text $current_page/$num_pages]
         * Find the last white space which is the character before
         * $current_page.
         */
        header_p = rindex(state->header_text, ' ');
        if (header_p) {
            remaining = sizeof(state->header_text) -
                (header_p - state->header_text);
        }
    }

    /*
     * header_p now points to the white space character before where
     * $current_page should go. Update the page number info.
     */
    if (header_p) {
        INVOKE_CHK(ngt_linear_layout_get_page_info(NGT_LINEAR_LAYOUT(layout),
                                                   &num_pages, &current_page),
                   "Unable to get scroll page info");

        snprintf(header_p, remaining, " %d/%d]", current_page + 1, num_pages);
    }

    INVOKE_CHK(ngt_label_set_text(state->header_label, state->header_text),
               "Unable to set scroll page header label");

 done:
    return rval;
}

/*
 * Invoked to handle a scrolling event for a scroll screen. Main purpose
 * is to update the current page number in the scroll screen header line.
 */
static void
on_scroll_callback (ngt_layout_t *layout,
                    ngt_scroll_t scroll, void *arg)
{
    scrm_screen_t *scrm_screen = arg;

    UNUSED(scroll);

    update_scroll_screen_header(layout, scrm_screen, NULL);
}

/*
 * See scrm_ops.h
 */
int
scrm_scroll_screen_create (const char *header_text,
                           ngt_widget_t *widgets[],
                           scrm_button_request_t *button_req,
                           unsigned int num_button_req,
                           void **screen_handle)
{
    int rval = 0;
    scrm_scroll_screen_state_t *state = NULL;
    scrm_screen_t *scrm_screen = NULL;
    ngt_linear_layout_t *linear_layout_scrolling;
    ngt_linear_layout_t *linear_layout_container;
    int ix;

    /*
     * These "dispose" variables are used to track which widgets need
     * to be disposed on error clean up. Non-NULL means the widget has been
     * created but not added to a layout so needs to be disposed directly.
     * If NULL means either not created so no need to dispose or created
     * and added to a layout in which case the layout dispose will auto-dispose
     * the child widgets.
     */
    ngt_linear_layout_t *linear_layout_scrolling_dispose = NULL;
    ngt_linear_layout_t *linear_layout_container_dispose = NULL;
    ngt_label_t *header_label_dispose = NULL;
    int widget_dispose_ix = 0;

    if (!screen_handle) {
        return -EINVAL;
    }

    *screen_handle = NULL;

    INVOKE_CHK(scrm_screen_create_internal(SCRM_SCREEN_MESSAGE,
                                           SCRM_BAR_FLAG_NONE,
                                           SCRM_BAR_FLAG_CREATE |
                                           SCRM_BAR_FLAG_FILL,
                                           button_req, num_button_req,
                                           screen_handle, &scrm_screen),
               "Unable to create screen");

    state = &(scrm_screen->state.scroll);

    /*
     * Create top level linear layout container for the screen.
     * Will contain the header line followed vertically by the scrolling
     * layout.
     */
    linear_layout_container = ngt_linear_layout_new(NGT_VERTICAL);
    if (!linear_layout_container) {
        return -1;
    }
    linear_layout_container_dispose = linear_layout_container;

    /* Create the scrolling layout. */
    linear_layout_scrolling = ngt_linear_layout_new(NGT_VERTICAL);
    if (!linear_layout_scrolling) {
        rval = -1;
        goto done;
    }
    INVOKE_CHK(ngt_layout_set_scroll(NGT_LAYOUT(linear_layout_scrolling),
                                     NGT_SCROLL_VERTICAL, on_scroll_callback,
                                     scrm_screen),
               "Unable to set scroll");

    /* Create the header. */
    state->header_label = ngt_label_new("");
    if (!state->header_label) {
        rval = -1;
        goto done;
    }
    header_label_dispose = state->header_label;
    INVOKE_CHK(ngt_widget_set_font(NGT_WIDGET(state->header_label), NULL,
                                   SCRM_DEFAULT_TEXT_SIZE),
               "Unable to set header font");
    INVOKE_CHK(ngt_linear_layout_add_widget(linear_layout_container,
                                            NGT_WIDGET(state->header_label)),
               "Unable to add header label widget");
    header_label_dispose = NULL;

    /* Add the scrolling layout to the container layout. */
    INVOKE_CHK(ngt_linear_layout_add_widget
               (linear_layout_container, NGT_WIDGET(linear_layout_scrolling)),
               "Unable to add inner layout");
    linear_layout_scrolling_dispose = NULL;

    /* Add all the caller provided widgets into the scrolling layout. */
    for (ix = 0; widgets[ix] != NULL; ix++) {
        INVOKE_CHK(ngt_linear_layout_add_widget(linear_layout_scrolling,
                                                widgets[ix]),
                   "Unable to add widget to linear layout");

        /*
         * This records the start index of widgets which have not been added
         * to the layout and hence need direct dispose on error.
         */
        widget_dispose_ix++;
    }

    INVOKE_CHK(scrm_screen_set_main_content
               (*screen_handle, NGT_WIDGET(linear_layout_container)),
               "Failed to set main content");
    linear_layout_container_dispose = NULL;

    /*
     * Update the scroll screen header. This needs to be done after all
     * the layout and widgets have been set so that the number of pages
     * in the scroll layout can be calculated. Also, need to first trigger
     * a screen layout as the scroll layout number of pages is only caculated
     * when a layout occurs (which has not occured yet as it has just been
     * created).
     */
    INVOKE_CHK(ngt_screen_do_layout(scrm_screen_widget_get(*screen_handle)),
               "Unable to do screen layout");
    update_scroll_screen_header(NGT_LAYOUT(linear_layout_scrolling),
                                scrm_screen, header_text);

 done:
    if (rval) {
        while (widgets[widget_dispose_ix]) {
            ngt_widget_dispose(widgets[widget_dispose_ix++]);
        }
        if (linear_layout_container_dispose) {
            ngt_widget_dispose(NGT_WIDGET(linear_layout_container_dispose));
        }
        if (linear_layout_scrolling_dispose) {
            ngt_widget_dispose(NGT_WIDGET(linear_layout_scrolling_dispose));
        }
        if (header_label_dispose) {
            ngt_widget_dispose(NGT_WIDGET(header_label_dispose));
        }
        if (*screen_handle) {
            scrm_screen_destroy(screen_handle);
        }
    }

    return rval;
}

/*
 * See scrm_ops.h
 */
int
scrm_add_top_menu_item (const char *text,
                        ngt_widget_callback_t selected_cb,
                        void *selected_cb_arg)
{
    int rval;
    static int menu_item_id = 0;

    if (!top_menu) {
        assert(0);
        return -EINVAL;
    }

    rval = ngt_list_add_item(top_menu, text, selected_cb, selected_cb_arg);

    if (rval) {
        return rval;
    } else {
        return menu_item_id++;
    }
}

/*
 * See scrm_ops.h
 */
int
scrm_set_top_menu_item_text (int menu_item_id, const char *text)
{
    if (!top_menu) {
        assert(0);
        return -EINVAL;
    }

    return (ngt_list_set_item_text(top_menu, menu_item_id, text));
}

/*
 * See scrm_ops.h
 */
int
scrm_led_show_pattern (const char *pattern)
{
    int rval = -ENOSYS;

    if (scrm_led_vtable) {
        rval = scrm_led_vtable->show_pattern(pattern);
    }

    return rval;
}

/*
 * See scrm_ops.h
 */
int
scrm_led_show_status (scrm_status_t status)
{
    int rval = -ENOSYS;

    if (scrm_led_vtable) {
        rval = scrm_led_vtable->show_status(status);
    }

    return rval;
}

/*
 * See scrm_ops.h
 */
int
scrm_speaker_play_sound (const char *file_name)
{
     int rval = -ENOSYS;

    if (scrm_speaker_vtable) {
        rval = scrm_speaker_vtable->play_sound(file_name);
    }

    return rval;
}

/*
 * See scrm_ops.h
 */
int
scrm_speaker_play_status (scrm_status_t status)
{
    int rval = -ENOSYS;

    if (scrm_speaker_vtable) {
        rval = scrm_speaker_vtable->play_status(status);
    }

    return rval;
}
