#ifndef QMI_SVCVER_H_15405612052016
#define QMI_SVCVER_H_15405612052016
/*
 * QMI service version.
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

#include "minilib.h"

struct qmi_service_version_t {
    unsigned short major;
    unsigned short minor;
} __packed;

extern int _qmi_check_versions(void);

extern int (* _qmi_get_iccid)(int);
extern int (* _qmi_get_imsi)(int);
extern int (* _qmi_check_pin_status)(int);
extern int (* _qmi_control_pin)(int, const char *, int, int *, int *);
extern int (* _qmi_change_pin)(int, const char *, const char *, int *, int *);
extern int (* _qmi_verify_puk)(int, const char *, const char *, int *, int *);

extern void (* _batch_cmd_stop_gps)(void);
extern int (* _batch_cmd_start_gps)(int);
#endif
