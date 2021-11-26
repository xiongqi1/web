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
#include <string.h>
#include <alloca.h> 
#include <errno.h>

#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "../util/rdb_util.h"

#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../util/scheduled.h"

#include "model_default.h"

#ifdef PLATFORM_PLATYPUS
#include <nvram.h>
#endif

//#define IPW_DEBUG

#define IPW_TIMEOUT 10

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

extern struct model_t model_default;

char *strcasestr(const char *haystack, const char *needle);
int _getTokensWithDeli(char* atResp,int atPrefixCnt,char* tokenTbl[],int tblCnt,int tokenSize,const char* userDeli);
int _getTokens(char* atResp,int atPrefixCnt,char* tokenTbl[],int tblCnt,int tokenSize);
int stripStr(char* szValue);
static int handle_command_freq(const struct name_value_t* args);
static char* _stripHeadTail(const char* str,const char* headtail);
static int handle_command_profile(const struct name_value_t* args);

////////////////////////////////////////////////////////////////////////////////
static int setFreq(const char* freq)
{
	// build at command
	char atCmd[AT_RESPONSE_MAX_SIZE];
	int ok;

	if( !strcmp(freq,"auto") || !strcmp(freq,"") )
		snprintf(atCmd,sizeof(atCmd),"AT+CGATT=0");
	else
		snprintf(atCmd,sizeof(atCmd),"AT+CGATT=1,%s",freq);

	if(at_send_with_timeout(atCmd, NULL, "", &ok,IPW_TIMEOUT, 0))
		return -1;

	if(!ok)
		return -1;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int ipw_init(void)
{
	// read freq at begining
	struct name_value_t nameVal;
	nameVal.value="read";
	handle_command_freq(&nameVal);

	#ifdef PLATFORM_PLATYPUS
	nvram_init(RT2860_NVRAM);

	char* nwMode=nvram_get(RT2860_NVRAM, "wwan_nwsearch");
	char* freq = nvram_get(RT2860_NVRAM, "wwan_freq");
	char* authMode = nvram_get(RT2860_NVRAM, "wwan_authmode");

	if(!strcmp(nwMode,"change"))
		setFreq(freq);
	else
		setFreq("");

	if(!strcmp(authMode,"unpw"))
	{
		char* val;

		// copy NVRAM into RDB - APN
		val=nvram_get(RT2860_NVRAM, "wwan_APN");
		rdb_set_single(rdb_name(RDB_PROFILE_APN, ""), val);
		nvram_strfree(val);
		// copy NVRAM into RDB - USER
		val=nvram_get(RT2860_NVRAM, "wwan_user");
		rdb_set_single(rdb_name(RDB_PROFILE_USER, ""), val);
		nvram_strfree(val);
		// copy NVRAM into RDB - PASS
		val=nvram_get(RT2860_NVRAM, "wwan_pass");
		rdb_set_single(rdb_name(RDB_PROFILE_PW, ""), val);
		nvram_strfree(val);
		// write into the dongle
		nameVal.value="write";
		handle_command_profile(&nameVal);
	}
	
	nvram_strfree(freq);
	nvram_strfree(nwMode);
	nvram_strfree(authMode);

	nvram_close(RT2860_NVRAM);
	#endif

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static void ipw_update_network_name(void)
{
}
////////////////////////////////////////////////////////////////////////////////
static int ipw_update_imei(void)
{
	int ok = 0;
	char value[ AT_RESPONSE_MAX_SIZE ];

	if (at_send_with_timeout("AT+CIMI", value, "", &ok, IPW_TIMEOUT, 0) != 0 || !ok)
		return -1;

	stripStr(value);

	if (rdb_set_single(rdb_name(RDB_IMSI".msin", ""), value) < 0)
		SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(RDB_IMSI".msin", ""), value, strerror(errno));

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int ipw_update_txpwr(void)
{
	char atResp[AT_RESPONSE_MAX_SIZE];
	int ok;

	const char* atCmd="AT+LSTATUS?";

	if( at_send_with_timeout(atCmd, atResp, "", &ok, IPW_TIMEOUT, 0) )
	{
		SYSLOG_ERR("failed to write port - %s",atCmd);
		goto err;
	}

	if(!ok)
	{
		SYSLOG_ERR("AT command error occured - %s",atCmd);
		goto err;
	}

	char* lines[6];
	int i;
	for(i=0;i<6;i++)
		lines[i]=alloca(256);

	_getTokensWithDeli(atResp,0,lines,6,256,"+");

/*
+LSTATUS: TxPwr: 0 dBm

+LSTATUS: (R99 stats) 0,0,0, 0,0,0, 0,0,0

+LSTATUS: (ER7 stats) 0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0

OK
*/

	int txPower=0;
	if(strlen(lines[1])>sizeof("+LSTATUS: TxPwr:"))
	{
		const char* txPowerStr=lines[1]+sizeof("LSTATUS: TxPwr:");
		txPower=atoi(txPowerStr);
	}

	// put tx power
	char bufTxPower[128];
	sprintf(bufTxPower,"%d",txPower);
	rdb_set_single(rdb_name(RDB_NETWORK, "txpwr"), bufTxPower);

	// put lstatus r99
	if(strlen(lines[2])>sizeof("LSTATUS: (R99 stats)"))
		rdb_set_single(rdb_name(RDB_NETWORK, "lstatus_r99"), lines[2]+sizeof("LSTATUS: (R99 stats)"));
	else
		rdb_set_single(rdb_name(RDB_NETWORK, "lstatus_r99"), "");

	// put lstatus er7
	if(strlen(lines[3])>sizeof("+LSTATUS: (ER7 stats)"))
		rdb_set_single(rdb_name(RDB_NETWORK, "lstatus_er7"), lines[3]+sizeof("LSTATUS: (ER7 stats)"));
	else
		rdb_set_single(rdb_name(RDB_NETWORK, "lstatus_er7"), "");

	return 0;

err:
	return -1;
}
////////////////////////////////////////////////////////////////////////////////
int ipw_update_plmnid()
{
	const char* atCmd="AT+COPS?";
	char atResp[AT_RESPONSE_MAX_SIZE];
	int ok;

	// send at command
	if( at_send_with_timeout(atCmd, atResp, "", &ok,IPW_TIMEOUT, 0) )
	{
		SYSLOG_ERR("failed to write port - %s",atCmd);
		goto error;
	}

	// check if ok
	if(!ok)
	{
		SYSLOG_ERR("AT command error occured - %s",atCmd);
		goto error;
	}

	// check result
	char* tokens[3];
	int i;
	for(i=0;i<3;i++)
		tokens[i]=alloca(64);

	_getTokens(atResp,sizeof("+COPS:"),tokens,3,64);
	rdb_set_single(rdb_name(RDB_PLMNID,""),_stripHeadTail(tokens[2],"\""));

	return 0;

error:
	return -1;
}
////////////////////////////////////////////////////////////////////////////////
static int ipw_update_signal_strength(void)
{
	int ok = 0;
	char atResp[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	int rssi;

	int i;
	char* tokens[5];

	int ber;
	int rscp;
	int iscp;
	int bars;

	if (at_send_with_timeout("AT+CSQ", atResp, "+CSQ", &ok, IPW_TIMEOUT, 0) != 0 || !ok)
		return -1;

	/*
	at+csq
	+CSQ: 49,99,-68,-93,5
	*/

	// get tokens
	for(i=0;i<5;i++)
		tokens[i]=alloca(64);
	_getTokens(atResp,sizeof("+CSQ:"),tokens,5,64);

	rssi=atoi(tokens[0]);
	ber=atoi(tokens[1]);
	rscp=atoi(tokens[2]);
	iscp=atoi(tokens[3]);
	bars=atoi(tokens[4]);

	// set rssi
	if(!rssi)
	{
		rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), "");
	}
	else
	{
		sprintf(atResp, "%d dBm",  rssi-116);
		rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), atResp);
	}

	// set bars
	sprintf(atResp, "%d",bars);
	rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, "bars"), atResp);


	// set C over I
	sprintf(atResp, "%d dB", rscp-iscp);

	rdb_set_single(rdb_name(RDB_SIGNALQUALITY, ""), atResp);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int ipw_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	/* TODO : many of the following functions should be moving to init or triggered by chg_status*/

	// update ccid
	update_ccid();

	update_sim_status();
	ipw_update_signal_strength();

	ipw_update_txpwr();
	ipw_update_plmnid();

	ipw_update_imei();

