
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <ctype.h>
#include <unistd.h>
//#include <fcntl.h>
//#include <termios.h>
#include <errno.h>
#include <syslog.h>
//#include <sys/ioctl.h>
#include <signal.h>
//#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
//#include <sys/mman.h>
//#include <net/if.h>
//#include <linux/sockios.h>
//#include <linux/mii.h>
//#include <sys/stat.h>
//#include <sys/resource.h>
//#include <linux/types.h>
#include <sys/times.h>
#include <libgen.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/if.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>

#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <alloca.h>


#include "logger.h"
#include "daemon.h"
#include "rdb_ops.h"
#include "tickcount.h"

#include "passgen.h"

/* Verison Information */
#define VER_MJ		0
#define VER_MN		1
#define VER_BLD	1

/* Local data */
#define MAX_CONNECTIONS	20
// seconds to wait until ip-down scripts finish
#define CONNECTION_SCRIPT_TERMINATE_DELAY 10
#define MAX_PROFILES	64
#define INACTIVE	-1
#define MAX_PROFILE_RDB		32
#define MAX_PROFILE_RDB_VAL_LEN	128

#define DAEMON_NAME "connection_mgr"
/* The user under which to run */
#define RUN_AS_USER "system"


#define CONNECTION_INIT_DELAY	(3*60)

/* RDB:link.profile "delflag" values */
#define PROFILE_NOT_NEEDED 1
#define PROFILE_DEFUNCT    2


/*
profile enable state
-1	--  the profile is ignored, not monitor at all
0	--  the profile is monitored and disconnected
1	--  the profile is enabled, and start to connecting
*/
#define PROFILE_IGNORED -1
#define PROFILE_DISABLED 0
#define PROFILE_ENABLED 1


#define run_once_define(name)		static int run_once_##name=0
#define run_once_reset(name)		do { run_once_##name=0; } while(0)

#define run_once_func(name,func)	\
	do \
	{ \
		if(!run_once_##name) { \
			func \
			run_once_##name=1; \
		} \
	} while(0)

#define run_once_define_array(name,max)		static int run_once_##name[max]={0,}
#define run_once_func_array(name,idx,func)	\
	do \
	{ \
		if(!run_once_##name[idx]) { \
			func \
			run_once_##name[idx]=1; \
		} \
	} while(0)
#define run_once_reset_array(name,idx)		do { run_once_##name[idx]=0; } while(0)




#define PRCHAR(x) (((x)>=0x20&&(x)<=0x7e)?(x):'.')

#define xtod(c) ((c>='0' && c<='9') ? c-'0' : ((c>='A' && c<='F') ? \
                 c-'A'+10 : ((c>='a' && c<='f') ? c-'a'+10 : 0)))


#define SAFE_STRNCPY(dst,src,dstsize) \
	do { \
		if (src) { \
			strncpy(dst,src,dstsize); \
			dst[(dstsize)-1]=0; \
		} \
		else { \
			dst[0]=0; \
		} \
	} while(0)

typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

struct connection
{
	int		profile;
	pid_t		pid;
	int		sub_interface;

	clock_t clkTerm;
	int clkTermValid;

	clock_t clkStart;
	int clkStartValid;

	int connectProvisional;

	int sigterm_sent;
	clock_t sigterm_sent_time;

};


struct _glb
{
	int			run;
	struct connection	active_connections[MAX_CONNECTIONS];
}glb;

//#define DEBUG_MEP

struct profile_t {
	char rdb_str[MAX_PROFILE_RDB][MAX_PROFILE_RDB_VAL_LEN];
	char rdb_int[MAX_PROFILE_RDB];

	int trigger;
};

struct profile_t profile_info[MAX_PROFILES];

/* default WWAN profile */
static int default_wwan_profile = INACTIVE;
/* previous default WWAN profile */
static int prev_default_wwan_profile = INACTIVE;

// code suffixes
#define PASSGEN_SUFFIX_MEP_CODE		"_NetworkUnlockCode"	// ${IMEI}_NetworkUnlockCode
#define PASSGEN_SUFFIX_MNCMCC_CODE	"_NetCommMCCMNC"		// ${MNC}${MCC}_NetCommMCCMNC

// length
#define MAX_MNCMCC_CODE_COUNT   50   // total mncmcc count
#define MEP_LOCK_DIGITS         10   // mep lock digit count
#define MNCMCC_CODE_DIGITS      10
#define MAX_IMEI_LENGTH			20

// directory information
#ifdef PLATFORM_AVIAN
	#define CONFIG_DIRECTORY     "/system/cdcs/etc/cdcs/conf"
	#define STORAGE_DIRECTORY    "/data/cdcs"
#else
	#define CONFIG_DIRECTORY     "/etc/cdcs/conf"
	#define STORAGE_DIRECTORY    "/etc/cdcs"
#endif
	#define STORAGE_DIRECTORY2    "/system/cdcs/etc"

#define MEPUNLOCKFILE_PREFIX "mepunlock-"
#define MEPUNLOCKFILE_SUFFIX ".key"
#define MNCMCC_CODE_FILENAME "mncmcc.dat"

#define MAX_DB_VALUE_LENGTH        128
#define MAX_SECRET_DIGIT_LENGTH    30

#define STRLEN(x)  (sizeof(x)-1)
#define TERMSTR(s) (s[STRLEN(s)]=0)

// database variable names
#define DB_MEPLOCK_STATUS  "meplock.status"
#define DB_MEPLOCK_STATUS_CHECK_IMEI  "checking IMEI"
#define DB_MEPLOCK_STATUS_CHECK_SIM   "checking SIM"
#define DB_MEPLOCK_STATUS_UNLOCKED    "unlocked"
#define DB_MEPLOCK_STATUS_LOCKED      "locked"
#define DB_MEPLOCK_STATUS_OK          "ok"

#define DB_MEPLOCK_CODE    "meplock.code"
#define DB_MEPLOCK_RESULT  "meplock.result"

const char shortopts[] = "pdvs:V?";

static int _mep_locked=0;
static int _wwan_ready_to_connect=0;	// true when 3g connection is ready (sim ready and network attached)
static clock_t _now;			// current clock (only avaiable in main loop)
static clock_t _clock_per_second=0;	// clock information - clock per seccond
static int _wwan_id=0;

static char _mccmnc_codes[MAX_MNCMCC_CODE_COUNT][MNCMCC_CODE_DIGITS+1];

/* This indirectly imports from V_CUSTOM_FEATURE_PACK. All the features are enabled at compiling time
*/
 #define MAX_SPEC	4
#if defined(V_TELSTRA_SPEC_Cinterion_module)
static int _default_spec=1;
#elif defined V_TELSTRA_SPEC_Santos
static int _default_spec=2;
#elif defined V_TELSTRA_SPEC_Sierra_LTE_module
static int _default_spec=3;
#else
static int _default_spec=0;
#endif

static int SPEC_NONE=0;
static int TELSTRA_SPEC_Cinterion_module=0;
static int TELSTRA_SPEC_Santos=0;
static int TELSTRA_SPEC_Sierra_LTE_module=0;

static int *_spec_list[MAX_SPEC+1]={
	&SPEC_NONE,
	&TELSTRA_SPEC_Cinterion_module,
	&TELSTRA_SPEC_Santos,
	&TELSTRA_SPEC_Sierra_LTE_module,
};

#ifdef PLATFORM_AVIAN
const char* wwan_if="rmnet0";
#else
const char* wwan_if=0;
#endif

#define SANTOS_ESM_RECONNECT_DELAY 30

int is_interface_up(const char* szIfNm);
int is_wwan_profile(int profile, int *wwan_id);
int is_dod_enabled();
int is_dod_up();
const char* get_connection_script(int profile,int instance,int end_script);
int is_profile_changed(int profile_no,const struct profile_t* profile_info_1,const struct profile_t* profile_info_2);
int read_profile(int profile_no,struct profile_t* profile_info);
int set_profile_connecting_status(int pf,int stat);
int setProfileVariableStr(int iPf, const char* szVariable, char* achValue);
const char* get_global_script();
void checkSetProfileDefunct(int profile);
void resetTelstraReconnectCount(int profile);

const char* profile_rdb_int[]={
	"link.profile.%d.reconnect_retries",
	"link.profile.%d.autoapn",
	"link.profile.%d.verbose_logging",
	"link.profile.%d.auth_type",
	"link.profile.%d.defaultroutemetric",
	"link.profile.%d.reconnect_delay",
	"link.profile.%d.userpeerdns",
	"link.profile.%d.snat",
	"link.profile.%d.mppe_en",
	"dialondemand.ignore_win7",
	"dialondemand.ignore_icmp",
	"dialondemand.ignore_tcp",
	"dialondemand.ignore_udp",
	"dialondemand.ignore_ntp",
	"dialondemand.poweron",
	"dialondemand.dod_verbose_logging",
	"dialondemand.deactivation_timer",
	"dialondemand.min_online",
	"dialondemand.periodic_online_random",
	"dialondemand.enable",
	"dialondemand.ports_en",
	"dialondemand.periodic_online",
	"dialondemand.traffic_online",
	"dialondemand.dial_delay",
	"dialondemand.profile",
	"link.profile.%d.preferred_ip.enable",
	NULL
};

const char* profile_rdb_str[]={
	"link.profile.%d.conntype",
	"link.profile.%d.dialstr",
	"link.profile.%d.dev",
	"link.profile.%d.apn",
	"link.profile.%d.enable",
	"link.profile.%d.defaultroute",
	"link.profile.%d.user",
	"link.profile.%d.pass",
	"link.profile.%d.conn_type",
	"dialondemand.ports_list",
	"link.profile.%d.routes",
	"link.profile.%d.opt",
 	"link.profile.%d.certi",
	"link.profile.%d.mip_mode",
	"link.profile.%d.preferred_ip.addr",
	NULL,
};

// a global RDB session handle
static struct rdb_session *g_rdb_session;

int HextoDec(char *hex)
{
	if (*hex == 0) return 0;
	return  HextoDec(hex -1)*16 +  xtod(*hex) ;
}
int xstrtoi(char *hex)    // hex string to integer
{
	return HextoDec(hex + strlen(hex) - 1);
}

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static clock_t __getTickCount(void)
{
	struct tms tm;

	return times(&tm);
}

////////////////////////////////////////////////////////////////////////
static const char* rdb_get_value(const char* variable)
{
	int stat;
	static char value[MAX_DB_VALUE_LENGTH];

	value[0]=0; // otherwise, will contain previous string (e.g. from last call to rdb_get_value)
	stat = rdb_get_string(g_rdb_session, (char*)variable, value, sizeof(value));

	(void)stat; // Disable 'unused' warning
	// disable this as it spams the log due to reading non-existent variables.
	// TODO - investigate if there is a real need to read all these non-existent variables.
//	if (stat<0)
//		syslog(LOG_INFO,"failed to rdb_get_string %s - %s",variable,strerror(errno));

	return value;
}

static int rdb_get_Int(const char* variable)
{
	return atoi(rdb_get_value(variable));
}

////////////////////////////////////////////////////////////////////////
static int rdb_set_value(const char* variable,const char* value)
{
	int stat;

	if( (stat=rdb_set_string(g_rdb_session, (char*)variable, value))<0)
	{
		stat=rdb_create_string(g_rdb_session, (char*)variable, value, CREATE, ALL_PERM);
		if(stat<0)
			syslog(LOG_ERR,"failed to rdb_create_string %s - %s",variable,strerror(errno));
	}

	return stat;
}
/*wrapper function to rdb_get_string.
If the rdb name contain 'wwan.0.', the 'wwan.0' will be replaced with 'wwan."id"

wwan_id	-- the "id" in RDB "wwan.id."
szName	-- RDB variable name
pValue	-- data read from RDB
len	-- the maximum pValue buffer length
$ 0 -- success
$ <0 -- error code
'*/
int rdb_get_i_string(int wwan_id, const char* szName, char* pValue, int len)
{
	char  name[MAX_DB_VALUE_LENGTH];
	if(wwan_id > 0 && strncmp(szName, "wwan.0.",7) ==0){
		sprintf(name, "wwan.%d.", wwan_id);
		strcat(name, &szName[7]);
	}else{
		strcpy(name, szName);
	}

	return rdb_get_string(g_rdb_session, name, pValue, len);
}

/*similar to rdb_get_value,
except if the rdb name contain 'wwan.0.', the 'wwan.0' will be replaced with 'wwan."id"'
wwan_id	-- the "id" in RDB "wwan.id."
variable -- RDB variable name
$ empty string  -- if reading failed, or RDB is empty.
$ string  -- the RDB value
*/
static const char* rdb_get_i_value(int wwan_id, const char* variable)
{
	static char value[MAX_DB_VALUE_LENGTH];
	value[0]=0; // otherwise, will contain previous string (e.g. from last call to rdb_get_value)
	rdb_get_i_string(wwan_id, (char*)variable, value, sizeof(value));
	return value;
}

////////////////////////////////////////////////////////////////////////
/*similar to rdb_set_value,
except if the rdb name contain 'wwan.0.', the 'wwan.0' will be replaced with 'wwan."id"'
wwan_id	-- the "id" in RDB "wwan.id."
variable-- RDB variable name
value	-- RDB value to be set
$ 0 -- success
$ <0 -- error code
*/
static int rdb_set_i_value(int wwan_id, const char* variable,const char* value)
{
	int stat;
	char  name[MAX_DB_VALUE_LENGTH];

	if(wwan_id > 0 && strncmp(variable, "wwan.0.",7) ==0){
		sprintf(name, "wwan.%d.", wwan_id);
		strcat(name, &variable[7]);
	}else{
		strcpy(name, variable);
	}

	if( (stat=rdb_set_string(g_rdb_session, (char*)name, value))<0)
	{
		stat=rdb_create_string(g_rdb_session, (char*)name, value, CREATE, ALL_PERM);
		if(stat<0)
			syslog(LOG_ERR,"failed to rdb_create_string %s - %s",name,strerror(errno));
	}

	return stat;
}
////////////////////////////////////////////////////////////////////////
const char* generate_secret_digits(const char* seed, int digits)
{
	static char secret_digits[MAX_SECRET_DIGIT_LENGTH+1];

	// return err if too big
	if(!(digits<MAX_SECRET_DIGIT_LENGTH))
		return 0;

	// init
	pg_init_string((const unsigned char*)seed,strlen(seed));

	// put 4 digit
	int i=0;
	while((i<digits) && (i<sizeof(secret_digits)))
	{
		secret_digits[i]=pg_ascii_range(PG_ASCII_DIGITS);
		i++;
	}
	secret_digits[i]=0;

	return secret_digits;
}

