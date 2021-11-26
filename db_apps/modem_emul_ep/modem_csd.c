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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
*/
//
// A part of modem emulator end point application (db_apps/modem_emul_ep)
// This is rework of "old" modem emulator. The new solution is integrated with Data Stream Manager
//
// This file contains functions specific to Circuit Switched Data (CSD) end point
// The CSD end point is one of three end points used on the other side of the
// Modem Emulator end point (IP modem and PPP are the other two), and the only
// one that needs any knowledge of the phone module.
//
// The main function of the data stream between ME end point and CSD ep
// is to move data transparently, as well as propagate signals (CTS, RTS,
// DTR, DSR, CD and RI) to/from module's virtual TTY port to the physical
// RS232 port on the router.
// Ext RS232 port           data stream             Module tty device
// DTR                      ->                      DTR
// RTS                      ->                      RTS
// DSR                      <-                      DSR
// CTS                      <-                      CTS
// RI                       <-                      RI
// CD                       <-                      CD
// Data                     <->                     Data
// Given that usb/serial drivers all support ioctl we can get/set the module's modem
// control lines. For qcserial.c driver used on Cinterion this requires a modification
// to support this functionality
//
// Remaining issues that cannot be resolved without module manufacturer's advice:
// 1) MC8704 Sierra does not appear to correctly report the status of the CD line, but reads DSR correctly
//      Solution:  a) state this in the release note/technical bulletin
//                 b) If DCD action is "session established" (proprietary to us option), then use
//                      module's DSR to drive the external DCD. Of course DCD->"always on" will also work
//                      if the customer's app is happy to have DCD line always high
//
// 2) Ericsson F5521 does not appear to detect the escape sequence (+++) correctly
//      Solution: a TB/release note should suggest that external entity should drop DTR
//                  line when disconnect/online command mode is required in combination
//                  with setting correct DTR line option (e.g. AT&D1 or AT&D2)
//
// The 6200 with Cinterion module and 6908 with Sierra MC8790 work correctly with no issues.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h> // need both time and times
#include <errno.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h> //hostent
#include <pthread.h>
#include "rdb_ops.h"
#include "modem_emul_ep.h"

// type of CSD end point initialization, as per init_method RDB variable
// of the end point. As saved by WebUI EndPoint14.html
#define CSD_INIT_DEFAULT (0)   // perform well-known, working init
#define CSD_INIT_USER (1)      // no initialization
#define CSD_INIT_ME_CONFIG (2)    // perform init derived from config of the modem emulator end point

// reach out only for globals that are necessary

// external serial port
extern FILE *g_comm_host;
extern t_conf_csd g_conf_csd;// configuration of csd end point
extern t_conf_v250 g_conf_v250; // configuration of ME end point

// the name of module's AT port
#define CSD_BUF_LEN 64    // to read interface name related info from RDB
char g_module_at_port[CSD_BUF_LEN];

// FILE pointer associated with tty port instantiated by the module
FILE *g_comm_module;

// size of buffer used in receiving/sending serial data from/to ext port to/from module
#define MAX_BUF_SIZE (1024)
static char buff[MAX_BUF_SIZE];

// stores old serial port settings
struct termios old_module_tio;

// supports saving and restoring of old serial port configuration
static int old_settings_stored = FALSE;

