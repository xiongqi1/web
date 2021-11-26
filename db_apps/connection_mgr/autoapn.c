


#include <stdio.h>
#include <unistd.h>
#include <error.h>
#include <time.h>
#include <sys/times.h>
#include <syslog.h>
#include <strings.h>

#define _GNU_SOURCE
#include <string.h>

#include "mxml.h"
#include "rdb_ops.h"

/*

webinterface.autoapn   (1 = autoapn, 0 = do not do autoapn)

wwan.0.system_network_status.simICCID
wwan.0.system_network_status.attached
wwan.0.imsi.plmn_mcc
wwan.0.imsi.plmn_mnc

link.profile.$PROFILE.autoapn
link.profile.$PROFILE.apn
link.profile.$PROFILE.dialstr
link.profile.$PROFILE.user
link.profile.$PROFILE.pass
link.profile.$PROFILE.verbose_logging
link.profile.$PROFILE.defaultroute
link.profile.$PROFILE.userpeerdns
link.profile.$PROFILE.retry
link.profile.$PROFILE.auth_type (pap/chap)

link.profile.$PROFILE.autoapn_stat	# auto apn status
link.profile.$PROFILE.ccid		# auto apn ccid


* avian
/system/cdcs/www/cgi-bin/apnList.xml
/system/cdcs/www/cgi-bin/platform_apn.xml

* bovine
/www/cgi-bin/apnList.xml
/www/cgi-bin/platform_apn.xml

* platypus
/etc_ro/www/internet/apnList.xml
/etc_ro/www/internet/extra_apn_3g8wv.xml

*/


// local defines
////////////////////////////////////////////////////////////////////////

#define AUTOAPN_MSG_MAX_BUFFER			256
#define AUTOAPN_DB_VALUE_MAX_BUFFER		256
#define AUTOAPN_DB_VARIABLE_MAX_NAME		256
#define AUTOAPN_DELAY_FOR_ATTACH		30	// seconds
#define AUTOAPN_DELAY_FOR_MCCMNC		30	// seconds
#define AUTOAPN_DELAY_FOR_CCID			30	// seconds
#define AUTOAPN_FIN_DELAY			10	// seconds
#define AUTOAPN_CONNECTION_STOP_DELAY		5	// seconds
#define AUTOAPN_CONNECTION_MAX_DELAY		60	// seconds

#define AUTOAPN_MAX_SCORING			1

#if defined(PLATFORM_AVIAN)
#define AUTOAPN_APNLIST_XML_FILE		"/system/cdcs/www/cgi-bin/apnList.xml"

#elif defined(PLATFORM_BOVINE)
#define AUTOAPN_APNLIST_XML_FILE		"/www/cgi-bin/apnList.xml"

#elif defined(PLATFORM_PLATYPUS)
#define AUTOAPN_APNLIST_XML_FILE		"/etc_ro/www/internet/apnList.xml"

#else
#error Unknown Platform

#endif


#define AUTOAPN_SCORING_SCRIPT			"score_connection.sh"

#define AUTOAPN_DB_PROFILE_APN		"apn"
#define AUTOAPN_DB_PROFILE_DIALSTR	"dialstr"
#define AUTOAPN_DB_PROFILE_USER		"user"
#define AUTOAPN_DB_PROFILE_PASS		"pass"
#define AUTOAPN_DB_PROFILE_AUTH		"auth_type"

#define AUTOAPN_DB_PROFILE_COUNTRY	"country"
#define AUTOAPN_DB_PROFILE_MCC		"mcc"
#define AUTOAPN_DB_PROFILE_CARRIER	"carrier"
#define AUTOAPN_DB_PROFILE_MNC		"mnc"
#define AUTOAPN_DB_PROFILE_SCORE	"score"

#define AUTOAPN_DB_STATIC		"autoapn.stage"
#define AUTOAPN_DB_PROGRESS		"autoapn.progress"
#define AUTOAPN_DB_RETRY		"autoapn.retry_count"

#define AUTOAPN_DB_PROFILE_PREFIX	"link.profile."
#define AUTOAPN_DB_WWAN_PREFIX		"wwan."

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

struct apn_login_struc {
	
	char apn[64];
	char user_name[64];
	char password[64];
	char auth[16];
	char dial[32];
	
	char country[32];
	char mcc[8];
	char carrier[64];
	char mnc[8];
	
	char score[8];
};



// staging enum
////////////////////////////////////////////////////////////////////////

// staging for post check 
enum {
	postcheck_idle=0,
	postcheck_sim, 
	postcheck_ccid, 
	postcheck_ccid_wait, 
	postcheck_mccmnc,
	postcheck_mccmnc_wait,
	postcheck_network_attach,
	postcheck_network_attach_wait,
	postcheck_findelay_init,
	postcheck_findelay_wait,
	postcheck_done, 
	postcheck_error, 
	postcheck_unknown
};

// staging for apnscan
enum {
	apnscan_idle=0,
 	apnscan_wait_until_quiet,
	apnscan_check_prev_ccid,
	apnscan_open_xml,
	apnscan_dryrun,
	apnscan_wetrun,
 	apnscan_hint_next,
	apnscan_search_mcc_first,
	apnscan_search_mcc_next,
	apnscan_search_mnc_first, 
	apnscan_search_apn_first,
	apnscan_search_apn_extract,
	apnscan_start_connect,
	apnscan_wait_until_up,
	apnscan_score, 
	apnscan_disconnect,
	apnscan_disconnect_wait,
	apnscan_search_apn_next,
	apnscan_search_mnc_next,
	apnscan_pick_best,
	apnscan_connect_static,
	apnscan_done,
	apnscan_error,
	apnscan_unknown
};

// running mode
enum {
	running_mode_dry, 
	running_mode_hint_only, 
 	running_mode_normal,
	running_mode_hint
};

// extern functions
////////////////////////////////////////////////////////////////////////
char *strcasestr(const char *haystack, const char *needle);

// dummy call-back functions
////////////////////////////////////////////////////////////////////////
static int dummy_callback_func1();
static void dummy_callback_func2();

