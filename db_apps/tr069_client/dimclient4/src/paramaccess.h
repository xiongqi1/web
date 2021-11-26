/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef paramaccess_H
#define paramaccess_H

#include "parameter.h"

#define STRING_GET 	*(char **)
#define INT_GET		*(int *)
#define UINT_GET	*(unsigned int *)
#define BOOL_GET	*(int *)
#define TIME_GET	*(time_t *)

#define INT_SET		**(int **)
#define BOOL_SET	**(int **)
#define UINT_SET	**(unsigned int **)
#define TIME_SET	**(time_t **)
#define STRING_SET	*(char**)
#define BASE64_SET	*(char**)

/* Because we have to store different kind of values
 * use a union
 */
typedef union
	{
		char *in_cval;	/*! Characters, Bytes and Base64 into param access */
		char *out_cval;	/*! Characters, Bytes and Base64 from param access */
		int in_int;					/*! Integers into param access */
		int out_int;				/*! Integers from param access */
		unsigned int in_uint;		/*! Unsigned Integers into param access */
		unsigned int out_uint;		/*! Unsigned Integers from param access */
		time_t in_timet;			/*! Date times Integers into param access */
		time_t out_timet;			/*! Date times from param access */
	} ParameterValue;

extern char *server_url;

int initAccess( int, const char*, ParameterType, ParameterValue * );
int deleteAccess( int, const char*, ParameterType, ParameterValue * );
int getAccess( int, const char *, ParameterType, ParameterValue * );
int setAccess( int, const char*, ParameterType, ParameterValue * );
int getUdpCRU ( const char *, ParameterType, ParameterValue * );
int is_IP_local( char * );

#endif /* paramaccess_H */