////////////////////////////////////////////////////////////////////////
const char* generate_mccmnc_code(const char* imei,const char* mnc,const char* mcc)
{
	char mccmnc_seed[3+3+STRLEN(PASSGEN_SUFFIX_MNCMCC_CODE)+1+MAX_IMEI_LENGTH+1]; // mnc + mcc + suffix + cr + null + imei

	snprintf(mccmnc_seed,sizeof(mccmnc_seed),"%s %s_%s%s\n",mnc,mcc,imei,PASSGEN_SUFFIX_MNCMCC_CODE);
	TERMSTR(mccmnc_seed);

#ifdef DEBUG_MEP
	log_ERR("mncmcc seed = %s",mccmnc_seed);
#endif

	return generate_secret_digits(mccmnc_seed,MNCMCC_CODE_DIGITS);
}

////////////////////////////////////////////////////////////////////////
const char* generate_mep_code(int wwan_id, char* ret_imei)
{
	const char* imei;

	char mep_seed[STRLEN(PASSGEN_SUFFIX_MEP_CODE)+1+1 +MAX_IMEI_LENGTH] ; // imei + suffix + cr + null + alpha for weird imei
	char paranoid_imei[1 +MAX_IMEI_LENGTH];
	int i;

	// read IMEI
	for(i=0; i<20; i++) {
		imei = rdb_get_i_value(wwan_id, "wwan.0.imei");
		if( strlen(imei) )
			break;
		sleep(2);
	}

	if (!strlen(imei)) {
		log_ERR("failed to read wwan.0.imei - %s",strerror(errno));
		return 0;
	}

	if(ret_imei)
	{
		strncpy(ret_imei,imei,MAX_IMEI_LENGTH);
		ret_imei[MAX_IMEI_LENGTH]=0;
	}

	strncpy(paranoid_imei,imei,sizeof(paranoid_imei));
	TERMSTR(paranoid_imei);

	// get seed
	snprintf(mep_seed,sizeof(mep_seed),"%s%s\n",paranoid_imei,PASSGEN_SUFFIX_MEP_CODE);
	TERMSTR(mep_seed);

#ifdef DEBUG_MEP
	log_ERR("mep_seed seed = %s",mep_seed);
#endif

	return generate_secret_digits(mep_seed,MEP_LOCK_DIGITS);
}

