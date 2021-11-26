/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/* TCP client in the Internet domain */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int sock;

static int readresult( int );
static int writeline( int, const char * );
static int readline( int, char *, int );
static int get_char( int );
static int sockconnect( const char *, const char * );
static void errorexit( const char * );
static void errormsg( const char * );

int main(int argc, char *argv[])
{
	int n;
	int loopcnt = 1;
	char *hostname, *port;

  if (argc != 3) { 
		hostname = "localhost";
		port = "8081";
  }else{
		hostname = argv[1];
		port = argv[2];
  }

	for (;loopcnt > 0 ; loopcnt --)
	{

#if 1

	if (( sock = sockconnect( hostname, port )) < 0 )
			errorexit( "socket connect");

    n = writeline(sock, "set");
    if (n < 0) { errormsg("Sending cmd\n"); close(sock); continue; }
    n = writeline(sock, "Device.ManagementServer.URL");
    if (n < 0) { errormsg("Sending para\n"); close(sock); continue; }
    n = writeline(sock, "http://test.dimark.com:8080/dps/TR069");
    if (n < 0) { errormsg("Sending value\n"); close(sock); continue; }
	n = readresult(sock);
    close(sock);

#endif

#if 1

	// Set the InternetGatewayDevice.ManagementServer.PeriodicInformInterval to 90
	//
	if (( sock = sockconnect( hostname, port )) < 0 )
			errorexit( "socket connect");

    n = writeline(sock, "set");
    if (n < 0) { errormsg("Sending cmd\n"); close(sock); continue; }
    n = writeline(sock, "InternetGatewayDevice.ManagementServer.PeriodicInformInterval");
    if (n < 0) { errormsg("Sending para\n"); close(sock); continue; }
    n = writeline(sock, "90");
    if (n < 0) { errormsg("Sending value\n"); close(sock); continue; }
	n = readresult(sock);
    close(sock);
	sleep(1);

	// Read back InternetGatewayDevice.ManagementServer.PeriodicInformInterval
	//
	if (( sock = sockconnect( hostname, port )) < 0 )
		errorexit( "socket connect");

    n = writeline(sock, "get");
    if (n < 0) { errormsg("Sending cmd\n"); close(sock); continue; }
    n = writeline(sock, "InternetGatewayDevice.ManagementServer.PeriodicInformInterval");
    if (n < 0) { errormsg("Sending param\n"); close(sock); continue; }
	n = readresult(sock);
    close(sock);
	sleep(1);

	// Set a string type parameter
	//
	if (( sock = sockconnect( hostname, port )) < 0 )
		errorexit( "socket connect");

    n = writeline(sock, "set");
    if (n < 0) { errormsg("Sending cmd\n"); close(sock); continue; }
    n = writeline(sock,"InternetGatewayDevice.DeviceInfo.Description");
    if (n < 0) { errormsg("Sending param\n"); close(sock); continue; }
    n = writeline(sock,"A very new Description");
    if (n < 0) { errormsg("Sending value\n"); close(sock); continue; }
	n = readresult(sock);
    close(sock);
	sleep(1);

	// add a new Object
	if (( sock = sockconnect( hostname, port )) < 0 )
		errorexit( "socket connect");

    n = writeline(sock, "add");
    if (n < 0) { errormsg("Sending cmd\n"); close(sock); continue; }
    n = writeline(sock,"InternetGatewayDevice.Layer3Forwarding.Forwarding.");
    if (n < 0) { errormsg("Sending param\n"); close(sock); continue; }
	n = readresult(sock);
    close(sock);
	sleep(1);

	// delete the newly created Object
	if (( sock = sockconnect( hostname, port )) < 0 )
			errorexit( "socket connect");

	n = writeline(sock, "del");
    if (n < 0) { errormsg("Sending cmd\n"); close(sock); continue; }
    n = writeline(sock,"InternetGatewayDevice.Layer3Forwarding.Forwarding.2.");
    if (n < 0) { errormsg("Sending para\n"); close(sock); continue; }
	n = readresult(sock);
    close(sock);
	sleep(1);

#endif
   }
exit(0);
}

static int
readresult( int sock )
{
	char rcvBuf[256];
	int n;

  do
	{
   	n = readline(sock,rcvBuf,255);
   	if (n < 0) { 
			errormsg("recvfrom\n");
			return n;
   	}

   	printf("\n---------- FROM CLIENT ----------------\n");
	printf("\nMESSAGE :%s:", rcvBuf);
   	printf("\n---------------------------------------\n");
	}
	while(n>0 && rcvBuf[0] != '\0');

	return n;
}

static int
writeline( int sock, const char *data )
{
  int ret = 0;
  ret = write( sock, data, strlen(data));
  if ( ret > 0 )
		ret = write( sock, "\n", 1 );
  return ret;
}

static int
readline( int fd, char *buf, int len )
{
  int i = len;
  int c;
	
  bzero(buf, len);
  while( --i > 0 ) {
    c = get_char(fd);
    if ((char)c == '\r' || (char)c == '\n')
        break;
    if ((int)c == EOF)
        return EOF;
    *buf++ = (char)c;
  }
  if (c != '\n')
    c = get_char(fd); /* got \r, now get \n */
  if (c == '\n') 
    *buf = '\0';
  else if ((int)c == EOF)
    return EOF;
  
  return i;
}

static int
get_char(int fd )
{
  char c;

  if ( read(fd, &c, sizeof(c)) <= 0 )
    return EOF;
  else
    return (int)c;
}

static int
sockconnect(const char *hostname, const char *port)
{
  int sock;
  struct sockaddr_in server;
  struct timespec timeout;
  struct hostent *hp;

  sock= socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
		errormsg("socket\n");
		return sock;
  }
  timeout.tv_sec = 5;
  timeout.tv_nsec = 0;
  setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( struct timespec ));

  hp = gethostbyname(hostname);            /* Server IP Address */
  if (hp==0) {
	 errormsg("Unknown host\n");
	 return -1;
  }

  bzero((char*)&server, sizeof( server));
  server.sin_family = AF_INET;
  memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);
  server.sin_port = htons(atoi(port));  /* Destination Port */
  //length=sizeof(struct sockaddr_in);

	if( connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0 ) {
		errormsg( "Error connect");
		return -1;
  }
  return sock;
}

void errorexit( const char *msg )
{
	errormsg(msg);
	exit(0);
}

static void
errormsg( const char *msg )
{
    perror (msg);
}
