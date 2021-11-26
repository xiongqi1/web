/*
 * NAND scrub daemon utility definitions.
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
#include "rdb_ops.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

extern struct rdb_session *rdb_session;

/*
 * Generates an rdb scrub variable name and stores into into a buffer.
 *
 * @param buf A buffer to store the var name. Must be a statically allocated array.
 * @param dev_num MTD device number.
 * @param var Scrub variable name not including the scrub prefix.
 */
#define RDB_SCRUB_VAR(buf, dev_num, var) \
    snprintf(buf, sizeof(buf), RDB_SCRUB_PREFIX ".%d.%s", dev_num, var)

/*
 * Convenience wrapper to read an rdb scrub variable as a string.
 *
 * @param var Scrub variable name not including the scrub prefix.
 * @param dev_num MTD device number.
 * @param rdb_val A buffer to store the rdb value.
 * @param rdb_val_len Length in bytes of the rdb_val buffer.
 * @return 0 on success and an error code on failure.
 */
int
read_rdb_var_str (const char *var, int dev_num, char *rdb_val,
                  size_t rdb_val_len)
{
    char rdb_name[MAX_RDB_NAME_LEN];

    RDB_SCRUB_VAR(rdb_name, dev_num, var);
    return rdb_get_string(rdb_session, rdb_name, rdb_val, rdb_val_len);
}

/*
 * Convenience wrapper to read an rdb scrub variable as an int.
 *
 * @param var Scrub variable name not including the scrub prefix.
 * @param dev_num MTD device number. If value is -1 then read treat 'var'
 * as a non-device variable and read it as is without forming the RDB var
 * name from the dev_num.
 * @param rdb_val A buffer to store the rdb value.
 * @param rdb_val_len Length in bytes of the rdb_val buffer.
 * @return 0 on success and an error code on failure.
 */
int
read_rdb_var_int (const char *var, int dev_num, int *rdb_val)
{
    if (dev_num >= 0) {
        char rdb_name[MAX_RDB_NAME_LEN];
        RDB_SCRUB_VAR(rdb_name, dev_num, var);
        return rdb_get_int(rdb_session, rdb_name, rdb_val);
    } else {
        return rdb_get_int(rdb_session, var, rdb_val);
    }
}

/*
 * Convenience wrapper to write a string value to an rdb scrub variable.
 *
 * @param var Scrub variable name not including the scrub prefix.
 * @param dev_num MTD device number.
 * @param rdb_val A buffer containing the rdb value to set.
 * @return 0 on success and an error code on failure.
 */
int
write_rdb_var_str (const char *var, int dev_num, const char *rdb_val)
{
    char rdb_name[MAX_RDB_NAME_LEN];

    RDB_SCRUB_VAR(rdb_name, dev_num, var);
    return rdb_update_string(rdb_session,rdb_name, rdb_val, 0, 0);
}

/*
 * Create an ordered scan scheduling queue for all the mtd devices. The queue
 * is maintained in order of increasing next scan time.
 *
 * @param mtd_info_list The list of mtd device info nodes.
 * @return The head of the sched queue. The nodes are dynamically allocated.
 */
struct nsd_sched_item *
create_sched_queue (struct nsd_mtd_info *mtd_info_list)
{
    struct nsd_sched_item *sched_list = NULL;
    struct nsd_sched_item *sched_item;

    for (; mtd_info_list; mtd_info_list = mtd_info_list->next) {
        if (!mtd_info_list->enable) {
            continue;
        }

        sched_item = malloc(sizeof(*sched_item));
        if (!sched_item) {
            free_sched_list(sched_list);
            return NULL;
        }

        sched_item->mtd_info = mtd_info_list;
        insert_sched_item(&sched_list, sched_item);
    }

    return sched_list;
}

/*
 * Freed the scheduling queue memory.
 *
 * @param sched_list The head of the queue to be freed.
 */
void
free_sched_list (struct nsd_sched_item *sched_list)
{
    while (sched_list) {
        struct nsd_sched_item *next = sched_list->next;
        free(sched_list);
        sched_list = next;
    }
}

/*
 * Insert an item into the sched queue. The insertion maintains the ordering
 * of the queue.
 *
 * @param sched_list A pointer to pointer to the head of the queue.
 * @param sched_item The item to be inserted.
 */
void
insert_sched_item (struct nsd_sched_item **sched_list,
                   struct nsd_sched_item *sched_item)
{
    struct nsd_sched_item *cur;
    struct nsd_sched_item *prev;

    for (prev = NULL, cur = *sched_list; cur; prev = cur, cur = cur->next) {
        if (cur->mtd_info->next_run > sched_item->mtd_info->next_run) {
            break;
        }
    }

    sched_item->next = cur;
    if (!prev) {
        *sched_list = sched_item;
    } else {
        prev->next = sched_item;
    }
}

/*
 * Remove the front item of the sched queue.
 *
 * @param sched_list A pointer to pointer to the head of the queue.
 * @return The head item in the queue, or NULL if queue is empty.
 */
struct nsd_sched_item *
dequeue_sched_item (struct nsd_sched_item **sched_list)
{
    struct nsd_sched_item *sched_item = *sched_list;

    if (sched_item) {
        *sched_list = sched_item->next;
    }
    return sched_item;
}
