#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/select.h>

#include <sys/time.h>
#include "rdb_ops.h"
#include <syslog.h>

#include "libgpio.h"
#define GPIO_DEV "/dev/gpio"

/* TODO: Use / create more suitable V_Variable */

#if defined(BOARD_falcon) || defined(BOARD_nguni) || defined(BOARD_nmc1000) || defined(BOARD_ntc_20) || \
	defined(BOARD_ntc_70) || defined(BOARD_kewel) || defined(BOARD_monty) || defined(BOARD_norman)
void check_timing(void);
#else
static void add_gpio(int *g, int gpio);
#endif

// -----> platypus variant selection <-----
#if defined(BOARD_3g22wv) || defined(BOARD_4g100w) || defined(BOARD_nhd1w) || defined(BOARD_4gt101w)

#define BUTTON_MEDPRESS  2  /* Medium button press time in seconds. */
#define BUTTON_LONGPRESS 10 /* Long button press time in seconds. */

/* Select gpio line(s) for button(s). This needs to define  BUTTON_RST,
   BUTTON_WPS and BUTTON_WAN. They may be the same GPIO. If a function
   is unused please set to -1.

   There are three possible functions on a button: WPS, WAN swap and
   factory reset. If all three functions are on the same button, the
   timing is as follows:

   <3s   = WPS
   <10s  = WAN swap
   >10s  = Factory reset

   In case of factory reset and one other function (WPS or WAN), the
   timing is

   <10s  = (WPS or WAN)
   >10s  = Factory reset

   If ony WPS and WAN share a button, the timing is

   <3s   = WPS
   >3s   = WAN
*/

	#define BUTTON_RST 0
	#define BUTTON_WPS 0
	#define BUTTON_WAN -1
	#define WPS_AP_CATCH_CONFIGURED_TIMER     1
	#define WPS_AP_TIMEOUT_SECS               120    // 120 seconds
#elif defined(BOARD_falcon) || defined(BOARD_nmc1000) || defined(BOARD_ntc_20) || defined(BOARD_ntc_70) || defined(BOARD_norman)

	//#define FALCON_POLLING_GPIO

	#define BUTTON_MEDPRESS  5  /* Medium button press time in seconds. */
	#define BUTTON_LONGPRESS 60 /* Long button press time in seconds. */

#if defined(BOARD_norman)
	#define BUTTON_RST 52
#else
	#define BUTTON_RST 125
#endif // BOARD_norman

	static int gpio_no = BUTTON_RST;
	static int gpio_stat;
	static int gpio_prev_stat = -1;

#elif defined(BOARD_nguni) || defined(BOARD_kewel) || defined(BOARD_monty)
	#define BUTTON_MEDPRESS  5  /* Medium button press time in seconds. */
	#define BUTTON_LONGPRESS 15 /* Long button press time in seconds. */
#ifdef IOBOARD_clarke
	#define BUTTON_RST 64
#else
	#define BUTTON_RST 14
#endif
	#define BUTTON_WPS -1
	#define BUTTON_WAN -1

	static int gpio_no = BUTTON_RST;
	static int gpio_stat;
	static int gpio_prev_stat = -1;
#else
	#error Unknown board - grep "platypus variant selection" to add a new variant
#endif


static char rdb_buf[64];

#if defined(BOARD_falcon) || defined(BOARD_nguni) || defined(BOARD_nmc1000) || defined(BOARD_ntc_20) || \
    defined(BOARD_ntc_70) || defined(BOARD_kewel) || defined(BOARD_monty)   || defined(BOARD_norman)
/* nothing to do */
#else
static void RST(void) { /* Trigger factory reset */
	printf("Load default and reboot..\n");
	// driving leds
	system(
		"cd /sys/class/leds; "
		"for i in *; "
			"do led set $i trigger timer delay_on 50 delay_off 50; "
		"done;"
	);
	// Load factory defaults and reboot
	system("dbcfg_default -f -r");
}
/* inserts a GPIO number into table, avoiding duplicates. */
static void add_gpio(int *g, int gpio) {
	int idx=0;
	while (g[idx] != -1 && g[idx] != gpio) idx++;
	if (g[idx] == -1) {
		g[idx]=gpio;
	}
}

