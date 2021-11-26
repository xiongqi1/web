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

#define _XOPEN_SOURCE
#include <time.h>

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
#include "../sms/sms.h"
#include "../sms/pdu.h"
#include "../sms/ussd.h"

#include "../dyna.h"
#include "suppl.h"
#include "../util/scheduled.h"

#include "../featurehash.h"

#include <sys/times.h>

#include <fcntl.h>
#include <dirent.h>

#ifdef USSD_SUPPORT
#include "../sms/ussd.h"
#endif

#include "../logmask.h"

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

///////////////////////////////////////////////////////////////////////////////
const char* _getFirstToken(const char* szSrc, const char* szDeli);
const char* _getNextToken();
int __convNibbleHexToInt(char chNibble);
int __convHexArrayToStr(char* pHex, int cbHex, char* achBuf, int cbBuf);
void __swapNibbles(char* pArray, int cbArray);
void convClearPwd(char* szOffPin);
int handleUpdateNetworkStat(int sync);
char *strip_quotes (char *string);
extern volatile BOOL sms_disabled;
extern volatile BOOL pdu_mode_sms;
extern volatile BOOL ira_char_supported;
extern volatile BOOL numeric_cmgl_index;
extern int custom_roaming_algorithm;
int default_handle_command_nwctrl(const struct name_value_t* args);

char *strptime(const char *s, const char *format, struct tm *tm);

static const char* service_types[] = { "GSM", "GSM Compact", "UMTS", "EGPRS", "HSDPA", "HSUPA", "HSDPA/HSUPA", "LTE" };

extern int automatic_operator_setting;  // 1 -> Automatic, 0 -> Manual

#ifdef V_MANUAL_ROAMING_vdfglobal
extern void suspend_connection_mgr();
#endif

extern int do_custom_manual_roaming(int quiet);
extern int update_cinterion_SMS_retransmission_timer(int time);
///////////////////////////////////////////////////////////////////////////////

static int _clirSubscriberStatus = 4;
static int _clirSubscriberStatusN = 0;
static int _clipSubscriberStatus = 1;
static int _clipSubscriberStatusN = 0;
int network_stat = 0;

/* special flag to use !gband command and hard-coded bands list rather than !band command */
int use_gband_command = -1;

/* Check whether the module support AT+CPINC command to 
 * read remaining PIN/PUK count value. */
int support_pin_counter_at_cmd = -1;
void check_or_update_pin_counter(int sim_exist);
volatile char last_failed_pin[16] = {0, };

// For GSM
arfcnStructType afrcnTbl[] =
{
	{ 0,	"GSM450",		259,	293,	{0} },
	{ 0,	"GSM480",		306,	340,	{0} },
	{ 0,	"GSM750",		438,	511,	{0} },
	{ 0,	"GSM850",		128,	251,	{0} },
	{ 0,	"GSM900",		1,	124,	{0} },
	{ 0,	"GSM1800/GSM1900",	512,	810,	{0} },
	{ 0,	"GSM1800",		811,	885,	{0} },
	{ 0,	"GSM900",		955,	1023,	{0} },
	{ 0,	NULL,			0,	0,	{0} },
};

// For WCDMA
arfcnStructType uafrcnTbl[] = {
	{ 1,	"WCDMA2100",	10562,	10838,	{0} },
	{ 2,	"WCDMA1900",	9662,	9938,	{ 412, 437, 462, 487, 512, 537, 562, 587, 612, 637, 662, 687, 0} },
	{ 3,	"WCDMA1800",	1162,	1513,	{0}},
	{ 4,	"WCDMA1700",	1537,	1738,	{ 1887, 1912, 1937, 1962, 1987, 2012, 2037, 2062, 2087, 0} },
	{ 5,	"WCDMA850",	4357,	4458,	{ 1007, 1012, 1032, 1037, 1062, 1087, 0} },
	{ 6,	"WCDMA800",	4387,	4413,	{ 1037, 1062, 0} },
	{ 7,	"WCDMA2600",	2237,	2563,	{ 2587, 2612, 2637, 2662, 2687, 2712, 2737, 2762, 2787, 2812, 2837, 2862, 2887, 2912, 0} },
	{ 8,	"WCDMA900",	2937,	3088,	{0}},
	{ 9,	"WCDMA1700",	9237,	9387,	{0}},
	{ 10,	"WCDMA1700",	3112,	3388,	{ 3412, 3437, 3462, 3487, 3512, 3537, 3562, 3587, 3612, 3637, 3662, 3687, 0} },
	{ 11,	"WCDMA1500",	3712,	3812,	{0}},
	{ 12,	"WCDMA700",	3837,	3903,	{ 3927, 3932, 3957, 3962, 3987, 3992, 0} },
	{ 13,	"WCDMA700",	4017,	4043,	{ 4067, 4092, 0} },
	{ 14,	"WCDMA700",	4117,	4143,	{ 4167, 4192, 0} },
	{ 15,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{ 16,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{ 17,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{ 18,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{ 19,	"WCDMA800",	712,	763,	{ 787, 812, 837, 0} },
	{ 0,	NULL,		0,	0,	{0}},
};

// For LTE
arfcnStructType eafrcnTbl[] = {
	{ 1,	"LTE Band 1 - 2100MHz",	0,		599,	{0}},	// 2.1 GHz
	{ 2,	"LTE Band 2 - 1900MHz",	600,	1199,	{0}},	// US PCS 1900
	{ 3,	"LTE Band 3 - 1800MHz",	1200,	1949,	{0}},	// 1800 MHz
	{ 4,	"LTE Band 4 - AWS",		1950,	2399,	{0}},	// AWS
	{ 5,	"LTE Band 5 - 850MHz",	2400,	2649,	{0}},	// 850 MHz
	{ 6,	"LTE Band 6",			2650,	2749,	{0}},	//
	{ 7,	"LTE Band 7 - 2600MHz",	2750,	3449,	{0}},	// 2.6 GHz
	{ 8,	"LTE Band 8 - 900MHz",	3450,	3799,	{0}},	// 900 MHz
	{ 9,	"LTE Band 9 - 1700 MHz",	3800,	4149,	{0}},	// 1700 MHz
	{10,	"LTE Band 10 - Extended AWS",	4150,	4749,	{0}},	// Extended AWS
	{11,	"LTE Band 11 - 1500MHz",	4750,	4949,	{0}},	// Japan 1.5 GHz
	{12,	"LTE Band 12 - Lower 700MHz",	5010,	5179,	{0}},	// Lower 700 MHz, A+B+C
	{13,	"LTE Band 13 - Upper 700MHz",	5180,	5279,	{0}},	// Upper 700 MHz
	{14,	"LTE Band 14 - Public Safety",	5280,	5379,	{0}},	// Public Safety
	{15,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{16,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{17,	"LTE Band 17 - Lower 700MHz",	5730,	5849,	{0}},	// Lower 700 MHz, B+C
	{18,	"LTE Band 18 - 800MHz lower",	5850,	5999,	{0}},	// Japan 800 MHz lower
	{19,	"LTE Band 19 - 800MHz upper",	6000,	6149,	{0}},	// Japan 800 MHz upper
	{20,	"LTE Band 20 - 800MHz EDD",	6150,	6449,	{0}},	// 800 MHz EDD
	{21,	"LTE Band 21 - 1500MHz",	6450,	6599,	{0}},	// 1.5 GHz
	{22,	"LTE Band 22 - 3500MHZ",	6600,	7399,	{0}},	// 3.5 Ghz
	{23,	"LTE Band 23 - 2000MHz S-Band",	7500,	7699,	{0}},	// 2 GHz S-Band
	{24,	"LTE Band 24 - L Band",	7700,	8039,	{0}},	// L Band
	{25,	"LTE Band 25 - US PCS + G Block",	8040,	8689,	{0}},	// US PCS + G Block
	{26,	"LTE Band 26 - Upper 850MHz",	8690,	9039,	{0}},	// Upper 850 MHz
	{27,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{28,	"LTE Band 28 - 700Mhz",		9210,	9659,	{0}},	// Band not in use
	{29,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{30,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{31,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{32,	"Band Not In Use",	0,	0,	{0}},	// Band not in use
	{33,	"LTE Band 33 - TDD 2000MHz",	36000,	36199,	{0}},	// TDD 2000
	{34,	"LTE Band 34 - TDD 2000MHz",	36200,	36349,	{0}},	// TDD 2000
	{35,	"LTE Band 35 - TDD 1900MHz",	36350,	36949,	{0}},	// TDD 1900
	{36,	"LTE Band 36 - TDD 1900MHz",	36950,	37549,	{0}},	// TDD 1900
	{37,	"LTE Band 37 - TDD PCS",	37550,	37749,	{0}},	// TDD PCS
	{38,	"LTE Band 38 - TDD 2600MHz",	37750,	38249,	{0}},	// TDD 2.6 GHz
	{39,	"LTE Band 39 - TDD 1900MHz",	38250,	38649,	{0}},	// China TDD 1.9 GHz
	{40,	"LTE Band 40 - TDD 2300MHz",	38650,	39649,	{0}},	// China TDD 2.3 GHz
	{41,	"LTE Band 41 - TDD 2500MHz",	39650,	41589,	{0}},	// TDD 2.5 GHz
	{42,	"LTE Band 42 - TDD 3400MHz",	41590,	43589,	{0}},	// TDD 3.4 GHz
	{43,	"LTE Band 43 - TDD 3600MHz",	43590,	45589,	{0}},	// TDD 3.6 GHz
	{ 0,	NULL,		0,	0,	{0}},
};


///////////////////////////////////////////////////////////////////////////////
/*
	protocol:	1 --> GSM
			2 --> WCDMA
			3 --> LTE
	number:		arfcn
*/

char * convert_Arfcn2BandType (int protocol, int number) {
	int	i, j;
	static char response[128];
	arfcnStructType *arfcnTbl = NULL;
	int	arfcn = number;

	memset (response, 0, sizeof(response));

	if (protocol == 1 )
		arfcnTbl = afrcnTbl;
	else if (protocol == 2 )
		arfcnTbl = uafrcnTbl;
	else if (protocol == 3 )
		arfcnTbl = eafrcnTbl;
	else
		return response;

	for(i = 0; arfcnTbl[i].bandName != NULL; i++) {

		if (  arfcnTbl[i].min <= arfcn && arfcn <= arfcnTbl[i].max  ) {
			strcpy(response, arfcnTbl[i].bandName);
			break;
		}

		for(j = 0; arfcnTbl[i].additional[j] != 0 && j < SIZE_OF_ADDTBL ; j++) {
			if (arfcn == arfcnTbl[i].additional[j]) {
				strcpy(response, arfcnTbl[i].bandName);
				return response;
			}
		}
	}

	return response;
}

///////////////////////////////////////////////////////////////////////////////
static int getRdbAutoPin(char* achBuf, int cbBuf)
{
	char buf[32];

	// read autopin
	if (rdb_get_single(rdb_name(RDB_SIMAUTOPIN, ""), buf, sizeof(buf)) != 0)
		return -1;

	// get autopin
	int fAutoPin = atoi(buf);
	if (!fAutoPin)
		return -1;

	// get pin
	if (rdb_get_single(rdb_name(RDB_SIMPIN, ""), buf, sizeof(buf)) != 0)
	{
		SYSLOG_ERR("failed to read auto-pin");
		return -1;
	}

	strncpy(achBuf, buf, cbBuf);

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int wait_for_sim_card(int count)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char command[32];
	int ok;

	int i;

	i=0;
	while(i++<count) {
		// get mep lock status
		sprintf(command, "at+clck=\"PN\",2");
		if (!at_send(command, response, "", &ok, 0) && ok) {
			return 0;
		}

		sleep(1);
	}

	return -1;
}

int updateSIMStat()
{
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;
	static int fail_counter=0;
	const char* mep_lock_msg;
	const char* szSIMStat;

	char command[32];

	// get PIN enabled status
	if (at_send("at+clck=\"SC\",2", response, "", &ok, 0) != 0 || !ok)
	{
		//SYSLOG_ERR("at+clck=\"SC\" command failed");
	} else {
		// +CLCK: 0
		int fEn = atoi(response + strlen("+CLCK: "));
		rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), fEn ? "Enabled" : "Disabled");
	}

	// get mep lock status
	sprintf(command, "at+clck=\"PN\",2");

	mep_lock_msg="unknown";

	// query mep-lock
	if (!at_send(command, response, "", &ok, 0) && ok) {
		if(strstr(response,"+clck: 1"))
			mep_lock_msg="locked";
		else if(strstr(response,"+clck: 0"))
			mep_lock_msg="unlocked";
	}

	// set database variable
	rdb_set_single(rdb_name(RDB_SIM_MEPLOCK,""),mep_lock_msg);

	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0) {
		check_or_update_pin_counter(0);
		return -1;
	}

	if (!ok)
	{
		if(++fail_counter > 2) {	/* changed to 2 from 5 */
			szSIMStat = "SIM not inserted";
		} else {
			szSIMStat = "Negotiating";
		}
		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), szSIMStat);
		/* clean simICCID when No SIM detected. */
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), "");
	}
	else
	{
		szSIMStat = response + strlen("+CPIN: ");
		fail_counter=0;
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

	check_or_update_pin_counter(strcmp(szSIMStat,"SIM not inserted"));
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int getOffset()
{
	int nOff = 0;

	FILE* fp = popen("echo \"$(rdb_get uboot.snextra)_D9\" | passgen '%9n'", "r");

	if (!fp)
		return 0;

	char achBuf[64];
	if (fgets(achBuf, sizeof(achBuf), fp))
		nOff = atoi(achBuf);

	fclose(fp);

	return nOff;
}

///////////////////////////////////////////////////////////////////////////////
void convClearPwd(char* szOffPin)
{
	if (szOffPin[0] == '#')
	{
		int iPin = atoi(&szOffPin[1]);
		int nOff = getOffset();

		sprintf(szOffPin, "%04d", iPin - nOff);
	}
}

///////////////////////////////////////////////////////////////////////////////
int verifyPin(const char* szPin)
{
	int ok = 0;
	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char achStrCCID[64];

	SET_LOG_LEVEL_TO_SIM_LOGMASK_LEVEL
	sprintf(command, "AT+CPIN=\"%s\"", szPin);
	if (at_send(command, response, "OK", &ok, 0) != 0 || !ok) {
#ifdef PLATFORM_PLATYPUS
		nvram_init(RT2860_NVRAM);
#endif
/* This #ifdef can be conbined with upper #ifdef but is deliberately left separately
 * to divide logical part and nvram part */
#ifdef PLATFORM_PLATYPUS
		char *pinStrCCID = nvram_get(RT2860_NVRAM, "wwan_pin_ccid");
#else
		char *pinStrCCID = alloca(64);
		rdb_create_and_get_single("wwan_pin_ccid", pinStrCCID, 64);
#endif
		// get current ccid
		if (rdb_get_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), achStrCCID, sizeof(achStrCCID)) < 0)
			*achStrCCID = 0;
		if( *achStrCCID && *pinStrCCID && !strcmp(achStrCCID, pinStrCCID) ) {
#ifdef PLATFORM_PLATYPUS
			nvram_set(RT2860_NVRAM, "wwan_pin_ccid", ""); //the pin for this ICCID has failed
			nvram_set(RT2860_NVRAM, "wwan_pin", ""); //remove this pin
#else
			rdb_set_single("wwan_pin_ccid", ""); 	//the pin for this ICCID has failed
			rdb_set_single("wwan_pin", ""); 		//remove this pin
#endif
		}
#ifdef PLATFORM_PLATYPUS
		nvram_strfree(pinStrCCID);
		nvram_close(RT2860_NVRAM);
#endif
 		goto error;
	}
#ifndef PLATFORM_PLATYPUS
	else {
		// save current ccid and pin
		if (rdb_get_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), achStrCCID, sizeof(achStrCCID)) == 0) {
			SYSLOG_DEBUG("set pin & ccid : %s, %s", achStrCCID, szPin);
			(void) rdb_set_single("wwan_pin_ccid", achStrCCID);
			(void) rdb_set_single("wwan_pin", szPin);
		}
	}
#endif			

	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return 0;

error:
	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return -1;
}
static int sierra_simcard_pin_enable(int en,const char* pin)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	char command[64];
		
	/* build AT command */
	snprintf(command, sizeof(command), "AT+CLCK=\"SC\",%d,\"%s\"", en?1:0, pin);

	/* send AT command */
	if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
		goto err;

	return 0;

err:
	return -1;
}

