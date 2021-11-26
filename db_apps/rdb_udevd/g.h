#ifndef __G_H__
#define __G_H__

#define _GNU_SOURCE

#include <syslog.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <stdarg.h> 
#include <wait.h>
#include <sys/types.h>
#include <regex.h>
#include <dirent.h>
#include <ctype.h>

#include <udev.h>
#include <udevd.h>

#define DBG(level,fmt,...)	syslog(level,"<%s:%s:%d> " fmt,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)

#define STRLEN(s)	(sizeof(s)-1)
#define COUNTOF(s)	(sizeof(s)/sizeof(*s))


int search_str_in_array(const char* key,const char** strs,int cnt,int def);

#endif
