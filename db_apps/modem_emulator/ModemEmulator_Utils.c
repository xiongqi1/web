/*!
 * Copyright Notice:
 * Copyright (C) 2002-2010 Call Direct Cellular Solutions Pty. Ltd.
 *
 */

#include <stddef.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include "syslog.h"

#include "ModemEmulator.h"
#include "ModemEmulator_Utils.h"

int _callStatus = 255;
int connectionState = -1;
char oldCallStatus=STATUS_IDLE;

extern FILE* comm_host;
extern FILE* comm_phat;
extern char rdb_buf[MAX_RDB_VAR_SIZE];

char *getSingle( char *myname )
{
	if(rdb_get_single(myname, rdb_buf, sizeof(rdb_buf))<0) {
		*rdb_buf=0;
	}
	return rdb_buf;
}

char *getSingleNA( char *myname )
{
	if(rdb_get_single(myname, rdb_buf, sizeof(rdb_buf))<0) {
		strcpy(rdb_buf,"N/A");
	}
	else if(*rdb_buf==0) {
		strcpy(rdb_buf,"N/A");
	}
	return rdb_buf;
}

int getSingleVal( char *myname )
{
	return atoi(getSingleNA(myname));
}

int setSingleVal( char* name, u_long var)
{
	char buf[32];
	sprintf( buf, "%lu", var);

    return rdb_set_single( name, buf );
}

static int unhex(char c)
{
  return (c >= '0' && c <= '9' ? c - '0': c >= 'A' && c <= 'F' ? c - 'A' + 10: c - 'a' + 10);
}

int isHex(char c)
{
	return (unhex(c)>=0 && unhex(c)<=15);
}

int unescape_to_buf(char* d, char* s, int maxd)
{
  char* p;
  p = d;
  while( (p - d) < maxd)
  {
    if(*s == '\0')
      break;
    if( (p - d) == maxd)
      return  - 1;
    if( (*p = * s++) == '%')
    {
      if(*s != '\0')
      {
        *p = unhex(*s) << 4;
        s++;
        if(*s != '\0')
        {
          *p += unhex(*s);
          s++;
        }
      }
    }
    p++;
  }
  *p = '\0';
  return p - d;
}

int baud_to_int(speed_t baud)
{
  switch(baud)
  {
    case B300:
      return 300;
    case B1200:
      return 1200;
    case B2400:
      return 2400;
    case B4800:
      return 4800;
    case B9600:
      return 9600;
      /*  case B14400:
      return 300; */
    case B19200:
      return 19200;
    case B38400:
      return 38400;
    case B57600:
      return 57600;
    case B115200:
      return 115200;
    case B230400:
      return 230400;
    default:
      return  - 1;
  }
}

speed_t int_to_baud(int i)
{
  switch(i)
  {
    case 300:
      return B300;
    case 1200:
      return B1200;
    case 2400:
      return B2400;
    case 4800:
      return B4800;
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    case 230400:
      return B230400;
    default:
      return  - 1;
  }
}

/* Wait for one of the readFds to become readable , writeFds to become writeable
 *   or timeout
 *   return 0 for timeout
 *   return count of readable and writeable fds, and
 *   readMask and writeMask set
 *   > 0 and errno set for error
 *   members of lists can be set < 0 to be skipped
 */
