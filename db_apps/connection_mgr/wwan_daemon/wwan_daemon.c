
/*!
 * Copyright Notice:
 * Copyright (C) 2009 Call Direct Cellular Solutions Pty. Ltd.
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <linux/limits.h>
#include <nvram.h>
#include <linux/ethtool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/sockios.h>

#include <libgpio.h>

#include "logger.h"
#include "daemon.h"
#include "rdb_ops.h"
#include "rearswitch.h"
#include "ping.h"
#include "tickcount.h"
#include "passgen.h"

#include "wwan_daemon.h"

#include "../autoapn.h"

#include "tnslookup.h"


typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

#define PRCHAR(x) (((x)>=0x20&&(x)<=0x7e)?(x):'.')

#define xtod(c) ((c>='0' && c<='9') ? c-'0' : ((c>='A' && c<='F') ? \
                 c-'A'+10 : ((c>='a' && c<='f') ? c-'a'+10 : 0)))

////////////////////////////////////////////////////////////////////////////////

//#define CONFIG_PLATYPUS_NTC3G17WN

#define CONN_TYPE_3GBACKUP	"wanBackup"
#define CONN_TYPE_ALWAYS_ON	"AlwaysOn"
#define CONN_TYPE_ONDEMAND	"OnDemand"

//#define DEBUG_FAILOVER

static int _f3GBackupEn = 0;
static int _fPingHostDown = 0;

int	_verbosity = 0;

// -----> platypus variant selection <-----
#if defined(BOARD_3g17wn)
static int _gpioRadioOnOff=-1;
static int _gpioBatteryLow=-1;
static int _gpioBatteryAC=-1;
static int _gpioBatteryCharge=-1;
static int _raethWanLED=-1;
#elif defined(BOARD_3g18wn)
static int _gpioRadioOnOff=13;
static int _gpioBatteryLow=-1;
static int _gpioBatteryAC=-1;
static int _gpioBatteryCharge=-1;
static int _raethWanLED=-1;
#elif defined(BOARD_3g8wv) || defined(BOARD_3g38wv) || defined(BOARD_3g38wv2) || defined(BOARD_3g36wv) || defined(BOARD_3g39w) || defined(BOARD_3g46)
static int _gpioRadioOnOff=-1;
static int _gpioBatteryLow=-1;
static int _gpioBatteryAC=-1;
static int _gpioBatteryCharge=-1;

	#if defined(SKIN_au) && defined(BOARD_3g8wv)
		static int _raethWanLED=1;
	#else
		static int _raethWanLED=-1;
	#endif

#elif defined(BOARD_3gt1wn)
static int _gpioRadioOnOff=-1;
static int _gpioBatteryLow=9;
static int _gpioBatteryAC=10;
static int _gpioBatteryCharge=14;
static int _raethWanLED=-1;
#elif defined(BOARD_3gt1wn2)
static int _gpioRadioOnOff=-1;
static int _gpioBatteryLow=9;
static int _gpioBatteryAC=10;
static int _gpioBatteryCharge=14;
static int _raethWanLED=-1;
#elif defined(BOARD_3g34wn)
#warning "Battery state GPIOs are not yet known for 3g34wn"
static int _gpioRadioOnOff=-1;
static int _gpioBatteryLow=-1;
static int _gpioBatteryAC=-1;
static int _gpioBatteryCharge=-1;
static int _raethWanLED=-1;
#elif defined(BOARD_3g23wn)
static int _gpioRadioOnOff=-1;
static int _gpioBatteryLow=-1;
static int _gpioBatteryAC=-1;
static int _gpioBatteryCharge=-1;
static int _raethWanLED=-1;
#elif defined(BOARD_3g23wnl)
static int _gpioRadioOnOff=-1;
static int _gpioBatteryLow=-1;
static int _gpioBatteryAC=-1;
static int _gpioBatteryCharge=-1;
static int _raethWanLED=-1;
#elif defined(BOARD_testbox)
static int _gpioRadioOnOff=13;
static int _gpioBatteryLow=-1;
static int _gpioBatteryAC=-1;
static int _gpioBatteryCharge=-1;
static int _raethWanLED=-1;
#else
#error BOARD_BOARD not specified - grep "platypus variant selection" to add a new variant
#endif
							
#ifdef USE_3G_FAILOVER_NOTIFICATION
#define FAILOVER_NOTI_DELAY 15
int send_failover_notification = 0;
int send_failover_countdown=FAILOVER_NOTI_DELAY;
int send_failback_countdown=FAILOVER_NOTI_DELAY;
int send_failback_notification =0;
static long long failover_rx_usage = 0;
static long long failover_tx_usage = 0;
#endif
							
// currently, VT is only locked
#if defined(MEPLOCK_router)
	int _meplock=1;
#elif defined(MEPLOCK_module)
	int _meplock=0;
#elif defined(MEPLOCK_none)
	int _meplock=0;
#else
#error V_MEPLOCK V variable not defined	
#endif


#define CDCSDB_BATTERY_LOW				"battery.low"
#define CDCSDB_BATTERY_AC					"battery.ac_power"
#define CDCSDB_BATTERY_CHARGING		"battery.charging"
#define CONNECTION_DELAY 60


////////////////////////////////////////////////////////////////////////////
struct dial_info {
	char achApn[64];
	char achDial[64];
	char achUn[64];
	char achPw[64];
	char achAuth[16];
};

static int apnscan_pass=0;


void stop_wwan(void);
int is_ppp_connected(void);

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

clock_t __getTicksPerSecond(void)
{
	return sysconf(_SC_CLK_TCK);
}
clock_t __getTickCount(void)
{
	struct tms tm;

	return times(&tm);
}

/* Verison Information */
#define VER_MJ		0
#define VER_MN		1
#define VER_BLD	1

/* Local data */
#define MAX_CONNECTIONS	8
#define INACTIVE	-1

/* Known Vendor ID's */
#define VID_SIERRA_WIRELESS	0x1199



enum if_type
{
	PPP,
	USBNET
};

struct _glb
{
	int				run;
	int				sck;
	char				wwan_mode[256];
	int				wwan_timer;
	enum	if_type	wwan_if_type;
	int				wwan_device_present;
	int				wwan_vendor_id;
	int				wwan_supervisor_pid;
	int				pppd_pid;

	int				pppd_running;

	int				cTermCnt;
	int				fWentOnline;
	int				connection_delay;
	int				req_term_conn;

	int cPingHost;
	char szPingHost[2][128];
	int iPingHost;

	int pingTimer;
	int pingAccTimer;
	int pingFailCnt;
	int pingSuccCnt;


}glb;

#define DAEMON_NAME "wwan_daemon"

/* The user under which to run */
#define RUN_AS_USER "system"

const char shortopts[] = "dvV?s:p:";

int _fRearRadioOff = 0;

#define MEPINFO_DIR 		"/etc_rw/mepinfo"
#define MEPINFO_MCCMNC		MEPINFO_DIR "/mncmcc.dat"
#define MEPINFO_MEPUNLOCK	MEPINFO_DIR "/mepunlock.key"
#define MEPINFO_MAC		MEPINFO_DIR "/mac.txt"

#define MAC_ADDRESS_LENGTH	(12*3)
#define UNLOCK_SEED_SUFFIX	"_NetworkUnlockCode"
#define MCCMNC_SEED_SUFFIX	"_NetCommMCCMNC"
#define UNLOCK_CODE_LENGTH	10

// mep unlock related variables
static char unlock_code[UNLOCK_CODE_LENGTH+1]={0,};
FILE* fp_file=0;
static FILE* fp_popen=0;
int module_locked_up=0;
int mep_in_process=1;


void usage(char **argv)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "%s [-p] [-d] [-v] \n", argv[0]);

	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");

	fprintf(stderr, "\t-d               \t Do not detach from controlling terminal (do not daemonise)\n");
	fprintf(stderr, "\t-v               \t Increases _verbosity\n");
	fprintf(stderr, "\t-V               \t Display version information\n");

	fprintf(stderr, "\t-p <gpio number> \t Specifies rear push button gpio number\n");
	fprintf(stderr, "\t-s <gpio number> \t Specifies rear radio on/off switch gpio number\n");

	fprintf(stderr, "\nGPIO information:\n");
	fprintf(stderr, "\tPlatypus 3G18Wn - push button = 12 / radio on off = 13\n");
	fprintf(stderr, "\tPlatypus 3G17Wn - front button = 0 / rear radio on off = 10 [default]\n");

	fprintf(stderr, "\n");
}

void wwand_shutdown(int n)
{
	log_INFO("exiting");
	close(glb.sck);
	rdb_close_db();
	closelog();
	exit(n);
}

static void sig_handler(int signum)
{
	pid_t	pid;

	int stat;

	switch (signum)
	{
		case SIGHUP:
			log_INFO("Caught Sig SIGHUP");
			glb.req_term_conn=1;
			//rdb_import_config (config_file_name, TRUE);
			break;

		case SIGTERM:
			log_DEBUG("SIGTERM detected");
			glb.run = 0;

			break;

		case SIGCHLD:
			if ((pid = waitpid(-1, &stat, WNOHANG)) >0)
			{
				log_DEBUG("Child %d terminated", pid);
				if (pid == glb.pppd_pid)
				{
					glb.pppd_pid = 0;
					rdb_set_single("wwan.0.module.lock", "0");

					notify_conn_term();
				}
				else if (pid == glb.wwan_supervisor_pid)
				{
					glb.wwan_supervisor_pid = 0;
					log_DEBUG("Supervisor terminated\n");
				}
				else
					log_DEBUG("Unknown Process Terminated\n");
			}
			break;
			
		default:
			log_INFO("Caught Sig %d", signum);
			break;
			
	}
}

int start_wwan_supervisor(char *mgr)
{
	system(mgr);
	return 1;
}

#define DB_SCRIPT_VARIABLE						"wwan.0.script"								// assume that wwan is always 0

int is_exist_interface_name(char *if_name)
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

int getConnScript(char* achBuf, int cbBuf)
{
	// get script from database
	if (rdb_get_single(DB_SCRIPT_VARIABLE, achBuf, cbBuf) < 0)
	{
		syslog(LOG_ERR, "fail to get database variable(%s) - %s", DB_SCRIPT_VARIABLE, strerror(errno));
		return -1;
	}

	return 0;
}

