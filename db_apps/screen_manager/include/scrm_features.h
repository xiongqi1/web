/*
 * scrm_features.h
 *    Screen Manager common feature defines.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
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
#ifndef __SCRM_FEATURES_H__
#define __SCRM_FEATURES_H__

/*
 * Every Screen Manager feature needs to define two instances of this plugin.
 *    1. A NULL weak definition in src/core/scrm_features.c. This is used when
 *       The feature is not enabled (via V Variables) on a platform.
 *    2. A non-weak non-NULL definition in src/feature/<V_Variable>. This
 *       is used when the feature is enabled when <V_Variable> is any value
 *       except "none".
 *
 * The plugin must then be added to the src/core/scrm_main.c "feature_plugins"
 * array.
 */
typedef struct scrm_feature_plugin_ {
    /*
     * Feature specific Screen Manager initialisation. Called once
     * during Screen Manager start up. This function can do any feature
     * specific initialisation but will typically also add one or more menu
     * items into the top level menu.
     *
     * Parameters:
     *    None.
     *
     * Returns:
     *    0 on success. Negative error code otherwise.
     */
    int (*init)(void);

    /*
     * Feature specific Screen Manager clean up. Called once
     * during Screen Manager shut down.
     *
     * Parameters:
     *    None.
     *
     * Returns:
     *    None.
     */
    void (*destroy)(void);
} scrm_feature_plugin_t;

/* V_BLUETOOTH */
extern scrm_feature_plugin_t scrm_bluetooth_plugin;
/* V_WIFI */
extern scrm_feature_plugin_t scrm_wifi_plugin;
/* V_MODULE */
extern scrm_feature_plugin_t scrm_module_plugin;
/* V_POWER_SCREEN_UI */
extern scrm_feature_plugin_t scrm_power_plugin;

/* V_LEDFUN */
extern scrm_feature_plugin_t scrm_led_plugin;

/* Forward declaration */
typedef enum scrm_status_ scrm_status_t;

/* vtable for LED support */
typedef struct scrm_led_vtable_ {
    /*
     * Show a given dispd LED pattern. This overrides any system level LED
     * behaviour until the pattern is unset.
     *
     * Parameters:
     *    pattern    The dispd pattern to show. Pass either an empty string or
     *               NULL to unset the pattern and revert to system level LED
     *               behaviour.
     *
     * Returns:
     *    0 on success. Negative error code otherwise.
     */
    int (*show_pattern)(const char *pattern);

    /*
     * Indicate a given screen manager status via the LED. This overrides any
     * system level LED behaviour until the status is set to SCRM_STATUS_NONE.
     *
     * Parameters:
     *    status    The screen manager status to show. Set to SCRM_STATUS_NONE
     *              to stop currently shown status and revert to system level
     *              LED behaviour.
     *
     * Returns:
     *    0 on success. Negative error code otherwise.
     */
    int (*show_status)(scrm_status_t status);
} scrm_led_vtable_t;

/*
 * LED vtable. If LED support is present this will be set by the scrm V_LEDFUN
 * feature code. Otherwise it will be NULL.
 */
extern scrm_led_vtable_t *scrm_led_vtable;

/* V_SPEAKER */
extern scrm_feature_plugin_t scrm_speaker_plugin;

/* vtable for speaker support */
typedef struct scrm_speaker_vtable_ {
    /*
     * Play a sound file.
     *
     * Parameters:
     *    file_name    The sound file to play. If a full path is given (ie,
     *                 file_name starting with '/') then it will be used as is
     *                 to access the sound file. Otherwise the given file_name
     *                 will be prefixed with the system sound path,
     *                 /usr/lib/sounds.
     *
     * Returns:
     *    0 on success. Negative error code otherwise.
     */
    int (*play_sound)(const char *file_name);

    /*
     * Play a system sound that indicates the given screen manager status.
     *
     * Parameters:
     *    status    The screen manager status to indicate.
     *
     * Returns:
     *    0 on success. Negative error code otherwise.
     */
    int (*play_status)(scrm_status_t status);
} scrm_speaker_vtable_t;

/*
 * Speaker vtable. If speaker support is present this will be set by the
 * scrm V_SPEAKER feature code. Otherwise it will be NULL.
 */
extern scrm_speaker_vtable_t *scrm_speaker_vtable;

#endif /* __SCRM_FEATURES_H__ */
