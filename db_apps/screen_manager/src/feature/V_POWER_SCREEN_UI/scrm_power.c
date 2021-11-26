/*
 * scrm_power.c
 *    Power Screen UI support.
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

#define SCRM_POWER_MENU_HEADER _("Power")

typedef struct scrm_power_menu_item_ {
    const char *label;
    ngt_widget_callback_t cb;
    void *cb_arg;
} scrm_power_menu_item_t;

#define POWER_STATUS_HEADER_FONT_SIZE 12
#define POWER_STATUS_ITEM_FONT_SIZE 11

static void *power_menu_screen_handle;
static ngt_list_t *power_menu_widget;
static struct rdb_session *rdb_s;

/*
 * Handler for a message button which is only to close the message screen
 */
static int scrm_power_cancel_message_handler (void *screen_handle, ngt_widget_t *widget, void *arg)
{
	UNUSED(widget);
	UNUSED(arg);
	scrm_screen_close(screen_handle);
	scrm_screen_destroy(&screen_handle);
	return 0;
}

/*
 * Handler for acceptance of switching off
 */
static int scrm_power_switching_off_confirmed (void *screen_handle, ngt_widget_t *widget, void *arg)
{
	UNUSED(widget);
	UNUSED(arg);
	void *switching_off_screen_handle = NULL;

	scrm_screen_close(screen_handle);
	scrm_screen_destroy(&screen_handle);

	/* show a screen which contains message "switching off" and do switching off procedure */
	if (!scrm_processing_screen_create(_("Switching off"), NULL, 0, &switching_off_screen_handle))
	{
		scrm_screen_show(switching_off_screen_handle, SCRM_STATUS_ALERT);
	}
#if defined(V_BATTERY_y)
#define RDB_POWER_BATTERY_EVENT_VAR "battery.event"
#define SWITCHING_OFF_EVENT_VAL	"4"
	rdb_set_string(rdb_s, RDB_POWER_BATTERY_EVENT_VAR, SWITCHING_OFF_EVENT_VAL);
#endif
	/* Other methods for switching off can be implemented here */

	return 0;
}

/*
 * Handler for menu option of switching off
 */
static int scrm_power_switch_off_handler (ngt_widget_t *widget, void *arg)
{
	scrm_button_request_t switching_off_confirm_button_req[] = {
			{ BTN_0, MENU_CANCEL_LABEL, scrm_power_cancel_message_handler, arg },
			{ BTN_1, OK_LABEL, scrm_power_switching_off_confirmed, arg }
	};
	void *message_screen_handle = NULL;

	UNUSED(widget);
	UNUSED(arg);

	/* Show message screen */
	if(scrm_message_screen_create(_("Are you sure you want to switch off?"),
			switching_off_confirm_button_req,
			sizeof(switching_off_confirm_button_req) / sizeof(scrm_button_request_t), &message_screen_handle)
	||
       scrm_screen_show(message_screen_handle, SCRM_STATUS_ALERT)){
		errp("Unable to create and show switching off confirmation screen");
		if (message_screen_handle) {
			scrm_screen_destroy(&message_screen_handle);
		}
		return -1;
	}

	return 0;
}

/*
 * Clean up all resources used by the Power screens.
 * This is called by the scrm core when the world ends and also when
 * the top level Power menu screen is closed.
 */
static void
scrm_power_destroy (void)
{
	if (power_menu_screen_handle) {
		scrm_screen_destroy(&power_menu_screen_handle);
		power_menu_widget = NULL;
	}

    if (rdb_s){
    	rdb_close(&rdb_s);
    }
}

/*
 * Close and destroy the top level Power menu screen.
 */
static int
scrm_power_screen_close(ngt_widget_t *widget, void *arg)
{
	int rval;

	rval = scrm_menu_screen_close(widget, arg);
	scrm_power_destroy();

	return rval;
}

/*
 * Handler for the Power main menu option.
 */
static int
scrm_power_selected (ngt_widget_t *widget, void *arg)
{
	int rval = 0;
	unsigned int ix;

	UNUSED(arg);
	UNUSED(widget);

	INVOKE_CHK(rdb_open(NULL, &rdb_s), "Error opening RDB session for Power screen UI");

	/* Create the Power menu screen with the Power menu options */
	INVOKE_CHK(scrm_menu_screen_create(SCRM_POWER_MENU_HEADER,
			&power_menu_screen_handle, &power_menu_widget,
			NULL, NULL),
			"Unable to create Power menu screen");

	scrm_power_menu_item_t menu_items[] = {
			{ _("Switch off"), scrm_power_switch_off_handler, NULL },
			{ MENU_ITEM_BACK_LABEL, scrm_power_screen_close, power_menu_screen_handle },
	};

	/* Add options into the menu */
	for (ix = 0; ix < sizeof(menu_items) / sizeof(*menu_items); ix++) {
		INVOKE_CHK(ngt_list_add_item(power_menu_widget, menu_items[ix].label,
				menu_items[ix].cb, menu_items[ix].cb_arg),
				"Unable to add Power menu item");
	}

	INVOKE_CHK(scrm_screen_show(power_menu_screen_handle, SCRM_STATUS_NONE),
			"Unable to show Power menu screen");

done:
	if (rval) {
		scrm_power_destroy();
	}
	return rval;
}

static int
scrm_power_init (void)
{
	int rval;

	rval = scrm_add_top_menu_item(SCRM_POWER_MENU_HEADER, scrm_power_selected, NULL);
	if (rval<0){
		return rval;
	}
	else{
		return 0;
	}
}

scrm_feature_plugin_t scrm_power_plugin = {
		scrm_power_init,
		scrm_power_destroy,
};