static int sierra_simcard_pin_check(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	const char* p;

	/* send AT command */
	if (at_send("at+clck=\"SC\",2", response, "", &ok, 0) != 0 || !ok)
		goto err;

	/* get result */
	p=strstr(response,"+CLCK: ");
	if(!p)
		goto err;
	
	/* parse result */
	return atoi(p + strlen("+CLCK: "));

err:
	return -1;	
}
///////////////////////////////////////////////////////////////////////////////
int default_handle_command_sim(const struct name_value_t* args)
{
	int ok = 0;
	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[128];

	char newpin[16];
	char simpuk[16];
	char pin[16];

	char* szCmdStat = NULL;

	SET_LOG_LEVEL_TO_SIM_LOGMASK_LEVEL
	// bypass if incorrect argument
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		goto error;
	}

	SYSLOG_DEBUG("default_handle_command_sim : %s", args[0].value);
	
	// check sim status
	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0 || !ok)
	{
		szCmdStat = "error - AT+CPIN? has failed";
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
			sprintf(buf, "error - %s has failed", "at+clck=\"SC\",2");
			szCmdStat = buf;
			goto error;
		}

		// +CLCK: 0
		int fEn = atoi(response + strlen("+CLCK: "));

		rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), fEn ? "Enabled" : "Disabled");
	}
	// sim commmand - lockmep
	else if (memcmp(args[0].value, "lockmep", STRLEN("lockmep")) == 0) {

		// get mep code
		char mep[16];
		if (rdb_get_single(rdb_name(RDB_SIMMEP, ""), mep, sizeof(mep)) != 0)
		{
			sprintf(buf, "error - failed to get %s", rdb_name(RDB_SIMMEP, ""));
			szCmdStat = buf;
			goto error;
		}

		// send command
		sprintf(command, "at+clck=\"PN\",2");
		if (at_send(command, response, "", &ok, 0) != 0 || !ok) {
			sprintf(buf, "error - %s has failed", command);
			szCmdStat = buf;
			goto error;
		}

		// lock only when it's not locked
		if(!strstr(response,"+clck: 1")) {
			// send command
			sprintf(command, "at+clck=\"PN\",1,\"%s\"", mep);
			if (at_send(command, response, "", &ok, 0) != 0)
			{
				sprintf(buf, "error - %s has failed", command);
				szCmdStat = buf;
				goto error;
			}

			if(!ok) {
				sprintf(buf, "error - MEP code not matched");
				szCmdStat = buf;
				goto error;

			}
		}
		else {
			sprintf(buf, "error - sim card already locked");
			szCmdStat = buf;
			goto error;
		}
	}
	// sim command - unlockmep
	else if (memcmp(args[0].value, "unlockmep", STRLEN("unlockmep")) == 0)
	{
		// get mep code
		char mep[16];
		if (rdb_get_single(rdb_name(RDB_SIMMEP, ""), mep, sizeof(mep)) != 0)
		{
			sprintf(buf, "failed to get %s", rdb_name(RDB_SIMMEP, ""));
			szCmdStat = buf;
			goto error;
		}

		// send command
		sprintf(command, "at+clck=\"PN\",2");
		if (at_send(command, response, "", &ok, 0) != 0 || !ok) {
			sprintf(buf, "%s has failed", command);
			szCmdStat = buf;
			goto error;
		}

		// unlock only when it's not unlocked
		if(!strstr(response,"+clck: 0")) {
			// send command
			sprintf(command, "at+clck=\"PN\",0,\"%s\"", mep);
			if (at_send(command, response, "", &ok, 0) != 0)
			{
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}

			if(!ok) {
				sprintf(buf, "error - MEP code not matched");
				szCmdStat = buf;
				goto error;

			}
		}
		else {
			sprintf(buf, "error - sim card already unlocked");
			szCmdStat = buf;
			goto error;
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
	// sim command - verifypin
	else if (!memcmp(args[0].value, "verifypin", STRLEN("verifypin")))
	{
		if (fSIMPin)
		{
			if (verifyPin(pin) < 0)
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
	// sim command - changepin
	else if (memcmp(args[0].value, "changepin", STRLEN("changepin")) == 0)
	{
		if (rdb_get_single(rdb_name(RDB_SIMNEWPIN, ""), newpin, sizeof(newpin)) != 0)
		{
			sprintf(buf, "failed to get %s", rdb_name(RDB_SIMNEWPIN, ""));
			szCmdStat = buf;
			goto error;
		}
		convClearPwd(newpin);

		sprintf(command, "AT+CPWD=\"SC\",\"%s\",\"%s\"", pin, newpin);
		if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "changepin operation has failed");
			szCmdStat = buf;
			goto error;
		}
	}
	// sim command - enablepin
	else if (memcmp(args[0].value, "enablepin", STRLEN("enablepin")) == 0)
	{
		int cur_pin_en;

		/* get current status of sim pin */
		cur_pin_en=sierra_simcard_pin_check();
		if(cur_pin_en<0) {
			sprintf(buf, "failed to get current SIM PIN status");
			szCmdStat = buf;
			goto error;
		}

		/* disable if already enable - check if the sim pin is correct */
		if(cur_pin_en) {
			if(sierra_simcard_pin_enable(0,pin)<0) {
				sprintf(buf, "failed to verify the pin");
				szCmdStat = buf;
				goto error;
			}
		}
			
		
		sprintf(command, "AT+CLCK=\"SC\",1,\"%s\"", pin);
		if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "enablepin operation has failed");
			szCmdStat = buf;
			goto error;
		}
		rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), "Enabled");
	}
	// sim command - disablepin
	else if (memcmp(args[0].value, "disablepin", STRLEN("disablepin")) == 0)
	{
		int cur_pin_en;
		
		/* get current status of sim pin */
		cur_pin_en=sierra_simcard_pin_check();
		if(cur_pin_en<0) {
			sprintf(buf, "failed to get current SIM PIN status");
			szCmdStat = buf;
			goto error;
		}

		/* disable if already enable - check if the sim pin is correct */
		if(!cur_pin_en) {
			if(sierra_simcard_pin_enable(1,pin)<0) {
				sprintf(buf, "failed to verify the pin");
				szCmdStat = buf;
				goto error;
			}
		}

		sprintf(command, "AT+CLCK=\"SC\",0,\"%s\"", pin);
		if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "disablepin operation has failed");
			szCmdStat = buf;
			goto error;
		}
		rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), "Disabled");
	}
	else
	{
		SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
		goto error;
	}

	updateSIMStat();

#if (defined PLATFORM_BOVINE)
	rdb_set_single(rdb_name(RDB_SIMRESULTOFUO, ""), "Operation succeeded");
#else
	rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "Operation succeeded");
#endif
	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return 0;

error:
	updateSIMStat();

	if (szCmdStat) {
#if (defined PLATFORM_BOVINE)
		rdb_set_single(rdb_name(RDB_SIMRESULTOFUO, ""), szCmdStat);
#else
		rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), szCmdStat);
#endif
	}

	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int sendCRSM(int nCmd, int iFileID, int nP1, int nP2, int nP3, int* pSW1, int* pSW2, void* pBuf, int cbBuf)
{
	char achATCmd[128];
	char achATRes[2048];
	int stat;
	int ok;
	char* achBuf = pBuf;

	// build CRSM command
	if (nP1 < 0 || nP2 < 0 || nP3 < 0)
		sprintf(achATCmd, "AT+CRSM=%u,%u", nCmd, iFileID);
	else
		sprintf(achATCmd, "AT+CRSM=%u,%u,%02u,%02u,%02u", nCmd, iFileID, nP1, nP2, nP3);

	// send CRSM command
	stat = at_send(achATCmd, achATRes, "", &ok, 2048);
	if (stat || !ok)
		return -1;

	// +CRSM: 144,0,"1E4A172C27FE05004000"

	// get SW1
	const char* pToken = _getFirstToken(achATRes, ",\"");
	int nSW1 = pToken ? atoi(pToken) : -1;
	if (pSW1)
		*pSW1 = nSW1;

	// get SW2
	pToken = _getNextToken();
	int nSW2 = pToken ? atoi(pToken) : -1;
	if (pSW2)
		*pSW2 = nSW2;

	// get value
	pToken = _getNextToken();
	if (!pToken)
		return -1;

	// convert value to byte array
	unsigned char bOct;
	int iT = 0;
	int cbToken = strlen(pToken);
	int iB = 0;
	while (iT < cbToken && iB < cbBuf)
	{
		bOct = __convNibbleHexToInt(pToken[iT++]) << 4;

		if (iT < cbToken)
			bOct |= __convNibbleHexToInt(pToken[iT++]);

		achBuf[iB++] = bOct;
	}

	return iB;
}

///////////////////////////////////////////////////////////////////////////////
int readSIMFile(int iFileID, void* pBuf, int cbBuf)
{
	char achATCmd[128];
	char achATRes[2048];
	char* achBuf = pBuf;
	int stat;
	int ok;
	int i;

	struct _pppp
	{
		int fileid;
		int cmd;
		signed char p1;
		signed char p2;
		signed char p3;
	} pppp[] =
	{
		{12258, 176, 0, 0, 10},		// 2fe2
		{28589, 176, 0, 0, 0},		// 6fad
		{28618, 178, 1, 4, 5},		// 6fca
		{28615, 178, 1, 4, 0},		// 6fc7
		{28613, 178, 1, 4, 0},		// 6fc5
		{28633, 176, 0, 0, 0},		// 6fd9
		{28480, 178, 1, 4, 0},		// 6f40
		{28439, 178, 1, 4, 0},		// 6f17
		{0, 0, 0, 0, 0}
	};

	// send at command
	//sprintf(achATCmd, "AT+CRSM=%d,%d", 176, iFileID);
	for (i = 0; pppp[i].fileid; i++)
	{
		if (iFileID == pppp[i].fileid)
		{
			if (pppp[i].p1 < 0)
				sprintf(achATCmd, "AT+CRSM=%u,%u", pppp[i].cmd, iFileID);
			else
				sprintf(achATCmd, "AT+CRSM=%u,%u,%02u,%02u,%02u", pppp[i].cmd, iFileID, pppp[i].p1, pppp[i].p2, pppp[i].p3);
			break;
		}
	}

	stat = at_send(achATCmd, achATRes, "", &ok, 2048);
	if (stat || !ok)
		return -1;

	// +CRSM: 144,0,"1E4A172C27FE05004000"

	// get SW1
	const char* pToken = _getFirstToken(achATRes, ",\"");
	//int nSW1=pToken?atoi(pToken):0;

	// get SW2
	pToken = _getNextToken();
	//int nSW2=pToken?atoi(pToken):0;

	// get value
	pToken = _getNextToken();
	if (!pToken)
		return 0;

	/*
		strncpy(achBuf,pToken,cbBuf);
		achBuf[cbBuf-1]=0;

		return strlen(achBuf);
	*/

	// convert value to byte array
	unsigned char bOct;
	int iT = 0;
	int cbToken = strlen(pToken);
	int iB = 0;
	while (iT < cbToken && iB < cbBuf)
	{
		bOct = __convNibbleHexToInt(pToken[iT++]) << 4;

		if (iT < cbToken)
			bOct |= __convNibbleHexToInt(pToken[iT++]);

		achBuf[iB++] = bOct;
	}

	return iB;
}

static int handle_clip_notification(const char* s)
{
	char* number = alloca(strlen(s));
	size_t size;
	char terminator = ',';
	const char* begin = s + strlen("+CLIP: ");
	const char* it;
	if (*begin == '"')
	{
		++begin;
		terminator = '"';
	}
	for (it = begin; *it != 0 && *it != terminator; ++it);
	if (*it == 0)
	{
		SYSLOG_ERR("invalid number in [%s]", s);
		return -1;
	}
	size = it - begin;
	memcpy(number, begin, size);
	number[size] = 0;
	SYSLOG_DEBUG("calling line number: '%s'", number);
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "clip.number"), number);
}

static int handle_ccwa_notification(const char* s)
{
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "calls.event"), "waiting");
}

static int handle_ring(const char* s)
{
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "calls.event"), "ring");
}

extern int update_cinterion_signal_strength(void);
extern int cinterion_network_name(void);
extern struct model_t model_cinterion;
extern int cinterion_update_service_type(const char* s);

static int handle_connect(const char* s)
{
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "calls.event"), "connect");
}

static int handle_disconnect(const char* s)
{
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "calls.event"), "disconnect");
}

typedef enum { service_enable = '*', service_disable = '#', service_query = '?' } service_type_enum;


////////////////////////////////////////////////////////////////////////////////
static void polling_network_reg_event(void* ref)
{
	syslog(LOG_INFO,"CREG detected - updating network name");
	scheduled_clear("polling_network_reg_event");
	update_network_name();
	update_roaming_status();
}

///////////////////////////////////////////////////////////////////////////////
static int handle_creg_notification(const char* s)
{
	if (is_ericsson) {
		syslog(LOG_DEBUG,"CREG detected - schedule to update network name and roaming status");

		/* update network event if any event is triggered */
		scheduled_func_schedule("polling_network_reg_event",polling_network_reg_event,10);
	}

	return 0;
}

static char* hextodec(const char* hex)
{
	static char dec_buf[64];
	int dec;
	
	dec_buf[0]=0;
	
	if(hex) {
		dec=strtol(hex,0,16);
		snprintf(dec_buf,sizeof(dec_buf),"%d",dec);
	}
	
	return dec_buf;
}

#ifdef V_TR069
static int handle_creg_cinterion (char* str) {

	char *lac_str; 		// LAC code
	char *newCellId;	// cell ID
	char prevCellId[64];

	strtok(str, ",");
	lac_str  = strtok(NULL, ",");
	newCellId = strtok(NULL, ",");

	if (newCellId) {
		newCellId = strip_quotes(newCellId);
		newCellId = hextodec(newCellId);

		if (rdb_get_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), prevCellId, sizeof(prevCellId)) != 0) {
			return 1;
		}
		if ((strlen(prevCellId) != 0) && (strcmp(newCellId, prevCellId) != 0)) {
			rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), newCellId);
			rdb_set_single("cellID_alarm.trigger", "1");
			//SYSLOG_INFO("Cell Id change triggered");
		}
	}

	return 0;
}
#endif

int creg_network_stat=0;

int handle_creg(const char* s)
{
	// +CREG: 1
	char* str;
	char* stat;

	str=strdup(s);

	stat=strstr(str,"+CREG:");
	if(!stat) {
		SYSLOG_ERR("CREG in incorrect format - %s",s);
		free(str);
		return -1;
	}

	creg_network_stat=atoi(stat+sizeof("+CREG:"));

	SYSLOG_INFO("CREG changed - %d (%s)",creg_network_stat,stat);

#ifdef V_TR069
	if(is_cinterion) {
		handle_creg_cinterion(str);
	}
#endif

	free(str);

	/* for Ericsson module */
	(void) handle_creg_notification(s);

	return 0;
}


typedef enum
{
	ss_call_forward_immediate = 21
	, ss_call_forward_busy = 24
	, ss_call_forward_no_reply = 61
	, ss_call_forward_not_reachable = 62
	, ss_clip = 30
	, ss_clir = 31
	, ss_call_waiting = 43
	, ss_call_barring_international = 331
} supplementary_service_enum;


BOOL empty_call_list = TRUE;	/* indicate module is not in call state */
static int handle_clcc(void)
{
	int ok;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char value[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char* r;
	char* v = value;
	if (at_send("AT+CLCC", response, "+CLCC", &ok, 0) != 0 || !ok)
	{
		empty_call_list = TRUE;
		return -1;
	}
	if (*response)
	{
		empty_call_list = FALSE;
		for (r = response + STRLEN("+CLCC: "); *r; ++v, r = *r == '\n' ? r + STRLEN("+CLCC: ") : r + 1)
		{
			*v = *r;
		}
	}
	*v = '\0';
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES".calls", "list"), value);
}

static int handle_call_control_command(const struct name_value_t* args)
{
	if (strcmp(args[0].value, "calls list") == 0)
	{
		return handle_clcc();
	}
	SYSLOG_ERR("don't know how to handle '%s'", args[0].value);
	return -1;
}


static int getMNCLen()
{
	struct
	{
		unsigned char bUEOperMode;
		unsigned short wAddInfo;
		unsigned char bLenOfMNC;
	} __attribute__((packed)) simDataEFad;

	int cbMNC = 2;

	int cbRead = readSIMFile(0x6fad, &simDataEFad, sizeof(simDataEFad));//28589
	if (cbRead != sizeof(simDataEFad))
		goto fini;

	if (simDataEFad.bLenOfMNC != 2 && simDataEFad.bLenOfMNC != 3)
		goto fini;

	cbMNC = simDataEFad.bLenOfMNC;

fini:
	return cbMNC;
}

