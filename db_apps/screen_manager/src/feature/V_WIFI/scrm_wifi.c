/*
 * scrm_wifi.c
 *    Wifi Screen UI support.
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

#include "scrm_wifi.h"

static struct rdb_session *rdb_s;
static void *wifi_menu_screen_handle;
static ngt_list_t *wifi_menu_widget;

static wifi_func_t ap_func = SCRM_WIFI_AP;
#ifdef V_WIFI_CLIENT_backports
static wifi_func_t client_func = SCRM_WIFI_CLIENT;
#endif

#define WIFI_AP_DISABLE _("Disable AP")
#define WIFI_AP_ENABLE _("Enable AP")
#define WIFI_CLIENT_DISABLE _("Disable Client")
#define WIFI_CLIENT_ENABLE _("Enable Client")

typedef enum wifi_index_ {
	WIFI_RADIO,
	WIFI_SSID,
	WIFI_AUTH,
	WIFI_CHANNEL,
#ifdef V_WIFI_CLIENT_backports
	WIFI_CLIENT_RADIO,
	WIFI_CLIENT_SSID,
	WIFI_CLIENT_STATUS,
	WIFI_CLIENT_IP,
#endif
	WIFI_MAX
} wifi_index_t;

typedef struct wifi_status_data_ {
	wifi_index_t index;
	const char *dbname;
	const char *format;
	ngt_label_t *label;
	int fontsize;
} wifi_status_data_t;

#define WIFI_STATUS_HEADER_FONT_SIZE 12
#define WIFI_STATUS_ITEM_FONT_SIZE 11
static wifi_status_data_t status_items[] = {
		{ WIFI_RADIO, RDB_WIFI_RADIO_VAR, _("WLAN AP %s"), NULL, WIFI_STATUS_HEADER_FONT_SIZE},
		{ WIFI_SSID, RDB_WIFI_SSID_VAR, _("SSID: %s"), NULL, WIFI_STATUS_ITEM_FONT_SIZE},
		{ WIFI_AUTH, RDB_WIFI_AUTH_VAR, _("Auth: %s"), NULL, WIFI_STATUS_ITEM_FONT_SIZE},
		{ WIFI_CHANNEL, RDB_WIFI_CHANNEL_VAR, _("Channel: %s"), NULL, WIFI_STATUS_ITEM_FONT_SIZE}
};

#ifdef V_WIFI_CLIENT_backports
#define WIFI_CLIENT_STATUS_HEADER_FONT_SIZE 12
#define WIFI_CLIENT_STATUS_ITEM_FONT_SIZE 11

static wifi_status_data_t client_status_items[] = {
		{ WIFI_CLIENT_RADIO, RDB_WIFI_CLIENT_RADIO_VAR, _("WLAN Client %s"), NULL, WIFI_STATUS_HEADER_FONT_SIZE},
		{ WIFI_CLIENT_SSID, RDB_WIFI_CLIENT_SSID_VAR, _("SSID: %s"), NULL, WIFI_STATUS_ITEM_FONT_SIZE},
		{ WIFI_CLIENT_STATUS, RDB_WIFI_CLIENT_STATUS_VAR, _("Status: %s"), NULL, WIFI_STATUS_ITEM_FONT_SIZE},
		{ WIFI_CLIENT_IP, RDB_WIFI_CLIENT_IP_VAR, _("IP: %s"), NULL, WIFI_STATUS_ITEM_FONT_SIZE}
};
#endif

#define RDB_WDS_VAR "wlan_sta.0.wds_sta_enable"

static ngt_event_t *enable_menu_event;

static struct rdb_session *rdb_ap_status_s;
static ngt_event_t *ap_status_event;

#ifdef V_WIFI_CLIENT_backports
static struct rdb_session *rdb_client_status_s;
static ngt_event_t *client_status_event;
#endif

/*
 * Get the current wifi enable/disable status.
 */
