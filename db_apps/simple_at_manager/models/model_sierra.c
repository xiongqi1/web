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

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/times.h>

#include <sys/types.h>
#include <regex.h>
#include <errno.h>

#include <time.h>

#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "../util/rdb_util.h"

#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../util/scheduled.h"

#include "model_default.h"
#include "../util/at_util.h"
#include "../featurehash.h"
#include "../gps.h"
#include "../sms/sms.h"
#include "../logmask.h"

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

#if defined(PLATFORM_AVIAN)
	static int _fDiversityEnable=1;
#elif defined(PLATFORM_BOVINE) || defined(PLATFORM_ANTELOPE) || defined(PLATFORM_SERPENT)
	static int _fDiversityEnable=-1;
#elif defined(PLATFORM_PLATYPUS)
	#warning 'TODO: Don't use board id's for this - define a V_DIVERSITY variable'
	#if defined(BOARD_3g17wn)
		static int _fDiversityEnable=-1;
	#elif defined(BOARD_3g18wn)
		static int _fDiversityEnable=-1;
	#elif defined(BOARD_3g8wv)
		static int _fDiversityEnable=0;
	#elif defined(BOARD_3g36wv)
		static int _fDiversityEnable=0;
	#elif defined(BOARD_3g38wv)
		static int _fDiversityEnable=0;
	#elif defined(BOARD_3g38wv2)
		static int _fDiversityEnable=0;
	#elif defined(BOARD_3g39w)
		static int _fDiversityEnable=0;
	#elif defined(BOARD_3gt1wn)
		static int _fDiversityEnable=-1;
	#elif defined(BOARD_3gt1wn2)
		static int _fDiversityEnable=-1;
	#elif defined(BOARD_3g23wn)
		static int _fDiversityEnable=1;
	#elif defined(BOARD_3g23wnl)
		static int _fDiversityEnable=1;
	#elif defined(BOARD_3g22wv)
		static int _fDiversityEnable=-1;
	#elif defined(BOARD_3g46)
		static int _fDiversityEnable=0;
	#elif defined(BOARD_testbox)
		static int _fDiversityEnable=1;
	#else
		#error Unknown Platypus board variant, grep "platypus variant selection" to add a new variant
	#endif

#elif defined(PLATFORM_PLATYPUS2)
	static int _fDiversityEnable=-1;
#else
	#error Unknown Platform
#endif

time_t convert_utc_to_local(struct tm* tm_utc);
#ifdef ENABLE_NETWORK_RAT_SEL
static const char* sierra_get_selrat(const char* selrat);
#endif

#ifdef V_MANUAL_ROAMING_vdfglobal
static int currentband_idx = 0xff;
#endif

static char* send_atcmd(const char* atcmd);

#ifdef FORCED_REGISTRATION
// Global variable to handle forced registration
extern int forced_registration;
#endif

extern struct model_t model_default;

/* special flag to use !gband command and hard-coded bands list rather than !band command */
extern int use_gband_command;

static int sierra_airprime=0;
static int sierra_cdma=0;

#ifdef ENABLE_NETWORK_RAT_SEL

/* max module RAT element count */
#define SIERRA_SELRAT_COUNT	255

/* RAT info */
struct selrat_info_t {
	int idx;
	char name[64];
};

int _selrat_cnt=0; /* module RAT selection count */
struct selrat_info_t _selrat_info[SIERRA_SELRAT_COUNT]; /* module RAT selection list */

#endif

#ifdef V_SUB_NETIF_1
#define NUM_OF_SUB_NETIF	1
#endif
int sierra_change_tx_mic_gain(void)
{
	int ok;
	static int set_default_mic_gain = 0;

	if (at_send("at!entercnd=\"A710\"", NULL, "", &ok, 0) != 0 || !ok)
	{
		return -1;
	}

	/* MC8704 default mic gain is 0x2000 until T3.0.2.1 and it is not loud enough
	 * for TNZ New Zealand. Apply new mic gain 0x8000 once when MCC is 530 */
	if (!set_default_mic_gain) {
		if (at_send("at!avtxmicgain=2,8000", NULL, "", &ok, 0) != 0 || !ok)
		{
			SYSLOG_ERR("at!avtxmicgain=2,8000 failed, retry next time");
			return -1;
		}
		SYSLOG_ERR("changed default mic gain to 8000");
		set_default_mic_gain = 1;
	}
	return 0;
}

#if defined(FORCED_REGISTRATION) //Vodafone force registration
static int sierra_handle_command_forcereg(const struct name_value_t* args)
{
	if (!args || !args[0].value) {
			return -1;
	}

	syslog(LOG_ERR,"[roaming] vf.force.registration RDB value is args[0].value=[%s]",args[0].value);
	if (!strcmp(args[0].value, "1")) {
            syslog(LOG_ERR, "[roaming] Setting global variable to trigger forced registration");
            forced_registration = 1;
	}

	return 0;
}
#endif

