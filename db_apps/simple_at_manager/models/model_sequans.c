
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "../at/at.h"
#include "../model/model.h"
#include "../util/rdb_util.h"
#include "../rdb_names.h"
#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "model_default.h"
#include "../util/at_util.h"

#include "../featurehash.h"


///////////////////////////////////////////////////////////////////////////////
static int model_sequans_init(void)
{
	char resp[AT_RESPONSE_MAX_SIZE];
	int ok;

	/* fix module band list as "LTE" only */
	rdb_set_single(rdb_name(RDB_MODULEBANDLIST, ""), "1,LTE&");

	/* enable auto-attach and enable cgatt */
	if (at_send("AT^AUTOATT=1", NULL, "", &ok, 0) == 0 && ok) {
		if (at_send("AT^AUTOATT?", resp, "", &ok, 0) == 0 && ok) {
			if (strstr(resp,"AUTOATT:1")) {
				SYSLOG_INFO("Auto Attach mode is set.");
			} else {
				SYSLOG_ERR("Failed to set Auto Attach mode");
				goto fini;
			}
		} else {
			SYSLOG_ERR("AT^AUTOATT? command failed");
			goto fini;
		}
	} else {
		SYSLOG_ERR("AT^AUTOATT=1 command failed");
		goto fini;
	}
	/* attach */
	if(send_cgactt(1)) {
		SYSLOG_INFO("Attach command succeeded");
	} else {
		SYSLOG_ERR("Attach command failed");
	}

fini:	
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_sequans_detect(const char* manufacture, const char* model_name)
{
	const char* model_names[]={
		"VZ20Q",
	};

	if(strstr(manufacture,"SEQUANS"))
		return 1;

	// compare model name
	int i;
	for (i=0;i<sizeof(model_names)/sizeof(const char*);i++)
	{
		if(!strcmp(model_names[i],model_name))
			return 1;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int update_sequans_pdp_status()
{
	// get pdp connections status
	int pdpStat;

	/* get pdp status of the profile */
	pdpStat=getPDPStatus(3);
	if(pdpStat>=0)
		rdb_set_single(rdb_name(RDB_PDP0STAT, ""), pdpStat==0?"down":"up");
	else
		rdb_set_single(rdb_name(RDB_PDP0STAT, ""), "");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_sequans_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	if(!at_is_open())
		return 0;

	update_sim_hint();
	update_roaming_status();
	update_ccid();
	update_sim_status();
	update_configuration_setting(new_status);
	if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS)) {
		update_signal_strength();
	}
	update_network_name();
	update_service_type();
	update_sequans_pdp_status();
	update_imsi();
	update_date_and_time();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int sequans_noti_mode(const char* s)
{
#if (0)
	int token_count = 0, sys_mode = 0, sys_submode = 0;
	const char* t;
	char* str = strdup(s);
	const char *sys_mode_name[] = {"No service", "Reserved", "Reserved,", "GSM/GPRS", "Reserved", "WCDMA",
								   "", "", "", "", "", "", "", "", "",
								   "TD-SCDMA", "FDD-LTE", "TDD-LTE"};
	const char *sys_submode_name[] = {"No service", "GSM", "GPRS", "EDGE", "WCDMA", "HSDPA", "HSUPA",
								   "HSUPA/HSDPA", "TD_SCDMA", "HSPA+", "", "", "", "", "", "", "", "",
								    "", "", "", "", "", "", "","TDD-LTE", "FDD-LTE"};
	SYSLOG_INFO("got noti: '%s'", s);
	if (!str) {
		SYSLOG_ERR("failed to allocate memory in %s()", __func__);
		goto error;
	}
	
	token_count = tokenize_at_response(str);
	if ( (token_count >= 1) && ((t = get_token(str, 1)) != NULL) ) {
		sys_mode = atoi(t);
	}
	if ( (token_count >= 2) && ((t = get_token(str, 2))!=NULL) ) {
		sys_submode = atoi(t);
	}
	SYSLOG_INFO("sys mode [%s], sys submode ]%s]", sys_mode_name[sys_mode], sys_submode_name[sys_submode]);
	rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), sys_submode_name[sys_submode]);

	if (str) free(str);	return 0;
error:		
	if (str) free(str);	return -1;
#else
	SYSLOG_INFO("got noti: '%s'", s);
#endif
	return 1;
}

