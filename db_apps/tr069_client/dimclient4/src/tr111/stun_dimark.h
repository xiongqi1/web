/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef stun_dimark_H
#define stun_dimark_H

#include "globals.h"

#ifdef WITH_STUN_CLIENT

#define 	DEFAULT_STUN_MINIMUM_KEEP_ALIVE		100

void rx_udp_connection_request (char *, int, int, int, char *, char *);
unsigned int getStunPeriodicInterval (char *);
unsigned int is_Stun_enabled (char *);
unsigned int validate_UDPConnectionRequest (char *, int, char *, char *);

#endif /* WITH_STUN_CLIENT */

#endif /* stun_dimark_H */
