
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#ifdef PLATFORM_PLATYPUS
#include <nvram.h>
#endif

#include "../at/at.h"
#include "../model/model.h"
#include "../util/rdb_util.h"
#include "../rdb_names.h"
#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "model_default.h"
#include "../util/at_util.h"

#ifdef PLATFORM_PLATYPUS
#include <nvram.h>
#endif

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

#define ENABLE_V2_BAND_SELECTION

extern const struct command_t model_default_commands[];
extern const struct notification_t model_default_notifications[];

extern int update_pdp_status();
extern int check_pin_status(char *response);
extern int _getTokens(char* atResp,int atPrefixCnt,char* tokenTbl[],int tblCnt,int tokenSize);
extern int _isMCCMNC(const char* str);
extern int _getMCC(const char* str);
extern int _getMNC(const char* str);
static int ericsson_handleBandSet();
static int ericsson_handleBandGet();

static struct {
	char achAPN[128];
	char achUr[128];
	char achPw[128];
	char achAuth[128];
} _profileInfo;

/* simplify band selection menu using all/gsm-all/wcdma-all only */
//#define USE_SIMPLIFIED_BAND_LIST
struct _channel_band
{
  char *number;
  char *bandname;
  int support;
  int gsmband;
  int wcdmaband;
} FrequencyBandtbl[] =
{
#ifdef USE_SIMPLIFIED_BAND_LIST
#define ALL_BAND_IDX		0
#define GSM_ALL_BAND_IDX	1
#define WCDMA_ALL_BAND_IDX	2
  {"00", "All bands", 	0, 31, 255},	// 00: All bands 	==> (GSM:00011111, WCDMA:11111111):31, 255
  {"01", "GSM ALL", 	0, 31, 0}, 		// 01 = GSM ALL		==> (G:00011101, W:00000000) (G:29, W:00)
  {"02", "WCDMA All",	0, 0, 255}, 	// 02 = WCDMA ALL	==> (G:00000000, W:00110011) (G:00, W:51)
  {NULL, NULL, 			0, 0, 0}
#else
#define ALL_BAND_IDX		0
#define GSM_ALL_BAND_IDX	5
#define WCDMA_ALL_BAND_IDX	8
  {"00", "All bands", 0, 31, 255}, // 00: All bands ==> (GSM:00011111, WCDMA:11111111):31, 255
  {"01", "WCDMA 2100", 0, 0, 1}, // 01 = WCDMA 2100			==> (G:00000000, W:00000001) (G:00, W:01)
  {"02", "WCDMA 850/1900",	 0, 0, 18}, // 02 = WCDMA 850/1900			==> (G:00000000, W:00010010) (G:00, W:18)
  {"03", "GSM 900/1800", 0, 12, 0}, //  03 = GSM 900/1800			==> (G:00001100, W:00000000) (G:12, W:00)
  {"04", "GSM 850/1900",	0, 17, 0}, // 04 = GSM 850/1900			==> (G:00010001, W:00000000) (G:17, W:00)
  {"05", "GSM ALL", 0, 31, 0}, // 05 = GSM ALL				==> (G:00011101, W:00000000) (G:29, W:00)
  {"06", "WCDMA 2100 GSM 900/1800", 0, 12, 1}, // 06 = WCDMA 2100 GSM 900/1800		==> (G:00001100, W:00000001) (G:12, W:01)
  {"07", "WCDMA 850/1900 GSM 850/1900", 0, 17, 18}, // 07 = WCDMA 850/1900 GSM 850/1900	==> (G:00010001, W:00010010) (G:17, W:18)
  {"08", "WCDMA All",	0, 0, 255}, // 08 = WCDMA ALL				==> (G:00000000, W:00110011) (G:00, W:51)
  {"09", "WCDMA 850/2100", 0, 0, 17}, // 09 = WCDMA 850/2100			==> (G:00000000, W:00010001) (G:00, W:17)
  {"10", "WCDMA 800/2100", 0, 0, 33}, // 0A = WCDMA 800/2100			==> (G:00000000, W:00100001) (G:00, W:33)
  {"11", "WCDMA 850/2100 GSM 900/1800", 0, 12, 17}, // 0B = WCDMA 850/2100 GSM 900/1800	==> (G:00001100, W:00010001) (G:12, W:17)
  {"12", "WCDMA 850 GSM 900/1800", 0, 12, 16}, // 0C = WCDMA 850 GSM 900/1800		==> (G:00001100, W:00010000) (G:12, W:16)
  {"13", "WCDMA 850", 0, 0, 16}, // 0D = WCDMA 850				==> (G:00000000, W:00010000) (G:00, W:16)
  {"14", "WCDMA 900", 0, 0, 64}, // 0E = WCDMA 900				==> WCDMA VIII
  {"15", "WCDMA 900/2100", 0, 0, 65}, // 0F = WCDMA 900/2100			==> WCDMA VIII / WCDMA I
  {NULL, NULL, 0, 0, 0}
#endif
};

