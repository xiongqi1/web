#define _GNU_SOURCE

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
#include "../sms/sms.h"
#include "../sms/pdu.h"

#include "../dyna.h"
#include "suppl.h"
#include "../util/scheduled.h"

#include <sys/times.h>

#include <fcntl.h>
#include <dirent.h>
#include "../gps.h"

#include <string.h>
#include <stdarg.h>

#ifdef FORCED_REGISTRATION
// Global variable to handle forced registration
extern int forced_registration;
#endif

int custom_roaming_algorithm=0;
int automatic_operator_setting=1;  // 1 -> Automatic, 0 -> Manual
#define PLMN_MANUAL_MODE "plmn_manual_mode"

#define STRLEN(str)		(sizeof(str)-1)
#define GPSONE_BOOTUP_DELAY	(30)

#ifdef GPS_ON_AT
#include <curl/curl.h>
#endif

//#define DEBUG
			 
//#define CINTERION_DEBUG_MODE

/* extern functions */
extern int _convATOpListToCNSOpList(const char* szAtOpers, char* achBuf, int cbBuf);
extern int reset_blacklist();
extern int do_custom_manual_roaming(int quiet);
extern void check_or_update_pin_counter(int sim_exist);
extern char *strip_quotes (char *string);
extern int handle_ussd_notification(const char* s);
extern time_t convert_utc_to_local(struct tm* tm_utc);

/* local functions */
static int cinterion_poll_network_time();
static void cinterion_update_carrier_selection(const char* cops_mode);
static int phs8p_poll_service_type();
static int cinterion_cdma_smoni(void);
static int cinterion_cdma_csq(void);
static char* send_atcmd(const char* atcmd);
static char* skip_prefix(char* p,const char* prefix);
int rdb_set_value(const char* rdb,const char* val);
static int pvs_at_sreg(int* sregs);
static int phs8p_update_service_type_rdb(const char* s);
static int pvs8_at_css();
static int pvs8_update_mip();
const char* rdb_get_value(const char* rdb);
char* send_atcmd_printf(const char* fmt,...);
static int cinterion_handleSetOpListInt(char* cns_network_mode,int store);
int update_cinterion_signal_strength(void);
int update_cinterion_pin_retry_number();
int update_cinterion_firmware_revision();
int update_cinterion_SMS_retransmission_timer(int time);
#ifdef GPS_ON_AT
static int pxs8_poll_gpsone(int rdb_gpsen);
static void pxs8_poll_gpsone_schedule(void* ref);
static int px8_set_gps_engine(int engine_val);
#endif
int handle_creg(const char* s);
int retry_cgatt_if_required_mcc_mnc(int mcc,int mnc);
cinterion_eum_type cinterion_type=cinterion_type_umts;
static char* wait_atcmd(const char* str, int timeout);

/* resp buffer for send_atcmd() and send_atcmd_printf */
static char _resp[AT_RESPONSE_MAX_SIZE];

//////////////////////////////////////////////////////////////////////////////////
#define __countOf(x)		( sizeof(x)/sizeof((x)[0]) )
#define __forEach(i,p,a)	for(i=0,p=&(a)[i];i<__countOf(a);p=&(a)[++i])
#define __pointTypeOf(i)	typeof(i)*

struct {
	char* szBand;
	unsigned int uCode;
} bandConvTbl[] = {
	{"All bands",	511},
	{"GSM All",		15},
	{"WCDMA All",	496},

	{"GSM 850",		4},
	{"GSM 900",		1},
	{"GSM 1800",	2},
	{"GSM 1900",	8},

	{"WCDMA 850",	64},
	{"WCDMA 900",	128},
	{"WCDMA 800",	256},
	{"WCDMA 1900",	32},
	{"WCDMA 2100",	16},
};

///////////////////////////////////////////////////////////////////////////////
typedef struct OPERATOR_NAME {
	int         numericName;
	const char* longName;
} OPERATOR_NAME;

static const OPERATOR_NAME s_operatorNamesInternal[] =
{
#include "cinterion-network-names.lst"
};
#define NUM_OPERATORNAMES_INTERNAL (sizeof(s_operatorNamesInternal) / sizeof(s_operatorNamesInternal[0]))

static int compareOperatorNames(const void* elem1, const void* elem2)
{
	int result = 1;
	int numericName1 = ((OPERATOR_NAME*)elem1)->numericName;
	int numericName2 = ((OPERATOR_NAME*)elem2)->numericName;

	if (numericName1 < numericName2) {
		result = -1;
	}
	else if (numericName1 == numericName2) {
		result = 0;
	}
	return result;
}

const char * _getOperatorNameFromList(const char * mccmnc_pair)
{
	OPERATOR_NAME key, *res;
	//qsort(s_operatorNamesInternal, NUM_OPERATORNAMES_INTERNAL, sizeof(OPERATOR_NAME), compareOperatorNames);
	if (!mccmnc_pair)
		return NULL;

	key.numericName = atoi(mccmnc_pair);
	res = bsearch(&key, s_operatorNamesInternal, NUM_OPERATORNAMES_INTERNAL, sizeof(OPERATOR_NAME), compareOperatorNames);

	if (res == NULL)
		return NULL;

	return res->longName;
}

static int cinterion_update_network_stat(int sync)
{
	int ok = 0;

	int stat;
	char achATRes[2048];
	//char *nameP = NULL;

	char network_name[128]={0,};
	char unencoded_network_name[128]={0,};
	char buf[32];
	const char * namefromList;
	int service_type;

	memset(buf, 0, sizeof(buf));
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
	while (szToken && i < 4) {
		strcpy(resTokens[i++], szToken);
		szToken = _getNextToken();
	}

	// get tokens
	const char* szMCCMNC = resTokens[2];
	const char* cops_mode =  resTokens[0];
	//int nNwType = !strlen(resTokens[3]) ? -1 : atoi(resTokens[3]);

	// store MCC and MNC
	if (_isMCCMNC(szMCCMNC)) {
		int nMCC = _getMCC(szMCCMNC);
		int nMNC = _getMNC(szMCCMNC);

		char achMCC[16];
		sprintf(achMCC, "%d", nMCC);

		char achMNC[16];
		sprintf(achMNC, "%d", nMNC);

		rdb_set_single(rdb_name(RDB_PLMNMCC, ""), achMCC);
		rdb_set_single(rdb_name(RDB_PLMNMNC, ""), achMNC);
	}
	cinterion_update_carrier_selection(cops_mode);

	namefromList = _getOperatorNameFromList(szMCCMNC);
	if (namefromList)
			strncpy(buf, namefromList, sizeof(buf)-1);

	/* replace "Telstra Mobile" to "Telstra" */
	str_replace(&buf[0], "Telstra Mobile", "Telstra");
	/* replace "3Telstra" to "Telstra" */
	str_replace(&buf[0], "3Telstra", "Telstra");

	for( i=0; i<strlen(buf); i++) {
		sprintf( network_name+strlen(network_name), "%%%02x", buf[i] );
		sprintf( unencoded_network_name+strlen(unencoded_network_name), "%c", buf[i] );
	}

	rdb_set_single(rdb_name(RDB_NETWORKNAME, ""), network_name);
	rdb_update_single(rdb_name(RDB_NETWORKNAME, "unencoded"), unencoded_network_name, CREATE, ALL_PERM, 0, 0);

	// send at command - set long operator
	stat = at_send_with_timeout("AT+COPS=3,0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);
	if (stat || !ok)
		return -1;
	service_type = (strlen(resTokens[3]) == 0? -1:atoi(resTokens[3]));
	if (sync) {
		sync_operation_mode(service_type);
	}

	return 0;

error:
	at_send_with_timeout("AT+COPS=3,0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);

	rdb_set_single(rdb_name(RDB_NETWORKNAME, ""), network_name);
	rdb_update_single(rdb_name(RDB_NETWORKNAME, "unencoded"), unencoded_network_name, CREATE, ALL_PERM, 0, 0);

	return -1;
}

extern int send_atcmd_cops_query();

///////////////////////////////////////////////////////////////////////////////
int pxs8_update_phys_cops(int auto_if_not_configured)
{
	char manual_mode[64];
	
	int manual_mode_existing;
	int manual_mode_int;
	int phone_auto_mode;
	
	
	/* get manual mode */
	*manual_mode=0;
	manual_mode_existing=rdb_get_single(PLMN_MANUAL_MODE, manual_mode, sizeof(manual_mode))==0;
	manual_mode_int=atoi(manual_mode);
	#ifdef V_MANUAL_ROAMING_vdfglobal	
	phone_auto_mode=send_atcmd_cops_query()==0;
	#else
	phone_auto_mode=0;
	#endif
	
	syslog(LOG_DEBUG,"[+cops] info - rdb_existing=%d, rdb_mode=%s, module_auto=%d",manual_mode_existing,manual_mode,phone_auto_mode);
	
	/*
		do not run AT+COPS=0 if possible. due to a buggy PHS8-P behaviour which always starts from 2G after the command
	*/
	
	/* set only when manual setting available */
	if(manual_mode_existing && manual_mode_int) {
		if(!auto_if_not_configured) {
			syslog(LOG_ERR,"[+cops] restoring previous carrier selection (mode=%s)", manual_mode);
			cinterion_handleSetOpListInt(manual_mode,0);
		}
	}
	else {
		if(!auto_if_not_configured) {
			syslog(LOG_ERR,"[+cops] no previous carrier selection found");
		}
		else if(phone_auto_mode) {
			syslog(LOG_ERR,"[+cops] no previous carrier selection found - auto-mode detected (mode=%s)",manual_mode);
		}
		else {
			syslog(LOG_ERR,"[+cops] no previous carrier selection found - setting auto-mode (mode=%s)",manual_mode);
			cinterion_handleSetOpListInt(manual_mode,0);
		}
	}
	
	return 0;
}


static int creg=-1;
static int cgreg=-1;

void init_noti_generic(int* stat)
{
	*stat=-1;
}

void init_noti_creg()
{
	init_noti_generic(&creg);
}

void init_noti_cgreg()
{
	init_noti_generic(&cgreg);
}

int wait_for_noti_generic(int sec,int* stat)
{
	int i;
	//int ok;
	
	update_heartBeat();
	
	i=0;
	while((i++<sec) && (*stat!=1) && (*stat!=5)) {
		update_heartBeat();
		syslog(LOG_ERR,"wait for notification #%d/%d",i,sec);
		at_wait_notification(-1);
		
		sleep(1);
	}
	
	return *stat;
}

int wait_for_noti_cgreg(int sec)
{
	return wait_for_noti_generic(sec,&cgreg);
}

int wait_for_noti_creg(int sec)
{
	return wait_for_noti_generic(sec,&creg);
}

static int cinterion_noti_generic(const char* s,const char* prefix)
{
	const char* p;

	/* bypass if it is NULL */
	if(!s)
		goto err;
	
	/* get prefix */
	p=strstr(s,prefix);
	if(!p) {
		syslog(LOG_ERR,"incorrect %s notificaiton formation (s=%s)",prefix,s);
		goto err;
	}
	p+=strlen(prefix);
	
	/* get cgreg */
	return atoi(p);
	
err:
	return -1;
}


static int cinterion_noti_cgreg(const char* s)
{
	syslog(LOG_ERR,"[PS] +CGREG (s=%s)",s);

	cgreg=cinterion_noti_generic(s,"+CGREG: ");
	return 0;
}

static int cinterion_noti_creg(const char* s)
{
	syslog(LOG_INFO,"[PS] +CREG (s=%s)",s);

	creg=cinterion_noti_generic(s,"+CREG: ");

	/* tr069 related job in default creg handler */
	#ifdef V_TR069
	handle_creg(s);
	#endif
	
	return 0;
}

const char* phs8p_get_nv1990(void)
{
	static char mod_nv_1190[AT_RESPONSE_MAX_SIZE];

	char* s;
	char* p;

	/* read stored nv1190 from PHS8-P */
	send_atcmd_printf("AT^SBNR=\"nv\",1190,2");
	if(!(s=strstr(_resp,"^SBNR:"))) {
		syslog(LOG_ERR,"[sbnw] failed to read NV1190");
		goto err;
	}
	s+=STRLEN("^SBNR:");

	/* extract value from resp */
	p=mod_nv_1190;
	while(*s && *s!='\n')
		*p++=*s++;
	*p=0;

	return mod_nv_1190;
	
err:
	*mod_nv_1190=0;
	return mod_nv_1190;
}

int phs8p_fix_band_priority(void)
{
	const char* rdb_nv_1190;
	const char* mod_nv_1190;

	char* resp;

	char nvdata[]="000b63059829d396abf774";
	int nvdata_len;
	int written;

	/* read stored nv1190 from RDB */
	rdb_nv_1190=strdupa(rdb_get_value("system.module.nvcfg"));
	
	/*
		AT^SBNR="nv",1190,2
		^SBNR:001BD143457681F1998FF9AC50FDBCE24B25D2C052B0952A5E0071

		OK
	*/
	mod_nv_1190=phs8p_get_nv1990();

		
	/* start NV procedure if not matching */
	if(!*rdb_nv_1190 || strcmp(rdb_nv_1190,mod_nv_1190)) {
		/*
			AT^SBNW="nv",2
			CONNECT (HEXMODE)

			NV READY: SEND FILE ...
			000b63059829d396abf774
			000b63059829d396abf774

			NV END OK

			OK
		*/
		
		/* write NV */
		send_atcmd_printf("AT^SBNW=\"nv\",2");
		if(!strstr(_resp,"NV READY: SEND FILE ...")) {
			syslog(LOG_ERR,"[sbnw] failed to initiate NV write");
			goto err;
		}

		/* write NV data */
		nvdata_len=strlen(nvdata);
		written=at_write_raw(nvdata,nvdata_len);
		if(nvdata_len!=written) {
			syslog(LOG_ERR,"[sbnw] incorrect written size (written=%d,nvdata_len=%d)",written,nvdata_len);
			goto err;
		}

		/* wait */
		resp=wait_atcmd("NV END OK",10);
		if(!resp) {
			syslog(LOG_ERR,"[sbnw] no reponse from NV1190");
			goto err;
		}

		/* read nv */
		mod_nv_1190=phs8p_get_nv1990();
		if(!*mod_nv_1190) {
			syslog(LOG_ERR,"[sbnw] failed to read NV1990");
			goto err;
		}

		/* store NV1990 to RDB */
		rdb_set_single("system.module.nvcfg", mod_nv_1190);

		syslog(LOG_ERR,"[sbnw] new NV config is written (nv=%s)",mod_nv_1190);
	}

	return 0;
	
err:
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int phs8_init(void)
{
	char buf[512];
	int i;
	__pointTypeOf(bandConvTbl[0]) pConvTbl;
	*buf=0;
	__forEach(i, pConvTbl, bandConvTbl) {
		if(*buf)
			sprintf(buf+strlen(buf),"&%u,%s", pConvTbl->uCode, pConvTbl->szBand);
		else
			sprintf(buf,"%u,%s", pConvTbl->uCode, pConvTbl->szBand);
	}
	rdb_set_single(rdb_name(RDB_MODULEBANDLIST, ""), buf);

	/* fix band priority */
	phs8p_fix_band_priority();
	
	/* set LED mode : Status pin is meaningful only in NTC-1101. NC in other variants
	 *
	 * 		PHS8-P Status 			<mode>=1	<mode>=1 	  		<mode>=2
	 * 											<flash>= default	<flash>= user defined
	 * -----------------------------------------------------------------------------
	 *  call in progress or			on			10 ms on			<flash> ms on
	 *  established								990 ms off			990 ms off
	 * -----------------------------------------------------------------------------
	 *  data transfer				on			10 ms on			<flash> ms on
	 * 											1990 ms off			1990 ms off
	 * -----------------------------------------------------------------------------
	 * registered, No call,			on			10 ms on			<flash> ms on
	 * no data transfer							3990 ms off			3990 ms off
	 * -----------------------------------------------------------------------------
	 * Limited Network Service		off			500 ms on			500 ms on
	 * no SIM, no PIN or						500 ms off			500 ms off
	 * during network search
	 */
	send_atcmd("AT^SLED=1");

	/* clear all previous profiles */
	send_atcmd("AT+CGDCONT=1");
	send_atcmd("AT+CGDCONT=2");
	send_atcmd("AT+CGDCONT=3");
	send_atcmd("AT+CGDCONT=4");
	/* setup two default profiles */
	send_atcmd("AT+CGDCONT=1,\"IP\"");
	send_atcmd("AT+CGDCONT=2,\"IP\"");
	
	at_send("AT+CREG=2", NULL, "", &i, 0);
	at_send("AT+CGREG=1", NULL, "", &i, 0);

	at_send("AT^SIND=\"rssi\",1", NULL, "", &i, 0);

	/* update revision */
	update_cinterion_firmware_revision();

	/* subscribe service type */
	phs8p_poll_service_type();

	/* subscribe network time */
	cinterion_poll_network_time();

	#ifdef GPS_ON_AT
	/* show the rdb that the module supports gpsOne */
	rdb_set_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_CAP),"1");
	/* start schedule of gpsOne */
	pxs8_poll_gpsone_schedule(NULL);
	#endif	
	
	/* set network controllable module */
	rdb_set_value(rdb_name(RDB_NWCTRLVALID,""),"1");
	
	/* 
		set the previous +COPS setting to module
		## work-around for buggy PHS8 that does not even remember a previous manual setting of+COPS command
	*/
	{
		static int init=0;
		
		
		if(!init) {
			pxs8_update_phys_cops(0);
		}
		
		init=1;
	}
	
	/* set ecn0 valid flag - phs8 is using ec/no instead of ec/io */
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ECN0_valid", ""), "1");

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int bgs2e_init(void)
{
	int st;

	/* set M20 compatibility mode
	* 	- voice call : respond with "BUSY TONE", "NO CARRIER" etcs for failure
	* 	- sms : respond with +CME ERROR for failure
	*/
	send_atcmd("AT^SM20=1,0");

	/* common event reporing configuration : enable +CIEV notification */
	send_atcmd("AT+CMER=1,0,0,2,0");

	/* UART setting */
	send_atcmd("AT\\Q3");	/* RTS/CTS f/c */
	send_atcmd("AT&D0");	/* DTR disable */

	/* clear all previous profiles */
	send_atcmd("AT+CGDCONT=1");
	send_atcmd("AT+CGDCONT=2");

	/* assign status pin to LED function */
	send_atcmd("AT^SSYNC=1");	/* set to LED mode */
	send_atcmd("AT^SPIO=0");	/* close GPIO driver */

	/* setup two default profiles */
	send_atcmd("AT+CGDCONT=1,\"IP\"");
	send_atcmd("AT+CGDCONT=2,\"IP\"");

	at_send("AT+CREG=2", NULL, "", &st, 0);
	at_send("AT+CGREG=1", NULL, "", &st, 0);

	at_send("AT^SIND=\"rssi\",1", NULL, "", &st, 0);

	#ifdef V_PHS8P_NETWORK_PRIORITY_FIXUP_y
	/* update revision */
	update_cinterion_firmware_revision();
	#endif

	/* subscribe network time */
	cinterion_poll_network_time();

	/* set network controllable module */
	rdb_set_value(rdb_name(RDB_NWCTRLVALID,""),"1");

	/* 
		set the previous +COPS setting to module
		## work-around for buggy PHS8 that does not even remember a previous manual setting of+COPS command
	*/
	{
		static int init=0;
		if(!init) {
			pxs8_update_phys_cops(0);
		}
		init=1;
	}

	/* set band list */
	rdb_set_single(rdb_name(RDB_MODULEBANDLIST, ""), "511,All bands&");

	/* set ecn0 valid flag - phs8 is using ec/no instead of ec/io */
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ECN0_valid", ""), "1");

	/* fixed service_types */
	rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "GSM");
	rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), "GSM");

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int pvs8_init(void);
static int cinterion_init(void)
{
	if (cinterion_type == cinterion_type_umts) {
		return phs8_init();
	} else if (cinterion_type == cinterion_type_2g) {
		return bgs2e_init();
	} else if (cinterion_type == cinterion_type_cdma) {
		return pvs8_init();
	} else {
		return -1;
	}
}
///////////////////////////////////////////////////////////////////////////////

