/*

types.cpp

WSDL parser and converter to gSOAP header file format

--------------------------------------------------------------------------------
gSOAP XML Web services tools
Copyright (C) 2001-2005, Robert van Engelen, Genivia Inc. All Rights Reserved.
This software is released under one of the following two licenses:
GPL or Genivia's license for commercial use.
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

#include "types.h"

static char *getline(char *s, size_t n, FILE *fd);
static const char *nonblank(const char *s);
static const char *fill(char *t, int n, const char *s, int e);
static const char *utf8(char *t, const char *s);
static const char *cstring(const char *s);
static const char *xstring(const char *s);
static bool is_integer(const char *s);
static void documentation(const char *text);

////////////////////////////////////////////////////////////////////////////////
//
//	Keywords and reserved words
//
////////////////////////////////////////////////////////////////////////////////

static const char *keywords[] =
{ "and",
  "auto",
  "bool",
  "break",
  "case",
  "char",
  "class",
  "const",
  "continue",
  "default",
  "do",
  "double",
  "else",	
  "enum",
  "errno",
  "explicit",
  "extern",
  "false",
  "FILE",
  "float",
  "for",
  "friend",
  "goto",
  "if",
  "inline",
  "int",
  "long",
  "LONG64",
  "max",
  "min",
  "mustUnderstand",
  "namespace",
  "not",
  "operator",
  "or",
  "private",
  "protected",
  "public",
  "_QName",
  "register",
  "restrict",
  "return",
  "short",
  "signed",
  "size_t",
  "sizeof",
  "static",
  "struct",
  "switch",
  "template",
  "this",
  "time_t",
  "true",
  "typedef",
  "ULONG64",
  "union",
  "unsigned",
  "using",
  "virtual",
  "void",
  "volatile",
  "wchar_t",
  "while",
  "XML",
  "_XML",
  "xor",
};

////////////////////////////////////////////////////////////////////////////////
//
//	Types methods
//
////////////////////////////////////////////////////////////////////////////////

Types::Types()
{ init();
}

int Types::read(const char *file)
{ FILE *fd = fopen(file, "r");
  char buf[1024], xsd[1024], def[1024], use[1024], ptr[1024], uri[1024];
  const char *s;
  short copy = 0;
  if (!fd)
  { fprintf(stderr, "Cannot open file '%s'\n", file);
    return SOAP_EOF;
  }
  fprintf(stderr, "Reading type map file '%s'\n\n", file);
  while (getline(buf, sizeof(buf), fd))
  { s = buf;
    if (copy)
    { if (*s == ']')
        copy = 0;
      else
        fprintf(stream, "%s\n", buf);
    }
    else if (*s == '[')
      copy = 1;
    else if (*s && *s != '#')
    { s = fill(xsd, sizeof(xsd), s, '=');
      if (strstr(xsd, "__"))
      { s = fill(def, sizeof(def), s, '|');
        s = fill(use, sizeof(use), s, '|');
        s = fill(ptr, sizeof(ptr), s, '|');
        if (*xsd)
        { s = estrdup(xsd);
	  if (*def == '$')
	  { const char *t = modtypemap[s];
	    if (t)
	    { char *r = (char*)emalloc(strlen(t) + strlen(def) + 1);
	      strcpy(r, t);
	      strcat(r, def);
	      free((void*)modtypemap[s]);
	      modtypemap[s] = r;
	    }
	    else
	      modtypemap[s] = estrdup(def);
	  }
          else
	  { if (*def)
              deftypemap[s] = estrdup(def);
            else
              deftypemap[s] = "";
            if (*use)
              usetypemap[s] = estrdup(use);
	    else
              usetypemap[s] = estrdup(xsd);
            if (*ptr)
              ptrtypemap[s] = estrdup(ptr);
          }
        }
      }
      else if (*xsd)
      { s = fill(uri, sizeof(uri), s, 0);
        if (uri[0] == '"')
        { uri[strlen(uri) - 1] = '\0';
          nsprefix(xsd, estrdup(uri + 1));
        }
        else if (uri[0] == '<')
        { uri[strlen(uri) - 1] = '\0';
	  char *s = estrdup(uri + 1);
	  nsprefix(xsd, s);
	  exturis.insert(s);
        }
        else
          nsprefix(xsd, estrdup(uri));
      }
    }
  }
  fclose(fd);
  return SOAP_OK;
}

void Types::init()
{ snum = 1;
  unum = 1;
  gnum = 1;
  knames.insert(keywords, keywords + sizeof(keywords)/sizeof(char*));
  if (cflag)
  { deftypemap["xsd__ur_type"] = "";
    usetypemap["xsd__ur_type"] = "_XML";
    ptrtypemap["xsd__ur_type"] = "_XML";
  }
  else
  { deftypemap["xsd__ur_type"] = "class xsd__ur_type { _XML __item; struct soap *soap; };";
    usetypemap["xsd__ur_type"] = "xsd__ur_type";
  }
  if (cflag)
  { deftypemap["xsd__anyType"] = "";
    usetypemap["xsd__anyType"] = "_XML";
    ptrtypemap["xsd__anyType"] = "_XML";
  }
  else
  { deftypemap["xsd__anyType"] = "class xsd__anyType { _XML __item; struct soap *soap; };";
    usetypemap["xsd__anyType"] = "xsd__anyType*";
  }
  if (cflag)
  { deftypemap["xsd__base64Binary"] = "struct xsd__base64Binary\n{\tunsigned char *__ptr;\n\tint __size;\n\tchar *id, *type, *option; /* NOTE: for DIME and MTOM XOP attachments only */\n};";
    usetypemap["xsd__base64Binary"] = "struct xsd__base64Binary";
  }
  else
  { deftypemap["xsd__base64Binary"] = "class xsd__base64Binary\n{\tunsigned char *__ptr;\n\tint __size;\n\tchar *id, *type, *option; /* NOTE: for DIME and MTOM XOP attachments only */\n\tstruct soap *soap;\n};";
    usetypemap["xsd__base64Binary"] = "xsd__base64Binary";
  }
  if (cflag)
  { if (eflag)
      deftypemap["xsd__boolean"] = "enum xsd__boolean { false_, true_ };";
    else
      deftypemap["xsd__boolean"] = "enum xsd__boolean { xsd__boolean__false_, xsd__boolean__true_ };";
    usetypemap["xsd__boolean"] = "enum xsd__boolean";
  }
  else
  { deftypemap["xsd__boolean"] = "";
    usetypemap["xsd__boolean"] = "bool";
  }
  deftypemap["xsd__byte"] = "";
  usetypemap["xsd__byte"] = "char";
  ptrtypemap["xsd__byte"] = "short*"; // avoid char*
  deftypemap["xsd__dateTime"] = "";
  usetypemap["xsd__dateTime"] = "time_t";
  deftypemap["xsd__double"] = "";
  usetypemap["xsd__double"] = "double";
  deftypemap["xsd__float"] = "";
  usetypemap["xsd__float"] = "float";
  if (cflag)
  { deftypemap["xsd__hexBinary"] = "struct xsd__hexBinary { unsigned char *__ptr; int __size; };";
    usetypemap["xsd__hexBinary"] = "struct xsd__hexBinary";
  }
  else
  { deftypemap["xsd__hexBinary"] = "class xsd__hexBinary { unsigned char *__ptr; int __size; };";
    usetypemap["xsd__hexBinary"] = "xsd__hexBinary";
  }
  deftypemap["xsd__int"] = "";
  usetypemap["xsd__int"] = "int";
  deftypemap["xsd__long"] = "";
  usetypemap["xsd__long"] = "LONG64";
  deftypemap["xsd__short"] = "";
  usetypemap["xsd__short"] = "short";
  if (cflag || sflag)
  { deftypemap["xsd__string"] = "";
    usetypemap["xsd__string"] = "char*";
  }
  else
  { deftypemap["xsd__string"] = "";
    usetypemap["xsd__string"] = "std::string";
  }
  deftypemap["xsd__unsignedByte"] = "";
  usetypemap["xsd__unsignedByte"] = "unsigned char";
  ptrtypemap["xsd__unsignedByte"] = "unsigned short*"; // avoid unsigned char*
  deftypemap["xsd__unsignedInt"] = "";
  usetypemap["xsd__unsignedInt"] = "unsigned int";
  deftypemap["xsd__unsignedLong"] = "";
  usetypemap["xsd__unsignedLong"] = "ULONG64";
  deftypemap["xsd__unsignedShort"] = "";
  usetypemap["xsd__unsignedShort"] = "unsigned short";
  if (cflag)
  { deftypemap["SOAP_ENC__base64Binary"] = "struct SOAP_ENC__base64Binary { unsigned char *__ptr; int __size; };";
    usetypemap["SOAP_ENC__base64Binary"] = "struct SOAP_ENC__base64Binary";
  }
  else
  { deftypemap["SOAP_ENC__base64Binary"] = "class SOAP_ENC__base64Binary { unsigned char *__ptr; int __size; };";
    usetypemap["SOAP_ENC__base64Binary"] = "SOAP_ENC__base64Binary";
  }
  if (cflag)
  { deftypemap["SOAP_ENC__boolean"] = "enum SOAP_ENC__boolean { false_, true_ };";
    usetypemap["SOAP_ENC__boolean"] = "enum SOAP_ENC__boolean";
  }
  else
  { deftypemap["SOAP_ENC__boolean"] = "";
    usetypemap["SOAP_ENC__boolean"] = "bool";
  }
  deftypemap["SOAP_ENC__byte"] = "";
  usetypemap["SOAP_ENC__byte"] = "char";
  deftypemap["SOAP_ENC__dateTime"] = "";
  usetypemap["SOAP_ENC__dateTime"] = "time_t";
  deftypemap["SOAP_ENC__double"] = "";
  usetypemap["SOAP_ENC__double"] = "double";
  deftypemap["SOAP_ENC__float"] = "";
  usetypemap["SOAP_ENC__float"] = "float";
  if (cflag)
  { deftypemap["SOAP_ENC__hexBinary"] = "struct SOAP_ENC__hexBinary { unsigned char *__ptr; int __size; };";
    usetypemap["SOAP_ENC__hexBinary"] = "struct SOAP_ENC__hexBinary";
  }
  else
  { deftypemap["SOAP_ENC__hexBinary"] = "class SOAP_ENC__hexBinary { unsigned char *__ptr; int __size; };";
    usetypemap["SOAP_ENC__hexBinary"] = "SOAP_ENC__hexBinary";
  }
  deftypemap["SOAP_ENC__int"] = "";
  usetypemap["SOAP_ENC__int"] = "int";
  deftypemap["SOAP_ENC__long"] = "";
  usetypemap["SOAP_ENC__long"] = "LONG64";
  deftypemap["SOAP_ENC__short"] = "";
  usetypemap["SOAP_ENC__short"] = "short";
  ptrtypemap["SOAP_ENC__short"] = "int*";
  if (cflag || sflag)
  { deftypemap["SOAP_ENC__string"] = "";
    usetypemap["SOAP_ENC__string"] = "char*";
  }
  else
  { deftypemap["SOAP_ENC__string"] = "";
    usetypemap["SOAP_ENC__string"] = "std::string";
  }
  deftypemap["SOAP_ENC__unsignedByte"] = "";
  usetypemap["SOAP_ENC__unsignedByte"] = "unsigned char";
  ptrtypemap["SOAP_ENC__unsignedByte"] = "unsigned short*";
  deftypemap["SOAP_ENC__unsignedInt"] = "";
  usetypemap["SOAP_ENC__unsignedInt"] = "unsigned long";
  deftypemap["SOAP_ENC__unsignedLong"] = "";
  usetypemap["SOAP_ENC__unsignedLong"] = "ULONG64";
  deftypemap["SOAP_ENC__unsignedShort"] = "";
  usetypemap["SOAP_ENC__unsignedShort"] = "unsigned short";
  deftypemap["SOAP_ENC__Array"] = "";
  if (read(mapfile))
    fprintf(stderr, "Problem reading type map file %s.\nUsing internal type definitions for %s instead.\n\n", mapfile, cflag?"C":"C++");
}

