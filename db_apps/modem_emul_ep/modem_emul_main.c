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
// Contains main function and supporting source code infrastructure
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include "rdb_ops.h"
#include "modem_emul_ep.h"

// the only globals are defined here
FILE *g_comm_host = 0;      // serial port file handle
t_conf_v250 g_conf_v250;    // configuration of modem emulator end point
t_conf_ppp g_conf_ppp;      // configuration of ppp end point
t_conf_ip_modem g_conf_ip_modem; // configuration of ip modem end point
t_conf_csd g_conf_csd;      // configuratioin of csd end point
struct rdb_session *g_rdb_session_p = NULL; // a pointer to established RDB session
int g_epb_type = 0;
t_cli_thread_info g_client_thread;
t_srv_thread_info g_server_thread;

// stores old serial port settings
struct termios old_tio;
// supports saving and restoring of old serial port configuration
static int old_settings_stored = FALSE;

#ifdef DEEP_DEBUG
static int modem_emul_verbosity = LOG_DEBUG;
#else
static int modem_emul_verbosity = LOG_ERR;
#endif

//
// a simple wrapper around printf or syslog, qualified by verbosity
//
void me_syslog(int priority, const char *format, ...)
{
    if (priority <= modem_emul_verbosity)
    {
        va_list fmtargs;
        char buffer[1024];

        va_start(fmtargs,format);
        vsnprintf(buffer,sizeof(buffer)-1, format, fmtargs);
        va_end(fmtargs);

        syslog(priority, "%s", buffer);
    }
}

//
// Set ppp configuration to default values
//
static void init_ppp_config(t_conf_ppp *p_conf)
{
    memset(p_conf, 0, sizeof(t_conf_ppp));

#ifdef DEEP_DEBUG
    p_conf->debug = 1;
#endif

    p_conf->mtu = 1500;
    p_conf->mru = 1500;

    // @TODO - work this out from the configuration of module, etc
    // set to TRUE if no other pppd processes can be running and to FALSE otherwise
    p_conf->exclusive = FALSE;
}

//
// Set v250 configuration to default values
//
static void init_v250_config(t_conf_v250 *p_conf)
{
    memset (p_conf, 0, sizeof(t_conf_v250));

    // initialize to some reasonable values
#if defined(V_GPIO_STYLE_atmel)
    strcpy(p_conf->dev_name, "/dev/ttyS2");
#elif defined(V_GPIO_STYLE_freescale)
    strcpy(p_conf->dev_name, "/dev/ttyAPP4");
#endif

    p_conf->opt_dsr = V250_DSR_ALWAYS;
    p_conf->opt_dcd = V250_DCD_CONNECT;
    p_conf->opt_ri = V250_RI_NEVER;
    p_conf->opt_1 = VERBOSE_RSLT | ECHO_ON | OK_ON_CR;
    p_conf->opt_backspace_character = 8;
    p_conf->bit_rate = 19200;
    p_conf->data_bits = 8;
    p_conf->stop_bits = 1;

    // the rest can be left at 0
}

//
// Set Modem IP end point configuration to default values
//
static void init_ip_modem_config(t_conf_ip_modem *p_conf)
{
    // can be left at 0
    memset (p_conf, 0, sizeof(t_conf_ip_modem));
}

//
// Set CSD configuration to default values
static void init_csd_config(t_conf_csd *p_conf)
{
    // can be left at 0
    memset (p_conf, 0, sizeof(t_conf_csd));
}

