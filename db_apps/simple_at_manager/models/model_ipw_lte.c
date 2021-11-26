
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

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

static struct {
	char achAPN[128];
	char achUr[128];
	char achPw[128];
	char achAuth[128];
} _profileInfo;

///////////////////////////////////////////////////////////////////////////////
extern int readSIMFile(int iFileID, void* pBuf, int cbBuf);
extern int __convNibbleHexToInt(char chNibble);
extern void __swapNibbles(char* pArray, int cbArray);
extern int __convHexArrayToStr(char* pHex, int cbHex, char* achBuf, int cbBuf);
///////////////////////////////////////////////////////////////////////////////
#if 0
static int getProfileID()
{
	char achPID[64]={0,};
	rdb_get_single(rdb_name(RDB_PROFILE_ID, ""), achPID, sizeof(achPID));

	return atoi(achPID)+1;
}
#endif
///////////////////////////////////////////////////////////////////////////////
static int handleProfileWrite_ipwlte()
{
	memset(&_profileInfo,0,sizeof(_profileInfo));

	// read profile id
	rdb_get_single(rdb_name(RDB_PROFILE_APN, ""), _profileInfo.achAPN, sizeof(_profileInfo.achAPN));
	rdb_get_single(rdb_name(RDB_PROFILE_AUTH, ""), _profileInfo.achAuth, sizeof(_profileInfo.achAuth));
	rdb_get_single(rdb_name(RDB_PROFILE_USER, ""), _profileInfo.achUr, sizeof(_profileInfo.achUr));
	rdb_get_single(rdb_name(RDB_PROFILE_PW, ""), _profileInfo.achPw, sizeof(_profileInfo.achPw));

#if 0
	int nPID=getProfileID();
#endif

	// send profile setting command
//	char achCmd[512];
//	char response[1024];
//	int ok;
//	int stat;
//	char *pos1;

//	char *iAuth;

#if 0 //ToDo later
	// if authication required
	if(strlen(_profileInfo.achUr) && strlen(_profileInfo.achPw))
	{
		if(!strcasecmp(_profileInfo.achAuth,"pap"))
			iAuth= "00010";
		else
			iAuth= "00100";
	}
	else
	{
      iAuth= "00111";
	}

   stat=at_send("AT*EIAAUR=0", response, "", &ok, 0);

   if ( !stat && ok)
   {
      pos1=strstr(response, "*EIAAUR: 1,");
      if(!pos1)
      {
		at_send("AT*EIAD=0", response, "", &ok, 0);
		at_send("AT*EIAC=1", response, "", &ok, 0);
      }
   }
   else
   {
      goto error;
   }

   sprintf(achCmd, "AT*EIAAUW=%d,1,%s,%s,%s,0",nPID, _profileInfo.achUr, _profileInfo.achPw, iAuth);

	// send command
	stat=at_send(achCmd,response,"",&ok, 0);
	if(stat<0 || !ok)
		SYSLOG_ERR("failed to set authentication - cmd=%s,res=%s", achCmd, response);
#endif

#if 0 //TEST CODE for IPW LTE DONGLE
	// build profile setting
	sprintf(achCmd,"AT+CGDCONT=%d,\"IP\",\"%s\"", nPID,_profileInfo.achAPN);
	stat=at_send(achCmd,response,"",&ok, 0);
	if(stat<0 || !ok)
		goto error;
#endif
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;

#if 0 //TEST CODE for IPW LTE DONGLE
error:
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[error] at command failed");
	return -1;
#endif
}

