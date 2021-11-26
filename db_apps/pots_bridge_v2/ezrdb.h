#ifndef __EZRDB_H_20180731_H__
#define __EZRDB_H_20180731_H__

/*
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

#include <stdint.h>

/* ezrdb.c */

typedef int (*_ez_batch_rdb_perform_cb)(const char* rdb, void* ref);

#define RDB_MAX_NAME_LEN 1024


/* ezrdb.c */
struct rdb_session *ezrdb_get_session(void);
void ezrdb_close(void);
int ezrdb_open(void);
int ezrdb_set_str(const char *rdb, const char *val);
int ezrdb_subscribe(const char *rdb);
int ezrdb_perform_array(const char *rdb[], int rdb_count, _ez_batch_rdb_perform_cb cb, void *ref);
int ezrdb_subscribe_array_cb(const char *rdb, void *ref);
int ezrdb_subscribe_array(const char *rdb[], int rdb_count);
int ezrdb_set_array_cb(const char *rdb, void *ref);
int ezrdb_set_array(const char *rdb[], int rdb_count, const char *val);
int ezrdb_get(const char *szName, char *pValue, int len);
const char *ezrdb_get_str(const char *rdb);
uint64_t ezrdb_get_int(const char *rdb);
int ezrdb_set_int(const char* rdb, uint64_t val);
int ezrdb_set_str_persist(const char* rdb, const char* val);
int ezrdb_set_int_persist(const char* rdb, uint64_t val);

#endif
