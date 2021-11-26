#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/times.h>
#include <stdarg.h>

//#include <mxml.h>
#include "rdb_ops.h"

#include "../rdb_names.h"
#include "./plmn_struc.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../util/scheduled.h"
#include "../util/rdb_util.h"
#include "netdevusage.h"
#include "../models/model_default.h"

#include "../featurehash.h"


//#define MODULE_TEST
//#define MODULE_TEST2

// use ATCOPS as Cinterion specific command is not very suitable now.
#define ENABLE_ATCOPS
// by request from Vodafone, we have to ignore 2g and 3g fields as these fields are not reliable in practice
#define IGNORE_PRL_ACT

#define PLMN_CSV_FILE		"/usr/local/cdcs/conf/plmn-%s.csv"
#define PLMN_CSV_FACTORY_FILE	"/etc/cdcs/conf/plmn-%s.csv.factory"
#define NETWORK_REG_TIMEOUT	20
#define TIMEOUT_CGACT_SET	30
#define TIMEOUT_ATT_TIMEOUT	30
#define ATT_RETRY		0

#define IMSI_RETRY_COUNT	7
#define IMSI_RETRY_SLEEP	5

//static mxml_node_t *tree=0;

struct rbtplmn_t* prl=0; // preferred roaming list - key:mcc,mnc (from the xml file)
struct llplmn_t* crl=0; // current network roaming list - key:rank (from AT+COPS=?)
struct llplmn_t* cprl=0; // current preferred roaming list - key:rank (current roaming list)

netdevusage* netusage=0;

struct plmn_t* current_plmn=0; // current plmn the router registers
static clock_t plmn_update_time=0;
static clock_t last_traffic_time=0;

#define RSSI_MONITOR_DRAFT
#ifdef RSSI_MONITOR_DRAFT
static clock_t last_rssi_inadequacy_time=0;
static clock_t last_rssi_traffic_time=0;

int rssi_inadequate_timeout=0;
const int rssi_adequacy_sec=15; // 15 seconds

netdevusage* netusage_rssi=0;
#endif

//This will activate IMSI registration monitor
//The monitor keep checking IMSI registration status
//and the status is "not registered" or "denied" for more than "manualroam.reg_monitor_timeout" seconds,
//"Network rescan" prodecure will be triggered.
#define REG_MONITOR_DRAFT
#ifdef REG_MONITOR_DRAFT
static clock_t last_imsi_unreg_time=0;

int reg_monitor_timeout_sec=15; //15 seconds
#define	RDB_REG_MONITOR_TIMEOUT	"manualroam.reg_monitor_timeout"
#endif

// 4g -> 0x04, 3g -> 0x02, 2g -> 0x01
// Least digit has more priority on searching order.
// So if perfer_act is set to 214, searchs 4G, 2G and 3G in order.
// And if perfer_act is set to 24, searchs 4G and 3G in order, 2G is not included.
int prefer_act=0x00;
int prefer_act_mask=0x00;
int best_network_mask=0x00;

#define MAX_NETWORKS	3

// enabled -> 1, disabled -> 0
int cost_effective_mode=1;

// powercycle schedule time
int powercycle_count=0;
int powercycle_scheduled=0;
static clock_t powercycle_scheduled_time=0;
int powercycle_schedule_delay=30;
#ifdef MODULE_TEST
int powercycle_schedule_max_delay=4*60;
#else
int powercycle_schedule_max_delay=20*60;
#endif
const int powercycle_schedule_delay_limit=31536000;
#ifdef FORCED_REGISTRATION
extern int forced_registration;
#endif
extern int custom_roaming_algorithm;
extern int automatic_operator_setting;  // 1 -> Automatic, 0 -> Manual
int term_manual_roaming=0;

const int rescan_retry_count=3;
const int algorithm_retry_count=3;
const int network_attach_retry_count=3;
const int pdp_attach_retry_count=5;
const int resend_csq_cmd=5;
#ifdef MODULE_TEST
const int retry_for_best_time=1*60; // 3 min. for testing
#else
const int retry_for_best_time=30*60; // 30 min.
#endif

const int traffic_idle_sec=15; // 15 seconds
const int pdp_domain_detach_period=3*60;


static char atcmd[128];
static char atresp[1024*4];
static int atok;

int rssi_user_threshold=-105;
int rscp_user_threshold=-105;
int rsrp_user_threshold=-120;

// 3GPP TS 27.007 AT+COPS
enum {cops_mode_reg=1,cops_mode_unreg=2};
// 3GPP TS 27.007 AT+CREG
enum {creg_stat_not_reg=0,creg_stat_reg_home=1,creg_stat_reg_searching=2,creg_stat_reg_denied=3,creg_stat_reg_roam=5};

// manual roaming simcard list
char* manual_roam_simcards[]={"20404","90128",0};

#define	RDB_LAST_MCC "manualroam.lastmcc"
#define	RDB_LAST_MNC "manualroam.lastmnc"
#define	RDB_LAST_ACT "manualroam.lastact"
#define RDB_LAST_COUNTRY "manualroam.lastcountry"
#define RDB_LAST_NW  "manualroam.lastnetwork"

#define	RDB_AUTH_APN "manualroam.auth.apn"
#define	RDB_AUTH_TYPE "manualroam.auth.type"
#define	RDB_AUTH_USER "manualroam.auth.user"
#define	RDB_AUTH_PW "manualroam.auth.pw"


#define	RDB_BEST_NETWORK_TIME		"manualroam.best_network_retry_time"
#define	RDB_PDP_VALIDATION_CHECK	"manualroam.pdp_validation_check"

// extern function
void update_heartBeat();
int update_sim_status(void);
int update_imei_from_ati();
#if defined (MODULE_cinterion)
int pxs8_update_phys_cops(int auto_if_not_configured);
#endif

// local functions
static int print_mccmnc_with_digit(char* buf,int buflen,int mccmnc,int mccmnc_digit);
static int act2genmask(int act);
static int scan_network(int count,int disp_retry,int disp_retry_total,int quiet);
static int rdb_setVal(const char* rdb,const char* val);
static int get_rssi();
static int get_rscp();
static int get_rsrp();
static int get_best_network_retry_time();
static const char* get_ui_network();
static void store_ui_info(struct plmn_t* plmn,int* rssi,clock_t* delay_start,int* delay_min);
int update_cinterion_firmware_revision();
int attach_imsi_registration(struct plmn_t* plmn,int unreg_first);
static int wait_for_ps_attach(int sec);
void suspend_connection_mgr();

#define SUSPEND_DURATION  120
void set_connection_mgr_suspend_timer(int delay);
void reset_connection_mgr_suspend_timer();

void init_noti_creg();
void init_noti_cgreg();

int wait_for_noti_creg(int sec);
int wait_for_noti_cgreg(int sec);

#if 0
static int send_atcmd_cfun(int stat);
#endif

/* roaming status rdb variables */
#define MANUALROAM_STAT_MSG		"manualroam.stat.msg"
#define MANUALROAM_STAT_NETWORK		"manualroam.stat.network"
#define MANUALROAM_STAT_RSSI		"manualroam.stat.rssi"
#define MANUALROAM_STAT_DELAY_MIN	"manualroam.stat.delay_min"
#define MANUALROAM_STAT_DELAY_START	"manualroam.stat.delay_start"

//Below rdb variable is implemented because in some case, it takes quite long time to apply operator setting in WEBUI.
//The messages set on below rdb variable is displayed directly in the message box when user click "APPLY" button.
#define MANUALROAM_SETTING_MSG		"manualroam.setting.msg"

/* roaming status */
static char ui_network[128]={0,};
static int ui_rssi=0;
static int ui_delay_min=0;
static clock_t ui_delay_start=0;

/* roaming status index */

enum {
	rs_commencing_reg=0,		/* Commencing registration... */
	rs_no_networks_avail=1,		/* No networks available, rescaning... */
	rs_no_networks_visible=2,	/* No networks visible, waiting x min to reboot */
	rs_conn_all_network_fail=3,	/* Connection to all networks failed. See registration log. Waiting to reboot in mm:ss mins. */
	rs_reg_provider_rssi_fail=4,	/* Registration to <provider> failed (Signal strength too low), trying next network... */
	rs_reg_provider_imsi_fail=5,	/* Registration to <provider> failed (IMSI attach failure), trying next network... */
	rs_pdp_fail=6,			/* Connection to APN failed (PDP context failure), trying next network... */
	rs_pdp_gprs_fail=7,		/* Registration to <provider> failed (GPRS attach failure), trying next network... */
	rs_start_best_network=8,	/* Starting "Best" network retry scan... */
	rs_done=99,			/* blank */
};

#define LINK_PROFILE_DELFLAG_USED 0		/* Value denoting the profile is in use */
#define LINK_PROFILE_MIN_DYNAMIC  7		/* Start of dynamically allocated profiles */

/*

sequence

<reboot_time>
<provider>

*/


static int rdb_set_printf(const char* rdb,const char* fmt,...)
{
	char rdb_val[1024];
	int stat;


	va_list ap;

	va_start(ap, fmt);

	vsnprintf(rdb_val,sizeof(rdb_val),fmt,ap);
	stat=rdb_setVal(rdb, rdb_val);

	va_end(ap);

	return stat;
}

static int set_manual_roam_msg_printf(int idx,const char* fmt,...)
{
	char rdb_val[1024];
	int stat;

	va_list ap;

	/* set rdb */
	va_start(ap, fmt);
	vsnprintf(rdb_val,sizeof(rdb_val),fmt,ap);
	stat=rdb_set_printf(MANUALROAM_STAT_MSG, "%d,%s",idx,rdb_val);
	va_end(ap);

	/* print to ui */
	syslog(LOG_ERR,"[roaming] [status] #%d %s",idx,rdb_val);

	return stat;
}

static int set_oper_setting_msg_printf(const char* fmt,...)
{
	char rdb_val[1024];
	int stat;

	va_list ap;

	/* set rdb */
	va_start(ap, fmt);
	vsnprintf(rdb_val,sizeof(rdb_val),fmt,ap);
	stat=rdb_set_printf(MANUALROAM_SETTING_MSG, "%s",rdb_val);
	va_end(ap);

	return stat;
}

static const char* plmn_rdb_get(const char* rdb)
{
	static char rdb_val[256];

	if(rdb_get_single(rdb, rdb_val, sizeof(rdb_val))!=0) {
		return "";
	}

	return rdb_val;
}

int save_plmn(struct plmn_t* plmn)
{
	char mcc[16]={0,};
	char mnc[16]={0,};
	char act[16]={0,};

	// save to memory
	if(!(current_plmn=rbtplmn_find(prl,plmn))) {
		syslog(LOG_ERR,"[roaming] saving - last plmn not found in the current PRL");
	}
	else {
		// keep the act
		current_plmn->act=plmn->act;
	}

	if(plmn->best) {
		syslog(LOG_ERR,"[roaming] record current network in persistant memory");

		// save to rdb
		print_mccmnc_with_digit(mcc,sizeof(mcc),plmn->mcc,plmn->mcc_digit);
		print_mccmnc_with_digit(mnc,sizeof(mnc),plmn->mnc,plmn->mnc_digit);
		snprintf(act,sizeof(act),"%d",plmn->act);
	}

	// store mcc code
	if( rdb_set_single(RDB_LAST_MCC, mcc)<0 ) {
		syslog(LOG_ERR,"failed to set last mcc(%s) - %s",RDB_LAST_MCC,strerror(errno));
		goto err;
	}

	// store mnc code
	if( rdb_set_single(RDB_LAST_MNC, mnc)<0 ) {
		syslog(LOG_ERR,"failed to set last mnc(%s) - %s",RDB_LAST_MNC,strerror(errno));
		goto err;
	}

	// store act
	if( rdb_set_single(RDB_LAST_ACT, act)<0 ) {
		syslog(LOG_ERR,"failed to set last act(%s) - %s",RDB_LAST_ACT,strerror(errno));
		goto err;
	}

	// store country - extra information by Vodafone request
	if( rdb_set_single(RDB_LAST_COUNTRY, plmn->country )<0 ) {
		syslog(LOG_ERR,"failed to set last country(%s) - %s",RDB_LAST_COUNTRY,strerror(errno));
		goto err;
	}

	// store network - extra information by Vodafone request
	if( rdb_set_single(RDB_LAST_NW, plmn->network )<0 ) {
		syslog(LOG_ERR,"failed to set last network(%s) - %s",RDB_LAST_NW,strerror(errno));
		goto err;
	}

	// syslog(LOG_ERR,"[roaming] mcc=%d,mnc=%d,act=%d,best=%d,network=%s,tier=%s",plmn->mcc,plmn->mnc,plmn->act,plmn->best,plmn->network,plmn->tier);

	return 0;
err:
	return -1;
}

int load_plmn(struct plmn_t* plmn)
{
	/*
		TODO:

		we use 16 bytes buffer otherwise we get SEG-FAULT.
		fix the rdb driver not to access the buffer behind the size limit
	*/

	char mcc[16];
	char mnc[16];
	char act[16];

	char conn_type[128];
	int max_act=2;


	// read mcc
	if( rdb_get_single(RDB_LAST_MCC, mcc, sizeof(mcc))<0 ) {
		syslog(LOG_ERR,"failed to read lastmcc(%s) - %s",RDB_LAST_MCC,strerror(errno));
		goto err;
	}

	// read mnc
	if( rdb_get_single(RDB_LAST_MNC, mnc, sizeof(mnc))<0 ) {
		syslog(LOG_ERR,"failed to read lastmnc(%s) - %s",RDB_LAST_MCC,strerror(errno));
		goto err;
	}

	// read act
	if( rdb_get_single(RDB_LAST_ACT, act, sizeof(act))<0 ) {
		syslog(LOG_ERR,"failed to read lastact(%s) - %s",RDB_LAST_ACT,strerror(errno));
		goto err;
	}

	memset(plmn,0,sizeof(*plmn));

	// set mcc
	plmn->mcc=atoi(mcc);
	plmn->mcc_digit=strlen(mcc);
	// set mnc
	plmn->mnc=atoi(mnc);
	plmn->mnc_digit=strlen(mnc);
	// set act
	plmn->act=atoi(act);


	/* Note: AT+COPS command on TS 127 007 standard
	 * <AcT>: integer type; access technology selected
	 * 0 GSM
	 * 1 GSM Compact
	 * 2 UTRAN
	 * 3 GSM w/EGPRS
	 * 4 UTRAN w/HSDPA
	 * 5 UTRAN w/HSUPA
	 * 6 UTRAN w/HSDPA and HSUPA
	 * 7 E-UTRAN
	 */
	conn_type[0]=0;
	rdb_get_single(rdb_name("conn_type", ""), conn_type, sizeof(conn_type));
	if(strcmp(conn_type,"3g")) { // in case of LTE
		max_act = 7;
	}

	// If Cost Effective Mode enabled, try higher Access Technology first.
	if ((cost_effective_mode == 1) && (plmn->act < max_act))
	{
		plmn->act=max_act;
		// store act
		snprintf(act,sizeof(act),"%d",plmn->act);
		if( rdb_set_single(RDB_LAST_ACT, act)<0 ) {
			syslog(LOG_ERR,"failed to set last act(%s) - %s",RDB_LAST_ACT,strerror(errno));
			goto err;
		}
	}

	strcpy(plmn->network,plmn_rdb_get(RDB_LAST_NW));
	strcpy(plmn->country,plmn_rdb_get(RDB_LAST_COUNTRY));

	// save to memory
	if(!(current_plmn=rbtplmn_find(prl,plmn))) {
		syslog(LOG_ERR,"[roaming] No record of last best network stored");
	}
	else {
		// keep the act
		current_plmn->act=plmn->act;
	}

	return 0;

err:
	return -1;
}

static int send_atcmd(int timeout)
{
	// send at cmd
	if( at_send_with_timeout(atcmd, atresp, "", &atok, timeout, 0)!=0 ) {
		syslog(LOG_ERR,"failed to send AT cmd - %s",atcmd);
		return -1;
	}

	// check reply
	if(!atok) {
		syslog(LOG_ERR,"AT cmd error response - %s",atcmd);
		return -1;
	}

	return 0;
}

static int strncpy_safe(char* dst,int dstlen,const char* src,int srclen)
{
	int n;
	int i;

	if(!dstlen) {
		 return -1;
	}

	*dst=0;

	if(!srclen) {
		return -1;
	}

	n=(dstlen-1<srclen)?(dstlen-1):srclen;

	i=0;
	while(*src && (i<n)) {
		*dst++=*src++;
		i++;
	}

	*dst=0;
	i++;

	return i;
}

static int trim_dq(char* dst,int dstlen,const char* src,int srclen)
{
	int dq;
	int n;
	int ch;
	int i;


	if(!dstlen || !srclen)
		return -1;

	n=(dstlen-1<srclen)?dstlen-1:srclen;

	dq=0;
	i=0;
	while(i++<n) {
		ch=*src++;

		if(!dq && (ch=='"')) {
			dq=1;
		}
		else if(dq && (ch=='"')) {
			break;
		}
		else if(dq) {
			*dst++=ch;
		}
	}

	*dst=0;
	i++;

	return 0;
}