int waitReadWriteFds(unsigned long* readMask, unsigned long* writeMask, int timeout, int nReadFds, int* listReadFds, int nWriteFds, int* listWriteFds)
{
	struct timeval tv;
	fd_set readSet;
	fd_set writeSet;
	int max_fd;
	int i;
	int ret;
	fd_set* readers;
	fd_set* writers;
	unsigned long bit;
	int count;
	if (readMask)
		*readMask = 0;
	if (writeMask)
		*writeMask = 0;
	if (nReadFds < 0 || nReadFds > 31 || nWriteFds < 0 || nWriteFds > 31)
	{
		errno = EINVAL;
		return  - 1;
	}
	max_fd =  - 1;
	readers = (fd_set*)NULL;
	if (listReadFds)
	{
		readers = &readSet;
		FD_ZERO(&readSet);
		for (i = 0; i < nReadFds; i++)
		{
			if (listReadFds[i] >= 0)
			{
				if (listReadFds[i] > max_fd)
					max_fd = listReadFds[i];
				FD_SET(listReadFds[i], &readSet);
			}
		}
	}
	writers = (fd_set*)NULL;
	if (listWriteFds)
	{
		writers = &writeSet;
		FD_ZERO(&writeSet);
		for (i = 0; i < nWriteFds; i++)
		{
			if (listWriteFds[i] >= 0)
			{
				if (listWriteFds[i] > max_fd)
					max_fd = listWriteFds[i];
				FD_SET(listWriteFds[i], &writeSet);
			}
		}
	}
	while (1)
	{
		tv.tv_sec = 0;
		tv.tv_usec = timeout * 1000;
		ret = select(max_fd + 1, readers, writers, (fd_set*)0, timeout < 0 ? NULL : &tv);
		if (ret == 0)
		{
			return 0;
		}
		if (ret < 0)
		{
			if (errno == EAGAIN || errno == EINTR)
				continue;
			return  - 1;
		}
		break;
	}
	count = 0;
	if (readers)
	{
		bit = 1;
		for (i = 0; i < nReadFds; i++)
		{
			if (listReadFds[i] >= 0 && FD_ISSET(listReadFds[i], readers))
			{
				if (readMask)
				{
					*readMask |= bit;
				}
				count++;
			}
			bit <<= 1;
		}
	}
	if (writers)
	{
		bit = 1;
		for (i = 0; i < nWriteFds; i++)
		{
			if (listWriteFds[i] >= 0 && FD_ISSET(listWriteFds[i], writers))
			{
				if (writeMask)
				{
					*writeMask |= bit;
				}
				count++;
			}
			bit <<= 1;
		}
	}
	if (count == 0)
		return  - 1;
	return count;
}

int cfg_port_details(int fd, speed_t baud, unsigned char vmin, unsigned char vtime, int blocking)
{
  struct termios tty_struct;

  tcgetattr(fd, &tty_struct);

  /* set up for raw operation */

  tty_struct.c_lflag = 0; /* no local flags */
  tty_struct.c_oflag &= ~OPOST; /* no output processing */
  tty_struct.c_oflag &= ~ONLCR; /* don't convert line feeds */

  /*
  Disable input processing: no parity checking or marking, no
  signals from break conditions, do not convert line feeds or
  carriage returns, and do not map upper case to lower case.
   */
  tty_struct.c_iflag &= ~(INPCK | PARMRK | BRKINT | INLCR | ICRNL | IUCLC | IXANY | IXON | IXOFF);

  /* ignore break conditions */
  tty_struct.c_iflag |= IGNBRK;

  /*
  Enable input, and hangup line (drop DTR) on last close.  Other
  c_cflag flags are set when setting options like csize, stop bits, etc.
   */
  tty_struct.c_cflag |= (CREAD | HUPCL);

  /*
  now set up non-blocking operation -- i.e. return from read
  immediately on receipt of each byte of data
   */
  tty_struct.c_cc[VMIN] = vmin;
  tty_struct.c_cc[VTIME] = vtime;

  tty_struct.c_lflag = 0; /* no local flags */
  tty_struct.c_oflag &= ~OPOST; /* no output processing */
  tty_struct.c_oflag &= ~ONLCR; /* don't convert line feeds */
  tty_struct.c_iflag &= ~(INPCK | PARMRK | BRKINT | INLCR | ICRNL | IUCLC | IXANY | IXON | IXOFF);

  // Set baud rate
  if(baud)
    cfsetospeed(&tty_struct, baud);

  tcsetattr(fd, TCSADRAIN, &tty_struct); /* set termios struct in driver */

  if( ! blocking)
    fcntl(fd, F_SETFL, O_NONBLOCK);
  return 0;
}

int cfg_serial_port(FILE* port, speed_t baud)
{
  return cfg_port_details(fileno(port), baud, 0, 1, 0); /* vmin, vtime, blocking */
}


int CDCS_Sleep(u_long time)
{
  return usleep(time* 1000);
}