// call-back functions
////////////////////////////////////////////////////////////////////////
// launch the connections script
static void (*start_wwan)(void)=dummy_callback_func2;
// stop the connection script
static void (*stop_wwan)(void)=dummy_callback_func2;
// check connection
static int (*is_wwan_up)(void)=dummy_callback_func1;
// save rdb to nvram - platypus specific
static void (*save_to_nvram)(void)=dummy_callback_func2;


// local variables
////////////////////////////////////////////////////////////////////////
static char wwan_db_prefix[AUTOAPN_DB_VARIABLE_MAX_NAME]="wwan.0.";
static char profile_db_prefix[AUTOAPN_DB_VARIABLE_MAX_NAME]="link.profile.0.";
static char autoapn_static_prefix[AUTOAPN_DB_VARIABLE_MAX_NAME]="autoapn.profile.";
static int conn_term=0;

// hint apnts
#define MAX_HINT_APN_LOGS	32
static struct apn_login_struc hint_apn_logins[MAX_HINT_APN_LOGS]; // about 11kb
static int hint_apn_login_idx=0;
static int total_hint_apn_cnt=0;

// best apn during autoapn scanning procedure
static struct apn_login_struc best_apn_login;
static int best_apn_score;

// current apn used during autoapn scanning procedure
static struct apn_login_struc current_apn_login;
static int current_apn_score;

// system value - clocks per second
static clock_t clock_per_second=0;

static FILE *fp_xml=0;
static mxml_node_t *tree=0;

// information for autoscan
static char autoapn_country[AUTOAPN_DB_VALUE_MAX_BUFFER]="";
static char autoapn_mcc[AUTOAPN_DB_VALUE_MAX_BUFFER];
static char autoapn_mnc[AUTOAPN_DB_VALUE_MAX_BUFFER];
static char autoapn_ccid[AUTOAPN_DB_VALUE_MAX_BUFFER];
static char autoapn_hint[AUTOAPN_DB_VALUE_MAX_BUFFER];

// current post stage
static int post_stage=postcheck_idle;
// current apnscan stage
static int apnscan_stage=apnscan_idle;

static int total_apn_cnt;		// total apn count
static int current_apn_idx;		// current apn index
static int autoscan_retry_count;
	
static int running_mode=running_mode_dry;
	
	

// local functions
////////////////////////////////////////////////////////////////////////
static const char* get_single(const char* prefix,const char* var);
static int set_single(const char* prefix,const char* var,const char* val);
static void update_apn_stage(const char* stage);
static int is_valid_ccid(const char* ccid);

	
static int load_apn_from_db(const char* db_prefix,struct apn_login_struc* apn_login)
{
	memset(apn_login,0,sizeof(*apn_login));
	
	// login detail
	SAFE_STRNCPY(apn_login->apn,get_single(db_prefix,AUTOAPN_DB_PROFILE_APN),sizeof(apn_login->apn));
	SAFE_STRNCPY(apn_login->user_name,get_single(db_prefix,AUTOAPN_DB_PROFILE_USER),sizeof(apn_login->user_name));
	SAFE_STRNCPY(apn_login->password,get_single(db_prefix,AUTOAPN_DB_PROFILE_PASS),sizeof(apn_login->password));
	SAFE_STRNCPY(apn_login->auth,get_single(db_prefix,AUTOAPN_DB_PROFILE_AUTH),sizeof(apn_login->auth));
	SAFE_STRNCPY(apn_login->dial,get_single(db_prefix,AUTOAPN_DB_PROFILE_DIALSTR),sizeof(apn_login->dial));
	
	// extra information
	SAFE_STRNCPY(apn_login->country,get_single(db_prefix,AUTOAPN_DB_PROFILE_COUNTRY),sizeof(apn_login->country));
	SAFE_STRNCPY(apn_login->mcc,get_single(db_prefix,AUTOAPN_DB_PROFILE_MCC),sizeof(apn_login->mcc));
	SAFE_STRNCPY(apn_login->carrier,get_single(db_prefix,AUTOAPN_DB_PROFILE_CARRIER),sizeof(apn_login->carrier));
	SAFE_STRNCPY(apn_login->mnc,get_single(db_prefix,AUTOAPN_DB_PROFILE_MNC),sizeof(apn_login->mnc));
	SAFE_STRNCPY(apn_login->score,get_single(db_prefix,AUTOAPN_DB_PROFILE_SCORE),sizeof(apn_login->score));
	
	return 0;
}

static int save_apn_to_db(const struct apn_login_struc* apn_login,const char* db_prefix)
{
	set_single(db_prefix,AUTOAPN_DB_PROFILE_APN,apn_login->apn);
	set_single(db_prefix,AUTOAPN_DB_PROFILE_USER,apn_login->user_name);
	set_single(db_prefix,AUTOAPN_DB_PROFILE_PASS,apn_login->password);
	set_single(db_prefix,AUTOAPN_DB_PROFILE_AUTH,apn_login->auth);
	set_single(db_prefix,AUTOAPN_DB_PROFILE_DIALSTR,apn_login->dial);
	
	// extra information
	set_single(db_prefix,AUTOAPN_DB_PROFILE_COUNTRY,apn_login->country);
	set_single(db_prefix,AUTOAPN_DB_PROFILE_MCC,apn_login->mcc);
	set_single(db_prefix,AUTOAPN_DB_PROFILE_CARRIER,apn_login->carrier);
	set_single(db_prefix,AUTOAPN_DB_PROFILE_MNC,apn_login->mnc);
	set_single(db_prefix,AUTOAPN_DB_PROFILE_SCORE,apn_login->score);
	
	return 0;
}
			
static int set_single(const char* prefix,const char* var,const char* val)
{
	char prefix_var[AUTOAPN_DB_VARIABLE_MAX_NAME];
	int stat;
	
	// get prefixed variable name
	snprintf(prefix_var,sizeof(prefix_var),"%s%s",prefix,var);
	
	stat=rdb_set_single(prefix_var,val);
	if((stat<0) && (errno==ENOENT))
		stat=rdb_create_variable(prefix_var, val, CREATE, DEFAULT_PERM, 0, 0);
	
	return stat;
}
		
