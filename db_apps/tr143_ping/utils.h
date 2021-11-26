#ifndef __UTILS_H
#include <sys/types.h>
#include <sys/time.h>

// convert string into data
// if string, start with 0x/0X, treat it as hex string
unsigned  long Atoi( const char *pStr, int *valid);
// calculate the timeval difference
unsigned int timevaldiff(const struct timeval *starttime, const struct timeval *finishtime);

// calculate the timeval difference
void timeval_plus(struct timeval *t1, const struct timeval *t2);
// calculate the timeval difference
void timeval_add(struct timeval *t1, int delta);

// calculate the timeval difference
void timeval_minus(struct timeval *t1, const struct timeval *t2);

// calculate the timeval difference
void timeval_div(struct timeval *t1, int d);

// calculate the timeval difference
//  t1 >t1 =1
//  t1 ==t1 =0
//  t1 <t1 =-1
int  timeval_cmp(const struct timeval *t1, const struct timeval *t2);

// call gettimeofday, convert to int64_t
int64_t gettimeofday64();

int64_t tv_us(int64_t *tv);

// $0 -- succes
// $-1 -- failed
int lookup_hostname(const char*hostname, unsigned int *ip);
// get local if_index from if name
//$0 --- success
// <0 -- failed
int get_if_index( int *ifIndex, const char *if_name);
// retreive rx/tx bytes from interface
// 1 -- success
// 0 -- failed

int get_if_rx_tx(const char *if_name,  int64_t *rx, int64_t *tx, int *rx_pkts,int *tx_pkts);



#endif