static int sierra_profile_set(void)
{
	int ok;

	if ( !strcmp(model_variants_name(), "AirCard 320U") )
		return 0;
	/* MC8704 latest firmware T3_0_2_1 returns error for this command and
	 * pots_bridge retrys 5 times at every call release.
	 * Do not use this command for MC8704.
	 */
	//SYSLOG_ERR("model_variants_name %s", model_variants_name());
	if (strcmp(model_variants_name(),"MC8704")) {
		if (at_send("at!avsetprofile=7,0,0,0,2", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
	}
	return 0;
}

static int sierra_profile_init(void)   // quick and dirty
{
	#if defined(MODULETYPE_removable)
		return 0;
	#else
	int ok;

	if ( !strcmp(model_variants_name(), "AirCard 320U") )
		return 0;
	if (at_send("at!entercnd=\"A710\"", NULL, "", &ok, 0) != 0 || !ok)
	{
		return -1;
	}
	/* non-MC8704 models need PCM relating command */
	if (strcmp(model_variants_name(),"MC8704"))
	{
		/* This command make modem PCM clock to prevent from being off after call end.
		 * It is terribly important. Please always imply this command to all Sierra modem.  */
		if (at_send("at!avextpcmstopclkoff=1", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
		/* setup 16 bits linear pcm mode for DTMF cutting and loopback test */
		SYSLOG_ERR("Setting the PCM 16 bit linear");
		if (at_send("at!avextpcmcfg=0,2,0", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
	}
	/* MC8704 needs I2S relating command */
	else
	{
		/* Arbitrary I2C settings, needed for next cmd */
		if (at_send("at!avcusti2ccfg=100,0,0,2,1", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
		/* Switch to 'commercial' mode (other codec) */
		if (at_send("at!avmodeset=1", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
		/* set I2S to slave mode */
		if (at_send("at!avmsswitch=1", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
		/* set to External codec control mode */
		if (at_send("at!avusemcu=1", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
		/* Set sample rate 8KHz */
		if (at_send("at!avsetsamp=8", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
	}
	/* MC8704 latest firmware T3_0_2_1 returns error for this command and
	 * pots_bridge retrys 5 times at every call release.
	 * Do not use this command for MC8704.
	 */
	if (strcmp(model_variants_name(),"MC8704")) {
		if (at_send("at!avsetprofile=7,0,0,0,2", NULL, "", &ok, 0) != 0 || !ok)
		{
			return -1;
		}
	}
	return 0;
	#endif
}

static int sierra_handle_command_phonesetup(const struct name_value_t* args)
{
	if (!args || !args[0].value)
	{
		return -1;
	}
	if (strcmp(args[0].value, "profile set") == 0)
	{
		return sierra_profile_set();
	}
	else if (strcmp(args[0].value, "profile init") == 0)
	{
		return sierra_profile_init();
	}

	SYSLOG_DEBUG("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
	return model_run_command_default(args);
}

///////////////////////////////////////////////////////////////////////////////
#ifdef SKIN_ro
struct
{
	const char* szCnsBandName;
	int iAtBandIdx;
} _bandNameTbl[] =
{
	{"Automatic",				0x00},
	{"HSPA 2100",				0x01},
	{"HSPA 850/1900",			0x02},
	{"GSM 900/1800",			0x03},
	{"GSM 850/1900",			0x04},
	{"2G All",					0x05},
	{"HSPA 2100 GSM 900/1800",	0x06},
	{"HSPA 850/1900 GSM 850/1900",	0x07},
	{"3G All",					0x08},
	{"HSPA 850/2100",			0x09},
	{"HSPA 800/2100",			0x0A},
	{"HSPA 850/2100 GSM 900/1800",	0x0B},
	{"HSPA 850 GSM 900/1800",	0x0c},
	{"HSPA 850Mhz Only",		0x0d},
	{"HSPA 900Mhz Only",		0x0e},
	{"HSPA 900/2100 GSM 900/1800",	0x0f},
	{"HSPA 900/2100",			0x10},
	{"HSPA 1900",				0x11},
	{NULL,                     -1}
};
#else
struct
{
	const char* szCnsBandName;
	int iAtBandIdx;
} _bandNameTbl[] =
{
	{"All bands",				0x00},
//	{"Autoband",				0x00},
	{"WCDMA 2100",				0x01},
	{"WCDMA 850/1900",			0x02},
	{"GSM 900/1800",			0x03},
	{"GSM 850/1900",			0x04},
	{"GSM ALL",				0x05},
//	{"2G",					0x05},
	{"WCDMA 2100 GSM 900/1800",		0x06},
	{"WCDMA 850/1900 GSM 850/1900",		0x07},
	{"WCDMA All",				0x08},
//	{"WCDMA Australia",			0x09},
	{"WCDMA 850/2100",			0x09},
	{"WCDMA 800/2100",			0x0A},
	{"WCDMA 850/2100 GSM 900/1800",		0x0B},
//	{"WCDMA Australia/GSM EU",		0x0B},
	{"WCDMA 850 GSM 900/1800",		0x0C},
//	{"UMTS 850Mhz,2G",			0x0C},
	{"WCDMA 850",				0x0D},
//	{"UMTS 850Mhz Only",			0x0D},
	{"WCDMA 900",				0x0E},
	{"WCDMA 900/2100 GSM 900/1800",		0x0F},
	{"WCDMA 900/2100",			0x10},
	{"WCDMA 1900",				0x11},
	{"WCDMA/GSM EU",           -1},
	{"WCDMA/GSM NA",           -1},
	{"GSM 900/1800",           -1},
	{"GSM 850/PCS",            -1},
	{"WCDMA NA",               -1},
	{"WCDMA Japan",            -1},
	{NULL,                     -1}
};
#endif
/* special band list used prepared for New Zealand with MC8704 */
struct
{
	const char* szCnsBandName;
	int iAtBandIdx;
	const char* szBandMask;
} _bandNameTbl_Telstra[] =
{
	{"All bands",				0x00,       "000200000CE80380"}, /* from bit OR all mask */
	//{"All bands",				0x00,       "000000003FFFFFFF"}, /* from user guide */
	{"WCDMA 850 only",			0x01,       "0000000000080000"},
	{"WCDMA 2100 only",			0x02,       "0000000000400000"},
	{"WCDMA ALL",               0x03,       "000200000CC00000"},
	{"GSM ALL",					0x04,       "0000000000280380"},
	{"WCDMA 850 GSM 900/1800",	0x05,       "0000000004000380"},
	{NULL,                      -1,         NULL}
};

const char Fixed_band_Telstra[] = "00,All bands&01,WCDMA 850 only&02,WCDMA 2100 only&03,WCDMA ALL&04,GSM ALL&05,WCDMA 850 GSM 900/1800";

///////////////////////////////////////////////////////////////////////////////
static const char* sierra_convert_bandidx_to_bandname(int iAtBandIdx)
{
	typeof(_bandNameTbl[0])* pBandNameTbl = _bandNameTbl;
//SYSLOG_DEBUG("iAtBandIdx=%x",iAtBandIdx);
	while (pBandNameTbl->szCnsBandName)
	{
		if (pBandNameTbl->iAtBandIdx == iAtBandIdx)
			return pBandNameTbl->szCnsBandName;

		pBandNameTbl++;
	}
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
/*static int sierra_convert_bandname_to_bandidx(const char* szBandName)
{
	typeof(_bandNameTbl[0])* pBandNameTbl = _bandNameTbl;

	while (pBandNameTbl->szCnsBandName)
	{
		if (!strcmp(pBandNameTbl->szCnsBandName, szBandName))
			return pBandNameTbl->iAtBandIdx;

		pBandNameTbl++;
	}

	return -1;
}*/

///////////////////////////////////////////////////////////////////////////////
static int sierra_get_gband_info(const char* szRes)
{
	typeof(_bandNameTbl_Telstra[0])* pBandNameTbl = _bandNameTbl_Telstra;
	const char* pSrc = szRes;

	while (*pSrc && (*pSrc != ','))
		pSrc++;
	pSrc+=2;
    if (!pSrc)
        return -1;
	while (pBandNameTbl->szBandMask)
	{
		if (strcmp(pBandNameTbl->szBandMask, pSrc) == 0)
			return pBandNameTbl->iAtBandIdx;
		pBandNameTbl++;
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int sierra_get_band_info(char* szRes, int* pBandIdx, char* achBandName, int cbBandName)
{
	char achBandIdx[10];
	int cbBandIdx = sizeof(achBandIdx) / sizeof(achBandIdx[0]);
	const char* pSrc = szRes;
	int i = 0;
	char *pos1;

	char* p;

	while(1) {
		char *pos=strstr(pSrc, "Band Mask");
		if(!pos) break;
		pSrc=pos+strlen("Band Mask");
	}

	/* remove delimiter */
	p=szRes;
	while(*p) {
		if(*p=='&')
			*p='/';
		p++;
	}

	/*** the AirCard 320U has the extra "," in Band Name ---> "01, LTE 1800, WCDMA 850/2100" ***/
	pos1=strstr( pSrc, "01, LTE 1800, WCDMA 850");
	if(pos1)
		*(pos1+12)=' ';
	/*** new AirCard 320U has the extra "," in Band Name ---> "01, LTE 1800, WCDMA 900/2100" ***/
	pos1=strstr( pSrc, "01, LTE 1800, WCDMA 900");
	if(pos1)
		*(pos1+12)=' ';
	/***********************************************************************************/

	while (*pSrc && (*pSrc != ',') && (i < cbBandIdx - 1))
		achBandIdx[i++] = *pSrc++;
	achBandIdx[i] = 0;

	// copy band name
	i = 0;
	while (*(++pSrc) && (i < cbBandName - 1)) {
		if (*pSrc != ',')
			achBandName[i++] = *pSrc;
		else
			break;
	}
	achBandName[i] = 0;

	if (!strlen(achBandIdx) || !strlen(achBandName))
		return -1;

	*pBandIdx = strtol(achBandIdx,0,16);

	return strlen(achBandName);
}

///////////////////////////////////////////////////////////////////////////////
static int handleATGBandGet()
{
	int ok = 0;
	int stat;

	// set at command
	char achATRes[AT_RESPONSE_MAX_SIZE];
	stat = at_send("AT!GBAND?", achATRes, "", &ok, 0);

	// bypass if error
	if (stat || !ok)
		goto error;

	int iBandIdx = sierra_get_gband_info(achATRes + strlen("!GBAND: "));
	if (iBandIdx >= 0) {
    	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), _bandNameTbl_Telstra[iBandIdx].szCnsBandName);
    	return 0;
	}

    /* set to default "All bands" */
	stat = at_send("AT!GBAND=000200000CE80380", achATRes, "", &ok, 0);
	if (stat || !ok)
		goto error;
   	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), _bandNameTbl_Telstra[0].szCnsBandName);

#ifdef V_MANUAL_ROAMING_vdfglobal
	currentband_idx = iBandIdx;
#endif
	return 0;

error:
//	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), "Autoband");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int handleATBandGet()
{
	int ok = 0;
	int stat;
	const char* szCnsBandName;

	// set at command
	char achATRes[AT_RESPONSE_MAX_SIZE];
	stat = at_send("AT!BAND?", achATRes, "", &ok, 0);

	// bypass if error
	if (stat || !ok)
		goto error;

	int iBandIdx;
	char achBandName[64];

#if defined(MODULE_MC7430)
#define BAND_HEADER_PREFIX "Index, Name,"
#define BAND_UNKNOWN "Unknown band mask. Use AT!BAND to set band."
	{
		/*
		 * Reference: 4114486 AirPrime MC73XX-8805 AT Command Reference.pdf
		 * Report the current band selection: AT!BAND?
		 * Response:
		 * 	Index, Name[,   GW Band Mask [, L Band Mask]]
		 * 	<Index>, <Name>[, <GWmask> [, <Lmask>]]
		 * 	OK
		 * or (If the current band mask doesn’t match a band set)
		 * 	Unknown band mask. Use AT!BAND to set band.
		 * 	<Index>
		 * 	OK
		 */
		char *pstring, *pstr, *pstr_e;
		char *token, *token_i;

		pstring = achATRes;
		while ((token = strsep(&pstring, "\n")) != NULL){
			if (!strncmp(token, BAND_HEADER_PREFIX, strlen(BAND_HEADER_PREFIX))) {
				/* skip header line */
				continue;
			}
			else if (!strncmp(token, BAND_UNKNOWN, strlen(BAND_UNKNOWN))) {
				szCnsBandName = BAND_UNKNOWN;
				break;
			}
			else {
				/* parse band name */
				if (pstr = strstr(token, ", ")) {
					pstr += strlen(", ");
					/* strip masks */
					if (pstr_e = strstr(token, ",  ")) {
						*pstr_e = 0;
					}
					szCnsBandName = pstr;
				}
				else {
					szCnsBandName = BAND_UNKNOWN;
				}
				break;
			}
		}
	}
#else
	// get band index and band name
	if (sierra_get_band_info(achATRes + strlen("!BAND: "), &iBandIdx, achBandName, sizeof(achBandName)) < 0)
		goto error;

	// convert to cns bandname
	if (!sierra_airprime)
		szCnsBandName = sierra_convert_bandidx_to_bandname(iBandIdx);
	else
		szCnsBandName = achBandName;

	if (!szCnsBandName)
		goto error;
	// write current band to database
	while(*szCnsBandName<=' ')
		szCnsBandName++;
#endif
	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), szCnsBandName);

#ifdef V_MANUAL_ROAMING_vdfglobal
	currentband_idx = iBandIdx;
#endif
	return 0;

error:
//	rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), "Autoband");

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int handleBandGet()
{
	#ifdef ENABLE_NETWORK_RAT_SEL
	const char* resp;
	int reg_init=0;
	const char* r;
	char* cur_selrat=NULL;

	regex_t reg;
	regmatch_t pm;
	int stat;
	int len;

	{
		/* get selrat result */
		resp=sierra_get_selrat("AT!SELRAT?");
		if(!resp) {
			syslog(LOG_ERR,"failed to get selrat for 'get' rdb command");
			goto err_selrat;
		}

		/*
			at!selrat?
			!SELRAT: 01, UMTS 3G Only
		*/

		/* compile regex */
		stat=regcomp(&reg," [0-9A-Fa-f][0-9A-Fa-f], .*$",REG_EXTENDED|REG_NEWLINE);
		if(stat<0) {
			SYSLOG_ERR("failed in regcomp() - %s",strerror(errno));
			goto err_selrat;
		}
		reg_init=1;

		/* start matching */
		stat=regexec(&reg,resp,1,&pm,0);

		/* bypass if no match */
		if(stat==REG_NOMATCH) {
			syslog(LOG_ERR,"incorrect response from at!selrat (resp=%s)",resp);
			goto err_selrat;
		}

		/* bypass if any error */
		if(stat!=REG_NOERROR) {
			syslog(LOG_ERR,"failed in regexec() - %s",strerror(errno));
			goto err_selrat;
		}

		/* get length */
		r=resp+pm.rm_so;
		len=pm.rm_eo-pm.rm_so;

		len-=STRLEN(" XX, ");
		r+=STRLEN(" XX, ");

		if(len<0) {
			syslog(LOG_ERR,"too short reponse from at!selrat (resp=%s)",resp);
			goto err_selrat;
		}

		/* take current selrat */
		cur_selrat=alloca(len+1);
		memcpy(cur_selrat,r,len);
		cur_selrat[len]=0;

	err_selrat:
		if(reg_init)
			regfree(&reg);

		rdb_set_single(rdb_name(RDB_RATCURSEL, ""), cur_selrat?cur_selrat:"");
	}

	#endif

    return (use_gband_command > 0)? handleATGBandGet():handleATBandGet();
}

///////////////////////////////////////////////////////////////////////////////
static int handleBandSet()
{
	char achBandParam[128];
	int iBandIdx;

	// build at command
	char achATCmd[128];

	int ok;
	int stat;
	char achATRes[AT_RESPONSE_MAX_SIZE];

	#ifdef ENABLE_NETWORK_RAT_SEL
	/* set RAT */
	char selrat[64];

	{
		int i;
		struct selrat_info_t* info;
		int idx;

		/* get RAT name */
		if (rdb_get_single(rdb_name(RDB_BANDPARAM_RAT, ""), selrat, sizeof(selrat)) != 0)
			goto selrat_err;

		/* search RAT index by given RAT name */
		idx=-1;
		for(i=0;i<_selrat_cnt;i++) {
			info=&_selrat_info[i];

			if(!strcmp(info->name,selrat))
				idx=info->idx;
		}

		if(idx<0) {
			syslog(LOG_ERR,"RAT name not found (RAT=%s)",selrat);
			goto selrat_err;
		}


		snprintf(achATCmd,sizeof(achATCmd),"AT!SELRAT=%02x",_selrat_info[idx].idx);
		stat = at_send(achATCmd, achATRes, "", &ok, 0);
		if (stat && !ok) {
			syslog(LOG_ERR,"failed to configure selrat (cmd=%s)",achATCmd);
			goto selrat_err;
		}

	selrat_err:
		(void)0;
	}

	#endif

	// get band param
	if (rdb_get_single(rdb_name(RDB_BANDPARAM, ""), achBandParam, sizeof(achBandParam)) != 0)
		goto error;

	// bypass if no parameter is given
	if(!achBandParam[0])
		goto error;

    /* use !gband command and hard-coded bands list rather than !band command
       for New Zealand same as Telstra requested band list for MC8704. */
    if (use_gband_command > 0) {
        iBandIdx = atoi(achBandParam);
        if (_bandNameTbl_Telstra[iBandIdx].iAtBandIdx < 0) {
			SYSLOG_ERR("band list[%d] is not defined!!!", iBandIdx);
    		goto error;
        }
    	sprintf(achATCmd, "AT!GBAND=%s", _bandNameTbl_Telstra[iBandIdx].szBandMask);
    } else {

// This is work-around on worng behavior of AT!SELRAT command via AT+COPS changes.
// At the monent with regards to Sierra MC7304 module,
// in case the value of RAT(Radio Access Technology) changed with "AT+COPS" command,
// the value of "AT!SELRAT" is also changed to correspending value of "AT+COPS".
// This issue is reported to Sierra and now we are waiting the fix.
// After reveiving proper fix, this work-around should be taken out.
#if defined(V_PRODUCT_vdf_nwl22) || defined(V_PRODUCT_vdf_nwl22w) || defined(MODULE_MC7304)
    	sprintf(achATCmd, "AT!SELRAT=0");
	stat = at_send(achATCmd, achATRes, "", &ok, 0);
	if (stat && !ok)
		syslog(LOG_ERR,"'AT!SELRAT=0' failed, but try 'AT!BAND' command anyway.");
#endif
    	sprintf(achATCmd, "AT!BAND=%s", achBandParam);
    }

	// send at command
	stat = at_send(achATCmd, achATRes, "", &ok, 0);
	if (stat && !ok)
		goto error;

#ifdef V_MANUAL_ROAMING_vdfglobal
	rdb_set_single(rdb_name(RDB_BANDCFG, ""), achBandParam);
#endif
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int sierra_handle_command_band(const struct name_value_t* args)
{
	if (!args || !args[0].value)
		return -1;

	if (!strcmp(args[0].value, "get")) {
		if(handleBandGet()==-1)
			rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");
		else
			rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
		if (sierra_airprime) {
			char achBandParam[128];
			if (rdb_get_single(rdb_name(RDB_BANDCURSEL, ""), achBandParam, sizeof(achBandParam)) == 0) {
				if(strstr(achBandParam,"Use AT!BAND to set band")>0) { //MC7710 will return this string if it has Unknown band mask
					SYSLOG_ERR("MC7710 has Unknown band mask, reset to autoband");
					rdb_set_single(rdb_name(RDB_BANDPARAM, ""), "00"); //set to Auto Band
					return handleBandSet();
				}
			}
		}
		return 0;
	}
	else if (!strcmp(args[0].value, "set"))
		return handleBandSet();

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


#define AT_SCACT_TIMEOUT	30

///////////////////////////////////////////////////////////////////////////////
static int getPDPConnStat(int nPID)
{
	int stat;

	int ok;
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
	char scan[32];
	char* cgact_profile;
	int scan_len;

	sprintf(achCmd,"AT+CGACT?");
	stat=at_send_with_timeout(achCmd,response,"",&ok,AT_SCACT_TIMEOUT, 0);
	if(stat<0 || !ok)
		return -1;

	scan_len=snprintf(scan,sizeof(scan),"+CGACT: %d,",nPID);
	if( (cgact_profile=strstr(response,scan))==0 )
		return -1;

	cgact_profile+=scan_len;

	return atoi(cgact_profile);
}

///////////////////////////////////////////////////////////////////////////////
static int getProfileStat(int nPID)
{
	int fUp;

	int stat;
	int ok;
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];

	sprintf(achCmd,"AT!SCACT?%d",nPID);
	stat=at_send(achCmd,response,"",&ok, 0);
	if(stat<0 || !ok)
		fUp=0;
	else
		fUp=atoi(response+strlen("!SCACT: 1,"));

	return fUp;
}

///////////////////////////////////////////////////////////////////////////////
static int handleProfileWrite()
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
	stat=at_send(achCmd,response,"",&ok, 0);
	if(stat<0 || !ok)
		SYSLOG_ERR("failed to set authentication - AT$QCPDPP failure");

	// build profile setting
	sprintf(achCmd,"AT+CGDCONT=%d,\"IP\",\"%s\"", nPID, _profileInfo.achAPN);
	stat=at_send(achCmd,response,"",&ok, 0);
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
		sprintf(achCmd,"AT+CREG=1");
		stat=at_send(achCmd,response,"",&ok, 0);
		if(stat<0 || !ok) {
			SYSLOG_ERR("failed to enable network registration notification - %s failure",achCmd);
		}

		// set default profile
		syslog(LOG_ERR,"setting the default profile - support LTE");
		sprintf(achCmd,"AT!SCDFTPROF=%d", nPID);
		stat=at_send(achCmd,response,"",&ok, 0);
		if(stat<0 || !ok) {
			SYSLOG_ERR("failed to set default profile - %s failure",achCmd);
		}

		/* re-attaching network for LTE */
		syslog(LOG_ERR,"re-attaching network by sending CGATT=0 and CGATT=1 - support LTE");

		// LTE 1 - detach network
		sprintf(achCmd,"AT+CGATT=0");
		stat=at_send(achCmd,response,"",&ok, 0);
		if(stat<0 || !ok) {
			syslog(LOG_ERR,"AT command failed - %s",achCmd);
			goto error;
		}

		// LTE 2 - wait for 5 seconds regardless of network registration status
		wait_on_network_reg_stat(-1,5,creg_network_stat);

		// LTE 3 - attach network
		creg_network_stat=0;
		sprintf(achCmd,"AT+CGATT=1");
		stat=at_send(achCmd,response,"",&ok, 0);
		if(stat<0 || !ok) {
			syslog(LOG_ERR,"AT command failed - %s",achCmd);
			goto error;
		}

		// LTE 4 - check CGATT result to see if registered
		char *str;
		if( (str=strstr(response,"+CREG:"))!=0) {
			creg_network_stat=atoi(str+sizeof("+CREG:"));
			syslog(LOG_INFO,"CREG found in CGATT result - %d",creg_network_stat);
		}

		// LTE 5 - wait until network registered
		if( wait_on_network_reg_stat(1,10,creg_network_stat)<0 ) {
			syslog(LOG_INFO,"network registration failure after AT+CGATT=1");
		}
		else {
			syslog(LOG_INFO,"network registered");

			// LTE 6 - wait for network deregistred from 3G
			if( wait_on_network_reg_stat(0,10,creg_network_stat)<0 ) {
				syslog(LOG_INFO,"LTE not available now");
			}
			else {
				syslog(LOG_INFO,"LTE activated");
			}
		}
	}


	#if defined(MODULETYPE_removable)
	// put the profile name otherwise, the user is not able to select the profile with 3G watcher
	sprintf(achCmd,"AT!SCPROF=%d,\"%s\",0,0,0,0", nPID, _profileInfo.achAPN);
	stat=at_send(achCmd,response,"",&ok, 0);
/*
	ignore the errors - This command may not be working for old Sierra dongles

	if(stat<0 || !ok)
		goto error;
*/
	#endif

	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[error] at command failed");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int handleProfileActivate(int fActivate, int internal)
{

	int nPID=getProfileID();

	int fUp;

	// send activation or deactivation command
	char achCmd[512];
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;
	int stat;

	sprintf(achCmd,"AT!SCACT=%d,%d", fActivate?1:0, nPID);
	stat=at_send_with_timeout(achCmd,response,"",&ok,TIMEOUT_NETWORK_QUERY, 0);
	if(stat<0 || !ok)
		goto fini;

fini:
	if(!internal) {
		// set current status
		fUp=getProfileStat(nPID);
		rdb_set_single(rdb_name(RDB_PROFILE_UP,""),fUp?"1":"0");

		// set command result
		rdb_set_single(rdb_name(RDB_PROFILE_STAT, ""), "[done]");
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int sierra_handle_command_profile(const struct name_value_t* args)
{
	int stat;

	if (!args || !args[0].value)
		return -1;

	if (!strcmp(args[0].value, "write"))
	{
		return handleProfileWrite();
	}
	else if (!strcmp(args[0].value, "activate"))
	{
		// disconnect if connection is up
		stat=getPDPConnStat(1);
		if(stat) {
			// deactivate
			handleProfileActivate(0,1);
		}

		stat=handleProfileActivate(1,0);

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}
	else if (!strcmp(args[0].value, "deactivate"))
	{
		stat=handleProfileActivate(0,0);

		if (model_physinit() < 0)
			SYSLOG_ERR("failed to initialize model");

		return stat;
	}

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
unsigned int Hex(char  hex) {
	unsigned char result;
	if(hex>='A' && hex<='F')
		result = hex-('A'-10);
	else if(hex>='a' && hex<='f')
		result = hex-('a'-10);
	else
		result = hex - '0';
	return result;
}

unsigned int decodeHex(char *hexin)
{
unsigned int retval=0;
int i;
	for(i=0;i<4;i++)
	{
		if(*(hexin+i))
		{
			if(retval) retval<<=4;
			retval|=Hex(*(hexin+i));
		}
		else
			break;
	}
	//SYSLOG_ERR("decodeHex:%s--%u", hexin, retval);
	return retval;
}

static int update_sierra_ccid(void)
{
	int stat;
	char response[4096];
	int ok;

	char *pos1;
	char value[64];

	// bypass if SIM not inserted
	if (at_send("AT+CPIN?", response, "+CPIN", &ok, 0) == 0 && !ok) {
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), "");
		return 0;
	}

	stat=at_send("AT!ICCID?", response, "", &ok, 4096);
	if ( !stat && ok)
	{
		/* MC7304/7354 pilot version responses without leading '!' */
		pos1=strstr(response, "ICCID");
		if( pos1 && sscanf(pos1+7, "%s", value)) {
			rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), value);
		}
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static void get_band_string(char *value, char *pos1, char *pos3) {
int len;
	while(*pos3==' ')
		pos3++;
	/* some modules report band string including space such as 'WCDMA 850' and
	 * these modules should be parsed in different way.
	 */
	if (!strcmp(model_variants_name(), "AirCard 320U") ||
		!strncmp(model_variants_name(), "MC77", 4) ||
		!strncmp(model_variants_name(), "MC88", 4)) {
		len=pos1-pos3;
		if(len<=0) {
			*value=0;
			return;
		}
		strncpy(value, pos3, len);
		pos3+=len;
		while(*pos3==' ')
			pos3--;
		*(pos3+1)=0;
	}
	else {
		regex_t reg;
		regmatch_t pm;
		int stat;
		int len;

		/* set default result */
		*value=0;

		/* compile regex */
		stat=regcomp(&reg,"^([a-zA-Z]+ [0-9]+|[a-zA-Z0-9]+|[a-zA-Z]+)",REG_EXTENDED);
		if(stat<0) {
			SYSLOG_ERR("failed in regcomp() - %s",strerror(errno));
			return;
		}

		/* match */
		stat=regexec(&reg,pos3,1,&pm,0);
		if(stat==REG_NOMATCH) {
			SYSLOG_ERR("WCDMA band information not found");
		}
		else if(stat==0) {
			len=pm.rm_eo-pm.rm_so;
			memcpy(value,pos3+pm.rm_so,len);
			value[len]=0;
		}
		else {
			SYSLOG_ERR("failed to match WCDMA band  - %s",strerror(errno));
		}

		/* free regex */
		regfree(&reg);
	}
}

static int isImplemented( const char * resp )
{
	if(!resp) return 0;
	return !strstr(resp, "NOT IMPLEMENTED");
}

struct field_to_rdb_t {
	const char* field;
	const char* rdb;
};


typedef int (*sierra_update_rdb_cb_t)(const char* field,const char* rdb,const char* val,void * ref);

/*
	this function converts fields in AT command result to RDB variables

	atresp		: AT command result
	field_to_rdb	: NULL-terminated fields information
	func		: callback function that is called when a field is found
	ref		: reference pointer for the callback function
*/

static int sierra_update_rdb(const char* atresp,struct field_to_rdb_t* field_to_rdb,sierra_update_rdb_cb_t func,void* ref)
{
	struct field_to_rdb_t* e; /* element of gstatus to rdb */
	const char* f; /* field pointer */
	char* f2;
	char c[AT_RESPONSE_MAX_SIZE]; /* content buffer */
	int t; /* target index */
	int sc; /* space count */
	char* p;

	if (!isImplemented(atresp))
		return 0;

	e=field_to_rdb;
	while(e->field) {

		/* clear field buffer */
		*c=0;

		/* search the field name */
		f=strstr(atresp,e->field);
		if(f) {
			/* skip the field name */
			f+=strlen(e->field);
			/* skip spaces */
			while(*f && isspace(*f))
				f++;

			/* copy content */
			t=0;
			while(*f && *f!=':' && *f!='\n' && *f!='\t')
				c[t++]=*f++;
			c[t]=0;

			/* find the end */
			f2=c;
			p=NULL;
			sc=0;
			while(*f2) {

				/* remember the first space */
				if(isspace(*f2)) {
					if(!sc)
						p=f2;
					sc++;
				}
				else {
					sc=0;
					p=NULL;
				}

				/* break if two spaces are found in a row */
				if(sc>=2) {
					break;
				}

				f2++;
			}

			/* cut off the tailer spaces */
			if(p)
				*p=0;
		}

		/* update rdb */
		if(!func || !func(e->field,e->rdb,c,ref))
			rdb_set_single(rdb_name(e->rdb, ""), c);

		e++;
	}

	return 0;
}

int sierra_update_rdb_cb_cellid(const char* field,const char* rdb,const char* val,void * ref)
{
	int* cellid_valid=(int*)ref;

	*cellid_valid=atoi(val);

	/* if cell id is valid, write it to RDB */
	if(*cellid_valid)
		return 0;

	return -1;
}

int sierra_update_rdb_cb_write_number(const char* field,const char* rdb,const char* val,void * ref)
{
	rdb_set_single(rdb_name(rdb, ""), atoi(val)?val:"");

	return -1;
}

int sierra_update_rdb_cb_write_hex(const char* field,const char* rdb,const char* val,void * ref)
{
	int v;

	/* get value */
	errno=0;
	v=strtoll(val, NULL, 16);
	if(errno)
		v=0;

	rdb_set_single(rdb_name(rdb, ""), v?val:"");

	return -1;
}


int sierra_update_rdb_cb_hex(const char* field,const char* rdb,const char* val,void * ref)
{
	char buf[256];

	snprintf(buf,sizeof(buf),"%ld",strtoul(val,NULL,16));
	rdb_set_single(rdb_name(rdb, ""), *val?buf:"");

	/* do not write rdb since we do in this function */
	return -1;
}

static int sierra_update_network_status(void)
{
	char response[4096];
	int ok;
	char *pos, *pos1, *pos2, *pos3;
	int stat, i;
	char value[16];
	FILE *fp;

	int cellid_valid=0;

	/* skip if the variable is updated by other port manager such as cnsmgr */
	if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS)) {
    	// get pdp connections status
    	int pdpStat;
    	pdpStat=getPDPConnStat(1);
	if(is_enabled_feature(FEATUREHASH_CMD_CONNECT)) {
    	if(pdpStat>=0)
    		rdb_set_single(rdb_name(RDB_PDP0STAT, ""), pdpStat==0?"down":"up");
	}
    }

	stat=at_send("AT!GSMINFO?", response, "", &ok, 4096);
	if ( stat || !ok)
		*response=0;

	{
		struct field_to_rdb_t gsminfo_cellid_field_to_rdb[]={
			{"Cell ID:",RDB_NETWORK_STATUS".CellID"},
			{NULL,NULL},
		};

		struct field_to_rdb_t gsminfo_lac_field_to_rdb[]={
   			{"LAC:",RDB_NETWORK_STATUS".LAC"},
			{NULL,NULL},
		};

		struct field_to_rdb_t gsminfo_rac_field_to_rdb[]={
			{"RAC:",RDB_NETWORK_STATUS".RAC"},
			{NULL,NULL},
		};

		/* write cell id *when* cell id is valid from gsminfo */
		sierra_update_rdb(response,gsminfo_cellid_field_to_rdb,sierra_update_rdb_cb_cellid,&cellid_valid);

		/* write LAC *when* cell id is valid from gsminfo */
		if(cellid_valid)
			sierra_update_rdb(response,gsminfo_lac_field_to_rdb,NULL,NULL);

		/* write RAC */
		sierra_update_rdb(response,gsminfo_rac_field_to_rdb,sierra_update_rdb_cb_write_number,NULL);
	}

	stat=at_send("AT+USET?0", response, "", &ok, 4096);
	if ( !stat && ok) {
		if (isImplemented(response)) {
			if ((fp = fopen( "/tmp/uset0", "w")) != 0) {
				get_rid_of_newline_char(response);
				fprintf(fp, "%s", response);
				fflush(fp);
				fclose(fp);
			}
			//rdb_set_single(rdb_name(RDB_NETWORK_STATUS".USET0", ""), response);
		}
	}

	stat=at_send("AT+USET?1", response, "", &ok, 4096);
	if ( !stat && ok) {
		if (isImplemented(response)) {
			if ((fp = fopen( "/tmp/uset1", "w")) != 0) {
				get_rid_of_newline_char(response);
				fprintf(fp, "%s", response);
				fflush(fp);
				fclose(fp);
			}
		}
	}
	stat=at_send("AT+USET?2", response, "", &ok, 4096);
	if ( !stat && ok) {
		if (isImplemented(response)) {
			if ((fp = fopen( "/tmp/uset2", "w")) != 0) {
				get_rid_of_newline_char(response);
				fprintf(fp, "%s", response);
				fflush(fp);
				fclose(fp);
			}
		}
	}

	struct field_to_rdb_t gstatus3_field_to_rdb[]={
		{"PSC:",RDB_PSC},
		{NULL,NULL},
	};

	sierra_update_rdb(response,gstatus3_field_to_rdb,sierra_update_rdb_cb_write_hex,NULL);

	stat=at_send("AT!GSTATUS?", response, "", &ok, 4096);

	if ( stat || !ok)
		*response=0;

	/* convert gstatus to rdb */
	{
		struct field_to_rdb_t gstatus_cellid_field_to_rdb[]={
			{"Cell ID:",RDB_NETWORK_STATUS".CellID"},
   			{"LAC:",RDB_NETWORK_STATUS".LAC"},
			{NULL,NULL},
		};

		struct field_to_rdb_t gstatus_field_to_rdb[]={
			{"RSRP (dBm):",RDB_RSRP0},
   			{"RSRQ (dB):",RDB_RSRQ},
			{NULL,NULL},
		};

		struct field_to_rdb_t gstatus2_field_to_rdb[]={
   			{"TAC:",RDB_TAC},
			{NULL,NULL},
		};

		/* get LAC and cell id from gstatus if we fail to get the information from !gsminfo */
		if(!cellid_valid)
			sierra_update_rdb(response,gstatus_cellid_field_to_rdb,sierra_update_rdb_cb_hex,NULL);

		/* update gstatus rdb */
#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
		if(is_enabled_feature(FEATUREHASH_CMD_SIGSTRENGTH))
#endif
		{
			sierra_update_rdb(response,gstatus_field_to_rdb,sierra_update_rdb_cb_write_number,NULL);
		}
		sierra_update_rdb(response,gstatus2_field_to_rdb,sierra_update_rdb_cb_write_hex,NULL);
	}

	if ( !stat && ok) {
		get_rid_of_newline_char(response);

		/****for MC 8801****/
		pos1=strstr(response, "RX level Carrier 0 (dBm):");
		pos2=strstr(response, "RX level Carrier 1 (dBm):");
		if(pos1 || pos2) {
			if(pos1) {
				sprintf(value, "%i", atoi(pos1+strlen("RX level Carrier 0 (dBm):")));
				rdb_set_single(rdb_name( RDB_RX_LEVEL"0", ""), value);
			}
			if(pos2) {
				sprintf(value, "%i", atoi(pos2+strlen("RX level Carrier 1 (dBm):")));
				rdb_set_single(rdb_name( RDB_RX_LEVEL"1", ""), value);
			}
		}
		else {
			pos1=strstr(response, "RX level C0:");
			pos2=strstr(response, "RX level C1:");
			if(pos1) {
				sprintf(value, "%i", atoi(pos1+strlen("RX level C0:")));
				rdb_set_single(rdb_name( RDB_RX_LEVEL"0", ""), value);
			}
			if(pos2) {
				sprintf(value, "%i", atoi(pos2+strlen("RX level C1:")));
				rdb_set_single(rdb_name( RDB_RX_LEVEL"1", ""), value);
			}
		}

		/**********************/
		pos1=strstr(response, "RRC State:");
		pos2=strstr(response, "RX level");
		if(pos1 && pos2) {
			*pos2=0;
			if(sscanf(pos1+10, "%s", value))
				rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RRC", ""), value);
		}

		if(is_enabled_feature(FEATUREHASH_CMD_BANDSTAT)) {
			pos1=strstr(response, "System mode:");
			pos2=strstr(response, "PS state:");

			if(pos1 && pos2) {
				*(pos2-1)=0;
				if(sscanf(pos2+strlen("PS state:"), "%s", value))
					rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CGATT", ""), value);
				(void) memset(value, 0x00, 16);
				if(strstr(pos1+strlen("System mode:"), "GSM")) { // get current band from GSM band
					pos3=strstr(pos2, "GSM band:");
					if(pos3)
						pos1=strstr(pos3, "GSM channel:");
					if(pos3 && pos1) {
						//*pos1=0;
						pos3+=strlen("GSM band:");
						get_band_string(value, pos1, pos3);
						rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), value);
					/*	if(sscanf(pos3+strlen("GSM band:"), "%s", value))
							rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), value);*/
						pos1+=strlen("GSM channel:");
					}
				}
				else if(strstr(pos1+strlen("System mode:"), "LTE")) {
					pos3=strstr(pos2, "LTE band:");
					pos1=strstr(pos2, "LTE bw:");
					if(pos3 && pos1) {
						//*pos1=0;
						pos3+=strlen("LTE band:");
						while(*pos3==' ')
							pos3++;
						if(*pos3=='B') {
							if(atoi(pos3+1) > 0)
								i=atoi(pos3+1)-1;
							else
								i=0;
							rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), eafrcnTbl[i].bandName);
						}
						/*if(strncmp(pos3,"B7",2)==0) {
							rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "LTE band 7");
						} else if(strncmp(pos3,"B3",2)==0) {
							rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "LTE band 3");
						} else if(strncmp(pos3,"B1",2)==0) {
							rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "LTE band 1");
						}*/
						else {
							get_band_string(value, pos1, pos3);
							rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), value);
						}

						pos = NULL;
						char * chan_titles[] = { "LTE channel:", "LTE Rx chan:", NULL}; // "LTE Rx chan:" is for MC7430
						for (i=0; chan_titles[i] != NULL; i++)
						{
							pos=strstr(pos1+strlen("LTE bw:"), chan_titles[i]);
							if(pos) {
								pos1 = pos + strlen(chan_titles[i]);
								break;
							}
						}
						if(!pos)
							pos1 = NULL;
					}
				}
				else if(strstr(pos1+strlen("System mode:"), "WCDMA")) { // get current band from WCDMA band
					pos3=strstr(pos2, "WCDMA band:");
					if(pos3)
						pos1=strstr(pos3, "WCDMA channel:");
					if(pos3 && pos1) {
						//*pos1=0;
						pos3+=strlen("WCDMA band:");
						get_band_string(value, pos1, pos3);
						/* Sierra Wireless modems report 'WCDMA 800' even when they are in
						* 'WCDMA 850' band. They don't have 'WCDMA 850' reponse for !GSTATUS
						* command. Replace band string here. Australia (Telstra NextG, Vodafone)
						* should be 'WCDMA 850'.
						*/
						if (!strncmp(value, "WCDMA 800", 9)) {
							strcpy(value, "WCDMA 850");
						} else if (!strncmp(value, "WCDMA800", 8)) {
							strcpy(value, "WCDMA850");
						}
						rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), value);
						pos1+=strlen("WCDMA channel:");
					}
				}
				else
					pos1=0;
				if(pos1) {
					/* pos1 = ' 4412GMM (PS) state:REGISTERED....' */
					(void) memset(value, 0x00, sizeof(value));
					while (*pos1 == ' ') pos1++;
					for (i = 0; i < 16 && *pos1 >= '0' && *pos1 <= '9'; i++, pos1++) {
						value[i] = *pos1;
					}
					rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ChannelNumber", ""), value);
				}
			}

		}
	}

#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
	if(is_enabled_feature(FEATUREHASH_CMD_SIGSTRENGTH))
#endif
	{
		/****for MC 8801****/
		stat=at_send("at+rscp?", response, "", &ok, 4096);
		if ( stat || !ok)
			*response=0;

		{
			struct field_to_rdb_t rscp_field_to_rdb[]={
				{"Car0  RSCP:",RDB_RSCP"0"},
				{"Car1  RSCP:",RDB_RSCP"1"},
				{NULL,NULL},
			};

			sierra_update_rdb(response,rscp_field_to_rdb,NULL,NULL);
		}
	}

	stat=at_send("at+ecio?", response, "", &ok, 4096);
	if ( stat || !ok)
		*response=0;

	{
		struct field_to_rdb_t rscp_field_to_rdb[]={
			{"Car0  Tot Ec/Io:",RDB_ECIO"0"},
			{"Car1  Tot Ec/Io:",RDB_ECIO"1"},
			{NULL,NULL},
		};

		sierra_update_rdb(response,rscp_field_to_rdb,NULL,NULL);
	}

	stat=at_send("at!hsdcat?", response, "", &ok, 4096);
	if ( !stat && ok) {
		if (isImplemented(response)) // Skip, when the AT command is not implemented.
		{
			get_rid_of_newline_char(response);
			pos1=strstr(response, "!HSDCAT:");
			if(pos1) {
				rdb_set_single(rdb_name(RDB_HSDCAT, ""), pos1+strlen("!HSDCAT:"));
			}
		}
	}
	stat=at_send("at!hsucat?", response, "", &ok, 4096);
	if ( !stat && ok) {
		if (isImplemented(response)) // Skip, when the AT command is not implemented.
		{
			get_rid_of_newline_char(response);
			pos1=strstr(response, "!HSUCAT:");
			if(pos1) {
				rdb_set_single(rdb_name(RDB_HSUCAT, ""), pos1+strlen("!HSUCAT:"));
			}
		}
	}

	/* For some Sierra Wireless modules, this active Radio Access Technology could
	 * be different from the result of 'at*cnti=0' command. Keep this here.
	 */
	stat=at_send("AT!GETRAT?", response, "", &ok, 4096);
	if ( !stat && ok) {
		pos1 = strstr(response, "!GETRAT:");
		if(pos1 && sscanf(pos1+8, "%s", value))
			rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RAT", ""), value);
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int sierra_update_boot_version_priid(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char value[16];
	int ok;
	char *pos1;
	int stat;

	// send AT command
	stat=at_send("AT!PRIID?", response, "", &ok, 0);
	if (stat && !ok)
		goto err;

	// get PRIID_REV
	pos1=strstr(response, "Revision:");
	if( pos1 && sscanf(pos1+9, "%s", value)) {
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".PRIID_REV", ""), value);
	}
	else {
		SYSLOG_ERR("Revision section not found in AT!PRIID");
        goto fini;
	}

	// get PRIID_PN
	*pos1=0;
	pos1=strstr(response, "PRI Part Number:");
	if( pos1 && sscanf(pos1+16, "%s", value)) {
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".PRIID_PN", ""), value);
	}
	else {
		SYSLOG_ERR("PRI Part Number not found in AT!PRIID");
	}
fini:
	return 0;

err:
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
static int sierra_update_boot_version_bcinf(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;
	char *pos1;
	char *pos2=0;
	int stat;

	// send AT command
	stat=at_send("AT!BCINF", response, "", &ok, 0);
	if (stat && !ok)
		goto err;

	get_rid_of_newline_char(response);

	// find date in OSBL section
	if( !(pos1 = strstr(response, "OSBL")) ) {
		SYSLOG_ERR("OSBL not found in AT!BCINF");
		goto fini;
	}
	if( !(pos2=strstr(pos1, "Ver:")) ) {
		SYSLOG_ERR("Ver: not found in AT!BCINF");
		goto fini;
	}
	if( !(pos1=strstr(pos2, "Date:")) ) {
		SYSLOG_ERR("Date: not found in AT!BCINF");
		goto fini;
	}

	*pos1=0;
	rdb_set_single(rdb_name(RDB_MODULE_BOOT_VERSION, ""), pos2+5);

fini:
	return 0;

err:
	return -1;
}
////////////////////////////////////////////////////////////////////////////////
static int sierra_update_boot_version(void)
{
	run_once_define(bcinf);
	run_once_define(priid);

	run_once_func(bcinf,sierra_update_boot_version_bcinf()>=0);
	run_once_func(priid,sierra_update_boot_version_priid()>=0);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
/* *CNTI command was introduced since Oct 2009 to report
 * current, available, and supported network technologies.
 * Latest Sierra Wireless modems do not report current network technologies
 * in +COPS? command so this command should be used for those modules.
 */
char* update_sierra_network_technology(void)
{
	int stat, ok;
	static char response[128];
	char *pos1 = &response[0];
	char* pos2;
	int token_count;
 	const char* t;
	int offset;
	int size;
	static char buf[128] = {0, };

	// workaround - CNTI command makes Sierra modules go crazy when no SIM card is inserted!
	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0 || !ok) {
		return NULL;
	}

	if(strcmp(response, "+CPIN: READY")) {
		return NULL;
	}
    /* MC8704 does not return no service status with "AT*CNTI=0", so check no service with "AT^SYSINFO" instead of "AT*CNTI=0". */
	if(!strncmp(model_variants_name(), "MC8704", 6)){
		int network_status=0;
		stat=at_send_with_timeout("AT^SYSINFO", response, "^SYSINFO: ", &ok, 1, sizeof(response));
		if (!stat && ok) {
			token_count = tokenize_at_response(response);
 			if (token_count >= 4)
			{
				t = get_token(response, 4);
				network_status = atoi(t);
				if (network_status == 0)
					return "No Network Service";
			}
		}
	}

    /* MC8704 does not give OK after CME_ERROR for CNTI command,
       give 1 second timeout to prevent long delay */
	stat=at_send_with_timeout("AT*CNTI=0", response, "", &ok, 1, sizeof(response));
	if (stat && !ok) {
		SYSLOG_DEBUG("AT*CNTI=0 command fail. This module may not support.");
		return NULL;
	}
	/* *CNTI: +CME ERROR: */
	// add missing validation check
	if(strstr(response,"*CNTI: 0,")) {
		/* response : *CNTI: 0,DC-HSPA+ */
		pos1+=strlen("*CNTI: 0,");
		if ( !pos1 || strlen(pos1) == 0 ) {
			SYSLOG_DEBUG("AT*CNTI=0 command parse error.");
			return NULL;
		}
	}
	else if (strstr(response,"*CNTI: +CME ERROR: ")) {
		pos1+=strlen("*CNTI: +CME ERROR: ");
		if ( !pos1 || strlen(pos1) == 0 ) {
			SYSLOG_DEBUG("AT*CNTI=0 command parse error.");
			return NULL;
		}

		// CNTI=0 command returns 'no network service' when no SIM, SIM PIN or roaming SIM with no network, so
		// network name 'Limited Service' from +COPS? command should be used combined with CNTI command
		stat = at_send_with_timeout("AT+COPS=3,0", response, "", &ok, AT_QUICK_COP_TIMEOUT, 0);
		if (stat || !ok) {
			return NULL;
		}

		if (at_send_with_timeout("AT+COPS?", response, "+COPS", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0 || !ok)
		{
			return NULL;
		}
		token_count = tokenize_at_response(response);
 		if (token_count >= 4)
		{
			t = get_token(response, 3);
			offset = *t == '"' ? 1 : 0;
			size = strlen(t) - offset * 2;
			size = size < 63 ? size : 63;
			memcpy(buf, t + offset, size);
			buf[size] = 0;
			pos1 = &buf[0];
			return pos1;
 		}
	}
	else {
		return NULL;
	}

	// remove carrige return
	pos2=pos1;
	while(*pos2) {
		if(*pos2=='\n')
			*pos2=0;
		pos2++;
	}

	return pos1;
}
////////////////////////////////////////////////////////////////////////////////
#define SEND_ATCMD_N_CHECK_ERROR(cmd, resp, err_ret) \
	do { \
		err = 0; \
		(void)err; \
		if (at_send_with_timeout(cmd, resp, "", &ok, 10, 0) != 0 || !ok) { \
			SYSLOG_ERR("error - %s has failed", cmd); \
			err = 1; \
			if (err_ret) return 0; \
		} \
	} while (0)
#define DB_POTSBRIDGE_DISABLED		"potsbridge_disabled"
static int sierra_synchronize_voice_call_setting(void)
{
	static int syncronized = 0;
	BOOL rdb_pots_disabled, module_voice_disabled = FALSE;
	char response[AT_RESPONSE_MAX_SIZE];
	char cmd[128];
	int ok, err;
	char *pToken;

	if (syncronized) {
		return 0;
	}

	rdb_pots_disabled = is_potsbridge_disabled();

	strcpy(cmd, "AT!CUSTOM?");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	pToken = strstr(response, "ISVOICEN");
	if (pToken == 0) {
		SYSLOG_ERR("error - can not find ISVOICEN option");
		return 0;
	}
	pToken += strlen("ISVOICEN");
	while (*pToken == '\t' || *pToken == ' ') {
		pToken++;
	}
	if (strncmp(pToken, "0x02", 4) == 0) {
		module_voice_disabled = TRUE;
	}
	if (rdb_pots_disabled == module_voice_disabled) {
		SYSLOG_ERR("voice call setting is already synchronized");
		syncronized = 1;
		return 0;
	}
	SYSLOG_ERR("voice call setting does not match : rdb %d, module %d", rdb_pots_disabled, module_voice_disabled);
	sprintf(cmd, "AT!CUSTOM=\"ISVOICEN\",%d", (rdb_pots_disabled? 2:1));
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	SYSLOG_ERR("voice call setting is synchronized, now reset the module to take effect");
	return 1;
}

#if defined(PLATFORM_PLATYPUS2)
static BOOL is_echocancellation_set(void)
{
	BOOL echo_cancel_set = FALSE;
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
	nvram_init(RT2860_NVRAM);
	rd_data = nvram_bufget(RT2860_NVRAM, RDB_ECHO_CANCELLATION);
	if (!rd_data) {
		goto ret;
	}
#else
	char rd_data[BSIZE_16] = {0, };
	if (rdb_get_single(rdb_name(RDB_ECHO_CANCELLATION, ""),rd_data, BSIZE_16) != 0) {
		goto ret;
	}
#endif
	if (strlen(rd_data) == 0) {
		goto ret;
	}
	if (strcmp(rd_data, "1") == 0) {
		echo_cancel_set = TRUE;
		goto ret;
	}
ret:
#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(rd_data);
	nvram_close(RT2860_NVRAM);
#endif
	return echo_cancel_set;

}

static int sierra_set_echo_cancellation(void)
{
	BOOL echo_cancel_set=FALSE;
	char response[AT_RESPONSE_MAX_SIZE];
	char cmd[128];
	int ok, err;
	char *pToken;

	echo_cancel_set = is_echocancellation_set();
	if(echo_cancel_set == TRUE){
		SYSLOG_INFO("Voice ECHO cancellation has been set!");
		return 0;
	}
	strcpy(cmd, "AT!CUSTOM?");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	pToken = strstr(response, "ISVOICEN");
	if (pToken == 0) {
		SYSLOG_ERR("error - can not find ISVOICEN option");
		return 0;
	}
	pToken += strlen("ISVOICEN");
	while (*pToken == '\t' || *pToken == ' ') {
		pToken++;
	}
	if (strncmp(pToken, "0x01", 4) != 0) {
		SYSLOG_ERR("Voice call is not enabled");
		return 0;
	}

	strcpy(cmd, "AT!AVEC=0,0");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, FALSE);
	if (err) {
		//The 3G module is likely to use old version firmware, try another command
		strcpy(cmd, "AT!AVEC=0");
		SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);
	}
	// Set cancel flag to 1 so in next reboot/powercycle, don't do this again
	rdb_update_single(rdb_name(RDB_ECHO_CANCELLATION, ""), "1", PERSIST, DEFAULT_PERM, 0, 0) ;
	SYSLOG_ERR("voice call ECHO cancellation has been set, now reset the module to take effect");
	return 1;
}
#endif
////////////////////////////////////////////////////////////////////////////////
/* New Sierra module such as MC7710, MC7700, MC8801 output more detailed
 * band list information after entering supervisor mode with !enterncd="A710"
 * command so these extra information should be removed. */
void get_rid_of_band_mask_bits(char * input_string)
{
	char value[4098] = {0, };
	char *pos1, *pos2;
	if(!strstr(input_string, "GW Band Mask") || !input_string) {
		return;
	}
	pos1 = input_string;
	pos2 = value;
	while( pos1 )
	{
		if (!strchr(pos1, ',')) break;
		while(*pos1 != ',') *pos2++ = *pos1++;
		*pos2++ = *pos1++;
		if (!strchr(pos1, ',')) break;
		while(*pos1 != ',') *pos2++ = *pos1++;
		if ( !pos1 ) break;
		*pos2++ = '\n';
		if (!strchr(pos1, '\n')) break;
		while(*pos1 != '\n') pos1++;
		if ( !pos1 ) break;
		pos1++;
	}
	strcpy(input_string, value);
}
//
static int initialize_bandlist(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char cmd[128];
	int ok, err;
	char *pos=NULL;
	int i, fully_configured = 1;
	typedef struct {
		const char* band_name;
		const char* gsm_mask;
		const char* lte_mask;
	} band_list_elem_type;
	const band_list_elem_type *bands_ptr, *bands_ptr2;
	static int first = 1;

	//This is to check mask values as well as band name.
	regex_t reg;
	int stat;
	char regexp[1024]={0,};

	const band_list_elem_type mc7700_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "All bands",				"000000000CE80380",		"0000000000010008" },
		{ "LTE/3G All",				"000000000CC00000",		"0000000000010008" },
		{ "3G/2G All",				"000000000CE80380",		"0000000000000000" },
		{ "LTE all",				"0000000000000000",		"0000000000010008" },
		{ "LTE 1700/2100 (AWS)",	"0000000000000000",		"0000000000000008" },
		{ "LTE 700",				"0000000000000000",		"0000000000010000" },
		{ "WCDMA all",				"000000000CC00000",		"0000000000000000" },
		{ "WCDMA 2100",				"0000000000400000",		"0000000000000000" },
		{ "WCDMA 1900",				"0000000000800000",		"0000000000000000" },
		{ "WCDMA 850",				"0000000004000000",		"0000000000000000" },
		{ "WCDMA 800",				"0000000008000000",		"0000000000000000" },
		{ "2G all",					"0000000000280380",		"0000000000000000" },
		{ "GSM 850",				"0000000000080000",		"0000000000000000" },
		{ "GSM 900",				"0000000000000300",		"0000000000000000" },
		//{ "DCS 1800",				"0000000000000080",		"0000000000000000" },
		{ "PCS 1900",				"0000000000200000",		"0000000000000000" },
		{ NULL,						NULL,					NULL }
	};

	const band_list_elem_type mc7710_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "All bands",				"0002000000600380",		"0000000000080085" },
		{ "3G/LTE All",				"0002000000400000",		"0000000000080045" },
		{ "2G All",					"0000000000200380",		"0000000000000000" },
		{ "LTE ALL",				"0000000000000000",		"0000000000080045" },
		{ "LTE 2100 Only",			"0000000000000000",		"0000000000000001" },
		{ "LTE 2600 Only",			"0000000000000000",		"0000000000000040" },
		{ "LTE 1800 Only",			"0000000000000000",		"0000000000000004" },
		{ "LTE 800 Only",			"0000000000000000",		"0000000000080000" },
		{ "LTE2600/WCDMA2100",		"0000000000400000",		"0000000000000040" },
		{ "LTE1800/WCDMA2100",		"0000000000400000",		"0000000000000004" },
		{ "WCDMA all",				"0002000000400000",		"0000000000000000" },
		{ "WCDMA 2100",				"0000000000400000",		"0000000000000000" },
		{ "WCDMA 900",				"0002000000000000",		"0000000000000000" },
		{ "GSM 900",				"0000000000000300",		"0000000000000000" },
		//{ "DCS 1800",				"0000000000000080",		"0000000000000000" },
		{ "PCS 1900",				"0000000000200000",		"0000000000000000" },
		{ NULL,						NULL,					NULL }
	};

	const band_list_elem_type mc8801_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "All bands",				"000200000CE80380",		NULL },
		{ "WCDMA 850 only",			"0000000004000000",		NULL },
		{ "WCDMA 2100 only",		"0000000000400000",		NULL },
		{ "WCDMA ALL",				"000200000CC00000",		NULL },
		{ "GSM All",				"0000000000280380",		NULL },
		{ "WCDMA 850/GSM 900/1800",	"0000000004000380",		NULL },
		{ NULL,						NULL,					NULL }
	};

	/*
		<Index> (Index of a band set. Use the Query List command to display all supported sets)
		• Valid range: 0–13 (Hexadecimal. There are 20 possible values.)
		<Name> (Name of the band set)
		• ASCII string—Up to 30 characters
		<GWmask> (GSM/WCDMA bands included in the set)
		• Format: 32-bit bitmask
		• Valid values:
		• 0000000000000003—C850
		• 0000000000000004—C1900
		• 0000000000000080—G1800
		• 0000000000000300—G900 (EGSM/GSM)
		• 0000000000080000—G850
		• 0000000000200000—G1900

		• 0000000000400000—W2100
		• 0000000000800000—W1900
		• 0000000002000000—W1700
		• 0000000004000000—W850
		• 0000000008000000—W800
		• 0002000000000000—W900

		<Lmask> (LTE bands included in the set)
		• Format: 32-bit bitmask
		• Valid values:
		• 0000000000000001—Band 1
		0000000000000002—Band 2
		...
		0000004000000000—Band 39
		0000008000000000—Band 40
	*/
