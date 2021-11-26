/*
 * LEDs manager: implementing business logic to control LEDs
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

#include "event_loop.h"
#include "rdb_helper.h"
#include "leds_model.h"

#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    int rval = 0;

    openlog("leds_manager", LOG_PID | LOG_PERROR, LOG_DEBUG);
    rval = init_rdb();
    if (rval) {
        deinit_rdb();
        syslog(LOG_ERR, "Unable to open rdb");
        closelog();
        return rval;
    }

    rval = event_loop_init();
    if (rval){
        syslog(LOG_ERR, "Failed to initialise event loop");
    } else {
        leds_model *main_leds_model = setup_leds_model();

        if (main_leds_model) {
            /* run event loop */
            event_loop_run();
        } else {
            syslog(LOG_ERR, "Failed to initialise LEDs model");
            rval = -1;
        }
        leds_model__delete(main_leds_model);
    }

    event_loop_free();
    deinit_rdb();
    closelog();

    return rval;
}
