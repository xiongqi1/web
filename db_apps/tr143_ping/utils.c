#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "log.h"

#include "utils.h"

#define USPES	1000000

// convert string into data
// if string, start with 0x/0X, treat it as hex string
// eg: ""->invalid, "10000000000000"-> invalid(errno == ERANGE) " 111" ->OK
// "100" ->OK " 0" -OK
//valid out -- 1-- OK, 0 error
unsigned long Atoi( const char *pStr, int *valid)
{
    long result;
    char*pEnd;
    if(valid)*valid =1;
    // long int strtoul(const char *sptr, char **endptr, int base);
	// if the  third parameter is 0,
    // it strtol will attempt to pick the base from pStr. Only Dec, Oct Hex supported.
    errno=0;// must be reset, otherwise, it may remain as error, even if the conversion success
	result = strtoul(pStr, &pEnd, 0);
	if(errno == ERANGE || ( pStr == pEnd) || result <0)
	{
		if(valid)*valid = 0;
	}
    return result;
}

// calculate the timeval difference
// return milisecond
unsigned int timevaldiff(const struct timeval *starttime, const struct timeval *finishtime)
{
  unsigned int msec;
  msec=(finishtime->tv_sec-starttime->tv_sec)*1000;
  msec+=(finishtime->tv_usec-starttime->tv_usec)/1000;
  return msec;
}
// calculate the timeval difference
void timeval_plus(struct timeval *t1, const struct timeval *t2)
{
	t1->tv_sec += t2->tv_sec;
	t1->tv_usec+= t2->tv_usec;
	if(t1->tv_usec >USPES)
	{
		t1->tv_usec-=USPES;
		t1->tv_sec++;
	}
}

// calculate the timeval difference
void timeval_add(struct timeval *t1, int delta)
{

	if(delta >0)
	{
		t1->tv_usec+= delta;
		if(t1->tv_usec >USPES)
		{
			t1->tv_sec += t1->tv_usec/USPES;
			t1->tv_usec%= USPES;
		}
	}
	else
	{
		int64_t t = (int64_t)t1->tv_sec*USPES + t1->tv_usec;
		t -= delta;
		t1->tv_sec = (int) (t/USPES);
		t1->tv_usec = (int) (t%USPES);
	}
}

// calculate the timeval difference
void timeval_minus(struct timeval *t1, const struct timeval *t2)
{
	if(t1->tv_usec < t2->tv_usec)
	{
		t1->tv_usec +=USPES;
		t1->tv_sec--;

	}
	t1->tv_usec -= t2->tv_usec;
	t1->tv_sec -= t2->tv_sec;
}
// calculate the timeval difference
void timeval_div(struct timeval *t1, int d)
{
	int64_t t = (int64_t)t1->tv_sec*USPES + t1->tv_usec;
	if(d >0)
	{
		t /=d;
		t1->tv_sec = (int) (t/USPES);
		t1->tv_usec = (int) (t%USPES);
	}

}

// calculate the timeval difference
//  t1 >t2 =1
//  t1 ==t2 =0
//  t1 <t2 =-1
int  timeval_cmp(const struct timeval *t1, const struct timeval *t2)
{
	if(t1->tv_sec > t2->tv_sec)
	{
		return 1;
	}
	else if(t1->tv_sec < t2->tv_sec)
	{
		return -1;
	}

	if(t1->tv_usec > t2->tv_usec)
	{
		return 1;
	}
	else if(t1->tv_usec < t2->tv_usec)
	{
		return -1;
	}
	return 0;


}

// call gettimeofday, convert to int64_t
int64_t gettimeofday64()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t)tv.tv_sec*USPES+tv.tv_usec;
}


int lookup_hostname(const char*hostname, unsigned int *ip)
{
	struct hostent *hp;

	hp = gethostbyname(hostname);            /* Server IP Address */
    if (hp==0)
    {
        return -1;
    }
	memcpy(ip, &hp->h_addr, 4);
	return 0;
}


// get local if_index from if name
//$0 --- success
// <0 -- failed
int get_if_index( int *ifIndex, const char *if_name)
{
    /* implementation for Linux */
    struct ifreq  ifri;
    int s;
    int ok = 0;

    //s = socket(AF_INET, SOCK_DGRAM, 0);
    s = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (s==-1)
    {
        return -1;
    }
    if(if_name)
    {

        strcpy(ifri.ifr_name, if_name);
        ok = ioctl(s,  SIOCGIFINDEX, &ifri) ==0;

    }
    close(s);

    if (ok)
    {
        *ifIndex = ifri.ifr_ifindex;
    }
    else
    {
        return -1;
    }
    return 0;
}

#define _PATH_PROCNET_DEV               "/proc/net/dev"


// retreive rx/tx bytes/packes from interface
// 1 -- success
// 0 -- failed

int get_if_rx_tx(const char *if_name,  int64_t *rx, int64_t *tx, int *rx_pkts,int *tx_pkts)
{

	FILE *fh;
	char buf[512];
	*rx =0;
	*tx =0;
	*rx_pkts =0;
	*tx_pkts =0;
	fh = fopen(_PATH_PROCNET_DEV, "r");
	if (!fh) {
		return 0;
	}
	fgets(buf, sizeof buf, fh);	/* eat line */
	fgets(buf, sizeof buf, fh);

	while (fgets(buf, sizeof buf, fh)) {
		char name[128];
		int t;
		char *p = strchr(buf, ':');
		if(p) *p=' ';
		//printf("line :%s", buf);
		//face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
		 if (sscanf(buf,
					"%s%llu%u%u%u%u%u%u%u%llu%u",
				   name,
				   rx, //rx_bytes, /* missing for 0 */
				   rx_pkts, //rx_packets,
				   &t, //rx_errors,
				   &t, //rx_dropped,
				   &t, //rx_fifo_errors,
				   &t, //rx_frame_errors,
				   &t, //rx_compressed, /* missing for <= 1 */
				   &t, //rx_multicast, /* missing for <= 1 */
				   tx, //tx_bytes, /* missing for 0 */
				   tx_pkts
				   //&tx_errors,
				   //&tx_dropped,
				   //&tx_fifo_errors,
				   //&collisions,
				   //&tx_carrier_errors,
				   //&tx_compressed /* missing for <= 1 */
		   )!= 11)continue;
		if (strcmp(if_name, name) ==0)
		{
 		   //printf("%s, %llu, %llu, %u, %u\n", name, *rx, *tx, *rx_pkts,  *tx_pkts);
			fclose(fh);
			return 1;
		}
	}
	fclose(fh);
	return 0;
}