static int
scrm_wifi_get_enable (int *wifi_enable, wifi_func_t wifi_func)
{
	if (wifi_func == SCRM_WIFI_AP){
		return (rdb_get_int(rdb_s, RDB_WIFI_RADIO_VAR, wifi_enable));
	}
#ifdef V_WIFI_CLIENT_backports
	else if (wifi_func == SCRM_WIFI_CLIENT){
		return (rdb_get_int(rdb_s, RDB_WIFI_CLIENT_RADIO_VAR, wifi_enable));
	}
#endif
	else{
		return -1;
	}
}

/*
 * Set the wifi enable/disable status.
 */
static int
scrm_wifi_set_enable (int enable, wifi_func_t wifi_func)
{
	char *rdb_val;

	rdb_val = enable ? "1" : "0";

	/* Setting rdb variable to enable wifi function */
	if (wifi_func == SCRM_WIFI_AP){
		return (rdb_set_string(rdb_s, RDB_WIFI_RADIO_VAR, rdb_val));
	}
#ifdef V_WIFI_CLIENT_backports
	else if (wifi_func == SCRM_WIFI_CLIENT){
		return (rdb_set_string(rdb_s, RDB_WIFI_CLIENT_RADIO_VAR, rdb_val));
	}
#endif
	else{
		return -1;
	}
}

/*
 * Handler for the Enable Result OK button.
 */
static int
scrm_wifi_enable_result_ok_handler (void *screen_handle, ngt_widget_t *widget,
		void *arg)
{
	UNUSED(widget);
	UNUSED(arg);
	scrm_screen_close(screen_handle);
	scrm_screen_destroy(&screen_handle);
	return 0;
}

/*
 * Handler for the Wifi enable/disable menu option.
 */
static int
scrm_wifi_enable_handler (ngt_widget_t *widget, void *arg)
{
	int wifi_enable;
	int rval = 0;
	char buf[SCRM_WIFI_LABEL_BUF_SIZE];
	void *enable_result_screen_handle = NULL;
	const char *enable_str;
	wifi_func_t wifi_func = *(wifi_func_t*)arg;

	scrm_button_request_t button_req[] = {
			{ BTN_1, OK_LABEL, scrm_wifi_enable_result_ok_handler, NULL }
	};

	assert(wifi_menu_widget);

	UNUSED(widget);

	INVOKE_CHK(scrm_wifi_get_enable(&wifi_enable, wifi_func), "Unable to get Wifi enable");

#ifdef V_WIFI_CLIENT_backports
	/* check Wifi AP channel config before enable client function */
	/* currently if AP is on, AP channel must be fixed */
	if (wifi_func == SCRM_WIFI_CLIENT && !wifi_enable){
		int ap_enabled, ap_channel;
		if (!rdb_get_int(rdb_s, RDB_WIFI_RADIO_VAR, &ap_enabled) && !rdb_get_int(rdb_s, RDB_WIFI_CHANNEL_VAR, &ap_channel)){
			if (ap_enabled && !ap_channel){
				strcpy(buf, _("Error: AP channel must not be in auto mode"));
				goto show_result_screen;
			}
		}
	}
#endif

	/* Toggle the Wifi enable status */
	rval = scrm_wifi_set_enable(!wifi_enable, wifi_func);
	if (rval) {
		enable_str =  wifi_enable ? _("disable") : _("enable");
		if (wifi_func == SCRM_WIFI_AP){
			snprintf(buf, sizeof(buf), _("Failed to %s Wireless AP"), enable_str);
		}
		else if (wifi_func == SCRM_WIFI_CLIENT){
			snprintf(buf, sizeof(buf), _("Failed to %s Wireless Client"), enable_str);
		}
		else{
			rval=-1;
			goto done;
		}
	} else {
		enable_str =  wifi_enable ? _("disabled") : _("enabled");
		if (wifi_func == SCRM_WIFI_AP){
			snprintf(buf, sizeof(buf), _("Wireless AP %s"), enable_str);
		}
		else if (wifi_func == SCRM_WIFI_CLIENT){
			snprintf(buf, sizeof(buf), _("Wireless Client %s"), enable_str);
		}
		else{
			rval=-1;
			goto done;
		}
	}
show_result_screen:
	/* Show a result screen */
	INVOKE_CHK(scrm_message_screen_create(buf,
			button_req,
			sizeof(button_req) / sizeof(scrm_button_request_t),
			&enable_result_screen_handle),
			"Unable to create enable result screen");

	INVOKE_CHK(scrm_screen_show(enable_result_screen_handle, SCRM_STATUS_ALERT),
			"Unable to show enable result screen");

done:
	if (rval) {
		if (enable_result_screen_handle) {
			scrm_screen_destroy(&enable_result_screen_handle);
		}
	}
	return rval;
}