const char *Types::nsprefix(const char *prefix, const char *URI)
{ if (URI)
  { const char *s = uris[URI];
    if (!s)
    { size_t n;
      if (!prefix || !*prefix || *prefix == '_')
        s = schema_prefix;
      else
        s = estrdup(prefix);
      if (!syms[s])
        n = syms[s] = 1;
      else
        n = ++syms[s];
      if (n != 1 || !prefix || !*prefix || *prefix == '_')
      { char *t = (char*)emalloc(strlen(s) + 16);
        sprintf(t, "%s%lu", s, (unsigned long)n);
	s = t;
      }
      uris[URI] = s;
      if (vflag)
        fprintf(stderr, "namespace prefix %s = \"%s\"\n", s, URI);
    }
    return s;
  }
  return NULL;
}

// Find a C name for a QName. If the name has no qualifier, use URI. Suggest prefix for URI
const char *Types::fname(const char *prefix, const char *URI, const char *qname, SetOfString *reserved, enum Lookup lookup)
{ char buf[1024], *t;
  const char *p, *s, *name;
  if (!qname)
  { fprintf(stream, "// Warning: internal error, undefined qname in fname()\n");
    qname = "?";
  }
  s = strrchr(qname, ':');
  if (s)
  { name = s + 1;
    if (*qname == '"')
    { t = (char*)emalloc(s - qname - 1);
      strncpy(t, qname + 1, s - qname - 2);
      t[s - qname - 2] = '\0';
      URI = t;
    }
    else if (!strncmp(qname, "xs:", 3))	// this hack is necessary since the nsmap table defines "xs" for "xsd"
    { s = "xsd";
      URI = NULL;
    }
    else
    { t = (char*)emalloc(s - qname + 1);
      strncpy(t, qname, s - qname);
      t[s - qname] = '\0';
      s = t;
      URI = NULL;
    }
  }
  else
    name = qname;
  if (URI)
    p = nsprefix(prefix, URI);
  else if (s)
    p = s;
  else
    p = "";
  if (lookup == LOOKUP)
  { s = qnames[Pair(p,name)];
    if (s)
      return s;
  }
  t = buf;
  if (!prefix || *prefix)
  { s = p;
    if (prefix && *prefix == '_') // ensures ns prefix starts with _...
    { strcpy(t, prefix);
      t += strlen(prefix);
    }
    if (s && *s)
    { for (; *s; s++)
      { if (isalnum(*s))
          *t++ = *s;
        else if (*s == '-' && s != p)
          *t++ = '_';
        else if (*s == '_')
        { strcpy(t, "_USCORE");
          t += 7;
        }
        else
	{ s = utf8(t, s);
          t += 6;
        }
      }
      if (!prefix || *prefix != '*')
      { *t++ = '_';
        *t++ = '_';
      }
    }
    else if (isdigit(*name))
      *t++ = '_';
  }
  for (s = name; *s; s++)
  { if (isalnum(*s))
      *t++ = *s;
    else if (*s == '-' && s != name)
      *t++ = '_';
    else if (*s == '_')
    { strcpy(t, "_USCORE");
      t += 7;
    }
    else
    { s = utf8(t, s);
      t += 6;
    }
  }
  *t = '\0';
  while (knames.find(buf) != knames.end() || (reserved && reserved->find(buf) != reserved->end()))
  { *t++ = '_';
    *t = '\0';
  }
  if (isalpha(*buf) || *buf == '_')
  { t = (char*)emalloc(strlen(buf) + 1);
    strcpy(t, buf);
  }
  else
  { t = (char*)emalloc(strlen(buf) + 2);
    *t = '_';
    strcpy(t + 1, buf);
  }
  if (lookup == LOOKUP)
    qnames[Pair(p,name)] = t;
  /*
  cerr << "[DEFINED " << p << ":" << name << "=" << t << "]" << endl;
  for (MapOfPairToString::const_iterator i = qnames.begin(); i != qnames.end(); ++i)
    cerr << "(" << (*i).first.first << "," << (*i).first.second << ") = " << (*i).second << endl;
  */
  return t;
}

bool Types::is_defined(const char *prefix, const char *URI, const char *qname)
{ const char *t = fname(prefix, URI, qname, NULL, LOOKUP);
  return usetypemap[t] != NULL;
}

const char *Types::aname(const char *prefix, const char *URI, const char *qname)
{ return fname(prefix, URI, qname, NULL, NOLOOKUP);
}

const char *Types::cname(const char *prefix, const char *URI, const char *qname)
{ return fname(prefix, URI, qname, NULL, LOOKUP);
}

const char *Types::tname(const char *prefix, const char *URI, const char *qname)
{ const char *s, *t = cname(prefix, URI, qname);
  s = usetypemap[t];
  if (!s)
  { s = t;
    fprintf(stream, "// Warning: internal error, undefined qname '%s' for type '%s'\n", qname?qname:"", t);
  }
  return s;
}

const char *Types::pname(bool flag, const char *prefix, const char *URI, const char *qname)
{ const char *r, *s, *t = cname(prefix, URI, qname);
  if (flag)
  { s = ptrtypemap[t];
    if (!s)
    { s = usetypemap[t];
      if (!s || !*s)
      { s = t;
        fprintf(stream, "// Warning: internal error, undefined: %s %s\n", qname, t);
      }
      r = s;
      do
      { r = strchr(r + 1, '*');
        if (r && *(r-1) != '/' && *(r+1) != '/')
	  break;
      } while (r);
      if (!r)	// already pointer?
      { char *p = (char*)emalloc(strlen(s) + 2);
        strcpy(p, s);
        strcat(p, "*");
        s = p;
      }
      ptrtypemap[t] = s;
    }
  }
  else
    s = usetypemap[t];
  if (!s)
  { s = t;
    fprintf(stream, "// Warning: internal error, undefined: %s %s\n", qname, t);
  }
  return s;
}

const char *Types::deftname(enum Type type, const char *pointer, bool is_pointer, const char *prefix, const char *URI, const char *qname)
{ char buf[1024];
  const char *q = NULL, *t = fname(prefix, URI, qname, NULL, LOOKUP);
  char *s;
  if (deftypemap[t])
    return NULL;
  switch (type)
  { case ENUM:
      q = "enum";
      if (yflag)
        knames.insert(t);
      break;
    case STRUCT:
      q = "struct";
      if (yflag)
        knames.insert(t);
      break;
    case CLASS:
    case TYPEDEF:
      knames.insert(t);
    default:
      break;
  }
  if (q)
  { strcpy(buf, q);
    strcat(buf, " ");
  }
  else
    buf[0] = '\0';
  strcat(buf, t);
  if (pointer)
    strcat(buf, pointer);
  s = (char*)emalloc(strlen(buf) + 1);
  strcpy(s, buf);
  usetypemap[t] = s;
  if (pointer || is_pointer)
    ptrtypemap[t] = s;
  return t;
}

// get enumeration value. URI/type refers to the enum simpleType.
const char *Types::ename(const char *type, const char *value)
{ const char *s = enames[Pair(type,value)];
  if (!s)
  { s = fname(NULL, NULL, value, &rnames, NOLOOKUP);
    if (!eflag && *type)
    { // Add prefix to enum
      char *buf = (char*)emalloc(strlen(type) + strlen(s) + 3);
      if (s[0] == '_' && s[1] != 'x')		// _xXXXX is OK here
        sprintf(buf, "%s_%s", type, s);
      else
        sprintf(buf, "%s__%s", type, s);
      s = buf;
    }
    else
      rnames.insert(s);
    enames[Pair(type,value)] = s;
  }
  return s;
}

// get operation name
const char *Types::oname(const char *prefix, const char *URI, const char *qname)
{ const char *s = fname(prefix, URI, qname, &onames, NOLOOKUP);
  onames.insert(s);
  return s;
}