#if defined(V_FIXED_BAND_lte3)
	// fixed LTE Band 3 only
	const band_list_elem_type mc7304_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "LTE 1800 only",			"0000000000000000",		"0000000000000004" },
		{ NULL,						NULL,					NULL }
	};

#elif defined(V_FIXED_BAND_lte7)
	// fixed LTE Band 7 only
	const band_list_elem_type mc7304_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "LTE 2600 only",			"0000000000000000",		"0000000000000040" },
		{ NULL,						NULL,					NULL }
	};

#elif defined(V_FIXED_BAND_lte1)
	// fixed LTE Band 1 only
	const band_list_elem_type mc7304_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "LTE 2100 only",			"0000000000000000",		"0000000000000001" },
		{ NULL,						NULL,					NULL }
	};

#elif !defined(V_PRODUCT_vdf_nwl22) && !defined(V_PRODUCT_vdf_nwl22w)
	const band_list_elem_type mc7304_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "All bands",				"0002000004E80180",		"00000000000800C5" },
						/*       0002000004E80180 */
		{ "LTE/3G all",				"0002000004C00000",		"00000000000800C5" },
		{ "3G/2G all",				"0002000004E80100",		"0000000000000000" },
		{ "LTE all",				"0000000000000000",		"00000000000800C5" },
		{ "LTE 2600 only",			"0000000000000000",		"0000000000000040" },
		{ "LTE 2100 only",			"0000000000000000",		"0000000000000001" },
		{ "LTE 1800 only",			"0000000000000000",		"0000000000000004" },
		{ "LTE 800 only",			"0000000000000000",		"0000000000080000" },
		{ "LTE 900 only",			"0000000000000000",		"0000000000000080" },
		{ "WCDMA all",				"0002000004C00000",		"0000000000000000" },
		{ "WCDMA 2100",				"0000000000400000",		"0000000000000000" },
		{ "WCDMA 1900",				"0000000000800000",		"0000000000000000" },
		{ "WCDMA 900",				"0002000000000000",		"0000000000000000" },
		{ "WCDMA 850",				"0000000004000000",		"0000000000000000" },
		{ "2G all",					"0000000000280180",		"0000000000000000" },
		{ "GSM 850",				"0000000000080000",		"0000000000000000" },
		{ "GSM 900E",				"0000000000000100",		"0000000000000000" },
		{ "DCS 1800",				"0000000000000080",		"0000000000000000" }, //Put this back with Joe's confirmation.
		{ "PCS 1900",				"0000000000200000",		"0000000000000000" },
		{ NULL,						NULL,					NULL }
	};

