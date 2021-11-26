/*
 * leds_ops.c
 *
 * Implementing functions for configuring underlying LEDs system
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

#include "rdb_helper.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define LED_RDB_NAME_LEN 64
static char led_rdb_name[LED_RDB_NAME_LEN];

/*
 * set_led. See leds_ops.h.
 */
int set_led(const char* led_name, const char *mode) {
    int rval = 0;

    rval = snprintf(led_rdb_name, sizeof(led_rdb_name),"system.leds.%s", led_name);
    if (rval >= sizeof(led_rdb_name)){
        return -ENOMEM;
    }
    else if (rval < 0){
        return rval;
    }

    char* current_led_mode = get_rdb(led_rdb_name);
    if (!current_led_mode || strcmp(current_led_mode, mode)) {
        rval = set_rdb(led_rdb_name, mode);
    }

    return rval;
}