static int model_cinterion_detect(const char* manufacture, const char* model_name)
{
	/* utms model names */
	const char* model_names_umts[]= {"PHS8-P","PHS8-J",NULL};
	/* 2G model names */
	const char* model_names_2g[]= {"BGS2-E",NULL};
	/* cdma model names */
	const char* model_names_cdma[]= {"PVS8",NULL};

	const char** model_names[]= {
		model_names_umts, /* cinterion_type_umts */
		model_names_2g,   /* cinterion_type_2g   */
		model_names_cdma, /* cinterion_type_cdma */
		NULL,
	};

	char resp[AT_RESPONSE_MAX_SIZE];
	int ok;
	int j;
	int i;

	int found;
	int init_port;

	const char* stat;

	found=0;

	syslog(LOG_DEBUG,"[cinterion] detection called - manufacture=%s,model=%s",manufacture,model_name);

	/* bypass if not cinterion */
	if(!strstr(manufacture,"Cinterion")) {
		syslog(LOG_DEBUG,"[cinterion] manufacture not matching - %s",manufacture);
		goto fini;
	}

	/* search models and cinterion model type */
	for(j=0; model_names[j]; j++) {
		syslog(LOG_DEBUG,"[cinterion] lookup %s in model type #%d",model_name,j);
		for (i=0; model_names[j][i]; i++) {
			if( (found=!strcmp(model_names[j][i],model_name))!=0 ) {
				cinterion_type=j;
				goto end_loop;
			}
		}
	}

end_loop:
	/* bypass if no model is found */
	if(!found) {
		syslog(LOG_DEBUG,"[cinterion] no cinterion model found - model=%s",model_name);
		goto fini;
	}

	syslog(LOG_DEBUG,"[cinterion] cinterion module detected - manufacture=%s,model=%s,type=%d",manufacture,model_name,cinterion_type);

	// always do the special initial procedure for all cinterion modules
	init_port=1;
	#if 0
	// check those models that we have to do special initial procedure
	init_port=!strcmp(model_name, "PHS8-P") || !strcmp(model_name, "PVS8");
	#endif						      

	// do special initial procedure
	if (init_port) {
		syslog(LOG_DEBUG,"[cinterion] start cinterion initial procedure");

		/* enable auto-attach and enable cgactt */
		if (at_send("at^scfg=\"GPRS/AutoAttach\"", resp, "^SCFG: \"GPRS/AutoAttach\"", &ok, 0) == 0 && ok) {
			stat = resp + strlen("^SCFG: \"GPRS/AutoAttach\",");
			if(strstr(stat, "disabled")) {
				at_send("at^scfg=\"GPRS/AutoAttach\", \"enabled\"", NULL, "", &ok, 0);
				at_send("AT+CGATT=1", NULL, "", &ok, 0);
			}
		}

		/* Cinterion 2G module BGS2-E does not support ^SDPORT command */
		if (cinterion_type != cinterion_type_2g) {
			/* change port mode to 3 (usb mode) */
			if (at_send("AT^SDPORT?", resp, "^SDPORT", &ok, 0) == 0 && ok) {
				stat = resp + strlen("^SDPORT: ");
				if(atoi(stat) != 3) {
					at_send("AT^SDPORT=3", NULL, "", &ok, 0);
					exit(1);
				}
			}
		}
	} else {
		syslog(LOG_DEBUG,"[cinterion] skip cinterion initial procedure");
	}

fini:
	return found;
}

static int phs8p_det(const char* manufacture, const char* model_name)
{
	/* detect only if it is umts */
	return model_cinterion_detect(manufacture,model_name) && (cinterion_type==cinterion_type_umts);
}

static int bgs2e_det(const char* manufacture, const char* model_name)
{
	/* detect only if it is umts */
	return model_cinterion_detect(manufacture,model_name) && (cinterion_type==cinterion_type_2g);
}

static int pvs8_det(const char* manufacture, const char* model_name)
{
	/* detect only if it is umts */
	return model_cinterion_detect(manufacture,model_name) && (cinterion_type==cinterion_type_cdma);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int handleBandGet()
{
	int ok = 0;
	int stat;
	char* pos;
	char achATRes[AT_RESPONSE_MAX_SIZE];

	stat = at_send("AT^SCFG=\"Radio/Band\"", achATRes, "", &ok, 0);

	// bypass if error
	if (stat || !ok)
		goto error;

	pos=strstr(achATRes, "Radio/Band\",\"");
	if(!pos)
		goto error;

	int i;
	__pointTypeOf(bandConvTbl[0]) pConvTbl;
	__forEach(i, pConvTbl, bandConvTbl) {
		if(atoi(pos+strlen("Radio/Band\",\""))==pConvTbl->uCode) {
			rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), pConvTbl->szBand);
			return 0;
		}
	}

error:
	return -1;
}

static int handleBandSet()
{
	char achBandParam[128];

	// send at command
	int ok;
	int stat;
	char achATRes[AT_RESPONSE_MAX_SIZE];

	// build at command
	char achATCmd[128];

#ifdef V_MANUAL_ROAMING_vdfglobal
	// to store current RAT
	char resTokens[4][64] = {{0, }, };
	int cops_mode = 0, cops_format = 0, cops_opname = 0, cops_rat = 0;
	int band = 0;
	int restore_cops = 0;
#endif

	// get band param
	if (rdb_get_single(rdb_name(RDB_BANDPARAM, ""), achBandParam, sizeof(achBandParam)) != 0)
		goto error;

	// bypass if no parameter is given
	if(!achBandParam[0])
		goto error;

	// This workaround is applied due to limitation of Cinterion module function. 
	// (If the settings of <rba> and <rat> are incompatible all bands supported by PHS8-P will be enabled after power-up)
	// To get more details, refer to [AT^SCFG="Radio/Band"] command of Cinterion AT Command reference.
#ifdef V_MANUAL_ROAMING_vdfglobal
	if(custom_roaming_algorithm) {

		// send at command - set numeric operator
		stat = at_send_with_timeout("AT+COPS=3,2", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);
		if (stat || !ok)
			goto error;

		// send at command - get current operator
		stat = at_send_with_timeout("AT+COPS?", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);
		if (stat || !ok)
			goto error;

		// +COPS: 0,2,"50503",2
		const char* szToken;

		// convert tokens into the array
		int i = 0;
		szToken = _getFirstToken(achATRes + strlen("+COPS: "), ",\"()");
		while (szToken && i < 4) {
			strcpy(resTokens[i++], szToken);
			szToken = _getNextToken();
		}

		cops_mode = strtol(resTokens[0],0,10);
		cops_format = strtol(resTokens[1],0,10);
		cops_rat = strtol(resTokens[3],0,10);
		band = strtol(achBandParam,0,10);
		if (cops_mode == 1) {
			if (band == 511 || (cops_rat == 0 && !(band & 0x000F)) || (cops_rat == 2 && !(band & 0x01F0))) {
				restore_cops = 1;
			}
		}

		if (restore_cops == 1) {
			sprintf(achATCmd, "AT+COPS=0");
			stat = at_send(achATCmd, achATRes, "", &ok, 0);
			if (stat && !ok)
				goto error;
		}
	}
#endif

	sprintf(achATCmd, "AT^SCFG=\"Radio/Band\",%s,1", achBandParam);
	stat = at_send(achATCmd, achATRes, "", &ok, 0);
	if (stat && !ok)
		goto error;

#ifdef V_MANUAL_ROAMING_vdfglobal
	if (custom_roaming_algorithm && restore_cops == 1) {
		int new_rat = 0;
		if (band == 511) {
			if (rdb_get_single("manualroam.2g3gpreference", achBandParam, sizeof(achBandParam)) != 0) {
				new_rat = 2; //WCDMA
			}
			else {
				if (strtol(achBandParam,0,10) == 0)
					new_rat = 0; //GSM
				else
					new_rat = 2; //WCDMA
			}
		}
		else {
			new_rat = cops_rat==0 ? 2:0;
		}
		sleep(3);
		sprintf(achATCmd, "AT+COPS=%d,%d,%s,%d", cops_mode, cops_format, resTokens[2], new_rat);
		stat = at_send(achATCmd, achATRes, "", &ok, 0);
		if (stat && !ok)
			goto error;
	}
#endif

	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
	return 0;

error:
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");

	return -1;
}

static int pvs8_cmd_mip(const struct name_value_t* args)
{
	int mode;
	char* mip_mode;
	
	/* bypass incorrect or empty cmd */
	if(!args || !args[0].value) {
		return -1;
	}

	if(0) {
	}
	/* get command does not exist - it is part of periodic update */
#if 0
	else if (!strcmp(args[0].value, "get")) {
	}
#endif
	else if (!strcmp(args[0].value, "set")) {
						
		/* get mip mode from rdb */						
		mip_mode=strdupa(rdb_get_value(rdb_name(RDB_CDMA_MIPCMDSETDATA,"")));
		
		/* convert to numeric parameter */
		if(!strcmp(mip_mode,"sip")) {
			mode=0;
		}
		else if(!strcmp(mip_mode,"mip/sip")) {
			mode=1;
		}
		else if(!strcmp(mip_mode,"mip")) {
			mode=2;
		}
		else {
			syslog(LOG_ERR,"incorrect mip parameter - '%s'",mip_mode);
			goto err;
		}
		
		/* send at command */
		if(!send_atcmd_printf("AT$QCMIP=%d",mode)) {
			syslog(LOG_ERR,"[pvs8] failed to change MIP mode - mode=%d",mode);
			goto err;
		}
		
		/* update new status */
		pvs8_update_mip();
				
		/* set succ */
		rdb_set_single(rdb_name(RDB_CDMA_MIPCMDSTATUS, ""), "[done]");
	}

	return 0;
err:
	rdb_set_value(rdb_name(RDB_CDMA_MIPCMDSTATUS, ""), "[error]");
	return -1;
}

static int pxs8_cmd_qxdm(const struct name_value_t* args,const char* cmd)
{
	char spc[128];
	char atcmd[256];
	int stat;
	int ok;

	stat=-1;

	if (!args || !args[0].value)
		return -1;


	if (!strcmp(args[0].value, "unlock")) {
		// get band param
		if (rdb_get_single(rdb_name(RDB_QXDM_SPC, ""), spc, sizeof(spc)) != 0) {
			syslog(LOG_ERR,"failed to read rdb %s - %s",RDB_QXDM_SPC,strerror(errno));
			goto err;
		}

		// build and send at command
		snprintf(atcmd,sizeof(atcmd),"AT^SCFG=\"security/passwd\",\"%s\",\"%s\"",cmd,spc);
		if( at_send(atcmd, NULL, "", &ok, 0)>=0 && ok)
			stat=0;
	}

	if(stat<0)
		goto err;

	rdb_set_single(rdb_name(RDB_QXDM_STAT, ""), "[done]");
	return 0;
err:
	rdb_set_single(rdb_name(RDB_QXDM_STAT, ""), "[error]");
	return -1;
}

#ifdef FORCED_REGISTRATION //Vodafone force registration
static int cinterion_handle_command_force_reg(const struct name_value_t* args)
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

static int phs8_cmd_qxdm(const struct name_value_t* args)
{
	return pxs8_cmd_qxdm(args,"spc");
}

static int pvs8_cmd_qxdm(const struct name_value_t* args)
{
	return pxs8_cmd_qxdm(args,"mspc");
}

static int phs8p_cmd_band(const struct name_value_t* args)
{
	if (!args || !args[0].value)
		return -1;

	if (!strcmp(args[0].value, "get")) {
		if(handleBandGet()==-1)
			rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[error]");
		else
			rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
		return 0;
	} else if (!strcmp(args[0].value, "set"))
		return handleBandSet();

	return -1;
}

static int bgs2e_cmd_band(const struct name_value_t* args)
{
	if (!args || !args[0].value)
		return -1;

	/* BGS2-E 2G module has no option to change band. */
	if (!strcmp(args[0].value, "get")) {
		rdb_set_single(rdb_name(RDB_BANDCURSEL, ""), "All bands");
	} else if (strcmp(args[0].value, "set")) {
		return -1;
	}
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "[done]");
	return 0;
}

static int cinterion_handle_command_sim(const struct name_value_t* args)
{
	default_handle_command_sim(args);
	update_cinterion_pin_retry_number(); //update retries remaining

	return 0;
}
static int pvs8_update_service_type_rdb(const char* resp)
{
/*
	11	CDMA 1xRTT available in currently used cell
	12	CDMA 1xEVDO Release 0 available in currently used cell
	13	CDMA 1xEVDO Release A available in currently used cell
	
*/	

	int cdma;
	const char* cdma_service_type;
	char* p;
	char* s;
	
	const char* service_types[]={
		"",
		"CDMA 1xRTT",	/* 11 */
  		"CDMA 1xEVDO Release 0", /* 12 */
    		"CDMA 1xEVDO Release A" /* 13 */
	};
	
	
	/* copy msg to writable memory */
	s=strdupa(resp);
	
	// check validation
	if(!(p=skip_prefix(s,"+CIEV: psinfo,"))) {
		if(!(p=skip_prefix(s,"^SIND: psinfo,1,"))) {
			if(!(p=skip_prefix(s,"^SIND: psinfo,0,"))) {
				syslog(LOG_ERR,"psinfo incorrect format - %s",resp);
				goto err;
			}
		}
	}
	
	cdma=atoi(p);
	
	/* check validation */
	if((cdma<11 || 13<cdma) && cdma!=0) {
		syslog(LOG_ERR,"unknown cdma packet switched status - %s",s);
		goto err;
	}
	
	syslog(LOG_DEBUG,"[pvs8] cdma service type =%d",cdma);
	
	/* set service types */
	cdma_service_type=service_types[(cdma==0)?0:cdma-10];
	rdb_set_value(rdb_name(RDB_SERVICETYPE, ""), cdma_service_type);
	rdb_set_value(rdb_name(RDB_PLMNSYSMODE, ""), cdma_service_type);
	
	
	return 0;
err:
	return -1;	
}

static int phs8p_update_service_type_rdb(const char* s)
{
	const char* p;
	int service_type;

	const char* service_types[] = {
		"GSM", /* 0 GPRS/EGPRS not available in currently used cell */
		"GPRS", /* 1 GPRS available in currently used cell */
		"GPRS", /* 2 GPRS attached */
		"EGPRS", /* 3 EGPRS available in currently used cell */
		"EGPRS", /* 4 EGPRS attached */
		"UMTS",  /* 5 WCDMA PS attached */
		"UMTS", /* 6 camped on HSDPA capable cell */
		"HSDPA", /* 7 PS attached in HSDPA capable cell */
		"HSDPA", /* 8 PS attached in HSDPA capable cell */
		"HSDPA/HSUPA", /* 9 camped on HSDPA/HSUPA capable cell */
		"HSDPA/HSUPA", /* 10 PS attached in HSDPA/HSUPA capable cell */
	};

	// check validation
	if( (p=strstr(s,"+CIEV: psinfo,"))!=NULL ) {
		p+=strlen("+CIEV: psinfo,");
	} else if((p=strstr(s,"^SIND: psinfo,1,"))!=NULL) {
		p+=strlen("^SIND: psinfo,1,");
	} else if((p=strstr(s,"^SIND: psinfo,0,"))!=NULL) {
		p+=strlen("^SIND: psinfo,0,");
	} else {
		syslog(LOG_ERR,"psinfo incorrect format - %s",s);
		goto err;
	}

	// get service type
	service_type=atoi(p);

	//syslog(LOG_ERR,"service poll = %d",service_type);

	// check range
	if(service_type<0 || 10<service_type) {
		syslog(LOG_ERR,"psinfo incorrect service type - %s",s);
		goto err;
	}

	// service_types
	rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), service_types[service_type]);
	rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), service_types[service_type]);

	return 0;
