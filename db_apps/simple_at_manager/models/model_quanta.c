
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/times.h>

#include "../rdb_names.h"
#include "../at/at.h"
#include "../featurehash.h"
#include "../util/rdb_util.h"
#include "../util/scheduled.h"
#include "../util/at_util.h"
#include "../model/model.h"

#include "rdb_ops.h"
#include "cdcs_syslog.h"
#include "model_default.h"
#include "../sms/sms.h"

#ifdef PLATFORM_PLATYPUS
#include <nvram.h>
#endif

#ifdef USSD_SUPPORT
#include "../sms/ussd.h"
#endif

// common at command and resp buffers
static char at_cmd[256];
static char at_resp[AT_RESPONSE_MAX_SIZE];

// pdp connection information
static struct {
	char apn[128];
	char user[128];
	char passwd[128];
	char auth[128];
} pdp_conn_info;

// notification info
static int quanta_creg_network_stat=0;
static int quanta_qrmnet_connect=0;
static int quanta_qrmnet_error=0;

// LTE configuration
static int do_lte_procedure=0;

// prefixes
#define QRMNET_RESP_PREFIX	"*QRMNET: "
#define QRMNET_ERR_RESP_PREFIX	"*QRMNET: ERROR, "
#define QRMNET_CONN_TIMEOUT	50
#define QRMNET_ATTACH_TIMEOUT	30
#define QRMNET_ATTACH_TIMEOUT2	50
#define CREG_RESP_PREFIX	"+CREG:"

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

static int set_rmnet_stat(int rmnet_stat)
{
	int stat;
	int ok;

	// build at command
	snprintf(at_cmd,sizeof(at_cmd),"AT*QRMNET=%d",rmnet_stat?1:0);

	// send command
	stat=at_send(at_cmd,at_resp,"",&ok, 0);
	if(stat<0||!ok) {
		SYSLOG_ERR("AT command failure - cmd=%s",at_cmd);
		return -1;
	}

	return 0;
}

static int get_rmnet_stat(int* rmnet_stat)
{
	int stat;
	int ok;
	const char* ptr;

	// send command
	sprintf(at_cmd,"AT*QRMNET?");
	stat=at_send(at_cmd,at_resp,"",&ok, 0);
	if(stat<0||!ok) {
		SYSLOG_ERR("AT command failure - cmd=%s",at_cmd);
		return -1;
	}

	// search the prefix in the at_resp
	if( !(ptr=strstr(at_resp,QRMNET_RESP_PREFIX)) ) {
		SYSLOG_ERR("incorrect AT at_resp found - cmd=%s, resp=%s",at_cmd,at_resp);
		return -1;
	}

	// get rmnet status
	ptr+=sizeof(QRMNET_RESP_PREFIX)-1;
	*rmnet_stat=atoi(ptr);

	return 0;
}


static int wait_on_rmnet_status(int connected, int sec)
{
	struct tms tmsbuf;
	clock_t now=times(&tmsbuf);
	clock_t persec=sysconf(_SC_CLK_TCK);
	clock_t start=now;

	quanta_qrmnet_error=quanta_qrmnet_connect=-1;

	while(1) {
		// read notificiaton
		at_wait_notification(-1);

		now=times(&tmsbuf);
		if( (now-start)/persec>sec ) {
			syslog(LOG_INFO,"waiting timeout");
			break;
		}

		if(quanta_qrmnet_connect!=-1) {
			break;
		}

		if(quanta_qrmnet_error!=-1) {
			syslog(LOG_ERR,"connection error found - %d",quanta_qrmnet_error);
			break;
		}

/*		
		// poll rmnet status
		if( get_rmnet_stat(&rmnet_stat)>=0 ) {
			if( (connected && rmnet_stat) || (!connected && !rmnet_stat) )
				quanta_qrmnet_connect=rmnet_stat;
		}
*/		

		sleep(1);
	}

	return (quanta_qrmnet_connect==0 && !connected) || (quanta_qrmnet_connect==1 && connected);
}

