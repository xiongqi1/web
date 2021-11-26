/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef WITH_STUN_CLIENT

#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <openssl/hmac.h>

#include "globals.h"
#include "paramaccess.h"
#include "parameterStore.h"
#include "utils.h"
#include "parameter.h"
#include "stun_dimark.h"
#include "dimclient.h"
#include "eventcode.h"

#define		QVALUE_SIZE		64

extern pthread_mutex_t paramLock ;
extern pthread_mutex_t informLock ;

static char previous_ts_str[32] = "";
static char previous_id_str[32] = "";

// used to inform the mainloop that the acsHandler could bind on the port.
void rx_udp_connection_request(char *host,
			       int  port,
			       int  listen_port,
			       int  sec_timeout,
			       char *cred_username,
			       char *cred_password)
{
    struct sockaddr_in c_addr, srv_addr;
    int s, n;
    struct hostent *he;
    char buf[8192];
    struct timeval timeout;
    int mutexStat = OK;
    time_t end_time, current_time ;

    DEBUG_OUTPUT (
    		dbglog (SVR_DEBUG, DBG_STUN,
    				"rx_udp_connection_request() enter host = %s port = %d listen_port = %d sec_timeout = %d\n",
    				host, port, listen_port, sec_timeout);
    )

//    s = socket(PF_INET, SOCK_DGRAM, 0);

    if ((he = gethostbyname(host)) == NULL) {
    	DEBUG_OUTPUT (
    			dbglog (SVR_ERROR, DBG_STUN, "rx_udp_connection_request->gethostbyname() No such host: \"%s\" h_errno=%d\n", host, h_errno);
    	)

    	return;
    }

// change location of socket() function.
    s = socket(PF_INET, SOCK_DGRAM, 0);

    timeout.tv_sec  = 10;
    timeout.tv_usec = 0;
    setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timespec));


    srv_addr.sin_addr = *((struct in_addr *) he->h_addr);
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    memset(&(srv_addr.sin_zero), 0, 8);

    c_addr.sin_family = AF_INET;
    c_addr.sin_port = htons(listen_port);
    c_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(c_addr.sin_zero), 0, 8);
    bind(s, (struct sockaddr *) &c_addr, sizeof(struct sockaddr));

    end_time = time(NULL);
    current_time = end_time;
    end_time += sec_timeout;

    while(current_time < end_time){
        DEBUG_OUTPUT (
        		dbglog (SVR_DEBUG, DBG_STUN,
        				"rx_udp_connection_request->recvfrom() pre current_time %d < %d end_time\n",
        				current_time, end_time);
        )
      fflush(stdout);
//      fsync(stdout);
      n = recvfrom(s, buf, 8192, 0, 0, 0);

      DEBUG_OUTPUT (
      		dbglog (SVR_DEBUG, DBG_STUN,
      				"STUN rx_udp_connection_request->recvfrom() post n = %3d current_time %d < %d end_time errno = %d\n",
      				n, current_time, end_time, errno);
      )

      fflush(stdout);
//      fsync(stdout);
      if ((n > 0) && (validate_UDPConnectionRequest(buf,n,cred_username,cred_password) == 0)) {
    	  DEBUG_OUTPUT (
    			  dbglog (SVR_INFO, DBG_STUN, "UDP Connection Request validated -> ACS Notification\n");
    	  )
	  mutexStat = pthread_mutex_trylock (&informLock);
	// reset nonce value to force a new Digest challenge next time
	if (mutexStat == OK){
	  pthread_mutex_unlock (&informLock);
	  addEventCodeSingle (EV_CONNECT_REQ);
	  setAsyncInform(true);
	} 
      }
      current_time = time(NULL);
    } // while
    close(s);                      
}

unsigned int
getStunPeriodicInterval( char *param_name )
{
  unsigned int *periodicInterval = NULL;

  if (getParameter (param_name, &periodicInterval) != OK)
    return( DEFAULT_STUN_MINIMUM_KEEP_ALIVE );
	
  if (*periodicInterval == 0 )
    return( DEFAULT_STUN_MINIMUM_KEEP_ALIVE );

  return(*periodicInterval);
}

unsigned int
is_Stun_enabled( char *param_name )
{
  unsigned int *periodicEnabled;

  if ( getParameter(param_name, &periodicEnabled) != OK)
    return (-1);

  DEBUG_OUTPUT (
  		dbglog (SVR_DEBUG, DBG_STUN, "STUN is_Stun_enabled periodicEnabled = %d\n", *periodicEnabled);
  )

  return (*periodicEnabled);
}