int start_wwan_interface(void)
{
	int profile;
	int instance;
	char cmd[128];

	int  iArgVal;
	char aArgVal[3][256];
	char* args[4];

	char achStartScript[PATH_MAX];

	profile = 1;
	instance = 0;

	// get script from database
	if (getConnScript(achStartScript, sizeof(achStartScript)) < 0)
		goto error;

	pid_t pid = fork();

	if (pid == 0) // Child
	{
		// Restore default signal handlers for children
		signal(SIGHUP, SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		/* Redirect standard files to /dev/null */
		freopen("/dev/null", "r", stdin);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);

		/* Create a new SID for the child process */
		int sid = setsid();
		if (sid < 0)
		{
			syslog(LOG_ERR, "unable to create a new session, code %d (%s)",
			       errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		snprintf(cmd, sizeof(cmd), "%s %d %d", achStartScript, profile, instance);

		log_INFO("Starting %s\n", cmd);

		iArgVal = 0;
		sprintf(aArgVal[iArgVal++], "%s", achStartScript);
		sprintf(aArgVal[iArgVal++], "%d", profile);
		sprintf(aArgVal[iArgVal++], "%d", instance);

		args[0] = aArgVal[0];
		args[1] = aArgVal[1];
		args[2] = aArgVal[2];
		args[3] = NULL;

		// launch
		if (execv(aArgVal[0], args) < 0)
		{
			log_ERR("failed to launch script %s", cmd);
			_exit(0);
		}

		log_INFO("Finished %s\n", cmd);
		_exit(0);
	}
	else if (pid > 0) // Parent
	{
		glb.wwan_supervisor_pid = pid;
		log_DEBUG("PPPD pid = %d\n", pid);
		//glb.active_connections[connection].pid = pid;
	}
	else //Error
	{
		log_ERR("can't fork, error %d (%s)\n", errno, strerror(errno));
	}

	return pid;

error:
	return 0;
}

void init_glb(void)
{
	glb.wwan_mode[0] = 0;

	glb.cPingHost=0;
	glb.iPingHost=0;
	memset(glb.szPingHost,0,sizeof(glb.szPingHost));

	glb.wwan_device_present = -1;
	glb.pppd_pid = 0;
	glb.wwan_vendor_id = 0;
	glb.wwan_supervisor_pid = 0;

	/* Get a socket handle. */
	/* We re-use this socket handle over and over, we don't need to allocate one each time */

	glb.sck = socket(AF_INET, SOCK_DGRAM, 0);
	if (glb.sck < 0)
	{
		log_ERR("socket: %s", strerror(errno));
		return;
	}
}

int get_net_interface(char *if_name)
{
	struct ifreq	ifr;

	if ( !is_exist_interface_name(if_name) )
	{
		return 0;
	}

	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	if (ioctl(glb.sck, SIOCGIFINDEX, &ifr) < 0)
	{
		//log_INFO ("ioctl(SIOCGIFNAME): %s", strerror (errno));
		return 0;
	}
	//log_DEBUG ("if_index = %d", ifr.ifr_ifindex);

	return ifr.ifr_ifindex;
}

int get_net_interface_state(char *if_name)
{
	struct ifreq	ifr;

	if ( !is_exist_interface_name(if_name) )
	{
		return 0;
	}

	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	if (ioctl(glb.sck, SIOCGIFFLAGS, &ifr) < 0)
	{
		log_INFO("ioctl(SIOCGIFFLAGS): %s", strerror(errno));
		return 0;
	}
	//log_DEBUG ("if_flags = %04x", ifr.ifr_flags);

	if (ifr.ifr_flags & IFF_UP)
	{
		return 1;
	}
	return 0;
}


int get_wwan_parameters(void)
{
	char* szWwanMode = nvram_get(RT2860_NVRAM, "wwan_opmode");
	char* szPingHost = nvram_get(RT2860_NVRAM, "internetHost");

	if (szWwanMode && strlen(szWwanMode)==0){
		nvram_strfree(szWwanMode);
		return 0;
	}
	
	// wwan mode
	glb.wwan_mode[0] = 0;
	if (szWwanMode)
		strcpy(glb.wwan_mode, szWwanMode);
	nvram_strfree(szWwanMode);

	// init host info
	glb.cPingHost=0;
	memset(glb.szPingHost,0,sizeof(glb.szPingHost));

	// ping host
	if (strlen(szPingHost))
		strcpy(glb.szPingHost[glb.cPingHost++], szPingHost);
	nvram_strfree(szPingHost);


	// advanced 3g backup fail-over ping
	{
		char* internetHost2;
		char* foptimer;
		char* fopacctimer;
		char* fopfailcnt;
		char* fopsucccnt;

		// get host2
		internetHost2=nvram_get(RT2860_NVRAM, "internetHost2");
		if(strlen(internetHost2))
			strcpy(glb.szPingHost[glb.cPingHost++], internetHost2);

		// fail-over timer
		foptimer=nvram_get(RT2860_NVRAM, "foptimer");
		glb.pingTimer=atoi(foptimer);

		// fail-over ping acc. timer
		fopacctimer=nvram_get(RT2860_NVRAM, "fopacctimer");
		glb.pingAccTimer=atoi(fopacctimer);

		// correct input error
		if(!glb.pingAccTimer)
			glb.pingAccTimer=glb.pingTimer;
		if(!glb.pingTimer)
			glb.pingTimer=glb.pingAccTimer;

		// fail-over ping fail count
		fopfailcnt=nvram_get(RT2860_NVRAM, "fopfailCnt");
		glb.pingFailCnt=atoi(fopfailcnt);
		
		fopsucccnt=nvram_get(RT2860_NVRAM, "fopsuccCnt");
		if(fopsucccnt[0])
			glb.pingSuccCnt=atoi(fopsucccnt);
		else
			glb.pingSuccCnt=5;
		
		// old style
		if(!strlen(foptimer) && !strlen(fopacctimer) && !strlen(fopfailcnt) )
		{
			glb.pingTimer=PING_TO3G_NORMAL_FREQ;
			glb.pingAccTimer=PING_TO3G_NORMAL_FREQ;
			glb.pingFailCnt=PING_TO3G_TOTAL_COUNT;
		};

		#ifdef DEBUG_FAILOVER
		syslog(LOG_DEBUG,"[3g-backup] hostcnt=%d,timer=%d,acctimer=%d,failcnt=%d",glb.cPingHost,glb.pingTimer,glb.pingAccTimer,glb.pingFailCnt);
		syslog(LOG_DEBUG,"[3g-backup] host1=%s",glb.szPingHost[0]);
		syslog(LOG_DEBUG,"[3g-backup] host2=%s",glb.szPingHost[1]);
		#endif		

		nvram_strfree(internetHost2);
		nvram_strfree(foptimer);
		nvram_strfree(fopacctimer);
		nvram_strfree(fopfailcnt);
		nvram_strfree(fopsucccnt);
	}


	{
		char* szOpTime = nvram_get(RT2860_NVRAM, "wwan_optime");
		glb.wwan_timer = atoi(szOpTime);
		nvram_strfree(szOpTime);
	}

	// Interface type is PPP unless a usb0 interface exists
	glb.wwan_if_type = PPP;
	if (get_net_interface("usb0"))
		glb.wwan_if_type = USBNET;

	//log_DEBUG("WWAN Mode = %s, Timer = %d, type = %s", glb.wwan_mode, glb.wwan_timer,
	//	glb.wwan_if_type == PPP ? "PPP" : "USBNET");

	return 0;
}

int wwan_device_check_pdpstat_capability(void)
{
	char achPdpStat[32];
	if( rdb_get_single("wwan.0.system_network_status.pdp0_stat",achPdpStat,sizeof(achPdpStat)) <0)
		return 0;

	if(!strlen(achPdpStat))
		return 0;

	return !0;
}

int wwan_device_is_pdpup(void)
{
	char achPdpStat[32];
	if( rdb_get_single("wwan.0.system_network_status.pdp0_stat",achPdpStat,sizeof(achPdpStat)) <0)
		return 0;

	return !strcmp(achPdpStat,"up") || !strcmp(achPdpStat,"1");
}


int wwan_device_check_attach_capability(void)
{
	char achAttached[32];

	if (rdb_get_single("wwan.0.system_network_status.attached", achAttached, sizeof(achAttached)) < 0)
		return 0;

	if(!strlen(achAttached))
		return 0;

	return !0;
}

int wwan_device_is_attached(void)
{
	char achAttached[32];

	if (rdb_get_single("wwan.0.system_network_status.attached", achAttached, sizeof(achAttached)) < 0)
		return 0;

	return atoi(achAttached);
}

int wwan_device_is_sim_ok(void)
{
	char achSimStat[64];
	char achSignal[64];

	// read sim status
	if (rdb_get_single("wwan.0.sim.status.status", achSimStat, sizeof(achSimStat)) < 0)
		return 0;

	// bypass if not okay
	if(strcmp(achSimStat, "SIM OK"))
		return 0;

	// read signal strength
	if (rdb_get_single("wwan.0.radio.information.signal_strength", achSignal, sizeof(achSignal)) < 0)
		return 0;

	return strlen(achSignal);
}

int wwan_device_is_present(void)
{
	FILE *fp;

	if (0 == (fp = fopen("/sys/bus/usb/devices/1-1/idVendor", "r")))
		return 0;

	if (fscanf(fp, "%x", &glb.wwan_vendor_id) != 1)
		glb.wwan_vendor_id = 0;

	fclose(fp);
	return 1;
}

int wwan_interface_is_up(void)
{
	if (glb.pppd_pid > 0)
		return 1;

	return 0;
}

#define DATA_ROAMING_DB_NAME	"roaming.data.en"
#define RDB_ROAMING_STATUS 		"wwan.0.system_network_status.roaming"
#define RDB_VALUE_SIZE_SMALL	128
static int roaming_call_blocked(void)
{
	char* value = alloca(RDB_VALUE_SIZE_SMALL);
	char *default_roaming = "0";
#if defined(PLATFORM_PLATYPUS)
	char* db_str;
	int result;
#endif

	if (rdb_get_single(RDB_ROAMING_STATUS, value, RDB_VALUE_SIZE_SMALL) != 0 ||
		*value == 0 || strcmp(value, "active") != 0)
	{
		return 0;
	}

#if defined(PLATFORM_PLATYPUS)
	db_str = nvram_get(RT2860_NVRAM, DATA_ROAMING_DB_NAME);
	//log_DEBUG("read NV item : %s : %s\n", DATA_ROAMING_DB_NAME, db_str);
	if (!db_str || !strlen(db_str))
	{
		log_DEBUG("Voice roaming variable is not defined. Set to default %s\n", default_roaming);
		nvram_strfree(db_str);
		result = nvram_bufset(RT2860_NVRAM, DATA_ROAMING_DB_NAME, default_roaming);
		if (result < 0)
		{
			log_ERR("write NV item failure: %s : %s\n", DATA_ROAMING_DB_NAME, default_roaming);
			return (strcmp(default_roaming, "0") == 0);
		}
		result = nvram_commit(RT2860_NVRAM);
		if (result < 0)
			log_ERR("commit NV items failure\n");
		return (strcmp(default_roaming, "0") == 0);
	}
	return (strcmp(db_str, "0") == 0);
#else
	if( rdb_get_single( DATA_ROAMING_DB_NAME, value, RDB_VALUE_SIZE_SMALL ) != 0)
	{
		log_ERR( "failed to read '%s'\n", DATA_ROAMING_DB_NAME );
		if (rdb_create_variable(DATA_ROAMING_DB_NAME, default_roaming, CREATE, ALL_PERM, 0, 0) != 0)
		{
			log_ERR("failed to create '%s' (%s)\n", DATA_ROAMING_DB_NAME, strerror(errno));
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
	if( rdb_get_single( DATA_ROAMING_BLOCKED_DB_NAME, value, RDB_VALUE_SIZE_SMALL ) != 0)
	{
		//log_ERR( "failed to read '%s'\n", VOICE_ROAMING_BLOCKED_DB_NAME );
		if (rdb_create_variable(DATA_ROAMING_BLOCKED_DB_NAME, "0", CREATE, ALL_PERM, 0, 0) != 0)
		{
			log_ERR("failed to create '%s' (%s)\n", DATA_ROAMING_BLOCKED_DB_NAME, strerror(errno));
		}
	}
	rdb_set_single( DATA_ROAMING_BLOCKED_DB_NAME, (blocked? "1":"0") );
}

int should_start_wwan(void)
{
	// start if 3g backup need
	int fStartBackup3G = _f3GBackupEn && _fPingHostDown;

	if (roaming_call_blocked())
	{
		//log_ERR("data call is blocked in roaming area\n");
		mark_data_roaming_call_blocked(1);
		return 0;
	}
	mark_data_roaming_call_blocked(0);

	if (fStartBackup3G)
		return 1;

	if (!strcmp(glb.wwan_mode, CONN_TYPE_ALWAYS_ON) || !strcmp(glb.wwan_mode, CONN_TYPE_ONDEMAND))
		return 1;

	return 0;
}

int apnIsAutoAPN()
{
	char* szAutoAPN=nvram_get(RT2860_NVRAM, "wwan_AutoAPN");

	int fAutoAPN=!szAutoAPN || strcmp(szAutoAPN,"0");

	nvram_strfree(szAutoAPN);

	return fAutoAPN;
}

int should_stop_wwan(void)
{
	int auto_apn=apnIsAutoAPN();
	
	if (roaming_call_blocked())
	{
		//log_ERR("stop current data call in roaming area\n");
		mark_data_roaming_call_blocked(1);
		return 1;
	}
	mark_data_roaming_call_blocked(0);

	if(!auto_apn) 
	{
		// init autoscan
		apnscan_pass=0;
		init_scan_apn(1,"");
	}
	
	if (!wwan_interface_is_up())
		return 0;

	// start if 3g backup need
	int fStartBackup3G = (_f3GBackupEn && _fPingHostDown);
	if (fStartBackup3G)
		return 0;

	if (!strcmp(glb.wwan_mode, CONN_TYPE_ALWAYS_ON) || !strcmp(glb.wwan_mode, CONN_TYPE_ONDEMAND))
		return 0;

	return 1;
}

int is_wwan_up(void)
{
	return glb.fWentOnline;
}

void start_wwan(void)
{
	log_INFO("Starting WWAN");

	glb.pppd_running=1;

	glb.pppd_pid = start_wwan_interface();
	glb.cTermCnt = 0;
	glb.req_term_conn = 0;
	glb.connection_delay = CONNECTION_DELAY;
	glb.fWentOnline = 0;
	log_DEBUG("PPPD pid = %d", glb.pppd_pid);
}

int is_ppp_connected(void)
{
	return glb.pppd_running;
}

int is_pppd_running(void)
{
	return glb.pppd_pid > 0;
}

void stop_wwan(void)
{
	log_INFO("Stoping WWAN");

	glb.pppd_running=0;

	if (glb.pppd_pid > 0)
	{
		if(glb.cTermCnt>5)
			kill(glb.pppd_pid, SIGKILL);
		else
			kill(glb.pppd_pid, SIGTERM);

		glb.cTermCnt++;
		
		// paranoid finishing - terminate all the 3G interface together
		system("ifconfig ppp1 down; ifconfig usb0 down;ifconfig lte0 down;ifconfig hso0 down;");
	}

	// disable connection profile
	rdb_set_single("link.profile.1.enable","0");
	
}

static rearswitch* _pR = NULL;

static rearswitch* _pBatteryLow = NULL;
static rearswitch* _pBatteryAC = NULL;
static rearswitch* _pBatteryCharge = NULL;

void fini_rearswitch()
{
	rearswitch_destroy(_pR);
	rearswitch_destroy(_pBatteryAC);
	rearswitch_destroy(_pBatteryLow);
}

//int checkFailOverStatus()
//{
//
//eth2.2
//
//service.systemmonitor.pingacceleratedtimer 101
//service.systemmonitor.destaddress 10.0.0.1
//service.systemmonitor.forcereset
//service.systemmonitor.destaddress2 10.0.0.2
//service.systemmonitor.failcount 102
//service.systemmonitor.periodicpingtimer 100


////////////////////////////////////////////////////////////////////////////////
int isIfUp(const char* szIfNm, int iPort)
{
	struct ifreq ifr;
	static int sck=-1;

	int hProc=-1;

	if ( !is_exist_interface_name((char *)szIfNm) )
	{
		return 0;
	}

	// open socket
	if(sck<0)
	{
		if( (sck = socket(AF_INET, SOCK_DGRAM,0))<0 )
		{
			syslog(LOG_ERR, "failed to open SOCK_DGRAM socket");
			goto error;
		}
	}

	hProc=open("/proc/rt2880/gmac",O_WRONLY|O_NOCTTY);
	if(hProc<0)
	{
		syslog(LOG_ERR, "failed to open gamc - %s",strerror(errno));
		goto error;
	}

	char achPort[10];
	snprintf(achPort,sizeof(achPort),"%d",iPort);
	achPort[sizeof(achPort)-1]=0;

	write(hProc,achPort,strlen(achPort));

	// build ethtool
	struct ethtool_value edata;
	edata.cmd = ETHTOOL_GLINK;

	// build ifr
	bzero(&ifr,sizeof(ifr));
	ifr.ifr_data = (caddr_t)&edata;

	char szRIfNm[64];
	strcpy(szRIfNm,szIfNm);
	char* pDot=strchr(szRIfNm,'.');
	if(pDot)
		pDot=0;

	strcpy(ifr.ifr_name, szRIfNm);

	// issue SIOCETHTOOL
	if(ioctl(sck, SIOCETHTOOL, &ifr)<0)
	{
		syslog(LOG_ERR, "failed to query SIOCETHTOOL - %s",strerror(errno));
		goto error;
	}

	close(hProc);

	return edata.data ? 1:0;

error:
	if(hProc>=0)
		close(hProc);

	return -1;
}
////////////////////////////////////////////////////////////////////////////////
int isPPTP()
{
	char* szWanMode=nvram_get(RT2860_NVRAM, "wanConnectionMode");
	int matched;

	matched=!strcmp(szWanMode,"PPTP");

	nvram_strfree(szWanMode);

	return matched;
}
////////////////////////////////////////////////////////////////////////////////
const char* getWanIfName()
{
	static char achWanIfName[64];

	char* szWanMode=nvram_get(RT2860_NVRAM, "wanConnectionMode");

	// TODO: we need to compare more strings to figure out what interface name is
	if(!strcmp(szWanMode,"PPPOE"))
		strcpy(achWanIfName,"ppp0");
	else
		strcpy(achWanIfName,"eth2.2");

	nvram_strfree(szWanMode);

	return achWanIfName;
}

////////////////////////////////////////////////////////////////////////////////
int init_rearswitch()
{

	// radio on-off switch - pin 13
	_pR=rearswitch_create_readpin(_gpioRadioOnOff);
	// create object for battery low
	_pBatteryLow=rearswitch_create_readpin(_gpioBatteryLow);
	// create object for battery low
	_pBatteryAC=rearswitch_create_readpin(_gpioBatteryAC);
	// create object for battery charge
	_pBatteryCharge=rearswitch_create_readpin(_gpioBatteryCharge);

	return 0;
}

static int _cFailCount = 0;
static int _cSuccCount = 0;

static int _cShiftCnt = 0;

static int _pingSent = 0;

static tick tickLastPing=0;


void resetCounters()
{
	_cFailCount=0;
	_cSuccCount=0;

	_cShiftCnt=0;
	
	tickLastPing=0;
	_pingSent=0;

	glb.iPingHost=0;
}


#define DNS_SERVER_LEN	128
#define DNS_SERVER_MAX	2

static int dns_server_cnt=0;
static char dns_servers[DNS_SERVER_MAX][DNS_SERVER_LEN];

int updateDnsServers()
{
	int i;
	int j;	
	char buf[128];
	
	for(i=0,j=0;i<DNS_SERVER_MAX;i++) {
		snprintf(buf,sizeof(buf),"wan.dns%d",i+1);
				
		if( rdb_get_single(buf,dns_servers[j],DNS_SERVER_LEN) < 0) {
			#ifdef DEBUG_FAILOVER			
			syslog(LOG_INFO,"failed to get primary dns server (%s) - %s",buf,strerror(errno));
			#endif			
		}
		else {
			j++;
		}
	}
	
	// update dns server count
	dns_server_cnt=j;
	#ifdef DEBUG_FAILOVER			
	syslog(LOG_INFO,"total dns server nubmer = %d",dns_server_cnt);
	#endif	
	
	if(!dns_server_cnt) {
		#ifdef DEBUG_FAILOVER			
		syslog(LOG_INFO,"no dns server found");
		#endif		
		return -1;
	}
	
	// copy primary to secondary
	if(dns_server_cnt<DNS_SERVER_MAX) {
		for(i=dns_server_cnt;i<DNS_SERVER_MAX;i++)
			strcpy(dns_servers[i],dns_servers[0]);
	}
	dns_server_cnt=DNS_SERVER_MAX;
	
	// list
/*	
	for(i=0;i<DNS_SERVER_MAX;i++) {
		syslog(LOG_INFO,"dns server %d - %s",i,dns_servers[i]);
	}
*/	
	
	return 0;
}

int checkHostDownByNslookup()
{
	int fPingHostDown=_fPingHostDown;
	
	enum {failover_start, failover_send_query, failover_wait, failover_check_resp, failover_wait_30sec, failover_stop};
	
	static int stage=failover_start;
	static int dns_server_idx=0;
	const char* cur_dns_server;
	
	tick cur_tick;
	static tick start_tick;
	static tick wait_period;
	static int next_stage;
	
	// get dns server
	cur_dns_server=dns_servers[dns_server_idx];
	cur_tick=getTickCountMS();
	
	switch(stage) {
		case failover_stop:
			break;
			
		case failover_start:
			syslog(LOG_INFO,"nslookup-based failover started");
			stage=failover_send_query;
			break;
			
		case failover_send_query:
			syslog(LOG_INFO,"send dns query - %s",cur_dns_server);
			
			// set dns server
			tnslookup_change_dns_server(cur_dns_server);
			
			// send query
			tnslookup_send(glb.szPingHost[0]);
			
			// wait for 10 sec - unconditional wait
			syslog(LOG_INFO,"wait for 10 sec");
			
			start_tick=cur_tick;
			next_stage=failover_check_resp;
			wait_period=10*1000;
			
			stage=failover_wait;
			
			break;
			
		case failover_wait:
			if(cur_tick-start_tick<wait_period) {
				break;
			}
			
			syslog(LOG_INFO,"%d sec timeout",(int)(wait_period/1000));
			stage=next_stage;
			break;
			
		case failover_check_resp: {
			int succ;
			
			#ifdef DEBUG_FAILOVER			
			syslog(LOG_INFO,"checking nslookup result");
			#endif			
			succ=tnslookup_isready() && (tnslookup_recv()>=0);
			
			if(succ) {
				_cSuccCount++;
				_cFailCount=0;
				syslog(LOG_INFO,"nslookup query succ");
			}
			else {
				_cSuccCount=0;
				_cFailCount++;
				syslog(LOG_INFO,"nslookup query failed");
			}
			
			// if nslookup fails while using WAN
			if(fPingHostDown) {
				if(succ) {
					if(_cSuccCount<glb.pingSuccCnt) {
						syslog(LOG_INFO,"nslookup succ (3G connection) %d/%d time(s) - server=%s",_cSuccCount,glb.pingSuccCnt,cur_dns_server);
						
						syslog(LOG_INFO,"wait for 30 sec");
						// wait for 30 sec
						start_tick=cur_tick;
						next_stage=failover_send_query;
						wait_period=30*1000;
		
						stage=failover_wait;
					}
					else {
						fPingHostDown=0;
						
						syslog(LOG_INFO,"## WAN recovered");
						stage=failover_send_query;
					}
				}
				else {
					// try next server
					dns_server_idx++;
					if(dns_server_idx>=dns_server_cnt) {
						int shift;
						
						syslog(LOG_INFO,"switching to the other server");
						
						// get shift
						shift=(_cFailCount/DNS_SERVER_MAX)-1;
						if(shift>3)
							shift=3;
						
						dns_server_idx=0;
						
						// wait for 30 sec
						start_tick=cur_tick;
						next_stage=failover_send_query;
						wait_period=30*1000*(1<<shift);
		
						stage=failover_wait;
						
						syslog(LOG_INFO,"waiting for %d sec",(int)(wait_period/1000));
					}
					else {
						syslog(LOG_INFO,"trying the dns server again");
						stage=failover_send_query;
					}
				}
			}
			else {
				if(succ) {
					#ifdef DEBUG_FAILOVER			
					syslog(LOG_INFO,"nslookup succ (WAN connection)");
					#endif					
					
					//dns_server_idx=0;
					
					// wait for 30 sec
					syslog(LOG_INFO,"wait for 30 sec");
					
					start_tick=cur_tick;
					next_stage=failover_send_query;
					wait_period=30*1000;
			
					stage=failover_wait;
				}
				else {
					syslog(LOG_INFO,"nslookup fail (WAN connection) %d/%d time(s) - server=%s",_cFailCount,glb.pingFailCnt,cur_dns_server);
					
					stage=failover_send_query;
					
					if(glb.pingFailCnt<=_cFailCount) {
						syslog(LOG_INFO,"switching to the other server");
					
						_cFailCount=0;
						
						// try next server
						dns_server_idx++;
						if(dns_server_idx>=dns_server_cnt) {
							syslog(LOG_INFO,"### WAN down");
									
							fPingHostDown=1;
							dns_server_idx=0;
							
							syslog(LOG_INFO,"wait for 30 sec");
									
							// wait for 30 sec
							start_tick=cur_tick;
							next_stage=failover_send_query;
							wait_period=30*1000;
			
							stage=failover_wait;
						}
					}
				}
			}
			break;
		}
	}
	
	return fPingHostDown;
}

int checkHostDown()
{
	static int totalRecvPing=0;
	int fPingHostDown=_fPingHostDown;

	// get current tick
	tick tickCur = getTickCountMS();

	// get current host
	const char* host;
	
	glb.iPingHost=glb.iPingHost%glb.cPingHost;
	host=glb.szPingHost[glb.iPingHost];

	// get current timer
	int freq;
	if( (_cFailCount && !fPingHostDown) || (_cSuccCount && fPingHostDown) )
	{
		freq=glb.pingAccTimer;
	}
	else
	{
		freq=glb.pingTimer;
	}

	int firstPing=(tickLastPing==0);
	int pingTime=firstPing || !(tickCur-tickLastPing<freq*1000);

	// recv ping until empty
	int recv;
	while( (recv=ping_recv()) > 0 )
	{
		totalRecvPing++;
		#ifdef DEBUG_FAILOVER			
		syslog(LOG_DEBUG,"[3g-backup] ping replied");
		#endif		
	}

	// send consecutive pings
	if(pingTime)
	{
		int i;
		for(i=0;i<PING_CONSECUTIVE_COUNT;i++)
			ping_sendto(host,PING_NSLOOKUP_TIMEOUT);
		tickLastPing=tickCur;

		_pingSent=!0;

		#ifdef DEBUG_FAILOVER			
		syslog(LOG_DEBUG,"[3g-backup] ping to host - %s",host);
		#endif		
	}

	// if any ping recived or timeout
	if( _pingSent && pingTime && !firstPing )
	{
		_pingSent=0;
	
		#ifdef DEBUG_FAILOVER			
		syslog(LOG_DEBUG,"[3g-backup] delay expired - timeout=%d, totalRecvPing=%d",freq,totalRecvPing);
		#endif		
	
		if(fPingHostDown)
		{
			if(totalRecvPing)
			{
				syslog(LOG_INFO,"[3g-backup] consecutive succ %d",_cSuccCount+1);
				_cSuccCount++;
				_cFailCount=0;
				_cShiftCnt=0;
			}
			else
			{
/*
				if(_cSuccCount)
					syslog(LOG_DEBUG,"[3g-backup] ping fail - reset count");
				else
					syslog(LOG_INFO,"[3g-backup] ping fail - host=%s, failtcnt=%d",host,_cFailCount+1);
*/	
				_cSuccCount=0;
				
				// let's try other host
				_cFailCount++;
				if(!(_cFailCount<glb.pingFailCnt))
				{
					glb.iPingHost=(glb.iPingHost+1)%glb.cPingHost;
	
					#ifdef DEBUG_FAILOVER			
					syslog(LOG_DEBUG,"[3g-backup] switch to a new server - %s",glb.szPingHost[glb.iPingHost]);
					#endif					
					_cFailCount=0;
					tickLastPing=0;
				}
			}
	
			if(!(_cSuccCount<glb.pingFailCnt))
			{
				fPingHostDown=0;
				syslog(LOG_DEBUG,"[3g-backup] wan connection up");
			}
		}
		else
		{
			if(!totalRecvPing)
			{
				#ifdef DEBUG_FAILOVER			
				syslog(LOG_INFO,"[3g-backup] consecutive failure %d",_cFailCount+1);
				#endif				
				_cFailCount++;
			}
			else
			{
				#ifdef DEBUG_FAILOVER			
				syslog(LOG_INFO,"[3g-backup] ping succ");
				#endif				
				_cFailCount=0;
			}
	
			if(!(_cFailCount<glb.pingFailCnt))
			{
				syslog(LOG_DEBUG,"[3g-backup] host down - %s",host);
					
				glb.iPingHost++;
	
				// switch to a new server if available
				if(glb.iPingHost<glb.cPingHost)
				{
					#ifdef DEBUG_FAILOVER			
					syslog(LOG_DEBUG,"[3g-backup] switch to a new server - %s",glb.szPingHost[glb.iPingHost]);
					#endif					
					_cFailCount=0;
					tickLastPing=0;
				}
				else
				{
					syslog(LOG_DEBUG,"[3g-backup] all hosts down");
					fPingHostDown=1;
				}
			}
		}
	
		totalRecvPing=0;
	}

	return fPingHostDown;
}

///////////////////////////////////////////////////////////////////
int getXmlTagVal(char *mybuf, char *mytag, char *tagval, int taglen, char *myval, char *valval, int vallen)
{
	char *pos = strstr(mybuf, mytag);
	char *val;

	if (!pos) return 0;
	pos += strlen(mytag);
	while (*pos)
	{
		if (*pos++ == '=')
		{
			if (*pos++ == '"')
			{
				val = pos;
				while (*pos++ != 0)
				{
					if (*pos == '"')
					{
						*pos = 0;
						strncpy(tagval, val, taglen);
						if (!vallen || !valval || !myval) return 1;
						pos = strstr(pos + 1, myval);
						if (!pos) return 0;
						pos += strlen(myval);
						strncpy(valval, pos, vallen + 2);
						while (*valval++)
						{
							if (*valval == '"')
							{
								*valval = 0;
								return 1;
							}
							else if (!isdigit(*valval) && !(*valval == ',' || *valval == ' '))
								return 0;
						}
						return 0;
					}
				}
			}
		}
		else if (*pos > ' ') return 0;
	}
	return 0;
}

char *getXmlNodeVal(char *mybuf, char *checkval, char *nodeval, int len)
{
	char *pos = strstr(mybuf, checkval);
	if (pos)
	{
		strncpy(nodeval, pos + strlen(checkval), len + 1);
		pos = nodeval;
		while (*pos)
		{
			if (*pos == '<' && *(pos + 1) == '/')
			{
				*pos = 0;
				return nodeval;
			}
			pos++;
		}
	}
	return 0;
}

int checkMNC(char *mncStr, int mnc)
{
	char *pos;

	while (*mncStr)
	{
		if (mnc == atoi(mncStr)) return 1;
		pos = (strstr(mncStr, ","));
		if (pos)
			mncStr = pos + 1;
		else
			return 0;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////
int getXmlInnerStr(const char* szLine,char* szVar,char* achBuf,int cbBuf)
{
	const char* szTag=strstr(szLine,szVar);

	if(!szTag)
		return -1;

	int nVarLen;
	nVarLen=strlen(szVar);

	const char* szInner;
	szInner=szTag+nVarLen;

	int iBuf=0;
	while(*szInner && *szInner!='<' && iBuf<(cbBuf-1))
		achBuf[iBuf++]=*szInner++;

	achBuf[iBuf]=0;

	return iBuf;
}

////////////////////////////////////////////////////////////////////////////
int getXmlStr(const char* szLine,char* szVar,char* achBuf,int cbBuf)
{
	const char* pS;
	int iBuf=0;

	int cVar=strlen(szVar);
	const char* pN=szLine;

	while( (pN=strstr(pN,szVar))!=0 )
	{
		pN+=cVar;
		pS=pN;

		// skip space
		while(*pS && isspace(*pS))
				pS++;

		if(!*pS)
			break;

		// is equal?
		if(*pS++ !='=')
			continue;

		// skip space
		while(*pS && isspace(*pS))
				pS++;

		if(!*pS)
			break;

		// is quotation?
		if(*pS++ != '"')
				continue;

		// copy until quotation
		iBuf=0;
		while(*pS && (*pS != '"') && (iBuf<cbBuf-1) )
			achBuf[iBuf++]=*pS++;

		achBuf[iBuf]=0;
	}

	return iBuf;
}
////////////////////////////////////////////////////////////////////////////
int rdb_set_single_create(const char* name,const char* value)
{
	int stat;

	stat=rdb_set_single(name,value);
	if(stat<0)
		stat=rdb_create_variable(name, value, CREATE, DEFAULT_PERM, 0, 0);

	return stat;
}

#define DB_WWAN_APN			"wwan_APN"
#define DB_WWAN_STATUS	"wwan_APN_status"
	#define DB_WWAN_STATUS_ERR	"Not supported"
	#define DB_WWAN_STATUS_OK		"Supported"
	#define DB_WWAN_STATUS_NA		"Not available"

#define DB_WWAN_DIAL			"wwan_dial"
	#define DB_WWAN_DIAL_DEF	"*99#"

#define DB_WWAN_USER			"wwan_user"
#define DB_WWAN_PASS			"wwan_pass"
#define DB_WWAN_AUTH			"wwan_auth"


////////////////////////////////////////////////////////////////////////////
int apnSetAutoAPN(struct dial_info* pDialInfo)
{

	if(pDialInfo)
	{
		rdb_set_single_create(DB_WWAN_APN,pDialInfo->achApn);
		rdb_set_single_create(DB_WWAN_DIAL,pDialInfo->achDial);
		rdb_set_single_create(DB_WWAN_USER,pDialInfo->achUn);
		rdb_set_single_create(DB_WWAN_PASS,pDialInfo->achPw);
		rdb_set_single_create(DB_WWAN_AUTH,pDialInfo->achAuth);
	}
	else
	{
		rdb_set_single_create(DB_WWAN_APN,"");
		rdb_set_single_create(DB_WWAN_DIAL,"");
		rdb_set_single_create(DB_WWAN_USER,"");
		rdb_set_single_create(DB_WWAN_PASS,"");
		rdb_set_single_create(DB_WWAN_AUTH,"");
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////
int apnGetNumber(const char* szVariable)
{
	char achValue[64];

	if(rdb_get_single(szVariable,achValue,sizeof(achValue))<0)
		return -1;

	return atoi(achValue);
}
////////////////////////////////////////////////////////////////////////////
int apnGetMCC()
{
	return apnGetNumber("wwan.0.imsi.plmn_mcc");
}
////////////////////////////////////////////////////////////////////////////
int apnGetMNC()
{
	return apnGetNumber("wwan.0.imsi.plmn_mnc");
}

////////////////////////////////////////////////////////////////////////////
char* safe_strncpy(char* dest,const char* src,size_t cnt)
{
	char* stat=strncpy(dest,src,cnt);

	if(cnt>0)
		dest[cnt-1]=0;

	return stat;
}

////////////////////////////////////////////////////////////////////////////
int apnGetList(int nMcc,int nMnc,struct dial_info* pDialInfo,int cList)
{
	FILE* pF;

	const char* szXml="/etc_ro/www/internet/apnList.xml";

	int iList=0;

	memset(pDialInfo,0,sizeof(*pDialInfo)*cList);

	// open xml
	pF=fopen(szXml,"r");
	if(!pF)
		goto error;

	int nCurMcc=-1;
	int nCurMnc=-1;

	int fTouched=0;

	char achLine[1024];
	while(fgets(achLine,sizeof(achLine),pF)!=0)
	{
		// get country
		if(strstr(achLine,"<Country"))
		{
			char achMcc[64];
			if(getXmlStr(achLine,"mcc",achMcc,sizeof(achMcc))>0)
			{
				nCurMcc=atoi(achMcc);
				nCurMnc=-1;
			}
		}

		// get carrier
		if(strstr(achLine,"<Carrier"))
		{
			char achMnc[64];
			if(getXmlStr(achLine,"mnc",achMnc,sizeof(achMnc))>0)
				nCurMnc=atoi(achMnc);
		}

		if(iList>=cList)
			break;

		if((nMcc!=nCurMcc) || (nCurMnc!=nMnc))
			continue;

		// get apn
		if(strstr(achLine,"<APN"))
		{
			char achApn[sizeof(pDialInfo[iList].achApn)];

			if(getXmlStr(achLine,"apn",achApn,sizeof(achApn))>=0)
			{
				if(fTouched)
					iList++;

				// reset
				memset(&pDialInfo[iList],0,sizeof(pDialInfo[iList]));

				strcpy(pDialInfo[iList].achApn,achApn);
				strcpy(pDialInfo[iList].achDial,DB_WWAN_DIAL_DEF);

				fTouched=1;
			}
		}
		// username
		else if (getXmlInnerStr(achLine,"<UserName>",pDialInfo[iList].achUn,sizeof(pDialInfo[iList].achUn))>=0)
			fTouched=1;
		// password
		else if (getXmlInnerStr(achLine,"<Password>",pDialInfo[iList].achPw,sizeof(pDialInfo[iList].achPw))>=0)
			fTouched=1;
		// dial
		else if (getXmlInnerStr(achLine,"<Dial>",pDialInfo[iList].achDial,sizeof(pDialInfo[iList].achDial))>=0)
			fTouched=1;
		// auth
		else if (getXmlInnerStr(achLine,"<Auth>",pDialInfo[iList].achAuth,sizeof(pDialInfo[iList].achAuth))>=0)
			fTouched=1;
	}

	if(fTouched)
		iList++;

	fclose(pF);

	return iList;


error:
	if(pF)
		fclose(pF);

	return -1;
}

int check_apn()
{
	FILE *fp = 0;
	char buf[128];
	char currentmcc[6], mcc[6], mnc[128], apn[32], buff[32];
	int imnc;
	char *myapn = 0;
	char country[64], carrier[64];
	int status = 0;

	// get user apn
	myapn = nvram_bufget(RT2860_NVRAM, "wwan_APN");
	if (strlen(myapn))
	{
		rdb_set_single("wwan_APN", myapn);
		goto fini;
	}

	// open
	const char* szXml="/etc_ro/www/internet/apnList.xml";
	fp = fopen(szXml, "r");
	if (!fp)
	{
		syslog(LOG_ERR, "failed to open file %s",szXml);
		goto err;
	}

	// get MCC
	//if (rdb_get_single("wwan.0.system_network_status.MCC", currentmcc, sizeof(currentmcc)) < 0)
	if (rdb_get_single("wwan.0.imsi.plmn_mcc", currentmcc, sizeof(currentmcc)) < 0)
	{
		syslog(LOG_ERR, "failed to get MCC");
		goto err;
	}

	// get MNC
	//if (rdb_get_single("wwan.0.system_network_status.MNC", buf, sizeof(buf)) < 0)
	if (rdb_get_single("wwan.0.imsi.plmn_mnc", buf, sizeof(buf)) < 0)
	{
		syslog(LOG_ERR, "failed to get MNC");
		goto err;
	}

	//syslog(LOG_ERR, "information MCC=%s,MNC=%s",currentmcc,buf);

	imnc = atoi(buf);

	while (1)
	{
		// get a line xml
		if (!fgets(buf, sizeof(buf), fp))
		{
			break;
		}

		switch (status)
		{
			case 0:
				if (strstr(buf, "<Country "))
				{
					if (getXmlTagVal(buf + 8, "country", country, 63, "mcc=\"", mcc, 3))
					{
						if (strcmp(currentmcc, mcc) == 0)
						{
							status++;
							break;
						}
					}
				}
				break;

			case 1:
				if (strstr(buf, "<Carrier "))
				{
					if (getXmlTagVal(buf + 8, "carrier", carrier, 63, "mnc=\"", mnc, 127))
					{
						if (checkMNC(mnc, imnc))
						{

							status++;
							break;
						}
					}
				}
				else if (strstr(buf, "</Country>"))
				{
					status = 0;
					break;
				}
				break;

			case 2:
				if (strstr(buf, "<APN "))
				{
					if (getXmlTagVal(buf + 4, "apn", apn, 31, NULL, NULL, 0))
					{

						status++;
						break;
					}
				}
				else if (strstr(buf, "</Carrier>"))
				{
					status = 1;
					break;
				}
				break;

			case 3:
				if (getXmlNodeVal(buf, "<Provider>", buff, 31))
				{
					if (rdb_get_single("wwan.0.service_provider_name", buf, sizeof(buf)) < 0)
						strcpy(buf,"");

					if (strcmp(buff, buf) == 0)
					{
						if(rdb_create_variable("wwan_APN", apn, CREATE, DEFAULT_PERM, 0, 0)<0)
						{
							if(rdb_set_single("wwan_APN", apn)<0)
								syslog(LOG_ERR, "failed to set wwan_APN");
						}

						syslog(LOG_INFO, "apn detected - %s",apn);
						goto fini;
					}
					else
					{
						status = 2;
						break;
					}
				}
				else if (strstr(buf, "<UserName>") || strstr(buf, "</APN>"))
				{
					if(rdb_create_variable("wwan_APN", apn, CREATE, DEFAULT_PERM, 0, 0)<0)
					{
						if(rdb_set_single("wwan_APN", apn)<0)
							syslog(LOG_ERR, "failed to set wwan_APN");
					}

					syslog(LOG_INFO, "apn detected - %s",apn);
					goto fini;
				}
				else
				{
					syslog(LOG_DEBUG, "apnList.xml format error");
					goto err;
				}
				break;

			default:
				break;
		}
	}


fini:
	if(fp)
		fclose(fp);

	if(myapn)
		free(myapn);

	return 0;

err:
	if(fp)
		fclose(fp);

	if(myapn)
		free(myapn);

	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int echoToFile(const char* baseDir,const char* fileName,const char* msgStr)
{
	char fullName[PATH_MAX];
	FILE* fp;
	int fullNameLen;

	fullName[0]=0;

	// add baseDir
	if(baseDir)
		strcat(fullName,baseDir);

	// add slash
	fullNameLen=strlen(fullName);
	if(fullNameLen && (fullName[fullNameLen-1]!='/'))
		strcat(fullName,"/");

	// get full file name
	strcat(fullName,fileName);

	// open
	fp=fopen(fullName,"r+");
	if(!fp)
		return -1;

	// put with cr
	fputs(msgStr,fp);
	fputs("\n",fp);

	// close
	fclose(fp);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
void batteryUpdateDbVariable(const char* db_variable,int db_value)
{
	char db_new_value[64];
	int db_old_int_value;

	// get old value
	if( rdb_get_single_int(db_variable,&db_old_int_value)<0 )
		db_old_int_value=-1;

	if(db_value!=db_old_int_value)
	{
		sprintf(db_new_value,"%d",db_value);
		if(rdb_set_single(db_variable,db_new_value)<0)
		{
			if(rdb_create_variable(db_variable,db_new_value,CREATE, DEFAULT_PERM, 0,0)<0)
				syslog(LOG_ERR, "rdb_set_single failed - %s",strerror(errno));
		}
	}
}
////////////////////////////////////////////////////////////////////////////////
int batteryUpdateStatus()
{
	int pin_stat;


	// low battery
	if(_pBatteryLow)
	{
		if(!rearswitch_readPin(_pBatteryLow))
			pin_stat=0;
		else
			pin_stat=1;

		batteryUpdateDbVariable(CDCSDB_BATTERY_LOW,pin_stat);
	}

	// battery ac
	if(_pBatteryAC)
	{
		if(!rearswitch_readPin(_pBatteryAC))
			pin_stat=1;
		else
			pin_stat=0;

		batteryUpdateDbVariable(CDCSDB_BATTERY_AC,pin_stat);
	}

	// battery charge
	if(_pBatteryAC)
	{
		if(!rearswitch_readPin(_pBatteryCharge))
			pin_stat=0;
		else
			pin_stat=1;

		batteryUpdateDbVariable(CDCSDB_BATTERY_CHARGING,pin_stat);
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
// update endpoints
int wwanUpdateEndPoints()
{
	#define USB_LEDS				"/proc/usb_cdcs_leds"
	#define VARIABLE_LENGTH 128

	static char epList[2][VARIABLE_LENGTH]={{0,},{0,}};
	char newEpList[2][VARIABLE_LENGTH];

	int epChg;

	// get end point information
	if(rdb_get_single("module.led.ep1",newEpList[0],VARIABLE_LENGTH)<0)
		newEpList[0][0]=0;
	if(rdb_get_single("module.led.ep2",newEpList[1],VARIABLE_LENGTH)<0)
		newEpList[1][0]=0;

	// if changed
	epChg=strcmp(newEpList[0],epList[0]) || strcmp(newEpList[1],epList[1]);

	if(epChg)
	{
		// delete old
		if(strlen(epList[0]))
			echoToFile(USB_LEDS,"delete",epList[0]);
		if(strlen(epList[1]))
			echoToFile(USB_LEDS,"delete",epList[1]);

		// store new ones
		strcpy(epList[0],newEpList[0]);
		strcpy(epList[1],newEpList[1]);

		// add
		if(strlen(epList[0]))
			echoToFile(USB_LEDS,"add",epList[0]);
		if(strlen(epList[1]))
			echoToFile(USB_LEDS,"add",epList[1]);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// assing connection led
int wwanTurnLed(const char* ledName,int enable)
{
	#define VARIABLE_LENGTH 128
	char wanLed[VARIABLE_LENGTH];
	char led_test_mode[64];
	
	// override led for factory mode
	if (rdb_get_single("factory.led_test_mode", led_test_mode, sizeof(led_test_mode)) == 0) {
		if(atoi(led_test_mode)) {
			syslog(LOG_ERR, "led_test_mode mode  - led control bypassed");
			return 0;
		}
	}

	
	// get led base name for connection led
	sprintf(wanLed,"/sys/class/leds/%s",ledName);

	if(enable>0)
	{
		echoToFile(wanLed,"trigger","cdcs-wwan");

		echoToFile(wanLed,"brightness_on","0");
		echoToFile(wanLed,"brightness_off","255");

		echoToFile(wanLed,"brightness","255");

		echoToFile(wanLed,"force","trigger");
	}
	else if(enable==0)
	{
		//echoToFile(wanLed,"trigger","cdcs-wwan");
		echoToFile(wanLed,"trigger","none");

		echoToFile(wanLed,"brightness_on","255");
		echoToFile(wanLed,"brightness_off","0");

		echoToFile(wanLed,"brightness","0");

		echoToFile(wanLed,"force","trigger");
	}
	else
	{
		echoToFile(wanLed,"trigger","none");

		echoToFile(wanLed,"brightness_on","0");
		echoToFile(wanLed,"brightness_off","255");
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Set led 'name' to brightness 'value'
int LedCtrl(const char *name, unsigned value)
{
	int fd;
	#define NAME_LENGTH 128
	char ledstr[NAME_LENGTH];
	int len;

	snprintf(ledstr,NAME_LENGTH,"/sys/class/leds/%s/brightness",name);
	ledstr[NAME_LENGTH-1]='\0';

	fd = open(ledstr,O_WRONLY);
	if (fd < 0) {
		syslog(LOG_ERR, "failed to access LED %s - %s",name,strerror(errno));
		return -1;
	}

	/* Re-use string */
	len=sprintf(ledstr,"%d\n",value);
	write(fd,ledstr,len);
	close(fd);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// RSSI support 0-5 bars. Anything >5 is treated as 5. Depending on available hardware,
// this may be translated into fewer 'bars' by the LED driver.
int LedRSSI(int bars)
{
	static int OldRSSI = -1;
	if (OldRSSI != bars) {
		OldRSSI = bars;
		return LedCtrl("rssi",bars);
	}
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
void wwanInitLeds(void)
{
	wwanTurnLed("l2g",-1);
	wwanTurnLed("wwan",-1);
}
////////////////////////////////////////////////////////////////////////////////
int is3GUp(void)
{
	char wwanDbStat[64];
	int pppConn=0;

	wwanDbStat[0]=0;
	if( rdb_get_single("link.profile.1.status",wwanDbStat,sizeof(wwanDbStat))<0 )
		pppConn=0;
	else if (!strcmp(wwanDbStat,"up"))
		pppConn=1;

	return pppConn;
}
////////////////////////////////////////////////////////////////////////////////
void wwanUpdateLedStat(void)
{
	char serviceType[128];
	int dongleExists;
	int pppConn;

	static int SStat=0;     /* Service LED state (0=off, 1=GSM, 2=GPRS, 3=UMTS, 4=LTE */
	static int wwanStat=-1; /* WWAN LED state */

	int newSStat;
	int newWwanStat;

	newSStat=SStat;
	newWwanStat=wwanStat;

	if(rdb_get_single("wwan.0.system_network_status.service_type",serviceType,sizeof(serviceType)-1)<0)
		serviceType[0]=0;

	if      (strstr(serviceType,"GSM"))  newSStat=1;
	else if (strstr(serviceType,"GPRS")) newSStat=2;
	else if (strstr(serviceType,"UMTS")) newSStat=3;
	else if (strstr(serviceType,"HSDPA")) newSStat=3;
	else if (strstr(serviceType,"HSUPA")) newSStat=3;
	else newSStat=0;

	dongleExists=wwan_device_is_present();

	pppConn=is3GUp();

	// paranoid check
	if( !ping_isIfUp("ppp1") && !ping_isIfUp("usb0")  &&  !ping_isIfUp("lte0") &&  !ping_isIfUp("hso0") &&  !ping_isIfUp("eth0"))
		pppConn=0;

	if(pppConn)
		glb.fWentOnline=1;

	if(!dongleExists)
	{
		newSStat=0;
		newWwanStat=-1;
	}
	else
	{
		if(pppConn)
		{
			newWwanStat=1;
		}
		else
		{
			// IPW does not like the LED lit when the 3G connection is not up
			#ifdef SKIN_ipw
			newWwanStat=-1;
			#else
			newWwanStat=0;
			#endif
		}
	}

	if(newSStat!=SStat) {
		if( wwanTurnLed("service",newSStat) )
			SStat=newSStat;
	}
	if(newWwanStat!=wwanStat) {
		if(wwanTurnLed("wwan",newWwanStat) )
			wwanStat=newWwanStat;
	}
}
////////////////////////////////////////////////////////////////////////////////
// Extracting signal strength -[0-9]+dB and setting RSSI LEDs
void DisplayRSSI(void)
{
	int bars=0;
	int rssi=0;

	rdb_get_single_int("wwan.0.radio.information.signal_strength",&rssi);

	// Using Sierra recommendations
	// -110 to -109 = 0 bars
	// -108 to -102 = 1 bar
	if (rssi > -108) bars++;
	// -101 to  -93 = 2 bars
	if (rssi > -101) bars++;
	//  -92 to  -87 = 3 bars
	if (rssi > -92)  bars++;
	//  -86 to  -78 = 4 bars
	if (rssi > -86)  bars++;
	//  -77 to  -47 = 5 bars
	if (rssi > -77)  bars++;

	// Phone module uses 0 to indicate no signal
	if (rssi == 0) bars=0;

	LedRSSI(bars);
}

////////////////////////////////////////////////////////////////////////////////
int turnWWLed(int fOn)
{
	static int fCurStat=-1;

	if( (fCurStat<0) || (fOn && !fCurStat) || (!fOn && fCurStat) )
		syslog(LOG_INFO, "wwan led changed - cur=%d,new=%d",fCurStat,fOn);
	fCurStat=fOn;

	return LedCtrl("www",fOn?255:0);
}
////////////////////////////////////////////////////////////////////////////////
int ctrlRathLed(int newLinkActivity,int newOn)
{
	static int curOn=-1;
	static int curLinkActivity=-1;

	char ledName[64];

	// skip if the variant does not support
	if(_raethWanLED<0)
		return -1;

	if( (newLinkActivity<0) || (newLinkActivity && !curLinkActivity) || (!newLinkActivity && curLinkActivity) )
	{
		curOn=-1;
		syslog(LOG_INFO, "rathled link changed - cur=%d,new=%d",curLinkActivity,newLinkActivity);
	}
	

	if( (curOn<0) || (newOn && !curOn) || (!newOn && curOn) )
		syslog(LOG_INFO, "rathled changed - cur=%d,new=%d",curOn,newOn);


	curOn=newOn;
	curLinkActivity=newLinkActivity;

#define RAETHLED	"/sys/class/leds/raethsw%d"

	sprintf(ledName,RAETHLED,_raethWanLED);

	if(curLinkActivity)
	{
		echoToFile(ledName,"trigger","raethsw");
		echoToFile(ledName,"type","link");
	}
	else
	{
		echoToFile(ledName,"trigger","none");
	}

	echoToFile(ledName,"brightness",curOn?"1":"0");

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int processInternetConnection()
{
	const char* szWanIf=getWanIfName(0);
	int wanMode;
	int fPhysIfUp;

	int fIfUp=0;

	char wanatp0tmp[64];
	char* wanatp0=nvram_get(RT2860_NVRAM, "wan_at_p0");

	int wanPortNumber=0;

	int pppoeWan=0;

	wanMode=0;

	if(!wanatp0)
		wanatp0="0";

	if(rdb_get_single("wan_at_p0",wanatp0tmp,sizeof(wanatp0tmp))<0)
		wanatp0tmp[0]=0;

	if(strlen(wanatp0tmp))
		wanMode=atoi(wanatp0tmp);
	else
		wanMode=atoi(wanatp0);

	if(!wanMode)
		wanPortNumber=4;

	nvram_strfree(wanatp0);

	// get ether link status
	fPhysIfUp=isIfUp("eth2.2",wanPortNumber);

	if(fPhysIfUp)
		rdb_set_single_create("wan.link","up");
	else
		rdb_set_single_create("wan.link","down");

	int fConn=is3GUp();
	int pptpWan=isPPTP() && !fConn;

	// if eth is connected and ppp is up
	pppoeWan=strstr(szWanIf,"ppp")!=0;
	if(pppoeWan) {
		if(fPhysIfUp)
			fIfUp=ping_isIfUp("ppp0");
	}
	else {
		fIfUp=fPhysIfUp;
	}

	// assume no connectioni if it fails
	if(fIfUp<0)
		fIfUp=0;

	static int fPrevIfUp=0;
	static int fInv=1;

	if(fInv || (fPrevIfUp && !fIfUp) || (!fPrevIfUp && fIfUp))
	{
		const char* stat;

		if(fIfUp)
			stat="Connected";
		else
			stat="Disconnected";
	
		rdb_set_single_create("wan.status",stat);
		if(!pppoeWan)
			system("wan.sh &");
	}

	fInv=0;
	fPrevIfUp=fIfUp;

	int fLed=(_f3GBackupEn && ((!_fPingHostDown && fIfUp) || fConn)) || (!_f3GBackupEn && (fIfUp || fConn));

	//printf("bk=%d,pingdown=%d,conn=%d,ifup=%d,led=%d\n",_f3GBackupEn,_fPingHostDown,fConn,fIfUp,fLed);

	// we are doing raether led here
	ctrlRathLed( !wanMode,fIfUp && !(wanMode && !pppoeWan && !pptpWan) );

	return turnWWLed(fLed);
}

////////////////////////////////////////////////////////////////////////////////
int monitor_traffic()
{

	static tick clkPrevChk=-1;
	tick clkNow=getTickCountMS();

	// do it every 10 seconds
	if( (clkPrevChk!=-1) && (clkNow-clkPrevChk<10000) )
		return 0;
	clkPrevChk=clkNow;

	// open net
	const char* procNetDev="/proc/net/dev";

	FILE* pF=fopen(procNetDev,"r");
	if(!pF)
	{
		log_ERR("open %s failed - %s",procNetDev,strerror(errno));
		return -1;
	}

	// read net
	long long lInPck=-1;
	long long lOutPck=-1;
	char achLine[1024];
	while(fgets(achLine,sizeof(achLine),pF))
	{
		// replace colon with space
		char* szColon=strchr(achLine,':');
		if(!szColon)
			continue;
		*szColon=' ';

		// sscan
		char achDev[16];
		long long lDummy;
		long long lInPckTmp;
		long long lOutPckTmp;
		int cItems=sscanf(achLine,"%s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",achDev,&lDummy,&lInPckTmp,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lOutPckTmp,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy);
		if(cItems!=17)
			continue;

		// done if ppp1
		if(!strcmp(achDev,"ppp1"))
		{
			lInPck=lInPckTmp;
			lOutPck=lOutPckTmp;
			break;
		}
	}

	fclose(pF);

	static long long lPrevInPck=-1;
	static long long lPrevOutPck=-1;

	static int cOutPckChg=0;

	// bypass if no ppp1
	if(lInPck<0 || lOutPck<0)
		cOutPckChg=0;

	// check to see if changed
	int fInPckChg=lInPck!=lPrevInPck;
	int fOutPckChg=lOutPck!=lPrevOutPck;
	lPrevInPck=lInPck;
	lPrevOutPck=lOutPck;

	// increase outpckchg count
	if(fOutPckChg)
		cOutPckChg++;

	// reset outpckchg count if inpck
	if(fInPckChg)
		cOutPckChg=0;


	if(cOutPckChg>12)
	{
		int stat;

		log_ERR("3g connection drop-off detected. Restarting pppd!");

		stat=system("reboot_module.sh");
		if(WEXITSTATUS(stat))
			stop_wwan();

		cOutPckChg=0;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
/* Check if MCC & MNC in SIM card is matching those of ISP in APN list.
	Currently check TELSTRA only as request from development team */
int check_isp_matching()
{
#if defined(BOARD_3gt1wn) && defined(SKIN_te)
	char achValue[64];
	if(rdb_get_single("wwan.0.imsi.plmn_mcc",achValue,sizeof(achValue))<0)
		return 0;
	if (strncmp(achValue, "505", 3))
	{
		log_ERR("ISP checking with MCC failed!");
		return 0;
	}
	if(rdb_get_single("wwan.0.imsi.plmn_mnc",achValue,sizeof(achValue))<0)
		return 0;
	if (strncmp(achValue, "001", 3) && strncmp(achValue, "01", 2) && strncmp(achValue, "1", 1))
	{
		log_ERR("ISP checking with MNC failed!");
		return 0;
	}
#endif
	return 1;
}

int check_ICCID_apn()
{
int i=0;
char buff[32];
char cmd[32];
char apn[32];
	*buff=0;
	while(rdb_get_single("wwan.0.system_network_status.simICCID", buff, sizeof(buff)) < 0 || !strlen(buff) )
	{
		if( ++i > 10)
		{
			syslog(LOG_ERR, "failed to get ICCID");
			return 0;
		}
		sleep(2);
	}
	sprintf(cmd, "wwan.0.apn.%s", buff);
	if (rdb_get_single( cmd, apn, sizeof(apn)) >= 0 && strlen(apn)>0 )
	{
		//rdb_set_single( cmd, ""); moved to ip-up
		rdb_set_single_create(DB_WWAN_APN, apn);
		return 1;
	}
	return 0;
}

#ifdef USE_3G_FAILOVER_NOTIFICATION
long long getCurTime()
{
	FILE* fp;
	char buf[128];

	long long upTime=0;

	fp=fopen("/proc/uptime", "r");
	if(fp) 
	{
		if( fgets(buf,sizeof(buf),fp)!=0 )
			upTime=atoll(buf);
		fclose(fp);
	}

	return upTime;
}

void set_failover_duration()
{

	long long upTime;
	unsigned int upday;
	unsigned int uphr;
	unsigned int upmin;
	unsigned int upsec;
	char rdbBuf[128];
	long long endTime;
	long long startTime;
	long long curTime;

	// get end time
	if(rdb_get_single("link.profile.1.endtime",rdbBuf,sizeof(rdbBuf))<0)
		endTime=0;
	else
		endTime=atoll(rdbBuf);

	// get start
	if(rdb_get_single("link.profile.1.starttime",rdbBuf,sizeof(rdbBuf))<0)
		startTime=0;
	else
		startTime=atoll(rdbBuf);

	// get current time
	curTime=getCurTime();

	if(endTime)
		upTime=endTime-startTime;
	else if(startTime)
		upTime=curTime-startTime;
	else
		upTime=0;

	// second
	upsec=upTime%60;
	upTime=(upTime-upsec)/60;
	// minute
	upmin=upTime%60;
	upTime=(upTime-upmin)/60;
	// hour
	uphr=upTime%24;
	upTime=(upTime-uphr)/24;
	// days
	upday=upTime;

	if(upday)
		sprintf(rdbBuf, "[%u] day(s) [%02u] hrs [%02u] mins [%02u] secs",upday, uphr, upmin, upsec);
	else
		sprintf(rdbBuf, "[%02u] hrs [%02u] mins [%02u] secs",uphr, upmin, upsec);

	rdb_set_single_create("FailoverNoti.failoverduration",rdbBuf);

	return;
}

int get_cur_traffic_usage(long long * InByte, long long * OutByte)
{
   char achValue[16];

	// open net
	const char* procNetDev="/proc/net/dev";

	FILE* pF=fopen(procNetDev,"r");
	if(!pF)
	{
		log_ERR("open %s failed - %s",procNetDev,strerror(errno));
		return -1;
	}

   if(rdb_get_single("link.profile.1.interface",achValue,sizeof(achValue))<0)
		return -1;


	// read net
	* InByte=0;
	* OutByte=0;
	char achLine[1024];
	while(fgets(achLine,sizeof(achLine),pF))
	{
		// replace colon with space
		char* szColon=strchr(achLine,':');
		if(!szColon)
			continue;
		*szColon=' ';

		// sscan
		char achDev[16];
		long long lDummy;
		long long lInByteTmp;
		long long lOutByteTmp;
		int cItems=sscanf(achLine,"%s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",achDev,&lInByteTmp,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lOutByteTmp,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy,&lDummy);
		if(cItems!=17)
			continue;

		if(!strcmp(achDev,achValue))
		{
			*InByte=lInByteTmp;
			*OutByte=lOutByteTmp;
			break;
		}
	}

	fclose(pF);

	return 0;
}

int set_failover_usage()
{
	char rxBuf[128];
	char txBuf[128];
	long long cur_rx_usage, cur_tx_usage, rx_usage, tx_usage;

	get_cur_traffic_usage(& cur_rx_usage, & cur_tx_usage);

	if(cur_rx_usage == -1 || cur_tx_usage == -1 || failover_rx_usage == -1 || failover_tx_usage == -1)
		goto error;

	rx_usage = cur_rx_usage - failover_rx_usage;
	tx_usage = cur_tx_usage - failover_tx_usage;

	if(rx_usage < 0 || tx_usage < 0)
		goto error;

	if(0 <= rx_usage && rx_usage <1024)
		sprintf(rxBuf, "Rx: %lld byte(s), ", rx_usage);
	else if(1024 <= rx_usage && rx_usage <1024*1024)
		sprintf(rxBuf, "Rx: %.3Lg kB, ",(long double) rx_usage/1024);
	else
		sprintf(rxBuf, "Rx: %.3Lg MB, ",(long double) rx_usage/(1024*1024));

	if(0 <= tx_usage && tx_usage <1024)
		sprintf(txBuf, "Tx: %lld byte(s)",tx_usage);
	else if(1024 <= tx_usage && tx_usage <1024*1024)
		sprintf(txBuf, "Tx: %.3Lg kB", (long double) tx_usage/1024);
	else
		sprintf(txBuf, "Tx: %.3Lg MB", (long double) tx_usage/(1024*1024));

	strcat(rxBuf, txBuf);

	rdb_set_single_create("FailoverNoti.failoverusage",rxBuf);
	return 0;
error:
	rdb_set_single_create("FailoverNoti.failoverusage","Rx:- MB, Tx:- MB");
	return -1;
}

int is_failovernoti_enabled()
{
	char achValue[16];
	int selectedidx = 0;

	if (rdb_get_single("wwan.0.FailoverNoti.selectedidx", achValue, sizeof(achValue)) < 0)
	{
		return 0;
	}
	
	selectedidx = atoi(achValue);

	if(selectedidx >= 1 && selectedidx <=3)
	   return 1;
	else
	   return 0;
}

int failover_send_sms (char * failover_flag)
{
    char command[128];

    if  (failover_flag == NULL || failover_flag[0] == 0)
         return 0;

    sprintf(command, "/usr/sbin/failover_SMS_noti %s", failover_flag);

    system(command);

    return 1;
}
#endif

#if ( defined(BOARD_3g38wv) || defined(BOARD_3g38wv2) ) && defined(SKIN_ro)
void sim_status_check () {
	static int prevSIMstatus = 0xff;
	int curSIMstatus;
	char achSimStat[64];
	char* triggercmd[]={
		"restore",
		"pinlock",
		"puklock",
		"meplock",
		"nosim",
		NULL,
	};

	// read sim status
	if (rdb_get_single("wwan.0.sim.status.status", achSimStat, sizeof(achSimStat)) < 0)
		return 0;

	if(!strcmp(achSimStat, "SIM PIN") || !strcmp(achSimStat, "SIM PIN Required") || !strncmp(achSimStat, "SIM locked", 10)) {
		curSIMstatus = 1;
	}
	else if(!strcmp(achSimStat, "SIM PUK") || strstr(achSimStat, "PUK")) {
		curSIMstatus = 2;
	}
	else if(!strcmp(achSimStat, "PH-NET PIN") || !strcmp(achSimStat, "SIM PH-NET") || strstr(achSimStat, "MEP")) {
		curSIMstatus = 3;
	}
	else if(!strcmp(achSimStat, "SIM not inserted")) {
		curSIMstatus = 4;
	}
	else if(strstr(achSimStat, "OK")) {
		curSIMstatus = 0;
	}

	if(curSIMstatus != prevSIMstatus) {
		if(triggercmd[curSIMstatus] == NULL) {
			prevSIMstatus = curSIMstatus;
			return 0;
		}

		rdb_set_single("simledctl.command.trigger", triggercmd[curSIMstatus]);

		memset(achSimStat, 0, sizeof(achSimStat));

		if (rdb_get_single("simledctl.command.trigger", achSimStat, sizeof(achSimStat)) < 0)
			return 0;

		if(!strcmp(achSimStat, triggercmd[curSIMstatus]))
			prevSIMstatus = curSIMstatus;
		else
			syslog(LOG_ERR, "+++J SIM_LED Trigger DOES NOT MATCH!!!");
	}
}
#endif

void load_from_nvram(void)
{
	char* buff;
	
	// ccid
	buff=nvram_get(RT2860_NVRAM, "autoapn_ccid");
	rdb_set_single_create("autoapn.profile.ccid",buff);
	
	// user
	buff=nvram_get(RT2860_NVRAM, "autoapn_user");
	rdb_set_single_create("autoapn.profile.user",buff);
	
	// pass
	buff=nvram_get(RT2860_NVRAM, "autoapn_pass");
	rdb_set_single_create("autoapn.profile.pass",buff);
	
	// apn
	buff=nvram_get(RT2860_NVRAM, "autoapn_apn");
	rdb_set_single_create("autoapn.profile.apn",buff);
	
	// auth_type
	buff=nvram_get(RT2860_NVRAM, "autoapn_auth_type");
	rdb_set_single_create("autoapn.profile.auth_type",buff);
	
	// dialstr
	buff=nvram_get(RT2860_NVRAM, "autoapn_dialstr");
	rdb_set_single_create("autoapn.profile.dialstr",buff);
	
	// country
	buff=nvram_get(RT2860_NVRAM, "autoapn_country");
	rdb_set_single_create("autoapn.profile.country",buff);
	
	// mcc
	buff=nvram_get(RT2860_NVRAM, "autoapn_mcc");
	rdb_set_single_create("autoapn.profile.mcc",buff);
	
	// carrier
	buff=nvram_get(RT2860_NVRAM, "autoapn_carrier");
	rdb_set_single_create("autoapn.profile.carrier",buff);
	
	// mnc
	buff=nvram_get(RT2860_NVRAM, "autoapn_mnc");
	rdb_set_single_create("autoapn.profile.mnc",buff);
	
	// score
	buff=nvram_get(RT2860_NVRAM, "autoapn_score");
	rdb_set_single_create("autoapn.profile.score",buff);
}

void save_to_nvram(void)
{
	char buff[256];
	
	// ccid
	if(rdb_get_single("autoapn.profile.ccid",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_ccid",buff);
	
	// user
	if(rdb_get_single("autoapn.profile.user",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_user",buff);
	
	// pass
	if(rdb_get_single("autoapn.profile.pass",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_pass",buff);
	
	// apn
	if(rdb_get_single("autoapn.profile.apn",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_apn",buff);
	
	// auth
	if(rdb_get_single("autoapn.profile.auth_type",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_auth_type",buff);
	
	// dialstr
	if(rdb_get_single("autoapn.profile.dialstr",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_dialstr",buff);
	
	// country
	if(rdb_get_single("autoapn.profile.country",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_country",buff);
	
	// mcc
	if(rdb_get_single("autoapn.profile.mcc",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_mcc",buff);
	
	// carrier
	if(rdb_get_single("autoapn.profile.carrier",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_carrier",buff);
	
	// mnc
	if(rdb_get_single("autoapn.profile.mnc",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_mnc",buff);
	
	// score
	if(rdb_get_single("autoapn.profile.score",buff,sizeof(buff))<0)
		buff[0]=0;
	nvram_set(RT2860_NVRAM,"autoapn_score",buff);
	
	
}

static const char* _rdb_get(const char* dbvar,const char* defval)
{
	static char buf[256];
	
	if( rdb_get_single(dbvar,buf,sizeof(buf))<0 ) {
		//syslog(LOG_NOTICE,"failed to get db variable(%s) - %s(%d)",dbvar,strerror(errno),errno);
		goto err;
	}
	
	return buf;
	
err:
	return defval;
}

static int _rdb_set(const char* var,const char* val)
{
	if( rdb_set_single(var,val)<0 ) {
		
		if(errno!=ENOENT) {
			syslog(LOG_ERR,"failed to set db variable(%s) val=%s - %s(%d)",var,val,strerror(errno),errno);
			goto err;
		}
		
		if( rdb_create_variable(var,val,CREATE, ALL_PERM, 0, 0)<0 ) {
			syslog(LOG_ERR,"failed to create db variable(%s) val=%s - %s(%d)",var,val,strerror(errno),errno);
			goto err;
		}
	}
	
	return 0;
err:
	return -1;
}

int open_output_cmd(const char* cmd)
{
	fp_popen=popen(cmd,"r");
	if(!fp_popen) {
		syslog(LOG_ERR,"popen failed - cmd=%s,err=%s",cmd,strerror(errno));
		return -1;
	}
	
	return 0;
}

const char* read_ouput_cmd()
{
	static char line[1024];
	char* ptr;
	
	if(!fp_popen) {
		syslog(LOG_ERR,"popen not open yet");
		return NULL;
	}
	
	if(!fgets(line,sizeof(line),fp_popen))
		return NULL;
	
	// remove CRLF
	ptr=line;
	while(*ptr && *ptr!='\n')
		ptr++;
	*ptr=0;
	
	return line;
}

void close_output_cmd()
{
	pclose(fp_popen);
}

const char* passgen(const char* seed,int digit)
{
	static char buf[64+1];
	
	int max_digit=sizeof(buf)-1;
	
	// check digit
	if(digit>max_digit) {
		syslog(LOG_ERR,"too many digit required - digit=%d,max=%d",digit,max_digit);
		return 0;
	}
	
	// init
	pg_init_string(seed,strlen(seed));

	int i=0;
	while((i<digit) && (i<sizeof(buf)))
	{
		buf[i]=pg_ascii_range(PG_ASCII_DIGITS);
		i++;
	}
	buf[i]=0;

	return buf;
}

int writeline(const char* line,const char* filename,int truncate)
{
	int stat;
	FILE* fp;
	int line_len;
	
	if(truncate)
		fp=fopen(filename,"w");
	else
		fp=fopen(filename,"a+");
	
	if(!fp) {
		syslog(LOG_ERR,"failed to open file - file=%s,err=%s",filename,strerror(errno));
		return 0;
	}
	
	line_len=strlen(line);
	
	if(line_len)
		stat=fprintf(fp,"%s\n",line);
	else
		stat=0;
	
	fclose(fp);
	
	return stat;
}

int exists(const char* filename)
{
	struct stat s;
	
	return !(stat(filename,&s)<0);
}

int open_file(const char* filename)
{
	fp_file=fopen(filename,"r");
	if(!fp_file) {
		syslog(LOG_ERR,"failed to open file - file=%s,err=%s",filename,strerror(errno));
		return -1;
	}
	
	return 0;
}

const char* read_file()
{
	static char line[1024];
	const char* out;
	char* ptr;
	
	out=fgets(line,sizeof(line),fp_file);
	
	if(out) {
		// remove CRLF
		ptr=line;
		while(*ptr && *ptr!='\n')
			ptr++;
		*ptr=0;
	}
	
	return out;
}

void close_file()
{
	fclose(fp_file);
}

const char* readline(const char* filename)
{
	static char line[1024];
	const char* out;
	
	FILE* fp;
	
	fp=fopen(filename,"r");
	if(!fp) {
		syslog(LOG_ERR,"failed to open file - file=%s,err=%s",filename,strerror(errno));
		return 0;
	}
	
	out=fgets(line,sizeof(line),fp);
	
	char* ptr;
	
	// remove CRLF
	ptr=line;
	while(*ptr && *ptr!='\n')
		ptr++;
	*ptr=0;
	
	fclose(fp);
	
	return out;
}


int process_mep_cmd()
{
	const char* user_unlock_code;
	int factory_unlock=0;
	
	user_unlock_code=_rdb_get("meplock.code","");
	if(!user_unlock_code[0]) {
		user_unlock_code=_rdb_get("meplock.code_factory","");
		if(!user_unlock_code[0])
			goto fini;
		factory_unlock=1;
	}
	
	if(factory_unlock) {
		syslog(LOG_INFO,"got factory meplock code from user - %s",user_unlock_code);
	}
	else {
		syslog(LOG_INFO,"got meplock code from user - %s",user_unlock_code);
	}
	
	// check unlock code
	if(!unlock_code[0]) {
		_rdb_set("meplock.result","error - empty MAC");
		goto fini;
	}
	
	
	if(!strcasecmp(user_unlock_code,"lock")) {
		mep_in_process=1;
		unlink(MEPINFO_MEPUNLOCK);
		
		_rdb_set("meplock.result","success");
		
		if(factory_unlock)
			_rdb_set("meplock.code_factory","");
		else
			_rdb_set("meplock.code","");
		
		goto fini;
	} 
	// compare
	else if(strcmp(unlock_code,user_unlock_code)) {
		syslog(LOG_ERR,"unlock code not matched");
		#ifdef DEBUG
		syslog(LOG_ERR,"user_unlock_code=%s,unlock_code=%s",user_unlock_code,unlock_code);
		#endif		
		_rdb_set("meplock.result","error - MEP code not matched");
		goto err;
	}
	
	if(factory_unlock) {
		syslog(LOG_ERR,"not creating unlock file - factory unlocking");
	}
	else {
		syslog(LOG_INFO,"creating unlock file");
		
		if(writeline(unlock_code,MEPINFO_MEPUNLOCK ".bak",1)<0) {
			syslog(LOG_ERR,"failed to write file - %s",MEPINFO_MEPUNLOCK ".bak");
			_rdb_set("meplock.result","error - storage failure");
			goto err;
		}
		
		if(rename(MEPINFO_MEPUNLOCK ".bak",MEPINFO_MEPUNLOCK)<0) {
			syslog(LOG_ERR,"failed to rename %s to %s - err=%s",MEPINFO_MEPUNLOCK ".bak",MEPINFO_MEPUNLOCK,strerror(errno));
			_rdb_set("meplock.result","error - storage failure");
			goto err;
		}
	}
				
	module_locked_up=0;
	
	_rdb_set("meplock.result","success");
	if(factory_unlock)
		_rdb_set("meplock.code_factory","");
	else
		_rdb_set("meplock.code","");
	
	_rdb_set("meplock.status","unlocked");
	
	syslog(LOG_INFO,"successfully unlocked");
	
fini:
	return 0;
		
err:
	if(factory_unlock)
		_rdb_set("meplock.code_factory","");
	else
		_rdb_set("meplock.code","");
	
	return -1;
}


int process_mep_status()
{
	static char mac[12*3+1]={0,};
	static char mcc[3+1]={0,};
	static char mnc[3+1]={0,};
	
	char unlock_seed[64];
	
	const char* output;
	const char* user_unlock_code;
	
	char mccmnc_seed[128];
	
	const char* encrypted_mccmnc;
	
	// get mac address
	///////////////////////////////////////
	
	if(!mac[0]) {
		syslog(LOG_INFO,"reading mac address");
		
		_rdb_set("meplock.status","checking MAC");
		
		// get mac address
		if(open_output_cmd("mac -r wlan | sed -n 's/\\([0-9A-Fa-f][0-9A-Fa-f]:[0-9A-Fa-f][0-9A-Fa-f]:[0-9A-Fa-f][0-9A-Fa-f]:[0-9A-Fa-f][0-9A-Fa-f]:[0-9A-Fa-f][0-9A-Fa-f]:[0-9A-Fa-f][0-9A-Fa-f]\\)/\\1/p'")<0) {
			syslog(LOG_ERR,"failed to run 'mac' command");
			goto err;
		}
		
		const char* output;
		// read output
		if(!(output=read_ouput_cmd())) {
			syslog(LOG_ERR,"failed to get mac address output from 'mac' command");
			goto err;
		}
		
		// copy mac
		strncpy(mac,output,sizeof(mac));
		mac[sizeof(mac)-1]=0;
	
		close_output_cmd();
		
		syslog(LOG_INFO,"mac address obtained : %s",mac);
		
		if(!mac[0]) {
			syslog(LOG_ERR,"mac address not ready");
			goto err;
		}
	}
	
	// generate unlock code
	///////////////////////////////////////
	if(!unlock_code[0]) {
		
		syslog(LOG_INFO,"generating mep unlock code");
		
		// generate seeds
		snprintf(unlock_seed,sizeof(unlock_seed),"%s%s\n",mac,UNLOCK_SEED_SUFFIX);
		
		#ifdef DEBUG
		syslog(LOG_INFO,"unlock_seed=%s",unlock_seed);
		#endif	
	
		// get unlock code
		if(!(output=passgen(unlock_seed,UNLOCK_CODE_LENGTH))) {
			syslog(LOG_ERR,"failed to get unlock code");
			goto err;
		}
		
		#ifdef DEBUG
		syslog(LOG_INFO,"unlock_code=%s", output);
		#endif	
		
		strncpy(unlock_code,output,sizeof(unlock_code));
		unlock_code[sizeof(unlock_code)-1]=0;
		
		if(!unlock_code[0]) {
			syslog(LOG_ERR,"invalid unlock code");
			goto err;
		}
	}
		
	// generate unlock code
	///////////////////////////////////////
	if(exists(MEPINFO_MEPUNLOCK)) {
		
		syslog(LOG_INFO,"checking user unlock code");
		
		user_unlock_code=readline(MEPINFO_MEPUNLOCK);
		
		if(user_unlock_code && !strcmp(user_unlock_code,unlock_code)) {
			#ifdef DEBUG
			syslog(LOG_INFO,"user unlock code matched=%s", user_unlock_code);
			#endif	
			
			_rdb_set("meplock.status","unlocked");
			module_locked_up=0;
			goto fini;
		}
		else {
			syslog(LOG_INFO,"user unlock code not matched");
		}
	}	
	
	// get mcc & mnc
	///////////////////////////////////////
	
	if(!mcc[0] || !mnc[0]) {
		
		syslog(LOG_INFO,"getting mcc and mnc");
		
		_rdb_set("meplock.status","checking SIM");
		
		// get mcc & mnc
		strncpy(mcc,_rdb_get("wwan.0.imsi.plmn_mcc",""),sizeof(mcc));
		mcc[3]=0;
		strncpy(mnc,_rdb_get("wwan.0.imsi.plmn_mnc",""),sizeof(mnc));
		mnc[3]=0;
		
		if(!mcc[0] || !mnc[0]) {
			syslog(LOG_ERR,"mcc and mnc not ready");
			goto err;
		}
		
		syslog(LOG_INFO,"mcc and mnc obtained : mcc=%s,mnc=%s",mcc,mnc);
	}
	
	// get encrypted mcc & mnc
	///////////////////////////////////////
	
	// "02 424_${MAC}_NetCommMCCMNC"
	snprintf(mccmnc_seed,sizeof(mccmnc_seed),"%s %s_%s%s\n", mnc, mcc, mac,MCCMNC_SEED_SUFFIX);
	#ifdef DEBUG
	syslog(LOG_INFO,"mccmnc_seed=%s",mccmnc_seed);
	#endif	
	
	encrypted_mccmnc=passgen(mccmnc_seed,UNLOCK_CODE_LENGTH);
	if(!encrypted_mccmnc) {
		syslog(LOG_ERR,"failed to generate mccmnc code");
		goto err;
	}
	
	#ifdef DEBUG
	syslog(LOG_INFO,"mccmnc_code=%s",encrypted_mccmnc);
	#endif	
	
	
	// check mcc & mnc
	///////////////////////////////////////
	
	if(open_file(MEPINFO_MCCMNC)<0) {
		syslog(LOG_ERR,"failed to open MCCMNC list - %s",MEPINFO_MCCMNC);
		_rdb_set("meplock.status","locked");
		module_locked_up=1;
		goto fini;
	}
	
	const char* user_mccmnc;
	
	while((user_mccmnc=read_file())!=0) {
		
		#ifdef DEBUG
		syslog(LOG_INFO,"checking user mccmnc - user=%s,cur=%s",user_mccmnc,encrypted_mccmnc);
		#endif	
		
		if(!strcmp(encrypted_mccmnc,user_mccmnc)) {
			syslog(LOG_INFO,"mccmnc matchined - %s",user_mccmnc);
			_rdb_set("meplock.status","ok");
			module_locked_up=0;
			
			goto fini;
		}
	}
	
	close_file();
	
	syslog(LOG_ERR,"mccmnc not matched - %s,%s",mcc,mnc);
	
	// by default locked
	_rdb_set("meplock.status","locked");
	module_locked_up=1;
	
fini:
	return 0;	
	
err:
	return -1;	
	
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	int	ret = 0;
	int	be_daemon = 1;
	int	rval;
	int auto_apn;
	
/*	
	syslog(LOG_INFO,"init");
	tnslookup_init("eth2.2");
	
	syslog(LOG_INFO,"set dns server");	
	tnslookup_change_dns_server("172.16.1.250");
	
	syslog(LOG_INFO,"send query");	
	tnslookup_send("alive.telus.com");
	
	int i=0;
	while(1) {
		i++;
		if(tnslookup_isready()) {
			syslog(LOG_INFO,"recv ready");	
			
			if(tnslookup_recv()>=0) {
				syslog(LOG_INFO,"get answer");	
			}
			else {
				syslog(LOG_INFO,"no answer");	
			}
		}
		
		if(i>10) {
			syslog(LOG_INFO,"send query again");	
			tnslookup_send("alive.telus.com");
			i=0;
		}
		
		sleep(1);
	}
	
	tnslookup_fini();
*/	
	
	// init. tickcount
	initTickCount();

	rval = gpio_init("/dev/gpio");
	if (rval < 0) {
		perror("Couldn't open /dev/gpio");
		return -1;
	}
	
	// Parse Options
	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 'd':
				be_daemon = 0;
				break;
			case 'v':
				_verbosity++ ;
				break;
			case 'V':
				fprintf(stderr, "%s Version %d.%d.%d\n", argv[0],
				        VER_MJ, VER_MN, VER_BLD);
				break;

			case 's':
				_gpioRadioOnOff=atoi(optarg);
				break;

			case '?':
				usage(argv);
				return 2;
		}
	}

	// Initialize the logging interface
	log_INIT(DAEMON_NAME, be_daemon);

	if (be_daemon)
	{
		daemonize("/var/lock/subsys/" DAEMON_NAME, RUN_AS_USER);
		log_INFO("daemonized");
	}

	log_INFO("GPIO pins specified info (raiod on=%d)",_gpioRadioOnOff);
	// Configure signals
	glb.run = 1;
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGCHLD, sig_handler);
	
	if (rdb_open_db() < 0)
		pabort("can't open device");
	
	init_rearswitch();
	init_glb();

	wwanInitLeds();

	// get initial status of stored status
	{
		char* szRearRadioOff = nvram_get(RT2860_NVRAM, "RearRadioOff");
		_fRearRadioOff = atol(szRearRadioOff);
		nvram_strfree(szRearRadioOff);
	}

	char achPrevWanIfName[64]={0,};

	int post_check_pass=0;
	
	// load nvram
	load_from_nvram();
	
	// register callbacks for apnscan
	init_scan_apn_callbacks(start_wwan,stop_wwan,is_wwan_up,save_to_nvram);
	// init. post check
	init_post_check(0);

	// creaet meplock command
	if(_meplock) {
		_rdb_set("meplock.code","");
		_rdb_set("meplock.result","");
		_rdb_set("meplock.code_factory","");
	}
	else {
		_rdb_set("meplock.status","unlocked");
	}
	
	while (glb.run)
	{
		// terminate the connection if requested
		if(glb.req_term_conn) {
			if(is_pppd_running()) {
				syslog(LOG_INFO,"trying to terminate connection by request");
				stop_wwan();
			}
			else {
				glb.req_term_conn=0;
				syslog(LOG_INFO,"connection terminated - clearing the request");
			}
		}
			
		// enjoy mep lock
		if(_meplock) {
			process_mep_cmd();
			
			if(mep_in_process)
				mep_in_process=process_mep_status()<0;
		}
		else {
			mep_in_process=0;
			module_locked_up=0;
		}

		// Ensure all required drivers are compiled into kernel not modules
		// This ensures drivers will be loaded if WWAN module is installed at power up
		
		if (!wwan_device_is_present() || mep_in_process || module_locked_up)
		{
			_fPingHostDown=0;

			// init. post check - wwan is hardcoded in platypus
			post_check_pass=0;
			init_post_check(0);
		}
		else
		{
			// -----> platypus variant selection <-----
			#if defined(BOARD_3g17wn)
			monitor_traffic();
			#elif defined(BOARD_3g18wn)
			monitor_traffic();
			#elif defined(BOARD_3g8wv)
			#elif defined(BOARD_3g36wv)
			#elif defined(BOARD_3g39w)
			#elif defined(BOARD_3g38wv)
			#elif defined(BOARD_3g38wv2)
			#elif defined(BOARD_3g46)
			#elif defined(BOARD_3gt1wn)
			monitor_traffic();
			#elif defined(BOARD_3gt1wn2)
			monitor_traffic();
			#elif defined(BOARD_3g34wn)
			monitor_traffic();
			#elif defined(BOARD_3g23wn)
			monitor_traffic();
			#elif defined(BOARD_3g23wnl)
			monitor_traffic();
			#elif defined(BOARD_testbox)
			monitor_traffic();
			#else
			#error BOARD_BOARD not specified - grep "platypus variant selection" to add a new variant
			#endif


			get_wwan_parameters();

			// check to see if 3G backup enable
#if defined(SKIN_ts)
			_f3GBackupEn = !strcmp(glb.wwan_mode, CONN_TYPE_3GBACKUP) && glb.cPingHost && glb.pingFailCnt && glb.pingSuccCnt;
#else
			_f3GBackupEn = !strcmp(glb.wwan_mode, CONN_TYPE_3GBACKUP) && glb.cPingHost && glb.pingTimer && glb.pingFailCnt;
#endif

#if ( defined(BOARD_3g38wv) || defined(BOARD_3g38wv2) ) && defined(SKIN_ro)
			sim_status_check ();
#endif

			auto_apn=apnIsAutoAPN();
			
			// detect pdp connection dropping
			if(wwan_device_check_pdpstat_capability())
			{
				if(auto_apn && !apnscan_pass) {
					//log_INFO("skip connection drop detection - apnscan in progress");
				}
				else {
					// if connection script is runnning and the connection drops
					if(is_ppp_connected())
					{
						int fCurPdpUp=wwan_device_is_pdpup();
						
						if(glb.connection_delay>0) {
							glb.connection_delay--;
							
							//log_ERR("connection delay applying #%d",glb.connection_delay);
						}
						else if(!fCurPdpUp) {
							log_ERR("connection drop detected - submit disconnection request");
							glb.req_term_conn=1;
						}
					}
				}
			}
			
			// do post check - sim card check and etc
			if(!post_check_pass)
			{
				if(do_post_check()==1)
				{
					log_INFO("post check passed");
					post_check_pass=1;
					
					// init autoscan
					apnscan_pass=0;
					init_scan_apn(1,"");
				}
			}
				
			if(check_isp_matching() && post_check_pass)
			{
#if defined(SKIN_ts)
				const char* szNewWanIfName=getWanIfName(1);
				int fWanChg=strcmp(achPrevWanIfName,szNewWanIfName);
				
				int fIfup=ping_isIfUp(szNewWanIfName);

				//  close nslookup to re-open later if wan is changed and the new wan is down
				if( (fWanChg || !fIfup) && tnslookup_isinit() )
					tnslookup_fini();
				
				// open if wan is up and tslookup is not open
				if(!tnslookup_isinit() && fIfup)
					tnslookup_init(szNewWanIfName);
				
				// update current wan name if changed
				if(fWanChg)
					strcpy(achPrevWanIfName,szNewWanIfName);
				
				// is host down?
				int fNewPingHostDown = 0;
				if (_f3GBackupEn) {
					if(updateDnsServers()<0) {
						#ifdef DEBUG_FAILOVER			
						syslog(LOG_ERR, "no dns sever found - immeidately launching 3G connection");
						#endif						
						fNewPingHostDown=1;
					}
					else {
						fNewPingHostDown=checkHostDownByNslookup();
					}
				}

				// print information
				if (!_fPingHostDown && fNewPingHostDown)
				{
					syslog(LOG_INFO, "### host(s) down - nslookup");
					resetCounters();

					tnslookup_fini();

					rdb_set_single_create("wan.connection","3g");
#ifdef USE_3G_FAILOVER_NOTIFICATION
					if(is_failovernoti_enabled() == 1) {
						send_failover_notification = 1;
						get_cur_traffic_usage(& failover_rx_usage, & failover_tx_usage);
					}
#endif
				}
				else if (_fPingHostDown && !fNewPingHostDown)
				{
					syslog(LOG_INFO, "### host(s) up - nslookup");
					resetCounters();

					rdb_set_single_create("wan.connection","wan");
#ifdef USE_3G_FAILOVER_NOTIFICATION
					if(is_failovernoti_enabled() == 1) {
						send_failback_notification = 1;
						set_failover_usage();
					}
#endif
				}

				
				// apply current information
				_fPingHostDown = fNewPingHostDown;
				
#else
				const char* szNewWanIfName=getWanIfName(1);
				int fWanChg=strcmp(achPrevWanIfName,szNewWanIfName);

				int fIfup=ping_isIfUp(szNewWanIfName);

				if( (fWanChg || !fIfup) && ping_isInit() )
					ping_fini();

				if(!ping_isInit() && fIfup)
					ping_init(szNewWanIfName);

				if(fWanChg)
					strcpy(achPrevWanIfName,szNewWanIfName);


				// is host down?
				int fNewPingHostDown = 0;
				static int switching_delay = 0;
				
				if (_f3GBackupEn) {
					
					#if defined(SKIN_ts)
					fNewPingHostDown=checkHostDown();
					#else
					if(switching_delay) {
						#ifdef DEBUG_FAILOVER
						syslog(LOG_INFO,"switching delay applied #%d",switching_delay);
						#endif
						
						switching_delay--;
						
						fNewPingHostDown=_fPingHostDown;
						
					}
					else {
						fNewPingHostDown=checkHostDown();
					}
					#endif
				}
				else {
					switching_delay=0;
				}

				// print information
				if (!_fPingHostDown && fNewPingHostDown)
				{
					syslog(LOG_INFO, "### host(s) down");
					resetCounters();

					ping_fini();

					rdb_set_single_create("wan.connection","3g");
#ifdef USE_3G_FAILOVER_NOTIFICATION
					
					if(is_failovernoti_enabled() == 1) {
						send_failover_notification = 1;
						get_cur_traffic_usage(& failover_rx_usage, & failover_tx_usage);
					}
#endif
					
					switching_delay=10;
				}
				else if (_fPingHostDown && !fNewPingHostDown)
				{
					syslog(LOG_INFO, "### host(s) up");
					resetCounters();

					ping_fini();
					
					rdb_set_single_create("wan.connection","wan");
#ifdef USE_3G_FAILOVER_NOTIFICATION
					if(is_failovernoti_enabled() == 1) {
						send_failback_notification = 1;
						set_failover_usage();
					}
#endif
					switching_delay=10;
				}

				// apply current information
				_fPingHostDown = fNewPingHostDown;
#endif
				
				// occationally we miss SIGCHLD - activately call
				sig_handler(SIGCHLD);
				
				if(should_start_wwan())
				{
					if(auto_apn && !apnscan_pass)
					{
						if(do_scan_apn()==1) {
							apnscan_pass=1;
						}
					}
					else if(!wwan_interface_is_up()) {
						start_wwan();
					}
				}

#ifdef USE_3G_FAILOVER_NOTIFICATION
				if(is_failovernoti_enabled() == 1) {
					if( send_failover_notification == 1 && is3GUp()) {
						if(send_failover_countdown <= 0) {
							send_failover_notification = 0;
							send_failover_countdown = FAILOVER_NOTI_DELAY;
							system("rdb_set FailoverNoti.Failovertime \"`date \"+%B %d, %Y at %r\"`\"");
							system("rdb_set FailoverNoti.FailoverGMTtime \"`date -u \"+%B %d, %Y at %r GMT\"`\"");
							rdb_set_single("FailoverNoti.email.trigger", "failover");
							failover_send_sms("failover");
						}
						else {
							send_failover_countdown--;
						}

						// reset failback count-down
						send_failback_countdown=FAILOVER_NOTI_DELAY;
					}
					else if( send_failback_notification == 1 ) {
						if(send_failback_countdown <= 0) {
							send_failback_notification =0;
							send_failback_countdown = FAILOVER_NOTI_DELAY;
							set_failover_duration();
							system("rdb_set FailoverNoti.Failovertime \"`date \"+%B %d, %Y at %r\"`\"");
							system("rdb_set FailoverNoti.FailoverGMTtime \"`date -u \"+%B %d, %Y at %r GMT\"`\"");
							rdb_set_single("FailoverNoti.email.trigger", "failback");
							failover_send_sms("failback");
						}
						else {
							send_failback_countdown--;
						}

						// reset failover count-down
						send_failover_countdown=FAILOVER_NOTI_DELAY;
					}
				}
				else {
					send_failover_notification=send_failback_notification=0;
					send_failover_countdown=send_failback_countdown=FAILOVER_NOTI_DELAY;
				}
#endif
			}

			if (should_stop_wwan())
				stop_wwan();
		}

		// monitor switch
		int nNewRearRadioOff=0;
		if(_pR)
			nNewRearRadioOff=rearswitch_readPin(_pR);

		if(nNewRearRadioOff<0)
		{
			syslog(LOG_ERR,"failed to read pin - %s", strerror(errno));
		}
		else
		{
			if((!_fRearRadioOff && nNewRearRadioOff) || (_fRearRadioOff && !nNewRearRadioOff))
			{
				syslog(LOG_INFO,"pin status changed - %d", nNewRearRadioOff);

				// write to nvram
				char achRearRadioOff[256];
				sprintf(achRearRadioOff,"%d",nNewRearRadioOff);
				nvram_set(RT2860_NVRAM,"RearRadioOff",achRearRadioOff);

				// read webpage configuration
				char* szWebRadioOff=nvram_get(RT2860_NVRAM,"RadioOff");
				int nWebRadioOff=atol(szWebRadioOff);
				nvram_strfree(szWebRadioOff);

				// if rear is off and web page is on
				if(nNewRearRadioOff && !nWebRadioOff)
					system("iwpriv ra0 set RadioOn=0; wlan_led.sh off");

				// if rear is on and web page is on
				if(!nNewRearRadioOff && !nWebRadioOff)
					system("iwpriv ra0 set RadioOn=1; wlan_led.sh on");
			}

			_fRearRadioOff=nNewRearRadioOff;
		}

		// monitor internet connection
		processInternetConnection();

		// Update RSSI indicator
		DisplayRSSI();

		// update wwan leds
		wwanUpdateEndPoints();
		wwanUpdateLedStat();

		// update battery status
		batteryUpdateStatus();

		sleep(1);
	}

	// in case, wwand gets killed, it has to kill its child
	if (glb.pppd_pid)
		kill(glb.pppd_pid, SIGTERM);

	fini_rearswitch();

	// fini ping
	ping_fini();

	// fini tick count
	finiTickCount();

	wwand_shutdown(0);
	return 0;
}

/*
* vim:ts=4:sw=4:
*/