static int wsc_timeout_counter = 0;
static int trigger_flag = 0;
static void WPSAPTimerHandler(void) {
	//TODO: check wps status and set to an rdb varible (the fuction can be moved from ajax.c: updateWPS())
	// check if timeout
	wsc_timeout_counter += WPS_AP_CATCH_CONFIGURED_TIMER;
	if(wsc_timeout_counter > WPS_AP_TIMEOUT_SECS && trigger_flag == 0) {
		wsc_timeout_counter = 0;
		trigger_flag = 1;
		rdb_get_single("wlan.0.enable",rdb_buf,sizeof(rdb_buf));
		if(atoi(rdb_buf)>0) {
			system("led sys wlan state 1");
		}
		else {
			system("led sys wlan state 0");
		}
		rdb_set_single("wlan.0.wps_trigger","0");

	}
}
static void WPS(void) { /* Trigger WPS function */
	if( rdb_get_single("wlan.0.wps_enable",rdb_buf,sizeof(rdb_buf))<0 ) {
		syslog(LOG_ERR,"failed to get wlan.0.wps_enable - %s",strerror(errno));
		return;
	}
	if(atoi(rdb_buf)!=1) {
		syslog(LOG_INFO,"The PBC button is pressed but WPS is disabled.");
		printf("The PBC button is pressed but WPS is disabled.\n");
	}
	else {
		rdb_set_single("wlan.0.wps_PIN_pbc","2");//triggering wps.template
		rdb_set_single("wlan.0.wps_trigger","1");
	}
}
static void WAN(void) { /* Trigger WAN swap function */
	//TODO
	printf("WAN switch not supported\n");
}
#endif

/*
 * gpio interrupt handler - distinguishes between (<3s), (>=3s<10) & (>=10s)
 *
 * arg is (void*)bitmap, with 1 for BUTTON_WPS, 2 for BUTTON_WAN and 4 for
 * BUTTON_RST. Any combination is possible.
 *
 */
#define BMASK_WPS (1<<0)
#define BMASK_WAN (1<<1)
#define BMASK_RST (1<<2)

static void gpioIrqHandler(void *arg, unsigned time) {
	/* Time is in milliseconds */

	//printf("sysmonitor: signal, button=%d, time=%d\n",button,time);

#if defined(BOARD_falcon) || defined (BOARD_nguni) || defined(BOARD_nmc1000) || defined(BOARD_ntc_20) || \
	defined(BOARD_ntc_70) || defined(BOARD_kewel) || defined(BOARD_monty)
	gpio_stat=!!gpio_read(gpio_no);
	check_timing();
#elif defined(BOARD_norman) // QC-based
    // TODO: WARNING!
    //       This block is untested and it's added here as a placeholder.  For now it copies
    //       the Freescale set of boards but it needs to be tested and modified for QC if needed.
    printf("sysmonitor: WARNING! QC based variants not fully tested with this program\n");
   	gpio_stat=!!gpio_read(gpio_no);
	check_timing();
#else
	unsigned button = (unsigned)arg;
	if (time < BUTTON_MEDPRESS*1000) { /* Button press is <3 secs */
		if (button & BMASK_WPS) {
			WPS();
		}
		#if BUTTON_WAN != BUTTON_WPS
		/* In case we don't share WPS & WAN */
		if (button & BMASK_WAN) {
			WAN();
		}
		#endif
	}
	else if (time < BUTTON_LONGPRESS*1000) { /* Button press is <10 secs */
		#if BUTTON_WAN != BUTTON_WPS
		/* In case we don't share WPS & WAN */
		if (button & BMASK_WPS) {
			WPS();
		}
		#endif
		if(button & BMASK_WAN) {
			WAN();
		}
	}
	else { /* Button press >10secs */
		if (button & BMASK_RST) {
			RST();
		}
	}
#endif
}

/* Error handling macro. If rv < 0 print error and exit. */
#define CHECK(rv, fmt, ...) do { \
	if ((rv) < 0) { \
		fprintf(stderr, "%s: ", strerror(errno)); \
		fprintf(stderr, fmt, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		exit(-1); \
	} \
} while(0)