static int get_profile_id()
{
	char achPID[64]={0,};
	rdb_get_single(rdb_name(RDB_PROFILE_ID, ""), achPID, sizeof(achPID));

	return atoi(achPID)+1;
}

static int rdb_cmd_handler_profile_write()
{
	int profile_idx;
	int ok;
	int stat;
	int auth_idx=0;

	memset(&pdp_conn_info,0,sizeof(pdp_conn_info));

	// read profile id
	profile_idx=get_profile_id();
	rdb_get_single(rdb_name(RDB_PROFILE_APN, ""), pdp_conn_info.apn, sizeof(pdp_conn_info.apn));
	rdb_get_single(rdb_name(RDB_PROFILE_AUTH, ""), pdp_conn_info.auth, sizeof(pdp_conn_info.auth));
	rdb_get_single(rdb_name(RDB_PROFILE_USER, ""), pdp_conn_info.user, sizeof(pdp_conn_info.user));
	rdb_get_single(rdb_name(RDB_PROFILE_PW, ""), pdp_conn_info.passwd, sizeof(pdp_conn_info.passwd));
	
	// if authication required
	if(strlen(pdp_conn_info.user) && strlen(pdp_conn_info.passwd))
	{
		if(!strcasecmp(pdp_conn_info.auth,"pap"))
			auth_idx=1;
		else
			auth_idx=2;
	}

	// build qcpdp command
	if(!auth_idx)
		sprintf(at_cmd,"AT$QCPDPP=%d,%d",profile_idx,auth_idx);
	else
		sprintf(at_cmd,"AT$QCPDPP=%d,%d,%s,%s",profile_idx,auth_idx,pdp_conn_info.passwd,pdp_conn_info.user);

	// send command
	stat=at_send(at_cmd,at_resp,"",&ok, 0);
	if(stat<0 || !ok)
		SYSLOG_ERR("failed to set authentication - AT$QCPDPP failure");

	// build profile setting
	sprintf(at_cmd,"AT+CGDCONT=%d,\"IP\",\"%s\"", profile_idx, pdp_conn_info.apn);
	stat=at_send(at_cmd,at_resp,"",&ok, 0);
	if(stat<0 || !ok)
		goto error;

	// LTE needs reattaching PS - does it upset any exist Sierra dongle?
	char conn_type[128];

	conn_type[0]=0;
	rdb_get_single(rdb_name("conn_type", ""), conn_type, sizeof(conn_type));
	if(strcmp(conn_type,"3g")) {
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
		wait_on_network_reg_stat(-1,5,quanta_creg_network_stat);

		// LTE 3 - attach network
		creg_network_stat=0;
		sprintf(at_cmd,"AT+CGATT=1");
		stat=at_send_with_timeout(at_cmd,at_resp,"",&ok, QRMNET_ATTACH_TIMEOUT, 0);
		if(stat<0 || !ok) {
			syslog(LOG_ERR,"AT command failed - %s",at_cmd);
			goto error;
		}

		// LTE 4 - check CGATT result to see if registered
		char *str;
		char *last_creg=NULL;
		
		// search the last CREG
		while( (str=strstr(last_creg?last_creg:at_resp,"+CREG:"))!=0 )
			last_creg=str+sizeof("+CREG:");
		
		if(last_creg) {
			creg_network_stat=atoi(last_creg);
			syslog(LOG_INFO,"CREG found in CGATT result - %d",creg_network_stat);
		}

		// LTE 5 - wait until network registered
		if( wait_on_network_reg_stat(1,10,quanta_creg_network_stat)<0 ) {
			syslog(LOG_INFO,"network registration failure after AT+CGATT=1");
		}
		else {
			syslog(LOG_INFO,"network registered");
			
			// LTE 6 - wait for network deregistred from 3G
			if( wait_on_network_reg_stat(0,10,quanta_creg_network_stat)<0 ) {
				syslog(LOG_INFO,"LTE not available now");
			}
			else {
				syslog(LOG_INFO,"LTE activated");
			}
		}
	}

	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[error] at command failed");
	return -1;
}