// generate struct name
const char *Types::sname(const char *URI)
{ char *t;
  if (URI)
  { const char *s = nsprefix(NULL, URI);
    t = (char*)emalloc(strlen(s) + 16);
    sprintf(t, "%s__struct_%d", s, snum++);
  }
  else
  { t = (char*)emalloc(16);
    sprintf(t, "struct_%d", snum++);
  }
  return t;
}

// generate union name
const char *Types::uname(const char *URI)
{ char *t;
  if (URI)
  { const char *s = nsprefix(NULL, URI);
    t = (char*)emalloc(strlen(s) + 16);
    sprintf(t, "%s__union_%d", s, unum++);
  }
  else
  { t = (char*)emalloc(16);
    sprintf(t, "union_%d", unum++);
  }
  return t;
}

// generate enum name
const char *Types::gname(const char *URI)
{ char *t;
  if (URI)
  { const char *s = nsprefix(NULL, URI);
    t = (char*)emalloc(strlen(s) + 16);
    sprintf(t, "%s__enum_%d", s, gnum++);
  }
  else
  { t = (char*)emalloc(16);
    sprintf(t, "enum_%d", gnum++);
  }
  return t;
}

// check if nillable or minOccurs=0 etc.
bool Types::is_nillable(const xs__element& element)
{ return ((element.nillable || (!element.default_ && element.minOccurs && !strcmp(element.minOccurs, "0"))));
}

bool Types::is_basetype(const char *type)
{ if (!strcmp(type, "xs:anyType")
   || !strcmp(tname(NULL, NULL, type), "std::string"))
    return false;
  return !strncmp(type, "xs:", 3) || !strncmp(type, "SOAP-ENC:", 9);
}

void Types::define(const char *URI, const char *name, const xs__complexType& complexType)
{ // generate prototype for structs/classes and store name
  const char *prefix = NULL;
  if (complexType.name)
    name = complexType.name;
  else
    prefix = "_";
  if (complexType.complexContent && complexType.complexContent->restriction && !strcmp(complexType.complexContent->restriction->base, "SOAP-ENC:Array"))
  { if (strcmp(schema_prefix, "ns"))
      prefix = "*";
    else
      prefix = "";
  }
  if (cflag)
  { const char *t = deftname(STRUCT, "*", true, prefix, URI, name);
    if (t)
    { if (yflag)
        fprintf(stream, "\n/// Typedef synonym for struct %s.\ntypedef struct %s %s;\n", t, t, t);
    }
    else if (name)
    { t = deftypemap[cname(prefix, URI, name)];
      if (t)
      { fprintf(stream, "\n/// Imported complexType \"%s\":%s from typemap %s.\n", URI, name, mapfile?mapfile:"");
        document(complexType.annotation);
        if (*t)
	  format(t);
        else
	  fprintf(stream, "// complexType definition intentionally left blank.\n");
      }
    }
  }
  else 
  { const char *t = deftname(CLASS, "*", true, prefix, URI, name);
    if (t)
      fprintf(stream, "\n//  Forward declaration of class %s.\nclass %s;\n", t, t);
    else if (name)
    { t = deftypemap[cname(prefix, URI, name)];
      if (t)
      { fprintf(stream, "\n/// Imported complexType \"%s\":%s from typemap %s.\n", URI, name, mapfile?mapfile:"");
        document(complexType.annotation);
        if (*t)
	  format(t);
        else
	  fprintf(stream, "// complexType definition intentionally left blank.\n");
      }
    }
  }
}

