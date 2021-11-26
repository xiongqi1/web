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
#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "../util/rdb_util.h"

#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../util/scheduled.h"

#include "model_default.h"

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

extern struct model_t model_default;

///////////////////////////////////////////////////////////////////////////////
static int cdma_handle_command_band(const struct name_value_t* args)
{
	// not implemented
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int cdma_default_init(void)
{
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int cdma_default_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
     char* resp;
	char* stat_str;
	int nw_stat;


	memset(new_status,0,sizeof(*new_status));

	if(status_needed->status[model_status_sim_ready])
	{
		// assume SIM is always okay for CDMA 
		new_status->status[model_status_sim_ready]=!0;
		err_status->status[model_status_registered]=0;
	}

	if(status_needed->status[model_status_registered])
	{
		if( (resp=at_bufsend("AT+CSS?",""))!=0 )
		{
			stat_str = strtok(resp, ",");

			if(strcmp(stat_str, "?") !=0 && strcmp(stat_str, "0") !=0) {
			   new_status->status[model_status_registered]= !0;
			   err_status->status[model_status_registered]=0;
			}
		}
	}

	if(status_needed->status[model_status_attached] && new_status->status[model_status_registered] != 0)
	{
		if( (resp=at_bufsend("AT+CSQ?",""))!=0 )
		{
			stat_str = strtok(resp, ",");

			nw_stat=99;

			if(stat_str)
				nw_stat=atoi(stat_str);

			if(nw_stat > 0 && nw_stat <= 31) {
			   new_status->status[model_status_attached]= !0;
			   err_status->status[model_status_attached]=0;
			}
		}

	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int cdma_update_network_sysinfo()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	int srv_status=0;
	int srv_domain=0;

	char msg[128];

	char* status_str[]={
		"No services",	// 0
		"limited services", // 1
		"service valid", // 2
		"limited region service", // 3
		"energy-saving deep sleep" // 4
	};

	char* domain_str[]={
		"No Services",	// 0
		"CS only",	// 1
		"PS only",	// 2
		"PS + CS",	// 3
		"CS, PS are not registered and is in searching state", // 4
		"CDMA not supported." // 255
	};

	if (at_send("AT^SYSINFO", response, "^SYSINFO", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("^SYSINFO:"))
		return -1;

	value = response + STRLEN("^SYSINFO:");
	
/*

	# format
	^SYSINFO:<srv_status>,<srv_domain>,<roam_status>,<sys_mode>,<sim_state>[,<lock_state>,<sys_submode>]

	# online
	^SYSINFO:2,255,1,2,240

	# offline
	^SYSINFO:0,255,1,6,240

*/

	// srv_status
	value=_getFirstToken(value,",");
	if(value)
		srv_status=atoi(value);

	// srv_domain
	value=_getNextToken();
	if(value)
		srv_domain=atoi(value);

	if(!srv_status)
	{
		strcpy(msg,status_str[0]);
	}
	else
	{
		if(srv_status<0 || srv_status>4)
			srv_status=0;

		if(srv_domain==255)
			srv_domain=5;
		if(srv_domain<0 || srv_domain>5)
			srv_domain=0;
		
		sprintf(msg,"%s%s",status_str[srv_status],domain_str[srv_domain]);
	}

	rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), msg);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int cdma_update_network_netpar(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	char outmsg[64];

	if (at_send("AT+NETPAR=0", response, "+NETPAR", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("+NETPAR:"))
		return -1;

	value = response + STRLEN("+NETPAR:");
	
/*
	# format
	+NETPAR:<BS_ID>,<BS_PRev>,<P_Rev_in_use>,<channel>,<PN>,<SID>,<NID>,<slot cycle indes>,<rssi>,<Ec/Io>,<Tx power>,<Tx Adj>

	# online
	+NETPAR:20930,6,3,1025,280,16420,33,2,-84,26,-150,0
	
	# offline
	+NETPAR:0,0,3,111,0,0,0,0,-106,0,-150,0

*/

	// BS_ID
	value=_getFirstToken(value,",'");
	if(!value)
		return -1;

	// BS_PRev
	value=_getNextToken();
	if(!value)
		return -1;

	// P_Rev_in_use
	value=_getNextToken();
	if(!value)
		return -1;
	
	// channel
	value=_getNextToken();
	if(!value)
		return -1;

	// PN
	value=_getNextToken();
	if(!value)
		return -1;

	// SID
	value=_getNextToken();
	if(!value)
		return -1;

	// NID
	value=_getNextToken();
	if(!value)
		return -1;

	// slot cycle indes
	value=_getNextToken();
	if(!value)
		return -1;

	// rssi
	value=_getNextToken();
	if(!value)
		return -1;

	// Ec/Io
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_ECIOS0, ""), "");
	}
	else
	{
		sprintf(outmsg,"%.2f",atoi(value)*0.5);
		rdb_set_single(rdb_name(RDB_ECIOS0, ""),outmsg);
	}

	// Tx power
	value=_getNextToken();
	if(!value)
		return -1;

	// Tx Adj
	value=_getNextToken();
	if(!value)
		return -1;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int cdma_update_network_info1(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	if (at_send("AT+EVCSQ1", response, "+EVCSQ1", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("+EVCSQ1:"))
		return -1;

	value = response + STRLEN("+EVCSQ1:");
	
/*
	# format
	+EVCSQ1:<RxAGC0>,<RxAGC1>,<C/I>,<SINR>,<TxPower>,<TxPilotPower>,<TxOpenLoopPower>,<TxClosedloopadjust>,<DRCCover>,<DRCRate>,<ActiveCount>

	# online
	+EVCSQ1:-73.95,-256.00,-256.00,-256.00,-256.00,-256.00,-4.83,0.00,1,'0',1

	# offline
	+EVCSQ1:-113.36,-256.00,-256.00,-256.00,-256.00,-256.00,106.74,0.00,0,'0',0
*/

	// rxagc0
	value=_getFirstToken(value,",'");
	if(!value)
		return -1;

	// rxagc1
	value=_getNextToken();
	if(!value)
		return -1;

	// c/i
	value=_getNextToken();
	if(!value)
		return -1;

	// sinr
	value=_getNextToken();
	if(!value)
		return -1;

	// txpower
	value=_getNextToken();
	if(!value)
		return -1;

	// txpilotpower
	value=_getNextToken();
	if(!value)
		return -1;

	// TxOpenLoopPower
	value=_getNextToken();
	if(!value)
		return -1;

	// TxClosedloopadjust
	value=_getNextToken();
	if(!value)
		return -1;

	// DRCCover
	value=_getNextToken();
	if(!value)
		return -1;

	// DRCRate
	value=_getNextToken();
	if(!value)
		return -1;

	// ActiveCount
	value=_getNextToken();
	if(!value)
		return -1;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int cdma_update_network_creg(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	if (at_send("AT+CREG?", response, "+CREG", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("+CREG:"))
		return -1;

	value = response + STRLEN("+CREG:");
	
/*
	# format
	+CREG: <n>,<stat>[,<lac>,<ci>]

	# online
	+CREG:0,16422,102,4

	# offline
	+CREG:1,0,0,0
*/

	// <n>
	value=_getFirstToken(value,",");
	if(!value)
		return -1;

	// <stat>
	value=_getFirstToken(value,",");
	if(!value)
		return -1;

	// <lac>
	value=_getFirstToken(value,",");
	if(!value)
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC",""), "");
	else
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC",""), value);

	// <ci>
	value=_getFirstToken(value,",");
	if(!value)
		return -1;


	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int cdma_update_network_info2(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	const char* evRev;
	const char* freqStr;

	const char* bandStr;

	const char* pnStr;
	int pn;


	if (at_send("AT+EVCSQ2", response, "+EVCSQ2", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("+EVCSQ2:"))
		return -1;

	value = response + STRLEN("+EVCSQ2:");
	
/*
	# format
	+EVCSQ2:<EV Revision>,<Frequency>,<Band>,<PN>,<Sector ID>,<Subnet Mask>,<Color-Code>,<UATI>,<Pilot Inc>,<Active Set Window>,<Remain Set Window>,<Neighbor Set Window>,<EV Sector User Served>

	# online
	+EVCSQ2:RevA,468,0,36,0x0090000000000000000A5A64030008B3,0x64,0xbf,0xc0cc5801,4,60,100,100,15

	# offline
	+EVCSQ2:Rev0,0,0,0,0x00000000000000000000000000000000,0x0,0x0,0x0,4,60,100,100,0
*/

	evRev=_getFirstToken(value,",");
	if(!evRev)
		return -1;


	freqStr=_getNextToken();
	if(!freqStr || !atoi(freqStr))
		rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
	else
		rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), freqStr);

	bandStr=_getNextToken();
	if(!bandStr || !atoi(bandStr))
		rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "");
	else
		rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), bandStr);


	pnStr=_getNextToken();
	if(!bandStr)
		return -1;
	pn=atoi(pnStr);
	
	// sector id
	_getNextToken();
	// subnet mask
	_getNextToken();
	// color code
	_getNextToken();
	// UATI
	_getNextToken();
	// Poilot inc
	_getNextToken();
	// active set window
	_getNextToken();
	// neightbour set window
	_getNextToken();
	// EV Sector User Served
	_getNextToken();

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int cdma_update_signal_strength(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	int value;

	// CSQ
	if (at_send("AT+CSQ", response, "+CSQ", &ok, 0) != 0 || !ok)
		return -1;

	value = atoi(response + STRLEN("+CSQ:"));
	sprintf(response, "%ddBm",  value < 0 || value == 99 ? 0 : (value * 2 - 113));
	return rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), response);
}

