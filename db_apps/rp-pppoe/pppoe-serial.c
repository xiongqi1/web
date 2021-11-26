/***********************************************************************
*
* pppoe-serial.c
*
* Implementation of a user-space PPPoE server
*
* Copyright (C) 2000 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
*
***********************************************************************/

#include "config.h"

#include <pthread.h>

#if defined(HAVE_NETPACKET_PACKET_H) || defined(HAVE_LINUX_IF_PACKET_H)
#define _POSIX_SOURCE 1 /* For sigaction defines */
#endif

#define _BSD_SOURCE 1 /* for gethostname */

#include "pppoe.h"
#include "md5.h"
// #include "../modem_emulator/CDCS_ModemEmulator.h"
#define usb_port  "/dev/phone"

//#ifdef HAVE_SYSLOG_H
#include <syslog.h>
//#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/ioctl.h>

/* Hack for daemonizing */
#define CLOSEFD 64

/* Socket for client's discovery phases */
int Socket = -1;

/* Random seed for cookie generation */
#define SEED_LEN 16
unsigned char CookieSeed[SEED_LEN];

/* Default interface if no -I option given */
#define DEFAULT_IF "eth0"
char* IfName = NULL;

/* Access concentrator name */
char* ACName = NULL;

char* TtyPortName = NULL;

char* DialCommand = NULL;

int DiscoveryState;

int DiscoverySocket = -1;
int SessionSocket = -1;
int TtyPort;
FILE* TTYportFile;

unsigned char MyEthAddr[ETH_ALEN];      /* Source hardware address */
unsigned char PeerEthAddr[ETH_ALEN]= {0,};   /* Destination hardware address */
unsigned char ZeroEthAddr[ETH_ALEN]= {0,};   /* Destination hardware address */
char* apnName = NULL;
char using_apn = 0;
UINT16_t Session = 0;
UINT16_t _nextSession = 0;

char *requiredServiceName;

FILE *DebugFile = NULL;        /* File for dumping debug output */

int optClampMSS = 0;        /* Clamp MSS to this value */
int optInactivityTimeout = 1;    /* Inactivity timeout */

// pppoe packet chopping
static unsigned int _bogusIP = 64 << (0 * 8) | 64 << (1 * 8) | 64 << (2 * 8) | 64 << (3 * 8);
const unsigned int _sierraBogusIP = 10 << (0 * 8) | 11 << (1 * 8) | 12 << (2 * 8) | 13 << (3 * 8);

static unsigned int _fIgnoreRequestUntilReady=1;
static unsigned int _fForceNumberedIpAddr=1;

static int terminate=0;


pthread_t writeSer_info;

struct PPPoETag iq;
struct PPPoETag relayId;
struct PPPoETag receivedCookie;
struct PPPoETag requestedService;
struct PPPoETag hostUniq= {0,};

struct PPPoETag cookie;        /* We have to send this if we get it */

int use_usb_port = 0;

#define FOURCC(a,c,b,d)     ( (unsigned)a<<0 | (unsigned)b<<8 | (unsigned)c<<16 | (unsigned)d<<24 )

#define PPPOE_TIMEOUT_DISCOVERY             5
#define PPPOE_TIMEOUT_REQECHO               12      // 12 secs - a bit longer than CISCO 831 that sends an echo request every 10 seconds
#define PPPOE_TIMEOUT_SESSION               15      // 1 min   - a bit longer than CISCO 831 that assumes the connection is dropped after 50 seconds

const unsigned int _dwHeartBeatMagic=FOURCC('m','Y','m','a');
const unsigned int _dwHeartBeatMessg=FOURCC('M','y','M','e');

int sendLCP_TermReq(void);

time_t _tmHeartBeat=-1;

#define HOSTNAMELEN 256

static void sendErrorPADS(int sock, unsigned char *source, unsigned char *dest,
                          int errorTag, char *errorMsg);

void genCookie(unsigned char const *peerEthAddr, unsigned char const *myEthAddr,
               unsigned char const *seed, unsigned char *cookie);

#define CHECK_ROOM(cursor, start, len) do {if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) { syslog(LOG_ERR, "Would create too-long packet"); return; } } while(0)
#define CHECK_ROOM2(cursor, start, len) do {if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) { syslog(LOG_ERR, "Would create too-long packet"); return 0; } } while(0)

void reopenModule(void);

///////////////////////////////////////////////////////////////////////////////

#include "rdb_ops.h"

#define DATABASE_PPPOE_STATUS		"pppoe.server.0.status"
#define DATABASE_PPPOE_IP				"pppoe.server.0.ipaddress"

typedef enum {
	pppoestat_init,pppoestat_idle,pppoestat_online,pppoestat_discovery
} pppoestat;

///////////////////////////////////////////////////////////////////////////////
int databaseWriteIpAddress(const char* szIpAddr)
{
	if(!szIpAddr)
		szIpAddr="0.0.0.0";

	return rdb_set_single(DATABASE_PPPOE_IP,szIpAddr);
}
///////////////////////////////////////////////////////////////////////////////
int databaseWritePPPoEStat(pppoestat pppoeStat)
{
	int hFd=rdb_get_fd();
	if(hFd<0)
		return hFd;

	const char* pppoeStatTbl[]= {
		"Preparing",
		"Idle",
		"Online",
		"Discovery"
	};

	return rdb_set_single(DATABASE_PPPOE_STATUS,pppoeStatTbl[pppoeStat]);
}

int SendATCommand(FILE* pPort, char* pReq, char* pResp, int resp_size, int timeout)
{
	u_char             inbuf [128];
	int                got;
	struct timeval     tv;
	fd_set             readset;
	int                fd;
	int                ret;
	int       resp_index = 0;


/*
	
	Do not change ATE status due to the following reason
	
	As Cinterion PHP-8 has global ECHO setting, changing ECHO setting in data port changes ECHO setting in AT port changed and makes 
	ATMGR not work. There must be more modules that act as PHP-8.
	
	AT response in PPPoE is not used for any purpose. so it is okay not to set up ECHO setting
	
	// Set the phone to no echo.
	// NOTE - each individual AT command sends a no echo as part of the command since the application
	// does the echoing so we don't want the echo from the phone too. However, we have to set no echo
	// here otherwise the first AT command (i.e ATE0+CSQ?) will be echoed! We don't want this to be echoed.
	fputs("ATE0\r", pPort);
	fflush(pPort);
	usleep(500000);
	// drop response on the floor
	// NOTE - we have to do the read to flush out what is in comm_phone otherwise
	// upon doing the next read we will read this response plus the response to the next read.
	fread(inbuf, 1, 127, TTYportFile);
*/
	
	if (pResp)
		*pResp = 0;

	if (pPort == NULL) {
		if (DebugFile) {
			fprintf(DebugFile, "pPort is NULL\n");
		}
		return -1;
	}

	fputs(pReq, pPort);
	fputs("\r", pPort);
	fflush(pPort);
	
	// quick work-around for PHP-8 - too slow reply
	sleep(1);

	fd = fileno(pPort);

	if (DebugFile) {
		fprintf(DebugFile, "called send at command: request is %s\n", pReq);
	}

	while (1) {
		FD_ZERO(&readset);
		FD_SET(fd, &readset);
		tv.tv_sec = 0;
		tv.tv_usec = timeout * 1000;
		//fprintf(stderr, "about to call select with timeout %ld and fd %d\n",tv.tv_usec,fd);
		ret = select(fd + 1, &readset, (fd_set*) 0, (fd_set*) 0, &tv);

		if (DebugFile) {
			fprintf(DebugFile, "called select,ret is %d\n", ret);
		}

		// check for a timeout.........
		if (ret == 0) {
			if (DebugFile) {
				fprintf(DebugFile, "timeout \n");
			}
			return -1;
		}

		// Normal Return
		if (ret != -1) {
			//fprintf(stderr, "about to call fread\n");
			// NOTE - the phone port is set up for non-blocking so we don't have to
			// wait for PASSTHRUBUFFSIZE - 1 bytes to be read from the stream.
			// Otherwise it will block until this many bytes are read.
			if ((got = fread(inbuf, 1, 127, TTYportFile)) > 0) {
				if (DebugFile) {
					fprintf(DebugFile, "called fread and got is %d \n", got);
				}
				if (pResp) {
					// we need to do a range check here....
					strncpy(pResp + resp_index, (const char*)inbuf, got);
					resp_index += got;
				}

				// NOTE - for PPPoE dial command we should only get a CONNECT response
				if (strstr((const char*)inbuf, "CONNECT")) {
					if (DebugFile) {
						fprintf(DebugFile, "got correct response which is %s \n", inbuf);
					}
					return 1;
				}

				if (strstr((const char*)inbuf, "OK")) {
					//fprintf(stderr, "got correct response which is %s \n",inbuf);
					return 1;
				}

				if (DebugFile) {
					fprintf(DebugFile, "got  a response which is %s \n", inbuf);
				}

				return -1;
			} else {
				if (DebugFile) {
					fprintf(DebugFile, "got is %d\n and error code is %d", got, errno);
				}
				return -1;
			}
		} else {
			if (DebugFile) {
				fprintf(DebugFile, "Select error in SendATCommand\n");
			}
			return -1;
		}
	}
}