int stripStr(char* szValue)
{
	char* pValue = szValue;

	char stripped[ AT_RESPONSE_MAX_SIZE ];

	while (*pValue)
	{
		if (isdigit(*pValue))
			break;

		pValue++;
	}

	strcpy(stripped, pValue);
	strcpy(szValue, stripped);

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int __convNibbleToChar(int nNibble)
{
	if (nNibble > 0x0a)
		return nNibble -0x0a + 'A';

	return nNibble + '0';
}
///////////////////////////////////////////////////////////////////////////////
/*
static int __convBCDToStr(const unsigned char* bcd, int cbBCD, char* achBuf, int cbBuf)
{
	int i = 0;
	int iBuf = 0;

	int nLoHOct;
	int nHiHOct;

	while (iBuf < cbBuf && i < cbBCD)
	{
		nLoHOct = bcd[i] & 0x0f;
		nHiHOct = (bcd[i] >> 4) & 0x0f;
		i++;

		// put low oct first
		achBuf[iBuf++] = __convNibbleToChar(nHiHOct);
		if (iBuf >= cbBuf || i >= cbBCD)
			break;

		// put high oct
		achBuf[iBuf++] = __convNibbleToChar(nLoHOct);
	}

	if (iBuf < cbBuf)
	{
		achBuf[iBuf] = 0;
		return strlen(achBuf);
	}

	achBuf[cbBuf-1] = 0;

	return -1;
}
*/
static int __convDialBCDToStr(unsigned char bTON, const unsigned char* bcd, int cbBCD, char* achBuf, int cbBuf)
{
	// allocate
	void* pSwapped = alloca(cbBCD);
	if (!pSwapped)
		return -1;

	// swap nibbles
	memcpy(pSwapped, bcd, cbBCD);
	__swapNibbles(pSwapped, cbBCD);

	char* pDst = achBuf;

	// get dial length
	if (bTON & 0x01)
		*pDst++ = '+';

	unsigned char bHi;
	unsigned char bLo;

	while (1)
	{
		bHi = (*bcd) & 0x0f;
		bLo = (*bcd >> 4) & 0x0f;

		if (bHi == 0x0f)
			break;

		*pDst++ = __convNibbleToChar(bHi);

		if (bLo == 0x0f)
			break;

		*pDst++ = __convNibbleToChar(bLo);

		bcd++;
	}

	*pDst = 0;

	return pDst -achBuf;
}
///////////////////////////////////////////////////////////////////////////////
extern int Hex(char hex);
int update_sim_hint()
{
	static int sim_hint_updated=0;
	int stat, i, j, ascii;
	char *pos1, *pos2;
	char response[AT_RESPONSE_MAX_SIZE];
	char encoded_hint[96];
	int ok;
	char value[32];
	int double_quated = 1, offset = 2;

	if(sim_hint_updated)
		return 0;

	stat=at_send("AT+CRSM=176,28486,0,0,17", response, "", &ok, 0);
	if ( !stat && ok) {
		pos1=strstr(response, "+CRSM:");
		if(pos1) {
			pos2=strstr(pos1, ",\"");
			if (!pos2) {
				pos2 = strrchr(pos1, ',');
				if (pos2) {
					double_quated = 0;
					offset = 1;
				}
			}
			/* Sequans VZ20Q module response has no double quato */
			if( pos2 && strlen(pos2)>5 ) {
				pos1=strchr( pos2+offset, '\"');
				if(pos1 || double_quated == 0) {
					*encoded_hint=0;
					if (pos1)
						*pos1=0;
					pos1=pos2+offset;
					for( i=0, j=0; i<strlen(pos2)-offset; i+=2) {
						if(*pos1=='F' || *pos1=='f') break;
						ascii= (Hex(*(pos1+i))<<4) + Hex(*(pos1+i+1));
						if( i==0 && !isgraph(ascii) ) continue;
						if( (j==0 && ascii=='<') || (ascii==0xff) ) break;//<No file on SIM>
						sprintf( encoded_hint+strlen(encoded_hint), "%%%02x", ascii );
						if(ascii=='\r' || ascii=='\n')
							value[j++]=' ';
						else
							value[j++]=ascii;
					}
					value[j]=0;
					rdb_set_single(rdb_name(RDB_NETWORK_STATUS".hint", ""), value);
					rdb_update_single(rdb_name(RDB_NETWORK_STATUS".hint", "encoded"), encoded_hint, CREATE, ALL_PERM, 0, 0);
					sim_hint_updated = 1;
				}
			}
		}
	}
	//else
		//return check_pin_status(response);//get hint befor ICCID

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
typedef enum { SIMFILE_MBN = 0, SIMFILE_MBDN, SIMFILE_MSISDN, SIMFILE_ADN } phone_no_simfile_enum;
static int handleSIMDataMbnMbdnMsisdnAdn(const struct name_value_t* args, phone_no_simfile_enum ftype)
{
	char achBuf[AT_RESPONSE_MAX_SIZE];

	struct
	{
		unsigned char bLenOfBCDNum;
		unsigned char bTON;
		unsigned char dialNo[10];
		unsigned char bCap;
		unsigned char bExt6;
	}* __attribute((packed)) pEFmbdn;

	int cbEFmbdn = sizeof(*pEFmbdn);
	int cbDialNo = sizeof(pEFmbdn->dialNo);
	const int findex[4] = { 0x6f17, 0x6fc7, 0x6f40, 0x6f3a };
	const char frdbname[4][20] = { RDB_SIM_DATA_MBN, RDB_SIM_DATA_MBDN, RDB_SIM_DATA_MSISDN, RDB_SIM_DATA_ADN };

	// read SIM file
	int cbRead = readSIMFile(findex[ftype], achBuf, sizeof(achBuf));
	if (cbRead < cbEFmbdn)
		goto error;

	// get response
	char achRes[AT_RESPONSE_MAX_SIZE];
	int cbRes;
	cbRes = sendCRSM(192, findex[ftype], 0, 0, 129, 0, 0, achRes, sizeof(achRes));
	if (cbRes < 7)
		goto error;

	// do paranoid-check
	int cbRecInRes = achRes[6] << 8 | achRes[7];
	if (cbRecInRes != cbRead)
		goto error;

	pEFmbdn = (typeof(pEFmbdn))(achBuf + cbRead - cbEFmbdn);

	// get dial string
	char achDailNo[32];

	if (pEFmbdn->bTON == 0xff)
		achDailNo[0] = 0;
	else
		__convDialBCDToStr(pEFmbdn->bTON, pEFmbdn->dialNo, cbDialNo, achDailNo, sizeof(achDailNo));

	SYSLOG_DEBUG("update '%s' to '%s'", rdb_name(frdbname[ftype], ""), achDailNo);
	return rdb_set_single(rdb_name(frdbname[ftype], ""), achDailNo);

error:
	return -1;
}

char __swapNibble(char ch)
{
	return ((ch >> 4) & 0x0f) | ((ch << 4) & 0xf0);
}

struct plmn
{
	unsigned char PLMN[3];
} __attribute__((packed));

static int hasMCCMNC(struct plmn* pPLMN, int cPLMN, int nwMCC, int nwMNC)
{
	int fMatched = 0;
	int iPLMN;

	for (iPLMN = 0;iPLMN < cPLMN;iPLMN++)
	{
		int nMCC;
		int nMNC;

		unsigned char PLMN[3];
		memcpy(PLMN, pPLMN[iPLMN].PLMN, sizeof(PLMN));

		if (PLMN[0] == 0x0ff && PLMN[1] == 0xff && PLMN[2] == 0xff)
			continue;

		nMCC = (((PLMN[0] >> 0) & 0x0f) * 100) + (((PLMN[0] >> 4) & 0x0f) * 10) + ((PLMN[1] >> 0) & 0x0f);
		nMNC = (((PLMN[2] >> 0) & 0x0f) * 100) + (((PLMN[2] >> 4) & 0x0f) * 10) + ((PLMN[1] >> 4) & 0x0f);

		if (nMCC != nwMCC || nMNC != nwMNC)
			continue;

		fMatched = 1;
		break;
	}

	return fMatched;
}

int updateRoamingStatus_method1(int nwMCC, int nwMNC)
{
	/* method 2 - EF PNN */

	unsigned char* pSIMDataEFpnn;
	int cbSIMDataEFpnn = 128;

	// bypass if no memory
	pSIMDataEFpnn = alloca(cbSIMDataEFpnn);
	if (!pSIMDataEFpnn)
		return -1;

	// read SIM file
	int cbRead;
	cbRead = readSIMFile(0x6fc5, pSIMDataEFpnn, cbSIMDataEFpnn);//28613

	// paranoide check - skip full network name
	int iBase = 1;
	if (cbRead <= iBase)
		return -1;
	iBase += 1 + pSIMDataEFpnn[iBase] + 1;

	// paranoide check - skip short network name
	if (cbRead <= iBase)
		return -1;
	iBase += 1 + pSIMDataEFpnn[iBase];

	struct
	{
		unsigned char bTag;
		unsigned char bLen;
		struct plmn plmnList[0];
	} __attribute__((packed))* pPLMNInfo;

	// get PLMN additional information
	pPLMNInfo = (typeof(pPLMNInfo))(&pSIMDataEFpnn[iBase]);

	// paranoide check - skip PLMN info
	iBase += sizeof(*pPLMNInfo);
	if (cbRead < iBase)
		return -1;

	// paranoide check - PLMN length
	iBase += pPLMNInfo->bLen;
	if (cbRead < iBase)
		return -1;

	// paranoide check - PLMN tag
	if (pPLMNInfo->bTag != 0x80)
		return -1;

	// get PLMN count
	int cPLMN = pPLMNInfo->bLen / sizeof(struct plmn);
	if (!cPLMN)
		return -1;

	// get PLMN list
	struct plmn* pPLMNList = pPLMNInfo->plmnList;

	if (!hasMCCMNC(pPLMNList, cPLMN, nwMCC, nwMNC))
		return 1;

	return 0;
}
int updateRoamingStatus_method2(int nwMCC, int nwMNC)
{
	/* method 2 - EF EHPLMN */
	struct plmn simDataEFehplmn[64];

	int cbRead;
	cbRead = readSIMFile(0x6fd9, simDataEFehplmn, sizeof(simDataEFehplmn));//28633

	if (cbRead <= 0)
		return -1;

	int cPLMN = cbRead / sizeof(simDataEFehplmn[0]);

	if (!cPLMN)
		return -1;

	if (!hasMCCMNC(simDataEFehplmn, cPLMN, nwMCC, nwMNC))
		return 1;

	return 0;
}
int updateRoamingStatus_method3(void)
{
	char* resp;
	char* stat_str;
	int nw_stat;

	nw_stat=-1;
/* Sequans VZ20Q module supports packet mode only */
#if defined (MODULE_VZ20Q)
	if (strstr(model_variants_name(),"VZ20Q"))
		resp=at_bufsend("AT+CEREG?","+CEREG: ");
	else
#endif	
		resp=at_bufsend("AT+CREG?","+CREG: ");
	if( resp!=0 )
	{
		strtok(resp, ",");
		stat_str=strtok(0, ",");
		if(stat_str)
		{
			nw_stat=atoi(stat_str);
			nw_stat=(nw_stat == 5? 1:0);
		}
	}
	return nw_stat;
}

time_t convert_utc_to_local(struct tm* tm_utc)
{
	time_t utc;
	
	tzset();
	
	// make utc
	utc=timegm(tm_utc);
	
	return utc;
}

int update_date_and_time()
{
	// wwan.0.modem_date_and_time

	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char* ptr;
	int ok;

	struct tm tm_utc;
	time_t time_network;
	time_t time_now;

	char rdb_buff[128];

	time_now=time(NULL);

	// build clock command
	sprintf(command, "AT+CCLK?");
	// send command
	if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		return -1;

/*
	AT command : 'AT+CCLK?'
	+CCLK: "12/05/29,02:07:28"

	"yy/MM/dd,hh:mm:ss±zz"

*/
	#define PREFIX_CCLK	"+CCLK: "

	// get time string
	if( !(ptr=strstr(response,PREFIX_CCLK)) ) {
		syslog(LOG_ERR,"prefix not found in commmand(%s) - %s",command,response);
		return -1;
	}
	// skip prefix
	ptr+=STRLEN(PREFIX_CCLK);

/* Sequans VZ20Q module's response has no double quotations. */
#if defined (MODULE_VZ20Q)
	if (!strstr(model_variants_name(),"VZ20Q")) {
#endif		
	// skip quotation
	if(*ptr++!='"') {
		syslog(LOG_ERR,"unknown AT command result format (%s) - %s",command,response);
		return -1;
	}
#if defined (MODULE_VZ20Q)
	}
#endif		

	// convert str to struct tm
	if(!(ptr=strptime(ptr,"%y/%m/%d,%H:%M:%S",&tm_utc))) {
		syslog(LOG_ERR,"unknown time format (%s) - %s",ptr,strerror(errno));
		return -1;
	}

	// take timezone difference - ignore if no timezone difference
	tm_utc.tm_sec+=atoi(ptr)*60*60/4;

	// convert to time_t
	time_network=convert_utc_to_local(&tm_utc);
	
	// set date and time rdb
	snprintf(rdb_buff,sizeof(rdb_buff),"%ld",time_network-time_now);
	rdb_set_single(rdb_name(RDB_DATE_AND_TIME, ""), rdb_buff);

	return 0;
}

int update_roaming_status(void)
{
    static int last_roaming_st = -1;
    int roaming_chk_method = 0;
    handleUpdateNetworkStat(1);

    char achMCC[16] = {0, };
    char achMNC[16] = {0, };

    int nwMCC;
    int nwMNC;
    int nRoaming;

    const char* szRoaming;

    char achSkin[16] = {0, };

    if(!is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
	    return 0;
    }

    rdb_get_single(rdb_name(RDB_PLMNMCC, ""), achMCC, sizeof(achMCC));
    rdb_get_single(rdb_name(RDB_PLMNMNC, ""), achMNC, sizeof(achMNC));
    rdb_get_single("system.skin", achSkin, sizeof(achSkin));

    nwMCC = atoi(achMCC);
    nwMNC = atoi(achMNC);

    // bypass if we are not online
    if (nwMCC == 0 && nwMNC == 0)
        return -1;

    /* use roaming check method 1 & 2 for Telus only */
    if (strcmp(achSkin, "ts") == 0) {
      nRoaming = updateRoamingStatus_method2(nwMCC, nwMNC);
      if (nRoaming >= 0)
      {
          roaming_chk_method = 2;
          goto roaming_set_exit;
      }
      nRoaming = updateRoamingStatus_method1(nwMCC, nwMNC);
      if (nRoaming >= 0)
      {
          roaming_chk_method = 1;
          goto roaming_set_exit;
      }
    }
    nRoaming = updateRoamingStatus_method3();
    roaming_chk_method = 3;

roaming_set_exit:
    // put database variable
    if (nRoaming == 1)
        szRoaming = "active";
    else if (nRoaming == 0)
        szRoaming = "deactive";
    else
        szRoaming = "";

    rdb_set_single(rdb_name(RDB_ROAMING, ""), szRoaming);

    if (last_roaming_st != nRoaming)
        SYSLOG_INFO("ROAMING CHECK #%d : roaming = %d", roaming_chk_method, nRoaming);
    last_roaming_st = nRoaming;

    return 0;
}

static int strncat_with_escape(char* buf,const char* src,int len)
{
	int i=0;
	char ch;
	
	while(i++<len) {
		/* read  a char from src */
		ch=*src++;
		
		/* put escape */
		if(ch=='&') {
			*buf++='\\';
			*buf++='\\';
		}
		
		/* write the char to buf */
		*buf++=ch;
		if(!ch)
			break;
	}
	
	/* put a null terminatino */
	*buf=0;
	
	return i;
}


#if defined(MODULE_EC21)
// The EC21 can take a long time to execute the AT+COPS=? request.  Advice from Quectel is to
// reissue the command if it fails the first time (the EC21 times out at the 180 second mark).
// Because we might be retrying the command several times we need to call update_heartBeat()
// so that the supervisor process doesn't think SAM has locked up.
//
// Trials have confirmed that immediately rerunning AT+COPS=? lets the algorithm in the EC21
// pick up from where it left off.
//
// None of this behaviour is documented in Quectel's AT manuals and is, presumably, subject to
// change without notification.
//
// Parameters:
// p_okay points to a boolean flag that's set false if the AT command is errored, true otherwise
// response is the output text buffer (the output from at_send_with_timeout() is unmodified)
// max_len corresponds to the parameter of the same name passed to at_send_with_timeout()
//
// Returns the status of the command - non-zero is bad. (Same as at_send_with_timeout().)
int quectel_list_all_cops(int *p_okay, char *response, int max_len)
{
    // Let's make up to three, 3 minute attempts to get the operator list.
    const int ATTEMPTS_TO_MAKE = 3;
    int attempts_left = ATTEMPTS_TO_MAKE;
    *p_okay = 0;
    while (!*p_okay && attempts_left--) {
        update_heartBeat();
        const int stat = at_send_with_timeout(
            "AT+COPS=?", response, "+COPS",
            p_okay, AT_COP_TIMEOUT_SCAN, max_len
        );
        if (stat) {
            // Status is bad.
            return stat;
        }
        if (!*p_okay) {
            syslog(LOG_WARNING, "AT_COPS=? request not okay.  Retrying");
        }
    }
    update_heartBeat();
    return 0;  // Status is good.
}

// From models/model_quectel.c; used in initialize_band_selection_mode() only
extern void reset_quectel_band_selection(void);

extern void quectel_band_fix(void);

#endif


static int handle_network_scan(const struct name_value_t* args)
{
	int ok = 0;
	char *pos;
	char *pos2;
	const char* t;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char *p_response = response;
	char buf[512] = "\0";
	int token_count;

	rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "2");

#if defined(MODULE_EC21)
	if (quectel_list_all_cops(&ok, response, 0)) {
		rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "-1");
		return 0;
	}
#else
	if (at_send_with_timeout("AT+COPS=?", response, "+COPS", &ok, AT_COP_TIMEOUT_SCAN, 0) != 0 || !ok)
	{
		rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "-1");
		return 0;
	}
#endif
	pos = strstr(response, ",,(0,1");
	if (pos)
		*(pos) = 0;
		
	do
	{
		pos = strchr(p_response, '(');
		if (pos)
		{
			pos2 = strchr(pos++, ')');
			if (!pos2) break;
			*pos2 = 0;
			p_response = ++pos2;
			token_count = tokenize_at_response(pos);
			if (token_count != 5)
			{
				break;
			}
			t = get_token(pos, 1);
			if (*buf)
				strcat(buf, "&");
				
			strncat(buf, t + 1, strlen(t) - 2);
			t = get_token(pos, 3);
			strcat(buf, ",");
			strncat(buf, t + 1, 3);
			strcat(buf, ",");
			strcat(buf, t + 4);
			*(buf + strlen(buf) - 1) = ',';
			t = get_token(pos, 0);
			strncat(buf, t, 1);
			t = get_token(pos, 4);
			strcat(buf, ",");
			strncat(buf, t, 1);
		}
	}
	while (pos);
	rdb_set_single(rdb_name(RDB_PLMNLIST, ""), buf);
	if (strlen(buf) > 10)
		rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "3");
	else
		rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "-2");
	return 0;
}

static int handle_network_setnet(const struct name_value_t* args)
{
	int ok = 0;
	char buf[16];
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better

	if (!args || !isdigit(*(args[0].value + 7)))
	{
		rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "-2");
		return -1;
	}
	rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "2");
	sprintf(buf, "AT+COPS=%s", args[0].value + 7);
	if (at_send_with_timeout(buf, response, "", &ok, AT_COP_TIMEOUT, 0) != 0 || !ok)
	{
		rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "-1");
		return -1;
	}
	rdb_set_single(rdb_name(RDB_PLMNSTATUS, ""), "4");
	return 0;
}

static int handle_at_command(const char* command, const char* response_var)
{
	int ok = 0;
	#define UMTS_CMD_RSP_MAX_LEN	AT_RESPONSE_MAX_SIZE*5
	/* max size 10k can contain almost 30 sms msgs list for worst case of UCS2 coding */
	char* response = __alloc(UMTS_CMD_RSP_MAX_LEN);
	__goToErrorIfFalse(response)
	
	memset(response, 0, UMTS_CMD_RSP_MAX_LEN);
    /* wait more for atcmd test program */
    if (strcmp(response_var, "umts.services.command.response") == 0)
        at_send_with_timeout(command, response, "", &ok, 180, UMTS_CMD_RSP_MAX_LEN);
    else
        at_send(command, response, "", &ok, UMTS_CMD_RSP_MAX_LEN);

	/* cut very long response less than AT_RESPONSE_MAX_SIZE to prevent rdb driver panic */
	if (strlen(response) >= UMTS_CMD_RSP_MAX_LEN)
		response[UMTS_CMD_RSP_MAX_LEN-1] = 0x00;
	rdb_set_single(rdb_name(response_var, ""), response);
	__free(response);
	return ok ? 0 : -1;
error:
	return -1;
}
static int handle_clip_command(const struct name_value_t* args)
{
	const char* param = args[0].value + STRLEN("clip ");
	if (strcmp(param, "enable") == 0)
	{
		return handle_clip(at_cmd_enable);
	}
	if (strcmp(param, "disable") == 0)
	{
		return handle_clip(at_cmd_disable);
	}
	SYSLOG_ERR("expected 'enable' or 'disable', got '%s'", param);
	return -1;
}

