/*
System monitor, responds to hotplug events

- checks the battery state (and sets LEDs)
- checks the tethering interface.

Iwo Mergler <Iwo@call-direct.com.au>
*/
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <linux/types.h>
#include <linux/netlink.h>

#include "battery.h"
#include "netdevices.h"
#include "rdb_ops.h"

#define DEBUG

#ifdef DEBUG
#define D(...) syslog(LOG_INFO, __VA_ARGS__)
#else
static inline int NOLOG(const char *fmt, ...)
{
	(void)fmt; return 0;
}
#define D NOLOG
#endif

#define POLLRATE 5 /* Seconds */
const char *progname = "SysMon";

static volatile int active=1;

void sighandler(int sig)
{
	syslog(LOG_INFO, "Caught signal %d", sig);
	active=0;
}

/* Extracting a few relevant hotplug variables */
struct hotplug {
#define UNKNOWN 0
	int action;
#define ACTION_ADD 1
#define ACTION_REMOVE 2
#define ACTION_CHANGE 3
	int subsystem;
#define SUBSYS_POWER 1
#define SUBSYS_SWITCH 3 /* File system, mass storage */
#define SUBSYS_NET 4
	/* Net interface only */
	const char *interface;
	/* Power only. Tracks state. -1 for unknown. */
	int supply_present;
	int charging;
	int voltage;           /* Don't use if 0. */
};
struct hotplug hot = { 0 };
/* struct hotplug is only valid while buffer is unchanged */
int parse_hotplug(struct hotplug *h, char *buffer, int len)
{
	char *name;
	char *value;
	char *subsys;
	int i;
	int power=0; /* 1 SUBSYS=power, 2 battery, 3 charger */

	h->action = h->subsystem = UNKNOWN;

	i = strlen(buffer)+1; /* Skip first entry (sysfs path) */
	while (i<len) {
		/* Separate name & value */
		name=buffer+i;
		i += strlen(name)+1;
		value=strchr(name,'=');
		*value='\0'; value++;
/*
		D("HOT: %s = %s", name,value);
*/
		/* Stupid & inefficient parser. FIXME. */
		if (strcmp("ACTION", name)==0) {
			if (strcmp("add", value)==0) {
				h->action = ACTION_ADD;
			} else if (strcmp("remove", value)==0) {
				h->action = ACTION_REMOVE;
			} else if (strcmp("change", value)==0) {
				h->action = ACTION_CHANGE;
			} else {
				h->action = UNKNOWN;
			}
		} else if (strcmp("SUBSYSTEM", name)==0) {
			subsys=value;
			if (strcmp("power_supply", value)==0) {
				power=1;
			} else if (strcmp("switch", value)==0) {
				h->subsystem = SUBSYS_SWITCH;
			} else if (strcmp("net", value)==0) {
				h->subsystem = SUBSYS_NET;
			} else {
				h->subsystem = UNKNOWN;
			}
		} else if (strcmp("INTERFACE", name)==0) {
			h->interface = value;
		} else if (power==1 && strcmp("POWER_SUPPLY_NAME", name)==0) {
			if (strcmp("battery",value)==0) {
				power=2;
			} else {
				power=3; /* USB or AC */
			}
			h->subsystem = SUBSYS_POWER;
		} else if (power==3 && strcmp("POWER_SUPPLY_ONLINE", name)==0) {
			h->supply_present = atoi(value);
		} else if (power==2 && strcmp("POWER_SUPPLY_STATUS", name)==0) {
			if (strcmp("Discharging", value)==0) {
				h->charging = 0;
			} else {
				h->charging = 1;
			}
		} else if (power==2 && strcmp("POWER_SUPPLY_VOLTAGE_NOW", name)==0) {
			h->voltage = atoi(value);
		}
	}

	switch (h->subsystem) {
		case SUBSYS_POWER:
			/* We don't want to spam the log with voltage updates. */
			if (power==3)
				D("Power: supply=%d, chr=%d, V=%dmV", h->supply_present,
						h->charging, h->voltage);
			break;
		case SUBSYS_SWITCH:
			D("Storage: switched");
			break;
		case SUBSYS_NET:
			D("Net: %s %s", h->action==ACTION_ADD?"added":"removed",
					h->interface);
			break;
		default:
			D("??? (%s)", subsys);
			break;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int v;
	struct sockaddr_nl nls;
	struct pollfd pfd;
	char hotbuf[512];
	int rval;
	int hotlen;

	/* Set up syslog output. */
	openlog(progname, LOG_PID, LOG_LOCAL5);
	setlogmask(LOG_UPTO(LOG_NOTICE+LOG_ERR));
	if (rdb_open_db()==-1) {
		syslog(LOG_INFO, "Failed to open rdb");
		return 1;
	}
	syslog(LOG_INFO, "started");

	/* Catch signals */
	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGTERM, sighandler);

	/* Set up for listening on the netlink socket for hotplug events */
	memset(&nls,0,sizeof(struct sockaddr_nl));
	nls.nl_family = AF_NETLINK;
	nls.nl_pid = getpid();
	nls.nl_groups = -1;
	pfd.events = POLLIN;
	pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (pfd.fd==-1) {
		D("Can't open netlink socket.");
		exit(1);
	}
	/* start listening */
	if (bind(pfd.fd, (void *)&nls, sizeof(struct sockaddr_nl))) {
		D("Bind failed");
	}

	hot.supply_present = battery_usb();
	hot.charging = battery_charging();
	hot.voltage = battery_voltage();

	/*
	This is a temporary feature to invoke LED IOCTLS, for
	driver reverse engineering. May be removed later.
	*/
	if (argc==3)
		init_battery(atoi(argv[1]),atoi(argv[2]));
	else
		init_battery(0,0);

	init_netdevices();

	while (active) {
		rval=poll(&pfd, 1, -1);
		if (rval<=0) {
			D("poll()=%d, errno=%d", rval, errno);
			break;
		}
		hotlen = recv(pfd.fd, hotbuf, sizeof(hotbuf), MSG_DONTWAIT);
		if (hotlen<0) {
			D("recv()=%d", hotlen);
			break;
		}

		parse_hotplug(&hot, hotbuf, hotlen);

		if (hot.subsystem==SUBSYS_POWER) {
			v = poll_battery(hot.supply_present, hot.charging, hot.voltage);
		}

		if (hot.subsystem==SUBSYS_NET) {
			poll_netdevices(hot.interface, (hot.action==ACTION_ADD));
		}
	}
	rdb_close_db();
	syslog(LOG_INFO, "exit");
	closelog();
	return 0;
}
