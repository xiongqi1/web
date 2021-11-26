/* wwan_monitor2.c
*  Support multiple PDP sessions. (for Bovine Platform)
*/
#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <libgen.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <signal.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <syslog.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "mtd_statistics.h"

#include "cdcs_syslog.h"
#include "daemon.h"
#include "rdb_ops.h"

#define PATH_PROCNET_DEV "/proc/net/dev"
#define SLEEP_TIME 2

#define APPLICATION_NAME "wwan_monitor"
#define MAX_PROFILES 6

#define MAXRDBNAME 128
#define MAXRDBVALUE 128

struct usage wwan_usage_current[MAX_PROFILES];
struct usage wwan_usage_total[MAX_PROFILES];

char interface_name[MAX_PROFILES][16];

int link_triggered[MAX_PROFILES];
int if_up_ever[MAX_PROFILES]; //indicate whether interface has been up ever.

unsigned int restore_period = 5*60;
u_int32_t saveUsageTrigger = 5*1000000;
u_int64_t SessionReceived_Offset[MAX_PROFILES];
u_int64_t SessionSent_Offset[MAX_PROFILES];
u_int64_t SessionPacketsReceived_Offset[MAX_PROFILES];
u_int64_t SessionPacketsSent_Offset[MAX_PROFILES];
u_int64_t SessionErrorsReceived_Offset[MAX_PROFILES];
u_int64_t SessionErrorsSent_Offset[MAX_PROFILES];
u_int64_t SessionDiscardPacketsReceived_Offset[MAX_PROFILES];
u_int64_t SessionDiscardPacketsSent_Offset[MAX_PROFILES];
u_int64_t Prev_SessionReceived[MAX_PROFILES];
u_int64_t Prev_SessionSent[MAX_PROFILES];
u_int64_t Prev_SessionPacketsReceived[MAX_PROFILES];
u_int64_t Prev_SessionPacketsSent[MAX_PROFILES];
u_int64_t Prev_SessionErrorsReceived[MAX_PROFILES];
u_int64_t Prev_SessionErrorsSent[MAX_PROFILES];
u_int64_t Prev_SessionDiscardPacketsReceived[MAX_PROFILES];
u_int64_t Prev_SessionDiscardPacketsSent[MAX_PROFILES];
u_int32_t currentTime;

//unsigned int profilenum = 1;
unsigned int actionTimer = 0;
u_int64_t currentTotal[MAX_PROFILES];
u_int64_t savedUsage[MAX_PROFILES];

int max_sessions=MAX_PROFILES;


extern mtd_info_t meminfo;

char	value[HISTORY_LIMIT*64];

static struct rdb_session *s=NULL;

unsigned char net_dir[] = "/sys/class/net";
static int is_exist_interface_name(char *if_name) {
    int fd;
    char device[255];
    sprintf(device, "/sys/class/net/%s", if_name);
    fd = open(device, O_RDONLY);
    if (fd < 0)
        return 0;
    else
        close(fd);
    return 1;
}