// Gets a PID of a process
int getPID(char* process)
{
  FILE* pFile;
  int PID;
  char respBuff[10];
  char msg[20];

  strcpy(msg, "pidof ");
  strcat(msg, process);

  PID = 0;
  if( (pFile = popen(msg, "r") ) == 0)
  {
    syslog(LOG_ERR, "getPID failed to open pipe\n");
    return PID;
  }

  fgets(respBuff, 10, pFile);
  PID = atoi(respBuff);
  pclose(pFile);
  return PID;
}


// kills a process that is running
void killProcess(char* processName)
{
  int PID;
  char PIDStr[6];
  char killMsg[132];

  u_char killsent = 0;
  u_char retries = 0;

  while(1)
  {
    PID = getPID(processName);
    if( ! PID)
      break;
    if(killsent == 0)
    {
      sprintf(PIDStr, "%d", PID);

      // NOTE - use a standard kill meaning kill when you can, otherwise a harsh kill may not actually
      // kill the process if it cannot kill it straight away.
      strcpy(killMsg, "killall ");
      strcat(killMsg, processName);
      strcat(killMsg, " >/dev/null 2>&1");
      system(killMsg);
      killsent = 1;
    }
    else
    {
      // if cannot kill after 10 secs then do a harsh kill
      if(++retries >= 100)
      {
        sprintf(killMsg, "kill -9 %d >/dev/null 2>&1", PID);
        system(killMsg);
      }
      // The process may not die straight away
      // Just wait around until it does.
      // We could put a timeout here if needed...
      CDCS_Sleep(100);
    }
  }
}

int splitat(char* buf, char** newptr, char** ptrs, int nitems, char atchar)
{
  int i;
  char* cp, * next;
  i = 0;
  cp = buf;
  while(cp && i < nitems)
  {
    ptrs[i++] = cp;
    if(*cp == '\0')
    {
      *newptr = cp;
      return i;
    }
    next = strchr(cp, atchar);
    if(next)
    {
      *next++ = '\0';
      cp = next;
      continue;
    }
    if( (next = strchr(cp, '\n') ) != NULL)
    {
      *next = '\0';
      next++;
      if(*next == '\r')
      {
        next++;
      }
      *newptr = next;
      return i;
    }
    if( (next = strchr(cp, '\r') ) != NULL)
    {
      *next = '\0';
      next++;
      if(*next == '\n')
      {
        next++;
      }
      *newptr = next;
      return i;
    }
    *newptr = NULL;
    return i;
  }
  return i;
}

unsigned long remoteAsNumber(char* host)
{
  struct hostent* he;
  unsigned long n;

  if( (he = gethostbyname(host) ) == 0)
    return 0;
  n = ntohl(*(unsigned long*)he->h_addr);
  return n;
}

#define RDBMANAGER_STATISTIC_IFNAME "wwan.0.netif"

const char* get3GIfName()
{
	char *pos;
	static char netIf[128];

	pos=getSingle(RDBMANAGER_STATISTIC_IFNAME);
	if(*pos==0)
	{
		syslog(LOG_ERR, "failed to read %s - %s", RDBMANAGER_STATISTIC_IFNAME, strerror(errno));
		return "wwan0";
	}

	strcpy(netIf,pos);
        strcat(netIf,"0");

        return netIf;
}

// monitors for an active WWAN session
int WWANOpened(void)
{
	//TODO match 888
	struct ifreq ifr;
	int sockfd;

	const char* netif_name=get3GIfName();

	// NOTE - that this socket is just created to be used as a hook in to the socket layer
	// for the ioctl call.
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd != -1)
	{
		strcpy(ifr.ifr_name, netif_name);

		if (ioctl(sockfd, SIOCGIFINDEX, &ifr) != -1)
		{
			if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1)
			{
				connectionState = -1;
				_callStatus = STATUS_IDLE;
				close(sockfd);
				return 0;
			}
			else
			{
				if (ifr.ifr_flags &IFF_UP)
				{
					close(sockfd);
					connectionState = CONNECTION_ACTIVE;
					_callStatus = STATUS_CONNECTED;
					oldCallStatus = STATUS_CONNECTED;
					return 1;
				}
				else
				{
					connectionState = -1;
					_callStatus = STATUS_IDLE;
					close(sockfd);
					return 0;
				}
			}
		}
		else
		{
			connectionState = -1;
			_callStatus = STATUS_IDLE;
		}
	}
	else
		fprintf(stderr, " no socket %d %s\r\n", sockfd, strerror(errno));
	close(sockfd);
	return 0;
}

