/*
This is a module for the system monitor that watches the Avian battery state
through sysfs.

The built-in test reads battery voltages from the testdata.txt file, containing
3 columns - seconds, charge/discharge and battery voltage, one line per second.

Iwo Mergler <Iwo@call-direct.com.au>
*/
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define LOG(...) syslog(LOG_INFO, __VA_ARGS__)

#ifdef TEST
#include <string.h>
#else
#include "rdb_ops.h"
#endif

#ifdef TEST
#define get_voltage current_voltage_test
static int current_voltage_test(char *c);
#else

/* To save memory, the batpath variable is manipulated to assemble
a full file path as needed. */
#define BAT_PATH "/sys/devices/platform/qci-battery/power_supply/"
#define PATHLENGTH (sizeof(BAT_PATH)-1)
#define MAXPATH (PATHLENGTH+32)
static char batpath[MAXPATH]=BAT_PATH;

int battery_charging(void)
{
	const char *fname;
	const char *buf;
	fname=filename(batpath,PATHLENGTH,"battery/status");
	buf=readfile(fname);
	return !(buf[0]=='D');
}

int battery_usb(void)
{
	const char *fname;
	int usb=0;
	fname=filename(batpath,PATHLENGTH,"ac/online");
	usb = read_natural(fname);
	if (!usb) {
		fname=filename(batpath,PATHLENGTH,"usb/online");
		usb = read_natural(fname);
	}
	return usb;
}

int battery_voltage(void)
{
	const char *fname;
	fname=filename(batpath,PATHLENGTH,"battery/voltage_now");
	return read_natural(fname);
}

/* Returns voltage in mV and *c='c'|'d' for charge/discharge. Returns -1 on error. */
/* ac & usb are booleans showing AC adapter and USB cable power status. */
int get_voltage(char *c, int *ac, int *usb)
{
	const char *fname;
	const char *buf;
	int volt;

	fname=filename(batpath,PATHLENGTH,"battery/voltage_now");
	volt=read_natural(fname);

	if (c) {
		fname=filename(batpath,PATHLENGTH,"battery/status");
		buf=readfile(fname);
		if (buf[0]=='D') {
			*c='d';
		} else {
			*c='c';
		}
	}

	if (ac) {
		fname=filename(batpath,PATHLENGTH,"ac/online");
		*ac = read_natural(fname);
	}

	if (usb) {
		fname=filename(batpath,PATHLENGTH,"usb/online");
		*usb = read_natural(fname);
	}

	return volt;
}
#endif


/* Variable order infinite impulse response filter.
   delay[], A[] & B[] must be the size (order+1).
   delay[] is used internally, A[] are the forward coefficients,
   B[] are the feedback coefficients, B[0]=1.0. */
double iir(double sample, int order, double delay[], double A[], double B[])
{
	int i;
	double output;
	/* Shift the delay line */
	for (i=order; i>=1; i--) {
		delay[i]=delay[i-1];
	}

	/* Feedback branch (we skip B[0]) */
	delay[0]=sample;
	for (i=1; i<=order; i++) {
		delay[0]+=delay[i]*B[i];
	}

	/* Feed forward branch */
	output=0.0;
	for (i=0; i<=order; i++) {
		output += delay[i]*A[i];
	}

	return output;
}


static double filterstep(double sample)
{
	double rval;
	int i;
	#define IIR_ORDER 5
	#define IIR_STEP  0.01
	static double delay[IIR_ORDER];
	static int first=1;

	if (first) {
		for (i=0; i<IIR_ORDER; i++) delay[i]=sample;
		first=0;
	}

	delay[0]=sample;
	for (i=IIR_ORDER-1; i>=1; i--) {
		delay[i]=(delay[i]*(1.0-IIR_STEP)) + (delay[i-1]*IIR_STEP);
	}

	rval=delay[IIR_ORDER-1];

	if (rval<3000) {
		LOG("filtered = %0.2f", rval);
	}

	return rval;
}

/* Converts a filtered voltage into charge % (assuming a discharge curve) */
static double voltage2discharge(double voltage)
{
	/* Least squares 3d order approximation of discharge curve */
	/* The approximation is valid between 3.6 & 4.1V only */
	#define lsa -9.88309497682279e-07
	#define lsb 1.12458162945008e-02
	#define lsc -4.23956119857913e+01
	#define lsd 5.29907964135273e+04
	if (voltage>4080.0) return 100.0;
	else if (voltage<3600.0) return 0.0;
	else return(((lsa*voltage+lsb)*voltage+lsc)*voltage+lsd);
}

/* Quantizes the discharge level to 5% steps, applies a 5% upward hysteresis and converts to int */
static int discharge(double percent)
{
	int now;
	static int prev=0;
	now=rint(percent/5)*5;
	if (prev>now || prev<now-5) prev=now;
	return prev;
}

/* Converts a filtered voltage into charge % (assuming a charging+running curve */
static double voltage2charge(double voltage)
{
	/* Linear approximation of charge curve (unit is running) */
	#define lia 0.2
	#define lib -740.0
	if (voltage>4200) return 100.0;
	else if (voltage<3700) return 0.0;
	else return(voltage*lia+lib);
}

/* Quantizes the charging level to 5% steps, applies a 5% downward hysteresis and converts to int */
static int charge(double percent)
{
	int now;
	static int prev=0;
	now=rint(percent/5)*5;
	if (prev<now || prev>now+5) prev=now;
	return prev;
}

