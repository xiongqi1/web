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
#include "../util/at_util.h"

#include "model_default.h"

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

extern struct model_t model_default;


struct qcmip_data_struct {
int profileId;
int profileValid;
char Home_Addr[18];
char HA_pri_Addr[18];
char HA_sec_Addr[18];
char mn_nai[76];
char dun_nai[76];
int rev_tunneling;
int ha_spi_enable;
int ha_spi;
int aaa_spi_enable;
int aaa_spi;
char HA_shared_secret[37];
char AAA_shared_secret[37];
};

int fusion_update_hwver(void);
int fusion_update_imei(void);

static int service_status=0xff;  //0=> No Service, 1=> In Service
static int prev_in_use_1xrtt = 0;
static char evdo_revision[16] = "N/A";

///////////////////////////////////////////////////////////////////////////////
static int fusion_handle_command_band(const struct name_value_t* args)
{
	// not implemented
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int mip_data_get(struct qcmip_data_struct *mip_data)
{
	int profile_id, ok=0, token_count=0;
	char response[AT_RESPONSE_MAX_SIZE];
	char atcmd_buf[32];
	const char *tokenp;
	char *value;

	if (at_send("AT$QCMIPP?", response, "$QCMIPP", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("$QCMIPP:"))
		return -1;

	value = response + STRLEN("$QCMIPP:");

	profile_id = atoi(value);

	if(profile_id <0 || profile_id >7)
		return -1;

	memset(mip_data, 0, sizeof(struct qcmip_data_struct));

	sprintf(atcmd_buf,"AT$QCMIPGETP=%d",profile_id);
	
	if (at_send(atcmd_buf, response, "", &ok, 0) != 0 || !ok)
		return -1;

/*

	# format
	ProfileId, Valid, MN_HOME_IP_ADDR, HA_PRI_IP_ADDR,HA_SEC_IP_ADDR, MN_NAI, DUN_NAI, MN_REV_TUNNELING,MN_HA_SPI_Enable, MN_HA_SPI, MN_AAA_SPI_Enable, MN_AAA_SPI

	# offline
	0,1,"0.0.0.0","255.255.255.255","255.255.255.255","0000000000@vzw3g.com","0000000000@vzw3g.com",1,1,300,1,2
	Or 1,0,"0.0.0.0","0.0.0.0","0.0.0.0","","",0,0,0,0,0

*/

	token_count = tokenize_at_response(response);

	if(token_count != 12)
		return -1;

	//ProfileId
	tokenp = get_token(response, 0);
	if(tokenp)
	   mip_data->profileId = atoi(tokenp);

	//Valid
	tokenp = get_token(response, 1);
	if(tokenp)
	   mip_data->profileValid = atoi(tokenp);
	if(mip_data->profileValid != 0 && mip_data->profileValid != 1)
	   return -1;

	//MN_HOME_IP_ADDR
	tokenp = get_token(response, 2);
	if(tokenp)
	   strcpy(mip_data->Home_Addr, tokenp);

	//HA_PRI_IP_ADDR
	tokenp = get_token(response, 3);
	if(tokenp)
	   strcpy(mip_data->HA_pri_Addr, tokenp);

	//HA_SEC_IP_ADDR
	tokenp = get_token(response, 4);
	if(tokenp)
	   strcpy(mip_data->HA_sec_Addr, tokenp);

	//MN_NAI
	tokenp = get_token(response, 5);
	if(tokenp)
	   strcpy(mip_data->mn_nai, tokenp);

	//DUN_NAI
	tokenp = get_token(response, 6);
	if(tokenp)
	   strcpy(mip_data->dun_nai, tokenp);

	//MN_REV_TUNNELING
	tokenp = get_token(response, 7);
	if(tokenp)
	   mip_data->rev_tunneling = atoi(tokenp);
	if(mip_data->rev_tunneling != 0 && mip_data->rev_tunneling != 1)
	   return -1;

	//MN_HA_SPI_Enable
	tokenp = get_token(response, 8);
	if(tokenp)
	   mip_data->ha_spi_enable = atoi(tokenp);
	if(mip_data->ha_spi_enable != 0 && mip_data->ha_spi_enable != 1)
	   return -1;

	//MN_HA_SPI
	tokenp = get_token(response, 9);
	if(tokenp)
	   mip_data->ha_spi = atoi(tokenp);
	if(mip_data->ha_spi < 0 || mip_data->ha_spi > 0xFFFFFFFF)
	   return -1;

	//MN_AAA_SPI_Enable
	tokenp = get_token(response, 10);
	if(tokenp)
	   mip_data->aaa_spi_enable = atoi(tokenp);
	if(mip_data->aaa_spi_enable != 0 && mip_data->aaa_spi_enable != 1)
	   return -1;

	//MN_AAA_SPI
	tokenp = get_token(response, 11);
	if(tokenp)
	   mip_data->aaa_spi = atoi(tokenp);
	if(mip_data->aaa_spi < 0 || mip_data->aaa_spi > 0xFFFFFFFF)
	   return -1;

 // TODO: HA_shared_secret, AAA_shared_secret
	mip_data->HA_shared_secret[0] = 0;
	mip_data->AAA_shared_secret[0] = 0;

	return 0;
	
}
///////////////////////////////////////////////////////////////////////////////
static int handleMIPGetcmd()
{
	char mipoutput[1024];
	struct qcmip_data_struct mipdata;

/*
	# RDB return format
	NAI;PRI HA ADDR;SEC HA ADDR;Home ADDR;HA SS;AAA SS;HA SPI;AAA SPI;Rev Tunneling
*/

	if(mip_data_get(&mipdata) != 0)
		goto error;	

	sprintf(mipoutput, "%s;%s;%s;%s;%s;%s;%d;%d;%d",mipdata.mn_nai, mipdata.HA_pri_Addr, mipdata.HA_sec_Addr,
	                                                            mipdata.Home_Addr, mipdata.HA_shared_secret, mipdata.AAA_shared_secret, 
	                                                            mipdata.ha_spi, mipdata.aaa_spi, mipdata.rev_tunneling);

	rdb_set_single(rdb_name(RDB_CDMA_MIPCMDGETDATA, ""), mipoutput);
	rdb_set_single(rdb_name(RDB_CDMA_MIPCMDSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_CDMA_MIPCMDGETDATA, ""), "");
	rdb_set_single(rdb_name(RDB_CDMA_MIPCMDSTATUS, ""), "[error]");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int handleMIPSetcmd()
{
	int ok = 0, token_count;;
	char response[AT_RESPONSE_MAX_SIZE];
	char mipoutput[AT_RESPONSE_MAX_SIZE];
	const char *tokenp;
	struct qcmip_data_struct mipdata;

	if (rdb_get_single(rdb_name(RDB_CDMA_MIPCMDSETDATA, ""), response, sizeof(response)) != 0)
		goto error;

/*
	# RDB return format
	NAI,PRI HA ADDR,SEC HA ADDR,Home ADDR,HA SS,AAA SS,HA SPI,AAA SPI,Rev Tunneling
*/

	if(mip_data_get(&mipdata) != 0)
		goto error;


	token_count = tokenize_with_semicolon(response);

	if(token_count != 9)
	   goto error;
//Network Access Identifier
	tokenp = get_token(response, 0);
	if(!tokenp) {
	   strcpy(mipdata.mn_nai, "");
	}
	else {
	   strcpy(mipdata.mn_nai, tokenp);
	}
//Primary Home Agent Address
	tokenp= get_token(response, 1);
	if(!tokenp) {
	   strcpy(mipdata.HA_pri_Addr, "");
	}
	else {
	   strcpy(mipdata.HA_pri_Addr, tokenp);
	}
//Secondary Home Agent Address
	tokenp= get_token(response, 2);
	if(!tokenp) {
	   strcpy(mipdata.HA_sec_Addr, "");
	}
	else {
	   strcpy(mipdata.HA_sec_Addr, tokenp);
	}
//Home Address
	tokenp= get_token(response, 3);
	if(!tokenp) {
	   strcpy(mipdata.Home_Addr, "");
	}
	else {
	   strcpy(mipdata.Home_Addr, tokenp);
	}
//Home Agent Shared Secret
	tokenp= get_token(response, 4);
	if(!tokenp) {
	   strcpy(mipdata.HA_shared_secret, "");
	}
	else {
	   strcpy(mipdata.HA_shared_secret, tokenp);
	}
//AAA Server Shared Secret
	tokenp= get_token(response, 5);
	if(!tokenp) {
	   strcpy(mipdata.AAA_shared_secret, "");
	}
	else {
	   strcpy(mipdata.AAA_shared_secret, tokenp);
	}
//Home Agent Security Parameter Index
	tokenp= get_token(response, 6);
	if(!tokenp)
	   goto error;
	mipdata.ha_spi = atoi(tokenp);
//AAA Server Security Parameter Index
	tokenp= get_token(response, 7);
	if(!tokenp)
	   goto error;
	mipdata.aaa_spi = atoi(tokenp);
//Reverse Tunneling Preference
	tokenp= get_token(response, 8);
	if(!tokenp)
	   goto error;
	mipdata.rev_tunneling = atoi(tokenp);

	sprintf(mipoutput,"AT$QCMIPSETP=%d,%d,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d",mipdata.profileId,mipdata.profileValid,mipdata.Home_Addr,
	                         mipdata.HA_pri_Addr, mipdata.HA_sec_Addr, mipdata.mn_nai, mipdata.dun_nai, mipdata.rev_tunneling, mipdata.ha_spi_enable,
	                         mipdata.ha_spi, mipdata.aaa_spi_enable, mipdata.aaa_spi);

	if (at_send(mipoutput, response, "", &ok, 0) != 0 || !ok)
		goto error;

 // TODO: HA_shared_secret, AAA_shared_secret

	if(mip_data_get(&mipdata) != 0)
		goto error;	

	sprintf(mipoutput, "%s;%s;%s;%s;%s;%s;%d;%d;%d",mipdata.mn_nai, mipdata.HA_pri_Addr, mipdata.HA_sec_Addr,
	                                                            mipdata.Home_Addr, mipdata.HA_shared_secret, mipdata.AAA_shared_secret, 
	                                                            mipdata.ha_spi, mipdata.aaa_spi, mipdata.rev_tunneling);

	rdb_set_single(rdb_name(RDB_CDMA_MIPCMDGETDATA, ""), mipoutput);
	rdb_set_single(rdb_name(RDB_CDMA_MIPCMDSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_CDMA_MIPCMDGETDATA, ""), "");
	rdb_set_single(rdb_name(RDB_CDMA_MIPCMDSTATUS, ""), "[error]");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int fusion_handle_command_mip(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}

	if (!strcmp(args[0].value, "get")) {
		return handleMIPGetcmd();
	}
	else if (!strcmp(args[0].value, "set")) {
		return handleMIPSetcmd();
	}

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int fusion_handle_command_reset(const struct name_value_t* args)
{
	int ok = 0;
	
	if (!args || !args[0].value)
	{
		return -1;
	}

	if (!strcmp(args[0].value, "reset")) {
		if (at_send("AT+REBOOT", NULL, "", &ok, 0) != 0 || !ok)
		   goto error;
	}
 	else  {
 		goto error;
 	}

 	rdb_set_single(rdb_name(RDB_MODULE_RESETCMDSTATUS, ""), "[done]");
 	return 0;

error:
 	rdb_set_single(rdb_name(RDB_MODULE_RESETCMDSTATUS, ""), "[error]");
	return -1;

}
///////////////////////////////////////////////////////////////////////////////
static int handleAdParamGetcmd()
{
	int ok =0, ret;
	char response[AT_RESPONSE_MAX_SIZE];
	char ret_string[512]="";
	const char *value;

	//[1]Station Class Mark
	ret = at_send("AT+TEMP", response, "+TEMP", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+TEMP:"))) {  //TODO: must modify proper command
		value = response + STRLEN("+TEMP:"); //TODO: must modify proper command
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}
	strcat(ret_string, ";");

	//[2]Slot Mode
	ret = at_send("AT+TEMP", response, "+TEMP", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+TEMP:"))) {  //TODO: must modify proper command
		value = response + STRLEN("+TEMP:"); //TODO: must modify proper command
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}
	strcat(ret_string, ";");

	//[3]Slot Cycle Index 
	ret = at_send("AT+CSSCI?", response, "+CSSCI", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+CSSCI:"))) {
		value = response + STRLEN("+CSSCI:");
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}
	strcat(ret_string, ";");

	//[4]Home System Identification Number
	ret = at_send("AT+TEMP", response, "+TEMP", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+TEMP:"))) {  //TODO: must modify proper command
		value = response + STRLEN("+TEMP:"); //TODO: must modify proper command
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}
	strcat(ret_string, ";");

	//[5]Home Network Identification Number
	ret = at_send("AT+TEMP", response, "+TEMP", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+TEMP:"))) {  //TODO: must modify proper command
		value = response + STRLEN("+TEMP:"); //TODO: must modify proper command
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}
	strcat(ret_string, ";");

	//[6]Mobile Country Code
	ret = at_send("AT+TEMP", response, "+TEMP", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+TEMP:"))) {  //TODO: must modify proper command
		value = response + STRLEN("+TEMP:"); //TODO: must modify proper command
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}
	strcat(ret_string, ";");

	//[7]Mobile Network Code
	ret = at_send("AT+TEMP", response, "+TEMP", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+TEMP:"))) {  //TODO: must modify proper command
		value = response + STRLEN("+TEMP:"); //TODO: must modify proper command
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}
	strcat(ret_string, ";");

	//[8]Access Overload Class
	ret = at_send("AT+TEMP", response, "+TEMP", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+TEMP:"))) {  //TODO: must modify proper command
		value = response + STRLEN("+TEMP:"); //TODO: must modify proper command
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}
	strcat(ret_string, ";");

	//[9]Home System Registration
	ret = at_send("AT+TEMP", response, "+TEMP", &ok, 0);

	if((ret == 0) && (ok == 1) && (strlen(response)>STRLEN("+TEMP:"))) {  //TODO: must modify proper command
		value = response + STRLEN("+TEMP:"); //TODO: must modify proper command
		if(value && value[0] != 0) {
			strcat(ret_string, value);
		}
	}

	rdb_set_single(rdb_name(RDB_CDMA_ADPARABUFFER, ""), ret_string);
	rdb_set_single(rdb_name(RDB_CDMA_ADPARASTATUS, ""), "[done]");
	return 0;

}
///////////////////////////////////////////////////////////////////////////////
static int handleAdParamSetcmd()
{
	int ok =0, token_count;
	char response[512];
	char error_string[512] = "";
	char at_cmd_buf[128];
	const char *tokenp;

	if (rdb_get_single(rdb_name(RDB_CDMA_ADPARABUFFER, ""), response, sizeof(response)) != 0)
		goto error;

	token_count = tokenize_with_semicolon(response);

	//[1]Station Class Mark
	tokenp = get_token(response, 0);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+TEMP=%s", tokenp); //TODO: must modify proper command
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}
	strcat(error_string, ";");
	
	//[2]Slot Mode
	tokenp = get_token(response, 1);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+TEMP=%s", tokenp); //TODO: must modify proper command
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}
	strcat(error_string, ";");

	//[3]Slot Cycle Index
	tokenp = get_token(response, 2);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+CSSCI=%s", tokenp);
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}
	strcat(error_string, ";");

	//[4]Home System Identification Number
	tokenp = get_token(response, 3);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+TEMP=%s", tokenp); //TODO: must modify proper command
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}
	strcat(error_string, ";");

	//[5]Home Network Identification Number
	tokenp = get_token(response, 4);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+TEMP=%s", tokenp); //TODO: must modify proper command
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}
	strcat(error_string, ";");

	//[6]Mobile Country Code
	tokenp = get_token(response, 5);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+TEMP=%s", tokenp); //TODO: must modify proper command
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}
	strcat(error_string, ";");

	//[7]Mobile Network Code
	tokenp = get_token(response, 6);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+TEMP=%s", tokenp); //TODO: must modify proper command
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}
	strcat(error_string, ";");

	//[8]Access Overload Class
	tokenp = get_token(response, 7);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+TEMP=%s", tokenp); //TODO: must modify proper command
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}
	strcat(error_string, ";");

	//[9]Home System Registration
	tokenp = get_token(response, 8);
	if(tokenp && tokenp[0] != 0) {
	   sprintf(at_cmd_buf,"AT+TEMP=%s", tokenp); //TODO: must modify proper command
	   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
	      strcat(error_string,"[error]");
	   }
	   else {
	      strcat(error_string,"[success]");
	   }
	}
	else {
	   strcat(error_string,"[success]");
	}

	rdb_set_single(rdb_name(RDB_CDMA_ADPARAERRCODE, ""), error_string);
	rdb_set_single(rdb_name(RDB_CDMA_ADPARASTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_CDMA_ADPARASTATUS, ""), "[error]");
	return -1;

}
///////////////////////////////////////////////////////////////////////////////
static int fusion_handle_command_advancedparam(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}

	if (!strcmp(args[0].value, "get")) {
		return handleAdParamGetcmd();
	}
	else if (!strcmp(args[0].value, "set")) {
		return handleAdParamSetcmd();
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int handleConfigurationSetcmd()
{
	int ok =0, cmd_param=0xff, loop_cnt=0xff;
	char response[32];
	char error_string[512] = "";
	char at_cmd_buf[128];
	char* value=NULL;

	struct cmd_list
	{
	  char *cmdname;
	  char *atcmdnamd;
	} configcmd[] =
	{
	  {"PRLUpdate", "+PRL"}, // OMA-DM Preferred Roaming List Updates(EMB-15)
	  {"FUMO", "+FUMO"}, // OMA-DM Firmware Updates(EMB-17)
	  {"ROAMPref", "$ROAM"}, // Roaming Preferences(EMB-103)
	  {NULL, NULL}
	};

	if (rdb_get_single(rdb_name(RDB_MODULE_CONFCMDPARAM, ""), response, sizeof(response)) != 0)
		goto error;

	strcpy(error_string, response);

	for(loop_cnt=0 ; configcmd[loop_cnt].cmdname != NULL; loop_cnt++) {
		if((value=strstr(response, configcmd[loop_cnt].cmdname)) != NULL) {
			value = value + strlen(configcmd[loop_cnt].cmdname)+1;
			break;
		}
	}
	if ((value == NULL) || ( configcmd[loop_cnt].cmdname == NULL))
		goto error;

	cmd_param=atoi(value);

	sprintf(at_cmd_buf,"AT%s=%d", configcmd[loop_cnt].atcmdnamd, cmd_param);

	if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok)
		goto error;

	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), error_string);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), error_string);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[error]");
	return -1;

}