static void Register_button_gpio(int gpio, unsigned mask) {
	int rval;

	rval = gpio_request_pin(gpio);
	CHECK(rval,"Couldn't request gpio %d",gpio);

	rval = gpio_set_input(gpio);
	CHECK(rval,"Couldn't switch gpio %d to input",gpio);
#if defined(BOARD_falcon) || defined(BOARD_nguni) || defined(BOARD_nmc1000) || defined(BOARD_ntc_20) || \
	defined(BOARD_ntc_70) || defined(BOARD_kewel) || defined(BOARD_monty) || defined(BOARD_norman)
	rval = gpio_register_callback(gpio, 1, gpioIrqHandler, (void*)mask);
	CHECK(rval, "Couldn't register interrupt for gpio %d", gpio);
	rval = gpio_register_callback(gpio, 0, gpioIrqHandler, (void*)mask);
	CHECK(rval, "Couldn't register interrupt for gpio %d", gpio);
#else
	/* Button is active low on all all variants */
	rval = gpio_register_callback_time(gpio, 0, BUTTON_LONGPRESS*1000, gpioIrqHandler, (void*)mask);
	CHECK(rval, "Couldn't register interrupt for gpio %d", gpio);
#endif
}

/*
 * Initialise GPIOs and GPIO interrupt handler for all buttons.
 */
#if defined(BOARD_falcon) || defined(BOARD_nguni) || defined(BOARD_nmc1000) || defined(BOARD_ntc_20) || \
	defined(BOARD_ntc_70) || defined(BOARD_kewel) || defined(BOARD_monty) || defined(BOARD_norman)
int initGpio(void) {
	if(gpio_init(GPIO_DEV)<0) {
		syslog(LOG_ERR,"failed to initialize gpio - gpio=%d,err=%s",gpio_no,strerror(errno));
		return -1;
	}
#ifdef FALCON_POLLING_GPIO
	// request
	if(gpio_request_pin(gpio_no)<0) {
		syslog(LOG_ERR,"request failed - gpio=%d,err=%s",gpio_no,strerror(errno));
		return -1;
	}

	// set to input
	if(gpio_set_input(gpio_no)<0) {
		syslog(LOG_ERR,"input setting failed - gpio=%d,err=%s",gpio_no,strerror(errno));
		return -1;
	}
#else
	Register_button_gpio(BUTTON_RST, 0);
#endif
	return 0;
}

void check_timing() {
	if(!rdb_get_single("hw.reset.disable", rdb_buf, sizeof(rdb_buf)) &&
	   !strcmp(rdb_buf, "1")) {
		syslog(LOG_INFO, "reset button is disabled");
		return;
	}
	if(gpio_stat==0) {
		syslog(LOG_INFO,"reset button detected - pressed");
		// start reset button procedure in dispd, which will set dispd.pattern.subtype according to LED status
		rdb_set_single("dispd.pattern.type", "resetbutton");
	}
	else if( gpio_prev_stat >=0 ) {
		// button released, read reset LED status to determine what to do
		rdb_get_single("dispd.pattern.subtype", rdb_buf, sizeof(rdb_buf));
		syslog(LOG_INFO,"reset button detected - released (status=%s)",rdb_buf);

		if(strcmp(rdb_buf, "recovery")==0) {
			rdb_set_single("dispd.pattern.type", "recoveryflashing");

			syslog(LOG_INFO,"reset button - **reboot** to recovery partition");
			system("environment -w emergency button && reboot");
		}
		else if(strcmp(rdb_buf, "factory")==0) {
			rdb_set_single("dispd.pattern.type", "factoryflashing");

			syslog(LOG_INFO,"reset button - factory reset and **reboot** to main partition");
			system("dbcfg_default -f -r");
		}
		else { // main
			rdb_set_single("dispd.pattern.type", "mainflashing");

			syslog(LOG_INFO,"reset button - **reboot** to main partition");
			system("sleep 1 && reboot");
		}
	}
	gpio_prev_stat=gpio_stat;
}

