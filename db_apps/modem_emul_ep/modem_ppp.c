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
//
// Contains functions dealing with ppp connection - which we launch, monitor and tear down.
// Unlike the old modem emulator, ppp is not terminated on the module, but instead
// we run ppp server ourselves. This completely de-couples this (dial up client support)
// functionality from any code dealing with 3G/4G/LTE modules.
//
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <sys/socket.h> // for socket(), connect(), send(), and recv()
#include <arpa/inet.h>  // for sockaddr_in and inet_addr()
#include <sys/ioctl.h>
#include <linux/if.h>
#include <fcntl.h>

#include "modem_emul_ep.h"

#define PPP_POLL_TIME_US 1000000	// 1 second poll - things are tuned to this so don't change
#define PPP_CONN_RETRIES 6			// connection attempts before giving up.

// declare only externs needed
extern t_conf_v250 g_conf_v250;
extern t_conf_ppp g_conf_ppp;

//
// TRUE if our router is registered on the network, but it is not necessary to
// have a fully working upstream connection
// TODO:
// Functionally, this is meant to be the &S1 option which states that:
// "DSR is ON when signal present and phone registered on the network".
//
// May be this is not exactly the same as Sim Ok status - review this.
// Also, code below is quite un-generic
//
static BOOL get_registered_status(void)
{
    return (strcmp(getSingle("wwan.0.sim.status.status"), "SIM OK") == 0);
}

//
// TRUE if pdp session is up
// TODO - this may functionaly be not entirely correct
// The old modem emulator manual states:
// "On when cellular PPP session is established" probably
// meaning PDP
// Also, code below is quite un-generic
//
static BOOL get_pdp_session_status(void)
{
    // 1) read wwan.0.profile.current to get the current profile
    // 2) read link.profile.X.status to see if it is up
    // If both are TRUE then pdp session is up and running
    char rdb_name[32];
    int current_profile = getSingleInt("wwan.0.profile.current");


    if ((current_profile >= 1) && (current_profile <= 6)) {
        snprintf(rdb_name, sizeof(rdb_name), "link.profile.%d.status", current_profile);
        return (strcmp(getSingle(rdb_name), "up") == 0);
    }
    return FALSE;
}

//
// monitors for an active PPP session. Largely borrowed from the old 882 code
// and works well
//
// Return ppp status as per e_ppp_connection_status enumeration
//
e_ppp_connection_status get_ppp_connection_status(void)
{
    struct ifreq ifr;
    int sockfd;

    // has to be static as we only want it to be UNDEFINED on startup
    static e_ppp_connection_status conn_status = STATUS_UNDEFINED;

    // NOTE - that this socket is just created to be used as a hook in to the socket layer
    // for the ioctl call.
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd != -1) {
        strcpy(ifr.ifr_name, "ppp0");

        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) != -1) {
            if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1) {
                conn_status = STATUS_IDLE;
                close(sockfd);
                return conn_status;
            } else {
                if (ifr.ifr_flags & IFF_UP) {
                    close(sockfd);
                    conn_status = STATUS_CONNECTED;
                    return conn_status;
                } else {
                    conn_status = STATUS_IDLE;
                    close(sockfd);
                    return conn_status;
                }
            }
        } else {
            conn_status = STATUS_IDLE;
        }
    } else {
        // old conn_status will be returned
        fprintf(stderr, " no socket %d %s\r\n", sockfd, strerror(errno));
    }

    close(sockfd);
    return conn_status;
}

//
// When ppp daemon was laucnhed, it was told via linkname option to
// save its pid in a file. This function reads this pid and returns
// to the caller.
// Returns pid if successfully read from the file, or -1 if any
// error occurred
long int read_daemon_pid(const char* pid_file_name)
{
    char buf[10];
    FILE *fp;
    long int pid;

    // open
    fp = fopen(pid_file_name, "r");
    if (!fp)
        return -1;

    // read a few bytes enough to fit the integer with pid
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return -1;
    }

    // close file
    fclose(fp);
    pid = strtol(buf, NULL, 10);

    // check overflow and underflow and return -1 if either occurred
    return ((pid != LONG_MIN) && (pid != LONG_MAX)) ? pid : -1;
}