static const char* get_single(const char* prefix,const char* var)
{
	static char val[AUTOAPN_DB_VALUE_MAX_BUFFER];
	char prefix_var[AUTOAPN_DB_VARIABLE_MAX_NAME];
	
	// get prefixed variable name
	snprintf(prefix_var,sizeof(prefix_var),"%s%s",prefix,var);
	prefix_var[sizeof(prefix_var)-1]=0;
		
	// completely ignore errors
	val[0]=0;
	if(rdb_get_single(prefix_var, val, sizeof(val))<0)
		val[0]=0;
	
	return val;
}


const char* get_post_stage_string(int stage)
{
	const char* msg[]={
		"postcheck_idle",
		"postcheck_sim",
		"postcheck_ccid",
		"postcheck_ccid_wait",
		"postcheck_mccmnc",
		"postcheck_mccmnc_wait",
		"postcheck_network_attach",
    		"postcheck_network_attach_wait",
		"postcheck_findelay_init",
		"postcheck_findelay_wait",
		"postcheck_done",
		"postcheck_error",
		"postcheck_unknown"
	};
	
	static char last_stage[AUTOAPN_DB_VALUE_MAX_BUFFER]={0,};
	static char combined_stage[AUTOAPN_DB_VALUE_MAX_BUFFER];
	const char* ret_str;
			
	if((stage<0) || (stage>postcheck_error))
		stage=postcheck_unknown;
	
	if(stage!=postcheck_error) {
		// store the last stage
		SAFE_STRNCPY(last_stage,msg[stage],sizeof(last_stage));
		
		update_apn_stage(msg[stage]);
		
		ret_str=msg[stage];
	} else {
		snprintf(combined_stage,sizeof(combined_stage),"%s,%s",last_stage,msg[stage]);
		combined_stage[sizeof(combined_stage)-1]=0;
		
		update_apn_stage(combined_stage);
		
		ret_str=combined_stage;
	}
	
	return ret_str;
}

int init_post_check(int wwan_no)
{
	clock_per_second=sysconf(_SC_CLK_TCK);
	
	sprintf(wwan_db_prefix,AUTOAPN_DB_WWAN_PREFIX "%d.", wwan_no);
	
	post_stage=postcheck_idle;
	
	return 0;
}

static int close_xml(void)
{
	if(fp_xml)
		fclose(fp_xml);
	fp_xml=0;
	
	return 0;
}

static int open_xml(void)
{
	// open xml file
	fp_xml=fopen(AUTOAPN_APNLIST_XML_FILE,"r");
	if(!fp_xml)
	{
		syslog(LOG_CRIT, "apnscan: panic - fail to open xml file (%s) / %s",AUTOAPN_APNLIST_XML_FILE, strerror(errno));
		return -1;
	}
			
			// load mxml
	tree=mxmlLoadFile(0, fp_xml, MXML_NO_CALLBACK);
	if(!tree)
	{
		syslog(LOG_CRIT, "apnscan: panic - mxmlLoadFile() failed / %s", strerror(errno));
		return -1;
	}
	
	return 0;
}

static int score_connection()
{
	FILE* fp;
	
	char line[256];
	char* score_line;
	int score=0;
	
	fp=popen(AUTOAPN_SCORING_SCRIPT,"r");
	
	while(fgets(line,sizeof(line),fp))
	{
		score_line=strstr(line,"score : ");
		if(score_line)
			score=atoi(score_line+sizeof("score : ")-1);
	}
		
	pclose(fp);
	
	return score;
}

static const char* get_apnscan_stage_string(int stage)
{
	const char* msg[]={
		"apnscan_idle",
  		"apnscan_wait_until_quiet",
		"apnscan_check_prev_ccid",
		"apnscan_open_xml",
		"apnscan_dryrun",
		"apnscan_wetrun",
		"apnscan_wetrun",
		"apnscan_search_mcc_first",
		"apnscan_search_mcc_next",
		"apnscan_search_mnc_first",
		"apnscan_search_apn_first",
		"apnscan_search_apn_extract",
		"apnscan_start_connect",
		"apnscan_wait_until_up",
		"apnscan_score", 
		"apnscan_disconnect",
		"apnscan_disconnect_wait",
		"apnscan_search_apn_next",
		"apnscan_search_mnc_next",
		"apnscan_pick_best",
		"apnscan_connect_static",
		"apnscan_done",
		"apnscan_error",
		"apnscan_unknown"
	};
			
	static char last_stage[AUTOAPN_DB_VALUE_MAX_BUFFER]={0,};
	static char combined_stage[AUTOAPN_DB_VALUE_MAX_BUFFER];
	
	const char* ret_str;
	
	if((stage<0) || (stage>apnscan_unknown))
		stage=postcheck_unknown;
		
	if(stage!=apnscan_error) {
		// store the last stage
		SAFE_STRNCPY(last_stage,msg[stage],sizeof(last_stage));
		
		update_apn_stage(msg[stage]);
		
		ret_str=msg[stage];
	}
	else {
		snprintf(combined_stage,sizeof(combined_stage),"%s,%s",last_stage,msg[stage]);
		combined_stage[sizeof(combined_stage)-1]=0;
		
		update_apn_stage(combined_stage);
		
		ret_str=combined_stage;
	}
		
	return ret_str;
}

static int mnc_exists(const char* mnc_str_list,int mnc)
{
	char list_buff[128];
	const char* mnc_item;
	
	SAFE_STRNCPY(list_buff,mnc_str_list,sizeof(list_buff));
	
	mnc_item=strtok(list_buff,",");
	
	while(mnc_item)
	{
		if(atoi(mnc_item)==mnc)
			return 1;
		
		mnc_item=strtok(0,",");
	}
	
	return 0;
}


static void init_conn_term(void)
{
	conn_term=0;	
}

void notify_conn_term(void)
{
	conn_term=1;
}

