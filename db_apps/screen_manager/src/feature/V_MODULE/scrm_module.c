/*
 * scrm_module.c
 *    Module Screen UI support.
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
#include "scrm_module.h"
#include "rdb_defs.h"
#include "wwan_status.h"

/*
 * V_MEPLOCK and V_MANUAL_ROAMING are not considered in this implementation.
 * Processing SIM status and Network Registration status may need to be changed
 * if there are products with screen and those Vvariables enabled.
 */

typedef enum module_index_ {
	MODULE_SIM_STATUS,
	MODULE_NETWORK_REGISTRATION,
	MODULE_CURRENT_OPERATOR,
	MODULE_CURRENT_BAND,
	MODULE_MAX
} module_index_t;

typedef enum wwan_status_index_ {
	MODULE_WWAN_STATUS,
	MODULE_WWAN_IP,
	MODULE_WWAN_APN,
	MODULE_WWAN_MAX
} wwan_status_index_t;

typedef struct module_status_data_ {
	module_index_t index;
	const char *dbname;
	const char *format;
	ngt_label_t *label;
	int fontsize;
} module_status_data_t;

typedef struct wwan_status_data_ {
	wwan_status_index_t index;
	const char *format;
	ngt_label_t *label;
	int fontsize;
} wwan_status_data_t;

#define MODULE_STATUS_HEADER_FONT_SIZE 12
#define MODULE_STATUS_ITEM_FONT_SIZE 11
static module_status_data_t status_items[] = {
	{ MODULE_SIM_STATUS, RDB_MODULE_SIM_STATUS_VAR, _("SIM: %s"), NULL, MODULE_STATUS_ITEM_FONT_SIZE},
	{ MODULE_NETWORK_REGISTRATION, RDB_MODULE_NETWORK_REGISTRATION_VAR, _("Reg.: %s"), NULL, MODULE_STATUS_ITEM_FONT_SIZE},
	{ MODULE_CURRENT_OPERATOR, RDB_MODULE_CURRENT_OPERATOR_VAR, _("Operator: %s"), NULL, MODULE_STATUS_ITEM_FONT_SIZE},
	{ MODULE_CURRENT_BAND, RDB_MODULE_CURRENT_BAND_VAR, _("Band: %s"), NULL, MODULE_STATUS_ITEM_FONT_SIZE}
};
/* WWAN status items */
static wwan_status_data_t wwan_status_items[] = {
	{ MODULE_WWAN_STATUS, _("%s"), NULL, MODULE_STATUS_ITEM_FONT_SIZE},
	{ MODULE_WWAN_IP, _("IP: %s"), NULL, MODULE_STATUS_ITEM_FONT_SIZE},
	{ MODULE_WWAN_APN, _("APN: %s"), NULL, MODULE_STATUS_ITEM_FONT_SIZE}
};

/* definition of struct to map rdb variable updated to status data index */
typedef struct dbname_to_index_ {
	const char *dbname;
	module_index_t index;
} dbname_to_index_t;

static dbname_to_index_t dbname2index[] = {
	{ RDB_MODULE_SIM_STATUS_VAR, MODULE_SIM_STATUS },
	{ RDB_MODULE_NETWORK_REGISTRATION_VAR, MODULE_NETWORK_REGISTRATION },
	{ RDB_MODULE_CURRENT_OPERATOR_VAR, MODULE_CURRENT_OPERATOR },
	{ RDB_MODULE_CURRENT_BAND_VAR, MODULE_CURRENT_BAND },
	{ RDB_PUK_RETRIES_VAR, MODULE_SIM_STATUS },
	{ RDB_MANUAL_ROAM_RESETTING_VAR, MODULE_SIM_STATUS },
	{ RDB_AUTO_PIN_VAR, MODULE_SIM_STATUS }
};

/* index of registration string in array registration_str[] defined by REG_XXXX */
/* REG_MAX is the end of registration string list (array).
 * If strings are added or deleted, value of REG_MAX must be updated.  */
static const char *registration_str[]={
		_("Not registered"), /* full string: "Not registered, searching stopped" */
		_("Registered"), /* full string "Registered, home network" */
		_("Searching"), /* full string "Not registered, searching..." */
		_("Denied"), /* full string "Registration denied" */
		_("Unknown"),
		_("Registered (R)"), /* registered, roaming */
		_("Registered SMS"), /* full string "Registered for SMS (home network)" */
		_("Registered SMS (R)"), /* full string "Registered for SMS (roaming)" */
		_("Emergency"),
		_("N/A")
};

/* struct for band list */
typedef struct band_ {
	struct band_ *next;
	char *name;
	char *value;
} band_t;

static band_t *band_head, *band_tail;

/* struct for waiting screen while changing band */
typedef struct waiting_rdb_event_ {
	void *screen_handle;
	ngt_event_t *check_event_updated;
}waiting_rdb_event_t;

static waiting_rdb_event_t change_band_waiting;

static void *module_menu_screen_handle;
static ngt_list_t *module_menu_widget;
/* rdb session for Status screen, opened when the screen is created and closed when the screen is closed */
static struct rdb_session *rdb_status_s;
static ngt_event_t *status_event;

/* rdb session for Change Band screen, opened when the screen is created and closed when the screen is closed  */
static struct rdb_session *rdb_band_s;
static void *change_band_screen_handle;
static ngt_list_t *band_list;

static module_status_t cellular_status = CELLULAR_STATUS;
static module_status_t wwan_status = WWAN_STATUS;