///////////////////////////////////////////////////////////////////////////////
static int handleConfigurationGetcmd()
{
	int ok =0, loop_cnt=0xff;
	char response[AT_RESPONSE_MAX_SIZE];
	char error_string[512] = "";
	char at_cmd_buf[128];
	char* value=NULL;

	struct cmd_list
	{
	  char *cmdname;
	  char *atcmdnamd;
	} configcmd[] =
	{
	  {"PRLUpdate", "+PRL"}, // OMA-DM Preferred Roaming List Updates(EMB-15)
	  {"FUMO", "+FUMO"}, // OMA-DM Firmware Updates(EMB-17)
	  {"ROAMPref", "$ROAM"}, // Roaming Preferences(EMB-103)
	  {NULL, NULL}
	};

	if (rdb_get_single(rdb_name(RDB_MODULE_CONFCMDPARAM, ""), response, sizeof(response)) != 0)
		goto error;

	strcpy(error_string, response);

	for(loop_cnt=0 ; configcmd[loop_cnt].cmdname != NULL; loop_cnt++) {
		if(strstr(response, configcmd[loop_cnt].cmdname)) {
			break;
		}
	}

	if (configcmd[loop_cnt].cmdname == NULL)
		goto error;

	sprintf(at_cmd_buf,"AT%s?", configcmd[loop_cnt].atcmdnamd);

	if (at_send(at_cmd_buf, response, configcmd[loop_cnt].atcmdnamd, &ok, 0) != 0 || !ok)
		goto error;

	value =  response + strlen(configcmd[loop_cnt].atcmdnamd)+1;

	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), value);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), error_string);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[error]");
	return -1;

}