//
// Try to avoid killing other pppd processes that may be nothing to do with us.
// So firstly, we try to kill only instance started by us, by looking up its pid.
// If this fails, and exclusive flag is set, we kill pppd processes wholesale.
//
// The idea is that exclusive flag can be set on platforms that do not use pppd
// for any other connectivity - this depends on the type of module used.
//
// Note this function is called just in case before ppp is launched even if
// ppp daemon was cleanly killed in the previous session.
//
void terminate_ppp_daemon(void)
{
    char buf[128];
    long int pid;
    sprintf(buf, "/var/run/ppp-%s.pid", g_conf_ppp.tty_name);

    pid = read_daemon_pid(buf);

    // try to kill one process only by pid which we just found
    if (pid > 0) {
        // delete the file with pid as we no longer need it
        unlink(buf);

        // reuse the buffer to format the command
        sprintf(buf, "kill -9 %ld", pid);
        me_syslog(LOG_DEBUG, "Killing pppd via command %s", buf);
        system(buf);

    } else {
        //
        // Could not find pid. Make another attempt - if we are
        // in exclusive mode, no other ppp daemons are possible,
        // so kill all pppd processes in the system
        //
        if (g_conf_ppp.exclusive) {
            me_syslog(LOG_DEBUG, "Killing pppd via killall command");
            system("killall pppd >/dev/null 2>&1");
        } else {
            me_syslog(LOG_DEBUG, "No ppp daemon to kill");
        }
    }
}

//
// Start ppp daemon. Format the command string based on configuration
// and launch via system command
//
// Return : not needed as there is  no easy way to work out if daemon actually
// launched successfully. We use get_ppp_connection_status instead.
//
void start_ppp_daemon(t_conf_v250 *p_conf_v250, t_conf_ppp *p_conf_ppp)
{
    char ppp_command_string[MAX_CMD_LENGTH];

    //
    // "local" option is the opposite of the "modem" option meaning modem control
    // lines are NOT touched by PPP which is exactly what we want
    //
    snprintf(ppp_command_string, MAX_CMD_LENGTH, "pppd local -detach %s %ld linkname %s &",
        g_conf_v250.dev_name, g_conf_v250.bit_rate, g_conf_ppp.tty_name);
    me_syslog(LOG_DEBUG, "Executing data connection system command %s", ppp_command_string);
    system(ppp_command_string);
}

//
// Function controlling modem lines which are specific to PPP
// At the momemnt, these 3 functions return 0 regardless. An error could be returned (-1) and
// will be propagated back to caller function - control_modem_output_lines_xxx
//
// Control the External Serial port's DSR line according to the state of the device
//
static int ppp_control_dsr(void)
{
    switch (g_conf_v250.opt_dsr) {
    case V250_DSR_ALWAYS:
        dsr_on();
        break;

    case V250_DSR_REGISTERED:
        if (get_registered_status())
            dsr_on();
        else
            dsr_off();
        break;

    case V250_DSR_PDP:
        if (get_pdp_session_status())
            dsr_on();
        else
            dsr_off();
        break;

    case V250_DSR_NEVER:
        dsr_off();
        break;

    case V250_DSR_MIMIC_DTR:
        if (is_dtr_high())
            dsr_on();
        else
            dsr_off();
        break;
    default:
        dsr_on();
        break;
    }
    return 0;
}

//
// Control the External Serial port's DCD line according to the state of the device
//
static int ppp_control_dcd(void)
{
    int modem_state = get_modem_state();
    switch (g_conf_v250.opt_dcd) {

    case V250_DCD_ALWAYS:
        dcd_on();
        break;

    case V250_DCD_CONNECT:
        if ((modem_state != ON_LINE_COMMAND) && (modem_state != ON_LINE))
            dcd_off();
        else
            dcd_on();
        break;

    case V250_DCD_NEVER:
        dcd_off();
        break;

    case V250_DCD_PDP:
        if (get_pdp_session_status())
            dcd_on();
        else
            dcd_off();
        break;

    default:
        dcd_on();
        break;
    }
    return 0;
}