void Types::gen(const char *URI, const char *name, const xs__simpleType& simpleType)
{ const char *t = NULL;
  const char *prefix = NULL;
  if (simpleType.name)
  { name = simpleType.name;
  }
  else
    prefix = "_";
  if (name)
  { t = deftypemap[cname(NULL, URI, name)];
    if (t)
    { fprintf(stream, "\n/// Imported simpleType \"%s\":%s from typemap %s.\n", URI, name, mapfile?mapfile:"");
      document(simpleType.annotation);
      if (*t)
	format(t);
      else
	fprintf(stream, "// simpleType definition intentionally left blank.\n");
      return;
    }
  }
  if (simpleType.restriction && simpleType.restriction->base)
  { if (name)
      fprintf(stream, "\n/// \"%s\":%s is a simpleType restriction of %s.\n", URI?URI:"", name, simpleType.restriction->base);
    document(simpleType.annotation);
    if (!simpleType.restriction->enumeration.empty())
    { bool is_numeric = true; // check if all enumeration values are numeric
      bool is_qname = !strcmp(simpleType.restriction->base, "xs:QName");
      if (name)
      { t = deftname(ENUM, NULL, false, prefix, URI, name);
        if (!eflag)
          fprintf(stream, "/// Note: enum values are prefixed with '%s' to avoid name clashes, please use wsdl2h option -e to omit this prefix\n", t);
      }
      else
        t = gname(URI);
      if (name)
        fprintf(stream, "enum %s\n{\n", t);
      else
        fprintf(stream, "    enum %s\n    {\n", t);
      for (vector<xs__enumeration>::const_iterator enumeration1 = simpleType.restriction->enumeration.begin(); enumeration1 != simpleType.restriction->enumeration.end(); ++enumeration1)
      { const char *s;
        if ((s = (*enumeration1).value))
	  is_numeric &= is_integer(s);
      }
      SetOfString enumvals;
      for (vector<xs__enumeration>::const_iterator enumeration2 = simpleType.restriction->enumeration.begin(); enumeration2 != simpleType.restriction->enumeration.end(); ++enumeration2)
      { const char *s;
        document((*enumeration2).annotation);
        if ((s = (*enumeration2).value))
	{ if (!enumvals.count(s))
	  { enumvals.insert(s);
	    if (is_numeric)
              fprintf(stream, "\t%s = %s,\t///< %s value=\"%s\"\n", ename(t, s), s, simpleType.restriction->base, s);
            else if (is_qname && (*enumeration2).value_)
              fprintf(stream, "\t%s,\t///< %s value=\"%s\"\n", ename(t, (*enumeration2).value_), simpleType.restriction->base, (*enumeration2).value_);
            else
              fprintf(stream, "\t%s,\t///< %s value=\"%s\"\n", ename(t, s), simpleType.restriction->base, s);
	  }
	}
        else
          fprintf(stream, "//\tunrecognized: enumeration '%s' has no value\n", name?name:"");
      }
      if (name)
      { fprintf(stream, "};\n");
        if (yflag)
	  fprintf(stream, "/// Typedef synonym for enum %s.\ntypedef enum %s %s;\n", t, t, t);
        if (pflag)
        { const char *s = aname(prefix, URI, name);
	  knames.insert(s);
          s = aname(prefix, URI, name);
          fprintf(stream, "\n/// Class wrapper\n");
          fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", s);
          fprintf(stream, elementformat, tname(prefix, URI, name), "__item;");
	  modify(s);
          fprintf(stream, "\n};\n");
        }
      }
      else
        fprintf(stream, "    }\n");
    }
    else
    { if (simpleType.restriction->length && simpleType.restriction->length->value)
        fprintf(stream, "/// Length of this string is exactly %s characters\n", simpleType.restriction->length->value);
      else
      { const char *a = NULL, *b = NULL;
        if (simpleType.restriction->minLength)
          a = simpleType.restriction->minLength->value;
        if (simpleType.restriction->maxLength)
          b = simpleType.restriction->maxLength->value;
        if (a || b)
          fprintf(stream, "/// Length of this string is within %s..%s characters\n", a?a:"0", b?b:"");
      }
      if (simpleType.restriction->precision && simpleType.restriction->precision->value)
        fprintf(stream, "/// %sprecision is %s\n", simpleType.restriction->precision->fixed?"fixed ":"", simpleType.restriction->precision->value);
      if (simpleType.restriction->scale && simpleType.restriction->scale->value)
        fprintf(stream, "/// %sscale is %s\n", simpleType.restriction->scale->fixed?"fixed ":"", simpleType.restriction->scale->value);
      if (simpleType.restriction->totalDigits && simpleType.restriction->totalDigits->value)
        fprintf(stream, "/// %snumber of total digits is %s\n", simpleType.restriction->totalDigits->fixed?"fixed ":"", simpleType.restriction->totalDigits->value);
      if (simpleType.restriction->fractionDigits && simpleType.restriction->fractionDigits->value)
        fprintf(stream, "/// %snumber of fraction digits is %s\n", simpleType.restriction->fractionDigits->fixed?"fixed ":"", simpleType.restriction->fractionDigits->value);
      for (vector<xs__pattern>::const_iterator pattern1 = simpleType.restriction->pattern.begin(); pattern1 != simpleType.restriction->pattern.end(); ++pattern1)
        fprintf(stream, "/// Content pattern is \"%s\"\n", xstring((*pattern1).value));
      const char *ai = NULL, *ae = NULL, *bi = NULL, *be = NULL;
      if (simpleType.restriction->minInclusive)
        ai = simpleType.restriction->minInclusive->value;
      else if (simpleType.restriction->minExclusive)
        ae = simpleType.restriction->minExclusive->value;
      if (simpleType.restriction->maxInclusive)
        bi = simpleType.restriction->maxInclusive->value;
      else if (simpleType.restriction->maxExclusive)
        be = simpleType.restriction->maxExclusive->value;
      if (ai || ae || bi || be)
      { fprintf(stream, "/// Value range is ");
        if (ai)
	  fprintf(stream, "[%s..", ai);
        else if (ae)
	  fprintf(stream, "(%s..", ae);
        else
	  fprintf(stream, "[-INF..");
        if (bi)
	  fprintf(stream, "%s]\n", bi);
        else if (be)
	  fprintf(stream, "%s)\n", be);
        else
	  fprintf(stream, "INF]\n");
      }
      if (!simpleType.restriction->attribute.empty())
      { fprintf(stderr, "\nWarning: simpleType '%s' should not have attributes\n", name?name:"");
      }
      const char *s = tname(NULL, NULL, simpleType.restriction->base);
      if (name)
      { bool is_ptr = false;
	is_ptr = (strchr(s, '*') != NULL) || (s == pname(true, NULL, NULL, simpleType.restriction->base));
        t = deftname(TYPEDEF, NULL, is_ptr, prefix, URI, name);
        if (t)
	  fprintf(stream, "typedef %s %s", s, t);
      }
      else
      { t = "";
        fprintf(stream, elementformat, s, "");
        fprintf(stream, "\n");
      }
      if (t)
      { if (name && !simpleType.restriction->pattern.empty())
        { fprintf(stream, " \"");
          for (vector<xs__pattern>::const_iterator pattern2 = simpleType.restriction->pattern.begin(); pattern2 != simpleType.restriction->pattern.end(); ++pattern2)
          { if (pattern2 != simpleType.restriction->pattern.begin())
              fprintf(stream, "|");
            fprintf(stream, "%s", xstring((*pattern2).value));
          }
          fprintf(stream, "\"");
        }
	// add range info only when type is numeric
	bool is_numeric = false;
	if (!strncmp(s, "unsigned ", 9))
	  s += 9;
	if (strstr("char short int LONG64 float double ", s))
	  is_numeric = true;
        if (name && simpleType.restriction->minLength && simpleType.restriction->minLength->value)
          fprintf(stream, " %s", simpleType.restriction->minLength->value);
        else if (is_numeric && name && simpleType.restriction->minInclusive && simpleType.restriction->minInclusive->value && is_integer(simpleType.restriction->minInclusive->value))
          fprintf(stream, " %s", simpleType.restriction->minInclusive->value);
        if (name && simpleType.restriction->maxLength && simpleType.restriction->maxLength->value)
          fprintf(stream, ":%s", simpleType.restriction->maxLength->value);
        else if (is_numeric && name && simpleType.restriction->maxInclusive && simpleType.restriction->maxInclusive->value && is_integer(simpleType.restriction->maxInclusive->value))
          fprintf(stream, ":%s", simpleType.restriction->maxInclusive->value);
        if (name)
        { fprintf(stream, ";\n");
          if (pflag)
          { const char *s = aname(prefix, URI, name);
	    knames.insert(s);
            s = aname(prefix, URI, name);
            fprintf(stream, "\n/// Class wrapper\n");
            fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", s);
            fprintf(stream, elementformat, tname(prefix, URI, name), "__item;");
	    modify(s);
            fprintf(stream, "\n};\n");
          }
        }
      }
    }
  }
  else if (simpleType.list)
  { if (simpleType.list->restriction && simpleType.list->restriction->base)
    { if (name)
        fprintf(stream, "\n/// \"%s\":%s is a simpleType list restriction of %s.\n", URI?URI:"", name, simpleType.list->restriction->base);
      document(simpleType.annotation);
      if (name)
      { t = deftname(ENUM, NULL, false, prefix, URI, name);
	if (t)
	  fprintf(stream, "enum * %s\n{\n", t);
      }
      else
      { t = "";
	fprintf(stream, "enum *\n{\n");
      }
      if (t)
      { for (vector<xs__enumeration>::const_iterator enumeration = simpleType.list->restriction->enumeration.begin(); enumeration != simpleType.list->restriction->enumeration.end(); ++enumeration)
        { if ((*enumeration).value)
          { if (!strcmp(simpleType.list->restriction->base, "xs:QName") && (*enumeration).value_)
              fprintf(stream, "\t%s,\t///< %s value=\"%s\"\n", ename(t, (*enumeration).value_), simpleType.list->restriction->base, (*enumeration).value_);
	    else
              fprintf(stream, "\t%s,\t///< %s value=\"%s\"\n", ename(t, (*enumeration).value), simpleType.list->restriction->base, (*enumeration).value);
	  }
          else
            fprintf(stream, "//\tunrecognized: bitmask enumeration '%s' has no value\n", t);
        }
        if (name)
        { fprintf(stream, "};\n");
          if (yflag)
	    fprintf(stream, "/// Typedef synonym for enum %s.\ntypedef enum %s %s;\n", t, t, t);
          if (pflag)
          { const char *s = aname(prefix, URI, name);
	    knames.insert(s);
            s = aname(prefix, URI, name);
            fprintf(stream, "\n/// Class wrapper\n");
            fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", s);
            fprintf(stream, elementformat, tname(prefix, URI, name), "__item;");
	    modify(s);
            fprintf(stream, "\n};\n");
          }
        }
        else
          fprintf(stream, "}\n");
      }
    }
    else if (simpleType.list->itemType)
    { const xs__simpleType *p = simpleType.list->itemTypePtr();
      if (p && p->restriction && p->restriction->base && !p->restriction->enumeration.empty() && p->restriction->enumeration.size() <= 64)
      { if (name)
          fprintf(stream, "\n/// \"%s\":%s is a simpleType list of %s.\n", URI?URI:"", name, simpleType.list->itemType);
        document(simpleType.annotation);
        if (name)
        { t = deftname(ENUM, NULL, false, prefix, URI, name);
	  if (t)
	    fprintf(stream, "enum * %s\n{\n", t);
        }
        else
        { t = "";
	  fprintf(stream, "enum *\n{\n");
        }
        if (t)
	{ for (vector<xs__enumeration>::const_iterator enumeration = p->restriction->enumeration.begin(); enumeration != p->restriction->enumeration.end(); ++enumeration)
          { if ((*enumeration).value)
              fprintf(stream, "\t%s,\t///< %s value=\"%s\"\n", ename(t, (*enumeration).value), p->restriction->base, (*enumeration).value);
            else
              fprintf(stream, "//\tunrecognized: bitmask enumeration '%s' has no value\n", t);
          }
          if (name)
          { fprintf(stream, "};\n");
            if (yflag)
	      fprintf(stream, "/// Typedef synonym for enum %s.\ntypedef enum %s %s;\n", t, t, t);
            if (pflag)
            { const char *s = aname(prefix, URI, name);
	      knames.insert(s);
              s = aname(prefix, URI, name);
              fprintf(stream, "\n/// Class wrapper.\n");
              fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", s);
              fprintf(stream, elementformat, tname(prefix, URI, name), "__item;");
	      modify(s);
              fprintf(stream, "\n};\n");
            }
          }
          else
            fprintf(stream, "}\n");
        }
      }
      else
      { const char *s = tname(NULL, NULL, "xsd:string");
        if (name)
        { fprintf(stream, "\n/// \"%s\":%s is a simpleType containing a whitespace separated list of %s.\n", URI?URI:"", name, simpleType.list->itemType);
          t = deftname(TYPEDEF, NULL, strchr(s, '*') != NULL, prefix, URI, name);
	}
        document(simpleType.annotation);
        if (t)
          fprintf(stream, "typedef %s %s;\n", s, t);
        else
        { fprintf(stream, elementformat, s, "");
          fprintf(stream, "\n");
        }
      }
    }
    else
    { if (name)
        fprintf(stream, "\n/// \"%s\":%s is a simpleType list.\n", URI?URI:"", name);
      document(simpleType.annotation);
      if (name)
      { t = deftname(ENUM, NULL, false, prefix, URI, name);
        if (!eflag)
          fprintf(stream, "/// Note: enum values are prefixed with '%s' to avoid name clashes, please use wsdl2h option -e to omit this prefix\n", t);
      }
      else
        t = "";
      if (t)
      { fprintf(stream, "enum * %s\n{\n", t);
        for (vector<xs__simpleType>::const_iterator simple = simpleType.list->simpleType.begin(); simple != simpleType.list->simpleType.end(); ++simple)
        { if ((*simple).restriction && (*simple).restriction->base)
          { for (vector<xs__enumeration>::const_iterator enumeration = (*simple).restriction->enumeration.begin(); enumeration != (*simple).restriction->enumeration.end(); ++enumeration)
            { if ((*enumeration).value)
                fprintf(stream, "\t%s,\t///< %s value=\"%s\"\n", ename(t, (*enumeration).value), (*simple).restriction->base, (*enumeration).value);
              else
                fprintf(stream, "//\tunrecognized: bitmask enumeration '%s' has no value\n", t);
            }
          }
        }
        if (name)
        { fprintf(stream, "};\n");
          if (yflag)
	    fprintf(stream, "/// Typedef synonym for enum %s.\ntypedef enum %s %s;\n", t, t, t);
          if (pflag)
          { const char *s = aname(prefix, URI, name);
	    knames.insert(s);
            s = aname(prefix, URI, name);
            fprintf(stream, "\n/// Class wrapper.\n");
            fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", s);
            fprintf(stream, elementformat, tname(prefix, URI, name), "__item;");
	    modify(s);
            fprintf(stream, "\n};\n");
          }
        }
        else
          fprintf(stream, "}\n");
      }
    }
  }
  else if (simpleType.union_)
  { if (simpleType.union_->memberTypes)
    { const char *s = tname(NULL, NULL, "xsd:string");
      if (name)
        t = deftname(TYPEDEF, NULL, strchr(s, '*') != NULL, prefix, URI, name);
      fprintf(stream, "\n/// union of values \"%s\"\n", simpleType.union_->memberTypes);
      if (t)
        fprintf(stream, "typedef %s %s;\n", s, t);
      else
      { fprintf(stream, elementformat, s, "");
        fprintf(stream, "\n");
      }
    }
    else if (!simpleType.union_->simpleType.empty())
    { const char *s = tname(NULL, NULL, "xsd:string");
      fprintf(stream, "\n");
      if (name)
        t = deftname(TYPEDEF, NULL, strchr(s, '*') != NULL, prefix, URI, name);
      for (vector<xs__simpleType>::const_iterator simpleType1 = simpleType.union_->simpleType.begin(); simpleType1 != simpleType.union_->simpleType.end(); ++simpleType1)
        if ((*simpleType1).restriction)
	{ fprintf(stream, "/// union of values from \"%s\"\n", (*simpleType1).restriction->base);
          // TODO: are there any other types we should report here?
        }
      if (t)
        fprintf(stream, "typedef %s %s;\n", s, t);
      else
      { fprintf(stream, elementformat, s, "");
        fprintf(stream, "\n");
      }
    }
    else
      fprintf(stream, "//\tunrecognized\n");
  }
  else
    fprintf(stream, "//\tunrecognized simpleType\n");
}