///////////////////////////////////////////////////////////////////////////////
static int handlesetActInfocmd()
{
	int ok =0, loop_cnt=0xff, token_count;
	char response[128];
	char result_buf[256] = "";
	char error_string[512] = "";
	char at_cmd_buf[128];
	const char *tokenp;

	struct cmd_list
	{
	  char *atcmdname;
	} configcmd[] =
	{
	  { "AT+TEMP" }, // System Identification (SID)
	  { "AT+TEMP" }, // Network Identification (NID)
	  { "AT+TEMP" }, // Master subsidy lock (MSL)
	  { "AT+VMDN" }, // Mobile directory number (MDN)
	  { "AT+VMIN" }, // International mobile station identify (IMSI)
	  { "AT+TEMP" }, // Simple IP user identification (SIP-ID)
	  { "AT+TEMP" }, // Simple IP password (SIP-Password)
	  { NULL}
	};

	if (rdb_get_single(rdb_name(RDB_MODULE_CONFCMDPARAM, ""), response, sizeof(response)) != 0)
		goto error;

	strcpy(error_string, response);

	token_count = tokenize_with_semicolon(response);

	if (token_count != 7) {
		goto error;
	}

	for(loop_cnt=0 ; configcmd[loop_cnt].atcmdname != NULL; loop_cnt++) {
	
		tokenp = get_token(response, loop_cnt);
		if(tokenp && tokenp[0] != 0) {
		   sprintf(at_cmd_buf,"%s=%s", configcmd[loop_cnt].atcmdname, tokenp); //TODO: must modify proper command
		   if (at_send(at_cmd_buf, NULL, "", &ok, 0) != 0 || !ok) {
		      strcat(result_buf,"[error]");
		   }
		   else {
		      strcat(result_buf,"[success]");
		   }
		}
		else {
		   strcat(result_buf,"[success]");
		}
		strcat(result_buf, ";");
	}

	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), result_buf);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), error_string);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[error]");
	return -1;

}