//
// Read v250 configuration from RDB "root" point into a caller-allocated
// v250 configuration structure. This covers modem emulator end point's
// user configurable settings
//
static int read_v250_config(const char *rdb_root, t_conf_v250 *p_conf)
{
    int dev_name_size = DEV_NAME_LENGTH;

    // since the device name may be actually a name of another
    // RDB variable where the device name is stored, need a temp variable
    char    temp[DEV_NAME_LENGTH];
    char    par_str[16];
    int     par_size = sizeof(par_str);
    memset(par_str, 0, sizeof(par_str));

    if (get_single_str_root(rdb_root, "dev_name", temp, &dev_name_size) ||
            get_single_ulong_root(rdb_root, "bit_rate", &p_conf->bit_rate) ||
            get_single_uchar_root(rdb_root, "data_bits", &p_conf->data_bits) ||
            get_single_uchar_root(rdb_root, "stop_bits", &p_conf->stop_bits) ||
            get_single_str_root(rdb_root, "parity", par_str, &par_size) ||
            get_single_uchar_root(rdb_root, "opt_1", &p_conf->opt_1) ||
            get_single_uchar_root(rdb_root, "opt_dsr", &p_conf->opt_dsr) ||
            get_single_uchar_root(rdb_root, "opt_dtr", &p_conf->opt_dtr) ||
            get_single_uchar_root(rdb_root, "opt_dcd", &p_conf->opt_dcd) ||
            get_single_uchar_root(rdb_root, "opt_ri", &p_conf->opt_ri) ||
            get_single_uchar_root(rdb_root, "opt_modem_auto_answer", &p_conf->opt_modem_auto_answer) ||
            get_single_uchar_root(rdb_root, "opt_hw_fc", &p_conf->opt_hw_fc) ||
            get_single_uchar_root(rdb_root, "opt_sw_fc", &p_conf->opt_sw_fc)) {
        me_syslog(LOG_ERR,"read config failed");
        return -1;
    }

    // these were added after initial release, so for backward compatibility
    // do not bomb out if RDB value is not there
    if (get_single_uchar_root(rdb_root, "opt_auto_answer_enable", &p_conf->opt_auto_answer_enable)) {
        p_conf->opt_auto_answer_enable = TRUE;
    }

    // this is reserved for future use. This is the capability to "knock out" the outgoing connection
    // and, instead, allow an incoming connection
    // For now, because the variable will NOT be present in the RDB, this functionality is disabled
    //
    if (get_single_uchar_root(rdb_root, "opt_in_call_connect", &p_conf->opt_in_call_connect)) {
        p_conf->opt_in_call_connect = 0;
    }

    if (strcmp(par_str, "none")==0) {
        p_conf->parity = 0;
    } else if (strcmp(par_str, "even")==0) {
        p_conf->parity = 1;
    } else if (strcmp(par_str, "odd")==0) {
        p_conf->parity = 2;
    } else {
        me_syslog(LOG_ERR,"read config failed - parity valued invalid %s", par_str);
        return -1;
    }

    dev_name_size = DEV_NAME_LENGTH;
    if (strstr(temp, "/dev/") == NULL) {
        if ((strlen(temp) == 0) || (rdb_get(g_rdb_session_p, temp, p_conf->dev_name, &dev_name_size) != 0 )) {
            me_syslog(LOG_ERR,"Invalid device name %s, %s", temp, p_conf->dev_name);
            return -1;
        }
    } else {
        // no redirection, /dev/ttyX is recorded directly in ep RDB properties
        strncpy(p_conf->dev_name, temp, DEV_NAME_LENGTH-1);
    }

    me_syslog(LOG_INFO,"read v250 config ok");
    return 0;
}

//
// Read ppp configuration from RDB "root" point into a caller-allocated
// ppp configuration structure
//
static int read_ppp_config(const char *rdb_root, t_conf_ppp *p_conf)
{
    int ip_addr_size_srv = IP_ADDR_LENGTH, ip_addr_size_cli = IP_ADDR_LENGTH;
    if (get_single_str_root(rdb_root, "ip_addr_srv", p_conf->ip_addr_srv, &ip_addr_size_srv) ||
            get_single_str_root(rdb_root, "ip_addr_cli", p_conf->ip_addr_cli, &ip_addr_size_cli) ||
            get_single_ulong_root(rdb_root, "mru", &p_conf->mru) ||
            get_single_ulong_root(rdb_root, "mtu", &p_conf->mtu) ||
            get_single_int_root(rdb_root, "raw_ppp", &p_conf->raw_ppp) ||
            get_single_int_root(rdb_root, "disable_ccp", &p_conf->disable_ccp)
        ) {
        return -1;
    }

    me_syslog(LOG_INFO,"Read PPP config ok");
    return 0;
}