static char* getFilterDialCommand(const char* szCmd)
{
	const char* szDN = szCmd + strlen("atd");
	char achDN[128];
	// CLIR dial
	char szDNPrefix[16]={0,};
	int is_clip = 0, is_clir = 0;

	SYSLOG_ERR("dial in - %s", szCmd);
#ifdef SKIN_ts

	// bypas if supplementary regex
	if (*szDN == '*' || *szDN == '#')
	{
		struct name_value_t newArgs;

		newArgs.name = 0;
		newArgs.value = szDN;

		if (handleSupplServ(&newArgs) >= 0)
			return 0;
	}

	switch (_clirSubscriberStatus)
	{
		case 3:
			strcpy(szDNPrefix, "I");
			break;

		case 4:
			strcpy(szDNPrefix, "i");
			break;

		default:
			strcpy(szDNPrefix, "");
			break;
	}
#else
	// bypas if supplementary regex
	if (*szDN == '*' || *szDN == '#')
	{
		struct name_value_t newArgs;

		newArgs.name = 0;
		newArgs.value = szDN;

		if (handleSupplServ(&newArgs) >= 0)
			return 0;

		is_clip = (strncmp((szDN+1), "30", 2) == 0);
		is_clir = (strncmp((szDN+1), "31", 2) == 0);
		if((is_clir && (_clirSubscriberStatus==4 || _clirSubscriberStatus==3)) ||
		   (is_clip && _clipSubscriberStatus==1)) {

			if(*szDN == '*')
				strcpy(szDNPrefix, "I");
			else
				strcpy(szDNPrefix, "i");
		}

	}
#endif

	// copy
	strcpy(achDN, szDN);
	if (achDN[strlen(achDN)-1] == ';')
		achDN[strlen(achDN)-1] = 0;

	// build new at command
	static char dialCmd[AT_RESPONSE_MAX_SIZE];
	strcpy(dialCmd, "atd");
	strcat(dialCmd, achDN);
	strcat(dialCmd, szDNPrefix);
	strcat(dialCmd, ";");

	if (is_clip) {
		SYSLOG_ERR("dial out - %s , clip n=%d,m=%d", dialCmd,_clipSubscriberStatusN,_clipSubscriberStatus);
	} else {
		SYSLOG_ERR("dial out - %s , clir n=%d,m=%d", dialCmd,_clirSubscriberStatusN,_clirSubscriberStatus);
	}

	return dialCmd;
}

static int default_handle_command_umts(const struct name_value_t* pRawArgs)
{
	struct name_value_t newArgs;
	struct name_value_t* args = &newArgs;

	memcpy(args, pRawArgs, sizeof(*args));

	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}


	// apply restrict dial command
	if (!strncasecmp(args[0].value, "atd", strlen("atd")))
		args[0].value = getFilterDialCommand(args[0].value);

	// bypass if already processed
	if (!args[0].value || !strlen(args[0].value))
		return 0;

	if (is_at_command(args[0].value))
	{
		return handle_at_command(args[0].value, RDB_UMTS_SERVICES".command.response");
	}
	if (memcmp(args[0].value, "calls ", STRLEN("calls ")) == 0)
	{
		return handle_call_control_command(args);
	}
	if (memcmp(args[0].value, "clip ", STRLEN("clip ")) == 0)
	{
		return handle_clip_command(args);
	}
	if (memcmp(args[0].value, "netscan", STRLEN("netscan")) == 0)
	{
		return handle_network_scan(args);
	}
	if (memcmp(args[0].value, "setnet ", STRLEN("setnet ")) == 0)
	{
		return handle_network_setnet(args);
	}
	else if (strlen(args[0].value) > 2 && (args[0].value[0] == '*' || args[0].value[0] == '#'))
	{
		return handleSupplServ(args);
	}

	SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
	return -1;
}

static int default_handle_command_phonesetup(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}
	if (is_at_command(args[0].value))
	{
		return handle_at_command(args[0].value, RDB_PHONE_SETUP".command.response");
	}
	if (memcmp(args[0].value, "profile", STRLEN("profile")) == 0)
	{
		return 0;
	}
	SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
#define RDB_POTSBRIDGE_DISABLED		"potsbridge_disabled"
BOOL is_potsbridge_disabled(void)
{
	BOOL is_disabled = FALSE;
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
	nvram_init(RT2860_NVRAM);
	rd_data = nvram_bufget(RT2860_NVRAM, RDB_POTSBRIDGE_DISABLED);
	if (!rd_data) {
		goto ret;
	}
#else
	char rd_data[BSIZE_16] = {0, };
	if (rdb_get_single(RDB_POTSBRIDGE_DISABLED, rd_data, BSIZE_16) != 0) {
		goto ret;
	}
#endif
	if (strlen(rd_data) == 0) {
		goto ret;
	}
	if (strcmp(rd_data, "1") == 0) {
		is_disabled = TRUE;
		goto ret;
	}
ret:	
#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(rd_data);
	nvram_close(RT2860_NVRAM);
#endif
	return is_disabled;
}
////////////////////////////////////////////////////////////////////////////////
int update_configuration_setting(const struct model_status_info_t* new_status)
{
	int stat;
	int fOK;
	int fOPN = 0;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	int sim_card_ready = new_status->status[model_status_sim_ready];
	char* command = __alloc(AT_RESPONSE_MAX_SIZE);
    /* read VM status from SIM card when SIM ok and pots_bridge is ready */
	struct
	{
		unsigned char bMsgIndStat;
		unsigned char bNumOfVM;
		unsigned char bNumOfFM;
		unsigned char bNumOfEM;
		unsigned char bNumOfOM;
		unsigned char bNumOfVdoM;
	} __attribute__((packed)) simDataEFmwis;

	static int fVM = 0;
    static int pots_disabled = 0;
    int sim_status = 0;
    int pots_ready = 0;

	static int fCMGF = 0;
	static int fCNMI = 0;
	static int fCPMS = 0;
	static int fCSCA = 0;
	static int fCSCS = 0;
	
	__goToErrorIfFalse(response)
	__goToErrorIfFalse(command)

	if(isNetworkTestMode()) {
		__goToError()
	}
	
	if (is_potsbridge_disabled())
	{
	    pots_disabled = 1;
	}
	if (!pots_disabled)
	{
    	stat = rdb_get_single(rdb_name(RDB_SIM_STATUS, ""), response, 128);
    	if (stat >= 0 && strcmp(response, "SIM OK") == 0)
    	{
    	    sim_status = 1;
    	}
    	stat = rdb_get_single("pots.status", response, 128);
    	if (stat >= 0 && (strcmp(response, "pots_ready") == 0 || strcmp(response, "pots_initialized") == 0))
    	{
    	    pots_ready = 1;
    	}

		//SYSLOG_DEBUG("fVM: %d, sim st: %d, pots_ready: %d", fVM, sim_status, pots_ready);
    	if (!fVM && sim_status && pots_ready)
    	{
    		// read initial VM
    		int cbRead;
    		cbRead = readSIMFile(0x6fca, &simDataEFmwis, sizeof(simDataEFmwis));//28618

    		if (cbRead < 5)
    		{
    			static int fWarned = 0;

    			if (!fWarned)
    				SYSLOG_ERR("failed to read EFMWIS - initial voice mail status");

    			fWarned = 1;
    		}
    		else
    		{
    			SYSLOG_ERR("got EFMWIS: %02x%02x%02x%02x%02x",
    			    simDataEFmwis.bMsgIndStat,
    			    simDataEFmwis.bNumOfVM,
    			    simDataEFmwis.bNumOfFM,
    			    simDataEFmwis.bNumOfEM,
    			    simDataEFmwis.bNumOfOM);
    			fVM = 1;

    			int indiActive = simDataEFmwis.bMsgIndStat & 0x01;

    			if (indiActive)
    				rdb_set_single(rdb_name(RDB_SMS_VOICEMAIL_STATUS, ""), "active");
    			else
    				rdb_set_single(rdb_name(RDB_SMS_VOICEMAIL_STATUS, ""), "inactive");
    		}
    	}
    }
    
	/* fix message flooding when SIM PIN - try the followin AT commands only when SIM card is ready */
	if(sim_status) {

	if (sms_disabled)
		fCMGF = 1;
		
	if (!fCMGF)
	{
		/* override sms text/pdu mode */
		stat = rdb_get_single("smstools.msgmode", response, 128);
		if (stat < 0 || strlen(response) == 0) {
			/* Huawei and Ericsson modems do not support text mode but
			 * Huawei GPRS modem uses Sierra driver so it should compare with variant name. */
			if (!strcmp(model_variants_name(),"EM820U") || is_ericsson)
				pdu_mode_sms = TRUE;
			/* Sequans VZ20Q module is quite buggy in PDU mode */
			else if (strstr(model_variants_name(),"VZ20Q"))
				pdu_mode_sms = FALSE;
			else if (is_cinterion || is_cinterion_cdma) {
				pdu_mode_sms = TRUE;
				syslog(LOG_ERR,"PDU mode forced");
			}
		} else {
			if (strcasecmp(response, "PDU") == 0)
	        	pdu_mode_sms = TRUE;
			else if (strcasecmp(response, "TEXT") == 0)
	        	pdu_mode_sms = FALSE;
		}
		SYSLOG_INFO("pdu mode = %d", pdu_mode_sms);
        if (pdu_mode_sms)
        	// using PDU mode for special modems which do not support text mode
    		stat = at_send("AT+CMGF=0", NULL, "", &fOK, 0);
        else
        	// text mode for message
    		stat = at_send("AT+CMGF=1", NULL, "", &fOK, 0);
		fCMGF = !(stat < 0 || !fOK);
		SYSLOG_INFO("AT+CMGF=%d command %s", (pdu_mode_sms? 0:1), (fCMGF? "success":"failed"));
        if (!pdu_mode_sms) {
			/* set rdb variable to display SMS tx length limit warning message in SMS send web page
			 * for Text Mode */
			rdb_set_single(rdb_name(RDB_SMS_TX_CONCAT_EN, ""), "0");
		}
		// For Cinterion module update SMS Retransmission Timer value to 5s(TT Defect-17861)
		if(is_cinterion) {
			stat = update_cinterion_SMS_retransmission_timer(5);
			if(stat < 0) {
			  SYSLOG_ERR("AT^SCFG=Sms/Retrm,5 command failed");
			}
		}
		/* Some modules (Sequans VZ20Q) only accept text style index such as "ALL" etcs.
		 * So it is better to check which index type is supported. */
		numeric_cmgl_index = pdu_mode_sms;
   		stat = at_send("AT+CMGL=?", response, "", &fOK, 0);
		if (stat >= 0 && fOK && strstr(response, "ALL")) {
			numeric_cmgl_index = 0;
		}
		SYSLOG_INFO("numeric_cmgl_index = %d", numeric_cmgl_index);
	}

	/* need CSDH=1 for smstools to separate text/pdu message */
	if (!pdu_mode_sms) {
		static int fCSDH = 0;
		if (sms_disabled)
			fCSDH = 1;
		if (!fCSDH && !is_cinterion_cdma)
		{
			// enable text mode parameter show
			stat = at_send("AT+CSDH=1", NULL, "", &fOK, 0);
			fCSDH = !(stat < 0 || !fOK);
			SYSLOG_ERR("AT+CSDH=1 command %s", fCSDH? "success":"failed");
		}
	}

	if (sms_disabled)
		fCNMI = 1;
	if (!fCNMI)
	{
		/* Ericsson modem supports mode 2 only */
        if (is_ericsson) {
        	stat = at_send("AT+CNMI=2,3,0,0", NULL, "", &fOK, 0);
    		fCNMI = !(stat < 0 || !fOK);
    		SYSLOG_INFO("AT+CNMI=2,3,0,0 command %s", (fCNMI? "success":"failed"));
		}
		/* Cinterion modem supports MT mode 0 and 1 only */
		else if (is_cinterion || is_cinterion_cdma) {
        	stat = at_send("AT+CNMI=1,1,0,0", NULL, "", &fOK, 0);
    		fCNMI = !(stat < 0 || !fOK);
    		SYSLOG_INFO("AT+CNMI=1,1,0,0 command %s", (fCNMI? "success":"failed"));
		} else if (strstr(model_variants_name(),"VZ20Q")) {
        	stat = at_send("AT+CNMI=3,1,0,0", NULL, "", &fOK, 0);
    		fCNMI = !(stat < 0 || !fOK);
    		SYSLOG_INFO("AT+CNMI=3,1,0,0 command %s", (fCNMI? "success":"failed"));
        } else {
        	stat = at_send("AT+CNMI=1,3,0,0", NULL, "", &fOK, 0);
    		fCNMI = !(stat < 0 || !fOK);
    		SYSLOG_INFO("AT+CNMI=1,3,0,0 command %s", (fCNMI? "success":"failed"));
        }
	}

	if (sms_disabled)
		fCPMS = 1;
	if (!fCPMS)
	{
		/* Try to set SMS storage to ME first for bigger memory space. If fails, then SIM space.
		 * Don't forget Quanta and Ericson modem always store messages to ME regardless of
		 * CPMS setting.
		 */
		if (sms_disabled) {
			sprintf(command, "AT+CPMS=\"MT\",\"MT\",\"MT\"");
		} else {
			sprintf(command, "AT+CPMS=\"ME\",\"ME\",\"ME\"");
		}
		stat = at_send(command, NULL, "", &fOK, 0);
		fCPMS = !(stat < 0 || !fOK);
		SYSLOG_INFO("%s command %s", command, (fCPMS? "success":"failed"));
		if (!fCPMS && !sms_disabled) {
			sprintf(command, "AT+CPMS=\"SM\",\"SM\",\"SM\"");
			stat = at_send(command, NULL, "", &fOK, 0);
			fCPMS = !(stat < 0 || !fOK);
			SYSLOG_INFO("%s command %s", command, (fCPMS? "success":"failed"));
		}
	}

	if (!fCSCA)
	{
		if(!is_cinterion_cdma)
		{
			/* Cinterion 2G module, BGS2-E responds UCS2 format address so
			 * should set GSM mode before reading address */
			if (!(cinterion_type == cinterion_type_2g && char_set_save_restore(1) < 0)) {
				stat = at_send("AT+CSCA?", response, "", &fOK, 0);
				fCSCA = !(stat < 0 || !fOK);
				SYSLOG_INFO("AT+CSCA? command %s", (fCSCA? "success":"failed"));
			}
			if (fCSCA) {
				const char* pToken;
				char *pSavePtr, *pATRes;
				pATRes = response;
				pATRes += (strlen("+CSCA: ")+1);
				pToken = strtok_r(pATRes, "\"", &pSavePtr);
				if (!pToken) {
					SYSLOG_ERR("got +CSCA but failed to parse smsc addr");
					fCSCA = 0;
				} else {
					rdb_set_single(rdb_name(RDB_SMS_SMSC_ADDR, ""), pToken);
				}
			}
			/* Cinterion 2G module, BGS2-E responds UCS2 format address so
			 * should set GSM mode before reading address */
			if (cinterion_type == cinterion_type_2g) {
				(void) char_set_save_restore(0);
			}
		}
		else
		{
			rdb_set_single(rdb_name(RDB_SMS_SMSC_ADDR, ""), "");
			fCSCA = 1;
		}
	}

	}

	/* TODO: more AT commands may require SIM OK status*/

	/* send +CSCS to check IRA character scheme is supported for concatenated message */
	if (sms_disabled)
		fCSCS = 1;
	if (!fCSCS)
	{
		stat = at_send("AT+CSCS=?", response, "", &fOK, 0);
		fCSCS = !(stat < 0 || !fOK);
		SYSLOG_INFO("AT+CSCS=? command %s", (fCSCS? "success":"failed"));
		if (fCSCS && strstr(response,"IRA") != 0) {
			SYSLOG_INFO("IRA char scheme is supported, concatenated Tx SMS is possible");
			ira_char_supported = TRUE;
		} else if (cinterion_type == cinterion_type_2g) {
			SYSLOG_INFO("Cinterion 2G module has implicit IRA mode, concatenated Tx SMS is possible");
			ira_char_supported = TRUE;
		} else {
			SYSLOG_INFO("IRA char scheme is not supported, concatenated Tx SMS is impossible");
			ira_char_supported = FALSE;
			/* set rdb variable to display SMS tx length limit warning message in SMS send web page
			 * for some modules that can not send concatenated Tx message */
			rdb_set_single(rdb_name(RDB_SMS_TX_CONCAT_EN, ""), "0");
		}
	}
	
	
	/* DO NOT SEND +CLIR? COMMAND PRIOR TO +CLIP? COMMAND
	 * Another mysterious behavior of Sierra MC8704 module
	 * If +clip? command is sent later than +clir? and +clir=1 commands
	 * following ussd commands are quite unstable. Module simply returns
	 * CME ERROR or unknown error.
	 */
	static int fGetCLIP = 0;
#ifdef HAS_VOICE_DEVICE
	if (!fGetCLIP && sim_card_ready)
	{
		// get network CLIR
		if (at_send("AT+CLIP?", response, "", &fOK, 0) != 0 || !fOK)
		{
			SYSLOG_ERR("AT+CLIP? command failed - CLIP not able to work");
		}
		else
		{
			char achM[16];
			const char* pN;
			int nM;

			// get CLIP M
			strncpy(achM, response + strlen("+CLIP: 0,"), 1);
			achM[1] = 0;

			// convert to number
			_clipSubscriberStatus = nM = atoi(achM);

			// convert to number
			pN=response + strlen("+CLIP: ");
			_clipSubscriberStatusN = atoi(pN);

			// convert back to string
			sprintf(achM, "%d", nM);

			rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "clip.subscriber_status"), achM);
			SYSLOG_INFO("AT+CLIP? command success");
		}
		fGetCLIP = 1;	/* do not retry CLIP query */
	}
#else
	fGetCLIP=1;
#endif

	static int fGetCLIR = 0;
#ifdef HAS_VOICE_DEVICE
	if (!fGetCLIR && sim_card_ready)
	{
		// get network CLIR
		if (at_send("AT+CLIR?", response, "", &fOK, 0) != 0 || !fOK)
		{
			SYSLOG_ERR("AT+CLIR? command failed - CLIR not able to work");
		}
		else
		{
			char achM[16];
			const char* pN;
			int nM;

			// get CLIR M
			strncpy(achM, response + strlen("+CLIR: 0,"), 1);
			achM[1] = 0;

			// convert to number
			_clirSubscriberStatus = nM = atoi(achM);

			// convert to number
			pN=response + strlen("+CLIR: ");
			_clirSubscriberStatusN = atoi(pN);

			// convert back to string
			sprintf(achM, "%d", nM);

			rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "clir.subscriber_status"), achM);

			SYSLOG_INFO("AT+CLIR? command success");

			// stop reading if not unknown
			if(_clirSubscriberStatus!=2)
				fGetCLIR = 1;
		}
	}
