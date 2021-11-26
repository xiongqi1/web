/*
 * Call back timer maintains internal timers.
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

#include "callback_timer.h"
#include <syslog.h>

static struct callback_timer_entry_t* _timers = NULL;

/**
 * @brief walks through each of hash entries.
 *
 * @param cb is call back function that is called with each of hash entries.
 * @param ref is reference data that is given to call back function as a parameter.
 */
static void callback_timer_walk_through(callback_timer_walk_through_cb cb, void* ref)
{
    struct callback_timer_entry_t* ent;
    struct callback_timer_entry_t* tmp;

    HASH_ITER(hh, _timers, ent, tmp) {
        cb(ent, ref);
    }
}

/**
 * @brief callback to search and fire timer.
 *
 * @param ent is a timer hash entry.
 * @param ref is reference data.
 */
static void callback_timer_process_cb(struct callback_timer_entry_t* ent, void* ref)
{
    SiVoiceControlInterfaceType* ctrlInterface;
    uInt32 elapsed_time;

    /* get elapsed time */
    ctrlInterface = ent->channel_ptr->deviceId->ctrlInterface;
    ctrlInterface->timeElapsed_fptr(ctrlInterface->hTimer, ent->start_time, (int*)&elapsed_time);

    /* bypass if timer is not expired */
    if (elapsed_time < ent->timeout_ms) {
        goto fini;
    }

    /* remove entity */
    HASH_DEL(_timers, ent);

    syslog(LOG_DEBUG, "[callback timer] trigger callback timer (name='%s',ptr=0x%p,timeout=%d msec)", ent->timer_name,
           ent->start_time, ent->timeout_ms);

    /* call timeout callback */
    ent->cb(ent, elapsed_time, ent->ref);
    /* delete entity */
    free(ent);

fini:
    return;
}

/**
 * @brief processes all of timer hash entries.
 */
void callback_timer_process(void)
{
    callback_timer_walk_through(callback_timer_process_cb, NULL);
}

/**
 * @brief cancels a timer.
 *
 * @param start_time is timer handle to cancel.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int callback_timer_cancel(timeStamp* start_time)
{
    struct callback_timer_entry_t* ent = NULL;

    /* find entity */
    HASH_FIND_PTR(_timers, &start_time, ent);
    if (!ent) {
        goto err;
    }

    syslog(LOG_DEBUG, "[callback timer] cancel callback timer (name='%s',ptr=0x%p,timeout=%d msec)", ent->timer_name,
           ent->start_time, ent->timeout_ms);

    /* remove entity */
    HASH_DEL(_timers, ent);
    /* delete entity */
    free(ent);

    return 0;

err:
    return -1;
}

/**
 * @brief sets a timer.
 *
 * @param channel_ptr is ProSLIC channel pointer.
 * @param timer_name is timer name.
 * @param start_time is timer handle and timer start time storage.
 * @param timeout_ms is time-out in msec.
 * @param cb is custom byte in timer hash entry.
 * @param ref reference data in timer hash entry.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int callback_timer_set(SiVoiceChanType_ptr channel_ptr, const char* timer_name, timeStamp* start_time,
                       uInt32 timeout_ms, callback_timer_timeout_cb cb, void* ref)
{
    struct callback_timer_entry_t* ent = NULL;
    SiVoiceControlInterfaceType* ctrlInterface;
    int new_timer_created = 0;

    /* remove if we have a previous timer */
    HASH_FIND_PTR(_timers, &start_time, ent);
    if (!ent) {
        new_timer_created = 1;

        /* allocate ent */
        ent = (struct callback_timer_entry_t*)calloc(1, sizeof(*ent));
        if (!ent) {
            syslog(LOG_ERR, "failed to allocate ent");
            goto err;
        }
    }

    /* initiate members */
    ent->channel_ptr = channel_ptr;
    ent->timer_name = timer_name;
    ent->start_time = start_time;
    ent->timeout_ms = timeout_ms;
    ent->cb = cb;
    ent->ref = ref;


    /* get start time */
    ctrlInterface = ent->channel_ptr->deviceId->ctrlInterface;
    ctrlInterface->getTime_fptr(ctrlInterface->hTimer, start_time);

    if (new_timer_created) {
        syslog(LOG_DEBUG, "[callback timer] set callback timer (name='%s',ptr=0x%p,timeout=%d msec)", timer_name, start_time,
               timeout_ms);

        /* add ent */
        HASH_ADD_PTR(_timers, start_time, ent);
    } else {
        syslog(LOG_DEBUG, "[callback timer] update callback timer (name='%s',ptr=0x%p,timeout=%d msec)", timer_name, start_time,
               timeout_ms);
    }


    return 0;

err:
    return -1;
}


/**
 * @brief initializes call back timer module.
 */
void callback_timer_init(void)
{
}

/**
 * @brief call back function to delete timer hash entries.
 *
 * @param ent is a timer hash entry.
 * @param ref is reference data.
 */
static void callback_timer_fini_cb(struct callback_timer_entry_t* ent, void* ref)
{
    HASH_DEL(_timers, ent);
    free(ent);
}


/**
 * @brief finalizes call back timer module.
 */
void callback_timer_fini(void)
{
    callback_timer_walk_through(callback_timer_fini_cb, NULL);
}