//
// Function find_module_at_interface_name, sets module g_module_at_port variable to
// the name of tty device that is module's AT command port.
//
// Returns 0 on success and -1 otherwise
//
// Different modules use different drivers and thus name of this tty device varies
// This function is modelled on phone-extra-port.template
//
// There are obviously many legacy issues here that need to be considered.
//
// module_type=$(rdb_get "wwan.0.if")
// module_at_port=$(rdb_get "wwan.0.V250_if.1")
// module_dat_port=$(rdb_get "wwan.0.data_if.2")
// module_made=$(rdb_get "wwan.0.module_type")
// do not launch modem emulator if at port is used
// if [ "$module_type" = "at" -o "$module_type" = "atcns" -o "$module_type" = "atqmi" ]; then
// case "$module_made" in
//		'ericsson' | 'Cinterion')
//			module_at_port="$module_dat_port"
//			log "set module_at_port="$module_at_port
//			;;
//		*)
//			log "current phone module type does not support modem_emulator (module type = '$module_type') - use no phone mode"
//			module_at_port="nophone"
//			;;
//
//	esac
//
//elif [ -z "$module_at_port" ]; then
//	log "at port not found. wwan.0.V250_if.1 is empty - use no phone mode"
//	module_at_port="nophone"
//fi
//
int find_module_at_interface_name(void)
{
    int str_length = CSD_BUF_LEN;
    char buf[CSD_BUF_LEN];
    char module_made[CSD_BUF_LEN];

    g_module_at_port[0] = 0; // terminate

    // wwan.0.if has the module type, read it first
     // will catch "atcns", "atqmi" and simple "at"
    if ((get_single_str_root("wwan.0", "if", buf, &str_length) == 0) && strstr(buf, "at")) {
        str_length = CSD_BUF_LEN; // reload length
        if (get_single_str_root("wwan.0", "module_type", module_made, &str_length) == 0) {
            if (strstr(module_made, "ericsson") || strstr(module_made, "Cinterion")) {
                str_length = CSD_BUF_LEN; // reload length
                // get module at port
                if (get_single_str_root("wwan.0", "data_if.2", g_module_at_port, &str_length) == 0) {
                    return 0;
                }
            }
        }
    } else {
        str_length = CSD_BUF_LEN;
        if (get_single_str_root("wwan.0", "V250_if.1", g_module_at_port, &str_length) == 0) {
            return 0;
        }
    }

    return -1;
}

//
// Set or clear one input signal to the module.
// There are only 2 of these, DTR and RTS
// If drivers worked correctly, they
// would ignore module's output bits (our inputs) in the set command e.g. CD, RI.
// (In other words, logically it should not be possible to set the status of module's output)
//
// However this appears not to be the case so it is better not to modify them, so we do a "get"
// to get the current status of the inputs and then set/clear a bit, "or" with inputs and do a "set"
//
// Note that we do not use TIOCMBIS/TIOCMBIC as it may not implemented in
// the usb-serial drivers.
//
// Arguments:
// on = TRUE to set the bit/signal, FALSE to clear
// signal_bit - TIOCM_DTR or TIOCM_RTS
//
static int set_module_input_bit(BOOL on, int signal_bit)
{
    int succ;
    int ioCtl;
    int fd = fileno(g_comm_module);
    int stat;

    if (!fd) {
        return -1;
    }

    if ((signal_bit != TIOCM_DTR) && (signal_bit != TIOCM_RTS)) {
        me_syslog(LOG_ERR,"Invalid argument for signal bit %02x\n", signal_bit);
        return -1;
    }

    // read the status of all signals. This will return
    // also the status of module's outputs such as CD, RI and so on
    ioCtl = TIOCMGET;
    stat = 0;
    succ = ioctl(fd, ioCtl, &stat);
    if (succ < 0) {
        me_syslog(LOG_ERR,"Failed TIOCMGET ioctrl in set_module_dtr %d\n", succ);
        return -1;
    }

    // for set, we want to set or clear one bit only, leaving all other bits from get untouched.
    ioCtl = TIOCMSET;
    succ = 0;
    if (on) {
        if (!(stat & signal_bit)) {
            stat |= signal_bit;
            succ = ioctl(fd, ioCtl, &stat);
        }
    }
    else {
        if (stat & signal_bit) {
            stat &= ~signal_bit;
            succ = ioctl(fd, ioCtl, &stat);
        }
    }

    if (succ < 0) {
        me_syslog(LOG_ERR,"Failed TIOCMSET ioctrl in set_module_dtr %d\n", succ);
        return -1;
    }
    return succ;
}

//
// Copied from the old modem emulator.
// Module port may not be yet available when this process is started
// port - name of device e.g. /dev/ttyUSB3
// timeout_sec - total timeout in seconds to wait before giving up
//
// Return 0 on success, -1 on failure
//
static int wait_for_port(const char* port, int timeout_sec)
{
    struct stat port_stat;
    while (stat(port, &port_stat) < 0) {
        if (timeout_sec-- <= 0) {
            me_syslog(LOG_INFO,"waiting for port %s timed out (%s)", port, strerror(errno));
            return -1;
        }
        sleep(1);
    }
    me_syslog(LOG_DEBUG,"waiting for port '%s' ok, port now available", port);
    return 0;
}