#else
	fGetCLIR=1;
#endif

#ifdef SKIN_ts
	static int fSetCLIR = 0;
#else
	static int fSetCLIR = 1;
#endif

	if (!fSetCLIR && sim_card_ready)
	{
		// enable CLIR
		if (at_send("AT+CLIR=1", response, "", &fOK, 0) != 0 || !fOK)
		{
			SYSLOG_ERR("AT+CLIR=1 command failed - CLIR not able to work");
		}
		else
		{
			fSetCLIR = 1;
			SYSLOG_ERR("AT+CLIR=1 command success");
		}
	}

	/* out band DTMF sending */
	static int fSetVTD = 0;
#ifdef HAS_VOICE_DEVICE
	if (!fSetVTD)
	{
		// set DTMF length to 200 ms
		if (at_send("AT+VTD=2", response, "", &fOK, 0) != 0 || !fOK)
		{
			SYSLOG_ERR("AT+VTD=2 command failed - DTMF length can not be set");
			fSetVTD = 1;	// try once
		}
		else
		{
			fSetVTD = 1;
			SYSLOG_ERR("AT+VTD=2 command success");
		}
	}
#else
	fSetVTD = 1;
#endif

	/* MBDN is referred by SMS and pots_bridge */
	static int fMBDN = 0;
#ifndef HAS_VOICE_DEVICE
	if (sms_disabled)
		fMBDN = 1;
#endif		
	if (!fMBDN && sim_card_ready)
	{
		(void)handleSIMDataMbnMbdnMsisdnAdn(0, SIMFILE_MBDN);
		fMBDN = 1;
	}

	/* MBN and ADN are only referred by pots_bridge */
	static int fMBN = 0;
	static int fADN = 0;
#ifdef HAS_VOICE_DEVICE
	if (!fMBN && sim_card_ready)
	{
		(void) handleSIMDataMbnMbdnMsisdnAdn(0, SIMFILE_MBN);
		fMBN = 1;
	}

	if (!fADN && sim_card_ready)
	{
		(void) handleSIMDataMbnMbdnMsisdnAdn(0, SIMFILE_ADN);
		fADN = 1;
	}
#else
	fMBN = 1;
	fADN = 1;
#endif

	// get own number
	if (sim_card_ready)
    	fOPN = update_call_msisdn();

	__free(response);
	__free(command);
	return fCMGF && fCNMI && fCSCA && fCSCS &&
		   fGetCLIR && fSetCLIR && fGetCLIP &&
		   fSetVTD && (fMBDN | fMBN | fADN) && fOPN;
error:
	__free(response);
	__free(command);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int update_call_msisdn(void)
{
    static int fOPN = 0;
	if (!fOPN)
	{
		if (handleSIMDataMbnMbdnMsisdnAdn(0, SIMFILE_MSISDN) >= 0)
			fOPN = 1;
	}
	return fOPN;
}
////////////////////////////////////////////////////////////////////////////////
int update_call_forwarding(void)
{
#ifdef HAS_VOICE_DEVICE
	// call forwarding
	static int fCallForwarding = 0;
	struct tms tmsbuf;

	if(fCallForwarding)
		return 0;

	long tckPerSec = sysconf(_SC_CLK_TCK);
	clock_t tckCur = times(&tmsbuf);
	static clock_t tckLast = -1;

	int timeExpired = (tckLast == -1) || !((tckCur - tckLast) / tckPerSec < 1 * 60);

	if (timeExpired && !isNetworkTestMode())
	{
		int fError = 0;

		fError = fError || (handle_ccwa_query() < 0);
		fError = fError || (handle_ccfc_query(ccfc_unconditional) < 0);
		fError = fError || (handle_ccfc_query(ccfc_busy) < 0);
		fError = fError || (handle_ccfc_query(ccfc_no_reply) < 0);
		fError = fError || (handle_ccfc_query(ccfc_not_reachable) < 0);


		if (!fError)
			fCallForwarding = 1;

		tckLast = tckCur;
	}
#endif
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int update_signal_strength(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
    char temp[64];
    int token_count;
    const char* t;
	int csq;

	if(!is_enabled_feature(FEATUREHASH_CMD_SIGSTRENGTH)) {
		return 0;
	}
			
	if (at_send("AT+CSQ", response, "+CSQ", &ok, 0) != 0 || !ok)
		return -1;
    token_count = tokenize_at_response(response);
    if (token_count >= 1)
    {
        t = get_token(response, 1);
        csq = atoi(t);
        sprintf(temp, "%ddBm",  csq < 0 || csq == 99 ? 0 : (csq * 2 - 113));
        rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), temp);
    }
    if (token_count >= 2)
    {
        t = get_token(response, 2);
        rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, "bars"), t);
    }
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
extern char* update_sierra_network_technology(void);
static char* check_sierra_cnti_command(void)
{
	char *nameP = NULL;
	nameP = update_sierra_network_technology();
	return (nameP);
}

////////////////////////////////////////////////////////////////////////////////
/* convert PLMN command parameter value (WEBUI) to +COPS argument value */
int convert_network_type(int iMode)
{
	int iAtNw;
#if (defined PLATFORM_PLATYPUS2) || (defined PLATFORM_BOVINE)
	switch (iMode) {
	case 1:
		iAtNw = 0; //GSM (2G)
		break;
	case 2: // Not supported
		iAtNw = 1; //GSM Compact (2G)
		break;
	case 7:
		iAtNw = 2; //UMTS (3G)
		break;
	case 3:
		iAtNw = 3; //GSM/EGPRS (2G)
		break;
	case 4:
		iAtNw = 4; //UMTS/HSDPA (3G)
		break;
	case 5:  // Not supported
		iAtNw = 5; //UMTS/HSUPA (3G)
		break;
	case 6:
		iAtNw = 6; //UMTS/HSUPA (3G)
		break;
	case 9:
		iAtNw = 7; //LTE (4G)
		break;
	case 10:
		iAtNw = 8; //EC-GSM-IoT (A/Gb mode)
		break;
	case 11:
		iAtNw = 9; //E-UTRAN (NB-S1 mode)
		break;
	// case 255: // Automatic network type. Currently this is used only in "set forceplmn" command of SMS command execution function.
	default:
		iAtNw = -1;//Automatic
		break;
	}
#else
	if (iMode == 1)
		iAtNw = 2;
	else
		iAtNw = 0;
#endif
	return iAtNw;
}

/* Once changed operation mode 3G/LTE module keeps it in NV memory over router reboot or
 * factory reset, config restore. This function is implemented in order to restore to
 * saved value in configuration file.
 */
void sync_operation_mode(int service_type)
{
	char plmn_val[64];
	int token_count, saved_service_type = 0, manual_selmode = 0, saved_selmode = 1;
	const struct name_value_t newArgs = { RDB_PLMNCOMMAND, "5" };		/* fill set op command */
	static int synchronized = 0;
	int saved_mcc = 0, saved_mnc = 0, curr_mcc = 0, curr_mnc = 0;

	if (synchronized) {
		syslog(LOG_DEBUG,"[PLMN SYNC] already synchronized, return");
		return;
	}

	/* check module status before changing operation mode */
	if (service_type < 0) {
		syslog(LOG_DEBUG,"[PLMN SYNC] invalid service_type, return");
		return;
	}
	if (rdb_get_single(rdb_name(RDB_SIM_STATUS, ""), plmn_val, sizeof(plmn_val)) != 0 ||
		strcmp(plmn_val, "SIM OK") != 0) {
		syslog(LOG_DEBUG,"[PLMN SYNC] SIM is not ready, return");
		return;
	}
	if (rdb_get_single(rdb_name(RDB_PLMNCOMMAND, ""), plmn_val, sizeof(plmn_val)) != 0 ||
		strlen(plmn_val) != 0) {
		syslog(LOG_DEBUG,"[PLMN SYNC] PLMN cmd status failure or processing ext PLMN cmd, return");
		return;
	}
	if (rdb_get_single(rdb_name(RDB_SERVICETYPE, ""), plmn_val, sizeof(plmn_val)) != 0 ||
		strlen(plmn_val) == 0) {
		syslog(LOG_DEBUG,"[PLMN SYNC] service type is not ready, return");
		return;
	}
	if (rdb_get_single(rdb_name(RDB_PLMNSYSMODE, ""), plmn_val, sizeof(plmn_val)) != 0 ||
		strcmp(plmn_val, "no network service") == 0 || strcmp(plmn_val, "No Network Service") == 0 ||
		strcmp(plmn_val, "Not Available") == 0 || strcmp(plmn_val, "None") == 0 ||
		strcmp(plmn_val, "Invalid service") == 0) {
		syslog(LOG_DEBUG,"[PLMN SYNC] system mode is not ready, return");
		return;
	}

	if (rdb_get_single(rdb_name(RDB_PLMNSELMODE, ""), plmn_val, sizeof(plmn_val)) != 0 || strlen(plmn_val) == 0) {
		syslog(LOG_DEBUG,"[PLMN SYNC] failed to read PLMN_selectionMode or empty, return");
		return;
	}

	if (strcmp(plmn_val, "Manual") == 0) {
		manual_selmode = 1;
	}

	if (rdb_get_single(rdb_name(RDB_PLMNSEL, ""), plmn_val, sizeof(plmn_val)) != 0 || strlen(plmn_val) == 0) {
		syslog(LOG_DEBUG,"[PLMN SYNC] failed to read PLMN_select or empty, return");
		return;
	}

	token_count = tokenize_at_response(plmn_val);
	syslog(LOG_DEBUG,"[PLMN SYNC] PLMN_select '%s', token count %d", plmn_val, token_count);
	if (token_count == 1) {
		saved_selmode = atoi(plmn_val);
	}
	if (token_count == 3) {
		saved_service_type = convert_network_type(atoi(plmn_val));

		// "saved_selmode" is set to specific value when rdb variable "RDB_PLMNSEL" has only one argument.
		// Otherwise, saved_selmode is always regarded as its initial value "1" (Manual operator mode).
		// However, value "0,0,0" of the rdb variable is also used to set Automatic operator mode, so this causes redundant operator mode changes.
		if (saved_service_type == 0)
			saved_selmode = 0;

		const char* t = get_token(plmn_val, 1);
		saved_mcc=strtol(t,0,10);
		t = get_token(plmn_val, 2);
		saved_mnc=strtol(t,0,10);

		syslog(LOG_DEBUG,"[PLMN SYNC] saved svctype %d, mcc %d, mnc %d", saved_service_type, saved_mcc, saved_mnc);
	} else if (token_count != 1 || (token_count == 1 && saved_selmode != 0)) {
		syslog(LOG_DEBUG,"[PLMN SYNC] invalid operation mode, give up");
		synchronized = 1;
		return;
	}

	plmn_val[0] = 0;
	if( rdb_get_single(rdb_name(RDB_PLMNMCC, ""), plmn_val, sizeof(plmn_val)) == 0)
		curr_mcc=strtol(plmn_val,0,10);

	plmn_val[0] = 0;
	if( rdb_get_single(rdb_name(RDB_PLMNMNC, ""), plmn_val, sizeof(plmn_val)) == 0)
		curr_mnc=strtol(plmn_val,0,10);


	syslog(LOG_DEBUG,"[PLMN SYNC] curr PLMN_selMode '%s', curr svctype %d, curr mcc %d, curr mnc %d, saved PLMN_selMode '%s', saved svctype %d, saved mcc %d, saved mnc %d",
		(manual_selmode? "Manual":"Automatic"), service_type, curr_mcc, curr_mnc,
		(saved_selmode? "Manual":"Automatic"), saved_service_type, saved_mcc, saved_mnc);
	if (manual_selmode == saved_selmode && (service_type == saved_service_type || saved_service_type == -1/* auto network type */)) {
		//In the case that current MCC or MNC does not match with saved MCC or MCC in manual selection mode, needed to be synchronized
		if (manual_selmode == 0 || (curr_mcc == saved_mcc && curr_mnc == saved_mnc))
		{
			syslog(LOG_DEBUG,"[PLMN SYNC] no need to synchronise, return");
			synchronized = 1;
			return;
		}
	}

	// It does not need to synchronise in Automatic mode.
	if (manual_selmode == 0 && saved_selmode == 0) {
		syslog(LOG_DEBUG,"[PLMN SYNC] no need to synchronise in Automatic mode, return");
		synchronized = 1;
		return;
	}

	/* now need to change PLMN mode to Manual with saved PLMN_select variable */
	syslog(LOG_INFO,"[PLMN SYNC] synchronise operation mode by force");
	if (model_run_command(&newArgs,0) != 0) {
		SYSLOG_INFO("'%s' '%s' failed", newArgs.name, newArgs.value);
	} else {
		synchronized = 1;
	}
}

////////////////////////////////////////////////////////////////////////////////
// update RDB network name & service type, update output arguments if given
static int update_network_name_service_type(int *service_type_arg, int *plmn_mode_arg)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];

	if (at_send_with_timeout("AT+COPS?", response, "+COPS", &ok, AT_QUICK_COP_TIMEOUT, sizeof(response)) != 0 || !ok) {
		return -1;
	}

	int token_count = tokenize_at_response(response);

	if (token_count >= 1) {
		const char* t = get_token(response, 1);
		rdb_set_single(rdb_name(RDB_PLMNMODE, ""), t);

		// update plmn mode argument if given
		if (plmn_mode_arg) {
			*plmn_mode_arg = atoi(t);
		}
	}

	char buf[64] = {0};	// to store network name, 3GPP TS 24.008 specifies minimum 3 octets but no upper limit.
				// we use max 64 octets as GUI can't display more.
	char network_name[64*3] = {0};	// in HEX format, each char as '%HH', so 3 times size of buf total.
	char *unencoded_network_name = buf; // in uuencoded format (%c) which is same as buf
	int service_type = -1; // default value of -1 means service type unknown.

	if (token_count >= 4) {
		const char* t = get_token(response, 3);
		int offset = *t == '"' ? 1 : 0;				// skip the double qoute
		int size = strlen(t) - offset * 2;			// number of char in token minus qoutes
		size = size < sizeof(buf) ? size : sizeof(buf) - 1;	// max number of char to copy to buf
		memcpy(buf, t + offset, size);
		buf[size] = 0;						// ensure buf is null terminated
		str_replace(&buf[0], "Telstra Mobile", "Telstra");	// replace "Telstra Mobile" to "Telstra"
		str_replace(&buf[0], "3Telstra", "Telstra");		// replace "3Telstra" to "Telstra"
		str_replace(&buf[0], "Telstra Telstra", "Telstra");	// replace "Telstra Telstra" to "Telstra"

		int i, len = 0;
		size = strlen(buf);
		for (i = 0; i < size && len < sizeof(network_name); i++) {
			len += snprintf(network_name + len, sizeof(network_name) - len, "%%%02x", buf[i] );
		}

		if (token_count > 4) {
			service_type = atoi(get_token(response, 4));
			if (service_type < 0 || service_type >= sizeof(service_types) / sizeof(char*)) {
				SYSLOG_ERR("expected service type from 0 to %d, got %d", sizeof(service_types) / sizeof(char*), service_type);
				service_type = -1;
			}
		}
	}

	rdb_set_single(rdb_name(RDB_NETWORKNAME, ""), network_name);
	rdb_update_single(rdb_name(RDB_NETWORKNAME, "unencoded"), unencoded_network_name, CREATE, ALL_PERM, 0, 0);

	char *nameP = NULL;
#if !defined (MODULE_VZ20Q) && !defined(MODULE_EC21)
	nameP = check_sierra_cnti_command();
#endif
	// we use cnti if cnti command returns and the result is not NONE - ZTE821 and Quanta modules always return NONE
	if(nameP && strcmp(nameP,"NONE")) {
		rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), nameP);
	}
	else if (service_type != -1) {
		// only update service type RDB variable if it is successfully read from the module
		rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), service_types[service_type]);
	}

	// update service type arg if given
	if (service_type_arg && service_type != -1) {
		*service_type_arg = service_type;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
const char *plmn_mode_name[5] = {"Automatic", "Manual", "De-register", "Set Only", "manual/automatic" };
int update_network_name(void)
{
	int ok = 0;
	// send at command - set long operator
	int stat = at_send_with_timeout("AT+COPS=3,0", NULL, "", &ok, AT_QUICK_COP_TIMEOUT, 0);
	if (stat || !ok) {
		return -1;
	}

	int service_type = 0;
	int plmn_mode = 0xff;
	if (update_network_name_service_type(&service_type, &plmn_mode) == -1) {
		return -1;
	}

	if (plmn_mode < 5) {
		rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""), plmn_mode_name[plmn_mode]);
	}

	char achOperSel[64];
	int saved_AutoNw=0;
	if(custom_roaming_algorithm){
		achOperSel[0]=0;
		if (rdb_get_single(rdb_name(RDB_PLMNSEL, ""), achOperSel, sizeof(achOperSel)) == 0)
		{
			saved_AutoNw=strtol(achOperSel,0,10);
			if (!saved_AutoNw) {
				rdb_set_single(rdb_name(RDB_PLMNCURMODE, ""), "Automatic network selection");
				rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""), "Automatic");
				service_type=0;
			} else {
				rdb_set_single(rdb_name(RDB_PLMNCURMODE, ""), "Manual network selection");
				rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""), "Manual");
			}
		}
	}

	sync_operation_mode(service_type);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int update_service_type(void)
{
	return update_network_name_service_type(NULL, NULL);
}
////////////////////////////////////////////////////////////////////////////////
#define RDB_OVERRIDE_MCC_TO_NZ  "test.mcc_override"
extern const char Fixed_band_Telstra[];

/* TODO: this function has an incorrect name - function to be called imsi not IMEI */