#else
	const band_list_elem_type mc7304_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "All bands",				"0002000004E80180",		"00000000000800C5" },
						/*       0002000004E80180 */
		//{ "LTE/3G all",				"0002000004C00000",		"00000000000800C5" }, //Tempoparily taken out for MR3
		//{ "3G/2G all",				"0002000004E80100",		"0000000000000000" }, //Tempoparily taken out for MR3
		{ "LTE all",				"0000000000000000",		"00000000000800C5" },
		{ "LTE 2600 only",			"0000000000000000",		"0000000000000040" },
		{ "LTE 2100 only",			"0000000000000000",		"0000000000000001" },
		{ "LTE 1800 only",			"0000000000000000",		"0000000000000004" },
		{ "LTE 800 only",			"0000000000000000",		"0000000000080000" },
		{ "LTE 900 only",			"0000000000000000",		"0000000000000080" },
		{ "WCDMA all",				"0002000004C00000",		"0000000000000000" },
		{ "WCDMA 2100",				"0000000000400000",		"0000000000000000" },
		{ "WCDMA 1900",				"0000000000800000",		"0000000000000000" },
		{ "WCDMA 900",				"0002000000000000",		"0000000000000000" },
		{ "WCDMA 850",				"0000000004000000",		"0000000000000000" },
		{ "GSM all",					"0000000000280180",		"0000000000000000" },
		{ "GSM 850",				"0000000000080000",		"0000000000000000" },
		{ "GSM 900E",				"0000000000000100",		"0000000000000000" },
		{ "GSM DCS 1800",				"0000000000000080",		"0000000000000000" }, //Put this back with Joe's confirmation.
		{ "GSM PCS 1900",				"0000000000200000",		"0000000000000000" },
		{ NULL,						NULL,					NULL }
	};
#endif

#if 0
	const band_list_elem_type mc7354_bands[] = {
		/* band name				GSM band mask			LTE band mask */
		{ "All bands",				"000200000CE80380",		"000000000001001A" },
		{ "Europe 3G",				"0002000000400000",		"0000000000000000" },
		{ "North America 3G",		"000000000C800000",		"0000000000000000" },
		{ "Europe 2G",				"0000000000000380",		"0000000000000000" },
		{ "North America 2G",		"0000000000280000",		"0000000000000000" },
		{ "GSM ALL",				"0000000000280380",		"0000000000000000" },
		{ "Europe",					"0002000000400380",		"0000000000000000" },
		{ "North America",			"000000000CA80000",		"000000000001001A" },
		{ "WCDMA ALL",				"0002000004C00000",		"0000000000000000" },
		{ "LTE ALL",				"0000000000000000",		"000000000001001A" },
		{ NULL,						NULL,					NULL }
	};
#endif
	/* This function is called by model_physinit() --> sierra_init() and model_physinit() can be called multiple times
	 * so prevent multiple calling here */
	if (!first) {
		return 0;
	}
	first = 0;

	/*
		!!! WARNING !!!

		Do not change factory band settings.
		The factory band settings are setup based on PRIs and PRIs could be different even on the same module.

		The following code is overwriting factory band settings and this is only to correct factory mistakes of released units.

		!!! Be careful not to overrite valid factory band settings. !!!
	*/

	if(!strncmp(model_variants_name(), "MC7700", 6)){
		bands_ptr=bands_ptr2=&mc7700_bands[0];
	}else if(!strncmp(model_variants_name(), "MC7710", 6)){
		bands_ptr=bands_ptr2=&mc7710_bands[0];
	}else if(!strncmp(model_variants_name(), "MC8801", 6)){
		bands_ptr=bands_ptr2=&mc8801_bands[0];
	}else if(!strncmp(model_variants_name(), "MC7304", 6)){

		/*
			at!gobiimpref?
			!GOBIIMPREF:
			preferred fw version:   05.05.26.02
			preferred carrier name: GENEU-4G
			preferred config name:  GENEU-4G_005.006_001
			current fw version:     05.05.26.02
			current carrier name:   GENEU-4G
			current config name:    GENEU-4G_005.006_001

			OK
		*/

		char resp[AT_RESPONSE_MAX_SIZE];

		/* bypass if we fail to get PRI setting */
		if (at_send("AT!GOBIIMPREF?", resp, "", &ok, 0) != 0 || !ok) {
			SYSLOG_ERR("[PRI] failed to get result from AT!GOBIIMPREF?");
			return 0;
		}

#if !defined(V_PRODUCT_vdf_nwl22) && !defined(V_PRODUCT_vdf_nwl22w)  // This is taken out for vdf_nwl22 due to Talal's request.
		/* bypass if the firmware is not generic */
		if(!strstr(resp,"preferred carrier name: GENEU")) {
			SYSLOG_ERR("[PRI] no generic PRI found - preserve factory band settings");
			return 0;
		}

		SYSLOG_ERR("[PRI] generic PRI found - apply generic band settings");
#endif
		bands_ptr=bands_ptr2=&mc7304_bands[0];

	}else if(!strncmp(model_variants_name(), "MC8704", 6)){
		rdb_set_single(rdb_name("conn_type", ""), "3g");
#if defined(PLATFORM_PLATYPUS) && defined(BOARD_3g38wv2) && defined(SKIN_au)
		strcpy(cmd, "AT!CUSTOM?");
		SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

		if(!strstr(response, "PRLREGION"))
			return 0;

		strcpy(cmd, "AT!CUSTOM=\"PRLREGION\",0"); // set PRLREGION to Default(internal)
		SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);
		SYSLOG_ERR("PRLREGION is set to 0");
		return 1;
#elif defined(PLATFORM_PLATYPUS2) && defined(BOARD_3g22wv) && defined(SKIN_ntc)
		strcpy(cmd, "AT!CUSTOM?");
		SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

		if(strstr(response, "PRLREGION\t\t0x03"))
			return 0;

		strcpy(cmd, "AT!CUSTOM=\"PRLREGION\",3"); // set PRLREGION to Australia
		SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);
		SYSLOG_ERR("PRLREGION is set to 3");
		return 1;
#endif
		return 0;
	}else{
		return 0;
	}
#if !defined(V_PRODUCT_vdf_nwl22w)
	if (rdb_get_single(rdb_name(RDB_BAND_INITIALIZED, ""), response, sizeof(response)) == 0 &&
		strcmp(response, "1") == 0) {
		SYSLOG_ERR("Band list already configured");
		return 0;
	}
#endif
	strcpy(cmd, "AT!BAND=?");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	// This is to check index, mask value as well as band name.
	// This will fix the problem that band list on module is not update when band list from module includes all of band name in the fixed list but it has extra bands.
	// With previous routine only modlue band list is updated when the band name is not matched on both lists or band name does not exist in module band list.
	for (i = 0; bands_ptr->band_name; bands_ptr++, i++) {
		/* compile regex */
		sprintf(regexp,"%02X,[[:blank:]]+%s,[[:blank:]]+%s[[:blank:]]+%s", i, bands_ptr->band_name, bands_ptr->gsm_mask, bands_ptr->lte_mask);
		stat=regcomp(&reg,regexp,REG_EXTENDED|REG_NEWLINE);
		if(stat>=0) {
			stat=regexec(&reg,response,0,NULL,0);
			if (stat == REG_NOMATCH) {
				fully_configured = 0;
				regfree(&reg);
				break;
			}
			regfree(&reg);
		}
		else {
			SYSLOG_ERR("Compile Regular expression Failure: regexp=[%s]", regexp);
		}
	}

	/* In the case that Band list has not been configured, we trigger to configure it. */
	if(fully_configured) {
		SYSLOG_ERR("Band list was fully configured already, set band_initialized flag to 1");
		rdb_set_single(rdb_name(RDB_BAND_INITIALIZED, ""), "1");
		return 0;
	}

	SYSLOG_ERR("Band list was not set, set default band list");
	if(!strncmp(model_variants_name(), "MC8801", 6)){
		/* set to 3G preferred for Telstra */
		strcpy(cmd, "AT!SELRAT=03");
	} else {
		strcpy(cmd, "AT!SELRAT=00");
	}
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	/* set default band list */
	for (i = 0; bands_ptr2->band_name; i++, bands_ptr2++) {
		if(!strncmp(model_variants_name(), "MC8801", 6)){
			sprintf(cmd, "AT!BAND=%02x,\"%s\",%s", i, bands_ptr2->band_name, bands_ptr2->gsm_mask);
		} else {
			sprintf(cmd, "AT!BAND=%02x,\"%s\",%s,%s", i, bands_ptr2->band_name, bands_ptr2->gsm_mask, bands_ptr2->lte_mask);
		}
		SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);
	}
	for (; i < 20; i++) {
		if(!strncmp(model_variants_name(), "MC8801", 6)){
			sprintf(cmd, "AT!BAND=%02x,\"\",0000000000000000", i);
		} else {
			sprintf(cmd, "AT!BAND=%02x,\"\",0000000000000000,0000000000000000", i);
		}
		SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);
	}

	/* set default band setting */
	strcpy(cmd, "AT!BAND=00");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	rdb_set_single(rdb_name(RDB_BAND_INITIALIZED, ""), "0");
	SYSLOG_ERR("Band list was changed, reboot module to take effect new configuration");
	return 1;
}