//
// Initialize the module from a hard-coded configuration.
// It will make max_attempts to initialize, sleeping retry_timeout_secs
// (in seconds) before giving up.
//
// Return 0 on success, -1 on failure
static int init_module_default(int max_attempts, int retry_timeout_secs)
{
    int nsent;
    int attempts = 0;

    const char *at_default_commands =
    {
        "AT" // start AT command string
        "E1" // enable echo
        "Q0" // disable quiet mode
        "V1" // enable verbose mode
        "&D2" // DTR line disconnects the calls
        "&S1"// DSR becomes active when "tone" is detected
        "&C1"// CD becomes active when call is connected
        //"AT+IPR=115200\r", // bit rate is ignored by the module anyway as we use USB
        "\\Q3" // enable hw flow control as recommended in CSD mode
    };

    // add number of rings dynamically
    char at_buf[MAX_CMD_LENGTH];
    if (g_conf_v250.opt_auto_answer_enable && g_conf_v250.opt_modem_auto_answer) {
        sprintf(at_buf, "%sS0=%d\r", at_default_commands, g_conf_v250.opt_modem_auto_answer);
    } else {
        // disable auto-answer
        sprintf(at_buf, "%sS0=0\r", at_default_commands);
    }

    while (attempts++ < max_attempts) {
        me_syslog(LOG_DEBUG, "Trying to send default AT initialization, retry attempt no %d", attempts);
        nsent = write(fileno(g_comm_module), at_buf, strlen(at_buf));
        if (nsent < 0) {
            me_syslog(LOG_DEBUG, "Sent failed (%d), err: %s", nsent, strerror(errno));
            sleep(retry_timeout_secs);
        } else {
            me_syslog(LOG_INFO, "Successfully sent default AT initialization string %s to module", at_buf);
            return 0;
        }
    }
    me_syslog(LOG_ERR, "Failed to send default AT initialization commands");
    return -1;
}

//
// Initialize the module based on the configuration settings of the modem emulator end point
// (the other side of the data stream). This happens when Custom initialization is
// selected in the WebUI.
//
// After some deliberation, we have removed this option from the user interface as it may
// be confusing but leave the function here as we may get some feedback from customers.
//
// We are building the buffer dynamically, so adjust the pointer to the next non-NULL
// character
//
#define adjust_ptr(ptr) \
    do {\
        ptr++; \
    } while (*ptr && ptr<p_at_buf_end)

int init_module_custom(void)
{
    char at_buf[MAX_CMD_LENGTH], *p_at_buf = &at_buf[0], *p_at_buf_end = &at_buf[MAX_CMD_LENGTH-1];

    memset(at_buf, 0, MAX_CMD_LENGTH); // algorithm requires the entire buffer to be zeroed
    strcpy(at_buf, "AT"); // disable echo
    adjust_ptr(p_at_buf);

    // 1) modem auto-answer options
    if (g_conf_v250.opt_auto_answer_enable && g_conf_v250.opt_modem_auto_answer) {
        sprintf(p_at_buf, "S0=%d", g_conf_v250.opt_modem_auto_answer);
    } else {
        // disable auto-answer
        sprintf(p_at_buf, "S0=0");
    }
    adjust_ptr(p_at_buf);

    // 2) RI option is not sent to the module

    // 3) hardware flow control
    // 4) software flow control. Enable only when there is no hw flow control enabled
    if (g_conf_v250.opt_hw_fc == 1) {
        strcpy(p_at_buf, "\\Q3");
    }
    else if (g_conf_v250.opt_sw_fc == 1) {
        strcpy(p_at_buf, "\\Q1");
    } else {
        // no flow control
        strcpy(p_at_buf, "\\Q0");
    }
    adjust_ptr(p_at_buf);

    // 5) DCD. Let the module indicate to us the call status, and we will decide
    // what to do with the external serial port's CD line
    strcpy(p_at_buf, "&C1");
    adjust_ptr(p_at_buf);

    // 6) DSR. Same thing as DCD, let module indicate to us anyway
    strcpy(p_at_buf, "&S1");
    adjust_ptr(p_at_buf);

    // 7) DTR - important to sent to the module the required behaviour.
    switch (g_conf_v250.opt_dtr) {
        case V250_DTR_IGNORE:
            strcpy(p_at_buf, "&D0");
            break;
        case V250_DTR_COMMAND:
            strcpy(p_at_buf, "&D1");
            break;
        case V250_DTR_HANGUP:
        default:
            strcpy(p_at_buf, "&D2");
            break;
    }
    adjust_ptr(p_at_buf);

    // now deal with bitmapped options
    // Setting these incorrectly will almost certainly cause problems on models
    // with simple at manager. For example, simple_at_manager expects echo on
    // all commands it sends to the module's application port and they can
    // be stopped by sending ATE0

    // 8) enable/disable echo
    sprintf(p_at_buf, "E%d", g_conf_v250.opt_1 & ECHO_ON ? 1 : 0);
    adjust_ptr(p_at_buf);

    // 9) quiet mode
    sprintf(p_at_buf, "Q%d", g_conf_v250.opt_1 & QUIET_ON ? 1 : 0);
    adjust_ptr(p_at_buf);

    // 10) verbose mode, add \r as this is the last command
    sprintf(p_at_buf, "V%d\r", g_conf_v250.opt_1 & VERBOSE_RSLT ? 1 : 0);
    adjust_ptr(p_at_buf);

    me_syslog(LOG_INFO, "Sent custom AT initialization string %s to module", at_buf);
    return write(fileno(g_comm_module), at_buf, strlen(at_buf));
}

