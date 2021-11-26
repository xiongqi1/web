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
// This is rework of "old" modem emulator integrated with Data Stream Manager
// These functions are applicable to the external serial port, NOT the
// phone module's serial port. In fact, only CSD applications need to know
// about the phone module's serial port.
//
// Contains functions dealing with the comms port control lines, namely:
// DTR (input)
// DCD (output)
// RI (output)
// DSR (output)
//
// CTS (output) and RTS (input) should not be handled here as these are meant to be
// dealt with in UART hardware. In case if manual handling of these lines is needed
// functions are provided but SHOULD NOT BE USED WITHOUT A VERY GOOD REASON.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// only need definitions in the low-level include file
#include "modem_hw.h"

#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
// @TODO - wrap this in a do { } while (0)
#define CHK(function) {int rval; rval=function; syslog(LOG_INFO, "CHK[" #function "]=>%d\n",rval); rval=rval; }
#endif

extern FILE *g_comm_host;

// a variable that stores the last status of modem control lines (really need only for inputs)
static unsigned long this_port_vals = 0;

// Two functions are permitted:
// 1) if do_get is set, then it will return the status of modem
// control lines.
// 2) if do_get is 0, then this
// allows to set/clear the entire status of the underlying modem info
// driver structure,  using TIOCMSET command
// For example, the driver will do:
// switch (command) {
//      case TIOCMSET:
//                 ch->ch_mout = mval;
//                 break;
// }
// Returns 0 on success or -1 on error
int modem_ctrl_line_status(int do_get, int *stat)
{
    int cur_fd = fileno(g_comm_host);
    int succ;
    int io_ctl;

    if (!cur_fd) {
        me_syslog(LOG_ERR,"Failed to find comm port file descriptor %s\n", strerror(errno));
        return -1;
    }

    if (do_get)
        io_ctl = TIOCMGET;
    else
        io_ctl = TIOCMSET;

    succ = ioctl(cur_fd, io_ctl, stat);
    if (succ < 0) {
        me_syslog(LOG_ERR,"Failed ioctrl %d\n", succ);
        return -1;
    }
    return 0;
}

// Two functions are permitted:
// 1) if on_or_off is set, then it will set bits as per nStat
// parameter in the driver's modem control structure.
// 2) if on_or_off is 0, then it will clear bits as per nStat
// parameter in the driver's modem control structure.
//
// You cannot clear some bits and set some other bits using this function
// (use modem_ctrl_line_status instead).
// For example, the driver will do:
//         switch (command) {
//         case TIOCMBIS:
//                 ch->ch_mout |= mval;
//                 break;
//         case TIOCMBIC:
//                 ch->ch_mout &= ~mval;
//                 break;
//
// Returns 0 on success or -1 on error
int set_modem_ctrl_line(int do_set_bits, int stat)
{
    int cur_fd = fileno(g_comm_host);

    int io_ctl;
    int succ;

    if (!cur_fd)
        return -1;

    if (do_set_bits)
        io_ctl = TIOCMBIS;
    else
        io_ctl = TIOCMBIC;

    succ = ioctl(cur_fd, io_ctl, &stat);
    if (succ < 0) {
        me_syslog(LOG_ERR,"Failed ioctrl %d\n", succ);
        return -1;
    }

    return 0;
}

// Set modem control line(s)
// Returns 0 on success or -1 on error
int set_modem_ctrl_line_on(int stat)
{
    return set_modem_ctrl_line(1, stat);
}

// Clear modem control line(s)
// Returns 0 on success or -1 on error
int set_modem_ctrl_line_off(int stat)
{
    return set_modem_ctrl_line(0, stat);
}

