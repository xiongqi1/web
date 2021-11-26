/*
Simple and stupid static rule check. This needs to be more configurable.
*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/times.h>
#include <dirent.h>
#include <regex.h>

#include "rdb_ops.h"
#include "rules.h"

#define LOGINFO(...) syslog(LOG_DAEMON | LOG_INFO, __VA_ARGS__)
#define LOGERROR(...) do { syslog(LOG_DAEMON | LOG_INFO, __VA_ARGS__); printf("supervisor: " __VA_ARGS__); }while(0)

extern long tick_per_second;

/* heartbeat timeout for portmanagers */
extern int portmgr_heartbeat_timeout;

/* port manager type */
enum {
	qmmgr=0,
	atmgr,
	cnsmgr,
	main,
	mgrcount
};

/* current heart beat counts */
static int last_heart_beat[mgrcount]={-1,-1,-1,-1};
/* last heart beat update tick */
static long last_heart_beat_tick[mgrcount]={-1,-1,-1,-1};

static unsigned long uptime_secs(void)
{
	FILE *f = NULL;
	char Line[128];
	unsigned long secs = 0;
	f = fopen("/proc/uptime","r");
	fgets(Line,128,f);
	fclose(f);
	secs = strtoul(Line,NULL,10);
	return secs;
}


static int log_match(const char *LogLine, const char *match)
{
	return (LogLine && strstr(LogLine,match)!=NULL);
}

#ifdef V_WIFI_ralink
/* Returns if the ra0 network interface is present or not */
static int check_wifi_interface(int verbose)
{
	FILE *f=NULL;
	char Line[256];
	int count=0;

	f=fopen("/proc/net/dev","r");
	if (!f) return -1;
	while (fgets(Line,256,f)) {
		if (log_match(Line,"ra0:")) {
			count++;
			if (verbose) {
				LOGINFO("%s",Line);
			}
		}
	}
	fclose(f);
	return count;
}

/* Catches WiFi startup failure */
static int wifi_poll()
{
/* Timeouts in seconds */
#define WIFI_STARTUP_TIMEOUT 180
#define WIFI_TIMEOUT         50
	static unsigned long last_check = 0;

	unsigned long uptime = uptime_secs();

	if (check_wifi_interface(0)) {
		last_check = uptime;
		return RULES_OK;
	}
	if (uptime < WIFI_STARTUP_TIMEOUT)
		return RULES_OK;
	if ((uptime - last_check) < WIFI_TIMEOUT)
		return RULES_OK;

	sprintf(Exit_Reason, "WiFi module is stuck");
	return RULES_RESET;
}
#endif

#ifdef V_USB_EP0_TIMEOUT_WATHCDOG_y

/*

	USB workaround on the NTC-140wx, monitoring the kernel error messages to avoid system halt

	* Issue - USB errors by USB control message missing
	On the NTC-140wx, occasionally random USB failures occur at startup. These failures are recoverable only by router reboot.

	1. failure in reading USB description during USB enumeration procedure
	2. kernel timeout in setting USB configuration / failure of usb_control_msg()
	3. halt in accessing Kernel USB sysfs entries after USB enumeration successfully finishes
	4. extremely slow (2 minutes for phone module) enumeration of USB interfaces

	Possibly this issue belongs to Kernel 3.2 and can be resolved by the new Vanilla Linux kernel
*/