err:
	return -1;
}

static int pvs8_noti_ciev_psinfo(const char* s)
{
	syslog(LOG_DEBUG,"[pvs8] psinfo noti detected - %s",s);
	
	/* update rdb variable */
	pvs8_update_service_type_rdb(s);
	
	return 0;
}

static int pvs8_update_pdp_status_rdb(const char* resp)
{
/*
<callState>
   0	Packet data service is in the Inactive State
   1	Packet data service is in the Active State, and the call control function is in the
   	Initialization/Idle State
   2	Packet data service is in the Active State, and the call control function is in the
   	Initialization/Traffic State
   3	Packet data service is in the Active State, the call control function is in the Connected State,
   	and the packet data service option is using primary traffic
   4	Packet data service is in the Active State, the call control function is in the Connected State, 
   	and the packet data service option is using secondary traffic
   5	Packet data service is in the Active State, and the call control function is in the
   	Dormant/Idle State
   6	Packet data service is in the Active State, and the call control function is in the
   	Dormant/Traffic State
   7	Packet data service is in the Active State, and the call control function is in the
   	Reconnect/Idle State
   8	Packet data service is in the Active State, and the call control function is in the
   	Reconnect/Traffic State	
*/	

	int callState;
	char* p;
	char* s;
	
	/* copy msg to writable memory */
	s=strdupa(resp);
	
	// check validation
	if(!(p=skip_prefix(s,"+CPSR:"))) {
		syslog(LOG_ERR,"+CPSR incorrect format - %s",resp);
		goto err;
	}
	
	callState=atoi(p);
	
	/* check validation */
	if(callState<0 || callState>8) {
		syslog(LOG_ERR,"unknown packet call status - %s",s);
		goto err;
	}
	
	syslog(LOG_DEBUG,"[pvs8] cdma packet call status =%d",callState);
	
	rdb_set_single(rdb_name(RDB_PDP0STAT, ""), callState==0?"down":"up");
	
	return 0;
err:
	rdb_set_single(rdb_name(RDB_PDP0STAT, ""), "");
	return -1;	
}

static int pvs8_noti_cpsr(const char* s)
{
	syslog(LOG_DEBUG,"[pvs8] cpsr noti detected - %s",s);
	
	/* update rdb variable */
	pvs8_update_pdp_status_rdb(s);
	
	return 0;
}

static int pvs8_poll_service_type()
{
	char* resp;
	
/*
	AT^SIND="psinfo",1
	^SIND: psinfo,1,13
	
	OK
*/	

#ifdef CINTERION_DEBUG_MODE
	syslog(LOG_ERR,"!!!!!!!!!!!! WARNING !!!!!!!!!!!!");
	resp="^SIND: psinfo,1,13";
	syslog(LOG_ERR,"debug mode - using fake sind (%s)",resp);
#else
	
	/* subscribe psinfo */
	if(!(resp=send_atcmd("AT^SIND=\"psinfo\",1"))) {
		syslog(LOG_ERR,"[pvs8] failed to subscribe psinfo notification");
		goto err;
	}
#endif	
	
	/* update rdb */
	if(pvs8_update_service_type_rdb(resp)<0)
		goto err;
	
	return 0;
err:
	return -1;	
}

static int phs8p_poll_service_type()
{
	char atresp[128];
	int atok;
	char* atcmd;

	// send AT command
	atcmd="AT^SIND=\"psinfo\",1";
	if(at_send(atcmd, atresp, "", &atok, 0)<0) {
		syslog(LOG_ERR,"AT command failure - %s",atcmd);
		goto err;
	}

	// check AT OK status
	if(!atok) {
		syslog(LOG_ERR,"AT command ERROR reply - %s",atcmd);
		goto err;
	}

	//syslog(LOG_ERR,"service poll = atresp");

	return phs8p_update_service_type_rdb(atresp);

err:
	return -1;
}

static const char *plmn_mode_name[5] = {"Automatic", "Manual", "De-register", "Set Only", "manual/automatic" };

static void cinterion_update_carrier_selection(const char* cops_mode)
{
	char manual_mode[64];
	int plmn_mode;
	char buf[128]={0, };

	// [roaming] use PLMN_MANUAL_MODE rdb variable if custom roaming algorithm is used
	if(custom_roaming_algorithm) {

		if (rdb_get_single(PLMN_MANUAL_MODE, manual_mode, sizeof(manual_mode)) != 0)
			manual_mode[0]=0;

		if(atoi(manual_mode))
			cops_mode="1";
		else
			cops_mode="0";

		syslog(LOG_DEBUG,"[roaming] use router plmn manual mode (cops_mode=%s)", cops_mode);
	}

	rdb_set_single(rdb_name(RDB_PLMNMODE, ""), cops_mode);
	plmn_mode = atoi(cops_mode);
	
	// Workaround for nmc1000
	// There is very highly chance of "De-register" mode in EVT2 mice B/D but
	// couldn't find the root cause so applying this quick n dirty workaround.
	// If detect "De-register" mode for the first time after boot-up then reboot the module.
	if(rdb_get_single(rdb_name("plmn_mode_validity_checked", ""), buf, sizeof(buf)) >= 0 &&
	   buf[0] == '1') {
		if (plmn_mode == 2) {
			syslog(LOG_ERR,"plmn mode = De-register, reboot the module to recover");
			system("reboot_module.sh");
			return;
		}
	}
	
	if (plmn_mode < 5) {
		rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""), plmn_mode_name[plmn_mode]);
	} else {
		rdb_set_single(rdb_name(RDB_PLMNSELMODE, ""),"");
	}

	rdb_update_single(rdb_name("plmn_mode_validity_checked", ""), "1", CREATE, ALL_PERM, 0, 0);
}


int cinterion_network_name(void)
{
	static const char* service_types[] = { "GSM", "GSM Compact", "UMTS", "EGPRS", "HSDPA", "HSUPA", "HSDPA/HSUPA", "E-UMTS" };
	int i;
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char buf[32];
	char network_name[128];
	char unencoded_network_name[128];
	int token_count;
	int service_type = 0;
	const char* t;
	int offset;
	int size;
	int stat;
	//char *nameP = NULL;

	stat=-1;

	network_name[0] = 0;
	unencoded_network_name[0] = 0;

	// send at command - set long operator
	/* BGS2-E : 0 - long alphanumeric, 2 - numeric  */
	if (cinterion_type == cinterion_type_2g)
		stat = at_send_with_timeout("AT+COPS=3,0", response, "", &ok, AT_QUICK_COP_TIMEOUT, 0);
	else
		stat = at_send_with_timeout("AT+COPS=3,90", response, "", &ok, AT_QUICK_COP_TIMEOUT, 0);
	if (stat || !ok) {
		goto err;
	}

	if (at_send_with_timeout("AT+COPS?", response, "+COPS", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0 || !ok) {
		goto err;
	}
	
//printf( "AT+COPS? response=%s\n",response);
	token_count = tokenize_at_response(response);
	if (token_count >= 1) {
		t = get_token(response, 1);
		cinterion_update_carrier_selection(t);
	}
	if (token_count >= 4) {
		t = get_token(response, 3);
		offset = *t == '"' ? 1 : 0;
		size = strlen(t) - offset * 2;
		size = size < 63 ? size : 63; // quick and dirty

		/*	memcpy(network_name, t + offset, size);
			network_name[size] = 0;		*/

		memcpy(buf, t + offset, size);
		buf[size] = 0;
		/* replace "Telstra Mobile" to "Telstra" */
		str_replace(&buf[0], "Telstra Mobile", "Telstra");
		/* replace "3Telstra" to "Telstra" */
		str_replace(&buf[0], "3Telstra", "Telstra");
		for( i=0; i<strlen(buf); i++) {
			sprintf( network_name+strlen(network_name), "%%%02x", buf[i] );
			sprintf( unencoded_network_name+strlen(unencoded_network_name), "%c", buf[i] );
		}

		if (token_count > 4) {
			service_type = atoi(get_token(response, 4));
			if (service_type < 0 || service_type >= sizeof(service_types) / sizeof(char*)) {
				SYSLOG_ERR("expected service type from 0 to %d, got %d", sizeof(service_types) / sizeof(char*), service_type);
				service_type = 0;
			}
		}
	}
	
	stat=0;

err:
	rdb_set_single(rdb_name(RDB_NETWORKNAME, ""), network_name);
	rdb_update_single(rdb_name(RDB_NETWORKNAME, "unencoded"), unencoded_network_name, CREATE, ALL_PERM, 0, 0);
	
	//rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), (nameP? nameP:service_types[service_type]));

	return stat;
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

static time_t cinterion_get_real_time_clock()
{
	// wwan.0.modem_date_and_time

	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char* ptr;
	int ok;

	struct tm tm_utc;
	time_t time_network;

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
	ptr+=strlen(PREFIX_CCLK);

	// skip quotation
	if(*ptr++!='"') {
		syslog(LOG_ERR,"unknown AT command result format (%s) - %s",command,response);
		return -1;
	}

	// convert str to struct tm
	if(!(ptr=strptime(ptr,"%y/%m/%d,%H:%M:%S",&tm_utc))) {
		syslog(LOG_ERR,"unknown time format (%s) - %s",ptr,strerror(errno));
		return -1;
	}
	
	// convert to time_t
	time_network=convert_utc_to_local(&tm_utc);

	return time_network;
}

static int cinterion_update_date_and_time()
{
	time_t time_now;
	time_t time_network;

	char rdb_buff[128];

	time_now=time(NULL);

	/* get real time clock */
	time_network=cinterion_get_real_time_clock();
	if(time_network<0)
		goto err;

	// set date and time rdb
	snprintf(rdb_buff,sizeof(rdb_buff),"%ld",time_network-time_now);
	rdb_set_single(rdb_name(RDB_DATE_AND_TIME, ""), rdb_buff);

	return 0;

err:
	return -1;
}

static int cinterion_set_realtime_clock(const char* resp, char* p)
{
	char* p2;

	char atcmd[128];
	char resp2[128];
	int ok;


	if( *(p+2)!='/' || *(p+5)!='/' || *(p+8)!=',') {
		syslog(LOG_ERR,"incorrect NITZ result format #2 - %s",resp);
		goto err;
	}

	// get time
	if((p2=strchr(p,'"'))==NULL) {
		syslog(LOG_ERR,"incorrect NITZ result format #3 - %s",resp);
		goto err;
	}
	*p2=0;

	// sync
	snprintf(atcmd,sizeof(atcmd),"AT+CCLK=\"%s\"",p);
	if (at_send_with_timeout(atcmd, resp2, "OK", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0 || !ok) {
		syslog(LOG_ERR,"AT+CCLK AT command failed");
		goto err;
	}

	syslog(LOG_INFO,"sync real-time clock - %s", p);

	return 0;
err:
	return -1;
}

static int cinterion_poll_network_time()
{

	char resp[128];
	int ok;

	char* p;
	static int clock_sync=0;

	// bypass if already synchronized
	if(clock_sync)
		return 0;

	// run NITZ command
	if(at_send_with_timeout("AT^SIND=nitz,1", resp, "^SIND: nitz,", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0) {
		syslog(LOG_ERR,"NIZ command timeout");
		goto err;
	}

	if(!ok) {
		syslog(LOG_INFO,"NITZ not supported");
		goto err;
	}

	//^SIND: nitz,1,"13/01/13,19:52:05",+44,1 to "2013.01.13-19:52:05"

	if((p=strstr(resp,"^SIND: nitz,1,\""))==NULL) {
		syslog(LOG_ERR,"incorrect NITZ result format #1 - %s",resp);
		goto err;
	}

	// check validation
	p+=strlen("^SIND: nitz,1,\"");

	if(*p=='"') {
		syslog(LOG_INFO,"NITZ not supported by network");
		goto err;
	}

	/* set real-time clock */
	cinterion_set_realtime_clock(resp,p);

	clock_sync=1;

	return 0;

err:
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
static int cinterion_sevingcell_monitor_smoni(void)
{
	int ok = 0, token_count, i, len, protocol = 0;
	const char * token;
	char response[AT_RESPONSE_MAX_SIZE];
	char * service_type;
	int arfcn = 0;
	int ret = 0;

	/*
		AT^SMONI

		* 2G
		^SMONI: ACT,ARFCN,BCCH,MCC,MNC,LAC,cell,C1,C2,NCC,BCC,GPRS,ARFCN,TS,timAdv,dBm,Q,ChMod

		* 3G
		^SMONI: ACT,UARFCN,PSC,EC/n0,RSCP,MCC,MNC,LAC,cell,SQual,SRxLev,PhysCh,SF,Slot,EC/n0,RSCP,ComMod,HSUPA,HSDPA

		* example
		^SMONI: 3G,4412,96,-6.0,-65,505,01,0151,0CA4D54,36,53,NOCONN
	*/

	if (at_send_with_timeout("AT^SMONI=255", response, "^SMONI:", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0 || !ok) {
		goto error;
	}

	token_count = tokenize_at_response(response);
	if (token_count <= 3)
		goto error;

	token = get_token(response, 1);
	if (!strcmp(token, "2G"))
		protocol = 1;
	else if (!strcmp(token, "3G"))
		protocol = 2;
	else
		goto error;

	char prevRAT[32];
	char * newRAT[2] = {"GSM", "UMTS"};

	if (!rdb_get_single(rdb_name(RDB_NETWORK_STATUS".RAT", ""), prevRAT, sizeof(prevRAT))) {
		if (protocol == 1 || protocol == 2) {
			if(strcmp(newRAT[protocol-1], prevRAT) != 0) {
				rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RAT", ""), newRAT[protocol-1]);
			}

			if ((strlen(prevRAT) != 0) && (strcmp(newRAT[protocol-1], prevRAT) != 0)) {
				rdb_set_single("rat_alarm.trigger", "1");
				SYSLOG_ERR("TR069 RAT change triggered");
			}
		}
	}

////////////////// <Channel Number & Service Type> //////////////////
	ret = 0;
	token = get_token(response, 2);

	len = strlen(token);
	for (i =0 ; i < len; i++) {
		if (!isdigit(token[i])) {
			//possible SIM PIN is locked
			//SYSLOG_ERR("Error: token is not digit");
			ret = 1;
		}
	}

	if (!ret) {
		arfcn = atoi(token);
		service_type = convert_Arfcn2BandType(protocol, arfcn);

		rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), service_type);
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ChannelNumber", ""), token);
	} else {
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ChannelNumber", ""), "");
		rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
	}

////////////////// <Primary Scrambling Code> //////////////////
	if (protocol == 2) {
		token = get_token(response, 3);
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".PSCs0", ""), token);
	} else {
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".PSCs0", ""), "");
	}

////////////////// <Signal Quality> //////////////////
	if (protocol == 2) {
		token = get_token(response, 4);
		if (token[0] == '-') {
			token = token +1;
		}
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ECN0s0", ""), token);
	} else {
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ECN0s0", ""), "");
	}

////////////////// <Location Area Code> //////////////////
	if (protocol == 2) {
		token = get_token(response, 8);
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC", ""), token);
	} else {
		token = get_token(response, 6);
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC", ""), token);
	}

////////////////// <Cell ID> //////////////////
	if (protocol == 2) {
		token = get_token(response, 9);
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), hextodec(token));
	} else {
		token = get_token(response, 7);
		rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), hextodec(token));
	}

////////////////// <HSUPA/HSDPA> //////////////////
	if (protocol == 2) {
		/* currently the the module is use cat 6 for HSUPA and cat 10 for HSDPA fixed */
		rdb_set_single(rdb_name(RDB_HSUCAT, ""), "6");
		rdb_set_single(rdb_name(RDB_HSDCAT, ""), "10");
	} else {
		rdb_set_single(rdb_name(RDB_HSUCAT, ""), "");
		rdb_set_single(rdb_name(RDB_HSDCAT, ""), "");
	}

	return 0;
error:
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ECIOs0", ""), "");
//	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RSCPs0", ""), "");
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".PSCs0", ""), "");
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC", ""), "");
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RAC", ""), "");
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), "");
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ChannelNumber", ""), "");

	rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int cinterion_sevingcell_monitor_moni(void)
{
	int ok = 0, arfcn = 0, i;
	const char *token;
	char *pToken, *pSavePtr, *pSavePtr2, *pATRes, *service_type;
	char response[AT_RESPONSE_MAX_SIZE];

	/*
		AT^MONI
		-------------------------------------------------------------------------------------
		Serving Cell                                        I Dedicated channel
		chann rs dBm MCC MNC LAC cell NCC BCC PWR RXLev C1  I chann TS timAdv PWR dBm Q ChMod
		1013  21 -71 001 01 1001 0103 7   7   33  -105  33 	I No connection
		-------------------------------------------------------------------------------------
		Serving Cell                                        I Dedicated channel
		chann rs dBm MCC MNC LAC cell NCC BCC PWR RXLev C1  I chann TS timAdv PWR dBm Q ChMod
		1013  21 -71 001 01 1001 0103 7   7   33  -105  33 	I Limited Service
		-------------------------------------------------------------------------------------
		Serving Cell                                        I Dedicated channel
		chann rs dBm MCC MNC LAC cell NCC BCC PWR RXLev C1  I chann TS timAdv PWR dBm Q ChMod
		1013  21 -71 001 01 1001 0103 7   7   33  -105  33 	I Cell Reselection
		-------------------------------------------------------------------------------------
		Serving Cell                                        I Dedicated channel
		chann rs dBm MCC MNC LAC cell NCC BCC PWR RXLev C1  I chann TS timAdv PWR dBm Q ChMod
		Searching
		-------------------------------------------------------------------------------------
		Serving Cell                                        I Dedicated channel
		chann rs dBm MCC MNC LAC cell NCC BCC PWR RXLev C1  I chann TS timAdv PWR dBm Q ChMod
		1013  19 -76 001 01 1001 0103 7   7   33  -105  33 	I 1015 1 0 5 -76 0 S_HR
		-------------------------------------------------------------------------------------
	*/

	/* set non-2G variables to default values */
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RAT", ""), "GSM");	// RAT
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".PSCs0", ""), "");	// Primary Scrambling Code
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ECN0s0", ""), "");	// Signal Quality
	rdb_set_single(rdb_name(RDB_HSUCAT, ""), "");					// HSUPA
	rdb_set_single(rdb_name(RDB_HSDCAT, ""), "");					// HSDPA
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RAC", ""), "");		// RAC

	if (at_send_with_timeout("AT^MONI", response, "", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0 || !ok) {
		goto error;
	}

	pATRes = response;
	pToken = strtok_r(pATRes, "\n", &pSavePtr);
	if (!pSavePtr)
		goto error;
	pToken = strtok_r(NULL, "\n", &pSavePtr);
	if (!pSavePtr)
		goto error;
	if (strstr(pSavePtr, "Searching")) {
		goto error;
	}

	////////////////// <Channel Number & Service Type> //////////////////
	token = strtok_r(pSavePtr, " ", &pSavePtr2);
	arfcn = atoi(token);
	service_type = convert_Arfcn2BandType(1, arfcn);
	rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), service_type);
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ChannelNumber", ""), token);
	////////////////// <Location Area Code> //////////////////
	for (i=0;i<5;i++)
		token = strtok_r(NULL, " ", &pSavePtr2);
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC", ""), token);
	////////////////// <Cell ID> //////////////////
	token = strtok_r(NULL, " ", &pSavePtr2);
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), hextodec(token));
	return 0;
