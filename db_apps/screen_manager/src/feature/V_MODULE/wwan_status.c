/*
 * wwan_status.c
 * Parse WWAN status from RDB variables. Based on algorithm in webif's ajax.c and status.html.
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
#include <rdb_ops.h>
#include <scrm_ops.h>
#include "wwan_status.h"
#include "rdb_defs.h"

#define LINK_PROFILE_RDB_NAME_LENGTH 32
#define RDB_DATA_SHORT_LENGTH 16

static int profile_status(struct rdb_session *rdb_sess, int profile_rdb_index, char *buf, int len);

/*
 * Is DoD enabled?
 * returns 1 if enabled, 0 if disabled or error
 */
static int is_dod_en(struct rdb_session *rdb_sess)
{
	int val;
	char rdb_name[LINK_PROFILE_RDB_NAME_LENGTH];
	int ret;

	if (!rdb_sess){
		return 0;
	}
	/* get dod enable status */
	if (rdb_get_int(rdb_sess, "dialondemand.enable", &val) || !val) {
		return 0;
	}
	/* check matching profile */
	if (!rdb_get_int(rdb_sess, "dialondemand.profile", &val)
		&& (ret = snprintf(rdb_name, sizeof(rdb_name), "link.profile.%d.enable", val)) < (int)sizeof(rdb_name)
		&& ret > 0
		&& !rdb_get_int(rdb_sess, rdb_name, &val)) {
		return 1;
	}
	else {
		return 0;
	}
}

/*
 * check whether SIM is OK
 *
 * returns 1 if SIM OK, 0 if not or error
 */
static int is_sim_ok(struct rdb_session *rdb_sess)
{
	char buf[RDB_DATA_SHORT_LENGTH];

	if (rdb_sess && !rdb_get_string(rdb_sess, RDB_MODULE_SIM_STATUS_VAR, buf, sizeof(buf)) && !strcmp(buf, "SIM OK")) {
		return 1;
	}
	else {
		return 0;
	}
}

/*
 * always connected WWAN?
 *
 * returns 1 if yes, 0 if not or error
 */
static int always_connected_wwan(struct rdb_session *rdb_sess)
{
	char buf[RDB_DATA_SHORT_LENGTH];

	if (rdb_sess && !rdb_get_string(rdb_sess, "service.failover.alwaysConnectWWAN", buf, sizeof(buf)) && !strcmp(buf, "enable")) {
		return 1;
	}
	else {
		return 0;
	}
}

/*
 * get profile enable status
 * @profile_rdb_index: index in link.profile.{index}
 *
 * returns 1 if enabled, 0 if not, or negative error code
 */
static int get_profile_enable(struct rdb_session *rdb_sess, int profile_rdb_index)
{
	char dbname[LINK_PROFILE_RDB_NAME_LENGTH];
	int rval;
	int profile_enable;

	if (!rdb_sess || profile_rdb_index<0){
		return -EINVAL;
	}

	rval = snprintf(dbname, sizeof(dbname),"link.profile.%d.enable", profile_rdb_index);
	if (rval >= (int)sizeof(dbname)){
		return -ENOMEM;
	}
	else if (rval < 0){
		return rval;
	}

	rval = rdb_get_int(rdb_sess, dbname, &profile_enable);
	if (rval) {
		return rval;
	}
	else {
		return (profile_enable > 0);
	}
}

/*
 * is wwan profile?
 * @profile_rdb_index: index in link.profile.{index}
 *
 * returns 1 if yes, 0 if no, or negative value on error
 */
static int is_wwan_profile(struct rdb_session *rdb_sess, int profile_rdb_index)
{
	char dbname[LINK_PROFILE_RDB_NAME_LENGTH];
	int rval;
	char profile_dev[RDB_DATA_SHORT_LENGTH];

	if (!rdb_sess || profile_rdb_index<0){
		return -EINVAL;
	}

	rval = snprintf(dbname, sizeof(dbname),"link.profile.%d.dev", profile_rdb_index);
	if (rval >= (int)sizeof(dbname)){
		return -ENOMEM;
	}
	else if (rval < 0){
		return rval;
	}

	rval = rdb_get_string(rdb_sess, dbname, profile_dev, sizeof(profile_dev));
	if (rval) {
		return rval;
	}
	else {
		/* prefix "wwan." is sufficient  */
		return (strncmp(profile_dev, "wwan.", 5) == 0);
	}
}