static int usb_logcheck(const char *LogLine)
{
#define MAX_USB_ERROR_FAILURE 1

	static regex_t reg;
	static regex_t reg2;
	static int reg_init=0;
	static int usb_fail_count=0;

	regmatch_t pm;
	int stat;
	int stat2;

/*
	* kernel error messages

	user.debug kernel: [  343.755798] usb 2-1.2: kworker/u:3 timed out on ep0in len=0/4
	user.debug kernel: [  0.0] usb 2-1.1: khubd timed out on ep0out len=0/0
	user.debug kernel: [  0.0] usb 2-1.2: kworker/u:0 timed out on ep0in len=0/4
	user.debug kernel: [  0.0] usb 2-1.2: kworker/u:0 timed out on ep0out len=0/0
	user.debug kernel: [  0.0] usb 2-1.2: kworker/u:1 timed out on ep0in len=0/4
	user.debug kernel: [  0.0] usb 2-1.2: kworker/u:1 timed out on ep0out len=0/0
	user.debug kernel: [  0.0] usb 2-1.2: kworker/u:2 timed out on ep0in len=0/4
	user.debug kernel: [  0.0] usb 2-1.2: kworker/u:2 timed out on ep0out len=0/0
	user.debug kernel: [  0.0] usb 2-1.2: kworker/u:3 timed out on ep0in len=0/4
	user.debug kernel: [  0.0] usb 2-1.2: kworker/u:3 timed out on ep0out len=0/0
	user.debug kernel: [  0.0] usb 2-1: khubd timed out on ep0in len=0/4
	
	user.err kernel: [ 6968.469329] hub 2-1:1.0: hub_port_status failed (err = -110)
*/

	/* compile regex */
	if(!reg_init) {
		stat=regcomp(&reg,"user\\.debug kernel: \\[ +[0-9\\.]+\\] usb [0-9\\.:-]+: khubd timed out on ep0(in|out) len=[0-9/]+",REG_EXTENDED);
		if(stat<0) {
			LOGERROR("failed in regcomp() - %s",strerror(errno));
			return RULES_OK;
		}

		stat=regcomp(&reg2,"user\\.err kernel: \\[ +[0-9\\.]+\\] hub [0-9\\.:-]+: hub_port_status failed \\(err = -110\\)",REG_EXTENDED);
		if(stat<0) {
			LOGERROR("failed in regcomp() - %s",strerror(errno));
			return RULES_OK;
		}

		reg_init=1;
	}

	/* match */
	stat=regexec(&reg,LogLine,1,&pm,0);
	stat2=regexec(&reg2,LogLine,1,&pm,0);
	if(stat!=0 && stat2!=0) {
		return RULES_OK;
	}

	/* bypass if less than error count */
	usb_fail_count++;
	if(usb_fail_count<MAX_USB_ERROR_FAILURE) {
		LOGERROR("USB control EP timeout detected #%d/%d\n",usb_fail_count,MAX_USB_ERROR_FAILURE);
		return RULES_OK;
	}

	sprintf(Exit_Reason,"%s-TO%lu","USB",uptime_secs());
	LOGERROR("USB control EP timeout detected, reboot\n");

	return RULES_RESET;
}
#endif

#ifdef V_WIFI_ralink
/* Catches wifi control channel failure */
static int wifi_logcheck(const char *LogLine)
{
#define MAX_VENDOR_FAILURE 5
	static int errcount = 0;
	if (log_match(LogLine, "RTUSB_VendorRequest failed")) {
		errcount++;
		LOGINFO("RTUSB vendor request failure %d/%d\n",errcount,MAX_VENDOR_FAILURE);
	}
	if (errcount >= MAX_VENDOR_FAILURE) {
		LOGERROR("WiFi module is stuck, reboot\n");
		sprintf(Exit_Reason, "WiFi module is stuck");
		return RULES_RESET;
	}
	return RULES_OK;
}
#endif

/*
** test function to test that supervisor is working by logging some predefined string into the log
*/
#if 0
static int test_logcheck(const char *LogLine)
{
    if (log_match(LogLine, "<<<TEST REBOOT>>>")) {
        strcpy(Exit_Reason, "Supervisor daemon test");
        return RULES_RESET;
    }
    return RULES_OK;
}
#endif

char* getPidExeName(pid_t pid)
{
	char fileName[128];
	static char exeName[256+1];
	int len;

	snprintf(fileName,sizeof(fileName),"/proc/%d/exe",pid);

	/* get symblic link */
	if( (len=readlink(fileName,exeName,sizeof(exeName)-1))<0 )
		return NULL;

	/* put zero termination */
	exeName[len]=0;

	return exeName;
}

static int isNumeric(const char* str)
{
	while(*str)
	{
		if(!isdigit(*str))
			return 0;

		str++;
	}

	return 1;
}

static int isRunning(const char* cmd)
{
	struct dirent **nameList;
	int nameCnt = scandir("/proc/.", &nameList, NULL, NULL);

	int nameIdx;

	char bin_name[256];

	int stat=0;
	int len1;
	int len2;

	char* cmdLine;
	pid_t pid;

	/* put starting slash */
	len2=snprintf(bin_name,sizeof(bin_name),"/%s",cmd);

	for(nameIdx=0;nameIdx<nameCnt;nameIdx++)
	{
		/* is numeric? otherwise bypass */
		if(!isNumeric(nameList[nameIdx]->d_name))
			continue;

		/* get pid */
		pid=atoi(nameList[nameIdx]->d_name);
		if(!pid)
			continue;

		/* get cmdline */
		cmdLine=getPidExeName(pid);
		if(!cmdLine)
			continue;

		/* bypass if short than the name */
		len1=strlen(cmdLine);
		if(len1<len2)
			continue;

		/* compare the actual binary name */
		stat=!strcmp(cmdLine+len1-len2,bin_name);

		if(stat) {
			#if 0
			syslog(LOG_ERR,"cmdLine - %s / cmd=%s / stat=%d",cmdLine,bin_name,stat); */
			#endif
			break;
		}
	}

	/* free */
	for(nameIdx=0;nameIdx<nameCnt;nameIdx++)
		free(nameList[nameIdx]);

	free(nameList);

	return stat;
}