int SendATCommandRetry(FILE *pPort, char* pReq, int timeout, int retries)
{
	while (retries--) {
		//fprintf(stderr, "timeout is %d\n",timeout);
		//fflush(stderr);
		if (SendATCommand(pPort, pReq, NULL, 0, timeout) == 1) {
			//fprintf(stderr, "send at command success!!\n");
			//fflush(stderr);
			return 1;
		}
	}
	return -1;
}

void configModemPort(int sport)
{
	struct termios   tty_struct;

	tcgetattr(sport, &tty_struct);

	cfgetispeed(&tty_struct);
	cfgetospeed(&tty_struct);

	/* set up for raw operation */

	tty_struct.c_lflag = 0;       /* no local flags */
	tty_struct.c_oflag &= ~OPOST; /* no output processing */
	tty_struct.c_oflag &= ~ONLCR; /* don't convert line feeds */

	/*
	   Disable input processing: no parity checking or marking, no
	   signals from break conditions, do not convert line feeds or
	   carriage returns, and do not map upper case to lower case.
	*/
	tty_struct.c_iflag &= ~(INPCK | PARMRK | BRKINT | INLCR | ICRNL | IUCLC | IXANY);

	/* ignore break conditions */
	tty_struct.c_iflag |= IGNBRK;

	/*
	    Enable input, and hangup line (drop DTR) on last close.  Other
	    c_cflag flags are set when setting options like csize, stop bits, etc.
	*/
	tty_struct.c_cflag |= (CREAD | HUPCL);
//    tty_struct.c_cflag |= CRTSCTS;         /* enable RTS/CTS flow */

	/*
	   now set up non-blocking operation -- i.e. return from read
	   immediately on receipt of each byte of data
	*/
	tty_struct.c_cc[VMIN] = 1;
	tty_struct.c_cc[VTIME] = 1;

	cfsetospeed(&tty_struct, B230400);        /* set output to */
	cfsetispeed(&tty_struct, B230400);        /* set input to */

	tcsetattr(sport, TCSADRAIN, &tty_struct);    /* set termios struct in driver */

	fcntl(sport, F_SETFL, O_NONBLOCK);
}

/**********************************************************************
*%FUNCTION: parsePADITags
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks interesting tags out of a PADI packet
***********************************************************************/
void
parsePADITags(UINT16_t type, UINT16_t len, unsigned char *data,
              void *extra)
{
	switch (type) {
	case TAG_SERVICE_NAME:
		requestedService.type = htons(type);
		requestedService.length = htons(len);
		memcpy(requestedService.payload, data, len);
		break;

	case TAG_RELAY_SESSION_ID:
		relayId.type = htons(type);
		relayId.length = htons(len);
		memcpy(relayId.payload, data, len);
		break;

	case TAG_HOST_UNIQ:
		hostUniq.type = htons(type);
		hostUniq.length = htons(len);
		memcpy(hostUniq.payload, data, len);
		break;
	}
}

/**********************************************************************
*%FUNCTION: parsePADRTags
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks interesting tags out of a PADR packet
***********************************************************************/
void
parsePADRTags(UINT16_t type, UINT16_t len, unsigned char *data,
              void *extra)
{
	switch (type) {
	case TAG_RELAY_SESSION_ID:
		relayId.type = htons(type);
		relayId.length = htons(len);
		memcpy(relayId.payload, data, len);
		break;

	case TAG_HOST_UNIQ:
		hostUniq.type = htons(type);
		hostUniq.length = htons(len);
		memcpy(hostUniq.payload, data, len);
		break;

	case TAG_AC_COOKIE:
		receivedCookie.type = htons(type);
		receivedCookie.length = htons(len);
		memcpy(receivedCookie.payload, data, len);
		break;

	case TAG_SERVICE_NAME:
		requestedService.type = htons(type);
		requestedService.length = htons(len);
		memcpy(requestedService.payload, data, len);
		break;
	}
}

/**********************************************************************
*%FUNCTION: parseLogErrs
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks error tags out of a packet and logs them.
***********************************************************************/
void
parseLogErrs(UINT16_t type, UINT16_t len, unsigned char *data,
             void *extra)
{
	switch (type) {
	case TAG_SERVICE_NAME_ERROR:
		syslog(LOG_ERR, "PADT: Service-Name-Error: %.*s", (int) len, data);
		break;

	case TAG_AC_SYSTEM_ERROR:
		syslog(LOG_ERR, "PADT: System-Error: %.*s", (int) len, data);
		break;

	case TAG_GENERIC_ERROR:
		syslog(LOG_ERR, "PADT: Generic-Error: %.*s", (int) len, data);
		break;
	}
}

/**********************************************************************
*%FUNCTION: processPADI
*%ARGUMENTS:
* sock -- Ethernet socket
* myAddr -- my Ethernet address
* packet -- PPPoE PADI packet
* len -- length of received packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADO packet back to client
***********************************************************************/
int processPADI(int sock, unsigned char *myAddr,
                struct PPPoEPacket *packet, int len)
{
	struct PPPoEPacket pado;
	struct PPPoETag acname;
	// struct PPPoETag servname;
	struct PPPoETag cookie;
	size_t acname_len;
	struct stat statbuf;
	int    ret;
	size_t sername_len;
	char sername[256];

	unsigned char *cursor = pado.payload;
	UINT16_t plen;

	if (DebugFile) {
		fprintf(DebugFile, "Got PADI\n");
	}

	syslog(LOG_DEBUG, "got PADI \n");

	if (use_usb_port) {
		// wait until the USB port is there (since if resetting the module it will not be valid)
		while (1) {
			usleep(2000000);

			ret = stat(usb_port, &statbuf);

			if (DebugFile) {
				fprintf(DebugFile, "ret from stat for usb port is %d\n", ret);
			}

			if (ret >= 0) {
				// the USB port may not be fully open so wait a bit
				usleep(3000000);
				break;
			}
		}
	} else {
		int cRetry=3;
		int fSucc=0;

		while(!fSucc && cRetry--) {
			sendLCP_TermReq();
			SendATCommand(TTYportFile, "AT", NULL, 0, 1000);

			fSucc=SendATCommand(TTYportFile, "AT", NULL, 0, 1000) == 1;

			if(!fSucc)
				reopenModule();
		}

		if(!fSucc)
			return 0;
	}


	acname.type = htons(TAG_AC_NAME);
	acname_len = strlen(ACName);
	acname.length = htons(acname_len);
	memcpy(acname.payload, ACName, acname_len);

	requestedService.type = 0;
	requestedService.length = 0;
	relayId.type = 0;
	hostUniq.type = 0;
	parsePacket(packet, parsePADITags, NULL);
	if (!requestedService.type) {
		syslog(LOG_ERR, "Received PADI packet with no SERVICE_NAME tag");
		return 0;
	}
	sername_len = ntohs(requestedService.length);
	if (requiredServiceName &&
	    requiredServiceName[0]) {
		if (!sername_len ||
		    memcmp(requestedService.payload, requiredServiceName, sername_len)) {
			
			if(sername_len>=sizeof(sername)-1)
				sername_len=sizeof(sername)-1;
						
			strncpy(sername,(char*)requestedService.payload,sername_len);
			sername[sername_len]=0;
		
			syslog(LOG_ERR, "Received PADI packet asking for unsupported service '%s'", sername);
			return 0;
		}

	}

	memcpy(PeerEthAddr, packet->ethHdr.h_source, ETH_ALEN);
	/* save address of originating peer */

	/* Generate a cookie */
	cookie.type = htons(TAG_AC_COOKIE);
	cookie.length = htons(16);        /* MD5 output is 16 bytes */
	genCookie(packet->ethHdr.h_source, myAddr, CookieSeed, cookie.payload);

	/* Construct a PADO packet */
	memcpy(pado.ethHdr.h_dest, packet->ethHdr.h_source, ETH_ALEN);
	memcpy(pado.ethHdr.h_source, myAddr, ETH_ALEN);
	pado.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
	pado.ver = 1;
	pado.type = 1;
	pado.code = CODE_PADO;
	pado.session = 0;
	plen = TAG_HDR_SIZE + acname_len;

