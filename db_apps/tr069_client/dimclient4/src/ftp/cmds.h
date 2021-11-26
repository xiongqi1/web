/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef cmds_H
#define cmds_H

int doLogin( const char *host, const char *user, const char *pass );
int dimget( char *remoteFile, char *localFile, long *size );
int dimput( char *remoteFile, char *localFile, long *size );
void disconnect(void);
void setpassive(void);

#endif
