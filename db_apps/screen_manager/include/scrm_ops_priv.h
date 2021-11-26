/*
 * scrm_ops_priv.h
 *    Screen manager private core defines.
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
#ifndef __SCRM_OPS_PRIV_H__
#define __SCRM_OPS_PRIV_H__

#define SCRM_HEARTBEAT_TICK_MSEC 1000
#define SCRM_HEARTBEAT_BUF_LEN 6
#define SCRM_HEARTBEAT_CHAR '.'

typedef enum scrm_screen_type_ {
    SCRM_SCREEN_BASE,
    SCRM_SCREEN_MENU,
    SCRM_SCREEN_MESSAGE,
    SCRM_SCREEN_PROCESSING,
} scrm_screen_type_t;

/*
 * State for a processing screen. Used in the hearbeat "tick" callback.
 */
typedef struct scrm_processing_screen_state_ {
    ngt_event_t *hearbeat_timer;
    ngt_label_t *heartbeat_label;
    unsigned int counter;
    char heartbeat_text[SCRM_HEARTBEAT_BUF_LEN];
    ngt_event_t *heartbeat_timer;
} scrm_processing_screen_state_t;

/*
 * State for a scroll screen. Used in the on_scroll callback to update
 * the current page number in the header line.
 */
typedef struct scrm_scroll_screen_state_ {
    ngt_label_t *header_label;
    char header_text[SCRM_MAX_TEXT_LEN];
} scrm_scroll_screen_state_t;

typedef struct scrm_screen_ {
    scrm_screen_type_t type;
    void *screen_handle;
    scrm_screen_callback_t timeout_cb;
    void *timeout_cb_arg;
    ngt_event_t *timeout_timer;
    union {
        scrm_processing_screen_state_t processing;
        scrm_scroll_screen_state_t scroll;
    } state;
} scrm_screen_t;

#endif /* __SCRM_OPS_PRIV_H__ */
