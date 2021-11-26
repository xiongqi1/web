/**
 * NAND scrub daemon MTD level definitions.
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
#include "nand_scrub_daemon.h"
#include <stdlib.h>
#include <mtd/mtd-user.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

/*
 * The standard ECCGETSTATS ioctl does not scale. It does a full scan
 * of the mtd device on each invocation to fill in the bad blocks stats.
 * This ioctl is the same as ECCGETSTATS except it skips the bad blocks
 * collation and is thus much faster.
 *
 * Defined here as ECCGETSTATS2 is a Netcomm extension and is this not present
 * in the toolchains mtd-abi.h header
 */
#define ECCGETSTATS2 _IOWR('M', 10000, struct mtd_ecc_stats)

#define MTD_DEV_PREFIX "/dev/mtd"
#define MTD_SYSFS_PREFIX "/sys/class/mtd/mtd"

/**
 * Used to size arrays for storing MTD sysfs path strings.
 * "numeraseregions" is the curently longest MTD sysfs entry.
 */
#define MTD_SYSFS_MAX_PATH_LEN \
    strlen(MTD_SYSFS_PREFIX) + MAX_NUM_DEV_STR_LEN + 32

/**
 * Read an MTD sysfs entry as an unsigned int64.
 *
 * @param val Pointer to an unsigned int where the read value will be stored.
 * @param dev_num The MTD device number to get the info for.
 * @param entry Name string of the sysfs entry to get.
 * @return 0 on success and an error code otherwise.
 */
static int
read_mtd_sysfs_uint64 (uint64_t *val, unsigned dev_num, const char *entry)
{
    char sysfs_path[MTD_SYSFS_MAX_PATH_LEN];
    FILE *file;
    int rval;

    snprintf(sysfs_path, sizeof(sysfs_path), MTD_SYSFS_PREFIX "%u/%s",
             dev_num, entry);
    file = fopen(sysfs_path, "r");
    if (!file) {
        rval = -errno;
        errp("Unable to open mtd sysfs file %s", sysfs_path);
        return rval;
    }

    rval = fscanf(file, "%"PRIu64, val);
    if (rval != 1) {
        rval = -errno;
        errp("Unable to read mtd sysfs file %s", sysfs_path);
    } else {
        rval = 0;
    }

    fclose(file);
    return rval;
}

/**
 * Allocate and fill in MTD device specific info.
 *
 * @param mtd_info_p The dynamically allocated device info is returned in this
 * in this pointer.
 * @param dev_num The MTD device number to get the info for.
 * @return 0 on success and an error code otherwise.
 */
int
mtd_fill_dev_params (struct nsd_mtd_info **mtd_info_p, unsigned dev_num)
{
    char dev_str[strlen(MTD_DEV_PREFIX) + MAX_NUM_DEV_STR_LEN + 1];
    int rval;
    int ix;

    *mtd_info_p = NULL;
    struct nsd_mtd_info *mtd_info = malloc(sizeof(*mtd_info));
    if (!mtd_info) {
        return errno;
    }

    mtd_info->dev_num = dev_num;
    snprintf(dev_str, sizeof(dev_str), MTD_DEV_PREFIX "%u", dev_num);
    mtd_info->fd = open(dev_str, O_RDONLY);
    if (mtd_info->fd == -1) {
        rval = errno;
        errp("Unable to open device for mtd%d err=%d", dev_num, rval);
        free(mtd_info);
        return rval;
    }

    struct {
        const char *entry;
        uint64_t *val_p;
    } sysfs_read_data[] = {
        {"erasesize", &mtd_info->block_size},
        {"writesize", &mtd_info->page_size},
        {"size", &mtd_info->dev_size}
    };

    for (ix = 0; ix < sizeof(sysfs_read_data) / sizeof(sysfs_read_data[0]);
         ix++) {
        rval = read_mtd_sysfs_uint64(sysfs_read_data[ix].val_p, dev_num,
                                     sysfs_read_data[ix].entry);
        if (rval) {
            close(mtd_info->fd);
            free(mtd_info);
            return rval;
        }
    }

    mtd_info->num_blocks = mtd_info->dev_size / mtd_info->block_size;

    /*
     * Allocate buffer for mtd data reads. Would have like to make this smaller
     * than the whole page to slightly improve performance by minimising the
     * kernel to user data copy. However, there is is a bug in the msm_qpic_nand
     * driver when reading partial pages - if the page has bit errors it is
     * returning the whole page length rather than the requested read length which
     * results in an EFAULT at user level due to overflowing the user buffer.
     */
    mtd_info->data_buf_size = mtd_info->page_size;
    mtd_info->data_buf = malloc(mtd_info->data_buf_size);
    if (!mtd_info->data_buf) {
        rval = errno;
        close(mtd_info->fd);
        free(mtd_info);
        errp("Unable to allocate data buf for mtd%d", mtd_info->dev_num);
        return rval;
    }

    *mtd_info_p = mtd_info;

    return 0;
}