//	if(new_status->status[model_status_registered])
//		update_call_forwarding();

	ipw_update_network_name();

	// read profile
	struct name_value_t nameVal;
	nameVal.value="read";
	handle_command_profile(&nameVal);

	// read freq
	nameVal.value="read";
	handle_command_freq(&nameVal);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int ipw_detect(const char* manufacture, const char* model_name)
{
#ifdef IPW_DEBUG
	return 1;
#else
	return strcasestr(manufacture,"IPWireless Inc")!=0;
#endif
}

struct band_to_freq_t {
	int band;
	int freqTbl[2];
};

const struct band_to_freq_t _bandToFreq[] ={
	{0,    /* 2.5 GHz MMDS frequency band */  {2506000,}},
	{1,    /* 1.9 GHz frequency band */	{1902400,}},
	{2,    /* 3.4 GHz frequency band */	{3483000,}},
	{4,    /* 2.0 GHz frequency band (initially for Walker Wireless) */ {2055600,}},
	{5,    /* 2.0 GHz frequency band (initially for Hong Kong based customer) */ {2012400,}},
	{13,  /* Dual band UE 1900 - 1920 & 2010 - 2025 */ {1902400, 2012400}},
	{17,  /* Dual band 872MHz (FDD 870-876UL/915-921DL) & 1.9GHz */ {918900, 1902400}},
	{22,  /* 872MHz band */ {918900,}},
	{29,  /* 2.3 GHz Max Phy */ {2352600,}},
	{30,  /* 700Mhz Wideband TDD */ {737000,}},
	{31,  /* 700MHz FDD frequency band (FDD 698-716UL/728-746DL or 776-798UL/746-768DL) & 2.5GHz TDD */ {737000, 2506000}},
	{32,  /* 700MHz FDD frequency band */ {737000,}},

	{0,{0,}}
};