/* read rdb for wifi status. outbuf must be pre-allocated */
static void wifi_read_rdb(struct rdb_session * rdb_sess, wifi_index_t index, const char *dbname, const char *format, char *outbuf, int buflen){
    char buf[SCRM_WIFI_BUF_SIZE];
    int len = SCRM_WIFI_BUF_SIZE;
#ifdef V_WIFI_CLIENT_backports
    int wds_sta_mode = 0;
#endif

	if (!rdb_sess || (index >= WIFI_MAX)){
		outbuf[0]='\0';
		return;
	}

	switch (index){
		case WIFI_RADIO:
			if (rdb_get_string(rdb_sess, dbname, buf, len)){
				snprintf(outbuf, buflen, format, _("Unknown"));
			}
			else{
				if (!strcmp(buf, "1")){
					if (rdb_get_string(rdb_sess, RDB_WIFI_ENABLE_VAR, buf, len)){
						snprintf(outbuf, buflen, format, _("Disabled"));
					}
					else{
						snprintf(outbuf, buflen, format, strcmp(buf, "1") == 0 ? _("Enabled") : _("Disabled"));
					}
				}
				else{
					snprintf(outbuf, buflen, format, _("Disabled"));
				}
			}
			break;

		case WIFI_CHANNEL:
			if (rdb_get_string(rdb_sess, dbname, buf, len)){
				buf[0] = '\0';
			}
			else{
				if (!strcmp(buf, "0")){
					if (rdb_get_string(rdb_sess, RDB_WIFI_CUR_CHANNEL_VAR, buf, len) || !strcmp(buf, "0")){
						buf[0] = '\0';
					}
				}
			}
			snprintf(outbuf, buflen, format, buf);
			break;
#ifdef V_WIFI_CLIENT_backports
		case WIFI_CLIENT_RADIO:
			if (rdb_get_string(rdb_sess, dbname, buf, len)){
				snprintf(outbuf, buflen, format, _("Unknown"));
			}
			else{
				snprintf(outbuf, buflen, format, strcmp(buf, "1") == 0 ? _("Enabled") : _("Disabled"));
			}
			break;

		case WIFI_CLIENT_STATUS:
		    rdb_get_int(rdb_sess, RDB_WDS_VAR, &wds_sta_mode);
			if (wds_sta_mode){
				strcpy(buf, _("WDS"));
			}
			else{
				if(rdb_get_string(rdb_sess, dbname, buf, len)){
					buf[0]='\0';
				}
			}
			snprintf(outbuf, buflen, format, buf);
			break;

		case WIFI_CLIENT_IP:
		    rdb_get_int(rdb_sess, RDB_WDS_VAR, &wds_sta_mode);
			if (wds_sta_mode){
				/* no IP address with WDS mode, labeled as "IP" hence no need to show MAC address */
				strcpy(buf, _("N/A"));
			}
			else{
				if (rdb_get_string(rdb_sess, dbname, buf, len)){
					buf[0] = '\0';
				}
			}
			snprintf(outbuf, buflen, format, buf);
			break;

#endif
		default:
			if (rdb_get_string(rdb_sess, dbname, buf, len)){
				buf[0] = '\0';
			}
			snprintf(outbuf, buflen, format, buf);
			break;
	}

	return;
}

/* wifi label update from rdb */
static void scrm_wifi_rdb_status_update(struct rdb_session *rdb_sess, wifi_status_data_t *p_wifi_status_data, int data_num){
	int i;
	char buf[SCRM_WIFI_LABEL_BUF_SIZE];

	if (!rdb_sess || !p_wifi_status_data){
		return;
	}

	for (i=0;i<data_num;i++){
		if (p_wifi_status_data->label){
			wifi_read_rdb(rdb_sess, p_wifi_status_data->index, p_wifi_status_data->dbname, p_wifi_status_data->format, buf, SCRM_WIFI_LABEL_BUF_SIZE);
			ngt_label_set_text(p_wifi_status_data->label, buf);
		}
		p_wifi_status_data++;
	}
}