error:
	rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".ChannelNumber", ""), "");
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".LAC", ""), "");
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".CellID", ""), "");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int update_cinterion_pin_retry_number()
{
	int ok = 0, stat;
	char* response = __alloc(AT_RESPONSE_MAX_SIZE);
	char* command = __alloc(128);
	char buf[8];
	char* pos;

	__goToErrorIfFalse(response)
	__goToErrorIfFalse(command)

	stat = rdb_get_single(rdb_name(RDB_SIM_STATUS, ""), response, 128);

	/* BGS2-E command format : AT^SPIC=<facility>, it returns
	 * PIN or PUK remaining count depending on current status */
	if (stat >= 0 && strcmp(response, "SIM OK") == 0)
	{
		if(cinterion_type == cinterion_type_2g)
			sprintf(command, "AT^SPIC=\"SC\"");
		else
			sprintf(command, "AT^SPIC=\"SC\",0");
	}
	else if (stat >= 0 && strcmp(response, "SIM PIN") == 0)
	{
		if(cinterion_type == cinterion_type_2g)
			sprintf(command, "AT^SPIC=\"SC\"");
		else
			sprintf(command, "AT^SPIC=\"SC\",0");
	}
	else if (stat >= 0 && strcmp(response, "SIM PUK") == 0)
	{
		if(cinterion_type == cinterion_type_2g)
			sprintf(command, "AT^SPIC=\"SC\"");
		else
			sprintf(command, "AT^SPIC=\"SC\",1");
	}
	else
	{
		sprintf(command, "AT^SPIC");
	}

	// get SIM PIN retry number
	if (at_send(command, response, "^SPIC:", &ok, 0) != 0) {
		rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), "");
		rdb_set_single(rdb_name(RDB_SIMPUKREMAIN, ""), "");
		goto error;
	}
	if (ok) {
		pos=strstr(response, "^SPIC:");
		if(pos) {
			sprintf(buf, "%u", atoi(response+6));
			rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), buf);
			rdb_set_single(rdb_name(RDB_SIMPUKREMAIN, ""), buf);
			goto success;
		}
	}

	rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), "");
	rdb_set_single(rdb_name(RDB_SIMPUKREMAIN, ""), "");

success:
error:
	__free(response);
	__free(command);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
/* 
 * PHS8-P operating temperature table
 * ---------------------------------------------
 * operating temp			-30 ~ +85
 * restricted temp			< -40, > +95
 * 
 * The module shutdowns itself (and rebooting) automatically beyond above
 * restricted temperature range to recover network connection when return to normal
 * operating temperature range but it was observed sometimes it failed to
 * return to normal operation so manual recovery logic is needed by monitoring
 * critical operating temperature.
 */
typedef enum {
	NEG_RESTRICTED_TEMP = -2,
	NEG_ALERT_TEMP = -1,
	NORMAL_TEMP = 0,
	POS_ALERT_TEMP = 1,
	POS_RESTRICTED_TEMP = 2
} UrcCauseEnumType;

int update_cinterion_tr069()
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[128];
	int token_count, voltage = 0;
	const char* token;
	UrcCauseEnumType last_temp_status, curr_temp_status;
	int stat;

	if (!at_send("AT^SCTM?", response, "^SCTM: ", &ok, 0) && ok) {
		token_count = tokenize_at_response(response+sizeof("^SCTM:"));
		if (token_count < 3) {
			at_send("AT^SCTM=0,1", NULL, "", &ok, 0); // to get module temperature
			rdb_set_single(rdb_name(RDB_MODULETEMPERATURE, ""), "");
		} else {
			token = get_token(response, 2);
			if (token == NULL)
				rdb_set_single(rdb_name(RDB_MODULETEMPERATURE, ""), "");
			else
				rdb_set_single(rdb_name(RDB_MODULETEMPERATURE, ""), token);

			stat = rdb_get_single(rdb_name(RDB_CRIT_TEMP_STATUS, ""), buf, 128);
			if (stat >= 0) {
				last_temp_status = (UrcCauseEnumType)atoi(buf);
			} else {
				last_temp_status = NORMAL_TEMP;
			}
			curr_temp_status = atoi(get_token(response, 1));
			SYSLOG_DEBUG("last temp_status = %d, curr_temp_status = %d", last_temp_status, curr_temp_status);
			if (curr_temp_status < NEG_RESTRICTED_TEMP || NEG_RESTRICTED_TEMP > curr_temp_status) {
				SYSLOG_ERR("curr_temp_status %d is out of range, ignore", curr_temp_status);
			} else {
				if (last_temp_status != curr_temp_status) {
					SYSLOG_ERR("critical temp status changed from %d to %d", last_temp_status, curr_temp_status);
					// if return to normal temp from +/- alert or restricted temp, then manually recover connection
					if (curr_temp_status == NORMAL_TEMP) {
						SYSLOG_ERR("manually recover network connection");
						system("/usr/bin/trigger_active_profile.sh");
					}
				}
				sprintf(buf, "%d", curr_temp_status);
				rdb_set_single(rdb_name(RDB_CRIT_TEMP_STATUS, ""), buf);
			}
		}
	} else {
		rdb_set_single(rdb_name(RDB_MODULETEMPERATURE, ""), "");
	}

	if (!at_send("AT^SBV", response, "^SBV:", &ok, 0) && ok) {
		token = response+sizeof("^SBV");
		if (token) {
			voltage = atoi(token);
			sprintf(buf, "%dmV", voltage);
			rdb_set_single(rdb_name(RDB_MODULEVOLTAGE, ""), buf);
		} else {
			rdb_set_single(rdb_name(RDB_MODULEVOLTAGE, ""), "");
		}
	} else {
		rdb_set_single(rdb_name(RDB_MODULEVOLTAGE, ""), "");
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int update_cinterion_signal_strength(void)
{
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	char temp[64];
	int token_count;
	const char* t;
	int dbm;
	int csq;

	int searching=0;

	int rssi=0;
	int rscp=0;
	int use_rscp=0;
	int dpch;

	static int search_count=0;

	// send +CSQ
#ifdef CINTERION_DEBUG_MODE
	syslog(LOG_ERR,"!!!!!!!!!!!! WARNING !!!!!!!!!!!!");
	strcpy(response,"+CSQ: 20, 99");
	syslog(LOG_ERR,"debug mode - using fake csq (%s)",response);
	{
#else
	if (at_send("AT+CSQ", response, "+CSQ", &ok, 0) == 0 && ok) {
#endif
		rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, "raw"), response);
		token_count = tokenize_at_response(response);

		// get RSSI
		if ( (token_count >= 1) && ((t = get_token(response, 1))!=NULL) ) {
			csq = atoi(t);
			rssi=csq < 0 || csq == 99 ? 0 : (csq * 2 - 113);
		}

		// get bers - TODO: typo - the name is supposed to be bers
		if ( (token_count >= 2) && ((t = get_token(response, 2))!=NULL) )
			rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, "bars"), t);
	}

	/* BGS2-E does not support ^SMONI command */
	if (cinterion_type != cinterion_type_2g) {

		// PHP-8 v03.001 firmware has a defect in getting RSCP via +CSQ. Instead of +CSQ, use ^SMONI for 3G

		// send ^SMONI
		if (at_send_with_timeout("AT^SMONI=255", response, "^SMONI:", &ok, AT_QUICK_COP_TIMEOUT, 0) == 0 && ok) {
			// tokenize
			token_count = tokenize_at_response(response);

			// read ACT
			if((token_count>=1) && ((t = get_token(response,1))!=NULL))
				use_rscp=!strcmp(t, "3G");

			// DPCH allocation
			dpch=0;
			if((token_count>=12) && ((t = get_token(response,12))!=NULL))
				dpch=!strcmp(t, "DPCH");

			// read RSCP
			if(token_count>=5 && (t = get_token(response,5))!=NULL) {
				rscp=atoi(t);
			}

			// read dedicated RSCP
			if(dpch) {
				if(token_count>=16 && (t = get_token(response,16))!=NULL) {
					rscp=atoi(t);
				}
			}

			/* do not use rscp if rscp is 0 */
			if(!rscp)
				use_rscp=0;

			// read search or not
			if((token_count>=2) && ((t = get_token(response,2))!=NULL)) {
				searching=!strcmp(t,"SEARCH");
			}
		}

		// ignore the first 3 searching - workaround for blinking LEDs
		if( searching ) {
			if(search_count<3) {
				//syslog(LOG_ERR,"SMONI - ignore");
				search_count++;
				goto fini;
			}
		} else {
			search_count=0;
			//syslog(LOG_ERR,"SMONI - clear");
		}
	}

	// write RSCP
	if(use_rscp) {
		// work-around for buggy RSCP of PHS-8
		if(rscp>=-120)
			snprintf(temp,sizeof(temp),"%d",rscp<0?-rscp:rscp);
		else
			snprintf(temp,sizeof(temp),"%d",rssi<0?-rssi:rssi);
	} else {
		*temp=0;
	}
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".RSCPs0", ""), temp);

	// write signal strength - use RSCP when RSCP is available (3G)
	if(use_rscp && (rscp>=-120) ) {
		dbm=rscp;
	} else {
		dbm=rssi;
	}

	//syslog(LOG_ERR,"rssi=%d,rscp=%d,dbm=%d",rssi,rscp,dbm);

	sprintf(temp, "%ddBm", dbm);
	rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), temp);

fini:
	return 0;
}

static int cinterion_update_roaming_status(void)
{
	const char* szRoaming;
	int nRoaming;

	nRoaming = updateRoamingStatus_method3();

	// put database variable
	if (nRoaming == 1)
		szRoaming = "active";
	else if (nRoaming == 0)
		szRoaming = "deactive";
	else
		szRoaming = "";

	rdb_set_single(rdb_name(RDB_ROAMING, ""), szRoaming);

	return 0;
}

static int cinterion_cdma_update_sim_status(void)
{
	char* p;
	char* resp;

	/* use dummy sim status - not make other parts upset */
	syslog(LOG_DEBUG,"[cinterion] update cdma sim card status - dummy status");
	rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM OK");


	/* get MDN */

	/*
		AT$MDN
		$MDN: 0000005299
	*/

	if(!(p=resp=send_atcmd("AT$MDN?"))) {
		syslog(LOG_ERR,"AT$MDN command failed");
	} else {
		if(!(p=skip_prefix(p,"$MDN: "))) {
			syslog(LOG_ERR,"unknown result from AT$MDN - %s",resp);
		} else {
			syslog(LOG_DEBUG,"[pvs8] mdn='%s'",p);
			rdb_set_value(rdb_name(RDB_MDN,""),p);
		}

		/* check otasp status - valid MDN does not start with 0 */
		rdb_set_value(rdb_name(RDB_MODULE_ACTIVATED,""),(p[0]=='0') && (p[1]=='0') ?"0":"1");
	}

	return 0;
}

#define cinterion_call_func(func_umts,func_cdma) { \
	if(cinterion_type==cinterion_type_umts) {\
		func_umts; \
	} \
	else { \
		func_cdma; \
	} \
}