/* 
 * validate_UDPConnectionRequest() examines the packet data
 * contents for presence of the information required by TR-111
 * Specifically: 
 *    HTTP GET Request for version HTTP 1.1
 *    Valid URI
 *    Empty PATH
 *    Query String with fields: ts, id, un, cn, and sig
 */
unsigned int
validate_UDPConnectionRequest( char 	*data, 
			       int 	len,	
			       char 	*connection_username,
			       char	*connection_password)
{
   char *end_data, *to,*from;
   int  test_len, count;
   char URI[128];
   char URI_host[64], URI_port[32];
   char ts[QVALUE_SIZE], id[QVALUE_SIZE], un[QVALUE_SIZE], cn[QVALUE_SIZE], sig[QVALUE_SIZE];
   int  ts_f, id_f, un_f, cn_f, sig_f;
   char sha_key[64], sha_text[128], sha_md[64];
   int  sha_key_len, sha_text_len,  sha_md_len;
   HMAC_CTX ctx;
   int i;
   char cmp[64];
   unsigned long previous_ts, this_ts;

   // len      = strlen(test_data);
   // data     = test_data;
   end_data = data + len - 1;
   test_len = data - end_data;

   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN,
   				"validate_UDPConnectionRequest() data = %p len = %d user = %s pass= %s\n",
   				data, len, connection_username, connection_password);
   )

   /* Check for "GET " at start */
   if ((strncmp(data, "GET ", 4) != 0))
     return(-1);
   data += 4;

   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN, "Found \"GET \"\n");
   )

   /* Check for "http://" next  */
   if (strncasecmp( data, "http://", 7 ) != 0)
      return (-2);

   /* Copy URI up to '?' char, or end of URI or data buffer */
   to = URI;
   count = 1;
   while ( (*data != '?' ) && (count<sizeof(URI)) && (data < end_data)) {
     *to = *data;
     to++;
     data++;
     count++;
     if ( count >= sizeof(URI) )
       return(-3);
     if ( data >= end_data )
       return(-4);
   }

   *to = '\0';

   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN, "URI = %s\n",URI);
   )

   /* Extract host portion from URI */
   strcpy(URI_host,"");
   to       = URI_host;
   from     = URI + 7;
   count    = 1;
   while ( (*from != ':') && (*from != '/') && (*from != '\0')) {
     *to = *from;
     to++;
     from++;
     count++;
     if ( count >= sizeof(URI_host) )
       return(-5);
   }
   *to = '\0';

   /* Extract port portion if present */
   strcpy(URI_port,"");
   if ( *from == ':' ) {
     to     = URI_port;
     from  += 1;
     count  = 1;
     while ( (*from != '/') && (*from != '\0')) {
       *to = *from;
       to++;
       from++;
       count++;
       if ( count >= sizeof(URI_port) )
	 return(-6);
     }
     *to = '\0';
   }

   /* If URI contains a path component, reject as per TR111 */
   if (*from == '/')
     return(-7);

   DEBUG_OUTPUT (
		   dbglog (SVR_DEBUG, DBG_STUN, "URI_host = %s\n", URI_host);
		   dbglog (SVR_DEBUG, DBG_STUN, "URI_port = %s\n", URI_port);
   )

   /* Now need to parse out query fields */
   ts[0] = id[0] = un[0] = cn[0] = sig[0] = '\0';
   ts_f  = id_f  = un_f  = cn_f  = sig_f  = 1;

   while ( data < end_data ) {
     data++;
     if ( data >= end_data )
       return(-8);

     /* recognize next query name<>value pair? */
     /* If yes, set ptrs */
     /* Recognize with any order of pairs */
     if ( strncmp(data,"ts=",3)  == 0 ) {
         to    = ts;
	 ts_f  = 0;
	 data += 3;
     }
     if ( strncmp(data,"id=",3)  == 0 ) {
         to    = id;
         id_f  = 0;
	 data += 3;
     }
     if ( strncmp(data,"un=",3)  == 0 ) {
         to    = un;
         un_f  = 0;
	 data += 3;
     } 
     if ( strncmp(data,"cn=",3)  == 0 ) {
         to    = cn;
         cn_f  = 0;
	 data += 3;
     }
     if ( strncmp(data,"sig=",4) == 0 ) {
         to    = sig;
         sig_f = 0;
	 data += 4;
     }

     /* Copy value */
     count = 1;
     while ((*data != '&' ) && (*data != ' ') && ( count<sizeof(ts) ) && ( data < end_data )) {
       *to = *data;
       to++;
       data++;
       count++;
       if ( count >= sizeof(ts) )
	 return(-6);
       if ( data >= end_data )
	 return(-9);
     }
     *to = '\0';

     /* got all values, even if null string? */
     if ( (ts_f == 0) && (id_f == 0) && (un_f == 0) && (cn_f == 0) && (sig_f == 0) )
       break;

   } // while ( data < end_data )

   if ( (ts_f != 0)  || (strlen(ts) == 0 ))  // missing or null ts name<>value
     return(-10);
   if ( (id_f != 0)  || (strlen(id) == 0 ))  // missing or null id name<>value
     return(-11);
   if ( (un_f != 0)  || (strlen(un) == 0 ))  // missing or null un name<>value
     return(-12);
   if ( (cn_f != 0)  || (strlen(cn) == 0 ))  // missing or null cn name<>value
     return(-13);
   if ( (sig_f != 0) || (strlen(sig) == 0 ))  // missing or null sig name<>value
     return(-14);

   /* Should have all required query name<>value pairs now */
   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN, "ts = %s\n",ts);
		dbglog (SVR_DEBUG, DBG_STUN, "id = %s\n",id);
   		dbglog (SVR_DEBUG, DBG_STUN, "un = %s\n",un);
   		dbglog (SVR_DEBUG, DBG_STUN, "cn = %s\n",cn);
   		dbglog (SVR_DEBUG, DBG_STUN, "sig = %s\n",sig);
   )

   /* Skip space terminating query string */
   if ( *data == ' ' )
     data++;

   /* Test if any chars left in receive buffer */
   if ( data >= end_data )
     return(-15);

   /* Check for HTTP Version */
   if (strncmp(data,"HTTP/1.1",8) != 0)
     return(-16);

   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN, "Found \"HTTP/1.1\"\n");
   )

   // Check that un value  matches connection_username
   if ( strcmp( un, connection_username) != 0 )
     return(-17);

   strcpy( sha_key, connection_password);
   sha_key_len = 3;
   sprintf( sha_text, "%s%s%s%s",ts,id,un,cn);
   sha_text_len = strlen(sha_text);
   sha_md_len   = 0;

   HMAC_CTX_init( &ctx);
   HMAC_Init(   &ctx, sha_key,  sha_key_len, EVP_sha1());
   HMAC_Update( &ctx, (unsigned char *)sha_text, sha_text_len );
   HMAC_Final(  &ctx, (unsigned char *)sha_md, (unsigned int *)&sha_md_len );


   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN, "sha_key [%3d] = %s\n", sha_key_len, sha_key);
		dbglog (SVR_DEBUG, DBG_STUN, "sha_text[%3d] = %s\n", sha_text_len, sha_text);
   )

   to = cmp;

   for (i = 0; i < sha_md_len; i++) {

      sprintf(to,"%02x", sha_md[i] & 0xff);
      to += 2;
   }

   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN, "sig [%3d] = %s\n", strlen(sig), sig);
		dbglog (SVR_DEBUG, DBG_STUN, "cmp [%3d] = %s\n", strlen(cmp), cmp);
   )

   i = strncasecmp( sig, cmp, strlen(sig));

   HMAC_CTX_cleanup( &ctx );

   if ( i !=  0 ) {
	   DEBUG_OUTPUT (
	   		dbglog (SVR_ERROR, DBG_STUN, "STUN UDP Connection Request failed authentication\n");
	   )

     return(-18); 
   }

   DEBUG_OUTPUT (
   		dbglog (SVR_INFO, DBG_STUN, "STUN UDP Connection Request is Authentic\n");
   )

   /************************************************/
   /* UDP Connection Request Message Is AUTHENTIC! */
   /************************************************/

   /* Check that ts value is larger than previous_ts */
   /* If previous string is null, this is first time */
   if (strlen(previous_ts_str) != 0 ) {
     previous_ts = atol(previous_ts_str);
     this_ts     = atol(ts);
     if ( this_ts <= previous_ts ) {
    	   DEBUG_OUTPUT (
    	   		dbglog (SVR_ERROR, DBG_STUN, "STUN Check timestamp failed\n");
    	   )

       return(-19);
     }
   }
   strcpy( previous_ts_str, ts );
   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN, "STUN Check timestamp OK\n");
   )

   /* Check that id value different from previous */
   /* If previous string is null, this is first time */
   if ( strlen(previous_id_str) != 0 ) {
     if ( strcmp( id, previous_id_str) == 0 ) {
  	   DEBUG_OUTPUT (
  	   		dbglog (SVR_ERROR, DBG_STUN, "STUN Check ID failed\n");
  	   )

       return(-20);
     }
   }

   strcpy( previous_id_str, id );
   DEBUG_OUTPUT (
   		dbglog (SVR_DEBUG, DBG_STUN, "STUN Check ID OK\n");
   )

   return(0);
}

#endif /* WITH_STUN_CLIENT */
