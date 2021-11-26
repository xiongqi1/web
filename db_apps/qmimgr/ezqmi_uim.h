#ifndef EZQMI_UIM_H_15422713052016
#define EZQMI_UIM_H_15422713052016
/*
 * QMI user identify module service.
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "qmimsg.h"

extern int
_qmi_uim_get_iccid(int wait);

extern int
_qmi_uim_check_pin_status(int wait);

extern int
_qmi_uim_control_pin(int pin_id, const char* pin, int action,
                     int* verify_left, int* unblock_left);

extern int
_qmi_uim_change_pin(int pin_id, const char* pin, const char* newpin,
                    int* verify_left, int* unblock_left);

extern int
_qmi_uim_verify_puk(int pin_id, const char* puk, const char* pin,
                    int* verify_left, int* unblock_left);

extern void
qmimgr_callback_on_uim(unsigned char msg_type,struct qmimsg_t* msg,
                       unsigned short qmi_result, unsigned short qmi_error,
                       int noti, unsigned short tran_id);

#endif
