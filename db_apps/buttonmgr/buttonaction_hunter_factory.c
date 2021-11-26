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

#define NOTI_COMMAND_WPS_ON "wps on"
#define NOTI_COMMAND_WPS_OFF "wps off"
#define NOTI_COMMAND_POWER_OFF "power off"
#define NOTI_COMMAND_POWER_ON "power on"

///////////////////////////////////////////////////////////////////////////////////////////////////
// button functions for Hunter factory
///////////////////////////////////////////////////////////////////////////////////////////////////

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
    syslog(LOG_DEBUG, "got wps event (depressed=%d)", depressed);

    button_exec_call_script(depressed ? NOTI_COMMAND_WPS_ON : NOTI_COMMAND_WPS_OFF);
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
    syslog(LOG_DEBUG, "got lid event (depressed=%d)", depressed);

    button_exec_call_script(!depressed ? NOTI_COMMAND_POWER_ON : NOTI_COMMAND_POWER_OFF);
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