//
// Control the External Serial port's RI line according to the state of the device
//
static int ppp_control_ri(void)
{
    switch (g_conf_v250.opt_ri) {
    case V250_RI_ALWAYS:
        ri_on();
        break;

    // there is no ring in possible in PPP mode
    case V250_RI_RING:
        ri_off();
        break;

    default:
    case V250_RI_NEVER:
        ri_off();
        break;
    }
    return 0;
}


//
// Calls 3 other functions that control DCD, RI and DSR
// The idea is that this is called periodically in control loop
// (at the same time as scan_modem_ctrl_lines_status is called)
// Returns 0 on success or -1 on error
//
int control_modem_output_lines_ppp(void)
{
    if (ppp_control_dcd() < 0) {
        me_syslog(LOG_ERR,"failed to control DCD");
        return -1;
    }
    if (ppp_control_ri() < 0) {
        me_syslog(LOG_ERR,"failed to control RI");
        return -1;
    }
    if (ppp_control_dsr() < 0) {
        me_syslog(LOG_ERR,"failed to control DSR");
        return -1;
    }

    return 0;
}

//
// This function is called when dial command in AT engine detects that a PPP
// connection should be established (other, non-PPP types of end point B
// will be handled in the future as well)
//
// It will only return when ppp connection has terminated for whatever reason
// Since control is no longer in the modem emulator poll loop, it is necessary
// to explicitely call scan_modem_ctrl_lines_status and control_modem_output_lines
// here - so ppp daemon deals with data through the port but this control look
// deals with modem control lines (with exception of CTS and RTS that are
// also handled by ppp via the driver)
//
// Built as a state machine with 3 states
//
void maintain_ppp_connection(void)
{
    // supports state
    typedef enum {
        LAUNCHING_DAEMON,
        RUNNING_DAEMON,
        SHUTTING_DOWN_DAEMON
    } e_local_state_machine;

    e_local_state_machine state = LAUNCHING_DAEMON;
    int retries = PPP_CONN_RETRIES;
    int sleep_time; // in microseconds
    int status = STATUS_IDLE;

    // control loop
    while (1) {

        // reload sleep time to default
        sleep_time = PPP_POLL_TIME_US;

        //
        // read inputs - really only want to know the status of DTR.
        // also, control 3 outputs - DCD, RI and DSR
        //
        if ((scan_modem_ctrl_lines_status() < 0) ||
                (control_modem_output_lines_ppp() < 0)) {
            // force exit on any error
            me_syslog(LOG_ERR,"Failed to control/scan modem lines");
            state = SHUTTING_DOWN_DAEMON;
        }

        // a simple state machine
        switch (state) {

        case LAUNCHING_DAEMON:
            // terminate any existing pppd related to this data stream
            terminate_ppp_daemon();

            // start a new one. There is no return code but we will find
            // if ppp is running soon enough
            start_ppp_daemon(&g_conf_v250, &g_conf_ppp);

            // go to next state
            state = RUNNING_DAEMON;

            // reload retry counter
            retries = PPP_CONN_RETRIES;
            break;

        case RUNNING_DAEMON:
            // check connection status
            status = get_ppp_connection_status();
            if (status == STATUS_CONNECTED) {
                // all good, nothing to do other than check for DTR
                retries = PPP_CONN_RETRIES; // reload number of retries

                // check DTR pin low, only if we are told to monitor for this condition
                // if so, drop the PPP connection ASAP
                if (!is_dtr_high() && ((g_conf_v250.opt_dtr == V250_DTR_COMMAND) || (g_conf_v250.opt_dtr == V250_DTR_HANGUP))) {
                    state = SHUTTING_DOWN_DAEMON;
                    sleep_time = 0; // drop connection as soon as possible.
                    break;
                }
            } else {
                // no ppp. After retries, enter SHUTTING_DOWN state
                if (--retries <= 0) {
                    state = SHUTTING_DOWN_DAEMON;
                }
            }
            break;

        case SHUTTING_DOWN_DAEMON:
        default:
            terminate_ppp_daemon();
            return;
        }

        if (sleep_time)
            usleep(sleep_time);

#ifdef DEEP_DEBUG
        printf("State %d, call status %d DTR %d\n", state, status, is_dtr_high());
#endif
    }
}

