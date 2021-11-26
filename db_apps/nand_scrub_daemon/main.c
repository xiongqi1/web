/**
 * NAND scrub daemon main file. The scrub daemon's basic task is to
 * scan each MTD device, detect any blocks that contain bit errors
 * beyond a particular threshold and try to recover such blocks. The
 * recovery mechanism is specific to the partition type (e.g. UBI scrub
 * or PEB erase).
 *
 * Notes:
 * - Some partition types, namely UBI, already have the capability of
 * detecting high bit errors and recovering such blocks. However, UBI
 * only detects errors when a page is read so this means rarely read
 * pages could develop errors and escape detection. Hence the scrub
 * daemon operates by periodically reading all the data.
 * - The scrub daemon scans at the MTD level to cover partition meta data,
 * such as UBI EC and VID headers, which can't easily be read at the UBI level.
 *
 * To add support for a new partition type:
 *  1. Implement the partition specific functions as defined in
 *     struct partition_plugin.
 *  2. Add a new enum entry into enum mtd_partition_type
 *  3. Add a new entry into the partition_plugin array.
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
#define _POSIX_C_SOURCE 199309L
#include "nand_scrub_daemon.h"
#include "util.h"
#include "ubi.h"
#include "daemon.h"
#include "rdb_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <sys/select.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>

struct rdb_session *rdb_session;
bool debug_enable = false;
int log_level = LOG_INFO;

/**
 * Partition plugin data for the scrubber. To support a new partition
 * type a new entry should be added to this array before the MTD_PART_OTHER
 * entry. Each entry corresponds to a partition type that the scrubber supports
 * except for the last two entries which have the meaning:
 *  MTD_PART_OTHER: Partition type not configured and unable to be detected.
 *                  Such a partition will still be scanned but never scrubbed.
 *  MTD_PART_AUTO: Partition type not configured or configured to be auto
 *                 detected. This value is only used during initialisation.
 */
static struct partition_plugin partition_plugin[] = {
    /* {type_name, scan_start, scan_end, partition_detect, scrubber, free} */

    /* MTD_PART_UBI */
    {"ubi", NULL, NULL, ubi_detect_partition, ubi_recover, ubi_free},

    /* MTD_PART_QMI */
    {"qmi", NULL, NULL, NULL, NULL, NULL},

    /*
     * Do not add any entries below this line. New partition type support
     * should add an entry above.
     */
    {"other", NULL, NULL, NULL},                             /* MTD_PART_OTHER */
    {"auto", NULL, NULL, NULL},                              /* MTD_PART_AUTO */
};

/**
 * Detect the parition type of a mtd device.
 *
 * @param mtd_info MTD info for the device to detect.
 * @param The detected partition type. MTD_PART_OTHER is returned if no
 * specific paritition type was detectable.
 */
static enum mtd_partition_type
detect_partition_type (struct nsd_mtd_info *mtd_info)
{
    int ix;

    for (ix = 0; ix < (sizeof(partition_plugin) / sizeof(partition_plugin[0]));
         ix++) {
        if (partition_plugin[ix].partition_detect &&
            partition_plugin[ix].partition_detect(mtd_info)) {
            return ix;
        }
    }

    return MTD_PART_OTHER;
}

/**
 * Gather device and config info for each of the mtd devices on the system.
 *
 * @param interval_units_multiplier Mutliplier to convert rdb interval value
 * into seconds.
 * @return A dynamically allocated list of mtd device info nodes.
 */