static ngt_event_t *wwan_status_update_timer;
#define WWAN_STATUS_UPDATE_INTERVAL_MSECS 1000

/*
 * Closes and destroys change band menu screen.
 */
static int scrm_module_change_band_destroy (ngt_widget_t *widget, void *arg)
{
	band_t *pband;
    UNUSED(widget);
    UNUSED(arg);

    if (change_band_screen_handle) {
    	scrm_screen_close(change_band_screen_handle);
        scrm_screen_destroy(&change_band_screen_handle);
        band_list = NULL;
    }

	while (band_head){
		pband = band_head->next;
		free(band_head->name);
		free(band_head->value);
		free(band_head);
		band_head=pband;
	}
	band_tail = NULL;

    if (rdb_band_s){
    	rdb_close(&rdb_band_s);
    }

    return 0;
}

/*
 * Handler for change band message button which is only to close the message screen
 */
static int scrm_module_change_band_message_handler (void *screen_handle, ngt_widget_t *widget, void *arg)
{
	UNUSED(widget);
	UNUSED(arg);
	scrm_screen_close(screen_handle);
	scrm_screen_destroy(&screen_handle);
	return 0;
}

/* create and show a message screen with OK button */
static int scrm_module_show_message_screen(const char *text,
                                           scrm_status_t status)
{
	int rval = 0;
	void *message_screen_handle = NULL;
	scrm_button_request_t button_req[] = {
			{ BTN_1, OK_LABEL, scrm_module_change_band_message_handler, NULL }
	};
	INVOKE_CHK(scrm_message_screen_create(text, button_req, sizeof(button_req) / sizeof(scrm_button_request_t), &message_screen_handle),
		"Unable to create message screen");

	INVOKE_CHK(scrm_screen_show(message_screen_handle, status), "Unable to show message screen");

done:
	if (rval) {
		if (message_screen_handle) {
			scrm_screen_destroy(&message_screen_handle);
		}
	}
	return rval;
}

/* close changing band waiting screen */
static int scrm_module_close_waiting_screen(void *screen_handle)
{
	if (screen_handle) {
		scrm_screen_close(screen_handle);
		scrm_screen_destroy(&screen_handle);
	}
	return 0;
}

/* close structure related to waiting screen and checking rdb changing band status */
static void close_waiting_rdb_event(waiting_rdb_event_t *p_waiting_rdb_event)
{
	if (p_waiting_rdb_event->screen_handle){
		scrm_module_close_waiting_screen(p_waiting_rdb_event->screen_handle);
		p_waiting_rdb_event->screen_handle = NULL;
	}
	if (p_waiting_rdb_event->check_event_updated){
		ngt_run_loop_delete_event(p_waiting_rdb_event->check_event_updated);
		p_waiting_rdb_event->check_event_updated = NULL;
	}
}

/*
 * check whether changing band completed
 * Return:
 * 	1: completed; 0: no response yet; -1: error
 */
static int is_changing_band_completed(void)
{
	char buf[SCRM_MODULE_BUF_SIZE];
	int len = SCRM_MODULE_BUF_SIZE;

	if (rdb_band_s && !rdb_get_string(rdb_band_s, RDB_BAND_CMD_STATUS, buf, len)){
		if (!strcmp(buf, "[done]")){
			return 1;
		}
		else if (strstr(buf, "[error]")){
			return -1;
		}
		else{
			/* Currently there is only emptry string in this case. */
			return 0;
		}
	}
	else{
		return 0;
	}
}

/* issue "get" command regardless of changing band completed */
static void issue_get_band_cmd(void)
{
	assert(rdb_band_s);
	rdb_set_string(rdb_band_s, RDB_BAND_CMD_COMMAND, "get");
}

/* handler for changing band timeout */
static int waiting_screen_timeout_handler (void *screen_handle, void *arg)
{
	int ret;

	UNUSED(screen_handle);

	assert(arg);
	assert(rdb_band_s);

	ret = is_changing_band_completed();

	close_waiting_rdb_event(arg);
	/* in case changing band completed but timeout event happens before rdb notification */
	if (ret == 1){
		rdb_set_string(rdb_band_s, RDB_BAND_CMD_BACKUP_CONFIG, "");
		issue_get_band_cmd();
		/* completed, close band menu */
		scrm_module_change_band_destroy(NULL,NULL);
		scrm_module_show_message_screen(_("Changing band completed"),
                                        SCRM_STATUS_ALERT);
	}
	else if (ret == -1){
		issue_get_band_cmd();
		scrm_module_show_message_screen(_("Changing band failed"),
                                        SCRM_STATUS_FAIL);
	}
	else{
		issue_get_band_cmd();
		scrm_module_show_message_screen(_("Timeout!"),
                                        SCRM_STATUS_FAIL);
	}

    return NGT_STOP;
}

/* handler for band command status update */
static int change_band_checking_handler (void *arg)
{
	int ret;

	assert(arg);
	assert(rdb_band_s);

	ret = is_changing_band_completed();

	if (ret){
		if (ret == 1){
			rdb_set_string(rdb_band_s, RDB_BAND_CMD_BACKUP_CONFIG, "");
			close_waiting_rdb_event(arg);
			issue_get_band_cmd();
			/* completed, close band menu */
			scrm_module_change_band_destroy(NULL,NULL);
			scrm_module_show_message_screen(_("Changing band completed"),
                                            SCRM_STATUS_ALERT);
		}
		else{
			close_waiting_rdb_event(arg);
			issue_get_band_cmd();
			scrm_module_show_message_screen(_("Changing band failed"),
                                            SCRM_STATUS_FAIL);
		}

		return NGT_STOP;
	}
	else{
		return NGT_CONTINUE;
	}
}

