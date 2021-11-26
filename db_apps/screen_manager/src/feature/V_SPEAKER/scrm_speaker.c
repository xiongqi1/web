/*
 * scrm_speaker.c
 *    Screen UI Speaker support. Provides the Speaker top level menu as well as
 *    the Speaker vtable. The Speaker vtable exports speaker functionality to
 *    the other components of the screen manager via a controlled API.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or
 * object forms) without the expressed written consent of NetComm Wireless Pty.
 * Ltd Copyright laws and International Treaties protect the contents of this
 * file. Unauthorized use is prohibited.
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <linux/input.h>

#include <scrm.h>
#include <scrm_ops.h>
#include <rdb_ops.h>
#include <ngt.h>

#define SCRM_SPEAKER_BUF_SIZE 64
#define SCRM_SPEAKER_MAX_CMD_LEN 256
#define SCRM_SOUND_SYS_PATH "/usr/lib/sounds/"

static bool g_speaker_enable;
static int g_speaker_menu_id = -1;
static struct rdb_session *g_rdb_s;
static ngt_event_t *g_rdb_event;
static char *g_status_sound[SCRM_STATUS_MAX];

/*
 * Generates the command line to play a sound file.
 * The return value is the value returned by snprintf. That is, -1 on general
 * error and the length of the string not including the NUL terminator otherwise.
 * Thus, as for snprintf, the a return value of cmd_len or more means that the
 * output was truncated.
 */
static int
speaker_cmd_gen (char *cmd_buf, unsigned int cmd_len, const char *file_name)
{
    /*
     * The file name is assumed to be a full path if it starts with '/'.
     * Otherwise the file name is prepended with the system sound path.
     */
    return (snprintf(cmd_buf, cmd_len, "speaker-play < %s%s",
                     (file_name[0] == '/') ? "" : SCRM_SOUND_SYS_PATH, file_name));
}

/*
 * Speaker plugin API implementation. Play the given sound file.
 */
static int
speaker_play_sound (const char *file_name)
{
    int rval = 0;

    if (!file_name) {
        return -EINVAL;
    }

    assert(g_rdb_s);

    if (g_speaker_enable) {
        char cmd[SCRM_SPEAKER_MAX_CMD_LEN];
        char *cmd_p;
        int cmd_len;

        /* First try to generate the speaker play command using a static buffer */
        cmd_p = cmd;
        cmd_len = sizeof(cmd);
        rval = speaker_cmd_gen(cmd_p, cmd_len, file_name);

        if (rval >= cmd_len) {
            /*
             * Command does not fit in the static buffer. Allocate a dynamic
             * buffer large enough to fit the command and re-generate the command.
             */
            cmd_len = rval + 1;
            cmd_p = malloc(cmd_len);
            if (!cmd_p) {
                return -errno;
            }
            rval = speaker_cmd_gen(cmd_p, cmd_len, file_name);
            assert(rval < cmd_len);
        }

        if (rval > 0) {
            /* Run the command */
            rval = system(cmd_p);
            if (rval) {
                rval = -1;
            }

            /* Free the command buffer if it was dynamically allocated. */
            if (cmd_p != cmd) {
                free(cmd_p);
            }
        }
    }

    return rval;
}

/*
 * Speaker plugin API implementation. Plays a system sound to indicate the
 * given screen manager status.
 */
static int
speaker_play_status (scrm_status_t status)
{
    assert(g_rdb_s);

    if (status >= SCRM_STATUS_MAX) {
        return -EINVAL;
    }

    /*
     * Play a sound if the speaker is enabled and there is a system sound
     * associated with the given status.
     */
    if (g_speaker_enable && g_status_sound[status]) {
        return speaker_play_sound(g_status_sound[status]);
    }

    return 0;
}

/*
 * Re-reads and processes the speaker configuration from RDB.
 */
static int
process_speaker_conf (void)
{
    int rval = 0;
    int enable;
    unsigned int ix;
    const char *enable_str;
    char buf[SCRM_SPEAKER_BUF_SIZE];
    int len;

    /*
     * Maps scrm status values to rdb variables containing the sound
     * file for each status value.
     */
    struct map {
        scrm_status_t status;
        const char *rdb_var;
    } status_rdb_map[] = {
        { SCRM_STATUS_SUCCESS, RDB_SCRM_CONF_SOUND_SUCCESS_VAR },
        { SCRM_STATUS_FAIL, RDB_SCRM_CONF_SOUND_FAIL_VAR },
        { SCRM_STATUS_ALERT, RDB_SCRM_CONF_SOUND_ALERT_VAR },
    };
    unsigned int num_var = sizeof(status_rdb_map) / sizeof(struct map);

    assert(g_rdb_s);

    INVOKE_CHK(rdb_get_int(g_rdb_s, RDB_SCRM_CONF_SPKR_ENABLE_VAR, &enable),
               "Unable to get rdb var %s", RDB_SCRM_CONF_SPKR_ENABLE_VAR);

    g_speaker_enable = enable ? true : false;

    assert(g_speaker_menu_id >= 0);

    /*
     * Set the Speaker menu item text. The menu item toggles the state
     * so the text string is opposite to current enable value.
     */
    if (g_speaker_enable) {
        enable_str = _("Disable");
    } else {
        enable_str = _("Enable");
    }
    snprintf(buf, sizeof(buf),  _("%s Speaker"), enable_str);
    scrm_set_top_menu_item_text(g_speaker_menu_id, buf);

    /* Get the sound file name for each scrm status that has a sound. */
    for (ix = 0; ix < num_var; ix++) {
        len = 0;
        rval = rdb_get_alloc(g_rdb_s, status_rdb_map[ix].rdb_var,
                             &(g_status_sound[status_rdb_map[ix].status]), &len);

        if (rval) {
            /* Log the error but continue reading rest of the sound files. */
            assert(0);
            errp("Unable to get rdb var %s", status_rdb_map[ix].rdb_var);
        }

        dbgp("speaker status = %d, sound = %s\n", status_rdb_map[ix].status,
             g_status_sound[status_rdb_map[ix].status]);
    }

 done:
    return rval;
}