//
// After main initalization (if any), it is possible to provide custom AT command string
// that will be sent to the module when CSD data stream is initializing.
// If "str" is non-empty, the whole string is sent to the module as is, with the
// only exception is that AT is added if not already at the beginning of the string
//
int init_additional(char *str)
{
    // send to the module without questioning
    if (*str != 0) {

        // if AT is not given in the beginning, add it!
        if (((str[0] != 'A') && (str[0] != 'a')) ||
            ((str[1] != 'T') && (str[1] != 't'))) {
            if (write(fileno(g_comm_module), "AT", strlen("AT")) < 0) {
                me_syslog(LOG_ERR, "Failed to send additional AT initialization string %s", str);
                return -1;
            }
        }

        if ((write(fileno(g_comm_module), str, strlen(str)) < 0) ||
            (write(fileno(g_comm_module), "\r", 1) < 0)) {
            me_syslog(LOG_ERR, "Failed to send additional AT initialization string %s", str);
            return -1;
        }
        me_syslog(LOG_INFO, "Successfully sent an additional AT initialization string %s", str);
    }
    return 0;
}

// A wrapper to request disconnect call from the module.
// Simply drop DTR line
// The alternative would have been to send the escape sequence (if module had no DTR
// ioctr support)
static int module_disconnect_call(void)
{
    // disconnect any CSD connections that module might still be involved in
    return set_module_input_bit(FALSE, TIOCM_DTR);
}

//
// Cleanup before data stream process (modem_emul_ep) exits
//
void csd_cleanup(void)
{
    // disconnect any CSD calls before exiting
    (void)module_disconnect_call();

    // restore old termios settings on the module
    if (g_comm_module) {
        if (old_settings_stored) {
            tcsetattr(fileno(g_comm_module), TCSANOW, &old_module_tio);
        }
        fclose(g_comm_module);
    }
}

//
// Implements timeout monitoring of a connected call. If
// monitoring of timeouts is disabled, then g_conf_csd.inactivity_timeout_mins
// configuration variable is set to 0.
// We restart the timer in the following circumstances:
// 1) There is no connection
// 2) There is connection but we just saw some data
//
// When timer is restarted, we take the "start_time" timestamp
// Otherwise, this function is called periodically (the frequency is not important)
// and as soon as current timestamp minus "start time" timestamp exceeds the
// specified timeout, we disconnect the call.
//
static BOOL restart_timer = TRUE;
extern time_t me_get_uptime(void);
static BOOL inactivity_monitor(BOOL is_connected)
{
    static int count = 100;
    static time_t start_time, current_time;

    if (!is_connected || !g_conf_csd.inactivity_timeout_mins) {
        restart_timer = TRUE;
        return FALSE;
    }

    //
    // Simple optimization: as this doesn't need to be very accurate, do nothing most of the time
    // and avoid getting and comparing time. This comes at a cost of a slight inaccuracy of
    // timeout detection which will occur 5-10 seconds after the specified limit which is
    //
    if (--count) {
        return FALSE;
    }
    count = 100;

    current_time = me_get_uptime();
    if (restart_timer)
    {
        start_time = current_time;
        restart_timer = FALSE;
        return FALSE;
    }

#ifdef DEEP_DEBUG
    me_syslog(LOG_DEBUG, "Connection was up and inactive for %d seconds", abs((int)difftime(start_time, current_time)));
#endif

    // this will be slighly longer (by a few seconds) than the specified timeout
    if (abs((int)difftime(start_time, current_time)) >= (g_conf_csd.inactivity_timeout_mins * 60)) {
        restart_timer = TRUE;
        return TRUE;
    }

    return FALSE;
}