static int openSckIfNeeded(void) {
	static int main_skfd=-1;
	// bypass if it is already open
	if(!(main_skfd<0))
		return main_skfd;

	if((main_skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		return -1;
	}
	return main_skfd;
}

static int find_interface(int idx) {
char cmd[64];
char buf[64];
	if( *interface_name[idx]==0 ) {
		sprintf(cmd, "link.profile.%u.enable", idx+1);
		*buf=0;
		rdb_get_string(s, cmd, buf, sizeof(buf) );
		if(atoi(buf)) {
			sprintf(cmd, "link.profile.%u.status", idx+1);
			sleep(2);
			rdb_get_string(s, cmd, buf, sizeof(buf) );
			if(strstr(buf, "up")) {
				sprintf(cmd, "link.profile.%u.interface", idx+1);
				sleep(2);
				rdb_get_string(s, cmd, interface_name[idx], 16);
				SYSLOG_INFO( APPLICATION_NAME " Profile[%u] interface:%s", idx+1, interface_name[idx]);
				sprintf(cmd, "link.profile.%u.trigger", idx+1);
				rdb_subscribe_signal(s, cmd, SIGUSR2);
				sprintf(cmd, "link.profile.%u.enable", idx+1);
				rdb_subscribe_signal(s, cmd, SIGUSR2);
				return 1;
			}
		}
		return -1;
	}
	P("find interface : %s", interface_name[idx]);
	return 1;
}

/*******************************************/
static void restoreStatistics(void) {
	if ( init_mtd() < 0) {
		restore_period = 5*60;
		saveUsageTrigger = 5*1000000;
	}
	else {
		if( meminfo.size > 0 ) {
			restore_period = 60;
			saveUsageTrigger = 1000000;
		}
		else {
			restore_period = 5*60;
			saveUsageTrigger = 5*1000000;
		}
	}
	P("Rest Stat : restore_period %d, saveUsageTrigger %lld", restore_period, saveUsageTrigger);
	system( "/bin/mtd_statistics -r" );
}

static void saveStatistics(int idx) {
	system( "/bin/mtd_statistics -s" ); //to each profile
	actionTimer = 0;
	savedUsage[idx] = currentTotal[idx];
	P("Save Stat : savedUsage[%d] = currentTotal[%d] = %lld", idx, idx, savedUsage[idx]);
}

static int split( char *str1, char *str2, char*myArray[] ) {
	int i;

	myArray[0]=strtok(str1, str2);
	if(!myArray[0])
		return 0;
	for( i=1; i<HISTORY_LIMIT; i++ ) {
		myArray[i]=strtok( 0, str2);
		if(!myArray[i])
			break;
	}
	return i;
}

static int init_current(int idx) {
	int ret=-1;
	char *myArray[HISTORY_LIMIT];
	char cmd[64];

	sprintf(cmd, "link.profile.%u.usage_current", idx+1);
	ret = rdb_get_string(s, cmd, value, sizeof(value) );
	//### Current session usage ###
	if( !ret && split(value, ",", myArray)==10 ) { // continue current session
		wwan_usage_current[idx].StartTime = atol(myArray[0]);	// currentSessionStartTime
		wwan_usage_current[idx].endTime = atol(myArray[1]);		//currentSessionCurrentTime
		wwan_usage_current[idx].DataReceived = atoll(myArray[2]);	//currentSessionReceived
		wwan_usage_current[idx].DataSent = atoll(myArray[3]);		//currentSessionSent
		wwan_usage_current[idx].DataPacketsReceived = atoll(myArray[4]);
		wwan_usage_current[idx].DataPacketsSent = atoll(myArray[5]);
		wwan_usage_current[idx].DataErrorsReceived = atoll(myArray[6]);
		wwan_usage_current[idx].DataErrorsSent = atoll(myArray[7]);
		wwan_usage_current[idx].DataDiscardPacketsReceived = atoll(myArray[8]);
		wwan_usage_current[idx].DataDiscardPacketsSent = atoll(myArray[9]);

		sprintf(cmd, "link.profile.%u.usage_current_startTimeInsysUpTime", idx+1);
		ret = rdb_get_string(s, cmd, value, sizeof(value) );
		//### Current session usage start time in system up time ###
		if( !ret ) {
			wwan_usage_current[idx].Start_SysUpTime = atoll(value);
		}
		else {
			wwan_usage_current[idx].Start_SysUpTime = 0;
		}
	}
	else {
		wwan_usage_current[idx].StartTime = 0;
		wwan_usage_current[idx].endTime = 0;
		wwan_usage_current[idx].DataReceived = 0;
		wwan_usage_current[idx].DataSent = 0;
		wwan_usage_current[idx].DataPacketsReceived = 0;
		wwan_usage_current[idx].DataPacketsSent = 0;
		wwan_usage_current[idx].DataErrorsReceived = 0;
		wwan_usage_current[idx].DataErrorsSent = 0;
		wwan_usage_current[idx].DataDiscardPacketsReceived = 0;
		wwan_usage_current[idx].DataDiscardPacketsSent = 0;
		wwan_usage_current[idx].Start_SysUpTime = 0;
	}
	P("init current : ST %ld, ET %ld, Rx %lld, Rx %lld, STinSysUp", wwan_usage_current[idx].StartTime, wwan_usage_current[idx].endTime,\
		wwan_usage_current[idx].DataReceived, wwan_usage_current[idx].DataSent, wwan_usage_current[idx].Start_SysUpTime);
	currentTotal[idx] = wwan_usage_current[idx].DataReceived+wwan_usage_current[idx].DataSent;
	savedUsage[idx] = currentTotal[idx];
	actionTimer = 0;
//#print( "CurrentSessionStartTime=" wwan_usage_current[idx].StartTime" Current_Session_Received=" wwan_usage_current[idx].DataReceived" Current_Session_Sent=" wwan_usage_current[idx].DataSent);
	return ret;
}

static void init_total(int idx) {
	int ret;
	char *myArray[HISTORY_LIMIT];
	char cmd[64];

	sprintf(cmd, "link.profile.%u.usage_total", idx+1);
	ret = rdb_get_string(s, cmd, value, sizeof(value) );
	//### Total usage ###
	if( !ret && split(value, ",", myArray)==10 ) {
		wwan_usage_total[idx].StartTime = atol(myArray[0]);		//totalUsageStartTime = $1;
		wwan_usage_total[idx].DataReceived = atoll(myArray[2]);	//totalDataReceived = $3+0;
		wwan_usage_total[idx].DataSent = atoll(myArray[3]);		//totalDataSent = $4+0;
		wwan_usage_total[idx].DataPacketsReceived = atoll(myArray[4]);
		wwan_usage_total[idx].DataPacketsSent = atoll(myArray[5]);
		wwan_usage_total[idx].DataErrorsReceived = atoll(myArray[6]);
		wwan_usage_total[idx].DataErrorsSent = atoll(myArray[7]);
		wwan_usage_total[idx].DataDiscardPacketsReceived = atoll(myArray[8]);
		wwan_usage_total[idx].DataDiscardPacketsSent = atoll(myArray[9]);
	}
	else {
		wwan_usage_total[idx].StartTime = currentTime;
		wwan_usage_total[idx].DataReceived = 0;
		wwan_usage_total[idx].DataSent = 0;
		wwan_usage_total[idx].DataPacketsReceived = 0;
		wwan_usage_total[idx].DataPacketsSent = 0;
		wwan_usage_total[idx].DataErrorsReceived = 0;
		wwan_usage_total[idx].DataErrorsSent = 0;
		wwan_usage_total[idx].DataDiscardPacketsReceived = 0;
		wwan_usage_total[idx].DataDiscardPacketsSent = 0;
	}
	P("init total : ST %ld, Rx %lld, Rx %lld", wwan_usage_total[idx].StartTime,\
		wwan_usage_total[idx].DataReceived, wwan_usage_total[idx].DataSent);
}

static const char *const dev_fmt = "%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu";

/* When looking up the offset for the first time since network down-up or launch appl,
 * it needs to check several times for accurate offset because without any outgoing packet,
 * Tx Bytes in /proc/net/dev changes from 0 to 1124 slowly after 3G module reboot.
 */
#define RETRY_COUNT		5
static void update_offset(int idx, int initial) {
	FILE *fp;
	u_int64_t  dummy;
        u_int64_t  SessionReceived;
        u_int64_t  SessionSent;
        u_int64_t  SessionPacketsReceived;
        u_int64_t  SessionPacketsSent;
        u_int64_t  SessionErrorsReceived;
        u_int64_t  SessionErrorsSent;
        u_int64_t  SessionDiscardPacketsReceived;
        u_int64_t  SessionDiscardPacketsSent;
	char* pos;
	int retry_count = (initial? RETRY_COUNT:1);
	int ii;

	for (ii = 0; ii < retry_count; ii++) {
		if ((fp = fopen( PATH_PROCNET_DEV, "r")) != 0) {
			while (fgets(value, sizeof(value), fp)) {
				if (interface_name[idx][0] == '\0') continue; // This interface is never up
				pos=strstr(value, interface_name[idx] );
				if( pos ) {
					P("curr rx %lld, rx off %lld, curr tx %lld, tx off %lld",\
						wwan_usage_current[idx].DataReceived, SessionReceived_Offset[idx],\
						wwan_usage_current[idx].DataSent, SessionSent_Offset[idx]);
					//*(pos+strlen(interface_name[idx]))=' ';
					sscanf(pos+strlen(interface_name[idx])+1, dev_fmt,
						&SessionReceived, &SessionPacketsReceived, &SessionErrorsReceived,
						&SessionDiscardPacketsReceived, &dummy, &dummy, &dummy, &dummy,
						&SessionSent, &SessionPacketsSent, &SessionErrorsSent,
						&SessionDiscardPacketsSent);
					SessionReceived_Offset[idx] = SessionReceived;
					SessionSent_Offset[idx] = SessionSent;
					SessionPacketsReceived_Offset[idx] = SessionPacketsReceived;
					SessionPacketsSent_Offset[idx] = SessionPacketsSent;
					SessionErrorsReceived_Offset[idx] = SessionErrorsReceived;
					SessionErrorsSent_Offset[idx] = SessionErrorsSent;
					SessionDiscardPacketsReceived_Offset[idx] = SessionDiscardPacketsReceived;
					SessionDiscardPacketsSent_Offset[idx] = SessionDiscardPacketsSent;
	//SYSLOG_INFO( "idx=%u SessionSent=%s  SessionSent-atoll=%llu", idx, SessionSent, SessionSent_Offset[idx]);
					break;
				}
			}
		}
/*		SYSLOG_ERR( "retry index [%d]: SessionReceived_Offset%u=%llu  SessionSent_Offset%u=%llu",
			ii, idx+1, SessionReceived_Offset[idx], idx+1, SessionSent_Offset[idx]);*/
		fclose(fp);
		sleep(1);
	}
}

static int check_wwan(int idx) {
	struct ifreq ifr;
	int skfd;
	int ret;
	char cmd[64];

	if(*interface_name[idx]==0) {
		if(find_interface(idx)<0) {
			return -1;
		}
		// Do not update offset here since we want to count the existing data to the current usage when the interface is first up.
		//update_offset(idx, 1);
	}
	//SYSLOG_INFO("interface%u=%s", idx, interface_name[idx]);

	sprintf(cmd, "link.profile.%u.status", idx+1);
	ret = rdb_get_string(s, cmd, value, sizeof(value) );
	if( !ret && strcmp(value, "up") ) {
		return -1;
	}

	if ( !is_exist_interface_name(interface_name[idx]) ) {
		return -1;
	}

	if((skfd = openSckIfNeeded()) < 0)
		return -1;

	strncpy(ifr.ifr_name, interface_name[idx], sizeof(ifr.ifr_name));
	if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0) {
	//SYSLOG_INFO("SIOCGIFFLAGS failed: %s\n", strerror(errno));
		return -1;
	}
	if (ifr.ifr_flags & IFF_UP) {
		if_up_ever[idx] = 1;
		return 1;
	}
	else {
		return 0;
	}
}