///////////////////////////////////////////////////////////////////////////////
static int sequans_handle_command_band(const struct name_value_t* args)
{
	if (!args || !args[0].value)
		return -1;

	SYSLOG_INFO("got band command: '%s'", args[0].value);

	/* VZ20Q has LTE band only. */
	if (!strcmp(args[0].value, "get")) {
		rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), "LTE");
		rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
		return 0;
	}
	else if (!strcmp(args[0].value, "set"))
		rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
		return 0;

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static struct {
	char achAPN[128];
	char achUr[128];
	char achPw[128];
	char achAuth[128];
} _profileInfo;
///////////////////////////////////////////////////////////////////////////////
static int getProfileID()
{
	char achPID[64]={0,};
	rdb_get_single(rdb_name(RDB_PROFILE_ID, ""), achPID, sizeof(achPID));

	return atoi(achPID)+1;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileWrite_sequans()
{
	char at_cmd[256];
	char at_resp[AT_RESPONSE_MAX_SIZE];

	memset(&_profileInfo,0,sizeof(_profileInfo));

	// read profile id
	rdb_get_single(rdb_name(RDB_PROFILE_APN, ""), _profileInfo.achAPN, sizeof(_profileInfo.achAPN));
	rdb_get_single(rdb_name(RDB_PROFILE_AUTH, ""), _profileInfo.achAuth, sizeof(_profileInfo.achAuth));
	rdb_get_single(rdb_name(RDB_PROFILE_USER, ""), _profileInfo.achUr, sizeof(_profileInfo.achUr));
	rdb_get_single(rdb_name(RDB_PROFILE_PW, ""), _profileInfo.achPw, sizeof(_profileInfo.achPw));

	// VZ20Q : rb 1 -- connection between network and sequansd
	//         rb 3 -- real connection between network and router
	//int nPID=getProfileID();
	int nPID=3;

	// send profile setting command
	int ok;
	int stat;

	// build profile setting
	sprintf(at_cmd,"AT+CGDCONT=%d,\"IP\",\"%s\"", nPID,_profileInfo.achAPN);
	stat=at_send(at_cmd,at_resp,"",&ok, 0);
	if(stat<0 || !ok)
		goto error;

	// LTE needs reattaching PS - does it upset any exist Sierra dongle?
	syslog(LOG_ERR,"starting LTE procedure");
	
	// enable network registration notification
	syslog(LOG_ERR,"enabling network registration unsolicited messages");
	sprintf(at_cmd,"AT+CREG=1");
	stat=at_send(at_cmd,at_resp,"",&ok, 0);
	if(stat<0 || !ok) {
		SYSLOG_ERR("failed to enable network registration notification - %s failure",at_cmd);
	}

	/* re-attaching network for LTE */
	syslog(LOG_ERR,"re-attaching network by sending CGATT=0 and CGATT=1 - support LTE");

	// LTE 1 - detach network
	sprintf(at_cmd,"AT+CGATT=0");
	stat=at_send(at_cmd,at_resp,"",&ok, 0);
	if(stat<0 || !ok) {
		syslog(LOG_ERR,"AT command failed - %s",at_cmd);
		goto error;
	}

	// LTE 2 - wait for 5 seconds regardless of network registration status
	wait_on_network_reg_stat(-1,5,creg_network_stat);

	// LTE 3 - attach network
	creg_network_stat=0;
	sprintf(at_cmd,"AT+CGATT=1");
	stat=at_send_with_timeout(at_cmd,at_resp,"",&ok, 20, 0);
	if(stat<0 || !ok) {
		syslog(LOG_ERR,"AT command failed - %s",at_cmd);
		goto error;
	}

	// LTE 4 - check CGATT result to see if registered
	char *str;
	char *last_creg=NULL;
	
	// search the last CEREG
	while( (str=strstr(last_creg?last_creg:at_resp,"+CEREG:"))!=0 )
		last_creg=str+sizeof("+CEREG:");
	
	if(last_creg) {
		creg_network_stat=atoi(last_creg);
		syslog(LOG_INFO,"CEREG found in CGATT result - %d",creg_network_stat);
	}

	// LTE 5 - wait until network registered
	if( wait_on_network_reg_stat(1,10,creg_network_stat)<0 ) {
		syslog(LOG_INFO,"network registration failure after AT+CGATT=1");
	}
	else {
		syslog(LOG_INFO,"network registered");
		
		// LTE 6 - wait for network deregistred from 3G
		//if( wait_on_network_reg_stat(0,10)<0 ) {
		//	syslog(LOG_INFO,"LTE not available now");
		//}
		//else {
			syslog(LOG_INFO,"LTE activated");
		//}
	}
		
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[error] at command failed");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int handleProfileActivate_sequans(int act)
{
	// VZ20Q : rb 1 -- connection between network and sequansd
	//         rb 3 -- real connection between network and router
	//int nPID=getProfileID();
	int nPID=3;

	// send activation or deactivation command
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;

	sprintf(achCmd,"AT+CGACT=%d,%d", act, nPID);
	at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY,0);

success:
	rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"1");

	// set command result
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int sequans_handle_command_profile(const struct name_value_t* args)
{
	int stat;
	if (!args || !args[0].value)
		return -1;


	if (!strcmp(args[0].value, "write"))
	{
		return handleProfileWrite_sequans();
	}
	else if (!strcmp(args[0].value, "activate"))
	{
		stat=handleProfileActivate_sequans(1);

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}
	else if (!strcmp(args[0].value, "deactivate"))
	{
		stat=handleProfileActivate_sequans(0);

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}

	return -1;
}


///////////////////////////////////////////////////////////////////////////////
static int sequans_noti_cereg(const char* s)
{
	SYSLOG_INFO("got noti : '%s'", s);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
const struct notification_t sequans_noti_struc[]= {
	{.name="^MODE:",.action=sequans_noti_mode},
	{.name="+CEREG:",.action=sequans_noti_cereg},
	{0,}
};

////////////////////////////////////////////////////////////////////////////////
struct command_t sequans_cmd_struc[] =
{
	{ .name = RDB_BANDCMMAND,		.action = sequans_handle_command_band },
	{ .name = RDB_PROFILE_CMD,		.action = sequans_handle_command_profile },
	{0,}
};

///////////////////////////////////////////////////////////////////////////////
struct model_t model_sequans =
{
	.name = "SEQUANS",

	.init = model_sequans_init,
	.detect = model_sequans_detect,

	.get_status = model_default_get_status,
	.set_status = model_sequans_set_status,

	.commands = sequans_cmd_struc,
	.notifications = sequans_noti_struc
};