static int rdb_cmd_handler_profile_activate(int activate, int no_rdb_result)
{
	// set rmnet stat
	if( set_rmnet_stat(activate)<0 ) {
		SYSLOG_ERR("failed to control rmnet");
		goto err;
	}

	// wait until rmnet is up
	if( wait_on_rmnet_status(activate,QRMNET_CONN_TIMEOUT)<0 ) {
		if(activate)
			SYSLOG_ERR("failed to establish rmnet connection");
		else
			SYSLOG_ERR("failed to disconnect rmnet connection");
		goto err;
	}
       
	if(!no_rdb_result) {
		int rmnet_stat;
		int rmnet_up;

		// set profile up status in rdb
		rmnet_up=(get_rmnet_stat(&rmnet_stat)>=0) && rmnet_stat;
		rdb_set_single(rdb_name(RDB_PROFILE_UP,""),rmnet_up?"1":"0");

		// set command result
		rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	}

	return 0;

err:
	if(!no_rdb_result) {
		rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[error] at command failed");
	}

	return -1;
	
}

static int rdb_cmd_handler_profile(const struct name_value_t* args)
{
	int stat=-1;

	// bypass if no arg is given
	if (!args || !args[0].value)
		return -1;

	if (!strcmp(args[0].value, "write")) {
		stat=rdb_cmd_handler_profile_write();
	} else if (!strcmp(args[0].value, "activate")) {
		int rmnet_stat;
		
		// check previous connection
		if( (get_rmnet_stat(&rmnet_stat)>=0) && rmnet_stat ) {
			SYSLOG_INFO("rmnet is alrady up - disconnecting previous connection");
			rdb_cmd_handler_profile_activate(0,1);
		}

		stat=rdb_cmd_handler_profile_activate(1,0);
	} else if (!strcmp(args[0].value, "deactivate")) {
		stat=rdb_cmd_handler_profile_activate(0,0);
	}

	return stat;
}

#define USE_SIMPLIFIED_LIST
static struct _channel_band
{
  char *number;
  char *bandname;
  char *atRetName;
  int  mode;
} serviceModeTbl[] =
{
#ifdef USE_SIMPLIFIED_LIST
  {"01", "LTE and 3G",	"WL",		35},
  {"02", "LTE Only",	"LTE",		30},
  {"03", "3G Only",	"WCDMA",	14},
  {NULL, NULL,		NULL}
#else
  {"00", "Automatic",	"AUTO",		 4},
  {"01", "CDMA",	"CDMA",		 9},
  {"02", "HDR",		"HDR",		10},
  {"03", "GSM",		"GSM",		13},
  {"04", "WCDMA",	"WCDMA",	14},
  {"05", "LTE",		"LTE",		30},
  {"06", "GWL",		"GWL",		31},
  {"07", "WL",		"WL",		35},
  {NULL, NULL,		NULL}
#endif
};

