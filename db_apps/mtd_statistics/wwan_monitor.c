/* wwan_monitor.c

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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "mtd_statistics.h"

#include "cdcs_syslog.h"
#include "daemon.h"
#include "rdb_ops.h"

#define PATH_PROCNET_DEV "/proc/net/dev"
#define SLEEP_TIME 2

#ifdef 	PLATFORM_Platypus
#define APPLICATION_NAME "usage_monitor"
#elif defined PLATFORM_Platypus2
#define APPLICATION_NAME "usage_monitor"
#else
#define APPLICATION_NAME "wwan_monitor"
#endif
struct usage wwan_usage_current;
struct usage wwan_usage_total;

char interface_name[16];

unsigned int restore_period = 5*60;
u_int32_t saveUsageTrigger = 5*1000000;
int64_t SessionReceived_Offset;
int64_t SessionSent_Offset;
u_int64_t Prev_SessionReceived;
u_int64_t Prev_SessionSent;
u_int32_t currentTime;
u_int32_t currentTimeMonotonic;

unsigned int actionTimer = 0;
u_int64_t currentTotal = 0;
u_int64_t savedUsage=0;

extern mtd_info_t meminfo;

char	value[HISTORY_LIMIT*64];

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

static int find_interface(void) {
#ifdef PLATFORM_Avian
	strcpy( interface_name, "rmnet0" );
#elif defined(PLATFORM_Platypus) && ( defined(BOARD_3g38wv) || defined(BOARD_3g38wv2) || defined(BOARD_3g46) )
	if( *interface_name==0 ) {
		rdb_get_single("link.profile.1.interface", interface_name, sizeof(interface_name) );
		if ( !is_exist_interface_name(interface_name) )
			*interface_name=0;
	}
#else
char cmd[64];
char buf[64];
int i=1;
	if( *interface_name==0 ) {
		while(1) {
			*buf=0;
			sprintf(cmd, "link.profile.%u.name", i);
			rdb_get_single( cmd, buf, sizeof(buf) );
			if(*buf==0) {
				return -1;
			}
			sprintf(cmd, "link.profile.%u.enable", i);
			*buf=0;
			rdb_get_single( cmd, buf, sizeof(buf) );
			if(atoi(buf)) {
				sprintf(cmd, "link.profile.%u.status", i);
				sleep(2);
				rdb_get_single( cmd, buf, sizeof(buf) );
				if(strstr(buf, "up")) {
					sprintf(cmd, "link.profile.%u.interface", i);
					sleep(2);
					rdb_get_single( cmd, interface_name, sizeof(interface_name) );
					SYSLOG_INFO( APPLICATION_NAME " interface:%s", interface_name);
					P("find interface : %s", interface_name);
					return 1;
				}
			}
			++i;
		}
	}
#endif
}

static int check_wwan(char *wan_name) {
	struct ifreq ifr;
	int skfd;

	if(*wan_name==0) {
		if(find_interface()<0) {
			return -1;
		}
	}
    if ( !is_exist_interface_name(wan_name) )
        return -1;

	if((skfd = openSckIfNeeded()) < 0)
		return -1;

	strncpy(ifr.ifr_name, wan_name, sizeof(ifr.ifr_name));
	if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0) {
//printf( "SIOCGIFFLAGS failed: %s\n", strerror(errno));
		return -1;
	}
	if (ifr.ifr_flags & IFF_UP)
		return 1;
	else
		return 0;
}
/*******************************************/
static void restoreStatistics(void) {
#ifdef 	PLATFORM_Platypus
	restore_period = 60;
	saveUsageTrigger = 1000000;
#else
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
#endif
	P("Rest Stat : restore_period %d, saveUsageTrigger %lld", restore_period, saveUsageTrigger);
#ifdef PLATFORM_Avian
	system( "/system/cdcs/bin/mtd_statistics -r" );
#else
	system( "/bin/mtd_statistics -r" );
#endif
}

