/*
 * leds_model.c
 *
 * Implementing common interfaces related to all LEDs model implementation
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
#include <string.h>
#include <stdlib.h>

/*
 * event_trigger__new. See leds_model.h.
 */
event_trigger *event_trigger__new(const char* rdb_name, leds_model *model, void (*handler)(event_trigger *)) {
    event_trigger *trigger = (event_trigger *) calloc(1, sizeof(*trigger));
    if (trigger) {
        linked_list_init(&trigger->list);
        trigger->rdb_name = strdup(rdb_name);
        trigger->data = model;
        trigger->handler = handler;
        return trigger;
    } else {
        return NULL;
    }
}

/*
 * event_trigger__delete. See leds_model.h.
 */
void event_trigger__delete(event_trigger *trigger) {
    if (trigger) {
        linked_list_del(&trigger->list);
        free(trigger->rdb_name);

        /* data is a leds_model so it is not free-ed here */

        free(trigger);
    }
}

/*
 * periodic_monitor__new. See leds_model.h.
 */
periodic_monitor *periodic_monitor__new(leds_model *model, void (*handler)(periodic_monitor *)) {
    periodic_monitor *monitor = (periodic_monitor *) calloc(1, sizeof(*monitor));
    if (monitor) {
        linked_list_init(&monitor->list);
        monitor->data = model;
        monitor->handler = handler;
        return monitor;
    } else {
        return NULL;
    }
}

/*
 * periodic_monitor__delete. See leds_model.h.
 */
void periodic_monitor__delete(periodic_monitor *monitor) {
    if (monitor) {
        linked_list_del(&monitor->list);

        /* data is a leds_model so it is not free-ed here */

        free(monitor);
    }
}

/*
 * leds_model__delete. See leds_model.h.
 */
void leds_model__delete(leds_model *model) {
    if (model) {
        model->deinit(model);
        free(model);
    }
}