int update_imsi(void)
{
	int ok = 0;
	char value[ AT_RESPONSE_MAX_SIZE ];

	char mccStr[10];
	char mncStr[10];

	int mcc_override = 0;
	char test_val[10];

	if (at_send("AT+CIMI", value, "", &ok, 0) != 0 || !ok)
	{
		SYSLOG_DEBUG("'%s' not available and scheduled", "AT+CIMI");
		return -1;
	}

	stripStr(value);

	int cbMNC = getMNCLen();

	// telus     : 302220 000000775
	// vodafone  : 50503 7001117767

	if (strlen(value) > 3 + cbMNC)
	{
		if (rdb_set_single(rdb_name(RDB_IMSI".msin", ""), value) < 0)
			SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(RDB_IMSI".msin", ""), value, strerror(errno));
		// copy mcc
		strncpy(mccStr, value + 0, 3);
		mccStr[3] = 0;
		// copy mnc
		strncpy(mncStr, value + 3, cbMNC);
		mncStr[cbMNC] = 0;
		if (rdb_set_single(rdb_name(RDB_IMSI".plmn_mcc", ""), mccStr) < 0)
			SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(RDB_IMSI".plmn_mcc", ""), value, strerror(errno));
		if (rdb_set_single(rdb_name(RDB_IMSI".plmn_mnc", ""), mncStr) < 0)
			SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(RDB_IMSI".plmn_mnc", ""), value, strerror(errno));

		/* use !gband command and hard-coded bands list rather than !band command
			for New Zealand same as Telstra requested band list for MC8704. */
		if (use_gband_command < 0) {
			if (rdb_get_single(RDB_OVERRIDE_MCC_TO_NZ, test_val, sizeof(test_val)) >= 0 &&
				strcmp(test_val, "1") == 0) {
				mcc_override = 1;
			}
			if ((strcmp(model_variants_name(),"MC8704") == 0 && strcmp(mccStr, "530") == 0) || mcc_override) {
				SYSLOG_INFO("set use_gband_command = 1");
				use_gband_command = 1;
				SYSLOG_DEBUG("set '%s' to '%s'", rdb_name(RDB_MODULEBANDLIST, ""), Fixed_band_Telstra);
				rdb_set_single(rdb_name(RDB_MODULEBANDLIST, ""), Fixed_band_Telstra);
				/* MC8704 default mic gain is 0x2000 until T3.0.2.1 and it is not loud enough
				* for TNZ New Zealand. Apply new mic gain 0x8000 once when MCC is 530 */
				(void)sierra_change_tx_mic_gain();
			} else {
				SYSLOG_INFO("set use_gband_command = 0");
				use_gband_command = 0;
			}
		}
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int model_default_write_sim_status(const char* sim_status)
{
	const char* db_sim_status;
	int stat = 0;

	if(!sim_status)
		db_sim_status="SIM not inserted";
	else if(!strcmp(sim_status,"SIM PIN"))
		db_sim_status="SIM PIN";
	else if (!strcmp(sim_status, "READY") || !strcmp(sim_status, "USIM READY"))
		db_sim_status="SIM OK";
	else
		db_sim_status=sim_status;

	if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS) || support_pin_counter_at_cmd) {
		stat = rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), db_sim_status);
	}
	check_or_update_pin_counter(strcmp(db_sim_status,"SIM not inserted"));
	return stat;
}

////////////////////////////////////////////////////////////////////////////////
// TT#5686 power-cycle moduel if SIM not inserted
static int detect_sim_card_failure(int atres,int atok, int max_sim_failure_count)
{
	int onhook;
	char offhook[16];
	static int sim_card_failure=0;

	// TODO: the prefix name is incorrect.
	// Note: If no RDB (as it is in most cases), will still continue with SIM failure detection.
	// May be we can delete this code altogether.
	if(rdb_get_single("manualroam.sim_fail_recovery_off", offhook, sizeof(offhook))!=0)
		*offhook=0;
	onhook=!atoi(offhook);

	//syslog(LOG_ERR,"onhook=%d,atres=%d,atok=%d",onhook,atres,atok);

	// power cycle if sim card failure is detected only it is onhook
	if(onhook && (!atres || !atok)) {
		if(sim_card_failure<max_sim_failure_count) {
			sim_card_failure++;
			syslog(LOG_ERR,"SIM card failure detected! #%d/%d",sim_card_failure,max_sim_failure_count);
		}
	}
	else {
		sim_card_failure=0;
	}

	// reboot module if it fails enough
	if(sim_card_failure>=max_sim_failure_count) {
		syslog(LOG_ERR,"SIM card failure detected! power-cycle the module");
		system("reboot_module.sh");
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int update_sim_status(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	int token_count;
	const char* sim_status;
	static int fail_counter=0;
	int sim_pin_cmd_ok;

	SET_LOG_LEVEL_TO_SIM_LOGMASK_LEVEL
	// query SIM status
	sim_pin_cmd_ok=at_send("AT+CPIN?", response, "+CPIN", &ok, 0)==0;

	#if defined(BOARD_falcon)
		#ifdef CELL_NW_cdma
			// we do not apply this hardware workaround if cdma
		#else
			detect_sim_card_failure(sim_pin_cmd_ok,ok, 10);
		#endif
	#elif defined(BOARD_vicky) && defined(MODULE_cinterion)
		// 7 chosen as optimal number after testing on 8000c
		detect_sim_card_failure(sim_pin_cmd_ok,ok, 7);
	#endif

	// get SIM status
	if (!sim_pin_cmd_ok) {
		check_or_update_pin_counter(0);
		goto ret_ok;
	}

	// bypass if SIM not inserted
	if (!ok)
	{
		if(++fail_counter > 2) {	/* changed to 2 from 5 */
			rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM not inserted");
			check_or_update_pin_counter(0);
		} else {
			rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "Negotiating");
		}
		/* clean simICCID when No SIM detected */
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), "");
		
		goto ret_ok;
	}
	fail_counter=0;
	// get tokens
	token_count = tokenize_at_response(response);
	if (token_count < 2)
	{
		SYSLOG_ERR("invalid response from AT");
		goto ret_error;
	}

	sim_status = get_token(response, 1);
	if (!strcmp(sim_status, "SIM PIN") || !strcmp(sim_status, "USIM PIN"))
	{
		char szStoredPin[32] = {0, };

		static int fTried = 0;
		int stat, auto_pin;

		stat = getRdbAutoPin(szStoredPin, sizeof(szStoredPin));
		auto_pin = (stat == 0);

#ifdef PLATFORM_PLATYPUS
		nvram_init(RT2860_NVRAM);
#endif		
		if (stat < 0 || !strlen(szStoredPin))
		{
			// get stored pin
#ifdef PLATFORM_PLATYPUS
			char* pin = nvram_get(RT2860_NVRAM, "wwan_pin");
			auto_pin = !0;
#else
			char* pin = alloca(64);
			rdb_create_and_get_single("wwan_pin", pin, 64);
#endif
			if (pin)
				strcpy(szStoredPin, pin);
#ifdef PLATFORM_PLATYPUS
			nvram_strfree(pin);
#endif
		}

		char achStrCCID[64];
#ifdef PLATFORM_PLATYPUS
		char* szCCID = nvram_get(RT2860_NVRAM, "wwan_ccid");
		char *pinStrCCID = nvram_get(RT2860_NVRAM, "wwan_pin_ccid");
#else
		char* szCCID = alloca(64);
		char *pinStrCCID = alloca(64);
		rdb_create_and_get_single("wwan_ccid", szCCID, 64);
		rdb_create_and_get_single("wwan_pin_ccid", pinStrCCID, 64);
#endif
		// get current ccid
		stat = rdb_get_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), achStrCCID, sizeof(achStrCCID));
		if (stat < 0) {
			achStrCCID[0] = 0;
		}
		SYSLOG_DEBUG("wwan_ccid = %s, wwan_pin_ccid = %s", szCCID, pinStrCCID);
		SYSLOG_DEBUG("achStrCCID = %s, szCCID = %s", achStrCCID, szCCID);
		
		// do not try if ICCIDs are not match
		if ( *achStrCCID && *pinStrCCID && strcmp(achStrCCID, pinStrCCID)) {
			SYSLOG_ERR("strcmp(achStrCCID, pinStrCCID): %s does not match with %s, clear achStrCCID", achStrCCID, pinStrCCID);
			szStoredPin[0] = 0;
		}
#ifdef PLATFORM_PLATYPUS
		// do not try if already tried
		if (strlen(achStrCCID) && !strcmp(achStrCCID, szCCID)) {
			SYSLOG_ERR("strcmp(achStrCCID, szCCID): %s does not match with %s, clear achStrCCID", achStrCCID, szCCID);
			szStoredPin[0] = 0;
		}
#endif

/* This #ifdef can be conbined with upper #ifdef but is deliberately left separately
 * to divide logical part and nvram part */
#ifdef PLATFORM_PLATYPUS
		nvram_strfree(szCCID);
		nvram_strfree(pinStrCCID);
#endif
		convClearPwd(szStoredPin);

#ifdef PLATFORM_PLATYPUS
		nvram_close(RT2860_NVRAM);
#endif

		/* ----------------------------------------------------------------------------------
		 * unlock SIM only when it is really needed
		 * ----------------------------------------------------------------------------------
		 * Added more condition to do auto unlocking operation because of PIN retry count
		 * consumption error. It caused by WEBUI modification which leads users turn on
		 * autopin option unconsciously when the user enters PIN code. Previous bug flow is as below;
		 * on 1st try with incorrect PIN: remaining count-- and autopin is set to 1 so do unlocking,
		 * 								  remaining count-- again to decrease to 1.
		 * on 2nd try with incorrect PIN: remaining count-- then turn into PUK state.
		 * Allow auto unlocking only when last pin number failed to verify is different to
		 * auto pin number not to waste retry count after previous SIM operation failed.
		 * If any problem in this logic, please contact Kwonhee
		 */
		if (strlen(szStoredPin) && !fTried && auto_pin &&
			(strlen((char *)&last_failed_pin) == 0 || strcmp((char *)&last_failed_pin, szStoredPin)))
		{
			verifyPin(szStoredPin);
			rdb_set_single(rdb_name(RDB_AUTOPIN_TRIED, ""), "1");
			fTried = !0;
			if (is_ericsson)
				sleep(1);
		}
	}

	updateSIMStat();
ret_ok:
	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return 0;

ret_error:
	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return -1;
}
////////////////////////////////////////////////////////////////////////////////

/*
 * Helper function for model_default_get_status to strip quotations from some
 * strings such as the LAC and cellid strings.
 * @string The string to be stripped
 * @return A pointer to the new string
 */
char *strip_quotes (char *string)
{
	int last_char;
	if (string[0] == '"') {
		/* move the start of the string to the next character */
		string++;
	}
	last_char = strlen(string) - 1;
	if (last_char != 0 && string[last_char] == '"') {
		/* delete the quotes character */
		string[last_char] = '\0';
	}
	return string;
}

static int convert_cpinr_to_cpinc(char *s, int len)
{
	char* pNewS = strdup(s);
	char *pToken, *pToken2, *pSavePtr, *pSavePtr2, *pATRes;
	int pinc1, pinc2, pukc1, pukc2;
	if (!pNewS) {
		SYSLOG_ERR("failed to allocate memory in %s()", __func__);
		goto error;
	}

	pToken = strtok_r(pNewS, ",", &pSavePtr); if (!pToken) goto error;
	pToken = strtok_r(NULL, ",", &pSavePtr); if (!pToken) goto error;
	pinc1 = atoi(pToken);
	pToken = strtok_r(NULL, "\n", &pSavePtr); if (!pToken) goto error;
	pinc2 = atoi(pToken);
	pToken = strtok_r(NULL, ",", &pSavePtr); if (!pToken) goto error;
	pToken = strtok_r(NULL, ",", &pSavePtr); if (!pToken) goto error;
	pukc1 = atoi(pToken);
	pToken = strtok_r(NULL, "\n", &pSavePtr); if (!pToken) goto error;
	pukc2 = atoi(pToken);
	snprintf(s, len, "+CPINC: %d, %d, %d, %d", pinc1, pinc2, pukc1, pukc1);
	
	if (pNewS)
		free(pNewS);
	return 0;
error:		
	if (pNewS)
		free(pNewS);
	return -1;
}


/* Check whether the module support AT+CPINC command to 
 * read remaining PIN/PUK count value.
 * This function should be called first before calling subscribe() in init_at_manager() and
 * set wwan.0.sim.cpinc_supported variable which will be checked by cnsmgr when it launches. */
void check_or_update_pin_counter(int sim_exist)
{
	int stat = 0, fOK, token_count, tmp_cnt;
	int pin_count = 255, puk_count = 255;
	const char* t;
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[16] = {0, };

	if(!is_enabled_feature(FEATUREHASH_CMD_SIMCARD)) {
		SYSLOG_DEBUG("%s: command profile feature not enabled - %s",__FUNCTION__,FEATUREHASH_CMD_SIMCARD);
		return;
	}

	if(support_pin_counter_at_cmd == 0) {
		return;
	}
	SET_LOG_LEVEL_TO_SIM_LOGMASK_LEVEL
	stat = at_send("AT+CPINC?", response, "+CPINC", &fOK, AT_RESPONSE_MAX_SIZE);
	if (stat < 0 || !fOK || strstr(response, "NOT IMPLEMENTED")) {
		support_pin_counter_at_cmd = 0;
		/* check Sequans +CPINR command */
		if (strstr(model_variants_name(),"VZ20Q")) {
			stat = at_send("AT+CPINR", response, "+CPINR", &fOK, AT_RESPONSE_MAX_SIZE);
			if (stat >= 0 && fOK) {
				SYSLOG_DEBUG("converting +CPINR resp to +CPINC resp format");
				if (convert_cpinr_to_cpinc((char *)&response[0], AT_RESPONSE_MAX_SIZE) < 0) {
					return;
				}
				support_pin_counter_at_cmd = 1;
			}
		}
	} else {
		support_pin_counter_at_cmd = 1;
	}
	SYSLOG_DEBUG("support_pin_counter_at_cmd = %d", support_pin_counter_at_cmd);
	rdb_set_single(rdb_name(RDB_SIMCPINC, ""), support_pin_counter_at_cmd ? "1" : "0");
	
	/* parse remaining counter if SIM card is ready and supports +CPINC command */
	if (sim_exist && support_pin_counter_at_cmd) {
	    token_count = tokenize_at_response(response);
	    /* +CPINC: <n1>, <n2>, <k1>, <k2>
			<n1> (Remaining attempts for PIN1/CHV1):
				0=Blocked
				1-3=Remaining attempts
			<n2> (Remaining attempts for PIN2/CHV2):
				0=Blocked
				1-3=Remaining attempts
			<k1> (Remaining attempts for PUK1):
				0=Blocked
				1-10=Remaining attempts
			<k2> (Remaining attempts for PUK2):
				0=Blocked
				1-10=Remaining attempts
		*/
		/* read PIN count */
	    if (token_count >= 1)
	    {
	        t = get_token(response, 1);
	        if (!t) {
				SYSLOG_ERR("failed to get PIN count token");
	        } else {
		        tmp_cnt = atoi(t);
				SYSLOG_DEBUG("PIN count = %d", tmp_cnt);
		        if (tmp_cnt <= 3) {
					pin_count = tmp_cnt;
		        }
		    }
	    }
		/* read PUK count */
	    if (token_count >= 3)
	    {
	        t = get_token(response, 3);
	        if (!t) {
				SYSLOG_ERR("failed to get PUK count token");
	        } else {
		        tmp_cnt = atoi(t);
				SYSLOG_DEBUG("PUK count = %d", tmp_cnt);
		        if (tmp_cnt <= 10) {
					puk_count = tmp_cnt;
		        }
		    }
	    }
	}
    sprintf(buf, "%d", pin_count);
    rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), buf);
    sprintf(buf, "%d", puk_count);
    rdb_set_single(rdb_name(RDB_SIMPUKREMAIN, ""), buf);
	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
}

int model_default_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
	char* resp;
	if (support_pin_counter_at_cmd == -1) {
		check_or_update_pin_counter(0);
	}
	
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

	if(status_needed->status[model_status_registered])
	{
		char *stat_str;		// network status
		char *lac_str; 		// LAC code
		char *cellid_str;	// cell ID

		int nw_stat;

/* Sequans VZ20Q module supports packet mode only */
#if defined (MODULE_VZ20Q)
		char *rac_str; 		// RAC code
		char *act_str; 		// AcT code
		int use_cereg = 0;
	if (strstr(model_variants_name(),"VZ20Q")) {
		resp = at_bufsend("AT+CEREG?","+CEREG: ");
		use_cereg = 1;
	} else
#endif		
		resp = at_bufsend("AT+CREG?","+CREG: ");
		if (resp != 0)
		{
			strtok(resp, ",");
			stat_str = strtok(NULL, ",");
			lac_str  = strtok(NULL, ",");
/* Sequans VZ20Q module supports packet mode only */
#if defined (MODULE_VZ20Q)
			if (use_cereg)
				rac_str  = strtok(NULL, ",");
#endif


/* Sequans VZ20Q module returns +CEREG response without CI field? */
#if !defined (MODULE_VZ20Q)
			cellid_str = strtok(NULL, ",");
#endif

/* Sequans VZ20Q module supports packet mode only */
#if defined (MODULE_VZ20Q)
			if (use_cereg) {
				act_str  = strtok(NULL, ",");
				if (act_str) {
					SYSLOG_DEBUG("Act = %s", act_str);
					nw_stat = atoi(act_str);
					new_status->status[model_status_attached]=(nw_stat==7);
					err_status->status[model_status_attached]=0;
					/* VZ20Q may have nw_stat as 7 always */ 
					if(nw_stat >= 0 && nw_stat <= 7) {
						rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), service_types[nw_stat]);
						rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), service_types[nw_stat]);
					}
					else {
						rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "");
						rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
					}
				} else {
					SYSLOG_DEBUG("Act = NULL");
				}
			}
#endif

			nw_stat = -1;
			if (stat_str) {
				nw_stat = atoi(stat_str);
			}
			if (lac_str) {
				lac_str = strip_quotes(lac_str);
				rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC", ""), lac_str);
			}
/* Sequans VZ20Q module supports packet mode only */
#if defined (MODULE_VZ20Q)
			if (use_cereg && rac_str) {
				rac_str = strip_quotes(rac_str);
				rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RAC", ""), rac_str);
			}