static int get_heart_beat(char* suffix)
{
	char heart_beat_var[64];
	char heart_beat_val[64];
	int len;
	int stat;
	char* p;

	#define HEART_BEAT_MAIN	"wwan.0.heart_beat"

	/* generate heart beat rdb variable name */
	if(suffix)
		snprintf(heart_beat_var,sizeof(heart_beat_var),"%s.%s",HEART_BEAT_MAIN,suffix);
	else
		snprintf(heart_beat_var,sizeof(heart_beat_var),HEART_BEAT_MAIN);

	/* get current heart beat */
	len=sizeof(heart_beat_val);
	stat=rdb_get_single(heart_beat_var,heart_beat_val,len);
	if(stat<0)
		heart_beat_val[0]=0;

	/* get heart beat */
	len=strlen(heart_beat_val);
	p=(len<=5)?heart_beat_val:heart_beat_val+len-5;
	return atol(p);
}

void reset_mgr_heart_beat()
{
	int i;
	long curtick;
	struct tms tm;

	curtick=times(&tm)/tick_per_second;

	for(i=0;i<mgrcount;i++) {
		last_heart_beat[i]=-1;
		last_heart_beat_tick[i]=curtick;
	}
}

static int cnsmgr_control_fsm(const char* LogLine)
{
	int i;

	char* bins[mgrcount]={
		"qmimgr", /* qmmgr */
		"simple_at_manager", /* atmgr */
		"cnsmgr", /* cnsmgr */
		NULL, /* main */
	};

	int heart_beat;

	int running_stat;
	int running_stat_anymgr;

	long curtick;

	long timeout;
	char* bin;

	struct tms tm;

	/* get ms */
	curtick=times(&tm)/tick_per_second;

	running_stat_anymgr=0;
	for(i=0;i<mgrcount;i++) {

		/* check manager running status */
		if(i==main) {
			running_stat=running_stat_anymgr;
		}
		else {
			#if 0
			syslog(LOG_ERR,"bin[%d]=%s",i,bins[i]);
			#endif

			running_stat=isRunning(bins[i]);

			/* set global running status */
			running_stat_anymgr=running_stat_anymgr || running_stat;
		}

		/* get heart beat */
		heart_beat=get_heart_beat(bins[i]);

		/* get binary name */
		bin=bins[i]?bins[i]:"Port manager";

		/* update heart beat tick if increased */
		if(last_heart_beat[i]!=heart_beat)
			last_heart_beat_tick[i]=curtick;

		/* update last heart beat */
		last_heart_beat[i]=heart_beat;

		timeout=curtick-last_heart_beat_tick[i];

		/* bypass if the process is not running */
		if( !running_stat ) {
			last_heart_beat_tick[i]=curtick;
			continue;
		}

		#if 0
		syslog(LOG_ERR,"bin=%s,heart_beat=%d,timeout=%d",bins[i],(int)heart_beat,(int)timeout);
		#endif

		/* let's check to see if we can bear with that */
		if( !(timeout<portmgr_heartbeat_timeout) )
		{
			sprintf(Exit_Reason,"%s-TO%lu",bin,uptime_secs());
			LOGERROR("%s heart beat stopped (for %ld sec), reboot\n",bin,timeout);
			return RULES_RESET;
		}
	}


	return RULES_OK;
}

int rules_logcheck(const char *LogLine)
{
	int rval=RULES_OK;
#ifdef V_WIFI_ralink
	if (rval==RULES_OK) rval=wifi_logcheck(LogLine);
#endif

#ifdef V_USB_EP0_TIMEOUT_WATHCDOG_y
	if (rval==RULES_OK) rval=usb_logcheck(LogLine);
#endif

#if 0 /* enable only to test */
    if (rval==RULES_OK) rval=test_logcheck(LogLine);
#endif

	return rval;
}

int rules_poll(void)
{
	int rval=RULES_OK;
	if (rval==RULES_OK) rval=cnsmgr_control_fsm(NULL);
#ifdef V_WIFI_ralink
	if (rval==RULES_OK) rval=wifi_poll(NULL);
#endif
	return rval;
}