/* set up wifi label from rdb
 * Return 0 on success, -1 on error */
static int scrm_wifi_rdb_status_setup(struct rdb_session *rdb_sess, wifi_status_data_t *p_wifi_status_data, int data_num, ngt_linear_layout_t *linear_layout){
	int i;
	char buf[SCRM_WIFI_LABEL_BUF_SIZE];

	if (!rdb_sess || !p_wifi_status_data || !linear_layout){
		return -1;
	}

	for (i=0;i<data_num;i++){
		wifi_read_rdb(rdb_sess, p_wifi_status_data->index, p_wifi_status_data->dbname, p_wifi_status_data->format, buf, SCRM_WIFI_LABEL_BUF_SIZE);

		p_wifi_status_data->label = ngt_label_new(buf);
		if (p_wifi_status_data->label){
			/* set font size */
			if (ngt_widget_set_font(NGT_WIDGET(p_wifi_status_data->label), NULL, p_wifi_status_data->fontsize)){
				errp("Wifi status: Unable to set font for label %d", p_wifi_status_data->index);
				return -1;
			}
			else{
				if (ngt_linear_layout_add_widget(linear_layout, NGT_WIDGET(p_wifi_status_data->label))){
					errp("Wifi status: Unable to add wifi status label %d", p_wifi_status_data->index);
					return -1;
				}
			}
		}
		p_wifi_status_data++;
	}

	return 0;
}

/*
 * Close AP status screen
 */
static int
scrm_wifi_ap_status_close (void *screen_handle, ngt_widget_t *widget, void *arg)
{
	unsigned int i;

	UNUSED(widget);
	UNUSED(arg);

	if (ap_status_event){
		ngt_run_loop_delete_event(ap_status_event);
		ap_status_event = NULL;
	}

	/* Close and destroy the processing screen to go back to previous screen */
	scrm_screen_close(screen_handle);
	scrm_screen_destroy(&screen_handle);

	if (rdb_ap_status_s) {
		rdb_close(&rdb_ap_status_s);
	}

	for (i=0;i<sizeof(status_items)/sizeof(wifi_status_data_t);i++){
		status_items[i].label = NULL;
	}

	return 0;
}

/* handler for rdb updates related to Wifi AP */
static int rdb_ap_status_handler (void *arg)
{
    UNUSED(arg);

    scrm_wifi_rdb_status_update(rdb_ap_status_s, status_items, sizeof(status_items)/sizeof(wifi_status_data_t));

	/* subscribe again in case rdb variable of current channel did not exist previously */
	rdb_subscribe(rdb_ap_status_s, RDB_WIFI_CUR_CHANNEL_VAR);

    return NGT_CONTINUE;
}

/*
 * Handler for the Wifi AP Status menu option.
 */
