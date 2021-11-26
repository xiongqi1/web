/**
 * C header declare of ECGI (Global cell id) linked list
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __GCELL_H__
#define __GCELL_H__

#include <list.h>

/* global cell information */
struct gcell_t {
    struct list_head list; /* link list head */

    char mcc[8]; /* MCC */
    char mnc[8]; /* MNC */

    int phys_cell_id; /* physical cell ID */
    int cell_id;      /* base station cell ID (CGI) */
    int freq;         /* EARFCN frequency */
};

struct gcell_t *gcell_get_first_or_next(struct gcell_t *gc);
struct gcell_t *gcell_new();
void gcell_del(struct gcell_t *gc);
void gcell_add(struct gcell_t *gc);
void gcell_remove_all();
void gcell_move_to_tail(struct gcell_t *gc);
struct gcell_t *gcell_find(struct gcell_t *gc);
int gcell_get_count();
int gcell_compare(struct gcell_t *a, struct gcell_t *b);

#endif