static int initialize_custom_settings(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char cmd[128];
	int ok, err;
	char *pToken;
	int fully_configured;

	fully_configured=0;

	strcpy(cmd, "AT!CUSTOM=?");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);
	pToken = strstr(response, "DHCPRELAYENABLE");;

	/* bypass if DHCPRELAYENABLE not supported */
	if(!pToken) {
		return 0;
	}

	strcpy(cmd, "AT!CUSTOM?");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);
	pToken = strstr(response, "DHCPRELAYENABLE");
	if (pToken == 0) {
		SYSLOG_ERR("DHCPRELAY is not enabled yet, try to enable now");
		strcpy(cmd, "AT!CUSTOM=\"DHCPRELAYENABLE\",0X01");
		SEND_ATCMD_N_CHECK_ERROR(cmd, NULL, TRUE);

		fully_configured=1;
	} else {
		pToken += strlen("DHCPRELAYENABLE");
		while (*pToken == '\t' || *pToken == ' ') {
			pToken++;
		}
		if (strncmp(pToken, "0x01", 4) != 0) {
			SYSLOG_ERR("DHCPRELAY is not enabled yet, try to enable now");
			strcpy(cmd, "AT!CUSTOM=\"GPSENABLE\",0X01");
			SEND_ATCMD_N_CHECK_ERROR(cmd, NULL, TRUE);

			fully_configured=1;
		}
	}

	return fully_configured;

}
//////////////////////////////////////////////////////////////////////////////
static int initialize_mtu_settings(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	int ok, err;
	char *pToken;
	int fully_configured;

	fully_configured=0;

	/*
	AT!SCUMMTU?
	!SCUMMTU:
	3GPP MTU : 1358
	HRPD MTU : undefined
	EHRPD MTU: undefined
	USB MTU  : 1358


	OK
	*/

	SEND_ATCMD_N_CHECK_ERROR("AT!SCUMMTU?", response, TRUE);
	pToken = strstr(response, "!SCUMMTU:");;

	/* Not proper response */
	if(!pToken) {
		return 0;
	}

	pToken = strstr(response, "3GPP MTU : 1500");
	if (pToken == 0) {
		SYSLOG_ERR("MTU size should be 1500");
		SEND_ATCMD_N_CHECK_ERROR("AT!SCUMMTU=1500", NULL, TRUE);

		fully_configured=1;
	}

	return fully_configured;

}
//////////////////////////////////////////////////////////////////////////////
static int initialize_gps_settings(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char cmd[128];
	int ok, err, i, is_mc77xx = 0, is_mc73xx = 0, fully_configured = 1;
	char *pToken;
	static int first = 1;
	typedef struct {
		const char* query_cmd;
		const char* exp_resp;
		const char* set_cmd;
	} gps_cmd_type;
	const gps_cmd_type gps_set_cmd[] = {
		{ "AT!GPSPOSMODE?",			"MASK: 0x0000007F",						"AT!GPSPOSMODE=7F" },
#ifdef V_CUSTOM_FEATURE_PACK_Vodafone_Global
		{ "AT!GPSSUPLURL?", 		"supl.vodafone.com:7275", 				"AT!GPSSUPLURL=\"supl.vodafone.com:7275\"" },
#else
		{ "AT!GPSSUPLURL?", 		"supl.google.com:7276", 				"AT!GPSSUPLURL=\"supl.google.com:7276\"" },
#endif
		{ "AT!GPSXTRADATAURL?",		"http://xtra1.gpsonextra.net/xtra.bin", "AT!GPSXTRADATAURL=1,\"http://xtra1.gpsonextra.net/xtra.bin\"" },
		{ "AT!GPSXTRADATAURL?",		"http://xtra2.gpsonextra.net/xtra.bin", "AT!GPSXTRADATAURL=2,\"http://xtra2.gpsonextra.net/xtra.bin\"" },
		{ "AT!GPSXTRADATAURL?",		"http://xtra3.gpsonextra.net/xtra.bin", "AT!GPSXTRADATAURL=3,\"http://xtra3.gpsonextra.net/xtra.bin\"" },
		{ "AT!GPSXTRATIMEENABLE?", 	"Time Info Enabled: 1",					"AT!GPSXTRATIMEENABLE=1,30000,10000" },
		{ "AT!GPSXTRATIMEENABLE?", 	"Time Uncertainty Threshold: 30000",	"AT!GPSXTRATIMEENABLE=1,30000,10000" },
		{ "AT!GPSXTRATIMEENABLE?", 	"Time Delay Threshold:10000",			"AT!GPSXTRATIMEENABLE=1,30000,10000" },
		{ "AT!GPSXTRATIMEURL?",		"xtra1.gpsonextra.net",					"AT!GPSXTRATIMEURL=1,\"xtra1.gpsonextra.net\"" },
		{ "AT!GPSXTRATIMEURL?",		"xtra2.gpsonextra.net",					"AT!GPSXTRATIMEURL=2,\"xtra2.gpsonextra.net\"" },
		{ "AT!GPSXTRATIMEURL?",		"xtra3.gpsonextra.net",					"AT!GPSXTRATIMEURL=3,\"xtra3.gpsonextra.net\"" },
		#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
  		/* This GPS command makes MC7304 and MC7354 crash after switching PRI */
		#else
		{ "AT!GPSTRANSSEC?", 		"Transport security: 0", 				"AT!GPSTRANSSEC=0" },
		#endif
		{ "AT!GPSXTRADATAENABLE?",	"Data Enabled: 1",						"AT!GPSXTRADATAENABLE=1,10,2,1,1" },
		{ "AT!GPSXTRADATAENABLE?",	"Data Retry Number: 10",				"AT!GPSXTRADATAENABLE=1,10,2,1,1" },
		{ "AT!GPSXTRADATAENABLE?",	"Data Retry Interval: 2",				"AT!GPSXTRADATAENABLE=1,10,2,1,1" },
		{ "AT!GPSXTRADATAENABLE?",	"Data Autodownload Enabled: 1",			"AT!GPSXTRADATAENABLE=1,10,2,1,1" },
		{ "AT!GPSXTRADATAENABLE?",	"Data Autodownload Interval: 1",		"AT!GPSXTRADATAENABLE=1,10,2,1,1" },
		{ "AT!GPSXTRADATAENABLE?",	"Data Validity Time: 168",				"AT!GPSXTRADATAENABLE=1,10,2,1,1" },
		#if defined(MODULE_MC7430)
		{ "AT+WANT?",			"+WANT: 1",					"AT+WANT=1" },
		#endif
		{ NULL,						NULL,									NULL }
	};
	/* MC7304/7354 has different value range for this parameter [24-168]. */
	const char* mc73XX_autodl_int = "Data Autodownload Interval: 24";

	/* This function is called by model_physinit() --> sierra_init() and model_physinit() can be called multiple times
	 * so prevent multiple calling here */
	if (!first) {
		return 0;
	}
	first = 0;

	/* check GPS supporting models */
	if (strncmp(model_variants_name(), "MC8704", 6) &&
		strncmp(model_variants_name(), "MC7700", 6) &&
		strncmp(model_variants_name(), "MC7710", 6) &&
		strncmp(model_variants_name(), "MC7804", 6) &&
		strncmp(model_variants_name(), "MC7304", 6) &&
		strncmp(model_variants_name(), "MC7354", 6) &&
		strncmp(model_variants_name(), "MC7430", 6)) {
		SYSLOG_ERR("This module does not have GPSENABLE option");
		return 0;
	}

	if (rdb_get_single(rdb_name(RDB_GPS_INITIALIZED, ""), response, sizeof(response)) == 0 &&
		strcmp(response, "1") == 0) {
		SYSLOG_ERR("GPS already configured");
		return 0;
	}

	strcpy(cmd, "AT!CUSTOM?");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);
	pToken = strstr(response, "GPSENABLE");
	if (pToken == 0) {
		SYSLOG_ERR("GPS is not enabled yet, try to enable now");
		fully_configured = 0;
		strcpy(cmd, "AT!CUSTOM=\"GPSENABLE\",0X01");
		SEND_ATCMD_N_CHECK_ERROR(cmd, NULL, TRUE);
	} else {
		pToken += strlen("GPSENABLE");
		while (*pToken == '\t' || *pToken == ' ') {
			pToken++;
		}
		if (strncmp(pToken, "0x01", 4) != 0) {
			SYSLOG_ERR("GPS is not enabled yet, try to enable now");
			fully_configured = 0;
			strcpy(cmd, "AT!CUSTOM=\"GPSENABLE\",0X01");
			SEND_ATCMD_N_CHECK_ERROR(cmd, NULL, TRUE);
		}
	}

	/* check current configuration and send set commands */
	is_mc77xx = (!strncmp(model_variants_name(), "MC7700", 6) | !strncmp(model_variants_name(), "MC7710", 6) |
				 !strncmp(model_variants_name(), "MC7804", 6));
	is_mc73xx = (!strncmp(model_variants_name(), "MC7304", 6) | !strncmp(model_variants_name(), "MC7354", 6) |
				 !strncmp(model_variants_name(), "MC7430", 6));
	for (i = 0; gps_set_cmd[i].query_cmd; i++) {
		/* skip sending autodownload command for MC7710/7700/7804 because it is always error */
		/* At the moment, Validity Time is only available in MC73XX */
		if (!is_mc73xx && (strstr(gps_set_cmd[i].exp_resp, "Autodownload") ||
						   strstr(gps_set_cmd[i].exp_resp, "Validity Time"))) {
			continue;
		}
		strcpy(cmd, gps_set_cmd[i].query_cmd);
		SEND_ATCMD_N_CHECK_ERROR(cmd, response, FALSE);
		if (err == 0 && !strstr(response, gps_set_cmd[i].exp_resp)) {
			/* MC73XX has different value range for this parameter [24-168]. */
			if (is_mc73xx && strstr(gps_set_cmd[i].exp_resp, "Autodownload Interval")) {
				if (strstr(response, mc73XX_autodl_int)) {
					continue;
				} else {
					fully_configured = 0;
					SYSLOG_ERR("can not find expected response, set to '%s'", mc73XX_autodl_int);
				}
			} else {
				fully_configured = 0;
				SYSLOG_ERR("can not find expected response, set to '%s'", gps_set_cmd[i].exp_resp);
			}

			strcpy(cmd, gps_set_cmd[i].set_cmd);
			/* MC7710/7700/7804 are supporting 6 arguments for !GPSXTRADATAENABLE command
			 * according to enquiry command but in reality it fails and only accept
			 * three arguments, so send 3 arguments command until Sierra Wireless fix
			 * this problem. */
			if (is_mc77xx && strstr(cmd, "AT!GPSXTRADATAENABLE")) {
				strcpy(cmd, "AT!GPSXTRADATAENABLE=1,10,2");
			}
			/* MC73XX has 6 arguements with different range */
			else if (is_mc73xx && strstr(cmd, "AT!GPSXTRADATAENABLE")) {
				strcpy(cmd, "AT!GPSXTRADATAENABLE=1,10,2,1,24,168");
			}
			SEND_ATCMD_N_CHECK_ERROR(cmd, NULL, FALSE);
		}
	}

	if(fully_configured) {
		SYSLOG_ERR("GPS settings were fully configured already, set gps_initialized flag to 1");
		rdb_set_single(rdb_name(RDB_GPS_INITIALIZED, ""), "1");
	} else {
		rdb_set_single(rdb_name(RDB_GPS_INITIALIZED, ""), "0");
		SYSLOG_ERR("GPS settings were changed, reboot module to take effect new configuration");
		return 1;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////
static int initialize_emergency_number(void)
{
	static int first = 1;
	char response[AT_RESPONSE_MAX_SIZE];
	char cmd[128];
	int ok, err;
	char *pToken;

	/* MC7804/7304 does not support !ENUM command though it has voice feature */
	if (!strncmp(model_variants_name(), "MC7804", 6)||!strncmp(model_variants_name(), "MC7304", 6)||!strncmp(model_variants_name(), "MC7354", 6)) {
		return 0;
	}

	if (!first) {
		return 0;
	}
	first = 0;

	strcpy(cmd, "AT!CUSTOM?");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	pToken = strstr(response, "ISVOICEN");
	if (pToken == 0) {
		SYSLOG_ERR("error - can not find ISVOICEN option");
		return 0;
	}

	strcpy(cmd, "AT!NVENUM?");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	if (strstr(response, "000")) {
		SYSLOG_ERR("emergency number 000 was already set");
		return 0;
	}

	if (strstr(response, "Obsolete command")) {
		SYSLOG_ERR("AT!NVENUM command is not supported in this module");
		return 0;
	}

	SYSLOG_ERR("add emergency number 000 to NV");
	strcpy(cmd, "AT!NVENUM=1,\"000\"");
	SEND_ATCMD_N_CHECK_ERROR(cmd, response, TRUE);

	SYSLOG_ERR("emergency number 000 is set, now reset the module to take effect");
	return 1;
}

#if defined(MODULE_MC7304)
/*
 * Check setting DTR-High-Enable support
 *
 * Returns:
 *  1: supported
 *  0: not supported
 *  -1: error
 */
static int check_set_dtr_high_enable_support()
{
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;
	int stat;

	stat=at_send("AT!CUSTOM=?", response, "", &ok, 0);

	if ( !stat && ok) {
		if (strstr(response, "\"SETDTRHIGHENABLE\"")) {
			return 1;
		}
		else {
			return 0;
		}
	}
	else {
		SYSLOG_ERR("Error with AT!CUSTOM=? - stat=%d, ok=%d",stat, ok);
		return -1;
	}
}

/*
 * Do setting DTR-High-Enable as 0x01
 *
 * Returns:
 *  1: setting is done and module is being reset
 *  -1: error
 */
static int do_set_dtr_high_enable_support()
{
	int ok, err;

	SEND_ATCMD_N_CHECK_ERROR("AT!CUSTOM=\"SETDTRHIGHENABLE\",01", NULL, TRUE);
	if (!err) {
		SEND_ATCMD_N_CHECK_ERROR("AT!RESET", NULL, TRUE);
		return 1;
	}
	else {
		return -1;
	}
}

/*
 * check and set DTR-High-Enable
 *
 * Return:
 *  1: DTR-High-Enable is set (as 0x01) and module is reset
 *  0: DTR-High-Enable has already been set as 0x01 OR setting DTR-High-Enable is not supported
 *  -1: Error
 */
static int set_dtr_high_enable()
{
	char response[AT_RESPONSE_MAX_SIZE];
	int ok;
	int stat;
	int ret;

	/* check whether SETDTRHIGHENABLE is already set */
	stat=at_send("AT!CUSTOM?", response, "", &ok, 0);
	if (!stat && ok) {
		char *start;
		if ((start = strstr(response, " SETDTRHIGHENABLE\t"))) {
			/* it is set now, check whether it is set as 0x01 */
			char *end_of_line;
#define xSETDTRHIGHENABLEx_LENGTH	18
			start += xSETDTRHIGHENABLEx_LENGTH;
			end_of_line = strchrnul(start, '\n');
			*end_of_line = 0;
			/* ignore tabs between SETDTRHIGHENABLE and value */
			while (start < end_of_line) {
				if (*start == '\t') {
					start++;
				}
				else {
					break;
				}
			}
			if (!strcmp(start, "0x01")) {
				/* already set as 0x01 */
				ret = 0;
			}
			else {
				/* not set as 0x01. set to 0x01 now */
				ret = do_set_dtr_high_enable_support();
			}
		}
		else {
			/*
			 * SETDTRHIGHENABLE is not set
			 * if SETDTRHIGHENABLE is supported, set to 0x01
			 */
			ret = check_set_dtr_high_enable_support();
			if (ret == 1) {
				ret = do_set_dtr_high_enable_support();
			}
			else if (ret == 0) {
				SYSLOG_ERR("AT!CUSTOM with SETDTRHIGHENABLE is not supported");
			}
		}
	}
	else {
		SYSLOG_ERR("Error with AT!CUSTOM? - stat=%d, ok=%d",stat, ok);
		ret = -1;
	}

	return ret;
}
#endif

//////////////////////////////////////////////////////////////////////////////
/* Run essential customization for Sierra Wireless modules that needs
   rebooting the module, ie. GPS setting, band initialization,
   echo cancellation setting etcs,. */
static int sierra_customization(void)
{
	char cmd[128];
	int ok, err;
	int need_reboot = 0;

	SYSLOG_ERR("customization for Sierra Wireless modules");
	strcpy(cmd, "AT!ENTERCND=\"A710\"");
	SEND_ATCMD_N_CHECK_ERROR(cmd, NULL, TRUE);

#if defined(MODULE_MC7304)
	if (set_dtr_high_enable() == 1) {
		/* module is being reset, return now */
		return 0;
	}
#endif

	/* initialize GPS settings */
	need_reboot += initialize_gps_settings();

	/* initialize band list */
	need_reboot += initialize_bandlist();

	/* Synchronize voice call setting in module and router.
	 * For the case after factory reset with voice call enabled status,
	 * voice call feature is still enabled in module but rdb variable is
	 * initialized to default value and if this default value is 'disabled'
	 * then mismatch happens.
	 */
	need_reboot += sierra_synchronize_voice_call_setting();

	/* set echo cancellation mode off */
	#if defined(PLATFORM_PLATYPUS2)
	need_reboot += sierra_set_echo_cancellation();
	#endif

	/* set emergency number */
	need_reboot += initialize_emergency_number();

	need_reboot += initialize_custom_settings();

	// MTU size of Sierra MC7430 firmware for Telstra is set to 1358 by default.
	// This should be set to 1500.
	#if defined(SKIN_TEL) && defined(MODULE_MC7430)
	need_reboot += initialize_mtu_settings();
	#endif

	#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
	/* Reset after settings GPS command makes MC7304 and MC7354 crash after switching PRI */
	#elif defined(MODULE_MC7430)
	/* reset module if needed -  AT!RESET command does not work for MC7430*/
	if (need_reboot > 0) {
		SYSLOG_ERR("Reboot module in order to apply customization");
		system("reboot_module.sh");
	}
	#else
	/* reset module if needed */
	if (need_reboot > 0) {
		SYSLOG_ERR("send AT!RESET to reboot module in order to apply customization");
		strcpy(cmd, "AT!RESET");
		SEND_ATCMD_N_CHECK_ERROR(cmd, NULL, TRUE);
	}
	#endif

	return 0;
}

#ifdef ENABLE_NETWORK_RAT_SEL
/* Sierra technology select of selrat MC7304 */

static const char* sierra_get_selrat(const char* selrat)
{
	static char resp[AT_RESPONSE_MAX_SIZE];
	int stat;
	int ok;

	char* p;


	/* get supported network technology list */
	stat=at_send(selrat, resp, "", &ok, 0);

	/* check transfer result */
	if(stat<0) {
		syslog(LOG_ERR,"failed to send selrat AT command (selrat=%s)",selrat);
		goto err;
	}

	/* check AT command result */
	if(!ok) {
		syslog(LOG_ERR,"got error result from selrat AT command (selrat=%s)",selrat);
		goto err;
	}

	/* replace delimiter */
	p=resp;
	while(*p) {
		if(*p=='&')
			*p='/';
		p++;
	}

	return resp;

err:
	return NULL;
}

static int sierra_init_selrat(void)
{
	const char *resp;

	char linebuf[AT_RESPONSE_MAX_SIZE];

	int reg_init=0;
	regex_t reg;
	regmatch_t pm;
	int len;

	const char* r;

	int stat;

	int rc=-1;

	int selrat_pcnt;
	int selrat_idx;
	char selrat_name[AT_RESPONSE_MAX_SIZE];
	char selrat_amp[AT_RESPONSE_MAX_SIZE+255+1]; /* resp + maximum items - delimiters (255) + null */

	selrat_amp[0]=0;

	/* reset selrat counter */
	_selrat_cnt=0;

	/* get selrat result */
	resp=sierra_get_selrat("AT!SELRAT=?");
	if(!resp) {
		syslog(LOG_ERR,"failed in sierra_get_selra()");
		goto err;
	}

	/* start parse AT command response */

/*
	* example of !selrat

	AT!SELRAT=?
	!SELRAT: Index, Name
	00, Automatic
	01, UMTS 3G Only
	02, GSM 2G Only
	03, Automatic
	04, Automatic
	05, GSM and UMTS Only
	06, LTE Only
	07, GSM, UMTS, LTE
	08, CDMA, HRPD, GSM, UMTS, LTE
	09, CDMA only
	0A, HRPD only
	0B, Hybrid CDMA/HRPD
	0C, CDMA, LTE
	0D, HRPD, LTE
	0E, CDMA, HRPD, LTE
	0F, CDMA, GSM, UMTS
	10, CDMA, HRPD, GSM, UMTS
	11, UMTS and LTE Only
	12, GSM and LTE Only
*/

	/* init loop */
	r=resp;

	/* compile regex */
	stat=regcomp(&reg,"^[0-9A-Fa-f][0-9A-Fa-f], .*$",REG_EXTENDED|REG_NEWLINE);
	if(stat<0) {
		SYSLOG_ERR("failed in regcomp() - %s",strerror(errno));
		goto err;
	}
	reg_init=1;

	/* start matching */
	while((stat=regexec(&reg,r,1,&pm,0))==REG_NOERROR) {

		/* add delimiter */
		if(*selrat_amp)
			strncat(selrat_amp,"&",sizeof(selrat_amp));

		/* extraact a new found item */
		len=pm.rm_eo-pm.rm_so;
		memcpy(linebuf,r+pm.rm_so,len);
		linebuf[len]=0;

		/* read into variables */
		selrat_pcnt=sscanf(linebuf,"%x, %[^\t\n]",&selrat_idx,selrat_name);
		if(selrat_pcnt!=2) {
			syslog(LOG_ERR,"incorrect selrat format found (line=%s)",linebuf);
		}
		else {
			struct selrat_info_t* info;

			/* write to buffer and concatenate */
			snprintf(linebuf,sizeof(linebuf),"%d,%s",selrat_idx,selrat_name);
			strncat(selrat_amp,linebuf,sizeof(selrat_amp));

			/* write into global selrat info */
			info=&_selrat_info[_selrat_cnt++];
			info->idx=selrat_idx;
			snprintf(info->name,sizeof(info->name),selrat_name);
		}

		/* increase source */
		r+=pm.rm_eo;
	}

	/* check regexec error */
	if(stat!=REG_NOMATCH) {
		syslog(LOG_ERR,"failed to use regexec() - %s",strerror(errno));
		goto err;
	}

	/* return success */
	rc=0;

fini:
	/* set rdb */
	rdb_set_single(rdb_name(RDB_MODULERATLIST, ""), selrat_amp);

	/* free regex */
	if(reg_init)
		regfree(&reg);

	return rc;

err:
	rc=-1;
	goto fini;
}
#endif

//////////////////////////////////////////////////////////////////////////////
static void get_preferred_PLMN_list(void)
{
	int ok;
	int stat;
	char* token;
	char response[4098];
	char buffer[4098]={0,};

	/* "AT+CPOL?" command is to get Preferred PLMN list in the SIM/USIM card.
	 * +CPOL: <index1>,<format>,<oper1>[,<GSM_AcT1>,<GSM_Compact_AcT1>,<UTRAN_AcT1>,<E-UTRAN_AcT1>]
	 *
	 * AT+CPOL?
	 * +CPOL: 1,2,"50501",0,0,0,1
	 * +CPOL: 2,2,"50501",0,0,1,0
	 * +CPOL: 3,2,"50501",1,0,0,0
	 */

	stat=at_send("AT+CPOL?", response, "+CPOL: ", &ok, 0);
	if ( !stat && ok && isImplemented(response)) {
		for(token = strtok(response, "\n");token!=0; token = strtok(0, "\n")) {
			if (strstr(token, "+CPOL: ")) {
				str_replace(&token[0], "+CPOL: ", "");
				strcat( buffer, token);
				strcat( buffer, "&");
			}
		}
		rdb_set_single(rdb_name(RDB_CPOL_PREF_PLMNLIST, ""), buffer);
	}
	else {
		rdb_set_single(rdb_name(RDB_CPOL_PREF_PLMNLIST, ""), "");
	}
}
//////////////////////////////////////////////////////////////////////////////
static int sierra_init(void)
{
	int ok;
	int stat;
	char response[4098];
	char value[4098];
	char *pos1, *pos2;
	const char* szCnsBandName;
	int idx;
	int hex;

	char* p;

	/* all essential customization */
	(void) sierra_customization();

	if(_fDiversityEnable==0)
		at_send("at!rxden=0", NULL, "", &ok, 0);
	else if(_fDiversityEnable==1)
		at_send("at!rxden=1", NULL, "", &ok, 0);

	/* use !gband command and hard-coded bands list rather than !band command
	   for New Zealand same as Telstra requested band list for MC8704. */
	if (use_gband_command > 0) {
		rdb_set_single(rdb_name(RDB_MODULEBANDLIST, ""), Fixed_band_Telstra);
		return 0;
	}

	if(is_enabled_feature(FEATUREHASH_CMD_BANDSEL)) {
		stat=at_send("AT!BAND=?", response, "", &ok, 0);
		if ( !stat && ok)
		{
#if defined(MODULE_MC7430)
			/*
			 * Reference: 4117727_AirPrime_EM74xx-MC74xx_AT_Command_Reference_v2.pdf
			 * Quoted:
			 * Query List: AT!BAND=?
			 *
			 * Response: Index, Name[,     GW Band Mask [ L Band Mask]]
			 * 	<Index1>, <Name1>[,     <GWmask1> [,     <Lmask1>]]
			 * 	...
			 * 	<Index1>, <Name1>[,     <GWmask1> [,     <Lmask1>]]
			 * 	OK
			 *
			 * 	ACTUALLY there is no comma (",") between <Name#> and masks
			 * 	and there is a list of masks following the band list
			 */
			char *pstring, *pstr;
			char *token;
			int first_line_done = 0, first_band_done = 0;
			int rval, len_to_write;
			char *pvalue = value;

			pstring = response;
			while ((token = strsep(&pstring, "\n")) != NULL){
				/* skip header */
				if (!first_line_done) {
					first_line_done = 1;
					continue;
				}
				/* only process band list - ignore list of masks */
				if (*token == ' ') {
					break;
				}
				/* strip masks */
				if (pstr = strstr(token, "   ")) {
					*pstr = 0;
				}
				/* build band list string to write to RDB variable */
				/* the space before <Name#> will be removed in webif */
				len_to_write = sizeof(value) - (pvalue - value);
				if (!first_band_done) {
					rval = snprintf(pvalue, len_to_write,"%s", token);
					first_band_done = 1;
				}
				else {
					rval = snprintf(pvalue, len_to_write,"&%s", token);
				}
				if (rval >= len_to_write){
					SYSLOG_ERR("Failed to parse band list - Band list response is too long.");
					value[0] = 0;
					break;
				}
				else if (rval < 0){
					SYSLOG_ERR("An output error is encountered while parsing band list.");
					value[0] = 0;
					break;
				}
				else {
					pvalue += rval;
				}
			}
#else
			get_rid_of_band_mask_bits(response);
			get_rid_of_newline_char(response);

			*value=0;
			if(!strstr(response, "GW Band Mask")) {

				/* remove delimiter */
				p=response;
				while(*p) {
					if(*p=='&')
						*p='/';
					p++;
				}

			/*** the AirCard 320U has the extra "," in Band Name ---> "01, LTE 1800, WCDMA 850/2100" ***/
			pos1=strstr( response, "01, LTE 1800, WCDMA 850");
			if(pos1)
				*(pos1+12)=' ';
			/*** new AirCard 320U has the extra "," in Band Name ---> "01, LTE 1800, WCDMA 900/2100" ***/
			pos1=strstr( response, "01, LTE 1800, WCDMA 900");
			if(pos1)
				*(pos1+12)=' ';
			/***********************************************************************************/

				pos1=strrchr( response, ',');
				while( pos1 )
				{
					if( !strstr(pos1, "N/A") )
					{
						if( *value )
							strcat( value, "&");

						hex=isxdigit(*(pos1-2)) && isxdigit(*(pos1-1));

						idx = Hex((char)*(pos1-2))<<4 | Hex((char)*(pos1-1));
						if (!sierra_airprime)
							szCnsBandName = sierra_convert_bandidx_to_bandname(idx);
						else
							szCnsBandName = pos1+1;
						while(*szCnsBandName<=' ')
							szCnsBandName++;

						if(hex)
							sprintf( value+strlen(value), "%02X,%s", idx, szCnsBandName );
						pos2=value+strlen(value)-1;
						while(*pos2<=' ')
							pos2--;
						*(pos2+1)=0;
					}
					if( strstr(pos1-2, "00,") ) break;
					*(pos1-2)=0;
					pos1=strrchr( response, ',');
				}
			}
#endif
			rdb_set_single(rdb_name(RDB_MODULEBANDLIST, ""), value);
		}

		handleBandGet(); // Both current band setting and band list should be updated together.
	}

	#ifdef ENABLE_NETWORK_RAT_SEL
	/* enumerate network technologies */
	sierra_init_selrat();
	#endif

	#ifdef V_SUB_NETIF_1
	stat=at_send("AT+CGDCONT?", response, "", &ok, 0);
	if ( !stat && ok)
	{
		for (idx = 1; idx <= NUM_OF_SUB_NETIF+1; idx++)
		{
			*value = 0;

			#ifdef MODULE_MC7430
			sprintf(value, "+CGDCONT: %d,\"IP\"" , idx);
			#else
			sprintf(value, "+CGDCONT: %d," , idx);
			#endif

			if (!strstr(response, value))
			{
				sprintf(value, "AT+CGDCONT=%d,\"IP\"" , idx);
				send_atcmd(value);
			}
		}
	}
	#endif

#ifdef V_MANUAL_ROAMING_vdfglobal
{
	int config_idx = 0xff;
	char *endptr;
	*value = 0;
	if ((rdb_get_single(rdb_name(RDB_BANDCFG, ""), value, sizeof(value)) == 0)
		&& currentband_idx != 0xff) { 
		errno = 0;
		config_idx = strtol(value, &endptr, 16);
		if(errno == 0 && (endptr != value) && currentband_idx != config_idx) {
			rdb_set_single(rdb_name(RDB_BANDPARAM, ""), value);
			handleBandSet();
		}
	}
}
#endif

#if defined(SKIN_TEL)
	get_preferred_PLMN_list();
#endif

	//update_ccid();
	return 0;
}
int sierra_update_temperature()
{
	char command[32];
	char response[500];
	int ok;
	char rdb_buff[128];
	char *pToken, *pSavePtr, *pLastValidToken, temperature[4];

	pLastValidToken=NULL;
	memset(temperature, '\0', 4);

	// build command
	sprintf(command, "AT!PCTEMP?");
	// send command
	if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		return -1;

	pToken = strtok_r(response, ":", &pSavePtr);

	while( pToken != NULL )
	{
	  pLastValidToken = pToken;
	  pToken = strtok_r(NULL, ":", &pSavePtr);

	}
	if (pLastValidToken != NULL) {
	  strncpy(temperature, pLastValidToken, 3);
	  // set temperature rdb
	  snprintf(rdb_buff,sizeof(rdb_buff),"%s",temperature);
	  rdb_set_single(rdb_name(RDB_MODULETEMPERATURE, ""), rdb_buff);
	}
	else {
	  syslog(LOG_ERR,"pLastValidToken is NULL");
	}


	return 0;
}
int sierra_update_date_and_time()
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

	/*
		Since Sierra MC7304/MC7354 +CCLK returns incorrect time-zone information, use !TIME? instead
		But before using !TIME?, make sure !TIME? was synchronized by sending +CCLK.
	*/

	// build clock command
	sprintf(command, "AT+CCLK?");
	// send command
	if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		return -1;

#if defined(MODULE_MC7430)
	// AT!TIME has been deprecated on MC7430.
	sprintf(command, "AT!UTCTIME?");
	// send command
	if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		return -1;

/*
	AT!UTCTIME?
	!UTCTIME: 
	2016/03/14
	03:54:58
	TZ:+44
	DST:+1

	OK
*/
	#define PREFIX_UTCTIME	"!UTCTIME:"
	// get time string
	if( !(ptr=strstr(response,PREFIX_UTCTIME)) ) {
		syslog(LOG_ERR,"prefix not found in commmand(%s) - %s",command,response);
		return -1;
	}
	// skip prefix
	ptr+=STRLEN(PREFIX_UTCTIME);
#else
	// build clock command
	sprintf(command, "AT!TIME?");
	// send command
	if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		return -1;

/*
	at!time?
	!TIME:
	2014/06/13
	09:33:35 (local)
	2014/06/12
	23:33:35 (UTC)
*/
	#define PREFIX_TIME	"!TIME: "
	#define PREFIX_TIME_UTC	"(local)\n"

	// get time string
	if( !(ptr=strstr(response,PREFIX_TIME)) ) {
		syslog(LOG_ERR,"prefix not found in commmand(%s) - %s",command,response);
		return -1;
	}
	// skip prefix
	ptr+=STRLEN(PREFIX_TIME);

	/* search lock time */
	ptr=strstr(ptr,PREFIX_TIME_UTC);
	if(!ptr) {
		syslog(LOG_ERR,"unknown AT command result format (%s) - %s",command,response);
		return -1;
	}
	// skip prefix
	ptr+=STRLEN(PREFIX_TIME_UTC);
#endif

	// convert str to struct tm
	if(!(ptr=strptime(ptr,"%Y/%m/%d\n%H:%M:%S",&tm_utc))) {
		syslog(LOG_ERR,"unknown time format (%s) - %s",ptr,strerror(errno));
		return -1;
	}

	// convert to time_t
	time_network=convert_utc_to_local(&tm_utc);

	// set date and time rdb
	snprintf(rdb_buff,sizeof(rdb_buff),"%ld",time_network-time_now);
	rdb_set_single(rdb_name(RDB_DATE_AND_TIME, ""), rdb_buff);

	return 0;
}