///////////////////////////////////////////////////////////////////////////////
static int handlegetActInfocmd()
{
	int ok =0, loop_cnt=0xff;
	char response[AT_RESPONSE_MAX_SIZE];
	char result_buf[256] = "";
	char* value=NULL;

	struct cmd_list
	{
	  char *atcmdname;
	  char *responsename;
	} configcmd[] =
	{
	  {"AT+TEMP", "+TEMP"}, // System Identification (SID)
	  {"AT+TEMP", "+TEMP"}, // Network Identification (NID)
	  {"AT+TEMP", "+TEMP"}, // Master subsidy lock (MSL)
	  {"AT+VMDN?", "+VMDN"}, // Mobile directory number (MDN)
	  {"AT+VMIN?", "+VMIN"}, // International mobile station identify (IMSI)
	  {"AT+TEMP", "+TEMP"}, // Simple IP user identification (SIP-ID)
	  {"AT+TEMP", "+TEMP"}, // Simple IP password (SIP-Password)
	  {NULL, NULL}
	};

	for(loop_cnt=0 ; configcmd[loop_cnt].atcmdname != NULL; loop_cnt++) {
		if (at_send(configcmd[loop_cnt].atcmdname, response, configcmd[loop_cnt].responsename, &ok, 0) != 0 || !ok) {
			strcat(result_buf, "[error]");
			strcat(result_buf, ";");
		}
		else{
			if(strlen(response)<strlen(configcmd[loop_cnt].responsename)+1) {
				strcat(result_buf, "[error]");
				strcat(result_buf, ";");
			}
			else {
				value = response + strlen(configcmd[loop_cnt].responsename)+1;
				strcat(result_buf, value);
				strcat(result_buf, ";");
			}
		}
	}


	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), result_buf);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[done]");
	return 0;

}
///////////////////////////////////////////////////////////////////////////////
static int handleCIupdatecmd()
{
	int ok =0, loop_cnt=0xff;
	char response[AT_RESPONSE_MAX_SIZE];
	char error_string[512] = "";
	char at_cmd_buf[128];

	struct cmd_list
	{
	  char *cmdname;
	  char *atcmdnamd;
	  int cmd_arg;
	} configcmd[] =
	{
	  {"CIActivation", "+OMADM", 2}, // Client Initiated Device Configuration session (EMB-12)
	  {"CIPRL", "+PRL", 2}, // Client Initiated PRL Update session(EMB-16)
	  {"CIFUMO", "+FUMO", 2}, // Client Initiated CIFUMO session(EMB-18)
	  {NULL, NULL}
	};

	if (rdb_get_single(rdb_name(RDB_MODULE_CONFCMDPARAM, ""), response, sizeof(response)) != 0)
		goto error;

	strcpy(error_string, response);

	for(loop_cnt=0 ; configcmd[loop_cnt].cmdname != NULL; loop_cnt++) {
		if(strstr(response, configcmd[loop_cnt].cmdname) != NULL) {
			break;
		}
	}

	if ( configcmd[loop_cnt].cmdname == NULL)
		goto error;


	sprintf(at_cmd_buf,"AT%s=%d", configcmd[loop_cnt].atcmdnamd, configcmd[loop_cnt].cmd_arg);

	if (at_send(at_cmd_buf, response, "", &ok, 0) != 0 || !ok)
		goto error;

	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), error_string);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), error_string);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[error]");
	return -1;

}
///////////////////////////////////////////////////////////////////////////////
static int handleCIstatuscmd()
{
	int ok =0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	if (at_send("AT+OMASESS?", response, "+OMASESS", &ok, 0) != 0 || !ok)
		goto error;

	if(strlen(response)<STRLEN("+OMASESS:"))
		return -1;

	value = response + STRLEN("+OMASESS:");

	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), value);
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUSPARAM, ""), "");
	rdb_set_single(rdb_name(RDB_MODULE_CONFSTATUS, ""), "[error]");
	return -1;

}
///////////////////////////////////////////////////////////////////////////////
static int fusion_handle_command_confset(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}

	if (!strcmp(args[0].value, "set")) {
		return handleConfigurationSetcmd();
	}
	else if (!strcmp(args[0].value, "get")) {
		return handleConfigurationGetcmd();
	}
	else if (!strcmp(args[0].value, "setActInfo")) {
		return handlesetActInfocmd();
	}
	else if (!strcmp(args[0].value, "getActInfo")) {
		return handlegetActInfocmd();
	}
	else if (!strcmp(args[0].value, "CIupdate")) {
		return handleCIupdatecmd();
	}
	else if (!strcmp(args[0].value, "CIstatus")) {
		return handleCIstatuscmd();
	}

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int fusion_init(void)
{
	fusion_update_hwver();
//	fusion_update_imei();

	rdb_set_single(rdb_name(RDB_SPRINT_MIPMSL, ""), "1234");
	rdb_set_single(rdb_name(RDB_SPRINT_ADPARAMSL, ""), "4321");

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int fusion_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
	memset(new_status,0,sizeof(*new_status));

	if(status_needed->status[model_status_sim_ready])
	{
		// assume SIM is always okay for CDMA - currently for Fusion 
		new_status->status[model_status_sim_ready]=!0;
		err_status->status[model_status_registered]=0;
	}

	if(status_needed->status[model_status_registered])
	{
		// assume it is always attached for CDMA - currently for Fusion
		new_status->status[model_status_registered]=!0;
		err_status->status[model_status_registered]=0;
	}

	if(status_needed->status[model_status_attached])
	{
		// assume it always fails
		new_status->status[model_status_attached]=1;
		err_status->status[model_status_attached]=0;
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int fusion_update_network_sysinfo()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	int srv_status=0;
	int srv_domain=0;
	int roam_status=0xff;

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

	// roam_status
	value=_getNextToken();
	if(value)
		roam_status=atoi(value);

	if((srv_status !=0) || (roam_status !=0 && roam_status !=1))
	   rdb_set_single(rdb_name(RDB_ROAMING, ""), "");
	else
	   rdb_set_single(rdb_name(RDB_ROAMING, ""), value);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int fusion_update_network_mncmcc()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	if (at_send("AT+VMCCMNC?", response, "+VMCCMNC", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("+VMCCMNC:"))
		return -1;

	value = response + STRLEN("+VMCCMNC:");
	
/*

	# format
	+VMCCMNC:<MccMnc>,<MCC>,<MNC>

	# online
	+VMCCMNC:0,302,64

	# offline
	+VMCCMNC:0,N/A,N/A

*/

	// BS_ID
	value=_getFirstToken(value,",");
	if(!value)
		return -1;

	// MCC - fusion does not need a SIM card that we put network id into SIM MCC and MNC
	value=_getNextToken();
	if(!value || !strcmp("N/A",value))
	{
		rdb_set_single(rdb_name(RDB_PLMNMCC, ""), "");
		rdb_name(RDB_IMSI".plmn_mcc", "");
	}
	else
	{
		rdb_set_single(rdb_name(RDB_PLMNMCC, ""), value);
		rdb_name(RDB_IMSI".plmn_mcc", value);
	}

	// MNC
	value=_getNextToken();
	if(!value || !strcmp("N/A",value))
	{
		rdb_set_single(rdb_name(RDB_PLMNMNC, ""), "");
		rdb_name(RDB_IMSI".plmn_mnc", "");
	}
	else
	{
		rdb_set_single(rdb_name(RDB_PLMNMNC, ""), value);
		rdb_name(RDB_IMSI".plmn_mnc", value);
	}
	

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int fusion_update_network_netpar_aset(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;
	int prev_in_use=0xff, aset_pn=0, aset_ecio=0;

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
	prev_in_use = atoi(value);
	if (prev_in_use>0 &&prev_in_use<8)
	   prev_in_use_1xrtt = prev_in_use;
	else
	   prev_in_use_1xrtt = 0;

	// channel
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_CDMA_1XRTTCHANNEL, ""), "");
	}
	else
	{
		rdb_set_single(rdb_name(RDB_CDMA_1XRTTCHANNEL, ""),value);
	}

	// PN
	value=_getNextToken();
	if(!value)
		return -1;
	else
		aset_pn = atoi(value);

	// SID
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_CDMA_SYSTEMID, ""), "");
	}
	else
	{
		rdb_set_single(rdb_name(RDB_CDMA_SYSTEMID, ""),value);
	}

	// NID
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_CDMA_NETWORKID, ""), "");
	}
	else
	{
		rdb_set_single(rdb_name(RDB_CDMA_NETWORKID, ""),value);
	}

	// slot cycle indes
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_CDMA_SLOTCYCLEIDX, ""), "");
	}
	else
	{
		rdb_set_single(rdb_name(RDB_CDMA_SLOTCYCLEIDX, ""),value);
	}

	// rssi
	value=_getNextToken();
	if(!value || service_status != 1)
	{
		rdb_set_single(rdb_name(RDB_CDMA_1XRTTRSSI, ""), "");
		rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), "");
	}
	else
	{
		sprintf(outmsg,"%ddBm",atoi(value));
		rdb_set_single(rdb_name(RDB_CDMA_1XRTTRSSI, ""), outmsg);
		rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), outmsg);
	}

	// Ec/Io
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_ECIOS0, ""), "");
	}
	else
	{
		aset_ecio = atoi(value);
		sprintf(outmsg,"%.2fdBm",aset_ecio*0.5);
		rdb_set_single(rdb_name(RDB_ECIOS0, ""),outmsg);
	}

	// Tx power
	value=_getNextToken();
	if(!value)
		return -1;

	// Tx Adj
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_CDMA_TXADJ, ""), "");
	}
	else
	{
		sprintf(outmsg,"%.1fdBm",atoi(value)*0.5);
		rdb_set_single(rdb_name(RDB_CDMA_TXADJ, ""),outmsg);
	}

	sprintf(outmsg,"%d,%.2fdBm",aset_pn, aset_ecio*0.5);
	rdb_set_single(rdb_name(RDB_CDMA_1XRTTASET, ""),outmsg);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int fusion_update_network_netpar_cset(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;

	char outmsg[64], ecio_data[16];

	if (at_send("AT+NETPAR=1", response, "+NETPAR", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("+NETPAR:"))
		return -1;

	value = response + STRLEN("+NETPAR:");
	
/*
	# format
	+NETPAR:<candidate PN>,<Candidate Ec/Io>

	# online
	???
	
	# offline
	+NETPAR:0,0

*/
	rdb_set_single(rdb_name(RDB_CDMA_1XRTTCSET, ""),"");

	// candidate PN
	value=_getFirstToken(value,",'");
	if(!value)
		return -1;

	strcpy(outmsg,value);
	strcat(outmsg,",");

	// Candidate Ec/Io
	value=_getNextToken();
	if(!value)
		return -1;

	sprintf(ecio_data,"%.2fdBm",atoi(value)*0.5);

	strcat(outmsg,ecio_data);
	strcat(outmsg,";");

	rdb_set_single(rdb_name(RDB_CDMA_1XRTTCSET, ""),outmsg);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int fusion_update_network_netpar_nset(void)
{
	int ok = 0, loop_cnt;
	char response[AT_RESPONSE_MAX_SIZE];
	char *value;
	const char *tokenp;
	char outmsg[256], ecio_data[16];

	if (at_send("AT+NETPAR=2", response, "", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("+NETPAR:"))
		return -1;

	value = strstr(response, "+NETPAR:");
	
	if(!value)
	   return -1;

	value +=  STRLEN("+NETPAR:");
	
/*
	# format
	+NETPAR: <channel>,<PN>,<SID>,<NID>,<slot cycle indes>,<rssi>,<Ec/Io>,<Txpower>,<Tx Adj><CR><LF>
                   [<neighbor 1 PN>,<neighbor 1 Ec/Io><CR><LF>
                   [<neighbor 2 PN>,<neighbor 2 Ec/Io><CR><LF>
                   ����
                   <neighbor 6 PN>,<neighbor 6 Ec/Io>

	# online
	???
	
	# offline
	+NETPAR:451,0,0,0,0,-79,0,-150,0

*/
	if ((value = strtok(value, "\n")) == NULL) {
	   return -1;
	}

	memset(outmsg, 0 , sizeof(outmsg));
	rdb_set_single(rdb_name(RDB_CDMA_1XRTTNSET, ""), "");

	for(loop_cnt=0; loop_cnt < 6 ; loop_cnt++)
	{
	   	if ((value = strtok(NULL, "\n")) == NULL) {
	      break;
	   }

	   tokenp = _getFirstToken(value,",'");
	   if(!tokenp)
		return -1;

	   strcat(outmsg,tokenp);
	   strcat(outmsg,",");

	   tokenp=_getNextToken();
	   if(!tokenp)
		return -1;
	   
	   sprintf(ecio_data,"%.2fdBm",atoi(tokenp)*0.5);

	   strcat(outmsg,ecio_data);
	   strcat(outmsg,";");
	}

	rdb_set_single(rdb_name(RDB_CDMA_1XRTTNSET, ""),outmsg);
	
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int fusion_update_network_info1(void)
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
	{
		rdb_set_single(rdb_name(RDB_CDMA_EVDODRCREQ, ""), "");
	}
	else
	{
		rdb_set_single(rdb_name(RDB_CDMA_EVDODRCREQ, ""),value);
	}

	// ActiveCount
	value=_getNextToken();
	if(!value)
		return -1;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int fusion_update_network_creg(void)
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
int fusion_update_network_info2(void)
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
	if(strlen(evRev) == 0)
	   strcpy(evdo_revision, "N/A");
	else
	   strcpy(evdo_revision, evRev);

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
int fusion_update_signal_strength(void)
{
	int ok = 0, loop_cnt;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
#if 0
	char outmsg[16];
#endif
	int sqm;
	const char *value;
	const char * fervalue[] = {"         FER <0.01%", "0.01% <= FER < 0.1%", "0.1%  <= FER < 0.5%", "0.5%  <= FER < 1.0%", 
	                                      "1.0%  <= FER < 2.0%", "2.0%  <= FER < 4.0%", "4.0%  <= FER < 8.0%", "8.0%  <= FER       "};

	struct _evdorssi
	{
	  int ret_val;
	  char *evdorssi;
	} EvdoRssitbl[] =
	{
	  {0, " " },
	  {20, "-105dBm" },
	  {40, "-97.5dBm" },
	  {60, "-82.5dBm" },
	  {80, "-67.5dBm" },
	  {99, "-60dBm" },
	  {0xff, NULL }
	};

	if( service_status != 1)
	{
//		rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), "");
		rdb_set_single(rdb_name(RDB_CDMA_EVDOPER, ""), "");
		return 0;
	}

	// CSQ
	if (at_send("AT+CSQ", response, "+CSQ", &ok, 0) != 0 || !ok)
		return -1;

	value = response + STRLEN("+CSQ:");
	value=_getFirstToken(value,",'");
#if 0
	if(!value) 
	{
      	rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), "");
	}
	else 
	{
         	sqm = atoi(value);
         	sprintf(outmsg, "%ddBm",  sqm < 0 || sqm == 99 ? 0 : (sqm * 2 - 113));
      	rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), outmsg);
     }
#endif
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_CDMA_1XRTTPER, ""), "");
	}
	else
	{
		sqm = atoi(value);

		if(sqm>=0 && sqm <=7)
		   rdb_set_single(rdb_name(RDB_CDMA_1XRTTPER, ""),fervalue[sqm]);
		else
		   rdb_set_single(rdb_name(RDB_CDMA_1XRTTPER, ""),"FER is not detectable.");
	}

	if (at_send("AT^HDRCSQ", response, "^HDRCSQ", &ok, 0) != 0 || !ok)
		return -1;

	sqm = atoi(response + STRLEN("^HDRCSQ:"));

	for(loop_cnt=0 ; EvdoRssitbl[loop_cnt].evdorssi != NULL; loop_cnt++) {
		if(EvdoRssitbl[loop_cnt].ret_val == sqm)
			break;
	}

	if ( EvdoRssitbl[loop_cnt].evdorssi == NULL) {
		rdb_set_single(rdb_name(RDB_CDMA_EVDORSSI, ""), "");
	}
	else {
		rdb_set_single(rdb_name(RDB_CDMA_EVDORSSI, ""), EvdoRssitbl[loop_cnt].evdorssi);
	}

	if (at_send("AT+EVPKT", response, "+EVPKT", &ok, 0) != 0 || !ok)
		return -1;

	value = response + STRLEN("+EVPKT:");
	