	CHECK_ROOM2(cursor, pado.payload, acname_len + TAG_HDR_SIZE);
	memcpy(cursor, &acname, acname_len + TAG_HDR_SIZE);
	cursor += acname_len + TAG_HDR_SIZE;

	CHECK_ROOM2(cursor, pado.payload, TAG_HDR_SIZE);
	memcpy(cursor, &requestedService, TAG_HDR_SIZE + sername_len);
	cursor += TAG_HDR_SIZE + sername_len;
	plen += TAG_HDR_SIZE + sername_len;

	CHECK_ROOM2(cursor, pado.payload, TAG_HDR_SIZE + 16);
	memcpy(cursor, &cookie, TAG_HDR_SIZE + 16);
	cursor += TAG_HDR_SIZE + 16;
	plen += TAG_HDR_SIZE + 16;

	if (relayId.type) {
		CHECK_ROOM2(cursor, pado.payload, ntohs(relayId.length) + TAG_HDR_SIZE);
		memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
		cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
		plen += ntohs(relayId.length) + TAG_HDR_SIZE;
	}

	if (hostUniq.type) {
		CHECK_ROOM2(cursor, pado.payload, ntohs(hostUniq.length) + TAG_HDR_SIZE);
		memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
		cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
		plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	}

	pado.length = htons(plen);
	sendPacket(sock, &pado, (int)(plen + HDR_SIZE));

	if (DebugFile) {
		fprintf(DebugFile, "SENT PADO \n");
		fprintf(DebugFile, "SENT ");
		dumpPacket(DebugFile, &pado);
		fprintf(DebugFile, "\n");
		fflush(DebugFile);
	}

	return 1;
}

void reopenModule(void)
{
	close(TtyPort);

	if (DebugFile) {
		fprintf(DebugFile, "Unable to send dial at command %s\n", TtyPortName);
	}

	// ....then re-open it
	if ((TTYportFile = fopen(TtyPortName, "rw+")) == 0) {
		syslog(LOG_ERR, "Can't open %s\n", TtyPortName);
		if (DebugFile) {
			fprintf(DebugFile, "Unable to open port %s\n", TtyPortName);
		}
		exit(1);
	}

	TtyPort = fileno(TTYportFile);

	configModemPort(TtyPort);
}


/**********************************************************************
*%FUNCTION: processPADR
*%ARGUMENTS:
* sock -- Ethernet socket
* myAddr -- my Ethernet address
* packet -- PPPoE PADR packet
* len -- length of received packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADS packet back to client and starts a PPP session if PADR
* packet is OK.
***********************************************************************/
int processPADR(int sock, unsigned char *myAddr,
                struct PPPoEPacket *packet, int len)
{
	int                ret;
	unsigned char      cookieBuffer[16];
	char      cdgcontCommand [128];
	struct PPPoEPacket pads;
	unsigned char*     cursor = pads.payload;
	UINT16_t           plen;
	size_t sername_len;
	char sername[256];
	// struct PPPoETag    servname;

	/* Initialize some globals */
	relayId.type = 0;
	hostUniq.type = 0;
	receivedCookie.type = 0;
	requestedService.type = 0;

	parsePacket(packet, parsePADRTags, NULL);

	/* Check that everything's cool */
	if (!receivedCookie.type) {
		syslog(LOG_ERR, "Received PADR packet without cookie tag");
		sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		              TAG_GENERIC_ERROR, "No cookie.  Me hungry!");
		return 0;
	}

	/* Is cookie kosher? */
	if (receivedCookie.length != htons(16)) {
		syslog(LOG_ERR, "Received PADR packet with invalid cookie tag length");
		sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		              TAG_GENERIC_ERROR, "Cookie wrong size.");
		return 0;
	}

	genCookie(packet->ethHdr.h_source, myAddr, CookieSeed, cookieBuffer);

	if (memcmp(receivedCookie.payload, cookieBuffer, 16)) {
		syslog(LOG_ERR, "Received PADR packet with invalid cookie tag");
		sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		              TAG_GENERIC_ERROR, "Bad cookie.  Me have tummy-ache.");
		return 0;
	}

	/* Check service name -- we only offer service "" */
	if (!requestedService.type) {
		syslog(LOG_ERR, "Received PADR packet with no SERVICE_NAME tag");
		sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		              TAG_SERVICE_NAME_ERROR, "No service name tag");
		return 0;
	}

	sername_len = ntohs(requestedService.length);
	if (sername_len &&
	    requiredServiceName &&
	    requiredServiceName[0] &&
	    memcmp(requestedService.payload, requiredServiceName, sername_len)) {
		
		if(sername_len>=sizeof(sername)-1)
			sername_len=sizeof(sername)-1;
		
		strncpy(sername,(char*)requestedService.payload,sername_len);
		sername[sername_len]=0;
		
		syslog(LOG_ERR, "Received PADR packet asking for unsupported service '%s'", sername);
		sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		              TAG_SERVICE_NAME_ERROR, "Invalid service name tag");
		return 0;
	}

	if (DebugFile) {
		fprintf(DebugFile, "GOT PADR \n");
	}

	// take me out
	//fprintf(stderr, "got PADR\n");
	//syslog(LOG_DEBUG, "got PADR \n");

	/* Send PADS */
	memcpy(pads.ethHdr.h_dest, packet->ethHdr.h_source, ETH_ALEN);
	memcpy(pads.ethHdr.h_source, myAddr, ETH_ALEN);

	pads.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
	pads.ver = 1;
	pads.type = 1;
	pads.code = CODE_PADS;

	Session=++_nextSession;
	pads.session = htons(Session);

	plen = 0;

	// servname.type = htons(TAG_SERVICE_NAME);
	// servname.length = 0;
	// memcpy(cursor, &servname, TAG_HDR_SIZE);
	memcpy(cursor, &requestedService, TAG_HDR_SIZE + sername_len);
	cursor += TAG_HDR_SIZE + sername_len;
	plen += TAG_HDR_SIZE + sername_len;

	if (relayId.type) {
		memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
		cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
		plen += ntohs(relayId.length) + TAG_HDR_SIZE;
	}

	if (hostUniq.type) {
		memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
		cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
		plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	}

	// This is a very IMPORTANT fix. This is because if there is no facility to retry the
	// at command, then when the ethernet cable is pulled out (hence module reset and PPPoE-serial killed)
	// and plugged back in (hence PPPOE-serial started again) a dial command is sent
	// to the phone, but this may fail if the module is still resetting. Hence a re-try will cater
	// for this because it is then very likely that by the time the 2nd or 3rd dial attempt is
	// made that the module would have been fully reset and powered up.
	// This has fixed a problem whereby a loopback detect error occurred (pop-up box in PPPoE
	// test connection on laptop/PC) and was unrecoverable unless an at+lcpppoe=0 followed by
	// a at+lcpppoe=1 was performed or a reboot of the modem.
	while (1) {
		// if we're using an APN name, i.e using UMTS,HSDPA
		if (using_apn) {
			sprintf(cdgcontCommand, "at+cgdcont=1,\"IP\",\"%s\"", apnName);

			// take me out
			//  fprintf(stderr, "apn name is %s\n",apnName);
			//fprintf(DebugFile, "command is %s\n",cdgcontCommand);
			if ((ret = SendATCommandRetry(TTYportFile, cdgcontCommand, 3000, 3)) != 1) {
				syslog(LOG_ERR, "AT+GDCONT no reply, resetModule\n");
				reopenModule();

				continue;
			}
		}
		// the dial command is best sent here  - before it was sent when the PPPoE daemon
		// comes up but now we send the dial to the modem when a PPPoE session is requested
		// so it does it as and when it needs to which makes more sense.
		// This is also done because if between the dial atd#777 and receiving a PADI the phone
		// reverts back to command mode for whatever reason,then the PADI data will just
		// be echoed and not responded to as it would in on-line mode which is incorrect.
		if ((ret = SendATCommandRetry(TTYportFile, DialCommand, 15000, 10)) != 1) {
			//fprintf(DebugFile, "resetting module at*99# failed\n");
			syslog(LOG_ERR, "DialCommand %s no reply, resetModule\n", DialCommand);
			reopenModule();

			continue;
		} else {
			break;
		}
	}


	pads.length = htons(plen);
	sendPacket(sock, &pads, (int)(plen + HDR_SIZE));

	if (DebugFile) {
		fprintf(DebugFile, "SENT PADS \n");
	}

	if (DebugFile) {
		fprintf(DebugFile, "SENT ");
		dumpPacket(DebugFile, &pads);
		fprintf(DebugFile, "\n");
		fflush(DebugFile);
	}

	DiscoveryState = STATE_SESSION;

	return 1;
}