static int quanta_handleBandGet() {
	/* AT*QCMO=?
		*QCMO:
		(MODE)
		" 4 -- AUTO"
		" 9 -- CDMA"
		"10 -- HDR"
		"13 -- GSM"
		"14 -- WCDMA"
		"30 -- LTE"
		"31 -- GWL"
		"35 -- WL"
		(RESET)
		"0 -- NOT RST"
		"1 -- DO RST"

	   AT*QCMO?
		*QCMO: "AUTO" -- (4)

	   AT*QCMO=["<mode>",[<rst>]]
	*/
	char value[1024], response[AT_RESPONSE_MAX_SIZE];
	char buf[64];
	int stat, ok, loop_cnt;
	char *pos1;

#define ERR_RETURN	{ if ( stat || !ok) goto error; }
#define ERR_RETURN2	{ if ( !pos1 ) goto error; }

	memset(value, 0, sizeof(value));
	
	stat=at_send("AT*QCMO=?", response, "", &ok, 0);
	ERR_RETURN
	pos1 = strstr(response, "*QCMO:");
	ERR_RETURN2
	
	for (loop_cnt =0; serviceModeTbl[loop_cnt].number != NULL ; loop_cnt++) {
		sprintf(buf, "%d -- %s", serviceModeTbl[loop_cnt].mode, serviceModeTbl[loop_cnt].atRetName);
		if(strstr(response, buf) != NULL) {
			strcat(value, serviceModeTbl[loop_cnt].bandname);
			strcat( value, ";");
		}
	}
	if (strlen(value) > 0 )
		memset(value+strlen(value)-1, 0,1);

	rdb_set_single(rdb_name("currentband.current_band", ""), value);
	
	stat=at_send("AT*QCMO?", response, "", &ok, 0);
	ERR_RETURN
	pos1 = strstr(response, "*QCMO:");
	ERR_RETURN2

	memset(buf, 0, sizeof(buf));
	for (loop_cnt =0; serviceModeTbl[loop_cnt].number != NULL ; loop_cnt++) {
		if(strstr(response, serviceModeTbl[loop_cnt].atRetName) != NULL) {
			strcpy(buf, serviceModeTbl[loop_cnt].bandname);
			break;
		}
	}

	SYSLOG_DEBUG("return current band name %s", buf);
	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), buf);
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
	return 0;
 error:
	SYSLOG_DEBUG("return current band name %s", "Autoband");
	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), "Autoband");
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");
	return -1;
}

static int quanta_handleBandSet() {
	char achBandParam[128], achATCmd[128];
	int requestedIdx = 0xff, loop_cnt;
	int ok, stat;

	if (rdb_get_single(rdb_name(RDB_BANDPARAM, ""), achBandParam, sizeof(achBandParam)) != 0)
		goto error;

	for (loop_cnt =0; serviceModeTbl[loop_cnt].number != NULL ; loop_cnt++) {
		if(strstr(achBandParam, serviceModeTbl[loop_cnt].bandname) != NULL) {
			requestedIdx = loop_cnt;
		}
	}

	if (requestedIdx == 0xff)
		goto error;
// Finally Quanta confirm that reset is not necessary for this AT command.
//	sprintf(achATCmd, "AT*QCMO=%d,1", serviceModeTbl[requestedIdx].mode);
	sprintf(achATCmd, "AT*QCMO=%d", serviceModeTbl[requestedIdx].mode);

	stat=at_send_with_timeout(achATCmd,NULL,"",&ok,TIMEOUT_CFUN_SET,0);
	if(stat<0 || !ok)
		goto error;

	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
	return 0;
error:
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");
	return -1;
}