#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
int V24_init_GPIO()
{
    int rval = 1;
    CHK(gpio_request_pin(DCE_PORT_RTS_IN));
    CHK(gpio_request_pin(DCE_PORT_DTR_IN));
    CHK(gpio_request_pin(DCE_PORT_CTS_OUT));
    CHK(gpio_request_pin(DCE_PORT_DSR_OUT));
    CHK(gpio_request_pin(DCE_PORT_RI_OUT));
    CHK(gpio_request_pin(DCE_PORT_DCD_OUT));

    CHK(gpio_set_input(DCE_PORT_RTS_IN));
    CHK(gpio_set_input(DCE_PORT_DTR_IN));
    CHK(gpio_gpio(DCE_PORT_CTS_OUT));
    CHK(gpio_gpio(DCE_PORT_DSR_OUT));
    CHK(gpio_gpio(DCE_PORT_RI_OUT));
    CHK(gpio_gpio(DCE_PORT_DCD_OUT));
    CHK(gpio_set_output(DCE_PORT_CTS_OUT, 0));
    CHK(gpio_set_output(DCE_PORT_DSR_OUT, 1));
    CHK(gpio_set_output(DCE_PORT_RI_OUT, 0));
    CHK(gpio_set_output(DCE_PORT_DCD_OUT, 0));

    return rval;
}
#endif


#if defined(V_SERIAL_HAS_FC_y)

// activate Carrier Detect line
void dcd_on(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    gpio_gpio(DCE_PORT_DCD_OUT);
    gpio_set_output(DCE_PORT_DCD_OUT, 0);
#else
    // Falcon & Eagle B/D : OUT1 = DCD, refer to kernel/arch/arm/boot/dts/imx28_falcon.dts
    if (set_modem_ctrl_line_on(TIOCM_OUT1) < 0) {
        me_syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
    }
#endif
}

// de-activate Carrier Detect line
void dcd_off(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    gpio_gpio(DCE_PORT_DCD_OUT);
    gpio_set_output(DCE_PORT_DCD_OUT, 1);
#else
    // Falcon & Eagle B/D : OUT1 = DCD, refer to kernel/arch/arm/boot/dts/imx28_falcon.dts
    if (set_modem_ctrl_line_off(TIOCM_OUT1) < 0) {
        me_syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
    }
#endif
}

// activate Ring Indicator line
void ri_on(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    gpio_gpio(DCE_PORT_RI_OUT);
    gpio_set_output(DCE_PORT_RI_OUT, 0);
#else
    // Falcon & Eagle B/D : OUT2 = RI, refer to kernel/arch/arm/boot/dts/imx28_falcon.dts
    if (set_modem_ctrl_line_on(TIOCM_OUT2) < 0) {
        me_syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
    }
#endif
}

// de-activate Ring Indicator line
void ri_off(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    gpio_gpio(DCE_PORT_RI_OUT);
    gpio_set_output(DCE_PORT_RI_OUT, 1);
#else
    // Falcon & Eagle B/D : OUT2 = RI, refer to kernel/arch/arm/boot/dts/imx28_falcon.dts
    if (set_modem_ctrl_line_off(TIOCM_OUT2) < 0) {
        me_syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
    }
#endif
}

// activate DSR line
void dsr_on(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    gpio_gpio(DCE_PORT_DSR_OUT);
    gpio_set_output(DCE_PORT_DSR_OUT, 0);
#else
    // Falcon & Eagle B/D : DTR(in DTE side) refers to DSR (in DCE side),
    // refer to kernel/arch/arm/boot/dts/imx28_falcon.dts and schematic
    if (set_modem_ctrl_line_on(TIOCM_DTR) < 0) {
        me_syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
    }
#endif
}

// de-activate DSR line
void dsr_off(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    gpio_gpio(DCE_PORT_DSR_OUT);
    gpio_set_output(DCE_PORT_DSR_OUT, 1);
#else
    // Falcon & Eagle B/D : DTR(in DTE side) refers to DSR (in DCE side),
    // refer to kernel/arch/arm/boot/dts/imx28_falcon.dts and schematic
    if (set_modem_ctrl_line_off(TIOCM_DTR) < 0) {
        me_syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
    }
#endif
}

