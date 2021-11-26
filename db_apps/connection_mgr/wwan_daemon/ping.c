#include <stdio.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip_icmp.h>
#include <time.h>

#include <errno.h>
#include <syslog.h>
#include <linux/if.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "wwan_daemon.h"

//static char* timeString(char* buf, unsigned int maxlen)
//{
//	char* cp;
//	char tbuf[30];
//	time_t now;
//	time(&now);
//	ctime_r(&now, tbuf);
//	if ((cp = strchr(tbuf, '\n')) != NULL)
//		* cp = '\0';
//	if ((cp = strchr(tbuf, '\r')) != NULL)
//		* cp = '\0';
//	strncpy(buf, tbuf, maxlen - 1);
//	buf[maxlen - 1] = '\0';
//	return buf;
//}

static int getChkSum(unsigned short* buf, int sz)
{
	int nleft = sz;
	int sum = 0;
	unsigned short* w = buf;
	unsigned short ans = 0;

	while (nleft > 1)
	{
		sum += * w++;
		nleft -= 2;
	}

	if (nleft == 1)
	{
		*(unsigned char*)(&ans) = *(unsigned char*)w;
		sum += ans;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	ans = ~sum;
	return (ans);
}


static int _hSck = -1;
static char _achDev[256] = {0, };
static short int _icmpIdShaker=0;


/*
#define DEFDATALEN	56
#define MAXIPLEN		60
#define MAXICMPLEN	76

192 + 34 = 226 

static char _pckBuf[DEFDATALEN+MAXIPLEN+MAXICMPLEN];
*/

#define DEFDATALEN	56
#define IPHDRLEN	20
#define ICMPHDRLEN	8

static char _pckBuf[DEFDATALEN+IPHDRLEN+ICMPHDRLEN];
static int _iBufIdx = 0;

extern int is_exist_interface_name(char *iname);

////////////////////////////////////////////////////////////////////////////////
int ping_recv()
{
	int nRecv = -1;

	if (_hSck < 0)
		goto fini;

	// select
	while (1)
	{
		// build fdset for reading
		fd_set fdsR;
		FD_ZERO(&fdsR);
		FD_SET(_hSck, &fdsR);
	
		// build timeout
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		int stat = select(_hSck + 1, &fdsR, (fd_set*)0, (fd_set*)0, &tv);

		// if timeout or error
		if (stat <= 0)
			break;

		// define from address
		struct sockaddr_in from;
		size_t fromlen = sizeof(from);

		// recv
		int cbLeft = sizeof(_pckBuf) - _iBufIdx;
		int cbRecv = recvfrom(_hSck, &_pckBuf[_iBufIdx], cbLeft, 0, (struct sockaddr*) & from, &fromlen);
		if (cbRecv < 0) {
			syslog(LOG_ERR,"recvfrom() failed - %s",strerror(errno));
			continue;
		}
		else if(cbRecv == 0) {
			continue;
		}

		_iBufIdx += cbRecv;

		// bypass if too small
		if (_iBufIdx < 76)
			continue;

		_iBufIdx = 0;

		// get ip header
		struct iphdr* pIpHdr = (struct pIpHdr*)_pckBuf;

		// get icmp header
		struct icmp* pIcmpPck = (struct icmp*)(_pckBuf + (pIpHdr->ihl << 2));

		if (nRecv < 0)
			nRecv = pIcmpPck->icmp_type == ICMP_ECHOREPLY;
	}

fini:
	if(nRecv>0)
		_icmpIdShaker=0;

	return nRecv;
}


#include <setjmp.h>
#include <signal.h>


////////////////////////////////////////////////////////////////////////////////
static sigjmp_buf env;

////////////////////////////////////////////////////////////////////////////////
static void sighandler(int signum)
{
	siglongjmp(env, 1);
}
////////////////////////////////////////////////////////////////////////////////
static struct hostent* dnslookup(const char* szHost,unsigned int timeout)
{
	struct hostent* pHostEnt=NULL;

	signal(SIGALRM, sighandler);
	
	if (!sigsetjmp(env,1))
	{
		alarm(timeout);
		pHostEnt = gethostbyname(szHost);
		alarm(0);
	}

	signal(SIGALRM, SIG_IGN);

	return pHostEnt;
}

static void get_mono(struct timespec *ts)
{
	if( clock_gettime(CLOCK_MONOTONIC, ts) < 0 )
		syslog(LOG_ERR, "clock_gettime(MONOTONIC) failed");
}

unsigned long long monotonic_us(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec * 1000000ULL + ts.tv_nsec/1000;
}

////////////////////////////////////////////////////////////////////////////////
int ping_sendto(const char* szHost,int timeout)
{
	if (_hSck < 0)
		goto error;

	int stat;

	static char szPrevHost[256] = {0, };
	static struct in_addr sin_addr;

	if (!strlen(szPrevHost) || strcmp(szPrevHost, szHost))
	{
		struct hostent* pHostEnt = dnslookup(szHost,timeout);
		if (!pHostEnt)
		{
			syslog(LOG_ERR, "failed to get host address - host=%s,err=%s", szHost, strerror(errno));
			goto error;
		}

		memcpy(&sin_addr, pHostEnt->h_addr_list[0], sizeof(sin_addr));
		strcpy(szPrevHost, szHost);
	}

	// build ping address
	struct sockaddr_in pingAddr;
	memset(&pingAddr, 0, sizeof(struct sockaddr_in));
	pingAddr.sin_family = AF_INET;
	memcpy(&pingAddr.sin_addr, &sin_addr, sizeof(pingAddr.sin_addr));

	static unsigned short ntransmitted=0;
	char pckSendBuf[ICMPHDRLEN+DEFDATALEN];

	// build ping packet
	struct icmp* pIcmpPck;
	pIcmpPck = (struct icmp*)pckSendBuf;
	memset(pIcmpPck, 0, sizeof(pckSendBuf));
	pIcmpPck->icmp_type = ICMP_ECHO;
	pIcmpPck->icmp_seq = htons(ntransmitted++);
	// we have to use a different icmp ID while ping fails, because some stupid intelligent NATs keep
	// ignoring ICMP packets with the same id in a row after a single ping fails
	// this can cause ping conflict once to a process but no other work-around now
	pIcmpPck->icmp_id = htons(getpid()+_icmpIdShaker++);

	*(uint32_t*)&pIcmpPck->icmp_dun = monotonic_us();
	pIcmpPck->icmp_cksum = getChkSum((unsigned short*)pIcmpPck, sizeof(pckSendBuf));

	// send
	stat = sendto(_hSck, pckSendBuf, sizeof(pckSendBuf), 0, (struct sockaddr*) & pingAddr, sizeof(struct sockaddr_in));
	if (stat < 0)
	{
		syslog(LOG_ERR, "failed to send icmp packet - host=%s,err=%s", szHost, strerror(errno));
		goto error;
	}


	return 0;

error:
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
void ping_fini()
{
	if (_hSck >= 0)
		close(_hSck);

	_hSck=-1;
}

////////////////////////////////////////////////////////////////////////////////
int ping_isInit()
{
	return _hSck>0;
}
////////////////////////////////////////////////////////////////////////////////
int ping_isIfUp(const char* szIfNm)
{
	struct ifreq ifr;
	int sockfd;

	if ( !is_exist_interface_name((char *)szIfNm) )
	{
		return 0;
	}

	// NOTE - that this socket is just created to be used as a hook in to the socket layer
	// for the ioctl call.
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd > 0)
	{
		char szRIfNm[64];
		strcpy(szRIfNm,szIfNm);
		char* pDot=strchr(szRIfNm,'.');
		if(pDot)
			pDot=0;

		strcpy(ifr.ifr_name, szRIfNm);

		if (ioctl(sockfd, SIOCGIFINDEX, &ifr) !=  - 1)
		{
			if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) ==  - 1)
			{
				close(sockfd);
				//fprintf(stderr, "returning 0 in second ioctl call\r\n");
				return 0;
			}
			else
			{
				if (ifr.ifr_flags &IFF_UP)
				{
					//fprintf(stderr, "PPP UP\r\n");
					close(sockfd);
					return 1;
				}
				else
				{
					close(sockfd);
					return 0;
				}
			}
		}
	}

	close(sockfd);

	return 0;
}


////////////////////////////////////////////////////////////////////////////////
int ping_init(const char* szDev)
{

	if (szDev)
		strcpy(_achDev, szDev);
	else
		_achDev[0] = 0;

	// open socket
	_hSck = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (_hSck < 0)
	{
		syslog(LOG_ERR, "failed to open socket");
		goto error;
	}

	_icmpIdShaker=0;

	int stat;

	int len = strlen(_achDev);

	// bind to the specific interface
	if (len)
	{
		stat = setsockopt(_hSck, SOL_SOCKET, SO_BINDTODEVICE, _achDev, len+1);
		if (stat < 0)
		{
			syslog(LOG_ERR, "failed to bind to interface - dev=%s,err=%s", _achDev, strerror(errno));
			goto error;
		}
	}

	return 0;

error:
	ping_fini();

	return -1;
}