//
// Read ip conn end point configuration from RDB "root" point into a caller-allocated
// ip conn configuration structure
//
static int read_ip_modem_config(const char *rdb_root, t_conf_ip_modem *p_conf)
{
    int ip_addr_size = IP_ADDR_LENGTH;
    int ident_size = IDENT_LENGTH;
    if (get_single_str_root(rdb_root, "ip_address", p_conf->ip_or_host_remote, &ip_addr_size) ||
            get_single_int_root(rdb_root, "port_remote", &p_conf->port_remote) ||
            get_single_int_root(rdb_root, "port_local", &p_conf->port_local) ||
            get_single_int_root(rdb_root, "is_udp", &p_conf->is_udp) ||
            get_single_str_root(rdb_root, "ident", p_conf->ident, &ident_size)) {
        return -1;
    }

    // TCP only configuration options
    if (!p_conf->is_udp) {
        if (get_single_int_root(rdb_root, "exclusive", &p_conf->exclusive) ||
                get_single_int_root(rdb_root, "keep_alive", &p_conf->keep_alive) ||
                get_single_int_root(rdb_root, "keepcnt", &p_conf->keepcnt) ||
                get_single_int_root(rdb_root, "keepidle", &p_conf->keepidle) ||
                get_single_int_root(rdb_root, "keepintvl", &p_conf->keepintvl) ||
                get_single_int_root(rdb_root, "no_delay", &p_conf->no_delay)) {
            return -1;
        }
    }

    me_syslog(LOG_INFO,"Read IP Modem config ok");
    return 0;
}

static int read_csd_config(const char *rdb_root, t_conf_csd *p_conf)
{

    int at_str_size = MAX_CMD_LENGTH;
    if (get_single_int_root(rdb_root, "inactivity_timeout_mins", &p_conf->inactivity_timeout_mins) ||
            get_single_int_root(rdb_root, "init_method", &p_conf->init_method) ||
            get_single_str_root(rdb_root, "additional_init_str", p_conf->additional_init_str, &at_str_size)) {
        return -1;
    }
    me_syslog(LOG_INFO,"Read CSD config ok");
    return 0;
}


//
// Print usage
//
static void usage(void)
{
    printf("Usage: modem_emul_ep epa_root epa_type epa_mode epb_root epb_type epb_mode\n"
            "For example: modem_emul_ep service.dsm.ep.conf.my_modem 11 1 service.dsm.ep.conf.my_ppp 12 1\n"
            "because Modem Emulator End point is type 11 and PPP end point is type 12\n"
            "and both end point modes are raw (mode 1)\n"
          );
}

//
// Looks slightly neater when conditional compilation is located in one function
//
static void _gpio_close(void)
{
#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
    gpio_exit();
#endif
}

//
// Common shutdown actions
//
static void _shutdown_me(const char *msg, int exit_code)
{
    me_syslog(LOG_ERR, msg);
    rdb_close(&g_rdb_session_p);
    _gpio_close();
    exit(exit_code);
}

//
// Perform end point specific cleanup. This allows to stop data
// mover threads and do any cleanup necessary - for example
// return termios to pre-existing setting
//
static void end_point_specific_cleanup(int ep_type)
{
    int count = 3; // wait for up to 3 seconds for threads to exit
    switch (ep_type) {
        case EP_PPP:
            terminate_ppp_daemon();
            sleep(1);
            break;

        case EP_IP_MODEM:
            g_client_thread.do_exit = TRUE;
            g_server_thread.do_exit = TRUE;
            // @TODO - try waiting for .is_running to be FALSE
            //while (count-- && (g_server_thread.is_running || g_client_thread.is_running)) {
            //    sleep(1);
            //}
            break;

        case EP_CSD:
            csd_cleanup();
            break;
    }
}

//
// Catch three signals, clean up and exit
//
static void sig_handler(int signum)
{
    switch(signum) {
    case SIGINT:
    case SIGTERM:
    case SIGQUIT:
        me_syslog(LOG_DEBUG,"Signal %d received, quitting\n", signum);
        end_point_specific_cleanup(g_epb_type);
        clear_modem_ctrl_lines();
        sleep(1);
        if (old_settings_stored)
            tcsetattr(fileno(g_comm_host), TCSANOW, &old_tio);
        if (g_comm_host)
            fclose(g_comm_host);
        if (g_rdb_session_p)
            rdb_close(&g_rdb_session_p);
        _gpio_close();
        exit(0);
        break;

    default:
        me_syslog(LOG_DEBUG,"Unexpected signal received %d\n", signum);
        break;
    }
}

