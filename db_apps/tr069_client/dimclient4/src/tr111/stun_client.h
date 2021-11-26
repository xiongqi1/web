/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef __stun_h__
#define __stun_h__

#include "globals.h"

#ifdef WITH_STUN_CLIENT

#define DEFAULT_STUN_SERVER_UDP_PORT	3478
#define DEFAULT_STUN_SERVER_UDP_ADDR	"test.dimark.com"

int stun_client( char *, int, char *, int, char *,int * );

// San 04 june 2011:
int binding_change( char *, int, char *, int, char *,int * );// Creating of binding_change_packet


#endif /* WITH_STUN_CLIENT */

#endif /* __stun_h__ */