static int
scrm_wifi_ap_status_handler (ngt_widget_t *widget, void *arg)
{
	int rval = 0;
	void *screen_handle = NULL;
	ngt_linear_layout_t *linear_layout = NULL;

	scrm_button_request_t button_req[] = {
			{ BTN_1, OK_LABEL, scrm_wifi_ap_status_close, NULL }
	};

	UNUSED(widget);
	UNUSED(arg);

	INVOKE_CHK(rdb_open(NULL, &rdb_ap_status_s), "Error opening RDB session for Wifi AP status");

	/* Create the screen */
	INVOKE_CHK(scrm_screen_create(SCRM_BAR_FLAG_NONE,
			SCRM_BAR_FLAG_CREATE | SCRM_BAR_FLAG_FILL,
			button_req,
			sizeof(button_req) /
			sizeof(scrm_button_request_t),
			&screen_handle),
			"Unable to create screen");

	/* create layout */
	INVOKE_CHK((linear_layout = ngt_linear_layout_new(NGT_VERTICAL)) == NULL, "Unable to create layout for wifi status");
	ngt_widget_set_vertical_alignment(NGT_WIDGET(linear_layout), NGT_ALIGN_TOP);
	ngt_widget_set_horizontal_alignment(NGT_WIDGET(linear_layout), NGT_ALIGN_LEFT);

	/* add labels representing wlan status */
	INVOKE_CHK(scrm_wifi_rdb_status_setup(rdb_ap_status_s, status_items, sizeof(status_items)/sizeof(wifi_status_data_t), linear_layout),
			"Failed to add wifi AP status labels");

	INVOKE_CHK(scrm_screen_set_main_content(screen_handle, NGT_WIDGET(linear_layout)),
			"Failed to set main content");

	INVOKE_CHK(scrm_screen_show(screen_handle, SCRM_STATUS_NONE),
			"Unable to show Wifi screen");

	/* register event */
	rdb_subscribe(rdb_ap_status_s, RDB_WIFI_RADIO_VAR);
	rdb_subscribe(rdb_ap_status_s, RDB_WIFI_ENABLE_VAR);
	rdb_subscribe(rdb_ap_status_s, RDB_WIFI_SSID_VAR);
	rdb_subscribe(rdb_ap_status_s, RDB_WIFI_AUTH_VAR);
	rdb_subscribe(rdb_ap_status_s, RDB_WIFI_CHANNEL_VAR);
	rdb_subscribe(rdb_ap_status_s, RDB_WIFI_CUR_CHANNEL_VAR);

	ap_status_event = ngt_run_loop_add_fd_event(rdb_fd(rdb_ap_status_s), rdb_ap_status_handler, NULL);
done:
	if (rval) {
		unsigned int i;
		if (screen_handle) {
			scrm_screen_close(screen_handle);
			scrm_screen_destroy(&screen_handle);
		}

		if (rdb_ap_status_s) {
			rdb_close(&rdb_ap_status_s);
		}

		for (i=0;i<sizeof(status_items)/sizeof(wifi_status_data_t);i++){
			status_items[i].label = NULL;
		}
	}

	return rval;
}

#ifdef V_WIFI_CLIENT_backports
/*
 * Close Client status screen
 */
static int
scrm_wifi_client_status_close (void *screen_handle, ngt_widget_t *widget, void *arg)
{
	unsigned int i;

	UNUSED(widget);
	UNUSED(arg);

	if (client_status_event){
		ngt_run_loop_delete_event(client_status_event);
		client_status_event = NULL;
	}

	/* Close and destroy the processing screen to go back to previous screen */
	scrm_screen_close(screen_handle);
	scrm_screen_destroy(&screen_handle);

	if (rdb_client_status_s) {
		rdb_close(&rdb_client_status_s);
	}

	for (i=0;i<sizeof(client_status_items)/sizeof(wifi_status_data_t);i++){
		client_status_items[i].label = NULL;
	}

	return 0;
}

/* handler for rdb updates related to Wifi Client mode */
static int rdb_client_status_handler (void *arg)
{
    UNUSED(arg);

    scrm_wifi_rdb_status_update(rdb_client_status_s, client_status_items, sizeof(client_status_items)/sizeof(wifi_status_data_t));

	/* subscribe again in case rdb variable did not exist */
	rdb_subscribe(rdb_client_status_s, RDB_WIFI_CLIENT_STATUS_VAR);
	rdb_subscribe(rdb_client_status_s, RDB_WIFI_CLIENT_IP_VAR);

    return NGT_CONTINUE;
}

/*
 * Handler for the Wifi Client Status menu option.
 */