/*
 * Changing band procedure. Based on process in setband.html
 *
 *
	SET blank wwan.0.currentband.cmd.status
	SET wwan.0.currentband.cmd.param.band = selected band
	SET wwan.0.currentband.backup_config = selected band
	SET "set" command: wwan.0.currentband.cmd.command=set

	IF module_type(wwan.0.module_type) != "quanta" AND need reset PLMN
		SET wwan.0.PLMN_select=0,0,0'
		SET wwan.0.PLMN_command_state=5

	During 10 seconds READ wwan.0.currentband.cmd.status until its value is "[done]" or "[error]..."

	(the process in setband.html is a little bit different at this step: it only polls and alerts message for [done],
	no message alerted for [error] or timeout)

	IF command status is [done], SET blank wwan.0.currentband.backup_config
	SET wwan.0.currentband.cmd.command=get
 */
static int scrm_module_change_band_procedure(band_t *selected_band, int reset_plmn){
#define POLLING_CHANGING_BAND_STATUS_IN_S 10
	char buf[SCRM_MODULE_BUF_SIZE];
	int len = SCRM_MODULE_BUF_SIZE;
	void *waiting_screen_handle = NULL;

	assert(selected_band);
	assert(rdb_band_s);

	UNUSED(reset_plmn);

	rdb_set_string(rdb_band_s, RDB_BAND_CMD_STATUS, "");

	if (selected_band->value[0] == '\0'){
		rdb_set_string(rdb_band_s, RDB_BAND_CMD_PARAM, "All bands");
		rdb_set_string(rdb_band_s, RDB_BAND_CMD_BACKUP_CONFIG, "All bands");
	}
	else{
		rdb_set_string(rdb_band_s, RDB_BAND_CMD_PARAM, selected_band->value);
		rdb_set_string(rdb_band_s, RDB_BAND_CMD_BACKUP_CONFIG, selected_band->value);
	}
	rdb_set_string(rdb_band_s, RDB_BAND_CMD_COMMAND, "set");

	if (!rdb_get_string(rdb_band_s, RDB_MODULE_TYPE, buf, len) && strcmp(buf,"quanta") && reset_plmn){
		rdb_set_string(rdb_band_s, RDB_PLMN_SELECTION_MODE_VAR, "");
		rdb_set_string(rdb_band_s, RDB_PLMN_SELECT, "0,0,0");
		rdb_set_string(rdb_band_s, RDB_PLMN_COMMAND_STATE, "5");
	}

	/* show waiting screen and check command status */
	if (scrm_processing_screen_create(_("Changing band"), NULL, 0, &waiting_screen_handle)
			||
        scrm_screen_show_with_timeout(waiting_screen_handle, SCRM_STATUS_ALERT, POLLING_CHANGING_BAND_STATUS_IN_S, waiting_screen_timeout_handler, &change_band_waiting))

	{
		scrm_module_close_waiting_screen(waiting_screen_handle);
	}
	else{
		change_band_waiting.screen_handle = waiting_screen_handle;
		rdb_subscribe(rdb_band_s, RDB_BAND_CMD_STATUS);
		change_band_waiting.check_event_updated = ngt_run_loop_add_fd_event(rdb_fd(rdb_band_s), change_band_checking_handler, &change_band_waiting);
	}

	return 0;
}

/*
 * Handler for acceptance of resetting PLMN selection mode
 */
static int scrm_module_change_band_reset_plmn_selection_confirmed (void *screen_handle, ngt_widget_t *widget, void *arg)
{
	UNUSED(widget);
	assert(arg);

	scrm_screen_close(screen_handle);
	scrm_screen_destroy(&screen_handle);
	scrm_module_change_band_procedure(arg, 1);

	return 0;
}

/*
 * Handler for changing band menu item selected
 */
static int scrm_module_change_band_item_selected (ngt_widget_t *widget, void *arg)
{
	char buf[SCRM_MODULE_BUF_SIZE];
	int len = SCRM_MODULE_BUF_SIZE;

	assert(rdb_band_s);
	assert(arg);

	UNUSED(widget);

	/* if PLMN_selectionMode == "Manual", ask for confirmation to reset PLMN_selectionMode */
	if (!rdb_get_string(rdb_band_s, RDB_PLMN_SELECTION_MODE_VAR, buf, len) && !strcmp(buf,"Manual")){
		scrm_button_request_t plmn_selection_confirm_button_req[] = {
				{ BTN_0, MENU_CANCEL_LABEL, scrm_module_change_band_message_handler, arg },
				{ BTN_1, OK_LABEL, scrm_module_change_band_reset_plmn_selection_confirmed, arg }
		};
		void *message_screen_handle = NULL;

		/* Show message screen */
		if(scrm_message_screen_create(_("Also changing PLMN selection mode to automatic. Are you sure?"),
				plmn_selection_confirm_button_req,
				sizeof(plmn_selection_confirm_button_req) / sizeof(scrm_button_request_t), &message_screen_handle)
		||
           scrm_screen_show(message_screen_handle, SCRM_STATUS_ALERT)){
			errp("Unable to create and show changing plmn selection confirmation screen");
			if (message_screen_handle) {
				scrm_screen_destroy(&message_screen_handle);
			}
			return -1;
		}
	}
	/* else: do changing band procedure without resetting PLMN */
	else{
		scrm_module_change_band_procedure(arg, 0);
	}

	return 0;
}

