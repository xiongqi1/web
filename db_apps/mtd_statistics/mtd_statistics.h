/* mtd_statistics.h

 */

////#include "crc32.h"

/* HACK: using GCC major version to detect 2.4 kernel series */
#if __GNUC__==2
	#include <linux/mtd/mtd.h>
#elif defined(PLATFORM_Platypus) || defined(PLATFORM_Platypus2)
	#include <linux/mtd/mtd.h>
#else
	#include <mtd/mtd-user.h>
#endif

#include  "rdb_ops.h"

#define PROGRAM "flash_statistics"
#define VERSION "$Revision: 1.1 $"

#define MTD_PAGE_SIZE	1056
#define STATISTICS_DATA_PRE_PAGE	2
#define STATISTICS_DATA_SIZE (MTD_PAGE_SIZE/STATISTICS_DATA_PRE_PAGE)

#ifdef PLATFORM_Avian
	#define LOGFILE_DIR "/data/cdcs/"
#elif defined PLATFORM_Platypus2
	#define LOGFILE_DIR "/etc/cdcs/"
#elif defined PLATFORM_Serpent
	#define LOGFILE_DIR "/usr/local/cdcs/conf/"
#else
	#define LOGFILE_DIR "/usr/local/"
#endif

//#define DATA_USAGE_DBG	1
#ifdef DATA_USAGE_DBG
#define P(fmt, ...) SYSLOG_ERR(fmt, ##__VA_ARGS__)
#else
#define P(fmt, ...)
#endif

/* to prevent overflow of data usage variable
 * 0xFFFFFFFFFFFF0000 = 18,446,744,073,709,486,080 bytes =
 *  18,446,744 TB = 18,446 PB (Peta)
 */
#define MAX_USAGE_LIMIT		0xFFFFFFFFFFFF0000ULL

struct flash
{
	u_int16_t magic;
	u_int16_t sequence;
	char data[STATISTICS_DATA_SIZE-2*4];
	u_int32_t checksum;
};

/*
# statistics.usage_total	//StartTime, currentTime, totalDataReceived, totalDataSent.
# statistics.usage_current	//StartTime, currentTime, currentReceived, currentDataSent.
# statistics.usage_history	//SessionStartTime, SessionEndTime, DataReceived, DataSent& ..... ,
# rdb_set statistics.usage_history_limit HISTORY_LIMIT 
*/
#ifdef V_MAX_USAGE_HISTORY_10
	#define HISTORY_LIMIT 10
#else
	#define HISTORY_LIMIT 5
#endif

struct usage{
	u_int32_t StartTime;
	u_int32_t endTime; //currentTime
	u_int32_t StartTimeMonotonic;
	u_int32_t endTimeMonotonic;
	u_int64_t DataReceived;
	u_int64_t DataSent;
	u_int64_t DataPacketsReceived;
	u_int64_t DataPacketsSent;
	u_int64_t DataErrorsReceived;
	u_int64_t DataErrorsSent;
	u_int64_t DataDiscardPacketsReceived;
	u_int64_t DataDiscardPacketsSent;
	long      Start_SysUpTime;
};

struct statistics
{
	struct usage usage_total;
	struct usage usage_current;
	struct usage usage_history[HISTORY_LIMIT];
};

int erase_flash( u_int32_t offset );
int write_flash( u_int32_t offset );
int read_flash( u_int32_t offset );

int statistics_put( int num );
int statistics_get( int num );

int mtd_store(void);
int mtd_restore(void);
int mtd_print();
int find_current(void);
int init_mtd(void);
void dumpBuf(char *buf);
//int set_single( char *name, char *value, int persist );
//int get_single(char *name, char *value, int len);