char* strchr_comma(char* src)
{
	int dq;
	char ch;

	dq=0;

	while((ch=*src)!=0) {
		if(ch=='"') {
			dq=!dq;
		}
		else if(!dq && (ch==','))
			return src;

		src++;
	}

	return 0;
}

#ifdef ENABLE_ATCOPS

#if defined(MODULE_EC21)
// Defined in models/model_default.c, used by build_crl() only.
extern int quectel_list_all_cops(int *p_okay, char *response, int max_len);
extern void quectel_band_fix(void);
#endif // MODULE_EC21

// [flowchart] build crl - current network roaming list (from AT+COPS=?)
int build_crl()
{
	char* p;
	char* s;

	char token[128];

	char mcc[3+1];
	char mnc[3+1];
	int mccmnc_len;

	struct plmn_t plmn;

	// destroy previous crl
	if(crl)
		llplmn_destroy(crl);

	// create
	if((crl=llplmn_create())==0) {
		syslog(LOG_ERR,"failed to create crl");
		goto err;
	}

#ifdef MODULE_TEST2

	#warning the firmware will do not perform "AT+COPS" in DEBUG mode

	// 1,X,202,05,Greece,Vodafone,Vodafone networks,1,x,x,VFGR

	syslog(LOG_ERR,"[roaming] !!!!!!!!! the firmware does not perform \"AT+COPS\" in module mode !!!!!!!");
/*
	// current office network
	p="+COPS: (1,\"vodafone Greece\",\"voda GR\",\"20205\",2),(2,\"vodafone AU\",\"voda AU\",\"50503\",2),(1,\"vodafone AU\",\"voda AU\",\"50503\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",0),,(0,1,2,3,4),(0,1,2,90,91)";

	// with the first non-connectable best network greece - black list test
	p="+COPS: (1,\"vodafone Greece\",\"voda GR\",\"20205\",2),(2,\"vodafone AU\",\"voda AU\",\"50503\",2),(1,\"vodafone AU\",\"voda AU\",\"50503\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",0),,(0,1,2,3,4),(0,1,2,90,91)";
*/

	static int entry_count=0;

	switch(entry_count++) {
		case 0:
			// telstra lowest rank
			p="+COPS: (1,\"Telstra Mobile\",\"Telstra\",\"50501\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",0),,(0,1,2,3,4),(0,1,2,90,91)";
			syslog(LOG_ERR,"[roaming] telstra network appears");
			break;

		case 1:
			// optus - higher than telstra
			p="+COPS: (1,\"YES OPTUS\",\"Optus\",\"50502\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",0),,(0,1,2,3,4),(0,1,2,90,91)";
			syslog(LOG_ERR,"[roaming] optus network appears");
			break;

		case 2:
		default:
			// vodafone - best network
			p="+COPS: (1,\"vodafone Greece\",\"voda GR\",\"20205\",2),(2,\"vodafone AU\",\"voda AU\",\"50503\",2),(1,\"vodafone AU\",\"voda AU\",\"50503\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",0),,(0,1,2,3,4),(0,1,2,90,91)";
			syslog(LOG_ERR,"[roaming] vodafone network appears");
			break;
	}

	// current office network
	//p="+COPS: (1,\"vodafone Greece\",\"voda GR\",\"20205\",2),(2,\"vodafone AU\",\"voda AU\",\"50503\",2),(1,\"vodafone AU\",\"voda AU\",\"50503\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",0),,(0,1,2,3,4),(0,1,2,90,91)";
	p="+COPS: (1,\"France\",\"SFR\",\"20810\",2),(2,\"vodafone AU\",\"voda AU\",\"50503\",2),(1,\"vodafone AU\",\"voda AU\",\"50503\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",0),(1,\"YES OPTUS\",\"Optus\",\"50502\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50501\",0),,(0,1,2,3,4),(0,1,2,90,91)";

#else // MODULE_TEST2
#if defined(MODULE_EC21)
    {
        // Because the EC21 may take several (lengthy) calls to complete an "AT+COPS=?" request,
        // we use a function that handles the required retries and heartbeat management.
        int okay = 0;
        const int stat = quectel_list_all_cops(&okay, atresp, 0);
        if (stat || !okay) {
            goto err;
        }
    }
#else // MODULE_EC21

	// update heart beat
	update_heartBeat();

	// build at cmd
	snprintf(atcmd,sizeof(atcmd),"AT+COPS=?");
	// send at cmd
	if(send_atcmd(AT_COP_TIMEOUT_SCAN)<0) {
		goto err;
	}
#endif // MODULE_EC21
	p=atresp;
#endif // MODULE_TEST2

	// skip prefix
	if(!(p=strstr(p,"+COPS: "))) {
		syslog(LOG_ERR,"incorrect AT cmd resp prefix - %s",atresp);
		goto err;
	}
	p+=strlen("+COPS: ");

	/*
		+COPS: (2,"vodafone AU","voda AU","50503",2),(1,"vodafone AU","voda AU","50503",0),(1,"YES OPTUS","Optus","50502",0),(1,"YES OPTUS","Optus","50502",2),(1,"Telstra Mobile","Telstra","50501",2),(1,"Telstra Mobile","Telstra","50501",0),,(0,1,2,3,4),(0,1,2,90,91)

		(stat,"long name", "short name", "numeric", act)
	*/

	while( (p=strchr(p,'('))!=0 ) {

		p++;

		// reset plmn
		memset(&plmn,0,sizeof(plmn));

		#warning TODO: may need to ignore forbidden or unavailable networks based on status

		// get stat comma
		s=p;
		if((p=strchr_comma(p))==0)
			break;

		// get stat token
		strncpy_safe(token,sizeof(token),s,p++-s);
		plmn.stat=atoi(token);

		// get lname comma
		s=p;
		if((p=strchr_comma(p))==0)
			break;

		// get lname token
		trim_dq(plmn.lname,sizeof(plmn.lname),s,p++-s);

		// get sname comma
		s=p;
		if((p=strchr_comma(p))==0)
			break;

		// get sname token
		trim_dq(plmn.sname,sizeof(plmn.sname),s,p++-s);

		// get mccmnc comma
		s=p;
		if((p=strchr_comma(p))==0)
			break;

		// get mccmnc token
		trim_dq(token,sizeof(token),s,p++-s);
		mccmnc_len=strlen(token);

		if(mccmnc_len==5) {
			plmn.mcc_digit=3;
			plmn.mnc_digit=2;
		}
		else if (mccmnc_len==6) {
			plmn.mcc_digit=3;
			plmn.mnc_digit=3;
		}
		else {
			continue;
		}

		// get mcc and mnc
		strncpy_safe(mcc,sizeof(mcc),token,plmn.mcc_digit);
		strncpy_safe(mnc,sizeof(mnc),token+plmn.mcc_digit,plmn.mnc_digit);
		plmn.mcc=atoi(mcc);
		plmn.mnc=atoi(mnc);

		// get act comma
		s=p;
		if((p=strchr(p,')'))==0)
			break;

		// get act token
		strncpy_safe(token,sizeof(token),s,p++-s);
		plmn.act=atoi(token);

		syslog(LOG_ERR,"[roaming] scanning - mcc=%d,mnc=%d,act=%d",plmn.mcc,plmn.mnc,plmn.act);

		// add plmn
		llplmn_add(crl,&plmn,1);
	}

	if(!llplmn_get_first(crl))
		goto err;

	return 0;

err:
	return -1;
}
#else
// [flowchart] build crl - current network roaming list (from AT+COPS=?)
int build_crl()
{
	char* p;
	char* s;
	char* p2;

	char* p3;
	int len;

	char token[128];
	char linebuf[1024];

	char mcc[16];
	char mnc[16];
	int mccmnc_len;

	int prefix_len;

	struct plmn_t plmn;

	// destroy previous crl
	if(crl)
		llplmn_destroy(crl);

	// create
	if((crl=llplmn_create())==0) {
		syslog(LOG_ERR,"failed to create crl");
		goto err;
	}

#ifdef MODULE_TEST2

	#warning the firmware will do not perform "AT+COPS" in DEBUG mode

	// 1,X,202,05,Greece,Vodafone,Vodafone networks,1,x,x,VFGR

	syslog(LOG_ERR,"[roaming] !!!!!!!!! the firmware does not perform \"AT+COPS\" in module mode !!!!!!!");
	p2="AT^SNMON=\"INS\",2\"\n" "\n"
		"^SNMON: \"INS\",0,2,3,\"50F8\",\"0B71\",\"50502\",583,31,\"-21\"" "\n"
		"^SNMON: \"INS\",0,2,3,\"50F7\",\"0B71\",\"50502\",596,31,\"-36\"" "\n"
		"^SNMON: \"INS\",0,128,2,\"041BCD17\",\"7D05\",\"50502\",3038,21,\"-72\",115" "\n"
		"^SNMON: \"INS\",0,64,2,\"04D025C3\",\"011B\",\"50503\",4387,16,\"-82\",115" "\n"
		"^SNMON: \"INS\",0,64,2,\"04D025BD\",\"011B\",\"50503\",4363,16,\"-82\",115" "\n"
		"^SNMON: \"INS\",0,16,2,\"041B1738\",\"7D05\",\"50502\",10737,12,\"-89\",61" "\n"
		"^SNMON: \"INS\",0,16,2,\"041BEFB1\",\"7D05\",\"50502\",10688,11,\"-92\",61" "\n"
		"^SNMON: \"INS\",0,1" "\n"
		"^SNMON: \"INS\",0,4" "\n"
		"^SNMON: \"INS\",0,8" "\n"
		"^SNMON: \"INS\",0,32" "\n"
		"^SNMON: \"INS\",0,256" "\n";

#else
	// set repeat count to 1
	snprintf(atcmd,sizeof(atcmd),"AT^SNMON=\"INS/CFG\",1,\"repeat\",1");
	if(send_atcmd(AT_QUICK_COP_TIMEOUT)<0) {
		goto err;
	}

	// build at cmd
	snprintf(atcmd,sizeof(atcmd),"AT^SNMON=\"INS\",2,511");
	// send at cmd
	if(send_atcmd(AT_COP_TIMEOUT_SCAN)<0) {
		goto err;
	}

	p2=atresp;
#endif

	/*
		AT^SNMON="INS",2
		^SNMON: "INS",0,2,3,"50F8","0B71","50502",583,31,"-21"
		^SNMON: "INS",0,2,3,"50F7","0B71","50502",596,31,"-36"
		^SNMON: "INS",0,128,2,"041BCD17","7D05","50502",3038,21,"-72",115
		^SNMON: "INS",0,64,2,"04D025C3","011B","50503",4387,16,"-82",115
		^SNMON: "INS",0,64,2,"04D025BD","011B","50503",4363,16,"-82",115
		^SNMON: "INS",0,16,2,"041B1738","7D05","50502",10737,12,"-89",61
		^SNMON: "INS",0,16,2,"041BEFB1","7D05","50502",10688,11,"-92",61
		^SNMON: "INS",0,1
		^SNMON: "INS",0,4
		^SNMON: "INS",0,8
		^SNMON: "INS",0,32
		^SNMON: "INS",0,256


	*/


	#define INS_PREFIX "^SNMON: \"INS\","

	prefix_len=strlen(INS_PREFIX);

	while( (p2=strstr(p2,INS_PREFIX))!=0 ) {

		// search a new line
		s=p2;
		if( (p2=strchr(p2,'\n'))==0 )
			break;
		strncpy_safe(linebuf,sizeof(linebuf),s,p2++-s);

		// skip prefix
		p=linebuf;
		if( (p=strstr(p,INS_PREFIX))==0 )
			continue;
		p+=prefix_len;

		// reset plmn
		memset(&plmn,0,sizeof(plmn));

		#warning TODO: may need to ignore forbidden or unavailable networks based on status

		/*
			^SNMON: "INS",0,2,3,"50F8","0B71","50502",583,31,"-21"
			^SNMON: "INS", <mode>, <rb>, <rat>, <cid>, <lac>, <plmn>, <uarfcn>, <rscp>, <dbm>, <psc>
		*/

		// get mode
		s=p;
		if((p=strchr_comma(p))==0)
			continue;
		strncpy_safe(token,sizeof(token),s,p++-s);
		//plmn.mode=atoi(token);

		// get rb
		s=p;
		if((p=strchr_comma(p))==0)
			continue;
		strncpy_safe(token,sizeof(token),s,p++-s);
		//plmn.rb=atoi(token);

		// get rat
		s=p;
		if((p=strchr_comma(p))==0)
			continue;
		strncpy_safe(token,sizeof(token),s,p++-s);
		plmn.act=atoi(token);

		// get cid - TODO: we may need it in hex
		s=p;
		if((p=strchr_comma(p))==0)
			continue;
		//trim_dq(plmn.cid,sizeof(plmn.cid),s,p++-s);
		p++;

		// get lac - TODO: we may need it in hex
		s=p;
		if((p=strchr_comma(p))==0)
			continue;
		//trim_dq(plmn.cid,sizeof(plmn.lac),s,p++-s);
		p++;

		/*
			^SNMON: "INS",0,2,3,"50F8","0B71","50502",583,31,"-21"
			^SNMON: "INS", <mode>, <rb>, <rat>, <cid>, <lac>, <plmn>, <uarfcn>, <rscp>, <dbm>, <psc>
		*/

		/*
			get mccmnc
		*/

		s=p;
		if((p=strchr_comma(p))==0)
			continue;

		// get mccmnc token
		trim_dq(token,sizeof(token),s,p++-s);
		mccmnc_len=strlen(token);

		if(mccmnc_len==5) {
			plmn.mcc_digit=3;
			plmn.mnc_digit=2;
		}
		else if (mccmnc_len==6) {
			plmn.mcc_digit=3;
			plmn.mnc_digit=3;
		}
		else {
			continue;
		}

		// get mcc and mnc
		strncpy_safe(mcc,sizeof(mcc),token,plmn.mcc_digit);
		strncpy_safe(mnc,sizeof(mnc),token+plmn.mcc_digit,plmn.mnc_digit);
		plmn.mcc=atoi(mcc);
		plmn.mnc=atoi(mnc);

		/*
			^SNMON: "INS",0,2,3,"50F8","0B71","50502",583,31,"-21"
			^SNMON: "INS", <mode>, <rb>, <rat>, <cid>, <lac>, <plmn>, <uarfcn>, <rscp>, <dbm>, <psc>
		*/

		// get uarfcn
		s=p;
		if((p=strchr_comma(p))==0)
			continue;
		strncpy_safe(token,sizeof(token),s,p++-s);
		//plmn.uarfcn=atoi(token);

		// get rscp
		s=p;
		if((p=strchr_comma(p))==0)
			continue;
		strncpy_safe(token,sizeof(token),s,p++-s);
		//plmn.uarfcn=atoi(token);

		// get dbm
		s=p;
		p3=strchr_comma(p);
		len=strlen(p);

		if(p3)
			p=p3;
		else if(len)
			p+=len;
		else
			continue;

		trim_dq(token,sizeof(token),s,p++-s);
		plmn.dbm=atoi(token);

		syslog(LOG_ERR,"[roaming] scanning - mcc=%d,mnc=%d,act=%d",plmn.mcc,plmn.mnc,plmn.act);

		// add plmn
		llplmn_add(crl,&plmn,1);
	}

	if(!llplmn_get_first(crl))
		goto err;

	return 0;

err:
	return -1;
}

#endif

int read_until(char * str, char *end, char *outbuf, int buflen)
{
	char *p;
	int cnt=1;

	if (!str || !end || !outbuf || !strlen(end) || buflen == 0)
		return -1;

	for(p=str; cnt < buflen && *p != '\0' && strncmp(p, end, strlen(end)); p++, cnt++)
	{
		*outbuf++=*p;
	}
	*outbuf = '\0';
	return 0;
}