static int set_led(int request, int state)
{
	int fd, rval;
	fd=open("/dev/qciled", O_RDWR);
	if (fd == -1) {
		LOG("Could not open /dev/qciled, exiting\n");
		exit(-1);
	}
	rval = ioctl(fd, request, state);
	close(fd);
	return rval;
}

static void update_db_led(int mV, int percent, char mode, int ac, int usb)
{
/* #define DEBUG */
	int rval;
	static int old_percent=-1;
	char dbb[10];
#ifdef DEBUG
	const char *ledmsg="";
#endif
	int ioctls[]={11,12,16,17,32,0};
	int ioctl=0;
	int i;

	/* Change LED status as needed, separate by charge/discharge. */
	if (usb || ac) {
		/* Charging indicated by modem via high priority solid red */
		if (percent >= 100) {
			/* Solid green, fully charged */
			ioctl = 16;
#ifdef DEBUG
			ledmsg="green solid";
#endif
		} else {
#ifdef DEBUG
			ledmsg="controlled by modem";
#endif
		}
	} else {
		if (percent <= 10) {
			/* Red blinking */
			ioctl = 12;
#ifdef DEBUG
			ledmsg="red flashing";
#endif
#if 0
/* Removed be request from Marketing */
		} else if (percent <= 50) {
			/* Amber blinking */
			ioctl = 32;
#ifdef DEBUG
			ledmsg="amber flashing";
#endif
#endif
		} else {
			/* green blinking */
			ioctl = 17;
#ifdef DEBUG
			ledmsg="green flashing";
#endif
		}
	}

	/* Switch all others off. */
	for (i=0; ioctls[i]; i++) {
		if (ioctls[i] != ioctl)
			set_led(ioctls[i],0);
	}
	set_led(ioctl,1);

#ifdef DEBUG
	LOG("Battery: %dmV, [%c, ac=%d, usb=%d], %d%%\n", mV, mode, ac, usb, percent);
	LOG("LED: ledmsg=%s\n", ledmsg);
#endif
	/* Update database. Percentage is only updated when it changes,
	this allows templates to be triggered less often */
	sprintf(dbb,"%d",mV);
	rval = rdb_update_single("battery.voltage", dbb, 0, DEFAULT_PERM, 0, 0);
	if (mode=='d') sprintf(dbb,"d");
	else sprintf(dbb,"c");
	rdb_update_single("battery.status", dbb, 0, DEFAULT_PERM, 0, 0);
	if (old_percent!=percent) {
		sprintf(dbb,"%d",percent);
		rdb_update_single("battery.percent", dbb, 0, DEFAULT_PERM, 0, 0);
		old_percent=percent;

		LOG("Battery at (%dmV) %d%% - %s, usb=%d, ac=%d\n", mV, percent,
				(mode=='d')?"discharging":"charging", usb, ac);
	}
}

/* Returns filtered voltage in mV. */
int poll_battery(int supply_present, int charging, int voltage)
{
	double filtered=0.0;
	int mV;
	int percent;
	char mode=0;
	int ac=0;
	int usb=0;
	int i;

	static time_t last_sample=0;
	time_t now;
	int elapsed_seconds;

	if (voltage<3000) return 0;

	now=time(NULL);
	elapsed_seconds = now - last_sample;
	/* Dealing with init state */
	if (!last_sample) elapsed_seconds=1;
	last_sample = now;

	if (elapsed_seconds==0) return 0;

	if (charging) mode='c'; else mode='d';
	if (supply_present) usb=1;

	/* The filter is calibrated in seconds, so we run it
	as many times as there have seconds elapsed. */
	i=elapsed_seconds;
	while (i--) {
		filtered=filterstep((double)voltage);
	}

	mV = rint(filtered);

/*
	LOG("bat: supply_present=%d, charging=%d, voltage=%d, filtered=%d, elapsed=%d",
			supply_present, charging, voltage, mV, elapsed_seconds);
*/

	/* Deal with charge/discharge cycles while fully charged and connected. */
	if (filtered > 4200.0) {
		percent=100;
	} else {
		if (mode=='d')
			percent = discharge(voltage2discharge(filtered));
		else
			percent = charge(voltage2charge(filtered));
	}

#ifdef TEST
	printf("%f %f %d\n", volt, filtered, percent);
#else
	update_db_led(mV, percent, mode, ac, usb);
#endif

	return mV;
}

int init_battery(int a, int b)
{
	if (a) {
		fprintf(stderr, "LED (%d/%d) rval=%d\n",a,b,set_led(a,b));
		exit(1);
	}
	return 0;
}

#ifdef TEST

static FILE *f;

static int current_voltage_test(char *c, int *ac, int *usb)
{
	static char buf[100];
	char *p;
	int v;
	(void)ac;
	(void)usb;
	if (feof(f)) exit(0);
	fgets(buf,100,f);
	p=buf;
	while (*(p++)!=' ') {};
	if (c) *c = *p;
	while (*(p++)!=' ') {};
	v = atoi(p);
	return v;
}

int main(void)
{
	int mV;
	f=fopen("testdata.txt","r");
	while (1) {
		mV = poll_battery();
	}
	fclose(f);
	return 0;
}
#endif