static int
scrm_wifi_client_status_handler (ngt_widget_t *widget, void *arg)
{
	int rval = 0;
	void *screen_handle = NULL;
	ngt_linear_layout_t *linear_layout = NULL;
	scrm_button_request_t button_req[] = {
			{ BTN_1, OK_LABEL, scrm_wifi_client_status_close, NULL }
	};

	UNUSED(widget);
	UNUSED(arg);

	INVOKE_CHK(rdb_open(NULL, &rdb_client_status_s), "Error opening RDB session for Wifi client status");
	/* Create the screen */
	INVOKE_CHK(scrm_screen_create(SCRM_BAR_FLAG_NONE,
			SCRM_BAR_FLAG_CREATE | SCRM_BAR_FLAG_FILL,
			button_req,
			sizeof(button_req) /
			sizeof(scrm_button_request_t),
			&screen_handle),
			"Unable to create screen");

	/* create layout */
	INVOKE_CHK((linear_layout = ngt_linear_layout_new(NGT_VERTICAL)) == NULL, "Unable to create layout for wifi Client status");
	ngt_widget_set_vertical_alignment(NGT_WIDGET(linear_layout), NGT_ALIGN_TOP);
	ngt_widget_set_horizontal_alignment(NGT_WIDGET(linear_layout), NGT_ALIGN_LEFT);

	/* add labels representing wlan status */
	INVOKE_CHK(scrm_wifi_rdb_status_setup(rdb_client_status_s, client_status_items, sizeof(client_status_items)/sizeof(wifi_status_data_t), linear_layout),
			"Failed to add wifi Client status labels");

	INVOKE_CHK(scrm_screen_set_main_content(screen_handle, NGT_WIDGET(linear_layout)),
			"Failed to set main content");

	INVOKE_CHK(scrm_screen_show(screen_handle, SCRM_STATUS_NONE),
			"Unable to show Wifi screen");

	/* register event */
	rdb_subscribe(rdb_client_status_s, RDB_WIFI_CLIENT_RADIO_VAR);
	rdb_subscribe(rdb_client_status_s, RDB_WIFI_CLIENT_SSID_VAR);
	rdb_subscribe(rdb_client_status_s, RDB_WIFI_CLIENT_STATUS_VAR);
	rdb_subscribe(rdb_client_status_s, RDB_WIFI_CLIENT_IP_VAR);
	rdb_subscribe(rdb_client_status_s, RDB_WDS_VAR);

	client_status_event = ngt_run_loop_add_fd_event(rdb_fd(rdb_client_status_s), rdb_client_status_handler, NULL);

done:
	if (rval) {
		unsigned int i;
		if (linear_layout) {
			ngt_widget_dispose(NGT_WIDGET(linear_layout));
		}
		if (screen_handle) {
			scrm_screen_close(screen_handle);
			scrm_screen_destroy(&screen_handle);
		}

		for (i=0;i<sizeof(client_status_items)/sizeof(wifi_status_data_t);i++){
			client_status_items[i].label = NULL;
		}
	}

	return rval;
}
#endif

/*
 * Clean up all resources used by the Wifi screens.
 * This is called by the scrm core when the world ends and also when
 * the top level Wifi menu screen is closed.
 */
static void
scrm_wifi_destroy (void)
{
	if (wifi_menu_screen_handle) {
		scrm_screen_destroy(&wifi_menu_screen_handle);
		wifi_menu_widget = NULL;
	}

	if (rdb_s) {
		rdb_close(&rdb_s);
	}
}

/*
 * Close and destroy the top level Wifi menu screen.
 */
static int
scrm_wifi_screen_close(ngt_widget_t *widget, void *arg)
{
	int rval;

	if (enable_menu_event){
		ngt_run_loop_delete_event(enable_menu_event);
		enable_menu_event = NULL;
	}

	rval = scrm_menu_screen_close(widget, arg);
	scrm_wifi_destroy();

	return rval;
}

/* handler for rdb updates related to Wifi modes enable for Wifi menu */
static int rdb_enable_menu_handler (void *arg)
{
	int wifi_ap_enable;
#ifdef V_WIFI_CLIENT_backports
	int wifi_client_enable;
#endif
    int rval = 0;

    UNUSED(arg);

	INVOKE_CHK(scrm_wifi_get_enable(&wifi_ap_enable, SCRM_WIFI_AP), "Unable to get Wifi AP enable");
#ifdef V_WIFI_CLIENT_backports
	INVOKE_CHK(scrm_wifi_get_enable(&wifi_client_enable, SCRM_WIFI_CLIENT), "Unable to get Wifi Client enable");
#endif

	if (wifi_ap_enable){
		ngt_list_set_item_text(wifi_menu_widget, SCRM_WIFI_MENU_AP_ENABLE, WIFI_AP_DISABLE);
	}
	else{
		ngt_list_set_item_text(wifi_menu_widget, SCRM_WIFI_MENU_AP_ENABLE, WIFI_AP_ENABLE);
	}
#ifdef V_WIFI_CLIENT_backports
	if (wifi_client_enable){
		ngt_list_set_item_text(wifi_menu_widget, SCRM_WIFI_MENU_CLIENT_ENABLE, WIFI_CLIENT_DISABLE);
	}
	else{
		ngt_list_set_item_text(wifi_menu_widget, SCRM_WIFI_MENU_CLIENT_ENABLE, WIFI_CLIENT_ENABLE);
	}
#endif
done:
    return NGT_CONTINUE;
}