#include <time.h>

static time_t _tmLastDiscovery=-1;

void finishDiscovery(void)
{
	_tmLastDiscovery=-1;
}

void startDiscovery(void)
{
	_tmLastDiscovery=time(NULL);
}

int canDiscovery(void)
{
	time_t tmNow=time(NULL);

	return (_tmLastDiscovery<0) || !(tmNow-_tmLastDiscovery<PPPOE_TIMEOUT_DISCOVERY);
}

void heartbeatTouch(void)
{
	_tmHeartBeat=time(NULL);
}

int heartbeatIsTimeToReqEcho(void)
{
	time_t tmPast=time(NULL)-_tmHeartBeat;

	return !(_tmHeartBeat<0) && !(tmPast<PPPOE_TIMEOUT_REQECHO);
}

int heartbeatIsDead(void)
{
	time_t tmPast=time(NULL)-_tmHeartBeat;

	int fDropped=DiscoveryState != STATE_SESSION;
	int fDead=(_tmHeartBeat>=0) && (tmPast>=PPPOE_TIMEOUT_SESSION);

	// check heartbeat if session
	if(!fDropped && fDead) {
		sendLCP_TermReq();

		DiscoveryState=0;
	}

	return DiscoveryState != STATE_SESSION;
}

void heartbeatStart(void)
{
	_tmHeartBeat=time(NULL);
}

void heartbeatFinish(void)
{
	_tmHeartBeat=-1;
}

void processDiscoveryPacket(int sock, unsigned char *myAddr)
{
	struct PPPoEPacket packet;
	int len;

	receivePacket(sock, &packet, &len);

	if (DebugFile) {
		fprintf(DebugFile, "RCVD in discover ");
		dumpPacket(DebugFile, &packet);
		fprintf(DebugFile, "\n");
		fflush(DebugFile);
	}

	/* Check length */
	if (ntohs(packet.length) + HDR_SIZE > len) {
		syslog(LOG_ERR, "len 1 %d", sizeof(struct ethhdr));
		syslog(LOG_ERR, "len 1 %d", HDR_SIZE);
		syslog(LOG_ERR, "Bogus PPPoE length field");
		return;
	}

	/* Sanity check on packet */
	if (packet.ver != 1 || packet.type != 1) {
		/* Syslog an error */
		return;
	}

	if (DebugFile) {
		fprintf(DebugFile, "switching on packet code\n");
	}
	switch (packet.code) {
	case CODE_PADI: {
		if( heartbeatIsDead() && canDiscovery() ) {
			if(processPADI(sock, myAddr, &packet, len)) {
				startDiscovery();

				// pppoe status - discovery
				databaseWritePPPoEStat(pppoestat_discovery);
			}
		}

		break;
	}

	case CODE_PADR:
		if( processPADR(sock, myAddr, &packet, len) ) {
			finishDiscovery();
			heartbeatStart();

			// pppoe status - online
			databaseWritePPPoEStat(pppoestat_online);
		} else {
			// pppoe status - idle
			databaseWritePPPoEStat(pppoestat_idle);
		}

		break;
	case CODE_PADT:
		/*   it might not be for us
		   if (DebugFile)
		   {
		   fprintf(DebugFile,"get PADT in discovery resetting module\n");
		   }
		   system("/usr/bin/module_reset");
		*/
		break;
	case CODE_SESS:
		/* Ignore PADT and SESS -- children will handle them */
		break;
	case CODE_PADO:
	case CODE_PADS:
		/* Ignore PADO and PADS totally */
		break;
	default:
		/* Syslog an error */
		break;
	}

	return;
}

/**********************************************************************
*%FUNCTION: sessionDiscoveryPacket
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* We got a discovery packet during the session stage.  This most likely
* means a PADT.
***********************************************************************/
void sessionDiscoveryPacket(int sock, unsigned char *myAddr)
{
	struct PPPoEPacket packet;
	int len;
	int wrong_type;

	//syslog(LOG_DEBUG, "In function sessionDiscoveryPacket");

	receivePacket(sock, &packet, &len);

	/* Check length */
	if (ntohs(packet.length) + HDR_SIZE > len) {
		syslog(LOG_ERR, "Bogus PPPoE length field");
		return;
	}

	if (DebugFile) {
		fprintf(DebugFile, "RCVD in session ");
		dumpPacket(DebugFile, &packet);
		fprintf(DebugFile, "\n");
		fflush(DebugFile);
	}

	wrong_type = 0;
	/* Sanity check */
	if (packet.code == CODE_PADT) {
		if (memcmp(PeerEthAddr, packet.ethHdr.h_source, ETH_ALEN))
			return;
		if (!memcmp(ZeroEthAddr, packet.ethHdr.h_source, ETH_ALEN))
			return;
		if (ntohs(packet.session) != Session)
			return;
	} else if (packet.code == CODE_PADI) {
		// check service name
		if (requiredServiceName && requiredServiceName[0]) {
			requestedService.type = 0;
			requestedService.length = 0;
			parsePacket(&packet, parsePADITags, NULL);
			if (!requestedService.length ||
			    memcmp(requestedService.payload, requiredServiceName, ntohs(requestedService.length))) {
				return;
			}
		}

		// if session exists
		if(!heartbeatIsDead())
			return;
	} else
		wrong_type = 1;
	if (wrong_type) {
		syslog(LOG_DEBUG, "Got discovery packet (code %d) during session",
		       (int) packet.code);
		return;
	}

	// reset the module - this is just in-case the resulting LCP terminate does not succeed for whatever
	// reason thus leaving the PPP session in an intermediate state. Resetting the module will put the PPP
	// session back in a known state.
	memset(PeerEthAddr, 0, ETH_ALEN);

	syslog(LOG_INFO,
	       "Session terminated -- received PADT or PADI %d from peer", packet.code);

	parsePacket(&packet, parseLogErrs, NULL);
	if (DebugFile) {
		fprintf(DebugFile, "got PADT during session so reset module\n");
	}

	syslog(LOG_ERR, "PADT during session, resetModule\n");

	DiscoveryState = 0;

	sendLCP_TermReq();

	// pppoe status - idle
	databaseWritePPPoEStat(pppoestat_idle);
	databaseWriteIpAddress(NULL);
}

UINT16_t fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/**********************************************************************
*%FUNCTION: pppFCS16
*%ARGUMENTS:
* fcs -- current fcs
* cp -- a buffer's worth of data
* len -- length of buffer "cp"
*%RETURNS:
* A new FCS
*%DESCRIPTION:
* Updates the PPP FCS.
***********************************************************************/
UINT16_t
pppFCS16(UINT16_t fcs,
         unsigned char * cp,
         int len)
{
	while (len--)
		fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];

	return (fcs);
}


struct PPPCtrlFrameHeader {
	unsigned char bCode;
	unsigned char bIdent;
	unsigned short wLen;
} __attribute__((packed));

struct PPPCtrlFrameHeader_EchoReq {
	unsigned char bCode;
	unsigned char bIdent;
	unsigned short wLen;
	unsigned int dwMacicNumber;
	unsigned int dwMessage;
} __attribute__((packed));

struct PPPCtrlFrameHeader_TermReq {
	unsigned char bCode;
	unsigned char bIdent;
	unsigned short wLen;
	char achData[12];
} __attribute__((packed));


struct PPPCtrlFrameOptHeader {
	unsigned char bType;
	unsigned char bLen;
} __attribute__((packed));

int CDCS_Sleep(u_long ms)
{
	struct timeval     tv;

	int waitS = ms / 1000;
	int waitUS = (ms % 1000) * 1000;

	tv.tv_sec = waitS;
	tv.tv_usec = waitUS;

	select(0, (fd_set*)0, (fd_set*) 0, (fd_set*) 0, &tv);

	return ms;
}