/**
 * Check whether an MTD block has bit error number that has reached a threshold.
 *
 * @param mtd_info mtd info node for the device to be checked
 * @param blk_num An mtd block within the mtd device.
 * @param threshold A threashold for page bit errors.
 * @return false if any page in the block has number of bit errors that has
 * reached the threshold. true otherwise.
 */
bool
mtd_check_block (struct nsd_mtd_info *mtd_info, unsigned blk_num, unsigned threshold)
{
    off_t block_start;
    unsigned num_pages;
    int pgnum;
    struct mtd_ecc_stats stat_pre;
    struct mtd_ecc_stats stat_post;
    int rval;
    __kernel_loff_t of;;

    /* Sanity check for invalid blk_num */
    if (blk_num * mtd_info->block_size > mtd_info->dev_size) {
        return true;
    }

    block_start = blk_num * mtd_info->block_size;
    num_pages = mtd_info->block_size / mtd_info->page_size;

    of = (__kernel_loff_t)block_start;
    rval = ioctl(mtd_info->fd, MEMGETBADBLOCK, &of);
    if (rval == -1) {
        errp("Error %d checking mtd%d blk%d for BADBLOCK\n",
             errno, mtd_info->dev_num, blk_num);
        return false;
    } else if (rval) {
        /* Must not scan or scrub bad blocks */
        dbgp("Skipping scan of mtd%d bad blk%d\n", mtd_info->dev_num, blk_num);
        return true;
    }

    /*
     * Read each page in the block and get the number of bit errors
     * corrected due to that read. Note that there is a slight race
     * condition in that the bit errors can come from other reads that
     * occured. This is unlikely and will only result in an extra recovery
     * for the block.
     */
    for (pgnum = 0; pgnum < num_pages; pgnum++) {
        lseek(mtd_info->fd, block_start + pgnum * mtd_info->page_size, SEEK_SET);
        rval = ioctl(mtd_info->fd, ECCGETSTATS2, &stat_pre);
        if (rval) {
            errp("Error %d reading pre eccstats for mtd%d blk%d\n",
                 errno, mtd_info->dev_num, blk_num);
            continue;
        }

        rval = read(mtd_info->fd, mtd_info->data_buf, mtd_info->data_buf_size);
        if (rval != mtd_info->data_buf_size) {
            errp("Error %d/%d reading data for mtd%d blk%d\n",
                 errno, rval, mtd_info->dev_num, blk_num);
            continue;
        }

        rval = ioctl(mtd_info->fd, ECCGETSTATS2, &stat_post);
        if (rval) {
            errp("Error %d reading post eccstats for mtd%d blk%d\n",
                 errno, mtd_info->dev_num, blk_num);
            continue;
        }

        /*
         * Block needs recovery if corrected bits exceeds threshold or
         * there were uncorrectable bit errors.
         */
        if (((stat_post.corrected - stat_pre.corrected) >= threshold) ||
            (stat_post.failed - stat_pre.failed) > 0) {
            dbgp("mtd%d blk%u Corrected=%u exceeded threshold=%u\n",
                 mtd_info->dev_num, blk_num,
                 stat_post.corrected - stat_pre.corrected,
                 threshold);
            return false;
        }
    }

    return true;
}