///////////////////////////////////////////////////////////////////////////////
static int handleProfileActivate_ipwlte()
{

//   int nPID=getProfileID();

   int ok, stat, return_val, loop_cnt;

   // send activation or deactivation command
//   char achCmd[512];
   char response[AT_RESPONSE_MAX_SIZE];

#if 0 //ToDo later
   stat = at_send_with_timeout("AT*ENAP?",response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

   if ( !stat && ok){
      if(strstr(response, "*ENAP:1"))
         goto success;
   }

   sprintf(achCmd,"AT*ENAP=1,%d", nPID);
   at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

   for (loop_cnt=0;loop_cnt<5 ;loop_cnt++)
   {
      sleep(1);

      stat = at_send_with_timeout("AT*ENAP?",response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

      if ( !stat && ok){
         if(strstr(response, "*ENAP:1"))
            goto success;

         if(strstr(response, "*ENAP:0")) {
            sleep(1);
            //at_send_with_timeout("AT+CGATT=0",response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

            sprintf(achCmd,"AT*ENAP=1,%d", nPID);
            at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);
         }
      }
   }

   handleProfileDeactivate_ericsson();
   return -1;
#endif

#if 1 //TEST CODE for IPW LTE DONGLE
   stat = at_send_with_timeout("AT+CGATT=1", NULL, "", &ok, TIMEOUT_NETWORK_QUERY, 0);

   for (loop_cnt=0;loop_cnt<5 ;loop_cnt++)
   {
      sleep(1);

      stat = at_send_with_timeout("AT+CGATT?",response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

      if ( !stat && ok){
         return_val = atoi(response + STRLEN("+CGATT: "));

         if(return_val == 1)
            goto success;

         if(return_val == 0) {
            sleep(1);

            at_send_with_timeout("AT+CGATT=1", NULL, "", &ok, TIMEOUT_NETWORK_QUERY, 0);
         }
      }
   }
#endif

   rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"0");

   // set command result
   rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");

   return -1;

success:
   rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"1");

   // set command result
   rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
   return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileDeactivate_ipwlte()
{

	// send activation or deactivation command
//	char achCmd[512];
//	char response[AT_RESPONSE_MAX_SIZE];
//	int ok;

#if 0 //ToDo later
	sprintf(achCmd,"AT*ENAP=0");
	at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);
#endif

//	at_send_with_timeout("AT+CGATT=0", NULL, "", &ok, 5, 0);

	rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"0");

	// set command result
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////


static int ipwlte_handle_command_profile(const struct name_value_t* args)
{
	int stat;
	if (!args || !args[0].value)
		return -1;


	if (!strcmp(args[0].value, "write"))
	{
		return handleProfileWrite_ipwlte();
	}
	else if (!strcmp(args[0].value, "activate"))
	{
		stat=handleProfileActivate_ipwlte();

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}
	else if (!strcmp(args[0].value, "deactivate"))
	{
		stat=handleProfileDeactivate_ipwlte();

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int model_ipwlte_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
	char* resp;
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	static const char* service_types[] = { "GSM", "GSM Compact", "UMTS", "EGPRS", "HSDPA", "HSUPA", "HSDPA/HSUPA", "LTE", NULL };

	memset(new_status,0,sizeof(*new_status));

	if(status_needed->status[model_status_sim_ready])
	{
		resp=at_bufsend("AT+CPIN?","+CPIN: ");

		err_status->status[model_status_sim_ready]=!resp;

		/* write sim pin */
		if(resp)
		{
			model_default_write_sim_status(resp);
			new_status->status[model_status_sim_ready]=!strcmp(resp,"READY");
		}
	}

#if 0 //TEST CODE for IPW LTE DONGLE
	if(status_needed->status[model_status_registered])
	{
		char* stat_str;
		int nw_stat;

		if( (resp=at_bufsend("AT+CREG?","+CREG: "))!=0 )
		{
			strtok(resp, ",");
			stat_str=strtok(0, ",");

			nw_stat=-1;
			if(stat_str)
				nw_stat=atoi(stat_str);

			new_status->status[model_status_registered]=(nw_stat==2);
		}

		err_status->status[model_status_registered]=!resp;
	}
#endif

	if((status_needed->status[model_status_registered]) || (status_needed->status[model_status_attached]))
	{
		int nw_stat=0xff, token_count;
		const char *token;
		char * response_val;

		if ((at_send("AT+CEREG?", response, "+CEREG:", &ok, 0) != 0 || !ok) || (strlen(response)<STRLEN("+CEREG:"))) {
			err_status->status[model_status_registered]=1;
			err_status->status[model_status_attached]=1;
		}
		else {
			response_val = response + STRLEN("+CEREG:");

			token_count = tokenize_at_response(response_val);

			if (token_count == 5)
			{
				token = get_token(response_val, 4);

				if(token)
					nw_stat=atoi(token);

				new_status->status[model_status_attached]=(nw_stat==7);
				err_status->status[model_status_attached]=0;

				if(nw_stat>= 0 && nw_stat <= 7) {
					rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), service_types[nw_stat]);
					rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), service_types[nw_stat]);
				}
				else {
					rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "");
					rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
				}
			}

			if (token_count == 2)
			{
				token = get_token(response_val, 1);

				if(token)
					nw_stat=atoi(token);

				new_status->status[model_status_registered]=(nw_stat==2);
				err_status->status[model_status_registered]=0;

				new_status->status[model_status_attached]=0;
				err_status->status[model_status_attached]=0;
				rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "");
				rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
			}
		}


	}

#if 0 //TEST CODE for IPW LTE DONGLE
	if(status_needed->status[model_status_attached])
	{
		if( (resp=at_bufsend("AT+CGATT?","+CGATT: ")) !=0 ) {
			if (atoi(resp) == 0) {
				if (at_send("AT+CSQ", response, "+CSQ", &ok, 0) != 0 || !ok)
				   return -1;

				value = atoi(response + STRLEN("+CSQ: "));

				if (value > 0 && value < 99) {
	               at_send_with_timeout("AT+CGATT=1", NULL, "", &ok, 5, 0);
				}
			}
			new_status->status[model_status_attached]=atoi(resp)!=0;
		}

		err_status->status[model_status_attached]=!resp;
	}