#endif
			if (cellid_str) {
				cellid_str = strip_quotes(cellid_str);
				rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), hextodec(cellid_str));
			}

			if(is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
				if( rdb_set_single_int(rdb_name(RDB_NETWORKREG_STAT,""),nw_stat)<0 ) {
					syslog(LOG_ERR,"failed to update rdb - %s",RDB_NETWORKREG_STAT);
				}
			}

			new_status->status[model_status_registered]=(nw_stat==1) || (nw_stat>=5);
		}

		err_status->status[model_status_registered]=!resp;
	}

	if(status_needed->status[model_status_attached])
	{
		if( (resp=at_bufsend("AT+CGATT?","+CGATT: ")) !=0 )
			new_status->status[model_status_attached]=atoi(resp)!=0;

		err_status->status[model_status_attached]=!resp;
	}

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int getPDPStatus(int pid)
{
	#define AT_SCACT_TIMEOUT	30

	int stat;

	int ok;
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
	char* resp;

	/* Cinterion 2G module, BGS2-E only has GPRS mode and +CGACT does not show
	 * PDP status properly, instead has to use +CGATT command for GPRS attach status. */
	if (is_cinterion && cinterion_type == cinterion_type_2g) {
		if( (resp=at_bufsend("AT+CGATT?","+CGATT: ")) !=0 )
			return atoi(resp);
		else
			goto err;
	}

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

	char* p;
	const char* szPdpNo;
	const char* szPdpStat;
	
	int pdp_no;
	int pdp_stat;
	
	p=strstr(response,"+CGACT: ");
	
	while(p) {
		p+=STRLEN("+CGACT: ");

		// get pdp number
		szPdpNo = _getFirstToken(p, "(),");
		if(!szPdpNo)
			break;
		pdp_no=atoi(szPdpNo);

		// gget pdp stat
		szPdpStat = _getNextToken();
		if(!szPdpStat)
			break;
		pdp_stat=atoi(szPdpStat);
			
		/* return pdp stat */
		if(!pid || (pid==pdp_no))
			return pdp_stat;
			
		p=strstr(p,"+CGACT: ");
	}

err:
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int update_pdp_status()
{
	/* skip if the variable is updated by other port manager such as cnsmgr */
	if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS)) {
    	// get pdp connections status
    	int pdpStat;
    	
    	const char* rdbs[]={RDB_PDP0STAT,RDB_PDP1STAT,NULL};
    	int i;

	i=0;
	/* while rdb exists and pdp status returns */
	while(rdbs[i]) {
		/* get pdp status of the profile */
		pdpStat=getPDPStatus(i+1);
		
		if(pdpStat>=0)
			rdb_set_single(rdb_name(rdbs[i], ""), pdpStat==0?"down":"up");
		else
			rdb_set_single(rdb_name(rdbs[i], ""), "");
			
		i++;
	}
    }
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int model_default_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	if(!at_is_open())
		return 0;

	update_sim_hint();
	update_roaming_status();

    /* set default band sel mode to auto after SIM card is ready to prevent
     * 3G module is locked in limited service state */
    initialize_band_selection_mode(new_status);

	update_network_name();
	update_service_type();

	//update_date_and_time();

	/* skip if the variable is updated by other port manager such as cnsmgr or
	 * process SIM operation if module supports +CPINC command */
	if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS) || support_pin_counter_at_cmd) {
		update_sim_status();
		/* Latest Sierra modems like MC8704, MC8801 etcs returns error for +CSQ command
		 * when SIM card is locked so update signal strength with cnsmgr. */
		if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS)) {
			update_signal_strength();
		}
		//update_pdp_status();
	}
	update_ccid();
	update_imsi();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int update_ccid(void)
{
	char achCCID[64];
	int cbCCID;
	char response[4096];
	int ok;

	static int ccidLoad=0;

	if(ccidLoad)
		return 0;

	// bypass if SIM not inserted
	if (at_send("AT+CPIN?", response, "+CPIN", &ok, 0) == 0 && !ok) {
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), "");
		return 0;
	}

	// get CCID
	cbCCID = readSIMFile(0x2fe2, achCCID, sizeof(achCCID));//12258
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

///////////////////////////////////////////////////////////////////////////////
// A RDB boolean flag ("1" of "0") only used in initialize_band_selection_mode()
// (and for the Quanta phone module, in products that use that).
// The value is "0" following a factory reset and is set to "1" by initialize_band_selection_mode()
// on first run.  It only returns to "0" on factory reset.
// If "1" then band settings in the phone module are not reset on start-up.
#define RDB_BAND_SEL_MODE_INIT		"band_selection_mode_initialized"


// Called from init_at_manager() and each model's model_t.set_status() callback.
//
// Ensures that the band selection setting in a phone module is cleared on factory reset but
// otherwise persists over reboots and power-cycles.  (An example selection might be restricting the
// router to use LTE Band 3 only.)   As the modem needs this information when it starts up we must
// let it store the band preferences its own non-volatile memory, rather than as a persistent RDB
// variables.
void initialize_band_selection_mode(const struct model_status_info_t* new_status)
{
	int sim_card_ready = 0, sim_card_inserted = 1;
	static int band_sel_initialized = 0, band_mode_initialized = 0;
	int stat, ok = 0;
	char achATRes[16];
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
#else
	char rd_data[10] = {0, };
#endif
    char response[64];
	char band_init_cmd[128] = {0, };

#if defined(MODULE_EC21)
    // For the EC21/EC25 we disregard all of the band_sel_initialized and band_mode_initialized
    // flags.  The logic is simple: if the non-volatile RDB variable band_selection_mode_initialized
    // is false then the router has just been factory reset and we need to clear any band selection
    // settings from the phone module.  Having done this, we set the RDB variable to true; it
    // retains this value until the next factory reset.
    if (rdb_get_single(RDB_BAND_SEL_MODE_INIT, rd_data, 10) != 0) {
        SYSLOG_ERR("Read of band_selection_mode_initialized failed");
        return;
    }
    if (strcmp(rd_data, "0") == 0) {
        SYSLOG_NOTICE("Reset Quectel band settings following factory reset");
        reset_quectel_band_selection();
        rdb_set_single(RDB_BAND_SEL_MODE_INIT, "1");
    }
    return;
#endif  // defined(MODULE_EC21)

	sim_card_ready = new_status->status[model_status_sim_ready];
    if (!sim_card_ready && (!band_sel_initialized || !band_mode_initialized)) {
    	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0) {
    	    // Error
    		return;
    	}
    	/* initialize band selection mode when SIM is not inserted */
    	if(!ok) {
    		sim_card_inserted = 0;
    	}
    }

	if (band_sel_initialized && band_mode_initialized) {
		// We've already set up the band (in this process instance), so ...
		return;
	}

#if defined(PLATFORM_PLATYPUS)
	nvram_init(RT2860_NVRAM);
	rd_data = nvram_bufget(RT2860_NVRAM, RDB_BAND_SEL_MODE_INIT);
	if (!rd_data)
		goto ret;
#else
	if (rdb_get_single(RDB_BAND_SEL_MODE_INIT, rd_data, 10) != 0)
		goto ret;
#endif
	if (strlen(rd_data) == 0) {
		//ERROR
		goto ret;
	}

	if (strcmp(rd_data, "0") == 0) {
	    // The band selection mode has not been initiated since factory restart.
	    // We need to wipe any band settings kept in the module's NV storage.

		/* AT!BAND=0 only for Sierra modem */
		//SYSLOG_INFO("model_name == %s", model_name());
		/* do not send at!band=0 command when SIM card is locked to prevent
			* 3G modules are locked in Limited service. */
		if (!strncmp(model_name(), "sierra", sizeof("sierra"))) {
			strcpy(band_init_cmd, "AT!BAND=0");
		} else if (is_cinterion) {
			/* BGS2-E can only select handover */
			if (cinterion_type == cinterion_type_2g)
				strcpy(band_init_cmd, "AT^SCFG=\"Radio/Band/Handover\",1");
			else
				strcpy(band_init_cmd, "AT^SCFG=\"Radio/Band\",511,1");
		}
		if (strlen(band_init_cmd)) {
			//if (!band_sel_initialized && (sim_card_ready || !sim_card_inserted)) {
			if (!band_sel_initialized) {
				stat = at_send_with_timeout(band_init_cmd, achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 0);
				if (stat || !ok)
					goto ret;
				SYSLOG_ERR("succeed : %s", band_init_cmd);
				band_sel_initialized = 1;
			}
		}
		/* MC8704 module dies with this command after 3G module power-up without SIM card,
			* so do not send this command until SIM card is not ready. */
		if (!band_mode_initialized && sim_card_ready && sim_card_inserted) {
			/* set to automatic mode */
			stat = at_send_with_timeout("AT+COPS=0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 0);
			if (stat || !ok)
				goto ret;
			SYSLOG_ERR("succeed : AT+COPS=0");
			band_mode_initialized = 1;
		}
		if (band_sel_initialized && band_mode_initialized) {
			#if defined(PLATFORM_PLATYPUS)
			nvram_set(RT2860_NVRAM, RDB_BAND_SEL_MODE_INIT, "1");
			#else
			rdb_set_single(RDB_BAND_SEL_MODE_INIT, "1");
			#endif
		}
	} else if (strcmp(rd_data, "1") == 0) {
		band_sel_initialized = band_mode_initialized = 1;
	}
ret:
#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(rd_data);
	nvram_close(RT2860_NVRAM);
#endif
	return;
}
///////////////////////////////////////////////////////////////////////////////
static int model_default_init(void)
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int wait_on_network_reg_stat(int registered,int sec, int net_stat)
{
	struct tms tmsbuf;
	/*
		at_send_with_timeout tries 5 times
	*/
	

	clock_t now=times(&tmsbuf);
	clock_t persec=sysconf(_SC_CLK_TCK);
	clock_t start=now;

	if(registered<0)
		syslog(LOG_INFO,"waiting for %d - detaching",sec);
	else
		syslog(LOG_INFO,registered?"wait until network registered":"wait until network unregistered");

	while(1) {
		switch(net_stat) {
			case 2: // not registered
				if(registered==0)
					return 0;
				break;

			case 3: // registration denied
			case 4: // unknown
			case 6: // registered SMS only, home network
			case 7: // registered SMS only, roaming
			case 8: // emergency bearer
				break;

			case 1: // registered, home network
			case 5: // registered, roaming
				if(registered>0)
					return 0;
				break;
		}

		at_wait_notification(-1);

		now=times(&tmsbuf);
		if( (now-start)/persec>sec ) {
			syslog(LOG_INFO,"waiting timeout");
			break;
		}
		
		sleep(1);
	}

	return -1;
}


///////////////////////////////////////////////////////////////////////////////
const char* _szTokenDeli = NULL;
const char* _pTokenSrc = NULL;

const char* _getNextToken()
{
	static char achToken[256];
	const char* szRet = NULL;

	int fDel = TRUE;
	int fNewDel;
	const char* pS = _pTokenSrc;

	int iS = 0;
	int iE;

	/* This function was designed based on the assumption all field are existing but
	 * Cinterion 2G modem BGS2-E responses with empty field against +COPS=? command so
	 * it is required to check empty field here.
	 */
	if (*(_pTokenSrc-1) == ',' && *_pTokenSrc == ',') {
		syslog(LOG_ERR,"found empty field, return immediately");
		_pTokenSrc++;
		return "";
	}
	
	while (!szRet)
	{
		fNewDel = !*_pTokenSrc || (strchr(_szTokenDeli, *_pTokenSrc) != 0);

		if (fDel && !fNewDel)
		{
			iS = _pTokenSrc - pS;
		}
		else if (!fDel && fNewDel)
		{
			iE = _pTokenSrc - pS;

			memcpy(achToken, &pS[iS], iE - iS);
			achToken[iE-iS] = 0;

			szRet = achToken;
		}

		if (!*_pTokenSrc)
			break;

		_pTokenSrc++;

		fDel = fNewDel;
	}

	return szRet;
}
///////////////////////////////////////////////////////////////////////////////
int _isMCCMNC(const char* str)
{
	if ((strlen(str) != strlen("50503")) && (strlen(str) != strlen("505003")))
		return FALSE;

	while (*str)
	{
		if (!isdigit(*str++))
			return FALSE;
	}

	return TRUE;
}
///////////////////////////////////////////////////////////////////////////////
int _getMCC(const char* str)
{
	char achMCC[16] = {0, };

	memcpy(achMCC, str, 3);

	return atoi(achMCC);
}
///////////////////////////////////////////////////////////////////////////////
int _getMNC(const char* str)
{
	char achMNC[16] = {0, };

	strcpy(achMNC, &str[3]);

	return atoi(achMNC);
}
///////////////////////////////////////////////////////////////////////////////
const char* _getFirstToken(const char* szSrc, const char* szDeli)
{
	_pTokenSrc = szSrc;
	_szTokenDeli = szDeli;

	return _getNextToken();
}

///////////////////////////////////////////////////////////////////////////////
int _convATOpListToCNSOpList(const char* szAtOpers, char* achBuf, int cbBuf)
{
	// clear the return
	achBuf[0] = 0;

	int iToken = 0;
	char opTokens[5][32];
	
	char mcc_str[4];
	char mnc_str[4];

	const char* pToken = _getFirstToken(szAtOpers, "(),");
	while (pToken)
	{
		int iToklen = 0;
		// store the token
		if (pToken[0] == '"')
		{
			iToklen = strlen(pToken);
			strncpy(opTokens[iToken], &pToken[1], 30);
			opTokens[iToken][iToklen-2] = '\0';
		}
		else
		{
			strncpy(opTokens[iToken], pToken, 31);
		}
		opTokens[iToken][31] = '\0';

		// get next token
		pToken = _getNextToken(szAtOpers, "(),");
		if (cinterion_type == cinterion_type_2g)
			iToken = (iToken + 1) % 4;
		else
			iToken = (iToken + 1) % 5;

		// keep on doing if not the last
		if (iToken)
			continue;

		// bypass if not MCCMNC
		const char* szMCCMNC = opTokens[3];
		if (!_isMCCMNC(szMCCMNC))
			continue;

		/*

				# cns format : vodafone AU,505,3,20,1

				# at format : UMTS

				  0 1             2         3       4
				 (2,"vodafone AU","voda AU","50503",2)

				# at format : 2G only

				  0 1             2         3      
				 (2,"vodafone AU","voda AU","50503")

		*/

		// get MCC & MNC
		strncpy(mcc_str, szMCCMNC, sizeof(mcc_str));
		mcc_str[sizeof(mcc_str)-1]=0;
		strncpy(mnc_str, szMCCMNC+3, sizeof(mnc_str));
		mcc_str[sizeof(mnc_str)-1]=0;
		
		// get all tokens
		const char* szOp = opTokens[1];
		int nStat = atoi(opTokens[0]);
		int nAcT;

		if (cinterion_type == cinterion_type_2g)
			nAcT = 0;
		else
			nAcT = atoi(opTokens[4]);

		int nNwType;
		switch (nAcT)
		{
			case 0:
				nNwType = 1; //GSM (2G)
				break;
			case 1:
				nNwType = 2; //GSM Compact (2G)
				break;
			case 2:
				nNwType = 7; //UMTS (3G)
				break;
			case 3:
				nNwType = 3; //GSM/EGPRS (2G)
				break;
			case 4:
				nNwType = 4; //UMTS/HSDPA (3G)
				break;
			case 5:
				nNwType = 5; //UMTS/HSUPA (3G)
				break;
			case 6:
				nNwType = 6; //HSDPA+HSUPA (3G)
				break;
			case 7:
				nNwType = 9; //E-UTRAN
				break;
			case 8:
				nNwType = 10; // EC-GSM-IoT (A/Gb mode)
				break;
			case 9:
				nNwType = 11; // E-UTRAN (NB-S1 mode)
				break;
			default:
				nNwType = 0;
				break;
		}

		int nCnsStat;
		switch (nStat)
		{
			case 0:
				nCnsStat = 0;
				break; // unknown
			case 1:
				nCnsStat = 1;
				break; // available
			case 2:
				nCnsStat = 4;
				break; // current
			case 3:
				nCnsStat = 2;
				break; // fobidden
			default:
				nCnsStat = 0;
				break;
		}

		char achCnSOp[128];
		
		char szEscOp[128];
		strncat_with_escape(szEscOp,szOp,strlen(szOp));

		// -- Workaround
		// Cinterion module returns wrong alpha tag for "AT+COPS=?" command.
		// Only for Cinterion module, used "cinterion-network-names.lst" file instead of the alpha tag from "AT+COPS=?" command
		if (is_cinterion)
		{
			extern char * _getOperatorNameFromList(const char * mccmnc_pair);
			const char * namefromList = NULL;
			namefromList = _getOperatorNameFromList(szMCCMNC);
			if (namefromList) 
				strncat_with_escape(szEscOp,namefromList,strlen(namefromList));
		}

		// convert to cns
		sprintf(achCnSOp, "%s,%s,%s,%d,%d", szEscOp, mcc_str, mnc_str, nCnsStat, nNwType);

		// append into buf
		if (strlen(achBuf))
			strcat(achBuf, "&");
		strcat(achBuf, achCnSOp);
	}

	return strlen(achBuf);
}
///////////////////////////////////////////////////////////////////////////////
int handleUpdateNetworkStat(int sync)
{
	int ok = 0;

	int stat;
	char achATRes[2048];
	char *nameP = NULL;

	// send at command - set numeric operator
	stat = at_send_with_timeout("AT+COPS=3,2", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);
	if (stat || !ok)
		goto error;

	// send at command - get current operator
	stat = at_send_with_timeout("AT+COPS?", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);
	if (stat || !ok)
		goto error;

	// +COPS: 0,2,"50503",2

	char resTokens[4][64] = {{0, }, };
	const char* szToken;

	// convert tokens into the array
	int i = 0;
	szToken = _getFirstToken(achATRes + strlen("+COPS: "), ",\"()");
	while (szToken && i < 4)
	{
		strcpy(resTokens[i++], szToken);
		szToken = _getNextToken();
	}

	// get tokens
	const char* szMCCMNC = resTokens[2];
	int fAutoNw = atoi(resTokens[0]);
	int nNwType = !strlen(resTokens[3]) ? -1 : atoi(resTokens[3]);

	// store MCC and MNC
	if (_isMCCMNC(szMCCMNC))
	{
		int nMCC = _getMCC(szMCCMNC);
		int nMNC = _getMNC(szMCCMNC);

		char achMCC[16];
		sprintf(achMCC, "%d", nMCC);

		char achMNC[16];
		sprintf(achMNC, "%d", nMNC);

		rdb_set_single(rdb_name(RDB_PLMNMCC, ""), achMCC);
		rdb_set_single(rdb_name(RDB_PLMNMNC, ""), achMNC);
	}

	// store manual mode
	if (!fAutoNw) {
		rdb_set_single(rdb_name(RDB_PLMNCURMODE, ""), "Automatic network selection");
		rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""), "Automatic");
	} else {
		rdb_set_single(rdb_name(RDB_PLMNCURMODE, ""), "Manual network selection");
		rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""), "Manual");
	}