int convPPPOE2PPP(const void* pSrc,int cbSrc,void* pDst)
{
	UINT16_t fcs;
	unsigned char tail[2];

	struct PPPoEPacket* pPck=(struct PPPoEPacket*)pSrc;
	char* ptr=(char*)pDst;

	unsigned char header[2] = {FRAME_ADDR, FRAME_CTRL};

	int i;
	char c;

	/* Compute FCS */
	fcs = pppFCS16(PPPINITFCS16, header, 2);
	fcs = pppFCS16(fcs, pPck->payload, cbSrc) ^ 0xffff;
	tail[0] = fcs & 0x00ff;
	tail[1] = (fcs >> 8) & 0x00ff;

	/* Build a buffer to send to PPP */
	*ptr++ = FRAME_FLAG;
	*ptr++ = FRAME_ADDR;
	*ptr++ = FRAME_ESC;
	*ptr++ = FRAME_CTRL ^ FRAME_ENC;

	for (i = 0; i < cbSrc; i++) {
		c = pPck->payload[i];
		if (c == FRAME_FLAG || c == FRAME_ADDR || c == FRAME_ESC || c < 0x20) {
			*ptr++ = FRAME_ESC;
			*ptr++ = c ^ FRAME_ENC;
		} else {
			*ptr++ = c;
		}
	}

	for (i = 0; i < 2; i++) {
		c = tail[i];
		if (c == FRAME_FLAG || c == FRAME_ADDR || c == FRAME_ESC || c < 0x20) {
			*ptr++ = FRAME_ESC;
			*ptr++ = c ^ FRAME_ENC;
		} else {
			*ptr++ = c;
		}
	}

	*ptr++ = FRAME_FLAG;

	return ((u_long) ptr - (u_long) pDst);
}

int sendLCP_TermReq(void)
{
	struct PPPoEPacket pckPPPoE;
	char achPckPPP[sizeof(pckPPPoE)];

	// get the points to payload
	unsigned short* pProto=(unsigned short*)pckPPPoE.payload;
	int cbProto=sizeof(*pProto);
	struct PPPCtrlFrameHeader_TermReq* pTermReq=(struct PPPCtrlFrameHeader_TermReq*)&(pckPPPoE.payload[cbProto]);

	int cbPckPPPoE=HDR_SIZE + sizeof(*pTermReq);
	int cbPckPPP;

	// zero pckPPPoE
	memset(&pckPPPoE,0,sizeof(pckPPPoE));

	// build pppoe header for session data
	pckPPPoE.length = htons( cbProto + sizeof(*pTermReq) );

	// build payload for each request
	*pProto=htons(0xc021);
	pTermReq->bCode=0x05;
	pTermReq->bIdent=0xff;
	pTermReq->wLen=htons(sizeof(*pTermReq));

	cbPckPPP=convPPPOE2PPP(&pckPPPoE,cbPckPPPoE,achPckPPP);

	return write(TtyPort,achPckPPP,cbPckPPP);
}


/**********************************************************************
*%FUNCTION: ReadFromEth
*%ARGUMENTS:
* sock -- Ethernet socket
* clampMss -- if non-zero, do MSS-clamping
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Reads a packet from the Ethernet interface and sends it to async PPP
* device.
***********************************************************************/
int ReadFromEth(int sock, char* buffer, int* lenp, int clampMss)
{
	struct PPPoEPacket packet;
	int len;
	int plen;
	int i;
	char *ptr = buffer;
	unsigned char c;
	UINT16_t fcs;
	unsigned char header[2] = {FRAME_ADDR, FRAME_CTRL};
	unsigned char tail[2];

	receivePacket(sock, &packet, &len);

	/* Check length */
	if (ntohs(packet.length) + HDR_SIZE > len) {
		syslog(LOG_ERR, "Bogus PPPoE length field");
		return(0);
	}

	if (DebugFile) {
		fprintf(DebugFile, "RCVD ");
		dumpPacket(DebugFile, &packet);
		fprintf(DebugFile, "\n");
		fflush(DebugFile);
	}

	/* Sanity check */
	if (packet.code != CODE_SESS) {
		syslog(LOG_ERR, "Unexpected packet code %d", (int) packet.code);
		return(0);
	}

	if (packet.ver != 1) {
		syslog(LOG_ERR, "Unexpected packet version %d", (int) packet.ver);
		return(0);
	}

	if (packet.type != 1) {
		syslog(LOG_ERR, "Unexpected packet type %d", (int) packet.type);
		return(0);
	}

	if (memcmp(packet.ethHdr.h_source, PeerEthAddr, ETH_ALEN)) {
		/* Not for us -- must be another session.  This is not an error,
		   so don't log anything.  */
		return(0);
	}

	if (ntohs(packet.session) != Session) {
		/* Not for us -- must be another session.  This is not an error,
		   so don't log anything.  */
		return(0);
	}

	plen = ntohs(packet.length);
	if (plen + HDR_SIZE > len) {
		syslog(LOG_ERR, "Bogus length field in session packet %d (%d)",
		       (int) plen, (int) len);
		return(0);
	}

	/* Clamp MSS */
	if (clampMss) {
		clampMSS(&packet, "incoming", clampMss);
	}


	heartbeatTouch();

	// filter - eth to serial
	{
		int cbProto=2;
		int nProto=ntohs( *(unsigned short*)packet.payload );
		struct PPPCtrlFrameHeader* pCtrlFrameHdr=(struct PPPCtrlFrameHeader*)&(packet.payload[cbProto]);
		struct PPPCtrlFrameOptHeader* pOptHdr=(struct PPPCtrlFrameOptHeader*)(pCtrlFrameHdr+1);
		unsigned int* pIP=(unsigned int*)(pOptHdr+1);

		// check heartbeat
		if( (nProto==0xc021) && (pCtrlFrameHdr->bCode==0x0a)) {
			unsigned int* pMagic=(unsigned int*)pOptHdr;
			unsigned int* pMessg=(pMagic+1);
			int cbMinLen=sizeof(struct PPPCtrlFrameHeader)+sizeof(*pMagic)+sizeof(*pMessg);

			if( !(ntohs(pCtrlFrameHdr->wLen)<cbMinLen) ) {
				if( (ntohl(*pMagic) == _dwHeartBeatMessg) && (ntohl(*pMessg) == _dwHeartBeatMagic) )
					return 0;
			}
		}

		if(_fForceNumberedIpAddr) {
			// clear the IP option if ACK with our bogus IP
			if( (nProto==0x8021) && (pCtrlFrameHdr->bCode==0x02) && (ntohs(pCtrlFrameHdr->wLen)==10) && (pOptHdr->bType==0x03) && (*pIP==_bogusIP)) {
				unsigned char bIdent=pCtrlFrameHdr->bIdent;

				memset(pOptHdr,0,ntohs(pCtrlFrameHdr->wLen)-sizeof(struct PPPCtrlFrameOptHeader));

				// adjust pppoe payload length
				plen=sizeof(struct PPPCtrlFrameHeader)+2;
				packet.length=htonl(plen);

				// adjust control frame header length
				pCtrlFrameHdr->wLen=htons(sizeof(struct PPPCtrlFrameHeader));

				if(DebugFile) {
					fprintf(DebugFile, "ACK modified - cut all bogus IP address (bIdent=0x%02x)\n", bIdent);
					dumpPacket(DebugFile, &packet);
				}
			}
		}
	}

	/* Compute FCS */
	fcs = pppFCS16(PPPINITFCS16, header, 2);
	fcs = pppFCS16(fcs, packet.payload, plen) ^ 0xffff;
	tail[0] = fcs & 0x00ff;
	tail[1] = (fcs >> 8) & 0x00ff;

	/* Build a buffer to send to PPP */
	*ptr++ = FRAME_FLAG;
	*ptr++ = FRAME_ADDR;
	*ptr++ = FRAME_ESC;
	*ptr++ = FRAME_CTRL ^ FRAME_ENC;

	for (i = 0; i < plen; i++) {
		c = packet.payload[i];
		if (c == FRAME_FLAG || c == FRAME_ADDR || c == FRAME_ESC || c < 0x20) {
			*ptr++ = FRAME_ESC;
			*ptr++ = c ^ FRAME_ENC;
		} else {
			*ptr++ = c;
		}
	}

	for (i = 0; i < 2; i++) {
		c = tail[i];
		if (c == FRAME_FLAG || c == FRAME_ADDR || c == FRAME_ESC || c < 0x20) {
			*ptr++ = FRAME_ESC;
			*ptr++ = c ^ FRAME_ENC;
		} else {
			*ptr++ = c;
		}
	}

	*ptr++ = FRAME_FLAG;

	*lenp = ((u_long) ptr - (u_long) buffer);

	if (DebugFile) {
		fprintf(DebugFile, "async ppp packet\n");

		dumpHex(DebugFile, (unsigned char*)buffer, *lenp);
	}

	return(1);
}