///////////////////////////////////////////////////////////////////////////////
static int cinterion_update_serial(void)
{
	int ok = 0;
	char response[4096];
	char* p;
	char* sgsn="not available";

	static int tried=0;
	int i;

	if(tried)
		return 0;


	for(i=0;i<3;i++) {
	
		// send AT^SGSN command
		if (at_send("AT^SGSN", response, "", &ok, 0) != 0 || !ok) {
			SYSLOG(LOG_ERR,"AT^SGSN command failed");
			sleep(1);
			continue;
		}

		/* skip spaces */
		p=response;
		while(*p && isspace(*p))
			p++;

		/* get sgsn */
		sgsn=p;

		/* put null termination */
		while(*p && !isspace(*p))
			p++;
		*p=0;

		break;
	}

	tried=1;

	/* update rdb */
	rdb_set_single(rdb_name(RDB_SERIAL, ""), sgsn);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int pvs8_set_stat(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	if(!at_is_open())
		return 0;

	/* cdma specific function calls */
	if(cinterion_type==cinterion_type_cdma) {

		/* AT^SMONI */
		cinterion_cdma_smoni();

		/* sim status */
		cinterion_cdma_update_sim_status();

		/* update serial */
		//cinterion_update_serial();
		
		/* update roaming status */
		cinterion_update_roaming_status();

		/* signal strength */
		cinterion_cdma_csq();

		/* roaming status */
		/* this function is called by pvs8_get_stat() */
		// pvs_at_sreg();

		/* update rdb datetime with real-time clock after real-time clock is sync. */
		cinterion_update_date_and_time();

		/* update network service type */
		pvs8_poll_service_type();

		/* update band info */
		pvs8_at_css();
		
		/* update mip ip address */
		pvs8_update_mip();

		/* update hardware related information */
		update_cinterion_tr069();

		/* update sms */
		update_configuration_setting(new_status);
	}
	/* umts specific function calls */
	else {
		/*
			Cinterion modules take a bit long time to do AT commands while PDP session is up
			We do increase heart beat before each SIM access function
		*/

		update_sim_hint();
		cinterion_update_roaming_status();

		initialize_band_selection_mode(new_status);
// The alpha tag from AT+COPS command is very firmware version dependent.
// Instead of using long aplhanumeric format opName, converting mcc_mnc pair to Operator name is used in cinterion_update_network_stat() function.
		//cinterion_network_name();
		if(cinterion_type==cinterion_type_2g) {
			cinterion_sevingcell_monitor_moni();
		} else {
			cinterion_sevingcell_monitor_smoni();
		}
		update_ccid();
		update_sim_status();

		/* update serial */
		cinterion_update_serial();
		
		update_configuration_setting(new_status);
		update_cinterion_signal_strength();
		update_imsi();
		update_pdp_status();
		update_cinterion_pin_retry_number();

		update_cinterion_tr069();

		// run only after real-time clock is sync.
		cinterion_update_date_and_time();

		cinterion_update_network_stat(1);
		if(cinterion_type!=cinterion_type_2g) {
			phs8p_poll_service_type();
		}
	}

	return 0;
}

static int pvs8_get_stat(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
	char* resp;
	int reg;

	syslog(LOG_DEBUG,"pvs8_get_stat() called");

	memset(new_status,0,sizeof(*new_status));

	if(status_needed->status[model_status_sim_ready]) {
		resp="READY";
		//resp=at_bufsend("AT+CPIN?","+CPIN: ");

		err_status->status[model_status_sim_ready]=!resp;

		/* write sim pin */
		if(resp) {
			model_default_write_sim_status(resp);
			new_status->status[model_status_sim_ready]=!strcmp(resp,"READY");
		}
	}

	if(status_needed->status[model_status_registered] || status_needed->status[model_status_attached]) {
		int nw_stat=0;

		/*

		* cdma
		0 Not registered on cdma2000 network.
		1 Registered on cdma2000 home network.
		2 Not registered, but UE is currently searching for a new cdma2000 operator.
		3 Registration denied.
		4 Unknown registration state.
		5 Registered on roaming network.

		* umts
		0 not registered, MT is not currently searching a new operator to register to ETSI 3GPP TS 27.007 version 10.6.0 Release 10 ETSI TS 127 007 V10.6.0 (2012-01)
		1 registered, home network
		2 not registered, but MT is currently searching a new operator to register to
		3 registration denied
		4 unknown (e.g. out of GERAN/UTRAN/E-UTRAN coverage)
		5 registered, roaming
		6 registered for "SMS only", home network
		7 registered for "SMS only", roaming
		(applicable only when <AcT> indicates E-UTRAN)
		(applicable only when <AcT> indicates E-UTRAN)
		8 attached for emergency bearer services only (see NOTE 2) (not applicable)

		*/
		resp=0;
		if(pvs_at_sreg(&nw_stat)>=0) {
			resp="SUCCESS";
			if( rdb_set_single_int(rdb_name(RDB_NETWORKREG_STAT,""),nw_stat)<0 ) {
				syslog(LOG_ERR,"failed to update rdb - %s",RDB_NETWORKREG_STAT);
				resp=0;
			}

			syslog(LOG_DEBUG,"[pvs8] reg status = '%d'",nw_stat);
			
			/* network register status */
			reg=(nw_stat==1) || (nw_stat>=5);
			if(status_needed->status[model_status_registered])
				new_status->status[model_status_registered]=reg;
			
			/* pdp domain attached - cdma does not have PD domain attached status and we genereate the information */
			if(status_needed->status[model_status_attached])
				new_status->status[model_status_attached]=reg;
		}

		if(status_needed->status[model_status_registered])
			err_status->status[model_status_registered]=!resp;
		
		if(status_needed->status[model_status_attached])
			err_status->status[model_status_attached]=!resp;
	}
#ifdef V_CELL_NW_cdma
	/* MIP Profile information */
	resp=send_atcmd("AT$QCMIPGETP=0");
	if(resp) {
		char *pos1, *pos2;
		pos1=strstr(resp, "AT$QCMIPGETP=0");
		while(pos1) {
			resp=pos1+strlen("AT$QCMIPGETP=0");
			pos1=strstr(resp, "AT$QCMIPGETP=0");
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
#endif
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
static int cinterion_handleGetOpList()
{
	int ok = 0;

	int stat;
	char achATRes[2048];

	cinterion_update_network_stat(0);

	/* BGS2-E : 0 - long alphanumeric, 2 - numeric  */
	if (cinterion_type == cinterion_type_2g)
		stat = at_send_with_timeout("AT+COPS=3,0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);
	else
		stat = at_send_with_timeout("AT+COPS=3,90", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 2048);
	if (stat || !ok) {
		rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "error-1");
		return -1;
	}

	update_heartBeat();

	// send at command
	stat = at_send_with_timeout("AT+COPS=?", achATRes, "+COPS", &ok, AT_COP_TIMEOUT_SCAN, 2048);
	if (stat || !ok) {
		rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "error-1");
		return -1;
	}

	update_heartBeat();

	// get the list of operators
	char achCnsOpers[2048];
	if (!_convATOpListToCNSOpList(achATRes + strlen("+COPS: "), achCnsOpers, sizeof(achCnsOpers))) {
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

static int cinterion_handleSetOpListInt(char* cns_network_mode,int store)
{
	char achOperSel[64];
	
	int iMode;
	char achMNC[16];
	char achMCC[16];
	
	char* p;
	
#ifdef V_MANUAL_ROAMING_vdfglobal
	int ms;
#endif	

	// get param
	strncpy(achOperSel,cns_network_mode,sizeof(achOperSel));
	achOperSel[sizeof(achOperSel)-1]=0;
	
	/* get 1st token - cns network selection mode */
	if(!(p=strtok(cns_network_mode,","))) {
		goto error;
	}
	iMode=atoi(p);
	
	/* get 2nd token - mcc */
	achMCC[0]=0;
	achMNC[0]=0;
	if((p=strtok(NULL,","))!=0) {
		strncpy(achMCC,p,sizeof(achMCC));
		achMCC[sizeof(achMCC)-1]=0;
	
		/* get 3rd token - mnc */
		if((p=strtok(NULL,","))!=0) {
			strncpy(achMNC,p,sizeof(achMNC));
			achMNC[sizeof(achMCC)-1]=0;
			
			/* assume MNC is 2-digit - make it 2 digit if not */
			if(strlen(achMNC)==1) {
				snprintf(achMNC,sizeof(achMNC),"%02d",atoi(achMNC));
			}
		}
	}

	// get at command network type
	int iAtNw = convert_network_type(iMode);

	// get at command mode
	int iAtMode;
	if (!iMode)
		iAtMode = 0;
	else
		iAtMode = 1;

	// [roaming] store current manual mode to rdb
	if(store) {
		syslog(LOG_ERR,"[+cops] storing current carrier selection (mode=%s)", achOperSel);
		rdb_set_single(PLMN_MANUAL_MODE,achOperSel);
	}

	// build command
	char achATCmd[128];
	
#ifdef V_MANUAL_ROAMING_vdfglobal
	ms=0;
#endif	
	if (iMode)
		if(!achMCC[0] || !achMNC[0]) {
			sprintf(achATCmd, "AT+COPS=%d", iAtMode);
		}
		else if( iAtNw<0 ) { //Automatic
			sprintf(achATCmd, "AT+COPS=%d,2,\"%s%s\"", iAtMode, achMCC, achMNC);
			automatic_operator_setting = 1;
		} else {
			sprintf(achATCmd, "AT+COPS=%d,2,\"%s%s\",%d", iAtMode, achMCC, achMNC, iAtNw);
			automatic_operator_setting = 0;
#ifdef V_MANUAL_ROAMING_vdfglobal
			ms=1;
#endif			
		}
	else {
		automatic_operator_setting = 1;

		// [roaming] detach network to trigger the custom roaming algorithm to start
		if(custom_roaming_algorithm) {
			//sprintf(achATCmd, "AT+COPS=2");
			//syslog(LOG_ERR,"[roaming] deregister - triggering custom roaming algorithm");
#ifdef V_MANUAL_ROAMING_vdfglobal
			if(store) {
				syslog(LOG_ERR,"[roaming] auto-network selected - clear blacklist");
				reset_blacklist();
			}
			do_custom_manual_roaming(0);
			goto fini;
#endif
		} else {
			sprintf(achATCmd, "AT+COPS=0");
		}
	}

	// send at command
	char achATRes[AT_RESPONSE_MAX_SIZE];
	int stat;
	int ok = 0;
	
#ifdef V_GCF_NETREG_WORKAROUND
	/* workaround for a GCF failure - only for NTC NWL12 */
	syslog(LOG_ERR,"[GCF] detach IMSI from network");
	stat = at_send_with_timeout("AT+COPS=2", achATRes, "", &ok, AT_COP_TIMEOUT, 0);
	if (stat || !ok)
		goto error;
	
	sleep(3);
#endif	
	
	/* init. notis */
	init_noti_creg();
	init_noti_cgreg();
	
	syslog(LOG_ERR,"[GCF] attach IMSI to network");
	stat = at_send_with_timeout(achATCmd, achATRes, "", &ok, AT_COP_TIMEOUT, 0);
	if (stat || !ok)
		goto error;
		
#ifdef V_MANUAL_ROAMING_vdfglobal

	if(ms) {
		if(retry_cgatt_if_required_mcc_mnc(atoi(achMCC),atoi(achMNC))<0) {
			syslog(LOG_ERR,"[roaming] PS re-attach procedure failed #ms");
			goto error;
		}
	}

fini:
#endif
	// back to alphanumeric
	stat = at_send_with_timeout("AT+COPS=3,0", achATRes, "", &ok, AT_QUICK_COP_TIMEOUT, 0);


	// update network
	cinterion_update_network_stat(0);
	//sleep(3);
	//handle_network_scan(NULL);


	return 0;

error:
	return -1;
}

static int cinterion_handleSetOpList()
{
	char achOperSel[64];
	
	if (rdb_get_single(rdb_name(RDB_PLMNSEL, ""), achOperSel, sizeof(achOperSel)) != 0)
		goto error;
	
	if(cinterion_handleSetOpListInt(achOperSel,1)<0)
		goto error;
	
	// set result
	rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "[DONE]");
	
	return 0;
	
error:
	rdb_set_single(rdb_name(RDB_PLMNSTAT, ""), "error");
	return -1;
}

static int cinterion_cmd_plmn(const struct name_value_t* args)
{
	if (!args || !args[0].value) {
		return -1;
	}

	if (!strcmp(args[0].value, "1"))
		return cinterion_handleGetOpList();
	else if (!strcmp(args[0].value, "5")) {
		return cinterion_handleSetOpList();
	}

	return -1;
}

#ifdef GPS_ON_AT

#if 0
static int SendCinterionGpsCommand(const char* cmd)
{
	int stat, fOK;
	char* response = alloca(AT_RESPONSE_MAX_SIZE);

	stat = at_send(cmd, response, "", &fOK, AT_RESPONSE_MAX_SIZE);
	SYSLOG_ERR("response='%s'", response);
	if (strncmp(response, "+CME ERROR:", 11) == 0) {
		response += 12;
		rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE), response);
	}
	if (stat < 0 || !fOK) {
		SYSLOG_ERR("'%s' command failed", cmd);
		return -1;
	}
	return 0;
}
#endif
						      
static int handleCinterionGpsCommand(gps_cmd_enum_type cmd)
{
	int stat = 0;
	switch (cmd) {
		case GPS_DISABLE_CMD:
			stat = pxs8_poll_gpsone(0);
			#warning [TODO] save error code to the RDB with AT+CMEE 
			#if 0
			stat = SendCinterionGpsCommand("AT^SGPSC=\"Engine\",0");
			#endif								       
			break;
	
		case GPS_ENABLE_CMD:
			stat = pxs8_poll_gpsone(1);
			#warning [TODO] save error code to the RDB with AT+CMEE
			#if 0
			stat = SendCinterionGpsCommand("AT^SGPSC=\"Engine\",1");
			#endif								       
			break;
			
		default:
			syslog(LOG_ERR,"unknown command detected - %d",cmd);
			stat = 1;
			break;
	}
	return stat;
}

static int phs8p_cmd_gps(const struct name_value_t* args)
{
	/* bypass if incorrect argument */
	if (!args || !args[0].value) {
		SYSLOG_ERR("expected command, got NULL");
		goto error;
	}

	//setlogmask(LOG_UPTO(4 + LOG_ERR));

	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE), "");

	/* process GPS DISABLE command */
	if (strcmp(args[0].value, "disable") == 0) {
		SYSLOG_ERR("Got GPS DISABLE command");
		if (handleCinterionGpsCommand(GPS_DISABLE_CMD) < 0)
			goto error;
	}

	/* process GPS ENABLE command */
	else if (strcmp(args[0].value, "enable") == 0) {
		SYSLOG_ERR("Got GPS ENABLE command");
		if (handleCinterionGpsCommand(GPS_ENABLE_CMD) < 0)
			goto error;
	}

	/* process AGPS command */
	else if (strcmp(args[0].value, "agps") == 0) {
		SYSLOG_ERR("Got AGPS command");
		/* Cinterion module does not support location fix so return error
		 * immediately. */
		rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_ERRCODE), "Not Support");
		goto error;
	}

	else {
		SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
		goto error;
	}

	//setlogmask(LOG_UPTO(0 + LOG_ERR));
	SYSLOG_ERR("GPS [%s] command succeeded", args[0].value);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS), "[done]");
	return 0;

error:
	//setlogmask(LOG_UPTO(0 + LOG_ERR));
	SYSLOG_ERR("GPS [%s] command failed", args[0].value);
	rdb_set_single(rdb_name(RDB_GPS_PREFIX, RDB_GPS_CMD_STATUS), "[error]");
	return -1;
}
#endif  /* GPS_ON_AT */

/* generic functions */

static char* skip_prefix(char* p,const char* prefix)
{
	char* s;

	if(!(s=strstr(p,prefix)))
		return NULL;

	p+=strlen(prefix);

	return p;
}


/* rdb functions */

int rdb_set_value(const char* rdb,const char* val)
{
	return rdb_set_single(rdb, val==NULL?"":val);
}

static int rdb_set_printf(const char* rdb,const char* fmt,...)
{
	char rdb_val[1024];
	int stat;


	va_list ap;

	va_start(ap, fmt);

	vsnprintf(rdb_val,sizeof(rdb_val),fmt,ap);
	stat=rdb_set_single(rdb, rdb_val);

	va_end(ap);

	return stat;
}


const char* rdb_get_value(const char* rdb)
{
	static char rdb_val[256];

	if(rdb_get_single(rdb, rdb_val, sizeof(rdb_val))!=0) {
		return "";
	}

	return rdb_val;
}

/*
	phs8p noti functions
*/

static int phs8p_noti_ciev_psinfo(const char* s)
{
	syslog(LOG_DEBUG,"+CIEV: psinfo - %s",s);

	/* update rdb variable cased on noti msg */
	phs8p_update_service_type_rdb(s);

	return 0;
}

static void cinterion_noti_ciev_rssi_schedule(void* ref)
{
	syslog(LOG_DEBUG,"polling signal strength (csq)");
	update_cinterion_signal_strength();

	scheduled_clear("cinterion_noti_ciev_rssi_schedule");
}

static int cinterion_noti_ciev_rssi(const char* s)
{
	syslog(LOG_DEBUG,"+CIEV: rssi - we are doing CSQ soon");

	/* immediately poll csq */
	scheduled_func_schedule("cinterion_noti_ciev_rssi_schedule",cinterion_noti_ciev_rssi_schedule,-1);

	return 0;
}


/*
	pvs8 AT functions
*/

static char* wait_atcmd(const char* str, int timeout)
{
	static char resp[AT_RESPONSE_MAX_SIZE];
	int ok;

	*resp=0;
	
	int i;
	int found;
	
	found=0;
	
	i=0;
	while(i++<timeout) {
		
		/* wait for a second */
		*resp=0;
		at_send_with_timeout(NULL, resp, "", &ok, 1, 0);
	
		/* break if found */
		found=strstr(resp,str)!=NULL;
		if(found)
			break;
	}

	if(!found)
		goto err;
	
	return resp;
err:
	return NULL;
}

static char* send_atcmd(const char* atcmd)
{
	
	int ok;

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

char* send_atcmd_printf(const char* fmt,...)
{
	char atcmd[1024];
	char* resp;


	va_list ap;

	va_start(ap, fmt);

	vsnprintf(atcmd,sizeof(atcmd),fmt,ap);
	resp=send_atcmd(atcmd);

	va_end(ap);

	return resp;
}

static int pvs8_update_mip()
{
	char* resp;
	char* p;
	
	char* mip_addr;
	char* mip_mode;
	int mode;
	
	/*	
		at+cmip?
		+CMIP: 166.149.197.32
	*/	
	
	mip_addr=NULL;
	mip_mode=NULL;
	
	/* query MIP address */
	if(!(resp=send_atcmd("AT+CMIP?"))) {
		syslog(LOG_ERR,"+CMIP? AT command failed");
	}
	else {
		/* skip prefix */
		if(!(p=skip_prefix(resp,"+CMIP: "))) {
			syslog(LOG_ERR,"unknown response from +CMIP? - %s",resp);
		}
		else {
			mip_addr=strdupa(p);
		}
	}
	
	/* query MIP mode */
	if(!(resp=send_atcmd("AT$QCMIP?"))) {
		syslog(LOG_ERR,"$QCMIP? AT command failed");
	}
	else {
		/* skip prefix */
		if(!(p=skip_prefix(resp,"$QCMIP: "))) {
			syslog(LOG_ERR,"unknown response from $QCMIP? - %s",resp);
		}
		else {
			mode=atoi(p);
			
			switch(mode) {
				case 0:
					mip_mode="sip";
					break;
					
				case 1:
					mip_mode="mip/sip";
					break;
					
				case 2:
					mip_mode="mip";
					break;
					
				default:
					syslog(LOG_ERR,"unknown mip/sip mode - %s",resp);
					break;
			}
		}
	}
	
	
	/* set rdb */
	rdb_set_value(rdb_name(RDB_SPRINT_CUR_MIP,""),mip_addr?mip_addr:"");
	rdb_set_value(rdb_name(RDB_SPRINT_MIP_MODE,""),mip_mode?mip_mode:"");
	
	return 0;
}

static int pvs8_at_css()
{
	
	/*
		AT+CSS?
		+CSS: 507,B,48
	*/
	
	char* resp;
	char* p;
	
	int ch;
	char* band;
	int sid;
	
	char bandname[128];
	
	#ifdef CINTERION_DEBUG_MODE
	syslog(LOG_ERR,"!!!!!!!!!!!! WARNING !!!!!!!!!!!!");
	resp=strdupa("+CSS: 507,B,48");
	syslog(LOG_ERR,"debug mode - using fake +css? (%s)",resp);
	#else
	/* query serving system */
	if(!(resp=send_atcmd("AT+CSS?"))) {
		syslog(LOG_ERR,"+CSS AT command failed");
		goto err;
	}
	#endif
	
	/* skip prefix */
	if(!(p=skip_prefix(resp,"+CSS: "))) {
		syslog(LOG_ERR,"unknown response from +CSS? - %s",resp);
		goto err;
	}
	
	/* get channel */
	if(!(p=strtok(p,","))) {
		syslog(LOG_ERR,"channel class field missing in +CSS - %s",resp);
		goto err;
	}
	ch=atoi(p);
			
	/* get band class */
	if(!(p=strtok(NULL,","))) {
		syslog(LOG_ERR,"band class field missing in +CSS - %s",resp);
		goto err;
	}
	band=strdupa(p);
	
	/* get sid */
	if(!(p=strtok(NULL,","))) {
		syslog(LOG_ERR,"sid field missing in +CSS - %s",resp);
		goto err;
	}
	sid=atoi(p);
		
	syslog(LOG_DEBUG,"[pvs8] ch=%d,band=%s,sid=%d",ch,band,sid);
	
	/* build band name */
	if(*band=='Z') {
		syslog(LOG_DEBUG,"[pvs8] band class Z - not registered");
		*bandname=0;
	}
	else if(ch==9999) {
		syslog(LOG_DEBUG,"[pvs8] channel 9999 - not registered");
		*bandname=0;
	}
	else {
		snprintf(bandname,sizeof(bandname),"CH:%d PCS-%s Band",ch,band);
	}
	
	rdb_set_value(rdb_name(RDB_CURRENTBAND,""),bandname);
	
	
	return 0;
err:
	return -1;	
}

static int cinterion_cdma_csq(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char temp[64];
	int token_count;
	const char* t;
	int csq;

	int rssi=0;
	int ok=0;


	/* send +CSQ */
#ifdef CINTERION_DEBUG_MODE
	ok=0;
	syslog(LOG_ERR,"!!!!!!!!!!!! WARNING !!!!!!!!!!!!");
	strcpy(response,"+CSQ: 20, 99");
	syslog(LOG_ERR,"debug mode - using fake csq (%s)",response);
	{
#else
	if (at_send("AT+CSQ?", response, "+CSQ", &ok, 0) == 0 && ok) {
#endif
		token_count = tokenize_at_response(response);

		/* get RSSI */
		if ( (token_count >= 1) && ((t = get_token(response, 1))!=NULL) ) {
			csq = atoi(t);
			rssi=csq < 0 || csq == 99 ? 0 : (csq * 2 - 113);
		}

		/* get bers - TODO: typo - the name is supposed to be bers */
		if ( (token_count >= 2) && ((t = get_token(response, 2))!=NULL) )
			rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, "bars"), t);
	}

	/* use blank if rssi is zero */
	if(rssi==0)
		*temp=0;
	else
		sprintf(temp, "%ddBm", rssi);

	rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), temp);

	syslog(LOG_DEBUG,"[pvs8] rssi = '%s'",temp);

	return 0;
}