int isProfileEnabled(int n)
{
	char buf[32];
	sprintf( buf,"link.profile.%u.enable", n );
	return getSingleVal( buf );
}

int isAnyProfileEnabled( void )
{
	char buf[32];
	int i;
	for( i=1; ; i++)
	{
		sprintf( buf,"link.profile.%u.dev", i );
		if( strncmp( getSingle(buf), "wwan.0", 6 )==0 )
		{
			if(isProfileEnabled(i))
				return i;
		}
		else
			break;

	}
	return 0;
}
#define IPSEC_PFX "ipsec "

void stopIPSec(void)
{
	if (getPID("pluto"))
	{
		system(IPSEC_PFX "whack --shutdown --ctlbase /tmp/ipsec");
		system(IPSEC_PFX "tncfg --detach --virtual ipsec0");
		system("ifconfig ipsec0 down");
	}
}

// Kills the WWAN session
void CDCS_WWAN_Terminate( void )
{
char buf[32];
int i;
	stopIPSec();
	connectionState = -1;
	_callStatus = STATUS_IDLE;
	oldCallStatus = STATUS_IDLE;

	for(i=1; i<=6; i++)
	{
		if (isProfileEnabled(i))
		{
			sprintf( buf,"link.profile.%u.enable", i );
			setSingleVal( buf, 0 );
		}
	}
}

// this routine loads a specific profile from the XML file so that it can be modified
int CDCS_getWWANProfile(WWANPARAMS* p, int num)
{
char buf[64];
	if( num<1 || num>6 )
		return -1;
	sprintf( buf, "link.profile.%u.dialstr", num);
	strncpy( p->dial_number, getSingle(buf), 31);

	sprintf( buf, "link.profile.%u.user", num);
	strncpy( p->user, getSingle(buf), 127);

	sprintf( buf, "link.profile.%u.pass", num);
	strncpy( p->pass, getSingle(buf), 127);

	sprintf( buf, "link.profile.%u.pad_mode", num);
	p->padm = (u_char)getSingleVal(buf);

	sprintf( buf, "link.profile.%u.pad_o", num);
	p->pado = (u_char)getSingleVal(buf);

	sprintf( buf, "link.profile.%u.pad_host", num);
	strncpy( p->rhost, getSingle(buf), 127);

	sprintf( buf, "link.profile.%u.pad_encode", num);
	p->padp = (u_short)getSingleVal(buf);

	sprintf( buf, "link.profile.%u.apn", num);
	strncpy( p->apn_name, getSingle(buf), 127);

	sprintf( buf, "link.profile.%u.pad_connection_op", num);
	p->connection_op = (u_char)getSingleVal(buf);

	sprintf( buf, "link.profile.%u.tcp_nodelay", num);
	p->tcp_nodelay = (u_char)getSingleVal(buf);

	sprintf( buf, "link.profile.%u.readonly", num);
	p->readOnly = (u_char)getSingleVal(buf);

	return 0;
}

int CDCS_SaveWWANConfig(WWANPARAMS* p, int num)
{
char buf[32];
char buf2[16];
	if( num<1 || num>6 )
		return -1;
	sprintf( buf, "link.profile.%u.dev", num);
	rdb_set_single(buf, "wwan.0" );

	sprintf( buf, "link.profile.%u.dialstr", num);
	rdb_set_single(buf, p->dial_number);

	sprintf( buf, "link.profile.%u.user", num);
	rdb_set_single(buf, p->user);

	sprintf( buf, "link.profile.%u.pass", num);
	rdb_set_single(buf, p->pass);

	sprintf( buf, "link.profile.%u.pad_mode", num);
	setSingleVal(buf, p->padm);

	sprintf( buf, "link.profile.%u.pad_o", num);
	setSingleVal(buf, p->pado);

	sprintf( buf, "link.profile.%u.pad_host", num);
	rdb_set_single(buf, p->rhost);

	sprintf( buf, "link.profile.%u.pad_encode", num);
	setSingleVal(buf, p->padp);

	sprintf( buf, "link.profile.%u.apn", num);
	rdb_set_single(buf, p->apn_name);

	sprintf( buf, "link.profile.%u.name", num);
	if(*getSingle(buf)==0) {
		sprintf( buf2, "Profile_%u", num);
		rdb_set_single(buf, buf2);
	}
	setSingleVal("admin.remote.pad_encode", p->padp);
	return 0;
}