static void gen_soap_array(Types *types, const char *name, const char *t, const char *item, char *type)
{ char *dims = NULL, size[8];
  *size = '\0';
  if (type)
    dims = strrchr(type, '[');
  if (dims)
    *dims++ = '\0';
  fprintf(stream, "/// SOAP encoded array of %s\n", type ? type : "xs:anyType");
  if (cflag)
    fprintf(stream, "struct %s\n{\n", t);
  else if (pflag)
    fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", t);
  else
    fprintf(stream, "class %s\n{ public:\n", t);
  if (dims)
  { char *s = strchr(dims, ']');
    if (s && s != dims)
      sprintf(size, "[%d]", (int)(s - dims + 1));
  }
  if (type)
  { if (strchr(type, '[') != NULL)
    { gen_soap_array(types, NULL, "", item, type);
      fprintf(stream, arrayformat, "}", item ? types->aname(NULL, NULL, item) : "");
      fprintf(stream, ";\n");
    }
    else
    { const char *s = types->pname(!types->is_basetype(type), NULL, NULL, type);
      fprintf(stream, "/// Pointer to array of %s.\n", s);
      fprintf(stream, arrayformat, s, item ? types->aname(NULL, NULL, item) : "");
      fprintf(stream, ";\n");
    }
    if (*size)
      fprintf(stream, "/// Size of the multidimensional dynamic array with dimensions=%s\n", size);
    else 
      fprintf(stream, "/// Size of the dynamic array.\n");
    fprintf(stream, sizeformat, "int", size);
    fprintf(stream, ";\n/// Offset for partially transmitted arrays (uncomment only when required).\n");
    fprintf(stream, offsetformat, "int", size);
    fprintf(stream, ";\n");
  }
  else
  { // TODO: handle generic SOAP array, e.g. as an array of anyType
    fprintf(stream, "// TODO: handle generic SOAP-ENC:Array (array of anyType)\n");
  }
}

void Types::gen(const char *URI, const char *name, const xs__complexType& complexType)
{ const char *t = NULL;
  bool soapflag = false;
  if (complexType.name)
  { name = complexType.name;
  }
  if (name)
  { t = cname(NULL, URI, name);
    if (deftypemap[t])
      return;
  }
  else
    t = sname(URI);
  if (complexType.simpleContent)
  { if (name)
      fprintf(stream, "\n/// \"%s\":%s is a%s complexType with simpleContent.\n", URI?URI:"", name, complexType.abstract?"n abstract":"");
    document(complexType.annotation);
    if (complexType.simpleContent->restriction)
    { if (!name)
        fprintf(stream, "    struct %s\n    {\n", t);
      else if (cflag)
        fprintf(stream, "struct %s\n{\n", t);
      else if (pflag)
        fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", t);
      else
        fprintf(stream, "class %s\n{ public:\n", t);
      const char *base = "xs:string";
      const xs__complexType *complextype = &complexType; 
      do
      { if (!complextype->simpleContent)
          break;
        if (complextype->simpleContent->restriction)
        { if (complextype->simpleContent->restriction->complexTypePtr())
	    complextype = complextype->simpleContent->restriction->complexTypePtr();
	  else
          { base = complextype->simpleContent->restriction->base;   
	    break;
	  }
	}
        else if (complextype->simpleContent->extension)
        { if (complextype->simpleContent->extension->complexTypePtr())
	    complextype = complextype->simpleContent->extension->complexTypePtr();
          else
          { base = complextype->simpleContent->extension->base;   
	    break;
          }
	}
	else
	  break;
      }
      while (complextype);
      fprintf(stream, "/// __item wraps '%s' simpleContent.\n", base);
      fprintf(stream, elementformat, tname(NULL, NULL, base), "__item");
      fprintf(stream, ";\n");
      gen(NULL, complexType.simpleContent->restriction->attribute);
      if (complexType.simpleContent->restriction->anyAttribute)
        gen(NULL, *complexType.simpleContent->restriction->anyAttribute);
    }
    else if (complexType.simpleContent->extension)
    { if (cflag || fflag || !name)
      { if (!name)
          fprintf(stream, "    struct %s\n    {\n", t);
        else if (cflag)
          fprintf(stream, "struct %s\n{\n", t);
        else if (pflag)
          fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", t);
        else
          fprintf(stream, "class %s\n{ public:\n", t);
        const char *base = "xs:string";
        const xs__complexType *p = &complexType; 
        do
        { if (!p->simpleContent)
            break;
          if (p->simpleContent->restriction)
          { if (p->simpleContent->restriction->complexTypePtr())
	      p = p->simpleContent->restriction->complexTypePtr();
	    else
            { base = p->simpleContent->restriction->base;   
	      break;
	    }
	  }
          else if (p->simpleContent->extension)
          { if (p->simpleContent->extension->complexTypePtr())
	      p = p->simpleContent->extension->complexTypePtr();
            else
            { base = p->simpleContent->extension->base;   
	      break;
            }
	  }
	  else
	    break;
        }
        while (p);
        fprintf(stream, "/// __item wraps '%s' simpleContent.\n", base);
        fprintf(stream, elementformat, tname(NULL, NULL, base), "__item");
        fprintf(stream, ";\n");
        p = &complexType; 
        bool flag = true;
        do
        { if (!p->simpleContent)
            break;
          if (p->simpleContent->restriction)
          { gen(NULL, p->simpleContent->restriction->attribute);
            if (p->simpleContent->restriction->anyAttribute && flag)
              gen(NULL, *p->simpleContent->restriction->anyAttribute);
	    break;
	  }
          else if (p->simpleContent->extension)
          { gen(NULL, p->simpleContent->extension->attribute);
            gen(NULL, p->simpleContent->extension->attributeGroup);
            if (p->simpleContent->extension->anyAttribute && flag)
            { gen(NULL, *p->simpleContent->extension->anyAttribute);
	      flag = false;
	    }
            if (p->simpleContent->extension->complexTypePtr())
	      p = p->simpleContent->extension->complexTypePtr();
            else
	      break;
	  }
	  else
	    break;
        }
        while (p);
      }
      else
      { if (
        /* TODO: should add check for base type == class
	  complexType.simpleContent->extension->simpleTypePtr()
	  ||
	*/
	  complexType.simpleContent->extension->complexTypePtr())
	{ fprintf(stream, "class %s : public %s\n{ public:\n", t, cname(NULL, NULL, complexType.simpleContent->extension->base));
	  soapflag = true;
	}
        else
	{ if (pflag)
            fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", t);
          else
            fprintf(stream, "class %s\n{ public:\n", t);
          fprintf(stream, "/// __item wraps '%s' simpleContent.\n", complexType.simpleContent->extension->base);
          fprintf(stream, elementformat, tname(NULL, NULL, complexType.simpleContent->extension->base), "__item");
          fprintf(stream, ";\n");
	}
        gen(NULL, complexType.simpleContent->extension->attribute);
        gen(NULL, complexType.simpleContent->extension->attributeGroup);
        if (complexType.simpleContent->extension->anyAttribute)
          gen(NULL, *complexType.simpleContent->extension->anyAttribute);
      }
    }
    else
      fprintf(stream, "//\tunrecognized\n");
  }
  else if (complexType.complexContent)
  { if (complexType.complexContent->restriction)
    { if (name)
        fprintf(stream, "\n/// \"%s\":%s is a%s complexType with complexContent restriction of %s.\n", URI?URI:"", name, complexType.abstract?"n abstract":"", complexType.complexContent->restriction->base);
      document(complexType.annotation);
      if (!strcmp(complexType.complexContent->restriction->base, "SOAP-ENC:Array"))
      { char *item = NULL, *type = NULL;
	if (!complexType.complexContent->restriction->attribute.empty())
	{ xs__attribute& attribute = complexType.complexContent->restriction->attribute.front();
	  if (attribute.wsdl__arrayType)
	  { type = (char*)malloc(strlen(attribute.wsdl__arrayType)+1);
	    strcpy(type, attribute.wsdl__arrayType);
	  }
	}
        if (complexType.complexContent->restriction->sequence && !complexType.complexContent->restriction->sequence->element.empty())
	{ xs__element& element = complexType.complexContent->restriction->sequence->element.front();
	  if (!type)
	  { type = (char*)malloc(strlen(element.type)+1);
	    strcpy(type, element.type);
	  }
	  item = element.name;
	}
	gen_soap_array(this, name, t, item, type);
	if (type)
	  free(type);
      }
      else
      { if (!name)
          fprintf(stream, "    struct %s\n    {\n", t);
        else if (cflag)
          fprintf(stream, "struct %s\n{\n", t);
        else if (pflag)
          fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", t);
        else
          fprintf(stream, "class %s\n{ public:\n", t);
	if (!complexType.mixed)
        { if (complexType.complexContent->restriction->group)
            gen(NULL, *complexType.complexContent->restriction->group);
          if (complexType.complexContent->restriction->all)
            gen(NULL, *complexType.complexContent->restriction->all);
          if (complexType.complexContent->restriction->sequence)
            gen(NULL, *complexType.complexContent->restriction->sequence);
          if (complexType.complexContent->restriction->choice)
            gen(NULL, *complexType.complexContent->restriction->choice);
        }
	else
        { fprintf(stream, "/// TODO: mixed complexType/complexContent is user-definable.\n//       Consult the protocol documentation to change or insert declarations.\n");
          fprintf(stream, elementformat, "_XML", "__any");
          fprintf(stream, ";\t///< Catch any element content in XML string\n");
        }
        gen(NULL, complexType.complexContent->restriction->attribute);
      }
    }
    else if (complexType.complexContent->extension)
    { if (name)
        fprintf(stream, "\n/// \"%s\":%s is a%s complexType with complexContent extension of %s.\n", URI?URI:"", name, complexType.abstract?"n abstract":"", complexType.complexContent->extension->base);
      document(complexType.annotation);
      if (!name)
        fprintf(stream, "    struct %s\n    {\n", t);
      else if (cflag)
        fprintf(stream, "struct %s\n{\n", t);
      else if (fflag)
        fprintf(stream, "class %s\n{ public:\n", t);
      else // TODO: what to do if base class is in another namespace and elements must be qualified in XML payload?
      { fprintf(stream, "class %s : public %s\n{ public:\n", t, cname(NULL, NULL, complexType.complexContent->extension->base));
        soapflag = true;
      }
      xs__complexType *p = complexType.complexContent->extension->complexTypePtr();
      while (p)
      { const char *b = cname(NULL, p->schemaPtr()->targetNamespace, p->name);
        static int nesting = 0;
	if (cflag || fflag || !name)
          fprintf(stream, "/// INHERITED FROM %s:\n", b);
        else if (nesting == 0)
          fprintf(stream, "/*  INHERITED FROM %s:\n", b);
        else
          fprintf(stream, "    INHERITED FROM %s:\n", b);
	nesting++;
        if (p->complexContent && p->complexContent->extension)
        { if (p->complexContent->extension->group)
            gen(NULL, *p->complexContent->extension->group); // schema URI?
          if (p->complexContent->extension->all)
            gen(NULL, *p->complexContent->extension->all);
          if (p->complexContent->extension->sequence)
            gen(NULL, *p->complexContent->extension->sequence);
          if (p->complexContent->extension->choice)
            gen(NULL, *p->complexContent->extension->choice);
          gen(NULL, p->complexContent->extension->attribute);
          gen(NULL, p->complexContent->extension->attributeGroup);
          if (p->complexContent->extension->anyAttribute)
            gen(NULL, *p->complexContent->extension->anyAttribute);
	  p = p->complexContent->extension->complexTypePtr();
          modify(b);
	  nesting--;
	  if (cflag || fflag || !name)
	    fprintf(stream, "//  END OF INHERITED\n");
	  else if (nesting == 0)
	    fprintf(stream, "    END OF INHERITED */\n");
	  else
	    fprintf(stream, "    END OF INHERITED\n");
        }
	else
        { if (p->all)
            gen(NULL, p->all->element); // what about schema URI?
          else if (p->choice)
	    gen(NULL, *p->choice);
          else if (p->all)
            gen(NULL, *p->all);
          else if (p->sequence)
            gen(NULL, *p->sequence);
          else if (p->any)
            gen(NULL, *p->any);
          gen(NULL, p->attribute);
          gen(NULL, p->attributeGroup);
	  if (p->anyAttribute)
            gen(NULL, *p->anyAttribute);
          modify(b);
	  nesting--;
	  if (cflag || fflag || !name)
	    fprintf(stream, "//  END OF INHERITED\n");
	  else if (nesting == 0)
	    fprintf(stream, "    END OF INHERITED */\n");
	  else
	    fprintf(stream, "    END OF INHERITED\n");
	  break;
        }
      }
      if (complexType.complexContent->extension->group)
        gen(NULL, *complexType.complexContent->extension->group);
      if (complexType.complexContent->extension->all)
        gen(NULL, *complexType.complexContent->extension->all);
      if (complexType.complexContent->extension->sequence)
        gen(NULL, *complexType.complexContent->extension->sequence);
      if (complexType.complexContent->extension->choice)
        gen(NULL, *complexType.complexContent->extension->choice);
      gen(NULL, complexType.complexContent->extension->attribute);
      gen(NULL, complexType.complexContent->extension->attributeGroup);
      if (complexType.complexContent->extension->anyAttribute)
        gen(NULL, *complexType.complexContent->extension->anyAttribute);
    }
    else
      fprintf(stream, "//\tunrecognized\n");
  }
  else
  { if (name)
      fprintf(stream, "\n/// \"%s\":%s is a%s complexType.\n", URI?URI:"", name, complexType.abstract?"n abstract":"");
    document(complexType.annotation);
    if (!name)
      fprintf(stream, "    struct %s\n    {\n", t);
    else if (cflag)
      fprintf(stream, "struct %s\n{\n", t);
    else if (pflag)
      fprintf(stream, "class %s : public xsd__anyType\n{ public:\n", t);
    else
      fprintf(stream, "class %s\n{ public:\n", t);
    if (complexType.all)
      gen(NULL, *complexType.all);
    else if (complexType.choice)
      gen(NULL, *complexType.choice);
    else if (complexType.sequence)
      gen(NULL, *complexType.sequence);
    else if (complexType.any)
      gen(NULL, *complexType.any);
  }
  gen(NULL, complexType.attribute);
  gen(NULL, complexType.attributeGroup);
  if (complexType.anyAttribute)
    gen(NULL, *complexType.anyAttribute);
  if (name)
  { if (!cflag && !pflag && !soapflag)
    { if (!complexType.complexContent || !complexType.complexContent->extension || !complexType.complexContent->extension->complexTypePtr())
      { fprintf(stream, "/// A handle to the soap struct that manages this instance (automatically set)\n");
        fprintf(stream, pointerformat, "struct soap", "soap");
        fprintf(stream, ";\n");
      }
    }
    modify(t);
    fprintf(stream, "};\n");
  }
}