////////////////////////////////////////////////////////////////////////////////
int cdma_update_imei(void)
{
	int ok = 0;
	char response[ AT_RESPONSE_MAX_SIZE ];
	char* value;
	

	if (at_send("AT+GSN", response, "+GSN", &ok, 0) != 0 || !ok)
	{
		SYSLOG_DEBUG("'%s' not available and scheduled", "AT+GSN");
		return -1;
	}

	if(strlen(response)<STRLEN("+GSN:"))
	{
		SYSLOG_DEBUG("'%s' incorrect result", "AT+GSN");
		return -1;
	}


	value = response + STRLEN("+GSN:");

	if (rdb_set_single(rdb_name(RDB_IMEI, ""), value) < 0)
		SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(RDB_IMSI".msin", ""), value, strerror(errno));

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
void cdma_update_sim()
{
	rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM OK");
}

////////////////////////////////////////////////////////////////////////////////
int cdma_default_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{

//	cdma_update_network_netpar();
//	cdma_update_network_info1();
//	cdma_update_network_info2();
	cdma_update_network_creg();

	cdma_update_signal_strength();

	cdma_update_network_sysinfo();

	cdma_update_imei();

	cdma_update_sim();


/*
	update_network_name();
	update_service_type();
*/

	return 0;
}

char *strcasestr(const char *haystack, const char *needle);