int update_rdb_if_chg(const char* rdb, const char* val)
{
	int stat;
	char cval[sizeof(value)];

	/* read rdb */
	stat = rdb_get_string(s, rdb, cval, sizeof(cval) );
	/* write rdb only if val is changed */
	if(stat<0 || strcmp(cval,val)) {
		stat = rdb_update_string(s, rdb, val, STATISTICS, DEFAULT_PERM);
	}

	return stat;
}

static int link_is_triggered(int idx)
{
	if (link_triggered[idx]) {
		link_triggered[idx] = 0;
		return 1;
	}
	return 0;
}

static int update_current(int idx, long sys_uptime) {
	FILE *fp;
	int ret;
	char* pos;
	u_int64_t  dummy;
        u_int64_t  SessionReceived;
        u_int64_t  SessionSent;
        u_int64_t  SessionPacketsReceived;
        u_int64_t  SessionPacketsSent;
        u_int64_t  SessionErrorsReceived;
        u_int64_t  SessionErrorsSent;
        u_int64_t  SessionDiscardPacketsReceived;
        u_int64_t  SessionDiscardPacketsSent;

	char cmd[64];
	static int if_downed[MAX_PROFILES] = {0, };
	long currTimeBySystime = 0;

	if( check_wwan(idx) <= 0 || link_is_triggered(idx)) {

		/* skip if the interface has not been up at all or if the down activity is already reported */
		if(!if_up_ever[idx] || if_downed[idx]) {
			goto skip_update_statistics;
		}

		if (if_up_ever[idx]) {
			if_downed[idx] = 1;
		}
		return 0; // wwan interface down
	}

	/* update offset again when interface down --> up */
	if (if_downed[idx]) {
		if_downed[idx] = 0;
		update_offset(idx, 1);
	}

	if ((fp = fopen( PATH_PROCNET_DEV, "r")) != 0) {
		while (fgets(value, sizeof(value), fp)) {
			pos=strstr(value, interface_name[idx] );
			if( pos ) {
				sscanf(pos+strlen(interface_name[idx])+1, dev_fmt,
					&SessionReceived, &SessionPacketsReceived, &SessionErrorsReceived,
					&SessionDiscardPacketsReceived, &dummy, &dummy, &dummy, &dummy,
					&SessionSent, &SessionPacketsSent, &SessionErrorsSent,
					&SessionDiscardPacketsSent);

				wwan_usage_current[idx].DataReceived=SessionReceived;
				wwan_usage_current[idx].DataSent=SessionSent;
				wwan_usage_current[idx].DataPacketsReceived = SessionPacketsReceived;
				wwan_usage_current[idx].DataPacketsSent = SessionPacketsSent;
				wwan_usage_current[idx].DataErrorsReceived = SessionErrorsReceived;
				wwan_usage_current[idx].DataErrorsSent = SessionErrorsSent;
				wwan_usage_current[idx].DataDiscardPacketsReceived = SessionDiscardPacketsReceived;
				wwan_usage_current[idx].DataDiscardPacketsSent = SessionDiscardPacketsSent;

				P("Rx %lld, Rxoff %lld, Tx %lld, TxOff %lld", wwan_usage_current[idx].DataReceived,\
					SessionReceived_Offset[idx],\
					wwan_usage_current[idx].DataSent,\
					SessionSent_Offset[idx]);

				/* prevent 32 bit data overflow from /proc/net/dev */
				if(wwan_usage_current[idx].DataReceived < Prev_SessionReceived[idx]) {
					SessionReceived_Offset[idx]-=0x100000000ULL;
				}
				Prev_SessionReceived[idx]=wwan_usage_current[idx].DataReceived;
				if(wwan_usage_current[idx].DataSent < Prev_SessionSent[idx]) {
					SessionSent_Offset[idx]-=0x100000000ULL;
				}
				Prev_SessionSent[idx]=wwan_usage_current[idx].DataSent;

				/*
					 The algorithm is somewhat complicated and defective. This daemon depends on the traffic accumulation in /proc/net/dev, which
					overlaps every 4 GB. To maintain more than 4 GB traffic and not to lose carriers after overlapping, in the daemon, each
					of accumulation is calculated as an offset to the value in /proc/net/dev from a virtual base called "Offset" such as SessionReceived_Offset,
					SessionSent_Offset, SessionDiscardPacketsReceived_Offset and so on. Once the daemon detects 4GB overlap in /proc/net/dev by comparing(!)
					the last measurement to the one before, it lowers down this virtual base by 4 GB. This lowered base is actually correction of overlaps. This
					virtual base is lowered down every overlap in /proc/net/dev and it is always getting further and further negative.

					The algorithm is very complicated. Why not simply maintain 64 bit accumulation by accumulating delta between measurements?

					The following *if* condition statement is commented out since it does not make sense because,

						1. the if statement compares a signed variable to an unsigned variable(!)
						2. offset (base) is a virtual bias to correct overlaps, which is always negative once overlap happens. Although, the variable itself
						   is not signed.

					** issue **
					In result, the condition is always true once a value in /proc/net/dev overlaps and the usage is not increasing.

					** solution **
					do not check wrong data usage because unfortunately, in this algorithm, there is no information to check to see if it is
					wrong data. even if base is bigger than a measured value or even if base is smaller than the measure value, it is not possible to
					tell that the base is wrong or the measured value is wrong.

				*/

				#if 0
				/* prevent negative value caculated */
				if ((int64_t)wwan_usage_current[idx].DataReceived < SessionReceived_Offset[idx] ||
					(int64_t)wwan_usage_current[idx].DataSent < SessionSent_Offset[idx]) {
					P("wrong data usage: curr rx %lld < rx off %lld? or curr tx %lld < tx off %lld?, ignore",\
						wwan_usage_current[idx].DataReceived, SessionReceived_Offset[idx],\
						wwan_usage_current[idx].DataSent, SessionSent_Offset[idx]);
					goto save_statistics_return;
				}
				#endif

				wwan_usage_current[idx].DataReceived -= SessionReceived_Offset[idx]; //currentSessionReceived
				wwan_usage_current[idx].DataSent -= SessionSent_Offset[idx];
				wwan_usage_current[idx].DataPacketsReceived -= SessionPacketsReceived_Offset[idx];
				wwan_usage_current[idx].DataPacketsSent -= SessionPacketsSent_Offset[idx];
				wwan_usage_current[idx].DataErrorsReceived -= SessionErrorsReceived_Offset[idx];
				wwan_usage_current[idx].DataErrorsSent -= SessionErrorsSent_Offset[idx];
				wwan_usage_current[idx].DataDiscardPacketsReceived -= SessionDiscardPacketsReceived_Offset[idx];
				wwan_usage_current[idx].DataDiscardPacketsSent -= SessionDiscardPacketsSent_Offset[idx];

				/* check overflow for 64 bits data usage variable */
				if (wwan_usage_current[idx].DataReceived > MAX_USAGE_LIMIT ||
					wwan_usage_current[idx].DataSent > MAX_USAGE_LIMIT) {
					P("data ovf: curr rx %lld, rx off %lld, curr tx %lld, tx off %lld",\
						wwan_usage_current[idx].DataReceived, SessionReceived_Offset[idx],\
						wwan_usage_current[idx].DataSent, SessionSent_Offset[idx]);
					goto update_history_return;
				}

				if(wwan_usage_current[idx].StartTime == 0 || wwan_usage_current[idx].Start_SysUpTime == 0) { //first time interface up
					wwan_usage_current[idx].StartTime = currentTime;
					wwan_usage_current[idx].Start_SysUpTime = sys_uptime;
				}
				else {
					currTimeBySystime = wwan_usage_current[idx].StartTime + (sys_uptime - wwan_usage_current[idx].Start_SysUpTime);
					if( (currTimeBySystime > currentTime? currTimeBySystime - currentTime : currentTime - currTimeBySystime) > 5) {
						wwan_usage_current[idx].StartTime=currentTime - (sys_uptime - wwan_usage_current[idx].Start_SysUpTime);
						P("update StartTime: idx=%d, StartTime=%ld", idx, wwan_usage_current[idx].StartTime);
					}
				}
// below condition has logical error. (In case that endTime is bigger than currentTime, below condition is always true, because two variables are unsigned type)
//				else if( (currentTime-wwan_usage_current[idx].endTime)>10000 ) {//NTP just update the time, we need correcting the start time.
//					wwan_usage_current[idx].StartTime=currentTime - (wwan_usage_current[idx].endTime-wwan_usage_current[idx].StartTime);
//				}
				wwan_usage_current[idx].endTime = currentTime; //currentSessionCurrentTime
				snprintf( value, sizeof(value), "%lu,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
					wwan_usage_current[idx].StartTime, wwan_usage_current[idx].endTime,
					wwan_usage_current[idx].DataReceived, wwan_usage_current[idx].DataSent,
					wwan_usage_current[idx].DataPacketsReceived, wwan_usage_current[idx].DataPacketsSent,
					wwan_usage_current[idx].DataErrorsReceived, wwan_usage_current[idx].DataErrorsSent,
					wwan_usage_current[idx].DataDiscardPacketsReceived, wwan_usage_current[idx].DataDiscardPacketsSent
					);
				sprintf(cmd, "link.profile.%u.usage_current", idx+1);
				P("set '%s' to '%s'", cmd, value);
				update_rdb_if_chg(cmd, value);
				currentTotal[idx] = wwan_usage_current[idx].DataReceived+wwan_usage_current[idx].DataSent;

				sprintf( value, "%lu", wwan_usage_current[idx].Start_SysUpTime);
				sprintf(cmd, "link.profile.%u.usage_current_startTimeInsysUpTime", idx+1);
				P("set '%s' to '%s'", cmd, value);
				update_rdb_if_chg(cmd, value);
				goto save_statistics_return;
			}
		}
	}
update_history_return:
	fclose(fp);
	return 0;
save_statistics_return:
	fclose(fp);
	return 1;

skip_update_statistics:
	return -1;
}