/*
 * Handler for menu option of changing band.
 * The processed based on webif setband.html.
 */
static int scrm_module_change_band_handler (ngt_widget_t *widget, void *arg)
{
	int rval = 0;
	char *buf = NULL;
	int len = 0;
	char *token, *token_i;
	char *pstring;
	band_t *pband;
	int curr_band_read = 0, curr_band_index = 0, i = 0;

	UNUSED(widget);
	UNUSED(arg);

	INVOKE_CHK(rdb_open(NULL, &rdb_band_s), "Error opening RDB session for Module change band");

    /* Create a band menu */
    INVOKE_CHK(scrm_menu_screen_create(_("Change band"), &change_band_screen_handle, &band_list, NULL, NULL),
    		"Unable to create change band screen");

    /*
     * build band list
     * wwan.0.module_band_list contains string like:
     * 511,All bands&15,GSM All&496,WCDMA All&4,GSM 850&1,GSM 900&2,GSM 1800&8,GSM 1900&64,WCDMA 850&128,WCDMA 900&256,WCDMA 800&32,WCDMA 1900&16,WCDMA 2100
     *
     * wwan.0.currentband.current_band contains bands delimited by ";"
     */
    if (rdb_get_alloc(rdb_band_s, RDB_MODULE_BAND_LIST_VAR, &buf, &len) || buf[0]=='\0' || !strcmp(buf, "N/A")){
    	if (rdb_get_alloc(rdb_band_s, RDB_MODULE_BAND_LIST2_VAR, &buf, &len) || buf[0]=='\0'){
    		errp("Unable to read band list from RDB");
    		rval = -1;
    		goto done;
    	}
    	else{
    		pstring = buf;
    		while ((token = strsep(&pstring, ";")) != NULL){
    			pband = malloc(sizeof(band_t));
    			pband->name = strdup(token);
    			pband->value = strdup(token);
    			pband->next = NULL;

    			if (!band_head){
    				band_head = pband;
    			}
    			if (band_tail){
    				band_tail->next = pband;
    			}
    			band_tail = pband;
    		}
    	}
    }
    else{
		pstring = buf;
		while ((token = strsep(&pstring, "&")) != NULL){
			if ((token_i = strsep(&token, ",")) != NULL){
				pband = malloc(sizeof(band_t));
				pband->name = strdup(token);
				pband->value = strdup(token_i);
				pband->next = NULL;

				if (!band_head){
					band_head = pband;
				}
				if (band_tail){
					band_tail->next = pband;
				}
				band_tail = pband;
			}
		}
    }
    /* add to list widget */
    INVOKE_CHK(((pband = band_head) == NULL), "Band list is empty");

    /* current band */
    if (!rdb_get_alloc(rdb_band_s, RDB_BAND_CMD_PARAM,  &buf, &len)){
    	curr_band_read = 1;
    }

    while (pband){
        INVOKE_CHK(ngt_list_add_item(band_list, pband->name, scrm_module_change_band_item_selected, pband),
        		"Unable to add band to the list");
        if (curr_band_read){
        	if (!strcmp(buf,pband->value)){
        		curr_band_index = i;
        	}
        	i++;
        }

        pband = pband->next;
    }
    /* Add the Back option to exit this screen */
    INVOKE_CHK(ngt_list_add_item(band_list, MENU_ITEM_BACK_LABEL, scrm_module_change_band_destroy, NULL),
               "Unable to add BT available device back");

    if (curr_band_index){
    	ngt_list_set_selected_index(band_list, curr_band_index);
    }

    INVOKE_CHK(scrm_screen_show(change_band_screen_handle, SCRM_STATUS_NONE), "Unable to show change band menu screen");

done:
	free(buf);
	if (rval) {
		scrm_module_change_band_destroy(NULL,NULL);
	}

	return rval;
}

