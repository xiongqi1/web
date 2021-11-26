/*
This is a module for the system monitor that watches
the creation and removal of network devices.

At startup, it scans all existing interfaces and
writes them into the 'netinterfaces' rdb variable.
The variable is updated every time an interface
appears or disappears.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <syslog.h>

#include "rdb_ops.h"

#include "netdevices.h"

#define LOG(...) syslog(LOG_INFO, __VA_ARGS__)

/* Maximum length of list of all network devices */
#define MAX_IFLIST 256

static const char *ifpath = "/sys/class/net/";

static char intlist[MAX_IFLIST];

static int get_iflist(char *list, int max)
{
	struct dirent *dp;
	DIR *dir;
	int ofs=0;
	int nifs=0;
	*list='\0';
	dir=opendir(ifpath);
	while((dp=readdir(dir)) != NULL) {
		/* Skip '.' & '..' entries */
		if (dp->d_name[0]=='.') continue;
		ofs+=snprintf(list+ofs, max-ofs, "%s ", dp->d_name);
		nifs++;
	}
	*(list+max-1)='\0';
	return nifs;
}

static void update_db(char *list)
{
	LOG("Net: %s", list);
	rdb_update_single("net.interfaces", list, 0, DEFAULT_PERM, 0, 0);
}

int init_netdevices(void)
{
	int nifs;
	nifs = get_iflist(intlist, MAX_IFLIST);
	update_db(intlist);
	return nifs;
}

int poll_netdevices(const char *interface, int add)
{
	int nifs;
	nifs = get_iflist(intlist, MAX_IFLIST);
	update_db(intlist);
	return nifs;
}
