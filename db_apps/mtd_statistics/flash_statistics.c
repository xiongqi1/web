/* flash_statistics.c -- read/write statistics data from MTD

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

#include "mtd_statistics.h"

mtd_info_t meminfo;
int fd;
struct flash mtd_flash;
struct statistics *mtd_statistics;

int erase_flash( u_int32_t offset )
{
	struct erase_info_user erase;
	erase.start = offset;
	erase.length = meminfo.erasesize;
	if( ioctl (fd,MEMERASE, &erase) < 0)
	{
		fprintf(stderr, "erase ERROR\n" );
		return (-1);
	}
	return (0);
}

int write_flash(u_int32_t offset )
{
	if(offset != lseek (fd,offset,SEEK_SET))
	{
		fprintf(stderr, "lseek ERROR\n" );
		return (-1);
	}
	if( write (fd, (void*)&mtd_flash, STATISTICS_DATA_SIZE) < 0)
	{
		fprintf (stderr, "write ERROR\n");
		return (-1);
	}
	return (0);
}

int read_flash( u_int32_t offset )
{
	if (offset != lseek (fd,offset,SEEK_SET))
	{
		fprintf(stderr, "lseek ERROR\n" );
		return (-1);
	}
	if (read (fd, (void*)&mtd_flash, STATISTICS_DATA_SIZE) < 0)
	{
		fprintf (stderr, "read ERROR %x --- %x\n",offset, STATISTICS_DATA_SIZE);
		return (-1);
	}
	return (0);
}

int statistics_put( int num )
{
u_int32_t myOffset;

	if( num >= meminfo.size/STATISTICS_DATA_SIZE )
		return -1;
	myOffset = STATISTICS_DATA_SIZE * num;
	if( (num % STATISTICS_DATA_PRE_PAGE) == 0 )
		erase_flash( myOffset );
	return write_flash( myOffset );
}

int statistics_get( int num )
{
u_int32_t myOffset;

	if( num >= meminfo.size/STATISTICS_DATA_SIZE )
		return -1;
	myOffset = STATISTICS_DATA_SIZE * num;

	return read_flash( myOffset );
}

u_int32_t checksum()
{
	int i;
	u_int32_t cksum=0;
	u_int32_t *pos = (u_int32_t *)&mtd_flash;

	for( i=0; i<(STATISTICS_DATA_SIZE/4-1); i++ )
	{
		cksum += *pos;
	}
//fprintf (stderr, "\n\ncksum= %x --- %x\n",cksum, mtd_flash.checksum);
	return cksum;
}

int find_current()
{
int i, j=-1;
int max_sequence=0;
u_int32_t offset;

	for( i = meminfo.size/STATISTICS_DATA_SIZE-1; i>=0; i--)
	{

		offset = i*STATISTICS_DATA_SIZE;
		if (offset != lseek (fd, offset, SEEK_SET))
		{
			fprintf(stderr, "lseek ERROR\n" );
			return (-1);
		}
		if (read(fd, (void*)(&(mtd_flash.sequence)), sizeof(mtd_flash.sequence)) < 0)
		{
			fprintf (stderr, "read ERROR %x --- %x\n",offset, sizeof(mtd_flash.sequence));
			return (-1);
		}
		if( mtd_flash.sequence!=-1 && mtd_flash.sequence>=max_sequence ) //warning: comparison is always true
		{
			if ( read_flash(offset) < 0)
			{
				return (-1);
			}
			if( mtd_flash.checksum == checksum() )
			{
				j=i;
				max_sequence=mtd_flash.sequence;
			}
		}
	}
	mtd_flash.sequence = (max_sequence<0) ? 0:max_sequence;
	return j;
}

char *splitUsage( char *str, struct usage *mydata )
{
char * pch;

	pch = strtok (str,  " ,"); 
	if( pch )
		mydata->StartTime=atol(pch);
	else
		return 0;
	pch = strtok (NULL, " ,");
	if( pch )
		mydata->endTime=atol(pch);
	else
		return 0;
	pch = strtok (NULL, " ,");

	if( pch )
		mydata->DataReceived=atol(pch);
	else
		return 0;
	pch = strtok (NULL, " ,");
	if( pch )
		mydata->DataSent=atol(pch);
	else
		return 0;
	return pch;
}

int splitHistory( char *str, struct usage *mydata )
{
	char *mypch=str;
	char *pos;
	int i;

	for(i=0; i<HISTORY_LIMIT && *mypch; i++)
	{
		pos = strchr(mypch, '&');
		if(pos) *pos = 0;
		if( splitUsage( mypch, &(mtd_statistics->usage_history[i]))==0 )
			break;
		mypch = ++pos;
	}
	return i;
}


/*
statistics.usage_total	//StartTime, currentTime, totalDataReceived, totalDataSent.
statistics.usage_current	//StartTime, currentTime, currentReceived, currentDataSent.
statistics.usage_history	//SessionStartTime, SessionEndTime, DataReceived, DataSent& ..... ,
rdb_set statistics.usage_history_limit HISTORY_LIMIT */

