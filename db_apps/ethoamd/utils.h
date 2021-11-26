#ifndef __UTILS_H
#define __UTILS_H
#include <sys/types.h>
#include <sys/time.h>

#include "cdcs_syslog.h"


// convert string into data
// if string, start with 0x/0X, treat it as hex string
// eg: ""->invalid, "10000000000000"-> invalid(errno == ERANGE) " 111" ->OK
// "100" ->OK " 0" -OK
//err out -- 0-- OK, <0 error
unsigned long Atoi( const char *pStr, int *err);


// call gettimeofday, convert to int64_t
int64_t gettimeofday64();

// call gettimeofday, convert to ms
int gettimeofdayMS();

// get local mac address and the interface index
//$0 --- success
// <0 -- failed
int get_local_mac( u_char *addr, int *ifIndex, const char *if_name);

/*--------------------------------------------------------------------------*/
/* A simple utility routine that converts a MAC-address character string of
 * the form "12:34:56:78:AB:CD" to a 6-byte data array.
 */
int str2MAC(unsigned char *macAdr, const char *str);

//convert MAC-address into string format
//the 6-bytes data array "12,34,56,78,AB,CD" to "12:34:56:78:AB:CD"
void MAC2str(char *str, const unsigned char *mac);


static inline void print_mac( const char *pTag, const unsigned char *mac)
{
	NTCLOG_DEBUG("%s%02x:%02x:%02x:%02x:%02x:%02x", pTag, mac[0],
			 mac[1],mac[2],mac[3],
			 mac[4],mac[5]);
};
#ifdef _MULTI_MEP

#define NTCSLOG_DEBUG(fmt,...) 		NTCLOG_DEBUG("%d:"fmt,pSession->m_node, ##__VA_ARGS__)
#define NTCSLOG_WARNING(fmt,...) 	NTCLOG_WARNING("%d:"fmt,pSession->m_node, ##__VA_ARGS__)
#define NTCSLOG_INFO(fmt,...)		NTCLOG_INFO("%d:"fmt,pSession->m_node, ##__VA_ARGS__)
#define NTCSLOG_ERR(fmt,...)		NTCLOG_ERR("%d:"fmt,pSession->m_node, ##__VA_ARGS__)

#else
#define NTCSLOG_DEBUG 	NTCLOG_DEBUG
#define NTCSLOG_WARNING NTCLOG_WARNING
#define NTCSLOG_INFO	NTCLOG_INFO
#define NTCSLOG_ERR		NTCLOG_ERR
#endif

#endif
