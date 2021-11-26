/*
 * scrm_main.c
 *    Screen manager core. The scrm core provides the framework and
 *    abstractions that allows integration and plugging in of common,
 *    plugin and feature code.
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
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <linux/input.h>

#include <ngt.h>
#include <scrm.h>
#include <scrm_ops.h>

#define MAIN_MENU_HEADER _("Main Menu")
#define MENU_BUTTON_LABEL _("Menu")

/*
 * List of plugins for features known to Screen Manager. Each entry in this
 * list has two definitions.
 *    1. A WEAK definition in core/scrm_features.c. This is the default
 *       NULL definition which will be used if not overriden by a (non-weak)
 *       feature definition.
 *    2. A feature definition. These exist in the feature/<V_Variable>
 *       directory and are included if the corresponding V_Variable value
 *       is not "none".
 */
static scrm_feature_plugin_t *feature_plugins[] = {
    &scrm_user_menu_plugin,
    &scrm_screen_saver_plugin,
    &scrm_bluetooth_plugin,
    &scrm_wifi_plugin,
    &scrm_module_plugin,
    &scrm_power_plugin,
    &scrm_speaker_plugin,
    &scrm_led_plugin,
};

static void *top_menu_screen_handle;
ngt_list_t *top_menu;

/*
 * Display the main menu screen.
 */
static int
show_menu (void *screen_handle, ngt_widget_t *widget, void *arg)
{
    void **menu_screen_handle_p = arg;

    UNUSED(screen_handle);
    UNUSED(widget);
    UNUSED(arg);

    if (!menu_screen_handle_p || !(*menu_screen_handle_p)) {
        return -EINVAL;
    }

    return (scrm_screen_show(*menu_screen_handle_p, SCRM_STATUS_NONE));
}

static void
terminate_handler (int sig)
{
    UNUSED(sig);
    ngt_run_quit();
}

/*
 * Screen manager daemon entry point.
 */
int
main (int argc, char **argv)
{
    ngt_image_t *main_image = NULL;
    char image_file_name[SCRM_MAX_NAME_LEN];
    void *home_screen_handle = NULL;
    scrm_button_request_t button_req[] = {
        { BTN_1, MENU_BUTTON_LABEL, show_menu, &top_menu_screen_handle }
    };
    unsigned int ix;
    int rval;
    struct sigaction sa;

    UNUSED(argc);
    UNUSED(argv);

    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = terminate_handler;
    sigaction(SIGTERM, &sa, NULL);

    INVOKE_CHK(scrm_plugin.init(), "Unable to init plugin");

    /* Create the main menu */
    INVOKE_CHK(scrm_menu_screen_create(MAIN_MENU_HEADER,
                                       &top_menu_screen_handle, &top_menu,
                                       NULL, NULL),
               "Unable to create top menu");

    /*
     * Invoke each of the feature plugin init functions. Each feature
     * can insert a menu item into the main menu.
     */
    for (ix = 0; ix < (sizeof(feature_plugins) / sizeof(*feature_plugins));
         ix++) {
        if (feature_plugins[ix]->init) {
            INVOKE_CHK(feature_plugins[ix]->init(), "Feature %d init failed",
                       ix);
        }
    }

    /* Add the "Back" option at the end of the main menu item list */
    rval = scrm_add_top_menu_item(MENU_ITEM_BACK_LABEL,
                                  scrm_menu_screen_close,
                                  top_menu_screen_handle);
    if (rval < 0){
        errp("Unable to add Back menu item");
        goto done;
    }

    /* Create the home screen */
    INVOKE_CHK(scrm_screen_create(SCRM_BAR_FLAG_CREATE | SCRM_BAR_FLAG_FILL,
                                  SCRM_BAR_FLAG_CREATE | SCRM_BAR_FLAG_FILL,
                                  button_req,
                                  sizeof(button_req) /
                                  sizeof(scrm_button_request_t),
                                  &home_screen_handle),
               "Unable to create home screen");

    // TODO: get logo file name from rdb
    sprintf(image_file_name, "%s", SCRM_HOME_IMAGE_DEFAULT);
    main_image = ngt_image_new(image_file_name);
    if (!main_image) {
        errp("Unable to create logo image\n");
        goto done;
    }

    INVOKE_CHK(ngt_widget_set_horizontal_alignment(NGT_WIDGET(main_image),
                                                   NGT_ALIGN_CENTER),
               "Unable to set horizontal alignment for main image");

    INVOKE_CHK(ngt_widget_set_vertical_alignment(NGT_WIDGET(main_image),
                                                 NGT_ALIGN_CENTER),
               "Unable to set vertical alignment for main image");

    INVOKE_CHK(scrm_screen_set_main_content(home_screen_handle,
                                            NGT_WIDGET(main_image)),
               "Failed to set main content");

    INVOKE_CHK(scrm_screen_show(home_screen_handle, SCRM_STATUS_NONE),
               "Failed to show home screen");

    /*
     * Make sure the display is on. Should already be on but just in case
     * something left it in the off state.
     */
    ngt_display_mode_set(NGT_DISPLAY_MODE_ON);

    ngt_run();

 done:
    /* Clean up everything */

    ngt_display_mode_set(NGT_DISPLAY_MODE_OFF);

    if (top_menu_screen_handle) {
        scrm_screen_destroy(&top_menu_screen_handle);
    }

    if (main_image) {
        /*
         * Dipose the main image explicitly and not rely on scrm_screen_destroy
         * in case there was an error after the image was created but before
         * it was added to the screen.
         */
        ngt_widget_dispose(NGT_WIDGET(main_image));
    }

    if (home_screen_handle) {
        scrm_screen_set_main_content(home_screen_handle, NULL);
        scrm_screen_destroy(&home_screen_handle);
    }

    for (ix = 0; ix < (sizeof(feature_plugins) / sizeof(*feature_plugins));
         ix++) {
        if (feature_plugins[ix]->destroy) {
            feature_plugins[ix]->destroy();
        }
    }
    scrm_plugin.destroy();

    return 0;
}
