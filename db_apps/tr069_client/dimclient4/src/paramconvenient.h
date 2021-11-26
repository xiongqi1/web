/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef paramconvenient_H
#define paramconvenient_H

#include "utils.h"

/**	get	some useful Parameter values */
unsigned int getPeriodicInterval( void );
unsigned int getPeriodicTime( void );
const char *getServerURL( void );
const char *getUsername( void );
const char *getPassword( void );
int getConnectionURL( void * );
int getConnectionUsername( void * );
int getConnectionPassword( void * );
struct DeviceId* getDeviceId( void );

int initConfStruct(const char *confPath );
char *getServerURLFromConf( void );
char *getUsernameFromConf( void );
char *getPasswordFromConf( void );

#ifdef WITH_STUN_CLIENT
const char *getServerAddr( void );
#endif /* WITH_STUN_CLIENT */

#endif /* paramconvenient_H */
