/*****************************************************************************
*
*    Copyright (C) 2005 Paul Dwerryhouse
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; version 2 of the License.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*****************************************************************************/

#include "globals.h"

#ifdef WITH_STUN_CLIENT

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "globals.h"
#include "paramaccess.h"
#include "parameterStore.h"
#include "utils.h"
#include "stun_client.h"
#include "stun_packet.h"
#include "stun_util.h"

int stun_client(	char 	*stun_host,
					int 	 stun_port,
					char 	*stun_username,
					int 	 listen_port,
					char 	*my_ip,
					int  	*my_port)
{

    struct sockaddr_in c_addr, srv_addr;
    struct sockaddr_in sa;
    int s, n;
    struct hostent *he;
    unsigned char buf[8192];
    struct timeval timeout;
    char local_addr[32];
    int  local_port;
    struct packet *p, *q;
    struct packet_s *ps;
    char *hostname;
    unsigned short port = DEFAULT_STUN_SERVER_UDP_PORT;
    stun_tid_t tid;

    srandom(time(0));
    my_ip[0] = 0;
    *my_port = 444;

    DEBUG_OUTPUT (
    		dbglog (SVR_DEBUG, DBG_STUN,
    				"stun_client() enter host = %s port = %d username = %s listen_port = %d\n",
    				stun_host,stun_port, stun_username, listen_port);
    )

    hostname = stun_host ;

    s = socket(PF_INET, SOCK_DGRAM, 0);

    if ((he = gethostbyname(hostname)) == NULL) {
    	DEBUG_OUTPUT (
    			dbglog (SVR_ERROR, DBG_STUN, "stun_client->gethostbyname() No such host: \"%s\" h_errno=%d\n", hostname, h_errno);
    	)

        return(-1) ;
    }
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;
    setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timespec));

    if (stun_port != 0)
    	port = stun_port;

    srv_addr.sin_addr = *((struct in_addr *) he->h_addr);
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    memset(&(srv_addr.sin_zero), 0, 8);

    c_addr.sin_family = AF_INET;
//	c_addr.sin_port = htons(7777);
    c_addr.sin_port = htons(listen_port);
    c_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(c_addr.sin_zero), 0, 8);

    create_random_tid(&tid);
    p = packet_create(STUN_MT_BINREQ, tid);
/*
    if (change_ip || change_port) {
		add_change_request(p, change_ip, change_port);
    }

    add_response_address(p, 0, 1600);
    add_reflected_from(p, 0, 1600);
    add_changed_address(p, 0, 1600);
*/
    if (stun_username && !add_username(p, stun_username)) {
    	DEBUG_OUTPUT (
    			dbglog (SVR_ERROR, DBG_STUN, "Can't add username\n");
    	)

        return(-2) ;
    }

    ps = serialise_packet(p);
    bind(s, (struct sockaddr *) &c_addr, sizeof(struct sockaddr));

    /* Ask socket for local IP address of bind	     */
    /* More often that not, this will be "0.0.0.0"   */
    socklen_t sizeofsa = sizeof(sa);

    if ( getsockname( s, (struct sockaddr *)&sa, &sizeofsa) == -1 ) {
      (void) strcpy( local_addr, "" );
      local_port = 0;
    } else {
      (void) strcpy( local_addr, inet_ntoa(sa.sin_addr));
      local_port = (int) ntohs(sa.sin_port);
    }

    DEBUG_OUTPUT (
    		dbglog (SVR_DEBUG, DBG_STUN, "stun_client() local_addr = %s local_port = %d\n", local_addr, local_port);
    )

    /* Sent STUN query to server and expect immediate */
    /* response back                                  */
    sendto(s, ps->data, ps->len, 0, 
	   (struct sockaddr *) &srv_addr,sizeof(struct sockaddr));
    n = recvfrom(s, buf, 8192, 0, 0, 0);
    close(s);
    if(n == -1){
        DEBUG_OUTPUT (
        		dbglog (SVR_ERROR, DBG_STUN,
        				"stun_client->recvfrom() FAIL n = %d errno = %d h_errno = %d\n",
        				n, errno, h_errno);
        )

      return (-3);
    }

    DEBUG_OUTPUT (
    		dbglog (SVR_DEBUG, DBG_STUN,
    				"stun_client() Response from: %s port %d\n",
    				inet_ntoa(srv_addr.sin_addr), ntohs(srv_addr.sin_port));
    )

    /* Parse received packet as per STUN */
    q = packet_parse(buf, n);
    switch (q->type) {
        case STUN_MT_BINREQ:
            DEBUG_OUTPUT (
            		dbglog (SVR_INFO, DBG_STUN, "stun_client() Binding Request\n");
            )
            break;

        case STUN_MT_BINRES:
            DEBUG_OUTPUT (
            		dbglog (SVR_INFO, DBG_STUN, "stun_client() Binding Response\n");
            )
            break;

        case STUN_MT_BINERR:
            DEBUG_OUTPUT (
            		dbglog (SVR_INFO, DBG_STUN, "stun_client() Binding error Response\n");
            )
	    break;

        default:
	    break;
     }
		  
     if (q->mapped_address && (q->type == STUN_MT_BINRES)) {
          DEBUG_OUTPUT (
        		  dbglog (SVR_INFO, DBG_STUN, "Mapped Address: %s port %d\n", addr_ntoa(q->mapped_address->address),q->mapped_address->port);
          )

          strcpy(my_ip,addr_ntoa(q->mapped_address->address));
          *my_port = q->mapped_address->port;
          return OK ;
    }
     return(-4);
}


