#ifndef __CALL_TRACK_H_20180620__
#define __CALL_TRACK_H_20180620__

/*
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

#include "uthash.h"

/* call direction */
enum call_dir_t {
    call_dir_incoming,
    call_dir_outgoing,
};

/* call type */
enum call_type_t {
    call_type_invalid,
    call_type_normal,
    call_type_conference,
    call_type_emergency,
};

/* call status */
enum call_status_t {
    call_status_none,

    /* outgoing call */
    call_status_progressed,
    call_status_ringback,
    /* incoming call */
    call_status_setup,
    call_status_ringing,
    call_status_waiting,
    /* common */
    call_status_connected,
    call_status_held,
    call_status_disconnected,
};

/* call track entry */
struct call_track_t {
    int call_track_id; /* call track id - key */

    enum call_dir_t call_dir; /* call direction */
    enum call_type_t call_type; /* call type */
    enum call_status_t call_status; /* call status */

    char* cid_num_pi; /* privacy information */
    char* cid_num; /* call ID */
    char* cid_name_pi;
    char* cid_name; /* caller name */

    unsigned long ring_timestamp; /* ring timestamp */
    int ring_timestamp_valid; /* ring timestamp validation flag */
    int ringing; /* ring flag */

    void* ref; /* FSM object */

    /* call timestamps */
    time_t timestamp_created;
    uint64_t timestamp_connected_msec;
    uint64_t timestamp_disconnected_msec;

    //timeoffset = "UTC time" - "local system time"
    int system_timeoffset;

    int duration;
    int duration_valid;

    int call_blocked;    /* if set, the call was terminated for blocked reason */
    int mt_call_declined; /* if set, the MT call was declined */

    UT_hash_handle hh; /* hash handle */

    int incoming_call_count_coeff; /* incoming call count coefficient */
    int outgoing_call_count_coeff; /* outgoing call count coefficient */

    int being_disconnected; /* call is already in the middle of hanging-up procedure */
    int orphan_call; /* call that is externally placed */
};


typedef void (*call_track_callback_func_ptr)(struct call_track_t* call, void* ref);

int call_track_is_empty(void);
struct call_track_t *call_track_add(int call_track_id);
struct call_track_t *call_track_find(int call_track_id);
struct call_track_t* call_track_get_calls(void);
void call_track_del(struct call_track_t *call);
void call_track_fini(void);
void call_track_fini_cb(struct call_track_t *call, void *ref);
void call_track_init(void);
void call_track_update(struct call_track_t* call);

#define call_track_walk_for_each_begin(call) \
    struct call_track_t* tmp; \
    \
    HASH_ITER(hh, call_track_get_calls(), call, tmp)

#define call_track_walk_for_each_end()

#endif
