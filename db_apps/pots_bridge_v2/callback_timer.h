#ifndef __CALLBACK_TIMER_H_20180620__
#define __CALLBACK_TIMER_H_20180620__

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
#include "netcomm_proslic.h"

#include <si_voice_datatypes.h>
#include <proslic.h>

struct callback_timer_entry_t;

typedef void (*callback_timer_timeout_cb)(struct callback_timer_entry_t* ent, uInt32 elapsed_time, void* ref);
typedef void (*callback_timer_walk_through_cb)(struct callback_timer_entry_t* ent, void* ref);

/* callback timer entry */
struct callback_timer_entry_t {
    UT_hash_handle hh; /* hash handle */

    SiVoiceChanType_ptr channel_ptr; /* ProSLIC voice channel type */

    timeStamp* start_time; /* timer start time - key */
    uInt32 timeout_ms; /* timer timeout msec */
    callback_timer_timeout_cb cb; /* timer callback */
    void* ref; /* timer callback reference */

    const char* timer_name; /* timer name - information only */
};

int callback_timer_cancel(timeStamp *start_time);
int callback_timer_set(SiVoiceChanType_ptr channel_ptr, const char* timer_name, timeStamp* start_time,
                       uInt32 timeout_ms, callback_timer_timeout_cb cb, void* ref);
void callback_timer_fini(void);
void callback_timer_init(void);
void callback_timer_process(void);

#endif