/*
	# format
	+EVPKT:<EV_RxPACKETThroughput>,<EV_RxPER>,<EV_TxPacketThroughput>,<EV_TxPER>,<EV_SU_Packet_Thr>,<EV_MU_Packet_Thr>,<EV_Rx_PkrThr_Instant>,<EV_Rx_PkrThr_Istant_SU>,<EV_Rx_PkrThr_Instant_MU>

*/

	// EV_RxPACKETThroughput
	value=_getFirstToken(value,",'");
	if(!value)
		return -1;

	// EV_RxPER
	value=_getNextToken();
	if(!value)
	{
		rdb_set_single(rdb_name(RDB_CDMA_EVDOPER, ""), "");
	}
	else
	{
		strcat((char *)value, "%");
		rdb_set_single(rdb_name(RDB_CDMA_EVDOPER, ""),value);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int fusion_update_imei(void)
{
	int ok = 0, mdn_len=0;
	char response[ AT_RESPONSE_MAX_SIZE ];
	char mdn_string[16] = "0000000000";
	char* value;
	char esn_buf[16];
	
	//ESN
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

	sprintf(esn_buf,"0x%s", value);

	if (rdb_set_single(rdb_name(RDB_IMEI, ""), esn_buf) < 0)
		SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(RDB_IMSI".msin", ""), esn_buf, strerror(errno));


	//Mobile Directory Number
	if (at_send("AT+VMDN?", response, "+VMDN", &ok, 0) != 0 || !ok)
	{
		SYSLOG_DEBUG("'%s' not available and scheduled", "AT+VMDN?");
		goto error;
	}

	if(strlen(response)<STRLEN("+VMDN:"))
	{
		SYSLOG_DEBUG("'%s' incorrect result", "AT+VMDN?");
		goto error;
	}

	value = response + STRLEN("+VMDN:");
	mdn_len = strlen(value);
	if( mdn_len >0 && mdn_len <= 10) {
	   strcpy(mdn_string+(10-mdn_len), value);
	   memmove(mdn_string+4, mdn_string+3, 8);
	   memset(mdn_string+3, '-', 1);
	   memmove(mdn_string+8, mdn_string+7, 5);
	   memset(mdn_string+7, '-', 1);	}
	else {
	   strcpy(mdn_string, "N/A");
	}

	if (rdb_set_single(rdb_name(RDB_MDN, ""), mdn_string) < 0)
		SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(RDB_MDN, ""), mdn_string, strerror(errno));


	//Mobile Subscriber Identification
	if (at_send("AT+VMIN?", response, "+VMIN", &ok, 0) != 0 || !ok)
	{
		SYSLOG_DEBUG("'%s' not available and scheduled", "AT+VMIN?");
		goto error;
	}

	if(strlen(response)<STRLEN("+VMIN:"))
	{
		SYSLOG_DEBUG("'%s' incorrect result", "AT+VMIN?");
		goto error;
	}

	value = response + STRLEN("+VMIN:");

	if (rdb_set_single(rdb_name(RDB_MSID, ""), value) < 0)
		SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(RDB_MSID, ""), value, strerror(errno));

	if((strlen(mdn_string) != 12) || (strlen(value) != 10)) {
	  rdb_set_single(rdb_name(RDB_MODULE_ACTIVATED, ""), "0");
	}
	else {
	  rdb_set_single(rdb_name(RDB_MODULE_ACTIVATED, ""), "1");
	}

	return 0;

