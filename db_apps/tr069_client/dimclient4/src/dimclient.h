/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef dimclient_H
#define dimclient_H

#include "httpda.h"

extern struct http_da_info info;
extern struct soap *globalSoap;

int authorizeClient( struct soap *, struct http_da_info *);
enum _Enum_1 *soap_in_xsd__boolean_(struct soap *, const char *, enum _Enum_1 *, const char *);
int soap_out_xsd__boolean_(struct soap *, const char *, int, const enum _Enum_1 *, const char *);
void soap_default_xsd__boolean_(struct soap *, enum _Enum_1 *);

#endif /* dimclient_H */