/* Read rdb for module cellular status. outbuf must be pre-allocated */
/* The algorithm is based on webif status.html */
static void module_read_rdb(struct rdb_session * rdb_sess, module_index_t index, const char *dbname, const char *format, char *outbuf, int buflen){
    char buf[SCRM_MODULE_BUF_SIZE];
    int len = SCRM_MODULE_BUF_SIZE;
    int reg_stat;

	if (!rdb_sess || (index >= MODULE_MAX)){
		outbuf[0]='\0';
		return;
	}

	switch (index){
		case MODULE_SIM_STATUS:
			/*
			Based on process in status.html
			*/
			if (rdb_get_string(rdb_sess, dbname, buf, len)){
				snprintf(outbuf, buflen, format, "");
			}
			else{
				len = SCRM_MODULE_BUF_SIZE;
				if (strstr(buf,"SIM locked") || strstr(buf,"incorrect SIM") || strstr(buf,"SIM PIN")){

					if (!rdb_get_string(rdb_sess, RDB_AUTO_PIN_VAR, buf, len) && !strcmp(buf,"0")){
						snprintf(outbuf, buflen, format,_("SIM is PIN locked"));
					}
					else{
						snprintf(outbuf, buflen, format,_("Detecting SIM..."));
					}
				}
				else if (strstr(buf,"CHV1") || strstr(buf,"PUK")){
					if (rdb_get_string(rdb_sess, RDB_PUK_RETRIES_VAR, buf, len) || strcmp(buf,"0")){
						snprintf(outbuf, buflen, format,_("SIM is PUK locked"));
					}
					else{
						snprintf(outbuf, buflen, format,_("SIM hardware error"));
					}
				}
				else if (strstr(buf,"MEP")){
					snprintf(outbuf, buflen, format,_("MEP locked"));
				}
				else if (!strcmp(buf, "SIM OK")){
					snprintf(outbuf, buflen, format,_("SIM OK"));
				}
				else if (!strcmp(buf, "N/A") || !strcmp(buf, "Negotiating")){
					/* status.html: if (pukRetries != "0") */
					if (rdb_get_string(rdb_sess, RDB_PUK_RETRIES_VAR, buf, len) || strcmp(buf,"0")){
						/* if(manualroamResetting=="1"), $("#simID").html(_("resettingModem") */
						if (!rdb_get_string(rdb_sess, RDB_MANUAL_ROAM_RESETTING_VAR, buf, len) && !strcmp(buf,"1")){
							snprintf(outbuf, buflen, format,_("Resetting modem"));
						}
						/* else  $("#simID").html(_("detectingSIM") */
						else{
							snprintf(outbuf, buflen, format,_("Detecting SIM..."));
						}
					}
					/* else (pukRetries != "0"): $("#simID").html(_("puk warningMsg1")  */
					else{
						snprintf(outbuf, buflen, format,_("SIM hardware error"));
					}
				}
				else if (!strcmp(buf, "SIM not inserted") || !strcmp(buf, "SIM removed")){
					/* if (pukRetries != "0") $("#simID").html(_("simIsNotInserted") */
					if (rdb_get_string(rdb_sess, RDB_PUK_RETRIES_VAR, buf, len) || strcmp(buf,"0")){
						snprintf(outbuf, buflen, format,_("SIM is not inserted"));
					}
					/* else $("#simID").html(_("puk warningMsg1") */
					else{
						snprintf(outbuf, buflen, format,_("SIM hardware error"));
					}
				}
				/* status.html: "SIM general failure", ""PH-NET PIN", "SIM PH-NET", "Network reject - Account",
				 * "Network reject", and else
				 */
				else{
					/*
					 * In status.html it is "$("#simID").html(simStatus)"
					 * which does not seem to comply to international string mechanism.
					 * Currently international string mechanism has not been implemented in screen UI
					 * and _(buf) would require translation at run-time
					 * TODO: When international string mechanism is implemented, this may need to be changed.
					 */
					snprintf(outbuf, buflen, format, _(buf));
				}
			}
			break;

		case MODULE_NETWORK_REGISTRATION:
			if (rdb_get_string(rdb_sess, RDB_MODULE_SIM_STATUS_VAR, buf, len) || strcmp(buf,"SIM OK") || rdb_get_int(rdb_sess, dbname, &reg_stat) || reg_stat>=REG_MAX){
				snprintf(outbuf, buflen, format, registration_str[REG_NA]);
			}
			else{
				snprintf(outbuf, buflen, format, registration_str[reg_stat]);
			}
			break;

		case MODULE_CURRENT_OPERATOR:
			/*
			 * wwan.0.system_network_status.network contains string like %54%65%6c%73%74%72%61 --> Telstra
			 *
			 * process in ajax.c and status.html:
			 *
			 * printf("var provider='%s';", gt_single("wwan.0.system_network_status.network"));
			 * ....
			 * function UrlDecode(str) {
			 * 		var output = "";
			 * 		for (var i = 0; i < str.length; i+=3) {
			 * 			var val = parseInt("0x" + str.substring(i + 1, i + 3));
			 * 			output += String.fromCharCode(val);
			 * 		}
			 * 		return output;
			 * }
			 * if (provider.charAt(0) == "%") {
			 * 		provider = UrlDecode(provider).replace("&","&#38");
			 * }
			 * if(provider=="Limited Service") {
			 * 		provider=_("limited service");
			 * }
			 * else if (provider=="N/A") {
			 * 		provider=_("na");
			 * }
			 * $("#provider").html(provider);
			 */
			if (rdb_get_string(rdb_sess, dbname, buf, len) || (buf[0]!='%')){
				snprintf(outbuf, buflen, format, "");
			}
			else{
				size_t str_len = strlen(buf);
				char parsed_str[SCRM_MODULE_BUF_SIZE];
				char *endptr;
				long char_code;
				int i=0, j=0;

				while (i <= (int)str_len - 3){
					*(buf+i+3) = '\0';
					char_code = strtol(buf+i+1, &endptr, 16);
					if (*endptr == '\0'){
						parsed_str[j++] = (char)char_code;
					}
					i += 3;
				}
				parsed_str[j] = '\0';

				if (!strcmp(parsed_str, "Limited Service")){
					snprintf(outbuf, buflen, format, _("Limited service"));
				}
				else if (!strcmp(parsed_str, "N/A")){
					snprintf(outbuf, buflen, format, _("N/A"));
				}
				else{
					snprintf(outbuf, buflen, format, parsed_str);
				}
			}
			break;

		case MODULE_CURRENT_BAND:
			/*
			 * Process in ajax.c and status.html:
			 * printf("freq='%s';", get_single("wwan.0.system_network_status.current_band"));
			 * ....
				switch(parseInt(networkRegistration)) {
						case 0:
						case 2:
						case 9:
						freq=_("na");
						break;
				}

        		$("#freq").html(freq.replace("IMT2000", "WCDMA 2100"));
			 */

			if (!rdb_get_int(rdb_sess, RDB_MODULE_NETWORK_REGISTRATION_VAR, &reg_stat)
					&& (reg_stat== REG_UNREGISTERED_NO_SEARCH || reg_stat==REG_UNREGISTERED_SEARCHING || reg_stat==REG_NA)){
				snprintf(outbuf, buflen, format, _("N/A"));
			}
			else{
				len = SCRM_MODULE_BUF_SIZE;
				if (rdb_get_string(rdb_sess, dbname, buf, len)){
					snprintf(outbuf, buflen, format, "");
				}
				else{
					/* this is not similar to $("#freq").html(freq.replace("IMT2000", "WCDMA 2100")) but
					 * for this context current band is not a list of band, so a replace function is not needed  */
					if (!strcmp(buf, "IMT2000")){
						snprintf(outbuf, buflen, format, "WCDMA 2100");
					}
					else{
						snprintf(outbuf, buflen, format, buf);
					}
				}
			}
			break;

		default:
			if (rdb_get_string(rdb_sess, dbname, buf, len)){
				buf[0] = '\0';
			}
			snprintf(outbuf, buflen, format, buf);
			break;
	}

	return;
}