#endif
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int ipwlte_update_network_name(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char network_name[32];
	int token_count;
	const char* t;
	int offset;
	int size;
	int stat;

	// send at command - set long operator
	stat = at_send_with_timeout("AT+COPS=3,0", response, "", &ok, AT_QUICK_COP_TIMEOUT, 0);
	if (stat || !ok) {
		return -1;
	}

	if (at_send_with_timeout("AT+COPS?", response, "+COPS", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0 || !ok)
	{
		return -1;
	}
	network_name[0] = 0;
//printf( "AT+COPS? response=%s\n",response);
	token_count = tokenize_at_response(response);
	if (token_count >= 1)
	{
		t = get_token(response, 1);
		rdb_set_single(rdb_name(RDB_PLMNMODE, ""), t);
	}
	if (token_count >= 4)
	{
		t = get_token(response, 3);
		offset = *t == '"' ? 1 : 0;
		size = strlen(t) - offset * 2;
		size = size < 63 ? size : 63; // quick and dirty
		memcpy(network_name, t + offset, size);
		network_name[size] = 0;
	}
    /* replace "Telstra Mobile" to "Telstra" */
    str_replace(&network_name[0], "Telstra Mobile", "Telstra");
    /* replace "3Telstra" to "Telstra" */
    str_replace(&network_name[0], "3Telstra", "Telstra");
	rdb_set_single(rdb_name(RDB_NETWORKNAME, ""), network_name);
//	rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), service_types[service_type]);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int ipwlte_update_ccid(void)
{
	char achCCID[64];
	int cbCCID;
//	char achATCmd[128];
	char achATRes[AT_RESPONSE_MAX_SIZE];
	int ok, stat;
	char * value;

	static int ccidLoad=0;

	if(ccidLoad)
		return 0;

	// get CCID
	cbCCID = readSIMFile(0x2fe2, achCCID, sizeof(achCCID));//12258

	if(cbCCID == 0) {
		stat = at_send("AT+CRSM=176,12258,00,00,10", achATRes, "", &ok, 0);

		if (stat || !ok)
			return -1;

		if ((value = strchr(achATRes, 0xA))  != NULL) {
			*value = ',';
		}

		const char* pToken = _getFirstToken(achATRes, ",\"");

		pToken = _getNextToken();

		pToken = _getNextToken();
		if (!pToken)
			return 0;

	// convert value to byte array
		unsigned char bOct;
		int iT = 0;
		int cbToken = strlen(pToken);
		int iB = 0;
		while (iT < cbToken && iB < sizeof(achCCID))
		{
			bOct = __convNibbleHexToInt(pToken[iT++]) << 4;

			if (iT < cbToken)
				bOct |= __convNibbleHexToInt(pToken[iT++]);

			achCCID[iB++] = bOct;
		}

		cbCCID = iB;

	}

	if (cbCCID>0) {
		char achStrCCID[64];

		__swapNibbles(achCCID, cbCCID);
		__convHexArrayToStr(achCCID, cbCCID, achStrCCID, sizeof(achStrCCID));

		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), achStrCCID);
		ccidLoad=1;
	}
	else if(cbCCID==0) {
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), "");
		ccidLoad=1;
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int model_ipwlte_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{

	if(!at_is_open())
		return 0;

	ipwlte_update_ccid();

	update_sim_status();

	update_signal_strength();

	update_imsi();

	ipwlte_update_network_name();
//	update_service_type();

//	update_pdp_status();

//	if(new_status->status[model_status_registered])
//		update_call_forwarding();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_ipwlte_init(void)
{

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_ipwlte_detect(const char* manufacture, const char* model_name)
{

	const char* model_names[]={
		"ALT3100",
	};

	if(strstr(manufacture,"ALTAIR-SEMICONDUCTOR"))
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

///////////////////////////////////////////////////////////////////////////////
const struct command_t model_ipwlte_commands[] =
{
	{ .name = RDB_PROFILE_CMD,		.action = ipwlte_handle_command_profile },

	{0,0}
};

///////////////////////////////////////////////////////////////////////////////
struct model_t model_ipwlte =
{
	.name = "IP Wireless LTE",

	.init = model_ipwlte_init,
	.detect = model_ipwlte_detect,

	.get_status = model_ipwlte_get_status,
	.set_status = model_ipwlte_set_status,

	.commands = model_ipwlte_commands,
	.notifications = NULL
};

