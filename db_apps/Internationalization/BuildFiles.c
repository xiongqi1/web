#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include "webif_spec.h"
/* Version Information */
#define VERSION_INFO	"2.2"

#define ENTRIES 6000
#define IDSIZE 64
#define SPECSIZE 64
#define STRSIZE 4096
#define HTML_LINE_SIZE 2048
#define NO_OF_SEMICOLONS 13

enum { EN=0, AR, FR, DE, IT, ES, PT, CZ, NL, TW, CN, JP };
const char* p_language_list[] = {"English", "Arabic", "French", "German", "Italian", "Spanish", "Portugese", "Czech", "Dutch", "TradChinese", "SimpChinese", "Japanese"};
const char* p_language_files[] = {"en", "ar", "fr", "de", "it", "es", "pt", "cz", "nl", "tw", "cn", "jp"};

#define ARRAY_SIZE sizeof(p_language_files)/sizeof(p_language_files[0])

typedef struct entry {
	char id[IDSIZE];
	char msgstr[STRSIZE]; //for the option -B (Build) only
	char spec[SPECSIZE];
	char lang[ARRAY_SIZE][STRSIZE];
} entry;

struct entry list[ENTRIES]; //all the itams from csv file are stored to this struct

char* p_language = 0;
char* p_fileDir = 0;
FILE* csvFileIn = 0;
FILE* htmlFileIn = 0;
int language = EN;
static char buffer[STRSIZE];
int file_ind= 0; //counting for found ids
char htmlbuf[HTML_LINE_SIZE];

unsigned int found_ind=0;
struct found {
	char id[IDSIZE];
	char spec[SPECSIZE];
	int ind;
} found[ENTRIES/2];

char* fileList[] = { //for the option -B (Build) only
	"NTC-GUI",
	"TELUS-GUI",
	"firewall",
	"admin",
	"internet",
	"main",
	"storage",
	"wireless"
	"\0"
};

