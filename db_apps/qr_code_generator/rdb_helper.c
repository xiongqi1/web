/*
 * rdb_helper.c
 *
 * Implementing helper functions to work with RDB
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
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

#include "rdb_helper.h"
#include <rdb_ops.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static struct rdb_session *rdb_session = NULL;
#define RDB_VALUE_SIZE 256
static char *rdb_value_buf = NULL;
static int rdb_value_buf_len = 0;

/*
 * init_rdb. See rdb_helper.h.
 */
int init_rdb()
{
    int ret = rdb_open(NULL, &rdb_session);
    if (ret){
        return ret;
    }

    rdb_value_buf = (char *) calloc(1, RDB_VALUE_SIZE);
    rdb_value_buf_len = RDB_VALUE_SIZE;

    return rdb_value_buf ? 0 : -ENOMEM;
}

/*
 * Check whether given buffer contains a string i.e null-terminated
 * @param buf buffer to check
 * @param buf_len Buffer length
 * @return 1 if given buffer is null-terminated; otherwise returns 0
 */
static int is_string(char *buf, int buf_len)
{
    if (buf) {
        int i;
        for (i = 0; i < buf_len; i++) {
            if (buf[i] == 0) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * get_rdb. See rdb_helper.h.
 */
char *get_rdb(const char *rdb_name)
{
    if (!rdb_session || !rdb_name || !rdb_value_buf) {
        return NULL;
    }
    if (!rdb_get_alloc(rdb_session, rdb_name, &rdb_value_buf, &rdb_value_buf_len)
            && is_string(rdb_value_buf, rdb_value_buf_len)) {
        return rdb_value_buf;
    }

    return NULL;
}

/*
 * deinit_rdb. See rdb_helper.h.
 */
void deinit_rdb()
{
    if (rdb_session){
        rdb_close(&rdb_session);
        free(rdb_value_buf);
        rdb_value_buf = NULL;
        rdb_value_buf_len = 0;
    }
}