////////////////////////////////////////////////////////////////////////////////
static int cdma_default_detect(const char* manufacture, const char* model_name)
{
   int loop_cnt;

   struct manufacture_model_pair
   {
    char *manufacture;
    char *model;
   } manufacturetbl[] =
   {
    {"Personal Communications Devices", "UM185C"}, //Pantech UM185C
    {"Cal-Comp Electronics and Communications Company Limited", "A600"}, //Cal-Comp A600

    {NULL, NULL}
   };

   for(loop_cnt=0 ; manufacturetbl[loop_cnt].manufacture != NULL; loop_cnt++)
   {
      if (strstr(manufacture, manufacturetbl[loop_cnt].manufacture) && strstr(model_name,manufacturetbl[loop_cnt].model)) {
         return 1;
      }
   }

   return 0;

}

////////////////////////////////////////////////////////////////////////////////
struct command_t cdma_default_commands[] =
{
	{ .name = RDB_BANDCMMAND,		.action = cdma_handle_command_band },

	{0,}
};

////////////////////////////////////////////////////////////////////////////////
struct model_t model_cdma_default = {
	.name = "cdma_default",
	.init = cdma_default_init,
	.detect = cdma_default_detect,

	.get_status = cdma_default_get_status,
	.set_status = cdma_default_set_status,

	.commands = cdma_default_commands,
	.notifications = NULL
};

////////////////////////////////////////////////////////////////////////////////
 