static void cleanup_history(int idx) {
	char buf[HISTORY_LIMIT*64];
	char *myArray[HISTORY_LIMIT], hist1[64], hist2[64], *ar1[11], *ar2[11];
	int i, ret;
	char cmd[64];

	sprintf(cmd, "link.profile.%u.usage_history", idx+1);
	ret = rdb_get_string(s, cmd, value, sizeof(value) );
	(void) memset(buf, 0x00, HISTORY_LIMIT*64);
	if( !ret ) {
		ret = split(value, "&", myArray);
		for(i=0; i<ret && i<HISTORY_LIMIT; i++) {
			/* remove duplicated entry from history */
			if (i > 0) {
				strcpy(hist1, myArray[i]);
				strcpy(hist2, myArray[i-1]);
				(void)split(hist1, ",", ar1);
				(void)split(hist2, ",", ar2);
				if (strcmp(myArray[i], myArray[i-1]) == 0 || strcmp(ar1[0], ar2[0]) == 0) {
					P( "==========================================================" );
					P( "found duplicated data usage in history, remove it" );
					P( "myArray[%d] %s, myArray[%d] %s", i, myArray[i], i-1, myArray[i-1] );
					P( "==========================================================" );
					continue;
				}
			}
			if (i > 0) strcat(buf, "&");
			strcat(buf, myArray[i]);
		}
	}
	P("set '%s' to '%s'", cmd, buf);
	rdb_update_string(s, cmd, buf, NONBLOCK, DEFAULT_PERM);
}