////////////////////////////////////////////////////////////////////////////////
static int getFreqByBand(int band,int freqTbl[2])
{
	const struct band_to_freq_t* bandFreq=_bandToFreq;

	while(bandFreq->freqTbl[0])
	{
		if(bandFreq->band==band)
		{
			memcpy(freqTbl,bandFreq->freqTbl,sizeof(bandFreq->freqTbl));
			return 0;
		}

		bandFreq++;
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////
static int handle_command_freq(const struct name_value_t* args)
{
	const char* cmd;

	char atResp[AT_RESPONSE_MAX_SIZE];
	int ok;

	SYSLOG_DEBUG("handle_command_freq() called");
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}

	cmd=args[0].value;

	if(!strcmp(cmd,"read"))
	{
		// +FBAND: 0
		const char* atCmd="AT+FBAND?";

		// send at command
		if( at_send_with_timeout(atCmd, atResp, "", &ok, IPW_TIMEOUT, 0) )
		{
			SYSLOG_ERR("failed to write port - %s",atCmd);
			rdb_set_single(rdb_name(RDB_FREQ,"msg"), "failed to write port");
			rdb_set_single(rdb_name(RDB_FREQ,"status"), "error");
			goto fini;
		}

		// check if ok
		if(!ok)
		{
			SYSLOG_ERR("AT command error occured - %s",atCmd);
			rdb_set_single(rdb_name(RDB_FREQ,"msg"), "AT command error occured");
			rdb_set_single(rdb_name(RDB_FREQ,"status"), "error");
			goto fini;
		}

		// check result
		char* token[1];
		token[0]=alloca(64);
		int tokenCnt=_getTokens(atResp,sizeof("+FBAND:"),token,1,64);
		if(tokenCnt!=1)
		{
			SYSLOG_ERR("incorrect format of %s returned - %s",atCmd,atResp);
			rdb_set_single(rdb_name(RDB_FREQ,"msg"), "incorrect format of returned");
			rdb_set_single(rdb_name(RDB_FREQ,"status"), "error");
			goto fini;
		}

		// convert band to freq
		int freqTbl[2];
		if(getFreqByBand(atoi(token[0]),freqTbl)<0)
		{
			SYSLOG_ERR("incorrect band value (%s) returned - %s",token[0],atResp);
			rdb_set_single(rdb_name(RDB_FREQ,"msg"), "incorrect band value");
			rdb_set_single(rdb_name(RDB_FREQ,"status"), "error");
			goto fini;
		}

		char dbBuf[1024];
		if(freqTbl[1])
			snprintf(dbBuf,sizeof(dbBuf),"%d,%d",freqTbl[0],freqTbl[1]);
		else
			snprintf(dbBuf,sizeof(dbBuf),"%d",freqTbl[0]);

		rdb_set_single(rdb_name(RDB_FREQ, "band"), dbBuf);
		rdb_set_single(rdb_name(RDB_FREQ,"msg"), "success");
		rdb_set_single(rdb_name(RDB_FREQ,"status"), "done");
	}
	else if(!strcmp(cmd,"write"))
	{
		char freq[1024]={0,};

		rdb_get_single(rdb_name(RDB_FREQ,"freq"),freq,sizeof(freq));

		// check if ok
		if(setFreq(freq)<0)
		{
			SYSLOG_ERR("AT command error occured - setFreq");
			rdb_set_single(rdb_name(RDB_FREQ,"msg"), "AT command error occured");
			rdb_set_single(rdb_name(RDB_FREQ,"status"), "error");
			goto fini;
		}

		// read
		struct name_value_t nameVal;
		nameVal.value="read";
		handle_command_freq(&nameVal);
	}
	else
	{
			SYSLOG_ERR("unknown command - %s",cmd);
			rdb_set_single(rdb_name(RDB_PROFILE,"msg"), "unknown command");
			rdb_set_single(rdb_name(RDB_PROFILE,"status"), "error");
	}

fini:
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static char* _stripHeadTail(const char* str,const char* headtail)
{
	static char strippedBuf[AT_RESPONSE_MAX_SIZE];
	const char* contentHead=0;
	const char* contentTail=0;
	int contentLen;

	strippedBuf[0]=0;

	while(*str)
	{
		if(!strchr(headtail,*str))
		{
			if(!contentHead)
				contentHead=str;
			contentTail=str;
		}

		str++;
	}

	contentLen=contentTail-contentHead+1;
	if(contentLen>AT_RESPONSE_MAX_SIZE)
		contentLen=AT_RESPONSE_MAX_SIZE;

	if(contentLen && contentTail)
	{
		strncpy(strippedBuf,contentHead,contentLen);
		strippedBuf[contentLen]=0;
	}
	strippedBuf[AT_RESPONSE_MAX_SIZE-1]=0;

	return strippedBuf;
}
////////////////////////////////////////////////////////////////////////////////
int _getTokensWithDeli(char* atResp,int atPrefixCnt,char* tokenTbl[],int tblCnt,int tokenSize,const char* userDeli)
{
	int tokenIdx;

	const char* braces="\"";
	const char* deli=",";
	int inTheBrace=0;

	if(userDeli)
		deli=userDeli;

	for(tokenIdx=0;tokenIdx<tblCnt;tokenIdx++)
		tokenTbl[tokenIdx][0]=0;

	// bypass if too short
	if(strlen(atResp)<atPrefixCnt)
		return 0;
	
	atResp+=atPrefixCnt;

	tokenIdx=0;
	tokenTbl[tokenIdx]=atResp;
	while(*atResp)
	{
		if(strchr(braces,*atResp))
			inTheBrace=!inTheBrace;

		if(!inTheBrace && strchr(deli,*atResp))
		{
			*atResp++=0;

			if(tokenIdx+1>=tblCnt)
				break;

			tokenTbl[++tokenIdx]=atResp;
		}
		else
		{
			atResp++;
		}
	}

	tokenIdx++;

	return tokenIdx;
}
////////////////////////////////////////////////////////////////////////////////
int _getTokens(char* atResp,int atPrefixCnt,char* tokenTbl[],int tblCnt,int tokenSize)
{
	return _getTokensWithDeli(atResp,atPrefixCnt,tokenTbl,tblCnt,tokenSize,0);
}

////////////////////////////////////////////////////////////////////////////////
static int isIpAddr(const char* ipAddr)
{
	while(*ipAddr)
	{
		if( ! (('0'<=*ipAddr && *ipAddr<='9') || *ipAddr=='.') )
			return 0;

		ipAddr++;
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////
static int handle_command_lstatus(const struct name_value_t* args)
{
	char atResp[AT_RESPONSE_MAX_SIZE];
	int ok;

	const char* cmd;

	SYSLOG_DEBUG("handle_command_ums() called");
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}

	cmd=args[0].value;

	// +UMS: 0,"0.0.0.0","",9001,1440,0,0,0,0

	if(!strcmp(cmd,"reset"))
	{
		const char* atCmd="AT+LSTATUS=0";
		if( at_send_with_timeout(atCmd, atResp, "", &ok, IPW_TIMEOUT, 0) )
		{
			SYSLOG_ERR("failed to write port - %s",atCmd);
			rdb_set_single(rdb_name(RDB_LSTATUS,"msg"), "failed to write port");
			rdb_set_single(rdb_name(RDB_LSTATUS,"status"), "error");
			goto fini;
		}

		if(!ok)
		{
			SYSLOG_ERR("AT command error occured - %s",atCmd);
			rdb_set_single(rdb_name(RDB_LSTATUS,"msg"), "AT command error occured");
			rdb_set_single(rdb_name(RDB_LSTATUS,"status"), "error");
			goto fini;
		}

		ipw_update_txpwr();

		rdb_set_single(rdb_name(RDB_LSTATUS,"msg"), "success");		
		rdb_set_single(rdb_name(RDB_LSTATUS,"status"), "done");		
	}
	else
	{
			SYSLOG_ERR("unknown command - %s",cmd);
			rdb_set_single(rdb_name(RDB_LSTATUS,"msg"), "unknown command");
			rdb_set_single(rdb_name(RDB_LSTATUS,"status"), "error");
	}

fini:
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static int handle_command_ums(const struct name_value_t* args)
{
	const char* cmd;

	char atResp[AT_RESPONSE_MAX_SIZE];
	int ok;

	SYSLOG_DEBUG("handle_command_ums() called");
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}

	cmd=args[0].value;

	// +UMS: 0,"0.0.0.0","",9001,1440,0,0,0,0

	if(!strcmp(cmd,"read"))
	{
		if( at_send_with_timeout("AT+UMS?", atResp, "", &ok, IPW_TIMEOUT, 0) )
		{
			SYSLOG_ERR("failed to write port - AT+UMS?");
			rdb_set_single(rdb_name(RDB_UMS,"msg"), "failed to write port");
			rdb_set_single(rdb_name(RDB_UMS,"status"), "error");
			goto fini;
		}

		if(!ok)
		{
			SYSLOG_ERR("AT command error occured - AT+UMS?");
			rdb_set_single(rdb_name(RDB_UMS,"msg"), "AT command error occured");
			rdb_set_single(rdb_name(RDB_UMS,"status"), "error");
			goto fini;
		}

		char* umsTokens[5];
		int i;
		for(i=0;i<5;i++)
			umsTokens[i]=alloca(64);

		int umsTokenCnt;
		umsTokenCnt=_getTokens(atResp,sizeof("+UMS:"),umsTokens,5,64);
		if(umsTokenCnt<5)
		{
			SYSLOG_ERR("incorrect format of UMS returned - %s",atResp);
			rdb_set_single(rdb_name(RDB_UMS,"msg"), "incorrect format of UMS returned");
			rdb_set_single(rdb_name(RDB_UMS,"status"), "error");
			goto fini;
		}

		rdb_set_single(rdb_name(RDB_UMS,"enable"),umsTokens[0]);
		rdb_set_single(rdb_name(RDB_UMS,"ipaddr"),_stripHeadTail(umsTokens[1],"\""));
		rdb_set_single(rdb_name(RDB_UMS,"name"),_stripHeadTail(umsTokens[2],"\""));
		rdb_set_single(rdb_name(RDB_UMS,"port"),umsTokens[3]);
		rdb_set_single(rdb_name(RDB_UMS,"timeout"),umsTokens[4]);

		rdb_set_single(rdb_name(RDB_UMS,"msg"), "success");
		rdb_set_single(rdb_name(RDB_UMS,"status"), "done");
	}
	else if(!strcmp(cmd,"write"))
	{

		char rdbEn[256]={0,};
		char rdbIpAddr[64]={0,};
		char rdbName[64]={0,};
		char rdbPort[64]={0,};
		char rdbTimeout[64]={0,};
		char atCmd[AT_RESPONSE_MAX_SIZE];

		rdb_get_single(rdb_name(RDB_UMS,"enable"),rdbEn,sizeof(rdbEn));
		rdb_get_single(rdb_name(RDB_UMS,"ipaddr"),rdbIpAddr,sizeof(rdbIpAddr));
		rdb_get_single(rdb_name(RDB_UMS,"name"),rdbName,sizeof(rdbName));
		rdb_get_single(rdb_name(RDB_UMS,"port"),rdbPort,sizeof(rdbPort));
		rdb_get_single(rdb_name(RDB_UMS,"timeout"),rdbTimeout,sizeof(rdbTimeout));

		int umsMode;
		if(!atoi(rdbEn))
			umsMode=0;
		else if (isIpAddr(rdbIpAddr))
			umsMode=1;
		else
			umsMode=2;

		sprintf(atCmd,"AT+UMS=%d,\"%s\",\"%s\",%d,%d",umsMode,rdbIpAddr,rdbName,atoi(rdbPort),atoi(rdbTimeout));

		if( at_send_with_timeout(atCmd, atResp, "", &ok, IPW_TIMEOUT, 0) )
		{
			SYSLOG_ERR("failed to write port - %s",atCmd);
			rdb_set_single(rdb_name(RDB_UMS,"msg"), "failed to write port");
			rdb_set_single(rdb_name(RDB_UMS,"status"), "error");
			goto fini;
		}

		if(!ok)
		{
			SYSLOG_ERR("AT command error occured - %s",atCmd);
			rdb_set_single(rdb_name(RDB_UMS,"msg"), "AT command error occured");
			rdb_set_single(rdb_name(RDB_UMS,"status"), "error");
			goto fini;
		}

		rdb_set_single(rdb_name(RDB_UMS,"msg"), "success");		
		rdb_set_single(rdb_name(RDB_UMS,"status"), "done");		
	}


fini:
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int handle_command_profile(const struct name_value_t* args)
{
	const char* cmd=args[0].value;

	if (!args || !args[0].value)
		return -1;

	char atResp[AT_RESPONSE_MAX_SIZE];
	int ok;

	if (!strcmp(cmd, "read"))
	{
		const char* atCmd="AT+CGDCONT?";

		if( at_send_with_timeout(atCmd, atResp, "", &ok, IPW_TIMEOUT, 0) )
		{
			SYSLOG_ERR("failed to write port - %s",atCmd);
			rdb_set_single(rdb_name(RDB_PROFILE,"msg"), "failed to write port");
			rdb_set_single(rdb_name(RDB_PROFILE,"status"), "error");
			goto fini;
		}

		if(!ok)
		{
			SYSLOG_ERR("AT command error occured - %s",atCmd);
			rdb_set_single(rdb_name(RDB_PROFILE,"msg"), "AT command error occured");
			rdb_set_single(rdb_name(RDB_PROFILE,"status"), "error");
			goto fini;
		}
	
		// +CGDCONT: 1,"PPP","ipwireless.com","357425030062721,default",0,0
		char* cgdcontTokens[4];
		int i;
		for(i=0;i<4;i++)
			cgdcontTokens[i]=alloca(64);

		int cgdcontTokenCnt;
		cgdcontTokenCnt=_getTokens(atResp,sizeof("+CGDCONT:"),cgdcontTokens,4,64);
		if(cgdcontTokenCnt<4)
		{
			SYSLOG_ERR("incorrect format of CGDCONT returned - %s",atResp);
			rdb_set_single(rdb_name(RDB_PROFILE,"msg"), "incorrect format of UMS returned");
			rdb_set_single(rdb_name(RDB_PROFILE,"status"), "error");
			goto fini;
		}

		// APN
		rdb_set_single(rdb_name(RDB_PROFILE_APN, ""),_stripHeadTail(cgdcontTokens[2],"\""));

		// UNPW
		char* unpwTokens[2];
		for(i=0;i<2;i++)
			unpwTokens[i]=alloca(64);

		_getTokens(_stripHeadTail(cgdcontTokens[3],"\""),0,unpwTokens,2,64);

		rdb_set_single(rdb_name(RDB_PROFILE_USER, ""),unpwTokens[0]);
		rdb_set_single(rdb_name(RDB_PROFILE_PW, ""),unpwTokens[1]);

		rdb_set_single(rdb_name(RDB_PROFILE,"msg"), "success");		
		rdb_set_single(rdb_name(RDB_PROFILE,"status"), "done");		
	}
	else if (!strcmp(cmd, "write"))
	{
		char apn[128]={0,};
		char user[128]={0,};
		char pw[128]={0,};
		char auth[128]={0,};

		// at+cgdcont=1,"PPP","ipwireless.com","357425030062721,default",0,0
		rdb_get_single(rdb_name(RDB_PROFILE_APN, ""), apn, sizeof(apn));
		rdb_get_single(rdb_name(RDB_PROFILE_AUTH, ""), auth, sizeof(auth));
		rdb_get_single(rdb_name(RDB_PROFILE_USER, ""), user, sizeof(user));
		rdb_get_single(rdb_name(RDB_PROFILE_PW, ""), pw, sizeof(pw));

		char atCmd[AT_RESPONSE_MAX_SIZE];
		snprintf(atCmd,AT_RESPONSE_MAX_SIZE,"AT+CGDCONT=1,\"PPP\",\"%s\",\"%s,%s\",0,0",apn,user,pw);

		if( at_send_with_timeout(atCmd, atResp, "", &ok,IPW_TIMEOUT, 0) )
		{
			SYSLOG_ERR("failed to write port - %s",atCmd);
			rdb_set_single(rdb_name(RDB_PROFILE,"msg"), "failed to write port");
			rdb_set_single(rdb_name(RDB_PROFILE,"status"), "error");
			goto fini;
		}

		if(!ok)
		{
			SYSLOG_ERR("AT command error occured - %s",atCmd);
			rdb_set_single(rdb_name(RDB_PROFILE,"msg"), "AT command error occured");
			rdb_set_single(rdb_name(RDB_PROFILE,"status"), "error");
			goto fini;
		}

		// read
		struct name_value_t nameVal;
		nameVal.value="read";
		handle_command_profile(&nameVal);
	}
	else
	{
			SYSLOG_ERR("unknown command - %s",cmd);
			rdb_set_single(rdb_name(RDB_PROFILE,"msg"), "unknown command");
			rdb_set_single(rdb_name(RDB_PROFILE,"status"), "error");
	}

fini:
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int ipw_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
	char* resp;

	memset(new_status,0,sizeof(*new_status));

	if(status_needed->status[model_status_sim_ready])
	{
		resp=at_bufsend_with_timeout("AT+CPIN?","+CPIN: ",IPW_TIMEOUT);

		err_status->status[model_status_sim_ready]=!resp;

		/* write sim pin */
		if(resp)
		{
			SYSLOG_DEBUG("cpin = %s",resp);
			model_default_write_sim_status(resp);
			new_status->status[model_status_sim_ready]=!strcmp(resp,"READY");
		}
	}

	if(status_needed->status[model_status_registered])
	{
		int nw_stat;

		at_send_with_timeout("AT+CREG=2",NULL,"",NULL, IPW_TIMEOUT, 0);
		if( (resp=at_bufsend_with_timeout("AT+CREG?","+CREG: ",IPW_TIMEOUT))!=0 )
		{
			char* cregTokens[4];
			int i;
			for(i=0;i<4;i++)
				cregTokens[i]=alloca(64);

			// 2,"0000","0028",6
			_getTokens(resp,0,cregTokens,4,64);
			nw_stat=atoi(cregTokens[0]);

			// status
			new_status->status[model_status_registered]=(nw_stat==1) || (nw_stat==5);

			char* regStatMsg[]={
				"not registered, ME is not currently searching a new operator to register to", // 0
				"registered, home PLMN", // 1
				"not registered, but ME is currently searching a new operator to register to", // 2
				"registration denied", // 3
				"unknown", // 4
				"registered, roaming", // 5
			};

			if(nw_stat<0 || nw_stat>5)
				rdb_set_single(rdb_name(RDB_REGSTATUS,""),regStatMsg[4]);
			else
				rdb_set_single(rdb_name(RDB_REGSTATUS,""),regStatMsg[nw_stat]);

			// put roaming status
			const char* szRoaming;
			if (nw_stat==5)
				szRoaming = "active";
			else
				szRoaming = "deactive";
			rdb_set_single(rdb_name(RDB_ROAMING, ""), szRoaming);

			// put network status
			int utran=atoi(cregTokens[3]);

			if(utran==2)
			{
				rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "UTRAN");
				rdb_set_single(rdb_name(RDB_SERVICETYPE_EXT, ""), "R99");
			}
			else if(utran==6)
			{
				rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "UTRAN w/HSDPA and HSUPA");
				rdb_set_single(rdb_name(RDB_SERVICETYPE_EXT, ""), "ER7");
			}
			else
			{
				rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "N/A");
				rdb_set_single(rdb_name(RDB_SERVICETYPE_EXT, ""), "N/A");
			}
				

			// put area code
			rdb_set_single(rdb_name(RDB_NETWORK, "areacode"), _stripHeadTail(cregTokens[1],"\""));
		}

		err_status->status[model_status_registered]=!resp;
	}

	if(status_needed->status[model_status_attached])
	{
		if( (resp=at_bufsend_with_timeout("AT+CGATT?","+CGATT: ",IPW_TIMEOUT)) !=0 )
		{
			char* cgattTokens[5];
			int i;
			for(i=0;i<5;i++)
				cgattTokens[i]=alloca(64);

			// +CGATT: -, -, -, -
			_getTokens(resp,0,cgattTokens,5,64);
			int att_stat=cgattTokens[0][0] && cgattTokens[0][0]!='-';

			if(att_stat)
			{
				// put freq
				rdb_set_single(rdb_name(RDB_WANFREQ, ""), cgattTokens[0]);
				// put current band
				rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), cgattTokens[1]);
				rdb_set_single(rdb_name(RDB_CHIPRATE, ""), cgattTokens[1]);
				// put toffset
				rdb_set_single(rdb_name(RDB_TOFFSET, ""), cgattTokens[2]);
				// put cell id
				rdb_set_single(rdb_name(RDB_NETWORK, "cellid"), cgattTokens[3]);
			}
			else
			{
				// put freq
				rdb_set_single(rdb_name(RDB_WANFREQ, ""), "");
				// put current band
				rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
				rdb_set_single(rdb_name(RDB_CHIPRATE, ""), "");
				// put toffset
				rdb_set_single(rdb_name(RDB_TOFFSET, ""), "");
				// put cell id
				rdb_set_single(rdb_name(RDB_NETWORK, "cellid"), "");
			}

			new_status->status[model_status_attached]=att_stat;
		}

		err_status->status[model_status_attached]=!resp;
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
static int ipw_update_sim_stat()
{
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;

	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0)
		return -1;

	if (!ok)
	{
		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM not inserted");
	}
	else
	{
		const char* szSIMStat = response + strlen("+CPIN: ");

		if (!strcmp(szSIMStat, "READY") || !strcmp(szSIMStat, "USIM READY"))
			szSIMStat = "SIM OK";

		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), szSIMStat);
	}

	static int fPrevReady = 0;
	int fReady = ok && (strstr(response, "READY") != 0);

	if (!fPrevReady && fReady)
	{
		SYSLOG_INFO("SIM becomes ready - calling physinit()");
		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");
	}

	fPrevReady = fReady;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int ipw_verify_pin(const char* szPin)
{
	int ok = 0;
	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];

	sprintf(command, "AT+CPIN=\"%s\"", szPin);
	if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
		goto error;

	return 0;

