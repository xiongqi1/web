/*
 * Button execution.
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

#include "buttonexec.h"
#include "lstddef.h"
#include "buttonaction.h"
#include "iomutex.h"

#include <linux/input.h>
#include <linux/stddef.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>

#define KEY_TYPE_CODE(type,code) (((type)<<16) | ((code)&0xffff))


static struct button_exec_hash_entry_t* _button_exec_hash_head = NULL;
static const char* _noti_script = NULL;

static struct iomutex_t* mutex = NULL;


#define MAX_COMMAND_LINE_LEN 1024

/**
 * @brief calls notification script with parameters.
 *
 * @param opt is parameters for notification script.
 *
 * @return is the result from system().
 */
int button_exec_call_script(const char* opt)
{
    char cmd[MAX_COMMAND_LINE_LEN];
    int rc;

    snprintf(cmd, sizeof(cmd), "%s %s", _noti_script, opt);

    syslog(LOG_DEBUG, "prepare to run noti script (cmd=%s)", cmd);

    iomutex_enter(mutex);

    syslog(LOG_DEBUG, "run noti script (cmd=%s)", cmd);
    rc = system(cmd);

    iomutex_leave(mutex);

    return rc;
}

/**
 * @brief calls a corresponding function to key type and key code.
 *
 * @param evdev is an evdev object.
 * @param key_type is key type.
 * @param key_code is key code.
 * @param depressed is a depressed flag (0=released, 1 depressed)
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int button_exec_call_func(struct evdev_t* evdev, int key_type, int key_code, int depressed)
{
    int key_type_code;
    struct button_exec_hash_entry_t* be;

    key_type_code = KEY_TYPE_CODE(key_type, key_code);

    HASH_FIND_INT(_button_exec_hash_head, &key_type_code, be);

    if (!be) {
        syslog(LOG_ERR, "unknown button detected (type=%d,code=%d)", key_type, key_code);
        goto err;
    }

    /* execute exec. function */
    be->button_exec_func(evdev,  key_type, key_code, depressed);

    return 0;

err:
    return -1;
}

/**
 * @brief destroys hashes.
 */
void button_exec_fini(void)
{
    struct button_exec_hash_entry_t* be;
    struct button_exec_hash_entry_t* tmp;

    /* free button exec hash entries */
    HASH_ITER(hh, _button_exec_hash_head, be, tmp) {
        HASH_DEL(_button_exec_hash_head, be);
        free(be);
    }

    iomutex_destroy(mutex);
}

/**
 * @brief initiates hashes accordingly to buttonaction.
 *
 * @param noti_script is notification script full path name.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int button_exec_init(const char* noti_script)
{
    struct button_exec_hash_entry_t* be;
    const struct button_exec_setup_t* bs;
    int i;

    /* create iomutex */
    mutex = iomutex_create("hunter-power-saving");
    if (!mutex) {
        syslog(LOG_ERR, "failed to create iomutex");
        goto err;
    }

    /* update script name */
    _noti_script = noti_script;

    for (i = 0; i < button_exec_setup_count; i++) {
        /* allocate memory for object */
        be = (struct button_exec_hash_entry_t*)calloc(1, sizeof(*be));
        if (!be) {
            syslog(LOG_ERR, "failed to allocate button exec hash entry");
            goto err;
        }

        /* update members */
        bs = &button_exec_setup[i];
        be->key_type_code = KEY_TYPE_CODE(bs->key_type, bs->key_code);
        be->button_exec_func = bs->button_exec_func;

        HASH_ADD_INT(_button_exec_hash_head, key_type_code, be);
    }

    return 0;

err:
    return -1;
}

