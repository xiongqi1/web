/*
 * qmirdbctrl - QMI RDB voice command control.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
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

#include "qmirdbctrl.h"

struct dbhash_element_t rcchp_cmds[] = {
    {"ANSWER", vc_answer},
    {"CALL", vc_call},
    {"PCALL", vc_pcall},
    {"OCALL", vc_ocall},
    {"ECALL", vc_ecall},
    {"HANGUP", vc_hangup},
    {"SETUP_ANSWER", vc_hangup},
    {"REJECT", vc_reject},
    {"CALL_NOTIFY", vc_call_notify},
    {"DTMF_NOTIFY", vc_dtmf_notify},
    {"SUPPL_FORWARD", vc_suppl_forward},
    {"SET_TTY_MODE", vc_set_tty_mode},

    {"START_DTMF", vc_start_dtmf},
    {"STOP_DTMF", vc_stop_dtmf},

    {"MANAGE_CALLS", vc_manage_calls},
    {"MANAGE_IP_CALLS", vc_manage_ip_calls},

    {"INIT_VOICE_CALL", vc_init_voice_call},
    {"FINI_VOICE_CALL", vc_fini_voice_call},
};

int rcchp_cmds_count = sizeof(rcchp_cmds) / sizeof(rcchp_cmds[0]);