static int quanta_handle_command_band(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}

	if (!strcmp(args[0].value, "get"))
		return quanta_handleBandGet();
	else if (!strcmp(args[0].value, "set"))
		return quanta_handleBandSet();

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int quanta_handleSetOpList()
{
	char achOperSel[64];
	if (rdb_get_single(rdb_name(RDB_PLMNSEL, ""), achOperSel, sizeof(achOperSel)) != 0)
		goto error;

	int iMode;
	int nMNC;
	int nMCC;
	int cCnt;

	// get param
	cCnt = sscanf(achOperSel, "%d,%d,%d", &iMode, &nMNC, &nMCC);
	if (cCnt != 3 && ((cCnt != 1) || (iMode != 0)))
		goto error;

	// get at command network type
	int iAtNw;
#if (defined PLATFORM_PLATYPUS2) || (defined PLATFORM_BOVINE)
	switch (iMode)
	{
		case 1:
			iAtNw = 0; //GSM (2G)
			break;
		case 2:
			iAtNw = 1; //GSM Compact (2G)
			break;
		case 7:
			iAtNw = 2; //LTE (4G)
			break;
		case 3:
			iAtNw = 3; //GSM/EGPRS (2G)
			break;
		case 4:
			iAtNw = 4; //UMTS/HSDPA (3G)
			break;
		case 5:
			iAtNw = 5; //UMTS/HSUPA (3G)
			break;
		case 6:
			iAtNw = 6; //UMTS/HSUPA (3G)
			break;
		case 9:
			iAtNw = 7; //LTE (4G)
			break;
		default:
			iAtNw = 0;
			break;
	}
#else
	if (iMode == 1)
		iAtNw = 2;
	else
		iAtNw = 0;
#endif
	// get at command mode
	int iAtMode;
	if (!iMode)
		iAtMode = 0;
	else
		iAtMode = 1;

	// build command
	char achATCmd[128];
	if (iMode)
		sprintf(achATCmd, "AT+COPS=%d,2,\"%03d%02d\",%d", iAtMode, nMNC, nMCC, iAtNw);
	else
		sprintf(achATCmd, "AT+COPS=0");

	// send at command
	char achATRes[AT_RESPONSE_MAX_SIZE];
	int stat;
	int ok = 0;
	stat = at_send_with_timeout(achATCmd, achATRes, "", &ok, AT_COP_TIMEOUT, 0);
	if (stat || !ok)
		goto error;

	// back to numeric
	stat = at_send_with_timeout("AT+COPS=3,0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 0);

	// update network
	handleUpdateNetworkStat(0);
	//sleep(3);
	//handle_network_scan(NULL);

	/* 
		by request, this work-around is applied for Quanta - change the band frequency to LTE and WCDMA.
		otherwise, the module goes to AUTO that scans GSM
	*/
	
	if(!iMode) {
		if( at_send_with_timeout("AT*QCMO=35",NULL,"",&ok,TIMEOUT_CFUN_SET,0)<0 ) {
			SYSLOG_ERR("AT command failed - AT*QCMO=35");
		}
	}
	
	/*
		another workaround for Quanta module - attach to network. Otherwise, Quanta module does not connect to any network!
	*/
	SYSLOG_INFO("attaching to network after setting PLMN...");
	if( at_send_with_timeout("AT+CGATT=1",NULL,"",&ok,QRMNET_ATTACH_TIMEOUT2,0)<0 ) {
		SYSLOG_ERR("AT command failed - AT+CGATT=1");
	}
	
	
	// set result
	rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "[DONE]");
	return 0;

error:
		rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "error");
	return -1;
}

static int quanta_handle_command_setoplist(const struct name_value_t* args)
{
	int stat;
	
	if (!args || !args[0].value)
	{
		return -1;
	}

	if (!strcmp(args[0].value, "1"))
		return handleGetOpList();
	else if (!strcmp(args[0].value, "5")) {
		stat=quanta_handleSetOpList();
		return stat;
	}

	return -1;
}


///////////////////////////////////////////////////////////////////////////////
#define RDB_BAND_SEL_MODE_INIT		"band_selection_mode_initialized"
static int initialize_service_mode(void)
{
	int stat, ok = 0;
	char achATRes[16];
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
	nvram_init(RT2860_NVRAM);
	rd_data = nvram_bufget(RT2860_NVRAM, RDB_BAND_SEL_MODE_INIT);
	if (!rd_data)
		goto ret;
#else
	char rd_data[10] = {0, };
	if (rdb_get_single(RDB_BAND_SEL_MODE_INIT, rd_data, 10) != 0)
		goto ret;
#endif
	if (strlen(rd_data) == 0)
		goto ret;
	if (strcmp(rd_data, "0") == 0) {
		int i;
		
		SYSLOG_ERR("setting PLMN to auto mode");
		snprintf(achATRes,sizeof(achATRes),"AT+COPS=0");

		SYSLOG_ERR("setting PLMN to Automatic mode");
		
		i=0;
		while(i++<5) {
			// send command
			stat=at_send(achATRes,at_resp,"",&ok, 0);
			if(stat<0||!ok) {
				SYSLOG_ERR("AT command failure - cmd=%s",achATRes);
				sleep(3);
				continue;
			}
			
			break;
		}
		
		SYSLOG_ERR("Service Mode Initialized to Automatic mode");
		snprintf(achATRes,sizeof(achATRes),"AT*QCMO=35");

		// send command
		stat=at_send(achATRes,at_resp,"",&ok, 0);
		if(stat<0||!ok) {
			SYSLOG_ERR("AT command failure - cmd=%s",achATRes);
		}
	


#if defined(PLATFORM_PLATYPUS)
		nvram_set(RT2860_NVRAM, RDB_BAND_SEL_MODE_INIT, "1");
#else
		rdb_set_single(RDB_BAND_SEL_MODE_INIT, "1");
#endif
	}
ret:
#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(rd_data);
	nvram_close(RT2860_NVRAM);
#endif
	return 0;
}

