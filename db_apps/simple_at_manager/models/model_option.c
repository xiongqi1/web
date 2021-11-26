// 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "../at/at.h"
#include "../model/model.h"
#include "../util/rdb_util.h"
#include "../rdb_names.h"
#include "../util/at_util.h"
#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "model_default.h"


static struct {
	char achAPN[128];
	char achUr[128];
	char achPw[128];
	char achAuth[128];
} _profileInfo;

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

////////////////////////////////////////////////////////////////////////////////
int option_update_hwver()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char* value;

	// module hardware version
	if (at_send("AT_OHWV", response, "_OHWV", &ok, 0) != 0 || !ok)
		return -1;

	value = response + STRLEN("_OHWV:");
	sprintf(response, "%s", value);
	return rdb_set_single(rdb_name(RDB_HARDWARE_VERSION, ""), response);
}

///////////////////////////////////////////////////////////////////////////////
static int model_option_init(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	int token_count, ok = 0;
	const char * value;
	
	at_send("AT+CREG=2", NULL, "", &ok, 0);
	option_update_hwver();
	
	if (at_send("AT+CGSN", response, "", &ok, 0) != 0 || !ok)
	{
		SYSLOG_DEBUG("'%s' not available and scheduled", "AT+CGSN");
		return -1;
	}

	token_count = tokenize_at_response(response);
	
	if (token_count < 1)
		return -1;
	
	value = get_token(response, 0);
	
	rdb_set_single(rdb_name(RDB_IMEI, ""), value);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_option_detect(const char* manufacture, const char* model_name)
{
   int loop_cnt;

   struct manufacture_model_pair
   {
    char *manufacture;
    char *model;
   } manufacturetbl[] =
   {
    {"Option N.V.", "GTM382"}, //Option Wireless GTM382

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
int update_mccmnc(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	int token_count;
	const char* tokenp, * value;
	char * temp_str;
	char mccStr[10];
	char mncStr[10];

	if (at_send("AT_OSIMOP", response, "_OSIMOP", &ok, 0) != 0)
		goto error;

	if(strlen(response)<STRLEN("_OSIMOP:"))
		goto error;

	value = response + STRLEN("_OSIMOP:");

	token_count = tokenize_at_response((char *)value);

	if(token_count != 3)
		goto error;

	tokenp = get_token(value, 2);
	if(!tokenp)
	   goto error;

	if(*tokenp == '"')
		tokenp++;

	if ((temp_str = strstr(tokenp,"\"")) != NULL)
		*temp_str = 0;

	if(strlen(tokenp) <= 3)
		goto error;

	strncpy(mccStr, tokenp + 0, 3);
		mccStr[3] = 0;

	strcpy(mncStr, tokenp + 3);

	rdb_set_single(rdb_name(RDB_IMSI".plmn_mcc", ""), mccStr);
	rdb_set_single(rdb_name(RDB_IMSI".plmn_mnc", ""), mncStr);

	return 0;

error:
	rdb_set_single(rdb_name(RDB_IMSI".plmn_mcc", ""), "");
	rdb_set_single(rdb_name(RDB_IMSI".plmn_mnc", ""), "");

	return -1;
}
////////////////////////////////////////////////////////////////////////////////
int option_update_pdp_status()
{
	// get pdp connections status
	int pdpStat = 0, ok = 0;
	int nwErrRegistration=0, nwErrAttach=0, nwErrActivation=0;
	char response[AT_RESPONSE_MAX_SIZE];
	int token_count;
	const char* tokenp, * value;
	
	if (at_send("AT_OWANNWERROR?", response, "_OWANNWERROR", &ok, 0) != 0)
		goto error;

	if(strlen(response)<STRLEN("_OWANNWERROR:"))
		goto error;

	value = response + STRLEN("_OWANNWERROR:");

	token_count = tokenize_at_response((char *)value);

	if(token_count != 3)
		goto error;

	tokenp = get_token(value, 0);
	if(!tokenp)
	   goto error;

	nwErrRegistration = atoi(tokenp);

	tokenp = get_token(value, 1);
	if(!tokenp)
	   goto error;

	nwErrAttach = atoi(tokenp);

	tokenp = get_token(value, 2);
	if(!tokenp)
	   goto error;

	nwErrActivation = atoi(tokenp);

	if((nwErrRegistration == 300) || (nwErrRegistration != 0) || (nwErrRegistration != 0)) {
		pdpStat = 0;
	}
	else {
		pdpStat = 1;
	}
	
	if(pdpStat>=0)
		rdb_set_single(rdb_name(RDB_PDP0STAT, ""), pdpStat==0?"down":"up");
	
	return 0;

error:

	return -1;
}
////////////////////////////////////////////////////////////////////////////////
int option_update_pdp_status_01()
{
	// get pdp connections status
	int pdpStat = 0, ok = 0, system_status = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	int token_count;
	const char* tokenp, * value;
	
	if (at_send("AT_OSSYS?", response, "_OSSYS", &ok, 0) != 0)
		goto error;

	if(strlen(response)<STRLEN("_OSSYS:"))
		goto error;

	value = response + STRLEN("_OSSYS:");

	token_count = tokenize_at_response((char *)value);

	if(token_count != 2)
		goto error;

	tokenp = get_token(value, 1);
	if(!tokenp)
	   goto error;

	system_status = atoi(tokenp);

	if((system_status == 0) || (system_status == 2)) {
		pdpStat = 1;
	}
	else {
         pdpStat = 0;
	}

	if(pdpStat>=0)
		rdb_set_single(rdb_name(RDB_PDP0STAT, ""), pdpStat==0?"down":"up");
	
	return 0;

error:

	return -1;
}
////////////////////////////////////////////////////////////////////////////////
char *replaceAll(char *s, const char *olds, const char *news) {
  char *sr;
  char *result = alloca(AT_RESPONSE_MAX_SIZE);
  size_t i, count = 0;
  size_t oldlen = strlen(olds); if (oldlen < 1) return s;
  size_t newlen = strlen(news);


  if (newlen != oldlen) {
    for (i = 0; s[i] != '\0';) {
      if (memcmp(&s[i], olds, oldlen) == 0) count++, i += oldlen;
      else i++;
    }
  } else i = strlen(s);

  result[0] = 0;

  sr = result;
  while (*s) {
    if (memcmp(s, olds, oldlen) == 0) {
      memcpy(sr, news, newlen);
      sr += newlen;
      s  += oldlen;
    } else *sr++ = *s++;
  }
  *sr = '\0';

  return result;
}

int option_update_retries_remaining() {
	int ok = 0, stat;
	char response[AT_RESPONSE_MAX_SIZE];

	stat=at_send("AT_OERCN?", response, "_OERCN", &ok, 0);
	if (!stat && ok) {
		rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), response+8);
	}
	
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int option_update_misc()
{
	int ok = 0, stat, token_count;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char *value;
	const char *token;

	stat=at_send("AT+CREG?", response, "+CREG", &ok, 0);
	if (stat || !ok) {
		return -1;
	}

	value = response + STRLEN("+CREG:");

	token_count = tokenize_at_response(value);

	if (token_count < 4)
	   return -1;

	token=get_token(value,2);
	if(token) {
		token = replaceAll((char *)token, "\"", "");
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC", ""), token);
	}

	token=get_token(value,3);
	if(token) {
		token = replaceAll((char *)token, "\"", "");
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), token);
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int option_update_band()
{
	int ok = 0, stat;
	char response[AT_RESPONSE_MAX_SIZE];
	char *pos, *pos2;

	stat=at_send("AT_OPBM?", response, "", &ok, 0);
	if (stat || !ok) {
		return -1;
	}
	pos=strchr(response, ':');
	while( pos ) {
		pos2=strchr(pos, '1');
		if( !pos2 ) {
			pos2=strchr(pos, '0');
			if( !pos2 )
				break;
		}
		*(++pos2)=',';
		pos=strchr(pos2, ':');
	}
	rdb_set_single(rdb_name(RDB_MODULEBANDLIST, ""), response);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int model_option_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	if(!at_is_open())
		return 0;

	update_sim_status();

	update_signal_strength();

//	update_imsi();

	update_mccmnc();
	
	update_network_name();
	update_service_type();

	update_configuration_setting(new_status);

	if(new_status->status[model_status_registered])
		update_call_forwarding();

//	option_update_pdp_status();
	update_pdp_status();
	option_update_misc();
	option_update_band();
	option_update_retries_remaining();
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int option_handle_command_band(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
char* rtrim(char* s) {
  char t[4000];
  char *end;

  strcpy(t, s);
  end = t + strlen(t) - 1;
  while (end != t && isspace(*end))
    end--;
  *(end + 1) = '\0';
  s = t;

  return s;
}
///////////////////////////////////////////////////////////////////////////////
char* ltrim(char *s) {
  char* begin;
  begin = s;

  while (*begin != '\0') {
    if (isspace(*begin))
      begin++;
    else {
      s = begin;
      break;
    }
  }

  return s;
}
///////////////////////////////////////////////////////////////////////////////
char* trim(char *s) {
  return rtrim(ltrim(s));
}
///////////////////////////////////////////////////////////////////////////////
static int getProfileID()
{
	char achPID[64]={0,};
	rdb_get_single(rdb_name(RDB_PROFILE_ID, ""), achPID, sizeof(achPID));

	return atoi(achPID)+1;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileWrite_option()
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
		sprintf(achCmd,"AT$QCPDPP=%d,%d",nPID,iAuth);
	else
		sprintf(achCmd,"AT$QCPDPP=%d,%d,%s,%s",nPID,iAuth,_profileInfo.achPw,_profileInfo.achUr);

	// send command
	stat=at_send(achCmd,response,"",&ok,0);
	if(stat<0 || !ok)
		SYSLOG_ERR("failed to set authentication - AT$QCPDPP failure");

	// build profile setting
	sprintf(achCmd,"AT+CGDCONT=%d,\"IP\",\"%s\"", nPID,_profileInfo.achAPN);
	stat=at_send(achCmd,response,"",&ok,0);
	if(stat<0 || !ok)
		goto error;

	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[error] at command failed");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int getProfileStat_option(int nPID)
{
	int fUp;

	int stat, ok, call_state = 0;
	char achCmd[128];
	char response[AT_RESPONSE_MAX_SIZE];
	char * pos1;
	
//#define OPTION_TEST_UNKNOWN_STATE
		
#ifdef OPTION_TEST_UNKNOWN_STATE
	static int test_mode=0;
#endif	

	stat=at_send("AT_OWANCALL?",response,"_OWANCALL",&ok,0);
	if(stat<0 || !ok)
		return 0;

#ifdef OPTION_TEST_UNKNOWN_STATE
	if(test_mode>10) {
		syslog(LOG_ERR,"[option test mode] pretending to have unknown status");
		snprintf(response,sizeof(response),"_OWANCALL: 1, 3, 0");
	}
	else {
		test_mode++;
		syslog(LOG_ERR,"[option test mode] test_mode=%d",test_mode);
	}
#endif			
	
	sprintf(achCmd, "_OWANCALL: %d,",nPID);
	pos1 = strstr(response, achCmd);
	
	if(!pos1)
		return 0;
	

	call_state = atoi(pos1+strlen(achCmd));

	if ((call_state == 1) ||(call_state == 2) ||(call_state == 3))
		fUp = call_state;
	else 
		fUp = 0;

	return fUp;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileDeActivate_option()
{
	int nPID=getProfileID();

	// send activation or deactivation command
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;

	sprintf(achCmd,"AT_OWANCALL=%d,0,0", nPID);
	at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY,0);

	rdb_set_single(rdb_name(RDB_NETIF_IPADDR,""),"");
	rdb_set_single(rdb_name(RDB_NETIF_GWADDR,""),"");
	rdb_set_single(rdb_name(RDB_NETIF_PDNSADDR,""),"");
	rdb_set_single(rdb_name(RDB_NETIF_SDNSADDR,""),"");

	rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"0");

	// set command result
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileActivate_option()
{

	int nPID=getProfileID();

	int fUp, loop_cnt = 0, token_count=0;

	// send activation or deactivation command
	char achCmd[128];
	char response[AT_RESPONSE_MAX_SIZE];
	int ok, stat, cgdcont_id=0;
	const char *tokenp;
	char * value, *outp;

	sprintf(achCmd,"AT_OWANCALL=%d,1,0", nPID);
	stat=at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY,0);

	do {
		sleep(1);
		fUp=getProfileStat_option(nPID);
		loop_cnt++;
	}while((loop_cnt <7) && ( (fUp == 2) || (fUp == 3) ));

	if(fUp == 3) {
		syslog(LOG_ERR,"AT_OWANCALL returns unknown status (3). sending AT+CFUN command to reset");
		sprintf(achCmd,"AT+CFUN=1,1");
		if (at_send_with_timeout(achCmd,response,"_OWANDATA",&ok,TIMEOUT_NETWORK_QUERY,512) != 0 || !ok) {
			syslog(LOG_ERR,"failed to reset module");
			goto error;
		}
	}
	
	if((fUp == 2) || (fUp == 0))
		goto error;

	sprintf(achCmd,"AT_OWANDATA=%d", nPID);
	if (at_send_with_timeout(achCmd,response,"_OWANDATA",&ok,TIMEOUT_NETWORK_QUERY,512) != 0 || !ok)
		goto error;

	if(strlen(response)<STRLEN("_OWANDATA:"))
		goto error;

	value = response + STRLEN("_OWANDATA:");

/*

	# format
	_OWANDATA: <c>, <ip>, <gw>, <dns1>,<dns2>, <nbns1>, <nbns2>, <csp>

	_OWANDATA: 1, 10.167.20.238, 0.0.0.0, 139.130.4.4, 203.50.2.71, 0.0.0.0, 0.0.0.0, 72000

*/

	token_count = tokenize_at_response(value);

	if(token_count != 8)
		goto error;

	//[1]The context corresponding to the cgdcont id
	tokenp = get_token(value, 0);
	if(!tokenp)
	   goto error;

	cgdcont_id = atoi(tokenp);

	if(cgdcont_id != nPID)
	   goto error;

	//[2]IP address
	tokenp = get_token(value, 1);
	if(!tokenp)
	   goto error;
	else {
		outp = trim((char *)tokenp);
		rdb_set_single(rdb_name(RDB_NETIF_IPADDR,""),outp);
	}

	//[3]Gateway address
	tokenp = get_token(value, 2);
	if(!tokenp)
	   goto error;
	else {
		outp = trim((char *)tokenp);
		rdb_set_single(rdb_name(RDB_NETIF_GWADDR,""),outp);
	}

	//[4]First DNS server
	tokenp = get_token(value, 3);
	if(!tokenp)
	   goto error;
	else {
		outp = trim((char *)tokenp);
		rdb_set_single(rdb_name(RDB_NETIF_PDNSADDR,""),outp);
	}

	//[5]Second DNS server
	tokenp = get_token(value, 4);
	if(!tokenp)
	   goto error;
	else {
		outp = trim((char *)tokenp);
		rdb_set_single(rdb_name(RDB_NETIF_SDNSADDR,""),outp);
	}

	rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"1");

	// set command result
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;

error:
	handleProfileDeActivate_option();
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int option_handle_command_profile(const struct name_value_t* args)
{
	int stat;
	if (!args || !args[0].value)
		return -1;


	if (!strcmp(args[0].value, "write"))
	{
		return handleProfileWrite_option();
	}
	else if (!strcmp(args[0].value, "activate"))
	{
		stat=handleProfileActivate_option();

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}
	else if (!strcmp(args[0].value, "deactivate"))
	{
		stat=handleProfileDeActivate_option();

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int option_profile_set(void)
{
	int ok;
	if (at_send("AT+CLVL=4", NULL, "", &ok, 0) != 0 || !ok)
	{
		return -1;
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int option_profile_init(void)   // quick and dirty
{
	int ok;
	if (at_send("AT_ODO=0", NULL, "", &ok, 0) != 0 || !ok)
	{
		return -1;
	}
	if (at_send("AT_OPCMENABLE=1", NULL, "", &ok, 0) != 0 || !ok)
	{
		return -1;
	}
	/* This command make modem PCM clock to prevent from being off after call end.
	 * It is terribly important. Please always imply this command to all Sierra modem.  */
//	if (at_send("at!avextpcmstopclkoff=1", NULL, "", &ok) != 0 || !ok)
//	{
//		return -1;
//	}
	if (at_send("AT+VIP=0", NULL, "", &ok, 0) != 0 || !ok)
	{
		return -1;
	}
	if (at_send("AT+CLVL=4", NULL, "", &ok, 0) != 0 || !ok)
	{
		return -1;
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int option_handle_command_phonesetup(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}
	if (strcmp(args[0].value, "profile set") == 0)
	{
		return option_profile_set();
	}
	else if (strcmp(args[0].value, "profile init") == 0)
	{
		return option_profile_init();
	}

	SYSLOG_DEBUG("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
	return model_run_command_default(args);
}
///////////////////////////////////////////////////////////////////////////////
int option_handle_command_sim(const struct name_value_t* args)
{
	default_handle_command_sim(args);
	option_update_retries_remaining(); //update retries remaining
	
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
struct command_t option_commands[] =
{
	{ .name = RDB_BANDCMMAND,		.action = option_handle_command_band },
	{ .name = RDB_PROFILE_CMD,		.action = option_handle_command_profile },
	{ .name = RDB_PHONE_SETUP".command",	.action = option_handle_command_phonesetup },
	{ .name = RDB_SIMCMMAND,		.action = option_handle_command_sim },
	{0,}
};

///////////////////////////////////////////////////////////////////////////////
struct model_t model_optionwireless =
{
	.name = "Option Wireless",

	.init = model_option_init,
	.detect = model_option_detect,

	.get_status = model_default_get_status,
	.set_status = model_option_set_status,

	.commands = option_commands,
	.notifications = NULL
}; 
 
 