//
// Move data from/to external serial port to/from module USB tty port
//
void move_data_csd(int host_port_fd, int module_port_fd, int select_timeout_ms, BOOL is_connected)
{
    int bytes_received;
    fd_set fdsetR;
    struct timeval tv;

    // determine the highest fd out of two file descriptors
    int max_fd = host_port_fd > module_port_fd ? host_port_fd : module_port_fd;

    FD_ZERO(&fdsetR);
    FD_SET(host_port_fd, &fdsetR);
    FD_SET(module_port_fd, &fdsetR);

    //
    // Check if anything is there ready to be read from socket
    // or from the serial port
    //
    tv.tv_sec = 0;
    tv.tv_usec = select_timeout_ms * 1000;

    select(max_fd + 1, &fdsetR, (fd_set*)0, (fd_set*)0, &tv);

    // Part 1: external serial port -> module port
    if (FD_ISSET(host_port_fd, &fdsetR)) {

        bytes_received = read(host_port_fd, buff, MAX_BUF_SIZE);

        if (bytes_received > 0) {
            restart_timer = TRUE;
            if (send_all(module_port_fd, buff, bytes_received, 0, 10000, TRUE, NULL, 0) < 0) {
                me_syslog(LOG_ERR, "Failure: send_all to module");
            }
        } else if (bytes_received < 0) {
            me_syslog(LOG_ERR, "Receive fail from host #1");
        } else {
            // isset is true but could not read any bytes
            me_syslog(LOG_ERR, "Receive fail from host #2");
        }
    } else {
        // serial timeout. Still kick the monitor in case of one way traffic
        if (inactivity_monitor(is_connected)) {
            me_syslog(LOG_NOTICE, "Inactivity timer expired, exceeded %d minutes, disconnecting", g_conf_csd.inactivity_timeout_mins);
            (void)module_disconnect_call();
#ifdef DEEP_DEBUG
            print_modem_ctrl_lines(g_comm_module, 1, TRUE);
#endif
        }
    }

    // Part 2: module port -> external serial port
    if (FD_ISSET(module_port_fd, &fdsetR)) {

        bytes_received = read(module_port_fd, buff, MAX_BUF_SIZE);

        if (bytes_received > 0) {
            restart_timer = TRUE;
            if (send_all(host_port_fd, buff, bytes_received, 0, 10000, TRUE, NULL, 0) < 0) {
                me_syslog(LOG_ERR, "Send all failed to host");
            }
        } else if (bytes_received < 0) {
            me_syslog(LOG_ERR, "Receive fail from module #1");
        } else {
            me_syslog(LOG_ERR, "Receive fail from module #2");
        }
    } else {
        // module timeout. Kick the inactivity monitor
        if (inactivity_monitor(is_connected)) {
            me_syslog(LOG_NOTICE, "Inactivity timer expired, exceeded %d minutes, disconnecting", g_conf_csd.inactivity_timeout_mins * 60);
            (void)module_disconnect_call();
#ifdef DEEP_DEBUG
            print_modem_ctrl_lines(g_comm_module, 1, TRUE);
#endif
        }
    }
}

