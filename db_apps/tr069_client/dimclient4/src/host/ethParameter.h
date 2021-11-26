/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef ethParameter_H
#define ethParameter_H

#include "parameter.h"
#include "paramaccess.h"

#define ETH0_START_IDX 800

int getETH0EnabledForInternet( const char *, ParameterType, ParameterValue * );
int setETH0EnabledForInternet( const char *, ParameterType, ParameterValue * );
int getETH0WANAccessType( const char *, ParameterType, ParameterValue * );
int getETH0Layer1UpstreamMaxBitRate( const char *, ParameterType, ParameterValue * );
int getETH0ReceivedBytes( const char *, ParameterType, ParameterValue * );
int getETH0ReceivedPackets( const char *, ParameterType, ParameterValue * );
int getETH0SentBytes( const char *, ParameterType, ParameterValue * );
int getETH0SentPackets( const char *, ParameterType, ParameterValue * );

#endif /* ethParameter_H */
