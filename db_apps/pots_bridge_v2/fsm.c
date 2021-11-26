/*
 * fsm is a general finite-state machine for FXS call states.
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


#include "fsm.h"
#include <linux/stddef.h>
#include <syslog.h>

/* fsm macro current state */
#define fsm_cur_state_idx fsm->cur_state_idx
#define fsm_cur_state (&fsm->states[fsm_cur_state_idx])

/* fsm macro for action functions */
#define fsm_cur_state_action(action) fsm_cur_state->actions[action]
#define fsm_cur_state_action_enter fsm_cur_state_action(fsm_action_func_enter)
#define fsm_cur_state_action_leave fsm_cur_state_action(fsm_action_func_leave)
#define fsm_cur_state_action_process fsm_cur_state_action(fsm_action_func_process)

/**
 * @brief registers event names.
 *
 * @param fsm is FSM object.
 * @param event_names is an array of event names.
 * @param num_of_event_names is total number of event names.
 */
void fsm_register_event_names(struct fsm_t* fsm, char** event_names, int num_of_event_names)
{
    fsm->event_names = event_names;
    fsm->num_of_event_names = num_of_event_names;
}

/**
 * @brief registers state names
 *
 * @param fsm is FSM object.
 * @param state_names is an array of state names.
 * @param num_of_state_names is total number of state names.
 */
void fsm_register_state_names(struct fsm_t* fsm, char** state_names, int num_of_state_names)
{
    fsm->state_names = state_names;
    fsm->num_of_state_names = num_of_state_names;
}

/**
 * @brief finalizes FSM object.
 *
 * @param fsm
 */
void fsm_fini(struct fsm_t* fsm)
{
}

/**
 * @brief initializes FSM object.
 *
 * @param fsm is a pointer to FSM object.
 * @param states is array of states.
 * @param num_of_states is total number of states.
 * @param ref is reference data to FSM.
 *
 * @return 0 when it succeeds.
 */
int fsm_init(struct fsm_t* fsm, const struct fsm_state_t* states, int num_of_states, void* ref)
{
    fsm->states = states;
    fsm->num_of_states = num_of_states;
    fsm->cur_state_idx = 0;
    fsm->ref = ref;

    return 0;
}

/**
 * @brief gets string from string array.
 *
 * @param idx is index to select a string.
 * @param strs is an array of string.
 * @param num_of_strs is total number of string.
 *
 * @return string at chosen index when it succeeds. Otherwise, NULL.
 */
static const char* fsm_get_str_by_int(int idx, char** strs, int num_of_strs)
{
    /* check range */
    if (idx < 0 || idx >= num_of_strs) {
        syslog(LOG_ERR, "[fsm] index out of range (idx=%d,num_of_strs=%d)", idx, num_of_strs);
        goto err;
    }

    return strs[idx];

err:
    return NULL;
}

/**
 * @brief gets event name.
 *
 * @param fsm is FSM object.
 * @param event is event number.
 *
 * @return event string name.
 */
const char* fsm_get_event_name(struct fsm_t* fsm, int event)
{
    return fsm_get_str_by_int(event, fsm->event_names, fsm->num_of_event_names);
}

/**
 * @brief gets current state name.
 *
 * @param fsm is FSM object.
 *
 * @return state string name.
 */
const char* fsm_get_current_state_name(struct fsm_t* fsm)
{
    return fsm_get_state_name(fsm, fsm_cur_state_idx);
}

/**
 * @brief gets current state index.
 *
 * @param fsm is FSM object.
 *
 * @return state integer index.
 */
int fsm_get_current_state_index(struct fsm_t* fsm)
{
    return fsm_cur_state_idx;
}

/**
 * @brief posts event to FSM.
 *
 * @param fsm is FSM object.
 * @param event is event to send.
 * @param event_arg is additional parameter for event.
 * @param event_arg_len is length of parameter.
 */
void fsm_post_event(struct fsm_t* fsm, int event, void* event_arg, int event_arg_len)
{
    const char* event_name;
    const char* cur_state_name;

    /* get state name */
    event_name = fsm_get_event_name(fsm, event);
    cur_state_name = fsm_get_state_name(fsm, fsm_cur_state_idx);

    syslog(LOG_DEBUG, "[fsm] post event, [event '%s'] to [state '%s']", event_name, cur_state_name);

    if (fsm_cur_state_action_process) {
        fsm_cur_state_action_process(fsm, fsm_cur_state_idx, event, event_arg, event_arg_len);
    }
}