static void update_history(int idx) { /* will be called when wwan ( wwan0/ppp0/rmnet0 ) connection is down */
	char buf[HISTORY_LIMIT*64];
	char *myArray[HISTORY_LIMIT];
	char currUsage[64], lastUsage[64], *lastArray[11], hist1[64], hist2[64], *ar1[11], *ar2[11];
	int i, ret, duplicated = 0;
	char cmd[64];

	*interface_name[idx]=0;
	Prev_SessionReceived[idx]=0;
	Prev_SessionSent[idx]=0;
	update_offset(idx, 0);

	//### Updata History ###
	wwan_usage_total[idx].DataReceived += wwan_usage_current[idx].DataReceived;
	wwan_usage_total[idx].DataSent += wwan_usage_current[idx].DataSent;
	wwan_usage_total[idx].DataPacketsReceived += wwan_usage_current[idx].DataPacketsReceived;
	wwan_usage_total[idx].DataPacketsSent += wwan_usage_current[idx].DataPacketsSent;
	wwan_usage_total[idx].DataErrorsReceived += wwan_usage_current[idx].DataErrorsReceived;
	wwan_usage_total[idx].DataErrorsSent += wwan_usage_current[idx].DataErrorsSent;
	wwan_usage_total[idx].DataDiscardPacketsReceived += wwan_usage_current[idx].DataDiscardPacketsReceived;
	wwan_usage_total[idx].DataDiscardPacketsSent += wwan_usage_current[idx].DataDiscardPacketsSent;

	sprintf( value, "%lu,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
			wwan_usage_total[idx].StartTime, currentTime,
			wwan_usage_total[idx].DataReceived,
			wwan_usage_total[idx].DataSent,
			wwan_usage_total[idx].DataPacketsReceived,
			wwan_usage_total[idx].DataPacketsSent,
			wwan_usage_total[idx].DataErrorsReceived,
			wwan_usage_total[idx].DataErrorsSent,
			wwan_usage_total[idx].DataDiscardPacketsReceived,
			wwan_usage_total[idx].DataDiscardPacketsSent);
	sprintf(cmd, "link.profile.%u.usage_total", idx+1);
	P("set '%s' to '%s'", cmd, value);
	rdb_update_string(s, cmd, value, NONBLOCK, DEFAULT_PERM);
	if( wwan_usage_current[idx].StartTime ) {
		sprintf(buf,"%lu,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
				wwan_usage_current[idx].StartTime, currentTime,
				wwan_usage_current[idx].DataReceived,
				wwan_usage_current[idx].DataSent,
				wwan_usage_current[idx].DataPacketsReceived,
				wwan_usage_current[idx].DataPacketsSent,
				wwan_usage_current[idx].DataErrorsReceived,
				wwan_usage_current[idx].DataErrorsSent,
				wwan_usage_current[idx].DataDiscardPacketsReceived,
				wwan_usage_current[idx].DataDiscardPacketsSent);
	}
	else {
		*buf=0;
	}
	sprintf(cmd, "link.profile.%u.usage_history", idx+1);
	ret = rdb_get_string(s, cmd, value, sizeof(value) );
	if( !ret ) {
		ret = split(value, "&", myArray);
		/* workaround for preventing duplicated data usage which appears sometimes but
			* is very hard to replicate and fix it */
		snprintf( currUsage, sizeof(currUsage), "%lu", wwan_usage_current[idx].StartTime);
		for(i=0; i<ret && i<HISTORY_LIMIT-1; i++) {
			strcpy(lastUsage, myArray[i]);
			(void)split(lastUsage, ",", lastArray);
			/* if start time is same, then assume duplicated usage */
			if (strcmp(currUsage, lastArray[0]) == 0) {
				duplicated = 1;
				break;
			}
		}
		if (duplicated) {
			rdb_get_string(s, "statistics.usage_history", value, sizeof(value) );
			P( "==========================================================" );
			P( "found duplicated data usage in history, ignore current one" );
			P( "current : %s", buf );
			P( "history : %s", value );
			P( "==========================================================" );
			strcpy(buf, value);
		}
		else {
			for(i=0; i<ret && i<HISTORY_LIMIT; i++) {
				/* remove duplicated entry from history */
				if (i > 0) {
					strcpy(hist1, myArray[i]);
					strcpy(hist2, myArray[i-1]);
					(void)split(hist1, ",", ar1);
					(void)split(hist2, ",", ar2);
					if (strcmp(myArray[i], myArray[i-1]) == 0 || strcmp(ar1[0], ar2[0]) == 0) {
						P( "==========================================================" );
						P( "found duplicated data usage in history, remove it" );
						P( "myArray[%d] %s, myArray[%d] %s", i, myArray[i], i-1, myArray[i-1] );
						P( "==========================================================" );
						continue;
					}
				}
				if(*buf) {
					strcat(buf, "&");
				}
				strcat(buf, myArray[i]);
			}
		}
	}
	P("set '%s' to '%s'", cmd, buf);
	rdb_update_string(s, cmd, buf, NONBLOCK, DEFAULT_PERM);

	wwan_usage_current[idx].StartTime = 0;
	wwan_usage_current[idx].endTime = 0;
	wwan_usage_current[idx].DataReceived = 0;
	wwan_usage_current[idx].DataSent = 0;
	wwan_usage_current[idx].DataPacketsReceived = 0;
	wwan_usage_current[idx].DataPacketsSent = 0;
	wwan_usage_current[idx].DataErrorsReceived = 0;
	wwan_usage_current[idx].DataErrorsSent = 0;
	wwan_usage_current[idx].DataDiscardPacketsReceived = 0;
	wwan_usage_current[idx].DataDiscardPacketsSent = 0;
	saveStatistics(idx);

	sprintf(cmd, "link.profile.%u.usage_current", idx+1);
	P("set '%s' to 'wwan down'", cmd);
	update_rdb_if_chg( cmd, "wwan down");

	sprintf(cmd, "link.profile.%u.usage_current_startTimeInsysUpTime", idx+1);
	P("set '%s' to '0'", cmd);
	update_rdb_if_chg( cmd, "0");
}
/*******************************************/

