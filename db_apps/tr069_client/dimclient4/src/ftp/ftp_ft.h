/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef ftp_ft_h
#define ftp_ft_h

int ftp_login(char *, const char *, const char *);
int ftp_get( char *, char *, int *);
int ftp_put( char *, char *, long * );
void ftp_disconnect (void);

#endif 