int mtd_store()
{
	
unsigned int current = find_current()+1;
char buf[256];

	current = ( current<meminfo.size/STATISTICS_DATA_SIZE && (int)current>0)? current:0;
	mtd_flash.sequence++;
	rdb_get_single("statistics.usage_total", buf, sizeof(buf) );
	if( strlen(buf)<26 || splitUsage( buf, &(mtd_statistics->usage_total) )==0)
		mtd_statistics->usage_total.StartTime=0;
	//	fprintf(stderr, "statistics.usage_total format error\n");
	rdb_get_single("statistics.usage_current", buf, sizeof(buf) );
	if( (strstr(buf, "down")) || strlen(buf)<26 || splitUsage( buf, &(mtd_statistics->usage_current))==0 )
	{
		mtd_statistics->usage_current.StartTime=0;
	}
	rdb_get_single("statistics.usage_history", buf, sizeof(buf) );
	if( strlen(buf)>26 )
		splitHistory( buf, &(mtd_statistics->usage_history[0]) );
	mtd_flash.checksum = checksum();
//fprintf(stderr, "Store: location=%d sequence=%d checksum=%x\n", current, mtd_flash.sequence, mtd_flash.checksum);
	return statistics_put(current);
}

int mtd_to_str( char *mybuf, struct usage *mydata)
{
	if( mydata->StartTime == 0 )
		return 0;
	sprintf( mybuf, "%u,%u,%u,%u", mydata->StartTime, mydata->endTime, mydata->DataReceived, mydata->DataSent );
	return 1;
}

int mtd_restore()
{
int i;
int current = find_current();
char buf[64];
char history[256]={"\0"};

	if( current<0 )
	{
		fprintf(stderr, "Statistics data not find\n");
		return -1;
	}
//fprintf(stderr, "Retore: location=%d sequence=%d checksum=%x\n", current, mtd_flash.sequence, mtd_flash.checksum);
	if (statistics_get(current)<0)
		return -1;
	mtd_to_str( buf, &(mtd_statistics->usage_total));
	//set_single("statistics.usage_total", buf, 0 );
	rdb_update_single("statistics.usage_total", buf, NONBLOCK, DEFAULT_PERM,0,0);
	if(mtd_statistics->usage_current.StartTime==0)
	{
		//set_single("statistics.usage_current", "ppp down", 0 );
		rdb_update_single("statistics.usage_current", "ppp down", NONBLOCK, DEFAULT_PERM,0,0);
	}
	else
	{
		mtd_to_str( buf, &(mtd_statistics->usage_current));
		//set_single("statistics.usage_current", buf, 0 );
		rdb_update_single("statistics.usage_current", buf, NONBLOCK, DEFAULT_PERM,0,0);
	}
	for(i=0; i<HISTORY_LIMIT; i++){
		if( mtd_to_str( buf, &(mtd_statistics->usage_history[i]))==0)
			break;
		if( *history )
			strcat(history, "&");
		strcat(history, buf);
	}
	//set_single("statistics.usage_history", history, 0 );
	rdb_update_single("statistics.usage_history", history, NONBLOCK, DEFAULT_PERM,0,0);
	return 0;
}

int mtd_print()
{
int i;
int current = find_current();
char buf[64];
char history[256]={"\0"};

	if( current<0 )
	{
		printf( "Statistics data not find\n");
		return -1;
	}
	if (statistics_get(current)<0)
		return -1;
	printf( "mtd location:%d   sequence number:%d\n", current, mtd_flash.sequence);
	mtd_to_str( buf, &(mtd_statistics->usage_total));
	printf( "usage_total:%s\n", buf);
	if(mtd_statistics->usage_current.StartTime==0)
	{
		printf( "usage_current: ppp down\n");
	}
	else
	{
		mtd_to_str( buf, &(mtd_statistics->usage_current));
		printf( "usage_current:%s\n", buf);
	}
	for(i=0; i<HISTORY_LIMIT; i++){
		if( mtd_to_str( buf, &(mtd_statistics->usage_history[i]))==0)
			break;
		if( *history )
			strcat(history, "&");
		strcat(history, buf);
	}
	printf( "usage_history:%s\n",history);
	return 0;
}

int init_mtd(void)
{
	// use cat /proc/mtd 
	if ((fd = open("/dev/mtd/3", O_RDWR)) < 0) {
		//fprintf(stderr, "Can not open MTD driver %s\n", strerror(errno));
		return -1;
	}
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		fprintf(stderr, "unable to get MTD device info\n" );
		return -1;
	}
//fprintf(stderr, "erase.length=%u\nmeminfo.type=%u\nmeminfo.size=%u\n", meminfo.erasesize, meminfo.type, meminfo.size);
	if( meminfo.erasesize != MTD_PAGE_SIZE ) {
		fprintf(stderr, "incorrect page size: %u --- %u\n", meminfo.erasesize, MTD_PAGE_SIZE);
		return -1;
	}
	mtd_flash.sequence = 1;
	mtd_statistics = (void*)mtd_flash.data;
	return 0;
}

#define PRCHAR(x) (((x)>=0x20&&(x)<=0x7e)?(x):'.')
void dumpBuf(char *buf) {
	unsigned int j, i;
	for (j = 0; j<STATISTICS_DATA_SIZE; j += 8) {
		fprintf(stderr, "%4.4x ", j);
		for (i=j; i<STATISTICS_DATA_SIZE && i<j+8; i++)
			fprintf(stderr, "%2.2x ", buf[i]);
		fprintf(stderr, "  ");
		for (i=j; i<STATISTICS_DATA_SIZE && i<j+8; i++)
			fprintf(stderr, "%c", PRCHAR(buf[i]));
		fprintf(stderr, "\n\r");
	}
}


