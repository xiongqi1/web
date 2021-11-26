/*
 * scrm_features.c
 *    Screen Manager weak definitions for feature plugins.
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
#include "scrm_features.h"

/*
 * Each screen UI feature should have a single weak entry in this file
 * and a non-weak real implementation in the feature code.
 */

/* V_BLUETOOTH */
scrm_feature_plugin_t scrm_bluetooth_plugin __attribute__ ((weak));
/* V_WIFI */
scrm_feature_plugin_t scrm_wifi_plugin __attribute__ ((weak));
/* V_MODULE */
scrm_feature_plugin_t scrm_module_plugin __attribute__ ((weak));
/* V_POWER_SCREEN_UI */
scrm_feature_plugin_t scrm_power_plugin __attribute__ ((weak));
/* V_LEDFUN */
scrm_feature_plugin_t scrm_led_plugin __attribute__ ((weak));
scrm_led_vtable_t *scrm_led_vtable;
/* V_SPEAKER */
scrm_feature_plugin_t scrm_speaker_plugin __attribute__ ((weak));
scrm_speaker_vtable_t *scrm_speaker_vtable;
