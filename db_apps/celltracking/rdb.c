/*
 * RDB interface for the project
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
*/

#include "rdb.h"

#include <errno.h>
#include <string.h>
#include <syslog.h>

/* rdb session handle */
struct rdb_session *_s = NULL;

/*
 Open RDB.

 Params:
  None

 Return:
  0 = success. Otherwise, failure.
*/
int rdb_init()
{
    if (rdb_open(NULL, &_s) < 0) {
        syslog(LOG_ERR, "cannot open Netcomm RDB (errno=%d,str='%s')", errno, strerror(errno));
        return -1;
    }
    return 0;
}

/*
 Close RDB.

 Params:
  None

 Return:
  None
*/
void rdb_fini()
{
    if (_s) {
        rdb_close(&_s);
    }
}

/*
 Subscribe RDB.

 Params:
  rdb : rdb variable to subscribe.

 Return:
  0 = success. Otherwise, failure.
*/
int rdb_watch(const char *rdb)
{
    rdb_create_string(_s, rdb, "", 0, 0);

    if (rdb_subscribe(_s, rdb) < 0) {
        return -1;
    }

    return 0;
}

/*
 Read RDB.

 Params:
  str : buffer to get RDB value.
  str_len : size of buffer.
  rdb : rdb to read.

 Return:
  Return RDB value. Otherwise, blank string.
*/
char *rdb_get_str(char *str, int str_len, const char *rdb)
{

    if ((rdb_get(_s, rdb, str, &str_len)) < 0) {
        *str = 0;
    }

    return str;
}