// [flowchart] build prl - preferred roaming list (from the xml file)
int build_prl(const char* imsi)
{
	FILE* fp;
	char linebuf[1024];
	char token[128];
	char fname[128];

	struct plmn_t plmn;

	char* s;
	char* p;
	char* e;
	char* ver;

	int i;
	int total;

	struct stat st;

	// Assumed "Version:" string will lead the version number in prl file.
	char * leading = "Version:";
	const int leadingLen = strlen(leading);

	snprintf(linebuf,sizeof(linebuf),PLMN_CSV_FILE,imsi);
	if(stat(linebuf,&st)<0) {
		if(errno==ENOENT)
			snprintf(linebuf,sizeof(linebuf),PLMN_CSV_FACTORY_FILE,imsi);
	}

	syslog(LOG_ERR,"load PRL - %s",linebuf);
	strcpy(fname,linebuf);

	fp=fopen(linebuf,"r");
	if(!fp) {
		syslog(LOG_ERR,"failed to open %s - %s",linebuf,strerror(errno));
		goto err;
	}

	/*
		Rank,"""Best""",MCC,MNC,Country ,Network name,Tier,Priority,2G,3G,"OpCo_Code"

		1,X,202,05,Greece,Vodafone,Vodafone networks,1,x,x,VFGR
	*/

	rdb_setVal("manualroam.current_PRL_imsi_range",imsi);

	total=i=0;
	while( (p=fgets(linebuf,sizeof(linebuf),fp))!=0 ) {

		// get rid of
		if( (e=strchr(p,'\n'))!=0 )
			*e=0;

		// get rid of dos carrige return
		if( (e=strchr(p,'\r'))!=0 )
			*e=0;

		i++;

		// To get version number from PRL file
		if(ver=strstr(p,leading))
	       	{
			char * vernum = ver+leadingLen;
			while (isspace(*vernum)) vernum++; // trim leading whitespace

			token[0] = 0;
			read_until(vernum, "]", token, sizeof(token));

			rdb_setVal("manualroam.current_PRL_version",token);
		}

		/*
			using strchr_comma() and strchr() instead of strtok() as we may need a special parser in the future
		*/

		// reset plmn
		memset(&plmn,0,sizeof(plmn));

		// get rank
		s=p;
		if((p=strchr_comma(p))==0)
			continue;
		strncpy_safe(token,sizeof(token),s,p++-s);
		// quietly ignore - they are may be title
		if(isalpha(token[0]) && (i<=2))
			continue;
		if( (plmn.rank=atoi(token))==0 ) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no rank number available",i,fname);
			continue;
		}

		// get best
		s=p;
		if((p=strchr_comma(p))==0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no best field available",i,fname);
			continue;
		}
		strncpy_safe(token,sizeof(token),s,p++-s);

		plmn.best=(toupper(token[0])=='X') || (token[0]=='1');

		// get mcc
		s=p;
		if((p=strchr_comma(p))==0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no mcc field available",i,fname);
			continue;
		}
		strncpy_safe(token,sizeof(token),s,p++-s);
		plmn.mcc=atoi(token);
		plmn.mcc_digit=strlen(token);

		// get mnc
		s=p;
		if((p=strchr_comma(p))==0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no mnc field available",i,fname);
			continue;
		}
		strncpy_safe(token,sizeof(token),s,p++-s);
		plmn.mnc=atoi(token);
		plmn.mnc_digit=strlen(token);

		// get country
		s=p;
		if((p=strchr_comma(p))==0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no country field available",i,fname);
			continue;
		}
		strncpy_safe(plmn.country,sizeof(plmn.country),s,p++-s);

		// get network name
		s=p;
		if((strchr_comma(p))!=0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no network name field available",i,fname);
			continue;
		}
		strncpy_safe(plmn.network,sizeof(plmn.network),s,sizeof(plmn.network));

/* TT#5825 Update PRL list importer to only expect 6 columns in imported CSV file
		// get tier
		s=p;
		if((p=strchr_comma(p))==0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no tier field available",i,fname);
			continue;
		}
		strncpy_safe(plmn.tier,sizeof(plmn.tier),s,p++-s);

		// get priority
		s=p;
		if((p=strchr_comma(p))==0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no priority field available",i,fname);
			continue;
		}
		strncpy_safe(token,sizeof(token),s,p++-s);
		plmn.priority=atoi(token);

		plmn.act_genmask=0;

		// get 2g
		s=p;
		if((p=strchr_comma(p))==0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no 2g field available",i,fname);
			continue;
		}
		strncpy_safe(token,sizeof(token),s,p++-s);
		if(toupper(token[0])=='X')
			plmn.act_genmask|=1<<2;

		// get 3g
		s=p;
		if((p=strchr_comma(p))==0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - no 3g field available",i,fname);
			continue;
		}
		strncpy_safe(token,sizeof(token),s,p++-s);
		if(toupper(token[0])=='X')
			plmn.act_genmask|=1<<3;

		// get opcode
		s=p;
		if((strchr_comma(p))!=0) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - incorrect field count",i,fname);
			continue;
		};

		strncpy_safe(plmn.opcode,sizeof(plmn.opcode),s,sizeof(plmn.opcode));*/

		if(!rbtplmn_add(prl,&plmn,1)) {
			syslog(LOG_ERR,"[roaming] ignore line#%d in %s - failed to add (mcc=%d,mnc=%d)",i,fname,plmn.mcc,plmn.mnc);
		}
		else {
			total++;
		}

		//syslog(LOG_ERR,"[roaming] prl line#%d - mcc=%d,mnc=%d,act=%d,best=%d,network=%s",i,plmn.mcc,plmn.mnc,plmn.act,plmn.best,plmn.network);
	}


	fclose(fp);

	return total;

err:
	return -1;
}

static int act2genmask(int act)
{
	int gen=0;

	// 3GPP TS 27.007 AT+COPS <AcT>

	switch(act) {
		case 0: // GSM
		case 1: // GSM Compact
			gen=2;
			break;

		case 2: // UTRAN
			gen=3;
			break;

		case 3: // GSM w/EGPRS
			gen=2;
			break;

		case 4: // UTRAN w/HSDPA
		case 5: // UTRAN w/HSUPA
		case 6: // UTRAN w/HSUPA and HSUPA
			gen=3;
			break;

		case 7: // E-UTRAN
			gen=4;
			break;
	}

	return 1<<gen;
}

static char* send_atcmd_cimi()
{
	char* p;

	snprintf(atcmd,sizeof(atcmd),"AT+CIMI");
	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		goto err;

	p=atresp;

	if(!isdigit(*p)) {
		syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
		goto err;
	}

	// put a terminal null
	if( (p=strchr(p,'\n'))!=0 )
		*p=0;

	return atresp;

err:
	return 0;
}

extern int update_cinterion_signal_strength(void);

// RETURN VALUE
//  3 -> RSRP is Okay
//  2 -> RSCP is Okay
//  1 -> RSSI is Okay
//  0 -> error
// -1 -> RSSI is too low
// -2 -> RSCP is too low
// -3 -> RSRP is too low
// [flowchart] - check if the signal strength is adequate
int is_rssi_adequate(int rssi_threshold,int rscp_threshold,int rsrp_threshold,int* rssi_ret, int quiet)
{
	int rssi;
	int rscp;
	int rsrp; // 4G
	int res;

	int best;

	if(!quiet) {
		syslog(LOG_ERR,"[roaming] check Signal Strength");
	}

#if defined (MODULE_cinterion)
	// show operator and signal strength
	update_heartBeat();
	update_cinterion_signal_strength();
#endif

	update_heartBeat();

	rssi=get_rssi();
	rscp=get_rscp();
	rsrp=get_rsrp();

	if(rssi>=0 && rscp>=0 && rsrp>=0) {
		syslog(LOG_ERR,"failed to get rssi, rscp and rsrp");
		goto err;
	}

	if (rsrp<0) { // 4G
		res=rsrp>=rsrp_threshold?3:-3;
		best=rsrp;

		if(!quiet) {
			if(res>0) {
				syslog(LOG_ERR,"[roaming] RSRP signal OK (rsrp=%d)",best);
			}
			else {
				syslog(LOG_ERR,"[roaming] RSRP signal too weak (rsrp=%d)",best);
			}
		}
	}
	else if (rscp<0) { // 3G
		res=rscp>=rscp_threshold?2:-2;
		best=rscp;

		if(!quiet) {
			if(res>0) {
				syslog(LOG_ERR,"[roaming] RSCP signal OK (rscp=%d)",best);
			}
			else {
				syslog(LOG_ERR,"[roaming] RSCP signal too weak (rscp=%d)",best);
			}
		}
	}
	else { // 2G
		res=rssi>=rssi_threshold?1:-1;
		best=rssi;

		if(!quiet) {
			if(res>0) {
				syslog(LOG_ERR,"[roaming] RSSI signal OK (rssi=%d)",best);
			}
			else {
				syslog(LOG_ERR,"[roaming] RSSI signal too weak (rssi=%d)",best);
			}
		}
	}

	/* return rssi */
	if(rssi_ret)
		*rssi_ret=best;

	return res;
err:
	if(rssi_ret)
		rssi_ret=0;

	return 0;
}

int is_rssi_adequate_retry(struct plmn_t* plmn,int rssi_threshold,int rscp_threshold,int rsrp_threshold,int rssi_retry)
{
	int retry;
	int stat;
	int rssi;

	// get rssi stat
	stat=is_rssi_adequate(rssi_threshold,rscp_threshold,rsrp_threshold,&rssi, 0);

	// retry if it failed
	retry=0;
	while((stat<=0) && (retry++<rssi_retry)) {
		syslog(LOG_ERR,"[roaming] retry to get signal strength #%d/%d",retry,rssi_retry);

		sleep(1);
		stat=is_rssi_adequate(rssi_threshold,rscp_threshold,rsrp_threshold,&rssi, 0);
	}

	if(stat<=0) {
		if(stat==0) {
			/* update roaming ui information */
			store_ui_info(plmn,&rssi,NULL,NULL);
			set_manual_roam_msg_printf(rs_reg_provider_rssi_fail,"Registration to %s failed (Signal strength too low), trying next network...",get_ui_network());
		}
		else {
			/* update roaming ui information */
			store_ui_info(plmn,&rssi,NULL,NULL);
			set_manual_roam_msg_printf(rs_reg_provider_rssi_fail,"Registration to %s failed (Signal strength %d too low), trying next network...",get_ui_network(),rssi);
		}
	}

	return stat;
}

// [flowchart] - check if the sim card is of Vodafone global to support manual roaming algorithm
const char* is_manual_roam_simcard(char *buf, int buflen)
{
	char* imsi;
	char** p;

	int l1;
	int l2;

	if(buf && buflen)
		*buf=0;

	int retry;

	// get cimi
	imsi=send_atcmd_cimi();

	// retry
	retry=0;
	while(retry++<IMSI_RETRY_COUNT) {
		if(imsi && *imsi) {
			syslog(LOG_ERR,"[roaming] IMSI detected - '%s'",imsi);
			break;
		}

		syslog(LOG_ERR,"[roaming] retry to read IMSI #%d/%d",retry,IMSI_RETRY_COUNT);

		sleep(IMSI_RETRY_SLEEP);
		imsi=send_atcmd_cimi();
	}

	if(!imsi) {
		syslog(LOG_ERR,"[roaming] failed to get imsi - cannot apply manual roaming algorithm");
		goto err;
	}


	l1=strlen(imsi);

	// copy imsi
	if(buf && (buflen>0)) {
		strncpy(buf,imsi,buflen);
		buf[buflen-1]=0;
	}

	// compare prefix
	p=manual_roam_simcards;
	while(*p) {
		l2=strlen(*p);

		if( (l1>=l2) && !strncmp(imsi,*p,l2) )
			return *p;
		p++;
	}

	return 0;

err:
	return 0;
}

#if 0
static int send_atcmd_cfun(int stat)
{
	snprintf(atcmd,sizeof(atcmd),"AT+CFUN=%d",stat);
	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		return -1;

	return 0;
}
#endif

static int get_rscp()
{
	char* p;
	int rscp=1;

#if defined (MODULE_cinterion)
	char* saveptr;
	char* token;
	int idx;

/*
	AT^SMONI
	^SMONI: 3G,4436,96,-5.5,-60,505,01,0151,0E84D51,37,58,NOCONN
*/

	snprintf(atcmd,sizeof(atcmd),"AT^SMONI=255");
	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		goto fini;

	p=atresp;

	// skip prefix
	if(!(p=strstr(p,"^SMONI: "))) {
		syslog(LOG_ERR,"incorrect AT cmd resp prefix - cmd=%s,resp=%s",atcmd,atresp);
		goto fini;
	}
	p+=strlen("^SMONI: ");

	// bypass if band is not 3G
	if(strncmp(p,"3G",strlen("3G")))
		goto fini;

	// walk through all tokens
	token=strtok_r(p, ",", &saveptr);
	idx=0;
	while(token) {
		if(idx==4)
			rscp=atoi(token);

		token=strtok_r(NULL, ",", &saveptr);
		idx++;
	}
#elif defined (MODULE_MC7304)
/*
	+RSCP:
	Car0  RSCP: -91 dBm
	Car1  RSCP: n/a
*/

	snprintf(atcmd,sizeof(atcmd),"AT+RSCP?");
	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		goto fini;

	p=atresp;

	// skip prefix
	if(!(p=strstr(p,"+RSCP:"))) {
		//syslog(LOG_ERR,"incorrect AT cmd resp prefix - cmd=%s,resp=%s",atcmd,atresp);
		goto fini;
	}

	p+=strlen("+RSCP:");

	if(!(p=strstr(p,"Car0  RSCP:")))
		goto fini;

	p=p+strlen("Car0  RSCP:");
	rscp=(int) strtol(p, NULL, 10);
#else
	#warning TODO: need proper function to use Vodafone Manual Roaming Algorithm
#endif

fini:
	return rscp;
}

static int get_rsrp()
{
	char* p;
	int rsrp=1;

#if defined (MODULE_MC7304)
	snprintf(atcmd,sizeof(atcmd),"AT!GSTATUS?");
	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		goto fini;

	p=atresp;

/*
 * Example)
 *
 * !GSTATUS: 
 * Current Time:  1571             Temperature: 32
 * Bootup Time:   0                Mode:        ONLINE         
 * System mode:   LTE              PS state:    Attached     
 * LTE band:      B3               LTE bw:      20 MHz  
 * LTE Rx chan:   1550             LTE Tx chan: 19550
 * EMM state:     Registered       Normal Service 
 * RRC state:     RRC Idle       
 * IMS reg state: No Srv           
 *
 * RSSI (dBm):    -76              Tx Power:    0
 * RSRP (dBm):    -107             TAC:         4F3B (20283)
 * RSRQ (dB):     -9               Cell ID:     004ED30C (5165836)
 * SINR (dB):      9.6
 *
 */

	// skip prefix
	if(!(p=strstr(p,"RSRP (dBm):"))) {
		goto fini;
	}

	p+=strlen("RSRP (dBm):");
	rsrp=(int) strtol(p, NULL, 10);
#else
	#warning TODO: need proper function to use Vodafone Manual Roaming Algorithm (4G)
#endif

fini:
	return rsrp;
}

static char* send_atcmd_cgmr()
{
	char* p;

	snprintf(atcmd,sizeof(atcmd),"AT+CGMR");
	if( send_atcmd(MAX_TIMEOUT_CNT)<0)
		goto err;

	p=atresp;

	// put a null-termination at the end
	p=strstr(p,"\n");
	if(p)
		*p=0;

	return atresp;

err:
		return NULL;
}

static char* send_atcmd_cgsn()
{
	char* p;

	snprintf(atcmd,sizeof(atcmd),"AT+CGSN");
	if( send_atcmd(MAX_TIMEOUT_CNT)<0)
		goto err;

	p=atresp;

	// check validation
	if(!isdigit(*p)) {
		syslog(LOG_ERR,"incorrect AT cmd resp - cmd=%s,resp=%s",atcmd,atresp);
		goto err;
	}

	// put a null-termination at the end
	p=strstr(p,"\n");
	if(p)
		*p=0;

	return atresp;

err:
	return NULL;
}

static int get_rssi()
{
	char* p;
	int rssi;

	snprintf(atcmd,sizeof(atcmd),"AT+CSQ");
	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		goto err;

	p=atresp;

	// skip prefix
	if(!(p=strstr(p,"+CSQ: "))) {
		syslog(LOG_ERR,"incorrect AT cmd resp prefix - cmd=%s,resp=%s",atcmd,atresp);
		goto err;
	}
	p+=strlen("+CSQ: ");

	/*
	   at+csq
	   +CSQ: 13,99

	   0 -113 dBm or less
	   1 -111 dBm
	   2...30 -109... -53 dBm
	   31 -51 dBm or greater
	   99 not known or not detectable
	   */
	if(!isdigit(*p)) {
		syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
		goto err;
	}

	rssi=atoi(p);

	/*
	 * Below formula is based on the following table from 3GPP ETSI TS 127.007 standard
	 *
	 * =================================================
	 * +CSQ: <rssi>,<ber>
	 *
	 * Defined values
	 *
	 * <rssi>: integer type
	 * 0	-113 dBm or less
	 * 1	-111 dBm
	 * 2...30	-109... -53 dBm
	 * 31	-51 dBm or greater
	 * 99	not known or not detectable
	 *
	 * <ber>: integer type; channel bit error rate (in percent)
	 * 0...7	as RXQUAL values in the table in 3GPP TS 45.008 [20] subclause 8.2.4
	 * 99	not known or not detectable
	 * =================================================
	 */
	rssi = (rssi == 99) ? 0 : (rssi * 2 - 113);

	return rssi;
err:
	return 1;
}

static int send_atcmd_cgreg()
{
	char* p;

	if(is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
		snprintf(atcmd,sizeof(atcmd),"AT+CGREG?");
		if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
			return -1;

		p=atresp;

		// skip prefix
		if(!(p=strstr(p,"+CGREG: "))) {
			syslog(LOG_ERR,"incorrect AT cmd resp prefix - cmd=%s,resp=%s",atcmd,atresp);
			goto err;
		}
		p+=strlen("+CGREG: ");

		// skip stat
		if( (p=strchr_comma(p))==0 ) {
			syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
			goto err;
		}
		p++;

		if(!isdigit(*p)) {
			syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
			goto err;
		}

		return atoi(p);
	}
	else {
		char rdb_val[128];
		if(rdb_get_single(rdb_name(RDB_NETWORKREG_STAT, ""),rdb_val,sizeof(rdb_val))<0)
			goto err;

		return (int) strtol(rdb_val, NULL, 10);
	}
err:
	return -1;
}