// San 04 june 2011:
// Creating, sending of binding_change_packet
int binding_change(	char 	*stun_host,
					int 	 stun_port,
					char 	*stun_username,
					int 	 listen_port,
					char 	*my_ip,
					int  	*my_port)
{

    struct sockaddr_in c_addr, srv_addr;
    struct sockaddr_in sa;
    int s, n;
    struct hostent *he;
    unsigned char buf[8192];
    struct timeval timeout;
    char local_addr[32];
    int  local_port;
    struct packet *p, *q;
    struct packet_s *ps;
    char *hostname;
    unsigned short port = DEFAULT_STUN_SERVER_UDP_PORT;
    stun_tid_t tid;

    srandom(time(0));
    my_ip[0] = 0;
    *my_port = 444;

    DEBUG_OUTPUT (
    		dbglog (SVR_DEBUG, DBG_STUN,
    				"binding_change() enter host = %s port = %d username = %s listen_port = %d\n",
    				stun_host,stun_port, stun_username, listen_port);
    )

    hostname = stun_host ;

    s = socket(PF_INET, SOCK_DGRAM, 0);

    if ((he = gethostbyname(hostname)) == NULL) {
    	DEBUG_OUTPUT (
    			dbglog (SVR_ERROR, DBG_STUN, "binding_change->gethostbyname() No such host: \"%s\" h_errno=%d\n", hostname, h_errno);
    	)

        return(-1) ;
    }
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;
    setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timespec));

    if (stun_port != 0)
    	port = stun_port;

    srv_addr.sin_addr = *((struct in_addr *) he->h_addr);
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    memset(&(srv_addr.sin_zero), 0, 8);

    c_addr.sin_family = AF_INET;
//	c_addr.sin_port = htons(7777);
    c_addr.sin_port = htons(listen_port);
    c_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(c_addr.sin_zero), 0, 8);

    create_random_tid(&tid);
    p = packet_create(STUN_MT_BINREQ, tid);
/*
    if (change_ip || change_port) {
		add_change_request(p, change_ip, change_port);
    }

    add_response_address(p, 0, 1600);
    add_reflected_from(p, 0, 1600);
    add_changed_address(p, 0, 1600);
*/

    // San 04 june 2011:
    if (!add_binding_change_attr(p)) {
    	DEBUG_OUTPUT (
    			dbglog (SVR_ERROR, DBG_STUN, "Can't add binding_change_attr\n");
    	)

        return(-2) ;
    }

    if (!add_connection_request_binding_attr(p)) {
    	DEBUG_OUTPUT (
    			dbglog (SVR_ERROR, DBG_STUN, "Can't add connection_request_binding_attr\n");
    	)

        return(-2) ;
    }

    if (stun_username && !add_username(p, stun_username)) {
    	DEBUG_OUTPUT (
    			dbglog (SVR_ERROR, DBG_STUN, "Can't add username\n");
    	)

        return(-2) ;
    }

    ps = serialise_packet(p);
    bind(s, (struct sockaddr *) &c_addr, sizeof(struct sockaddr));

    /* Ask socket for local IP address of bind	     */
    /* More often that not, this will be "0.0.0.0"   */
    socklen_t sizeofsa = sizeof(sa);

    if ( getsockname( s, (struct sockaddr *)&sa, &sizeofsa) == -1 ) {
      (void) strcpy( local_addr, "" );
      local_port = 0;
    } else {
      (void) strcpy( local_addr, inet_ntoa(sa.sin_addr));
      local_port = (int) ntohs(sa.sin_port);
    }

    DEBUG_OUTPUT (
    		dbglog (SVR_DEBUG, DBG_STUN, "binding_change() local_addr = %s local_port = %d\n", local_addr, local_port);
    )

    /* Sent STUN query to server and expect immediate */
    /* response back                                  */
    sendto(s, ps->data, ps->len, 0,
	   (struct sockaddr *) &srv_addr,sizeof(struct sockaddr));
    n = recvfrom(s, buf, 8192, 0, 0, 0);
    close(s);

    DEBUG_OUTPUT (
    		dbglog (SVR_DEBUG, DBG_STUN,
    				"binding_change()->recvfrom() returned value = %d\n",n);
    )

    if(n == -1){
        DEBUG_OUTPUT (
        		dbglog (SVR_ERROR, DBG_STUN,
        				"binding_change->recvfrom() FAIL n = %d errno = %d h_errno = %d\n",
        				n, errno, h_errno);
        )

      return (-3);
    }

    DEBUG_OUTPUT (
    		dbglog (SVR_DEBUG, DBG_STUN,
    				"binding_change() Response from: %s port %d\n",
    				inet_ntoa(srv_addr.sin_addr), ntohs(srv_addr.sin_port));
    )

    /* Parse received packet as per STUN */
    q = packet_parse(buf, n);
    switch (q->type) {
        case STUN_MT_BINREQ:
            DEBUG_OUTPUT (
            		dbglog (SVR_INFO, DBG_STUN, "binding_change() Binding Request\n");
            )
            break;

        case STUN_MT_BINRES:
            DEBUG_OUTPUT (
            		dbglog (SVR_INFO, DBG_STUN, "binding_change() Binding Response\n");
            )
            break;

        case STUN_MT_BINERR:
            DEBUG_OUTPUT (
            		dbglog (SVR_INFO, DBG_STUN, "binding_change() Binding error Response\n");
            )
	    break;

        default:
	    break;
     }

     if (q->mapped_address && (q->type == STUN_MT_BINRES)) {
          DEBUG_OUTPUT (
        		  dbglog (SVR_INFO, DBG_STUN, "Mapped Address: %s port %d\n", addr_ntoa(q->mapped_address->address),q->mapped_address->port);
          )

          strcpy(my_ip,addr_ntoa(q->mapped_address->address));
          *my_port = q->mapped_address->port;
          return OK ;
    }
     return(-4);
}


#endif /* WITH_STUN_CLIENT */
