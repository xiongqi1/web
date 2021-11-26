/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef voipParameter_H
#define voipParameter_H

int initVoIP( void );
int initVdMaxLine( const char *, ParameterType, ParameterValue * );
int initVdMaxProfile( const char *, ParameterType, ParameterValue * );
int setVdLineEnable( const char *, ParameterType, ParameterValue * );
int getVdLineEnable( const char *, const ParameterType , ParameterValue *);
int setVdLineDirectoryNumber( const char *, ParameterType, ParameterValue * );
int getVdLineDirectoryNumber( const char *, const ParameterType , ParameterValue *);
int getVdLinePacketsSent( const char *, ParameterType, ParameterValue * );

#endif /* voipParameter_H */