//
// Prepare CSD data stream
// 1) Find the module at interface
// 2) Open and initialize it, making sure old termios settings are stored
// 3) Initialize module with necessary initialization method (custom, default or none)
// 4) Just in case, disconnect module's calls in progress, if any
int prepare_csd(t_conf_csd *p_conf)
{
    int fd, ret = 0;

    //
    // find the name of module's at interface, normally this is something like
    // /dev/ttyUSBx
    //
    if (find_module_at_interface_name() != 0) {
        me_syslog(LOG_ERR, "Failed to find module AT interface name %s", g_module_at_port);
        return -1;
    }
    me_syslog(LOG_DEBUG, "Modem emulator (CSD) found module AT interface %s", g_module_at_port);

    //
    // we have no control over when we have been started, which may well be before the
    // module AT port becomes available. So wait up to 30 seconds which is the value
    // used in the old legacy modem emulator.
    //
    if (wait_for_port(g_module_at_port, 30) != 0) {
        me_syslog(LOG_ERR, "Failed to find module port %s", g_module_at_port);
        return -1;
    }

    // open the module port
    if ((g_comm_module = fopen(g_module_at_port, "r+")) == 0) {
        me_syslog(LOG_ERR, "Failed to open module port %s", g_module_at_port);
        return -1;
    }

    // save current port settings for module's tty port
    fd = fileno(g_comm_module);
    if (tcgetattr(fd, &old_module_tio) == 0) {
        old_settings_stored = TRUE;
    }

    // set the module port to default, working values.
    if ((set_port_defaults(g_comm_module) < 0) || (set_baud_rate(g_comm_module, 115200) < 0)) {
        me_syslog(LOG_ERR, "Failed to set module port defaults");
        return -1;
    }

    // depending on how things were configured, initialize accordingly
    // Proceed even on error return, as CSD may still work correctly without init
    me_syslog(LOG_ERR, "Init method %d", g_conf_csd.init_method);
    if (g_conf_csd.init_method == CSD_INIT_DEFAULT) {
        // initialize module as per in-built default configuration
        // do 5 attempts, allow 3 seconds in between.
        (void)init_module_default(5, 3);
    } else if (g_conf_csd.init_method == CSD_INIT_ME_CONFIG) {
        // initialize module as per modem emulator end point configuration
        // at the moment, WebUI does not allow this.
        (void)init_module_custom();
    }

    // if there are any additional initialization commands provided by the user, send them
    (void)init_additional(g_conf_csd.additional_init_str);

    //
    // Disconnect existing CSD calls, if any. Depending on how previous sessions
    // terminated, the module may still be in a connected call state.
    //
    if (module_disconnect_call() < 0) {
        me_syslog(LOG_ERR, "ioctr is not available on module's tty device %s. This may be a usb-serial driver problem",
                  g_module_at_port);
        return -1;
    }

    // Drop the DTR line on the module, and the module will drop any old connections.
    // 2 seconds are stated as minimum on Wiki http://en.wikipedia.org/wiki/Data_Terminal_Ready.
    // In practice it looks like even 100ms works, but we want to be on the safe side.
    // After 2 seconds module's DTR will start mimicing the external port's DTR
    sleep(2);

    return ret;
}

//
// Functions to control the modem control lines. These functions
// are specific to the type of end point connected to the modem emulator end point
// Note that functions use debouncing via a tri-state variable to avoid calling ioctl
// unnecessarily (e.g. if the last remembered state is the same as required, nothing is
// done.
//
// Control the External Serial port's DSR line according to the state of the device
//
static void csd_control_dsr(BOOL module_dsr_status)
{
    static int last_dsr_state = -1; // undefined
    int required_dsr_state = 1; // default

    if (g_conf_v250.opt_dsr == V250_DSR_ALWAYS) {
        required_dsr_state = 1;
    } else if (g_conf_v250.opt_dsr == V250_DSR_NEVER) {
        required_dsr_state = 0;
    } else if ((g_conf_v250.opt_dsr == V250_DSR_REGISTERED) ||
            (g_conf_v250.opt_dsr == V250_DSR_PDP)) {
        required_dsr_state = module_dsr_status ? 1 : 0;
    } else if (g_conf_v250.opt_dsr == V250_DSR_MIMIC_DTR) {
        // not sure if this even useful
        required_dsr_state = is_dtr_high() ? 1 : 0;
    }

    // optimization: avoid calling dsr_on/dsr_off when there is no state change required
    if (required_dsr_state != last_dsr_state) {
        last_dsr_state = required_dsr_state;
        if (required_dsr_state) {
            dsr_on();
        } else {
            dsr_off();
        }
    }
}