static void sig_handler_pipe(int signum)
{
    switch(signum) {
    case SIGPIPE:
        //g_client_thread.connected_status = FALSE;
        me_syslog(LOG_INFO, "Sig pipe received");
        break;
    }
}

//
// Initializes the serial port
// Sets up hw/sw flow control, baud rate and data line settings
// as well as non-configurable default settings for the port
//
int initialize_serial_port(void)
{
    // set defaults for the port related to the setting that are not user-configurable
    if (set_port_defaults(g_comm_host) < 0) {
        me_syslog(LOG_ERR, "Failed to set serial port defaults");
        return -1;
    }

    // set h/w flow control
    if (set_hw_flow_ctrl(g_comm_host, g_conf_v250.opt_hw_fc) < 0) {
        me_syslog(LOG_ERR, "Failed to set h/w flow control %d", g_conf_v250.opt_hw_fc);
        return -1;
    }

    // only enable s/w flow control if hw flow control is not already enabled
    if (!g_conf_v250.opt_hw_fc) {
        if (set_sw_flow_ctrl(g_comm_host, g_conf_v250.opt_sw_fc) < 0) {
            me_syslog(LOG_ERR, "Failed to set s/w flow control %d", g_conf_v250.opt_hw_fc);
            return -1;
        }
    }

    // set baud rate
    if (set_baud_rate(g_comm_host, g_conf_v250.bit_rate) < 0) {
        me_syslog(LOG_ERR, "Failed to set baud rate %ld", g_conf_v250.bit_rate);
        return -1;
    }

    // set character format (data bits, stop bits, parity)
    if (set_character_format(g_comm_host, g_conf_v250.data_bits, g_conf_v250.parity, g_conf_v250.stop_bits) < 0) {
        me_syslog(LOG_ERR, "Failed to set character format (data %d parity %d stop bits %d)",
               g_conf_v250.data_bits, g_conf_v250.parity, g_conf_v250.stop_bits);
        return -1;
    }
    return 0;
}

static int get_ep_type(const char *ep_str)
{
    int ep_type = atoi(ep_str);
    me_syslog(LOG_DEBUG, "Argv %s, EP type %d\n", ep_str, ep_type);
    return ep_type;
}

static int get_ep_mode(const char *ep_str)
{
    int ep_mode = atoi(ep_str);
    me_syslog(LOG_DEBUG, "Argv %s, EP mode %d\n", ep_str, ep_mode);
    return ep_mode;
}

BOOL is_epb_type_ppp(void)
{
    return (g_epb_type == EP_PPP) ? TRUE : FALSE;
}

BOOL is_epb_type_ip_modem(void)
{
    return (g_epb_type == EP_IP_MODEM) ? TRUE : FALSE;
}

BOOL is_epb_type_csd(void)
{
    // circuit switched data connection
    return (g_epb_type == EP_CSD) ? TRUE : FALSE;
}

//
// C code main entry point
//
int main(int argc, char *argv[])
{
    int fd;
    int epa_type, epa_mode, epb_mode;

    if (argc != 7) {
        usage();
        exit(EXIT_FAILURE);
    }

    // do a sanity checking on argv
    if (!strstr(argv[1], "service.dsm.ep.conf") ||
            !strstr(argv[4], "service.dsm.ep.conf")) {
        usage();
        exit(EXIT_FAILURE);
    }

    // open an rdb session
    if (rdb_open(NULL, &g_rdb_session_p) || !g_rdb_session_p) {
        me_syslog(LOG_ERR, "failed to open RDB!" );
        exit(EXIT_FAILURE);
    }

#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
    // on with direct GPIO control initialize gpio driver
    if (gpio_init("/dev/gpio") < 0) {
        me_syslog(LOG_ERR, "gpio_init(/dev/gpio) failed - terminating");
        exit(EXIT_FAILURE);
    }
#endif

    // set up signal handlers
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGPIPE, sig_handler_pipe);

    // initialize the modem end point object regardless of end point b type
    init_v250_config(&g_conf_v250);
    // read v250 configuration from RDB
    if (read_v250_config(argv[1], &g_conf_v250) < 0) {
        _shutdown_me("failed to read v250 configuration", EXIT_FAILURE);
    }

#ifdef DEEP_DEBUG
    // if needed, print it
    print_v250_config(&g_conf_v250);