static int send_atcmd_creg()
{
	char* p;

	if(is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
		snprintf(atcmd,sizeof(atcmd),"AT+CREG?");
		if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
			return -1;

		p=atresp;

		// skip prefix
		if(!(p=strstr(p,"+CREG: "))) {
			syslog(LOG_ERR,"incorrect AT cmd resp prefix - cmd=%s,resp=%s",atcmd,atresp);
			goto err;
		}
		p+=strlen("+CREG: ");

		// skip stat
		if( (p=strchr_comma(p))==0 ) {
			syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
			goto err;
		}
		p++;

		if(!isdigit(*p)) {
			syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
			goto err;
		}

		return atoi(p);
	}
	else {
		char rdb_val[128];
		if(rdb_get_single(rdb_name(RDB_NETWORKREG_STAT, ""),rdb_val,sizeof(rdb_val))<0)
			goto err;

		return (int) strtol(rdb_val, NULL, 10);
	}
err:
	return -1;
}

static int is_blank_apn()
{
	snprintf(atcmd,sizeof(atcmd),"AT+CGDCONT?");
	if( send_atcmd(TIMEOUT_CGACT_SET)<0)
		return -1;

	if(strstr(atresp,"+CGDCONT: 1,\"IP\",\"\"")) {
		return 0;
	}

	return -1;
}


static int send_atcmd_cgdcont(const char* apn)
{
	snprintf(atcmd,sizeof(atcmd),"AT+CGDCONT=1,\"IP\",\"%s\"",apn);
	if( send_atcmd(TIMEOUT_CGACT_SET)<0)
		return -1;

	return 0;
}

static int send_atcmd_cgact_set(int active,int profile)
{
	snprintf(atcmd,sizeof(atcmd),"AT+CGACT=%d,%d",active?1:0,profile);
	if( send_atcmd(TIMEOUT_CGACT_SET)<0)
		return -1;

	return 0;
}

static int send_atcmd_at()
{
	snprintf(atcmd,sizeof(atcmd),"AT");
	if( send_atcmd(MAX_TIMEOUT_CNT)<0)
		return -1;

	return 0;
}

static int send_atcmd_cgatt_set(int active,int sec)
{
	if(!sec)
		sec=TIMEOUT_CGACT_SET;

	snprintf(atcmd,sizeof(atcmd),"AT+CGATT=%d",active?1:0);
	if( send_atcmd(sec)<0)
		return -1;

	return 0;
}

/*
static int is_auto_apn_enabled()
{
	char rdb_val[128];

	if(rdb_get_single("webinterface.autoapn",rdb_val,sizeof(rdb_val))<0) {
		syslog(LOG_ERR,"failed to get rdb[webinterface.autoapn] - %s",strerror(errno));
		return 0;
	}

	return atoi(rdb_val);
}
*/

static int get_enabled_wwan_profile_value(const char* rdb, char* buf,int buflen)
{
	char rdb_var[128];
	char rdb_val[128];
	int i;
	int en;

	int len;
	const char* wwan="wwan.";
	int wwan_len;

	wwan_len=strlen(wwan);

	for(i=0;i<32;i++) {

		// get dev name
		snprintf(rdb_var,sizeof(rdb_var),"link.profile.%d.dev",i);
		if(rdb_get_single(rdb_var,rdb_val,sizeof(rdb_val))<0) {
			if(errno!=ENOENT)
				syslog(LOG_ERR,"failed to read rdb[%s] - %s",rdb_var,strerror(errno));
			continue;
		}

		// bypass if shorter prefix
		if( (len=strlen(rdb_val))<wwan_len ) {
			continue;
		}

		// bypass if not matching
		if(strncmp(rdb_val,wwan,wwan_len) ) {
			continue;
		}

		// Only Bovine has dynamic link profiles.
#ifdef PLATFORM_BOVINE
		// Ignore if this profile is not in use.
		// Continue checking if the "delflag" does not exist as profiles below
		// LINK_PROFILE_MIN_DYNAMIC do not have "delflag"s.
		snprintf(rdb_var, sizeof(rdb_var), "link.profile.%d.delflag", i);
		if (rdb_get_single(rdb_var, rdb_val, sizeof(rdb_val)) == 0) {
			if (atoi(rdb_val) != LINK_PROFILE_DELFLAG_USED) {
				// Either marked for deletion or defunct, so ignore this profile.
				continue;
			}
		}
		else if (i >= LINK_PROFILE_MIN_DYNAMIC) {
			// Raise an error but continue checking anyway.
			syslog(LOG_ERR, "Dynamic profile is missing delflag: %d", i);
		}
#endif
		// get enable
		snprintf(rdb_var,sizeof(rdb_var),"link.profile.%d.enable",i);
		if(rdb_get_single(rdb_var,rdb_val,sizeof(rdb_val))<0) {
			syslog(LOG_ERR,"failed to read rdb[%s] - %s",rdb_var,strerror(errno));
			continue;
		}

		// bypass if not enabled
		en=atoi(rdb_val);
		if(!en)
			continue;

		if(rdb) {
			// get apn
			snprintf(rdb_var,sizeof(rdb_var),"link.profile.%d.%s",i,rdb);
			if(rdb_get_single(rdb_var,rdb_val,sizeof(rdb_val))<0) {
				syslog(LOG_ERR,"failed to read rdb[%s] - %s",rdb_var,strerror(errno));
				continue;
			}

			// copy apn
			strncpy(buf,rdb_val,buflen);
			buf[buflen-1]=0;
		}
		return 0;
	}

	return -1;
}

#if defined (MODULE_cinterion)
static int send_atcmd_sgauth(int auth,const char* user, const char* pw)
{
	if(auth) {
		snprintf(atcmd,sizeof(atcmd),"AT^SGAUTH=1,%d,\"%s\",\"%s\"",auth,pw,user);
	}
	else {
		snprintf(atcmd,sizeof(atcmd),"AT^SGAUTH=1,%d",auth);
	}

	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		return -1;

	return 0;
}
#elif defined (MODULE_MC7304)
static int send_atcmd_qcpdpp(int auth,const char* user, const char* pw)
{
	if(auth) {
		snprintf(atcmd,sizeof(atcmd),"AT$QCPDPP=1,%d,\"%s\",\"%s\"",auth,pw,user);
	}
	else {
		snprintf(atcmd,sizeof(atcmd),"AT$QCPDPP=1,%d",auth);
	}

	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		return -1;

	return 0;
}
#else
	#warning TODO: need proper function to use Vodafone Manual Roaming Algorithm
#endif

static int send_atcmd_cgact_query()
{
	char* p;

	snprintf(atcmd,sizeof(atcmd),"AT+CGACT?");
	if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
		return -1;

	p=atresp;

	// skip prefix
	if(!(p=strstr(p,"+CGACT: "))) {
		syslog(LOG_ERR,"incorrect AT cmd resp prefix - cmd=%s,resp=%s",atcmd,atresp);
		goto err;
	}
	p+=strlen("+CGACT: ");

	// get profile 1
	if(!(p=strstr(p,"1,"))) {
		syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
		goto err;
	}
	p+=strlen("1,");

	// check status
	if(!isdigit(*p)) {
		syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
		goto err;
	}

	return atoi(p);
err:
	return -1;
}

static int send_atcmd_cgatt_query()
{
	char* p;

	if(is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
		snprintf(atcmd,sizeof(atcmd),"AT+CGATT?");
		if( send_atcmd(MAX_TIMEOUT_CNT)< 0)
			return -1;

		p=atresp;

		// skip prefix
		if(!(p=strstr(p,"+CGATT: "))) {
			syslog(LOG_ERR,"incorrect AT cmd resp prefix - cmd=%s,resp=%s",atcmd,atresp);
			goto err;
		}
		p+=strlen("+CGATT: ");

		// check status
		if(!isdigit(*p)) {
			syslog(LOG_ERR,"incorrect AT cmd resp format - cmd=%s,resp=%s",atcmd,atresp);
			goto err;
		}

		return atoi(p);
	}
	else {
		char rdb_val[128];
		if(rdb_get_single(rdb_name(RDB_NETWORKATTACHED, ""),rdb_val,sizeof(rdb_val))<0)
			goto err;

		return (int) strtol(rdb_val, NULL, 10);
	}
err:
	return -1;
}

int attach_pdp_session(const char* apn, int auth_type,const char* user,const char* pw)
{
	int stat;

	// update heart beat
	update_heartBeat();

#if defined (MODULE_cinterion)
	/* setup user name and password */
	if(send_atcmd_sgauth(auth_type,user,pw)<0) {
		syslog(LOG_ERR,"failed to set auth setup - auth=%d,user=%s,pw=%s",auth_type,user,pw);
		goto err;
	}
#elif defined (MODULE_MC7304)
	/* setup user name and password */
	if(send_atcmd_qcpdpp(auth_type,user,pw)<0) {
		syslog(LOG_ERR,"failed to set auth setup - auth=%d,user=%s,pw=%s",auth_type,user,pw);
		goto err;
	}
#else
	#warning TODO: need proper routine to use Vodafone Manual Roaming Algorithm
#endif
	// setup apn
	if(send_atcmd_cgdcont(apn)<0) {
		syslog(LOG_ERR,"failed to set APN");
		goto err;
	}

	// make pdp up
	if( send_atcmd_cgact_set(1,1)<0) {
		syslog(LOG_ERR,"failed to establish PDP session");
		goto err;
	}

	// get pdp status
	if( (stat=send_atcmd_cgact_query())<0) {
		syslog(LOG_ERR,"failed to get PDP session status");
		goto err;
	}

	// check PDP status
	if(!stat) {
		syslog(LOG_ERR,"PDP session is not up");
		goto err;
	}

	// query
	send_atcmd_cgact_set(0,1);

	return 0;
err:
	return -1;
}

static char* rdb_get_printf(const char* fmt,...)
{
	char var[1024];
	static char val[1024];

	va_list ap;

	va_start(ap, fmt);

	vsnprintf(var,sizeof(var),fmt,ap);

	if( rdb_get_single(var, val, sizeof(val))<0 )
		*val=0;

	va_end(ap);

	return val;
}

// First, pick up a profile that is enabled and set to defaultroute.
// If none, pick up a profile that is set to defaultroute amoung disabled profiles.
// If failed, return -1.
static int get_default_profile()
{
	int i;
	int en;
	int gw;
	const char* dev;


	for(i=1;i<=6;i++) {

		/* bypass if not enabled */
		en=atoi(rdb_get_printf("link.profile.%d.enable",i));
		if(!en)
			continue;

		/* bypass if dev not matched */
		dev=rdb_get_printf("link.profile.%d.dev",i);
		if(strcmp(dev,wwan_prefix))
			continue;

		/* bypass if not default gateway */
		gw=atoi(rdb_get_printf("link.profile.%d.defaultroute",i));
		if(!gw)
			continue;

		return i;
	}

	for(i=1;i<=6;i++) {

		/* bypass if dev not matched */
		dev=rdb_get_printf("link.profile.%d.dev",i);
		if(strcmp(dev,wwan_prefix))
			continue;

		/* bypass if not default gateway */
		gw=atoi(rdb_get_printf("link.profile.%d.defaultroute",i));
		if(!gw)
			continue;

		return i;
	}

	return -1;
}

static int wait_for_ps_attach(int sec)
{
	int i;
	int attached=0;

	i=0;
	while(i++<sec) {
		syslog(LOG_ERR,"[roaming] wait for PS attach #%d/%d",i,sec);
		attached=send_atcmd_cgatt_query()>0;
		if(attached) {
			syslog(LOG_ERR,"[roaming] PS attached");
			break;
		}

		sleep(1);
	}

	return attached;
}


int is_network_slow(struct plmn_t* plmn)
{
	char* network1;

	char* mcc;
	char* mnc;

	char* sp;

	network1=strdupa(plmn_rdb_get("manualroam.plmn.1"));

	/* do default network */
	if(!*network1) {
		/* movistar only */
		network1=strdupa("214,07");
	}

	syslog(LOG_ERR,"[roaming] [PS] RDB network info. (list=%s)",network1);

	/* get inital mcc & mnc */
	mcc=strtok_r(network1,",",&sp);
	mnc=strtok_r(NULL,",",&sp);

	while(mcc && mnc) {

		/* matching to plmn */
		if(plmn->mcc==atoi(mcc) && plmn->mnc==atoi(mnc)) {
			return 1;
		}

		/* get next mcc & mnc */
		mcc=strtok_r(NULL,",",&sp);
		mnc=strtok_r(NULL,",",&sp);
	}

	return 0;
}


/*
	PHS8-P workaround for a network issue (aka Movistar issue)
*/
int retry_cgatt_if_required(struct plmn_t* plmn)
{
	int creg;
	int cgreg;
	int i;

	char imsi[14+1];

	update_heartBeat();

	if(!is_manual_roam_simcard(imsi,sizeof(imsi))) {
		syslog(LOG_ERR,"[roaming] [PS] bypass PS re-attach procedure - not GDSP SIM card (imsi=%s)",imsi);
		return 0;
	}

	/* bypass if not required */
	if(!is_network_slow(plmn)) {
		syslog(LOG_ERR,"[roaming] [PS] bypass PS re-attach procedure - not target network (mcc=%d,mnc=%d,act=%d)",plmn->mcc,plmn->mnc,plmn->act);
		return 0;
	}

	/* process queued notification */
	at_wait_notification(-1);

	#define PS_REATTACH_CREG_TIMEOUT		20
	#define PS_REATTACH_CGREG_DETACH_TIMEOUT	5
	#define PS_REATTACH_CGREG_TIMEOUT		10
	#define PS_REATTACH_CGREG_RETRY			10

	syslog(LOG_ERR,"[roaming] [PS] start PS re-attach procedure");

	/* wait for CREG */
	syslog(LOG_ERR,"[roaming] [PS] check +CREG: 5");

	/* query and wait */
	creg=send_atcmd_creg();
	if(creg!=5 && creg!=1) {
		syslog(LOG_ERR,"[roaming] [PS] wait for +CREG: 5 (timeout=%d)",PS_REATTACH_CREG_TIMEOUT);
		creg=wait_for_noti_creg(PS_REATTACH_CREG_TIMEOUT);
		init_noti_creg();
	}

	if(creg!=5 && creg!=1) {
		syslog(LOG_ERR,"[roaming] [PS] failed - no +CREG: 5 found (creg=%d)",creg);
		goto err;
	}

	syslog(LOG_ERR,"[roaming] [PS] +CREG: 5 detected (creg=%d)",creg);

	/* query and wait */
	syslog(LOG_ERR,"[roaming] [PS] check +CGREG: 5");
	cgreg=send_atcmd_cgreg();
	if(cgreg!=5 && cgreg!=1) {
		syslog(LOG_ERR,"[roaming] [PS] wait for +CGREG: 5 (timeout=%d)",PS_REATTACH_CGREG_TIMEOUT);
		cgreg=wait_for_noti_cgreg(PS_REATTACH_CGREG_TIMEOUT);
		init_noti_cgreg();
	}

	/* retry if not attached */
	i=0;
	while(cgreg!=5 && cgreg!=1 && (i++<PS_REATTACH_CGREG_RETRY)) {

		update_heartBeat();
		syslog(LOG_ERR,"[roaming] [PS] send AT - stop previous command #%d/%d",i,PS_REATTACH_CGREG_RETRY);
		send_atcmd_at();

		syslog(LOG_ERR,"[roaming] [PS] send AT+CGATT=0 #%d/%d",i,PS_REATTACH_CGREG_RETRY);
		update_heartBeat();
		if(send_atcmd_cgatt_set(0,PS_REATTACH_CGREG_DETACH_TIMEOUT)<0) {
			syslog(LOG_ERR,"[roaming] [PS] failed in AT+CGATT=0 #%d/%d",i,PS_REATTACH_CGREG_RETRY);
			goto cgreg_fini;
		}

		syslog(LOG_ERR,"[roaming] [PS] wait for 3 seconds #%d/%d",i,PS_REATTACH_CGREG_RETRY);
		update_heartBeat();
		sleep(3);

		syslog(LOG_ERR,"[roaming] [PS] send AT+CGATT=1 #%d/%d",i,PS_REATTACH_CGREG_RETRY);
		update_heartBeat();
		if(send_atcmd_cgatt_set(1,PS_REATTACH_CGREG_TIMEOUT)<0) {
			syslog(LOG_ERR,"[roaming] [PS] failed in AT+CGATT=1 #%d/%d",i,PS_REATTACH_CGREG_RETRY);
			goto cgreg_fini;
		}

		syslog(LOG_ERR,"[roaming] [PS] wait for +CGREG: 5 (timeout=%d) #%d/%d",PS_REATTACH_CGREG_TIMEOUT,i,PS_REATTACH_CGREG_RETRY);
		cgreg=wait_for_noti_cgreg(PS_REATTACH_CGREG_TIMEOUT);

	cgreg_fini:
		/* clear queued messages */
		at_wait_notification(-1);
		init_noti_cgreg();
	}


	if(cgreg==5 || cgreg==1) {
		syslog(LOG_ERR,"[roaming] [PS] +CGREG: 5 detected (cgreg=%d)",cgreg);
	}
	else {
		syslog(LOG_ERR,"[roaming] [PS] failed - no +CGREG: 5 detected (cgreg=%d)",cgreg);
	}

	syslog(LOG_ERR,"[roaming] [PS] PS re-attach procedure completed");

	return 0;
err:
	return -1;
}