/*
 * Handler for the Wifi main menu option.
 */
static int
scrm_wifi_selected (ngt_widget_t *widget, void *arg)
{
	int rval = 0;
	unsigned int ix;
	int wifi_ap_enable;
#ifdef V_WIFI_CLIENT_backports
	int wifi_client_enable;
#endif

	UNUSED(arg);
	UNUSED(widget);

	INVOKE_CHK(rdb_open(NULL, &rdb_s), "Error opening RDB session");

	/* Create the Wifi menu screen with the Wifi menu options */
	INVOKE_CHK(scrm_menu_screen_create(SCRM_WIFI_MENU_HEADER,
			&wifi_menu_screen_handle, &wifi_menu_widget,
			NULL, NULL),
			"Unable to create Wifi menu screen");

	INVOKE_CHK(scrm_wifi_get_enable(&wifi_ap_enable, SCRM_WIFI_AP), "Unable to get Wifi AP enable");
#ifdef V_WIFI_CLIENT_backports
	INVOKE_CHK(scrm_wifi_get_enable(&wifi_client_enable, SCRM_WIFI_CLIENT), "Unable to get Wifi Client enable");
#endif

	scrm_wifi_menu_item_t menu_items[] = {
			{ _("AP status"), scrm_wifi_ap_status_handler, &ap_func },
#ifdef V_WIFI_CLIENT_backports
			{ _("Client status"), scrm_wifi_client_status_handler, &client_func },
#endif
			/* The text of enable is the opposite of the current status as it is a toggle */
			{ wifi_ap_enable ? WIFI_AP_DISABLE : WIFI_AP_ENABLE, scrm_wifi_enable_handler, &ap_func },
#ifdef V_WIFI_CLIENT_backports
			{ wifi_client_enable ? WIFI_CLIENT_DISABLE : WIFI_CLIENT_ENABLE, scrm_wifi_enable_handler, &client_func },
#endif
			{ MENU_ITEM_BACK_LABEL, scrm_wifi_screen_close, wifi_menu_screen_handle },
	};

	/* Add all the Wifi options into the Wifi menu */
	for (ix = 0; ix < sizeof(menu_items) / sizeof(*menu_items); ix++) {
		INVOKE_CHK(ngt_list_add_item(wifi_menu_widget, menu_items[ix].label,
				menu_items[ix].cb, menu_items[ix].cb_arg),
				"Unable to add Wifi menu item");
	}

	INVOKE_CHK(scrm_screen_show(wifi_menu_screen_handle, SCRM_STATUS_NONE),
			"Unable to show Wifi menu screen");

	/* register event */
	rdb_subscribe(rdb_s, RDB_WIFI_RADIO_VAR);
	rdb_subscribe(rdb_s, RDB_WIFI_CLIENT_RADIO_VAR);

	enable_menu_event = ngt_run_loop_add_fd_event(rdb_fd(rdb_s), rdb_enable_menu_handler, NULL);

done:
	if (rval) {
		scrm_wifi_destroy();
	}
	return rval;
}

static int
scrm_wifi_init (void)
{
	int rval;

	rval = scrm_add_top_menu_item(SCRM_WIFI_MENU_HEADER, scrm_wifi_selected, NULL);

    if (rval < 0) {
        return rval;
    } else {
        return 0;
    }
}

scrm_feature_plugin_t scrm_wifi_plugin = {
		scrm_wifi_init,
		scrm_wifi_destroy,
};
