/*
 * Button action.
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

#include "buttonaction.h"

#include <linux/input.h>
#include <linux/stddef.h>
#include <syslog.h>

#include "buttonexec.h"
#include "lstddef.h"
#include "timer.h"

#define WPS_TIMEOUT_MSEC 1000

#define NOTI_COMMAND_WPS_PRESS "wps press"
#define NOTI_COMMAND_WPS_HOLD "wps hold"
#define NOTI_COMMAND_POWER_OFF "power off"
#define NOTI_COMMAND_POWER_OFF2 "power off2"
#define NOTI_COMMAND_POWER_ON "power on"

#define TIMER_WPS "timer:wps"

///////////////////////////////////////////////////////////////////////////////////////////////////
// button functions for Hunter
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief executes WPS script when WPS button timer is expired.
 *
 * @param te is a Timer entry object.
 * @param now_msec is monotonic clock of current time.
 * @param ref is a reference pointer.
 */
static void button_exec_func_wps_cb(struct timer_entry_t* te, timer_monotonic_t now_msec, void* ref)
{
    button_exec_call_script(NOTI_COMMAND_WPS_HOLD);
}

/**
 * @brief handles WPS key.
 *
 * @param evdev is an evdev object.
 * @param key_type is key type.
 * @param key_code is key code.
 * @param depressed is a depress flag (0=released, 1=depressed).
 */
static void button_exec_func_wps(struct evdev_t* evdev, int key_type, int key_code, int depressed)
{
    syslog(LOG_DEBUG, "got wps event");

    if (depressed) {
        button_exec_call_script(NOTI_COMMAND_WPS_PRESS);
        timer_set(TIMER_WPS, WPS_TIMEOUT_MSEC, button_exec_func_wps_cb, NULL);
    } else {
        timer_cancel(TIMER_WPS);
    }
}

/**
 * @brief handles LID key.
 *
 * @param evdev is an evdev object.
 * @param key_type is key type.
 * @param key_code is key code.
 * @param depressed is a depress flag (0=released, 1=depressed).
 */
static void button_exec_func_lid(struct evdev_t* evdev, int key_type, int key_code, int depressed)
{
    int flag;

    if (depressed) {
        syslog(LOG_DEBUG, "got lid event depressed");

        /* execute top-half */
        button_exec_call_script(NOTI_COMMAND_POWER_OFF);

        /*
            ## note ##

            1. power switch release event is not reliable when the system
               wakes up. Instead of relying on the event, we directly read
               flags.

            2. event 'on' (top-half) and event 'on2' (bottom-half) are to
               minimize the race condition window that we lose raising or
               falling edge event.

            TODO:
              top-half and bottom-half mechanism does not completely eliminate
              the race condition window. hardware improvement is required in the
              future.

        */

        flag = evdev_get_key_flag(evdev, key_type, key_code);
        syslog(LOG_DEBUG, "prepare bottom-half execution (flag=%d)", flag);

        evdev_set_key_flag_without_callback(evdev, key_type, key_code, flag);

        /* execute bottomhalf if flag not changed */
        if (flag) {
            syslog(LOG_DEBUG, "execute bottom-half (flag=%d)", flag);

            button_exec_call_script(NOTI_COMMAND_POWER_OFF2);
        } else {
            syslog(LOG_DEBUG, "skip bottom-half, call lid off (flag=%d)", flag);
            button_exec_call_script(NOTI_COMMAND_POWER_ON);
        }
    } else {
        syslog(LOG_DEBUG, "got lid event released");
        button_exec_call_script(NOTI_COMMAND_POWER_ON);
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// key defines for Hunter
///////////////////////////////////////////////////////////////////////////////////////////////////

struct button_exec_setup_t button_exec_setup[EV_CNT] = {
    {EV_KEY, KEY_WPS_BUTTON, button_exec_func_wps},
    {EV_SW,  SW_LID,         button_exec_func_lid},
};

const int button_exec_setup_count = COUNTOF(button_exec_setup);


/**
 * @brief initiates Button action module.
 */
void buttonaction_init(void)
{

}

/**
 * @brief finalize Button action module.
 */
void buttonaction_fini(void)
{
}