int retry_cgatt_if_required_mcc_mnc(int mcc,int mnc)
{
	struct plmn_t plmn;

	memset(&plmn,0,sizeof(plmn));

	plmn.mcc=mcc;
	plmn.mnc=mnc;

	return retry_cgatt_if_required(&plmn);
}

int retry_attach_pdp_session(struct plmn_t* plmn, int count)
{
	int stat;
	int retry;
	char apn[256];

	int cgatt_retry=TIMEOUT_ATT_TIMEOUT;
	int i;

	char auth_user[128];
	char auth_pw[128];
	char auth_pw_hidden[128];
	char rdb_pdp_validation[16];
	int auth_type_int;
	int pdp_validation;

	int len;

	int dp;

	int attached;


	if(retry_cgatt_if_required(plmn)<0) {
		syslog(LOG_ERR,"[roaming] PS re-attach procedure failed #as");
		return -1;
	}

	// wait until pdp domain is attached
	attached=wait_for_ps_attach(cgatt_retry);

	#if 0
	/* try once more after re-attach imsi / work around Vodafone steering - do not apply now */
	i=0;
	while(!attached && (i++)<ATT_RETRY) {
		syslog(LOG_ERR,"[roaming] [PS] attach failure - deregister and retry #%d/%d",i,ATT_RETRY);
		attach_imsi_registration(plmn,1);
		attached=wait_for_ps_attach(cgatt_retry);
	}
	#endif

	/* get rdb of pdp validation */
	if(rdb_get_single(RDB_PDP_VALIDATION_CHECK,rdb_pdp_validation,sizeof(rdb_pdp_validation))<0)
		strcpy(rdb_pdp_validation,"1");

	/* get pdp validation flag */
	pdp_validation=atoi(rdb_pdp_validation);

	if(pdp_validation) {
		syslog(LOG_ERR,"[roaming] PDP validation enabled");

		/* init. auth configuration */
		apn[0]=0;
		auth_user[0]=0;
		auth_pw[0]=0;

		/* search default profile */
		dp=get_default_profile();

		/* use manual roaming authentication if no default profile found */
		if(dp<0) {
			syslog(LOG_ERR,"[roaming] no default profile found - use default roaming apn/user/pw authentication");

			auth_type_int=2;
			strcpy(auth_user,"guest");
			strcpy(auth_pw,"guest");
			strcpy(apn,"");
		}
		else {
			syslog(LOG_ERR,"[roaming] use default profile apn/user/pw authentication (profile=%d)",dp);

#define STRNCPY(d,s,n) \
	do { \
		strncpy(d,s,n); \
		d[(n)-1]=0; \
	} while(0)

			STRNCPY(apn,rdb_get_printf("link.profile.%d.apn",dp),sizeof(apn));
			STRNCPY(auth_user,rdb_get_printf("link.profile.%d.user",dp),sizeof(auth_user));
			STRNCPY(auth_pw,rdb_get_printf("link.profile.%d.pass",dp),sizeof(auth_pw));

			/* get auth */
			if(!strcmp(rdb_get_printf("link.profile.%d.auth_type",dp),"pap"))
				auth_type_int=1;
			else
				auth_type_int=2;

			/* awesome Vodafone fake user */
			if(!*auth_user)
				strcpy(auth_user,"guest");
			else if(!strcmp(auth_user,"(blank)"))
				strcpy(auth_user,"");

			/* awesome Vodafone fake password */
			if(!*auth_pw)
				strcpy(auth_pw,"guest");
			else if(!strcmp(auth_pw,"(blank)"))
				strcpy(auth_pw,"");
		}

		/* get hidden password */
		len=strlen(auth_pw);
		for(i=0;i<len;i++) {
			if(!i)
				auth_pw_hidden[i]=auth_pw[i];
			else
				auth_pw_hidden[i]='*';
		}
		auth_pw_hidden[i]=0;

		if(!*auth_user || !*auth_pw) {
			syslog(LOG_ERR,"[roaming] blank user name or password detected - ignore authentication");
			auth_type_int=0;
		}

		// try apn
		syslog(LOG_ERR,"[roaming] attempt GPRS attach for PDP service (apn='%s',auth=%d,user=%s,pw=%s)",apn,auth_type_int,auth_user,auth_pw_hidden);
		stat=attach_pdp_session(apn,auth_type_int,auth_user,auth_pw);

		// retry if it fails
		retry=0;
		while( (stat<0) && (retry++<count) ) {
			syslog(LOG_ERR,"[roaming] retry GPRS attach for PDP service #%d/%d (apn='%s',auth=%d,user=%s,pw=%s)",retry,count,apn,auth_type_int,auth_user,auth_pw_hidden);

			sleep(1);
			stat=attach_pdp_session(apn,auth_type_int,auth_user,auth_pw);
		}

		if(stat<0) {
			syslog(LOG_ERR,"failed to attach for PDP service");

			/* update roaming ui information */
			store_ui_info(plmn,NULL,NULL,NULL);
			set_manual_roam_msg_printf(rs_pdp_fail,"Connection to APN on %s failed (PDP context failure), trying next network...",get_ui_network());

			goto err;
		}
	}
	else {
		syslog(LOG_ERR,"[roaming] PDP validation disabled");

		// try apn
		syslog(LOG_ERR,"[roaming] attempt GPRS attach for PS (with no PDP validation)");
		stat=send_atcmd_cgatt_set(1,0);

		// retry if it fails
		retry=0;
		while( (stat<0) && (retry++<count) ) {
			syslog(LOG_ERR,"[roaming] retry GPRS attach for PS (with no PDP validation) #%d/%d",retry,count);

			sleep(1);
			stat=send_atcmd_cgatt_set(1,0);
		}

		if(stat<0) {
			syslog(LOG_ERR,"failed in GPRS attach for PS");

			/* update roaming ui information */
			store_ui_info(plmn,NULL,NULL,NULL);
			set_manual_roam_msg_printf(rs_pdp_gprs_fail,"Registration to %s failed (GPRS attach failure), trying next network...",get_ui_network());

			goto err;
		}

	}

	return 0;

err:
	return -1;
}

static int print_mccmnc_with_digit(char* buf,int buflen,int mccmnc,int mccmnc_digit)
{
	char mccmnc_fmt[64];

	snprintf(mccmnc_fmt,sizeof(mccmnc_fmt),"%%0%dd",mccmnc_digit);
	return snprintf(buf,buflen,mccmnc_fmt,mccmnc);
}

int send_atcmd_cops_query()
{
	const char* p;

	snprintf(atcmd,sizeof(atcmd),"AT+COPS?");
	if(send_atcmd(AT_QUICK_COP_TIMEOUT)<0) {
		goto err;
	}

	p=atresp;

	// skip prefix
	if(!(p=strstr(p,"+COPS: "))) {
		syslog(LOG_ERR,"incorrect AT cmd resp prefix - cmd=%s,resp=%s",atcmd,atresp);
		goto err;
	}
	p+=strlen("+COPS: ");

	syslog(LOG_ERR,"[roaming] +COPS result (p=%s)",p);

	return *p-'0';

err:
	return -1;
}

#if defined(MODULE_MC7304)
/*
 * During testing VDF_NWL22W in Germany, it is found that time for COPS command to register to LTE Vodafone network can take up to 65 seconds.
 * Hence double that value for COPS timeout to register network.
 */
#define IMSI_ATTACHMENT_TIMEOUT (100)
#else
// Vodafone recommends imsi attachment timeout as 15 seconds.
#define IMSI_ATTACHMENT_TIMEOUT (15)
#endif


static int send_atcmd_cops(int mode,struct plmn_t* plmn)
{
	char mcc[3+1];
	char mnc[3+1];
	int timeout = AT_QUICK_COP_TIMEOUT;

	if(plmn) {
		// get mcc and mcc format
		print_mccmnc_with_digit(mcc,sizeof(mcc),plmn->mcc,plmn->mcc_digit);
		print_mccmnc_with_digit(mnc,sizeof(mcc),plmn->mnc,plmn->mnc_digit);

		// build at cmd - manual
		snprintf(atcmd,sizeof(atcmd),"AT+COPS=%d,%d,\"%s%s\",%d", mode, 2, mcc, mnc, plmn->act);
		timeout = IMSI_ATTACHMENT_TIMEOUT;
	}
	else {
		// build at cmd - manual
		snprintf(atcmd,sizeof(atcmd),"AT+COPS=%d", mode);
		timeout = AT_QUICK_COP_TIMEOUT;
	}

	/* init. notis */
	init_noti_creg();
	init_noti_cgreg();

#if defined(MODULE_MC7304)
	/* update heart beat now as COPS may take time */
	update_heartBeat();
#endif

	// send at cmd
	if(send_atcmd(timeout)<0) {
		goto err;
	}

#if defined(MODULE_EC21)
	quectel_band_fix();
#endif // MODULE_EC21

	return 0;

err:
	return -1;
}

// [flowchart]
int deregister_from_network()
{
	if(send_atcmd_cops(cops_mode_unreg,0)<0) {
		syslog(LOG_ERR,"failed to disable automatic cops");
		goto err;
	}

	return 0;

err:
	return -1;
}

// [flowchart]
int attach_imsi_registration(struct plmn_t* plmn,int unreg_first)
{
	int stat=0;
	int i;

	if(unreg_first) {
		// update heart beat
		update_heartBeat();

		// deregister
		if(send_atcmd_cops(cops_mode_unreg,0)<0) {
			syslog(LOG_ERR,"failed to send deregisteration command");
			goto err;
		}

		// wait
		i=0;
		while((i++<NETWORK_REG_TIMEOUT)) {
			stat=send_atcmd_creg();

			// 3GPP TS 27.007 AT+CREG
			if((stat<0) || ((stat!=creg_stat_reg_home) && (stat!=creg_stat_reg_roam)))
				break;
			sleep(1);
		}

		// check last stat
		// Regard as deregistered excpet creg_stat_reg_home and creg_stat_reg_roam
		// Because some modules return creg_stat_reg_searching(2) or creg_stat_reg_denied(3) instead of creg_stat_not_reg(0).
		if((stat==creg_stat_reg_home) || (stat==creg_stat_reg_roam)) {
			syslog(LOG_ERR,"failed to deregister from network - stat=%d",stat);
			goto err;
		}

	}

	// update heart beat
	update_heartBeat();

	// register
	if(send_atcmd_cops(cops_mode_reg,plmn)<0) {
		syslog(LOG_ERR,"failed to manually register to network");
		goto err;
	}

	// wait
	i=0;
	while((i++<NETWORK_REG_TIMEOUT)) {
		stat=send_atcmd_creg();

		// registered, homenetwork and registered, roaming
		if( (stat<0) || (stat==creg_stat_reg_home) || (stat==creg_stat_reg_roam) ) {
			break;
		}

		sleep(1);
	}

	// check last stat - registered, homenetwork and registered, roaming
	if((stat!=creg_stat_reg_home) && (stat!=creg_stat_reg_roam)) {
		syslog(LOG_ERR,"failed to register with network - stat=%d",stat);
		goto err;
	}

	return 0;
err:
	return -1;
}


static int is_3g(struct plmn_t* plmn)
{
	switch(plmn->act) {
		case 0:
		case 1:
		case 3:
			return 0;

	}

	return 1;
}

static int is_best_network(struct plmn_t* plmn)
{
	int network;

	if(!plmn)
		return 0;

	network=act2genmask(plmn->act);

	return plmn->best && ((cost_effective_mode == 1) || (network & (best_network_mask<<2)) == network);
}


struct plmn_t* get_highest_plmn(int preferred, int costEffectiveMode)
{
	struct plmn_t* plmn;
	struct plmn_t* hplmn=0;

	int act_genmask;
	int loop_cnt=0, temp=0;
	int serching_order[MAX_NETWORKS] ={0,0,0};

	/* 
	 * Example of cprl list (Sorted by Rank and then by act)
	 * Rank=210, mcc=505,mnc=3,act=7
	 * Rank=210, mcc=505,mnc=3,act=2
	 * Rank=210, mcc=505,mnc=3,act=0
	 * Rank=257, mcc=505,mnc=2,act=7
	 * Rank=257, mcc=505,mnc=2,act=2
	 * Rank=257, mcc=505,mnc=2,act=0
	 * Rank=508, mcc=505,mnc=1,act=7
	 * Rank=508, mcc=505,mnc=1,act=2
	 * Rank=508, mcc=505,mnc=1,act=0
	 */

	// check if crl exists
	if(!cprl) {
		syslog(LOG_ERR,"current preferred network PLMN list is not created");
		goto err;
	}

	temp=preferred;
	for (loop_cnt=0; loop_cnt < MAX_NETWORKS && temp != 0 ; loop_cnt++)
	{
		serching_order[loop_cnt] = (temp%10);
		temp = (int) temp/10;
	}

	for(loop_cnt=0; (loop_cnt < MAX_NETWORKS) && (serching_order[loop_cnt] != 0) && !hplmn; loop_cnt++)
	{
		plmn=llplmn_get_first(cprl);
		while (plmn && !hplmn) {
			if ((costEffectiveMode == 1) || ((act2genmask(plmn->act) & serching_order[loop_cnt]<<2)!=0))
					hplmn = plmn;

			plmn=llplmn_get_next(cprl);
		}
	}

#if 0 //OLD 
	act_genmask=preferred<<2;

	// search the preferred act network
	plmn=llplmn_get_first(cprl);

	while(plmn && !hplmn) {

		if( (costEffectiveMode == 1) || ((act2genmask(plmn->act) & act_genmask)!=0) )
			hplmn=plmn;

		plmn=llplmn_get_next(cprl);
	}

	// pick up the highiest network
	if(!hplmn) {
		hplmn=llplmn_get_first(cprl);
	}
#endif
	return hplmn;

err:
	return 0;
}

void check_prl()
{
	struct plmn_t plmn;
	struct plmn_t* prl_plmn;

	plmn.mcc=505;
	plmn.mnc=3;

	prl_plmn=rbtplmn_find(prl,&plmn);
	if(prl_plmn && prl_plmn->black_plmn) {
		syslog(LOG_ERR,"skip block plmn (mcc=%d,mnc=%d,act=%d)",plmn.mcc,plmn.mnc,plmn.act);
	}
}

int reset_blacklist()
{
	struct plmn_t* plmn;

	plmn=rbtplmn_get_first(prl);
	while(plmn) {
		plmn->black_plmn=0;
		plmn->bl_reason=e_bl_reason_none;
		plmn=rbtplmn_get_next(prl);
	}

	return 0;
}

int is_all_blacklisted_via_reg_failure()
{
	struct plmn_t* plmn;
	struct plmn_t* prl_plmn;

	// check if prl exists
	if(!prl) {
		syslog(LOG_ERR,"preferred roaming PLMN list is not created");
		goto err;
	}

	// check if crl exists
	if(!crl) {
		syslog(LOG_ERR,"current network PLMN list is not created");
		goto err;
	}

	// walk through crl (current availaable roaming list)
	plmn=llplmn_get_first(crl);
	while(plmn) {
		// find mcc mnc
		prl_plmn=rbtplmn_find(prl,plmn);

		if(prl_plmn) {
			if(!(prl_plmn->black_plmn & (1<<plmn->act)) || (prl_plmn->bl_reason != e_bl_reason_imsi_reg_failure)) {
				goto err; // there is plmn which is not blacklisted or has other reason rather than IMSI registration failure.
			}
		}

		plmn=llplmn_get_next(crl);
	}

	return 0;

err:
	return -1;
}

int is_autosim_enabled()
{
	char buf[32];

	/*
	 * "$SIM1_FAILBACK" = "cdn" -a "$SIM1_FAILOVER" = "n_inst" -a "$SIM_CDN" =  "0" -a "$SEC_PROF" != "0"
	 */

	if(rdb_get_single("service.simmgmt.sim1_failback",buf,sizeof(buf))<0 || strcmp(buf, "cdn"))
		goto err;

	if(rdb_get_single("service.simmgmt.sim1_failover",buf,sizeof(buf))<0 || strcmp(buf, "n_inst"))
		goto err;

	if(rdb_get_single("service.simmgmt.sim_cdn",buf,sizeof(buf))<0 || strcmp(buf, "0"))
		goto err;

	if(rdb_get_single("service.simmgmt.secondary",buf,sizeof(buf))<0 || !strcmp(buf, "0"))
		goto err;

	/* AutoSIM is enabled */
	return 0;

err:
	/* AutoSIM is disabled */
	return -1;
}

