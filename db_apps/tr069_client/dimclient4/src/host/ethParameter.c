/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/*
 * This is a sample implementation of how to incorporate additional
 * message information between the ACS and the CPE.
 * 
 * This implementation handles configuration information for the
 * eht0 network adapter on the ADI Coyote platform. The eth0 interface
 * is the one on the PCI Board for development purposes that typically
 * hooks into the internal development network.
 *
 * The following Hierarchy is supported:

 *	InternetGatewayDevice.WANDevice
 *
 * The init method will create a new object of the hierarchy type since
 * there could be multiple entries for WANDevices. After that each parameter
 * is created in the client. The following parameters are supported:
 *
 *	Name				Type		Write	Read
 *	Enabled				boolean		yes	yes
 *	WANAccessType			string		no	yes
 *	Layer1UpstreamMaxBitRate	unsignedInt	no	yes
 *	Layer1DownstreamMaxBitRate	unsignedInt	no	yes
 *	PhysicalLinkStatus		string		no	yes
 *	WANAccessProvider		string		no	yes
 *	TotalBytesSent			unsignedInt	no	yes
 *	TotalBytesReceived		unsignedInt	no	yes
 *	TotalPacketsSent		unsignedInt	no	yes
 *	TotalPacketsReceived		unsignedInt	no	yes
 *
 * Information for the device is parsed from the /proc/net/dev file for
 * each method being called. There is a small overhead with this kind of
 * handling, however it assures the most accurate data.
 */

/*
 * Where do we get our information for the statistics from
 */

#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>

#include	"ethParameter.h"
#include	"globals.h"
#include	"utils.h"

#define		_PATH_PROC_NET_DEV		"/proc/net/dev"

struct interface {
  unsigned long	sentBytes;
  unsigned long receivedBytes;
  unsigned long sentPackets;
  unsigned long receivedPackets;
} curData;

static const int upstreamMaxBit = 128000;
static int procnetdev_vsn = 1;
static int enabled = 1;

static int procnetdev_version( char * );
static int getDevFields( char * );
static char *getName( char *, char * );
static int readProcNetDev( void );

/**
 * Determine what version the /proc/net/dev uses.
 */
static int procnetdev_version(char *buf)
{
  if (strstr(buf, "compressed"))
    return 3;
  if (strstr(buf, "bytes"))
    return 2;
  return 1;
}

/**
 * Read the entries and parse the ones we need.
 */
static int getDevFields(char *bp)
{
  switch (procnetdev_vsn) {
  case 3:
    sscanf(bp,
       "%ld %ld %ld %ld",
       &curData.receivedBytes,
	   &curData.receivedPackets,
	   &curData.sentBytes,
	   &curData.sentPackets);
    break;
  case 2:
    sscanf(bp,
       "%ld %ld %ld %ld",
	   &curData.receivedBytes,
	   &curData.receivedPackets,
	   &curData.sentBytes,
	   &curData.sentPackets);
    break;
  case 1:			/* this format does not provide Byte data */
    sscanf(bp,
       "%ld %ld",
	   &curData.receivedPackets,
	   &curData.sentPackets);
    curData.receivedBytes = 0;
    curData.sentBytes = 0;
    break;
  }
  return 0;
}

/**
 * Extract the name from the /proc/net/dev entry.
 */
static char *getName(char *name, char *p)
{
  while (isspace(*p))
    p++;
  while (*p) {
    if (isspace(*p))
      break;
    if (*p == ':') {	/* could be an alias */
      char *dot = p, *dotname = name;
      *name++ = *p++;
      while (isdigit(*p))
	*name++ = *p++;
      if (*p != ':') {	/* it wasn't, backup */
	p = dot;
	name = dotname;
      }
      if (*p == '\0')
	return NULL;
      p++;
      break;
    }
    *name++ = *p++;
  }
  *name++ = '\0';
  return p;
}

/**
 * Read the /proc/net/dev file and parse its messages to extract the ones
 * we need to report back to the caller.
 */
static int readProcNetDev (void)
{
  FILE *fh;
  char buf[512];
  int err;

  fh = fopen(_PATH_PROC_NET_DEV, "r");
  if (!fh) {
    return -1;
  }
  fgets(buf, sizeof buf, fh);	/* eat header lines */
  fgets(buf, sizeof buf, fh);

  /* Find out what version we're using */
  procnetdev_vsn = procnetdev_version(buf);

  /* Read the remaining lines from the file */
  err = 0;
  while (fgets(buf, sizeof buf, fh)) {
    char ifName[80];
    char *s;
    
    s = getName(ifName, buf);
    if (strcmp(ifName, "eth0") == 0) {
      /* Parse the one that we realy need */
      getDevFields(s);
    }
  }
  fclose(fh);
  /*
  if (!err)
    err = if_readconf();
  */

  return err;
}

int getETH0EnabledForInternet( const char *name, ParameterType type, ParameterValue *value )
{
	INT_GET value = enabled;
	return OK;
}


int setETH0EnabledForInternet( const char *name, ParameterType type, ParameterValue *value )
{
	enabled = value->in_int;
	return OK;
}

int getETH0WANAccessType(const char *name, ParameterType type, ParameterValue *value )
{
	STRING_GET value = "Ethernet";  
	return OK;
}

int getETH0Layer1UpstreamMaxBitRate( const char *name, ParameterType type, ParameterValue *value )
{
	INT_GET value = (int)upstreamMaxBit;
	return OK;
}

int getETH0ReceivedBytes( const char *name, ParameterType type, ParameterValue *value )
{
	readProcNetDev();
	UINT_GET value = (unsigned int)curData.receivedBytes;
	return OK;
}

int getETH0ReceivedPackets( const char *name, ParameterType type, ParameterValue *value )
{
	readProcNetDev();
	UINT_GET value = (unsigned int)curData.receivedPackets;
	return OK;
}

int getETH0SentBytes( const char *name, ParameterType type, ParameterValue *value )
{
	readProcNetDev();
	UINT_GET value = (unsigned int)curData.sentBytes;
	return OK;
}

int getETH0SentPackets( const char *name, ParameterType type, ParameterValue *value )
{
	readProcNetDev();
	UINT_GET value = (unsigned int)curData.sentPackets;
	return OK;
}