/* module cellular status label update from rdb */
static void scrm_module_rdb_status_update(struct rdb_session *rdb_sess, module_status_data_t *p_module_status_data, int data_num){
	unsigned int i;
	char buf[SCRM_MODULE_LABEL_BUF_SIZE];
	char *dbnames = NULL;
	int dbnames_len = SCRM_MODULE_BUF_SIZE;
	char *dbname;
	char *pstring;
	dbname_to_index_t *p_name2index;
	int triggered_bm = 0;

	if (!rdb_sess || !p_module_status_data){
		return;
	}

	/* check rdb variables updated and set bitmap to update corresponding labels */
	dbnames = malloc(SCRM_MODULE_BUF_SIZE);

	if (dbnames && !rdb_getnames_alloc(rdb_sess, "", &dbnames, &dbnames_len, TRIGGERED)){
		pstring = dbnames;

		while ((dbname = strsep(&pstring, RDB_GETNAMES_DELIMITER_STR)) != NULL){
			for (i=0; i< sizeof(dbname2index)/sizeof(dbname_to_index_t); i++){
				p_name2index = &dbname2index[i];
				if (!strcmp(dbname,p_name2index->dbname)){
					triggered_bm |= (1 << p_name2index->index);
				}
			}
		}

		for (i=0;i<(unsigned int)data_num;i++){
			if (p_module_status_data->label && (triggered_bm & (1 << p_module_status_data->index))){
				module_read_rdb(rdb_sess, p_module_status_data->index, p_module_status_data->dbname, p_module_status_data->format, buf, SCRM_MODULE_LABEL_BUF_SIZE);
				ngt_label_set_text(p_module_status_data->label, buf);
			}
			p_module_status_data++;
		}
	}

	free(dbnames);
}

/* set up module cellular status labels of which values are read from rdb variables
 * Return 0 on success, -1 on error */
static int scrm_module_rdb_status_setup(struct rdb_session *rdb_sess, module_status_data_t *p_module_status_data, int data_num, ngt_linear_layout_t *linear_layout){
	int i;
	char buf[SCRM_MODULE_LABEL_BUF_SIZE];

	if (!rdb_sess || !p_module_status_data || !linear_layout){
		return -1;
	}

	for (i=0;i<data_num;i++){
		module_read_rdb(rdb_sess, p_module_status_data->index, p_module_status_data->dbname, p_module_status_data->format, buf, SCRM_MODULE_LABEL_BUF_SIZE);

		p_module_status_data->label = ngt_label_new(buf);
		if (p_module_status_data->label){
			if (ngt_widget_set_font(NGT_WIDGET(p_module_status_data->label), NULL, p_module_status_data->fontsize)){
				errp("Module status: Unable to set font for label index %d", p_module_status_data->index);
				return -1;
			}
			else{
				if (ngt_linear_layout_add_widget(linear_layout, NGT_WIDGET(p_module_status_data->label))){
					errp("Module status: Unable to add module status label index %d", p_module_status_data->index);
					return -1;
				}
			}
		}
		p_module_status_data++;
	}

	return 0;
}

/*
 * update wwan status
 * @p_wwan_data: pointer to input data structure
 * @index: index of status item on screen
 * @format: format of label text
 * @output: output text buffer
 * @buflen: length of output buffer
 */
static void wwan_status_update(wwan_data_t *p_wwan_data, wwan_status_index_t index, const char *format, char *outbuf, int buflen)
{
	switch (index) {
		case MODULE_WWAN_STATUS:
			snprintf(outbuf, buflen, format, p_wwan_data->status);
			break;

		case MODULE_WWAN_IP:
			snprintf(outbuf, buflen, format, p_wwan_data->ip);
			break;

		case MODULE_WWAN_APN:
			snprintf(outbuf, buflen, format, p_wwan_data->apn);
			break;

		default:
			break;
	}
}

/* set up wwan status labels
 * @rdb_sess: rdb session
 * @p_wwan_status_data: pointer to array representing status items on screen
 * @data_num: size of array representing status items on screen
 * @linear_layout: layout of wwan status screen
 *
 * Return 0 on success, -1 on error */