static int pvs_at_sreg(int* sreg)
{
	char* p;
	char* t;

	int regstat;
	int regstat_cdma;
	int regstat_evdo;
	const char* roaming_msg;
	
	int service;
	int evdo;

/*
	0 No service
	1 1xRTT service
	2 EVDO service
	3 EVDO Rev A
	4 GPRS
	5 UMTS
	6 EGPRS	
	
	AT+SERVICE?
	+SERVICE: 3
	
	
	OK
	
*/		
	/* send at command - AT+SERVICE */
	if(!(p=send_atcmd("AT+SERVICE?"))) {
		syslog(LOG_ERR,"AT+SERVICE? command failed");
		goto err;
	}
	
	/* skip prefix */
	if(!(t=strstr(p,"+SERVICE: "))) {
		syslog(LOG_ERR,"unknown AT result from ^AT+SERVICE? - %s",p);
		goto err;
	}
	/* get service */
	t+=STRLEN("+SERVICE: ");
	service=atoi(t);
	
	/* is evdo */
	evdo=(service==2) || (service==3);
			
	if(!(p=send_atcmd("AT^SREG?"))) {
		syslog(LOG_ERR,"AT^SREG command failed");
		goto err;
	}
	
	/*
		AT^SREG?
		^SREG: 0,0

		OK
	*/

	/* get URC mode */
	if(!(t=strtok(p,","))) {
		syslog(LOG_ERR,"unknown AT result from ^SREG (no URC found) - %s",p);
		goto err;
	}

	/* get regstatus */
	if(!(t=strtok(NULL,","))) {
		syslog(LOG_INFO,"unknown AT result from ^SREG (no reg status found) #1 - %s",p);
		regstat_cdma=0;
	}
	else {
		regstat_cdma=atoi(t);
	}
	
	/* get regstatus2 */
	if(!(t=strtok(NULL,","))) {
		syslog(LOG_INFO,"unknown AT result from ^SREG (no reg status found) #2 - %s",p);
		regstat_evdo=0;
	}
	else {
		regstat_evdo=atoi(t);
	}
	
	/* get correct regstat */
	if(evdo)
		regstat=regstat_evdo;
	else
		regstat=regstat_cdma;
	
	syslog(LOG_DEBUG,"[pvs8] service='%d',reg_cdma='%d',reg_evdo='%d'",service,regstat_cdma,regstat_evdo);

	*sreg=regstat;

	/* get roaming message */
	if(regstat==5) {
		syslog(LOG_DEBUG,"[pvs8] roaming status detected");

		roaming_msg="active";
	} else {
		syslog(LOG_DEBUG,"[pvs8] home-network detected");

		roaming_msg="deactive";
	}

	rdb_set_value(rdb_name(RDB_ROAMING, ""), roaming_msg);
	return 0;

err:
	rdb_set_value(rdb_name(RDB_ROAMING, ""), "");
	return 0;
}


static int cinterion_cdma_smoni(void)
{
	const char* resp;

	/*
	cdma2000 and 1x Pilot acquired:
	Syntax:
		^SMONI: CH,BC,PN,MCC,MNC,SID,NID, CELL, CLASS, PZID,LAT,LONG,PREV,RX,ECIO
		CH,BC,PN,RX
	Example:
		^SMONI: 426,0,16,302,0,16422,102,7602,0,162,708472,6617580,6,-90,15
		468,0,16,-72
	1xEVDO Pilot acquired:
	Syntax:
		^SMONI: CH BC,PN,RX
	Example:
		^SMONI: 468,0,16,-72
	UE is searching for a Pilot Channel:
		^SMONI: Searching
	*/
	char *CH,*BC,*PN,*MCC,*MNC,*SID,*NID,*CELL,*CLASS,*PZID,*LAT,*LONG,*PREV,*RX,*ECIO,*CH2,*BC2,*PN2,*RX2;
	char* p;
	char* d;

	// init. local variables
	CH=BC=PN=MCC=MNC=SID=NID= CELL= CLASS= PZID=LAT=LONG=PREV=RX=ECIO=CH2=BC2=PN2=RX2=NULL;

#define SMONI_PREFIX	"^SMONI: "
#define	SMONI_CMD	"AT^SMONI"
	if((resp=send_atcmd(SMONI_CMD))==NULL) {
		syslog(LOG_ERR,"AT command failed - %s",SMONI_CMD);
		goto fini;
	}

#ifdef CINTERION_DEBUG_MODE
	syslog(LOG_ERR,"!!!!!!!!!!!! WARNING !!!!!!!!!!!!");
	resp="^SMONI: 548,0,153,310,0,64,49,3267,0,49,0,0,6,-87,13\nSearching";
	syslog(LOG_ERR,"debug mode - using fake smoni (%s)",resp);
#endif

	d=strdupa(resp);

	/* search prefix and skip it */
	if((p=strstr(d,SMONI_PREFIX))==0) {
		syslog(LOG_ERR,"%s returned unkonwn result (result prefix missing) - %s",SMONI_CMD,resp);
		goto fini;
	}
	p+=strlen(SMONI_PREFIX);

	/* get filed script */
#define STRTOK_FIELD(str,field,label) do { \
			if( (p=strtok(str, ",\n"))==0 ) { \
				syslog(LOG_ERR,"%s returned unkonwn result (%s field) - %s",#field,SMONI_CMD,resp); \
				goto label; \
			} \
			field=strdupa(p); \
		} while(0)

#define STRTOK_FIELD_NOERR(str,field,label) do { \
			if( (p=strtok(str, ",\n"))==0 ) { \
				syslog(LOG_DEBUG,"%s returned unkonwn result (%s field) - %s",#field,SMONI_CMD,resp); \
				goto label; \
			} \
			field=strdupa(p); \
		} while(0)
	
	/*
	^SMONI: 507,0,12,310,0,48,4,424,0,2,0,0,6,-74,11
	
	^SMONI: CH,BC,PN,MCC,MNC,SID,NID, CELL, CLASS, PZID,LAT,LONG,PREV,RX,ECIO,CH2,BC2,PN2,RX2;
	^SMONI: CH BC,PN,RX
	*/

	
	
	/* check if it is searching */
	if(!strncmp(p,"Searching",STRLEN("Searching"))) {
		syslog(LOG_DEBUG,"%s returned searching result - %s",SMONI_CMD,resp);
	} else {
		STRTOK_FIELD(p,CH,fini);

		STRTOK_FIELD(NULL,BC,fini);
		STRTOK_FIELD(NULL,PN,fini);
		STRTOK_FIELD(NULL,MCC,fini);
		STRTOK_FIELD_NOERR(NULL,MNC,parse_1xevdo_only);

		/* cdma2000 */
		STRTOK_FIELD(NULL,SID,fini);
		STRTOK_FIELD(NULL,NID,fini);
		STRTOK_FIELD(NULL,CELL,fini);
		STRTOK_FIELD(NULL,CLASS,fini);
		STRTOK_FIELD(NULL,PZID,fini);
		STRTOK_FIELD(NULL,LAT,fini);
		STRTOK_FIELD(NULL,LONG,fini);
		STRTOK_FIELD(NULL,PREV,fini);
		STRTOK_FIELD(NULL,RX,fini);
		STRTOK_FIELD(NULL,ECIO,fini);
		
		/* 1xevdo - 2nd line from cdma2000 */
		STRTOK_FIELD_NOERR(NULL,CH2,parse_without_1xvdo);
		
		/* bypass if 1x stil searching */
		if(!strncmp(p,"Searching",STRLEN("Searching"))) {
			goto parse_without_1xvdo;
		}
		
		STRTOK_FIELD(NULL,BC2,fini);
		STRTOK_FIELD(NULL,PN2,fini);
		STRTOK_FIELD(NULL,RX2,fini);
		syslog(LOG_DEBUG,"[pvs8] PN2=%s,RX2=%s",PN2,RX2);
		goto fini;
		
		
/* cdma2000 - single line */
parse_without_1xvdo:
		syslog(LOG_DEBUG,"[pvs8] RX=%s,ECHO=%s",RX,ECIO);
		goto fini;
		
/* 1xEVDO */
parse_1xevdo_only:
		CH2=CH;
		BC2=BC;
		PN2=PN;
		RX2=MCC;
		
		CH=BC=PN=MCC=NULL;
		syslog(LOG_DEBUG,"[pvs8] RX=%s",RX);
		goto fini;
	}

	
	/* set rdb variables based on result */
fini:
	/* mcc */
	rdb_set_value(rdb_name(RDB_PLMNMCC, ""), MCC);
	/* mnc */
	rdb_set_value(rdb_name(RDB_PLMNMNC, ""), MNC);
	/* sid */
	rdb_set_value(rdb_name(RDB_CDMA_SYSTEMID,""), SID);
	/* nid */
	rdb_set_value(rdb_name(RDB_CDMA_NETWORKID,""), NID);
	/* channel */
	rdb_set_value(rdb_name(RDB_CDMA_1XRTTCHANNEL,""), CH);
	rdb_set_value(rdb_name(RDB_CDMA_1XEVDOCHANNEL,""), CH2);
	/* pn */	
	rdb_set_value(rdb_name(RDB_CDMA_1XRTTPN,""), PN);
	rdb_set_value(rdb_name(RDB_CDMA_1XEVDOPN,""), PN2);
	/* cell  */
	rdb_set_value(rdb_name(RDB_NETWORK_STATUS".CellID", ""), CELL);
	
	/* rx level */
	rdb_set_value(rdb_name(RDB_CDMA_1XRTT_RX_LEVEL,""), RX);
	rdb_set_value(rdb_name(RDB_CDMA_1XEVDO_RX_LEVEL,""), RX2);
	/* ecio */
	rdb_set_value(rdb_name(RDB_ECIO"0", ""), ECIO);

	syslog(LOG_DEBUG,"[pvs8] mcc=%s,mnc=%s",MCC,MNC);

	return 0;
}

/*
	cdma command functions
*/

static enum {activation_wait=0,activation_succ,activation_fail} otasp_activation_stat=0; /* 1 or 0 after getting activation succ notification */
static int otasp_no_carrier=0;	/* true after getting "no carrier" notification */

/*
	pvs8 notification functions
*/

static int pvs8_noti_nocarrier(const char* s)
{
	/*
		NO CARRIER
	*/

	syslog(LOG_DEBUG,"[pvs8] 'no carrier' noti detected - %s",s);

	/* check 'no carrier' string in result */
	if(!strcmp(s,"NO CARRIER")) {
		otasp_no_carrier=1;
		syslog(LOG_DEBUG,"[pvs8] 'no carrier' detected");
	} else {
		syslog(LOG_ERR,"[pvs8] unknown 'no carrier' notification format - %s",s);
	}

	return 0;
}

static void pvs8_noti_cdma_rssi_schedule(void* ref)
{
	syslog(LOG_DEBUG,"[pvs8] polling signal strength (csq)");
	cinterion_cdma_csq();

	scheduled_clear("pvs8_noti_cdma_rssi_schedule");
}

static int pvs8_noti_ciev_rssi(const char* s)
{
	syslog(LOG_DEBUG,"[pvs8] rssi noti detected - %s",s);

	// immeidiately poll signal strength
	scheduled_func_schedule("pvs8_noti_cdma_rssi_schedule",pvs8_noti_cdma_rssi_schedule,-1);

	return 0;
}

static int cinterion_noti_ciev_nitz(const char* s)
{
	char* p;
	char* str;

	syslog(LOG_DEBUG,"ciev noti detected - %s",s);

	/*
		+CIEV: nitz,"13/03/20,02:45:43",-07
	*/

	/* copy to writable memory */
	str=strdupa(s);

	/* skip prefix */
	if(!(p=skip_prefix(str,"+CIEV: nitz,\"")) ) {
		syslog(LOG_ERR,"unknown CIEV not - %s",str);
		goto err;
	}

	/* set realtime clock */
	cinterion_set_realtime_clock(str,p);

	return 0;
err:
	return -1;

}

static int pvs8_noti_activation(const char* s)
{
	/*

		ACTIVATION: Success

		NO CARRIER

	*/

	const char* p;

	syslog(LOG_DEBUG,"[pvs8] activation noti detected - %s",s);

	/* search prefix in result */
	if(!(p=strstr(s,"ACTIVATION: "))) {
		syslog(LOG_ERR,"unknown activation notification format - %s",s);
		goto err;
	}
	p+=strlen("ACTIVATION: ");

	/* check if success */
	if(strstr(p,"Success")!=NULL) {

		syslog(LOG_DEBUG,"[pvs8] ota-sp provisioning succ - %s",p);

		otasp_activation_stat=activation_succ;
	} else {
		syslog(LOG_DEBUG,"[pvs8] ota-sp provisioning fail - %s",p);

		otasp_activation_stat=activation_fail;
	}

	return 0;
err:
	return -1;
}

static int pvs8_cmd_prefset(const struct name_value_t* args)
{
	const char* opt;
	
	char* network_mode_name;
	int network_mode;
	char* roam_mode;
	
	char* p;
	char* p2;
	char* resp;
	
	syslog(LOG_DEBUG,"[pvs8] prefset command detected - '%s'",args[0].value);
	
	opt=args[0].value;
	
	/* get network mode and roam mode */
	if(!strcmp(opt,"get")) {
		syslog(LOG_ERR,"[pvs8] prefset get command");
		
		/*
			AT^PREFMODE?
			^PREFMODE: 8
	
			OK
		*/
		
		/* send prefmode query */
		if(!(resp=send_atcmd("AT^PREFMODE?"))) {
			syslog(LOG_ERR,"[pvs8] failed in querying network prefmode");
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to get network prefmode");
			goto err;
		}
				
		/* skip prefix in result */
		if(!(p=skip_prefix(resp,"^PREFMODE: "))) {
			syslog(LOG_ERR,"[pvs8] unknown result format from prefmode query - '%s'",resp);
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to get network prefmode");
			goto err;
		}
		
		/* convert network mode index to name */
		network_mode=atoi(p);
		if(network_mode==2) {
			network_mode_name="1x";
		}
		else if(network_mode==4) {
			network_mode_name="HDR";
		}
		else if(network_mode==8) {
			network_mode_name="1x/HDR";
		}
		else {
			syslog(LOG_ERR,"[pvs8] unknown network mode from query - '%s'",resp);
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to get network prefmode");
			goto err;
		}
		
		/*		
			AT^SCFG="CDMA/AutoAB"
			^SCFG: "CDMA/AutoAB","AutoB"
			
			OK
		*/
		
		/* send roam prefmode query */
		if(!(resp=send_atcmd("AT^SCFG=\"CDMA/AutoAB\""))) {
			syslog(LOG_ERR,"[pvs8] failed in querying roam prefmode");
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to get roam prefmode");
			goto err;
		}
				
		/* skip prefix in result */
		if(!(p=skip_prefix(resp,"^SCFG: \"CDMA/AutoAB\",\""))) {
			syslog(LOG_ERR,"[pvs8] unknown result format from roam prefmode query - '%s'",resp);
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to get roam prefmode");
			goto err;
		}
		
		/* remove the last double quotation mark */
		if(!(p2=strchr(p,'"'))) {
			syslog(LOG_ERR,"[pvs8] unknown result format from roam prefmode query - '%s'",resp);
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to get roam prefmode");
			goto err;
		}
		*p2=0;
		
		roam_mode=strdupa(p);
		
		/* check validation of roam mode */
		if(!strcasecmp(roam_mode,"autoa")) {
		}
		else if(!strcasecmp(roam_mode,"autob")) {
		}
		else if(!strcasecmp(roam_mode,"any")) {
		}
		else {
			syslog(LOG_ERR,"[pvs8] unknown roam prefmode from query - '%s'",resp);
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to get roam prefmode");
			goto err;
		}

		/* return result to rdb variables */
		rdb_set_value(rdb_name(RDB_CDMA_PREFSET_NETWORK_MODE,""),network_mode_name);
		rdb_set_value(rdb_name(RDB_CDMA_PREFSET_ROAM_MODE,""),roam_mode);
	}
	/* set network mode and roam mode */
	else if(!strcmp(opt,"set")) {
		
		/* get parameters */
		network_mode_name=strdupa(rdb_get_value(rdb_name(RDB_CDMA_PREFSET_NETWORK_MODE,"")));
		roam_mode=strdupa(rdb_get_value(rdb_name(RDB_CDMA_PREFSET_ROAM_MODE,"")));
		
		syslog(LOG_ERR,"[pvs8] prefset set command - network=%s,roam=%s",network_mode_name,roam_mode);
		
		/* convert network mode name to network mode */
		if(!strcasecmp(network_mode_name,"1x")) {
			network_mode=2;
		}
		else if(!strcasecmp(network_mode_name,"HDR")) {
			network_mode=4;
		}
		else if(!strcasecmp(network_mode_name,"1x/HDR")) {
			network_mode=8;
		}
		else if(*network_mode_name) {
			syslog(LOG_ERR,"[pvs8] unknown network mode found - '%s'",network_mode_name);
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] parameter error - unknown network mode");
			goto err;
		}
		else {
			syslog(LOG_DEBUG,"[pvs8] unknown network mode parameter skip");
			network_mode=-1;
		}
		
		/* check validation of roam mode */
		if(!strcasecmp(roam_mode,"autoa")) {
		}
		else if(!strcasecmp(roam_mode,"autob")) {
		}
		else if(!strcasecmp(roam_mode,"any")) {
		}
		else if(*roam_mode) {
			syslog(LOG_ERR,"[pvs8] unknown roam mode found - '%s'",roam_mode);
			
			/* set error message */
			rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] parameter error - unknown roam mode");
			goto err;
		}
		else {
			syslog(LOG_DEBUG,"[pvs8] roam mode rdb in prefset command not specified");
		}
		
		syslog(LOG_ERR,"[pvs8] prefset set command - network=%d",network_mode);
		
		/* skip network preference */
		if(network_mode<0) {
			syslog(LOG_DEBUG,"[pvs8] skip network preference configuration");
		}
		else {
			/* send prefmode - select network preference */
			if(!send_atcmd_printf("AT^PREFMODE=%d",network_mode)) {
				syslog(LOG_ERR,"[pvs8] failed in changing network preference - '%s(%d)'",network_mode_name,network_mode);
				
				/* set error message */
				rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to change network preference mode");
				goto err;
			}
		}
		
		/* skip roaming preference mode */
		if(!*roam_mode) {
			syslog(LOG_DEBUG,"[pvs8] skip roaming preference configuration");
		}
		else {
			if(!send_atcmd_printf("AT^SCFG=\"CDMA/AutoAB\",\"%s\"",roam_mode)) {
				syslog(LOG_ERR,"[pvs8] failed in changing roam preference mode - '%s'",roam_mode);
				
				/* set error message */
				rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[error] failed to change roam preference mode");
				goto err;
			}
		}
		
		
	}
	else {
		syslog(LOG_ERR,"[pvs8] unknown prefset command - '%s'",opt);
		return -1;
	}
	
	/* set result message */
	rdb_set_value(rdb_name(RDB_CDMA_PREFSET_STAT,""),"[done]");
	return 0;
	
