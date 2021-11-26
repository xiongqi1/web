/*
 * Call Track tracks voice calls by UT hash
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

#include "call_track.h"
#include <linux/stddef.h>
#include <syslog.h>

/*
///////////////////////////////////////////////////////////////////////////////
// hash for call
///////////////////////////////////////////////////////////////////////////////
*/

static struct call_track_t* _calls = NULL;
static int _call_count = 0;

/**
 * @brief obtains call hash header.
 *
 * @return header of calls.
 */
struct call_track_t* call_track_get_calls(void)
{
    return _calls;
}

/**
 * @brief checks if call hash is empty.
 *
 * @return TRUE if the hash is empty. Otherwise, FALSE.
 */
int call_track_is_empty(void)
{
    return !_call_count;
}

/**
 * @brief remove a call from call hash.
 *
 * @param call is a call to remove.
 */
void call_track_del(struct call_track_t* call)
{
    HASH_DEL(_calls, call);

    _call_count--;

    free(call);
}

/**
 * @brief finds a call by call track id
 *
 * @param call_track_id is call track id to find.
 *
 * @return call found by call track id.
 */
struct call_track_t* call_track_find(int call_track_id)
{
    struct call_track_t* call;

    /* find */
    HASH_FIND_INT(_calls, &call_track_id, call);

    return call;
}

/**
 * @brief remove and re-add a call to call hash.
 *
 * @param call is a call to remove and re-add.
 */
void call_track_update(struct call_track_t* call)
{
    HASH_DEL(_calls, call);
    HASH_ADD_INT(_calls, call_track_id, call);
}

/**
 * @brief create and add a call.
 *
 * @param call_track_id is call track ID to create and add.
 *
 * @return call that is created.
 */
struct call_track_t* call_track_add(int call_track_id)
{
    struct call_track_t* call;

    /* allocate call state */
    call = (struct call_track_t*)calloc(sizeof(*call), 1);
    if (!call) {
        syslog(LOG_ERR, "failed to allocate call_track_state_t");
        goto err;
    }

    /* set members */
    call->call_track_id = call_track_id;

    /* add */
    HASH_ADD_INT(_calls, call_track_id, call);

    _call_count++;

    return call;

err:
    return NULL;
}

/**
 * @brief initiates call track module.
 */
void call_track_init(void)
{
}

/**
 * @brief finalizes call track module.
 */
void call_track_fini(void)
{
    struct call_track_t* call;

    call_track_walk_for_each_begin(call) {
        call_track_del(call);
    }
}