// This function should not be called in normal circumstances
// CTS/RTS is hardware flow control and should not be handled manually
// but left to UART driver and hardware. However, we do have the ability
// to drive this pin directly - as soon as this is called, on Freescale
// platform, hardware flow control in the driver will be disabled!
// See code in the UART driver /kernel/drivers/tty/serial/mxs-auart.c
// function mxs_auart_set_mctrl
void cts_on(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    gpio_gpio(DCE_PORT_CTS_OUT);
    gpio_set_output(DCE_PORT_CTS_OUT, 0);
#else
    // Falcon & Eagle B/D : RTS(in DTE side) refers to CTS (in DCE side),
    // refer to kernel/arch/arm/boot/dts/imx28_falcon.dts and schematic
    if (set_modem_ctrl_line_on(TIOCM_RTS) < 0) {
        me_syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
    }
#endif
}

// see comments in cts_on function header
void cts_off(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    gpio_gpio(DCE_PORT_CTS_OUT);
    gpio_set_output(DCE_PORT_CTS_OUT, 1);
#else
    // Falcon & Eagle B/D : RTS(in DTE side) refers to CTS (in DCE side),
    // refer to kernel/arch/arm/boot/dts/imx28_falcon.dts and schematic
    if (set_modem_ctrl_line_off(TIOCM_RTS) < 0) {
        me_syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
    }
#endif
}

// TRUE if dtr is on and FALSE otherwise
BOOL is_dtr_high(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    return (this_port_vals & IOCTL_DTR) ? FALSE : TRUE;
#else
    return (this_port_vals & IOCTL_DTR) ? TRUE : FALSE;
#endif
}

//
// again, this function should not be used unless there is a very good reason
// rts/cts handshaking is handled by hardware. There is no need for us to know
// what state RTS is in unless it is used for something non-standard
//
BOOL is_rts_high(void)
{
#ifdef CONTROL_DIRECT_GPIO_PIN
    return (this_port_vals & IOCTL_RTS) ? FALSE : TRUE;
#else
    return (this_port_vals & IOCTL_RTS) ? TRUE : FALSE;
#endif
}
#else
// stubs for platforms with no V_SERIAL
void dcd_on(void) {}
void dcd_off(void) {}
void ri_on(void) {}
void ri_off(void) {}
void dsr_on(void) {}
void dsr_off(void) {}
void cts_on(void) {}
void cts_off(void) {}
BOOL is_dtr_high(void)
{
    return 0;
}
BOOL is_rts_high(void)
{
    return 0;
}
#endif

//
// Needs to be called regularly to read the status of modem
// control line inputs
// Returns 0 on success or -1 on error
//
int scan_modem_ctrl_lines_status(void)
{
    // zeroise before reading
    this_port_vals = 0;

#if defined(V_SERIAL_HAS_FC_y)
#ifdef CONTROL_DIRECT_GPIO_PIN
    this_port_vals |= (gpio_read(DCE_PORT_RTS_IN) ? TIOCM_RTS : 0);
    this_port_vals |= (gpio_read(DCE_PORT_DTR_IN) ? TIOCM_DTR : 0);
    this_port_vals |= (gpio_read(DCE_PORT_CTS_OUT) ? TIOCM_CTS : 0);
    this_port_vals |= (gpio_read(DCE_PORT_DSR_OUT) ? TIOCM_DSR : 0);
    this_port_vals |= (gpio_read(DCE_PORT_RI_OUT) ? TIOCM_RI : 0);
    this_port_vals |= (gpio_read(DCE_PORT_DCD_OUT) ? TIOCM_CD : 0);
#else
    if (modem_ctrl_line_status(1, (int *)&this_port_vals) < 0) {
        me_syslog(LOG_ERR,"failed to read modem control line status");
        return -1;
    }
#endif
#endif

#if 0 // handy for debugging but will really fill the log, so for now commented out but left for future reference
#ifdef DEEP_DEBUG
    static int count = 0;

    if (count++ >= 10) {
        count = 0;
        me_syslog(LOG_INFO, "This port vals 0x%lx", this_port_vals);
    }
#endif
#endif
    return 0;
}

//
// Clears DCD, DSR, RI
//
void clear_modem_ctrl_lines(void)
{
    // clear all modem control line outputs from our (DCE) side
    dcd_off();
    dsr_off();
    ri_off();
}