int running = 1;

static void release_resources( void ) {
	unlink( "/var/lock/subsys/"APPLICATION_NAME );
}

static int init_all( int be_daemon ) {
	int ret, i;
	int ispowerup=0;
	char *myArray[HISTORY_LIMIT];
	char buf[16];
	char cmd[64];

	currentTime = (u_int32_t)time(NULL);

	for(i=0; i<MAX_PROFILES; i++) {
		P("===== init all for profile id %d ======", i+1);
		sprintf(cmd, "link.profile.%u.usage_total", i+1);
		ret = rdb_get_string(s, cmd, value, sizeof(value) );
		if( ret || !split(value, ",", myArray) ) {
			P("statistics.usage_total NOT exist, unit just powerd up, try get data from log file");
			ispowerup = 1;
			break;
		}
	}
	// restore previous usage data RDB before parsing into memory
	if( ispowerup ) {
		restoreStatistics();
	}
	for(i=0; i<MAX_PROFILES; i++) {
		find_interface(i);
		init_total(i);
		init_current(i);
		cleanup_history(i);
	}
	if( ispowerup ) {
		for(i=0; i<MAX_PROFILES; i++) {
			currentTime = wwan_usage_current[i].endTime;//currentSessionCurrentTime;
			update_history(i);
			savedUsage[i] = 0;
		}
		actionTimer = 0;
	}
	return 0;
}

