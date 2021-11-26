/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef WITH_STUN_CLIENT

#include <pthread.h>

#include "stun_client.h"
#include "stun_dimark.h"
#include "paramconvenient.h"

extern pthread_mutex_t informLock;

/*
 * This thread handles the STUN client process, including interaction
 * with the STUN server as well as receiving UDP Connection Requests
 */
void *
stunHandler (void *param)
{
	unsigned int periodicIntervalDelay;
    char *stun_host;
    int  *stun_port, nat_stat;
    char *udp_conn, new_conn[32];
    char mapped_ip[32];
    int  mapped_port;
    int stat;
    bool b;
    int mutexStat = OK;
    char name[32] = { '\0' };
    ParameterType  type = StringType;
    ParameterValue value;
    char *tmp_username,   *tmp_password;
    char  cr_username[32], cr_password[32];

    fflush(stdout);
//    fsync(stdout);

    DEBUG_OUTPUT (
    		dbglog (SVR_INFO, DBG_STUN, "stun_process started\n");
    )

    for (;;) {
    	/* Initial Call to Server */

    	/* mutexGo is unlocked and we wait for condGo
    	 * after cond wait mutexGo is locked
    	 */
    	periodicIntervalDelay = getStunPeriodicInterval(STUNMINIMUMKEEPALIVEPERIOD);
    	fflush(stdout);
    	//	fsync(stdout);

    	DEBUG_OUTPUT (
    			dbglog (SVR_INFO, DBG_STUN, "stun_process periodic interval %d\n",periodicIntervalDelay);
    	)

    	/* periodicIntervalDelay > 0 periodicInform is enabled */
    	if ( periodicIntervalDelay > 0 ) {
    		/* Update local UDP IP address information */
    		stat = getUdpCRU (name, type, &value);

    		DEBUG_OUTPUT (
    				dbglog (SVR_DEBUG, DBG_STUN, "getUdpCRU() = %d value = %s\n", stat, value);
    		)

    		/* is_Stun_enabled() returns value of parameter (e.g */
    		/* 0=disabled, 1=enabled, or -1 if param fetch fails */

    		if (is_Stun_enabled(STUNENABLE) == 1 )
    		{
    			DEBUG_OUTPUT (
    					dbglog (SVR_DEBUG, DBG_STUN, "STUN is enabled\n");
    			)

    			fflush(stdout);
    			//fsync(stdout);

    			/* Fetch stun_host and stun_port parameters */
    			/* If not available, fetch default values */
    			if ( getParameter(STUNSERVERADDRESS, &stun_host) != OK) {
    				DEBUG_OUTPUT (
    						dbglog (SVR_ERROR, DBG_STUN, "STUN parameter fetch of %s failed\n", STUNSERVERADDRESS);
    				)

    				stun_host = DEFAULT_STUN_SERVER_UDP_ADDR;
    			}

    			if ( getParameter(STUNSERVERPORT, &stun_port) != OK)	{
    				DEBUG_OUTPUT (
    						dbglog (SVR_ERROR, DBG_STUN, "STUN parameter fetch of %s failed\n", STUNSERVERPORT);
    				)

    				*stun_port = DEFAULT_STUN_SERVER_UDP_PORT;
    			}

    			DEBUG_OUTPUT (
    					dbglog (SVR_DEBUG, DBG_STUN, "stunHandler->stun_client() pre stun_host = %s stun_port = %d\n", stun_host, *stun_port);
    			)

    			fflush(stdout);
    			//fsync(stdout);

				stat = stun_client(stun_host,*stun_port,NULL,ACS_UDP_NOTIFICATION_PORT,mapped_ip,&mapped_port);

    			DEBUG_OUTPUT (
    					dbglog (SVR_DEBUG, DBG_STUN,
    							"STUN server at %s:%d returned mapped ip %s port %d with status %d\n",
    							stun_host, *stun_port,mapped_ip,mapped_port, stat);
    					)

    			/* if OK, then got a valid response from STUN server */
    			/* use mapped_ip:mapped_port as new UDP connection info */
    			/* Otherwise, use default */
    			if (stat == OK) {
    				sprintf(new_conn,"%s:%d",mapped_ip,mapped_port);
    			}
    			else {
    				DEBUG_OUTPUT (
    						dbglog (SVR_ERROR, DBG_STUN, "stunHandler->stun_client() FAIL stat = %d\n", stat);
    				)
    				getUdpCRU(name,type,&value);
    				sprintf(new_conn,"%s",value.out_cval);
    			}

				if ( (is_IP_local( mapped_ip) == 0) && ( mapped_port == ACS_UDP_NOTIFICATION_PORT))
				{
					nat_stat = 0;		// no addr or port translation detected
				}
				else
				{
					nat_stat = 1;		// translation detected
				}

    			DEBUG_OUTPUT (
    					dbglog (SVR_DEBUG, DBG_STUN, "STUN enabled new_conn = %s  nat_stat = %d\n", new_conn, nat_stat);
    			)

    			if (nat_stat)
    			{
        			// TODO
        			// San 04 june 2011:
    				stat = binding_change(stun_host,*stun_port,NULL,ACS_UDP_NOTIFICATION_PORT,mapped_ip,&mapped_port);
        			DEBUG_OUTPUT (
        					dbglog (SVR_DEBUG, DBG_STUN,
        							"stunHandler()->binding_change(): STUN server at %s:%d returned mapped ip %s port %d with status %d\n",
        							stun_host, *stun_port,mapped_ip,mapped_port, stat);
        					)

    			}
    		}
    		else {
    			/* STUN Disabled, use local info */
    			getUdpCRU(name,type,&value);
    			sprintf(new_conn,"%s",value.out_cval);
    			nat_stat = 0;

    			DEBUG_OUTPUT (
    					dbglog (SVR_DEBUG, DBG_STUN, "STUN disabled new_conn = %s\n", new_conn);
    			)
    		} // end  "if (is_Stun_enabled(STUNENABLE) == 1 )"

    		// Us result we have value in  nat_stat

    		stat = getParameter(UDPCONNECTIONREQUESTADDRESS,&udp_conn);

    		DEBUG_OUTPUT (
    				dbglog (SVR_DEBUG, DBG_STUN, "STUN current conn = %s stat = %d\n", udp_conn, stat);
    		dbglog (SVR_DEBUG, DBG_STUN, "STUN NAT Detected nat_stat = %d\n", nat_stat);
    		)

    		/* If report info has changed, send update to ACS */
    		if ( strcmp(udp_conn,new_conn) != 0 ) {
    			DEBUG_OUTPUT (
    					dbglog (SVR_DEBUG, DBG_STUN, "STUN udp_conn %s != %s new_conn -> update parameters\n", udp_conn, new_conn);
    			)

    			setParameter2Host (UDPCONNECTIONREQUESTADDRESS, &b, new_conn);
    			setParameter2Host (NATDETECTED, &b, (char *)&nat_stat);

    			mutexStat = pthread_mutex_trylock (&informLock);
    			if (mutexStat == OK) {
    				pthread_mutex_unlock (&informLock);
				setAsyncInform(true);

    			}
    		}
    		else {
    			DEBUG_OUTPUT (
    					dbglog (SVR_DEBUG, DBG_STUN, "STUN udp_conn %s == %s new_conn -> no update parameters\n", udp_conn, new_conn);
    			)
    		}

    		/* fetch connection request credentials, default to "dps" "dps" */
    		if ( getParameter(CONNECTION_REQUEST_USERNAME, &tmp_username ) != OK)
    			strcpy(  cr_username, "dps" );
    		else
    			strcpy(  cr_username, tmp_username );

    		if ( getParameter(CONNECTION_REQUEST_PASSWORD, &tmp_password ) != OK)
    			strcpy(  cr_password, "dps" );
    		else
    			strcpy(  cr_password, tmp_password );

    		/* Always listen for UDP Connection Requests regardless of */
    		/* whether STUN is enabled (as per TR111) */
    		rx_udp_connection_request(
    				mapped_ip,
    				mapped_port,
    				ACS_UDP_NOTIFICATION_PORT,
    				periodicIntervalDelay,
    				cr_username,
    				cr_password);

    		usleep(periodicIntervalDelay * 1000000);
    	}
    	else
    		usleep(periodicIntervalDelay == 0 ? 100 * 1000000 : periodicIntervalDelay * 1000000);
    } // for ()
}

#endif /* WITH_STUN_CLIENT */
