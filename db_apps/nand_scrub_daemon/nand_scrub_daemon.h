/*
 * NAND scrub daemon common definitions.
 *
 * Copyright Notice:
 * Copyright (C) 2017 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in
 * any form (including but not limited to printed or electronic forms
 * and binary or object forms) without the expressed written consent
 * of NetComm Wireless Pty. Ltd Copyright laws and International
 * Treaties protect the contents of this file.  Unauthorized use is
 * prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef NAND_SCRUB_DAEMON_H_13000013012017
#define NAND_SCRUB_DAEMON_H_13000013012017

#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

extern bool debug_enable;
extern int log_level;

#define DEBUG
#ifdef DEBUG
#define NDEBUG
#endif

/* Log a debug message */
#define dbgp(x, ...) \
    if (debug_enable) { \
        syslog(log_level, "%s:%d: " x, __func__, __LINE__, ##__VA_ARGS__); \
    }

/* Log an info msg */
#define infop(x, ...) \
    syslog(LOG_NOTICE, "%s:%d: " x, __func__,__LINE__, ##__VA_ARGS__)

/* Log an error message */
#define errp(x, ...) \
    syslog(LOG_ERR, "%s:%d: " x, __func__, __LINE__, ##__VA_ARGS__)

#define RUN_CHK(op, msg)                                                \
    do {                                                                \
        int rval = op;                                                  \
        if (rval) {                                                     \
            syslog(LOG_ERR, "%s:%d: " msg, __func__, __LINE__);         \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
    } while (0);

/* RDB variable names for the scrub daemon */
#define RDB_SCRUB_PREFIX "service.scrub"
#define RDB_SCRUB_CONF "conf"
#define RDB_SCRUB_CONF_START_DELAY RDB_SCRUB_PREFIX"."RDB_SCRUB_CONF".startdelay"
#define RDB_SCRUB_CONF_TYPE RDB_SCRUB_CONF".type"
#define RDB_SCRUB_CONF_ENABLE RDB_SCRUB_CONF".enable"
#define RDB_SCRUB_CONF_INTERVAL RDB_SCRUB_CONF".interval"
#define RDB_SCRUB_CONF_THRESH RDB_SCRUB_CONF".threshold"
#define RDB_SCRUB_CONF_NEXT RDB_SCRUB_CONF".next"
#define RDB_SCRUB_STAT "stat"
#define RDB_SCRUB_STAT_TS RDB_SCRUB_STAT".timestamp"
#define RDB_SCRUB_STAT_MAXERR RDB_SCRUB_STAT".maxerr"
#define RDB_SCRUB_STAT_AVGERR RDB_SCRUB_STAT".avgerr"
#define RDB_SCRUB_STAT_CNT RDB_SCRUB_STAT".count"
#define RDB_SCRUB_STAT_TOTAL RDB_SCRUB_STAT".total"
#define RDB_SCRUB_STAT_SCANS RDB_SCRUB_STAT".scans"

/* Stringify a literal int value */
#define xstr(literal_int) str(literal_int)
#define str(literal_int) #literal_int

#define MAX_RDB_NAME_LEN 64
#define MAX_RDB_VAR_LEN 64
#define MAX_NUM_DEV 100
#define MAX_NUM_DEV_STR_LEN strlen(xstr(MAX_NUM_DEV))

/* Default number of seconds to wait before doing a full scan after start up */
#define STARTUP_SCAN_DEFAULT_DELAY_SECS (60 * 5)

/*
 * MTD partition types.
 *
 * To support a new partition type a new entry should be added to this
 * enum before the MTD_PART_OTHER entry. Each entry corresponds to a
 * partition type that the scrubber supports except for the last two
 * entries which have the meaning:
 *  MTD_PART_OTHER: Partition type not configured and unable to be detected.
 *                  Such a partition will still be scanned but never scrubbed.
 *  MTD_PART_AUTO: Partition type not configured or configured to be auto
 *                 detected. This value is only used during initialisation.
 */
enum mtd_partition_type {
    MTD_PART_UBI,
    MTD_PART_QMI,

    /*
     * Do not add any entries below this line. New partition type support
     * should add an entry above.
     */
    MTD_PART_OTHER,
    MTD_PART_AUTO
};

/* Forward declaration */
struct nsd_mtd_info;

/* Function prototypes for partition plugins */
typedef void (*partition_scrub_func_t)(struct nsd_mtd_info *mtd_info,
                                       unsigned blk_num);
typedef bool (*partition_detect_func_t)(struct nsd_mtd_info *mtd_info);
typedef void (*partition_free_func_t)(void);
typedef void (*scan_start_func_t)(struct nsd_mtd_info *mtd_info_list);
typedef void (*scan_end_func_t)(struct nsd_mtd_info *mtd_info_list);

/*
 * Partition modules need to provide an entry of this type in the
 * global partition_conf array.
 */
struct partition_plugin {
    /*
     * String representation of the partition type. Must match rdb
     * service.scrub.X.type.
     */
    const char *type_name;

    /*
     * This function will be called when a scheduled scan cycle starts.
     * Implementation of this function is optional and plugins can provide NULL.
     */
    scan_start_func_t scan_start;

    /*
     * This function will be called when a scheduled scan cycle ends.
     * Implementation of this function is optional and plugins can provide NULL.
     */
    scan_end_func_t scan_end;

    /*
     * Function to detect whether an MTD dev uses the partition type.
     * Implementation of this function is optional and plugins can provide NULL.
     * If not provided then it means autodetection of this partition type is
     * not supported and that they type will either come from the rdb config
     * or be marked as the generic MTD_PART_OTHER type.
     */
    partition_detect_func_t partition_detect;

    /*
     * Scrubber function called to recover a block of this parition type.
     * Implementation of this function is optional and plugins can provide NULL.
     * If not provided the partition will still be scanned (if enabled) but no
     * recovery will be attempted upon high bit error detection.
     */
    partition_scrub_func_t scrubber;

    /*
     * Partition resource clean up.
     * Implementation of this function is optional and plugins can provide NULL.
     */
    partition_free_func_t free;
};

/* MTD device info/config used by the NAND scrubber */
struct nsd_mtd_info {
    enum mtd_partition_type partition_type;

    /* File descriptor for opened device file */
    int fd;

    /* MTD device number */
    unsigned dev_num;

    /* Size of one block */
    uint64_t block_size;

    /* Num blocks in the device */
    uint64_t num_blocks;

    /* Size of one page */
	uint64_t page_size;

    /* Total device size */
	uint64_t dev_size;

    /* Enable scanning of this device */
    unsigned enable;

    /*
     * Interval between scans. By default this is in hours. Units can
     * be changed for testing via the -m multiplier cmd line option.
     */
    unsigned interval;

    /*
     * A block needs to be scrubbed if any of its pages has a number of
     * bit errors with this threshold value or higher.
     */
    unsigned threshold;

    /* Next time that scan for this device should be run */
    time_t next_run;

    /* buffer for data reads */
    char *data_buf;
    unsigned data_buf_size;

    struct nsd_mtd_info *next;
};

/* Scan schedule queue node */
struct nsd_sched_item {
    struct nsd_mtd_info *mtd_info;
    struct nsd_sched_item *next;
};

extern int mtd_fill_dev_params(struct nsd_mtd_info **mtd_info_p, unsigned dev_num);
extern bool mtd_check_block(struct nsd_mtd_info *mtd_info, unsigned blk_num,
                            unsigned threshold);

#endif /* NAND_SCRUB_DAEMON_H_13000013012017 */