static int supported_gsmband =0;
static int supported_wcdmaband =0;
static int current_gsmband = 0;
static int current_wcdmaband = 0;

///////////////////////////////////////////////////////////////////////////////
static int model_ericsson_init(void)
{
	int ok;
	char user_band_name[128]={0,};

	at_send("AT*E2CHAN=0,0", NULL, "", &ok, 0);
	at_send("AT+CREG=2", NULL, "", &ok, 0);

	/* update support flags before starting set */
	ericsson_handleBandGet();

	/*** restore the band setting from power up ****/
	if (rdb_get_single(rdb_name(RDB_BANDCFG, ""), user_band_name, sizeof(user_band_name)) == 0) {
		if(*user_band_name)
			rdb_set_single(rdb_name(RDB_BANDPARAM, ""), user_band_name);
	}
	
	/* set the previous band */
	ericsson_handleBandSet();
	
	/* update current band after setting */
	ericsson_handleBandGet();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_ericsson_detect(const char* manufacture, const char* model_name)
{
	const char* model_names[]={
		"F3607gw",
		"F5521gw",
	};

	// search for Ericsson in manufacture string
	if(is_ericsson)
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
/*
 * Query the Ericsson module for temperature status.
 *
 * Modules supported: f5521gw
 */
static void ericsson_update_temperature (void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char *token;
	int ok, i;
	int status;

	/* Query the Ericsson module for a temperature reading */
	status = at_send("AT*E2OTR?", response, "*E2OTR", &ok,
			 AT_RESPONSE_MAX_SIZE);
	if (status == 0) {
		/*
		 * If we get a valid response, break the response up into
		 * tokens and get the final one, which is the temp reading.
		 */
		token = strtok(response, ",");
		/* The temperature will be the last token, which is the fifth one */
		for (i = 0; i < 5; i++) {
			token = strtok(NULL, ",");
		}
		if (token != NULL) {
			rdb_set_single(rdb_name("module_temperature",""), token);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
extern int Hex( char  hex);

static void ericsson_update_sim(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char value[32];
	int ok;
	char *pos1, *pos2;
	int stat, i;

	update_sim_hint();

	stat=at_send("AT+CRSM=176,12258,0,0,10", response, "", &ok, AT_RESPONSE_MAX_SIZE);
	if ( !stat && ok)
	{
		pos1=strstr(response, "+CRSM:");
		if(pos1)
		{
			pos2=strstr(pos1, ",\"");
			if( pos2 && strlen(pos2)>5 )
			{
				pos1=strchr( pos2+2, '\"');
				if(pos1)
				{
					*pos1=0;
					pos1=pos2+2;
					for( i=0; i<strlen(pos2)-2; i+=2)
					{
						value[i]=*(pos1+i+1);
						if(*(pos1+i)=='F') *(pos1+i)=0;
						value[i+1]=*(pos1+i);
					}
					value[i]=0;
					rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), value);
				}
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void ericsson_current_band(void) {
   char* resp;
   int gsmband=0xff, wcdmaband=0xff;
   char value[24] = "N/A";

   char * gsmbandtbl[] ={"N/A", "GSM 850", "ESGM 900", "DCS 1800", "PCS 1900", NULL};

   char * wcdmabandtbl[] ={"N/A", "WCDMA 2100", "WCDMA 1900", "N/A", "N/A", "WCDMA 850", "WCDMA 800", "N/A", "WCDMA 900", NULL};

   if( (resp=at_bufsend_with_timeout("AT*E2CHAN?","*E2CHAN: ",TIMEOUT_NETWORK_QUERY)) !=0 )
   {
      char* cgattTokens[5];
      int i;
      for(i=0;i<5;i++)
         cgattTokens[i]=alloca(64);

      _getTokens(resp,0,cgattTokens,5,64);

      gsmband = atoi(cgattTokens[2]);
      wcdmaband = atoi(cgattTokens[3]);

      if((gsmband < 0 || gsmband >= sizeof(gsmbandtbl)/sizeof(*gsmbandtbl))
         || (wcdmaband < 0 || wcdmaband >= sizeof(wcdmabandtbl)/sizeof(*wcdmabandtbl)))
      {
         rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
         return;
      }

      if (gsmband != 0)
      {
         strcpy(value, gsmbandtbl[gsmband]);
      }
      else if (wcdmaband !=0)
      {
         strcpy(value, wcdmabandtbl[wcdmaband]);
      }

      rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), value);
      rdb_set_single(rdb_name("system_network_status.ChannelNumber", ""), cgattTokens[4]);
   }
}
///////////////////////////////////////////////////////////////////////////////
#ifdef PLATFORM_BOVINE
void ericsson_service_type(void) {
	char* resp;
	int gsm_idx=0xff, wcdma_idx=0xff;
	char value[64] = "";
	char * gsm_strings[] ={"", "GPRS", "EGPRS", NULL};
	char * wcdm_strings[] ={"", "UMTS", "HSDPA", "HSPA+", NULL};
	char* cgattTokens[3];
	int i;

	if ((resp=at_bufsend_with_timeout("AT*ERINFO?","*ERINFO: ",
					  TIMEOUT_NETWORK_QUERY)) !=0)
	{
		for (i=0; i<3; i++) {
			cgattTokens[i]=alloca(64);
		}

		_getTokens(resp, 0, cgattTokens, 3, 64);
		gsm_idx = atoi(cgattTokens[1]);
		wcdma_idx = atoi(cgattTokens[2]);

		if((gsm_idx < 0 || 
		    gsm_idx >= sizeof(gsm_strings)/sizeof(*gsm_strings)) ||
		    (wcdma_idx < 0 || 
		     wcdma_idx >= sizeof(wcdm_strings)/sizeof(*wcdm_strings)) ||
		    ((gsm_idx == 0) && (wcdma_idx == 0)))
		{
			rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), 
				       "Not Available");
			return;
		}

		if(gsm_idx != 0) {
			strcat(value, gsm_strings[gsm_idx]);
			if (wcdma_idx != 0) {
				strcat(value, "/");
			}
		}
		strcat(value, wcdm_strings[wcdma_idx]);
		rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), value);
	}
}

#endif
///////////////////////////////////////////////////////////////////////////////
/* Ericsson modem does not report PDP status correctly.
 * So it needs to check PDP address as well to ensure PDP deactive state
 */
static int ericsson_getPDPStatus(void)
{
	#define AT_SCACT_TIMEOUT	30

	int stat;

	int ok;
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
    int pdp_no;

/*
	+CGACT: (0,1)

	or

	+CGACT: 1,1
	+CGACT: 2,0
	+CGACT: 3,0
	+CGACT: 4,0
	+CGACT: 5,0

*/

	sprintf(achCmd,"AT+CGACT?");
	stat=at_send_with_timeout(achCmd,response,"",&ok,AT_SCACT_TIMEOUT,0);
	if(stat<0 || !ok)
		goto err;

	char* pdpStat=response+strlen("+CGACT: ");

	// get pdp number
	const char* szPdpNo = _getFirstToken(pdpStat, "(),");
	if(!szPdpNo)
		goto err;

    pdp_no = atoi(szPdpNo);

	// gget pdp stat
	const char* szPdpStat =	_getNextToken();
	if(!szPdpStat)
		goto err;

	/* return 'active' status if at+cgact command reports correctly. */
	if (strcmp(szPdpStat, "1") == 0)
		return 1;

	/* read PDP address if at+cgact command reports 'down' status to make sure */
	#define AT_CGPADDR_TIMEOUT	5
	sprintf(achCmd,"AT+CGPADDR=%d", pdp_no);
	stat=at_send_with_timeout(achCmd,response,"",&ok,AT_CGPADDR_TIMEOUT,0);
	if(stat<0 || !ok)
		goto err;

	char* pdpAddr=response+strlen("+CGPADDR: ");

	// get pdp number
	szPdpNo = _getFirstToken(pdpAddr, "(),");
	// gget pdp addr
	const char* szPdpAddr =	_getNextToken();

	if(!szPdpAddr || strlen(szPdpAddr) == 0)
		return 0;

	return 1;

err:
	return -1;
}
////////////////////////////////////////////////////////////////////////////////
int ericsson_update_pdp_status()
{
	// get pdp connections status
	int pdpStat;

	pdpStat=ericsson_getPDPStatus();
	if(pdpStat>=0)
		rdb_set_single(rdb_name(RDB_PDP0STAT, ""), pdpStat==0?"down":"up");

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int model_ericsson_set_status (const struct model_status_info_t* chg_status,
	const struct model_status_info_t* new_status,
	const struct model_status_info_t* err_status)
{
	static int update_network_name_at_startup=1;

	if(!at_is_open())
		return 0;

	ericsson_update_sim();
	update_sim_hint();
	update_sim_status();

	update_configuration_setting(new_status);

	update_signal_strength();

	update_imsi();

	// update_network_name() will be automatically done by model_default.c - by CREG notification
	if(update_network_name_at_startup) {
		update_network_name_at_startup=0;
		update_network_name();
		update_roaming_status();
	}

	ericsson_current_band();

#ifdef PLATFORM_BOVINE
	ericsson_service_type();
#endif

	ericsson_update_pdp_status();

	//if(new_status->status[model_status_registered])
		//update_call_forwarding();

	ericsson_update_temperature();

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int ericsson_handleBandGet()
{
	char value[1024], response[AT_RESPONSE_MAX_SIZE];
	int stat, ok, loop_cnt, cfun;
	char *pos1, * tokenp;
	char curBandName[64];
	int current_bandidx = 0xff;

#define ERR_RETURN	{ if ( stat || !ok) goto error; }
#define ERR_RETURN2	{ if ( !pos1 ) goto error; }

	SYSLOG_INFO("##cmd## [getband] start");

	memset(value, 0, sizeof(value));

	//SYSLOG_ERR("call ericsson_handleBandGet()");

	stat=at_send("AT*E2BAND?", response, "", &ok, 0);
	ERR_RETURN
	pos1 = strstr(response, "*E2BAND: ");
	ERR_RETURN2
	if ((tokenp = strtok(pos1+strlen("*E2BAND: "), ",")) == NULL)
		goto error;
	supported_gsmband = atoi(tokenp);
	if ((tokenp = strtok(NULL, ",")) == NULL)
		goto error;
	current_gsmband = atoi(tokenp);
	if ((tokenp = strtok(NULL, ",")) == NULL)
		goto error;
	supported_wcdmaband = atoi(tokenp);
	if ((tokenp = strtok(NULL, ",")) == NULL)
		goto error;
	current_wcdmaband = atoi(tokenp);

	//SYSLOG_ERR("sgb %d, cgb %d, swb %d, cwb %d",
	//		supported_gsmband, current_gsmband, supported_wcdmaband, current_wcdmaband);
	//All bands
	FrequencyBandtbl[ALL_BAND_IDX].gsmband = supported_gsmband;
	FrequencyBandtbl[ALL_BAND_IDX].wcdmaband = supported_wcdmaband;
	// GSM ALL
	FrequencyBandtbl[GSM_ALL_BAND_IDX].gsmband = supported_gsmband;
	FrequencyBandtbl[GSM_ALL_BAND_IDX].wcdmaband = 0;
	// WCDMA All
	FrequencyBandtbl[WCDMA_ALL_BAND_IDX].gsmband = 0;
	FrequencyBandtbl[WCDMA_ALL_BAND_IDX].wcdmaband = supported_wcdmaband;

	stat=at_send("AT+CFUN?", response, "", &ok, 0);
	ERR_RETURN
	pos1 = strstr(response, "+CFUN:");
	ERR_RETURN2
	cfun=atoi(response+strlen("+CFUN:"));
	if( cfun==6 )
		current_gsmband=0;
	else if( cfun==5 )
		current_wcdmaband=0;

	for(loop_cnt=0 ; FrequencyBandtbl[loop_cnt].number != NULL; loop_cnt++)
	{
		FrequencyBandtbl[loop_cnt].support = 0; // not support
// temp Ericsson modules don't support GSM only or WCDMA only.
// That is, the response of "AT*E2BAND=0,1" command is Error. So the items that have only one kinds of band should be deleted.
/* But the command AT+CFUN=5 or AT+CFUN=6 can be used
        if((FrequencyBandtbl[loop_cnt].gsmband == 0) || (FrequencyBandtbl[loop_cnt].wcdmaband == 0))
          continue;
*/
		//SYSLOG_ERR("%d, %d, %d : %d, %d, %d",
		//		FrequencyBandtbl[loop_cnt].gsmband , (FrequencyBandtbl[loop_cnt].gsmband & supported_gsmband), FrequencyBandtbl[loop_cnt].gsmband,
		//		FrequencyBandtbl[loop_cnt].wcdmaband, (FrequencyBandtbl[loop_cnt].wcdmaband & supported_wcdmaband), FrequencyBandtbl[loop_cnt].wcdmaband);
		if(((FrequencyBandtbl[loop_cnt].gsmband & supported_gsmband) == FrequencyBandtbl[loop_cnt].gsmband) &&
		   ((FrequencyBandtbl[loop_cnt].wcdmaband & supported_wcdmaband) == FrequencyBandtbl[loop_cnt].wcdmaband)) {
#ifndef ENABLE_V2_BAND_SELECTION
			FrequencyBandtbl[loop_cnt].support = 1; // support
			strcat(value, FrequencyBandtbl[loop_cnt].bandname);
			strcat( value, ";");
#else
			FrequencyBandtbl[loop_cnt].support = 1; // support
			strcat(value, FrequencyBandtbl[loop_cnt].number);
			strcat( value, ",");
			strcat(value, FrequencyBandtbl[loop_cnt].bandname);
			strcat( value, "&");
#endif
        }

//#ifndef USE_SIMPLIFIED_BAND_LIST
		if((FrequencyBandtbl[loop_cnt].gsmband == current_gsmband) && (FrequencyBandtbl[loop_cnt].wcdmaband == current_wcdmaband))
			current_bandidx = loop_cnt;
//#endif
	}

#ifdef USE_SIMPLIFIED_BAND_LIST
	current_bandidx = (cfun == 1? 0: (cfun == 5? 1: (cfun == 6? 2:0xff)));
#endif

	SYSLOG_INFO("##cmd## [getband] read band settings from module (band=%d,cfun=%d,gsm=%d,wcdma=%d)",current_bandidx,cfun,current_gsmband,current_wcdmaband);

	if(current_bandidx == 0xff)
		goto error;

	memset(value+strlen(value)-1, 0,1);
#ifndef ENABLE_V2_BAND_SELECTION
	rdb_set_single(rdb_name("currentband.current_band", ""), value);
#else
	rdb_set_single(rdb_name(RDB_MODULEBANDLIST, ""), value);
#endif

	if(FrequencyBandtbl[current_bandidx].bandname != NULL)
		strcpy(curBandName, FrequencyBandtbl[current_bandidx].bandname);
	else
		goto error;

	SYSLOG_INFO("##cmd## [getband] return current band configuration (band='%s')", curBandName);
	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), curBandName);
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
	
	SYSLOG_INFO("##cmd## [getband] done");
	return 0;

error:
	SYSLOG_INFO("##cmd## [getband] return default band configuration (band='%s')", "Autoband");
	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), "Autoband");
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");
	
	SYSLOG_ERR("##cmd## [getband] done - error");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int ericsson_handleBandSet()
{
   char achBandParam[128], achATCmd[128], response[AT_RESPONSE_MAX_SIZE];
   int requestidx=0xff;
   int ok, stat;
#ifndef ENABLE_V2_BAND_SELECTION
   int loop_cnt;
#endif
   int curr_cfun, new_cfun, curr_gsmband, new_gsmband, curr_wcdmaband, new_wcdmaband;
   char *pToken, *pos;
   int retry_cnt = 0;

   char user_band_name[128];

   //SYSLOG_ERR("call ericsson_handleBandSet()");
   
   SYSLOG_INFO("##cmd## [setband] start");

   if (rdb_get_single(rdb_name(RDB_BANDPARAM, ""), achBandParam, sizeof(achBandParam)) != 0) {
   	syslog(LOG_ERR,"##cmd## [setband] band not specified [rdb=%s]",rdb_name(RDB_BANDPARAM, ""));
      goto error;
  }

   strcpy(user_band_name,achBandParam);
   
#ifndef ENABLE_V2_BAND_SELECTION
   for(loop_cnt=0 ; FrequencyBandtbl[loop_cnt].number != NULL; loop_cnt++) {
      if(!strcmp( FrequencyBandtbl[loop_cnt].bandname, achBandParam)) {
         strcpy(achBandParam, FrequencyBandtbl[loop_cnt].number);
         break;
      }
   }
#endif

   requestidx = atoi(achBandParam);
   
   SYSLOG_INFO("##cmd## [setband] got rdb band settings (reqidx=%d)",requestidx);

   if(requestidx < 0 || requestidx >= sizeof(FrequencyBandtbl)/sizeof(*FrequencyBandtbl)) {
   	syslog(LOG_ERR,"##cmd## [setband] out of band index (reqidx=%d,max=%d)",requestidx,sizeof(FrequencyBandtbl)/sizeof(*FrequencyBandtbl));
      goto error;
     }


   if(FrequencyBandtbl[requestidx].support != 1) {
   	SYSLOG_INFO("##cmd## [setband] band not supported (reqidx=%d)",requestidx);
      goto error;
}


/************************************************************************************************
	Since the module is not accept the AT*E2BAND=0,X nor AT*E2BAND=X,0 .....
	to set the WCDMA only band(s) we have to use the command AT+CFUN=6 to set the GSM radio off.
	to set the GSM only band(s) use the command AT+CFUN=5 to set the WCDMA radio off.
*************************************************************************************************/
	if(!FrequencyBandtbl[requestidx].gsmband) {
		new_gsmband = FrequencyBandtbl[0].gsmband;
		new_wcdmaband = FrequencyBandtbl[requestidx].wcdmaband;
		new_cfun = 6;
	}
	else if(!FrequencyBandtbl[requestidx].wcdmaband) {
		new_gsmband = FrequencyBandtbl[requestidx].gsmband;
		new_wcdmaband = FrequencyBandtbl[0].wcdmaband;
		new_cfun = 5;
	}
	else {
		new_gsmband = FrequencyBandtbl[requestidx].gsmband;
		new_wcdmaband = FrequencyBandtbl[requestidx].wcdmaband;
		new_cfun = 1;
	}

	stat=at_send_with_timeout("AT*E2BAND?",response,"",&ok,TIMEOUT_CFUN_SET,0);
	if ( !stat && ok && strstr(response, "*E2BAND: ") ) {
	    pos = strstr(response, "*E2BAND: ");
		if ((pToken = strtok(pos+strlen("*E2BAND: "), ",")) == NULL)
			goto error;
		if ((pToken = strtok(NULL, ",")) == NULL)
			goto error;
		curr_gsmband = atoi(pToken);
		if ((pToken = strtok(NULL, ",")) == NULL)
			goto error;
		if ((pToken = strtok(NULL, ",")) == NULL)
			goto error;
		curr_wcdmaband = atoi(pToken);
	} else {
		SYSLOG_DEBUG("*E2BAND query command error: %s", response);
		goto error;
	}
	stat = at_send_with_timeout("AT+CFUN?",response,"",&ok,TIMEOUT_CFUN_SET,0);
	if ( !stat && ok && strstr(response, "+CFUN:") )
			curr_cfun = atoi(response + strlen("+CFUN:"));
	else {
		SYSLOG_DEBUG("+CFUN query command error: %s", response);
		goto error;
	}

	stat=at_send_with_timeout("AT*ENAP?",response,"",&ok,TIMEOUT_CFUN_SET,0);
	if(stat<0 || !ok) {
		SYSLOG_ERR("failed 'AT*ENAP?' command");
		goto error;
	}

	if ((curr_cfun != new_cfun || curr_gsmband != new_gsmband || curr_wcdmaband != new_wcdmaband) &&
		strncmp(response, "*ENAP:0", 7)) {
		SYSLOG_DEBUG("send *ENAP=0 command to disconnect before changing band");
		stat=at_send_with_timeout("AT*ENAP=0",response,"",&ok,TIMEOUT_CFUN_SET,0);
		if((stat<0 || !ok || strcmp(response, "ERROR")) && ++retry_cnt > 3) {
			SYSLOG_ERR("failed 'AT*ENAP=0' command");
			goto error;
		}
		retry_cnt = 0;
enap_retry:
		stat=at_send_with_timeout("AT*ENAP?",response,"",&ok,TIMEOUT_CFUN_SET,0);
		retry_cnt++;
		if((stat<0 || !ok) && retry_cnt > 3) {
			SYSLOG_ERR("failed 'AT*ENAP?' command");
			goto error;
		}
		if (strncmp(response, "*ENAP:0", 7)) {
			sleep(1);
			if (retry_cnt > 10)
				goto error;
			else
				goto enap_retry;
		} else {
			SYSLOG_DEBUG("Succeeded 'AT*ENAP=0' command, ready to change band");
		}
	}

	/* send *E2BAND command */
	SYSLOG_DEBUG("curr band: %d, %d, new band: %d, %d", curr_gsmband, curr_wcdmaband, new_gsmband, new_wcdmaband);
	if ( curr_gsmband != new_gsmband || curr_wcdmaband != new_wcdmaband ) {
		sprintf(achATCmd, "AT*E2BAND=%d,%d", new_gsmband, new_wcdmaband);
		stat=at_send_with_timeout(achATCmd,response,"",&ok,TIMEOUT_CFUN_SET,0);
		if(stat<0 || !ok) {
			SYSLOG_ERR("##cmd## [setband] failed to apply new band settings (cfun=%d,gsm=%d,wcdma=%d)",new_cfun,new_gsmband,new_wcdmaband);
			goto error;
		}
		sleep(1);
		
		rdb_set_single(rdb_name(RDB_BANDCFG,""),user_band_name);
	} else {
		SYSLOG_DEBUG("E2BAND already set to target value, skip");
	}

	/* send +CFUN command */
	if (curr_cfun != new_cfun) {
		sprintf(achATCmd, "AT+CFUN=%d", new_cfun);
		stat = at_send_with_timeout(achATCmd,response,"",&ok,TIMEOUT_CFUN_SET,0);
		if(stat < 0 || !ok)
			goto error;
	} else {
		SYSLOG_DEBUG("CFUN already set to target value, skip");
	}

	SYSLOG_INFO("##cmd## [setband] new band settings applied (cfun=%d,gsm=%d,wcdma=%d)",new_cfun,new_gsmband,new_wcdmaband);

   rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
   
	SYSLOG_INFO("##cmd## [setband] done");
      return 0;

error:
   rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");
   SYSLOG_ERR("##cmd## [setband] done - error");
   return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int ericsson_handle_command_band(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}

	if (!strcmp(args[0].value, "get"))
		return ericsson_handleBandGet();
	else if (!strcmp(args[0].value, "set"))
		return ericsson_handleBandSet();

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int getProfileID()
{
	char achPID[64]={0,};
	rdb_get_single(rdb_name(RDB_PROFILE_ID, ""), achPID, sizeof(achPID));

	return atoi(achPID)+1;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileWrite_ericsson()
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
	char *pos1;

	char *iAuth;

	/* +CGDCONT silently does not work when module is online */
	sprintf(achCmd,"AT*ENAP=0");
	at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

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

   sprintf(achCmd, "AT*EIAAUW=%d,1,\"%s\",\"%s\",%s,0",nPID, _profileInfo.achUr, _profileInfo.achPw, iAuth);

	// send command
	stat=at_send(achCmd,response,"",&ok, 0);
	if(stat<0 || !ok)
		SYSLOG_ERR("failed to set authentication - cmd=%s,res=%s", achCmd, response);

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
static int handleProfileDeactivate_ericsson()
{

	// send activation or deactivation command
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;

	sprintf(achCmd,"AT*ENAP=0");
	at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);

	rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"0");

	// set command result
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileActivate_ericsson()
{

   int nPID=getProfileID();

   int ok, stat, loop_cnt;

   // send activation or deactivation command
   char achCmd[512];
   char response[AT_RESPONSE_MAX_SIZE];

   stat = at_send_with_timeout("AT*ENAP?",response,"",&ok,TIMEOUT_NETWORK_QUERY,0);

   if ( !stat && ok){
      if(strstr(response, "*ENAP:1"))
         goto success;
   }

   sprintf(achCmd,"AT*ENAP=1,%d", nPID);
   at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY,0);

   for (loop_cnt=0;loop_cnt<10;loop_cnt++)
   {
      sleep(1);

      stat = at_send_with_timeout("AT*ENAP?",response,"",&ok,TIMEOUT_NETWORK_QUERY,0);

      if ( !stat && ok){
         if(strstr(response, "*ENAP:1"))
            goto success;

         if(strstr(response, "*ENAP:0")) {
            sleep(1);
            //at_send_with_timeout("AT+CGATT=0",response,"",&ok,TIMEOUT_NETWORK_QUERY,0);

            sprintf(achCmd,"AT*ENAP=1,%d", nPID);
            at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY,0);
         }
      }
   }

   handleProfileDeactivate_ericsson();
   return -1;