static char* send_atcmd(const char* atcmd)
{

	int ok;

	static char _resp[AT_RESPONSE_MAX_SIZE];

	_resp[0]=0;

	if(at_send(atcmd, _resp, "", &ok, 0)<0) {
		syslog(LOG_ERR,"failed to send AT command - %s",atcmd);
		goto err;
	}

	if(!ok) {
		//syslog(LOG_ERR,"get failure code from AT command - %s",atcmd);
		goto err;
	}

	return _resp;
err:
	return NULL;
}

static void sierra_update_cdma_msid(void)
{
	char* resp;

	/*
		AT$MSID?
		9132692818
	*/

	/* get current profile */
	resp=send_atcmd("AT$MSID?");
	if(!resp)
		return;

	rdb_set_single(rdb_name(RDB_MSID, ""), resp);
}

static void sierra_update_cdma_mip_profile_info(void)
{
	char* resp;
	char* p;
	char cmd[256];
	int pf=0;

	/*
		AT$QCMIPP?
		$QCMIPP: 1
	*/

	/* get current profile */
	resp=send_atcmd("AT$QCMIPP?");
	if(!resp)
		return;

	p=strstr(resp,"$QCMIPP: ");
	if(p) {
		p+=STRLEN("$QCMIPP: ");
		pf=atoi(p);
	}

	snprintf(cmd,sizeof(cmd),"AT$QCMIPGETP=%d",pf);
	/* MIP Profile information */
	resp=send_atcmd(cmd);
	if(resp) {
		char *pos1, *pos2;
		pos1=strstr(resp, cmd);
		while(pos1) {
			resp=pos1+strlen(cmd);
			pos1=strstr(resp, cmd);
		}
		pos2=resp;
		pos1=strchr(pos2, '\n');
		while(pos1) {
			*pos1='&';
			pos2=++pos1;
			pos1=strchr(pos2, '\n');
		}
		pos1=strstr(resp, "OK");
		if(pos1) {
			*pos1=0;
		}
		rdb_set_single(rdb_name(RDB_CDMA_PREFSET_MIPINFO, ""), resp);
	}
}

#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
int sierra_update_impref()
{
	char* resp;
	char* p;
	char* p2;

	char* carrier=NULL;
	char* config=NULL;

	/*
		at!gobiimpref?
		!GOBIIMPREF:
		preferred fw version:   05.05.16.03
		preferred carrier name: VZW
		preferred config name:  VZW_005.013_013
		current fw version:     05.05.16.03
		current carrier name:   VZW
		current config name:    VZW_005.013_013

		OK
	*/


	/* get gobi image preference */
	resp=send_atcmd("AT!GOBIIMPREF?");

	/* bypass if any error */
	if(!resp) {
		goto err;
	}


	/* get carrier */
	p=strstr(resp, "current carrier name:");
	if(p) {
		p+=STRLEN("current carrier name:");
		while(*p && isspace(*p))
			p++;
		p2=strchr(p,'\n');

		if(p2)
			carrier=strndupa(p,p2-p);
		else
			carrier=strdupa(p);
	}

	/* get config */
	p=strstr(resp, "current config name:");
	if(p) {
		p+=STRLEN("current config name:");
		while(*p && isspace(*p))
			p++;
		p2=strchr(p,'\n');

		if(p2)
			config=strndupa(p,p2-p);
		else
			config=strdupa(p);
	}


	/* set RDB */
	if(carrier)
		rdb_set_single(rdb_name(RDB_PRIID_CARRIER, ""), carrier);
	else
		rdb_set_single(rdb_name(RDB_PRIID_CARRIER, ""), "");

	if(config)
		rdb_set_single(rdb_name(RDB_PRIID_CONFIG, ""), config);
	else
		rdb_set_single(rdb_name(RDB_PRIID_CONFIG, ""), "");

	return 0;

err:
	return -1;
}

int sierra_update_meid_esn()
{
	char* resp;
	char* p;
	char* p2;

	char* esn=NULL;
	char* meid=NULL;

	/*
		ati
		Manufacturer: Sierra Wireless, Incorporated
		Model: MC7354
		Revision: SWI9X15C_05.05.16.03 r22385 carmd-fwbuild1 2014/06/04 15:01:26
		MEID: A100003DB252CE
		ESN: 12812169004, 80B9AF2C
		IMEI: 359225050024702
		IMEI SV: 13
		FSN: J8341600150310
		+GCAP: +CIS707-A, CIS-856, CIS-856-A, +CGSM, +CLTE2, +MS, +ES, +DS, +FCLASS


		OK
	*/


	/* get meid and esn */
	resp=send_atcmd("ATI");

	/* bypass if any error */
	if(!resp) {
		goto err;
	}


	/* get MEID */
	p=strstr(resp, "MEID: ");
	if(p) {
		p+=STRLEN("MEID: ");
		p2=strchr(p,'\n');
		if(p2)
			meid=strndupa(p,p2-p);
		else
			meid=strdupa(p);
	}

	/* get ESN */
	p=strstr(resp, "ESN: ");
	if(p) {
		p+=STRLEN("ESN: ");
		p2=strchr(p,'\n');
		if(p2)
			esn=strndupa(p,p2-p);
		else
			esn=strdupa(p);
	}

	/* set RDB */
	if(meid)
		rdb_set_single(rdb_name(RDB_MEID, ""), meid);
	else
		rdb_set_single(rdb_name(RDB_MEID, ""), "");

	if(esn)
		rdb_set_single(rdb_name(RDB_ESN, ""), esn);
	else
		rdb_set_single(rdb_name(RDB_ESN, ""), "");

	return 0;

err:
	return -1;
}