/**********************************************************************
*%FUNCTION: ReadFromSerial
*%ARGUMENTS:
* packet -- buffer in which to place PPPoE packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Reads from an async PPP device and builds a PPPoE packet to transmit
***********************************************************************/
void ReadFromSerial(int fd, struct PPPoEPacket *packet)
{
	/* current settings of the state machine */
	static unsigned char  PPPState = STATE_WAITFOR_FLAG_SEQ;
	static unsigned short PPPPacketSize = 0;
	static unsigned char  PPPXorValue = 0;

	unsigned char buf[READ_CHUNK];
	unsigned char *ptr = buf;
	unsigned char c;

	int r;

	r = read(fd, buf, READ_CHUNK);

	if (DebugFile) {
		fprintf(DebugFile, "read serial data - %d\n",PPPState);
		dumpHex(DebugFile, buf, r);
	}

	if (r < 0) {
		fatalSys("read (ReadFromSerial)");
	}

	if (r == 0) {
		syslog(LOG_INFO, "end-of-file in ReadFromSerial");
		sendPADT();
	}

	while (r) {
		switch (PPPState) {
		default:
			syslog(LOG_ERR, "Asynch PPPState corrupted 0x%x resetting", PPPState);

			PPPState = STATE_WAITFOR_FLAG_SEQ;
			break;

		case STATE_WAITFOR_FLAG_SEQ:
			while (r) {
				r--;
				if (*ptr++ == FRAME_FLAG) {
					if(*ptr != FRAME_FLAG) {
						PPPState = STATE_WAITFOR_FRAME_ADDR;
						break;
					}
				}
			}

			break;

		case STATE_WAITFOR_FRAME_ADDR:
			/*
				In address/control field compression, the following packets may not have address and control fields.
				We have to skip address and control field parser.
			*/
			if(r) {
				if(*ptr == FRAME_ADDR) {
					PPPState = STATE_DROP_PROTO;
					ptr++;
					r--;
				} else {
					PPPState = STATE_BUILDING_PACKET;
				}
			}
			break;

		case STATE_DROP_PROTO:
			while (r) {
				r--;
				if (*ptr++ == (FRAME_CTRL ^ FRAME_ENC)) {
					PPPState = STATE_BUILDING_PACKET;
					break;
				}
			}

			break;

		case STATE_BUILDING_PACKET:
			/* Start building frame */
			while (r && PPPState == STATE_BUILDING_PACKET) {
				c = *ptr;
				switch (c) {
				case FRAME_ESC:
					PPPXorValue = FRAME_ENC;
					break;

				case FRAME_FLAG:
					if (PPPPacketSize < 2) {
						fatal("Packet too short from PPP (asyncReadFromPPP)");
					}
					// filter - serial to eth
					else {
						int fIgnore = 0;
						int cbProto = 2;
						int nProto = ntohs(*(unsigned short*)packet->payload);
						struct PPPCtrlFrameHeader* pCtrlFrameHdr = (struct PPPCtrlFrameHeader*) & (packet->payload[cbProto]);
						unsigned char bIdent = pCtrlFrameHdr->bIdent;
						struct PPPCtrlFrameOptHeader* pOptHdr = (struct PPPCtrlFrameOptHeader*)(pCtrlFrameHdr + 1);
						unsigned int* pIp = (unsigned int*)(pOptHdr + 1);

						// get information if ip address ACK
						if ((nProto == 0x8021) && (pCtrlFrameHdr->bCode == 0x02) && (ntohs(pCtrlFrameHdr->wLen)>sizeof(struct PPPCtrlFrameHeader)) && (pOptHdr->bType == 0x03)) {
							struct in_addr pppIpAddr;

							pppIpAddr.s_addr=*pIp;
							const char* szPPPAddr=inet_ntoa(pppIpAddr);

							databaseWriteIpAddress(szPPPAddr);
						}

						if(_fIgnoreRequestUntilReady) {
							// eat the packet if Sierra bogus NAK
							if ((nProto == 0x8021) && (pCtrlFrameHdr->bCode == 0x03) && (ntohs(pCtrlFrameHdr->wLen)>sizeof(struct PPPCtrlFrameHeader)) && (pOptHdr->bType == 0x81) && (*pIp == _sierraBogusIP)) {
								fIgnore = 1;

								if (DebugFile)
									fprintf(DebugFile, "NAK packet ignored (bIdent=0x%02x)\n", bIdent);
							}
						}

						if(_fForceNumberedIpAddr) {
							// add our bogus IP if Sierra empty REQ
							if ((nProto == 0x8021) && (pCtrlFrameHdr->bCode == 0x01) && (ntohs(pCtrlFrameHdr->wLen)==4) ) {
								// add an IP address option
								pOptHdr->bType=0x03;
								pOptHdr->bLen=6;
								*pIp=_bogusIP;

								// expand control packet
								pCtrlFrameHdr->wLen=htons(ntohs(pCtrlFrameHdr->wLen)+6);

								// expand pppoe packet
								PPPPacketSize+=6;
								packet->length=htons(PPPPacketSize);

								if (DebugFile)
									fprintf(DebugFile, "the bogus IP added in the empty REQ packet (bIdent=0x%02x)\n", bIdent);
							}
						}

						if (!fIgnore)
							sendSessionPacket(packet, PPPPacketSize - 2);
					}


					PPPPacketSize = 0;
					PPPXorValue = 0;
					PPPState = STATE_WAITFOR_FLAG_SEQ;
					
					// pass the flag seq
					ptr++;
					r--;
					
					goto process_more;

				default:
					if (PPPPacketSize >= ETH_DATA_LEN - 4) {
						syslog(LOG_ERR, "Packet too big!  Check MTU on PPP interface");
						PPPPacketSize = 0;
						PPPXorValue = 0;
						PPPState = STATE_WAITFOR_FLAG_SEQ;
						goto process_more;
					} else {
						packet->payload[PPPPacketSize++] = c ^ PPPXorValue;
						PPPXorValue = 0;
					}
				}
				
				// only pass if not FLAG SEQ.
				ptr++;
				r--;
			}
		}
process_more:
		;
	}
}

/**********************************************************************
*%FUNCTION: sendErrorPADS
*%ARGUMENTS:
* sock -- socket to write to
* source -- source Ethernet address
* dest -- destination Ethernet address
* errorTag -- error tag
* errorMsg -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADS packet with an error message
***********************************************************************/
void sendErrorPADS(int sock,
                   unsigned char *source,
                   unsigned char *dest,
                   int errorTag,
                   char *errorMsg)
{
	struct PPPoEPacket pads;
	unsigned char *cursor = pads.payload;
	UINT16_t plen;
	struct PPPoETag err;
	int elen = strlen(errorMsg);

	memcpy(pads.ethHdr.h_dest, dest, ETH_ALEN);
	memcpy(pads.ethHdr.h_source, source, ETH_ALEN);
	pads.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
	pads.ver = 1;
	pads.type = 1;
	pads.code = CODE_PADS;

	pads.session = htons(0);
	plen = 0;

	err.type = htons(errorTag);
	err.length = htons(elen);

	memcpy(err.payload, errorMsg, elen);
	memcpy(cursor, &err, TAG_HDR_SIZE + elen);
	cursor += TAG_HDR_SIZE + elen;
	plen += TAG_HDR_SIZE + elen;

	if (relayId.type) {
		memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
		cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
		plen += ntohs(relayId.length) + TAG_HDR_SIZE;
	}

	if (hostUniq.type) {
		memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
		cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
		plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	}

	pads.length = htons(plen);

	sendPacket(sock, &pads, (int)(plen + HDR_SIZE));
}


void sendLCP_ReqEcho(void)
{
	struct PPPoEPacket packet;

	// get the points to payload
	unsigned short* pProto=(unsigned short*)packet.payload;
	int cbProto=sizeof(*pProto);
	struct PPPCtrlFrameHeader_EchoReq* pEchoReq=(struct PPPCtrlFrameHeader_EchoReq*)&(packet.payload[cbProto]);

	if (DiscoveryState != STATE_SESSION)
		return;

	// zero packet
	memset(&packet,0,sizeof(packet));

	// build pppoe header for session data
	memcpy(packet.ethHdr.h_dest, PeerEthAddr, ETH_ALEN);
	memcpy(packet.ethHdr.h_source, MyEthAddr, ETH_ALEN);
	packet.ethHdr.h_proto = htons(Eth_PPPOE_Session);
	packet.ver = 1;
	packet.type = 1;
	packet.code = CODE_SESS;
	packet.session = htons(Session);
	packet.length = htons( cbProto + sizeof(struct PPPCtrlFrameHeader_EchoReq) );

	// build payload for each request
	*pProto=htons(0xc021);
	pEchoReq->bCode=0x09;
	pEchoReq->bIdent=0xff;
	pEchoReq->wLen=htons(sizeof(*pEchoReq));
	pEchoReq->dwMacicNumber=_dwHeartBeatMagic;
	pEchoReq->dwMessage=_dwHeartBeatMessg;

	sendPacket(SessionSocket, &packet, HDR_SIZE + cbProto + sizeof(struct PPPCtrlFrameHeader_EchoReq) );
}

