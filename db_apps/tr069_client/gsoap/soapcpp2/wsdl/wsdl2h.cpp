/*

wsdl2h.cpp

WSDL parser and converter to gSOAP header file format

--------------------------------------------------------------------------------
gSOAP XML Web services tools
Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc. All Rights Reserved.
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

Build:
	soapcpp2 -ipwsdl wsdl.h
	g++ -o wsdl2h wsdl2h.cpp types.cpp service.cpp wsdl.cpp schema.cpp wsdlC.cpp stdsoap2.cpp
	
TODO:
	Resolve relative versus absolute import paths for reading imported WSDL/schema (use URL local addresses)
	Do not generate abstract complexTypes, but include defs in derived types
	Handle simpleType derivation from base64
	Option to define base class for all classes, e.g. -b xsd__anyType
	Look into improving xs:any

*/

#include "includes.h"
#include "types.h"
#include "service.h"

static void init();
static void options(int argc, char **argv);

int cflag = 0,
    eflag = 0,
    fflag = 0,
    gflag = 0,
    iflag = 0,
    lflag = 0,
    mflag = 0,
    pflag = 0,
    sflag = 0,
    uflag = 0,
    vflag = 0,
    wflag = 0,
    xflag = 0,
    yflag = 0;

int infiles = 0;
char *infile[100],
     *outfile = NULL,
     *mapfile = "typemap.dat",
     *proxy_host = NULL,
     *import_path = NULL;

int proxy_port = 8080;

FILE *stream = stdout;

SetOfString exturis;

const char *service_prefix = NULL;
const char *schema_prefix = "ns";

char elementformat[]   = "    %-35s  %-30s";
char pointerformat[]   = "    %-35s *%-30s";
char attributeformat[] = "   @%-35s  %-30s";
char vectorformat[]    = "    std::vector<%-23s> %-30s";
char arrayformat[]     = "    %-35s *__ptr%-25s";
char sizeformat[]      = "    %-35s  __size%-24s";
char offsetformat[]    = "//  %-35s  __offset%-22s";
char choiceformat[]    = "    %-35s  __%-28s";
char schemaformat[]    = "//gsoap %-5s schema %s:\t%s\n";
char serviceformat[]   = "//gsoap %-4s service %s:\t%s %s\n";
char paraformat[]      = "    %-35s%s%s%s";
char anonformat[]      = "    %-35s%s_%s%s";

char copyrightnotice[] = "\n**  The gSOAP WSDL parser for C and C++ "VERSION"\n**  Copyright (C) 2000-2005 Robert van Engelen, Genivia Inc.\n**  All Rights Reserved. This product is provided \"as is\", without any warranty.\n**  The gSOAP WSDL parser is released under one of the following two licenses:\n**  GPL or the commercial license by Genivia Inc. Use option -l for more info.\n\n";

char licensenotice[]   = "\
--------------------------------------------------------------------------------\n\
gSOAP XML Web services tools\n\
Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc. All Rights Reserved.\n\
\n\
This software is released under one of the following two licenses:\n\
GPL or Genivia's license for commercial use.\n\
\n\
GPL license.\n\
\n\
This program is free software; you can redistribute it and/or modify it under\n\
the terms of the GNU General Public License as published by the Free Software\n\
Foundation; either version 2 of the License, or (at your option) any later\n\
version.\n\
\n\
This program is distributed in the hope that it will be useful, but WITHOUT ANY\n\
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A\n\
PARTICULAR PURPOSE. See the GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License along with\n\
this program; if not, write to the Free Software Foundation, Inc., 59 Temple\n\
Place, Suite 330, Boston, MA 02111-1307 USA\n\
\n\
Author contact information:\n\
engelen@genivia.com / engelen@acm.org\n\
--------------------------------------------------------------------------------\n\
A commercial use license is available from Genivia, Inc., contact@genivia.com\n\
--------------------------------------------------------------------------------\n";