int sierra_update_prlver()
{
	char* resp;
	char* p;
	char* p2;
	char* prlver=NULL;

	/*
		AT!PRLVER?
		PRL VER: 15339

		OK
	*/


	/* get PRL version */
	resp=send_atcmd("AT!PRLVER?");

	/* get ESN */
	p=strstr(resp, "PRL VER: ");
	if(p) {
		p+=STRLEN("PRL VER: ");
		p2=strchr(p,'\n');
		if(p2)
			prlver=strndupa(p,p2-p);
		else
			prlver=strdupa(p);
	}

	if(prlver && (atoi(prlver)!=0))
		rdb_set_single(rdb_name(RDB_PRLVER, ""), prlver);
	else
		rdb_set_single(rdb_name(RDB_PRLVER, ""), "");

	return 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////
int sierra_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	/* TODO : many of the following functions should be moving to init or triggered by chg_status*/

	int sim_card_ready;
	sim_card_ready=new_status->status[model_status_sim_ready];

	// do not do the following AT commands when SIM card is not ready - Otherwise, Sierra dongles are locked in Limited service

	if(sim_card_ready) {
		update_sim_hint();
		update_configuration_setting(new_status);
		update_roaming_status();
	}

	sierra_update_date_and_time();
	sierra_update_temperature();
	/* set default band sel mode to auto after SIM card is ready to prevent
	 * 3G module is locked in limited service state. */
	initialize_band_selection_mode(new_status);

	if(new_status->status[model_status_registered])
		update_call_forwarding();

	// do not run multiple times - this drops performance in AirCard 320 and 312
	sierra_update_boot_version();

	if(is_enabled_feature(FEATUREHASH_CMD_BANDSEL)) {
		handleBandGet(); // only do this if feature is enabled
	}

	sierra_update_network_status();

	if(is_enabled_feature(FEATUREHASH_UNSPECIFIED)) {
		update_sierra_ccid(); // other manager might take care of this
	}

	update_network_name();
	update_service_type();

	/* skip if the variable is updated by other port manager such as cnsmgr or
	 * process SIM operation if module supports +CPINC command */
	if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS) || support_pin_counter_at_cmd) {
		update_sim_status();
		/* Latest Sierra modems like MC8704, MC8801 etcs returns error for +CSQ command
		 * when SIM card is locked so update signal strength with cnsmgr. */
		if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS)) {
			update_signal_strength();
		}
	}

	update_imsi();

	#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
	sierra_update_meid_esn();
	sierra_update_prlver();
	sierra_update_impref();
	#endif

	if(sierra_cdma) {
		sierra_update_cdma_mip_profile_info();
		sierra_update_cdma_msid();
	}

	return 0;
}


static void strict_strncpy(char* dst,const char* src,int dst_len,int src_len)
{
	#define MIN(x,y)  ( (x)<(y)?(x):(y) )

	char* ptr=src_len<dst_len?dst+src_len:dst+dst_len;

	strncpy(dst,src,MIN(dst_len,src_len));
	*ptr=0;
}

static char* strip_spaces(const char* str)
{
	static char spaces[256];
	int idx;
	int len;

	// skip space
	while(*str && (*str==' ' || *str=='\n')) {
		str++;
	}

	// count total content
	len=idx=0;
	while(str[idx]) {
		if(str[idx]!=' ' && str[idx]!='\n')
			len=idx+1;
		idx++;
	}

	// copy content
	strict_strncpy(spaces,str,sizeof(spaces),len);

	return spaces;
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

int sierra_handle_command_sim(const struct name_value_t* args)
{
	int ok = 0;
	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[128];

	char newpin[16];
	char simpuk[16];
	char pin[16];

	char* szCmdStat = NULL;

	char* cmd;
	char* token;

	char mncmcc_list[256];
	int mncmcc_len;
	char* mncmcc_ptr;
	const char* mncmcc_ent;

	// bypass if incorrect argument
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		goto error;
	}

	// check sim status
	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0 || !ok)
	{
		szCmdStat = "error - AT+CPIN? has failed";
		goto error;
	}

	SET_LOG_LEVEL_TO_SIM_LOGMASK_LEVEL

	int fSIMReady = !strcmp(response, "+CPIN: READY") || !strcmp(response, "+CPIN: USIM READY");
	int fSIMPin = !strcmp(response, "+CPIN: SIM PIN") || !strcmp(response, "+CPIN: USIM PIN");

	// read pin
	if (rdb_get_single(rdb_name(RDB_SIMPARAM, ""), pin, sizeof(pin)) != 0)
		pin[0] = 0;

	convClearPwd(pin);

	//SYSLOG_ERR("SIM command : %s", args[0].value);

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

/*
		at!nvplmn?
		MCC MNC
		???,??


		OK
*/

		// unfortunately, privilege command required
		cmd="AT!ENTERCND=\"A710\"";
		if (at_send(cmd, NULL, "", &ok, 0) != 0 || !ok) {
			sprintf(buf, "error - %s has failed", cmd);
			szCmdStat = buf;
			goto error;
		}

		// get NVPLMN
		cmd="AT!NVPLMN?";
		if (at_send(cmd, response, "", &ok, 0) != 0 || !ok) {
			sprintf(buf, "error - %s has failed", cmd);
			szCmdStat = buf;
			goto error;
		}

		// tokenize by carridge return
		mncmcc_len=0;
		mncmcc_list[0]=0;
		for(token = strtok(response, "\n");token!=0; token = strtok(0, "\n")) {
			mncmcc_ptr=mncmcc_list+mncmcc_len;
			mncmcc_ent=strip_spaces(token);
			if(isdigit(mncmcc_ent[0]))
				mncmcc_len+=snprintf(mncmcc_ptr,sizeof(mncmcc_list)-mncmcc_len,mncmcc_len?",%s":"%s",mncmcc_ent);
		}

		// set rdb variable
		rdb_set_single(rdb_name(RDB_SIMMCCMNC,""),mncmcc_list);
	}
	// sim commmand - lockmep
	else if (memcmp(args[0].value, "lockmep", STRLEN("lockmep")) == 0) {
		char mccmnc[256];

		// unfortunately, privilege command required
		cmd="AT!ENTERCND=\"A710\"";
		if (at_send(cmd, NULL, "", &ok, 0) != 0 || !ok) {
			sprintf(buf, "error - %s has failed", cmd);
			szCmdStat = buf;
			goto error;
		}

		// get NVPLMN
		cmd="AT!NVPLMN?";
		if (at_send(cmd, response, "", &ok, 0) != 0 || !ok) {
			sprintf(buf, "error - %s has failed", cmd);
			szCmdStat = buf;
			goto error;
		}

		// tokenize by carridge return
		mncmcc_len=0;
		mncmcc_list[0]=0;
		for(token = strtok(response, "\n");token!=0; token = strtok(0, "\n")) {
			mncmcc_ptr=mncmcc_list+mncmcc_len;
			mncmcc_ent=strip_spaces(token);
			if(isdigit(mncmcc_ent[0]))
				mncmcc_len+=snprintf(mncmcc_ptr,sizeof(mncmcc_list)-mncmcc_len,mncmcc_len?",%s":"%s",mncmcc_ent);
		}

		// get mccmnc list
		if (rdb_get_single(rdb_name(RDB_SIMMCCMNC, ""), mccmnc, sizeof(mccmnc)) != 0) {
			mccmnc[0]=0;
		}

		// error if no PLMN is specified
		if(!mccmnc[0] && !mncmcc_list[0]) {
			sprintf(buf, "error - module has not been locked before. MCC MNC should be not specified");
			szCmdStat = buf;
			goto error;
		}

		// apply if different mccmnc list
		if(mccmnc[0] && strcmp(mccmnc,mncmcc_list)) {

			// send command - this is a priviledge command but we do not send the previledge unlock command but we do at the beginning
			sprintf(command, "at!nvplmnclr");
			if (at_send(command, response, "", &ok, 0) != 0 || !ok) {
				sprintf(buf, "error - %s has failed", command);
				szCmdStat = buf;
				goto error;
			}

			// send command
			sprintf(command, "at!nvplmn=%s",mccmnc);
			if (at_send(command, response, "", &ok, 0) != 0 || !ok) {
				sprintf(buf, "error - %s has failed", command);
				szCmdStat = buf;
				goto error;
			}
		}

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

			// sim busy until the sim becomes ready
			rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM BUSY");

			sprintf(command, "AT+CPIN=\"%s\",\"%s\"", simpuk, newpin);
			if (at_send(command, response, "", &ok, 0) != 0 || !ok)
			{
				sprintf(buf, "%s has failed", command);
				szCmdStat = buf;
				goto error;
			}

			// wait for sim card to be ready
			wait_for_sim_card(3);
		}
	}
	// sim command - verifypin
	else if (!memcmp(args[0].value, "verifypin", STRLEN("verifypin")))
	{
		if (fSIMPin)
		{
			// sim busy until the sim becomes ready
			rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM BUSY");

			if (verifyPin(pin) < 0)
			{
				sprintf(buf, "%s\"%s\" has failed", "AT+CPIN=", pin);
				szCmdStat = buf;
				strcpy((char *)&last_failed_pin, pin);
				goto error;
			} else {
				(void) memset((char *)&last_failed_pin, 0x00, 16);
			}

			// wait for sim card to be ready
			syslog(LOG_INFO,"waiting until the sim card is ready");
			wait_for_sim_card(3);
			syslog(LOG_INFO,"sim card is done");
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
			sprintf(buf, "%s has failed", command);
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
			sprintf(buf, "%s has failed", command);
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
			sprintf(buf, "%s has failed", command);
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

#ifdef PLATFORM_BOVINE
	rdb_set_single(rdb_name(RDB_SIMRESULTOFUO, ""), "Operation succeeded");
#else
	rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "Operation succeeded");
#endif

	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return 0;

error:
		updateSIMStat();

	if (szCmdStat) {
#ifdef PLATFORM_BOVINE
		rdb_set_single(rdb_name(RDB_SIMRESULTOFUO, ""), szCmdStat);
#else
		rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), szCmdStat);
#endif
	}

	SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL
	return -1;
}

#ifdef GPS_ON_AT
////////////////////////////////////////////////////////////////////////////////
const char* gps_gen_err[33] = {
    "Phone is offline",                                         /* 00 */
    "No service",                                               /* 01 */
    "No connection with PDE (Position Determining Entity)",     /* 02 */
    "No data available",                                        /* 03 */
    "Session Manager is busy",                                  /* 04 */
    "Reserved",                                                 /* 05 */
    "Phone is GPS-locked",                                      /* 06 */
    "Connection failure with PDE",                              /* 07 */
    "Session ended because of error condition",                 /* 08 */
    "User ended the session",                                   /* 09 */
    "End key pressed from UI",                                  /* 10 */
    "Network session was ended",                                /* 11 */
    "Timeout (for GPS search)",                                 /* 12 */
    "Conflicting request for session and level of privacy",     /* 13 */
    "Could not connect to the network",                         /* 14 */
    "Error in fix",                                             /* 15 */
    "Reject from PDE",                                          /* 16 */
    "Reserved",                                                 /* 17 */
    "Ending session due to E911 call",                          /* 18 */
    "Server error",                                             /* 19 */
    "Reserved",                                                 /* 20 */
    "Reserved",                                                 /* 21 */
    "Unknown system error",                                     /* 22 */
    "Unsupported service",                                      /* 23 */
    "Subscription violation",                                   /* 24 */
    "Desired fix method failed",                                /* 25 */
    "Reserved",                                                 /* 26 */
    "No fix reported because no Tx confirmation was received",  /* 27 */
    "Network indicated normal end of session",                  /* 28 */
    "No error specified by the network",                        /* 29 */
    "No resources left on the network",                         /* 30 */
    "Position server not available",                            /* 31 */
    "Network reported an unsupported version of protocol"       /* 32 */
};

const char* gps_fix_err[27] = {
    "No error",                                                 /* 00 */
    "Invalid client ID",                                        /* 01 */
    "Bad service parameter",                                    /* 02 */
    "Bad session type parameter",                               /* 03 */
    "Incorrect privacy parameter",                              /* 04 */
    "Incorrect download parameter",                             /* 05 */
    "Incorrect network access parameter",                       /* 06 */
    "Incorrect operation parameter",                            /* 07 */
    "Incorrect number of fixes parameter",                      /* 08 */
    "Incorrect server information parameter",                   /* 09 */
    "Error in timeout parameter",                               /* 10 */
    "Error in QOS accuracy threshold parameter",                /* 11 */
    "No active session to terminate",                           /* 12 */
    "Session is active",                                        /* 13 */
    "Session is busy",                                          /* 14 */
    "Phone is offline",                                         /* 15 */
    "Phone is CDMA locked",                                     /* 16 */
    "GPS is locked",                                            /* 17 */
    "Command is invalid in current state",                      /* 18 */
    "Connection failure with PDE",                              /* 19 */
    "PDSM command buffer unavailable to queue command",         /* 20 */
    "Search communication problem",                             /* 21 */
    "Temporary problem reporting position determination results",/* 22 */
    "Error mode not supported",                                 /* 23 */
    "Periodic NI in progress",                                  /* 24 */
    "Unknown error",                                            /* 25 */
    "Unknown error"                                             /* 26 */
};

static int SendSierraGpsCommand(const char* cmd, int set_error)
{
	int stat, fOK, errcode, err_ret = 0;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char* rp = response;
	__goToErrorIfFalse(response)

	stat = at_send(cmd, response, "", &fOK, AT_RESPONSE_MAX_SIZE);
	//SYSLOG_ERR("response='%s'", response);
    /* ErrCode = 12 */
    if (set_error) {
        if (strncmp(response, "ErrCode", 7) == 0) {
            rp += (sizeof("ErrCode = ")-1);
            errcode = atoi(rp);
            if (strncmp(cmd, "AT!GPSFIX", sizeof("AT!GPSFIX")) == 0 && errcode <= 26) {
            	err_ret = 1;
				SYSLOG_ERR("errcode %d, '%s'", errcode, gps_fix_err[errcode]);
            	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE), gps_fix_err[errcode]);
            } else if (errcode <= 32) {
            	err_ret = 1;
				SYSLOG_ERR("errcode %d, '%s'", errcode, gps_gen_err[errcode]);
            	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE), gps_gen_err[errcode]);
            }
        } else {
            rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE), "");
        }
    }
    __free(response);
	if (stat < 0 || !fOK || err_ret) {
		SYSLOG_ERR("'%s' command failed", cmd);
		return -1;
	}
	return 0;
error:
	return -1;
}

static int sierra_check_gps_status(int *result)
{
	int stat = 0, fOK;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char *pToken;
	__goToErrorIfFalse(response)

	stat = at_send("AT!GPSSTATUS?", response, "", &fOK, AT_RESPONSE_MAX_SIZE);
	//SYSLOG_ERR("response='%s'", response);
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("'AT!GPSSTATUS?' command failed");
		__goToError()
	}

	/* Current time: 2012 07 31 1 06:12:05
     * 2012 07 31 1 06:10:48 Last Fix Status    = SUCCESS
     * 2012 07 31 1 06:10:48 Fix Session Status = SUCCESS
     * TTFF (sec) = 33
	 */
	pToken = strstr(response, "Fix Session Status");
    if (!pToken || strlen(pToken) == 0)
        __goToError()
    pToken += sizeof("Fix Session Status =");
	if (!pToken)
        __goToError()
	SYSLOG_DEBUG("Fix Session Status = %s", pToken);
    if (strncmp(pToken, "SUCCESS", 7) == 0) {
        *result = 1;
    } else if (strncmp(pToken, "FAIL", 4) == 0) {
        *result = 0;
    } else {
        __goToError()
    }
    __free(response);
	return 0;

error:
    __free(response);
    return -1;
}

double convert_degree_sec_to_nmea(int degree,int minute,double sec)
{
	return degree*100 + minute + (sec/60);
}

static int sierra_get_gps_location_fix_result(void)
{
	int stat = 0, fOK, i, j;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char *pToken, *pSavePtr, *pATRes;
	int tint[10];
	float tfloat[10];
	char tstr[10][16];
	char value[32];
	__goToErrorIfFalse(response)

	double nmea_latitude;
	double nmea_longitude;

	stat = at_send("AT!GPSLOC?", response, "", &fOK, AT_RESPONSE_MAX_SIZE);
	//SYSLOG_ERR("response='%s'", response);
	if (stat < 0 || !fOK || strcmp(response, "Not Available") == 0) {
		SYSLOG_ERR("'AT!GPSLOC?' command failed");
		__goToError()
	}

#define CHECK_PARSE_ERROR \
    if (!pToken || strlen(pToken) == 0 || strcmp(pToken, "OK") == 0) __goToError()

	/* Lat: 33 Deg 48 Min 4.08 Sec S  (0xFF9FDAC4)
     * Lon: 151 Deg 8 Min 39.10 Sec E  (0x01ADEBD8)
	 * Time: 2012 03 13 1 23:42:30 (GPS)
	 * LocUncAngle: 0.0 deg  LocUncA: 3072.0 m  LocUncP: 3072.0 m  HEPE: 4344.464 m
	 * 2D Fix
	 */

    /* parse latitude */
	pATRes = response;
	pToken = strtok_r(pATRes, "\n", &pSavePtr);
	CHECK_PARSE_ERROR
	if (sscanf(pToken, "%s %d %s %d %s %f %s %s",
	        &tstr[0][0], &tint[0], &tstr[1][0], &tint[1], &tstr[2][0],
	        &tfloat[0], &tstr[3][0], &tstr[4][0]) < 8)
		__goToError()

	/* convert to NMEA (degree minute) */
	nmea_latitude=convert_degree_sec_to_nmea(tint[0], tint[1],tfloat[0]);
	sprintf(value, "%011.6f", nmea_latitude);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_LATI), value);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_LATI_DIR), tstr[4]);

    /* parse longitude */
	pToken = strtok_r(NULL, "\n", &pSavePtr);
	CHECK_PARSE_ERROR
	if (sscanf(pToken, "%s %d %s %d %s %f %s %s",
	        &tstr[0][0], &tint[0], &tstr[1][0], &tint[1], &tstr[2][0],
	        &tfloat[0], &tstr[3][0], &tstr[4][0]) < 8)
		__goToError()


	/* convert to NMEA (degree minute) */
	nmea_longitude=convert_degree_sec_to_nmea(tint[0], tint[1],tfloat[0]);
	sprintf(value, "%012.6f", nmea_longitude);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_LONG), value);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_LONG_DIR), tstr[4]);

    /* parse date & time */
	pToken = strtok_r(NULL, "\n", &pSavePtr);
	CHECK_PARSE_ERROR
	if (sscanf(pToken, "%s %d %d %d %d %s",
	        &tstr[0][0], &tint[0], &tint[1], &tint[2], &tint[3], &tstr[1][0]) < 6)
		__goToError()

    sprintf(value, "%02d%02d%02d", tint[2], tint[1], tint[0] % 100);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_DATE), value);
	(void) memset(value, 0x00, sizeof(value));
	for (i = 0, j = 0; i < strlen(tstr[1]); i++) {
	    if (tstr[1][i] == ':')
	        continue;
	    value[j++] = tstr[1][i];
	}
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_TIME), value);

    /* parse LocUncAngle & LocUncA & LocUncP & HEPE */
	pToken = strtok_r(NULL, "\n", &pSavePtr);
	CHECK_PARSE_ERROR
	if (sscanf(pToken, "%s %f %s  %s %f  %s %s %f %s  %s %f",
	        &tstr[0][0], &tfloat[0], &tstr[1][0], &tstr[2][0], &tfloat[1], &tstr[3][0],
	        &tstr[4][0], &tfloat[2], &tstr[5][0], &tstr[6][0], &tfloat[3]) < 11)
		__goToError()

    sprintf(value, "%f", tfloat[0]);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_LOCUNCANGLE), value);
    sprintf(value, "%f", tfloat[1]);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_LOCUNCA), value);
    sprintf(value, "%f", tfloat[2]);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_LOCUNCP), value);
    sprintf(value, "%f", tfloat[3]);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_HEPE), value);

    /* parse 3D fix */
	pToken = strtok_r(NULL, "\n", &pSavePtr);
	CHECK_PARSE_ERROR
	if (strncmp(pToken, "2D", 2) == 0) {
    	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_3D_FIX), "2");
	} else {
    	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_VAR_3D_FIX), "3");
	}

    __free(response);
	return 0;

