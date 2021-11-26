/*
 * DBHash keeps a hash table to convert string to integer.
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

#define _GNU_SOURCE

#include "dbhash.h"
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

/**
 * @brief looks up string in hash table.
 *
 * @param hash is hash header.
 * @param str is string to look up.
 *
 * @return
 */
int dbhash_lookup(struct dbhash_t* hash, const char* str)
{
    struct entry e;
    struct entry* ep;

    e.key = (char*)str;
    if (hsearch_r(e, FIND, &ep, &hash->htab) < 0)
        return -1;

    if (ep == NULL)
        return -1;

    return (int)ep->data;
}

/**
 * @brief destroys DB hash.
 *
 * @param hash
 */
void dbhash_destroy(struct dbhash_t* hash)
{
    if (!hash)
        return;

    // destroy hash
    if (hash->data_created)
        hdestroy_r(&hash->htab);

    free(hash);
}

/**
 * @brief builds DB hash.
 *
 * @param hash is hash header.
 * @param elements is an array of DB hash entries.
 * @param element_count is total number of DB hash entries.
 *
 * @return 0 when succeeds. Otherwise, -1.
 */
static int dbhash_build(struct dbhash_t* hash, const struct dbhash_element_t* elements, int element_count)
{
    struct entry e;
    struct entry* ep;
    const struct dbhash_element_t* element;
    int i;


    for (i = 0; i < element_count; i++) {
        element = &elements[i];

        e.key = (char*)element->str;
        e.data = (void*)(element->idx);

        if (hsearch_r(e, ENTER, &ep, &hash->htab) < 0) {
            syslog(LOG_ERR, "failed from hsearch_r()");
            goto err;
        }

        if (ep == NULL) {
            syslog(LOG_ERR, "entry failed");
            goto err;
        }
    }

    return 0;

err:
    return -1;
}

/**
 * @brief creates DB hash.
 *
 * @param elements[] is an array of DB hash entries.
 * @param element_count is total number of DB hash entries.
 *
 * @return DB hash when it succeeds. Otherwise, -1.
 */
struct dbhash_t* dbhash_create(const struct dbhash_element_t elements[], int element_count)
{
    struct dbhash_t* hash;

    // create the object
    hash = (struct dbhash_t*)calloc(1, sizeof(struct dbhash_t));
    if (!hash) {
        syslog(LOG_ERR, "failed to allocate dbhash_t - size=%d", sizeof(struct dbhash_t));
        goto err;
    }

    // create hash
    if (hcreate_r(element_count, &hash->htab) < 0) {
        syslog(LOG_ERR, "failed to create hash - size=%d", element_count);
        goto err;
    }

    hash->data_created = 1;

    // build tree
    if (dbhash_build(hash, elements, element_count) < 0)
        goto err;

    return hash;

err:
    dbhash_destroy(hash);

    return NULL;
}