success:
   rdb_set_single(rdb_name(RDB_PROFILE_UP,""),"1");

   // set command result
   rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
   return 0;
}

int ericsson_updateSIMStat()
{
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;

	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0)
		return -1;

	if (!ok) {
		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM not inserted");
	}
	else {
		const char* szSIMStat = response + strlen("+CPIN: ");

		if (!strcmp(szSIMStat, "READY") || !strcmp(szSIMStat, "USIM READY"))
			szSIMStat = "SIM OK";

		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), szSIMStat);
	}

	static int fPrevReady = 0;
	int fReady = ok && (strstr(response, "READY") != 0);

	if (!fPrevReady && fReady) {
		SYSLOG_INFO("SIM becomes ready - calling physinit()");
		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");
	}

	fPrevReady = fReady;

	if (at_send("AT*EPIN?", response, "", &ok, 0) != 0)
		return -1;

	if (ok) {
		const char* szSIMStat = response + strlen("*EPIN: ");
		char *pos1 = strchr(szSIMStat, ',');

		if(pos1) {
			char *pos2 = strchr(pos1+1, ',');
			*pos1=0;
			rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), szSIMStat);
			if(pos2) {
				*pos2=0;
				rdb_set_single(rdb_name(RDB_SIMPUKREMAIN, ""), pos1+1);
			}
		}
	}

	return 0;
}

