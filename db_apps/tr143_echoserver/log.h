#ifndef __LOG_H
#define __LOG_H
#include <syslog.h>
#ifdef _DEBUG
#include <stdio.h>
#define SYSLOG_ERR(...)     printf(__VA_ARGS__); putchar('\n')
#define SYSLOG_INFO(...)    printf(__VA_ARGS__); putchar('\n')
#define SYSLOG_DEBUG(...)   printf(__VA_ARGS__); putchar('\n')
#else
#include <syslog.h>

#define SYSLOG_ERR(...)     syslog(LOG_ERR, __VA_ARGS__)
#define SYSLOG_INFO(...)    syslog(LOG_INFO, __VA_ARGS__)
#define SYSLOG_DEBUG(...)   syslog(LOG_DEBUG, __VA_ARGS__)


#endif



#endif