/**
 * @brief gets state name.
 *
 * @param fsm is FSM object.
 * @param state_idx is state index.
 *
 * @return state string name.
 */
const char* fsm_get_state_name(struct fsm_t* fsm, int state_idx)
{
    return fsm_get_str_by_int(state_idx, fsm->state_names, fsm->num_of_state_names);
}

/**
 * @brief switches FSM into a state.
 *
 * @param fsm is FSM object.
 * @param new_state_idx is new state to switch to.
 * @param event is event to send.
 * @param event_arg is additional parameter for event.
 * @param event_arg_len is length of parameter.
 */
void fsm_switch_state(struct fsm_t* fsm, int new_state_idx, int event, void* event_arg, int event_arg_len)
{
    const char* cur_state_name;
    const char* new_state_name;

    /* bypass if new state is invalid */
    if (new_state_idx <= 0)
        return;

    /* get state name */
    cur_state_name = fsm_get_state_name(fsm, fsm_cur_state_idx);
    new_state_name = fsm_get_state_name(fsm, new_state_idx);

    syslog(LOG_DEBUG, "[fsm] switch state [state '%s'] ==> [state '%s']", cur_state_name, new_state_name);
    /* leave current state */
    if (fsm_cur_state_action_leave) {
        syslog(LOG_DEBUG, "[fsm] call leave action [state '%s']", cur_state_name);
        fsm_cur_state_action_leave(fsm, fsm_cur_state_idx, event, event_arg, event_arg_len);
    }

    /* enter new state */
    fsm_cur_state_idx = new_state_idx;
    if (fsm_cur_state_action_enter) {
        syslog(LOG_DEBUG, "[fsm] call enter action [state '%s']", new_state_name);
        fsm_cur_state_action_enter(fsm, fsm_cur_state_idx, event, event_arg, event_arg_len);
    }
}

/**
 * @brief prepares to switch state by event.
 *
 * @param fsm is FSM object.
 * @param event is event to send.
 * @param event_arg is additional parameter for event.
 * @param event_arg_len is length of parameter.
 * @param states is an array of states.
 * @param num_of_states is total number of states.
 *
 * @return
 */
int fsm_prep_switch_state_by_event(struct fsm_t* fsm, int event, void* event_arg, int event_arg_len, const int* states,
                                   int num_of_states)
{

    /* check range */
    if (event <= 0 || event >= num_of_states) {
        syslog(LOG_ERR, "[fsm] event out of range (event=%d,num_of_states=%d)", event, num_of_states);
        goto err;
    }

    /* get new state */
    return states[event];

err:
    return -1;
}

/**
 * @brief performs to siwtch state by event.
 *
 * @param fsm is FSM object.
 * @param event is event to send.
 * @param event_arg is additional parameter for event.
 * @param event_arg_len is length of parameter.
 * @param new_state_idx is a new state to switch to.
 *
 * @return
 */
int fsm_perform_switch_state_by_event(struct fsm_t* fsm, int event, void* event_arg, int event_arg_len,
                                      int new_state_idx)
{
    const char* event_name;
    const char* cur_state_name;
    const char* new_state_name;

    /* get state name */
    event_name = fsm_get_event_name(fsm, event);
    cur_state_name = fsm_get_state_name(fsm, fsm_cur_state_idx);
    new_state_name = fsm_get_state_name(fsm, new_state_idx);

    /* switch */
    if (new_state_idx <= 0) {
        syslog(LOG_DEBUG, "[fsm] skip event, [event '%s'] in [state '%s']", event_name, cur_state_name);
        goto err;
    }

    syslog(LOG_DEBUG, "[fsm] accept event, [event '%s'] changing [state '%s'] to [state '%s'] ", event_name, cur_state_name,
           new_state_name);

    fsm_switch_state(fsm, new_state_idx, event, event_arg, event_arg_len);

    return 0;

err:
    return -1;
}