int ericsson_handle_command_sim(const struct name_value_t* args)
{
	int ok = 0;
	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[128];

	char newpin[16];
	char simpuk[16];
	char pin[16];

	char* szCmdStat = NULL;

	// bypass if incorrect argument
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		goto error;
	}

	// check sim status
	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0 || !ok)
	{
		szCmdStat = "AT+CPIN? has failed";
		goto error;
	}

	int fSIMReady = strstr(response, "READY") > 0;
	int fSIMPin = strstr(response, "SIM PIN") > 0;

	// read pin
	if (rdb_get_single(rdb_name(RDB_SIMPARAM, ""), pin, sizeof(pin)) != 0)
		pin[0] = 0;

	convClearPwd(pin);

	// sim command - check
	if (memcmp(args[0].value, "check", STRLEN("check")) == 0) {
		if (at_send("at+clck=\"SC\",2", response, "", &ok, 0) != 0 || !ok) {
			rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), "Enabled" );
		}
		else {// +CLCK: 0
			int fEn = atoi(response + strlen("+CLCK: "));
			rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), fEn ? "Enabled" : "Disabled");
		}
	}
	// sim command - unlockmep
	else if (memcmp(args[0].value, "unlockmep", STRLEN("unlockmep")) == 0)
	{
		if (!fSIMReady)
		{
			// get mep code
			char mep[16];
			if (rdb_get_single(rdb_name(RDB_SIMMEP, ""), mep, sizeof(mep)) != 0) {
				sprintf(buf, "failed to get %s", rdb_name(RDB_SIMMEP, ""));
				szCmdStat = buf;
				goto error;
			}

			// send command
			sprintf(command, "at+clck=\"PN\",0,\"%s\"", mep);
			if (at_send(command, response, "", &ok, 0) != 0 || !ok) {
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}
		}
	}
	// sim command - verifypuk
	else if (memcmp(args[0].value, "verifypuk", STRLEN("verifypuk")) == 0) {
		if (!fSIMReady) {
			if (rdb_get_single(rdb_name(RDB_SIMNEWPIN, ""), newpin, sizeof(newpin)) != 0) {
				sprintf(buf, "failed to get %s", rdb_name(RDB_SIMNEWPIN, ""));
				szCmdStat = buf;
				goto error;
			}
			if (rdb_get_single(rdb_name(RDB_SIMPUK, ""), simpuk, sizeof(simpuk)) != 0) {
				sprintf(buf, "failed to get %s", rdb_name(RDB_SIMPUK, ""));
				szCmdStat = buf;
				goto error;
			}
			sprintf(command, "AT+CPIN=\"%s\",\"%s\"", simpuk, newpin);
			if (at_send(command, response, "", &ok, 0) != 0 || !ok) {
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}
		}
	}
	// sim command - verifypin
	else if (!memcmp(args[0].value, "verifypin", STRLEN("verifypin"))) {
		if (fSIMPin) {
			if (verifyPin(pin) < 0) {
				sprintf(buf, "%s\"%s\" has failed", "AT+CPIN=", pin);
				szCmdStat = buf;
				strcpy((char *)&last_failed_pin, pin);
				goto error;
			} else {
				(void) memset((char *)&last_failed_pin, 0x00, 16);
			}
		}
	}
	// sim command - changepin
	else if (memcmp(args[0].value, "changepin", STRLEN("changepin")) == 0) {
		if (rdb_get_single(rdb_name(RDB_SIMNEWPIN, ""), newpin, sizeof(newpin)) != 0) {
			sprintf(buf, "failed to get %s", rdb_name(RDB_SIMNEWPIN, ""));
			szCmdStat = buf;
			goto error;
		}
		convClearPwd(newpin);
		sprintf(command, "AT+CPWD=\"SC\",\"%s\",\"%s\"", pin, newpin);
		if (at_send(command, response, "", &ok, 0) != 0 || !ok) {
			sprintf(buf, "changepin operation has failed", command);
			szCmdStat = buf;
			goto error;
		}
	}
	// sim command - enablepin
	else if (memcmp(args[0].value, "enablepin", STRLEN("enablepin")) == 0) {
		verifyPin(pin);
		sprintf(command, "AT+CLCK=\"SC\",1,\"%s\"", pin);
		if (at_send(command, response, "OK", &ok, 0) != 0 || !ok) {
			sprintf(buf, "enablepin operation has failed", command);
			szCmdStat = buf;
			goto error;
		}
		rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), "Enabled");
	}
	// sim command - disablepin
	else if (memcmp(args[0].value, "disablepin", STRLEN("disablepin")) == 0) {
		verifyPin(pin);
		sprintf(command, "AT+CLCK=\"SC\",0,\"%s\"", pin);
		if (at_send(command, response, "OK", &ok, 0) != 0 || !ok) {
			sprintf(buf, "disablepin operation has failed", command);
			szCmdStat = buf;
			goto error;
		}

		rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), "Disabled");
	}
	else {
		SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
		goto error;
	}

	ericsson_updateSIMStat();