/***********************************************************************
*%FUNCTION: sendPADT
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADT packet
***********************************************************************/
void sendPADT(void)
{
	struct PPPoEPacket packet;
	unsigned char *cursor = packet.payload;

	UINT16_t plen = 0;

	/* Do nothing if no session established yet */
	if (!Session) return;

	memcpy(packet.ethHdr.h_dest, PeerEthAddr, ETH_ALEN);
	memcpy(packet.ethHdr.h_source, MyEthAddr, ETH_ALEN);

	packet.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
	packet.ver = 1;
	packet.type = 1;
	packet.code = CODE_PADT;
	packet.session = htons(Session);

	/* Reset Session to zero so there is no possibility of
	   recursive calls to this function by any signal handler */
	Session = 0;

	/* Copy cookie and relay-ID if needed */
	if (cookie.type) {
		CHECK_ROOM(cursor, packet.payload,
		           ntohs(cookie.length) + TAG_HDR_SIZE);
		memcpy(cursor, &cookie, ntohs(cookie.length) + TAG_HDR_SIZE);
		cursor += ntohs(cookie.length) + TAG_HDR_SIZE;
		plen += ntohs(cookie.length) + TAG_HDR_SIZE;
	}

	if (relayId.type) {
		CHECK_ROOM(cursor, packet.payload,
		           ntohs(relayId.length) + TAG_HDR_SIZE);
		memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
		cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
		plen += ntohs(relayId.length) + TAG_HDR_SIZE;
	}

	packet.length = htons(plen);

	sendPacket(DiscoverySocket, &packet, (int)(plen + HDR_SIZE));

	if (DebugFile) {
		fprintf(DebugFile, "SENT PADT\n");
	}

	if (DebugFile) {
		fprintf(DebugFile, "SENT ");
		dumpPacket(DebugFile, &packet);
		fprintf(DebugFile, "\n");
		fflush(DebugFile);
	}

	syslog(LOG_INFO, "Sent PADT");
}

/***********************************************************************
*%FUNCTION: sendSessionPacket
*%ARGUMENTS:
* packet -- the packet to send
# len -- length of data to send
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Transmits a session packet to the peer.
***********************************************************************/
void sendSessionPacket(struct PPPoEPacket *packet, int len)
{
	memcpy(packet->ethHdr.h_dest, PeerEthAddr, ETH_ALEN);
	memcpy(packet->ethHdr.h_source, MyEthAddr, ETH_ALEN);

	packet->length = htons(len);

	packet->session = htons(Session);

	if (optClampMSS) {
		clampMSS(packet, "outgoing", optClampMSS);
	}

	sendPacket(SessionSocket, packet, len + HDR_SIZE);

	if (DebugFile) {
		fprintf(DebugFile, "SENT ");
		dumpPacket(DebugFile, packet);
		fprintf(DebugFile, "\n");
		fflush(DebugFile);
	}
}

/**********************************************************************
*%FUNCTION: genCookie
*%ARGUMENTS:
* peerEthAddr -- peer Ethernet address (6 bytes)
* myEthAddr -- my Ethernet address (6 bytes)
* seed -- random cookie seed to make things tasty (16 bytes)
* cookie -- 16-byte buffer which is filled with md5 sum of previous items
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Forms the md5 sum of peer MAC address, our MAC address and seed, useful
* in a PPPoE Cookie tag.
***********************************************************************/
void
genCookie(unsigned char const *peerEthAddr,
          unsigned char const *myEthAddr,
          unsigned char const *seed,
          unsigned char *cookie)
{
	struct MD5Context ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, peerEthAddr, ETH_ALEN);
	MD5Update(&ctx, myEthAddr, ETH_ALEN);
	MD5Update(&ctx, seed, SEED_LEN);
	MD5Final(cookie, &ctx);
}

/**********************************************************************
*%FUNCTION: fatalSys
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to stderr and syslog and exits.
***********************************************************************/
void
fatalSys(char const *str)
{
	char buf[SMALLBUF];
	snprintf(buf, SMALLBUF, "%s: %s", str, strerror(errno));
	syslog(LOG_ERR, "%s\n", buf);
	printErr(buf);
}

/**********************************************************************
*%FUNCTION: fatal
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message to stderr and syslog and exits.
***********************************************************************/
void
fatal(char const *str)
{
	syslog(LOG_ERR, "%s\n", str);
	printErr(str);
}


// This is now in it's own thread to boost EVDO speeds..........so reading from serial port is in one thread
// and writing to serial port in another.
void* writeToSerial(void* arg)
{
	fd_set    readable;
	char    SrlBuffer[4096];    /* packet to be sent to serial */
	int     SrlBufferLen;
	char    *pBuff;
	int     i, r;
	struct timeval  tv;
	struct timeval*  tvp = NULL;
	int written;

	while (1) {
		if (DebugFile) {
			//fprintf(DebugFile,"in writeToSerial thread \n");
		}
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		tvp = &tv;

		FD_ZERO(&readable);
		FD_SET(SessionSocket, &readable);

		while (1) {
			r = select(SessionSocket + 1, &readable, NULL, NULL, tvp);
			if (r >= 0) break;
		}

		if (r < 0) {
			fatalSys("select (session)");
		}

		if (r == 0) {
			/* Inactivity timeout */
			//syslog(LOG_ERR, "Inactivity timeout... something wicked happened");
			//sendPADT();
			//exit(1);
			continue;
		}

		if (FD_ISSET(SessionSocket, &readable)) {
			if (DiscoveryState == STATE_SESSION) {
				do {
					/*
					 get packet from ethernet and send to serial port if valid

					 When writing the received packet to the serial port split it up into 512 byte chunks
					*/
					if (ReadFromEth(SessionSocket, SrlBuffer, &SrlBufferLen, optClampMSS)) {
						//fprintf(stderr, "TX: %d (%d) ", SrlBufferLen, ETHERMTU);

						pBuff = SrlBuffer;
						while (SrlBufferLen) {
							if (SrlBufferLen > 512)
								i = 512;
							else
								i = SrlBufferLen;

							//fprintf(stderr, "%d ", i);

							written=write(TtyPort, pBuff, i);
							
							
							if (written < 0) {
								if(errno!=EAGAIN)
									fatalSys("ReadFromEth: write");
							}
							else {
								SrlBufferLen -= written;
								pBuff += written;
							}
						}
						//fprintf(stderr, "\n");
					}
				} while (BPF_BUFFER_HAS_DATA);
			}
		}
	}
	
	return 0;
}


int doUsageExit(void)
{
	printf("pppoe-serial CDCS modification version\n");
	printf("\n");

	printf("Usage: pppoe-serial [-Fr] -p port name -d dial command [-a apn name] [-s service name]\n");
	printf("                    [-I eth interface name] [-D stderr | file name] [-b bogus ip address]\n");
	printf("\n");

	printf("Bridge between eth0 and ppp server by using pppoe protocol according to switches and options\n");
	printf("\n");

	printf("Switches:\n");
	printf("\t-F \tDisable daemoniziation feature\n");
	printf("\t-r \tReply a bogus IPCP until ready if any request - disable compatibility feature #1\n");
	printf("\t-u \tUse un-numbered IP mode - disable compatibility feature #2\n");
	printf("\n");

	printf("Options:\n");
	printf("\t-b IP address \t\tUse IP address as a bogus ip address - default: 64.64.64.64\n");
	printf("\n");

	exit(2);
}

void sighandler(int signo)
{
	switch(signo) {
		case SIGTERM:
			syslog(LOG_INFO,"SIGTERM caught");
			terminate=1;
			break;
			
		default:
			syslog(LOG_INFO,"signal(%d) caught",signo);
			break;
	}
}

