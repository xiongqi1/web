/*

soapcpp2.c

Main compiler.

gSOAP XML Web services tools
Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc. All Rights Reserved.
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
Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc. All Rights Reserved.
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

#include "soapcpp2.h"

extern int init();
extern int yyparse();
extern FILE *yyin;

int vflag = 0;		/* SOAP version, 0=not set, 1=1.1, 2=1.2 */
int wflag = 0;		/* when set, don't generate WSDL and schema files */
int Cflag = 0;		/* when set, generate only files for clients */
int cflag = 0;		/* when set, generate files with .c extension */
int eflag = 0;		/* when set, use SOAP RPC encoding by default */
int iflag = 0;		/* when set, generate new style proxy/object classes inherited from soap struct */
int mflag = 0;		/* when set, generate code that requires array/binary classes to explicitly remove malloced array */
int nflag = 0;		/* when set, names the namespaces global struct '%NAME%_namespaces */
int lflag = 0;		/* when set, create library */
int Lflag = 0;		/* when set, don't generate soapClientLib/soapServerLib */
int Sflag = 0;		/* when set, generate only files for servers */
int tflag = 0;		/* when set, generates typed messsages (with xsi:type attributes) */
int xflag = 0;		/* when set, excludes imported types */

int stop_flag = 0;

char dirpath[1024];	/* directory path for generated source files */
char *prefix = "soap";	/* file name prefix for generated source files */
char filename[1024];	/* current file name */
char *importpath = NULL;/* file import path */

/*
IMPORTANT:

The terms and conditions of use of this software do not allow for the removal
of the copyright notice from the main program for visual display. For
integrations with other software, a similar copyright notice must be produced
that is visible to users of the software.
*/

int
main(int argc, char **argv)
{	int i, g;
	char *a, *s;
	strcpy(filename, "<stdin>");
	for (i = 1; i < argc; i++)
	{	a = argv[i];
		if (*a == '-'
#ifdef WIN32
		 || *a == '/'
#endif
		)
		{	g = 1;
			while (g && *++a)
				switch (*a)
				{	case 'C':
						Cflag = 1;
						break;
					case 'c':
						cflag = 1;
						break;
					case 'd':
						a++;
						g = 0;
						if (*a)
							strcpy(dirpath, a);
						else if (i < argc && argv[++i])
							strcpy(dirpath, argv[i]);
						else
							execerror("Option -d requires a directory path");
						if (*dirpath && dirpath[strlen(dirpath)-1] != '/' && dirpath[strlen(dirpath)-1] != '\\')
#ifdef WIN32
							strcat(dirpath, "\\");
#else
							strcat(dirpath, "/");
#endif
						break;
					case 'e':
						eflag = 1;
						break;
					case '?':
					case 'h':
						fprintf(stderr, "Usage: soapcpp2 [-1|-2] [-C|-S] [-L] [-c] [-d path] [-e] [-h] [-i] [-I path:path:...] [-m] [-n] [-p name] [-t] [-v] [-w] [-x] [infile]\n\n");
						fprintf(stderr, "\
-1      generate SOAP 1.1 bindings\n\
-2      generate SOAP 1.2 bindings\n\
-e	generate SOAP RPC encoding style bindings\n\
-C	generate client-side code only\n\
-S	generate server-side code only\n\
-L	don't generate soapClientLib/soapServerLib\n\
-c      generate C source code\n\
-i      generate service proxies and objects inherited from soap struct\n\
-dpath  use path to save files\n\
-Ipath  use path(s) for #import\n\
-m      generate modules (experimental)\n\
-n      use service name to rename service functions and namespace table\n\
-pname  save files with new prefix name instead of 'soap'\n\
-t      generate code for fully xsi:type typed SOAP/XML messaging\n\
-w	don't generate WSDL and schema files\n\
-x	don't generate sample XML message files\n\
-h	display help info\n\
-v	display version info\n\
infile	header file to parse (or stdin)\n\
\n");
						exit(0);
					case 'I':
						a++;
						g = 0;
						s = NULL;
						if (*a)
							s = a;
						else if (i < argc && argv[++i])
							s = argv[i];
						else
							execerror("Option -I requires an import path");
						if (importpath && s)
						{	char *t	= emalloc(strlen(importpath) + strlen(s) + 2);
							strcpy(t, importpath);
							strcat(t, ":");
							strcat(t, s);
							importpath = t;
						}
						else
							importpath = s;
						break;
					case 'i':
						iflag = 1;
						break;
					case 'm':
						mflag = 1;
						break;
					case 'n':
						nflag = 1;
						break;
					case 'l':
						lflag = 1;
						break;
					case 'L':
						Lflag = 1;
						break;
					case 'S':
						Sflag = 1;
						break;
					case 't':
						tflag = 1;
						break;
					case 'w':
						wflag = 1;
						break;
					case 'x':
						xflag = 1;
						break;
					case 'p':
						a++;
						g = 0;
						if (*a)
							prefix = a;
						else if (i < argc && argv[++i])
							prefix = argv[i];
						else
							execerror("Option -p requires an output file name prefix");
						break;
					case '1':
						vflag = 1;
						envURI = "http://schemas.xmlsoap.org/soap/envelope/";
						encURI = "http://schemas.xmlsoap.org/soap/encoding/";
						break;
					case '2':
						vflag = 2;
						envURI = "http://www.w3.org/2003/05/soap-envelope";
						encURI = "http://www.w3.org/2003/05/soap-encoding";
						rpcURI = "http://www.w3.org/2003/05/soap-rpc";
						break;
					case 'v':
						stop_flag = 1;
						break;
					default:
            					fprintf(stderr, "soapcpp2: Unknown option %s\n", a);
            					exit(1);
				}
		}
		else if (!(yyin = fopen(argv[i], "r")))
		{	sprintf(errbuf, "Cannot open file \"%s\" for reading", argv[i]);
			execerror(errbuf);
		}
		else
			strcpy(filename, argv[i]);
	}
	fprintf(stdout, "\n**  The gSOAP Stub and Skeleton Compiler for C and C++ "VERSION"\n**  Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc.\n**  All Rights Reserved. This product is provided \"as is\", without any warranty.\n**  The gSOAP compiler is released under one of the following three licenses:\n**  GPL, the gSOAP public license, or the commercial license by Genivia Inc.\n\n");
	if (stop_flag)
	  exit(0);
	init();
	if (yyparse())
		synerror("skipping the remaining part of the input");
	return errstat();
}
