/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <alloca.h>
#ifdef PLATFORM_PLATYPUS
#include <nvram.h>
#endif

#include "cdcs_syslog.h"

#include "rdb_ops.h"
#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../util/at_util.h"
#include "../util/rdb_util.h"
#include "../util/scheduled.h"
#include "../util/cfg_util.h"
#include "model_default.h"

#include "../dyna.h"
#include "../sms/pdu.h"
#include "suppl.h"

#include <sys/times.h>

#include <fcntl.h>
#include <dirent.h>

///////////////////////////////////////////////////////////////////////////////
static struct {
	char achAPN[128];
	char achUr[128];
	char achPw[128];
	char achAuth[128];
} _profileInfo;
///////////////////////////////////////////////////////////////////////////////
extern int update_pdp_status();

///////////////////////////////////////////////////////////////////////////////
static int model_ZTE3805_init(void)
{
	char firmware_version[AT_RESPONSE_MAX_SIZE];
	int i;

/*
Modem mode
R1.5.3.3
EAGLE_R1.5.3.3
P4 rev: CL311710 with 0 local change(s).
*/

	rdb_get_single(rdb_name(RDB_FIRMWARE_VERSION, ""), firmware_version, sizeof(firmware_version));

	if(strstr(firmware_version, "\n") != NULL ) {
		rdb_set_single(rdb_name(RDB_FIRMWARE_VERSION, ""), "N/A");

		for( i=0; firmware_version[i] != '\0'; i++) {
			if(firmware_version[i] == '\n')
				firmware_version[i] = ',';
		}

		rdb_set_single(rdb_name(RDB_FIRMWARE_VERSION, ""), firmware_version);
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
static int model_ZTE3805_detect(const char* manufacture, const char* model_name)
{
	const char* model_names[]={
		"K3805-Z",
	};

	if(!strstr(manufacture,"Vodafone (ZTE)"))
		return 0;

	// compare model name
	int i;
	for (i=0;i<sizeof(model_names)/sizeof(const char*);i++)
	{
		if(!strcmp(model_names[i],model_name)) {
			return 1;
		}
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int model_ZTE3805_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	if(!at_is_open())
		return 0;

	// update ccid
	update_ccid();
	
	// update hint
	update_sim_hint();
	
	update_sim_status();

	update_roaming_status();

	update_signal_strength();

	update_imsi();

	update_network_name();
	update_service_type();

	update_pdp_status();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int getProfileID()
{
	char achPID[64]={0,};
	rdb_get_single(rdb_name(RDB_PROFILE_ID, ""), achPID, sizeof(achPID));

	return atoi(achPID)+1;
}

///////////////////////////////////////////////////////////////////////////////
static int handleProfileWrite_ZTE3805t()
{
	memset(&_profileInfo,0,sizeof(_profileInfo));

	// read profile id
	rdb_get_single(rdb_name(RDB_PROFILE_APN, ""), _profileInfo.achAPN, sizeof(_profileInfo.achAPN));
	rdb_get_single(rdb_name(RDB_PROFILE_AUTH, ""), _profileInfo.achAuth, sizeof(_profileInfo.achAuth));
	rdb_get_single(rdb_name(RDB_PROFILE_USER, ""), _profileInfo.achUr, sizeof(_profileInfo.achUr));
	rdb_get_single(rdb_name(RDB_PROFILE_PW, ""), _profileInfo.achPw, sizeof(_profileInfo.achPw));

	int nPID=getProfileID();

	// send profile setting command
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;
	int stat;

	int iAuth=0;

	// if authication required
	if(strlen(_profileInfo.achUr) && strlen(_profileInfo.achPw))
	{
		if(!strcasecmp(_profileInfo.achAuth,"pap"))
			iAuth=1;
		else
			iAuth=2;
	}

	// build qcpdp command
	if(!iAuth)
		sprintf(achCmd,"AT%%IPDPCFG=%d,0,%d",nPID,iAuth);
	else
		sprintf(achCmd,"AT%%IPDPCFG=%d,0,%d,\"%s\",\"%s\"",nPID,iAuth,_profileInfo.achUr,_profileInfo.achPw);

	// send command
	stat=at_send(achCmd,response,"",&ok, 0);
	if(stat<0 || !ok)
		SYSLOG_ERR("failed to set authentication - AT%%IPDPCFG failure");

	// build profile setting
	sprintf(achCmd,"AT+CGDCONT=%d,\"IP\",\"%s\"", nPID,_profileInfo.achAPN);
	stat=at_send(achCmd,response,"",&ok, 0);
	if(stat<0 || !ok)
		goto error;

	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[error] at command failed");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int getProfileStat_ZTE3805(int nPID)
{
	int stat, ok, call_state = 0;
	char achCmd[128];
	char response[AT_RESPONSE_MAX_SIZE];
	char * pos1;
	const char *tokenp;

	stat=at_send("AT%IPDPACT?",response,"%IPDPACT",&ok, 0);
	if(stat<0 || !ok)
		return 0;

	sprintf(achCmd, "%%IPDPACT: %d,",nPID);
	pos1 = strstr(response, achCmd);

	if(!pos1)
		return 0;

	tokenize_at_response(pos1);

	tokenp = get_token(pos1, 1);
	if(!tokenp)
	   goto error;
	
	call_state = atoi(tokenp);

	return call_state;

error:
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileDeActivate_ZTE3805()
{
	int nPID=getProfileID();

	// send activation or deactivation command
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;

	sprintf(achCmd,"AT%%IPDPACT=%d,0", nPID);
	at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

	rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"0");

	// set command result
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileActivate_ZTE3805()
{

	int nPID=getProfileID();

	int fUp, loop_cnt = 0;

	// send activation or deactivation command
	char achCmd[128];
	char response[AT_RESPONSE_MAX_SIZE];
	int ok, stat;

	sprintf(achCmd,"AT%%IPDPACT=%d,1", nPID);
	stat=at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

	do {
		sleep(1);
		fUp=getProfileStat_ZTE3805(nPID);
		loop_cnt++;
	}while((loop_cnt <7) && (fUp == 2));

	if((fUp == 2) || (fUp == 0))
		goto error;

	if (fUp == 1)
		rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"1");
	else
		rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"0");

	// set command result
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;

error:
	handleProfileDeActivate_ZTE3805();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int zte3805_handle_command_profile(const struct name_value_t* args)
{
	int stat;
	if (!args || !args[0].value)
		return -1;


	if (!strcmp(args[0].value, "write"))
	{
		return handleProfileWrite_ZTE3805t();
	}
	else if (!strcmp(args[0].value, "activate"))
	{
		stat=handleProfileActivate_ZTE3805();

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}
	else if (!strcmp(args[0].value, "deactivate"))
	{
		stat=handleProfileDeActivate_ZTE3805();

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
struct command_t ZTE3805_commands[] =
{
	{ .name = RDB_PROFILE_CMD,		.action = zte3805_handle_command_profile },

	{0,}
};

///////////////////////////////////////////////////////////////////////////////
struct model_t model_ZTE_k3805z =
{
	.name = "ZTE_k3805z",

	.init = model_ZTE3805_init,
	.detect = model_ZTE3805_detect,

	.get_status = model_default_get_status,
	.set_status = model_ZTE3805_set_status,

	.commands = ZTE3805_commands,
	.notifications = NULL
};

