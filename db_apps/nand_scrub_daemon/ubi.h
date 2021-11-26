/*
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
#ifndef UBI_H_13000013012017
#define UBI_H_13000013012017

#include <stdbool.h>

typedef struct ubi_info_ {
    struct ubi_info_ *next;
    unsigned ubi_num;
    unsigned mtd_num;
} ubi_info_t;

extern bool ubi_detect_partition(struct nsd_mtd_info *mtd_info);
extern void ubi_recover(struct nsd_mtd_info *mtd_info, unsigned blk_num);
extern void ubi_free(void);

#endif /* UBI_H_13000013012017 */