err:
	return -1;	
}

static int pvs8_cmd_otasp(const struct name_value_t* args)
{
	const char* otasp_xx;

	struct tms buf;
	clock_t clk;
	clock_t per_sec;
	
	const char* spc;

	int i;
	
	int power_cycle_module=0;

	syslog(LOG_DEBUG,"[pvs8] otasp command detected - '%s'",args[0].value);

	/* module factory reset - including ota-sp */
	if(!strcmp(args[0].value, "rtn")) {
		
		/* read spc code form rdb */
		spc=rdb_get_value(rdb_name(RDB_CDMA_OTASP_SPC,""));
		if(!*spc) {
			syslog(LOG_ERR,"[pvs8] spc code not found");
			rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] spc code not found");
			goto rtn_error;
		}
		
		/* send RTN factory reset command */
		if(!send_atcmd_printf("AT$RTN=%s",spc)) {
			syslog(LOG_ERR,"[pvs8] RTN factory reset command failed - spc=%s",spc);
			rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] RTN AT command failed");
			goto rtn_error;
		}
		
		/* write succ to rdb */
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[done]");
		
	rtn_error:
		/* nothing to do */
		;
	}
	/* ota-sp programming procedure */
	else if (!strcmp(args[0].value, "otasp")) {

		/* set otasp progress flag */
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_PROGRESS,""),"1");

		/* get otasp xx */
		otasp_xx=rdb_get_value(rdb_name(RDB_CDMA_OTASP_XX,""));

		/* send FC command */
		syslog(LOG_DEBUG,"[pvs8] send subscriber init - XX='%s'",otasp_xx);
		if(!send_atcmd_printf("ATD*228%s",otasp_xx)) {
			syslog(LOG_ERR,"OTA-SP programming failed - XX=%s",otasp_xx);
			goto otasp_error;
		}

		/* get start clock and per sec clock */
		clk=times(&buf);
		per_sec=sysconf(_SC_CLK_TCK);

		/* total ota-sp time period 3 minutes */
#define OTASP_TOTAL_TIME	(3*60)

		/* start otasp progress */
		otasp_activation_stat=activation_wait;	/* wait mode */
		otasp_no_carrier=0;	/* wait mode */


		/* wait for result */
		i=0;
		while( times(&buf)-clk < OTASP_TOTAL_TIME*per_sec) {
			i++;

			/* give up execution for notification handler - run notification handler */
			update_heartBeat();
			at_wait_notification(-1);

			/* check if activation noti is caught */
			if(otasp_activation_stat!=activation_wait) {

				if(otasp_activation_stat==activation_succ) {
					syslog(LOG_DEBUG,"[pvs8] otasp succ");
					rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[done]");
					
					/* after activating module, we need to power cycle module */
					power_cycle_module=1;
				} else if(otasp_activation_stat==activation_fail) {
					syslog(LOG_DEBUG,"[pvs8] otasp fail");
					rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] general failure");
				} else {
					syslog(LOG_ERR,"unknown status detected - internal error");
				}
				break;
			}
			/* check if 'no carrier' noti is caught */
			else if(otasp_no_carrier) {
				syslog(LOG_DEBUG,"[pvs8] otasp fail - 'no carrier' detected");
				rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] no carrier");
				break;
			}

			syslog(LOG_DEBUG,"[pvs8] waiting for otasp #%d",i);
			sleep(1);
		}

		/* reset progress flag */
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_PROGRESS,""),"0");
		
		/* power cycle module */
		if(power_cycle_module) {
			syslog(LOG_ERR,"[pvs8] otasp finished - power-cycling phone module");
			system("reboot_module.sh 2> /dev/null > /dev/null &");
		}

	otasp_error:
		/* reset progress flag */
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_PROGRESS,""),"0");
	} else {
		syslog(LOG_ERR,"unknown otasp command - %s",args[0].value);
		rdb_set_value(rdb_name(RDB_CDMA_OTASP_STAT,""),"[error] unknown command");
	}

	return 0;
}

int update_cinterion_SMS_retransmission_timer(int time)
{
	const char* resp;
	resp = send_atcmd_printf("AT^SCFG=Sms/Retrm,%d", time);
		if(!resp) {
		syslog(LOG_ERR,"failed in AT^SCFG=Sms/Retrm command");
		return -1;
	}

	return 0;
}

int update_cinterion_firmware_revision()
{
	const char* resp;
	const char* rev;
	char* t;

	/* send ati1 command */
	resp=send_atcmd("ATI1");
	if(!resp) {
		syslog(LOG_ERR,"failed in ATI1 commnad");
		goto err;
	}

/*
	ati1
	Cinterion
	PVS8
	REVISION 02.911
	A-REVISION 01.000.00
*/
	
	/* get revision */
	rev=strstr(resp,"A-REVISION ");
	if(!rev) {
		syslog(LOG_ERR,"[pxs8] unknown format of ATI1 command result (resp=%s)",resp);
		goto err;
	}
	rev+=STRLEN("A-REVISION ");

	/* get token */
	t=strtok(strdupa(rev),"\r");
	if(!t) {
		syslog(LOG_ERR,"revision token not found in ATI1 (resp=%s)",resp);
		goto err;
	}

	syslog(LOG_INFO,"a-revision detected (rev=%s)",t);

	/* set rdb */
	rdb_set_value(rdb_name(RDB_FIRMWARE_VERSION_CID,""),t);
	
	return 0;
err:
	rdb_set_value(rdb_name(RDB_FIRMWARE_VERSION_CID,""),"");

	return -1;
}
/*
	pvs8 functions
*/

static void pvs8_initialize_prefmode(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	static int initialized = 0;
	/* This function is called by model_physinit() --> sierra_init() and model_physinit() can be called multiple times
	 * so prevent multiple calling here */
	if (initialized ) {
		return;
	}

	if (rdb_get_single(rdb_name(RDB_CDMA_PREFSET_INIT, ""), response, sizeof(response)) == 0 &&
		strcmp(response, "1") == 0) {
		SYSLOG_DEBUG("[pvs8] Prefmode was already initialized");
		initialized = 1;
		return;
	}

	SYSLOG_ERR("[pvs8] prefmode is initialising to - network='1x/HDR',roam='AutoB'");

	if(!send_atcmd_printf("AT^PREFMODE=%d",8)) {
		SYSLOG_ERR("[pvs8] failed in changing network preference - '1x/HDR'");
		return;
	}

	if(!send_atcmd_printf("AT^SCFG=\"CDMA/AutoAB\",\"%s\"","AutoB")) {
		SYSLOG_ERR("[pvs8] failed in changing roam preference mode - 'AutoB'");
		return;
	}
			
	/* Now prefmode is initialized so change flag */
	SYSLOG_ERR("[pvs8] Prefmode is initialized, set prefset_initialized flag to 1");
	rdb_set_single(rdb_name(RDB_CDMA_PREFSET_INIT, ""), "1");
	initialized = 1;
	return;
}

static int pvs8_init(void)
{
	/* disable tethered nai - required */
	send_atcmd("AT^SCFG=\"cdma/tetheredNai\",disabled");
	/* set PPP Network Layer Packet Data Service - this is required even for QMI net interface */
	send_atcmd("AT+CRM=2");

	/* subscribe rssi noti */
	send_atcmd("AT^SIND=\"rssi\",1");

	/* Enable the packet call state reporting. */
	send_atcmd("AT+CPSR=1");

	/* update revision */
	update_cinterion_firmware_revision();

	/* subscribe service noti and update */
	pvs8_poll_service_type();

	/* initialize prefered mode */
	pvs8_initialize_prefmode();

	return 0;
}

#ifdef GPS_ON_AT
/* gpsOne - periodic schedule */
static void pxs8_poll_gpsone_schedule(void* ref)
{
	pxs8_poll_gpsone(-1);

	/* check gpsone parameters - every one second */
	scheduled_func_schedule("pxs8_poll_gpsone_schedule",pxs8_poll_gpsone_schedule,10);
}

struct xtra_mem_t {
	char *memory;
	size_t size;
};

/* gpsOne - callback for download_xtra() */
static size_t download_xtra_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct xtra_mem_t *mem = (struct xtra_mem_t *)userp;
	
	update_heartBeat();
	
	/* reallocate */
	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
		/* out of memory! */ 
		syslog(LOG_ERR,"[msb-agps] not enough memory for xtra"); 
		goto err;
	}
	
	/* copy content */
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	
	/* zero at the end */
	mem->memory[mem->size] = 0;
	
	return realsize;
err:
	return 0;	
}

/* gpsOne - download xtra to memory */
static int download_xtra(char** xtra_ptr,int* xtra_len)
{
	CURL *curl_handle;
	CURLcode res=-1;
 
	struct xtra_mem_t chunk;
	
	/* init. chunk */
	chunk.memory = malloc(1);
	chunk.size = 0;
	
	char* urls;
	char* sp;
	char* t;
	
	
	urls=strdupa(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_URLS)));
	t=strtok_r(urls,";",&sp);
	while(t) {
		
		update_heartBeat();
		
		/* bypass if zero-length url is detected */
		if(!*t)
			goto fini_t;
			
		/* init the curl session */ 
		curl_handle = curl_easy_init();
		
		/* init curl handle */ 
		curl_easy_setopt(curl_handle, CURLOPT_URL, t);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, download_xtra_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30);
		
		/* get it! */ 
		res=curl_easy_perform(curl_handle);
		
		/* bypass if success */
		if(res==CURLE_OK) {
			syslog(LOG_INFO,"[msb-agps] xtra downloaded (url=%s,len=%d)",t,chunk.size);
			break;
		}
		
		syslog(LOG_INFO,"[msb-agps] failed in downloading xtra (url=%s) - %s",t,curl_easy_strerror(res));
		
		
		/* cleanup curl stuff */ 
		curl_easy_cleanup(curl_handle);
		
		/* we're done with libcurl, so clean it up */ 
		curl_global_cleanup();
		
	fini_t:
		t=strtok_r(NULL,";",&sp);
	}
	
	*xtra_ptr=chunk.memory;
	*xtra_len=chunk.size;
	
	if(res==CURLE_OK)
		return 0;
	
	return -1;
}

/* gpsOne - download xtra to memory */
static int inject_xtra(time_t now)
{		
	int xtra_stat;
	char* xtra_ptr;
	int xtra_len;
	
	char* resp;
	
	int total_written;
	int written;
	
	int block_len;
	
	
	xtra_ptr=NULL;
	
	/* download xtra */
	update_heartBeat();
	xtra_stat=download_xtra(&xtra_ptr,&xtra_len);
	update_heartBeat();
	
	/* !! do not use change_stage_XXX() from this point - until releasing xtra_stat memory !! */
	if(xtra_stat<0) {
		syslog(LOG_ERR,"[msb-agps] failed to download xtra file");
		goto err;
	}
	
	/* turn off gps engine xtra */
	resp=send_atcmd("AT^SGPSC=\"Engine\",\"0\"");
	if(!resp) {
		syslog(LOG_ERR,"[msb-agps] failed to disable GPS engine");
		goto err;
	}
	
	/* erase xtra */
	resp=send_atcmd("AT^SBNW=\"AGPS\",-1");
	if(!resp) {
		syslog(LOG_ERR,"[msb-agps] failed to remove existing xtra");
		goto err;
	}
	
	/* send AT command to start to inject xtra */
	send_atcmd_printf("AT^SBNW=\"AGPS\",%d",xtra_len);
	if(!strstr(_resp,"AGPS READY: SEND FILE")) {
		syslog(LOG_ERR,"[msb-agps] failed to initiate injecting of xtra");
		goto err;
	}
	
	syslog(LOG_INFO,"[msb-agps] injecting xtra (size=%d)",xtra_len);
	total_written=0;
	while(total_written<xtra_len) {
		/* sleep if buffer not enough */
		if(total_written)
			sleep(1);
		
		/* 1024 block */
		block_len=xtra_len-total_written;
		if(block_len>4*1024)
			block_len=4*1024;
		
		/* pour xtra to AT port */
		written=at_write_raw(xtra_ptr+total_written,block_len);
		if(written<0) {
			syslog(LOG_ERR,"[msb-agps] failed in inecting of xtra - %s",strerror(errno));
			goto err;
		}
		
		/* increase progress */
		total_written+=written;
		syslog(LOG_INFO,"[msb-agps] injecting progress - %d/%d",total_written,xtra_len);
		
		update_heartBeat();
	}
	
	/* wait */
	resp=wait_atcmd("AGPS END OK",10);
	if(!resp) {
		syslog(LOG_ERR,"[msb-agps] no reponse from GPS engine"); 
		goto err;
	}
	
	/* update last xtra download time */
	rdb_set_printf(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_UPDATED_TIME),"%d",now);
	
	
	syslog(LOG_INFO,"[msb-agps] xtra updated");
	
	/* free xtra ptr */
	free(xtra_ptr);
	
	return 0;
err:
	/* free xtra ptr */
	if(xtra_ptr)
		free(xtra_ptr);
		
	return -1;
}
				


static time_t get_epoch_time(int year,int month,int day)
{
	struct tm t;
	
	memset(&t,0,sizeof(t));
	
	t.tm_year=year-1900;
	t.tm_mon=month-1;
	t.tm_mday=day;
	
	tzset();
	return timegm(&t);
}

static clock_t get_now_sec()
{
	struct tms buf;
	clock_t clk;
	clock_t per_sec;
	
	clk=times(&buf);
	per_sec=sysconf(_SC_CLK_TCK);
	
	return clk/per_sec;
}

static int px8_set_gps_engine(int engine_val)
{
	char* resp;
	
	syslog(LOG_INFO,"[msb-agps] change gps engine mode (engine_val=%d)",engine_val);
			
	resp=send_atcmd_printf("AT^SGPSC=\"Engine\",\"%d\"",engine_val);
	if(!resp) {
		syslog(LOG_ERR,"[msb-agps] failed to change GPS engine configuration (engine_val=%d)",engine_val);
		goto err;
	}
	
	return 0;
	
err:
	return -1;	
}

static int pxs8_update_agps_info()
{
	time_t epoch;
	time_t xtra_expiry_time;
	time_t rtime;
	
	char* resp;
	char* resp2;
	
	char* p;
	char* raw_week;
	char* raw_min;
	char* raw_duration;
	int week;
	int min;
	int duration;
	
	rtime=cinterion_get_real_time_clock();
	epoch=get_epoch_time(1980,1,1);

	/* send info AT commnad */
	resp=send_atcmd("AT^SGPSC=\"Info\",\"Xtra\"");
	if(!resp) {
		syslog(LOG_ERR,"[msb-agps] failed to get GPS info");
		goto err;
	}

	resp2=strdupa(resp);

	/*
		^SGPSC: "Info","xtra","0","0","0"
	*/

	/* skip prefix */
	p=strstr(resp2,"^SGPSC: ");
	if(!p) {
		syslog(LOG_ERR,"[msb-agps] incorrect format of AT command result from ^SGPSC (resp=%s)",resp);
		goto err;
	}
	p+=STRLEN("^SGPSC: ");
	
	/* check info string */
	p=strtok(p,",");
	if(strcasecmp(p,"\"info\"")) {
		syslog(LOG_ERR,"[msb-agps] incorrect format of AT command result (parm#1) from ^SGPSC (resp=%s)",resp);
		goto err;
	}
	
	/* check xtra string */
	p=strtok(NULL,",");
	if(!p || strcasecmp(p,"\"xtra\"")) {
		syslog(LOG_ERR,"[msb-agps] incorrect format of AT command result (parm#2) from ^SGPSC (resp=%s)",resp);
		goto err;
	}
	
	/* got parameters - week */
	raw_week=strtok(NULL,",");
	if(!raw_week) {
		syslog(LOG_ERR,"[msb-agps] incorrect format of AT command result (parm#3) from ^SGPSC (resp=%s)",resp);
		goto err;
	}
	week=atoi(raw_week+1);
		
	/* got parameters - min */
	raw_min=strtok(NULL,",");
	if(!raw_min) {
		syslog(LOG_ERR,"[msb-agps] incorrect format of AT command result (parm#4) from ^SGPSC (resp=%s)",resp);
		goto err;
	}
	min=atoi(raw_min+1);
	
	/* got parameters - duration */
	raw_duration=strtok(NULL,",");
	if(!raw_duration) {
		syslog(LOG_ERR,"[msb-agps] incorrect format of AT command result (parm#5) from ^SGPSC (resp=%s)",resp);
		goto err;
	}
	duration=atoi(raw_duration+1);
	
	/* update xtra expiry time */
	xtra_expiry_time=rtime+duration*60;
	
	/* store parameters to rdb - gnss time */
	if(!min && !week) {
		rdb_set_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_GNSS_TIME),"");
	}
	else {
		rdb_set_printf(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_GNSS_TIME),"%d",epoch+(week*7*24*60+min)*60);
	}
	
	/* store parameters to rdb - valid time */
	if(!duration) 
		rdb_set_printf(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_VALID_TIME),"");
	else
		rdb_set_printf(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_VALID_TIME),"%d",xtra_expiry_time);
		
	return 0;