int CDCS_LoadMappingConfig(TABLE_MAPPING* p, int num)
{
	char buf[36];
	char *pos, *pos1, *pos2;

	sprintf(buf, "service.firewall.dnat.%u", num);
	pos=getSingle( buf );
	if( *pos==0 )
		return -1;
	pos2=strstr( pos, "-p ");
	if(pos2)
	{
		pos1=strstr( pos, " -s ");
		if(pos1) *pos1=0;
		strncpy(p->protocol, pos2+3, 4);
	}
	else
		return -1;

	if(pos1)
	{
		pos2=strchr(pos1+4, ' ');
		if(!pos2)
			return -1;
		*pos2=0;
		strncpy(p->remoteSrcIPAddr, pos1+4, 32);
		pos=pos2+1;
	}
	else
		return -1;
	pos1=strstr( pos, "--dport ");
	if(pos1)
	{
		pos2=strchr(pos1+8, ' ');
		if(!pos2)
			return -1;
		*pos2=0;
		pos=pos2+1;
		p->localPort1=atoi( pos1+8 );
		pos2=strchr(pos1+8, ':');
		if(pos2)
			p->localPort2=atoi( pos2+1 );
		else
			p->localPort2=p->localPort1;
	}
	else
		return -1;
	pos1=strstr( pos, "--to-destination ");
	if( !pos1 )
		return -1;
	pos2=strchr(pos1+3, ':');
	if(!pos2)
		return -1;
	*pos2=0;
	strncpy(p->destIPAddr, pos1+17, 32);
	p->destPort1=atoi( pos2+1 );
	pos = strchr(pos2+1, '-');
	if(pos)
		p->destPort2=atoi( pos+1 );
	else
		p->destPort2=p->destPort1;
	return 0;
}

int CDCS_SaveMappingConfig(TABLE_MAPPING* p, int num )
{
	char buf[256];
	char cmd[32];
	char *pos;

	if(num>=1)
	{
		sprintf(cmd, "service.firewall.dnat.%u", num-1);
		pos=getSingle( cmd );
		if( *pos==0 )
			return -1;
	}
	else if(num<0)
		return -1;
	if(inet_addr(p->remoteSrcIPAddr) == INADDR_ANY)
		sprintf(buf, "\"-p %s --dport %u:%u -i [wanport] -j DNAT --to-destination %s:%u-%u \"", p->protocol,  p->localPort1, p->localPort2, p->destIPAddr, p->destPort1, p->destPort2);
	else
		sprintf(buf, "\"-p %s -s %s --dport %u:%u -i [wanport] -j DNAT --to-destination %s:%u-%u \"", p->protocol, p->remoteSrcIPAddr, p->localPort1, p->localPort2, p->destIPAddr, p->destPort1, p->destPort2);

	sprintf(cmd, "service.firewall.dnat.%u", num);
	rdb_set_single( cmd, buf );
	sprintf(cmd, "service.firewall.dnat.%u", num+1);
	pos=getSingle( cmd );
	if( *pos==0 )
		rdb_set_single( cmd, "" );
	rdb_set_single( "service.firewall.dnat.trigger", "1" );
	return 0;
}