/*
 * check whether a profile is up
 * @profile_rdb_index: index in link.profile.{index}
 *
 * return 1 if yes, 0 if no or error
 */
static int profile_up(struct rdb_session *rdb_sess, int profile_rdb_index)
{
	int wwan_activated;
	char buf[RDB_DATA_SHORT_LENGTH];

	if (!rdb_sess || profile_rdb_index<0){
		return -EINVAL;
	}

	/* if wwan is not activated, all profiles are down */
	/* check whether wwan is activated */
	if (rdb_get_int(rdb_sess, "wwan.0.activated", &wwan_activated)) {
		wwan_activated = 0;
	}

	if (wwan_activated) {
		if (!profile_status(rdb_sess, profile_rdb_index, buf, sizeof(buf))) {
			return (strcmp(buf, "up") == 0);
		}
	}

	return 0;
}

/*
 * check default profile
 * @profile_rdb_index: index in link.profile.{index}
 *
 * returns 1 if this is default gateway profile, 0 if not, or negative value on error
 */
static int is_default_profile(struct rdb_session *rdb_sess, int profile_rdb_index)
{
	char dbname[LINK_PROFILE_RDB_NAME_LENGTH];
	int rval;
	int default_profile;

	if (!rdb_sess || profile_rdb_index<0){
		return -EINVAL;
	}

	rval = snprintf(dbname, sizeof(dbname),"link.profile.%d.defaultroute", profile_rdb_index);
	if (rval >= (int)sizeof(dbname)){
		return -ENOMEM;
	}
	else if (rval < 0){
		return rval;
	}

	rval = rdb_get_int(rdb_sess, dbname, &default_profile);
	if (rval) {
		return rval;
	}
	else {
		return (default_profile == 1);
	}
}

/*
 * get profile status
 * @profile_rdb_index: index in link.profile.{index}
 * @buf: output buffer contains profile status
 * @len: length of the output buffer
 *
 * returns 0 if success, negative error otherwise
 */
static int profile_status(struct rdb_session *rdb_sess, int profile_rdb_index, char *buf, int len)
{
	char dbname[LINK_PROFILE_RDB_NAME_LENGTH];
	int rval;

	if (!rdb_sess || profile_rdb_index<0){
		return -EINVAL;
	}

	rval = snprintf(dbname, sizeof(dbname),"link.profile.%d.status", profile_rdb_index);
	if (rval >= (int)sizeof(dbname)){
		return -ENOMEM;
	}
	else if (rval < 0) {
		return rval;
	}
	else {
		return rdb_get_string(rdb_sess, dbname, buf, len);
	}
}

/*
 * get current APN
 * @buf: output buffer contains profile APN
 * @len: length of the output buffer
 *
 * returns 0 if success, -1 on error
 */
static int get_current_apn(struct rdb_session *rdb_sess, int profile_rdb_index, char *buf, int len)
{
	char dbname[LINK_PROFILE_RDB_NAME_LENGTH];
	int auto_apn;
	char roaming_status[RDB_DATA_SHORT_LENGTH];
	char apn[APN_LENGTH];
	int rval;

	if (!rdb_sess || profile_rdb_index<0){
		return -1;
	}

	/* get auto apn */
	rval = snprintf(dbname, sizeof(dbname),"link.profile.%d.autoapn", profile_rdb_index);
	if (rval >= (int)sizeof(dbname) || rval < 0){
		return -1;
	}
	else {

		if (rdb_get_int(rdb_sess, dbname, &auto_apn)) {
			auto_apn = 0;
		}
	}
	/* get roaming status */
	if (rdb_get_string(rdb_sess, "wwan.0.system_network_status.roaming", roaming_status, sizeof(roaming_status))) {
		roaming_status[0]='\0';
	}
	/*
	 * webif search "active" case-insensitive, however wwan.0.system_network_status.roaming is "active" or "deactive"
	 * so strncmp is sufficient
	 */
	if (auto_apn && strncmp(roaming_status, "active", 6)) {
#ifdef V_MULTIPLE_WWAN_PROFILES_y
		/* get current apn */
		rval = snprintf(dbname, sizeof(dbname),"wwan.%d.apn.current", profile_rdb_index);
		if (rval >= (int)sizeof(dbname) || rval < 0){
			return -1;
		}
		else {
			if (rdb_get_string(rdb_sess, dbname, apn, sizeof(apn))) {
				apn[0]='\0';
			}
		}
#else
		apn[0]='\0';
#endif
		if (apn[0]!='\0' && strcmp(apn, "N/A")) {
			if ((int)strlen(apn) < len) {
				strcpy(buf, apn);
			}
			else {
				return -1;
			}
		}
		else {
			/* get current apn from wwan.0.apn.current */
			if (!rdb_get_string(rdb_sess, "wwan.0.apn.current", apn, sizeof(apn))
					&& apn[0]!='\0' && strcmp(apn, "N/A") && (int)strlen(apn)<len) {
				strcpy(buf, apn);
			}
			else {
				buf[0] = '\0';
			}
		}
	}
	else {
		/* get profile apn */
		rval = snprintf(dbname, sizeof(dbname),"link.profile.%d.apn", profile_rdb_index);
		if (rval >= (int)sizeof(dbname) || rval < 0){
			return -1;
		}
		else {
			if (rdb_get_string(rdb_sess, dbname, buf, len)) {
				buf[0]='\0';
			}
		}
	}

	return 0;
}