static int is_conn_term(void)
{
	return conn_term;
}

void init_scan_apn(int profile_no,const char* country)
{
	const char* ccid;
	
	autoapn_country[0]=0;
	autoapn_mcc[0]=0;
	autoapn_mnc[0]=0;
	autoapn_ccid[0]=0;
	autoapn_hint[0]=0;
	
	sprintf(profile_db_prefix,AUTOAPN_DB_PROFILE_PREFIX "%d.", profile_no);
		
	if(country)
		SAFE_STRNCPY(autoapn_country,country,sizeof(autoapn_country));
	
	SAFE_STRNCPY(autoapn_mcc,get_single(wwan_db_prefix,"imsi.plmn_mcc"),sizeof(autoapn_mcc));
	SAFE_STRNCPY(autoapn_mnc,get_single(wwan_db_prefix,"imsi.plmn_mnc"),sizeof(autoapn_mnc));
	
	// copy ccid
	ccid=get_single(wwan_db_prefix,"system_network_status.simICCID");
	if(!is_valid_ccid(ccid))
		autoapn_ccid[0]=0;
	else
		SAFE_STRNCPY(autoapn_ccid,ccid,sizeof(autoapn_ccid));
	
	// copy hint
	SAFE_STRNCPY(autoapn_hint,get_single(wwan_db_prefix,"system_network_status.hint"),sizeof(autoapn_hint));
	
	apnscan_stage=apnscan_idle;
}


static void update_apn_stage(const char* stage)
{
	set_single("",AUTOAPN_DB_STATIC,stage);
}

static int update_apn_progress(int scan_stage,int stage)
{
	/*
		## customer friendly but ugly progressive bar ##
	
		progress 0~10%    : apnscan_idle ~ apnscan_wetrun
		progress 10%~90%  : apn index ( apnscan_search_mcc_first ~ 
		progress 100%     : apnscan_done - detected / 
	*/
	
	
	int progress_first=-1;
	int progress_second=-1;
	int progress=-1;
	char str[16];
	
	
	// the first of progression
	if(!scan_stage) {
		progress_first=stage*20/(postcheck_done-postcheck_idle);
	// the scan of progression
	} else {
		// progress_second - 0% (base 20%)
		if(stage<=apnscan_wait_until_quiet) {
			progress_second=0;
		}
		// progress_second - 0~10% (base 20%)
		else if(stage<=apnscan_wetrun) {
			progress_second=(stage-apnscan_idle)*10/(apnscan_wetrun-apnscan_idle);
		}
		// progress_second - 10~60% (base 20%)
		else if (stage<apnscan_connect_static) {
			progress_second=10;
			if((running_mode!=running_mode_dry) && (total_apn_cnt+total_hint_apn_cnt>0)) {
				progress_second+=( (current_apn_idx+hint_apn_login_idx)*50)/(total_apn_cnt+total_hint_apn_cnt);
				syslog(LOG_INFO, "apnscan: apn info - idx=%d,hint_idx=%d,cnt=%d",current_apn_idx,hint_apn_login_idx,total_apn_cnt);
			}
		}
		// progress_second - 70% (base 20%)
		else if (stage<apnscan_done) {
			progress_second=70;
		}
		// progress_second - 80% (base 20%)
		else if (stage==apnscan_done) {
			progress_second=80;
		}
		
		if(running_mode!=running_mode_dry)
			syslog(LOG_INFO, "apnscan: progress_second - stage=%d, progress_second=%d",stage,progress_second);
		
	}
	
	// assume the first stage is donw if in the second stage 
	if(progress_second>=0)
		progress_first=20;
	
	if(progress_first>=0) {
		// progress - 20:80
		progress=progress_first+progress_second;
		
		// convert progress_second to string
		snprintf(str,sizeof(str),"%d",progress);
		str[sizeof(str)-1]=0;
			
		// write progress into database
		set_single("",AUTOAPN_DB_PROGRESS,str);
		
		// convert retry to string
		snprintf(str,sizeof(str),"%d",autoscan_retry_count);
		str[sizeof(str)-1]=0;
	
		// write retry count into database
		set_single("",AUTOAPN_DB_RETRY,str);
	}
	
	
	return progress;
}