#ifdef PLATFORM_BOVINE
	rdb_set_single(rdb_name(RDB_SIMRESULTOFUO, ""), "Operation succeeded");
#else
	rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "Operation succeeded");
#endif

	return 0;

error:
	ericsson_updateSIMStat();

	if (szCmdStat) {
#ifdef PLATFORM_BOVINE
		rdb_set_single(rdb_name(RDB_SIMRESULTOFUO, ""), szCmdStat);
#else
		rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), szCmdStat);
#endif
	}

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int ericsson_handle_command_profile(const struct name_value_t* args)
{
	int stat;
	if (!args || !args[0].value)
		return -1;


	if (!strcmp(args[0].value, "write"))
	{
		return handleProfileWrite_ericsson();
	}
	else if (!strcmp(args[0].value, "activate"))
	{
		stat=handleProfileActivate_ericsson();

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}
	else if (!strcmp(args[0].value, "deactivate"))
	{
		stat=handleProfileDeactivate_ericsson();

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int ericsson_phone_reset(void)
{
	int ok, retry = 3;
retry_reset:
	if (at_send("AT*E2RESET", NULL, "", &ok, 0) != 0 || !ok)
	{
		if (retry-- > 0)
			goto retry_reset;
		SYSLOG_ERR("fail to send 'AT*E2RESET' command");
		return -1;
	}
	SYSLOG_ERR("sent 'AT*E2RESET' command");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int ericsson_handle_command_phonesetup(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}
	if (strcmp(args[0].value, "phone reset") == 0)
	{
		return ericsson_phone_reset();
	}
	SYSLOG_DEBUG("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
	return model_run_command_default(args);
}
///////////////////////////////////////////////////////////////////////////////
struct command_t ericsson_commands[] =
{
	{ .name = RDB_PHONE_SETUP".command", .action = ericsson_handle_command_phonesetup },
	{ .name = RDB_BANDCMMAND,		.action = ericsson_handle_command_band },
	{ .name = RDB_PROFILE_CMD,		.action = ericsson_handle_command_profile },
	{ .name = RDB_SIMCMMAND,		.action = ericsson_handle_command_sim },
	{0,}
};

///////////////////////////////////////////////////////////////////////////////
struct model_t model_ericsson =
{
	.name = "Ericsson",

	.init = model_ericsson_init,
	.detect = model_ericsson_detect,

	.get_status = model_default_get_status,
	.set_status = model_ericsson_set_status,

	.commands = ericsson_commands,
	.notifications = model_default_notifications
};