int main(int argc, char **argv)
{
	int     beDaemon = 1;
	int     opt;
	FILE*    fp;
	fd_set    readable;
	struct PPPoEPacket packet;             /* packet to be sent to ethernet */

	struct timeval  tv;
	struct timeval*  tvp = NULL;
	int     maxFD = 0;
	int     i, r;

	int total_retry;
	int retry;
	int succ=0;
	

	while ((opt = getopt(argc, argv, "FI:p:s:d:a:D:b:ru")) != -1) {
		switch (opt) {
		case 'F':
			beDaemon = 0;
			break;

		case 'p':
			SET_STRING(TtyPortName, optarg);
			break;

		case 's':
			SET_STRING(requiredServiceName, optarg);
			break;

		case 'd':
			SET_STRING(DialCommand, optarg);
			break;

		case 'I':
			SET_STRING(IfName, optarg);

			break;

		case 'a':
			SET_STRING(apnName, optarg);
			using_apn = 1;
			break;

		case 'D':
			if (strcmp(optarg, "stderr") == 0) {
				DebugFile = stderr;
			} else {
				DebugFile = fopen(optarg, "w");

				if (!DebugFile) {
					syslog(LOG_ERR, "Can't open debug %s\n", optarg);
					fprintf(stderr, "Could not open %s: %s\n",
					        optarg, strerror(errno));

					exit(1);
				}
			}

			fflush(DebugFile);

			break;

			// get a bogus IP address for pppoe
		case 'b': {
			struct in_addr inaddrBogus;

			if(inet_aton(optarg,&inaddrBogus))
				_bogusIP=inaddrBogus.s_addr;
			else
				doUsageExit();

			break;
		}

		// Reply a bogus IPCP until ready if any request - disable PPPOE compatibility feature #1
		case 'r':
			_fIgnoreRequestUntilReady=0;
			break;

			// Use un-numbered IP mode - disable PPPOE compatibility feature #2
		case 'u':
			_fForceNumberedIpAddr=0;
			break;

		default:
			doUsageExit();
			break;
		}
	}

	// do some validation
	if(!TtyPortName || !DialCommand)
		doUsageExit();

	if (strcmp(usb_port, argv[4]) == 0) {
		use_usb_port = 1;
		if (DebugFile) {
			fprintf(DebugFile, "Using USB port\n");
		}
	} else {
		if (DebugFile) {
			fprintf(DebugFile, "NOT Using USB port\n");
		}
	}


	if (!IfName) {
		IfName = DEFAULT_IF;
	}

	if (!ACName) {
		ACName = malloc(HOSTNAMELEN);

		if (gethostname(ACName, HOSTNAMELEN) < 0) {
			fatalSys("gethostname");
		}
	}

	/* Daemonize -- UNIX Network Programming, Vol. 1, Stevens */
	if (beDaemon) {
		i = fork();
		if (i < 0) {
			fatalSys("fork");
		} else if (i != 0) {
			/* parent */
			exit(0);
		}

		setsid();
		signal(SIGHUP, SIG_IGN);

		i = fork();

		if (i < 0) {
			fatalSys("fork");
		} else if (i != 0) {
			exit(0);
		}


		chdir("/");

		closelog();

		/*
		    Close all open file descriptors except debug file.
		    This includes the pipe to /dev/log (syslog descriptor)
		*/
		for (i = 0; i < CLOSEFD; i++) {
			if (!(DebugFile && (i == fileno(DebugFile))))
				close(i);
		}

		/* We nuked our syslog descriptor... */
		openlog("pppoe-serial", LOG_PID, LOG_DAEMON);
	}
	
	signal(SIGTERM, sighandler);

	// open database
	if(rdb_open_db() < 0) {
		syslog(LOG_ERR, "failed to open cdcs database");
	} else {
		if( rdb_update_single(DATABASE_PPPOE_STATUS,"",0,DEFAULT_PERM,0,0)<0 )
			syslog(LOG_ERR, "failed to create a variable - %s",DATABASE_PPPOE_STATUS);

		if( rdb_update_single(DATABASE_PPPOE_IP,"",0,DEFAULT_PERM,0,0)<0 )
			syslog(LOG_ERR, "failed to create a variable - %s",DATABASE_PPPOE_IP);
	}

	// pppoe status - INIT
	databaseWritePPPoEStat(pppoestat_init);
	// write initial values
	databaseWriteIpAddress(NULL);

	/* Initialize our random cookie.  Try /dev/urandom; if that fails,
	   use PID and rand() */
	fp = fopen("/dev/urandom", "r");

	if (fp) {
		fread(&CookieSeed, 1, SEED_LEN, fp);
		fclose(fp);
	} else {
		CookieSeed[0] = getpid() & 0xFF;
		CookieSeed[1] = (getpid() >> 8) & 0xFF;

		for (i = 2; i < SEED_LEN; i++) {
			CookieSeed[i] = (rand() >> (i % 9)) & 0xFF;
		}
	}

	if ((TTYportFile = fopen(TtyPortName, "r+")) == 0) {
		syslog(LOG_ERR, "Can't open %s\n", TtyPortName);
		if (DebugFile) {
			fprintf(DebugFile, "Unable to open port %s\n", TtyPortName);
		}
		exit(1);
	}


	TtyPort = fileno(TTYportFile);

	configModemPort(TtyPort);

	/*if (write(TtyPort, DialCommand, strlen(DialCommand)) < 0)
	{
	    fatalSys("Main: write");
	}
	if (write(TtyPort, "\r", 8) < 0)
	{
	    fatalSys("Main: write");
	}*/
	//fprintf(stderr, "sending dial command %s\n", DialCommand);

	/* Open a session socket */
	SessionSocket = openInterface(IfName, Eth_PPPOE_Session, NULL);

	DiscoverySocket = openInterface(IfName, Eth_PPPOE_Discovery, MyEthAddr);

	DiscoveryState = 0;

	/* Prepare for select() */
	if (SessionSocket > maxFD) maxFD = SessionSocket;
	if (DiscoverySocket > maxFD) maxFD = DiscoverySocket;
	maxFD++;

	/* Fill in the constant fields of the packet to save time */
	memcpy(packet.ethHdr.h_dest, PeerEthAddr, ETH_ALEN);
	memcpy(packet.ethHdr.h_source, MyEthAddr, ETH_ALEN);

	packet.ethHdr.h_proto = htons(Eth_PPPOE_Session);
	packet.ver = 1;
	packet.type = 1;
	packet.code = CODE_SESS;
	packet.session = htons(Session);

	pthread_create(&writeSer_info, NULL, writeToSerial, 0);

	//fprintf(stderr,"pppoe started \n");

	//syslog(LOG_DEBUG, "pppoe started \n");

	// pppoe status - IDLE
	databaseWritePPPoEStat(pppoestat_idle);

	for (;;) {
		if(terminate)
			break;
		
		if (optInactivityTimeout > 0) {
			tv.tv_sec = optInactivityTimeout;
			tv.tv_usec = 0;
			tvp = &tv;
		}

		FD_ZERO(&readable);
		FD_SET(TtyPort, &readable);     /* serial ppp packets */
		FD_SET(DiscoverySocket, &readable);
		// FD_SET(SessionSocket, &readable);

		while (1) {
			r = select(maxFD, &readable, NULL, NULL, tvp);
			if (r >= 0 || errno != EINTR) break;
		}

		if (r < 0) {
			fatalSys("select (session)");
		}

		//if (r == 0)
		//{
		//  /* Inactivity timeout */
		//  syslog(LOG_ERR, "Inactivity timeout... something wicked happened");
		//  sendPADT();
		//  exit(1);
		//}

		if(DiscoveryState == STATE_SESSION) {
			if(heartbeatIsTimeToReqEcho() && !heartbeatIsDead())
				sendLCP_ReqEcho();
		}

		/*
		    Handle ready sockets
		*/
		if (FD_ISSET(TtyPort, &readable)) {
			if (DiscoveryState != STATE_SESSION) {
				/* read serial data and ignore it */
			} else {
				ReadFromSerial(TtyPort, &packet);
				/* accumulate data from serial port and send to ethernet when a complete packet
				   has been received. */
			}
		}

		if (FD_ISSET(DiscoverySocket, &readable)) {
			if (DiscoveryState != STATE_SESSION) {
				processDiscoveryPacket(DiscoverySocket, MyEthAddr);
			} else {
				sessionDiscoveryPacket(DiscoverySocket, MyEthAddr);
			}
		}
	}
	
	
	if(Session) {
		syslog(LOG_INFO, "sending PADT to the session");
		sendPADT();
		
		total_retry=5;
		succ=0;
		
		retry=0;
		while(retry++<total_retry) {
			
			syslog(LOG_INFO, "sending LCP to module #%d/%d",retry,total_retry);
			sendLCP_TermReq();
			
			// make sure that port gets back to command mode
			SendATCommand(TTYportFile, "AT", NULL, 0, 1000);
			succ=(SendATCommand(TTYportFile, "AT", NULL, 0, 1000) == 1);
			if(succ)
				break;
		}
		
		if(succ) {
			syslog(LOG_INFO,"successfully switched to AT command");
		}
		else {
			syslog(LOG_ERR,"failed to switch to AT command mode");
		}
	}

	rdb_close_db();
	
	syslog(LOG_INFO, "Terminated");
	
	return 0;
}
/*
* vim:ts=4:sw=4
*/
