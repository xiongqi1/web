/*
 * leds_model_base.c
 *
 * Implementing base LEDs model
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

#include "leds_model.h"
#include "leds_model_base.h"
#include "linked_list.h"
#include "event_loop.h"
#include "leds_ops.h"
#include "rdb_helper.h"

#include <stdlib.h>
#include <string.h>

/*
 * init. See leds_model.h.
 */
static int init(leds_model *self) {
    // derived class/object should add triggers and period monitors to list

    // do any necessary procedure after adding triggers and period monitors
    int rval;
    if ((rval = self->post_adding_triggers(self))
            || (rval = self->post_adding_periodic_monitors(self))) {
        return rval;
    } else {
        return 0;
    }
}

/*
 * add_trigger. See leds_model.h.
 */
static int add_trigger(leds_model *self, event_trigger* trigger) {
    int rval = subscribe_rdb(trigger->rdb_name);
    if (rval) {
        return rval;
    }
    linked_list_add_head(&self->triggers_list, &trigger->list);
    return 0;
}

/*
 * post_adding_triggers. See leds_model.h.
 */
static int post_adding_triggers(leds_model *self) {
    int rval = post_subscribe_rdb();
    if (rval) {
        return rval;
    }
    // manually run all trigger handlers to process current system state
    event_trigger *trigger_iter;
    linked_list_for_each(trigger_iter, &self->triggers_list, event_trigger, list) {
        trigger_iter->handler(trigger_iter);
    }
    return event_loop_add_fd(get_rdb_fd(), self->trigger_handler, self);
}

/*
 * add_periodic_monitor. See leds_model.h.
 */
static int add_periodic_monitor(leds_model *self, periodic_monitor* monitor) {
    linked_list_add_head(&self->periodic_monitors_list, &monitor->list);
    return 0;
}

/*
 * post_adding_periodic_monitors. See leds_model.h.
 */
static int post_adding_periodic_monitors(leds_model *self) {
    if (!linked_list_empty(&self->periodic_monitors_list)) {
        return event_loop_add_timer(self->monitor_period_secs, self->monitor_period_usecs, self->monitor_handler, self);
    }
    return 0;
}

/*
 * monitor_handler. See leds_model.h.
 */
static void monitor_handler(void *data) {
    leds_model *self = (leds_model *) data;
    periodic_monitor *monitor_iter;
    linked_list_for_each(monitor_iter, &self->periodic_monitors_list, periodic_monitor, list) {
        if (monitor_iter->handler){
            monitor_iter->handler(monitor_iter);
        }
    }
    event_loop_add_timer(self->monitor_period_secs, self->monitor_period_usecs, self->monitor_handler, self);
}

/*
 * trigger_handler. See leds_model.h.
 */
static void trigger_handler(int fd, void *data) {
    leds_model *self = (leds_model *) data;
    char *triggered_rdb_names;
    triggered_rdb_names = get_triggered_rdb();
    if (triggered_rdb_names) {
        char *pstring;
        char *rdb_name;
        pstring = triggered_rdb_names;
        // for each triggered RDB, find matched handler to invoke
        while ((rdb_name = strsep(&pstring, "&")) != NULL){
            event_trigger *trigger_iter, *trigger_iter_saved;
            linked_list_for_each_safe(trigger_iter, trigger_iter_saved, &self->triggers_list, event_trigger, list) {
                if (!strcmp(trigger_iter->rdb_name, rdb_name) && trigger_iter->handler) {
                    trigger_iter->handler(trigger_iter);
                    break;
                }
            }
        }
    }
}

/*
 * deinit. See leds_model.h.
 */
static void deinit(leds_model *self) {
    periodic_monitor *monitor_iter, *monitor_iter_saved;
    event_trigger *trigger_iter, *trigger_iter_saved;

    if (!linked_list_empty(&self->periodic_monitors_list)) {
        event_loop_del_timer(self->monitor_handler, self);
    }

    event_loop_del_fd(get_rdb_fd());

    linked_list_for_each_safe(monitor_iter, monitor_iter_saved, &self->periodic_monitors_list, periodic_monitor, list) {
        periodic_monitor__delete(monitor_iter);
    }

    linked_list_for_each_safe(trigger_iter, trigger_iter_saved, &self->triggers_list, event_trigger, list) {
        event_trigger__delete(trigger_iter);
    }
}

/*
 * base_leds_model__new. See leds_model_base.h.
 */
leds_model *base_leds_model__new(leds_model *base_model) {
    leds_model *model = (leds_model*) calloc(1, sizeof(*model));
    if (!model) {
        return NULL;
    }

    model->base = base_model;

    linked_list_init(&model->triggers_list);
    linked_list_init(&model->periodic_monitors_list);

    model->monitor_period_secs = 1;
    model->monitor_period_usecs = 0;

    model->init = init;
    model->deinit = deinit;
    model->add_trigger = add_trigger;
    model->add_periodic_monitor = add_periodic_monitor;
    model->post_adding_triggers = post_adding_triggers;
    model->post_adding_periodic_monitors = post_adding_periodic_monitors;
    model->monitor_handler = monitor_handler;
    model->trigger_handler = trigger_handler;

    return model;
}