// [flowchart] build cprl - current preferred roaming list (current roaming list)
int build_cprl()
{
	struct plmn_t* plmn;
	struct plmn_t* prl_plmn;
	struct plmn_t* cprl_plmn;

	// destroy previous cprl
	if(cprl)
		llplmn_destroy(cprl);

	// create cprl
	if((cprl=llplmn_create())==0) {
		syslog(LOG_ERR,"failed to create cprl");
		goto err;
	}

	// check if prl exists
	if(!prl) {
		syslog(LOG_ERR,"preferred roaming PLMN list is not created");
		goto err;
	}

	// check if crl exists
	if(!crl) {
		syslog(LOG_ERR,"current network PLMN list is not created");
		goto err;
	}

	// walk through crl (current availaable roaming list) to get cprl (current preferred roaming list)
	plmn=llplmn_get_first(crl);
	while(plmn) {
		// find mcc mnc
		prl_plmn=rbtplmn_find(prl,plmn);

		if(prl_plmn) {
			if(prl_plmn->black_plmn & (1<<plmn->act)) {
				syslog(LOG_ERR,"skip block plmn (mcc=%d,mnc=%d,act=%d)",prl_plmn->mcc,prl_plmn->mnc,plmn->act);
			}
			#ifdef IGNORE_PRL_ACT
			else {
			#else
			else if( (act2genmask(plmn->act) & prl_plmn->act_genmask)!=0 ) {
			#endif

				//syslog(LOG_ERR,"[roaming] prl mcc=%d,mnc=%d,act=%d,best=%d,network=%s",prl_plmn->mcc,prl_plmn->mnc,prl_plmn->act,prl_plmn->best,prl_plmn->network);

				if( (cprl_plmn=llplmn_add(cprl,prl_plmn,1))==0 ) {
					syslog(LOG_ERR,"failed to add plmn to cprl - mcc=%d,mnc=%d",prl_plmn->mcc,prl_plmn->mnc);
				}
				else {
					cprl_plmn->act=plmn->act;
				}
			}
		}

		plmn=llplmn_get_next(crl);
	}

	// sort
	llplmn_sort(cprl);

	return 0;

err:
	return -1;
}

void fini_plmn()
{
	if(prl)
		rbtplmn_destroy(prl);

	if(netusage)
		netdevusage_destroy(netusage);

#ifdef RSSI_MONITOR_DRAFT
	if(netusage_rssi)
		netdevusage_destroy(netusage_rssi);
#endif
}


int put_on_reset_modem_schedule()
{
	clock_t now;
	char reset_count_buf[64];
	long long reset_count;
	struct tms tmsbuf;

	now=times(&tmsbuf)/sysconf(_SC_CLK_TCK);

	// bypass if scheduled already
	if(powercycle_scheduled) {
		syslog(LOG_ERR,"[roaming] powercycle modem already scheduled");
		goto err;
	}

	// read last reset count
	if(rdb_get_single("manualroam.reset_count",reset_count_buf,sizeof(reset_count_buf))<0) {
		reset_count_buf[0]=0;
	}

	// increase reset count
	reset_count=atoll(reset_count_buf);
	powercycle_schedule_delay=powercycle_schedule_delay*(1<<reset_count);

	// cap the delay
	if(powercycle_schedule_delay<powercycle_schedule_max_delay) {
		reset_count++;
		snprintf(reset_count_buf,sizeof(reset_count_buf),"%lld",reset_count);
		rdb_setVal("manualroam.reset_count",reset_count_buf);
	}
	else {
		powercycle_schedule_delay=powercycle_schedule_max_delay;
	}

	// set manual roam offline status
	rdb_setVal("manualroam.offline","1");

	// schedule
	if(powercycle_schedule_delay<60)
		syslog(LOG_ERR,"[roaming] [back-off-delay] schedule powercycle modem - %d sec later",powercycle_schedule_delay);
	else
		syslog(LOG_ERR,"[roaming] [back-off-delay] schedule powercycle modem - %d min later",powercycle_schedule_delay/60);

	powercycle_scheduled=1;
	powercycle_scheduled_time=now;

	rdb_setVal("system.hwfail_roaming","1");

#ifdef V_SIMMGMT
	if(rdb_get_single("service.simmgmt.manualroam.switching.modem_power_cycles",reset_count_buf,sizeof(reset_count_buf))<0) {
		reset_count_buf[0]=0;
	}

	reset_count=atoll(reset_count_buf);

	if (is_all_blacklisted_via_reg_failure() == 0 && is_autosim_enabled() == 0) {
		if (reset_count >= 3) {
			syslog(LOG_ERR,"[roaming] SIM switching is triggered via IMSI registration failures");
			clear_reset_modem_count();
			rdb_setVal("service.simmgmt.manualroam.switching.modem_power_cycles", "0");
			rdb_set_printf("service.simmgmt.manualroam.switching.trigger" ,"%d",getpid());
		} else {
			reset_count++;
			snprintf(reset_count_buf,sizeof(reset_count_buf),"%lld",reset_count);
			rdb_setVal("service.simmgmt.manualroam.switching.modem_power_cycles", reset_count_buf);
			syslog(LOG_ERR,"[roaming] IMSI registration failed for all of available networks. Consecutive modem power cycle counter is increased to: %d", reset_count);
		}
	}
	else if (reset_count != 0) {
			syslog(LOG_ERR,"[roaming] Reset consecutive modem power cycle counter");
			rdb_setVal("service.simmgmt.manualroam.switching.modem_power_cycles", "0");
	}
#endif

	// no network
	//send_atcmd_cfun(4);
	deregister_from_network();

	return 0;
err:
	return -1;
}

// [flowchart]
int reset_modem()
{
	syslog(LOG_ERR,"[roaming] resetting the modem");
	//syslog(LOG_ERR,"power-cycle the phone module");

	rdb_setVal("manualroam.resetting","1");

	system("reboot_module.sh --checkprofiles 2> /dev/null > /dev/null &");

	term_manual_roaming=1;

	return 0;
}

int clear_reset_modem_count()
{
	rdb_setVal("system.hwfail_roaming","0");

	return rdb_setVal("manualroam.reset_count","0");
}

int check_reset_modem_schedule()
{
	clock_t now;
	struct tms tmsbuf;

	now=times(&tmsbuf)/sysconf(_SC_CLK_TCK);

	// bypass if not schedueld
	if(!powercycle_scheduled)
		goto err;

	// bypass if not timed out
	if( (now-powercycle_scheduled_time)<powercycle_schedule_delay) {
		goto fini;
	}

	reset_modem();

fini:
	return 0;

err:
	return -1;
}

int set_black_tag_in_plmn(struct plmn_t* plmn, enum bl_reasons_t bl_reason)
{
	struct plmn_t* prl_plmn;

	syslog(LOG_ERR,"[roaming] add the plmn to black list (mcc=%d,mnc=%d,act=%d)",plmn->mcc,plmn->mnc,plmn->act);

	if( !(prl_plmn=rbtplmn_find(prl,plmn)) ) {
		// this error is not supposed to happen
		syslog(LOG_ERR,"unable to set black tag in plmn - not found (mcc=%d,mnc=%d,act=%d)",plmn->mcc,plmn->mnc,plmn->act);
		goto err;
	}

	/* set black plmn */
	prl_plmn->black_plmn|=1<<plmn->act;
	prl_plmn->bl_reason=bl_reason;

	return 0;
err:
	return -1;
}

int retry_attach_imsi_registration(struct plmn_t* plmn,int count,int quiet)
{
	int retry;
	int stat;
	int creg=send_atcmd_creg();

#define	DEFAULT_AUTH 2
#define	DEFAULT_USER "guest"
#define	DEFAULT_PW   "guest"
#define	DEFAULT_APN  ""

	#if 0
	int attached;
	#endif

	syslog(LOG_ERR,"[roaming] attempt GSM IMSI attach registration (mcc=%d,mnc=%d,act=%d,lname='%s')",plmn->mcc,plmn->mnc,plmn->act,plmn->network);

	// Registration in LTE network means registering a module in a network and activating PDP content with APN.
	// So, if module does not have available profile data, registration procedure will be failed.
#if defined (MODULE_MC7304)
	if (plmn->act == 7 && (creg == creg_stat_reg_denied || creg == creg_stat_reg_searching) && is_blank_apn() < 0)
	{
		if((send_atcmd_qcpdpp(DEFAULT_AUTH,DEFAULT_USER,DEFAULT_PW)==0) && (send_atcmd_cgdcont(DEFAULT_APN)) == 0) {
			syslog(LOG_ERR,"Set default APN, Usernam, Password in LTE");
		}
	}
#endif
	// attach
	stat=attach_imsi_registration(plmn,1);

	/* override Vodafone network steering - do not apply now */
	#if 0
	if(!stat) {
		syslog(LOG_ERR,"[roaming] [PS] check PS attach (mcc=%d,mnc=%d,act=%d,lname='%s')",plmn->mcc,plmn->mnc,plmn->act,plmn->network);

		attached=wait_for_ps_attach(3);
		if(attached) {
			syslog(LOG_ERR,"[roaming] [PS] IMSI and PS attached (mcc=%d,mnc=%d,act=%d,lname='%s')",plmn->mcc,plmn->mnc,plmn->act,plmn->network);
			goto fini;
		}

		syslog(LOG_ERR,"[roaming] [PS] IMSI attached without PS attached (mcc=%d,mnc=%d,act=%d,lname='%s')",plmn->mcc,plmn->mnc,plmn->act,plmn->network);
	}

	syslog(LOG_ERR,"[roaming] [PS] attempt GSM IMSI attach registration after deregstering (mcc=%d,mnc=%d,act=%d,lname='%s')",plmn->mcc,plmn->mnc,plmn->act,plmn->network);
	stat=attach_imsi_registration(plmn,1);
	#endif

	// retry to attach
	retry=0;
	while( (stat<0) && (retry++<count)) {
		syslog(LOG_ERR,"[roaming] re-attempt GSM IMSI attach registration #%d/%d (mcc=%d,mnc=%d,act=%d)",retry,count,plmn->mcc,plmn->mnc,plmn->act);

		sleep(1);
		stat=attach_imsi_registration(plmn,1);
	}

	if(stat<0) {
		if(!quiet) {
			/* update roaming ui information */
			store_ui_info(plmn,NULL,NULL,NULL);
			set_manual_roam_msg_printf(rs_reg_provider_imsi_fail,"Registration to %s failed (IMSI attach failure), trying next network...",get_ui_network());
		}
	}

#if 0
fini:
#endif
	return stat;
}

void update_roaming_params(int quiet)
{
	char prefer_act_str[16];
	char temp_buf[16];
	int cnt=0, temp=0;

	// get 2g3g preference
	if(rdb_get_single("manualroam.2g3gpreference",prefer_act_str,sizeof(prefer_act_str))<0) {
		// assume 3g connection by default
		strcpy(prefer_act_str,"0");
	}
	temp=prefer_act=atoi(prefer_act_str);
	prefer_act_mask=0;
	best_network_mask=0;
	for (cnt=0; cnt < MAX_NETWORKS && temp != 0 ; cnt++)
	{
		prefer_act_mask |= (temp%10);
		if (!best_network_mask)
			best_network_mask |= (temp%10);
		temp = (int) temp/10;
	}

	//get cost effective mode
	prefer_act_str[0] = 0;
	if(rdb_get_single("manualroam.costeffectivemode",prefer_act_str,sizeof(prefer_act_str))<0) {
		// assume Cost Effective Mode disabled by default
		strcpy(prefer_act_str,"0");
	}
	cost_effective_mode=atoi(prefer_act_str);

	// get rssi user threshold
	if(rdb_get_single("manualroam.rssi_user_threshold",temp_buf,sizeof(temp_buf))<0) {
		*temp_buf=0;
	}

	// get rssi threshold
	rssi_user_threshold=atoi(temp_buf);
	if(!rssi_user_threshold)
		rssi_user_threshold=-105;

	// get rscp user threshold
	if(rdb_get_single("manualroam.rscp_user_threshold",temp_buf,sizeof(temp_buf))<0) {
		*temp_buf=0;
	}

	// get rscp threshold
	rscp_user_threshold=atoi(temp_buf);
	if(!rscp_user_threshold)
		rscp_user_threshold=-105;

	// get rsrp user threshold
	if(rdb_get_single("manualroam.rsrp_user_threshold",temp_buf,sizeof(temp_buf))<0) {
		*temp_buf=0;
	}

	rsrp_user_threshold=atoi(temp_buf);
	if(!rsrp_user_threshold)
		rsrp_user_threshold=-120;

#ifdef REG_MONITOR_DRAFT
	// get IMSI registration timeout
	if(rdb_get_single(RDB_REG_MONITOR_TIMEOUT,temp_buf,sizeof(temp_buf))<0) {
		*temp_buf=0;
	}

	reg_monitor_timeout_sec=atoi(temp_buf);
	if(!reg_monitor_timeout_sec || reg_monitor_timeout_sec < 0)
		reg_monitor_timeout_sec=15;
#endif

	if(!quiet)
		syslog(LOG_ERR,"rssi_user_threshold = %d, rscp_user_threshold = %d, rsrp_user_threshold = %d, reg_monitor_timeout_sec = %d",rssi_user_threshold, rscp_user_threshold, rsrp_user_threshold, reg_monitor_timeout_sec);
}

// [flowchart]
struct plmn_t* select_network_by_algorithm(int count,int quiet)
{
	struct plmn_t* plmn=0;
	int retry;
	int scan_succ;

	// get current preferred roaming list
	if(build_cprl()<0)
		goto err;

	syslog(LOG_ERR,"[roaming] start select network algorithm procedure -[cost_effective_mode=%s ] [prefer_act=%d]",cost_effective_mode?"Enabled":"Disabled",prefer_act);

	// select network - Vodafone algorithm [select network algorithm sub-process]
	plmn=get_highest_plmn(prefer_act,cost_effective_mode);


	scan_succ=1;

	retry=0;
	while(!plmn && retry++<count) {
		set_connection_mgr_suspend_timer(SUSPEND_DURATION);

		if(!quiet && scan_succ) {
			/* update roaming ui information */
			store_ui_info(NULL,NULL,NULL,NULL);
			set_manual_roam_msg_printf(rs_no_networks_avail,"No networks available, rescaning...");
		}

		scan_succ=scan_network(0,retry,count,quiet)==0;
		if(scan_succ) {
			// get current preferred roaming list
			build_cprl();
			// select network - Vodafone algorithm [select network algorithm sub-process]
			plmn=get_highest_plmn(prefer_act,cost_effective_mode);
		}

		sleep(1);
	}


err:
	if(plmn) {
		syslog(LOG_ERR,"[roaming] end select network algorithm procedure - mcc=%d,mnc=%d,act=%d,rank=%d",plmn->mcc,plmn->mnc,plmn->act,plmn->rank);
	}
	else {
		syslog(LOG_ERR,"[roaming] end select network algorithm procedure - no network available");
	}

	return plmn;
}

static const char* get_powercycle_delay()
{
	static char power_delay_str[128];

	snprintf(power_delay_str,sizeof(power_delay_str),"%02d:%02d",powercycle_schedule_delay/60,powercycle_schedule_delay%60);

	return power_delay_str;
}

static const char* get_ui_network()
{
	return ui_network;
}

static void store_ui_info(struct plmn_t* plmn,int* rssi,clock_t* delay_start,int* delay_min)
{
	const char* act_str;

	if(plmn) {
		switch(plmn->act) {
			case 0:
			case 1:
			case 3:
				act_str="2G";
				break;

			case 7:
				act_str="4G";
				break;

			default:
				act_str="3G";
				break;
		}

		snprintf(ui_network,sizeof(ui_network),"%s (%s)",plmn->network,act_str);
		rdb_setVal(MANUALROAM_STAT_NETWORK,ui_network);
	}

	if(rssi) {
		ui_rssi=*rssi;
		rdb_set_printf(MANUALROAM_STAT_RSSI,"%d",ui_rssi);
	}

	if(delay_min) {
		ui_delay_min=*delay_min;
		rdb_set_printf(MANUALROAM_STAT_DELAY_MIN,"%d",ui_delay_min);
	}

	if(delay_start) {
		ui_delay_start=*delay_start;
		rdb_set_printf(MANUALROAM_STAT_DELAY_START,"%ld",ui_delay_start);
	}
}

// [flowchart]
struct plmn_t* do_network_registration()
{
	struct plmn_t* plmn;
	int stat;
	int algorithm_rescan_count;

	algorithm_rescan_count=algorithm_retry_count;

	while(1) {

		set_connection_mgr_suspend_timer(SUSPEND_DURATION);

		if(!(plmn=select_network_by_algorithm(algorithm_retry_count,0))) {
			put_on_reset_modem_schedule();

			/* update roaming ui information */
			store_ui_info(NULL,NULL,&powercycle_scheduled_time,&powercycle_schedule_delay);
			set_manual_roam_msg_printf(rs_conn_all_network_fail,"Connection to all networks failed. See registration log. Waiting %s (mm:ss) to retry.",get_powercycle_delay());
			set_connection_mgr_suspend_timer(powercycle_schedule_delay+5);
			goto err;
		}

		algorithm_rescan_count=0;

		// attach - if it fails, put to black list
		if(retry_attach_imsi_registration(plmn,network_attach_retry_count,0)<0) {
			set_black_tag_in_plmn(plmn,e_bl_reason_imsi_reg_failure);

			continue;
		}

		// check rssi
		if((stat=is_rssi_adequate_retry(plmn,rssi_user_threshold, rscp_user_threshold, rsrp_user_threshold,resend_csq_cmd))==0) {
			syslog(LOG_ERR,"cannot perform network registration - signal strength error");
			continue;
		}

		// check black tag in prl
		if(stat<0) {
			set_black_tag_in_plmn(plmn, e_bl_reason_signal_failure);
			continue;
		}

		// attach pdp - try up to 2 times if it fails
		if(retry_attach_pdp_session(plmn,pdp_attach_retry_count)<0) {
			set_black_tag_in_plmn(plmn, e_bl_reason_pdp_attach_failure);
			continue;
		}

		break;
	}

	return plmn;

err:
	return 0;
}

// [flowchart] "best" network registry sub-process
int select_last_best_network_if_possible(struct plmn_t* best_plmn)
{

	struct plmn_t* prl_plmn;

	int stat;

	// get prl plmn - paranoia check if it is in plmn (in case, the list is newly uploaded)
	if( (prl_plmn=rbtplmn_find(prl,best_plmn))==0 ) {
		syslog(LOG_ERR,"last best plmn does not exist in PRL any more");
		goto err;
	}

/* TT#5825 Update PRL list importer to only expect 6 columns in imported CSV file
	// paranoia check - if act is also matching (in case, the list is newly uploaded)
	if( (act2genmask(best_plmn->act) & prl_plmn->act_genmask)==0 ) {
		syslog(LOG_ERR,"last best plmn's act is not in PRL any more");
		goto err;
	}*/

	// paranoia check - if it is still best (in case, the list is newly uploaded)
	if(!prl_plmn->best) {
		syslog(LOG_ERR,"last best plmn is not best in PRL any more");
		goto err;
	}


	// attach - if it fails, try 2 more
	if(retry_attach_imsi_registration(best_plmn,network_attach_retry_count,0)<0) {
		syslog(LOG_ERR,"failed to attach to the best network (mcc=%d,mnc=%d,act=%d)",best_plmn->mcc,best_plmn->mnc,best_plmn->act);
		goto err;
	}

	// check rssi - reading failure? (TODO: no specific logic from Vodafone)
	if((stat=is_rssi_adequate_retry(prl_plmn,rssi_user_threshold, rscp_user_threshold, rsrp_user_threshold,resend_csq_cmd))==0) {
		syslog(LOG_ERR,"cannot perform network registration - signal strength error");
		goto err;
	}

	// check black tag in prl
	if(stat<0) {
		goto err;
	}

	// attach pdp - try up to 2 times if it fails
	if(retry_attach_pdp_session(prl_plmn,pdp_attach_retry_count)<0) {
		goto err;
	}


	return 0;
err:
	return -1;
}


static int scan_network(int count,int disp_retry,int disp_retry_total,int quiet)
{
	int stat;
	int retry;

	if(disp_retry_total)
		syslog(LOG_ERR,"[roaming] re-scan for available networks #%d/%d",disp_retry,disp_retry_total);
	else
		syslog(LOG_ERR,"[roaming] scan for available networks");

	// scan network
	stat=build_crl();

	// retry to scan network if fail
	retry=0;
	while( (stat<0) && (retry++<count) ) {

		if(!quiet) {
			/* update roaming ui information */
			store_ui_info(NULL,NULL,NULL,NULL);
			set_manual_roam_msg_printf(rs_no_networks_avail,"No networks available, rescaning...");
			syslog(LOG_ERR,"[roaming] re-scan for available networks #%d/%d",retry,rescan_retry_count);
		}

		sleep(5);
		// scan network
		stat=build_crl();
	}

	return stat;
}

int is_roam_mode_enabled()
{
	char enabled[16]={0,};

	if (rdb_get_single("manualroam.enable", enabled, sizeof(enabled)) != 0) {
		syslog(LOG_ERR,"failed to read manualroam.disable - %s",strerror(errno));
		enabled[0]=0;
	}

	if(*enabled)
		return atoi(enabled);

	return 1;
}

int is_custom_roam_mode()
{
	char manual_mode[16]={0,};

#if defined (MODULE_cinterion)
	if (rdb_get_single("plmn_manual_mode", manual_mode, sizeof(manual_mode)) != 0) {
		syslog(LOG_ERR,"failed to read plmn_manual_mode - %s",strerror(errno));
		manual_mode[0]=0;
	}
#elif defined (MODULE_MC7304)
	if (rdb_get_single(rdb_name(RDB_PLMNSEL, ""), manual_mode, sizeof(manual_mode)) != 0) {
		syslog(LOG_ERR,"failed to read plmn_manual_mode - %s",strerror(errno));
		manual_mode[0]=0;
	}
#else
	#warning TODO: need proper routine to use Vodafone Manual Roaming Algorithm
#endif

	return !atoi(manual_mode);
}


int do_custom_manual_roaming(int quiet)
{
	struct plmn_t* plmn;

	struct plmn_t last_plmn;
	struct plmn_t* prl_plmn;

	struct tms tmsbuf;

	int best_network;

	int reset_count;
	int auto_network;


	set_oper_setting_msg_printf("");
	/* get reset count */
	reset_count=atoi(plmn_rdb_get("manualroam.reset_count"));
	if(!reset_count) {
		/* update roaming ui information */
		store_ui_info(NULL,NULL,NULL,NULL);
		set_manual_roam_msg_printf(rs_commencing_reg,"Commencing registration...");
	}

	#if 0
	syslog(LOG_ERR,"[roaming] disable PDP connections");
	suspend_connection_mgr();
	send_atcmd_cgact_set(0,1);
	send_atcmd_cgact_set(0,2);
	#endif

	// load params
	update_roaming_params(0);

	// get last plmn
	prl_plmn=0;
	if(load_plmn(&last_plmn)==0) {
		if(!(prl_plmn=rbtplmn_find(prl,&last_plmn))) {
			syslog(LOG_ERR,"ignore last network");
		}
		else {
			if(prl_plmn->best)
				syslog(LOG_ERR,"[roaming] load last best network (mcc=%d,mnc=%d,act=%d)",last_plmn.mcc,last_plmn.mnc,last_plmn.act);
			else
				syslog(LOG_ERR,"[roaming] No record of last best network found");
		}
	}

	// disable auto roam
	syslog(LOG_ERR,"[roaming] check current network mode");
	auto_network=send_atcmd_cops_query()==0;
	if(auto_network) {
		#if 0
		syslog(LOG_ERR,"[roaming] automatic network selection mode detected");
		#endif
		syslog(LOG_ERR,"[roaming] disable automatic roaming");
		deregister_from_network();
	}
	else {
		syslog(LOG_ERR,"[roaming] manual network selection mode detected");
	}

	// if we fail to select the last network
	best_network=0;
	if(prl_plmn && prl_plmn->best) {
		syslog(LOG_ERR,"[roaming] start best network register procedure (mcc=%d,mnc=%d,act=%d)",last_plmn.mcc,last_plmn.mnc,last_plmn.act);

		if((cost_effective_mode == 0) && ((act2genmask(last_plmn.act) & (best_network_mask<<2)) == 0)) {
			syslog(LOG_ERR,"[roaming] user-preference network not matching to the last best network - [cost_effective_mode=%d, prefer_act=%d, perfer_act_mask=0x%x], best_network_mask=0x%x]",cost_effective_mode, prefer_act, prefer_act_mask, best_network_mask);
			if(!auto_network) {
				deregister_from_network();
			}
		}
		else {
			if(select_last_best_network_if_possible(&last_plmn)==0) {
				syslog(LOG_ERR,"[roaming] attached to last best network (mcc=%d,mnc=%d,act=%d)",last_plmn.mcc,last_plmn.mnc,last_plmn.act);
				best_network=1;

				if(!quiet)
					set_oper_setting_msg_printf("Attached to last best network");
			}
			else {
				syslog(LOG_ERR,"[roaming] failed to register to the last best network");
			}
		}
	}

	if(!best_network) {

		if(!quiet)
			set_oper_setting_msg_printf("Scanning radio networks");

		// reset if it fails
		if(scan_network(rescan_retry_count,0,0,0)<0) {
			put_on_reset_modem_schedule();

			/* update roaming ui information */
			store_ui_info(NULL,NULL,&powercycle_scheduled_time,&powercycle_schedule_delay);
			set_manual_roam_msg_printf(rs_no_networks_visible,"No networks visible, waiting %s (mm:ss) to retry.",get_powercycle_delay());
			set_connection_mgr_suspend_timer(powercycle_schedule_delay+5);
			goto fini;
		}
		else {
			syslog(LOG_ERR,"[roaming] start network registration procedure");

			if(!quiet)
				set_oper_setting_msg_printf("Start network registration procedure");

			if((plmn=do_network_registration())!=0) {
				// save the network if best
				syslog(LOG_ERR,"[roaming] end network registration procedure - GSM attach success (mcc=%d,mnc=%d)",plmn->mcc,plmn->mnc);
				save_plmn(plmn);

				if(is_best_network(plmn))
					best_network=1;
			}
			else {
				//syslog(LOG_ERR,"[roaming] end network registration procedure - GSM attach fail");
				goto fini;
			}
		}
	}

	if(best_network) {
		clear_reset_modem_count();

		/* update roaming ui information */
		store_ui_info(NULL,NULL,NULL,NULL);
		set_manual_roam_msg_printf(rs_done,"Done");

		syslog(LOG_ERR,"[roaming] custom roaming algorithm completed");
	}
	else {
		syslog(LOG_ERR,"[roaming] wait for %d min. to start best network retry procedure",get_best_network_retry_time()/60);

		/* update roaming ui information */
		store_ui_info(NULL,NULL,NULL,NULL);
		set_manual_roam_msg_printf(rs_done,"Done");
	}

	// update time
	plmn_update_time=times(&tmsbuf)/sysconf(_SC_CLK_TCK);

	reset_connection_mgr_suspend_timer();
fini:
	set_oper_setting_msg_printf("");
	return 0;
}

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

void set_connection_mgr_suspend_timer(int delay)
{
	char buf[32]={0,};
	long int system_time;

	if(!is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
		system_time = get_sys_uptime() + delay;

		sprintf(buf, "%ld", system_time);
		rdb_setVal("manualroam.suspend_connection_mgr",buf);
	}

}

void reset_connection_mgr_suspend_timer()
{
	if(!is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS)) {
		rdb_setVal("manualroam.suspend_connection_mgr","");
	}
}