////////////////////////////////////////////////////////////////////////
int lookup_mncmcc_code(const char* imei, const char* mnc,const char* mcc)
{
	int i;
	const char* mncmcc_code;

	mncmcc_code=generate_mccmnc_code(imei,mnc,mcc);

#ifdef DEBUG_MEP
	log_ERR("current mnc=%s,mcc=%s,mncmcc_code=%s",mnc,mcc,mncmcc_code);
#endif

	i=0;
	while( (i<MAX_MNCMCC_CODE_COUNT) && *_mccmnc_codes[i] )
	{
#ifdef DEBUG_MEP
		log_ERR("comparing %s to current mncmcc(%s)",_mccmnc_codes[i], mncmcc_code);
#endif

		if(!strcmp(_mccmnc_codes[i],mncmcc_code))
		{
#ifdef DEBUG_MEP
			log_ERR("mccmnc matched");
#endif
			return !0;
		}

		i++;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////
int update_mncmcc_codes()
{
	FILE* fp;
	char line[1024];
	int i;
	const char* mncmcc_code_file;


	mncmcc_code_file=CONFIG_DIRECTORY "/" MNCMCC_CODE_FILENAME;

#ifdef DEBUG_MEP
		log_ERR("mncmcc code file = %s",mncmcc_code_file);
#endif

	// reset mccmnc codes
	memset(&_mccmnc_codes,0,sizeof(_mccmnc_codes));

	// open mncmcc code file
	for( i=0; i<10; i++ ) {
		fp=fopen(mncmcc_code_file,"r");
		if(fp)
			break;
		sleep(2);
	}

	if(!fp) {
		log_ERR("failed to open file %s - %s",MNCMCC_CODE_FILENAME,strerror(errno));
		return -1;
	}

	// read mncmcc code file
	i=0;
	while( (i<MAX_MNCMCC_CODE_COUNT) && fgets(line,	sizeof(line),fp) ) {
		// bypass empty line
		if(!strlen(line))
			continue;

		// copy to mncmcc
		strncpy(_mccmnc_codes[i],line,sizeof(_mccmnc_codes[i]));
		TERMSTR(_mccmnc_codes[i]);

#ifdef DEBUG_MEP
		log_ERR("read a mncmcc code - %s",_mccmnc_codes[i]);
#endif

		i++;
	}

	// close mncmcc code file
	fclose(fp);

	return 0;
}

////////////////////////////////////////////////////////////////////////
int fileexists(const char* filename)
{
	struct stat sb;

	return !(stat(filename,&sb)<0);
}

////////////////////////////////////////////////////////////////////////
int rdb_wait(int timeout)
{
	fd_set fdR;
	int fd;

	struct timeval tv={0,0};

	fd = rdb_fd(g_rdb_session);

	FD_ZERO(&fdR);
	FD_SET(fd,&fdR);

	tv.tv_sec=timeout;

	// select
	if(select(fd+1,&fdR,NULL,NULL,&tv)<=0)
		return -1;

	return 0;
}
////////////////////////////////////////////////////////////////////////
int touch(const char* filename)
{
	FILE* fp;

#ifdef DEBUG_MEP
	log_ERR("creating file = %s",filename);
#endif

	fp=fopen(filename,"w+");
	if(fp)
	{
		fprintf(fp,"DO NOT DELETE THIS FILE.\n");
		fprintf(fp,"IF YOU DELETE THIS FILE, YOU ARE GOING TO BE ASEKD MEP LOCK CODE.\n");
		fclose(fp);
	}
	else
	{
#ifdef DEBUG_MEP
		log_ERR("failed to create a file - %s",filename);
#endif
		return -1;
	}


	return 0;
}

////////////////////////////////////////////////////////////////////////
int force_down_network()
{
	// This function is Avian specific for MEP-LOCKED. Currently, this MEP-LOCKED feature is
	// limited in Avian.

	if(!wwan_if)
		return 1;

	if(is_interface_up(wwan_if))
	{
		log_ERR("network mep locked - turn down %s",wwan_if);
		system("NetCfg stop");
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////
const char* get_mep_code_filename(int wwan_id, char* unit_mep_code,char* imei)
{
	const char* mep_code;
	static char mep_filename[STRLEN(STORAGE_DIRECTORY "/" MEPUNLOCKFILE_PREFIX MEPUNLOCKFILE_SUFFIX) + MEP_LOCK_DIGITS + 1];

	// get secret key
	mep_code=generate_mep_code(wwan_id, imei);

	// check mep lock
	if(!mep_code)
		return 0;

	// return mep_code
	if(unit_mep_code)
	{
		strncpy(unit_mep_code,mep_code,MEP_LOCK_DIGITS);
		unit_mep_code[MEP_LOCK_DIGITS]=0;
	}

	// generate mep lock filename
	snprintf(mep_filename,sizeof(mep_filename),"%s/%s%s%s",STORAGE_DIRECTORY,MEPUNLOCKFILE_PREFIX,mep_code,MEPUNLOCKFILE_SUFFIX);
	TERMSTR(mep_filename);

	return mep_filename;
}
////////////////////////////////////////////////////////////////////////
const char* get_mep_code_filename2(int wwan_id, char* unit_mep_code,char* imei)
{
	const char* mep_code;
	static char mep_filename[STRLEN(STORAGE_DIRECTORY2 "/" MEPUNLOCKFILE_PREFIX MEPUNLOCKFILE_SUFFIX) + MEP_LOCK_DIGITS + 1];

	// get secret key
	mep_code=generate_mep_code(wwan_id, imei);

	// check mep lock
	if(!mep_code)
		return 0;

	// return mep_code
	if(unit_mep_code)
	{
		strncpy(unit_mep_code,mep_code,MEP_LOCK_DIGITS);
		unit_mep_code[MEP_LOCK_DIGITS]=0;
	}

	// generate mep lock filename
	snprintf(mep_filename,sizeof(mep_filename),"%s/%s%s%s",STORAGE_DIRECTORY2,MEPUNLOCKFILE_PREFIX,mep_code,MEPUNLOCKFILE_SUFFIX);
	TERMSTR(mep_filename);

	return mep_filename;
}
////////////////////////////////////////////////////////////////////////
int handle_commands(int wwan_id)
{
	char mep_code[MEP_LOCK_DIGITS+1];
	char user_mep_code[MEP_LOCK_DIGITS+1];
	const char* mep_filename;
	const char* mep_filename2;

	// TODO:
	// since connection mgr does not have any other command than mepcode
	// currently it does not handle other command but eventually it has to
	// read the current trigger list

	log_INFO("mep command caught");

	// get user input mep code
	strncpy(user_mep_code,rdb_get_value(DB_MEPLOCK_CODE),sizeof(user_mep_code));
	TERMSTR(user_mep_code);

#ifdef DEBUG_MEP
	log_ERR("user mep code = %s",user_mep_code);
#endif

	// get my own ge
	mep_filename = get_mep_code_filename(wwan_id, mep_code,0);
	mep_filename2 = get_mep_code_filename2(wwan_id, mep_code,0);

	// check mep lock
	if(!mep_filename || !mep_filename2 || !strlen(mep_filename) || !strlen(mep_filename2))
	{
		log_INFO("error - empty IMEI");
		rdb_set_value(DB_MEPLOCK_RESULT,"error - empty IMEI");
		return -1;
	}

#ifdef DEBUG_MEP
	log_ERR("unit mep code = %s user = %s",mep_code,user_mep_code);
#endif

	if(strncmp(mep_code,user_mep_code,sizeof(mep_code)))
	{
		log_INFO("error - MEP code not matched");
		rdb_set_value(DB_MEPLOCK_RESULT,"error - MEP code not matched");
		return -1;
	}

	if(touch(mep_filename)<0 || touch(mep_filename2)<0)
	{
		log_INFO("error - storage failure");
		rdb_set_value(DB_MEPLOCK_RESULT,"error - storage failure");
		return -1;
	}

	log_INFO("mep code matched");
	rdb_set_value(DB_MEPLOCK_RESULT,"success");

	return 0;
}

////////////////////////////////////////////////////////////////////////
/*
 update MEP lock status for one WWAN profile only.
 wwan_id -- the "id" in RDB "wwan.id."
*/
int update_mep_status(int wwan_id)
{
	char mnc[4];
	char mcc[4];

	const char* mep_filename;
	char imei[MAX_IMEI_LENGTH+1];

	*imei=0;
	mep_filename=get_mep_code_filename(wwan_id, 0,imei);

	// check mep lock
	if(!strlen(mep_filename))
	{
		rdb_set_value(DB_MEPLOCK_STATUS, DB_MEPLOCK_STATUS_CHECK_IMEI);
		return 0;
	}

#ifdef DEBUG_MEP
	log_ERR("mep unlock file - %s",mep_filename);
#endif

	// if unlocked
	if(fileexists(mep_filename))
	{
		#ifdef DEBUG_MEP
			log_ERR("mep unlocked");
		#endif

		rdb_set_value(DB_MEPLOCK_STATUS,DB_MEPLOCK_STATUS_UNLOCKED);
		return 0;
	}

	// get mnc
	strncpy(mnc,rdb_get_i_value(wwan_id, "wwan.0.imsi.plmn_mnc"),sizeof(mnc));
	TERMSTR(mnc);
	// get mcc
	strncpy(mcc,rdb_get_i_value(wwan_id, "wwan.0.imsi.plmn_mcc"),sizeof(mcc));
	TERMSTR(mcc);

	// if mnc and mcc not ready
	if(!strlen(mnc) || !strlen(mcc))
	{
		rdb_set_value(DB_MEPLOCK_STATUS,DB_MEPLOCK_STATUS_CHECK_SIM);
		return 0;
	}

		#ifdef DEBUG_MEP
			log_ERR("imei = %s",imei);
		#endif

	// lookup
	if(lookup_mncmcc_code(imei,mnc,mcc))
	{
		rdb_set_value(DB_MEPLOCK_STATUS,DB_MEPLOCK_STATUS_OK);
		return 0;
	}

	// locked
	rdb_set_value(DB_MEPLOCK_STATUS,DB_MEPLOCK_STATUS_LOCKED);

	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int is_exist_interface_name(const char *if_name)
{
	int fd;
	char device[255];
	sprintf(device, "/sys/class/net/%s", if_name);
	fd = open(device, O_RDONLY, 0);
	if (fd < 0)
		return 0;
	else
		close(fd);
	return 1;
}
////////////////////////////////////////////////////////////////////////////////
// This function gets the ifreq structure and returns 1 if any of the flags in bitmask specified are set
int check_bit_in_ifrflags(const char* szIfNm, int bitmask )
{
	struct ifreq ifr;
	int sockfd;

	if ( !is_exist_interface_name(szIfNm) )
	{
		return 0;
	}

	// NOTE - that this socket is just created to be used as a hook in to the socket layer
	// for the ioctl call.
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd > 0)
	{
		strncpy(ifr.ifr_name, szIfNm, sizeof(ifr.ifr_name)-1);	// ifr_name is a small array of characters but string functions are used
		ifr.ifr_name[sizeof(ifr.ifr_name)-1] = 0;		// in the kernel so it must be null terminated
		char* pDot=strchr(ifr.ifr_name,'.');
		if(pDot)
			pDot=0;

		if (ioctl(sockfd, SIOCGIFINDEX, &ifr) != -1)
		{
			if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1)
			{
				close(sockfd);
				//fprintf(stderr, "returning 0 in second ioctl call\r\n");
				return 0;
			}
			else
			{
				close(sockfd);
				if (ifr.ifr_flags & bitmask)
				{
					//fprintf(stderr, "PPP UP\r\n");
					return 1;
				}
				else
				{
					return 0;
				}
			}
		}
		close(sockfd);
	}

	return 0;
}

int is_interface_up(const char* szIfNm)
{
    return check_bit_in_ifrflags(szIfNm, IFF_UP);
}
////////////////////////////////////////////////////////////////////////////////
int is_interface_running(const char* szIfNm)
{
    return check_bit_in_ifrflags(szIfNm, IFF_RUNNING);
}

void usage(char **argv)
{
	fprintf(stderr, "\nUsage: %s [-p] [-d] [-s SPEC] [-v] \n", argv[0]);
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-d Don't detach from controlling terminal (don't daemonise)\n");
	fprintf(stderr, "\t-s Custom feature SPEC, default: %d \n"
			"\t\t 0: No SPEC\n"
			"\t\t 1: TELSTRA Cinterion\n"
			"\t\t 2: TELSTRA Santos\n"
			"\t\t 3: TELSTRA Sierta_LTE\n", _default_spec);
	fprintf(stderr, "\t-v Increase verbosity\n");
	fprintf(stderr, "\t-V Display version information\n");
	fprintf(stderr, "\n");
}

void manager_shutdown(int n)
{
	log_INFO("exiting");
	rdb_close(&g_rdb_session);
	closelog();
	exit(n);
}

void launch_end_script(int connection)
{
	int profile;
	int instance;

	const char* end_script;
	char cmd[128];

	profile = glb.active_connections[connection].profile;
	instance = glb.active_connections[connection].sub_interface;

	if( (end_script=get_global_script()) == NULL) {
		// get end script and bypass if no end script
		if( !(end_script=get_connection_script(profile,instance,1)) )
			return;
	}

	// make sure if the script is executable
	if (access(end_script, X_OK) < 0)
	{
		if(errno==ENOENT)
			log_DEBUG("skipping connection end script (does not exist) - %s",end_script);
		else
			log_ERR("%s - %s", strerror(errno), end_script);

		return;
	}

	snprintf(cmd, sizeof(cmd), "%s %d %d %s", end_script,
			profile, instance, "stop");

	log_INFO("launching script : %s", cmd);
	system(cmd);
}

void collect_children()
{
	clock_t clkCur = __getTickCount();
	int x;

	pid_t pid;
	int stat;

	/* wait for a child */
	pid = waitpid(-1, &stat, WNOHANG);
	while(pid>0) {
		for (x = 0; x < MAX_CONNECTIONS; x++)
		{
			if (glb.active_connections[x].pid == pid)
			{
				log_INFO("Child matched - pid=%d,conn=%d", pid,x);

				launch_end_script(x);

				// clear profile connecting status
				set_profile_connecting_status(glb.active_connections[x].profile,0);

				checkSetProfileDefunct(glb.active_connections[x].profile);

				// init connection info
				memset(&glb.active_connections[x],0,sizeof(glb.active_connections[x]));
				glb.active_connections[x].profile = INACTIVE;
				glb.active_connections[x].pid = -1;
				glb.active_connections[x].sub_interface = -1;
				glb.active_connections[x].sigterm_sent=0;

				// store the time when the script terminates
				glb.active_connections[x].clkTermValid=1;
				glb.active_connections[x].clkTerm = clkCur;

if(TELSTRA_SPEC_Santos){
				resetTelstraReconnectCount(glb.active_connections[x].profile);
}

				return;
			}
		}

		pid = waitpid(-1, &stat, WNOHANG);
	}
}

#if 0
void cleanup_connection(pid_t pid,int stat)
{
	clock_t clkCur = __getTickCount();
	int x;

	for (x = 0; x < MAX_CONNECTIONS; x++)
	{
		if (glb.active_connections[x].pid == pid)
		{
			log_INFO("Child matched - pid=%d,conn=%d", pid,x);

			launch_end_script(x);

			// clear profile connecting status
			set_profile_connecting_status(glb.active_connections[x].profile,0);

			checkSetProfileDefunct(glb.active_connections[x].profile);

			// init connection info
			memset(&glb.active_connections[x],0,sizeof(glb.active_connections[x]));
			glb.active_connections[x].profile = INACTIVE;
			glb.active_connections[x].pid = -1;
			glb.active_connections[x].sub_interface = -1;
			glb.active_connections[x].sigterm_sent=0;

			// store the time when the script terminates
			glb.active_connections[x].clkTermValid=1;
			glb.active_connections[x].clkTerm = clkCur;

			return;
		}
	}
}
#endif

static void sig_handler(int signum)
{
#if 0
	pid_t	pid;
	int stat;
#endif

	switch (signum)
	{
		case SIGHUP:
			log_INFO("Caught Sig %d", signum);
			//rdb_import_config (config_file_name, TRUE);
			break;

		case SIGTERM:
			log_INFO("Caught Sig %d", signum);

			log_DEBUG("SIGTERM detected");
			glb.run = 0;
			break;

#if 0
		case SIGCHLD:
			if ( (pid = waitpid(-1, &stat, WNOHANG)) > 0 )
			{
				log_INFO("Child %d terminated (stat=%d,rc=%d)", pid, stat,WEXITSTATUS(stat));
				cleanup_connection(pid,stat);
			}
			break;
#endif

		default:
			log_INFO("Caught Sig %d", signum);
			break;

	}
}


int is_wwan_suspended_by_failover()
{
	const char* val;

	int wan_mode;

	// always [suspended] if wan mode
	val=rdb_get_value("service.failover.mode");	// can be disable, failover and quickfailover
#ifdef PLATFORM_BOVINE
	if(!strcmp(val,"wlan"))
#else
	if(!strcmp(val,"wan"))
#endif
		return 1;
	// always [not suspended] if wwan mode
	if(!strcmp(val,"wwan"))
		return 0;

	// always [not suspended] if always connect wwan
	val=rdb_get_value("service.failover.alwaysConnectWWAN");	// can be disable and enable
	if(!strcmp(val,"enable"))
		return 0;

	// get interface mode
	val=rdb_get_value("service.failover.interface");	// can be wan and wwan
#ifdef PLATFORM_BOVINE
	wan_mode=!strcmp(val,"wlan");
#else
	wan_mode=!strcmp(val,"wan");
#endif

	return wan_mode;
}

int setProfileVariableStr(int iPf, const char* szVariable, char* achValue)
{
	char achName[MAX_NAME_LENGTH+1];
	char dummy[16];

	// No .dev entry means the profile doesn't exist
	snprintf(achName, sizeof(achName), "link.profile.%d.dev", iPf);
	if (rdb_get_string(g_rdb_session, achName, dummy, sizeof(dummy)))
		return -1;

	// No .enable entry means the profile isn't managed by connection manager
	snprintf(achName, sizeof(achName), "link.profile.%d.%s", iPf, szVariable);

	int stat = rdb_set_string(g_rdb_session, achName, achValue);
	if (stat < 0)
		stat = rdb_create_string(g_rdb_session, achName, achValue, 0, 0);

	return stat;
}

int setProfileVariableInt(int iPf, const char* szVariable, int nValue)
{
	char achValue[32];
	snprintf(achValue, sizeof(achValue), "%d", nValue);

	return setProfileVariableStr(iPf,szVariable,achValue);
}

int set_profile_connecting_status(int pf,int stat)
{
	return setProfileVariableInt(pf,"connecting",stat);
}

const char* getProfileVariableStr(int iPf, const char* szVariable)
{
	char achName[MAX_NAME_LENGTH+1];
	static char achValue[128];

	// No .dev entry means the profile doesn't exist
	snprintf(achName, sizeof(achName), "link.profile.%d.%s", iPf,szVariable);
	if (rdb_get_string(g_rdb_session, achName, achValue, sizeof(achValue)))
		return "";

	return achValue;
}

// @retval -1   The profile does not exist.
//				Assumption: Variables will not be set to -1.
// @retval 0    The variable exists and its value is 0.
// @retval 0    The variable does not exist.
// @retval >0   The value of the variable.
//              Note If the variable cannot be converted to an int then the
//              return value is undefined.
int getProfileVariableInt(int iPf, const char* szVariable)
{
	char achName[MAX_NAME_LENGTH+1];
	char achValue[32];

	// No .dev entry means the profile doesn't exist
	snprintf(achName, sizeof(achName), "link.profile.%d.dev", iPf);
	if (rdb_get_string(g_rdb_session, achName, achValue, sizeof(achValue)))
		return -1;

	// No .enable entry means the profile isn't managed by connection manager
	snprintf(achName, sizeof(achName), "link.profile.%d.%s", iPf, szVariable);

	if (rdb_get_string(g_rdb_session, achName, achValue, sizeof(achValue)))
		return 0;

	return atoi(achValue);
}

int setProfileRetry(int iPf, int nRetry)
{
	return setProfileVariableInt(iPf, "retry", nRetry);
}

int getProfileRetry(int iPf)
{
	return getProfileVariableInt(iPf, "retry");
}

//#if defined VTELSTRA_SPEC_Santos || defined V_TELSTRA_SPEC_Cinterion_module || defined V_TELSTRA_SPEC_Sierra_LTE_module
int get_esm_cause_codes_groups(int iPf) {
// Telstra requirement(#TT 11500, 11501): ESM cause codes other then #8, 29, 32, 33 shall not retry. for other codes the device shall introduce a 12 minute backoff timer. After each rejected request, doubling the duration of this backoff timer and ultimately stopping the requests after 3 days.
	const char* p_verbose_call_end_reason;
	int cause_code, i;
	int group1_list[] = { 8, 29, 32, 33 };

	char cmd[64];
	snprintf(cmd,sizeof(cmd),"link.profile.%d.pdp_result",iPf);
	p_verbose_call_end_reason = rdb_get_value(cmd);

	if(*p_verbose_call_end_reason == '#') {
		cause_code=atoi(p_verbose_call_end_reason+1);

		for(i=0; i<sizeof(group1_list)/sizeof(group1_list[0]); i++) {
			if(group1_list[i]==cause_code) {
				return 1; // group1, don't retry
			}
		}
	}

	return 0; // group0, do retry
}
//#endif

int getProfileConnRetries(int iPf)
{
if(TELSTRA_SPEC_Santos ||  TELSTRA_SPEC_Cinterion_module)
	return 0;
else if (TELSTRA_SPEC_Sierra_LTE_module)
	// 2 days = 2*24*3600/720=240
	return 240;
else
	return getProfileVariableInt(iPf, "reconnect_retries");

}

#define VODAFONE_BACKOFF_DELAY	30
#define NETWORK_ATTACH_BACKOFF_DELAY	(3*60)

static int vodafone_backoff_max_delay=20*60;

int get_sim_card_recovery_config()
{
	int manulroam;
	int sim_recovery;
	int roam_simcard;
	int plmn_manual_mode;

	manulroam=rdb_get_Int("manualroam.enable");
	roam_simcard=rdb_get_Int("manualroam.custom_roam_simcard");
	plmn_manual_mode=rdb_get_Int("plmn_manual_mode");
	sim_recovery=rdb_get_Int("connmgr.sim_card_recovery.enable");

	// get reset max delay
	return (!manulroam || !roam_simcard || plmn_manual_mode) && sim_recovery;
}

void update_vdafone_config()
{
	int max_delay;
	// get reset max delay
	max_delay=rdb_get_Int("manualroam.reset_max_delay");
	if(max_delay>0)
		vodafone_backoff_max_delay=max_delay;
}

int get_vodafone_delay(int pf,int* ret_delay_idx, int* ret_delay_sec)
{
	int delay_idx;
	int delay_sec;

	char rdb_delay_idx[128];

	// get rdb profile name
	snprintf(rdb_delay_idx,sizeof(rdb_delay_idx),"link.profile.%d.retry",pf);

	// get current delay idx count
	delay_idx=rdb_get_Int(rdb_delay_idx);

	// get delay sec
	if(delay_idx<1)
		delay_sec=0;
	else
		delay_sec=VODAFONE_BACKOFF_DELAY*(1<<(delay_idx-1));

	if(ret_delay_idx)
		*ret_delay_idx=delay_idx;

	if(ret_delay_sec)
		*ret_delay_sec=delay_sec;

	return (delay_sec<vodafone_backoff_max_delay)?delay_sec:vodafone_backoff_max_delay;
}

void inc_vodafone_delay(int pf)
{
	int delay_idx;
	int delay_sec;

	char rdb_delay_idx[128];
	char rdb_val[64];

	// get rdb profile name
	snprintf(rdb_delay_idx,sizeof(rdb_delay_idx),"link.profile.%d.retry",pf);

	get_vodafone_delay(pf,&delay_idx,&delay_sec);

	if(delay_sec<vodafone_backoff_max_delay) {
		delay_idx++;

		snprintf(rdb_val,sizeof(rdb_val),"%d",delay_idx);
		rdb_set_value(rdb_delay_idx,rdb_val);
	}
}

void resetTelstraReconnectCount(int profile)
{
	setProfileVariableInt(profile, "reconnect_count", 0);
}

void resetTelstraReconnectCounts()
{
	int profile;
	for (profile = 1; profile < MAX_PROFILES; profile++) {
		resetTelstraReconnectCount(profile);
	}
}

void incrementTelstraReconnectCount(int profile) {
	int reconnect_count = getProfileVariableInt(profile, "reconnect_count");
	reconnect_count++;
	setProfileVariableInt(profile, "reconnect_count", reconnect_count);
}


int getProfileConnDelay(int iPf)
{
if(TELSTRA_SPEC_Santos)
	return SANTOS_ESM_RECONNECT_DELAY;

if(TELSTRA_SPEC_Cinterion_module || TELSTRA_SPEC_Sierra_LTE_module){
	char achName[MAX_NAME_LENGTH+1];
	char achValue[32];
	int delay;

	snprintf(achName, sizeof(achName), "link.profile.%d.ESM_reconnect_delay", iPf);
	if (rdb_get_string(g_rdb_session, achName, achValue, sizeof(achValue))) {
		delay=45;
	}
	else {
		delay=atoi(achValue);
	}
	return delay<45? 45:delay;//first retry 45 sec
}else{
	return getProfileVariableInt(iPf, "reconnect_delay");
}
}
/* get profile.x.enable
-1 -- ignored
0 -- monitored and disconnected
1 -- enabled
*/
int get_profile_enable(int profile)
{
	int val = getProfileVariableInt(profile, "enable");
	if(val < 0) return PROFILE_IGNORED;
	return val == 0 ? PROFILE_DISABLED :  PROFILE_ENABLED;
}

int getProfileDelflag(int profileNumber)
{
	return getProfileVariableInt(profileNumber, "delflag");
}

int device_sub_interface_is_free(char *device, int sub_if)
{
	int	len;
	int	x;
	char	name[32];
	char	value[128];

	for (x = 0; x < MAX_CONNECTIONS; x++)
	{
		if (glb.active_connections[x].profile == INACTIVE)
			continue;

		// Get device descriptor name for each active connection
		snprintf(name, sizeof(name), "link.profile.%d.dev", glb.active_connections[x].profile);
		len = sizeof(value);
		if (rdb_get_string(g_rdb_session, name, value, len) == 0)
		{
			if (strcmp(device, value) == 0)
			{
				if (glb.active_connections[x].sub_interface == sub_if)
					return glb.active_connections[x].pid<0;
			}
		}
	}

	return 1;
}

int get_next_instance(int profile)
{
	int	len;
	int	x;
	int	max_instance;
	char	name[32];
	char	device[32];
	char	value[128];
	int	wwan_id;

	run_once_define_array(no_slot_left,MAX_CONNECTIONS);

	int defgw;
	int wwan;

	// Get device descriptor name
	snprintf(name, sizeof(name), "link.profile.%d.dev", profile);
	len = sizeof(device);
	if (rdb_get_string(g_rdb_session, name, device, len))
	{
		log_ERR("No device defined for profile %d", profile);
		return -1;
	}

	// Get maximum number of sub interfaces for device
	// Assume 1 if max_sub_if not defined
	snprintf(name, sizeof(name), "%s.max_sub_if", device);
	len = sizeof(value);
	if (rdb_get_string(g_rdb_session, name, value, len))
	{
		max_instance = 1;
	}
	else
		max_instance = atoi(value);

	defgw=getProfileVariableInt(profile,"defaultroute")>0;
	wwan=is_wwan_profile(profile, &wwan_id);

	// use any instance - we can do any number of multiple connection if it is not WWAN
	if(!wwan) {
		for (x = 0; x < max_instance; x++)
		{
			if (device_sub_interface_is_free(device, x)) {
				run_once_reset_array(no_slot_left,profile);
				return x;
			}
		}
	}
	else {

		// use the first instance for the wwan primary connection
		if(defgw) {
			if(device_sub_interface_is_free(device, 0))
				return 0;
		}
		else {
			//log_DEBUG("max_instance=%d %s=%s\n", max_instance, name, value);
			for (x = 0; x < max_instance; x++){
				if (device_sub_interface_is_free(device, x)) {
					run_once_reset_array(no_slot_left,profile);
					return x;
				}
			}
		}
	}

	run_once_func_array(no_slot_left,profile,log_ERR("no data port available for profile %d (gw=%d,max=%d)",profile,defgw,max_instance););
	return -1;
}

#if defined(CELL_NW_cdma)
int is_cdma_activated(int wwan_id, int* activated)
{
	const char* active;

	/* get active rdb */
	active=rdb_get_i_value(wwan_id, "wwan.0.module_info.cdma.activated");

	/* return activation status */
	if(activated)
		*activated=atoi(active);

	/* return capability */
	return active[0];
}
#endif


#if defined(PLATFORM_PLATYPUS2) || defined(PLATFORM_BOVINE)
#define DATA_ROAMING_DB_NAME	"roaming.data.en"
#define RDB_ROAMING_STATUS	"wwan.0.system_network_status.roaming"
#define RDB_VALUE_SIZE_SMALL	128
static int roaming_call_blocked(int wwan_id)
{
	char* value = alloca(RDB_VALUE_SIZE_SMALL);
	char *default_roaming = "0";
#if defined(PLATFORM_PLATYPUS)
	char* db_str;
	int result;
#endif
	if (rdb_get_i_string(wwan_id, RDB_ROAMING_STATUS, value, RDB_VALUE_SIZE_SMALL) != 0 ||
		*value == 0 || strcmp(value, "active") != 0)
	{
		return 0;
	}

#if defined(PLATFORM_PLATYPUS)
	db_str = nvram_get(RT2860_NVRAM, DATA_ROAMING_DB_NAME);
	//log_DEBUG("read NV item : %s : %s\n", DATA_ROAMING_DB_NAME, db_str);
	if (!db_str || !strlen(db_str))
	{
		log_DEBUG("Voice roaming variable is not defined. Set to default %s", default_roaming);
		nvram_strfree(db_str);
		result = nvram_bufset(RT2860_NVRAM, DATA_ROAMING_DB_NAME, default_roaming);
		if (result < 0)
		{
			log_ERR("write NV item failure: %s : %s", DATA_ROAMING_DB_NAME, default_roaming);
			return (strcmp(default_roaming, "0") == 0);
		}
		result = nvram_commit(RT2860_NVRAM);
		if (result < 0)
			log_ERR("commit NV items failure");
		return (strcmp(default_roaming, "0") == 0);
	}
	return (strcmp(db_str, "0") == 0);
#else
	if( rdb_get_string(g_rdb_session,  DATA_ROAMING_DB_NAME, value, RDB_VALUE_SIZE_SMALL ) != 0)
	{
		log_ERR( "failed to read '%s'", DATA_ROAMING_DB_NAME );
		if (rdb_create_string(g_rdb_session, DATA_ROAMING_DB_NAME, default_roaming, CREATE, ALL_PERM) != 0)
		{
			log_ERR("failed to create '%s' (%s)", DATA_ROAMING_DB_NAME, strerror(errno));
		}
		return (strcmp(default_roaming, "0") == 0);
	}
	return (strcmp(value, "0") == 0);
#endif
}

#define DATA_ROAMING_BLOCKED_DB_NAME	"roaming.data.blocked"
static void mark_data_roaming_call_blocked(int blocked)
{
	char* value = alloca(RDB_VALUE_SIZE_SMALL);
	if( rdb_get_string(g_rdb_session,  DATA_ROAMING_BLOCKED_DB_NAME, value, RDB_VALUE_SIZE_SMALL ) != 0)
	{
		//log_ERR( "failed to read '%s'\n", VOICE_ROAMING_BLOCKED_DB_NAME );
		if (rdb_create_string(g_rdb_session, DATA_ROAMING_BLOCKED_DB_NAME, "0", CREATE, ALL_PERM) != 0)
		{
			log_ERR("failed to create '%s' (%s)", DATA_ROAMING_BLOCKED_DB_NAME, strerror(errno));
		}
		rdb_set_string(g_rdb_session, DATA_ROAMING_BLOCKED_DB_NAME, (blocked? "1":"0") );
	}
	else
	{
	    // for no apparent reason, DATA_ROAMING_BLOCKED_DB_NAME (roaming.data.blocked) rdb
	    // variable is persistent. Being updated too often is not a good idea because rdb_manager
	    // (configuration) will write all persistent RDB vars to a file when any of them
	    // is updated. So, write only if it is changing.
	    // Same change in pots_bridge.c
	    if (atoi(value) != blocked)
	    {
	        rdb_set_string(g_rdb_session, DATA_ROAMING_BLOCKED_DB_NAME, (blocked? "1":"0") );
	    }
	}
}
#endif


void rename_to_dod_script(char* script,int len)
{
	char dod_script[128];
	char* script_dir;
	char* script_fname;

	script_dir=dirname(strdupa(script));
	script_fname=basename(strdupa(script));

	if(*script_fname=='S')
		script_fname++;

	snprintf(dod_script,sizeof(dod_script),"%s/SDOD-%s",script_dir,script_fname);

	// copy the new connection script back to the buffer
	strncpy(script,dod_script,len);
	script[len-1]=0;
}

const char* get_global_script()
{
	static char script[128];

	const char* p;

	/* read global script */
	if( (p=rdb_get_value("dev.0.script")) == NULL )
		return NULL;

	/* copy to script */
	SAFE_STRNCPY(script,p,sizeof(script));

	return script;
}

const char* get_connection_script(int profile,int instance,int end_script)
{
	int len;
	int ret;
	char name[32];
	char device[32];
	static char conn_script[128];
	// change to end script
	char* file_name;
	run_once_define_array(script_not_ready,MAX_CONNECTIONS);

	// Get device descriptor name
	snprintf(name, sizeof(name), "link.profile.%d.dev", profile);

	len = sizeof(device);
	if ((ret = rdb_get_string(g_rdb_session, name, device, len))) {
		log_ERR("No device defined for profile %d", profile);
		goto err;
	}

	// get a slot-specified script
	snprintf(name, sizeof(name), "%s.script.%d", device,instance);
	len = sizeof(conn_script);
	ret = rdb_get_string(g_rdb_session, name, conn_script, len);

	// get global script if the slot-specified script does not exist
	if (ret!=0 || !conn_script[0]) {
		// Get device start script name
		snprintf(name, sizeof(name), "%s.script", device);

		len = sizeof(conn_script);
		ret = rdb_get_string(g_rdb_session, name, conn_script, len);
	}

	// print the error
	if (ret!=0) {
		run_once_func_array(script_not_ready,profile,log_ERR("No start script defined for device %s. - waiting until it's ready", device););
		goto err;
	}

	run_once_reset_array(script_not_ready,profile);

	if(end_script) {

		file_name=strrchr(conn_script,'/');
		if(!file_name)
			file_name=conn_script;
		else
			file_name++;

		if(*file_name!='S')
		{
			log_ERR("connection start script does not start with S - %s",conn_script);
			goto err;
		}

		*file_name='E';
	}

	return conn_script;

err:
	return 0;
}

/*
	manage_connection() maintains only connection list - not profile list
	any profile related task is not allowed in this function
*/
void manage_connection(int connection)
{
	int profile;
	int instance;
	int wwan_id;
	char start_script[128];
	char cmd[128];

	int  iArgVal;
	char aArgVal[4][256];
	char* args[5];

	const char* p;

	int wwan_profile;
	int do_vodafone_delay;

	clock_t clkDelay;
	run_once_define_array(vodafon_delay,MAX_PROFILES);

	// Is this connection already being managed?
	if (glb.active_connections[connection].pid != -1)
		return;

	// get profile
	profile = glb.active_connections[connection].profile;

	// get instance
	instance = get_next_instance(profile);
	if(instance<0)
	{
		// clear connection
		glb.active_connections[connection].profile = INACTIVE;
		glb.active_connections[connection].pid = -1;
		glb.active_connections[connection].sub_interface = -1;
		glb.active_connections[connection].connectProvisional = 0;
		glb.active_connections[connection].clkStartValid =0;

		return;
	}


	// get wwan profile flag
	wwan_profile=is_wwan_profile(profile, &wwan_id);

	/* check local script at first*/
	if( !(p=get_connection_script(profile,instance, 0)) )  {
	/* use global script if existing */
	if( (p=get_global_script())!=NULL ) {
		SAFE_STRNCPY(start_script,p,sizeof(start_script));
	}
	}else {
		// store the script to a local value
		strcpy(start_script,p);

		// do special startup script if dod is enabled
		if(is_dod_enabled(profile) && wwan_profile) {
			rename_to_dod_script(start_script,sizeof(start_script));
			//log_INFO("Dial-on-demand detected - %s",start_script);
		}
	}

	glb.active_connections[connection].sub_interface = instance;

	struct connection* pConn = &glb.active_connections[connection];

	#if V_VODAFONE_SPEC
	do_vodafone_delay=wwan_profile;
	#else
	do_vodafone_delay=0;
	#endif

	if(do_vodafone_delay) {
		// read vodafone configuration
		update_vdafone_config();

		// delay of Vodafone requirement - #TT5679
		clkDelay = get_vodafone_delay(profile,NULL,NULL)* _clock_per_second;

		run_once_func_array(vodafon_delay,profile,log_ERR("[back-off-delay] apply delay - profile=%d,delay=%ld sec",profile,clkDelay/_clock_per_second););
		if (pConn->clkTermValid && (clkDelay>0) && (_now - pConn->clkTerm < clkDelay))
			return;

		run_once_reset_array(vodafon_delay,profile);

		inc_vodafone_delay(profile);
	}
	else if(profile_info[profile].trigger==1) {
		//if trigger==1 start connection immediately
		profile_info[profile].trigger=0;
	}
	else {
		// check retries
		int cTotalRetryCnt;
		int cConnRetries;

		// bypass if it needs delay
		clock_t clkDelay = (clock_t)getProfileConnDelay(profile) * _clock_per_second;
		if (pConn->clkTermValid && (clkDelay>0) && (_now - pConn->clkTerm < clkDelay)) {
			return;
		}

if(TELSTRA_SPEC_Santos || TELSTRA_SPEC_Cinterion_module || TELSTRA_SPEC_Sierra_LTE_module){
		// Telstra requirement: ESM cause codes #8, 29, 32, 33 shall not retry.
		if( get_esm_cause_codes_groups(profile) ) {
			return;
		}
}else{
		// stop lauching if excceds the maximum retries
		cTotalRetryCnt = getProfileRetry(profile);
		cConnRetries = getProfileConnRetries(profile);
		// do not connect if we exceed total retry count
		if ((cConnRetries>0) && (cTotalRetryCnt >= cConnRetries)) {
			return;
		}
}
if(TELSTRA_SPEC_Santos){
		/* set delay t0 30, no matter what, no back off timer for santos
		 * we will let module handle timers as it is Telstra certified */
		setProfileVariableInt(profile, "ESM_reconnect_delay", SANTOS_ESM_RECONNECT_DELAY);

		incrementTelstraReconnectCount(profile);
}else if(TELSTRA_SPEC_Cinterion_module ){
		long delay=0;
		char achValue[32];
		snprintf(cmd, sizeof(cmd), "link.profile.%d.ESM_reconnect_delay", profile);
		if (rdb_get_string(g_rdb_session, cmd, achValue, sizeof(achValue))==0) {
			delay=atoi(achValue);
		}
		if(delay<45) {
			delay=45*2;
		}
		else {
			delay=delay*2;
		}
		if(delay>259200) { // 3 days, 3*24*3600
			return;
		}
		snprintf(achValue, sizeof(achValue), "%ld", delay);
		rdb_set_value(cmd, achValue);

}else if(TELSTRA_SPEC_Sierra_LTE_module ){
		int delay=0;
		delay=getProfileConnDelay(profile);
		if(delay<720) {
			if(delay<=45) {
				delay++;
			}
			else {
				//after 2 retris introduce a backoff timer to 12 minutes
				delay=720;
			}
			setProfileVariableInt(profile, "ESM_reconnect_delay", delay);
		}

}else{
		// increase reconnection time
		cTotalRetryCnt++;
		setProfileRetry(profile, cTotalRetryCnt);
}
		log_DEBUG("connection %d using profile %d - trycnt=%d/%d", connection, profile, cTotalRetryCnt, cConnRetries);
	}

	// set up starting information
	glb.active_connections[connection].clkStartValid = 1;
	glb.active_connections[connection].clkStart = _now;
	glb.active_connections[connection].connectProvisional = 1;

	pid_t pid = fork();

	if (pid == 0) // Child
	{
		// Restore default signal handlers for children
		signal(SIGHUP, SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		log_DEBUG("signals disabled");

		/* Redirect standard files to /dev/null */
		freopen( "/dev/null", "r", stdin);
		freopen( "/dev/null", "w", stdout);
		freopen( "/dev/null", "w", stderr);

		log_DEBUG("stdio redirected");

		/* Create a new SID for the child process */
		int sid = setsid();
		if (sid < 0) 	{
			syslog( LOG_ERR, "unable to create a new session, code %d (%s)", errno, strerror(errno) );
			exit(EXIT_FAILURE);
    		}

		log_DEBUG("new sid created");

		for (;;)
		{
			if (wwan_id >= 0)
			{
				snprintf(cmd, sizeof(cmd), "%s %d %d", start_script,
				         glb.active_connections[connection].profile, wwan_id+instance);

				log_INFO("Starting %s", cmd);
				iArgVal = 0;
				sprintf(aArgVal[iArgVal++], "%s", start_script);
				sprintf(aArgVal[iArgVal++], "%d", glb.active_connections[connection].profile);
				sprintf(aArgVal[iArgVal++], "%d", wwan_id +instance);
				sprintf(aArgVal[iArgVal++], "%s", "start");

				args[0] = aArgVal[0];
				args[1] = aArgVal[1];
				args[2] = aArgVal[2];
				args[3] = aArgVal[3];
				args[4] = NULL;

				// launch
				if (execv(aArgVal[0], args) < 0)
				{
					log_ERR("failed to launch script %s", cmd);
					_exit(0);
				}

				//system (cmd);
				log_INFO("Finished %s", cmd);
			}
			else
			{
				log_INFO("Cannot start connection for profile %d, interface %d not available",
			         glb.active_connections[connection].profile, instance);
			}

			// Is profile still enabled ?
			if (get_profile_enable(profile) != PROFILE_ENABLED)
			{
				log_DEBUG("Manager for profile %d exiting", profile);
				_exit(0);
			}

			// Add Retry Delay here
			sleep(5);
		}

		_exit(0);
	}
	else if (pid > 0) // Parent
	{
		log_DEBUG("manager pid for profile %d = %d", glb.active_connections[connection].profile, pid);
		glb.active_connections[connection].pid = pid;

		// set profile connecting status
		set_profile_connecting_status(glb.active_connections[connection].profile,1);
	}
	else //Error
	{
		log_ERR("can't fork, error %d (%s)", errno, strerror(errno));
	}
}

void manage_active_connections(void)
{
	int x;
	int connections_counter=0;
	for (x = 0; x < MAX_CONNECTIONS; x++)
	{
		if (glb.active_connections[x].profile != INACTIVE) {
			manage_connection(x);
			connections_counter++;
		}
	}
}

int get_next_free_connection()
{
	int x;

	for (x = 0; x < MAX_CONNECTIONS; x++)
	{
		if (glb.active_connections[x].profile == INACTIVE)
			return x;
	}
	return -1;
}
int profile_in_active_connections_list(int profile)
{
	int x;

	for (x = 0; x < MAX_CONNECTIONS; x++)
	{
		if (glb.active_connections[x].profile == profile)
			return x;
	}
	return -1;
}

int get_connected_profile_pid(int profile)
{
	int x;

	for (x = 0; x < MAX_CONNECTIONS; x++)
	{
		if (glb.active_connections[x].profile == profile)
			return glb.active_connections[x].pid;
	}

	return -1;
}

int wwan_device_get_heart_beat(int wwan_id)
{
	char heart_beat[256];

	if (rdb_get_i_string(wwan_id, "wwan.0.heart_beat", heart_beat, sizeof(heart_beat)) < 0)
		return -1;

	if(!heart_beat[0])
		return -1;

	return atoi(heart_beat);
}

int sim_status_check(int wwan_id) {
	static int prevSIMstatus = -1;
	int curSIMstatus=-1; /* curSIMstatus and  prevSIMstatus must have same initial value*/
	char achSimStat[64];
	char lockBuf[64];
	char* triggercmd[]={
		"restore",
		"pinlock",
		"puklock",
		"meplock",
		"nosim",
		NULL,
	};

	// read sim status
	if (rdb_get_i_string(wwan_id, "wwan.0.sim.status.status", achSimStat, sizeof(achSimStat)) < 0) {
		achSimStat[0]=0;
	}
	else {
		// convert the sim status to status number
		if(!strcmp(achSimStat, "SIM PIN") || !strcmp(achSimStat, "SIM PIN Required") || !strncmp(achSimStat, "SIM locked", 10)) {
			curSIMstatus = 1;
		}
		else if(!strcmp(achSimStat, "SIM PUK") || strstr(achSimStat, "PUK")) {
			curSIMstatus = 2;
		}
		else if(!strcmp(achSimStat, "PH-NET PIN") || !strcmp(achSimStat, "SIM PH-NET") || strstr(achSimStat, "MEP")) {
			curSIMstatus = 3;
		}
		else if(!strcmp(achSimStat,  "SIM not inserted") || !strcmp(achSimStat,  "SIM removed")) {
			curSIMstatus = 4;
		}
		else if( strstr(achSimStat,  "OK")) {
			curSIMstatus = 0;
		}

		// read mep-lock status and if it is locked
		if ( (rdb_get_string(g_rdb_session, "meplock.status", lockBuf, sizeof(lockBuf)) >= 0) && (strcmp(lockBuf, "locked")==0) ) {
			curSIMstatus = 3;
		}

		// trigger led rdb if sim status changed
		if(curSIMstatus != prevSIMstatus) {
			prevSIMstatus = curSIMstatus;
			if(triggercmd[curSIMstatus] != NULL) {
				rdb_set_string(g_rdb_session, "simledctl.command.trigger", triggercmd[curSIMstatus]);
			}
		}
	}

	return !strcmp(achSimStat, "SIM OK");
}

/*int wwan_device_is_sim_ok(void)
{
	char achSimStat[256];

	if (rdb_get_string(g_rdb_session, "wwan.0.sim.status.status", achSimStat, sizeof(achSimStat)) < 0)
		return 0;

	return !strcmp(achSimStat, "SIM OK");
}*/


int is_pri_available(int wwan_id)
{
#ifdef MODULE_PRI_BASED_OPERATION
	char pri[128];
	char otasp_progress[128];

	if (rdb_get_i_string(wwan_id, "wwan.0.link_profile_ready", pri, sizeof(pri)) < 0)
		*pri=0;

	/* [wwan.0.cdma.otasp.progress] RDB is coming from Simple AT manager */
	if (rdb_get_i_string(wwan_id, "wwan.0.cdma.otasp.progress", otasp_progress, sizeof(otasp_progress)) < 0)
		*otasp_progress=0;

	return atoi(pri) && !atoi(otasp_progress);
#else
	/* do not use pri information */
	return 1;
#endif
}

int wwan_device_signal_strength(int wwan_id, int* sig)
{
	char strength[32];

	*sig=0;

	if (rdb_get_i_string(wwan_id, "wwan.0.radio.information.signal_strength", strength, sizeof(strength)) < 0)
		return -1;

	if(!strength[0])
		return -1;

	*sig=atoi(strength);

	return 0;
}


int wwan_device_check_network_control(int wwan_id)
{
	char network_ctrl[32];

	if (rdb_get_i_string(wwan_id, "wwan.0.networkctrl.cmd.valid", network_ctrl, sizeof(network_ctrl)) < 0)
		return -1;

	if(!network_ctrl[0])
		return -1;

	return atoi(network_ctrl);
}

int wwan_device_check_network_registered(int wwan_id)
{
	char network_registered[32];

	if (rdb_get_i_string(wwan_id, "wwan.0.system_network_status.registered", network_registered, sizeof(network_registered)) < 0)
		return -1;

	if(!network_registered[0])
		return -1;

	return atoi(network_registered);
}

int wwan_device_check_network_denied(int wwan_id)
{
	char network_reg[32];

	if (rdb_get_i_string(wwan_id, "wwan.0.system_network_status.reg", network_reg, sizeof(network_reg)) < 0)
		return -1;

	if(!network_reg[0])
		return 0;

	return atoi(network_reg)==3;
}


int wwan_device_check_attach_capability(int wwan_id)
{
	char achAttached[32];

	if (rdb_get_i_string(wwan_id, "wwan.0.system_network_status.attached", achAttached, sizeof(achAttached)) < 0)
		return 0;

	if(!strlen(achAttached))
		return 0;

	return !0;
}

int wwan_device_is_attached(int wwan_id)
{
	char achAttached[32];

	if (rdb_get_i_string(wwan_id, "wwan.0.system_network_status.attached", achAttached, sizeof(achAttached)) < 0)
		return 0;

	return atoi(achAttached);
}

int isAntennaOff(int wwan_id)
{
	char strength[16];
	int antenna=1;
	int strength_no;

	// read signal strength - bypass if the rdb variable does not exist
	if(rdb_get_i_string(wwan_id, "wwan.0.radio.information.signal_strength", strength, sizeof(strength)) < 0)
		goto fini;

	// do not assume if it is not the correct format
	if(!strstr(strength,"dBm"))
		goto fini;

	strength_no=atoi(strength);

	// assume antenna exists if we get a non-zero value
	antenna=strength_no!=0;

fini:
	return !antenna;
}


int isPdpUp(int profile, int wwan_id, int instance,int* pdp_capable)
{
	char pdpStat[16];
	int pdpUp;

	char pdp_stat_rdb[128];

	char* netif;

	int cur_pdp_capable=0;

	/* assume the device is not pdp-capable */
	if(pdp_capable)
		*pdp_capable=0;

	// get network interface name
	netif=strdupa(getProfileVariableStr(profile,"interface"));
	if(!*netif) {
		//syslog(LOG_ERR,"profile %d does not have interface name",profile);
		return 0;
	}

	// read pdp status and bypass if not capable
	snprintf(pdp_stat_rdb,sizeof(pdp_stat_rdb),"wwan.0.system_network_status.pdp%d_stat",instance);
	if(rdb_get_i_string(wwan_id, pdp_stat_rdb, pdpStat, sizeof(pdpStat)) < 0)
	{
		pdpUp=1;

		/* if the rdb variable does not exist, the device is not pdp-capable */
		cur_pdp_capable=0;
	}
	else
	{
		// get pdp up status
		if(!strcasecmp(pdpStat,"down") || !strcasecmp(pdpStat,"0")) {
			pdpUp=0;
			cur_pdp_capable=1;
		}
		else if(!strcasecmp(pdpStat,"up") || !strcasecmp(pdpStat,"1")) {
			pdpUp=1;
			cur_pdp_capable=1;
		}
		else {
			pdpUp=1;
			cur_pdp_capable=0;
		}
	}

	// read wwan interface status and put to pdpUp
	if(wwan_if)
		pdpUp=pdpUp && is_interface_up(wwan_if);

	/* additionally check interface running status if qmi - this can be applying to all network interface */
	{
		const char* wwan_if=rdb_get_i_value(wwan_id, "wwan.0.if");
		if(strstr(wwan_if,"qmi"))
			pdpUp=pdpUp && is_interface_running(netif);
	}

	/* return pdp capable status */
	if(pdp_capable)
		*pdp_capable=cur_pdp_capable;

	return pdpUp;
}


/*
	start_profile_connection() does *not* enable the profile. This function starts the profile connectiong by putting the profile into a connection slot.
	Soon, the connection will be established by manage_active_connections(). Because manage_active_connections() maintains the connection list.
*/
int start_profile_connection(int profile)
{
	int conn;

	conn = get_next_free_connection();

	if(conn<0) {
		log_ERR("failed to set up connection for profile %d - no connection slot left (max=%d)",profile,MAX_CONNECTIONS);
		return -1;
	}

	// setup connection slot
	glb.active_connections[conn].profile = profile;
	glb.active_connections[conn].clkStartValid = 0;
	glb.active_connections[conn].sigterm_sent = 0;

	return 0;
}

/*
	stop_profile_connection() does *not* disable the profile. This function stops the profile's instance connection.
	If the profile is enabled, the profile will be put into a connection slot and will be connecting again soon.
*/
void stop_profile_connection(int profile)
{
	int pid;
	int conn;
	clock_t past;

	// get connection slot corresponding to the profile
	conn = profile_in_active_connections_list(profile);

	//log_INFO("stopping connectiong of profile %d", profile);

	pid = get_connected_profile_pid(profile);

	if(pid<0) {
		//log_ERR("no connection script pid found in profile %d", profile);
	}
	else if ((pid==1) || (pid==0)) {
		log_ERR("incorrect connection script pid found in profile %d", profile);
	}
	else if (pid>1) {
		// send SIGTERM to the connection process if we have not sent before
		if(!glb.active_connections[conn].sigterm_sent) {
			log_INFO("sending SIGTERM to the process - pid=%d", pid);
			kill(pid, SIGTERM);

			// Remember not to send another SIGTERM as one has already been sent.
			glb.active_connections[conn].sigterm_sent_time=_now;
			glb.active_connections[conn].sigterm_sent=!0;
		}
		else {
			past=(_now-glb.active_connections[conn].sigterm_sent_time)/_clock_per_second;
			// wait 10 seconds after SIGTERM is sent
			if(past<CONNECTION_SCRIPT_TERMINATE_DELAY ) {
				log_INFO("waiting until the process terminates - pid=%d #%ld/%d", pid, past,CONNECTION_SCRIPT_TERMINATE_DELAY);
			}
			// send SIGKILL if the process is still running
			else {
				log_ERR("process not responding. send SIGKILL to the process - pid=%d", pid);
				kill(pid, SIGKILL);
			}
		}
	}

	// reset reconnection-related information
	setProfileRetry(profile, 0);
}

int is_dod_enabled(int prof)
{
	char dod[12];
	char pf[12];
	char rdb_name[128];

	int dod_pf;

	// get dod enable status
	snprintf(rdb_name,sizeof(rdb_name),"dialondemand.enable");
	if(rdb_get_string(g_rdb_session, rdb_name,dod,sizeof(dod))<0)
		dod[0]=0;
	if(!atoi(dod))
		return 0;

	// get profile
	snprintf(rdb_name,sizeof(rdb_name),"dialondemand.profile");
	if(rdb_get_string(g_rdb_session, rdb_name,pf,sizeof(pf))<0)
		pf[0]=0;
	dod_pf=atoi(pf);

	return prof==dod_pf;

}

int is_dod_up(int prof)
{
	char dod[12];
	char rdb_name[128];

	snprintf(rdb_name,sizeof(rdb_name),"dialondemand.status");
	if(rdb_get_string(g_rdb_session, rdb_name,dod,sizeof(dod))<0)
		dod[0]=0;
	return atoi(dod);

}

/*
	is_wwan_profile() returns true when the profile is 3g connection
*/
int is_wwan_profile(int profile, int *wwan_id)
{
	const char* dev_name;
	*wwan_id=0;
	// get dev name
	dev_name=getProfileVariableStr(profile,"dev");
	if(strncmp(dev_name,"wwan.",5) ==0){
		*wwan_id = atoi(&dev_name[5]);
		return 1;
	}
	return 0;
}

/*
 * find default WWAN profile
 *
 * Returns index of the default WWAN profile; -1 if not found
 */
static int find_wwan_default_profile(void)
{
	int profile;
	int wwan_id;

	for (profile = 1; profile < MAX_PROFILES; profile++) {

		if (is_wwan_profile(profile, &wwan_id) && wwan_id == _wwan_id && getProfileVariableInt(profile,"defaultroute") > 0) {
			return profile;
		}
	}
	return -1;
}

/*
 * Check whether the provided profile is being connected
 * Returns FALSE (i.e not being connected) if link.profile.X.status = up (already UP)
 * or last call failed
 * or it is timeout during "provision" period.
 * Returns TRUE (being connected) otherwise
 *
 * @profile: profile index
 *
 * Return:	1 - TRUE; 0 - FALSE
 */
static int is_profile_being_connected(int profile)
{
	const char *profile_status, *pdp_result;

	if (profile <= 0  || get_profile_enable(profile) != PROFILE_ENABLED) {
		return 0;
	}
	/* is it up? */
	profile_status=getProfileVariableStr(profile,"status");
	if (!strcmp(profile_status,"up")) {
		return 0;
	}
	/* last call failed (link.profile.X.pdp_result is not empty) */
	else if ((pdp_result = getProfileVariableStr(profile,"pdp_result")) && pdp_result[0] != 0) {
		return 0;
	}
	/* is it timeout during "provision" period? */
	else {
		int connection = profile_in_active_connections_list(profile);
		if (connection >= 0
				&& glb.active_connections[connection].clkStartValid == 1
				&& glb.active_connections[connection].connectProvisional == 1
		) {
			unsigned long uDiff=(unsigned long)_now - (unsigned long)(glb.active_connections[connection].clkStart);
			/* timeout*/
			if(uDiff >= (unsigned long)(CONNECTION_INIT_DELAY*_clock_per_second)) {
				return 0;
			}
		}
	}
	return 1;
}

/*
 * Check whether the provided profile is disconnected
 * The profile must have been already processed by connection_mgr
 * hence link.profile.X.status is not blank.
 * Returns TRUE if link.profile.X.status == "down", FALSE otherwise
 *
 * @profile: profile index
 *
 * Return:	1 - TRUE; 0 - FALSE
 */
static int is_profile_disconnected(int profile)
{
	const char *profile_status;

	if (profile <= 0) {
		return 1;
	}
	profile_status=getProfileVariableStr(profile,"status");
	if (!strcmp(profile_status,"down")) {
		return 1;
	}
	return 0;
}

/*
	is_profile_to_connect() returns true when the profile needs connecting
	this function is called only when the profile does not have a connection slot
*/
int is_profile_to_connect(int profile)
{
	int enabled;
	int wwan;
	int wwan_id;

	static int nwctl=0;

	static int last_tick_valid=0; /* is last tick valid */
	static int delay_idx=0;

	run_once_define(mep_lock);
	#if defined(PLATFORM_PLATYPUS2) || defined(PLATFORM_BOVINE)
	run_once_define(data_roaming);
	#endif

	run_once_define(failover_wan_mode);

	// get dev name
	wwan=is_wwan_profile(profile, &wwan_id);
	// get profile enable status
	enabled = get_profile_enable(profile);
	// do not connect if the profile is not enabled
	if(enabled !=  PROFILE_ENABLED)
		goto not_connect;

	if(wwan) {
		/* if default WWAN profile changes, wait until the previous default profile is disconnected */
		if (prev_default_wwan_profile != INACTIVE && default_wwan_profile != INACTIVE && prev_default_wwan_profile != default_wwan_profile) {
			if (!is_profile_disconnected(prev_default_wwan_profile)) {
				goto not_connect;
			}
			else {
				prev_default_wwan_profile = default_wwan_profile;
			}
		}
		/*if this is not the default profile, wait for default WWAN profile being connected */
		if (default_wwan_profile != INACTIVE && default_wwan_profile != profile && is_profile_being_connected(default_wwan_profile)) {
			goto not_connect;
		}

		// do not connect if the router is mep locked
		if(_mep_locked) {
			run_once_func(mep_lock,log_ERR("the router is mep-locked. 3g connection is not able to connect"););
			goto not_connect;
		}
		else {
			run_once_reset(mep_lock);
		}

		#if defined(PLATFORM_PLATYPUS2) || defined(PLATFORM_BOVINE)
		if (roaming_call_blocked(wwan_id)) {
			run_once_func(data_roaming,log_ERR("data call is blocked in roaming area"););
			mark_data_roaming_call_blocked(1);

			goto not_connect;
		}
		else {
			run_once_reset(data_roaming);
		}
		#endif

		// do not connect if wwan is not ready
		if(!_wwan_ready_to_connect) {

			int network_stat=wwan_device_check_network_registered(wwan_id);
			int network_registered=network_stat>0;
			int ps_capable=wwan_device_check_attach_capability(wwan_id);
			int ps_attached=wwan_device_is_attached(wwan_id);
			int nwctrl_cap=wwan_device_check_network_control(wwan_id);
			int network_denied=wwan_device_check_network_denied(wwan_id);
			int i;
			const char* stat;


			static clock_t last_tick=0; /* last time we send network attach request */

			int delay_sec;

			run_once_define(attach_req);

			#define NWCTRL_TIMEOUT	30
			/* send network attach request if no attach */
			if((network_registered && ps_capable && !ps_attached) || network_denied) {
				if(nwctrl_cap != 0 && !nwctl) {

					// read vodafone configuration
					update_vdafone_config();

					// make it double each time
					delay_sec=NETWORK_ATTACH_BACKOFF_DELAY*(1<<delay_idx);
					// limit to back-off max delay
					if(delay_sec>vodafone_backoff_max_delay)
						delay_sec=vodafone_backoff_max_delay;


					/* initiate tick count */
					if(!last_tick_valid) {
						last_tick=_now;
						last_tick_valid=1;

						log_ERR("[network %d attach request] back-off delay %d sec - initial",wwan_id, delay_sec);
						goto not_connect;
					}

					// bypass during back-off delay
					if(last_tick_valid) {
						if(_now-last_tick<delay_sec*_clock_per_second) {
							run_once_func(attach_req,log_ERR("[network %d attach request] back-off delay %d sec",wwan_id,delay_sec););
							goto not_connect;
						}

						// increase delay
						if(delay_sec<vodafone_backoff_max_delay)
							delay_idx++;
					}


					// reset message
					run_once_reset(attach_req);
					// store last tick
					last_tick=_now;
					last_tick_valid=1;

					rdb_set_i_value(wwan_id, "wwan.0.networkctrl.cmd.status","");

					log_ERR("[network %d attach request] sending...",wwan_id);
					rdb_set_i_value(wwan_id, "wwan.0.networkctrl.cmd.command","attach");

					/* wait for status result */
					i=0;
					stat="";
					while(i++<NWCTRL_TIMEOUT) {
						stat=rdb_get_i_value(wwan_id, "wwan.0.networkctrl.cmd.status");
						if(*stat)
							break;
						sleep(1);
					}

					if(*stat && strstr(stat,"[done]")) {
						syslog(LOG_ERR,"[network %d attach request] succ",wwan_id);
						nwctl=1;
					}
					else {
						int sim_card_recovery;

						sim_card_recovery=get_sim_card_recovery_config();

						if(sim_card_recovery) {
							syslog(LOG_ERR,"[network %d attach request] fail - power-cycling module",wwan_id);
							system("reboot_module.sh");
						}
						else {
							syslog(LOG_ERR,"[network %d attach request] fail - retrying",wwan_id);
						}
					}

				}
			}

			goto not_connect;
		}

		/* clear network control timer */
		nwctl=0;
		last_tick_valid=0;
		delay_idx=0;

		// do not connect if failover enabled and currently wan connection
		if(is_wwan_suspended_by_failover()) {
			run_once_func(failover_wan_mode,log_ERR("failover configuration - 3g connection %d suspended",wwan_id););
			goto not_connect;
		}
	}

	return 1;

not_connect:
	return 0;
}


static int antenna_loss_reconnect=0;
static int ps_loss_reconnect=0;

void update_ps_stat(int wwan_id)
{
	const int ps_delay=30;

	int ps_capable;

	static clock_t ps_attach_off_tick= 0;
	static int prev_ps_attach_stat=0;
	int flag_to_update;
	int new_ps_attach_stat;
	int ps_attached;

	if(ps_attach_off_tick==0) ps_attach_off_tick= _now;
	/* get flags */
	ps_capable=wwan_device_check_attach_capability(wwan_id);
	ps_attached=wwan_device_is_attached(wwan_id);
	new_ps_attach_stat=!ps_capable || ps_attached;

	/* if falling edge */
	if(prev_ps_attach_stat && !new_ps_attach_stat) {
		ps_attach_off_tick=_now;
		syslog(LOG_ERR,"[ps-loss] PS detached detected. reconnect in %d sec.",ps_delay);

		flag_to_update=0;
	}
	/* if continious down period */
	else if (!prev_ps_attach_stat && !new_ps_attach_stat) {
		flag_to_update=!((_now-ps_attach_off_tick)/_clock_per_second<ps_delay);
	}
	/* if rising edge */
	else if (!prev_ps_attach_stat && new_ps_attach_stat) {
		if(!ps_loss_reconnect)
			syslog(LOG_ERR,"[ps-loss] PS attached - recovered in %ld sec(s) ",(_now-ps_attach_off_tick)/_clock_per_second);
		flag_to_update=1;
	}
	/* if continious up period */
	else {
		flag_to_update=1;
	}

	/* update previous ps stat */
	prev_ps_attach_stat=new_ps_attach_stat;

	/* update global ps stat */
	if(flag_to_update)
		ps_loss_reconnect=!new_ps_attach_stat;
}

void update_antenna_loss(int wwan_id)
{
	static int antenna_off_cur=0;
	int antenna_off_new=0;
	static clock_t antenna_off_tick=0;
	const int antenna_delay=5*60; // 5 minutes

	antenna_off_new=isAntennaOff(wwan_id);

	// if rasing edge
	if(!antenna_off_cur && antenna_off_new) {
		antenna_off_tick=_now;

		syslog(LOG_ERR,"[signal-loss] signal lost. reconnect %d min(s) later",antenna_delay/60);
	}
	// if continous period
	else if(antenna_off_cur && antenna_off_new) {
		if((_now-antenna_off_tick)/_clock_per_second>=antenna_delay) {
			syslog(LOG_ERR,"[signal-loss] reconnect (lost signal for %d secs)",antenna_delay);
			goto kill_connection;
		}
	}
	// if falling edge
	else if(antenna_off_cur && !antenna_off_new) {

		if(!antenna_loss_reconnect)
			syslog(LOG_ERR,"[signal-loss] signal recovered in %ld sec(s)",(_now-antenna_off_tick)/_clock_per_second);
	}

	antenna_off_cur=antenna_off_new;

	antenna_loss_reconnect=0;
	return;

kill_connection:
	antenna_loss_reconnect=1;
	return;
}


/*
	is_profile_to_kill() returns true when the profile's current instance needs to get killed.
	this function is called only when the profile has a connection slot
*/
int is_profile_connection_to_kill(int profile)
{
	int wwan;
	int enabled;
	int connection;

	int pdp;
	int pdp_capable;
	int dod_waiting;
	int dod_connected;

	int ps_capable;
	int ps_attached;
	int ps_detached;
	int wwan_id;

	// get dev name
	wwan=is_wwan_profile(profile, &wwan_id);
	// get profile enable status
	enabled = get_profile_enable(profile);
	// get connection slot corresponding to the profile
	connection = profile_in_active_connections_list(profile);

	// kill the connection if the profile is disabled
	if(enabled != PROFILE_ENABLED) {
		goto kill_connection;
	}

	if(wwan) {
		// detect pdp session dropping if this profile is wwan connection and 3g network is ready
		if(!(connection<0) && glb.active_connections[connection].clkStartValid) {
			unsigned long uDiff;

			/* NHD1W does not have 3G connection control */
			#ifdef BOARD_nhd1w
			pdp=0;
			#else
			pdp=isPdpUp(profile,wwan_id, glb.active_connections[connection].sub_interface,&pdp_capable);
			#endif

			/* ps attach status */
			ps_capable=wwan_device_check_attach_capability(wwan_id);
			ps_attached=wwan_device_is_attached(wwan_id);
			ps_detached=(ps_capable&&!ps_attached);

			// check provisional period
			uDiff=(unsigned long)_now-(unsigned long)(glb.active_connections[connection].clkStart);

			/* immediately finish provisioning period when PDP is up or timeout*/
			if((pdp_capable && pdp) || (uDiff >= (unsigned long)(CONNECTION_INIT_DELAY*_clock_per_second)))
				glb.active_connections[connection].connectProvisional = 0;

			// disable PDP connection dropping detection feature if dod is enabled
			dod_waiting=is_dod_enabled(profile) && !is_dod_up(profile);
			dod_connected=is_dod_enabled(profile) && is_dod_up(profile);

			run_once_define_array(dod_waiting,MAX_PROFILES);
			run_once_define_array(dod_connected,MAX_PROFILES);

			// print PDP drop detection feature status
			if(dod_waiting) {
				run_once_func_array(dod_waiting,profile,log_ERR("PDP drop detection feature disabled - dod offline (profile=%d)",profile););

				// reset dropping detection factors
				glb.active_connections[connection].connectProvisional = 1;
				glb.active_connections[connection].clkStart = _now;
			}
			else {
				run_once_reset_array(dod_waiting,profile);
			}

			if(dod_connected)
				run_once_func_array(dod_connected,profile,log_ERR("PDP drop detection feature enabled - dod online (profile=%d)",profile););
			else
				run_once_reset_array(dod_connected,profile);


			// For nhd1w variant, do not detect 3g connection dropping because we do not maintain 3g connection
			#if ! defined(BOARD_nhd1w)
			if( !dod_waiting && (!pdp || antenna_loss_reconnect || ps_loss_reconnect) && !glb.active_connections[connection].connectProvisional) {

				if(!pdp)
					log_ERR("PDP connection dropping detected");

				if(antenna_loss_reconnect)
					log_ERR("antenna loss detected");

				if(ps_detached)
					log_ERR("PS loss detected");

				log_ERR("reconnecting (conn=%d)", profile);

				goto kill_connection;
			}
			#endif
		}

		// kill the connection if this profile is 3g connection and currently the router is mep-locked
		if(_mep_locked) {
			log_INFO("meplock detected - terminate all 3g connections");
			goto kill_connection;
		}

		#if defined(PLATFORM_PLATYPUS2) || defined(PLATFORM_BOVINE)
		if (roaming_call_blocked(wwan_id)) {
			log_INFO("roaming detected - terminate all 3g connections");
			//log_ERR("skip detecting data call drop when data call blocked in roaming area");
			mark_data_roaming_call_blocked(1);
			goto kill_connection;
		}
		else {
			mark_data_roaming_call_blocked(0);
		}
		#endif

		// kill the connection if failover enabled and currently wan connection - we are using wan as a primary
		if(is_wwan_suspended_by_failover()) {
			log_INFO("failover configuration - terminate all 3g connections");
			goto kill_connection;
		}
	}

	return 0;

kill_connection:
	return 1;
}

void get_active_profiles()
{
	int profile;
	int connection;
	int enabled;
	int wwan_id;
	struct profile_t cur_profile_info;

	for(profile =1; profile<MAX_PROFILES; profile ++) {

		enabled=get_profile_enable(profile);
		if(enabled != PROFILE_IGNORED) {

			if(is_wwan_profile(profile, &wwan_id) &&
				wwan_id != _wwan_id) continue;

			// If profile is in active connections list
			if ((connection = profile_in_active_connections_list(profile)) != -1)
			{
				// read the profile and compare to the previous profile
				read_profile(profile,&cur_profile_info);

				if(cur_profile_info.trigger) {
					profile_info[profile].trigger=1;
				}

				// if the profile is changed or disabled
				if(is_profile_connection_to_kill(profile) || is_profile_changed(profile,&cur_profile_info,&profile_info[profile])) {
					// stop connection of profile
					stop_profile_connection(profile);
				}
			}
			else {
				if(is_profile_to_connect(profile)) {
					// store the profile
					read_profile(profile,&cur_profile_info);

					if(is_profile_changed(profile,&cur_profile_info,&profile_info[profile])) {
						log_INFO("start profile with new settings - reset pdp result (pf=%d)",profile);
						setProfileVariableStr(profile,"pdp_result","");
						// relaunch script immediately
						cur_profile_info.trigger=1;
					}

					// start the profile as a connection
					if(start_profile_connection(profile)>=0) {
						memcpy(&profile_info[profile],&cur_profile_info,sizeof(cur_profile_info));
					}
				}
			}
		}

	}
}

struct daemon_database {
	const char* dbName;
	const char* dbValue;

	const char* dbName2;
	const char* dbValue2;

	const char* psCmd;
	const char* psCmd2;
	const char* psCmd3;
#ifdef USE_DCCD
	const char* psCmd4;
#endif
	const char* execCmd;

	int checkPeriodSec;
	tick lastCheck;
};

//
// TODO:
// this is left-over from unstable 888. We may need to remove this monitoring feature in connection_mgr
// because supervisor looks after the router
//

#ifdef PLATFORM_AVIAN
struct daemon_database _daemonDb[]={
	// end
	{ 0,									0,		0,									0,		0,																				0}
};
struct daemon_database _daemonDb10[]={
	// end
	{ 0,									0,		0,									0,		0,																				0}
};
#else
#ifdef USE_DCCD
struct daemon_database _daemonDb[]={
	// hostapd
	{ "wlan.0.enable",	"1",	"wlan.0.activated",	"1",	"hostap",			0,					0, 0, "rdb_set wlan.0.enable 1",  20, 0},
	// cnsmgr
	{ "wwan.0.activated",	"1",	0,			0,	"/usr/bin/cnsmgr",		"/usr/bin/simple_at_manager",	"/usr/bin/dccd", 	"/usr/bin/qmimgr", "/usr/bin/relaunch_dccd.sh", 10, 0},
	// appweb
	{ 0,			0,	0,			0,	"appweb -f appweb.conf",	0,					0, 0, "/etc/init.d/rc.d/appweb.sh start", 10, 0},
#ifdef V_DISP_DAEMON
	// dispd
	{ "dispd.activated",	"1",	0,			0,	"/usr/bin/dispd",		0,					0, 0, "/usr/bin/dispd 2> /dev/null > /dev/null &", 10, 0},
#endif
	// end
	{ 0,			0,	0,			0,	0,				0,					0, 0, 0}
};
struct daemon_database _daemonDb10[]={
	// end
	{ 0,									0,		0,									0,		0,																				0}
};

#else
struct daemon_database _daemonDb[]={
	// hostapd
	{ "wlan.0.enable",	"1",	"wlan.0.activated",	"1",	"hostap",			0,					0, "rdb_set wlan.0.enable 1",  20, 0},
	// cnsmgr
	{ "wwan.0.activated",	"1",	0,			0,	"/usr/bin/cnsmgr",		"/usr/bin/simple_at_manager",		"/usr/bin/qmimgr", "sys -m 1; /usr/bin/cdcs_init_wwan_pm", 10, 0},
	// appweb
	{ 0,			0,	0,			0,	"appweb -f appweb.conf",	0,					0, "/etc/init.d/rc.d/appweb.sh start", 10, 0},
#ifdef V_DISP_DAEMON
	// dispd
	{ "dispd.activated",	"1",	0,			0,	"/usr/bin/dispd",		0,					0, "/usr/bin/dispd 2> /dev/null > /dev/null &", 10, 0},
#endif
	// end
	{ 0,			0,	0,			0,	0,				0,					0, 0}
};

struct daemon_database _daemonDb10[]={
#ifdef V_SECONDARY_3GWWAN
	{"wwan.10.activated",	"1",	0,			0,	"/usr/bin/cnsmgr10",		"/usr/bin/simple_at_manager10",		"/usr/bin/qmimgr10", "xsys -m 1; /usr/bin/cdcs_init_wwan_pm10", 10, 0},
#endif
	{ 0,			0,	0,			0,	0,				0,					0, 0}
};

#endif

#endif

char* getPidCmdLine(pid_t pid)
{
	char fileName[128];

	int i;

	char* src;
	char* dst;

	char rawCmdLine[256];
	int rawCmdLineLen;

	static char cmdLine[256+1];

	snprintf(fileName,sizeof(fileName),"/proc/%d/cmdline",pid);

	int fd=open(fileName,O_RDONLY | O_NOCTTY);
	if(fd<0)
		goto error;

	// read command line
	rawCmdLineLen=read(fd,rawCmdLine,sizeof(rawCmdLine));
	if(rawCmdLineLen<0)
		goto error;

	src=rawCmdLine;
	dst=cmdLine;

	// replace nulls with spaces
	i=0;
	while(i<rawCmdLineLen)
	{
		if(*src==0)
			*dst=' ';
		else
			*dst=*src;

		src++;
		dst++;
		i++;
	}

	*dst=0;

	close(fd);

	return cmdLine;

error:
	if(fd>=0)
		close(fd);

	return 0;
}

int isNumeric(const char* str)
{
	while(*str)
	{
		if(!isdigit(*str))
			return 0;

		str++;
	}

	return 1;
}

int isRunning(const char* cmd)
{
	struct dirent **nameList;
	int nameCnt = scandir("/proc/.", &nameList, NULL, NULL);

	int nameIdx;

	char *pStr;

	char* cmdLine;
	int found =0;

	pid_t pid;

	for(nameIdx=0;nameIdx<nameCnt;nameIdx++)
	{
		// is numeric? otherwise bypass
		if(!isNumeric(nameList[nameIdx]->d_name))
			continue;

		// get pid
		pid=atoi(nameList[nameIdx]->d_name);
		if(!pid)
			continue;

		// get cmdline
		cmdLine=getPidCmdLine(pid);
		if(!cmdLine)
			continue;

		// search
		pStr=strstr(cmdLine,cmd);

		if(pStr){
			// match the exact program name
			if(pStr[strlen(cmd)] ==' '){
				found =1;
				break;
			}
		}
	}

	// free
	for(nameIdx=0;nameIdx<nameCnt;nameIdx++)
		free(nameList[nameIdx]);

	free(nameList);

	return found;
}

#ifndef V_DISP_DAEMON
void leds_monitor()
{
	const char* wlan_led_activation_cmd;

	static int fFirstTime=1;
	static int fPrevStat=0;

	int fNewStat;

#ifdef PLATFORM_BOVINE
	fNewStat=isRunning("hostap");
	wlan_led_activation_cmd="led sys wlan state 1";
#elif PLATFORM_ANTELOPE
	fNewStat=isRunning("hostap");
	wlan_led_activation_cmd="led sys wlan state 1";
#elif PLATFORM_SERPENT
	fNewStat=isRunning("hostap");
	wlan_led_activation_cmd="led sys wlan state 1";
#elif PLATFORM_PLATYPUS2
	fNewStat=is_interface_up("ra0");
	wlan_led_activation_cmd="led sys wlan trigger 1";
#elif PLATFORM_AVIAN
	fNewStat=0;
	wlan_led_activation_cmd="";
	return;
#else
	#error PLATFORM_XXXX not defined
#endif

	// wlan led configuration
	if( fFirstTime || (fNewStat && !fPrevStat) || (!fNewStat && fPrevStat) )
	{
		syslog(LOG_INFO,"setting WLAN LED - new=%d,prev=%d",fNewStat,fPrevStat);

		if(fNewStat) {
			system(wlan_led_activation_cmd);
		}
		else {
			system("led sys wlan state 0");
		}
	}


	fPrevStat=fNewStat;
	fFirstTime=0;
}
#endif


void daemon_monitor(int wwan_id)
{
	struct daemon_database* daemonDbPtr;

	char value1[64];
	char value2[64];

	tick curTick=getTickCountMS();

	daemonDbPtr=_daemonDb;
	if(wwan_id == 10){
		daemonDbPtr=_daemonDb10;
	}

	while(daemonDbPtr->psCmd){
		if(!daemonDbPtr->lastCheck)
			daemonDbPtr->lastCheck=curTick;

		// get value
		value1[0]=0;
		if(daemonDbPtr->dbName)
			if(rdb_get_string(g_rdb_session, daemonDbPtr->dbName,value1,sizeof(value1))<0)

		// get value2
		value2[0]=0;
		if(daemonDbPtr->dbName2)
			rdb_get_string(g_rdb_session, daemonDbPtr->dbName2,value2,sizeof(value2));

		int valueMatched=!daemonDbPtr->dbValue || !strcmp(daemonDbPtr->dbValue,value1);
		int value2Matched=!daemonDbPtr->dbValue2 || !strcmp(daemonDbPtr->dbValue2,value2);
		int runningStatus;


		if(valueMatched && value2Matched)
		{
			// get running status
			runningStatus=isRunning(daemonDbPtr->psCmd);
			if(daemonDbPtr->psCmd2)
				runningStatus=runningStatus || isRunning(daemonDbPtr->psCmd2);
			if(daemonDbPtr->psCmd3)
				runningStatus=runningStatus || isRunning(daemonDbPtr->psCmd3);
#ifdef USE_DCCD
			if(daemonDbPtr->psCmd4)
				runningStatus=runningStatus || isRunning(daemonDbPtr->psCmd4);
#endif

			if(!runningStatus)
			{
				if(curTick-daemonDbPtr->lastCheck>daemonDbPtr->checkPeriodSec*1000)
				{
					syslog(LOG_ERR,"process malfunction detected - run %s",daemonDbPtr->execCmd);
					system(daemonDbPtr->execCmd);
					daemonDbPtr->lastCheck=curTick;
				}
			}
			else
			{
				daemonDbPtr->lastCheck=curTick;
			}
		}
		else
		{
			daemonDbPtr->lastCheck=curTick;
		}

		daemonDbPtr++;
	}

}

void init_glb(void)
{
	int x;

	// get clock per second
	_clock_per_second=sysconf(_SC_CLK_TCK);

	memset(glb.active_connections,0,sizeof(glb.active_connections));

	for (x = 0; x < MAX_CONNECTIONS; x++)
	{
		glb.active_connections[x].profile = INACTIVE;
		glb.active_connections[x].pid = -1;
		glb.active_connections[x].sub_interface = -1;
	}
}

int check_comm_manager_heartbeat(int wwan_id)
{
// Sequans VZ20Q sometimes locked up when repeating activating and deactivating profile
// so this heart beat checking logic should be used
#if (defined PLATFORM_AVIAN) || (defined USE_DCCD)
	// TODO:
	//	Apply this to all the platforms
	//	Make sure which one to kill - cnsmgr or simple_at_manger
	//	make sure if the manager is automatically launching again or not

	static long long prev_heartbeat=0;
	static clock_t hearbeat_change_sec=0;
	static clock_t cur_sec;
	long long new_heartbeat;

	// different heartbeats for platforms
	#define AVIAN_TIMEOUT	(1*60)
	#define BOVINE_TIMEOUT	(2*60)

	const clock_t heartbeat_timeout=BOVINE_TIMEOUT;

	// read heart beat
	new_heartbeat=atoll(rdb_get_i_value(wwan_id, "wwan.0.heart_beat"));

	cur_sec=__getTickCount()/_clock_per_second;

	// update last heart beat
	if(!hearbeat_change_sec || (new_heartbeat!=prev_heartbeat))
		hearbeat_change_sec=cur_sec;
	prev_heartbeat=new_heartbeat;


	// if heart beat timeout
	if( cur_sec-hearbeat_change_sec>heartbeat_timeout )
	{

#ifdef USE_DCCD
		log_INFO("*********************************************");
		log_INFO("DCCD heart beat failure detected");
		log_INFO("*********************************************");
		//system("killall -9 dccd");
		system("/usr/bin/relaunch_dccd.sh");
#else
		log_INFO("simple_at_manager heart beat failure detected");
		system("killall -9 simple_at_manager");
#endif
		hearbeat_change_sec=0;
	}
#else
	// Since all other platforms are checked and monitored by daemon_database, we do not check
	// simple_at_manager running status in this function
#endif
	return 0;
}

#ifdef V_MANUAL_ROAMING_vdfglobal
long int get_sys_uptime()
{
	FILE *fp;
	long int sys_uptime = 0;
	char buf[64];

	fp=fopen("/proc/uptime", "r");
	if( fp ) {
		fgets(buf, sizeof(buf), fp);
		sys_uptime = strtol(buf, NULL, 10);
		fclose(fp);
	}
	return sys_uptime;
}

// return value:
// 1 -> connection manager is suspended
// 0 -> connection manager is not suspended
int conn_mgr_suspend_state()
{
	char buf[32]={0,};
	long int suspend_delay=0, curr_sysuptime=0;

	if (rdb_get_string(g_rdb_session, "manualroam.suspend_connection_mgr", buf, sizeof(buf)) < 0)
		return 0;

	suspend_delay = strtol(buf, NULL, 10);

	if (suspend_delay == 0)
		return 0;

	curr_sysuptime = get_sys_uptime();

	if (suspend_delay > curr_sysuptime)
		return 1;

	rdb_set_value("manualroam.suspend_connection_mgr",""); // to avoid redundant function call on get_sys_uptime().
	return 0;
}
#endif
/* get the instance number from program name
 connection_mgr -> return 0
 connection_mgr10 -> return 10
*/
static int get_instance(const char *argv)
{
	const char *p= &argv[strlen(argv)-1];
	while(p > argv && *p >='0'&& *p <='9' ) p--;
	return atoi(p+1);
}

int main(int argc, char **argv)
{
	//int	argn;
	//char *argp;
	int	ret = 0;
	int	verbosity = 0;
	int	be_daemon = 1;

	int sim_ok;
	int cur_sim_ok;

	initTickCount();

	int strength;
	int init_delay=0;
	int profile=1;
	int wwan_id=0;

	int lte;
	/*get wwan_id from the program name*/
	_wwan_id = get_instance(argv[0]);

	// Parse Options
	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 'd':
				be_daemon = 0;
				break;
			case 'v':
				verbosity++ ;
				break;
			case 's':
				_default_spec = atoi(optarg) ;
				_default_spec = (_default_spec>=0 && _default_spec< MAX_SPEC)? _default_spec:0;
				break;
			case 'V':
				fprintf(stderr, "%s Version %d.%d.%d\n", argv[0],
				        VER_MJ, VER_MN, VER_BLD);
				break;
			case '?':
				usage(argv);
				return 2;
		}
	}
	*_spec_list[_default_spec]=1;
	// Initialize the logging interface
	log_INIT(DAEMON_NAME, be_daemon);
	//openlog( DAEMON_NAME, LOG_PID | (be_daemon ? 0 : LOG_PERROR), LOG_LOCAL5 );
	//syslog( LOG_INFO, "starting" );

	if (be_daemon)
	{
		char lockfile[128];
		sprintf(lockfile, "/var/lock/subsys/"DAEMON_NAME"%-d", _wwan_id);
		daemonize(lockfile, RUN_AS_USER);
		log_INFO("daemonized");
	}


	// Configure signals
	glb.run = 1;
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	//signal(SIGCHLD, sig_handler);

	if (rdb_open(NULL, &g_rdb_session) < 0)
		pabort("can't open device");

	init_glb();

if(TELSTRA_SPEC_Santos){
	resetTelstraReconnectCounts();
}

#ifdef DEBUG_MEP
	log_ERR("!!! MEP lock debug feature is enabled - %s", __TIME__);
	log_ERR("!!! Turn off the debug feature");
#endif

	#if (defined PLATFORM_AVIAN) || (defined V_MEPLOCK)
	// read mncmcc
	update_mncmcc_codes();

	// create the result variables
	rdb_set_value(DB_MEPLOCK_RESULT,"");
	rdb_set_value(DB_MEPLOCK_CODE,"");
	rdb_set_value(DB_MEPLOCK_STATUS,"");

	// create and subscribe status
	rdb_create_string(g_rdb_session, DB_MEPLOCK_CODE, "", CREATE, ALL_PERM);
	if(rdb_subscribe(g_rdb_session, DB_MEPLOCK_CODE)<0)
	{
		syslog(LOG_ERR,"failed to subscribe %s - %s",DB_MEPLOCK_CODE,strerror(errno));
		return -1;
	}


	#endif
	/*reset ESM_reconnect_delay*/
	for(profile =1; profile< MAX_PROFILES; profile++){
		if(get_profile_enable(profile) != PROFILE_IGNORED ){
			if(is_wwan_profile(profile, &wwan_id) && (wwan_id - (wwan_id%10)) == _wwan_id) {
				if(getProfileVariableInt(profile, "ESM_reconnect_delay") >0 )
					setProfileVariableInt(profile, "ESM_reconnect_delay", 0);
			}
		}
	}

	// We should populate the active connections list here with any active connections in case
	// we are accidentally killed.
	// For ppp we can read any /var/lock/ppp-<profile>.pid files to find what we need.

	sim_ok=1;
	while (glb.run){
		_now = __getTickCount();


		// check daemons
		daemon_monitor(_wwan_id);
		// check leds
		#ifndef V_DISP_DAEMON
		leds_monitor();
		#endif
		/*search matched wwan profile */
		for(profile =1; profile< MAX_PROFILES; profile++){
			if(get_profile_enable(profile) != PROFILE_IGNORED){
				if(is_wwan_profile(profile, &wwan_id) && (wwan_id - (wwan_id%10)) == _wwan_id) break;
			}
		}

		if(profile >=  MAX_PROFILES){
			sleep(1);
			continue;
		}
		#if (defined PLATFORM_AVIAN) || (defined V_MEPLOCK)
		// update mep status
		_mep_locked=update_mep_status(_wwan_id)<0;

		if(_mep_locked)
			force_down_network();

		if(!(rdb_wait(0)<0))
			handle_commands();
		#endif

		// check and log sim card status
		cur_sim_ok=sim_status_check(_wwan_id);
		if( (cur_sim_ok && !sim_ok) || (!cur_sim_ok && sim_ok) ) {

			if(cur_sim_ok) {
				#ifndef CELL_NW_cdma
				syslog(LOG_INFO,"SIM card is ready");
				#endif
			}
			else {
				char sim_status[64];

				if (rdb_get_i_string(_wwan_id, "wwan.0.sim.status.status", sim_status, sizeof(sim_status)) < 0) {
					#ifndef CELL_NW_cdma
					syslog(LOG_ERR,"fail to get sim card status - %s",strerror(errno));
					#endif
					strcpy(sim_status,"unknown");
				}

				#ifndef CELL_NW_cdma
				syslog(LOG_INFO,"SIM card is *not* ready - check sim card status (%s)",sim_status);
				#endif
			}

			sim_ok=cur_sim_ok;
		}

		#if defined(BOARD_nhd1w)
		// we do not maintain 3G connection for nhd1w - immeidiately connect to MHS and 3G connection is managed by MHS
		if(wwan_device_get_heart_beat(_wwan_id)>=0) {
			_wwan_ready_to_connect=1;
		}
		#else

		#define PS_WAIT_TIMEOUT 10
		#define NETWORK_WAIT_TIMEOUT 60
		#define LTE_PS_ATTACH_DELAY 10
		#define INIT_DELAY 30
		//#define INIT_DELAY 0

		static int ps_wait=PS_WAIT_TIMEOUT;
		static int network_wait=NETWORK_WAIT_TIMEOUT;

		run_once_define(start_wwan_ps);
		run_once_define(ps_wait);
		run_once_define(ps_wait_network);
		run_once_define(network_wait);
		run_once_define(network_wait_timeout);

		run_once_define(init_wait);

		int activated;
		#if defined(CELL_NW_cdma)
		run_once_define(cdma_activation);
		run_once_define(cdma_not_ready);
		#endif

		int pri_ready;

		// get signal strength status
		wwan_device_signal_strength(_wwan_id, &strength);

		#if defined(CELL_NW_cdma)
		if(!is_cdma_activated(_wwan_id, &activated)) {
			run_once_func(cdma_not_ready,log_ERR("cdma module not ready"););
		}
		else if(!activated) {
			run_once_func(cdma_activation,log_ERR("cdma module not activated"););
		}
		else {
			run_once_reset(cdma_activation);
			run_once_reset(cdma_not_ready);
		}

		#else
		activated=1;
		#endif

		/* is pri ready? */
		pri_ready=is_pri_available(_wwan_id);

		_wwan_ready_to_connect=0;

		run_once_func(init_wait,init_delay=INIT_DELAY; syslog(LOG_INFO,"start-up delay applied (delay=%d)",init_delay););

		// wait for a while for the module to settle down after SIM_OK
		if(init_delay)
			init_delay--;

		// assume it is LTE if it is not 3g
		lte=strcmp(rdb_get_i_value(_wwan_id, "wwan.0.conn_type"),"3g") ;
		// if sim okay and signal strength is higer than =110
		if(pri_ready && activated && !init_delay && cur_sim_ok)
		{
			/*

			This sequence works for LTE and UMTS together but the half a minute delay after PS attached is only applied to removable dongles

			1) force to start connection scripts (but a minute delay applied) even when not registered with network - registration requires the correct APN in LTE
			2) check signal strength - we ignore network registration status and PS attach and we don't want to start scripts without the antenna!
			3) apply half a minute delay after PS attached - do not connect to 3g immediately! just wait until LTE is ready

			*/
			int network_stat=wwan_device_check_network_registered(_wwan_id);
			int network_registered=network_stat>0;
			int network_capable=network_stat>=0;

			int ps_capable=wwan_device_check_attach_capability(_wwan_id);
			int ps_attached=wwan_device_is_attached(_wwan_id);

			// 1 -> connection manager is suspended
			// 0 -> connection manager is not suspended
			int is_conn_mgr_suspended = 0;
			#ifdef V_MANUAL_ROAMING_vdfglobal
			is_conn_mgr_suspended = conn_mgr_suspend_state();
			#endif

			int ps_att;

			// start wwan if ps attached or ps timeout
			if(!is_conn_mgr_suspended && (((ps_att=(ps_capable && ps_attached))!=0) || (!ps_wait && (lte || !ps_capable))) ) {
				run_once_func(start_wwan_ps, syslog(LOG_INFO,ps_att?"starting WWAN profile(s) - ps attached":"starting WWAN profile(s) - ps attach timeout"););

				_wwan_ready_to_connect=1;

				//run_once_reset(start_wwan_ps);
				run_once_reset(ps_wait);
				run_once_reset(ps_wait_network);
				run_once_reset(network_wait);
				run_once_reset(network_wait_timeout);
			}
			// count down ps timeout if network registered
			else if(network_capable && network_registered) {
				run_once_func(ps_wait_network,ps_wait=PS_WAIT_TIMEOUT; syslog(LOG_INFO,"network registered - wait until PS attach"););

				if(ps_wait)
					ps_wait--;

				run_once_reset(start_wwan_ps);
				run_once_reset(ps_wait);
				//run_once_reset(ps_wait_network);
				run_once_reset(network_wait);
				run_once_reset(network_wait_timeout);
			}
			// count down ps timeout if not network timeout
			else if(!network_wait) {
				run_once_func(ps_wait,ps_wait=PS_WAIT_TIMEOUT; syslog(LOG_INFO,"network reg. timeout - wait until PS attach"););

				if(ps_wait)
					ps_wait--;

				run_once_reset(start_wwan_ps);
				//run_once_reset(ps_wait);
				run_once_reset(ps_wait_network);
				run_once_reset(network_wait);
				run_once_reset(network_wait_timeout);
			}
			// reset ps timeout ps timeout if not registered
			else if(network_capable && !network_registered) {
				run_once_func(network_wait,network_wait=NETWORK_WAIT_TIMEOUT; syslog(LOG_INFO,"wait until network reg (capable, %d sec timeout)",network_wait););

				if(network_wait)
					network_wait--;

				run_once_reset(start_wwan_ps);
				run_once_reset(ps_wait);
				run_once_reset(ps_wait_network);
				//run_once_reset(network_wait);
				run_once_reset(network_wait_timeout);
			}
			// count down network timeout if network registration not capable
			else if( !network_capable ) {
				run_once_func(network_wait_timeout,network_wait=NETWORK_WAIT_TIMEOUT; syslog(LOG_INFO,"wait until network reg (not capable, %d sec timeout)",network_wait););

				if(network_wait)
					network_wait--;

				run_once_reset(start_wwan_ps);
				run_once_reset(ps_wait);
				run_once_reset(ps_wait_network);
				run_once_reset(network_wait);
				//run_once_reset(network_wait_timeout);
			}
		}
		else
		{
			ps_wait=PS_WAIT_TIMEOUT;
			network_wait=NETWORK_WAIT_TIMEOUT;

			run_once_reset(start_wwan_ps);
			run_once_reset(ps_wait);
			run_once_reset(ps_wait_network);
			run_once_reset(network_wait);
			run_once_reset(network_wait_timeout);
		}
		#endif

		// check antenna loss
		update_antenna_loss(_wwan_id);
		update_ps_stat(_wwan_id);

		/* update default WWAN profile */
		default_wwan_profile = find_wwan_default_profile();
		if (prev_default_wwan_profile == INACTIVE) {
			prev_default_wwan_profile = default_wwan_profile;
		}

		// We should populate
		get_active_profiles();

		// maintain connect list
		manage_active_connections();

		// workdaround for missing sigchild
		collect_children();

		check_comm_manager_heartbeat(_wwan_id);

		sleep(1);
	}

	manager_shutdown(0);
	return 0;
}

// suppress bogus warning when compiling with gcc 4.3
#if (__GNUC__ == 4 && __GNUC_MINOR__ == 3)
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

int is_profile_changed(int profile_no,const struct profile_t* profile_info_1,const struct profile_t* profile_info_2)
{
	int i;
	int quiet;
	int conn;

	if(profile_info_1->trigger || profile_info_2->trigger)
		return 1;

	//syslog(LOG_INFO,"[rdb check] compare profile#%d to rdb",profile_no);

	// quiet if we already started killing the connection
	conn = profile_in_active_connections_list(profile_no);
	quiet=glb.active_connections[conn].sigterm_sent;

	for(i=0;profile_rdb_str[i]!=0;i++) {
		if(strcmp(profile_info_1->rdb_str[i],profile_info_2->rdb_str[i])) {
			/*
			if(!quiet)
				syslog(LOG_INFO,"profile configuration changed - profile=%d,str-field=%d",profile_no,i);
			*/
			goto chg;
		}
	}

	for(i=0;profile_rdb_int[i]!=0;i++) {
		if(profile_info_1->rdb_int[i]!=profile_info_2->rdb_int[i]) {
			if(!quiet)
				syslog(LOG_INFO,"profile configuration changed - profile=%d,int-field=%d",profile_no,i);
			goto chg;
		}
	}

	//syslog(LOG_INFO,"[rdb check] profile#%d not changed",profile_no);
	return 0;

chg:
	//syslog(LOG_INFO,"modification  of profile#%d detected",profile_no);
		return 1;
}

int read_profile(int profile_no,struct profile_t* profile_info)
{
	const char* profile_val;
	char rdb[MAX_NAME_LENGTH+1];
	int i;

	//syslog(LOG_INFO,"[rdb check] read profile#%d from rdb",profile_no);

	memset(&profile_info,sizeof(profile_info),0);

	// store string rdb variables
	for(i=0;profile_rdb_str[i];i++) {

		if(MAX_PROFILE_RDB<=i) {
			syslog(LOG_ERR,"too many profile rdb variables configured - ignored #1 (max=%d)",MAX_PROFILE_RDB);
			continue;
		}

		// get profile rdb variable
		snprintf(rdb,sizeof(rdb),profile_rdb_str[i],profile_no);
		profile_val=rdb_get_value(rdb);

		// copy string
		strncpy(profile_info->rdb_str[i],profile_val,MAX_PROFILE_RDB_VAL_LEN);
		profile_info->rdb_str[i][MAX_PROFILE_RDB_VAL_LEN-1]=0;
	}

	// store int rdb variables
	for(i=0;profile_rdb_int[i];i++) {

		if(MAX_PROFILE_RDB<=i) {
			syslog(LOG_ERR,"too many profile rdb variables configured - ignored #2 (max=%d)",MAX_PROFILE_RDB);
			continue;
		}

		// get profile rdb variable
		snprintf(rdb,sizeof(rdb),profile_rdb_int[i],profile_no);
		profile_info->rdb_int[i]=rdb_get_Int(rdb);
	}

	/* read "changed" status */
	snprintf(rdb,sizeof(rdb),"link.profile.%d.trigger",profile_no);
	profile_info->trigger=rdb_get_Int(rdb);
	if(profile_info->trigger)
		rdb_set_value(rdb,"0");

	return 0;
}

// Note dynamic profiles are only supported on Bovine.
#ifdef PLATFORM_BOVINE
// Check if this profile is marked for deletion and if so set it defunct.
// This should be called when the profile is removed from the connection list.
// @param profile	RDB:link.profile number
void checkSetProfileDefunct(int profileNum)
{
	if (getProfileDelflag(profileNum) == PROFILE_NOT_NEEDED) {
		(void)setProfileVariableInt(profileNum, "delflag",	PROFILE_DEFUNCT);

		// Wakeup the reaper, so this profile can be deleted.
		const char *name = "link.profile.reaper_trigger";
		int stat = rdb_set_string(g_rdb_session, name, "1"); /* Note: 1 is an arbitrary value */
		if (stat < 0) {
			syslog(LOG_ERR, "rdb_set_string(%s) - %s", name, strerror(-stat));
		}
	}
}
#else
void checkSetProfileDefunct(int profileNum) { }
#endif

/*
* vim:ts=4:sw=4:
*/