static scrm_speaker_vtable_t speaker_vtable = {
    speaker_play_sound,
    speaker_play_status,
};

/*
 * Event handler for rdb notifications.
 */
static int
speaker_rdb_event_handler (void *arg)
{
    UNUSED(arg);

    /* Only conf variables are subscribed. Process the conf again. */
    return process_speaker_conf();
}

/*
 * Handler for the OK button press on the Speaker enabe/disable result
 * screen. Just closes and destroys the result screen.
 */
static int
result_ok_handler (void *screen_handle, ngt_widget_t *widget, void *arg)
{
    UNUSED(widget);
    UNUSED(arg);

    scrm_screen_close(screen_handle);
    scrm_screen_destroy(&screen_handle);

    return 0;
}

/*
 * Speaker enable/disable menu option select handler.
 */
static int
speaker_toggle_selected (ngt_widget_t *widget, void *arg)
{
    int rval;
    char buf[SCRM_SPEAKER_BUF_SIZE];
    scrm_status_t status;
    void *screen_handle;
    scrm_button_request_t button_req[] = {
        { BTN_1, OK_LABEL, result_ok_handler, NULL }
    };

    UNUSED(arg);
    UNUSED(widget);

    assert(g_rdb_s);

    /* toggle enable value */
    g_speaker_enable = !g_speaker_enable;

    INVOKE_CHK(rdb_set_string(g_rdb_s, RDB_SCRM_CONF_SPKR_ENABLE_VAR,
                              g_speaker_enable ? "1" : "0"),
               "Unable to set rdb var %s", RDB_SCRM_CONF_SPKR_ENABLE_VAR);

 done:
    if (!rval) {
        status = SCRM_STATUS_SUCCESS;
        snprintf(buf, sizeof(buf), _("Speaker %s."),
                 g_speaker_enable ? _("enabled") : _("disabled"));
    } else {
        status = SCRM_STATUS_FAIL;
        snprintf(buf, sizeof(buf), _("Failed to %s Speaker."),
                 g_speaker_enable ? _("enabled") : _("disabled"));
    }

    /* Show a result screen */
    scrm_message_screen_create(buf, button_req,
                               sizeof(button_req) / sizeof(scrm_button_request_t),
                               &screen_handle);

    scrm_screen_show_with_timeout(screen_handle, status, -1, NULL, NULL);

    return rval;
}

/*
 * Clean up.
 */
static void
speaker_destroy (void)
{
    unsigned int ix;

    if (g_rdb_event) {
        ngt_run_loop_delete_event(g_rdb_event);
        g_rdb_event = NULL;
    }

    if (g_rdb_s) {
        rdb_close(&g_rdb_s);
    }

    for (ix = 0; ix < sizeof(g_status_sound) / sizeof(char *); ix++) {
        free(g_status_sound[ix]);
        g_status_sound[ix] = NULL;
    }
}

/*
 * Initialisation. Called once at the beginning of time.
 */
static int
speaker_init (void)
{
    int rval;

    scrm_speaker_vtable = &speaker_vtable;

    /* Create a top level menu option to enable/disable the speaker. */
    g_speaker_menu_id =
        scrm_add_top_menu_item("", speaker_toggle_selected, NULL);

    if (g_speaker_menu_id < 0) {
        errp("Unable to add Speaker top menu item");
        speaker_destroy();

        /* holds an error code in failure case. */
        return g_speaker_menu_id;
    }

    INVOKE_CHK(rdb_open(NULL, &g_rdb_s), "Unable to open rdb");

    process_speaker_conf();

    /*
     * Subscribe to and hook up notifications for the Speaker enable variable.
     * The enable var is also a trigger for processing all other speaker
     * configuration values.
     */
    INVOKE_CHK(rdb_subscribe(g_rdb_s, RDB_SCRM_CONF_SPKR_ENABLE_VAR),
               "Unable to subscribe to rdb var %s",
               RDB_SCRM_CONF_SPKR_ENABLE_VAR);

    g_rdb_event = ngt_run_loop_add_fd_event(rdb_fd(g_rdb_s),
                                            speaker_rdb_event_handler,
                                            NULL);
    INVOKE_CHK((!g_rdb_event), "Unable to add fd event");

 done:
    if (rval) {
        speaker_destroy();
    }
    return rval;
}

scrm_feature_plugin_t scrm_speaker_plugin = {
    speaker_init,
    speaker_destroy,
};
