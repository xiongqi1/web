#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log.h"
#include "utils.h"
#include "nvram.h"
#include "rdb_comms.h"

#define USPES	1000000
#define IF_NAMESIZE  16

// convert string into data
// if string, start with 0x/0X, treat it as hex string
unsigned long Atoi( const char *pStr, int *valid)
{
    *valid =1;
    long result;
    if(pStr[0] =='0' && (pStr[1] == 'x'|| pStr[1] == 'X'))
    {
        result =  strtoul(&pStr[2], NULL, 16);
    }
    else
    {
        result = strtoul(pStr, NULL, 10);
    }
    if(errno == ERANGE)
    {
        *valid = 0;
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

void CurTime2Str( char *buf )
{
	char new_value[32] = {'\0'};
	struct timeval tv;
	struct timezone tz;
	struct tm *tm;
	gettimeofday(&tv, &tz);
	tm=gmtime(&tv.tv_sec);

	sprintf(new_value, "%4d-%02d-%02dT%02d:%02d:%02d.%06ld", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec);
	strcpy(buf, new_value);

	return;
}

#if defined(PLATFORM_PLATYPUS)

char* getLanIfName(void)
{
	char *mode = nvram_bufget(RT2860_NVRAM, "OperationMode");
	static char *if_name = "br0";

	if (NULL == mode)
		goto fini;
	if (!strncmp(mode, "0", 2))
		if_name = "br0";
	else if (!strncmp(mode, "1", 2)) {
		if_name = "br0";
	}
	else if (!strncmp(mode, "2", 2)) {
		if_name = "eth2";
	}
	else if (!strncmp(mode, "3", 2)) {
		if_name = "br0";
	}

fini:
	nvram_strfree(mode);
	return if_name;
}


char* getWanIfName(void)
{
	char *mode = nvram_bufget(RT2860_NVRAM, "OperationMode");
	static char *if_name = "br0";

	if (NULL == mode)
		goto fini;
	if (!strncmp(mode, "0", 2))
		if_name = "br0";
	else if (!strncmp(mode, "1", 2)) {
		if_name = "eth2.2";
	}
	else if (!strncmp(mode, "2", 2))
		if_name = "ra0";
	else if (!strncmp(mode, "3", 2))
		if_name = "apcli0";

fini:
	nvram_strfree(mode);
	return if_name;
}

char* getWanIfNamePPP(void)
{
    char *cm;
    cm = nvram_bufget(RT2860_NVRAM, "wanConnectionMode");
    int fPPP=!strncmp(cm, "PPPOE", 6) || !strncmp(cm, "L2TP", 5) || !strncmp(cm, "PPTP", 5);

    nvram_strfree(cm);

    if (fPPP)
        return "ppp0";

    return getWanIfName();
}

int is_exist_interface_name(char *if_name)
{
	int fd;
	char device[255];
	sprintf(device, "/sys/class/net/%s", if_name);
	fd = open(device, O_RDONLY, 0);
	if (fd < 0)
		return 0;
	else
		close(fd);
	return 1;
}

int getIfIp(char *ifname, char *if_addr)
{
	struct ifreq ifr;
	int skfd = 0;

	if ( !is_exist_interface_name(ifname) )
	{
		return -1;
	}

	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		return -1;
	}
	
	strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);

	if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0) {
		return -1;
	}
	strcpy(if_addr, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	close(skfd);
	return 0;
}

int getWanIp(char * addr) {
	if((getIfIp(getWanIfNamePPP(), addr)) == -1) {
		SYSLOG_DEBUG("Error, can't get WAN IP. (if=%s)\n",getWanIfNamePPP());
		return -1;
	}
	return 0;
}

int getLanIp(char * addr) {
	if((getIfIp(getLanIfName(), addr)) == -1) {
		SYSLOG_DEBUG("Error, can't get LAN IP. (if=%s)\n",getLanIfName());
		return -1;
	}
	return 0;
}

int get3GwanIp(char * addr) {
	char value[16] = {0,};
	
	if(rdb_get_single("link.profile.1.iplocal", value, sizeof(value)) == 0) {
		if (strlen(value) == 0) {
			SYSLOG_DEBUG("Error, can't get 3GWAN IP. RDB value is not set\n");
			return -1;
		}
		else {
			strncpy(addr, value, sizeof(value));
			return 0;
		}
	}
	SYSLOG_DEBUG("Error, can't get 3GWAN IP. RDB read Error\n");
	return -1;
}
#else
int getWanIp(char * addr) {
	return -1;
}

int getLanIp(char * addr) {
	return -1;
}

int get3GwanIp(char * addr) {
	return -1;
}
#endif