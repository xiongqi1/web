#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include "log.h"

#include "utils.h"



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
unsigned int timevaldiff(const struct timeval *starttime, const struct timeval *finishtime)
{
    unsigned int usec;
    usec=(finishtime->tv_sec-starttime->tv_sec)*1000000;
    usec+=(finishtime->tv_usec-starttime->tv_usec);
    return usec;
}
// call gettimeofday, convert to ms
int64_t gettimeofdayMS()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv_MS(&tv);
}
// call gettimeofday, convert to xs
int64_t gettimeofdayXS()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv_xs(&tv);
}


// $0 -- succes
// $-1 -- failed
int lookup_hostname(const char*hostname, unsigned int *ip)
{
	if(strlen(hostname) ==0) return -1;
	if(inet_aton(hostname, (struct in_addr*)ip))
	{

		return 0;
	}
	struct hostent *hp;
	hp = gethostbyname(hostname);            /* Server IP Address */
	if (hp==0)
	{
		return -1;
	}
	memcpy(ip, hp->h_addr, 4);
	if(ip ==0) return -1;
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
// UTC time format
//For example: 2008-04-09T15:01:05.123456
// format time value into string
char* format_time(char *time_string, int str_len, struct timeval tv)
{

    struct tm* ptm;

    char tmp_str[10];

    ptm = gmtime(&tv.tv_sec);

    /* Format the date and time, down to a single second. */
    strftime (time_string, str_len, "%Y-%m-%d %H:%M:%S", ptm);
	//NTCLOG_DEBUG(time_string);

    /* Compute milliseconds from microseconds. */
	sprintf(tmp_str, ".%ld",tv.tv_usec / 1000);
	strcat(time_string, tmp_str);
	return time_string;
}

/// split url into host, port
// eg: http://tr143.tidalfusion.com.au:5000/?size=500
int split_url(const char*url, char* host, int max_host, unsigned int *port)
{
	const char *p1, *p2;
	int len;
	*port =80;

	
	p1 = strstr(url, "//");
	if (p1)
	{
		p1 += 2;
	}
	else
	{
		p1 = url;
	}
	
	//host start
	// has port ? :

	p2 = strchr(p1, ':');

	if(p2 != NULL)
	{
		len = p2-p1;
		*port = atoi(p2+1);
		if(len >= max_host) return -1;

		strncpy(host, p1, len);
		host[len] =0;
		return 0;
	}
	// has path
	len = strcspn(p1, "/?&");
//	NTCLOG_DEBUG("len=%d", len);

	// '/test' => 0, "Test"=>4
	if(len >= max_host) return -1;
	strncpy(host, p1, len);
	host[len] =0;
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