static int main_loop( void ) {
unsigned int check_serviceTimer = 0;
#define CHECK_SERVICE_PERIOD	20
char buf1[8];
char buf2[8];
int i;
FILE *fp;
long sys_uptime = 0;
char buf[62];
int rc;
	actionTimer = restore_period; /*once interface is ready, save usage data at first*/
	while( running ) {
		currentTime = (u_int32_t)time(NULL);
		actionTimer += SLEEP_TIME;
		sys_uptime = 0;

		fp=fopen("/proc/uptime", "r");
		if( fp ) {
			fgets(buf, sizeof(buf), fp); 
			sys_uptime = atol(buf);
			fclose(fp);
		}
		for(i=0; i<MAX_PROFILES; i++) {
			rc = update_current(i, sys_uptime);
			if( rc == 1 ) {
				if( (currentTotal[i]-savedUsage[i]) > saveUsageTrigger || ( (actionTimer > restore_period)&&(currentTotal[i] != savedUsage[i])) ) {
					saveStatistics(i);
				}
			}
			else if ( rc == 0) {
				update_history(i);
			}
		}
		sleep(SLEEP_TIME);
	}
	return 0;
}

void display_help (void) {
	printf(	"  --- Wireless WAN Usage monitor ---\n");
	printf(	"  --- PLATFORM: Bovine (Support multiple PDP sessions) ---\n"); //Current Interface: %s -- %s\n", interface_name, check_wwan(interface_name)?"up":"down" );
	printf(	"\nUsage> " APPLICATION_NAME " [options]\n");
	printf(	"\t -h, --help \t --Display this help\n"
			//"\t -i \t\t --interface name\n"
			//"\t -d \t\t --don't detach from controlling terminal (don't daemonise)\n"
			);
}