#if !defined (MODULE_VZ20Q)
	nameP = check_sierra_cnti_command();
#endif
	if(is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
		if (nameP && strcmp(nameP,"NONE") ) {
			rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), nameP);
		} else {
			if (nNwType < 0 || nNwType > 7)
				rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), "None");
			else
				rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), service_types[nNwType]);
		}
	}

	// send at command - set long operator
	stat = at_send_with_timeout("AT+COPS=3,0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);
	if (stat || !ok) {
		return -1;
	}

	char achOperSel[64];
	int saved_AutoNw=0;
	if(custom_roaming_algorithm){
		achOperSel[0]=0;
		if (rdb_get_single(rdb_name(RDB_PLMNSEL, ""), achOperSel, sizeof(achOperSel)) == 0)
		{
			saved_AutoNw=strtol(achOperSel,0,10);
			if (!saved_AutoNw) {
				rdb_set_single(rdb_name(RDB_PLMNCURMODE, ""), "Automatic network selection");
				rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""), "Automatic");
				nNwType=0;
			} else {
				rdb_set_single(rdb_name(RDB_PLMNCURMODE, ""), "Manual network selection");
				rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""), "Manual");
			}
		}
	}

	if (sync) {
		sync_operation_mode(nNwType);
	}

	return 0;

error:
	at_send_with_timeout("AT+COPS=3,0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int handleGetOpList()
{
	int ok = 0;

	int stat;
	char achATRes[2048];

	handleUpdateNetworkStat(0);

#if defined(MODULE_EC21)
    stat = quectel_list_all_cops(&ok, achATRes, 2048);
#else
	// send at command
	stat = at_send_with_timeout("AT+COPS=?", achATRes, "+COPS", &ok, AT_COP_TIMEOUT_SCAN, 2048);
#endif
	if (stat || !ok)
	{
		rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "error-1");
		return -1;
	}
	// get the list of operators
	char achCnsOpers[2048];
	if (!_convATOpListToCNSOpList(achATRes + strlen("+COPS: "), achCnsOpers, sizeof(achCnsOpers)))
	{
		rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "error-2");
		return -1;
	}

	rdb_set_single(rdb_name(RDB_PLMNCURLIST, ""), achCnsOpers);
	rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "[done]");
	return 0;

	/*error:
		rdb_set_single(rdb_name(RDB_PLMNCURLIST, ""), achATRes);
		return -1;*/
}
///////////////////////////////////////////////////////////////////////////////
int handleSetOpList()
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
	int iAtNw = convert_network_type(iMode);

	// get at command mode
	int iAtMode;
	if (!iMode)
		iAtMode = 0;
	else
		iAtMode = 1;

	// build command
	char achATCmd[128];
	if (iMode) {
		if (iAtNw >= 0)
			sprintf(achATCmd, "AT+COPS=%d,2,\"%03d%02d\",%d", iAtMode, nMNC, nMCC, iAtNw);
		else
			sprintf(achATCmd, "AT+COPS=%d,2,\"%03d%02d\"", iAtMode, nMNC, nMCC); //Automatic Network Type

		automatic_operator_setting = 0;
	}
	else {
		automatic_operator_setting = 1;
		if(custom_roaming_algorithm) {
#ifdef V_MANUAL_ROAMING_vdfglobal
			syslog(LOG_ERR,"[roaming] clear blacklist");
			reset_blacklist();
			suspend_connection_mgr();
			do_custom_manual_roaming(0);
			rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "[DONE]");
			return 0;
#endif
		} else {
			sprintf(achATCmd, "AT+COPS=0");
		}
	}

	// send at command
	char achATRes[AT_RESPONSE_MAX_SIZE];
	int stat;
	int ok = 0;
#if defined (MODULE_VZ20Q)
	char buf[16];
	if (rdb_get_single(rdb_name(RDB_NETWORKREGISTERED, ""), buf, sizeof(buf)) == 0 &&
		atoi(buf) == 1)
#endif	
	stat = at_send_with_timeout(achATCmd, achATRes, "", &ok, AT_COP_TIMEOUT, 0);
	if (stat || !ok)
		goto error;
	// back to numeric
	stat = at_send_with_timeout("AT+COPS=3,0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 0);

	// update network
	handleUpdateNetworkStat(0);
	//sleep(3);
	//handle_network_scan(NULL);

#if defined(MODULE_EC21)
    quectel_band_fix();
#endif
	// set result
	rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "[DONE]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "error");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int default_handle_command_setoplist(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}

	if (!strcmp(args[0].value, "1"))
		return handleGetOpList();
	else if (!strcmp(args[0].value, "5"))
		return handleSetOpList();

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
void __swapNibbles(char* pArray, int cbArray)
{
	int i;
	char c;

	for (i = 0;i < cbArray;i++)
	{
		c = pArray[i];
		pArray[i] = ((c >> 4) & 0x0f) | ((c << 4) & 0xf0);
	}
}
///////////////////////////////////////////////////////////////////////////////
int __convNibbleHexToInt(char chNibble)
{
	if (('0' <= chNibble) && (chNibble <= '9'))
		return chNibble -'0';

	return ((chNibble | ('a' ^ 'A')) - 'a' + 0x0a) & 0x0f;
}
///////////////////////////////////////////////////////////////////////////////
static char __convNibbleIntToChar(int nNibble)
{
	return (0 <= nNibble) && (nNibble <= 9) ? (char)(nNibble + '0') : (char)(nNibble - 0x0a + 'A');
}
///////////////////////////////////////////////////////////////////////////////
int __convHexArrayToStr(char* pHex, int cbHex, char* achBuf, int cbBuf)
{
	int iDst = 0;
	int iSrc = 0;

	memset(achBuf, 0, cbBuf);

	while (iSrc < cbHex)
	{
		if (!(iDst < cbBuf))
			break;

		achBuf[iDst++] = __convNibbleIntToChar((pHex[iSrc] >> 4) & 0x0f);

		if (!(iDst < cbBuf))
			break;

		achBuf[iDst++] = __convNibbleIntToChar(pHex[iSrc] & 0x0f);

		iSrc++;
	}

	return iDst;
}

///////////////////////////////////////////////////////////////////////////////
extern char last_dtmf_keys[AT_RESPONSE_MAX_SIZE];
extern int dtmf_key_idx;
int default_handle_command_dtmf(const struct name_value_t* args)
{
	char achCmd[AT_RESPONSE_MAX_SIZE];
	int i, ok, timeout;

	/* bypass if incorrect argument */
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}

	if (strcmp(args[0].value, "") == 0)
	{
		dtmf_key_idx = 0;
		(void) strcpy(last_dtmf_keys, args[0].value);
		return 0;
	}
	if (strcmp(last_dtmf_keys, args[0].value) == 0 && dtmf_key_idx == strlen(args[0].value))
	{
		//SYSLOG_ERR("same as previous command value. skip : %s", last_dtmf_keys);
		return 0;
	}

	SYSLOG_ERR("Got DTMF command : '%s'", args[0].value);
	(void) strcpy(last_dtmf_keys, args[0].value);
	sprintf(achCmd, "AT");
	timeout = strlen(args[0].value)-dtmf_key_idx;
	for (i = dtmf_key_idx; i < strlen(args[0].value); i++, dtmf_key_idx++)
		sprintf(achCmd, "%s+VTS=%c;", achCmd, args[0].value[i]);
	SYSLOG_ERR("DTMF key AT command : '%s', d_idx = %d", achCmd, dtmf_key_idx);
	if (strcmp(achCmd, "AT") != 0)
	{
		if (at_send_with_timeout(achCmd, 0, "", &ok, timeout, 0) != 0 || !ok)
			return -1;
	}
	return 0;
}

int send_cgactt(int en)
{
	char command[64];
	int succ;
	int ok;
	char response[AT_RESPONSE_MAX_SIZE];
	int i;
	
	#define CGATT_RETRY 3
	
	
	snprintf(command,sizeof(command),"AT+CGATT=%d",en?1:0);
		
		/* send CGATT */
	syslog(LOG_ERR,en?"[network attach request] sending attach req...":"[network attach request] sending detach req...");
	succ=!at_send(command, response, "", &ok, 0) && ok;
		
		/* re-send CGATT if it fails */
		i=0;
		while(!succ && (i++<CGATT_RETRY)) {
			sleep(1);
		syslog(LOG_ERR,en?"[network attach request] re-sending attach req... #%d/%d":"[network attach request] re-sending detach req... #%d/%d",i,CGATT_RETRY);
		succ=!at_send(command, response, "", &ok, 0) && ok;
		}
		
		if(succ) {
			syslog(LOG_ERR,"[network attach request] succ");
		}
		else {
			syslog(LOG_ERR,"[network attach request] fail");
	}
			
	return succ;
}

int default_handle_command_nwctrl(const struct name_value_t* args)
{
	if (!args || !args[0].value)
		return -1;
	
	if (!strcmp(args[0].value, "attach")) {
		
		/* attach */
		if(send_cgactt(1)) {
			goto done_attach;
		}
		
		rdb_set_single(rdb_name(RDB_NWCTRLSTATUS, ""), "[error]");
		return 0;
		
	done_attach:
		rdb_set_single(rdb_name(RDB_NWCTRLSTATUS, ""), "[done]");
		return 0;
	}
	
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
#ifdef HAS_VOICE_DEVICE
int default_handle_command_pots(const struct name_value_t* args)
{
	char achCmd[32];
	int stat, fOK;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	__goToErrorIfFalse(response)

	/* bypass if incorrect argument */
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		__goToError()
	}
	if (strncmp(model_name(), "sierra", sizeof("sierra"))) {
		SYSLOG_ERR("Non-Sierra module does not support this command");
		goto ok_return;
	}
	if (strcmp(args[0].value, "0") && strcmp(args[0].value, "1")) {
		SYSLOG_ERR("invalid command value : %s", args[0].value);
		__goToError()
	}
	SYSLOG_ERR("Got POTS command : '%s' : %s voice feature", args[0].value,
		(strcmp(args[0].value, "1")? "enable":"disable"));
	sprintf(achCmd, "AT!ENTERCND=\"A710\"");
	stat = at_send(achCmd, response, "", &fOK, 0);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("'%s' command failed", achCmd);
		__goToError()
	}
	sprintf(achCmd, "AT!CUSTOM=\"ISVOICEN\",%d", (strcmp(args[0].value, "1")? 1:2));
	stat = at_send(achCmd, response, "", &fOK, 0);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("'%s' command failed", achCmd);
		__goToError()
	}
ok_return:
	__free(response);
	return 0;
error:
	__free(response);
	return -1;
}
#endif
///////////////////////////////////////////////////////////////////////////////
logmask_t log_db[MAX_LOG_TYPE] = {{"at", LOG_ERR},
								  {"sim", LOG_ERR},
								  {"sms", LOG_ERR},
								  {"pdu", LOG_ERR},
#ifdef USSD_SUPPORT
								  {"ussd", LOG_ERR},
								  [5 ... (MAX_LOG_TYPE-1)] = {0, 0}};
#else
								  [4 ... (MAX_LOG_TYPE-1)] = {0, 0}};
#endif

int default_handle_command_logmask(const struct name_value_t* args)
{
	char *pToken, *pToken2, *pSavePtr, *pSavePtr2, *pATRes;
	logmask_t *logp;
	int llevel;
	/* bypass if incorrect argument */
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}
	//SYSLOG_ERR("Got LOGMASK command : '%s'", args[0].value);

	pATRes = (char *)args[0].value;

	if (!strstr(pATRes, "at")) {
		update_debug_level();
	}

	log_db[LOGMASK_SIM].loglevel =
		log_db[LOGMASK_SMS].loglevel =
		log_db[LOGMASK_PDU].loglevel =
#ifdef USSD_SUPPORT
		log_db[LOGMASK_USSD].loglevel =
#endif
	  	log_db[LOGMASK_AT].loglevel;

	pToken = strtok_r(pATRes, ";", &pSavePtr);
	
	while (pToken) {
		pToken2 = strtok_r((char *)pToken, ",", &pSavePtr2);
		//SYSLOG_ERR("pToken2 '%s', pSavePtr2 '%s'", pToken2, pSavePtr2);
		if (!pToken2 || !pSavePtr2) {
			//SYSLOG_ERR("wrong mask value, skip");
			pToken = strtok_r(NULL, ";", &pSavePtr);
			continue;
		}
		logp = &log_db[0];
		while (logp->logname) {
			if (strcmp(logp->logname, pToken) == 0) {
				//SYSLOG_ERR("found logname in log_db '%s'", pToken2);
				llevel = atoi(pSavePtr2);
				if (llevel < LOG_EMERG || llevel > LOG_DEBUG) {
					//SYSLOG_ERR("wrong log level value, skip");
					logp++;
					continue;
				}
				//SYSLOG_ERR("set '%s' log level to %d", pToken2, llevel);
				if (strcmp(logp->logname, "at") == 0) {
					setlogmask(LOG_UPTO(llevel));
					log_db[LOGMASK_AT].loglevel = llevel;
				} else if (strcmp(logp->logname, "sim") == 0) {
					log_db[LOGMASK_SIM].loglevel = llevel;
				} else if (strcmp(logp->logname, "sms") == 0) {
					log_db[LOGMASK_SMS].loglevel = llevel;
				} else if (strcmp(logp->logname, "pdu") == 0) {
					log_db[LOGMASK_PDU].loglevel = llevel;
#ifdef USSD_SUPPORT
				} else if (strcmp(logp->logname, "ussd") == 0) {
					log_db[LOGMASK_USSD].loglevel = llevel;
#endif					
				}
			}
			logp++;
		}
		pToken = strtok_r(NULL, ";", &pSavePtr);
	}
	return 0;
}

int update_loglevel_via_logmask(void)
{
	struct name_value_t args[2];
	char buf[1024];

	// read autopin
	if (rdb_get_single(rdb_name(RDB_SYSLOG_PREFIX, RDB_SYSLOG_MASK), buf, sizeof(buf)) != 0)
		return -1;
	args[0].name = NULL;
	args[0].value = buf;
	args[1].name = NULL;
	args[1].value = NULL;
	return default_handle_command_logmask(args);
}
///////////////////////////////////////////////////////////////////////////////
static int model_default_detect(const char* manufacture, const char* model_name)
{
	return 1;
}
///////////////////////////////////////////////////////////////////////////////
static int notiDummy(const char* s)
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
const struct notification_t model_default_notifications[] =
{
	{ .name = "+CLIP", .action = handle_clip_notification }
	                             , { .name = "+CCWA", .action = handle_ccwa_notification }
	                             , { .name = "RING", .action = handle_ring }
	                             , { .name = "+CRING", .action = handle_ring }
	                             , { .name = "CONNECT", .action = handle_connect }
	                             , { .name = "REJECT", .action = handle_disconnect }
	                             , { .name = "DISCONNECT", .action = handle_disconnect }
	                             , { .name = "NO CARRIER", .action = handle_disconnect }
	                             , { .name = "BUSY", .action = handle_disconnect }
	                             , { .name = "+CMT:", .action = notiSMSRecv }
	                             , { .name = "+CMTI:", .action = handleNewSMSIndicator }

	                             , { .name = "+ZUSIMR:", .action = notiDummy }
	                             , { .name = "^BOOT:", .action = notiDummy }
                                 , { .name = "^DSFLOWRPT:", .action = notiDummy }
                                 , { .name = "+CREG:", .action = handle_creg }

				 // ignore notification from Huawei E353T
				 , { .name = "^RSSI:", .action = notiDummy }
				 , { .name = "^CSNR:", .action = notiDummy }
				 , { .name = "^STIN:", .action = notiDummy }
				 

#ifdef USSD_SUPPORT
								 , { .name = "+CUSD:", .action = handle_ussd_notification }
#endif	/* USSD_SUPPORT */
	                             , {0, } // zero-terminator
				
                             };

///////////////////////////////////////////////////////////////////////////////
const struct command_t model_default_commands[] =
{
	{
		.name = RDB_UMTS_SERVICES".command",
		.action = default_handle_command_umts
	},
	{
		.name = RDB_PHONE_SETUP".command",
		.action = default_handle_command_phonesetup
	},
	{
		.name = RDB_PLMNCOMMAND,
		.action = default_handle_command_setoplist
	},
	{
		.name = RDB_SIMCMMAND,
		.action = default_handle_command_sim
	},
	{
		.name = RDB_SMS_CMD,
		.action = default_handle_command_sms
	},
#ifdef USSD_SUPPORT
	{
		.name = RDB_USSD_CMD,
		.action = default_handle_command_ussd
	},
#endif	/* USSD_SUPPORT */
	{
		.name = RDB_DTMF_CMD,
		.action = default_handle_command_dtmf
	},
#ifdef HAS_VOICE_DEVICE
	{
		.name = RDB_POTS_CMD,
		.action = default_handle_command_pots
	},
#endif	
 	{
		.name = RDB_NWCTRLCOMMAND,
		.action = default_handle_command_nwctrl 
	},
 	{
		.name = RDB_SYSLOG_PREFIX"."RDB_SYSLOG_MASK,
		.action = default_handle_command_logmask 
	},
	{
		0,0
	}
};

///////////////////////////////////////////////////////////////////////////////
struct model_t model_default =
{
	.name = "default",

	.init = model_default_init,
	.detect = model_default_detect,

	.get_status = model_default_get_status,
	.set_status = model_default_set_status,

	.commands = model_default_commands,
	.notifications = model_default_notifications
};

