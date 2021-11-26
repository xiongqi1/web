/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <libxml/xmlreader.h>

static char	str[1024] = {0};

static	int cond (char *);
static	void correction (char *);
static void processNode (xmlTextReaderPtr);
static void streamFile (const char *);

static	int
cond (char *s)
{
	char	tmp[1024] = {0};
	char	*p;

	strcpy (tmp, str);
	p = strtok (tmp, ";");
	p = strtok (NULL, ";");

	return	p?(strcmp (p, "98") && strcmp (p, "99")):0;
}

static	void
correction (char *s)
{
	char	tmp[1024] = {0};
	char	*p;
	int		i;

	strcpy (tmp, str);

	if (cond(str) && (p = strtok (str, ";")))
	{
		printf ( "%s;", p );

		for (i = 0; i < 8; i++) {
			p = strtok (NULL, ";");
		}

		printf ("%s;", p);
		p = strtok (tmp, ";");

		for (i = 0; i < 7; i++) {
			p = strtok (NULL, ";");
			printf ( "%s;", p );
		}

		strtok (NULL, ";");
		p = strtok (NULL, ";");

		if (p) {
			printf ( ";%s", p );
		} else {
			printf ( ";" );
		}
	} else {
		printf("%s", str);
	}
}

static void
processNode(xmlTextReaderPtr reader)
{
    const	xmlChar *name;
    char			tmp[256] = {0};
    static xmlChar path[256], bpath[256], param[256], bparam[256], *p;

    name = xmlTextReaderConstName(reader);

    if (!strcmp ((char *)name, "object")) {
    	strcpy ((char *)bpath, (char *)xmlTextReaderGetAttribute(reader, (xmlChar *)"base"));
  		while ((p = (xmlChar *)strstr ((char *)bpath,"{i}"))) {
   			strcpy ((char *)p,"0");
   			memmove (p + 1,p + 3,strlen((char *)p + 3) + 1);
  		}

    	if (strcmp ((char *)path, (char *)bpath)) {
    		strcpy ((char *)path, (char *)bpath);

    		if (strrchr((char *)path,'0')
    			&& !strcmp(strrchr((char *)path,'0')-1,
    					    ((char *)path + strlen((char *)path)-3))
    					    && !strcmp(strrchr((char *)path,'0')-1,
    					    ".0.")) {
    			correction (str);
    			sprintf (str, "\n%.*s;99;0;0;0;0;0;0;0;;\n%s;97;0;0;0;0;0;0;0;;", (int)strlen((char *)path) - 2, path, path);
    		} else {
    			correction (str);
    			sprintf (str, "\n%s;98;0;0;0;0;0;0;0;;", path);
    		}
    	}
    }

    if (!strcmp((char *)name, "parameter")) {
       	strcpy((char *)bparam, (char *)xmlTextReaderGetAttribute((xmlTextReaderPtr)reader, (xmlChar *)"base"));

       	if (strcmp((char *)param, (char *)bparam)) {
			correction (str);
       		strcpy((char *)param, (char *)bparam);
       		sprintf (	str, "\n%s%s;%s;%s;%s;%s;%s;%s;%s;;", path, param,
						xmlTextReaderGetAttribute(reader, (xmlChar *)"instance"),
						xmlTextReaderGetAttribute(reader, (xmlChar *)"notification"),
						xmlTextReaderGetAttribute(reader, (xmlChar *)"maxNotification"),
						xmlTextReaderGetAttribute(reader, (xmlChar *)"reboot"),
						xmlTextReaderGetAttribute(reader, (xmlChar *)"initIdx"),
						xmlTextReaderGetAttribute(reader, (xmlChar *)"getIdx"),
						xmlTextReaderGetAttribute(reader, (xmlChar *)"setIdx"));
       	}
    }

    if (!strcmp((char *)name, "string")) {
    	sprintf (tmp, "%s;", "6");
    	strcat (str, tmp);
    } else {
    	if (!strcmp((char *)name, "int")) {
    		sprintf (tmp, "%s;", "7");
        	strcat (str, tmp);
        } else {
    		if (!strcmp((char *)name, "unsignedInt")) {
    			sprintf (tmp, "%s;", "9");
    	    	strcat (str, tmp);
    		} else {
    			if (!strcmp((char *)name, "boolean")) {
    				sprintf (tmp, "%s;", "18");
    		    	strcat (str, tmp);
			    } else {
    				if (!strcmp((char *)name, "dateTime")) {
    					sprintf (tmp, "%s;", "11");
    			    	strcat (str, tmp);
				    } else {
    					if (!strcmp((char *)name, "base64")) {
    						sprintf (tmp, "%s;", "12");
    				    	strcat (str, tmp);
    					}
    				}
    		    }
    		}
        }
    }

    if( !strcmp((char *)name, "default") ) {
    	sprintf (tmp, "%s", xmlTextReaderGetAttribute(reader, (xmlChar *)"value"));
    	strcat (str, tmp);
	}

//    if( !strcmp(name, "entry") ) {
//       	strcpy(bentry, xmlTextReaderReadString(reader));

//      	if (strcmp(entry,bentry)) {
//       		strcpy(entry,bentry);
//       		printf("%s,", entry);
//      	}
//	}
}

static void
streamFile(const char *filename)
{
    xmlTextReaderPtr reader;
    int ret;

    reader = xmlReaderForFile(filename, NULL, 0);
    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            processNode(reader);
            ret = xmlTextReaderRead(reader);
        }

	correction(str);
	printf("\n");
//        printf("%s", str);

        xmlFreeTextReader(reader);
        if (ret != 0) {
            fprintf(stderr, "%s : failed to parse\n", filename);
        }
    } else {
        fprintf(stderr, "Unable to open %s\n", filename);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return(1);

    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

    streamFile(argv[1]);

    /*
     * Cleanup function for the XML library.
     */
    xmlCleanupParser();
    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();
    return(0);
}
