/* mtd_statistics.c
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

static void display_help (void);
extern mtd_info_t meminfo;
extern int fd;
extern struct flash mtd_flash;
extern struct statistics mtd_statistics;
int ismtd3exsist;

#if defined(PLATFORM_Platypus)
void mtddb_to_rdb_create_sync(char *n, char *v) {
char buff[1024];
	sprintf( buff, "if [ -z \"`mtddb -l |grep %s`\" ]; then mtddb -c %s string; mtddb -s %s %s; rdb_set %s \"%s\"; else rdb_set %s \"`mtddb -g %s`\"; fi", n,n,n,v,n,v,n,n);
	system( buff );
}

void rdb_to_mtddb_sync(char *n) {
char v[512];
char buff[1024];
	rdb_get_single( n, v, sizeof(v) );
	if(*v)
		sprintf( buff, "mtddb -s %s \"%s\"", n,v);
	else
		sprintf( buff, "mtddb -s %s", n);
	system( buff );
}
#endif

static const struct option long_opts[] = {
	{ "help", 0, 0, 'h' },
	{ 0, 0, 0, 0 }
};

int main (int argc, char *argv[])
{
	int opt, error=0;
	int i;
	FILE *fp;
#ifdef PLATFORM_Platypus
	u_int32_t currentTime;
	char buf[256];
#endif
	if (rdb_open_db() < 0) {
		perror("Opening database");
		exit(-1);
	}
//	syslog(LOG_INFO, " Start...");
#ifdef PLATFORM_Platypus
	ismtd3exsist = 0;
#else
	if ( init_mtd() < 0)
		ismtd3exsist = 0;
	else
		ismtd3exsist = 1;
#endif
//printf("LOGFILE_DIR=%s\n",LOGFILE_DIR);
	if((opt=getopt_long(argc,argv,"h r s p e d:", long_opts, NULL)) != -1)
	{
		switch (opt) {
			case 0:
			case 'h':
				display_help();
				break;
			case 'r':
#if defined(PLATFORM_Platypus)
				currentTime = (u_int32_t)time(NULL);
				sprintf( buf, "%lu", currentTime);
				mtddb_to_rdb_create_sync("statistics.start_date", buf);
				*buf=0;
				mtddb_to_rdb_create_sync("statistics.usage_total", buf);
				mtddb_to_rdb_create_sync("statistics.usage_current", buf);
				mtddb_to_rdb_create_sync("statistics.usage_history", buf);
				mtddb_to_rdb_create_sync("statistics.usage.3GVoice.incoming_call_count", buf);
				mtddb_to_rdb_create_sync("statistics.usage.3GVoice.outgoing_call_count", buf);
#else
				if(ismtd3exsist) {
					mtd_restore();
					syslog(LOG_INFO, "Restore statistics data from mtd");
				}
				else {
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
				}
#endif
				break;
			case 's':
#if defined(PLATFORM_Platypus)
				rdb_to_mtddb_sync("statistics.usage_total");
				rdb_to_mtddb_sync("statistics.usage_current");
				rdb_to_mtddb_sync("statistics.usage_history");
				rdb_to_mtddb_sync("statistics.usage.3GVoice.incoming_call_count");
				rdb_to_mtddb_sync("statistics.usage.3GVoice.outgoing_call_count");
#else
				if(ismtd3exsist) {
					mtd_store();
				}
				else {
					if ((fp = fopen(LOGFILE_DIR"usage.log.new", "w")) == NULL) {
						syslog(LOG_ERR,"Can not open "LOGFILE_DIR"usage.log.new\n");
					}
					else {
						char buf[1024];
						rdb_get_single("statistics.usage_total", buf, sizeof(buf) );
						fprintf(fp, "statistics.usage_total;%s\n", buf);
						rdb_get_single("statistics.usage_current", buf, sizeof(buf) );
						fprintf(fp, "statistics.usage_current;%s\n", buf);
						rdb_get_single("statistics.usage_history", buf, sizeof(buf) );
						fprintf(fp, "statistics.usage_history;%s\n", buf);
						fprintf(fp, "statistics.usage_history_limit;%u\n", HISTORY_LIMIT);
						//fflush(fp);
						fclose(fp);
						sync();
						system( "mv -f "LOGFILE_DIR"usage.log.new "LOGFILE_DIR"usage.log" );
					}
				}
#endif
				break;
			case 'p':
#if defined(PLATFORM_Platypus)
				system( "mtddb -l" );
#else
				if(ismtd3exsist) {
					mtd_print();
				}
				else {
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
				}
#endif
				break;
			case 'e':
				rdb_update_single( "statistics.usage_current", "", STATISTICS, DEFAULT_PERM,0,0);
				rdb_update_single( "statistics.usage_total", "", NONBLOCK, DEFAULT_PERM,0,0);
				rdb_update_single( "statistics.usage_history", "", NONBLOCK, DEFAULT_PERM,0,0);
				rdb_update_single( "statistics.usage.3GVoice.incoming_call_count", "", NONBLOCK, DEFAULT_PERM,0,0);
				rdb_update_single( "statistics.usage.3GVoice.outgoing_call_count", "", NONBLOCK, DEFAULT_PERM,0,0);
#if defined(PLATFORM_Platypus)
				currentTime = (u_int32_t)time(NULL);
				sprintf( buf, "%lu", currentTime);
				rdb_update_single("statistics.start_date", buf, NONBLOCK, DEFAULT_PERM,0,0);
				rdb_to_mtddb_sync("statistics.start_date");
				system( "mtddb -u statistics.usage_current" );
				system( "mtddb -u statistics.usage_total" );
				system( "mtddb -u statistics.usage_history" );
				system( "mtddb -u statistics.usage.3GVoice.incoming_call_count" );
				system( "mtddb -u statistics.usage.3GVoice.outgoing_call_count" );
#else
				if(!ismtd3exsist) {
					//printf ("mtd3 not existing\n");
					system( "rm -f "LOGFILE_DIR"usage.log" );
					break;
				}
				for( i=0; i<meminfo.size/MTD_PAGE_SIZE; i++) {
					if( erase_flash( i*MTD_PAGE_SIZE )<0 )
						break;
					printf ("Erased %u bytes from address 0x%.8x in flash\n", MTD_PAGE_SIZE, i*MTD_PAGE_SIZE);
				}
#endif
				break;
			case 'd':
				if(!ismtd3exsist) {
					printf ("mtd3 not existing\n");
					break;
				}
				if( atoi(argv[2])>=0 && atoi(argv[2])<meminfo.size/MTD_PAGE_SIZE*STATISTICS_DATA_PRE_PAGE ) {
					read_flash(STATISTICS_DATA_SIZE*atoi(argv[2]) );
					dumpBuf( (void*)&mtd_flash );
				}
				else
					error = 1;
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


void display_help (void)
{
	char info[128];

	if(ismtd3exsist) {
		sprintf(info,"mtd3 total size: %u\n"
		"page size: %u\n"
		"statistics data size: %u\n"
		"statistics data per page: %u\n", meminfo.size, meminfo.erasesize, STATISTICS_DATA_SIZE, STATISTICS_DATA_PRE_PAGE);

		printf(	"%s\n"
			"mtd_statistics [OPTION]\n"
			"  -r, --Restore statistics data from MTD/File to database\n"
			"  -s, --Store current statistics data to MTD/File\n"
			"  -p  --Print current stored statistics data from MTD/File\n"
			"  -h  --Display this help\n"
			"  -e  --Reset usages (Erase mtd3 memory pages)\n"
			"  -d <0 - %u> --Dump statistics data from MTD memory\n"
			, info, (meminfo.size/STATISTICS_DATA_SIZE)-1);
	}
	else {
#ifdef PLATFORM_Platypus
		sprintf(info, "use mtddb to log the usage\n");
#else
		sprintf(info, "mtd3 NOT existing use "LOGFILE_DIR"usage.log\n");
#endif
		printf(	"%s\n"
			"mtd_statistics [OPTION]\n"
			"  -r, --Restore statistics data from MTD/File to database\n"
			"  -s, --Store current statistics data to MTD/File\n"
			"  -p  --Print current stored statistics data from MTD/File\n"
			"  -e  --Reset usages\n"
			"  -h  --Display this help\n"
			, info);
	}
}