void Types::gen(const char *URI, const vector<xs__attribute>& attributes)
{ for (vector<xs__attribute>::const_iterator attribute = attributes.begin(); attribute != attributes.end(); ++attribute)
    gen(URI, *attribute);
}

void Types::gen(const char *URI, const xs__attribute& attribute)
{ const char *name, *type;
  name = attribute.name;
  type = attribute.type;
  bool is_optional = attribute.use != required && attribute.use != default_;
  document(attribute.annotation);
  if (attribute.attributePtr()) // attribute ref
  { const char *typeURI = NULL;
    name = attribute.attributePtr()->name;
    if (attribute.schemaPtr()->attributeFormDefault == unqualified || attribute.schemaPtr() != attribute.attributePtr()->schemaPtr())
      URI = attribute.attributePtr()->schemaPtr()->targetNamespace;
    if (attribute.attributePtr()->type)
    { type = attribute.attributePtr()->type;
    }
    else
    { type = name;
      typeURI = attribute.attributePtr()->schemaPtr()->targetNamespace;
    }
    fprintf(stream, "/// Attribute reference %s.\n", attribute.ref);
    document(attribute.attributePtr()->annotation);
    fprintf(stream, attributeformat, pname(is_optional, NULL, typeURI, type), aname(NULL, URI, name)); // make sure no name - type clash
  }
  else if (name && type)
  { fprintf(stream, "/// Attribute %s of type %s.\n", name, type);
    fprintf(stream, attributeformat, pname(is_optional, NULL, NULL, type), aname(NULL, URI, name)); // make sure no name - type clash
  }
  else if (name && attribute.simpleTypePtr())
  { fprintf(stream, "@");
    gen(NULL, NULL, *attribute.simpleTypePtr());
    fprintf(stream, elementformat, "", aname(NULL, URI, name));
  }
  else if (attribute.ref)
  { fprintf(stream, "/// Attribute reference %s.\n", attribute.ref);
    fprintf(stream, attributeformat, pname(is_optional, NULL, NULL, attribute.ref), aname(NULL, NULL, attribute.ref));
  }
  else
  { fprintf(stream, "/// Warning: attribute '%s' has no type or ref. Assuming string content.\n", name?name:"");
    fprintf(stream, attributeformat, tname(NULL, NULL, "xs:string"), aname(NULL, URI, name));
  }
  switch (attribute.use)
  { case optional:
    case default_:
    case fixed_:	// is this correct???
      fprintf(stream, " 0");
      break;
    case prohibited:
      fprintf(stream, " 0:0");
      break;
    case required:
      fprintf(stream, " 1");
      break;
  }
  if (attribute.value)
  { if (type)
    { const char *t = tname(NULL, NULL, type);
      if (!strncmp(t, "unsigned ", 9))
        t += 9;
      if (!strcmp(t, "bool")
       || !strcmp(t, "char")
       || !strcmp(t, "double")
       || !strcmp(t, "float")
       || !strcmp(t, "int")
       || !strcmp(t, "long")
       || !strcmp(t, "LONG64")
       || !strcmp(t, "short")
       || !strcmp(t, "ULONG64"))
        fprintf(stream, " = %s", attribute.value);
      else if (!strcmp(t, "char*")
            || !strcmp(t, "char *"))	// not elegant
        fprintf(stream, " = \"%s\"", cstring(attribute.value));
      else if (!strncmp(t, "enum ", 5))
        fprintf(stream, " = %s", ename(t + 5, attribute.value));
      else if (!strcmp(t, "std::string")
            || !strcmp(t, "std::string*")
	    || !strcmp(t, "std::string *"))	// not elegant
        fprintf(stream, " = \"%s\"", cstring(attribute.value));
      else if (!strcmp(t, "xsd__QName") && attribute.value_)	// QName is in value_
        fprintf(stream, " = \"%s\"", cstring(attribute.value_));
    }
    fprintf(stream, ";\t///< Default value=\"%s\".\n", attribute.value);
  }
  else if (attribute.use == required)
    fprintf(stream, ";\t///< Required attribute.\n");
  else if (attribute.use == prohibited)
    fprintf(stream, ";\t///< Prohibited attribute.\n");
  else
    fprintf(stream, ";\t///< Optional attribute.\n");
}

void Types::gen(const char *URI, const vector<xs__attributeGroup>& attributeGroups)
{ for (vector<xs__attributeGroup>::const_iterator attributeGroup = attributeGroups.begin(); attributeGroup != attributeGroups.end(); ++attributeGroup)
  { if ((*attributeGroup).attributeGroupPtr()) // attributeGroup ref
    { if ((*attributeGroup).schemaPtr() == (*attributeGroup).attributeGroupPtr()->schemaPtr())
      { gen(URI, (*attributeGroup).attributeGroupPtr()->attribute);
        gen(URI, (*attributeGroup).attributeGroupPtr()->attributeGroup);
      }
      else
      { gen((*attributeGroup).attributeGroupPtr()->schemaPtr()->targetNamespace, (*attributeGroup).attributeGroupPtr()->attribute);
        gen((*attributeGroup).attributeGroupPtr()->schemaPtr()->targetNamespace, (*attributeGroup).attributeGroupPtr()->attributeGroup);
      }
      if ((*attributeGroup).attributeGroupPtr()->anyAttribute)
        gen(URI, *(*attributeGroup).attributeGroupPtr()->anyAttribute);
    }
    else
    { gen(URI, (*attributeGroup).attribute);
      gen(URI, (*attributeGroup).attributeGroup);
      if ((*attributeGroup).anyAttribute)
        gen(URI, *(*attributeGroup).anyAttribute);
    }
  }
}