#endif

    //
    // note: argv[2] should always be Modem Emulator end point type (11)
    // End point Modes (argv[3] and argv[6] should always be Raw)
    //
    epa_type = get_ep_type(argv[2]);
    epa_mode = get_ep_mode(argv[3]);
    g_epb_type = get_ep_type(argv[5]);
    epb_mode = get_ep_mode(argv[6]);

    if ((epa_type !=  EP_MODEM_EMULATOR) || (epa_mode != EP_MODE_RAW) || (epb_mode != EP_MODE_RAW)) {
        me_syslog(LOG_ERR, "Unsupported end point (type/mode), A(%d, %d), B(%d, %d)",
            epa_type, epa_mode, g_epb_type, epb_mode);
        _shutdown_me("failed to read v250 configuration", EXIT_FAILURE);
    }

    //
    // Get the logging level, default to LOG_NOTICE if a special "debug enable" RDB variable is not found.
    // This is because we do not want to fill the log with messages that could be the result
    // of connecting the wrong device, or using a wrong configuration, to the serial line.
    // Normally, there will be no RDB variable for this but if needed, the user can
    // enable further log messages by creating the service.dsm.modem_emul.log_level variable
    // with a numeric value that matches LOG_xx definitions from syslog.h. Setting it to 7 (LOG_DEBUG)
    // will enable everything to be logged (if syslog capture filter level enables this level).
    //
    if (rdb_get_int(g_rdb_session_p, "service.dsm.modem_emul.logging_level", &modem_emul_verbosity) != 0) {
        modem_emul_verbosity = LOG_NOTICE;
    }
    // since we implement our own verbosity filter, set log mask to log everything
    setlogmask(LOG_UPTO(LOG_DEBUG));
    me_syslog(LOG_NOTICE, "Set dsm modem emulator logging level to %d", modem_emul_verbosity);

    //
    // Initialize the physical serial port. This section has to run before the
    // data mover threads are started, since they need the serial port's file descriptor
    // (g_comm_host)
    //
#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
    // on platforms with direct i/o control of modem pins,
    // initialize them
    V24_init_GPIO();
#endif

    // open comms port
    g_comm_host = open_serial_port(g_conf_v250.dev_name);
    if (!g_comm_host) {
        _shutdown_me("Failed to open serial port", EXIT_FAILURE);
    }

    // save current serial port settings
    fd = fileno(g_comm_host);
    tcgetattr(fd, &old_tio);
    old_settings_stored = TRUE;
#ifdef DEEP_DEBUG
    print_termios("Before change", &old_tio);
#endif
    // initialize it
    if (initialize_serial_port() < 0) {
        // restore old settings
        tcsetattr(fd, TCSANOW, &old_tio);
        fclose(g_comm_host);
        // shutdown
        _shutdown_me("Failed to initialize serial port", EXIT_FAILURE);
    }

#ifdef DEEP_DEBUG
    struct termios new_tio;
    tcgetattr(fd, &new_tio);
    print_termios("After change", &new_tio);
