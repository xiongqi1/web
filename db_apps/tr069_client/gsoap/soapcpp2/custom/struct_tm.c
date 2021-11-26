/*

struct_tm.c

Custom serializer for <time.h> struct tm

gSOAP XML Web services tools
Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc., All Rights Reserved.
This part of the software is released under one of the following licenses:
GPL, the gSOAP public license, or Genivia's license for commercial use.
--------------------------------------------------------------------------------
gSOAP public license.

The contents of this file are subject to the gSOAP Public License Version 1.3
(the "License"); you may not use this file except in compliance with the
License. You may obtain a copy of the License at
http://www.cs.fsu.edu/~engelen/soaplicense.html
Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the License.

The Initial Developer of the Original Code is Robert A. van Engelen.
Copyright (C) 2000-2005, Robert van Engelen, Genivia, Inc., All Rights Reserved.
--------------------------------------------------------------------------------
GPL license.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

Author contact information:
engelen@genivia.com / engelen@acm.org
--------------------------------------------------------------------------------
A commercial use license is available from Genivia, Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

// soapH.h generated by soapcpp2 from .h file containing #import "struct_tm.h":
#include "soapH.h"

void soap_default_xsd__dateTime(struct soap *soap, struct tm *a)
{ memset(a, 0, sizeof(struct tm));
}

void soap_serialize_xsd__dateTime(struct soap *soap, struct tm const *a)
{ }

int soap_out_xsd__dateTime(struct soap *soap, const char *tag, int id, const struct tm *a, const char *type)
{ time_t t = mktime((struct tm*)a);
  return soap_outdateTime(soap, tag, id, &t, type, SOAP_TYPE_xsd__dateTime);
}

struct tm *soap_in_xsd__dateTime(struct soap *soap, const char *tag, struct tm *a, const char *type)
{ time_t t;
  if (!soap_indateTime(soap, tag, &t, type, SOAP_TYPE_xsd__dateTime))
    return NULL;
  if (!a)
  { if (!(a = (struct tm*)soap_malloc(soap, sizeof(struct tm))))
    { soap->error = SOAP_EOM;
      return NULL;
    }
  }
#ifdef HAVE_LOCALTIME_R
  return localtime_r(&t, a);
#else
  return localtime_r(&t);
#endif
}