void Types::gen(const char *URI, const vector<xs__all>& alls)
{ for (vector<xs__all>::const_iterator all = alls.begin(); all != alls.end(); ++all)
    gen(URI, *all);
}

void Types::gen(const char *URI, const xs__all& all)
{ gen(URI, all.element);
}

void Types::gen(const char *URI, const vector<xs__sequence>& sequences)
{ for (vector<xs__sequence>::const_iterator sequence = sequences.begin(); sequence != sequences.end(); ++sequence)
    gen(URI, *sequence);
}

void Types::gen(const char *URI, const vector<xs__sequence*>& sequences)
{ for (vector<xs__sequence*>::const_iterator sequence = sequences.begin(); sequence != sequences.end(); ++sequence)
    gen(URI, **sequence);
}

void Types::gen(const char *URI, const xs__sequence& sequence)
{ gen(URI, sequence.element);
  gen(URI, sequence.group);
  gen(URI, sequence.choice);
  gen(URI, sequence.sequence);
  gen(URI, sequence.any);
}

void Types::gen(const char *URI, const vector<xs__element>& elements)
{ for (vector<xs__element>::const_iterator element = elements.begin(); element != elements.end(); ++element)
    gen(URI, *element);
}

void Types::gen(const char *URI, const xs__element& element)
{ const char *name, *type;
  name = element.name;
  type = element.type;
  document(element.annotation);
  if (element.elementPtr()) // element ref
  { const char *typeURI = NULL;
    name = element.elementPtr()->name;
    // TODO: check qualification of element references
    if (element.schemaPtr()->elementFormDefault == unqualified || element.schemaPtr() != element.elementPtr()->schemaPtr())
      URI = element.elementPtr()->schemaPtr()->targetNamespace;
    if (element.elementPtr()->type)
    { type = element.elementPtr()->type;
    }
    else if (element.elementPtr()->schemaPtr())
    { type = name;
      typeURI = element.elementPtr()->schemaPtr()->targetNamespace;
    }
    else
      type = name;
    if (element.maxOccurs && strcmp(element.maxOccurs, "1")) // maxOccurs != "1"
    { const char *s = tname(NULL, typeURI, type);
      if (cflag || sflag)
      { fprintf(stream, "/// Size of the dynamic array of %s is %s..%s\n", s, element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
        fprintf(stream, sizeformat, "int", aname(NULL, NULL, name));
        if (is_integer(element.maxOccurs))
	  fprintf(stream, " %s:%s", element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
	fprintf(stream, ";\n");
        fprintf(stream, "/// Pointer to array of %s.\n", s);
	fprintf(stream, pointerformat, s, aname(NULL, URI, name));
      }
      else
      { fprintf(stream, "/// Vector of %s with length %s..%s\n", s, element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
        fprintf(stream, vectorformat, s, aname(NULL, URI, name));
      }
    }
    else
    { fprintf(stream, "/// Element reference %s.\n", element.ref);
      document(element.elementPtr()->annotation);
      fprintf(stream, elementformat, pname(is_nillable(element), NULL, typeURI, type), aname(NULL, URI, name));
    }
  }
  else if (name && type)
  { if (element.maxOccurs && strcmp(element.maxOccurs, "1")) // maxOccurs != "1"
    { const char *s = tname(NULL, NULL, type);
      if (cflag || sflag)
      { fprintf(stream, "/// Size of array of %s is %s..%s\n", s, element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
        fprintf(stream, sizeformat, "int", aname(NULL, NULL, name));
        if (is_integer(element.maxOccurs))
	  fprintf(stream, " %s:%s", element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
	fprintf(stream, ";\n");
        fprintf(stream, "/// Pointer to array of %s.\n", s);
	fprintf(stream, pointerformat, s, aname(NULL, URI, name));
      }
      else
      { fprintf(stream, "/// Vector of %s with length %s..%s\n", s, element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
	fprintf(stream, vectorformat, s, aname(NULL, URI, name));
      }
    }
    else
    { fprintf(stream, "/// Element %s of type %s.\n", name, type);
      fprintf(stream, elementformat, pname(is_nillable(element), NULL, NULL, type), aname(NULL, URI, name));
    }
  }
  else if (name && element.simpleTypePtr())
  { if (element.maxOccurs && strcmp(element.maxOccurs, "1")) // maxOccurs != "1"
    { fprintf(stream, "/// Size of %s array is %s..%s\n", name, element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
      fprintf(stream, sizeformat, "int", aname(NULL, NULL, name));
      if (is_integer(element.maxOccurs))
        fprintf(stream, " %s:%s", element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
      fprintf(stream, ";\n");
    }
    gen(NULL, NULL, *element.simpleTypePtr());
    if (is_nillable(element)
     || element.maxOccurs && strcmp(element.maxOccurs, "1")) // maxOccurs != "1"
      fprintf(stream, pointerformat, "", aname(NULL, URI, name));
    else
      fprintf(stream, elementformat, "", aname(NULL, URI, name));
  }
  else if (name && element.complexTypePtr())
  { if (element.maxOccurs && strcmp(element.maxOccurs, "1")) // maxOccurs != "1"
    { fprintf(stream, "/// Size of %s array is %s..%s\n", name, element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
      fprintf(stream, sizeformat, "int", aname(NULL, NULL, name));
      if (is_integer(element.maxOccurs))
        fprintf(stream, " %s:%s", element.minOccurs ? element.minOccurs : "1", element.maxOccurs);
      fprintf(stream, ";\n");
    }
    gen(NULL, NULL, *element.complexTypePtr());
    if (is_nillable(element)
     || element.maxOccurs && strcmp(element.maxOccurs, "1")) // maxOccurs != "1"
      fprintf(stream, pointerformat, "}", aname(NULL, URI, name));
    else
      fprintf(stream, elementformat, "}", aname(NULL, URI, name));
  }
  else if (element.ref)
  { fprintf(stream, "/// Element reference %s.\n", element.ref);
    fprintf(stream, elementformat, tname(NULL, NULL, element.ref), aname(NULL, NULL, element.ref));
  }
  else if (name)
  { fprintf(stream, "/// Warning: element '%s' has no type or ref. Assuming XML content.\n", name?name:"");
    fprintf(stream, elementformat, "_XML", aname(NULL, URI, name));
  }
  else
    fprintf(stream, "/// Warning: element has no type or ref.");
  if (!element.minOccurs && !element.nillable && !element.default_)
    fprintf(stream, " 1");
  else if (element.minOccurs)
    fprintf(stream, " %s", element.minOccurs);
  if (element.maxOccurs && strcmp(element.maxOccurs, "1") && is_integer(element.maxOccurs))
    fprintf(stream, ":%s", element.maxOccurs);
  if (element.default_)
  { // determine whether the element can be assigned a default value, this is dependent on the choice of mapping for primitive types
    if (type)
    { const char *t = tname(NULL, NULL, type);
      if (!strncmp(t, "unsigned ", 9))
        t += 9;
      if (!strcmp(t, "bool")
       || !strcmp(t, "char")
       || !strcmp(t, "double")
       || !strcmp(t, "float")
       || !strcmp(t, "int")
       || !strcmp(t, "long")
       || !strcmp(t, "LONG64")
       || !strcmp(t, "short")
       || !strcmp(t, "ULONG64"))
        fprintf(stream, " = %s", element.default_);
      else if (!strcmp(t, "char*")
            || !strcmp(t, "char *"))	// not elegant
        fprintf(stream, " = \"%s\"", element.default_);
      else if (!strncmp(t, "enum ", 5))
        fprintf(stream, " = %s", ename(t + 5, element.default_));
      else if (!strcmp(t, "std::string") || !strcmp(t, "std::string*") || !strcmp(t, "std::string *"))	// not elegant
        fprintf(stream, " = \"%s\"", element.default_);
    }
    fprintf(stream, ";\t///< Default value=\"%s\".\n", element.default_);
  }
  else if (element.nillable)
    fprintf(stream, ";\t///< Nullable pointer.\n");
  else if ((!element.minOccurs || !strcmp(element.minOccurs, "1")) && (!element.maxOccurs || !strcmp(element.maxOccurs, "1")))
    fprintf(stream, ";\t///< Required element.\n");
  else if (element.minOccurs && !strcmp(element.minOccurs, "0") && (!element.maxOccurs || !strcmp(element.maxOccurs, "1")))
    fprintf(stream, ";\t///< Optional element.\n");
  else
    fprintf(stream, ";\n");
}

void Types::gen(const char *URI, const vector<xs__group>& groups)
{ for (vector<xs__group>::const_iterator group = groups.begin(); group != groups.end(); ++group)
    gen(URI, *group);
}

void Types::gen(const char *URI, const xs__group& group)
{ if (group.groupPtr())
  { if (group.schemaPtr() == group.groupPtr()->schemaPtr())
      gen(URI, *group.groupPtr());
    else
      gen(group.groupPtr()->schemaPtr()->targetNamespace, *group.groupPtr());
  }
  else if (group.all)
    gen(URI, group.all->element);
  else if (group.sequence)
    gen(URI, group.sequence->element);
  else if (group.choice)
    gen(URI, *group.choice);
}

void Types::gen(const char *URI, const vector<xs__choice>& choices)
{ for (vector<xs__choice>::const_iterator choice = choices.begin(); choice != choices.end(); ++choice)
    gen(URI, *choice);
}

void Types::gen(const char *URI, const xs__choice& choice)
{ const char *r = NULL, *s = NULL, *t = NULL;
  bool use_union = !uflag;
  if (!URI && choice.schemaPtr())
    URI = choice.schemaPtr()->targetNamespace;
  fprintf(stream, "/// CHOICE OF ELEMENTS <choice");
  if (choice.minOccurs)
    fprintf(stream, " minOccurs=\"%s\"", choice.minOccurs);
  if (choice.maxOccurs)
    fprintf(stream, " maxOccurs=\"%s\"", choice.maxOccurs);
  fprintf(stream, ">\n");
  if (!choice.group.empty() || !choice.sequence.empty())
    use_union = false;
  else
  { for (vector<xs__element>::const_iterator el = choice.element.begin(); el != choice.element.end(); el++)
    { if ((*el).maxOccurs && strcmp((*el).maxOccurs, "1"))
      { use_union = false;
        break;
      }
    }
  }
  if (use_union)
  { t = uname(URI);
    s = strstr(t, "__");
    if (s)
      r = s + 2;
    else
    { r = t;
      s = "__union";
    }
    if (choice.maxOccurs && strcmp(choice.maxOccurs, "1"))
    { fprintf(stream, sizeformat, "int", r);
      if (choice.minOccurs)
        fprintf(stream, " %s", choice.minOccurs);
      if (choice.maxOccurs && strcmp(choice.maxOccurs, "1") && is_integer(choice.maxOccurs))
        fprintf(stream, ":%s", choice.maxOccurs);
      fprintf(stream, ";\nstruct __%s\n{\n", t);
    }
    fprintf(stream, choiceformat, "int", r);
    if (choice.minOccurs)
      fprintf(stream, " %s", choice.minOccurs);
    fprintf(stream, ";\t///< Union %s selector: set to SOAP_UNION_%s_<fieldname>%s\n    union %s\n    {\n", t, t, choice.minOccurs && !strcmp(choice.minOccurs, "0") ? " or 0" : "", t);
  }
  gen(NULL, choice.element);
  gen(NULL, choice.group);
  gen(NULL, choice.sequence);	// TODO: check
  gen(NULL, choice.any);	// TODO: check
  if (use_union)
  { fprintf(stream, elementformat, "}", r);
    if (choice.maxOccurs && strcmp(choice.maxOccurs, "1"))
    { fprintf(stream, ";\n");
      fprintf(stream, pointerformat, "}", s);
    }
    fprintf(stream, ";");
  }
  fprintf(stream, "\n//  END OF CHOICE\n");
}

void Types::gen(const char *URI, const vector<xs__any>& anys)
{ for (vector<xs__any>::const_iterator any = anys.begin(); any != anys.end(); ++any)
    gen(URI, *any);
}

void Types::gen(const char *URI, const xs__any& any)
{ fprintf(stream, "/// TODO: <any");
  if (any.namespace_)
    fprintf(stream, " namespace=\"%s\"", any.namespace_);
  if (any.minOccurs)
    fprintf(stream, " minOccurs=\"%s\"", any.minOccurs);
  if (any.maxOccurs)
    fprintf(stream, " maxOccurs=\"%s\"", any.maxOccurs);
  fprintf(stream, ">\n///       Schema extensibility is user-definable.\n///       Consult the protocol documentation to change and/or insert declarations.\n///       Use wsdl2h option -x to remove this element.\n");
  if (!xflag)
  { fprintf(stream, elementformat, "_XML", "__any");
    fprintf(stream, ";\t///< Catch any element content in XML string.\n");
  }
}

void Types::gen(const char *URI, const xs__anyAttribute& anyAttribute)
{ fprintf(stream, "/// TODO: <anyAttribute");
  if (anyAttribute.namespace_)
    fprintf(stream, " namespace=\"%s\"", anyAttribute.namespace_);
  fprintf(stream, ">\n///       Schema extensibility is user-definable.\n///       Consult the protocol documentation to change and/or insert declarations.\n///       Use wsdl2h option -x to remove this attribute.\n");
  if (!xflag)
  { fprintf(stream, attributeformat, "_XML", "__anyAttribute");
    fprintf(stream, ";\t///< Catch any attribute content in XML string.\n");
  }
}

void Types::document(const xs__annotation *annotation)
{ if (annotation && annotation->documentation)
  { fprintf(stream, "/// @brief");
    documentation(annotation->documentation);
  }
}

void Types::modify(const char *name)
{ // TODO: consider support removal of elements/attributes with ns__X = $- Y
  const char *s = modtypemap[name];
  if (s)
  { while (*s)
    { if (*s++ == '$')
        fprintf(stream, "/// Member declared in %s\n   ", mapfile);
      s = format(s);
    }
  }
}

const char* Types::format(const char *text)
{ const char *s = text;
  if (!s)
    return NULL;
  while (*s && *s != '$')
  { if (*s == '\\')
    { switch (s[1])
      { case 'n': 
          fputc('\n', stream);
	  break;
        case 't': 
          fputc('\t', stream);
	  break;
        default:
          fputc(s[1], stream);
      }
      s++;
    }
    else
      fputc(*s, stream);
    s++;
  }
  fputc('\n', stream);
  return s;
}

////////////////////////////////////////////////////////////////////////////////
//
//	Type map file parsing
//
////////////////////////////////////////////////////////////////////////////////

static char *getline(char *s, size_t n, FILE *fd)
{ int c;
  char *t = s;
  if (n)
    n--;
  for (;;)
  { c = fgetc(fd);
    if (c == '\r')
      continue;
    if (c == '\\')
    { c = fgetc(fd);
      if (c == '\r')
        c = fgetc(fd);
      if (c < ' ')
        continue;
      if (n)
      { *t++ = '\\';
        n--;
      }
    }
    if (c == '\n' || c == EOF)
      break;
    if (n)
    { *t++ = c;
      n--;
    }
  }
  *t++ = '\0';
  if (!*s && c == EOF)
    return NULL;
  return s;
}

static const char *nonblank(const char *s)
{ while (*s && isspace(*s))
    s++;
  return s;
}

static const char *fill(char *t, int n, const char *s, int e)
{ int i = n;
  s = nonblank(s);
  while (*s && *s != e && --i)
    *t++ = *s++;
  while (*s && *s != e)
    s++;
  if (*s)
    s++;
  i = n - i;
  while (isspace(*--t) && i--)
    ;
  t[1] = '\0';
  return s;
}

////////////////////////////////////////////////////////////////////////////////
//
//	Miscellaneous
//
////////////////////////////////////////////////////////////////////////////////

static const char *utf8(char *t, const char *s)
{ unsigned int c = 0;
  int c1, c2, c3, c4;
  c = (unsigned char)*s;
  if (c >= 0x80)
  { c1 = *++s;
    if (c1 < 0x80)
      s--;
    else
    { c1 &= 0x3F;
      if (c < 0xE0)
        c = ((c & 0x1F) << 6) | c1;
      else
      { c2 = *++s & 0x3F;
        if (c < 0xF0)
          c = ((c & 0x0F) << 12) | (c1 << 6) | c2;
        else
	{ c3 = *++s & 0x3F;
          if (c < 0xF8)
            c = ((c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
          else
	  { c4 = *++s & 0x3F;
            if (c < 0xFC)
              c = ((c & 0x03) << 24) | (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
            else
              c = ((c & 0x01) << 30) | (c1 << 24) | (c2 << 18) | (c3 << 12) | (c4 << 6) | *++s & 0x3F;
          }
        }
      }
    }
  }
  sprintf(t, "_x%.4x", c);
  return s;
}

static const char *cstring(const char *s)
{ size_t n;
  char *t;
  const char *r;
  for (n = 0, r = s; *r; n++, r++)
    if (*r == '"' || *r == '\\')
      n++;
    else if (*r < 32)
      n += 3;
  r = t = (char*)emalloc(n + 1);
  for (; *s; s++)
  { if (*s == '"' || *s == '\\')
    { *t++ = '\\';
      *t++ = *s;
    }
    else if (*s < 32)
    { sprintf(t, "\\%03o", (unsigned int)(unsigned char)*s);
      t += 4;
    }
    else
      *t++ = *s;
  }
  *t = '\0';
  return r;
}

static const char *xstring(const char *s)
{ size_t n;
  char *t;
  const char *r;
  for (n = 0, r = s; *r; n++, r++)
  { if (*r < 32 || *r >= 127)
      n += 4;
    else if (*r == '<' || *r == '>')
      n += 3;
    else if (*r == '&')
      n += 4;
    else if (*r == '"')
      n += 5;
  }
  r = t = (char*)emalloc(n + 1);
  for (; *s; s++)
  { if (*s < 32 || *s >= 127)
    { sprintf(t, "&#%.2x;", (unsigned char)*s);
      t += 5;
    }
    else if (*s == '<')
    { strcpy(t, "&lt;");
      t += 4;
    }
    else if (*s == '>')
    { strcpy(t, "&gt;");
      t += 4;
    }
    else if (*s == '&')
    { strcpy(t, "&amp;");
      t += 5;
    }
    else if (*s == '"')
    { strcpy(t, "&quot;");
      t += 6;
    }
    else
      *t++ = *s;
  }
  *t = '\0';
  return r;
}

static bool is_integer(const char *s)
{ if ((*s == '-' || *s == '+') && s[1])
    s++;
  while (*s && isdigit(*s))
    s++;
  return *s == '\0';
}

static void documentation(const char *text)
{ const char *s = text;
  bool flag = true;
  if (!s)
    return;
  while (*s)
  { switch (*s)
    { case '\n':
      case '\t':
      case ' ':
	flag = true;
        break;
      default:
        if (*s > 32)
	{ if (flag)
	  { fputc(' ', stream);
	    flag = false;
          }
	  fputc(*s, stream);
        }
    }
    s++;
  }
  fputc('\n', stream);
}

////////////////////////////////////////////////////////////////////////////////
//
//	Allocation
//
////////////////////////////////////////////////////////////////////////////////

void *emalloc(size_t size)
{ void *p = malloc(size);
  if (!p)
  { fprintf(stderr, "Error: Malloc failed\n");
    exit(1);
  }
  return p;
}

char *estrdup(const char *s)
{ char *t = (char*)emalloc(strlen(s) + 1);
  strcpy(t, s);
  return t;
}
