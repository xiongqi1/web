/*!
 * Copyright Notice:
 * Copyright (C) 2002-2010 Call Direct Cellular Solutions

 */
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/msg.h>
#include <ctype.h>
#include "string.h"
#include <sys/ioctl.h>
#include <signal.h>
#include <stdint.h>
#include <fcntl.h>
#include "rdb_util.h"

#ifdef PLATFORM_AVIAN
	union {
		char inbuffer[52];
		struct {
			char zone[48];
			char offset[4];
		}data;
	}timezone;
	uint32_t t_offset;
	#define IDXFILE "/system/usr/share/zoneinfo/zoneinfo.idx"
#else
	char buf[256];
	#define IDXFILE "/usr/zoneinfo.ref"
	char value[64];
	char *file_location, *tz, *dst, *name_en, *name_ar, *name_fr, *pos;
	char lang[3];
	int fd;
#endif

static void GetTimeZoneList(char *query) {
	FILE *fp = fopen(IDXFILE, "r");
	int i=0;
	if(!fp) {
		printf("var errstr='open "IDXFILE" error';");
		return;
	}
	else {
#ifdef PLATFORM_AVIAN
		printf( "var st=[" );
		for( n=fread(timezone.inbuffer, 1, 52, fp); n==52; n=fread(timezone.inbuffer, 1, 52, fp)) {
			if(i) printf(",");
			t_offset = (uint32_t)timezone.data.offset[0]*256*256*256+(uint32_t)timezone.data.offset[1]*256*256+(uint32_t)timezone.data.offset[2]*256+(uint32_t)timezone.data.offset[3];
			printf("{\n");
			printf("\"TZ\":\"%s\",\"offset\":\"%d\"", timezone.data.zone, t_offset);
			printf("}");
			i++;
		}
		printf( "];\n");
		fclose(fp);
#else
		if(rdb_start()) {
			printf("var errstr=\"can't open cdcs_DD");
			return;
		}
		pos=get_single_raw("webinterface.language");
		if(*pos)
			strncpy(lang, pos, 2);
		else
			strcpy(lang, "en");
		close(fd);
		printf( "var zoneinfo=[" );
		while((file_location=fgets(buf, sizeof(buf), fp)) != NULL) {
			tz=strchr(file_location, ';');
			if(tz) {
				*tz++=0;
				dst=strchr(tz, ';');
				if(dst) {
					*dst++=0; //DST
					name_en=strchr(dst, ';');
					if(name_en) {
						*name_en++=0; //NAME en
						name_ar=strchr(name_en, ';');
						if(name_ar) {
							*name_ar++=0; //Name ar
							name_fr=strchr(name_ar, ';');
							if(name_fr) {
								*name_fr++=0; //Name fr
								pos=strchr(name_fr, ';');
								if(pos)
									*pos=0;
							}
						}
					}
				}
			}
			if(!file_location || !tz || !dst || !name_en || !name_ar || !name_fr)
				break;
			if(!strcmp(lang, "ar"))
				pos=name_ar;
			else if(!strcmp(lang, "fr"))
				pos=name_fr;
			else
				pos=name_en;
			if(i)
				printf(",{");
			else
				printf("{\n");
			printf("\"FL\":\"%s\",\"TZ\":\"%s\",\"NAME\":\"%s\",\"DST\":\"%s\"",file_location, tz, pos, dst);
			printf("}");
			i++;
		}
		printf( "];\n");
		fclose(fp);
#endif
	}
}

int main(int argc, char* argv[], char* envp[]) {
char *query;
	printf("Content-Type: text/html; charset=utf-8 \r\n\r\n");
	query = getenv("QUERY_STRING");
	if (!query) {
		query="\0";
	}
	GetTimeZoneList( query );
	exit(0);
}