static int scrm_module_wwan_status_setup(struct rdb_session *rdb_sess, wwan_status_data_t *p_wwan_status_data, int data_num, ngt_linear_layout_t *linear_layout){
	int i;
	char buf[SCRM_MODULE_LABEL_BUF_SIZE];
	wwan_data_t wwan_data;

	if (!rdb_sess || !p_wwan_status_data || !linear_layout){
		return -1;
	}

	scrm_module_update_wwan_status(rdb_sess, &wwan_data);

	for (i=0;i<data_num;i++){

		wwan_status_update(&wwan_data, p_wwan_status_data->index, p_wwan_status_data->format, buf, sizeof(buf));

		p_wwan_status_data->label = ngt_label_new(buf);
		if (p_wwan_status_data->label){
			if (ngt_widget_set_font(NGT_WIDGET(p_wwan_status_data->label), NULL, p_wwan_status_data->fontsize)){
				errp("Module wwan status: Unable to set font for wwan label index %d", p_wwan_status_data->index);
				return -1;
			}
			else{
				if (ngt_linear_layout_add_widget(linear_layout, NGT_WIDGET(p_wwan_status_data->label))){
					errp("Module wwan status: Unable to add module wwan status label index %d", p_wwan_status_data->index);
					return -1;
				}
			}
		}
		p_wwan_status_data++;
	}

	return 0;
}

/*
 * Close status screen
 */
static int
scrm_module_status_close (void *screen_handle, ngt_widget_t *widget, void *arg)
{
	unsigned int i;
	module_status_t *status_type = (module_status_t*)arg;

	UNUSED(widget);

	if (*status_type == CELLULAR_STATUS && status_event){
		ngt_run_loop_delete_event(status_event);
		status_event = NULL;
	}
	else if (*status_type == WWAN_STATUS && wwan_status_update_timer) {
		ngt_run_loop_delete_event(wwan_status_update_timer);
		wwan_status_update_timer = NULL;
	}

	/* Close and destroy the processing screen to go back to previous screen */
	scrm_screen_close(screen_handle);
	scrm_screen_destroy(&screen_handle);

	if (rdb_status_s) {
		rdb_close(&rdb_status_s);
	}

	if (*status_type == CELLULAR_STATUS) {
		for (i=0;i<sizeof(status_items)/sizeof(module_status_data_t);i++){
			status_items[i].label = NULL;
		}
	}
	else if (*status_type == WWAN_STATUS) {
		for (i=0;i<sizeof(wwan_status_items)/sizeof(wwan_status_data_t);i++){
			wwan_status_items[i].label = NULL;
		}
	}

	return 0;
}

/* handler for rdb updates related to module cellular status */
static int rdb_status_handler (void *arg)
{
    UNUSED(arg);

    scrm_module_rdb_status_update(rdb_status_s, status_items, sizeof(status_items)/sizeof(module_status_data_t));

    return NGT_CONTINUE;
}

/* handler for wwan status update timer */
static int wwan_status_update_timer_handler(void *arg)
{
	unsigned int i;
	char buf[SCRM_MODULE_LABEL_BUF_SIZE];
	wwan_data_t wwan_data;
	wwan_status_data_t *p_wwan_status_data;

	UNUSED(arg);

	scrm_module_update_wwan_status(rdb_status_s, &wwan_data);

	p_wwan_status_data = wwan_status_items;
	for (i=0; i<sizeof(wwan_status_items)/sizeof(wwan_status_data_t) && p_wwan_status_data->label; i++){
		/* update and change label text */
		wwan_status_update(&wwan_data, p_wwan_status_data->index, p_wwan_status_data->format, buf, sizeof(buf));
		ngt_label_set_text(p_wwan_status_data->label, buf);
		p_wwan_status_data++;
	}

	return NGT_CONTINUE;
}

/*
 * Handler for the Module Status menu option.
 */
