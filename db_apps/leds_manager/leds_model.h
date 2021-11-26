/*
 * leds_model.h
 *
 * Defining generic LEDs business model interface to control LEDs which includes
 * periodic monitoring and event triggering. Individual platforms or products
 * should implement the interface to reflect their own business logics and processes.
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

#ifndef LEDS_MODEL_H_00000010102018
#define LEDS_MODEL_H_00000010102018

#include "linked_list.h"
#include "event_loop.h"

/*
 * event trigger
 */
typedef struct event_trigger_t {
    /* linking to model's trigger list */
    struct linked_list list;
    /* RDB variable name to subscribe for signal notification */
    char *rdb_name;
    /* internal data, usually pointing to model */
    void *data;
    /* handler function to be invoked while being triggered */
    void (*handler)(struct event_trigger_t *self);
} event_trigger;

/*
 * a monitoring entry to be invoked during periodic monitoring cycle
 */
typedef struct periodic_monitor_t {
    /* linking to model's monitoring list */
    struct linked_list list;
    /* internal data, usually pointing to model */
    void *data;
    /* handler function to be invoked periodically */
    void (*handler)(struct periodic_monitor_t *self);
} periodic_monitor;

/*
 * LEDs model
 */
typedef struct leds_model_t {
    /* base model */
    struct leds_model_t *base;
    /* trigger list */
    struct linked_list triggers_list;
    /* monitoring list */
    struct linked_list periodic_monitors_list;
    /* seconds of monitoring period */
    int monitor_period_secs;
    /* microseconds of monitoring period */
    int monitor_period_usecs;

    /* Method members. Parameter self represents current model. */
    /*
     * initialise current model
     */
    int (*init)(struct leds_model_t *self);
    /*
     * adding an event trigger
     * @param trigger event trigger to add
     * @return 0 on success or a negative number on error
     */
    int (*add_trigger)(struct leds_model_t *self, event_trigger* trigger);
    /*
     * adding a monitoring entry to periodic monitoring cycle
     * @param monitor monitoring entry to add
     * @return 0 on success or a negative number on error
     */
    int (*add_periodic_monitor)(struct leds_model_t *self, periodic_monitor* monitor);
    /*
     * necessary procedures to be invoked after adding all event triggers
     */
    int (*post_adding_triggers)(struct leds_model_t *self);
    /*
     * necessary procedures to be invoked after adding all periodic monitoring entries
     */
    int (*post_adding_periodic_monitors)(struct leds_model_t *self);
    /*
     * periodic monitor handler to be register to the event loop
     */
    event_loop_timer_handler monitor_handler;
    /*
     * RDB trigger handler to be register to the event loop
     */
    event_loop_fd_handler trigger_handler;
    /*
     * de-initialise current model
     */
    void (*deinit)(struct leds_model_t *self);
} leds_model;

/*
 * Create an event trigger
 * @param rdb_name RDB name to subscribe to get notification
 * @param model LEDs model to which this trigger is added
 * @param handler callback function to be invoked on being triggered by RDB notification
 * @return an event_trigger or NULL on error
 */
event_trigger *event_trigger__new(const char* rdb_name, leds_model *model, void (*handler)(event_trigger *));

/*
 * Delete an event trigger
 * @param trigger event trigger to delete
 */
void event_trigger__delete(event_trigger *trigger);

/*
 * Create a periodic monitoring entry
 * @param model LEDs model to which this trigger is added
 * @param handler callback function to be invoked in periodic monitoring cycle
 * @return a periodic_monitor or NULL on error
 */
periodic_monitor *periodic_monitor__new(leds_model *model, void (*handler)(periodic_monitor *));

/*
 * Delete an periodic monitoring entry
 * @param monitor periodic monitoring entry to delete
 */
void periodic_monitor__delete(periodic_monitor *monitor);

/*
 * Delete a LEDs model
 * @param model LEDS model to delete
 */
void leds_model__delete(leds_model *model);

/*
 * Setup LEDs model to be used in the program
 * @return LEDs model
 */
leds_model *setup_leds_model();

#endif