int CDCS_DeleteMappingConfig(int num)
{
	char cmd[32];
	char *pos;
	int i, j;

	if(num<0)
		return -1;
	sprintf(cmd, "service.firewall.dnat.%u", num);
	pos=getSingle( cmd );
	if( *pos==0 ) //checking empty rule
		return -1;
	sprintf(cmd, "service.firewall.dnat.%u", num+1);
	pos=getSingle( cmd );
	if( *pos==0 ) //checking last rule
	{
		if(num>0)
		{
			sprintf(cmd, "service.firewall.dnat.%u", num-1);
			pos=getSingle( cmd );
			if( *pos==0 )
				return -1;
		}
		sprintf(cmd, "service.firewall.dnat.%u", num);
		rdb_set_single( cmd, "" );
	}
	else
	{
		for(i=num+2;;i++)// find last rule
		{
			sprintf(cmd, "service.firewall.dnat.%u", i);
			pos=getSingle( cmd );
			if( *pos==0 ) //find last rule
				break;
		}
		for( j=num+1; j<i; j++)
		{
			sprintf(cmd, "service.firewall.dnat.%u", j);
			pos=getSingle( cmd );
			sprintf(cmd, "service.firewall.dnat.%u", j-1);
			rdb_set_single( cmd, pos );
		}
		sprintf(cmd, "service.firewall.dnat.%u", i-1);//remove last rule
		rdb_set_single( cmd, "" );
	}
	rdb_set_single( "service.firewall.dnat.trigger", "1" );
	return 0;
}