// Control the External ports CD line based on module's CD and DSR status
//
// the second argument looks out of place but we provide the capability
// to mirror module's DSR status on external DCD line because
// at least on one module (Sierra MC8704) CD detection
// is not working due to module firmware problems
static void csd_control_dcd(BOOL module_dcd_status, BOOL module_dsr_status)
{
    static int last_dcd_state = -1; // undefined
    int required_dcd_state = 1; // default

    if (g_conf_v250.opt_dcd == V250_DCD_ALWAYS) {
        required_dcd_state = 1;
    } else if (g_conf_v250.opt_dcd == V250_DCD_NEVER) {
        required_dcd_state = 0;
    } else if (g_conf_v250.opt_dcd == V250_DCD_CONNECT) {
        required_dcd_state = module_dcd_status ? 1 : 0;
    } else if (g_conf_v250.opt_dcd == V250_DCD_PDP) {
        required_dcd_state = module_dsr_status ? 1 : 0;
    }

    // optimization: avoid calling dcd_on/dcd_off when there is no state change required
    if (required_dcd_state != last_dcd_state) {
        last_dcd_state = required_dcd_state;
        if (required_dcd_state) {
            dcd_on();
        } else {
            dcd_off();
        }
    }
}

// Control external port RI line
static void csd_control_ri(BOOL module_ri_status)
{
    static int last_ri_state = -1; // undefined
    int required_ri_state = 0; // default

    if (g_conf_v250.opt_ri == V250_RI_ALWAYS) {
        required_ri_state = 1;
    } else if (g_conf_v250.opt_ri == V250_RI_NEVER) {
        required_ri_state = 0;
    } else if (g_conf_v250.opt_ri == V250_RI_RING) {
        required_ri_state = module_ri_status ? 1 : 0;
    }

    // optimization: avoid calling ri_on/ri_off when there is no state change required
    if (required_ri_state != last_ri_state) {
        last_ri_state = required_ri_state;
        if (required_ri_state) {
            ri_on();
        } else {
            ri_off();
        }
    }
}

// we have to do this by hand rather than relying on UART hardware.
// Reason: let's say module's CTS becomes low indicating that
// it is not ready to receive data. Despite the fact that write will probably block
// when we write to the module, we still have set CTS
// line of the external port or the external application will continue
// sending us data, and we would very soon run out of buffer space.
//
static void csd_control_cts(BOOL module_cts_status)
{
    static int last_cts_state = -1; // undefined
    int required_cts_state = 1; // default

    if (g_conf_v250.opt_hw_fc) {
        required_cts_state = module_cts_status ? 1 : 0;
    } else {
        required_cts_state = 1;
    }

    if (required_cts_state != last_cts_state) {
        last_cts_state = required_cts_state;
        if (required_cts_state) {
            cts_on();
        } else {
            cts_off();
        }
    }
}

//
// Handle DTR status of the external port.
// We pass the status to the module regardless of configuration
// The other option was to condition this based on the
// configuration, e.g. g_conf_v250.opt_dtr
// However, it is always possible to send to the module the desired &D
// initialization command for it to decide how to behave in case of
// DTR going low (rather than us making this decision), so this option
// was NOT chosen.
//
int csd_handle_dtr(void)
{
    static int last_state = -1; // undefined
    int ret = 0;

    if (is_dtr_high() && (last_state != 1)) {
        ret = set_module_input_bit(TRUE, TIOCM_DTR);
        last_state = 1;
        me_syslog(LOG_DEBUG, "Setting DTR status on module to high %d", ret);
    } else if (!is_dtr_high() && (last_state != 0)) {
        ret = set_module_input_bit(FALSE, TIOCM_DTR);
        last_state = 0;
        me_syslog(LOG_DEBUG, "Setting DTR status on module to low %d", ret);
    }
    return ret;
}

// Handle RTS status of the external port. We simply pass it to the module
int csd_handle_rts(void)
{
    static int last_state = -1; // undefined
    int ret = 0;

    if (is_rts_high() && (last_state != 1)) {
        ret = set_module_input_bit(TRUE, TIOCM_RTS);
        last_state = 1;
        me_syslog(LOG_DEBUG, "Setting RTS status on module to high %d", ret);
    } else if (!is_rts_high() && (last_state != 0)) {
        ret = set_module_input_bit(FALSE, TIOCM_RTS);
        last_state = 0;
        me_syslog(LOG_DEBUG, "Setting RTS status on module to low %d", ret);
    }
    return ret;
}

// Get the status of module's control lines, namely CD, DSR, CTS and RI
// This is returned as a bitmap in stat argument by reference.
int scan_module_ctrl_lines(int *stat)
{
    int succ;
    int fd = fileno(g_comm_module);

    if (!fd)
        return -1;

    *stat = 0;
    succ = ioctl(fd, TIOCMGET, stat);

    if (succ < 0) {
        me_syslog(LOG_ERR, "error %s", strerror(errno));
    }

    return succ;
}

