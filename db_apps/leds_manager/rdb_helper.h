/*
 * rdb_helper.h
 *
 * Helper functions to work with RDB
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

#ifndef RDB_HELPER_H_00000010102018
#define RDB_HELPER_H_00000010102018

/*
 * Initialise RDB session and related buffers
 * @returns 0 if success, negative error code otherwise
 */
int init_rdb();

/*
 * Subscribing a RDB variable for getting notification
 * @param rdb_name RDB variable name to subscribe to
 * @return 0 on success, negative number otherwise
 */
int subscribe_rdb(const char* rdb_name);

/*
 * Necessary procedure to invoke after completing all subscribing
 * @return 0 on success, negative number otherwise
 */
int post_subscribe_rdb();

/*
 * Get RDB fd
 * @return fd number on success, negative number otherwise
 */
int get_rdb_fd();

/*
 * Get triggered RDBs
 * Notice: Subsequent calls to this function will overwrite buffer of previous returned value.
 * @return string containing a single list of triggered variable names delimited by the '&' character
 * or NULL on error
 */
char* get_triggered_rdb();

/*
 * Get RDB string value
 * Notice: Subsequent calls to this function will overwrite buffer of previous returned value.
 * @param rdb_name RDB name to get string value
 * @return value string or NULL on error
 */
char *get_rdb(const char *rdb_name);

/*
 * Set RDB string value
 * @param rdb_name RDB name to set string value
 * @param value string value to set
 * @return 0 on success or negative number on error
 */
int set_rdb(const char *rdb_name, const char *value);

/*
 * Reset and free RDB resources
 */
void deinit_rdb();

#endif