extern int aChToFd[];
extern int g_aIoErr[];

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void InitPhysSerialStat(int iCh, int fd)
{
	aChToFd[iCh] = fd;
	syslog(LOG_DEBUG,"%s: aChToFd[%d] = %d", __func__, iCh, fd);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetPhysSerialStatInt(int iCh, int fGet, int* pStat)
{
	int curFd = aChToFd[iCh];
	int succ;
	int ioCtl;

	if (!curFd)
		return -1;
	if (fGet)
		ioCtl = TIOCMGET;
	else
		ioCtl = TIOCMSET;
	succ = ioctl(curFd, ioCtl, pStat);
	g_aIoErr[iCh] = 0;
	//fprintf(stderr, __func__" ch=%d, fd=%d pStat=%08x\n", iCh, curFd, *pStat);
	//syslog(LOG_DEBUG,"%s: ch=%d, fd=%d pStat=%08x\n", __func__, iCh, curFd, *pStat);
	if (succ < 0)
	{
		g_aIoErr[iCh] = 1;
		return -1;
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int TogglePhysSerialStatInt(int iCh, int fOn, int nStat)
{
	int curFd = aChToFd[iCh];

	int ioCtl;
	int succ;

	if (!curFd)
		return -1;

	if (fOn)
		ioCtl = TIOCMBIS;
	else
		ioCtl = TIOCMBIC;

	//syslog(LOG_DEBUG,"%s: ch=%d, fd=%d pStat=%08x\n", __func__, iCh, curFd, nStat);
	succ = ioctl(curFd, ioCtl, &nStat);
	if (succ < 0)
	{
	    syslog(LOG_DEBUG,"Failed ioctrl %d\n", succ);
		return -1;
	}

	return 0;
}

int TogglePhysSerialStatOn(int iCh, int nStat)
{
	return TogglePhysSerialStatInt(iCh, 1, nStat);
}

int TogglePhysSerialStatOff(int iCh, int nStat)
{
	return TogglePhysSerialStatInt(iCh, 0, nStat);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ConvSendCtlToV24(unsigned char bCtl)
{
	int v24 = 0;

	if (bCtl & S_RTC)
		v24 |= TIOCM_DTR;
	if (bCtl & S_RTR)
		v24 |= TIOCM_RTS;

	return v24;
}

#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
int V24_init_GPIO()
{
	int rval = 1;
	CHK(gpio_request_pin(DCE_PORT_RTS_IN));
	CHK(gpio_request_pin(DCE_PORT_DTR_IN));
	CHK(gpio_request_pin(DCE_PORT_CTS_OUT));
	CHK(gpio_request_pin(DCE_PORT_DSR_OUT));
	CHK(gpio_request_pin(DCE_PORT_RI_OUT));
	CHK(gpio_request_pin(DCE_PORT_DCD_OUT));

	CHK(gpio_set_input(DCE_PORT_RTS_IN));
	CHK(gpio_set_input(DCE_PORT_DTR_IN));
	CHK(gpio_gpio(DCE_PORT_CTS_OUT));
	CHK(gpio_gpio(DCE_PORT_DSR_OUT));
	CHK(gpio_gpio(DCE_PORT_RI_OUT));
	CHK(gpio_gpio(DCE_PORT_DCD_OUT));
	CHK(gpio_set_output(DCE_PORT_CTS_OUT, 0));
	CHK(gpio_set_output(DCE_PORT_DSR_OUT, 1));
	CHK(gpio_set_output(DCE_PORT_RI_OUT, 0));
	CHK(gpio_set_output(DCE_PORT_DCD_OUT, 0));

	return rval;
}
#endif

void send_keep_alive_at( void )
{
	char pResp[AT_RETRY_BUFFSIZE];

	if ( SendATCommand(comm_phat, "AT", pResp, AT_RETRY_BUFFSIZE, 1 ) != OK )
	{
		syslog(LOG_DEBUG,"AT failed");
		return;
	}
	//if (strstr(pResp, "OK"))
	//	syslog(LOG_DEBUG,"AT cmd got OK");
	//else
	//	syslog(LOG_DEBUG,"AT cmd got '%s'", pResp);
}


#define RDB_SYSLOG_MASK			"service.syslog.option.mask"
void check_logmask_change(void)
{
	static char prev_logmask[1024] = {0, };
	char logmask[1024] = {0, };
	char *pToken, *pToken2, *pSavePtr, *pSavePtr2, *pATRes;
	int llevel = LOG_ERR;

	strcpy(logmask, getSingle(RDB_SYSLOG_MASK));
	if (logmask[0] == 0 || strlen(logmask) == 0)
	{
		//syslog(LOG_ERR, "mask value is NULL");
		setlogmask(LOG_UPTO(llevel));
		(void)strcpy(prev_logmask, "");
		return;
	}
	//syslog(LOG_ERR, "Got LOGMASK command : '%s'", logmask);
	if (strcmp(logmask, prev_logmask) == 0) {
		//syslog(LOG_ERR, "same mask value, skip");
		return;
	}

	(void)strcpy(prev_logmask, logmask);
	pATRes = logmask;
	pToken = strtok_r(pATRes, ";", &pSavePtr);
	while (pToken) {
		pToken2 = strtok_r((char *)pToken, ",", &pSavePtr2);
		//syslog(LOG_ERR, "pToken2 '%s', pSavePtr2 '%s'", pToken2, pSavePtr2);
		if (!pToken2 || !pSavePtr2) {
			syslog(LOG_ERR, "wrong mask value, skip");
		} else if (strcmp(pToken2, "me") == 0) {
			//syslog(LOG_ERR, "found ME logmask");
			llevel = atoi(pSavePtr2);
			if (llevel < LOG_EMERG || llevel > LOG_DEBUG) {
				syslog(LOG_ERR, "wrong log level value, skip");
			} else {
				//syslog(LOG_ERR, "set '%s' log level to %d", pToken2, llevel);
				//setlogmask(LOG_UPTO(llevel));
				break;
			}
		}
		pToken = strtok_r(NULL, ";", &pSavePtr);
	}
	setlogmask(LOG_UPTO(llevel));
	return;
}

// this test toggles 4 outputs and reads the 2 inputs in the loop
//not enabled in production builds
#if 0
void LED_Test(void)
{
    unsigned long this_port_vals = 0;

    int pin, i = 0;
    int state = 0;

    while (1)
    {
        if (GetPhysSerialStatInt(IND_COUNT_PORT, 1, (int *)&this_port_vals) < 0) {
            return;
        }

        syslog(LOG_DEBUG,"pStat=%08x\n, DTR=%s, RTS=%s", this_port_vals, DTR_ON ? "On" : "Off", (this_port_vals & TIOCM_CTS) ? "On" : "Off");

        switch (i)
        {
        case 0:
            pin = TIOCM_OUT1;
            break;
        case 1:
            pin = TIOCM_OUT2;
            break;
        case 2:
            pin = TIOCM_DTR;
            break;
        case 3:
            pin = TIOCM_RTS;
            break;
        default:
            return;
        }
        TogglePhysSerialStatInt(IND_COUNT_PORT, state, pin);

        if (++i == 4)
        {
            i = 0;
            state = 1 - state;
        }

        if (GetPhysSerialStatInt(IND_COUNT_PORT, 1, (int *)&this_port_vals) < 0) {
            return;
        }

        syslog(LOG_DEBUG,"Toggle pin %08x, state %d, pStat=%08x\n", pin, state, this_port_vals);

        sleep (5);
    }
}
#endif

/*
 * vim:ts=4:sw=4
  */