static int quanta_init(void)
{
	char conn_type[128];

	// reset pdp connection info
	memset(&pdp_conn_info,0,sizeof(pdp_conn_info));

	// check lte procedure flag
	rdb_get_single(rdb_name("conn_type", ""), conn_type, sizeof(conn_type));
	do_lte_procedure=strcmp(conn_type,"3g");

	initialize_service_mode();  // This function is added due to "Defect #4463".

	rdb_set_single("service.tcp2diag.trigger", "1");  // to trigger tcp2diag bridge
#if 0
	// init at command
	snprintf(at_cmd,sizeof(at_cmd),"AT*QCMO=4");

	// send command
	stat=at_send(at_cmd,at_resp,"",&ok, 0);
	if(stat<0||!ok) {
		SYSLOG_ERR("AT command failure - cmd=%s",at_cmd);
	}
#endif
	return 0;
}

static int update_quanta_current_band(void)
{
	int ok = 0, token_count=0, i, len, protocol = 0;
	const char * token;
	char response[AT_RESPONSE_MAX_SIZE];
	char * service_type;
	int arfcn = 0;
/*
	Example)
	AT*QRFINFO?
	*QRFINFO: "WCDMA",1,10588,0,193,-95,0,2.5
	*QRFINFO: <mode>, <band>, <channel>, <bandwidth>, <cell_id>, <rx_power>, <rx_quality>, <sinr>

*/
	if (at_send_with_timeout("AT*QRFINFO?", response, "*QRFINFO: ", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0 || !ok)
	{
		goto error;
	}

	token_count = tokenize_at_response(response);

	if (token_count != 9)
		goto error;

	token = get_token(response, 1);

	if (!strcmp(token, "\"GSM\""))
		protocol = 1;
	else if (!strcmp(token, "\"WCDMA\""))
		protocol = 2;
	else if (!strcmp(token, "\"LTE\""))
		protocol = 3;
	else
		goto error;
	
	token = get_token(response, 3);

	len = strlen(token);
	for (i =0 ; i < len; i++) {
		if (!isdigit(token[i])) {
			SYSLOG_ERR("Error: token is not digit");
			goto error;
		}
	}

	arfcn = atoi(token);

	service_type = convert_Arfcn2BandType(protocol, arfcn);

	rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), service_type);
	return 0;
error:
	rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
	return -1;
}

int update_quanta_pdp_status()
{
	int rmnet_stat;
	int rmnet_up;

	// set profile up status in rdb - TODO: there should be a better AT command to check PDP session status
	rmnet_up=(get_rmnet_stat(&rmnet_stat)>=0) && rmnet_stat;
	rdb_set_single(rdb_name(RDB_PDP0STAT, ""),rmnet_up?"up":"down");

	return 0;
}

int update_quanta_pin_retry_number()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[8];
	char* pos;

	// get SIM PIN retry number
	if (at_send("AT$QPVRF=\"P1\"", response, "$QPVRF:", &ok, 0) != 0)
		return 0;
	if (ok) {
		pos=strstr(response, "$QPVRF:");
		if(pos) {
			sprintf(buf, "%u", atoi(response+7));
			rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), buf);
			rdb_set_single(rdb_name(RDB_SIMPUKREMAIN, ""), buf);
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int quanta_update_signal_strength(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	int value;

	if (at_send("AT*QRXPWR?", response, "*QRXPWR:", &ok, 0) != 0 || !ok)
		return -1;

	value = atoi(response + STRLEN("*QRXPWR: \""));
	sprintf(response, "%ddBm",  value);

	return rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), response);
}