static struct nsd_mtd_info *
create_mtd_info_list (unsigned interval_units_multiplier)
{
    unsigned int dev_num;
    char rdb_val[MAX_RDB_VAR_LEN];
    unsigned int start_delay;
    int rval;
    int ix;
    const char *part_name;
    struct nsd_mtd_info *mtd_info_list = NULL;
    struct nsd_mtd_info *mtd_info;
    struct timespec now;
    time_t start_scan_time;

    /*
     * The first scan is scheduled shortly after start up. Get that start up delay
     * from rdb or use the default if not configured in rdb.
     */
    rval = read_rdb_var_int(RDB_SCRUB_CONF_START_DELAY, -1,
                            (int *)&start_delay);
    if (rval) {
        if (rval == -ENOENT) {
            start_delay = STARTUP_SCAN_DEFAULT_DELAY_SECS;
        } else {
            errp("Error reading "RDB_SCRUB_CONF_START_DELAY);
            exit(EXIT_FAILURE);
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    start_scan_time = now.tv_sec + start_delay;

    for (dev_num = 0; dev_num < MAX_NUM_DEV; dev_num++) {

        /* Get the device info from the driver */
        rval = mtd_fill_dev_params(&mtd_info, dev_num);
        if (rval == ENOENT) {
            /* No more mtd devices */
            break;
        } else if (rval) {
            /* Error while getting the device info. Try the next device. */
            continue;
        }

        /*
         * Get scrubber config from rdb for each device.
         *
         * First, attempt to get the partition type of each mtd dev from
         * the rdb config.
         */
        mtd_info->partition_type = MTD_PART_AUTO;
        RUN_CHK(read_rdb_var_str(RDB_SCRUB_CONF_TYPE, dev_num, rdb_val,
                                 sizeof(rdb_val)),
                "Error reading "RDB_SCRUB_CONF_TYPE);

        for (ix = 0; ix < (sizeof(partition_plugin) /
                           sizeof(partition_plugin[0])); ix++) {
            if (!strncmp(partition_plugin[ix].type_name, rdb_val,
                         sizeof(rdb_val))) {
                mtd_info->partition_type = ix;
                break;
            }
        }

        /*
         * Auto detect if partition type not configured, configured to 'auto' or
         * configured to an unknown value.
         */
        if (mtd_info->partition_type == MTD_PART_AUTO) {
            mtd_info->partition_type = detect_partition_type(mtd_info);
            part_name = partition_plugin[mtd_info->partition_type].type_name;
            write_rdb_var_str(RDB_SCRUB_CONF_TYPE, dev_num, part_name);
        }

        /* Get the remaining config values */
        RUN_CHK(read_rdb_var_int(RDB_SCRUB_CONF_ENABLE, dev_num,
                                 (int *)&mtd_info->enable),
                "Error reading "RDB_SCRUB_CONF_ENABLE);
        RUN_CHK(read_rdb_var_int(RDB_SCRUB_CONF_INTERVAL, dev_num,
                                 (int *)&mtd_info->interval),
                "Error reading "RDB_SCRUB_CONF_INTERVAL);
        mtd_info->interval *= interval_units_multiplier;
        RUN_CHK(read_rdb_var_int(RDB_SCRUB_CONF_THRESH, dev_num,
                                 (int *)&mtd_info->threshold),
                "Error reading "RDB_SCRUB_CONF_THRESH);

        mtd_info->next_run = start_scan_time;
        mtd_info->next = mtd_info_list;
        mtd_info_list = mtd_info;

        dbgp("mtd%d: part=%d, blksz=%"PRIu64", pgsz=%"PRIu64", devsz=%"PRIu64", "
             "enable=%u, next_run=%s interval=%u", mtd_info->dev_num,
             mtd_info->partition_type, mtd_info->block_size, mtd_info->page_size,
             mtd_info->dev_size, mtd_info->enable,
             ctime(&mtd_info->next_run), mtd_info->interval);
    }

    return mtd_info_list;
}

/*
 * Time between block reads during a device scan. To limit the
 * performance disturbance of the scan.
 */
#define SCAN_SLEEP_MSEC 500

/**
 * Scan all the blocks in an mtd device. The scrubber (if any) is called
 * if a block is detected to require scrubbing.
 *
 * @param mtd_info Pointer to the device info node.
 */
static void
scan_dev_blocks (struct nsd_mtd_info *mtd_info)
{
    unsigned blk_num;
    struct timespec req_time;
    struct timespec rem_time;
    int rval;

    if (!mtd_info->enable) {
        return;
    }

    for (blk_num = 0; blk_num < mtd_info->num_blocks; blk_num++) {
        if (!mtd_check_block(mtd_info, blk_num, mtd_info->threshold)) {
            infop("mtd%d: block %u requires scrubbing", mtd_info->dev_num,
                 blk_num);
            if (partition_plugin[mtd_info->partition_type].scrubber) {
                partition_plugin[mtd_info->partition_type].scrubber(mtd_info,
                                                                    blk_num);
            } else {
                infop("mtd%d: No scrubber, block %u scrubbing skipped",
                      mtd_info->dev_num, blk_num);
            }
        }

        /* Sleep for a while to cap performance disturbance */
        req_time.tv_sec = 0;
        req_time.tv_nsec = SCAN_SLEEP_MSEC * 1000000;
        do {
            rval = nanosleep(&req_time, &rem_time);
            req_time = rem_time;
        } while ((rval == -1) && (errno = EINTR));
    }
}

/**
 * Outputs program help message.
 *
 * @param prog_name Program name.
 */
static void
usage (char *prog_name)
{
    fprintf(stderr, "Usage: %s [options]\n\n", prog_name);
    fprintf(stderr, "\t-d        Enable debug output. Default is debug off.\n");
    fprintf(stderr, "\t-f        Don't deamonise. Default is to deamonise.\n");
    fprintf(stderr, "\t-h        Usage output.\n");
    fprintf(stderr, "\t-m <val>  Scan interval multiplier. Default is 3600.\n");
}

int
main (int argc, char *argv[])
{
    int opt;
    bool foreground = false;
    struct nsd_mtd_info *mtd_info_list = NULL;
    struct nsd_sched_item *sched_list = NULL;
    int ix;
    int rval;

    /*
     * Mutliplier to convert rdb interval value into seconds.
     * The default units for the scan scheduling interval is hours.
     * For debugging the -m parameter can be used to change the multiplier
     * and hence the units for shorter scheduling times.
     */
    unsigned interval_units_multiplier = 60 * 60;

    while ((opt = getopt(argc, argv, "dfhm:")) != -1) {
         switch (opt) {
         case 'd':
             debug_enable = true;
             log_level = LOG_NOTICE;
             break;
         case 'f':
             foreground = true;
             break;
         case 'h':
             usage(argv[0]);
             exit(0);
         case 'm':
             interval_units_multiplier = atoi(optarg);
             break;
         default:
             usage(argv[0]);
             exit(EXIT_FAILURE);
         }
    }

    rval = rdb_open(NULL, &rdb_session);
    if (rval) {
        errp("Unable to open rdb");
        exit(rval);
    }

    if (!foreground) {
        daemon_init("nand_scrub_daemon", NULL, 0, log_level);
    }

    mtd_info_list = create_mtd_info_list(interval_units_multiplier);
    sched_list = create_sched_queue(mtd_info_list);

    /* Processing loop. Sleep until next scan time, scan, repeat. */
    while (1) {
        struct nsd_sched_item *next_sched = sched_list;
        struct timespec now;
        int next_timeout;
        struct timeval timeout, *timeout_p;
        time_t now_realtime;
        time_t next_realtime;

        if (!next_sched) {
            /*
             * No MTD dev has scan enabled. Sleep without any timeout.
             * That is, will only wake up for rdb triggers (which is TBD).
             */
            timeout_p = NULL;
            dbgp("scrub daemon sleeping with no timeout");
        } else {
            /*
             * Work out how long to the next scheduled scan. Clamp to 0 just in
             * case there is any delay in scheduling resulting in overrunning the
             * next scheduled time.
             */
            clock_gettime(CLOCK_MONOTONIC_RAW, &now);
            next_timeout = next_sched->mtd_info->next_run - now.tv_sec;
            if (next_timeout < 0) {
                next_timeout = 0;
            }
            timeout.tv_sec = next_timeout;
            timeout.tv_usec = 0;
            timeout_p = &timeout;
            dbgp("scrub daemon sleeping for %d", next_timeout);
        }
        rval = select(0, NULL, NULL, NULL, timeout_p);

        if (rval == -1) {
            if (errno == EINTR) {
                /*
                 * Signal received. Go back to the top of the loop and restart
                 * the sleep with a new remaining time til next scheduled scan.
                 */
                continue;
            } else {
                rval = errno;
                errp("Unexpected error for select, err=%d. Exiting..", rval);
                exit(rval);
            }
        }

        for (ix = 0; ix < (sizeof(partition_plugin) /
                           sizeof(partition_plugin[0])); ix++) {
            if (partition_plugin[ix].scan_start) {
                partition_plugin[ix].scan_start(mtd_info_list);
            }
        }

        /*
         * Go through sched list and scan each mtd device that has an
         * expired next scan time.
         */
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        now_realtime = time(NULL);
        while (sched_list && (now.tv_sec >= sched_list->mtd_info->next_run)) {
            struct nsd_sched_item *sched_item = dequeue_sched_item(&sched_list);
            scan_dev_blocks(sched_item->mtd_info);

            /*
             * Update next scan time and put it back into the sched list in
             * the right place.
             */
            sched_item->mtd_info->next_run = now.tv_sec +
                sched_item->mtd_info->interval;
            next_realtime = now_realtime + sched_item->mtd_info->interval;
            write_rdb_var_str(RDB_SCRUB_CONF_NEXT, sched_item->mtd_info->dev_num,
                              ctime(&next_realtime));

            insert_sched_item(&sched_list, sched_item);
        }

        for (ix = 0; ix < (sizeof(partition_plugin) /
                           sizeof(partition_plugin[0])); ix++) {
            if (partition_plugin[ix].scan_end) {
                partition_plugin[ix].scan_end(mtd_info_list);
            }
        }
    }

    /* Clean up */
    free_sched_list(sched_list);
    for (ix = 0; ix < (sizeof(partition_plugin) / sizeof(partition_plugin[0]));
         ix++) {
        if (partition_plugin[ix].free) {
            partition_plugin[ix].free();
        }
    }
    while (mtd_info_list) {
        struct nsd_mtd_info *next = mtd_info_list->next;
        close(mtd_info_list->fd);
        free(mtd_info_list->data_buf);
        free(mtd_info_list);
        mtd_info_list = next;
    }
    if (rdb_session) {
        rdb_close(&rdb_session);
    }

    if (!foreground) {
        daemon_fini();
    }

    return 0;
}
