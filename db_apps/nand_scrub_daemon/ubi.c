/**
 * NAND scrub daemon UBI specific definitions.
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
#include "ubi.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <glob.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define UBI_SYSFS_PREFIX "/sys/class/ubi/ubi"
#define UBI_SYSFS_RECOVER_PEB "recover_peb"

static ubi_info_t *ubi_info_list = NULL;

/**
 * Get info for all the UBI devices (not volumes) on the system. Each UBI device
 * corresponds to an MTD device. A UBI volume is a collection of LEBS within a
 * UBI device.
 *
 * @return The head of the list of ubi info nodes.
 */
static ubi_info_t *
get_ubi_info_list (void)
{
    glob_t globres;
    int ix;
    int rval;
    int ubi_num;
    unsigned int ubi_mtd_num;
    FILE *file;

    if (ubi_info_list) {
        /* Don't need to go to sysfs again. Return previously gatherered info. */
        return ubi_info_list;
    }

    /*
     * Get all the ubi entries from sysfs. The ubi entries include
     * both ubi devices and ubi volumes. Device entries are in the
     * format: "ubi<dev_num>". Volume entries are in the format:
     * "ubi<dev_num>_<volume_num>".
     */
    rval = glob(UBI_SYSFS_PREFIX"[0-9]*", 0, NULL, &globres);
    if (rval) {
        return NULL;
    }

    for (ix = 0; ix < globres.gl_pathc; ix++) {
        ubi_info_t *new_info;
        char *endptr = NULL;

        /*
         * First, check whether the glob entry is a ubi device or ubi volume.
         */

        /*
         * Point to the character immmedietely after
         * /sys/class/ubi/ubi. That is, the ubi device number.
         */
        char *ubi_num_str = strrchr(globres.gl_pathv[ix], '/');
        assert(ubi_num_str);
        ubi_num_str += strlen("/ubi");
        ubi_num = strtol(ubi_num_str, &endptr, 10);
        if (endptr != ubi_num_str + strlen(ubi_num_str)) {
            /*
             * Not a ubi device entry because next string token was
             * not an integer or contained some other characters (ie,
             * '_').
             */
            continue;
        }

        /* Read the mtd num from the sysfs entry class/ubi/ubiX/mtd_num */
        char sysfs_path[strlen(globres.gl_pathv[ix]) + strlen("/mtd_num") + 1];
        snprintf(sysfs_path, sizeof(sysfs_path), "%s/mtd_num",
                 globres.gl_pathv[ix]);
        file = fopen(sysfs_path, "r");
        if (!file) {
            errp("Unable to open ubi sysfs file %s", sysfs_path);
            continue;
        }
        rval = fscanf(file, "%d", &ubi_mtd_num);
        assert(rval == 1);
        fclose(file);

        /* Create new info node and store it away */
        new_info = malloc(sizeof(*new_info));
        assert(new_info);
        new_info->ubi_num = ubi_num;
        new_info->mtd_num = ubi_mtd_num;
        new_info->next = ubi_info_list;
        ubi_info_list = new_info;
    }

    globfree(&globres);

    return ubi_info_list;
}

/**
 * Get the ubi info node, if any, corresponding to an MTD dev.
 *
 * @param mtd_info The mtd info for the device number find.
 * @return The ubi info node if found or NULL if no ubi devices for the given
 * mtd dev num.
 */
static ubi_info_t *
get_ubi_info_for_mtd (struct nsd_mtd_info *mtd_info)
{
    ubi_info_t *ubi_info_p;

    for (ubi_info_p = get_ubi_info_list(); ubi_info_p;
         ubi_info_p = ubi_info_p->next) {
        if (ubi_info_p->mtd_num == mtd_info->dev_num) {
            return ubi_info_p;
        }
    }

    return NULL;
}

/*
 * Detect whether an mtd dev has an attached UBI dev.
 *
 * @param mtd_info The mtd info for the device to detect.
 * @return true if the mtd dev has an attached UBI dev. false otherwise.
 */
bool
ubi_detect_partition (struct nsd_mtd_info *mtd_info)
{
    return (get_ubi_info_for_mtd(mtd_info) != NULL);
}

/*
 * Recover a mtd/ubi block.
 *
 * @param mtd_info The mtd info for the device containing the blk.
 * @param blk_num The block num to be recovered.
 */
void
ubi_recover (struct nsd_mtd_info *mtd_info, unsigned blk_num)
{
    char sysfs_path[strlen(UBI_SYSFS_PREFIX) + MAX_NUM_DEV_STR_LEN +
                    strlen("/"UBI_SYSFS_RECOVER_PEB) + 1];
    ubi_info_t *ubi_info;
    int fd;
    #define MAX_BLK_NUM_STR_LEN 32
    char blk_num_str[MAX_BLK_NUM_STR_LEN];

    dbgp("UBI recover mtd%d, blk%d", mtd_info->dev_num, blk_num);

    ubi_info = get_ubi_info_for_mtd(mtd_info);
    if (!ubi_info) {
        errp("Unable to UBI recover mtd%d, blk%d. Could not get ubi num.",
             mtd_info->dev_num, blk_num);
        return;
    }
    snprintf(sysfs_path, sizeof(sysfs_path),
             UBI_SYSFS_PREFIX"%d/"UBI_SYSFS_RECOVER_PEB, ubi_info->ubi_num);
    fd = open(sysfs_path, O_WRONLY);
    if (fd == -1) {
        errp("Unable to open %s", sysfs_path);
        return;
    }

    snprintf(blk_num_str, sizeof(blk_num_str), "%d", blk_num);
    write(fd, blk_num_str, strlen(blk_num_str));
    close(fd);
}

/**
 * Release resources.
 */
void
ubi_free (void)
{
    while (ubi_info_list) {
        ubi_info_t *next = ubi_info_list->next;
        free(ubi_info_list);
        ubi_info_list = next;
    }
}