#endif

    switch (g_epb_type) {
        case EP_PPP:
            init_ppp_config(&g_conf_ppp);
            // read ppp config from RDB
            if (read_ppp_config(argv[4], &g_conf_ppp) < 0) {
                _shutdown_me("Failed to read ppp configuration", EXIT_FAILURE);
            }

#ifdef DEEP_DEBUG
            print_ppp_config(&g_conf_ppp);
#endif
            // prepare PPP configuration files as per configuration read
            if (prepare_ppp_files(&g_conf_v250, &g_conf_ppp) < 0) {
                _shutdown_me("Failed to prepare PPP", EXIT_FAILURE);
            }
            break;

        case EP_IP_MODEM:
            init_ip_modem_config(&g_conf_ip_modem);
            if (read_ip_modem_config(argv[4], &g_conf_ip_modem) < 0) {
                _shutdown_me("Failed to read IP modem configuration", EXIT_FAILURE);
            }
#ifdef DEEP_DEBUG
            print_ip_modem_config(&g_conf_ip_modem);
#endif
            memset(&g_client_thread, 0, sizeof(g_client_thread));
            if (g_conf_ip_modem.port_remote) {
                g_client_thread.sleep_time_ms = THREAD_SLEEP_TIME_MS;
                g_client_thread.select_timeout_ms = SELECT_TIMEOUT_MS;
                if (pthread_mutex_init(&g_client_thread.mutex, NULL) != 0) {
                    _shutdown_me("Failed to initialize client thread mutex", EXIT_FAILURE);
                }
                if (prepare_ip_modem_cli(&g_client_thread) != 0) {
                    _shutdown_me("Failed to prepare client thread", EXIT_FAILURE);
                }
            } else {
                // both local and remote ports cannot be 0. Makes no sense to allow this
                // as no data will ever be moved
                if (!g_conf_ip_modem.port_local) {
                    _shutdown_me("Failed to prepare IP Modem end point. Both ports are set to 0", EXIT_FAILURE);
                } else {
                    me_syslog(LOG_INFO, "Client connection disabled (remote port is 0)");
                }
            }

            memset(&g_server_thread, 0, sizeof(g_server_thread));
            if (g_conf_ip_modem.port_local) {
                g_server_thread.sleep_time_ms = THREAD_SLEEP_TIME_MS;
                g_server_thread.select_timeout_ms = SELECT_TIMEOUT_MS;
                g_server_thread.listen_select_timeout_ms = SELECT_TIMEOUT_MS;
                g_server_thread.exclusive = g_conf_ip_modem.exclusive;
                if (pthread_mutex_init(&g_server_thread.mutex, NULL) != 0) {
                    _shutdown_me("Failed to initialize server thread mutex", EXIT_FAILURE);
                }
                if (prepare_ip_modem_srv(&g_server_thread, &g_conf_ip_modem) != 0) {
                    _shutdown_me("Failed to prepare server thread", EXIT_FAILURE);
                }
            } else {
                me_syslog(LOG_INFO, "Server connection disabled (local port is 0)");
            }
            break;

        case EP_CSD:
            init_csd_config(&g_conf_csd);
            if (read_csd_config(argv[4], &g_conf_csd) < 0) {
                _shutdown_me("Failed to read CSD configuration", EXIT_FAILURE);
            }
#ifdef DEEP_DEBUG
            print_csd_config(&g_conf_csd);
#endif
            if (prepare_csd(&g_conf_csd) != 0) {
                _shutdown_me("Failed to prepare CSD", EXIT_FAILURE);
            }
            break;

        default:
            me_syslog(LOG_ERR, "Unsupported end point type %d", g_epb_type);
            exit(EXIT_FAILURE);
            break;
    }

    // enter the control loop
    while (1) {

        // at command engine. Not used in CSD Mode.
        if (!is_epb_type_csd()) {
            if (is_epb_type_ppp() && g_conf_ppp.raw_ppp) {
                // if raw ppp is selected, bypass AT command engine - pppd is always running
                maintain_ppp_connection();
            } else {
                CDCS_V250Engine(g_comm_host);
            }
        }

        // scan modem control lines (inputs)
        if (scan_modem_ctrl_lines_status() < 0) {
            me_syslog(LOG_ERR, "Scan modem control lines failed");
            break;
        }

        // control modem lines (outputs)
        if (is_epb_type_ppp()) {
            if (control_modem_output_lines_ppp() < 0) {
                me_syslog(LOG_ERR, "Control modem output lines (PPP) failed");
                break;
            }
        }
        else if (is_epb_type_ip_modem()) {
            if (control_modem_output_lines_ip() < 0) {
                me_syslog(LOG_ERR, "Control modem output lines (IP Modem) failed");
                break;
            }
        }
        else if (is_epb_type_csd()) {
            if (do_csd() < 0) {
                me_syslog(LOG_ERR, "Control modem output lines (CSD) failed");
                break;
            }
        }

        // sleep for a while
        usleep(POLL_TIME_US); // 100 milliseconds
    }

    //
    // in the regular course of event, we never exit the above loop - only via
    // signal, which is handled in sig_handler
    //

    // clear modem control lines
    clear_modem_ctrl_lines();

    // tidy up
    tcsetattr(fd, TCSANOW, &old_tio);

#ifdef DEEP_DEBUG
    // print just to prove we restored to original serial port settings
    tcgetattr(fd, &old_tio);
    print_termios("After restore", &old_tio);
#endif

    fclose(g_comm_host);
    rdb_close(&g_rdb_session_p);
    _gpio_close();

    return 0;
}