/*
 * read wwan status
 * @p_wwan_status_data: pointer to output structure
 */
void scrm_module_update_wwan_status(struct rdb_session *rdb_sess, wwan_data_t *p_wwan_status_data)
{
	int data_roaming_blocked;
	int reg_stat;
	int wwan_activated;
	char failover_mode[RDB_DATA_SHORT_LENGTH];
	char failover_interface[RDB_DATA_SHORT_LENGTH];
	int always_connect_wwan;
	int profile_index;
	int connected_profile_index = -1;
	int default_profile_index = -1;
	int profile_enable;
	char dbname[LINK_PROFILE_RDB_NAME_LENGTH];
	int max_enabled_profiles;
	char dbb[RDB_DATA_SHORT_LENGTH];

	if (!rdb_sess || !p_wwan_status_data) {
		return;
	}

	/* get max enabled profiles */
	if (rdb_get_int(rdb_sess, "wwan.0.max_sub_if", &max_enabled_profiles)) {
		max_enabled_profiles = 0;
	}
	/* 1 - connection is blocked due to data roaming. 0 - not blocked */
	if (rdb_get_int(rdb_sess, "roaming.data.blocked", &data_roaming_blocked)) {
		data_roaming_blocked = 0;
	}
	/* check whether wwan is activated */
	if (rdb_get_int(rdb_sess, "wwan.0.activated", &wwan_activated)) {
		wwan_activated = 0;
	}
	/* failover mode */
	if (rdb_get_string(rdb_sess, "service.failover.mode", failover_mode, sizeof(failover_mode))) {
		failover_mode[0] = '\0';
	}

	if (max_enabled_profiles<=0 || data_roaming_blocked || !is_sim_ok(rdb_sess) || !wwan_activated ||  !strcmp(failover_mode, "wan")) {
		/* status label: disconnected, other labels blank */
		strcpy(p_wwan_status_data->status, _("Disconnected"));
		p_wwan_status_data->ip[0] = '\0';
		p_wwan_status_data->apn[0] = '\0';
	}
	else {
		/* get network registration */
		if (rdb_get_int(rdb_sess, "wwan.0.system_network_status.reg_stat", &reg_stat) || reg_stat>=REG_MAX) {
			reg_stat = REG_NA;
		}
		/* failover interface */
		if (rdb_get_string(rdb_sess, "service.failover.interface", failover_interface, sizeof(failover_interface))) {
			failover_interface[0] = '\0';
		}
		/* always connect wwan */
		always_connect_wwan = always_connected_wwan(rdb_sess);

		/* iterate thru link.profile.x to find either up and connected or default profile */
		profile_index = 1;
		while (max_enabled_profiles>0 && (profile_enable = get_profile_enable(rdb_sess, profile_index)) >= 0) {
			/* only consider enabled wwan profiles */
			if (profile_enable && is_wwan_profile(rdb_sess, profile_index)==1) {
				max_enabled_profiles--;
				/* if found connected profile, exit the loop */
				if (profile_up(rdb_sess, profile_index)) {
					connected_profile_index = profile_index;
					break;
				}
				else {
					/* if it is not connected, check whether it is default profile */
					if (is_default_profile(rdb_sess, profile_index)) {
						default_profile_index = profile_index;
					}
				}
			}
			profile_index++;
		}

		/* process found connected or default profile */
		if (connected_profile_index > 0) {
			/* found connected profile */
			strcpy(p_wwan_status_data->status, _("Connected"));
			/* ip: iplocal */
			if (snprintf(dbname, sizeof(dbname),"link.profile.%d.iplocal", connected_profile_index)>=(int)sizeof(dbname)
					|| rdb_get_string(rdb_sess, dbname, p_wwan_status_data->ip, sizeof(p_wwan_status_data->ip))) {
				p_wwan_status_data->ip[0] = '\0';
			}
			/* apn */
			get_current_apn(rdb_sess, connected_profile_index, p_wwan_status_data->apn, sizeof(p_wwan_status_data->apn));
		}
		else if (default_profile_index > 0) {
			/* found default profile */

			if (!strcmp(failover_interface, "wlan")
					|| (!strcmp(failover_interface, "wwan") && !always_connect_wwan && (!strcmp(failover_mode,"wlanlink") || !strcmp(failover_mode,"wlanping")))) {
				/* process this like no profiles enabled */
				/* status label: disconnected, other labels blank */
				strcpy(p_wwan_status_data->status, _("Disconnected"));
				p_wwan_status_data->ip[0] = '\0';
				p_wwan_status_data->apn[0] = '\0';
			}
			else {

				if (is_dod_en(rdb_sess)) {
					/* status: waiting dod */
					strcpy(p_wwan_status_data->status, _("Waiting for demand"));
				}
				else if (!strcmp(failover_interface, "wan")) {
					/* status: waiting */
					strcpy(p_wwan_status_data->status, _("Waiting"));
				}
				else if (!profile_status(rdb_sess, default_profile_index, dbb, sizeof(dbb)) && !strcmp(dbb, "disconnecting")) {
					/* status: disconnecting */
					strcpy(p_wwan_status_data->status, _("Disconnecting"));
				}
				else {
					/* status "down" or other will go here */
					if (reg_stat!=REG_REGISTERED_HOME && reg_stat!=REG_REGISTERED_ROAMING && reg_stat!=REG_NA) {
						/* status: Waiting for registration */
						strcpy(p_wwan_status_data->status, _("Waiting for registration"));
					}
					else {
						char pdp_result[128];
						int ret;
						/* show pdp result or "unknown error" or "connecting" */
						ret = snprintf(dbname, sizeof(dbname),"link.profile.%d.pdp_result", default_profile_index);
						if (ret>=(int)sizeof(dbname) || ret<0 || rdb_get_string(rdb_sess, dbname, pdp_result, sizeof(pdp_result)) || pdp_result[0]=='\0') {
							/* webif shows "Connecting" in this case. */
							strcpy(p_wwan_status_data->status, _("Connecting"));
						}
						else {
							if (!strcmp(pdp_result, "User Authentication failed") || !strcmp(pdp_result, "Authentication failed")) {
								/* status: Authentication failed */
								strcpy(p_wwan_status_data->status, _("Authentication failed"));
							}
							else if (!strcmp(pdp_result, "Requested service option not subscribed")) {
								/* status: Failed due to incorrect APN */
								strcpy(p_wwan_status_data->status, _("Failed due to incorrect APN"));
							}
							else if (!strcmp(pdp_result, "Connection setup timeout") || !strcmp(pdp_result, "Network failure")) {
								/* status: Network timeout */
								strcpy(p_wwan_status_data->status, _("Network timeout"));
							}
							else {
								/* status: Unknown error */
								strcpy(p_wwan_status_data->status, _("Unknown error"));
							}
						}
					}
				}
				/* ip blank */
				p_wwan_status_data->ip[0] = '\0';
				/* apn */
				get_current_apn(rdb_sess, default_profile_index, p_wwan_status_data->apn, sizeof(p_wwan_status_data->apn));
			}
		}
		else {
			/* both connected profile and default profile are not found */
			/* status label: disconnected, other labels blank */
			strcpy(p_wwan_status_data->status, _("Disconnected"));
			p_wwan_status_data->ip[0] = '\0';
			p_wwan_status_data->apn[0] = '\0';
		}
	}
}
