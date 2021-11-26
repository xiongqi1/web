/*
 * indexed_rdb maintains a hash table to use index based RDB access.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include "indexed_rdb.h"
#include <stdlib.h>
#include <syslog.h>

/**
 * @brief destroys indexed RDB.
 *
 * @param ir is indexed rdb to destroy.
 */
void indexed_rdb_destroy(struct indexed_rdb_t* ir)
{
    if (!ir)
        return;

    dbhash_destroy(ir->hash_cmds);

    free(ir);
}

/**
 * @brief gets index corresponding to RDB command.
 *
 * @param ir is indexed RDB.
 * @param cmd_str is RDB command string.
 *
 * @return RDB command index.
 */
int indexed_rdb_get_cmd_idx(struct indexed_rdb_t* ir, const char* cmd_str)
{
    return dbhash_lookup(ir->hash_cmds, cmd_str);
}

/**
 * @brief gets string corresponding to RDB index.
 *
 * @param ir is indexed RDB.
 * @param cmd_idx is RDB command idex.
 *
 * @return  RDB command string.
 */
const char* indexed_rdb_get_cmd_str(struct indexed_rdb_t* ir, int cmd_idx)
{
    if (cmd_idx < 0 || cmd_idx >= vc_last) {
        syslog(LOG_ERR, "command index out of range in indexed_rdb_get_cmd_str()");
        goto err;
    }

    return ir->cmds[cmd_idx];
err:
    return NULL;
}

/**
 * @brief creates indexed RDB.
 *
 * @param cmds is an array of RDB commands.
 * @param cmd_cnt is total number of RDB commands.
 *
 * @return
 */
struct indexed_rdb_t* indexed_rdb_create(struct dbhash_element_t* cmds, int cmd_cnt)
{
    struct indexed_rdb_t* ir;

    ir = calloc(1, sizeof(*ir));
    if (!ir) {
        syslog(LOG_ERR, "calloc() failed in indexed_rdb_create()");
        goto err;
    }

    ir->hash_cmds = dbhash_create(cmds, cmd_cnt);
    if (!ir->hash_cmds) {
        syslog(LOG_ERR, "dbhash_create(cmds) failed in indexed_rdb_create()");
        goto err;
    }

    int i;
    for (i = 0; i < cmd_cnt; i++) {

        if (cmds[i].idx < 0 || cmds[i].idx >= vc_last) {
            syslog(LOG_ERR, "command index out of range in indexed_rdb_create()");
            goto err;
        }

        ir->cmds[i] = cmds[i].str;
    }

    return ir;
err:
    indexed_rdb_destroy(ir);
    return NULL;
}