error:
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int ipw_handle_command_sim(const struct name_value_t* args)
{
	int ok = 0;
	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[128];

	char newpin[16];
	char simpuk[16];
	char pin[16];

	char* szCmdStat = NULL;

	int fEn;

	int rebootModule=0;

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

	int fSIMReady = !strcmp(response, "+CPIN: READY") || !strcmp(response, "+CPIN: USIM READY");
	int fSIMPin = !strcmp(response, "+CPIN: SIM PIN") || !strcmp(response, "+CPIN: USIM PIN");

	// read pin
	if (rdb_get_single(rdb_name(RDB_SIMPARAM, ""), pin, sizeof(pin)) != 0)
		pin[0] = 0;

	convClearPwd(pin);

	// sim command - check
	if (memcmp(args[0].value, "check", STRLEN("check")) == 0)
	{
		if (at_send("at+clck=\"SC\",2", response, "", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "%s has failed", "at+clck=\"SC\",2");
			szCmdStat = buf;
			goto error;
		}

		// +CLCK: 0
		fEn = atoi(response + strlen("+CLCK: "));

		rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), fEn ? "Enabled" : "Disabled");
	}
	// sim command - verifypin
	else if (!memcmp(args[0].value, "verifypin", STRLEN("verifypin")))
	{
		if (fSIMPin)
		{
			if (ipw_verify_pin(pin) < 0)
			{
				sprintf(buf, "%s\"%s\" has failed", "AT+CPIN=", pin);
				szCmdStat = buf;
				strcpy((char *)&last_failed_pin, pin);
				goto error;
			} else {
				(void) memset((char *)&last_failed_pin, 0x00, 16);
			}
		}
	}
	// sim command - enablepin
	else if (memcmp(args[0].value, "enablepin", STRLEN("enablepin")) == 0)
	{
		SYSLOG_ERR("%s: enablepin",__FUNCTION__);

		if (at_send("at+clck=\"SC\",2", response, "", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "%s has failed", "at+clck=\"SC\",2");
			szCmdStat = buf;
			goto error;
		}

		fEn = atoi(response + strlen("+CLCK: "));
		if(!fEn)
		{
			SYSLOG_ERR("%s: sending clck",__FUNCTION__);

			// enable pin
			sprintf(command, "AT+CLCK=\"SC\",1,\"%s\"", pin);
			if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
			{
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}

			// need delay until it applies
			sleep(10);

			SYSLOG_ERR("%s: sending cpin",__FUNCTION__);

			// get pin status
			sprintf(command, "AT+CPIN?");
			if (at_send(command, response, "", &ok, 0) != 0 || !ok)
			{
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}
			
			if(!strcmp(response,"+CPIN: USIM READY"))
			{
				SYSLOG_ERR("%s: USIM READY detected",__FUNCTION__);
				// success
			}
			else if(!strcmp(response,"+CPIN: USIM PIN"))
			{
				SYSLOG_ERR("%s: USIM PIN detected#1",__FUNCTION__);
#ifdef PLATFORM_PLATYPUS
				SYSLOG_ERR("%s: USIM PIN detected#2",__FUNCTION__);
				// clear pin because the pin is wrong
				nvram_init(RT2860_NVRAM);
				nvram_set(RT2860_NVRAM, "wwan_pin","");
				nvram_close(RT2860_NVRAM);
#endif

				// reboot for IPW - there is no other way to get PIN status back to READY
				rebootModule=1;

				szCmdStat = "password incorrect";
				goto error;
			}
			else
			{
				SYSLOG_ERR("%s: USIM unknown",__FUNCTION__);

				szCmdStat = "password incorrect";
				goto error;
			}
		}

	}
	// sim command - disablepin
	else if (memcmp(args[0].value, "disablepin", STRLEN("disablepin")) == 0)
	{
		if (at_send("at+clck=\"SC\",2", response, "", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "%s has failed", "at+clck=\"SC\",2");
			szCmdStat = buf;
			goto error;
		}

		fEn = atoi(response + strlen("+CLCK: "));

		if(fEn)
		{
			sprintf(command, "AT+CLCK=\"SC\",0,\"%s\"", pin);
			if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
			{
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}

			// need delay until it applies
			sleep(10);

			// get pin status
			sprintf(command, "AT+CPIN?");
			if (at_send(command, response, "", &ok, 0) != 0 || !ok)
			{
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}

			if(!strcmp(response,"+CPIN: USIM READY"))
			{
				// success
			}
			else if(!strcmp(response,"+CPIN: USIM PIN"))
			{
#ifdef PLATFORM_PLATYPUS
				// clear pin because the pin is wrong
				nvram_init(RT2860_NVRAM);
				nvram_set(RT2860_NVRAM, "wwan_pin","");
				nvram_close(RT2860_NVRAM);
#endif

				// reboot for IPW - there is no other way to get PIN status back to READY
				rebootModule=1;

				szCmdStat = "password incorrect";
				goto error;
			}
			else
			{
				szCmdStat = "password incorrect";
				goto error;
			}
		}
	}
	// sim command - verifypuk
	else if (memcmp(args[0].value, "verifypuk", STRLEN("verifypuk")) == 0)
	{
		if (!fSIMReady)
		{
			if (rdb_get_single(rdb_name(RDB_SIMNEWPIN, ""), newpin, sizeof(newpin)) != 0)
			{
				sprintf(buf, "failed to get %s", rdb_name(RDB_SIMNEWPIN, ""));
				szCmdStat = buf;
				goto error;
			}
			if (rdb_get_single(rdb_name(RDB_SIMPUK, ""), simpuk, sizeof(simpuk)) != 0)
			{
				sprintf(buf, "failed to get %s", rdb_name(RDB_SIMPUK, ""));
				szCmdStat = buf;
				goto error;
			}
			sprintf(command, "AT+CPIN=\"%s\",\"%s\"", simpuk, newpin);
			if (at_send(command, response, "", &ok, 0) != 0 || !ok)
			{
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}
		}
	}
	else
	{
		SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
		goto error;
	}

	ipw_update_sim_stat();

	rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "Operation succeeded");

	SYSLOG_ERR("%s: success",__FUNCTION__);

	return 0;