int do_scan_apn(void)
{
	const char* ccid;
	const char* stage_str;
	
	static const char* country_country=0;
	static const char* country_mcc=0;
	static const char* carrier_mnc=0;
	static const char* carrier_carrier=0;
	
	mxml_node_t* node_current;
	int descend;
	
	static mxml_node_t* node_country=0;
	static mxml_node_t* node_carrier=0;
	static mxml_node_t* node_apn=0;
	
	int yield;
	
	int mcc=atoi(autoapn_mcc);
	int mnc=atoi(autoapn_mnc);
	
	const char* simcard_country=autoapn_country;
	const char* simcard_ccid=autoapn_ccid;
	const char* simcard_hint=autoapn_hint;
				
	static clock_t stop_start_clock=0;
	static clock_t init_delay_start_clock=0;
	static clock_t connect_start_clock=0;
	
	clock_t now;
	
	struct tms tmsbuf;
	
	now=times(&tmsbuf)/clock_per_second;
	
	static int scoring_count;
	
do_apnscan_stage_loop:
				
	yield=0;
	
	switch(apnscan_stage)
	{
		case apnscan_idle:
			autoscan_retry_count=0;
			best_apn_score=0;
			
			init_delay_start_clock=now;
			yield=1;
						
			apnscan_stage=apnscan_wait_until_quiet;
			
			syslog(LOG_INFO, "apnscan: starting... mcc=%d,mnc=%d",mcc,mnc);
			break;
			
		case apnscan_wait_until_quiet:
			if(now-init_delay_start_clock<AUTOAPN_CONNECTION_STOP_DELAY) {
				yield=1;
				break;
			}
			
			apnscan_stage=apnscan_check_prev_ccid;
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
			
		case apnscan_check_prev_ccid:
			ccid=get_single(autoapn_static_prefix,"ccid");
			
			if(ccid[0] && !strcasecmp(ccid,simcard_ccid))
			{
				syslog(LOG_INFO, "apnscan: sim card matched - %s",ccid);
				
				apnscan_stage=apnscan_connect_static;
			}
			else
			{
				syslog(LOG_INFO, "apnscan: new sim card detected - new:%s,old:%s",simcard_ccid,ccid);
				
				apnscan_stage=apnscan_open_xml;
			}
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
			
		case apnscan_open_xml:
			// try later if failed
			if(open_xml()<0)
				apnscan_stage=apnscan_error;
			else
				apnscan_stage=apnscan_dryrun;
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			
			break;
			
		case apnscan_dryrun:
			running_mode=running_mode_dry;
			
			hint_apn_login_idx=0;
			
			apnscan_stage=apnscan_search_mcc_first;
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
			
		case apnscan_wetrun:
			running_mode=running_mode_hint_only;
			
			// store total count of apns
			total_apn_cnt=current_apn_idx;
			total_hint_apn_cnt=hint_apn_login_idx;
			
			syslog(LOG_INFO, "apnscan: total apn count = %d",total_apn_cnt);
			syslog(LOG_INFO, "apnscan: total hint apn count = %d",total_hint_apn_cnt);
			
			// reset for the first time
			current_apn_idx=0;
			hint_apn_login_idx=0;
			
			if(!total_hint_apn_cnt && !total_apn_cnt) {
				syslog(LOG_ERR, "apnscan: no apn found mcc=%d,mnc=%d",mcc,mnc);
				apnscan_stage=apnscan_error;
			}
			else {
				apnscan_stage=apnscan_hint_next;
			}
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
			
		case apnscan_hint_next:
			
			// error if no apn found
			if( (total_apn_cnt==0) && (total_hint_apn_cnt==0) ) {
				apnscan_stage=apnscan_error;
				syslog(LOG_ERR, "apnscan: no apn found mcc=%d,mnc=%d",mcc,mnc);
				break;
			}
			
			// reset idx
			if(running_mode==running_mode_hint_only) {
				// back to normal if done with hint apns
				if( (hint_apn_login_idx>=total_hint_apn_cnt) && (total_apn_cnt>0) ) {
					hint_apn_login_idx=0;
					running_mode=running_mode_normal;
					apnscan_stage=apnscan_search_mcc_first;
				}
				// do hint only
				else {
					if(hint_apn_login_idx>=total_hint_apn_cnt) {
						hint_apn_login_idx=0;
						autoscan_retry_count++;
					}
						
					memcpy(&current_apn_login,&hint_apn_logins[hint_apn_login_idx],sizeof(current_apn_login));
					hint_apn_login_idx++;
					apnscan_stage=apnscan_start_connect;
				}
			}
			else {
				if(total_hint_apn_cnt==0)
					running_mode=running_mode_normal;
				if(total_apn_cnt==0)
					running_mode=running_mode_hint;
					
				// do hint if hint mode
				if(running_mode==running_mode_hint) {
					if(hint_apn_login_idx>=total_hint_apn_cnt)
						hint_apn_login_idx=0;
					
					memcpy(&current_apn_login,&hint_apn_logins[hint_apn_login_idx],sizeof(current_apn_login));
					hint_apn_login_idx++;
						
					apnscan_stage=apnscan_start_connect;
					
					running_mode=running_mode_normal;
				}
				// if normal mode
				else {
					apnscan_stage=apnscan_search_apn_next;
					running_mode=running_mode_hint;
				}
			}
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
				
			
		case apnscan_pick_best:
			if(best_apn_score>0)
			{
				// save auto apn to database
				save_apn_to_db(&best_apn_login,autoapn_static_prefix);
				// save simcard ccid to database
				set_single(autoapn_static_prefix,"ccid",simcard_ccid);
				// save to nvram
				save_to_nvram();
				
				
				syslog(LOG_INFO, "apnscan: best apn - %s",best_apn_login.apn);
				
				apnscan_stage=apnscan_connect_static;
			}
			else
			{
				apnscan_stage=apnscan_search_mcc_first;
				
				autoscan_retry_count++;
			}
				
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
			
			
		case apnscan_search_mcc_first:
		case apnscan_search_mcc_next:
			if(running_mode!=running_mode_dry)
				syslog(LOG_INFO, "apnscan: search mcc - %s",autoapn_mcc);

			if(apnscan_stage==apnscan_search_mcc_first)
			{
				// reset for wrapping
				current_apn_idx=0;
				
				node_current=tree;
				descend=MXML_DESCEND;
			}
			else
			{
				node_current=node_country;
				descend=MXML_NO_DESCEND;
			}
			
			// get country from root
			node_country=mxmlFindElement(node_current,tree,"Country",0,0,descend);
			if(!node_country)
			{
				if(apnscan_stage==apnscan_search_mcc_first) {
					syslog(LOG_INFO, "apnscan: no carrier found - mnc=%d, mcc=%d",mnc,mcc);
					apnscan_stage=apnscan_error;
				}
				else {
					if(running_mode==running_mode_dry)
						apnscan_stage=apnscan_wetrun;
					else
						apnscan_stage=apnscan_pick_best;
				}
			}
			else
			{
				country_country=mxmlElementGetAttr(node_country,"country");
				country_mcc=mxmlElementGetAttr(node_country,"mcc");
				
				if(running_mode!=running_mode_dry)
					syslog(LOG_INFO, "apnscan: country = %s, mcc = %s from xml",country_country,country_mcc);
				
				// no country specified
				if(!mcc && !simcard_country[0])
					apnscan_stage=apnscan_search_mnc_first;
				// mcc specified
				else if (mcc && (atoi(country_mcc)==mcc))
					apnscan_stage=apnscan_search_mnc_first;
				// country string specified
				else if (simcard_country[0] && !strcasecmp(simcard_country,country_country))
					apnscan_stage=apnscan_search_mnc_first;
				else
					apnscan_stage=apnscan_search_mcc_next;
			}
			
			if(running_mode!=running_mode_dry) {
				stage_str=get_apnscan_stage_string(apnscan_stage);
				syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			}
			break;
			
		case apnscan_search_mnc_first:
		case apnscan_search_mnc_next:
			if(running_mode!=running_mode_dry)
				syslog(LOG_INFO, "apnscan: search mnc - %d",mnc);
			
			if(apnscan_stage==apnscan_search_mnc_first)
			{
				node_current=node_country;
				descend=MXML_DESCEND_FIRST;
			}
			else
			{
				node_current=node_carrier;
				descend=MXML_NO_DESCEND;
			}
			
			node_carrier=mxmlFindElement(node_current,tree,"Carrier",0,0,descend);
			if(!node_carrier)
			{
				apnscan_stage=apnscan_search_mcc_next;
			}
			else
			{
				// carrier
				carrier_carrier=mxmlElementGetAttr(node_carrier,"carrier");
				// get mnc attribute
				carrier_mnc=mxmlElementGetAttr(node_carrier,"mnc");
				
				if(running_mode!=running_mode_dry)
					syslog(LOG_INFO, "apnscan: carrier=%s mnc=%s from xml",carrier_carrier,carrier_mnc);
				
				if(!mnc || mnc_exists(carrier_mnc,mnc))
					apnscan_stage=apnscan_search_apn_first;
				else
					apnscan_stage=apnscan_search_mnc_next;
			}
			
			if(running_mode!=running_mode_dry) {
				stage_str=get_apnscan_stage_string(apnscan_stage);
				syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			}
			break;
			
		case apnscan_search_apn_first:
		case apnscan_search_apn_next:			
			if(apnscan_stage==apnscan_search_apn_first)
			{
				node_current=node_carrier;
				descend=MXML_DESCEND_FIRST;
			}
			else
			{
				node_current=node_apn;
				descend=MXML_NO_DESCEND;
			}
			
			node_apn=mxmlFindElement(node_current,tree,"APN",0,0,descend);
			if(!node_apn) {
				apnscan_stage=apnscan_search_mnc_next;
			}
			else {
				apnscan_stage=apnscan_search_apn_extract;
			}
			
			if(running_mode!=running_mode_dry) {
				stage_str=get_apnscan_stage_string(apnscan_stage);
				syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			}
			break;
			
		case apnscan_search_apn_extract:
		{
			const char* apn;
			const char* login_var;
			const char* login_val;
			
			mxml_node_t* node_apn_login;
			mxml_node_t* node_apn_login_child;
			
			int hint_matched;
			
			// extract login detail
			apn=mxmlElementGetAttr(node_apn,"apn");
			
			if(!apn) {
				apnscan_stage=apnscan_search_apn_next;
			}
			else
			{
				syslog(LOG_INFO, "apnscan: try apn - %s",apn);
				
				memset(&current_apn_login,0,sizeof(current_apn_login));
				SAFE_STRNCPY(current_apn_login.apn,apn,sizeof(current_apn_login.apn));
				
				// extract extra information
				SAFE_STRNCPY(current_apn_login.country,country_country,sizeof(current_apn_login.country));
				SAFE_STRNCPY(current_apn_login.mcc,country_mcc,sizeof(current_apn_login.mcc));
				SAFE_STRNCPY(current_apn_login.carrier,carrier_carrier,sizeof(current_apn_login.carrier));
				SAFE_STRNCPY(current_apn_login.mnc,carrier_mnc,sizeof(current_apn_login.mnc));
				
				// extract login detail
				node_apn_login=node_apn->child;
				while(node_apn_login)
				{
					node_apn_login_child=node_apn_login->child;
					
					if(node_apn_login_child && node_apn_login_child->type==MXML_TEXT)
					{
						login_var=(node_apn_login->value).element.name;
						login_val=(node_apn_login_child->value).text.string;
							
						if(!strcmp(login_var,"UserName"))
							SAFE_STRNCPY(current_apn_login.user_name,login_val,sizeof(current_apn_login.user_name));
						else if(!strcmp(login_var,"Password"))
							SAFE_STRNCPY(current_apn_login.password,login_val,sizeof(current_apn_login.password));
						else if(!strcmp(login_var,"Auth"))
							SAFE_STRNCPY(current_apn_login.auth,login_val,sizeof(current_apn_login.auth));
						else if(!strcmp(login_var,"Dial"))
							SAFE_STRNCPY(current_apn_login.dial,login_val,sizeof(current_apn_login.dial));
					}
					
					node_apn_login=node_apn_login->next;
				}
				
				hint_matched=simcard_hint[0] && strcasestr(current_apn_login.carrier,simcard_hint);
				
				// collecting hint carrier
				if(running_mode==running_mode_dry) {
					if(hint_matched) {
						if(hint_apn_login_idx<MAX_HINT_APN_LOGS) {
							memcpy(&hint_apn_logins[hint_apn_login_idx],&current_apn_login,sizeof(current_apn_login));
							hint_apn_login_idx++;
							
							syslog(LOG_INFO, "hint apn found - carrier=%s, apn=%s",current_apn_login.carrier,current_apn_login.apn);
						}
						else {
							syslog(LOG_CRIT, "hint apn overflow - %d",MAX_HINT_APN_LOGS);
						}
					}
					else {
						current_apn_idx++;
					}
					
					apnscan_stage=apnscan_search_apn_next;
				}
				else {
					if(hint_matched) {
						syslog(LOG_INFO, "skip apn apn - carrier=%s, apn=%s",current_apn_login.carrier,current_apn_login.apn);
						apnscan_stage=apnscan_search_apn_next;
					}
					else {
						current_apn_idx++;
						apnscan_stage=apnscan_start_connect;
					}
				}
						
				
				scoring_count=0;
			}
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
		}
			
		case apnscan_start_connect:
		{
			scoring_count++;
			
			// save apn to profile database
			save_apn_to_db(&current_apn_login,profile_db_prefix);
			
			// launch connection script
			init_conn_term();
			
			syslog(LOG_INFO, "starting PDP connection #%d",scoring_count);
			start_wwan();
			
			connect_start_clock=now;
					
			apnscan_stage=apnscan_wait_until_up;
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
		}
			
		case apnscan_wait_until_up:
			if(is_conn_term())
			{
				if(scoring_count<AUTOAPN_MAX_SCORING)
					apnscan_stage=apnscan_start_connect;
				else
					apnscan_stage=apnscan_hint_next;
				
				syslog(LOG_INFO, "apnscan: connection script terminated");
			}
			else if(!is_wwan_up())
			{
				if(now-connect_start_clock<AUTOAPN_CONNECTION_MAX_DELAY) {
					yield=1;
					break;
				}
				else {
					apnscan_stage=apnscan_disconnect;
					syslog(LOG_INFO, "apnscan: connection timed out - %d sec",AUTOAPN_CONNECTION_MAX_DELAY);
				}
			}
			else
			{
				current_apn_score=0;
				apnscan_stage=apnscan_score;
			}
				
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
			
		case apnscan_score:
		{
			/*
				** apn score **
			
				PDP session is up - 1 point
				WGET returns unknown page - 2 points
				DNS lookup of WGET successes but fails to ge the page - 3 points
				WGET returns the correct page - 9 points
			
				if score becomes 10 or higher, we take the apn as a static
			*/
			
			current_apn_score=1;	// PDP session is up
			current_apn_score+=score_connection();
			
			syslog(LOG_INFO, "apnscan: apn(%s) scored - %d",current_apn_login.apn,current_apn_score);
			
			snprintf(current_apn_login.score,sizeof(current_apn_login.score),"%d",current_apn_score);
			
			// save apn to profile database
			save_apn_to_db(&current_apn_login,profile_db_prefix);
			
			// if connection dropped during scoring
			if(is_conn_term())
			{
				syslog(LOG_INFO, "apnscan: diconnected during scoring - %s", current_apn_login.apn);
				apnscan_stage=apnscan_disconnect_wait;
			}
			// if we get the jackpot
			else if(current_apn_score>=10)
			{
				// save auto apn to database
				save_apn_to_db(&current_apn_login,autoapn_static_prefix);
				// save simcard ccid to database
				set_single(autoapn_static_prefix,"ccid",simcard_ccid);
				
				// save to nvram
				save_to_nvram();
				
				apnscan_stage=apnscan_done;
			}
			else
			{
				// update best apn if better
				if(current_apn_score>best_apn_score)
				{
					memcpy(&best_apn_login,&current_apn_login,sizeof(best_apn_login));
					best_apn_score=current_apn_score;
				}
					
				apnscan_stage=apnscan_disconnect;
				
				yield=1;
			}
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			
			break;
		}
		
		case apnscan_disconnect:
			stop_wwan();
			
			stop_start_clock=now;
			apnscan_stage=apnscan_disconnect_wait;
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
			
		case apnscan_disconnect_wait:
			if(!is_conn_term())
			{
				if(now-stop_start_clock>=AUTOAPN_CONNECTION_STOP_DELAY)
				{
					stop_wwan();
					stop_start_clock=now;
				}
				
				yield=1;
				break;
			}
			
			if(scoring_count<AUTOAPN_MAX_SCORING)
				apnscan_stage=apnscan_start_connect;
			else
				apnscan_stage=apnscan_hint_next;
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			break;
		
			
		case apnscan_connect_static:
		{
			load_apn_from_db(autoapn_static_prefix,&current_apn_login);
			save_apn_to_db(&current_apn_login,profile_db_prefix);
			
			start_wwan();
			
			// launch script
			apnscan_stage=apnscan_done;
			
			stage_str=get_apnscan_stage_string(apnscan_stage);
			syslog(LOG_INFO, "apnscan: move to the stage of %s(%d)",stage_str,apnscan_stage);
			
			break;
		}
			
		case apnscan_done:
			close_xml();
			break;
			
		case apnscan_error:
			close_xml();
			break;
			
		default:
			syslog(LOG_CRIT, "apnscan: panic - unknown stage (%d)",apnscan_stage);
			
			apnscan_stage=apnscan_error;
			break;
	}

	// update progress
	update_apn_progress(1,apnscan_stage);
	
	if(apnscan_stage==apnscan_done)
		return 1;
	else if (apnscan_stage==apnscan_error)
		return -1;
	
	if(!yield)
		goto do_apnscan_stage_loop;
	
	
	return 0;
}