void usage( void ) {
int i;
	fprintf(stderr, "\n Usage: BuildFiles [OPTION]\n" );
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-B Build language .xml files (for Platypus platform) [ English ");
	for(i=1; p_language_list[i]; i++) {
		fprintf(stderr, "| %s ",p_language_list[i]);
	}
	fprintf(stderr, "]\n");
	fprintf(stderr, "\t-O Build Optimised language .xml files (for Bovine and Platypus2 platforms) [ English ");
	for(i=1; p_language_list[i]; i++) {
		fprintf(stderr, "| %s ",p_language_list[i]);
	}
	fprintf(stderr, "]\n");
	fprintf(stderr, "\t-V Version information\n");
	fprintf(stderr, "\n");
	exit(1);
}

void CheckString(char* buffer) {
	int i;
	int length = strlen(buffer);

	for (i = 0; i < length; i++) {
		if ( (buffer[i] == '\015') || (buffer[i] == '\012') ) {
			buffer[i] = '\0';
			return;
		}
	}
}

void CheckAndAddSemicolons(char* buffer) {
	int i;
	int length;
	int semiCount;
	int need;

	length = strlen(buffer);
	semiCount = 0;

	for (i = 0; i < length; i++) {
		if (buffer[i] == ';')
		    semiCount++;
	}

	need =  NO_OF_SEMICOLONS - semiCount;

	for (i = 0; i < need; i++) {
		strcat(buffer, ";");
	}
}

// quick html coding
const char* encodeHTML(const char* buf) {
	#define SAFETY_GAP		20
	#define STRLEN(x)		(sizeof(x)-1)

	static char html[STRSIZE*5+SAFETY_GAP];
	const char* s=buf;
	char* d=html;
	char c;
	char* replace;

	while(*s && (d<html+sizeof(html)-SAFETY_GAP)) {
		c=*s++;
		// if carrige return
		if(c=='\\' && *s=='n') {
			strcpy(d,"&#xa;");
			d+=STRLEN("&#xa;");
			s++;
			continue;
		}

		switch(c) {
			case '"':		replace="&quot;";	break;
			case '<':		replace="&lt;";		break;
			case '>':		replace="&gt;";		break;
			case '&':		replace="&amp;";	break;
			default:		replace=0; 		break;
		}

		if(replace) {
			strcpy(d,replace);
			d+=strlen(replace);
		}
		else {
			*d++=c;
		}
	}
	*d=0;
	return html;
}

void DumpStringsFor(char* file) {
	FILE* outputFile = 0;
	char  nameBuffer[32];
	int   i;

	snprintf(nameBuffer, sizeof(nameBuffer), "%s.xml", file);
	outputFile = fopen(nameBuffer, "w");
	if (!outputFile) {
		fprintf(stderr, "failed to open the file %s\n", strerror(errno));
		exit(1);
	}
	fprintf(outputFile, "<po>\n");
	for (i = 0; i < file_ind; i++) {
		if (strstr(list[i].spec, file) != 0) {
			fprintf(outputFile, "<message msgid=\"%s\"", list[i].id);
			fprintf(outputFile, " msgstr=\"%s\" ", encodeHTML(list[i].msgstr));
			fprintf(outputFile, "/>\n");
		}
	}
	fprintf(outputFile, "</po>\n");
	fclose(outputFile);
}

int build_xml( void ) { // for the option -B: build the xml fils depands on the filed of spec
int i;
char* id  = 0;
char* fle = 0;
char* msg = 0;
char* rets = 0;
int line_counter=0;

	/* Loop through the list of languages looking for a match */
	for (language = 0; p_language_list[language] != NULL; language++) {
		if (strcmp(p_language, p_language_list[language]) == 0) {
			break;
		}
	}

	/* If a match wasn't found, return the usage message */
	if (!p_language_list[language]) {
		usage();
	}

	for (file_ind= 0; file_ind< ENTRIES; file_ind++) {
		list[file_ind].id[0] = '\0';
		list[file_ind].spec[0] = '\0';
		list[file_ind].msgstr[0] = '\0';
	}

	file_ind= 0;

	csvFileIn = fopen("./strings.csv", "r");
	if (!csvFileIn) {
		fprintf(stderr, "failed to open the file %s\n", strerror(errno));
		exit(1);
	}

	rets = fgets(buffer, sizeof(buffer), csvFileIn);
	// the string file saved through excel or kspread (not sure about openOffice) does not
	// always write out the full set of semicolons when there are trilaing missing languge strings
	// The CheckAndAddSemicolons function will check and add the appropiate number.

	CheckAndAddSemicolons(buffer);

	file_ind= 0;
	line_counter=1;
	while ( rets ) {
		if ( (strlen(buffer) < 12) || (buffer[0] == ';') ) {
			;//fprintf(stderr, "ignored %s", buffer);
		}
		else {

			id = strtok(buffer, ";");
			fle = strtok(NULL, ";");

			if( !id || !fle ) {
				fprintf( stderr, "Incorrect file: strings.csv\nerror at line %i\n", line_counter);
				exit (1);
			}

			char* ptr = fle+strlen(fle)+1;
			for(i=0; i<=language; i++) {
				msg = ptr;
				ptr = strchr(ptr+1, ';');
			}
			if( ptr )
				*ptr = 0;
			if( !msg )
				msg="\0";

			CheckString(id);
			CheckString(fle);
			CheckString(msg);

			strncpy(list[file_ind].id     , id ,     STRSIZE);
			strncpy(list[file_ind].spec  , fle,     STRSIZE);
			strncpy(list[file_ind].msgstr  , msg,     STRSIZE);
			file_ind++;
		}
		rets = fgets(buffer, sizeof(buffer), csvFileIn);
		CheckAndAddSemicolons(buffer);
		line_counter++;
	}
	fclose(csvFileIn);
	csvFileIn = 0;

	int fIndex;

	for (fIndex = 0; fileList[fIndex]; fIndex++) {
		DumpStringsFor(fileList[fIndex]);
	}

	return 0;
}

void DumpOptimisedStringsFor(char* file, char *language, int idx) { // for the option -O: Dump Optimised id and strings to the xml file
	FILE* outputFile = 0;
	char  nameBuffer[64];
	int   i;

	snprintf(nameBuffer, sizeof(nameBuffer), "mkdir lang/%s/  > /dev/null 2> /dev/null", language);
	system( nameBuffer );
	snprintf(nameBuffer, sizeof(nameBuffer), "lang/%s/%s", language, file);
	outputFile = fopen(nameBuffer, "w");

	if (!outputFile) {
		fprintf(stderr, "failed to open the file: lang/%s/%s -- %s\n", language, file, strerror(errno));
		exit(1);
	}

	fprintf(outputFile, "<po>\n");
	for (i = 0; i < file_ind; i++) {
		fprintf(outputFile, "<message msgid=\"%s\"", list[i].id);
		fprintf(outputFile, " msgstr=\"%s\"", encodeHTML(list[i].lang[idx]));
		fprintf(outputFile, "/>\n");
	}

	/* we need merger the ids from common used javascript file util.js */
	if(strcmp(file,"util.xml")) {
		snprintf(nameBuffer, sizeof(nameBuffer), "lang/%s/util.xml", language);
		htmlFileIn = fopen(nameBuffer, "r");
		if (htmlFileIn) {
			while ( fgets(htmlbuf, sizeof(htmlbuf), htmlFileIn) ) {
				if(!strstr(htmlbuf, "po>")) {
					fprintf(outputFile, "%s", htmlbuf);
				}
			}
			fclose (htmlFileIn);
		}
	}
	fprintf(outputFile, "</po>\n");
	fclose(outputFile);
}

/* return values from check_found() */
enum { NOT_FOUND=0, ALREADY_FOUND, SPEC_NOT_MATCH, SPEC_MATCH };

int check_found( char *id, char *spec ) {
int i;
	for( i=0; i< found_ind; i++ ) {
		if(strcmp(id, found[i].id )==0) {
			/* the id is already in found list, checking for V_WEBIF_SPEC */
			if(found[i].ind!=file_ind) {
				return ALREADY_FOUND;
			}
			if(strlen(V_WEBIF_SPEC)==0) {
				/* the current V_WEBIF_SPEC is not defined we need the string without spec_xx in spec filed. */
				if(!strstr(found[i].spec, "spec_")) {
					return ALREADY_FOUND; //prev found string is not include "spec_xx", keep it
				}
				else if(strstr(spec, "spec_")) {
					return SPEC_NOT_MATCH; //currect found string is not include "spec_xx", this is the one we needed
				}
				else {
					found[i].spec[0]=0;
					return SPEC_MATCH; // keep the first id we found with the spec_xx
				}
			}
			else {
				/* the current V_WEBIF_SPEC is defined, we will update the string if it matchs V_WEBIF_SPEC */
				if(strstr(found[i].spec, V_WEBIF_SPEC)) {
					return ALREADY_FOUND; //prev found string is already match, keep it
				}
				else if(strstr(spec, V_WEBIF_SPEC)==0) {
					return SPEC_NOT_MATCH; // keep it
				}
				else {
					strncpy( found[i].spec, V_WEBIF_SPEC, SPECSIZE);
					return SPEC_MATCH; // this is the one we needed
				}
			}
		}
	}
	return NOT_FOUND;
}

int optimise( void ) { // for the option -O: build the xml file only with used ids from html file
int i;
int j;
int find;
int found_id=0;
char *p_htmlbuf;
char *pos1=0, *pos2=0;
const char* quote="\")";
struct fields {
	char *id;
	char *spec;
	char *lang[ARRAY_SIZE];
} fields;

	for (i= 0; i< ENTRIES; i++) {
		list[i].id[0] = '\0';
		list[i].spec[0] = '\0';
		for(j=0; j<ARRAY_SIZE; j++) {
			list[i].lang[j][0] = '\0';
		}
	}

	htmlFileIn = fopen(p_fileDir, "r");
	if (!htmlFileIn) {
		fprintf(stderr, "failed to open the file %s\n", p_fileDir);
		return 0;
	}

	csvFileIn = fopen("./strings.csv", "r");
	if (!csvFileIn) {
		fprintf(stderr, "failed to open the file %s\n", strerror(errno));
		exit(1);
	}
	file_ind= 0;
	while ( fgets(htmlbuf, sizeof(htmlbuf), htmlFileIn) ) {
		//read one line from html file
		p_htmlbuf=htmlbuf;
		while(p_htmlbuf) {
			// scan each line from html file
			quote="\")";
			pos1 = strstr(p_htmlbuf, "_(\"");
			if(!pos1) {
				pos1 = strstr(p_htmlbuf, "_('");
				if(!pos1) {
					p_htmlbuf=0;
					continue;
				}
				quote="')";
			}
			pos1+=3;
			pos2 = strstr(pos1, quote);
			if(!pos2) {
				p_htmlbuf=0;
				continue;
			}
			p_htmlbuf=pos2+2;
			*pos2=0;
			if(*pos1==0) {
				continue;//skip empty id;
			}
			find=NOT_FOUND;
			found_id=0;
			fseek(csvFileIn, 0, SEEK_SET);
			/*********start scan csv file ************/
			while ( fgets(buffer, sizeof(buffer), csvFileIn) ) {
				if ( (strlen(buffer) < 5) || (buffer[0] == ';') ) {
					continue;
				}
				CheckAndAddSemicolons(buffer);
				fields.id=buffer; // first one is id

				char* ptr = index(buffer, ';');
				if (ptr == 0) {
					fprintf(stderr, "csv file error: %s\n", buffer);
					continue;
				}
				*ptr=0;
				if(strcmp(fields.id, pos1)==0) {
				// found matched id
					ptr++;
					fields.spec = ptr; // 2nd is spec
					char* ptr2 = index(ptr, ';');
					if (ptr2 == 0) {
						if( i != ARRAY_SIZE-1) {
							fprintf(stderr, "Error: spec=%s\n", fields.spec);
							continue;
						}
						break;
					}
					found_id++;
					*ptr2=0;
					find=check_found(fields.id, fields.spec);

					if( find == ALREADY_FOUND ) {
						break;
					}
					else if( find == SPEC_NOT_MATCH ) {
						continue; //keep searching
					}
					ptr=ptr2;
					for( i=0; i<ARRAY_SIZE; i++) {
						ptr++;
						fields.lang[i] = ptr;
						char* ptr2 = index(ptr, ';');
						if (ptr2 == 0) {
							if( i != ARRAY_SIZE-1 ) {
								fprintf(stderr, "Error: leng=%s\n", p_language_list[i]);
								continue;;
							}
							break;
						}
						*ptr2=0;
						ptr=ptr2;
					}
					if( find == NOT_FOUND ) {
						strncpy( found[found_ind].id, pos1, IDSIZE);
						strncpy( found[found_ind].spec, fields.spec, SPECSIZE);
						found[found_ind].ind=file_ind;
						found_ind++;
					}

					for( i=0; i<ARRAY_SIZE; i++) {
						CheckString(fields.lang[i]);
					}

					strncpy(list[file_ind].id, 	fields.id,	IDSIZE);
					strncpy(list[file_ind].spec,	fields.spec,	SPECSIZE);
					strncpy(list[file_ind].lang[EN],	fields.lang[EN],	STRSIZE);
	#if defined (V_LANGUAGE_AR_y)
					strncpy(list[file_ind].lang[AR],	fields.lang[AR],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_FR_y)
					strncpy(list[file_ind].lang[FR],	fields.lang[FR],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_DE_y)
					strncpy(list[file_ind].lang[DE],	fields.lang[DE],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_IT_y)
					strncpy(list[file_ind].lang[IT],	fields.lang[IT],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_ES_y)
					strncpy(list[file_ind].lang[ES],	fields.lang[ES],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_PT_y)
					strncpy(list[file_ind].lang[PT],	fields.lang[PT],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_CZ_y)
					strncpy(list[file_ind].lang[CZ],	fields.lang[CZ],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_NL_y)
					strncpy(list[file_ind].lang[NL],	fields.lang[NL],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_TW_y)
					strncpy(list[file_ind].lang[TW],	fields.lang[TW],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_CN_y)
					strncpy(list[file_ind].lang[CN],	fields.lang[CN],	STRSIZE);
	#endif
	#if defined (V_LANGUAGE_JP_y)
					strncpy(list[file_ind].lang[JP],	fields.lang[JP],	STRSIZE);
	#endif
					/* checking for matching spec*/
					if(strlen(V_WEBIF_SPEC)==0) {
						if(!strstr(fields.spec, "spec_"))
							break; //spec matched
					}
					else if( strstr(fields.spec, V_WEBIF_SPEC)) {
						break; //spec matched
					}
				}
			}
			if(found_id==0) {
				fprintf(stderr, "Can't find id=%s\n", pos1);
			}
			else if( find != ALREADY_FOUND ){
				file_ind ++;
			}
		}
	}
	fclose (htmlFileIn);

	pos1 = strrchr(p_fileDir,'.');
	sprintf(pos1,".xml");
	pos1 = strrchr(p_fileDir,'/');
	if(pos1)
		p_fileDir = ++pos1;

	DumpOptimisedStringsFor(p_fileDir, "en", EN);
#if defined (V_LANGUAGE_AR_y)
	DumpOptimisedStringsFor(p_fileDir, "ar", AR);
#endif
#if defined (V_LANGUAGE_FR_y)
	DumpOptimisedStringsFor(p_fileDir, "fr", FR);
#endif
#if defined (V_LANGUAGE_DE_y)
	DumpOptimisedStringsFor(p_fileDir, "de", DE);
#endif
#if defined (V_LANGUAGE_IT_y)
	DumpOptimisedStringsFor(p_fileDir, "it", IT);
#endif
#if defined (V_LANGUAGE_ES_y)
	DumpOptimisedStringsFor(p_fileDir, "es", ES);
#endif
#if defined (V_LANGUAGE_PT_y)
	DumpOptimisedStringsFor(p_fileDir, "pt", PT);
#endif
#if defined (V_LANGUAGE_CZ_y)
	DumpOptimisedStringsFor(p_fileDir, "cz", CZ);
#endif
#if defined (V_LANGUAGE_NL_y)
	DumpOptimisedStringsFor(p_fileDir, "nl", NL);
#endif
#if defined (V_LANGUAGE_TW_y)
	DumpOptimisedStringsFor(p_fileDir, "tw", TW);
#endif
#if defined (V_LANGUAGE_CN_y)
	DumpOptimisedStringsFor(p_fileDir, "cn", CN);
#endif
#if defined (V_LANGUAGE_JP_y)
	DumpOptimisedStringsFor(p_fileDir, "jp", JP);
#endif
	return 0;
}

int main( int argc, char** argv) {
	int opt;

	if (argc < 2) {
		usage();
	}

	while( ( opt = getopt( argc, argv, "B:O:VvHh?" ) ) != EOF ) {
		switch( opt ) {
			case 'B':
				if(!optarg) {
					usage();
				}
				p_language = optarg;
				build_xml();
			break;
			case 'O':
				if(!optarg) {
					usage();
				}
				p_fileDir =  optarg;//argv[optind];
				optimise();
			break;
			case 'v':
			case 'V':
				fprintf( stderr, "%s Version %s\n", argv[0], VERSION_INFO );
				exit(0);
			break;
			case 'h':
			case '?':
			default:
				usage();
			break;
		}
	}

	return 0;
}