//
// The main function called from the modem_emul_ep, that calls
// other functions that pass control lines from the external
// serial port to module's port and vice versa, as well as
// pass data through.
//
// Returns 0 on success or -1 on error
// Because we have unusual mapping of h/w line on the external serial port (we use 2 i/o pins
// for DCD and RI outputs and swap RTS/CTS and DTR/DTS), we have to use low level functions
// from modem_crtl_lines as this will work on all platforms. Furthermore, on Atmel
// control lines are controlled via GPIO.
//
// Unlike other end points (PPP and IP Modem which do data moving in a separate thread),
// this function also moves data from serial to module port and vice versa
//
int do_csd(void)
{
    // the status of modem control signals
    int module_line_status = 0;

    // Part 1, module's outputs
    // read module's control line into a variable
    if (scan_module_ctrl_lines(&module_line_status) < 0) {
        me_syslog(LOG_ERR,"failed to scan module lines");
        return -1;
    }

    // basically mirror what is seen on the module to the external port
    // control external port outputs based on module's status
    csd_control_dcd(module_line_status & TIOCM_CD, module_line_status & TIOCM_DSR);
    csd_control_ri(module_line_status & TIOCM_RI);
    csd_control_dsr(module_line_status & TIOCM_DSR);
    csd_control_cts(module_line_status & TIOCM_CTS);

    // Part 2, module's inputs
    // read external port status of DTR and RTS and propagate to the module
    if (csd_handle_dtr() < 0) {
        me_syslog(LOG_ERR,"failed to handle DTR");
        return -1;
    }

    if (csd_handle_rts() < 0) {
        me_syslog(LOG_ERR,"failed to handle RTS");
        return -1;
    }

    move_data_csd(fileno(g_comm_host), fileno(g_comm_module), SELECT_TIMEOUT_MS, module_line_status & TIOCM_CD);

    // this only does logging when debugging is enabled, which can be enabled at run
    // time by setting service.dsm.modem_emul.logging_level RDB variable to 7 or 8
    // and restarting the data stream
    status_logger("CSD", module_line_status & TIOCM_CD ? 1 : 0, TRUE, FALSE,
                      module_line_status & TIOCM_CD ? ON_LINE : COMMAND, 0);

#ifdef DEEP_DEBUG
    // compile time deep debugging option
    static int counter = 0;
    if (++counter >= 50) {
        counter = 0;
        print_modem_ctrl_lines(g_comm_module, 1, TRUE);
        print_modem_ctrl_lines(g_comm_host, 1, FALSE);
    }
#endif

    return 0;
}

// debug function not compiled in commercial releases
#ifdef DEEP_DEBUG
int print_modem_ctrl_lines(FILE *f, int cntr_limit, BOOL is_module)
{
    static int counter = 0;
    int succ = 0, stat;
    int fd;

#if defined(CONTROL_DIRECT_GPIO_PIN)
    // cannot get information on control lines of external serial port via ioctl on these platforms
    if (!is_module) {
        return -1;
    }
#endif

    if (++counter >= cntr_limit) { // print every so often
        counter = 0;
        fd = fileno(f);
        if (!fd)
            return -1;

        stat = 0;
        succ = ioctl(fd, TIOCMGET, &stat);

        if (succ < 0) {
            me_syslog(LOG_ERR, "TIOCMGET error %s", strerror(errno));
        } else {
            if (is_module) {
                me_syslog(LOG_INFO,"Module fd=%d pStat=%08x succ %d, DSR %d CTS %d RI %d CD %d", fd, stat, succ,
                       (stat & TIOCM_DSR) ? 1 : 0, (stat & TIOCM_CTS) ? 1 : 0, (stat & TIOCM_RI) ? 1 : 0, (stat & TIOCM_CD) ? 1 : 0);
            } else {
                // note DTR/DSR and CTS/RTS are swapped on Freescale so use IOCTL_DTR
                me_syslog(LOG_INFO,"Ext port fd=%d pStat=%08x succ %d, DTR %d RTS %d", fd, stat, succ, (stat & IOCTL_DTR) ? 1 : 0,
                       (stat & IOCTL_RTS) ? 1 : 0);
            }
        }
    }

    return succ;
}
#endif