#else
int initGpio(void) {
	int rval;
	/* Maximum buttons */
	#define MAX_BUTTONS 3
	int g[MAX_BUTTONS+1]; /* Sentinel value at the end */
	int idx=0;
	unsigned mask=0;

	for(idx=0; idx<MAX_BUTTONS+1; idx++) g[idx]=-1;

	printf("Initialising button GPIO (%d,%d,%d)\n",
		BUTTON_WPS, BUTTON_WAN, BUTTON_RST);

	add_gpio(g,BUTTON_WPS);
	add_gpio(g,BUTTON_WAN);
	add_gpio(g,BUTTON_RST);

	if (g[0] == -1) {
		printf("sysmonitor: no buttons\n");
		return 0;
	} else {
		/* Initialise gpio library */
		rval = gpio_init(GPIO_DEV);
		CHECK(rval, "gpio_init(%s)", GPIO_DEV);

		idx=0;
		while (g[idx] != -1) {
			mask = ((g[idx]==BUTTON_WPS)?BMASK_WPS:0) |
			       ((g[idx]==BUTTON_WAN)?BMASK_WAN:0) |
			       ((g[idx]==BUTTON_RST)?BMASK_RST:0);
			printf("idx=%d, g[idx]=%d, mask=%02x\n", idx, g[idx], mask);
			printf("sysmonitor: registering gpio %d for %s%s%s\n", g[idx],
					(mask & BMASK_WPS)?"WPS ":"",
					(mask & BMASK_WAN)?"WAN ":"",
					(mask & BMASK_RST)?"RST ":"");
			Register_button_gpio(g[idx], mask);
			idx++;
		}
	}
	return 0;
}
#endif

int main(int argc,char **argv) {
	struct timeval outtime;
	int ret;

	openlog("sysmonitor", LOG_PID | LOG_PERROR, LOG_DEBUG);

	// open rdb
	if(rdb_open_db()<0) {
		syslog(LOG_ERR,"failed to open rdb - %s",strerror(errno));
		return -1;
	}

	// IRQ handler might be called immediately after this. So rdb should be opened beforehand
	if (initGpio() != 0) {
		rdb_close_db();
		exit(EXIT_FAILURE);
	}

#if defined (BOARD_falcon) || defined(BOARD_nguni) || defined(BOARD_nmc1000) || defined(BOARD_ntc_20) || \
	defined(BOARD_ntc_70) || defined(BOARD_kewel) || defined(BOARD_monty) || defined(BOARD_norman)
	if( (rdb_create_variable("dispd.pattern.type", "", CREATE, ALL_PERM, 0, 0))<0 ) {
		if(errno!=EEXIST) {
			syslog(LOG_ERR,"failed to create %s - %s", "dispd.pattern.type", strerror(errno));
			return -1;
		}
	}
	rdb_set_single("dispd.pattern.type", "");
	while(1) {
		outtime.tv_usec=100000; //100ms
#ifdef FALCON_POLLING_GPIO
		outtime.tv_sec=0;
#else
		outtime.tv_sec=1;
#endif
		// select - wait!
		ret=select(0,NULL,NULL,NULL,&outtime);

		if(ret==-1 && errno!=EINTR) {
			syslog(LOG_ERR,"unknown result from select() - stat=%d,err=%s",gpio_stat,strerror(errno));
			break;
		}
#ifdef FALCON_POLLING_GPIO
		gpio_stat=!!gpio_read(gpio_no);

		if(gpio_prev_stat<0 || (gpio_prev_stat != gpio_stat) ) {
			syslog(LOG_INFO,"gpio changed - gpio=%d,stat=%d",gpio_no,gpio_stat);
			check_timing();
		}
#endif
	}
#ifndef FALCON_POLLING_GPIO
	gpio_unregister_callbacks(BUTTON_RST);
#endif
#elif defined (BOARD_nguni) || defined(BOARD_kewel) || defined(BOARD_monty)
	fd_set timerset;
	while (1) {
		FD_ZERO(&timerset);
		outtime.tv_sec=1;
		outtime.tv_usec=0;
		FD_SET(0, &timerset);
		ret = select(0,NULL,NULL,NULL,&outtime);
		pause();
	}
	gpio_unregister_callbacks(BUTTON_RST);
#else
	fd_set timerset;
	while (1) {
		FD_ZERO(&timerset);
		outtime.tv_sec=WPS_AP_CATCH_CONFIGURED_TIMER;
		outtime.tv_usec=0;
		FD_SET(0, &timerset);
		ret = select(0,NULL,NULL,NULL,&outtime);
		WPSAPTimerHandler();
		//pause();
	}

	gpio_unregister_callbacks(BUTTON_WPS);
#if BUTTON_WPS != BUTTON_RST
	gpio_unregister_callbacks(BUTTON_RST);
#endif
#endif
	gpio_exit();
	rdb_close_db();
	exit(EXIT_SUCCESS);
}
