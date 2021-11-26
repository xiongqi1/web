#ifndef SYSREPO_CLIENT_H__12122017
#define SYSREPO_CLIENT_H__12122017

#include <cdcs_rdb.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <syslog.h>

#define TEST
#ifdef TEST
#include <stdio.h>
#define DBG(level,fmt,...) fprintf(level<LOG_ERR?stderr:stdout,"<%d:%s:%s> " fmt "\n",level,__FILE__,__PRETTY_FUNCTION__,##__VA_ARGS__)
#else
#define DBG(level,fmt,...)	syslog(level,"<%s:%s> " fmt,__FILE__,__FUNCTION__,##__VA_ARGS__)
#endif


#endif /* SYSREPO_CLIENT_H */

