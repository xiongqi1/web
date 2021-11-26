/*
 * dsm_bt_utils.c
 *    Utilities functions for Data Stream bluetooth endpoint.
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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

#include "dsm_bt_utils.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

/*
 * Convert a byte array to hex ascii representation with optional delimiter.
 * Params:
 *    buf - output buffer
 *    len - capacity of buf
 *    bytes - byte array
 *    nbytes - length of byte array
 *    delimiter - delimiter to separate hex bytes. NULL or "" means no separator
 * Return:
 *    -1 on error;
 *    required length of buffer to fit the converted string
 * Note: If and only if 0 <= rval <= len,
 *       the byte array has been successfully converted and stored in buf.
 */
int
snprint_bytes (char *buf, size_t len, const char *bytes, int nbytes,
               const char *delimiter)
{
    int ix;
    int written = 0;
    size_t delim_len = 0;

    if (len < 1) { /* at least one byte is required for terminating null */
        return -1;
    }
    if (nbytes <= 0) {
        *buf = '\0';
        return 1;
    }
    if (delimiter) {
        delim_len = strlen(delimiter);
    }
    for (ix = 0; ix < nbytes; ix++) {
        if (ix && delim_len) {
            if (written + delim_len >= len) {
                break;
            }
            strcpy(buf, delimiter);
            buf += delim_len;
            written += delim_len;
        }
        if (written + 2 >= len) {
            break;
        }
        sprintf(buf, "%02x", bytes[ix]);
        buf += 2;
        written += 2;
    }
    *buf = '\0';
    return nbytes*2 + delim_len*(nbytes-1) + 1;
}

/*
 * Convert a hex ascii representation of a byte array to binary form.
 * Params:
 *    bytes: output byte array
 *    nbytes: max size of bytes on input; actual number of bytes on ouput
 *    buf: string of hex ascii representation
 *    delimiter: string of delimiter (can be NULL or "" for nil delimiter).
 *               all delimiters will be removed from the output.
 * Return:
 *    -1 on parsing error;
 *    required size for bytes to hold all parsed result
 */
int
sscan_bytes (char *bytes, size_t *nbytes, const char *buf,
             const char *delimiter)
{
    int ix;
    size_t delim_len = 0;
    char c;
    int rval = 0;

    if (delimiter) {
        delim_len = strlen(delimiter);
    }
    for (ix = 0; strlen(buf); ix ++) {
        if (ix && delim_len) { /* skip the delimiter */
            if (strncmp(buf, delimiter, delim_len)) {
                rval = -1; /* error */
                break;
            }
            buf += delim_len;
        }
        if (strlen(buf) < 2) { /* no more bytes */
            break;
        }

        if (sscanf(buf, "%2hhx", &c) != 1) {
            rval = -1;
            break; /* error */
        }
        buf += 2;
        if (ix < *nbytes) {
            *bytes++ = c;
        }
    }

    if (ix < *nbytes) {
        *nbytes = ix;
    }

    return rval < 0 ? rval : ix;
}

#define DSM_BT_BLOB_BUF_LEN 256
#define DSM_BT_INT_BUF_LEN 32

#define USE_RDB_BIN_BLOB 1

#if USE_RDB_BIN_BLOB

/*
 * Update the rdb variable identified by rdb_var_name with blob.
 * The content in blob directly written to rdb var.
 * Return 0 on success; negative error code on failure.
 */
int
update_rdb_blob (struct rdb_session *rdb_s, const char *rdb_var_name,
                 const char *blob, int blob_len)
{
    INVOKE_CHK(rdb_update(rdb_s, rdb_var_name, blob, blob_len, 0, 0),
               "Failed to update rdb var %s\n", rdb_var_name);
    return 0;
}

/*
 * Create the rdb variable identified by rdb_var_name with blob.
 * The content in blob is directly written to rdb var.
 * Existing rdb variable will be left intact.
 * Return 0 on success; negative error code on failure (including -EEXIST).
 */
int
create_rdb_blob (struct rdb_session *rdb_s, const char *rdb_var_name,
                 const char *blob, int blob_len)
{
    int rval;
    rval = rdb_create(rdb_s, rdb_var_name, blob, blob_len, 0, 0);
    if (rval && rval != -EEXIST) {
        errp("Failed to create rdb var %s\n", rdb_var_name);
    }
    return rval;
}

#else

/*
 * Update the rdb variable identified by rdb_var_name with blob.
 * The content in blob is converted to ascii hex string before writing to rdb.
 * Return 0 on success; negative error code on failure.
 */
int
update_rdb_blob (struct rdb_session *rdb_s, const char *rdb_var_name,
                 const char *blob, int blob_len)
{
    char buf[DSM_BT_BLOB_BUF_LEN];
    int len;
    len = snprint_bytes(buf, sizeof buf, blob, blob_len, NULL);
    CHECK_COND(len >=0 && len <= sizeof buf, -1,
               "Failed to convert blob to ascii hex string\n");
    INVOKE_CHK(rdb_update_string(rdb_s, rdb_var_name, buf, 0, 0),
               "Failed to update rdb var %s\n", rdb_var_name);
    return 0;
}

/*
 * Create the rdb variable identified by rdb_var_name with blob.
 * The content in blob is converted to ascii hex string before writing to rdb.
 * Existing rdb variable will be left intact.
 * Return 0 on success; negative error code on failure (including -EEXIST).
 */
int
create_rdb_blob (struct rdb_session *rdb_s, const char *rdb_var_name,
                 const char *blob, int blob_len)
{
    char buf[DSM_BT_BLOB_BUF_LEN];
    int rval;
    rval = snprint_bytes(buf, sizeof buf, blob, blob_len, NULL);
    CHECK_COND(rval >=0 && rval <= sizeof buf, -1,
               "Failed to convert blob to ascii hex string\n");
    rval = rdb_create_string(rdb_s, rdb_var_name, buf, 0, 0);
    if (rval && rval != -EEXIST) {
        errp("Failed to create rdb var %s\n", rdb_var_name);
    }
    return rval;
}

#endif

/*
 * Update the rdb variable identified by rdb_var_name with integer val.
 * The integer val is converted to ascii string according to format before
 * writing to rdb. If format==NULL, assume "%d".
 * Return 0 on success; negative error code on failure.
 */
int
update_rdb_int (struct rdb_session *rdb_s, const char *rdb_var_name, int val,
                const char *format)
{
    char buf[DSM_BT_INT_BUF_LEN];
    snprintf(buf, sizeof buf, format ? format : "%d", val);
    INVOKE_CHK(rdb_update_string(rdb_s, rdb_var_name, buf, 0, 0),
               "Failed to update rdb var %s\n", rdb_var_name);
    return 0;
}

/*
 * Create the rdb variable identified by rdb_var_name with integer val.
 * The integer val is converted to ascii string according to format before
 * writing to rdb. If format==NULL, assume "%d".
 * Existing rdb variable will be left intact.
 * Return 0 on success; negative error code on failure (including -EEXIST).
 */
int
create_rdb_int (struct rdb_session *rdb_s, const char *rdb_var_name, int val,
                const char *format)
{
    char buf[DSM_BT_INT_BUF_LEN];
    int rval;
    snprintf(buf, sizeof buf, format ? format : "%d", val);
    rval = rdb_create_string(rdb_s, rdb_var_name, buf, 0, 0);
    if (rval && rval != -EEXIST) {
        errp("Failed to create rdb var %s\n", rdb_var_name);
    }
    return rval;
}