error:
	SYSLOG_ERR("failed to parsing GPS location fix result");
    __free(response);
    return -1;
}

static int sierra_gps_start_tracking_mode(void)
{
    int status = -1, retry_cnt = 0;

    while (status < 0 && retry_cnt++ < 5) {
        /* close previous GPS session if any exists */
    	status = SendSierraGpsCommand("AT!GPSEND=0", 0);
    	sleep(1);
        /* GPS tracking parameters are hard-coded at the moment */
    	status = SendSierraGpsCommand("AT!GPSTRACK=1,180,30,1000,1", 1);
	}
	return status;
}

#define AGPS_DEFAULT_TIMEOUT    180
#define STANDALONE_AGPS   1
#define MS_BASED_AGPS     2
#define MS_ASSISTED_AGPS  3
static int agps_timeout = AGPS_DEFAULT_TIMEOUT;
static int sierra_gps_start_agps_mode(void)
{
    int status = -1, retry_cnt = 0;
	char cmdstr[256];
	/* set accurary worst limit */
#ifdef V_CUSTOM_FEATURE_PACK_Vodafone_Global
	sprintf(cmdstr, "AT!GPSFIX=%d,%d,4294967279", MS_BASED_AGPS, agps_timeout);
#else
	sprintf(cmdstr, "AT!GPSFIX=%d,%d,4294967279", MS_ASSISTED_AGPS, agps_timeout);
#endif
    while (status < 0 && retry_cnt++ < 5) {
        /* close previous GPS session if any exists */
    	status = SendSierraGpsCommand("AT!GPSEND=0", 0);
    	sleep(1);
        /* GPS tracking parameters are hard-coded at the moment */
    	status = SendSierraGpsCommand(cmdstr, 1);
	}
	return status;
}

static int sierra_gps_return_to_tracking_mode_with_result(int error)
{
    int status;
    status = sierra_gps_start_tracking_mode();
    if (!error) {
    	SYSLOG_ERR("GPS location fix command succeeded");
    	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS), "[done]");
    } else {
    	SYSLOG_ERR("GPS location fix command failed");
    	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS), "[error]");
    }
    return status;
}

static int agps_disabled = 0;
static void sierra_gps_status_check_on_schedule(void* ref)
{
    int status, result = 0;
   	scheduled_clear("sierra_gps_status_check_on_schedule");
    if (agps_disabled) {
        //SYSLOG_ERR("agps_disabled, skip scheduler");
        return;
    }

    status = sierra_check_gps_status(&result);
    SYSLOG_DEBUG("sierra_check_gps_status = %d, agps_timeout = %d", status, agps_timeout);
    if (status < 0 && agps_timeout-- > 0) {
    	scheduled_func_schedule("sierra_gps_status_check_on_schedule",sierra_gps_status_check_on_schedule,1);
    	return;
    }
    if (status == 0 && result) {
        SYSLOG_ERR("AGPS location fix succeeded, read location fix result");
        status = sierra_get_gps_location_fix_result();
        (void) sierra_gps_return_to_tracking_mode_with_result(status);
        return;
    } else if (agps_timeout == 0) {
        SYSLOG_ERR("AGPS status check timeout, return to tracking mode");
    } else {
        SYSLOG_ERR("AGPS location fix failed, return to tracking mode");
    }
    (void) sierra_gps_return_to_tracking_mode_with_result(-1);
}

static int handleSierraGpsCommand(gps_cmd_enum_type cmd)
{
	int stat = 0, stat1 = 0;
	char timeout[16];

    agps_disabled = 1;
    switch (cmd) {
        case GPS_DISABLE_CMD:
			stat = SendSierraGpsCommand("AT!GPSEND=0", 0);
        	break;

        case GPS_ENABLE_CMD:
            stat = sierra_gps_start_tracking_mode();
        	break;

        case GPS_AGPS_CMD:
            /* close previous GPS session if any exists */
            /* GPS location initiate command parameters are hard-coded at the moment */
       		agps_timeout = AGPS_DEFAULT_TIMEOUT;
        	if (rdb_get_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_TIMEOUT), timeout, sizeof(timeout)) == 0 &&
        	    atoi(timeout) > 0) {
        		agps_timeout = atoi(timeout);
            }
            stat = sierra_gps_start_agps_mode();
			if (stat < 0) {
                stat1 = sierra_gps_start_tracking_mode();
    			stat |= stat1;
    		} else {
    		    /* start scheduler to check GPS status periodically */
            	scheduled_func_schedule("sierra_gps_status_check_on_schedule",sierra_gps_status_check_on_schedule,1);
    		    stat = 0xff;
                agps_disabled = 0;
    		}
        	break;
    }
    return stat;
}

int sierra_handle_command_gps(const struct name_value_t* args)
{
    int result;
	/* bypass if incorrect argument */
	if (!args || !args[0].value) {
		SYSLOG_ERR("expected command, got NULL");
		goto error;
	}

    rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE), "");

	/* process GPS DISABLE command */
	if (strcmp(args[0].value, "disable") == 0) {
		SYSLOG_ERR("Got GPS DISABLE command");
		if (handleSierraGpsCommand(GPS_DISABLE_CMD) < 0)
			goto error;
	}

	/* process GPS ENABLE command */
	else if (strcmp(args[0].value, "enable") == 0) {
		SYSLOG_ERR("Got GPS ENABLE command");
		if (handleSierraGpsCommand(GPS_ENABLE_CMD) < 0)
			goto error;
	}

	/* process AGPS command */
	else if (strcmp(args[0].value, "agps") == 0) {
		SYSLOG_ERR("Got AGPS command");
		result = handleSierraGpsCommand(GPS_AGPS_CMD);
		/* Return without setting command result after starting AGPS here.
		 * The result will be sent later. */
		if (result == 0xff) {
    	    return 0;
    	}
    	else if (result < 0)
			goto error;
	}

	else {
		SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
		goto error;
	}

	SYSLOG_ERR("GPS [%s] command succeeded", args[0].value);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS), "[done]");
	return 0;

error:
	SYSLOG_ERR("GPS [%s] command failed", args[0].value);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS), "[error]");
	return -1;
}
#endif  /* GPS_ON_AT */

////////////////////////////////////////////////////////////////////////////////
static int sierra_detect(const char* manufacture, const char* model_name)
{
	const char* model_names[]={
		"MC8780",
		"MC8790",
		"MC8785V",
		"MC8790V",
		"MC8792V",
		"MC8795V",
		"USB 308",
		"MC8801",
		"MC8705",
		"MC8704",
		"MC7710",
  		"MC7700",
		"MC7804",
		"MC7304",
		"MC7354",
		"AirCard 320U",
	};

	int sierra_module;


	sierra_module=0;

	// search Sierra in manufacture string
	if(strstr(manufacture,"Sierra"))
		sierra_module=1;

	// compare model name
	int i;
	for (i=0;i<sizeof(model_names)/sizeof(const char*);i++)	{
		if(!strcmp(model_names[i],model_name))
			sierra_module=1;
	}

	// check if the module is airprime series
	if( (sierra_airprime=sierra_module &&
					     (!strncmp(model_name,"MC77",4) ||
					      !strncmp(model_name,"MC73",4) ||
					      !strcmp(model_name,"AirCard 320U") ||
					      !strncmp(model_name,"MC88",4))) !=0 ) {
		syslog(LOG_INFO,"sierra airprime card detected - different band selection will be used (module=%s)",model_name);
	}

	/* check if the module is support CDMA */
	if( sierra_module && (!strcmp(model_name,"MC7304") || !strcmp(model_name,"MC7354")) ) {
		sierra_cdma=1;
		syslog(LOG_INFO,"Sierra CDMA modules (module=%s)",model_name);
	}



	return sierra_module;
}

////////////////////////////////////////////////////////////////////////////////
static int sierra_handle_command_nwctrl(const struct name_value_t* args)
{
	if (!args || !args[0].value)
		return -1;

	if (!strcmp(args[0].value, "attach")) {
		// Do not need AT+CGATT	command for sierra module, always return success for this command
		rdb_set_single(rdb_name(RDB_NWCTRLSTATUS, ""), "[done]");
		return 0;
	}

	return -1;
}

const char* rdb_get_value(const char* rdb);
int rdb_set_value(const char* rdb,const char* val);
char* send_atcmd_printf(const char* fmt,...);

static int sierra_cmd_otasp(const struct name_value_t* args)
{
	struct tms buf;
	clock_t clk;
	clock_t per_sec;

	const char* spc;
	int activation;

	char atcmd[1024];
	char resp[AT_RESPONSE_MAX_SIZE];
	int ok=0;
	int stat;

	int i;

	per_sec=sysconf(_SC_CLK_TCK);

	syslog(LOG_INFO,"[oma-dm] command detected - '%s'",args[0].value);

	int do_omadm = !strcmp(args[0].value, "omadm") || !strcmp(args[0].value, "omadm-prl");
	int do_prl = !strcmp(args[0].value, "prl") || !strcmp(args[0].value, "omadm-prl");

	/* module factory reset - including ota-sp */
	if(!strcmp(args[0].value, "rtn")) {

		/* read spc code form rdb */
		spc=rdb_get_value(rdb_name(RDB_CDMA_OTASP_SPC,""));
		if(!*spc) {
			syslog(LOG_ERR,"[oma-dm] MSL code not found");
			rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] MSL code not found");
			goto rtn_error;
		}

		/* build the AT command and send */
		syslog(LOG_ERR,"[oma-dm] send RTN command");
		snprintf(atcmd,sizeof(atcmd),"AT$RTN=%s",spc);
		stat=at_send(atcmd, resp, "", &ok, 0);

		/* Sierra MC7304/MC7354 does not return any result when the command succeeds */
		if(!(stat<0) && !ok) {
			syslog(LOG_ERR,"[oma-dm] RTN factory reset command failed - MSL=%s",spc);
			rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] RTN AT command failed");
			goto rtn_error;
		}

		/* get start clock and per sec clock */
		clk=times(&buf);

		/* startup delay */
		i=0;
		while( times(&buf)-clk < 30*per_sec) {
			i++;
			/* give up execution for notification handler - run notification handler */
			update_heartBeat();
			syslog(LOG_ERR,"[oma-dm] waiting for rtn #%d",i);
			sleep(1);
		}

		/* write succ to rdb */
		syslog(LOG_ERR,"[oma-dm] RTN done");
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[done]");

	rtn_error:
		/* nothing to do */
		;
	}
	/* ota-sp programming procedure */
	else if (do_omadm || do_prl) {

		/* set otasp progress flag */
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_PROGRESS,""),"1");

		syslog(LOG_ERR,"[oma-dm] start OMA-DM procedure - cmd=%s",args[0].value);

		/* get start clock and per sec clock */
		clk=times(&buf);

		/* startup delay */
		i=0;
		while( times(&buf)-clk < 10*per_sec) {
			i++;
			/* give up execution for notification handler - run notification handler */
			update_heartBeat();
			syslog(LOG_ERR,"[oma-dm] waiting for oma-dm (init) #%d",i);
			sleep(1);
		}

		if(do_omadm) {
			/* send profile update command */
			syslog(LOG_ERR,"[oma-dm] initiate profile device configuration update");
			if(!send_atcmd_printf("AT+OMADM=2")) {
				syslog(LOG_ERR,"oma-dm failed to get profile device configuration");
				goto otasp_error;
			}
		}

		if(do_prl) {
			/* send profile update command */
			syslog(LOG_ERR,"[oma-dm] initiate PRL update");
			if(!send_atcmd_printf("AT+PRL=2")) {
				syslog(LOG_ERR,"oma-dm failed to get PRL update");
				goto otasp_error;
			}
		}

#if 0
		/* send profile update command */
		syslog(LOG_ERR,"[oma-dm] initiate FUMO update");
		if(!send_atcmd_printf("AT+FUMO=2")) {
			syslog(LOG_ERR,"oma-dm failed to get FUMO update");
			goto otasp_error;
		}
#endif

		/* get start clock and per sec clock */
		clk=times(&buf);

		/* total ota-sp time period 3 minutes */
		#define OTASP_TOTAL_TIME	(40)

		/* wait for result */
		i=0;
		while( times(&buf)-clk < OTASP_TOTAL_TIME*per_sec) {
			i++;

			/* give up execution for notification handler - run notification handler */
			update_heartBeat();
			syslog(LOG_ERR,"[oma-dm] waiting for oma-dm (fini) #%d",i);
			sleep(1);
		}

		activation=atoi(rdb_get_value(rdb_name("omadm_activated","")));
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),activation?"[done]":"[error]");

		if(do_omadm) {
			/* notify to QMI manager - restart OMA-DM */
			syslog(LOG_ERR,"[oma-dm] notify to QMI manager - restart OMA-DM");
			rdb_set_value(rdb_name("link_profile_ready",""),"0");
			rdb_set_value("link.profile.wwan.pri","");
		}

		/* reset progress flag */
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_PROGRESS,""),"0");

		syslog(LOG_ERR,"[oma-dm] OMA-DM finished");


	otasp_error:
		/* reset progress flag */
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_PROGRESS,""),"0");
	}
	else if (!strcmp(args[0].value, "update-cdma-settings")) {

		const char* nai;
		const char* mdn;
		const char* msid;
		const char* resp;

		/* get parameters */
		spc=strdupa(rdb_get_value(rdb_name(RDB_CDMA_OTASP_SPC,"")));
		nai=strdupa(rdb_get_value(rdb_name(RDB_CDMA_OTASP_NAI,"")));
		mdn=strdupa(rdb_get_value(rdb_name(RDB_CDMA_OTASP_MDN,"")));
		msid=strdupa(rdb_get_value(rdb_name(RDB_CDMA_OTASP_MSID,"")));

		/* get parameters - NAV */
		if(*nai) {
			if(!strcmp(nai,"[blank]"))
				nai="";

			syslog(LOG_NOTICE,"[cdma-update] change NAI (nai=%s)",nai);
			resp=send_atcmd_printf("AT$QCMIPNAI=%s,1",nai);
			if(!resp) {
				syslog(LOG_ERR,"[cdma-update] failed to set NAI");
				rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] failed to set NAI");
				goto update_error;
			}
		}

		/* read spc code form rdb */
		if(!*spc && (*mdn||*msid)) {
			syslog(LOG_ERR,"[cdma-update] MSL code not found");
			rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] MSL code not found");
			goto update_error;
		}

		/* get parameters - MDN */
		if(*msid) {
			if(!strcmp(msid,"[blank]"))
				msid="";

			syslog(LOG_NOTICE,"[cdma-update] change msid (msid=%s)",msid);
			resp=send_atcmd_printf("AT$MSID=%s,%s",spc,msid);
			if(!resp) {
				syslog(LOG_ERR,"[cdma-update] failed to set MSID");
				rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] failed to set MSID");
				goto update_error;
			}
		}

		/* get parameters - MSID */
		if(*mdn) {
			if(!strcmp(mdn,"[blank]"))
				mdn="";

			syslog(LOG_NOTICE,"[cdma-update] change mdn (mdn=%s)",mdn);
			resp=send_atcmd_printf("AT$MSID=%s,%s",spc,mdn);
			if(!resp) {
				syslog(LOG_ERR,"[cdma-update] failed to set MDN");
				rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] failed to set MDN");
				goto update_error;
			}
		}

		/* write succ to rdb */
		syslog(LOG_NOTICE,"[cdma-update] done");
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[done]");
		goto update_fini;

	update_error:
		/* error */

	update_fini:
		/* clear all parameters */
		rdb_set_single(rdb_name(RDB_CDMA_OTASP_SPC,""),"");
		rdb_set_single(rdb_name(RDB_CDMA_OTASP_NAI,""),"");
		rdb_set_single(rdb_name(RDB_CDMA_OTASP_MDN,""),"");
		rdb_set_single(rdb_name(RDB_CDMA_OTASP_MSID,""),"");

	} else {
		syslog(LOG_ERR,"unknown otasp command - %s",args[0].value);
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] unknown command");
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
struct command_t sierra_commands[] =
{
	{ .name = RDB_PHONE_SETUP".command",	.action = sierra_handle_command_phonesetup },
	{ .name = RDB_BANDCMMAND,		.action = sierra_handle_command_band },
	{ .name = RDB_PROFILE_CMD,		.action = sierra_handle_command_profile },
	{ .name = RDB_SIMCMMAND,		.action = sierra_handle_command_sim },
	{ .name = RDB_SIMMEPCMMAND,		.action = sierra_handle_command_sim },
	{ .name=RDB_NWCTRLCOMMAND,		.action = sierra_handle_command_nwctrl},
#ifdef GPS_ON_AT
	{ .name = RDB_GPS_PREFIX".0."RDB_GPS_CMD,.action = sierra_handle_command_gps },
#endif
#ifdef FORCED_REGISTRATION //Vodafone force registration
	{ .name ="vf.force.registration",	.action = sierra_handle_command_forcereg },
#endif
	#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
	{.name = RDB_CDMA_OTASP_CMD,.action=sierra_cmd_otasp},
	#endif
	{0,}
};

////////////////////////////////////////////////////////////////////////////////
struct model_t model_sierra = {
	.name = "sierra",
	.detect = sierra_detect,
	.init = sierra_init,

	.get_status = model_default_get_status,
	.set_status = sierra_set_status,
	.commands = sierra_commands,
	.notifications = NULL
};

////////////////////////////////////////////////////////////////////////////////
