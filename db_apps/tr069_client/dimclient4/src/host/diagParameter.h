/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef diagParameter_H
#define diagParameter_H

#include "globals.h"
#include "parameter.h"
#include "paramaccess.h"

int setDownloadDiagnostics( const char *, ParameterType, ParameterValue * );
int setUploadDiagnostics( const char *, ParameterType, ParameterValue * );
int setIPPingDiagnostics( const char *, ParameterType, ParameterValue * );
int setTraceRouteDiagnostics( const char *, ParameterType, ParameterValue * );
int setWANDSLDiagnostics( const char *, ParameterType, ParameterValue * );
int setATMF5Diagnostics( const char *, ParameterType, ParameterValue * );
int setUDPEchoConfig( const char *name, ParameterType type , ParameterValue *value_in );
int SetSmartParam( const char *, void * );

#ifdef HAVE_UDP_ECHO
void *udpHandler(void *);
#endif /* HAVE_UDP_ECHO */

#endif /* diagParameter_H */