static const struct option long_opts[] = {
	{ "help", 0, 0, 'h' },
	{ 0, 0, 0, 0 }
};


static void notify_handler(int signum)
{
	int ret, len, idx;
	char action[MAXRDBNAME];
	char value[MAXRDBVALUE];
	char *triggerName[MAX_PROFILES*2];
	char *trigStr = NULL;
	if (signum != SIGUSR2) {
		return;
	}
	ret = rdb_getnames_alloc(s, "", &trigStr, &len, TRIGGERED);
	if (ret) { // Discard the error.
		P("rdb_getnmaes_alloc error!\n");
		return;
	}
	ret = split(trigStr, "&", triggerName);
	for(len = 0; len < ret; len++) {
		if (strstr(triggerName[len], "link.profile.")) {
			if (rdb_get_string(s, triggerName[len], value, sizeof(value))) {
				P("rdb_get_string error!\n");
				continue;
			}
			if (sscanf(triggerName[len], "link.profile.%d.%s", &idx, action) == 2) {
				if (strcmp(action, "trigger") == 0 && strcmp(value, "1") == 0) { // connection is reset
					link_triggered[idx-1] = 1;
					//update_offset(idx-1, 0);
				} else if (strcmp(action, "enable") == 0) { // link is being brought down
					value[0] = '\0';
					if (strcmp(value, "0") == 0) {
						link_triggered[idx-1] = 1;
						//update_offset(idx-1, 0);
					}
				}
			}
		}
	}

	free(trigStr);
}

int main (int argc, char *argv[]) {
	int ret = 0;
	int	be_daemon = 1;
	int opt, error=0;
	int i;
	struct sigaction sa;

	SYSLOG_INFO( APPLICATION_NAME " starting..." );
	if (rdb_open(NULL, &s) < 0) {
		perror("Opening database");
		exit(-1);
	}

	sa.sa_handler = notify_handler;
	sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGUSR2, &sa, NULL);

	for(i=0; i<MAX_PROFILES; i++) {
		*interface_name[i]=0;
		find_interface(i);
		currentTotal[i]=0;
		savedUsage[i]=0;
		SessionReceived_Offset[i]=0;
		SessionSent_Offset[i]=0;
		Prev_SessionReceived[i]=0;
		Prev_SessionSent[i]=0;
	}
	if((opt=getopt_long(argc,argv,"h i: d:", long_opts, NULL)) != -1) {
		switch (opt) {
			case 0:
			case 'h':
				display_help();
				goto end;
			case 'd': be_daemon = 0; break;
			//case 'i': strcpy(interface_name, optarg);break;
			default:
				error = 1;
		}
	}
	if (error) {
		printf( "Try -h for more information.\n" );
		rdb_close(&s);
		exit(1);
	}
	ret = init_all( be_daemon );
	if( ret != 0 )
		SYSLOG_ERR( "fatal: failed to initialize" );
	else
		ret = main_loop();
end:
	release_resources();
	closelog();
	rdb_close(&s);
	exit( ret );
}
