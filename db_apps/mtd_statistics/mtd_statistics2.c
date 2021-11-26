/* mtd_statistics2.c
*  Support multiple PDP sessions. (for Bovine Platform)
*/
#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <libgen.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <syslog.h>
#include "mtd_statistics.h"

#define MAX_SESSIONS 6

static void display_help (void);
extern mtd_info_t meminfo;
extern int fd;
extern struct flash mtd_flash;
extern struct statistics mtd_statistics;

static const struct option long_opts[] = {
	{ "help", 0, 0, 'h' },
	{ 0, 0, 0, 0 }
};

int main (int argc, char *argv[]) {
	int opt, error=0;
	int i;
	char cmd[256];
	FILE *fp;

	if (rdb_open_db() < 0) {
		perror("Opening database");
		exit(-1);
	}
//	syslog(LOG_INFO, " Start...");

//printf("LOGFILE_DIR=%s\n",LOGFILE_DIR);
	if((opt=getopt_long(argc,argv,"h r s p e d:", long_opts, NULL)) != -1) {
		switch (opt) {
			case 0:
			case 'h':
				display_help();
				break;
			case 'r':
				fp = fopen( LOGFILE_DIR"usage.log", "r");
				if (fp == NULL) {
					syslog(LOG_ERR,"Can not open "LOGFILE_DIR"usage.log");
				}
				else {
					char buff[1024];
					char *pos;
					while( fgets(buff, sizeof(buff), fp) ) {
						pos=strchr(buff, ';');
						if( !pos ) {
							continue;
							//printf(LOGFILE_DIR"/usage.log data format error.\n");
							//break;
						}
						*pos=0;
						if(iscntrl(*(pos+1))) {
							*(pos+1)=0;
						}
						if(strstr(buff, "usage_current")>0) {
							char buf[1024];
							rdb_get_single(buff, buf, sizeof(buf) );
							if(*buf==0 || atoi(buf)==0) {
								rdb_update_single(buff, pos+1, STATISTICS, DEFAULT_PERM,0,0);
							}
						}
						else {
							rdb_update_single(buff, pos+1, NONBLOCK, DEFAULT_PERM,0,0);
						}
					}
					fclose(fp);
					syslog(LOG_INFO, "Restore statistics data from "LOGFILE_DIR"usage.log");
				}
				break;
			case 's':
				if ((fp = fopen(LOGFILE_DIR"usage.log.new", "w")) == NULL) {
					syslog(LOG_ERR,"Can not open "LOGFILE_DIR"usage.log.new\n");
				}
				else {
					char buf[1024];
					for(i=1; i<=MAX_SESSIONS; i++) {
						sprintf(cmd, "link.profile.%u.usage_total", i);
						if(rdb_get_single(cmd, buf, sizeof(buf))<0) {
							*buf=0;
						}
						fprintf(fp, "%s;%s\n",cmd, buf);

						sprintf(cmd, "link.profile.%u.usage_current", i);
						if(rdb_get_single(cmd, buf, sizeof(buf))<0) {
							*buf=0;
						}
						fprintf(fp, "%s;%s\n",cmd, buf);

						sprintf(cmd, "link.profile.%u.usage_history", i);
						if(rdb_get_single(cmd, buf, sizeof(buf))<0) {
							*buf=0;
						}
						fprintf(fp, "%s;%s\n",cmd, buf);
					}
					fprintf(fp, "statistics.usage_history_limit;%u\n", HISTORY_LIMIT);
					//fflush(fp);
					fclose(fp);
					sync();
					system( "mv -f "LOGFILE_DIR"usage.log.new "LOGFILE_DIR"usage.log" );
				}
				break;
			case 'p':
				printf ("Print "LOGFILE_DIR"usage.log ...\n");
				if ((fp = fopen(LOGFILE_DIR"usage.log", "r")) == NULL) {
					syslog(LOG_ERR,"Can not open "LOGFILE_DIR"usage.log\n");
				}
				else {
					char buff[256];
					while( fgets(buff, sizeof(buff), fp) ) {
						printf ("%s", buff);
					}
					fclose(fp);
				}
				break;
			case 'e':
				for(i=1; i<=MAX_SESSIONS; i++) {
					sprintf(cmd, "link.profile.%u.usage_total", i);
					rdb_update_single( cmd, "", NONBLOCK, DEFAULT_PERM,0,0);
					sprintf(cmd, "link.profile.%u.usage_current", i);
					rdb_update_single( cmd, "", STATISTICS, DEFAULT_PERM,0,0);
					sprintf(cmd, "link.profile.%u.usage_history", i);
					rdb_update_single( cmd, "", NONBLOCK, DEFAULT_PERM,0,0);
				}
				rdb_update_single( "statistics.usage.3GVoice.incoming_call_count", "", NONBLOCK, DEFAULT_PERM,0,0);
				rdb_update_single( "statistics.usage.3GVoice.outgoing_call_count", "", NONBLOCK, DEFAULT_PERM,0,0);

				system( "rm -f "LOGFILE_DIR"usage.log" );
				break;
			default:
				error = 1;
		}
	}
	if (error) {
		printf( "Try -h for more information.\n" );
		exit(1);
	}
	return 0;
}

void display_help (void) {
	char info[128];

	sprintf(info, "Usage log file: "LOGFILE_DIR"usage.log\n");
	printf(	"%s\n"
		"mtd_statistics [OPTION]\n"
		"  -r, --Restore statistics data from log file to database\n"
		"  -s, --Store current statistics data to the log file\n"
		"  -p  --Print current stored statistics data from the log file\n"
		"  -e  --Reset usages\n"
		"  -h  --Display this help\n"
		, info);
}