error:
	rdb_set_single(rdb_name(RDB_MODULE_ACTIVATED, ""), "0");
	return -1;
}
////////////////////////////////////////////////////////////////////////////////
void fusion_update_sim()
{
	rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM OK");
}

////////////////////////////////////////////////////////////////////////////////
int fusion_update_hwver()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char* value;

	// module hardware version
	if (at_send("AT^HWVER", response, "^HWVER", &ok, 0) != 0 || !ok)
		return -1;

	value = response + STRLEN("^HWVER:");
	sprintf(response, "%s", value);
	return rdb_set_single(rdb_name(RDB_HARDWARE_VERSION, ""), response);
}
////////////////////////////////////////////////////////////////////////////////
int fusion_update_module_info_section()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char* value;
	const char * in_use_1xrtt_network[] = {"N/A","JSTD008", "N/A", "IS95A", "IS95B", "N/A", "IS2000", "IS2000 Rel A"};

	// Network Access Identifier(EMB-89)
	if (at_send("AT+SIPNAI", response, "+SIPNAI", &ok, 0) != 0 || !ok)
		return -1;

	value = response + STRLEN("+SIPNAI:");
	rdb_set_single(rdb_name(RDB_NAI, ""), value);

	// Available Data Networks( EMB-93)
	if(service_status == 1) {
	   if(strcmp(evdo_revision, "N/A")) {
	      strcpy(response,"EVDO ");
	      strcat(response, evdo_revision);
	   }
	   else {
	      strcpy(response, in_use_1xrtt_network[prev_in_use_1xrtt]);
	   }
	}
	else {
	   strcpy(response,"N/A");
	}

	rdb_set_single(rdb_name(RDB_AVAIL_DATA_NETWORK, ""), response);

	// Preferred Roaming List Version(EMB-86)
	if (at_send("AT+VPRLID?", response, "+VPRLID", &ok, 0) != 0 || !ok)
		return -1;

	value = response + STRLEN("+VPRLID:");
	rdb_set_single(rdb_name(RDB_PRL_VERSION, ""), value);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int fusion_update_current_band_class()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char *value;
	int curret_band_class=0xff;
	const char * st_cur_band_class[] = {"follow PRL","1900 Mhz", "800 Mhz"};


	if (at_send("AT+VDEBINFO?", response, "+VDEBINFO", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("+VDEBINFO:"))
		return -1;

	value = response + STRLEN("+VDEBINFO:");
	
/*
	# format
	+VDEBINFO: <SID>,<NID>,<CH>,<CURRENT BAND>, <FER>, <TXP>, <RXP>,<usedSCI>, <namSCI>,<maxSCI>

	# online
	+VDEBINFO:4145,7,25,1, 0.00%,-150,-69,2,2,2
	
	# offline
	+VDEBINFO:0,0,111,0, 0.00%,-150,-150,0,2,0

*/

	// SID - Current System ID
	value=_getFirstToken(value,",'");
	if(!value)
		return -1;

	// NID - Current Network ID
	value=_getNextToken();
	if(!value)
		return -1;

	// CH - Current Frequency Channel
	value=_getNextToken();
	if(!value)
		return -1;

	// CURRENT BAND - Current Band Class
	value=_getNextToken();
	if(!value)
		return -1;

	curret_band_class = atoi(value);

	if (curret_band_class >= 0 && curret_band_class <= 2)
		rdb_set_single(rdb_name(RDB_CDMA_BANDCLASS, ""), st_cur_band_class[curret_band_class]);
	else
		rdb_set_single(rdb_name(RDB_CDMA_BANDCLASS, ""), "");

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int fusion_update_check_noservice()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char* value;
	int inservice;

	if (at_send("AT+CAD?", response, "+CAD", &ok, 0) != 0 || !ok)
		return -1;

	value = response + STRLEN("+CAD:");
	inservice = atoi(value);

	if(inservice == 0)
	   service_status = 0;  //No Service
	else if ((inservice == 1) ||(inservice == 3))
	   service_status = 1; // In Service
	else
	   service_status = 0xff; //Error

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int fusion_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
//move to fusion_init()	fusion_update_hwver();

	fusion_update_check_noservice();
	fusion_update_network_mncmcc();

	fusion_update_network_netpar_aset();
	fusion_update_network_netpar_cset();
	fusion_update_network_netpar_nset();
	fusion_update_network_info1();
	fusion_update_network_info2();
	fusion_update_network_creg();

	fusion_update_signal_strength();

	fusion_update_network_sysinfo();

	fusion_update_module_info_section();
	fusion_update_current_band_class();

	fusion_update_imei();

	fusion_update_sim();

/*
	update_network_name();
	update_service_type();

	fusion_update_network_status();
*/

	return 0;
}