err:
	return -1;	
}

static int pxs8_poll_gpsone(int rdb_gps_en)
{
	enum {
		gpsone_stage_idle=0,
  
		gpsone_stat_init_check_update_time,
		gpsone_stat_check_update_time,
      		gpsone_stat_wait_update_time,
	
  		gpsone_stat_bootup_delay,
		gpsone_stat_wait_gps_release,
    
		gpsone_stat_download_init,
		gpsone_stat_download_now,
  		gpsone_stat_download_delay,
    
  		gpsone_stat_wait_in_stand_alone,
    		gpsone_stat_wait_in_offline,
      		gpsone_stat_wait_in_manual_mode,
	};
	
	static int gps_en=0;
	static int stage=gpsone_stage_idle;
	
	int rdb_gpsone_en;
	
	clock_t now_sec;
	time_t now;
		
	static int retry_cnt=0;
	static clock_t download_delay_clock=0;
	
	int rdb_period;
	int rdb_updated_time;
	int rdb_max_retry_cnt;
	int rdb_update_now;

	int rdb_retry_delay;
			
	static int gpsone_booting=1;
	static clock_t gpsone_booting_clock=0;
	static int gpsone_booting_clock_valid=0;
	
	int stat;
	
	const char* time_str;
	time_t update_time;
	
	
	#ifdef V_CELL_NW_umts
	const char* imei;
	static int imei_log=0;
	
	/* check imei - workaround for PHS-8P freezing issue when IMEI is not programmed */
	imei=rdb_get_value(rdb_name("",RDB_IMEI));
	if(!*imei || !strcasecmp(imei,"N/A")) {
	
		if(!imei_log)
			syslog(LOG_ERR,"[msb-agps] no IMEI found, bypass GPS feature (IMEI='%s')",imei);
		imei_log=1;
		
		goto err;
	}
	#endif
	
	/* store gps enable status */
	if(rdb_gps_en>=0)
		gps_en=rdb_gps_en;
	
	/* read rdbs */
	rdb_gpsone_en=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_EN)));
	
	if(rdb_gps_en) {
		pxs8_update_agps_info();
	}
	
#define change_stage_jump(s) \
		stage=s; \
		goto re_stage;
	
#define change_stage_switch(s) \
		stage=s; \
		goto fini;
		
	/* get base times */	
	now_sec=get_now_sec();
	time(&now);
	
	syslog(LOG_DEBUG,"[msb-agps] start staging procedure (staging=%d)",stage);
	
re_stage:
	update_heartBeat();
	
	syslog(LOG_DEBUG,"[msb-agps] start sub-staging procedure (staging=%d)",stage);
	
	/* auto-update period */
	rdb_period=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_AUTO_UPDATE_PERIOD)))*60;
	
	/* do pre-staging procedure - manual updating */
	rdb_update_now=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_UPDATE_NOW)));
	if(rdb_update_now) {
		syslog(LOG_INFO,"[msb-agps] start manual update");
		
		/* clear manual update flag */
		rdb_set_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_UPDATE_NOW),"");
		
		/* inject */
		stat=inject_xtra(now);
		
		/* update */
		pxs8_update_agps_info();
		
		if(stat<0) {
			syslog(LOG_ERR,"[msb-agps] manual update failed");
		}
		else {
			syslog(LOG_INFO,"[msb-agps] finish manual update");
		}
		
		/* set complete flag */
		rdb_set_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_UPDATED),(stat>=0)?"1":"0");
		
		change_stage_jump(gpsone_stage_idle);
	}
	
	/* maintain booting timer */
	if(gpsone_booting) {
		/* start timer */
		if(!gpsone_booting_clock_valid) {
			gpsone_booting_clock=now_sec;
			gpsone_booting_clock_valid=1;
					
			syslog(LOG_INFO,"[msb-agps] start bootup timer (delay=%d sec)",GPSONE_BOOTUP_DELAY);
		}
				
		/* wait until delay time */
		if(gpsone_booting_clock_valid && (now_sec-gpsone_booting_clock>=GPSONE_BOOTUP_DELAY)) {
			gpsone_booting=0;
			syslog(LOG_INFO,"[msb-agps] finish bootup timer");
		}
	}
	
	
	/* do pre-staging procedure - gps deactivation */
	switch(stage) {
		case gpsone_stat_wait_in_stand_alone:
		case gpsone_stat_wait_in_offline:
		case gpsone_stat_wait_in_manual_mode:
		case gpsone_stage_idle:
			break;
			
		default: {
			if(!rdb_gps_en) {
				syslog(LOG_INFO,"[msb-agps] gps deactivated");
				change_stage_jump(gpsone_stage_idle);
			}			
			else if(!rdb_gpsone_en) {
				syslog(LOG_INFO,"[msb-agps] agps deactivated");
				change_stage_jump(gpsone_stage_idle);
			}
			/* if automatic update disabled with gps and agps enabled */
			else if(!rdb_period) {
				syslog(LOG_INFO,"[msb-agps] automatic update disabled #1");
				change_stage_jump(gpsone_stat_wait_in_manual_mode);
			}
			
			break;
		}
	}
	
	/* do staging procedure */
	switch(stage) {
		case gpsone_stat_wait_update_time: {
			/* get rdbs */
			rdb_updated_time=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_UPDATED_TIME)));
			
			if(now-rdb_updated_time>=rdb_period*2) {
				syslog(LOG_ERR,"[msb-agps] update time expired (period=%d sec) - retry",rdb_period);
				change_stage_jump(gpsone_stat_bootup_delay);
			}
			break;
		}
		
		case gpsone_stat_download_delay: {
			
			rdb_retry_delay=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_AUTO_UPDATE_RETRY_DELAY)));
			
			/* if download delay expired */
			if(!rdb_retry_delay || (now_sec-download_delay_clock>=rdb_retry_delay)) {
				syslog(LOG_ERR,"[msb-agps] download retry delay expired (retry_delay=%d sec)",rdb_retry_delay);
				change_stage_jump(gpsone_stat_download_now);
			}
			
			break;
		}
		
		case gpsone_stat_download_now: {
			int stat;
			
			rdb_max_retry_cnt=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_AUTO_UPDATE_MAX_RETRY_CNT)));
			
			/* check retry count */
			if(rdb_max_retry_cnt && (retry_cnt>=rdb_max_retry_cnt)) {
				syslog(LOG_ERR,"[msb-agps] max retry count exceed (max_retry_cnt=%d)",rdb_max_retry_cnt);
				change_stage_switch(gpsone_stat_wait_update_time);
			}
			
			/* increase retry count */
			retry_cnt++;
			syslog(LOG_INFO,"[msb-agps] start download xtra #%d/%d",retry_cnt,rdb_max_retry_cnt);
			
			/* inject */
			stat=inject_xtra(now);
			
			/* activate gps */
			syslog(LOG_ERR,"[msg-agps] activate gps engine for gps/agps");
			if(px8_set_gps_engine(2)<0) {
				syslog(LOG_ERR,"[msg-agps] failed in activating gps engine mode");
				goto err_gpsone_stat_download_now;
			}
			
			/* finish based on result from inject_xtra */
			if(stat<0) {
				goto err_gpsone_stat_download_now;
			}
			
			
			/* print next update time */
			update_time=now+rdb_period;
			time_str=ctime(&update_time);
				
			syslog(LOG_INFO,"[msb-agps] update done - wait (period=%d sec,next_update_time='%s')",rdb_period,time_str);
			change_stage_switch(gpsone_stat_check_update_time);
			break;
			
		err_gpsone_stat_download_now:
			rdb_retry_delay=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_AUTO_UPDATE_RETRY_DELAY)));
			syslog(LOG_INFO,"[msb-agps] wait (retry_delay=%d sec)",rdb_retry_delay);
			
			/* switch to delay */
			download_delay_clock=now_sec;
			change_stage_switch(gpsone_stat_download_delay);
			break;
		}
		
		case gpsone_stat_download_init: {
			
			/* init retry count */
			retry_cnt=0;
			
			syslog(LOG_INFO,"[msb-agps] init. download");
			change_stage_jump(gpsone_stat_download_now);
			
			break;
		}
		
		case gpsone_stat_wait_gps_release: {
			const char* val;
			int locked;
			
			val=rdb_get_value("sensors.gps.0.assisted.valid");
			locked=!strcmp(val,"valid");
			
			/* if not valid */
			if(!locked) {
				syslog(LOG_INFO,"[msb-agps] gps not locked - immediately start updating xtra");
				change_stage_jump(gpsone_stat_download_init);
			}
			
			break;
		}
		
		case gpsone_stat_check_update_time: {
			/* get rdbs */
			rdb_updated_time=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_UPDATED_TIME)));
			
			if(now-rdb_updated_time>=rdb_period) {
				syslog(LOG_INFO,"[msb-agps] update time expired (period=%d sec) - start updating xtra",rdb_period);
				change_stage_jump(gpsone_stat_bootup_delay);
			}
			break;
		}
		
		case gpsone_stat_bootup_delay: {
			
			/* bypass init delay if not first time */
			if(!gpsone_booting) {
				syslog(LOG_INFO,"[msb-agps] check gps locking status");
				change_stage_jump(gpsone_stat_wait_gps_release);
			}
				
			break;
		}
		
		case gpsone_stat_wait_in_stand_alone: {
			if(!gps_en || rdb_gpsone_en) {
				syslog(LOG_INFO,"[msb-agps] gps/agps configuration detected #1 (gps=%d,gpsone=%d)",gps_en,rdb_gpsone_en);
				change_stage_jump(gpsone_stage_idle);
			}
			
			break;
		}
		
		case gpsone_stat_wait_in_offline: {
			if(gps_en) {
				syslog(LOG_INFO,"[msb-agps] gps/agps configuration detected #2 (gps=%d,gpsone=%d)",gps_en,rdb_gpsone_en);
				change_stage_jump(gpsone_stage_idle);
			}
			break;
		}
		
		case gpsone_stat_wait_in_manual_mode: {
			if(rdb_period) {
				syslog(LOG_INFO,"[msb-agps] automatic update enabled");
				change_stage_jump(gpsone_stat_init_check_update_time);
			}
			break;
		}

		case gpsone_stat_init_check_update_time: {
			/* calc. next update time */
			rdb_updated_time=atoi(rdb_get_value(rdb_name(RDB_GPS_PREFIX,RDB_GPS_GPSONE_XTRA_INFO_UPDATED_TIME)));
			if(!rdb_updated_time) {
				time_str="now";
			}
			else {
				update_time=rdb_updated_time+rdb_period;
				time_str=ctime(&update_time);
			}
			
			syslog(LOG_INFO,"[msb-agps] start agps update procedure - wait (period=%d sec,next_update_time='%s')",rdb_period,time_str);
			change_stage_jump(gpsone_stat_check_update_time);
			break;
		}
		
		case gpsone_stage_idle: {
			if(gps_en && rdb_gpsone_en) {
				syslog(LOG_INFO,"[msb-agps] start gps (agps mode)");
				px8_set_gps_engine(2);
				
				if(!rdb_period) {
					syslog(LOG_INFO,"[msb-agps] automatic update disabled #2");
					change_stage_jump(gpsone_stat_wait_in_manual_mode);
				}
				
				change_stage_jump(gpsone_stat_init_check_update_time);
			}
			else if(gps_en && !rdb_gpsone_en) {
				syslog(LOG_INFO,"[msb-agps] start gps (stand-alone mode)");
				px8_set_gps_engine(1);
				change_stage_jump(gpsone_stat_wait_in_stand_alone);
			}
			else {
				syslog(LOG_INFO,"[msb-agps] stop gps (off-line mode)");
				px8_set_gps_engine(0);
				change_stage_jump(gpsone_stat_wait_in_offline);
			}
			
			break;
		}
		
	}
	
fini:
	syslog(LOG_DEBUG,"[msb-agps] finish staging procedure (staging=%d)",stage);
	
	return 0;
	
#ifdef V_CELL_NW_umts	
err:
	return -1;	
#endif	
}

#endif


/*
	PHS modules structures
*/

const struct notification_t phs8p_noti_struc[] = {
	/* sms noti functions from default model */
	{.name="+CMT:",.action=notiSMSRecv},
	{.name="+CMTI:",.action=handleNewSMSIndicator},
#ifdef USSD_SUPPORT
	{ .name = "+CUSD:",.action=handle_ussd_notification},
#endif
	/* phs-8p noti functions */
	{.name="+CIEV: rssi",.action=cinterion_noti_ciev_rssi},
	{.name="+CIEV: psinfo",.action=phs8p_noti_ciev_psinfo},
	{.name="+CIEV: nitz",.action=cinterion_noti_ciev_nitz},

	{.name="+CGREG:",.action=cinterion_noti_cgreg},
	{.name="+CREG:",.action=cinterion_noti_creg},

	/* termination */
	{0, }
};

const struct command_t phs8p_cmd_struc[] = {
	{.name=RDB_PLMNCOMMAND,.action=cinterion_cmd_plmn },
	{.name=RDB_BANDCMMAND,.action=phs8p_cmd_band },
	{.name=RDB_QXDM_COMMAND,.action=phs8_cmd_qxdm},
	{.name=RDB_NWCTRLCOMMAND,.action=default_handle_command_nwctrl},
	{.name = RDB_SIMCMMAND,.action=cinterion_handle_command_sim},
#ifdef GPS_ON_AT
	{.name=RDB_GPS_PREFIX".0."RDB_GPS_CMD,.action=phs8p_cmd_gps },
#endif
#ifdef FORCED_REGISTRATION //Vodafone force registration
	{.name ="vf.force.registration",.action=cinterion_handle_command_force_reg},
#endif
	{0,}
};

struct model_t model_phs8p= {
	.name = "cinterion",

	.init = cinterion_init,
	.detect = phs8p_det,

	.get_status = model_default_get_status,
	.set_status = pvs8_set_stat,

	.commands = phs8p_cmd_struc,
	.notifications = phs8p_noti_struc
};

/*
	BGS modules structures
*/

const struct notification_t bgs2e_noti_struc[] = {
	/* sms noti functions from default model */
	{.name="+CMT:",.action=notiSMSRecv},
	{.name="+CMTI:",.action=handleNewSMSIndicator},
#ifdef USSD_SUPPORT
	{ .name = "+CUSD:",.action=handle_ussd_notification},
#endif
	{.name="+CIEV: rssi",.action=cinterion_noti_ciev_rssi},
	{.name="+CIEV: nitz",.action=cinterion_noti_ciev_nitz},

	{.name="+CGREG:",.action=cinterion_noti_cgreg},
	{.name="+CREG:",.action=cinterion_noti_creg},

	/* termination */
	{0, }
};

const struct command_t bgs2e_cmd_struc[] = {
	{.name=RDB_PLMNCOMMAND,.action=cinterion_cmd_plmn },
	{.name=RDB_BANDCMMAND,.action=bgs2e_cmd_band },
	{.name=RDB_NWCTRLCOMMAND,.action=default_handle_command_nwctrl},
	{.name =RDB_SIMCMMAND,.action=cinterion_handle_command_sim},
	{0,}
};

struct model_t model_bgs2e= {
	.name = "cinterion2G",

	.init = cinterion_init,
	.detect = bgs2e_det,

	.get_status = model_default_get_status,
	.set_status = pvs8_set_stat,

	.commands = bgs2e_cmd_struc,
	.notifications = bgs2e_noti_struc
};


/*
	PVS8 structures
*/

struct command_t pvs8_cmd_struc[] = {
	{.name = RDB_QXDM_COMMAND,.action=pvs8_cmd_qxdm},
	{.name = RDB_CDMA_OTASP_CMD,.action=pvs8_cmd_otasp},
	{.name = RDB_CDMA_PREFSET_CMD,.action=pvs8_cmd_prefset},
 	{.name = RDB_CDMA_MIPCMMAND,.action=pvs8_cmd_mip },
  
#ifdef GPS_ON_AT
	{ .name = RDB_GPS_PREFIX".0."RDB_GPS_CMD,.action=phs8p_cmd_gps },
#endif
	{0,}
};


const struct notification_t pvs8_noti_struc[]= {
	/* sms noti functions from default model */
	{.name="+CMT:",.action=notiSMSRecv},
	{.name="+CMTI:",.action=handleNewSMSIndicator},
	{.name="NO CARRIER",.action=pvs8_noti_nocarrier},
	{.name="ACTIVATION:",.action=pvs8_noti_activation},
	{.name = "+CIEV: rssi",.action=pvs8_noti_ciev_rssi},
	{.name="+CIEV: psinfo",.action=pvs8_noti_ciev_psinfo},
	{.name="+CPSR:",.action=pvs8_noti_cpsr},
	{0,}
};


struct model_t model_pvs8= {
	.name = "cinterion_cdma",

	.init = cinterion_init,
	.detect = pvs8_det,

	.get_status = pvs8_get_stat,
	.set_status = pvs8_set_stat,

	.commands = pvs8_cmd_struc,
	.notifications = pvs8_noti_struc
};