static int
scrm_module_status_handler (ngt_widget_t *widget, void *type_arg)
{
	int rval = 0;
	void *screen_handle = NULL;
	ngt_linear_layout_t *linear_layout = NULL;
	module_status_t *status_type = (module_status_t*)type_arg;

	scrm_button_request_t button_req[] = {
			{ BTN_1, OK_LABEL, scrm_module_status_close, type_arg }
	};

	UNUSED(widget);

	if (!status_type) {
		return -EINVAL;
	}

	INVOKE_CHK(rdb_open(NULL, &rdb_status_s), "Error opening RDB session for Module status");

	/* Create the screen */
	INVOKE_CHK(scrm_screen_create(SCRM_BAR_FLAG_NONE,
			SCRM_BAR_FLAG_CREATE | SCRM_BAR_FLAG_FILL,
			button_req,
			sizeof(button_req)/sizeof(scrm_button_request_t),
			&screen_handle),
			"Unable to create screen");

	/* create layout */
	INVOKE_CHK((linear_layout = ngt_linear_layout_new(NGT_VERTICAL)) == NULL, "Unable to create layout for module status");
	ngt_widget_set_vertical_alignment(NGT_WIDGET(linear_layout), NGT_ALIGN_TOP);
	ngt_widget_set_horizontal_alignment(NGT_WIDGET(linear_layout), NGT_ALIGN_LEFT);

	/* add labels */
	if (*status_type == CELLULAR_STATUS) {
		INVOKE_CHK(scrm_module_rdb_status_setup(rdb_status_s, status_items, sizeof(status_items)/sizeof(module_status_data_t), linear_layout),
			"Failed to add cellular status labels");
	}
	else if (*status_type == WWAN_STATUS) {
		INVOKE_CHK(scrm_module_wwan_status_setup(rdb_status_s, wwan_status_items, sizeof(wwan_status_items)/sizeof(wwan_status_data_t), linear_layout),
			"Failed to add wwan status labels");
	}
	else {
		rval = -EINVAL;
		goto done;
	}

	INVOKE_CHK(scrm_screen_set_main_content(screen_handle, NGT_WIDGET(linear_layout)),
			"Failed to set main content");

	INVOKE_CHK(scrm_screen_show(screen_handle, SCRM_STATUS_NONE),
			"Unable to show Module status screen");

	if (*status_type == CELLULAR_STATUS) {
		/* register event */
		rdb_subscribe(rdb_status_s, RDB_MODULE_SIM_STATUS_VAR);
		rdb_subscribe(rdb_status_s, RDB_MODULE_NETWORK_REGISTRATION_VAR);
		rdb_subscribe(rdb_status_s, RDB_MODULE_CURRENT_OPERATOR_VAR);
		rdb_subscribe(rdb_status_s, RDB_MODULE_CURRENT_BAND_VAR);
		rdb_subscribe(rdb_status_s, RDB_PUK_RETRIES_VAR);
		rdb_subscribe(rdb_status_s, RDB_MANUAL_ROAM_RESETTING_VAR);
		rdb_subscribe(rdb_status_s, RDB_AUTO_PIN_VAR);

		status_event = ngt_run_loop_add_fd_event(rdb_fd(rdb_status_s), rdb_status_handler, NULL);
	}
	else if (*status_type == WWAN_STATUS) {
		/* add wwan status update timer */
		wwan_status_update_timer = ngt_run_loop_add_timer_event(WWAN_STATUS_UPDATE_INTERVAL_MSECS, wwan_status_update_timer_handler, NULL);
	}
done:
	if (rval) {
		unsigned int i;
		if (screen_handle) {
			scrm_screen_close(screen_handle);
			scrm_screen_destroy(&screen_handle);
		}

		if (rdb_status_s) {
			rdb_close(&rdb_status_s);
		}

		if (*status_type == CELLULAR_STATUS) {
			for (i=0;i<sizeof(status_items)/sizeof(module_status_data_t);i++){
				status_items[i].label = NULL;
			}
		}
		else if (*status_type == WWAN_STATUS) {
			for (i=0;i<sizeof(wwan_status_items)/sizeof(wwan_status_data_t);i++){
				wwan_status_items[i].label = NULL;
			}
		}
	}

	return rval;
}

/*
 * Clean up all resources used by the Module screens.
 * This is called by the scrm core when the world ends and also when
 * the top level Module menu screen is closed.
 */
static void
scrm_module_destroy (void)
{
	if (status_event){
		ngt_run_loop_delete_event(status_event);
		status_event = NULL;
	}
	if (wwan_status_update_timer) {
		ngt_run_loop_delete_event(wwan_status_update_timer);
		wwan_status_update_timer = NULL;
	}

	scrm_module_change_band_destroy(NULL,NULL);

	if (module_menu_screen_handle) {
		scrm_screen_destroy(&module_menu_screen_handle);
		module_menu_widget = NULL;
	}

	if (rdb_status_s) {
		rdb_close(&rdb_status_s);
	}
}

/*
 * Close and destroy the top level Module menu screen.
 */
static int
scrm_module_screen_close(ngt_widget_t *widget, void *arg)
{
	int rval;

	rval = scrm_menu_screen_close(widget, arg);
	scrm_module_destroy();

	return rval;
}

/*
 * Handler for the Module main menu option.
 */
static int
scrm_module_selected (ngt_widget_t *widget, void *arg)
{
	int rval = 0;
	unsigned int ix;

	UNUSED(arg);
	UNUSED(widget);

	/* Create the Module menu screen with the Module menu options */
	INVOKE_CHK(scrm_menu_screen_create(SCRM_MODULE_MENU_HEADER,
			&module_menu_screen_handle, &module_menu_widget,
			NULL, NULL),
			"Unable to create Module menu screen");

	scrm_module_menu_item_t menu_items[] = {
			{ _("Cellular status"), scrm_module_status_handler, &cellular_status },
			{ _("WWAN status"), scrm_module_status_handler, &wwan_status },
			{ _("Change band"), scrm_module_change_band_handler, NULL },
			{ MENU_ITEM_BACK_LABEL, scrm_module_screen_close, module_menu_screen_handle },
	};

	/* Add all the Module options into the Module menu */
	for (ix = 0; ix < sizeof(menu_items) / sizeof(*menu_items); ix++) {
		INVOKE_CHK(ngt_list_add_item(module_menu_widget, menu_items[ix].label,
				menu_items[ix].cb, menu_items[ix].cb_arg),
				"Unable to add Module menu item");
	}

	INVOKE_CHK(scrm_screen_show(module_menu_screen_handle, SCRM_STATUS_NONE),
			"Unable to show Module menu screen");

done:
	if (rval) {
		scrm_module_destroy();
	}
	return rval;
}

static int
scrm_module_init (void)
{
	int rval;

	rval = scrm_add_top_menu_item(SCRM_MODULE_MENU_HEADER, scrm_module_selected, NULL);
	if (rval<0){
		return rval;
	}
	else{
		return 0;
	}
}

scrm_feature_plugin_t scrm_module_plugin = {
		scrm_module_init,
		scrm_module_destroy,
};
