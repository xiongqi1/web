/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#define CLIENT_LISTENER 1

/* 080715 Removed by Dimark to prevent memory leak in httpda.c */
/* when SOAP_DEBUG is defined.  Also, httpda.c follows what    */
/* SOAP_MALLOC as defined in gsoap/stdsoap2.h, rather than     */
/* be overridden by this declaration                           */

// #define SOAP_MALLOC(soap, size) calloc(1, size)

const char *getServerURL( void );


# define SOAP_MAXKEEPALIVE (2000) /* max iterations to keep server connection alive */

#define SOAP_BUFLEN (8192)
#define SOAP_PTRBLK     (32)
#define SOAP_PTRHASH   (32)
#define SOAP_IDHASH    (19)
#define SOAP_BLKLEN    (32)
#define SOAP_TAGLEN    (64)
// must be > 4096 to read cookies 
#define SOAP_HDRLEN  (4096+1024) 

