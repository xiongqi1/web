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
#ifndef UTIL_H_13000013012017
#define UTIL_H_13000013012017

#include "nand_scrub_daemon.h"

extern int read_rdb_var_str(const char *var, int dev_num, char *rdb_val,
                            size_t rdb_val_len);
extern int read_rdb_var_int(const char *var, int dev_num, int *rdb_val);
extern int write_rdb_var_str(const char *var, int dev_num, const char *rdb_val);

extern struct nsd_sched_item *create_sched_queue(struct nsd_mtd_info *mtd_info_list);
extern void free_sched_list(struct nsd_sched_item *sched_list);
extern void insert_sched_item(struct nsd_sched_item **sched_list,
                              struct nsd_sched_item *sched_item);
extern struct nsd_sched_item *dequeue_sched_item(struct nsd_sched_item **sched_list);

#endif /* UTIL_H_13000013012017 */