char *strcasestr(const char *haystack, const char *needle);

////////////////////////////////////////////////////////////////////////////////
static int fusion_detect(const char* manufacture, const char* model_name)
{
	// TODO: we may need to specify model_name as well
	if(strcasestr(manufacture,"VIA TeleCom") || strcasestr(manufacture,"Fusion Wireless"))
		return 1;

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static int handle_ccnt_notification(const char* s)
{
	int service_option = 0;
	const char* begin = s + strlen("+CCNT:");

	service_option = atoi(begin);

	if(service_option != 0)
	   rdb_set_single(rdb_name(RDB_CDMA_SERVICEOPTION, ""), begin);
	else
	   rdb_set_single(rdb_name(RDB_CDMA_SERVICEOPTION, ""), "N/A");

	rdb_set_single(rdb_name(RDB_CONNECTION_STATUS, ""), "Connected");

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int handle_cend_notification(const char* s)
{
	rdb_set_single(rdb_name(RDB_CDMA_SERVICEOPTION, ""), "N/A");
	rdb_set_single(rdb_name(RDB_CONNECTION_STATUS, ""), "Disconnected");

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int handle_vdormant_notification(const char* s)
{
	int packet_status = 0;
	const char* begin = s + strlen("+VDORMANT:");

	packet_status = atoi(begin);

	if(packet_status == 1)
	   return rdb_set_single(rdb_name(RDB_CONNECTION_STATUS, ""), "Dormant");
	else if(packet_status == 2)
	   return rdb_set_single(rdb_name(RDB_CONNECTION_STATUS, ""), "Active");

	return -1;
}

////////////////////////////////////////////////////////////////////////////////
struct command_t fusion_commands[] =
{
	{ .name = RDB_BANDCMMAND,		.action = fusion_handle_command_band },
	{ .name = RDB_CDMA_MIPCMMAND,		.action = fusion_handle_command_mip },
	{ .name = RDB_MODULE_RESETCMD,		.action = fusion_handle_command_reset },
	{ .name = RDB_CDMA_ADPARACMMAND,		.action = fusion_handle_command_advancedparam },
	{ .name = RDB_MODULE_CONFCMD,		.action = fusion_handle_command_confset },

	{0,}
};

///////////////////////////////////////////////////////////////////////////////
const struct notification_t fusion_notifications[] =
{
	  { .name = "+CCNT", .action = handle_ccnt_notification }
	, { .name = "+CEND", .action = handle_cend_notification }
	, { .name = "+VDORMANT", .action = handle_vdormant_notification }

//	, { .name = "+CEND", .action = notiDummy }
				     
	, {0, } // zero-terminator
                             };

////////////////////////////////////////////////////////////////////////////////
struct model_t model_fusion = {
	.name = "fusion",
	.init = fusion_init,
	.detect = fusion_detect,

	.get_status = fusion_get_status,
	.set_status = fusion_set_status,

	.commands = fusion_commands,
	.notifications = fusion_notifications
};

////////////////////////////////////////////////////////////////////////////////
