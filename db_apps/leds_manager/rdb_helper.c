/*
 * rdb_helper.c
 *
 * Implementing helper functions to work with RDB
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
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
static char *rdb_value_buf;
static size_t subscribed_rdb_name_len = 0;
static char *subscribed_rdb_name = NULL;

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

    return rdb_value_buf ? 0 : -ENOMEM;
}

/*
 * subscribe_rdb. See rdb_helper.h.
 */
int subscribe_rdb(const char* rdb_name) {

    if (!rdb_session || !rdb_name) {
        return -EINVAL;
    }

    // try creating RDB first
    int result = rdb_create(rdb_session, rdb_name, "", 0, 0, 0);
    if ((result != 0) && (result != -EEXIST)) {
        return -1;
    }

    // subscribe
    result = rdb_subscribe(rdb_session, rdb_name);
    if (result != 0) {
        return result;
    }

    // Adjust next s_nameBuffer length
    subscribed_rdb_name_len += strlen(rdb_name) + 1;

    return 0;
}

/*
 * post_subscribe_rdb. See rdb_helper.h.
 */
int post_subscribe_rdb() {
    if (subscribed_rdb_name_len > 0) {
        free(subscribed_rdb_name);
        subscribed_rdb_name = (char *) calloc(1, subscribed_rdb_name_len);
        return subscribed_rdb_name ? 0 : -ENOMEM;
    }
    return 0;
}

/*
 * get_rdb_fd. See rdb_helper.h.
 */
int get_rdb_fd() {
    if (!rdb_session) {
        return -EINVAL;
    }
    return rdb_fd(rdb_session);
}

/*
 * get_triggered_rdb. See rdb_helper.h.
 */
char* get_triggered_rdb() {
    if (!rdb_session || !subscribed_rdb_name) {
        return NULL;
    }
    int names_buffer_len = subscribed_rdb_name_len;
    if (rdb_getnames(rdb_session, "", subscribed_rdb_name, &names_buffer_len, TRIGGERED) == 0) {
        return subscribed_rdb_name;
    } else {
        return NULL;
    }
}

/*
 * get_rdb. See rdb_helper.h.
 */
char *get_rdb(const char *rdb_name) {
    if (!rdb_session || !rdb_name || !rdb_value_buf) {
        return NULL;
    }
    if (!rdb_get_string(rdb_session, rdb_name, rdb_value_buf, RDB_VALUE_SIZE)) {
        return rdb_value_buf;
    }

    return NULL;
}

/*
 * set_rdb. See rdb_helper.h.
 */
int set_rdb(const char *rdb_name, const char *value) {
    if (!rdb_session || !rdb_name || !value) {
        return -EINVAL;
    }
    return rdb_set_string(rdb_session, rdb_name, value);
}


/*
 * deinit_rdb. See rdb_helper.h.
 */
void deinit_rdb()
{
    if (rdb_session){
        rdb_close(&rdb_session);
        free(rdb_value_buf);
        free(subscribed_rdb_name);
        rdb_value_buf = NULL;
        subscribed_rdb_name = NULL;
    }
}
