#ifndef __FSM_20180618_H__
#define __FSM_20180618_H__

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
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>

struct fsm_t;
struct fsm_state_t;

/* state machine action function */
typedef void (*fsm_action_func_ptr)(struct fsm_t* fsm, int new_state_idx, int event, void* event_arg,
                                    int event_arg_len);
typedef void (*fsm_postaction_func_ptr)(struct fsm_t* fsm, int new_state_idx, int event, void* event_arg,
                                        int event_arg_len);

/* state machine action function type */
enum fsm_action_func_t {
    fsm_action_func_process,
    fsm_action_func_enter,
    fsm_action_func_leave,
    fsm_action_func_last,
};

/* finite state machine class */
struct fsm_t {
    const struct fsm_state_t* states;
    int num_of_states;
    int cur_state_idx;

    void* ref;

    char** event_names;
    int num_of_event_names;
    char** state_names;
    int num_of_state_names;
};

/* finite state entity */
struct fsm_state_t {
    fsm_action_func_ptr actions[fsm_action_func_last];

};

const char* fsm_get_current_state_name(struct fsm_t* fsm);
const char* fsm_get_event_name(struct fsm_t* fsm, int event);
const char* fsm_get_state_name(struct fsm_t* fsm, int state_idx);
int fsm_get_current_state_index(struct fsm_t* fsm);
int fsm_init(struct fsm_t *fsm, const struct fsm_state_t *states, int num_of_states, void* ref);
int fsm_perform_switch_state_by_event(struct fsm_t* fsm, int event, void* event_arg, int event_arg_len,
                                      int new_state_idx);
int fsm_prep_switch_state_by_event(struct fsm_t* fsm, int event, void* event_arg, int event_arg_len, const int* states,
                                   int num_of_states);
void fsm_fini(struct fsm_t* fsm);
void fsm_post_event(struct fsm_t *fsm, int event, void *event_arg, int event_arg_len);
void fsm_register_event_names(struct fsm_t* fsm, char** event_names, int num_of_event_names);
void fsm_register_state_names(struct fsm_t* fsm, char** state_names, int num_of_state_names);
void fsm_switch_state(struct fsm_t *fsm, int new_state_idx, int event, void *event_arg, int event_arg_len);

#endif
