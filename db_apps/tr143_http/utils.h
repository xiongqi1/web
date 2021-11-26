#ifndef __UTILS_H
#include <sys/types.h>
#include <sys/time.h>
#define USPES	1000000
#define MSPES	1000
#define USPEMS	1000

#define XSPES 1000
#define USPEXS 1000

 struct in_addr;

// convert string into data
// if string, start with 0x/0X, treat it as hex string
// eg: ""->invalid, "10000000000000"-> invalid(errno == ERANGE) " 111" ->OK
// "100" ->OK " 0" -OK
//err out -- 0-- OK, <0 error
unsigned long Atoi( const char *pStr, int *err);

// calculate the timeval difference
unsigned int timevaldiff(const struct timeval *starttime, const struct timeval *finishtime);

// call gettimeofday, convert to ms
int64_t gettimeofdayMS();

static inline int64_t tv_MS(struct timeval *tv)
{
	return (int64_t)tv->tv_sec*MSPES+tv->tv_usec/USPEMS;
}
// convert tv to 0.1s resolution
static inline int64_t tv_xs(struct timeval *tv)
{
	return (int64_t)tv->tv_sec*XSPES+tv->tv_usec/USPEXS;
}
static inline int64_t tv_us(struct timeval *tv)
{
	return (int64_t)tv->tv_sec*USPES+tv->tv_usec;
}

// call gettimeofday, convert to xs
int64_t gettimeofdayXS();

// $0 -- succes
// $-1 -- failed
int lookup_hostname(const char*hostname,  unsigned int *ip);

// get local if_index from if name
//$0 --- success
// <0 -- failed
int get_if_index( int *ifIndex, const char *if_name);

// UTC time format
//For example: 2008-04-09T15:01:05.123456
// format time value into string
char* format_time(char *time_string, int str_len, struct timeval tv);

// retreive rx/tx bytes from interface
// 1 -- success
// 0 -- failed

int get_if_rx_tx(const char *if_name,  int64_t *rx, int64_t *tx, int *rx_pkts,int *tx_pkts);


/// split url into host, port
int split_url(const char*url, char* host, int max_host,  unsigned int *port);

#endif