int main(int argc, char **argv)
{ init();
  options(argc, argv);
  fprintf(stderr, copyrightnotice);
  if (lflag)
  { fprintf(stderr, licensenotice);
    if (!infiles)
      exit(0);
  }
  wsdl__definitions definitions;
  if (infiles)
  { if (!outfile)
    { if (strncmp(infile[0], "http://", 7) && strncmp(infile[0], "https://", 8))
      { const char *s = strrchr(infile[0], '.');
        if (s && (!strcmp(s, ".wsdl") || !strcmp(s, ".gwsdl") || !strcmp(s, ".xsd")))
        { outfile = estrdup(infile[0]);
          outfile[s - infile[0] + 1] = 'h';
          outfile[s - infile[0] + 2] = '\0';
        }
        else
        { outfile = (char*)emalloc(strlen(infile[0]) + 3);
          strcpy(outfile, infile[0]);
          strcat(outfile, ".h");
        }
      }
    }
  }
  if (outfile)
  { stream = fopen(outfile, "w");
    if (!stream)
    { fprintf(stderr, "Cannot write to %s\n", outfile);
      exit(1);
    }
    fprintf(stderr, "Saving %s\n\n", outfile);
  }
  Definitions def;
  definitions.read(infiles, infile);
  if (definitions.error())
  { definitions.print_fault();
    exit(1);
  }
  definitions.traverse();
  def.compile(definitions);
  if (outfile)
  { fclose(stream);
    fprintf(stderr, "\nTo complete the process, compile with:\nsoapcpp2 %s\n\n", outfile);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
//	Initialization
//
////////////////////////////////////////////////////////////////////////////////

static void init()
{ struct Namespace *p = namespaces;
  if (p)
  { for (; p->id; p++)
    { if (p->in && *p->in)
        exturis.insert(p->in);
      if (p->ns && *p->ns)
        exturis.insert(p->ns);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//
//	Parse command line options
//
////////////////////////////////////////////////////////////////////////////////

static void options(int argc, char **argv)
{ int i;
  infiles = 0;
  for (i = 1; i < argc; i++)
  { char *a = argv[i];
    if (*a == '-'
#ifdef WIN32
     || *a == '/'
#endif
    )
    { int g = 1;
      while (g && *++a)
      { switch (*a)
        { case 'c':
            cflag = 1;
       	    break;
	  case 'e':
	    eflag = 1;
	    break;
	  case 'f':
	    fflag = 1;
	    break;
	  case 'g':
	    gflag = 1;
	    break;
	  case 'i':
	    iflag = 1;
	    break;
          case 'I':
            a++;
            g = 0;
            if (*a)
              import_path = a;
            else if (i < argc && argv[++i])
              import_path = argv[i];
            else
              fprintf(stderr, "wsdl2h: Option -I requires a path argument");
	    break;
	  case 'l':
	    lflag = 1;
	    break;
	  case 'm':
	    mflag = 1;
	    break;
          case 'n':
            a++;
            g = 0;
            if (*a)
              schema_prefix = a;
            else if (i < argc && argv[++i])
              schema_prefix = argv[i];
            else
              fprintf(stderr, "wsdl2h: Option -n requires a prefix name argument");
	    break;
          case 'N':
            a++;
            g = 0;
            if (*a)
              service_prefix = a;
            else if (i < argc && argv[++i])
              service_prefix = argv[i];
            else
              fprintf(stderr, "wsdl2h: Option -N requires a prefix name argument");
	    break;
          case 'o':
            a++;
            g = 0;
            if (*a)
              outfile = a;
            else if (i < argc && argv[++i])
              outfile = argv[i];
            else
              fprintf(stderr, "wsdl2h: Option -o requires an output file argument");
	    break;
	  case 'p':
	    pflag = 1;
	    break;
	  case 'r':
            a++;
            g = 0;
            if (*a)
              proxy_host = a;
            else if (i < argc && argv[++i])
              proxy_host = argv[i];
            else
              fprintf(stderr, "wsdl2h: Option -r requires a proxy host:port argument");
            if (proxy_host)
	    { char *s = (char*)emalloc(strlen(proxy_host + 1));
	      strcpy(s, proxy_host);
	      proxy_host = s;
	      s = strchr(proxy_host, ':');
	      if (s)
	      { proxy_port = soap_strtol(s + 1, NULL, 10);
	        *s = '\0';
	      }
	    }
	    break;
	  case 's':
	    sflag = 1;
	    break;
          case 't':
            a++;
            g = 0;
            if (*a)
              mapfile = a;
            else if (i < argc && argv[++i])
              mapfile = argv[i];
            else
              fprintf(stderr, "wsdl2h: Option -t requires a type map file argument");
	    break;
	  case 'u':
	    uflag = 1;
	    break;
	  case 'v':
	    vflag = 1;
	    break;
	  case 'w':
	    wflag = 1;
	    break;
	  case 'x':
	    xflag = 1;
	    break;
	  case 'y':
	    yflag = 1;
	    break;
          case '?':
          case 'h':
            fprintf(stderr, "Usage: wsdl2h [-c] [-e] [-f] [-g] [-h] [-I path] [-l] [-m] [-n name] [-N name] [-p] [-r proxyhost:port] [-s] [-t typemapfile.dat] [-u] [-v] [-w] [-x] [-y] [-o outfile.h] infile.wsdl infile.xsd http://www... ...\n\n");
            fprintf(stderr, "\
-c      generate C source code\n\
-e      don't qualify enum names\n\
-f      generate flat C++ class hierarchy\n\
-g      generate global top-level element declarations\n\
-h      display help info\n\
-Ipath  use path to find files\n\
-l      include license information in output\n\
-m      use xsd.h module to import primitive types\n\
-nname  use name as the base namespace prefix instead of 'ns'\n\
-Nname  use name as the base namespace prefix for service namespaces\n\
-ofile  output to file\n\
-p      create polymorphic types with C++ inheritance with base xsd__anyType\n\
-rhost:port\n\
        connect via proxy host and port\n\
-s      don't generate STL code (no std::string and no std::vector)\n\
-tfile  use type map file instead of the default file typemap.dat\n\
-u      don't generate unions\n\
-v      verbose output\n\
-w      always wrap response parameters in a response struct (<=1.1.4 behavior)\n\
-x      don't generate _XML any/anyAttribute extensibility elements\n\
-y      generate typedef synonyms for structs and enums\n\
infile.wsdl infile.xsd http://www... list of input sources (if none use stdin)\n\
\n");
            exit(0);
          default:
            fprintf(stderr, "wsdl2h: Unknown option %s\n", a);
            exit(1);
        }
      }
    }
    else
    { infile[infiles++] = argv[i];
      if (infiles >= 100)
      { fprintf(stderr, "wsdl2h: too many files\n");
        exit(1);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//
//	Namespaces
//
////////////////////////////////////////////////////////////////////////////////

struct Namespace namespaces[] =
{
  {"SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/", "http://www.w3.org/*/soap-envelope"},
  {"SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/", "http://www.w3.org/*/soap-encoding"},
  {"xsi", "http://www.w3.org/2001/XMLSchema-instance"},
  {"xsd", ""}, // http://www.w3.org/2001/XMLSchema"}, // don't use this, it might conflict with xs
  {"xml", "http://www.w3.org/XML/1998/namespace"},
  {"xs", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema" },
  {"http", "http://schemas.xmlsoap.org/wsdl/http/"},
  {"soap", "http://schemas.xmlsoap.org/wsdl/soap/", "http://schemas.xmlsoap.org/wsdl/soap*/"},
  {"mime", "http://schemas.xmlsoap.org/wsdl/mime/"},
  {"dime", "http://schemas.xmlsoap.org/ws/2002/04/dime/wsdl/", "http://schemas.xmlsoap.org/ws/*/dime/wsdl/"},
  {"wsdl", "http://schemas.xmlsoap.org/wsdl/"},
  {"gwsdl", "http://www.gridforum.org/namespaces/2003/03/gridWSDLExtensions"},
  {"sd", "http://www.gridforum.org/namespaces/2003/03/serviceData"},
  {NULL, NULL}
};