static int is_valid_ccid(const char* ccid)
{
	if(!ccid)
		return 0;
	
	if(!ccid[0])
		return 0;
	
	if(strcasestr(ccid,"error"))
		return 0;
	
	return 1;
}

int do_post_check(void)
{
	int mcc;
	int mnc;
	const char* network_attached;
	const char* sim_card_status;
	char ccid[64];
	char hint[64];
	
	const char* stage_str;
	
	static clock_t network_attach_start_clock=0;
	static clock_t ccid_check_start_clock=0;
	static clock_t mccmnc_check_start_clock=0;
	static clock_t findelay_start_clock=0;
	
	clock_t now;
	struct tms tmsbuf;
	
	int yield;
	
	now=times(&tmsbuf)/clock_per_second;
	
do_post_stage_loop:
			
	yield=0;
			
	switch(post_stage)
	{
		case postcheck_idle:
			post_stage=postcheck_sim;
			stage_str=get_post_stage_string(post_stage);
			syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
			break;
		
		case postcheck_sim:
			// read SIM card status
			sim_card_status=get_single(wwan_db_prefix,"sim.status.status");
			syslog(LOG_INFO, "poststage: sim status = %s",sim_card_status);
			// bypass if SIM card is not okay
			if(!strcmp(sim_card_status,"SIM OK"))
			{
				post_stage=postcheck_mccmnc;
				stage_str=get_post_stage_string(post_stage);
				syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
			}
			else
			{
				yield=1;
			}
			break;
			
		case postcheck_mccmnc:
			// read mcc
			mcc=atoi(get_single(wwan_db_prefix,"imsi.plmn_mcc"));
			// read mnc
			mnc=atoi(get_single(wwan_db_prefix,"imsi.plmn_mnc"));
			// bypass if no mcc or no mnc
			if(mcc && mnc)
			{
				syslog(LOG_INFO, "poststage: sim card mcc=%d,mnc=%d",mcc,mnc);
				
				post_stage=postcheck_network_attach;
			}
			else
			{
				mccmnc_check_start_clock=now;
				
				post_stage=postcheck_mccmnc_wait;
			}
			
			stage_str=get_post_stage_string(post_stage);
			syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
			
			break;
			
		case postcheck_mccmnc_wait:
		{
			// read mcc
			mcc=atoi(get_single(wwan_db_prefix,"imsi.plmn_mcc"));
			// read mnc
			mnc=atoi(get_single(wwan_db_prefix,"imsi.plmn_mnc"));
			
			if(!mcc || !mnc)
			{
				if(now-mccmnc_check_start_clock>=AUTOAPN_DELAY_FOR_MCCMNC)
				{
					syslog(LOG_INFO, "poststage: timeout(%ds) expired - mcc mnc not supported",AUTOAPN_DELAY_FOR_MCCMNC);
					post_stage=postcheck_network_attach;
				}
				else
				{
					yield=1;
					break;
				}
			}
			else
			{
				post_stage=postcheck_mccmnc;
			}
			
			stage_str=get_post_stage_string(post_stage);
			syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
		}
			
			
		case postcheck_network_attach:
		{
			// read network attachment status
			network_attached=get_single(wwan_db_prefix,"system_network_status.attached");
			
			// if network attach status does not exist - we apply delay for the dongle that do not support this status 
			if(!network_attached[0]) 
			{
				network_attach_start_clock=now;
				post_stage=postcheck_network_attach_wait;
			}
			else
			{
				// skip postcheck_network_attach_wait if attached
				if(atoi(network_attached))
				{			
					post_stage=postcheck_ccid;
				}
				else
				{
					yield=1;
					break;
				}
			}
			
			stage_str=get_post_stage_string(post_stage);
			syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
			break;
		}
	
		case postcheck_network_attach_wait:
			// read network attachment status
			network_attached=get_single(wwan_db_prefix,"system_network_status.attached");
			
			// go to the next step if delay is expired
			if(!network_attached[0])
			{
				if(now-network_attach_start_clock>=AUTOAPN_DELAY_FOR_ATTACH)
				{
					syslog(LOG_INFO, "poststage: timeout(%ds) expired - network attach status not supported",AUTOAPN_DELAY_FOR_ATTACH);
					post_stage=postcheck_ccid;
				}
				else
				{
					yield=1;
					break;
				}
			}
			else
			{
				post_stage=postcheck_network_attach;
			}
			
			stage_str=get_post_stage_string(post_stage);
			syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
			
			break;
			
		case postcheck_ccid:
			SAFE_STRNCPY(ccid,get_single(wwan_db_prefix,"system_network_status.simICCID"),sizeof(ccid));
			SAFE_STRNCPY(hint,get_single(wwan_db_prefix,"system_network_status.hint"),sizeof(hint));
			
			if(!is_valid_ccid(ccid) && !hint[0])
			{
				ccid_check_start_clock=now;
				post_stage=postcheck_ccid_wait;
			}
			else
			{
				syslog(LOG_INFO, "poststage: ccid=%s,hint=%s",ccid,hint);
				
				post_stage=postcheck_findelay_init;
			}
			
			stage_str=get_post_stage_string(post_stage);
			syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
			
			break;
			
		case postcheck_ccid_wait:
			SAFE_STRNCPY(ccid,get_single(wwan_db_prefix,"system_network_status.simICCID"),sizeof(ccid));
			SAFE_STRNCPY(hint,get_single(wwan_db_prefix,"system_network_status.hint"),sizeof(hint));
			
			if(!is_valid_ccid(ccid) && !hint[0] && (now-ccid_check_start_clock<AUTOAPN_DELAY_FOR_CCID))
			{
				yield=1;
				break;
			}
			else
			{
				post_stage=postcheck_findelay_init;
				
				syslog(LOG_INFO, "poststage: timeout(%ds) expired - ccid or hint not supported",AUTOAPN_DELAY_FOR_CCID);
			}
			
			stage_str=get_post_stage_string(post_stage);
			syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
			
			break;
			
		case postcheck_findelay_init:
			findelay_start_clock=now;
			
			post_stage=postcheck_findelay_wait;
			
			stage_str=get_post_stage_string(post_stage);
			syslog(LOG_INFO, "poststage: move to the stage of %s",stage_str);
			break;
		
		case postcheck_findelay_wait:
			if(now-findelay_start_clock<AUTOAPN_FIN_DELAY)
			{
				yield=1;
				break;
			}
			
			post_stage=postcheck_done;
			
			break;
			
		case postcheck_done:
			break;
			
		case postcheck_error:
			break;
		
		default:
			syslog(LOG_CRIT, "poststage: panic - unknown stage for post check is detected (%d)",post_stage);
			
			post_stage=postcheck_error;
			break;
	}
	
	
	// update progress
	update_apn_progress(0,post_stage);
	
	if(post_stage==postcheck_done)
		return 1;
	else if(post_stage==postcheck_error)
		return -1;
	
	if(!yield)
		goto do_post_stage_loop;
	
	return 0;
}

static int dummy_callback_func1(void)
{
	return 0;	
}

static void dummy_callback_func2(void)
{
}

int init_scan_apn_callbacks(void (*user_start_wwan)(void),void (*user_stop_wwan)(void),int (*user_is_wwan_up)(void),void (*user_save_to_nvram)(void))
{
	if(user_start_wwan)
		start_wwan=user_start_wwan;
	
	if(user_stop_wwan)
		stop_wwan=user_stop_wwan;
	
	if(user_is_wwan_up)
		is_wwan_up=user_is_wwan_up;
	
	if(user_save_to_nvram)
		save_to_nvram=user_save_to_nvram;
	
	return 0;
}