static void saveStatistics(void) {
#ifdef PLATFORM_Avian
	system( "/system/cdcs/bin/mtd_statistics -s" );
#else
	system( "/bin/mtd_statistics -s" );
#endif
	actionTimer = 0;
	savedUsage = currentTotal;
	P("Save Stat : savedUsage = currentTotal = %lld", savedUsage);
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

static int init_current(void) {
	int ret=-1;
	char *myArray[HISTORY_LIMIT];

	ret = rdb_get_single("statistics.usage_current", value, sizeof(value) );
	//### Current session usage ###
	if( !ret && split(value, ",", myArray)==4 ) { // continue current session
		wwan_usage_current.StartTime = atol(myArray[0]);	// currentSessionStartTime
		wwan_usage_current.endTime = atol(myArray[1]);		//currentSessionCurrentTime
		wwan_usage_current.DataReceived = atoll(myArray[2]);	//currentSessionReceived
		wwan_usage_current.DataSent = atoll(myArray[3]);		//currentSessionSent
	}
	else {
		wwan_usage_current.StartTime = 0;
		wwan_usage_current.DataReceived = 0;
		wwan_usage_current.DataSent = 0;
	}
	P("init current : ST %ld, ET %ld, Rx %lld, Rx %lld", wwan_usage_current.StartTime, wwan_usage_current.endTime,\
		wwan_usage_current.DataReceived, wwan_usage_current.DataSent);
	currentTotal = wwan_usage_current.DataReceived+wwan_usage_current.DataSent;
	savedUsage = currentTotal;
	actionTimer = 0;

	wwan_usage_current.StartTimeMonotonic = currentTimeMonotonic;
	wwan_usage_current.endTimeMonotonic = currentTimeMonotonic;
	return ret;
}

static void init_total(void) {
	int ret;
	char *myArray[HISTORY_LIMIT];

	ret = rdb_get_single("statistics.usage_total", value, sizeof(value) );
	//### Total usage ###
	if( !ret && split(value, ",", myArray)==4 ) {
		wwan_usage_total.StartTime = atol(myArray[0]);		//totalUsageStartTime = $1;
		wwan_usage_total.DataReceived = atoll(myArray[2]);	//totalDataReceived = $3+0;
		wwan_usage_total.DataSent = atoll(myArray[3]);		//totalDataSent = $4+0;
	}
	else {
		wwan_usage_total.StartTime = currentTime;
		wwan_usage_total.DataReceived = 0;
		wwan_usage_total.DataSent = 0;
	}
	P("init total : ST %ld, Rx %lld, Rx %lld", wwan_usage_total.StartTime,\
		wwan_usage_total.DataReceived, wwan_usage_total.DataSent);

	wwan_usage_total.StartTimeMonotonic = currentTimeMonotonic;
}

static const char *const dev_fmt = "%s%s%s%s%s%s%s%s%s";

/* When looking up the offset for the first time since network down-up or launch appl,
 * it needs to check several times for accurate offset because without any outgoing packet,
 * Tx Bytes in /proc/net/dev changes from 0 to 1124 slowly after 3G module reboot.
 */
#define RETRY_COUNT		5
static void update_offset(int initial) {
	FILE *fp;
	char dummy[15];
	char SessionReceived[32];
	char SessionSent[32];
	char* pos;
	int retry_count = (initial? RETRY_COUNT:1);
	int ii;

	for (ii = 0; ii < retry_count; ii++) {
		if ((fp = fopen( PATH_PROCNET_DEV, "r")) != 0) {
			while (fgets(value, sizeof(value), fp)) {
				pos=strstr(value, interface_name );
				if( pos ) {
					P("update offset: %s", value);
					P("curr rx %lld, rx off %lld, curr tx %lld, tx off %lld",\
						wwan_usage_current.DataReceived, SessionReceived_Offset,\
						wwan_usage_current.DataSent, SessionSent_Offset);
					*(pos+strlen(interface_name))=' ';
					sscanf(pos+strlen(interface_name)+1, dev_fmt,
					SessionReceived, dummy, dummy, dummy, dummy, dummy, dummy, dummy, SessionSent);
					SessionReceived_Offset=atoll(SessionReceived);
					SessionSent_Offset=atoll(SessionSent);
					break;
				}
			}
		}
		SYSLOG_ERR( "retry index [%d]: SessionReceived_Offset=%llu  SessionSent_Offset=%llu",
			ii, SessionReceived_Offset, SessionSent_Offset);
		fclose(fp);
		sleep(1);
	}
}

static int update_current(void) {
	FILE *fp;
	int ret;
	char dummy[15];
	char* pos;
	char SessionReceived[32];
	char SessionSent[32];
	static int if_downed = 0;

	find_interface();

	if( check_wwan(interface_name) <= 0 ) {
		if_downed = 1;
		return 0; // wwan interface down
	}

	/* update offset again when interface down --> up */
	if (if_downed) {
		if_downed = 0;
		update_offset(1);
	}

	if ((fp = fopen( PATH_PROCNET_DEV, "r")) != 0) {
		while (fgets(value, sizeof(value), fp)) {
			pos=strstr(value, interface_name );
			if( pos ) {
				P("update_current: %s", value);
				sscanf(pos+strlen(interface_name)+1, dev_fmt,
				SessionReceived, dummy, dummy, dummy, dummy, dummy, dummy, dummy, SessionSent);
				wwan_usage_current.DataReceived=atoll(SessionReceived);
				wwan_usage_current.DataSent=atoll(SessionSent);

				P("Rx %lld, Rxoff %lld, Tx %lld, TxOff %lld", wwan_usage_current.DataReceived,\
					SessionReceived_Offset,\
					wwan_usage_current.DataSent,\
					SessionSent_Offset);

#ifndef PLATFORM_882
				/* prevent 32 bit data overflow from /proc/net/dev */
				if(wwan_usage_current.DataReceived < Prev_SessionReceived) {
					SessionReceived_Offset-=0x100000000ULL;
				}
				Prev_SessionReceived=wwan_usage_current.DataReceived;
				if(wwan_usage_current.DataSent < Prev_SessionSent) {
					SessionSent_Offset-=0x100000000ULL;
				}
				Prev_SessionSent=wwan_usage_current.DataSent;
				/* prevent negative value caculated */
				if ((int64_t)wwan_usage_current.DataReceived < SessionReceived_Offset ||
					(int64_t)wwan_usage_current.DataSent < SessionSent_Offset) {
					P("wrong data usage: curr rx %lld < rx off %lld? or curr tx %lld < tx off %lld?, ignore",\
						wwan_usage_current.DataReceived, SessionReceived_Offset,\
						wwan_usage_current.DataSent, SessionSent_Offset);
					goto save_statistics_return;
				}
				//SYSLOG_INFO( "SessionSent=%s  wwan_usage_current.DataSent=%llu\n", SessionSent, wwan_usage_current.DataSent);
				wwan_usage_current.DataReceived -= SessionReceived_Offset; //currentSessionReceived
				wwan_usage_current.DataSent -= SessionSent_Offset;
				//SYSLOG_INFO( "SessionReceived_Offset=%llu  SessionSent_Offset=%llu wwan_usage_current.DataReceived=%llu wwan_usage_current.DataSent=%llu", SessionReceived_Offset, SessionSent_Offset, wwan_usage_current.DataReceived, wwan_usage_current.DataSent);
#endif

				/* check overflow for 64 bits data usage variable */
				if (wwan_usage_current.DataReceived > MAX_USAGE_LIMIT ||
					wwan_usage_current.DataSent > MAX_USAGE_LIMIT) {
					P("data ovf: curr rx %lld, rx off %lld, curr tx %lld, tx off %lld",\
						wwan_usage_current.DataReceived, SessionReceived_Offset,\
						wwan_usage_current.DataSent, SessionSent_Offset);
					goto update_history_return;
				}

				if(wwan_usage_current.StartTime == 0) { //first time interface up
				#ifdef PLATFORM_Avian
					struct ifreq ifr;
					int skfd;

					if((skfd = openSckIfNeeded()) < 0) {
						goto save_statistics_return;
					}
					strncpy(ifr.ifr_name, interface_name, sizeof(ifr.ifr_name));
					if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0) {
						goto save_statistics_return;
					}
					if (ioctl(skfd, SIOCGIFBRDADDR, &ifr) < 0) {
						goto save_statistics_return;
					}
				#endif
					wwan_usage_current.StartTime = currentTime;
					wwan_usage_current.StartTimeMonotonic = currentTimeMonotonic;
				}
				else if( (currentTime-wwan_usage_current.endTime)>10000 ) {//NTP just update the time, we need correcting the start time.
					wwan_usage_current.StartTime=currentTime - (wwan_usage_current.endTime-wwan_usage_current.StartTime);
				}
				wwan_usage_current.endTime = currentTime; //currentSessionCurrentTime
				wwan_usage_current.endTimeMonotonic = currentTimeMonotonic;

				sprintf( value, "%lu,%lu,%llu,%llu", wwan_usage_current.StartTime, wwan_usage_current.endTime, wwan_usage_current.DataReceived, wwan_usage_current.DataSent);
				P("set 'statistics.usage_current' to '%s'", value);
				rdb_update_single("statistics.usage_current", value, NONBLOCK, DEFAULT_PERM,0,0);
				currentTotal = wwan_usage_current.DataReceived+wwan_usage_current.DataSent;

				sprintf( value, "%lu", wwan_usage_current.endTimeMonotonic - wwan_usage_current.StartTimeMonotonic);
				rdb_update_single("statistics.wanuptime", value, NONBLOCK, DEFAULT_PERM,0,0);
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
}

static void cleanup_history(void) {
	char buf[HISTORY_LIMIT*64];
	char *myArray[HISTORY_LIMIT], hist1[64], hist2[64], *ar1[5], *ar2[5];
	int i, ret;

	ret = rdb_get_single("statistics.usage_history", value, sizeof(value) );
	if( !ret ) {
		(void) memset(buf, 0x00, HISTORY_LIMIT*64);
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
	P("set 'statistics.usage_history' to '%s'", buf);
	rdb_update_single( "statistics.usage_history", buf, NONBLOCK, DEFAULT_PERM,0,0);
}

static void update_history(void) {// will be called when wwan ( wwan0/ppp0/rmnet0 ) connection is down
	char buf[HISTORY_LIMIT*64];
	char *myArray[HISTORY_LIMIT];
	char currUsage[64], lastUsage[64], *lastArray[5], hist1[64], hist2[64], *ar1[5], *ar2[5];
	int i, ret, duplicated = 0;

	*interface_name=0;
	Prev_SessionReceived=0;
	Prev_SessionSent=0;

	if( wwan_usage_current.DataReceived > 0 || wwan_usage_current.DataSent > 0 ) {
		update_offset(0);
		//### Updata History ###
		wwan_usage_total.DataReceived += wwan_usage_current.DataReceived;
		wwan_usage_total.DataSent += wwan_usage_current.DataSent;
		sprintf( value, "%lu,%lu,%llu,%llu", wwan_usage_total.StartTime, currentTime, wwan_usage_total.DataReceived, wwan_usage_total.DataSent);
		P("set 'statistics.usage_total' to '%s'", value);
		rdb_update_single( "statistics.usage_total", value, NONBLOCK, DEFAULT_PERM,0,0);

		sprintf(buf,"%lu,%lu,%llu,%llu", wwan_usage_current.StartTime, currentTime, wwan_usage_current.DataReceived, wwan_usage_current.DataSent);
		ret = rdb_get_single("statistics.usage_history", value, sizeof(value) );
		if( !ret ) {
			ret = split(value, "&", myArray);
			/* workaround for preventing duplicated data usage which appears sometimes but
			 * is very hard to replicate and fix it */
			sprintf( currUsage, "%lu", wwan_usage_current.StartTime);
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
				rdb_get_single("statistics.usage_history", value, sizeof(value) );
				P( "==========================================================" );
				P( "found duplicated data usage in history, ignore current one" );
				P( "current : %s", buf );
				P( "history : %s", value );
				P( "==========================================================" );
				strcpy(buf, value);
			} else {
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
					strcat(buf, "&");
					strcat(buf, myArray[i]);
				}
			}
		}
		P("set 'statistics.usage_history' to '%s'", buf);
		rdb_update_single( "statistics.usage_history", buf, NONBLOCK, DEFAULT_PERM,0,0);
		wwan_usage_current.StartTime = 0;
		wwan_usage_current.endTime = 0;
		wwan_usage_current.StartTimeMonotonic = currentTimeMonotonic;
		wwan_usage_current.endTimeMonotonic = currentTimeMonotonic;
		wwan_usage_current.DataReceived = 0;
		wwan_usage_current.DataSent = 0;
		saveStatistics();
		//sleep(2);
	}
	P("set 'statistics.usage_current' to 'wwan down'");
	rdb_update_single( "statistics.usage_current", "wwan down", NONBLOCK, DEFAULT_PERM,0,0);
	rdb_update_single( "statistics.wanuptime", "wwan down", NONBLOCK, DEFAULT_PERM,0,0);
}
/*******************************************/

int running = 1;

#if defined PLATFORM_Platypus
static void sig_handler( int signum ) {
	switch( signum ) {
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
			running = 0;
			break;
   }
}

static void ensure_singleton( void ) {
	const char* lockfile = "/var/lock/subsys/"APPLICATION_NAME;
	if( open( lockfile, O_RDWR | O_CREAT | O_EXCL, 0640 ) >= 0 ) { SYSLOG_INFO( "got lock on %s", lockfile ); return; }
	SYSLOG_ERR( "another instance of %s already running (because creating lock file %s failed: %s)", APPLICATION_NAME, lockfile, strerror( errno ) );
	rdb_close_db();
	exit( EXIT_FAILURE );
}
#endif

static void release_resources( void ) {
	unlink( "/var/lock/subsys/"APPLICATION_NAME );
}

/*
 * Obtain time in seconds since the system booted up.
 * Not affected by changes in local system time (such as from
 * ntp changes).
 */

static u_int32_t time_monotonic()
{
	struct timespec result;
	result.tv_sec = 0;
	result.tv_nsec = 0;
#ifndef CLOCK_MONOTONIC
	return (u_int32_t)time(0);
#else
	if (clock_gettime(CLOCK_MONOTONIC, &result) != 0) {
		/*
		 * Failed, fall back to time().
		 */
		 return (u_int32_t)time(0);
	}
#endif
	return (u_int32_t)result.tv_sec;
}

static int init_all( int be_daemon ) {
	int ret;
	int ispowerup;
	char *myArray[HISTORY_LIMIT];
	char buf[16];

#if defined PLATFORM_Platypus
	signal( SIGHUP, sig_handler );
	signal( SIGINT, sig_handler );
	signal( SIGTERM, sig_handler );
	if( be_daemon ) {
		ensure_singleton();
		daemonize( "/var/lock/subsys/" APPLICATION_NAME, "" );
		SYSLOG_DEBUG( "setting signal handlers in daemon ..." );
		signal( SIGHUP, sig_handler );
		signal( SIGINT, sig_handler );
		signal( SIGTERM, sig_handler );
	}
#endif

	currentTime = (u_int32_t)time(NULL);
	currentTimeMonotonic = time_monotonic();
	ret = rdb_get_single("statistics.usage_total", value, sizeof(value) );
	if( ret || !split(value, ",", myArray) ) {
		//# statistics.usage_total NOT exist, unit just powerd up, try get data from log file
		ispowerup = 1;
		restoreStatistics();
	}
	else {
		ispowerup = 0;
	}

//	while ( *interface_name==0 ) {
//		sleep(1);
		find_interface();
//	}
	update_offset(1);
	init_total();
	init_current();
	cleanup_history();
	if( ispowerup ) {
		currentTime = wwan_usage_current.endTime;//currentSessionCurrentTime;
		update_history();
		actionTimer = 0;
		savedUsage = 0;
	}
	return 0;
}

static int main_loop( void ) {
unsigned int check_serviceTimer = 0;
#define CHECK_SERVICE_PERIOD	20
char buf1[8];
char buf2[8];
	while( running ) {
		currentTime = (u_int32_t)time(NULL);
		currentTimeMonotonic = time_monotonic();

		if( update_current() ) {
			actionTimer += SLEEP_TIME;
			if( (currentTotal-savedUsage) > saveUsageTrigger || ( (actionTimer > restore_period)&&(currentTotal != savedUsage)) ) {
				saveStatistics();
			}
		}
		else {
			update_history();
		}
		sleep(SLEEP_TIME);
#ifdef PLATFORM_Avian
		/*********** checking periodicpingd service *********/
		check_serviceTimer += SLEEP_TIME;
		if( check_serviceTimer >= CHECK_SERVICE_PERIOD) {
			check_serviceTimer = 0;
			rdb_get_single("service.systemmonitor.periodicpingtimer", buf1, sizeof(buf1) );
			rdb_get_single("service.systemmonitor.forcereset", buf2, sizeof(buf2) );
			if( atol(buf1) || atol(buf2)) {
				system("rdb_set check_pid `/system/cdcs/bin/pidof periodicpingd`");
				rdb_get_single("check_pid", buf1, sizeof(buf1) );
				if( !atol(buf1) )
					system("rdb_set service.systemmonitor.periodicpingtimer `rdb_get service.systemmonitor.periodicpingtimer`");
			}
			system("rdb_set check_pid `/system/cdcs/bin/pidof sysmon`");
			rdb_get_single("check_pid", buf1, sizeof(buf1) );
			if( !atol(buf1) )
				system("/system/cdcs/sbin/sysmon &");
		}
#endif
	}
	return 0;
}

void display_help (void) {
	printf(	"  --- Wireless WAN Usage monitor ---\n");
#ifdef PLATFORM_Bovine
	printf(	"  --- PLATFORM: Bovine ---\n  Current Interface: %s -- %s\n", interface_name, check_wwan(interface_name)?"up":"down" );
#elif defined PLATFORM_Avian
		printf(	"  --- PLATFORM: Avian ---\n  Current Interface: %s\n", interface_name );
#elif defined PLATFORM_882
		printf(	"  --- PLATFORM: 882 ---\n  Current Interface: %s\n", interface_name );
#elif defined PLATFORM_Platypus
		printf(	"  --- PLATFORM: Platypus ---\n  Current Interface: %s\n", interface_name );
#elif defined PLATFORM_Platypus2
		printf(	"  --- PLATFORM: Platypus2 ---\n  Current Interface: %s\n", interface_name );
#else
		printf(	"  --- PLATFORM: Unknow ---\n  Current Interface: %s\n", interface_name );
#endif
	printf(	"\nUsage> " APPLICATION_NAME " [options]\n");
	printf(	"\t -h, --help \t --Display this help\n"
			"\t -i \t\t --interface name\n"
			//"\t -d \t\t --don't detach from controlling terminal (don't daemonise)\n"
			);
}

static const struct option long_opts[] = {
	{ "help", 0, 0, 'h' },
	{ 0, 0, 0, 0 }
};

int main (int argc, char *argv[]) {
	int ret = 0;
	int	be_daemon = 1;
	int opt, error=0;

	SYSLOG_INFO( APPLICATION_NAME " starting..." );
	if (rdb_open_db() < 0) {
		perror("Opening database");
		exit(-1);
	}

	*interface_name=0;
	find_interface();
	currentTotal=0;
	savedUsage=0;
	SessionReceived_Offset=0;
	SessionSent_Offset=0;
	Prev_SessionReceived=0;
	Prev_SessionSent=0;

	if((opt=getopt_long(argc,argv,"h i: d:", long_opts, NULL)) != -1) {
		switch (opt) {
			case 0:
			case 'h':
				display_help();
				goto end;
			case 'd': be_daemon = 0; break;
			case 'i': strcpy(interface_name, optarg);break;
			default:
				error = 1;
		}
	}
	if (error) {
		printf( "Try -h for more information.\n" );
		rdb_close_db();
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
	rdb_close_db();
	exit( ret );
}