// It is possible to write options to options.devName, or just to options.
// Both seem to work. It is better to use devName file to avoid
// intefering with any other ppp daemons running
//
// Prepares ppp configuration files that are needed before launching ppp daemon
// 1) Generate /etc/ppp/option file
// 2) Generate /etc/ppp/options.devName file where devName depends on the name of
//  the serial device
//
// Note that this function is called when modem emulator first starts, NOT when
// ATD is detected. There should be nothing else that interferes with ppp
// configuration files, so this approach should be fine. The alternative is
// to generate these files just before ppp daemon starts.
//
int prepare_ppp_files(t_conf_v250 *p_conf_v250, t_conf_ppp *p_conf_ppp)
{
    // form the name of options.dev
    char *pos_fw_slash1, *pos_fw_slash2; // fw slash position in dev name, e.g. /dev/ttyAPP4
    char opt_file_name[FILE_NAME_LENGTH];
    FILE *opt_file;

    // sanity check
    if (!p_conf_v250 || !p_conf_ppp || !p_conf_v250->dev_name ||
            !p_conf_ppp->ip_addr_srv || !p_conf_ppp->ip_addr_cli) {
        me_syslog(LOG_ERR,"Failed to create PPP configuration files");
        return -1;
    }

    // work out the name of /etc/ppp/options.devName file
    pos_fw_slash1 = strchr(p_conf_v250->dev_name, '/');
    pos_fw_slash2 = strrchr(p_conf_v250->dev_name, '/');

    // should be offset by 4 characters which is the length of /dev/
    if (!pos_fw_slash1 || !pos_fw_slash2 || ((pos_fw_slash2 - pos_fw_slash1) != 4)) {
        me_syslog(LOG_ERR,"Device name in unknown format %s", p_conf_v250->dev_name);
        return -1;
    }

    // memorize tty device name as we will use it for the pid file name
    strncpy(p_conf_ppp->tty_name, pos_fw_slash2+1, DEV_NAME_LENGTH-1);
    sprintf(opt_file_name, "/etc/ppp/options.%s", p_conf_ppp->tty_name);

    if ((opt_file = fopen(opt_file_name, "w")) == 0) {
        me_syslog(LOG_ERR,"Failed to open %s file\n", opt_file_name);
        return -1;
    }

    fprintf(opt_file, "# Auto-generated\n");

    fprintf(opt_file, "-detach\n");
    fprintf(opt_file, "nodefaultroute\n");
    fprintf(opt_file, "netmask 255.255.255.255\n");
    fprintf(opt_file, "%s:%s\n", p_conf_ppp->ip_addr_srv, p_conf_ppp->ip_addr_cli);
    fprintf(opt_file, "proxyarp\n");
    fprintf(opt_file, p_conf_v250->opt_hw_fc ? "crtscts\n" : "#crtscts\n");
    fprintf(opt_file, "noauth\n");
    fprintf(opt_file, "ms-dns %s\n", p_conf_ppp->ip_addr_srv);
    fprintf(opt_file, p_conf_ppp->debug ? "debug\n" : "#debug\n");
    fprintf(opt_file, "local\n");
    fprintf(opt_file, "mtu %ld\n", p_conf_ppp->mtu);
    fprintf(opt_file, "mru %ld\n", p_conf_ppp->mru);

    // some old RTUs don't support the PPP protocol field value of 80fd -
    // (Compression Control protocol - CCP)
    if (p_conf_ppp->disable_ccp) {
        fprintf(opt_file, "noccp\n");
    }

    // should these 2 echo options be user-configurable (e.g. through Web UI)?
    fprintf(opt_file, "lcp-echo-failure 3\n");  // 3 failures means peer is dead
    fprintf(opt_file, "lcp-echo-interval 5\n"); // echos every 5 seconds

    fclose(opt_file);

    return 0;
}