void suspend_connection_mgr()
{

	set_connection_mgr_suspend_timer(SUSPEND_DURATION);

	// stop connection mgr
	rdb_set_single(rdb_name("system_network_status.registered","") ,"0");
	rdb_set_single(rdb_name("system_network_status.attached",""), "0");
	rdb_set_single(rdb_name("system_network_status.reg_stat",""), "2");
}

static int plmn_rdb_get_int(const char* rdb)
{
	char buf[128];

	// read mcc
	if( rdb_get_single(rdb, buf, sizeof(buf))<0 ) {
		buf[0]=0;
	}

	return atoi(buf);
}

static int rdb_setVal(const char* rdb,const char* val)
{
	int stat;

	if((stat=rdb_set_single(rdb,val))<0) {
		if(errno==ENOENT)
			stat=rdb_create_variable(rdb, val, CREATE, ALL_PERM, 0, 0);
	}

	if(stat<0)
		syslog(LOG_ERR,"failed to set rdb (%s => %s) - %s",rdb,val,strerror(errno));

	return stat;
}


int do_manual_roaming_if_necessary()
{
	const char* imsi_prefix;
	char imsi[14+1];
	int stat;
	char* imei;
	char* rev;

	// check if it is Vodafone global
	imsi_prefix=is_manual_roam_simcard(imsi,sizeof(imsi));

	// set roaming simcard status
	rdb_setVal("manualroam.custom_roam_simcard",imsi_prefix?"1":"0");
	// clear roaming error
	rdb_setVal("system.hwfail_roaming","0");
	// clear resetting flag
	rdb_setVal("manualroam.resetting","0");

	rdb_setVal("manualroam.current_PRL_imsi_range","");
	rdb_setVal("manualroam.current_PRL_version","");

	// bypass if imsi is not for manual roaming
	if(imsi_prefix==0) {
		syslog(LOG_ERR,"[roaming] skip custom roaming algorithm - no custom SIM card detected (imsi='%s')",imsi);

		/*
			when roaming algorithm gets disabled, PHS8-P forgets the previous selection
			to work-around this buggy PHS8-P behaviour, we are switching to automatic selection mode
		*/
#if defined (MODULE_cinterion)
		pxs8_update_phys_cops(1);
#endif
		goto err;
	}

	// bypass if disabled
	if(!is_roam_mode_enabled()) {
		syslog(LOG_ERR,"[roaming] custom roaming algorithm disabled by rdb [manualroam.enable]");

		/*
			when roaming algorithm gets disabled, PHS8-P forgets the previous selection
			to work-around this buggy PHS8-P behaviour, we are switching to automatic selection mode
		*/
#if defined (MODULE_cinterion)
		pxs8_update_phys_cops(1);
#endif
		goto err;
	}

	syslog(LOG_ERR,"[roaming] custom SIM card detected (imsi=%s)",imsi_prefix);

	// load csv
	syslog(LOG_ERR,"[roaming] load PRL list (imsi=%s)",imsi_prefix);
	stat=build_prl(imsi_prefix);
	if( stat==0 ) {
		syslog(LOG_ERR,"[roaming] PRL file is emtpy - reverting to SIM card auto roaming mode");
		goto fatal;
	}
	else if(stat<0) {
		syslog(LOG_ERR,"[roaming] PRL file does not exist - reverting to SIM card auto roaming mode");
		goto fatal;
	}


	custom_roaming_algorithm=1;

	#ifdef MODULE_TEST
	check_prl();
	#endif

	if(!is_custom_roam_mode()) {
		syslog(LOG_ERR,"[roaming] skip custom roaming algorithm - user specified manual mode");
		goto err;
	}

	// assume all the modules for manual plmn has this feature - let connection_mgr not try to connect during the manaul roaming procedure
	suspend_connection_mgr();

	/* do have some essential information ready before start roaming algorithm - imei, ccid and imsi */

	// update sim status
	syslog(LOG_ERR,"[roaming] preparing - setting sim status");
	update_sim_status();

	if(imsi_prefix) {
		/* imsi - wrong function name */
		syslog(LOG_ERR,"[roaming] preparing - setting imsi");
		update_imsi();

		/* ccid */
		syslog(LOG_ERR,"[roaming] preparing - setting ccid");
		update_ccid();
	}

	/* imei */
#if defined (MODULE_cinterion)
	if((imei=send_atcmd_cgsn())!=0) {
		syslog(LOG_ERR,"[roaming] preparing - setting imei (%s)",imei);
		rdb_setVal(rdb_name(RDB_IMEI,""),imei);
	}
#elif defined (MODULE_MC7304)
	update_imei_from_ati();
#else
	#warning TODO: need proper routine to use Vodafone Manual Roaming Algorithm
#endif

	/* firmware version */
	if((rev=send_atcmd_cgmr())!=0) {
		syslog(LOG_ERR,"[roaming] preparing - setting module firmware rev (%s)",rev);
		rdb_setVal(rdb_name(RDB_FIRMWARE_VERSION, ""), rev);
	}

#if defined (MODULE_cinterion)
	syslog(LOG_ERR,"[roaming] preparing - setting module firmware CID");
	update_cinterion_firmware_revision();
#endif

	return do_custom_manual_roaming(1);

err:
	return -1;

fatal:
	if(send_atcmd_cops_query()==0) {
		syslog(LOG_ERR,"[roaming] phone module is already using SIM card auto roaming mode");
	}
	else {
		syslog(LOG_ERR,"[roaming] switching to SIM card auto roaming mode");
		send_atcmd_cops(0,NULL);
	}

	rdb_setVal("system.hwfail_roaming","1");

	return -1;

}

int attach_to_current_plmn(int quiet)
{
	if(!current_plmn) {
		syslog(LOG_ERR,"no current plmn selected");
		return -1;
	}

	syslog(LOG_ERR,"try to register to the current network (mcc=%d,mnc=%d,act=%d)",current_plmn->mcc,current_plmn->mnc,current_plmn->act);

	// attach - if it fails, try 2 more
	if(retry_attach_imsi_registration(current_plmn,network_attach_retry_count,quiet)<0) {
		syslog(LOG_ERR,"failed to attach to the current network (mcc=%d,mnc=%d,act=%d)",current_plmn->mcc,current_plmn->mnc,current_plmn->act);
		return -1;
	}

	syslog(LOG_ERR,"registered to the current network (mcc=%d,mnc=%d,act=%d)",current_plmn->mcc,current_plmn->mnc,current_plmn->act);
	return 0;
}

static int get_best_network_retry_time()
{
	char rdb_best_network_time[64];
	int best_network_time;

	/* get network time */
	if(rdb_get_single(RDB_BEST_NETWORK_TIME,rdb_best_network_time,sizeof(rdb_best_network_time))<0)
		*rdb_best_network_time=0;

	/* use default best network scan time */
	if(!*rdb_best_network_time)
		best_network_time=retry_for_best_time;
	else
		best_network_time=atoi(rdb_best_network_time)*60;

	return best_network_time;
}

