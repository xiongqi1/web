/*!
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
*/
//
// A part of modem emulator end point application (db_apps/modem_emul_ep)
// Header file for hardware definitions
// This is included by modem_emul_ep.h
//
// Files dealing with low level h/w functions and not needing modem_emul_ep.h
// can include this file direct

#ifndef _MODEM_HW_H_
#define _MODEM_HW_H_

// define this to see lot more stuff when debugging
//#define DEEP_DEBUG

// make internal syslog wrapper function available even to low-level functions
extern void me_syslog(int priority, const char *format, ...);

//
// some handy types - define them here as all C files include
// this header file.
//
typedef int BOOL;

#ifndef NULL
#define NULL                0
#endif

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#if defined(V_GPIO_STYLE_atmel) || defined(V_GPIO_STYLE_ti)
/* NTC-6908 control GPIO directly. */
#include "libgpio.h"
#define CONTROL_DIRECT_GPIO_PIN
#define DCE_PORT_RTS_IN    PIN_B(29)    // 61
#define DCE_PORT_DTR_IN    PIN_B(31)    // 63
#define DCE_PORT_CTS_OUT   PIN_B(28)    // 60
#define DCE_PORT_DSR_OUT   PIN_B(30)    // 62
#define DCE_PORT_DCD_OUT   PIN_B(16)    // 48
#define DCE_PORT_RI_OUT    PIN_B(17)    // 49
#define IOCTL_DTR   TIOCM_DTR
#define IOCTL_RTS   TIOCM_RTS
#elif defined(V_GPIO_STYLE_freescale)

#if defined(V_SERIAL_HAS_FC_y)
/* NTC-NWL12 uses AUART4 for external serial port. */
#define TIOCM_OUT1  0x2000              // 1:4, 36		DCD
#define TIOCM_OUT2  0x4000              // 1:26, 58		RI
#else
/* NTC-1101 uses AUART1 for external serial port without flow control lines. */
#endif
#define IOCTL_DTR   TIOCM_DSR
#define IOCTL_RTS   TIOCM_CTS

#else
#error V_GPIO_STYLE should be defined!
#endif

#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
extern int V24_init_GPIO();
#endif

// the following functions can be used on external physical serial port only
// and NOT on the module port!
extern int port_ctrl_line_status(int do_get, int *stat);
extern int set_modem_ctrl_line_on(int stat);
extern int set_modem_ctrl_line_off(int stat);
extern void dcd_on(void);
extern void dcd_off(void);
extern void dsr_on(void);
extern void dsr_off(void);
extern void ri_on(void);
extern void ri_off(void);
extern void cts_on(void);
extern void cts_off(void);
extern BOOL is_dtr_high(void);
extern BOOL is_rts_high(void);

extern int scan_modem_ctrl_lines_status(void);
extern int control_modem_output_lines_ppp(void);
extern int control_modem_output_lines_ip(void);
extern int do_csd(void);
extern void csd_cleanup(void);
extern void clear_modem_ctrl_lines(void);
extern BOOL baud_rate_valid(u_long baud);

#endif