int quanta_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	if(!at_is_open())
		return 0;

	if(new_status->status[model_status_sim_ready]) {
		update_sim_hint();
		update_roaming_status();
	}

	/* skip if the variable is updated by other port manager such as cnsmgr or
	 * process SIM operation if module supports +CPINC command */
	if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS) || support_pin_counter_at_cmd) {

		if(new_status->status[model_status_sim_ready]) {
			update_ccid();
		}

		update_sim_status();
		/* Latest Sierra modems like MC8704, MC8801 etcs returns error for +CSQ command
		 * when SIM card is locked so update signal strength with cnsmgr. */
		if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS)) {
			quanta_update_signal_strength();
		}
		update_quanta_pin_retry_number();
		update_date_and_time();
		
		if(new_status->status[model_status_sim_ready]) {
			update_imsi();
			update_network_name();
			update_service_type();
			update_quanta_current_band();
		}

		update_quanta_pdp_status();
	}

	return 0;
}

static int quanta_detect(const char* manufacture, const char* model_name)
{
	return strstr(manufacture,"QUANTA")!=0;
}

static int quanta_noti_qrmnet(const char* str)
{
	const char* ptr;

	// if error
	if( (ptr=strstr(str,QRMNET_ERR_RESP_PREFIX)) != 0 ) {
		quanta_qrmnet_error=atoi(ptr+sizeof(QRMNET_ERR_RESP_PREFIX)-1);

		SYSLOG_INFO("qrmnet error code - %d (%s)",quanta_qrmnet_error,ptr);
	}
	// if connect status
	else if ( (ptr=strstr(str,QRMNET_RESP_PREFIX)) != 0 ) {
		quanta_qrmnet_connect=(!strncmp(ptr+sizeof(QRMNET_RESP_PREFIX)-1,"CONNECTED",sizeof("CONNECTED"))) ?1:0;

		SYSLOG_INFO("qrmnet connect status changed - %d (%s)",quanta_qrmnet_connect,ptr);
	}
	else {
		SYSLOG_ERR("incorrect qrmnet format found - noti=%s",str);
		return -1;
	}

	return 0;
}

static int quanta_noti_creg(const char* str)
{
	char* ptr;

	// check prefix
	if(!(ptr=strstr(str,CREG_RESP_PREFIX))) {
		SYSLOG_ERR("CREG in incorrect format - %s",str);
		return -1;
	}

	// get stat
	quanta_creg_network_stat=atoi(ptr+sizeof(CREG_RESP_PREFIX)-1);
	SYSLOG_INFO("CREG changed - %d (%s)",quanta_creg_network_stat,ptr);

	return 0;
}

// rdb command handlers
struct command_t quanta_commands[] = {
	{ .name=RDB_PROFILE_CMD,	.action=rdb_cmd_handler_profile },
	{ .name=RDB_BANDCMMAND,		.action=quanta_handle_command_band },
	{ .name=RDB_SIMCMMAND,		.action=default_handle_command_sim },
	{ .name=RDB_SMS_CMD,		.action=default_handle_command_sms },
	{ .name = RDB_PLMNCOMMAND,      .action=quanta_handle_command_setoplist },

	{0,}
};

// AT command notification handler
const struct notification_t quanta_notifications[] = {
	{ .name = "*QRMNET:",    .action = quanta_noti_qrmnet },
	{ .name = "+CREG:",      .action = quanta_noti_creg },
 
	#ifdef USSD_SUPPORT
	{ .name = "+CUSD:",   .action = handle_ussd_notification },
	#endif
	{0, }
};

// quanta module 
struct model_t model_quanta = {
	.name          = "quanta",
	.detect        = quanta_detect,
	.init          = quanta_init,
	.get_status    = model_default_get_status,
	.set_status    = quanta_set_status,
	.commands      = quanta_commands,
	.notifications = quanta_notifications
};