error:
	ipw_update_sim_stat();

	if (szCmdStat)
		rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), szCmdStat);

	if(rebootModule)
	{
		// reboot for IPW - there is no other way to get PIN status back to READY
		sprintf(command, "AT^C7");
		at_send(command, response, "OK", &ok, 0);
	}

	SYSLOG_ERR("%s: error",__FUNCTION__);

	return -1;
}
////////////////////////////////////////////////////////////////////////////////
struct command_t ipw_commands[] =
{
	{ .name = RDB_UMS ".command",			.action = handle_command_ums },
	{ .name = RDB_FREQ ".command",		.action = handle_command_freq },
	{ .name = RDB_PROFILE_CMD,	  		.action = handle_command_profile },
	{ .name = RDB_LSTATUS ".command",	.action = handle_command_lstatus },
	{ .name = RDB_SIMCMMAND,					.action = ipw_handle_command_sim },


	
	{0,}
};

////////////////////////////////////////////////////////////////////////////////
struct model_t model_ipw = {
	.name = "ipw",
	.init = ipw_init,
	.detect = ipw_detect,

	.get_status = ipw_get_status,
	.set_status = ipw_set_status,

	.commands = ipw_commands,
	.notifications = NULL
};

////////////////////////////////////////////////////////////////////////////////