int retry_for_best_network()
{
	clock_t now;
	clock_t past_sec;
	int creg_stat;
	int cgatt_stat;
	clock_t past_traffic_sec;
	struct tms tmsbuf;

	int stat;
	struct plmn_t* plmn=0;

	int rescan;

	int best_network_time;

	int scan_period_min;

	static int best_network_registered=0;

#ifdef RSSI_MONITOR_DRAFT
	int rescan_for_rssi_threshould = 0;
#endif

#ifdef REG_MONITOR_DRAFT
	int rescan_for_reg_monitor = 0;
#endif

#ifdef MODULE_TEST
	syslog(LOG_ERR,"tick");
#endif

	// bypass if manual roaming is terminating
	if(term_manual_roaming || !custom_roaming_algorithm)
		goto err;

	// bypass if no plmn is selected
	if(!current_plmn || !automatic_operator_setting)
		goto fini;

	// assume we do not rescan
	rescan=0;

	// get current time
	now=times(&tmsbuf)/sysconf(_SC_CLK_TCK);

	// get network attachment status
	creg_stat=send_atcmd_creg();
	cgatt_stat=send_atcmd_cgatt_query();

	// check if we have a pdp domain issue when a wwan profile is activated
	{
		int profile_en;
		int registered;
		int attached;

		int cur_detach_stat;

		static clock_t detach_time=0;
		static int prev_detach_stat=0;

		profile_en=get_enabled_wwan_profile_value(0,0,0)>=0;
		registered=(creg_stat==creg_stat_reg_home) || (creg_stat==creg_stat_reg_roam);
		attached=cgatt_stat==1;

		cur_detach_stat=profile_en && registered && !attached;

		if(!prev_detach_stat && cur_detach_stat) {
			syslog(LOG_ERR,"[roaming] start monitoring attach status - wait for %d sec",pdp_domain_detach_period);
			detach_time=now;
		}
		else if (prev_detach_stat && cur_detach_stat) {
			if(now-detach_time<pdp_domain_detach_period) {
			}
			else {
				put_on_reset_modem_schedule();
				set_connection_mgr_suspend_timer(powercycle_schedule_delay+5);
				goto fini;
			}
		}
		else if(prev_detach_stat && !cur_detach_stat) {
			syslog(LOG_ERR,"[roaming] stop monitoring attach status");
			detach_time=now;
		}
		else {
			detach_time=now;
		}

		prev_detach_stat=cur_detach_stat;
#ifdef FORCED_REGISTRATION
		// handle force registration request
		if(forced_registration == 1) {
			// reset the global variable for forced registration
			forced_registration = 0;
			syslog(LOG_ERR,"[roaming] forced registration handling");
			// forced registration handling
			rescan=1;
			goto rescan;
		}
#endif
#if defined (RSSI_MONITOR_DRAFT) || defined  (REG_MONITOR_DRAFT)
		update_roaming_params(1);
#endif

#ifdef RSSI_MONITOR_DRAFT
		// RSSI monitoring function
		int rssi;

		if(registered && (stat=is_rssi_adequate(rssi_user_threshold, rscp_user_threshold, rsrp_user_threshold,&rssi,1))<=0)
	       	{
			if(last_rssi_inadequacy_time == 0)
				last_rssi_inadequacy_time = now;

			if((now-last_rssi_inadequacy_time) > rssi_adequacy_sec)
		       	{
				last_rssi_inadequacy_time=0;
				rssi_inadequate_timeout=1;
				syslog(LOG_ERR,"[roaming] SignalStrength_Monitor: Inadequacy timer expired");
			}
		}
		else
		{
			if (rssi_inadequate_timeout != 0)
				syslog(LOG_ERR,"[roaming] SignalStrength_Monitor: Inadequacy timer expiration canceled");
			last_rssi_inadequacy_time=0;
			rssi_inadequate_timeout=0;
		}

		if (rssi_inadequate_timeout == 1)
		{

			// check if online
			if((stat=send_atcmd_cgact_query())<0) {
				syslog(LOG_ERR,"failed to PDP session status");
				goto err;
			}

			// hang up if pdp session is idle online
			if(stat==0) {
				syslog(LOG_ERR,"[roaming] SignalStrength_Monitor: traffic idle detected (PDP session is down)");
				rescan=1;
				rescan_for_rssi_threshould = 1;
				goto rescan;
			}
			else {
				char inf[64];
				unsigned long long recv;
				unsigned long long sent;

				// assume it is idle
				recv=~(0ULL);
				sent=~(0ULL);


				if(get_enabled_wwan_profile_value("interface",inf,sizeof(inf))<0) {
					syslog(LOG_ERR,"[roaming] SignalStrength_Monitor: traffic idle detected (no profile is enabled)");
					rescan=1;
					rescan_for_rssi_threshould = 1;
					goto rescan;
				}
				else {
					syslog(LOG_ERR,"PDP session is up - checking idle status (%s)",inf);

					netdevusage_setDev(netusage_rssi,inf);
					if(netdevusage_getUsageChg(netusage_rssi,&recv,&sent)<0) {
						syslog(LOG_ERR,"[roaming] SignalStrength_Monitor: traffic idle detected (no interface found)");
					}
				}

				// update traffic time if any traffic is found
				if(recv || sent)
					last_rssi_traffic_time=now;

				past_traffic_sec=now-last_rssi_traffic_time;
				if(past_traffic_sec<traffic_idle_sec) {
					syslog(LOG_ERR,"[roaming] SignalStrength_Monitor: traffic busy detected - check it later");
				}
				else {
					syslog(LOG_ERR,"[roaming] SignalStrength_Monitor: traffic idle detected (%ld seconds)",past_traffic_sec);
					rescan=1;
					rescan_for_rssi_threshould = 1;
					goto rescan;
				}
			}
		}
#endif /* RSSI_MONITOR_DRAFT */

#ifdef REG_MONITOR_DRAFT
		if (creg_stat == creg_stat_not_reg || creg_stat == creg_stat_reg_denied)
	       	{
			if(last_imsi_unreg_time == 0)
				last_imsi_unreg_time = now;

			if((now-last_imsi_unreg_time) > reg_monitor_timeout_sec)
		       	{
				last_imsi_unreg_time=0;
				rescan_for_reg_monitor=1;
				rescan=1;
				syslog(LOG_ERR,"[roaming] IMSI Registration Monitor: timer expired (%d sec)", reg_monitor_timeout_sec);
				goto rescan;
			}
		}
		else if (last_imsi_unreg_time != 0)
		{
			syslog(LOG_ERR,"[roaming] IMSI Registration Monitor: timer canceled");
			last_imsi_unreg_time=0;
		}
#endif /* REG_MONITOR_DRAFT */

	}

#ifdef MODULE_TEST
	syslog(LOG_ERR,"test mode assumes we are using a non-best network");
#else
	// additional logic - attach to the network if not searching
	if(is_best_network(current_plmn)) {
		switch(creg_stat) {
			// bypass if already best - this is orignal Vodafone logic
			case creg_stat_reg_home:
			case creg_stat_reg_roam:
			case creg_stat_reg_searching:
				/* have plmn update time update-to-date all the time if best network */
				plmn_update_time=now;

				best_network_registered=1;
				goto fini;

			/*
				the following additional logic
			*/

			// reattach to the best if not searching
			case creg_stat_not_reg:
				if(best_network_registered) {
					if(attach_to_current_plmn(1)<0) {
						syslog(LOG_ERR,"[roaming] failed to register to the current network");
					}
				}
				best_network_registered=0;
				break;

			case creg_stat_reg_denied:
				syslog(LOG_ERR,"[roaming] denied from best network - rescanning");
				break;

			case -1:
				//syslog(LOG_ERR,"[roaming] current network attachment status is unknown - try later");
				goto fini;

			default:
				syslog(LOG_ERR,"[roaming] unknown creg status (%d) from best network - rescanning",creg_stat);
				break;

		 }
	}
#endif

	// get past time
	past_sec=now-plmn_update_time;

	#ifdef MODULE_TEST
	syslog(LOG_ERR,"[roaming] %ld minutes past",past_sec/60);
	#endif

	/* get best network retry time */
	best_network_time=get_best_network_retry_time();

	// rescan if it's past 30 minutes and the plmn is not a best
	if(best_network_time && (past_sec>=best_network_time)) {
		syslog(LOG_ERR,"[roaming] best network scan timeout (%ld minutes past)",past_sec/60);

		// check if online
		if((stat=send_atcmd_cgact_query())<0) {
			syslog(LOG_ERR,"failed to PDP session status");
			goto err;
		}

		// hang up if pdp session is idle online
		if(stat==0) {
			syslog(LOG_ERR,"[roaming] traffic idle detected (PDP session is down)");
			rescan=1;
		}
		else {
			char inf[64];
			unsigned long long recv;
			unsigned long long sent;

			// assume it is idle
			recv=~(0ULL);
			sent=~(0ULL);


			if(get_enabled_wwan_profile_value("interface",inf,sizeof(inf))<0) {
				syslog(LOG_ERR,"[roaming] traffic idle detected (no profile is enabled)");
				rescan=1;
			}
			else {
				syslog(LOG_ERR,"PDP session is up - checking idle status (%s)",inf);

				netdevusage_setDev(netusage,inf);
				if(netdevusage_getUsageChg(netusage,&recv,&sent)<0) {
					syslog(LOG_ERR,"[roaming] traffic idle detected (no interface found)");
				}
			}

			// update traffic time if any traffic is found
			if(recv || sent)
				last_traffic_time=now;

			past_traffic_sec=now-last_traffic_time;
			if(past_traffic_sec<traffic_idle_sec) {
				syslog(LOG_ERR,"[roaming] traffic busy detected - check %d sec. later",traffic_idle_sec);
			}
			else {
				syslog(LOG_ERR,"[roaming] traffic idle detected (%ld seconds)",past_traffic_sec);
				rescan=1;
			}
		}

	}

#if defined (RSSI_MONITOR_DRAFT) || defined  (REG_MONITOR_DRAFT)
rescan:
#endif

	// bypass if no rescan is required
	if(!rescan)
		goto fini;

#ifdef RSSI_MONITOR_DRAFT
	last_rssi_inadequacy_time=0;
	rssi_inadequate_timeout=0;
#endif

#ifdef REG_MONITOR_DRAFT
	last_imsi_unreg_time=0;
#endif
	// load params
	update_roaming_params(0);

	syslog(LOG_ERR,"[roaming] start best network retry procedure");

	// stop connection mgr
	suspend_connection_mgr();

	// set rdb for offline status
	rdb_setVal("manualroam.offline","0");

#ifdef REG_MONITOR_DRAFT
	// Don't need to make deregistered in case rescan triggered via IMSI registration monitor.
	if (rescan_for_reg_monitor != 1)
#endif
	{
		syslog(LOG_ERR,"deregistering from network - hang up PDP to scan ");
		if(deregister_from_network()<0) {
			syslog(LOG_ERR,"failed to deregister. try later");
			reset_connection_mgr_suspend_timer();
			goto err;
		}
	}

	syslog(LOG_ERR,"[roaming] scan network (current mcc=%d,mnc=%d,act=%d)", current_plmn->mcc, current_plmn->mnc,current_plmn->act);

	#if 0
	/* clear blacklist - testing purpose */
	reset_blacklist();
	#endif

	/* update roaming ui information */
	store_ui_info(NULL,NULL,NULL,NULL);
	set_manual_roam_msg_printf(rs_start_best_network,"Starting \"Best\" network retry scan...");

	while(1) {

		set_connection_mgr_suspend_timer(SUSPEND_DURATION);

		// scan network
		if( (scan_network(0,0,0,1))<0 ) {
			syslog(LOG_ERR,"network scanning not available");
			break;
		}

		// select network
		if(!(plmn=select_network_by_algorithm(0,1))) {
			int retry_selection =0;
#ifdef RSSI_MONITOR_DRAFT
			if(rescan_for_rssi_threshould == 1 ) {
				retry_selection = 1;
			}
#endif

#ifdef REG_MONITOR_DRAFT
			if(rescan_for_reg_monitor == 1 ) {
				retry_selection = 1;
			}
#endif
			if(retry_selection == 1 && !(plmn=select_network_by_algorithm(algorithm_retry_count-1,0))) {
				put_on_reset_modem_schedule();

				/* update roaming ui information */
				store_ui_info(NULL,NULL,&powercycle_scheduled_time,&powercycle_schedule_delay);
				set_manual_roam_msg_printf(rs_conn_all_network_fail,"Connection to all networks failed. See registration log. Waiting %s (mm:ss) to retry.",get_powercycle_delay());
				goto err;
			}
			break;
		}

		// check rank - bypass if it is lower (bigger number)
		if(plmn->rank>=current_plmn->rank
#ifdef RSSI_MONITOR_DRAFT
		    && rescan_for_rssi_threshould != 1
#endif
#ifdef REG_MONITOR_DRAFT
		    && rescan_for_reg_monitor != 1
#endif
		)
			break;

		// attach
		if(retry_attach_imsi_registration(plmn,network_attach_retry_count,0)<0) {
			set_black_tag_in_plmn(plmn, e_bl_reason_imsi_reg_failure);
			continue;
		}

#ifdef RSSI_MONITOR_DRAFT
		if(rescan_for_rssi_threshould == 1)
			update_roaming_params(1);
#endif

		// check rssi - reading failure? (TODO: no specific logic from Vodafone)
		if((stat=is_rssi_adequate_retry(plmn,rssi_user_threshold, rscp_user_threshold, rsrp_user_threshold,resend_csq_cmd))==0) {
			syslog(LOG_ERR,"cannot perform network registration - signal strength error");

			set_black_tag_in_plmn(plmn,e_bl_reason_signal_failure);
			continue;
		}

		// check black tag in prl
		if(stat<0) {
			set_black_tag_in_plmn(plmn,e_bl_reason_signal_failure);
			continue;
		}

		// attach pdp - try up to 2 times if it fails
		if(retry_attach_pdp_session(plmn,pdp_attach_retry_count)<0) {
			set_black_tag_in_plmn(plmn,e_bl_reason_pdp_attach_failure);
			continue;
		}

		save_plmn(plmn);
		break;
	}

	syslog(LOG_ERR,"back to the previous network");
	attach_to_current_plmn(0);

	/* get network 3g status */
	if(is_best_network(plmn) ) {
		clear_reset_modem_count();

		/* update roaming ui information */
		store_ui_info(NULL,NULL,NULL,NULL);
		set_manual_roam_msg_printf(rs_done,"Done");

		syslog(LOG_ERR,"best network retry procedure completed");
	}
	else {
		scan_period_min=get_best_network_retry_time()/60;

		if(!scan_period_min)
			syslog(LOG_ERR,"[roaming] best network retry procedure is disabled");
		else
			syslog(LOG_ERR,"[roaming] wait for %d min. to re-start best network retry procedure",get_best_network_retry_time()/60);

		/* update roaming ui information */
		store_ui_info(NULL,NULL,NULL,NULL);
		set_manual_roam_msg_printf(rs_done,"Done");
	}

	reset_connection_mgr_suspend_timer();
	// scan 30 minutes later
	plmn_update_time=now;
	return 0;

fini:
	return 0;

err:
	return -1;
}


static void check_manual_roaming_on_schedule(void* ref)
{
	if(check_reset_modem_schedule()<0) {
		retry_for_best_network();
	}

	scheduled_func_schedule("check_manual_roaming_on_schedule",check_manual_roaming_on_schedule,10);
}

static int plmn_rdb_get_int(const char* rdb);

int init_plmn()
{
	int max_delay;
	int scan_period_min;

	if((prl=rbtplmn_create())==0) {
		syslog(LOG_ERR,"failed to create prl");
		goto err;
	}

	if((netusage=netdevusage_create())==0) {
		syslog(LOG_ERR,"failed to create netusage");
		goto err;
	}

#ifdef RSSI_MONITOR_DRAFT
	if((netusage_rssi=netdevusage_create())==0) {
		syslog(LOG_ERR,"failed to create netusage_rssi");
		goto err;
	}
#endif

	// get reset max delay
	if((max_delay=plmn_rdb_get_int("manualroam.reset_max_delay"))>0) {
		powercycle_schedule_max_delay=max_delay;
		syslog(LOG_ERR,"[roaming] [back-off-delay] using user specific delay - %d sec(s)",powercycle_schedule_max_delay);
	}
	else {
		syslog(LOG_ERR,"[roaming] [back-off-delay] using default delay - %d sec(s)",powercycle_schedule_max_delay);
	}

	scan_period_min=get_best_network_retry_time()/60;

	if(!scan_period_min)
		syslog(LOG_ERR,"[roaming] best network retry procedure is disabled (init)");
	else
		syslog(LOG_ERR,"[roaming] apply best network scan period - %d min",scan_period_min);

	// limit
	if(powercycle_schedule_max_delay>powercycle_schedule_delay_limit) {
		powercycle_schedule_max_delay=powercycle_schedule_delay_limit;
		syslog(LOG_ERR,"[roaming] [back-off-delay] too big delay - use %d sec(s)",powercycle_schedule_max_delay);
	}



	scheduled_func_schedule("check_manual_roaming_on_schedule",check_manual_roaming_on_schedule,10);

	return 0;
err:
	fini_plmn();
	return -1;
}

