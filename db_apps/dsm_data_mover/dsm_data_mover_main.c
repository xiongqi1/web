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
// Data Stream Manager Data Mover application.
// Initial usage is:
// 1) to replace socat in Modbus applications,
// 2) client on demand end point
//
// Intended future usage is 1:N data streams (one end point to many).
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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include "rdb_ops.h"
#include "dsm_data_mover.h"

// the only globals are defined here
FILE *g_comm_host = 0;      // serial port file handle
struct rdb_session *g_rdb_session_p = NULL; // a pointer to established RDB session
t_cli_thread_info g_client_thread;
t_srv_thread_info g_server_thread;

t_conf_serial g_conf_serial;
t_conf_tcp_client g_conf_tcp_client;
t_conf_tcp_client_cod g_conf_tcp_client_cod;
t_conf_tcp_server g_conf_tcp_server;
#ifdef UDP_SUPPORT_ENABLED
t_conf_udp_client g_conf_udp_client;
t_conf_udp_server g_conf_udp_server;
#endif

// stores old serial port settings
struct termios old_tio;
// supports saving and restoring of old serial port configuration
static int old_settings_stored = FALSE;

#ifdef DEEP_DEBUG
static int dsm_data_mover_verbosity = LOG_DEBUG;
#else
static int dsm_data_mover_verbosity = LOG_ERR;
#endif

extern void reset_modbus_error_counters(void);

//
// a simple wrapper around printf or syslog, qualified by verbosity
//
void dsm_dm_syslog(int priority, const char *format, ...)
{
    if (priority <= dsm_data_mover_verbosity)
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
// Helper to reset the TCP server configuration structure
//
static void init_tcp_server_config(t_conf_tcp_server *p_conf)
{
    memset (p_conf, 0, sizeof(t_conf_tcp_server));
}

//
// Helper to reset the TCP client configuration structure
//
static void init_tcp_client_config(t_conf_tcp_client *p_conf)
{
    memset (p_conf, 0, sizeof(t_conf_tcp_client));
}

//
// Helper to reset the TCP client on demand configuration structure
//
static void init_tcp_client_cod_config(t_conf_tcp_client_cod *p_conf)
{
    memset (p_conf, 0, sizeof(t_conf_tcp_client_cod));
}

#ifdef UDP_SUPPORT_ENABLED
//
// A helper to reset the UDP server configuration structure
//
static void init_udp_server_config(t_conf_udp_server *p_conf)
{
    memset (p_conf, 0, sizeof(t_conf_udp_server));
}

//
// A helper to reset the UDP client configuration structure
//
static void init_udp_client_config(t_conf_udp_client *p_conf)
{
    memset (p_conf, 0, sizeof(t_conf_udp_client));
}
#endif

//
// A helper to reset the serial port configuration structure
//
static void init_serial_config(t_conf_serial *p_conf)
{
    memset (p_conf, 0, sizeof(t_conf_serial));
}

//
// Read serial configuration from RDB "root" point into a caller-allocated
// serial configuration structure. This covers generic serial end points,
// as well as RS232, RS422 and RS485 end points
//
static int read_serial_config(const char *rdb_root, int epa_type, int epa_mode, t_conf_serial *p_conf)
{
    int dev_name_size = DEV_NAME_LENGTH;

    // since the device name may be actually a name of another
    // RDB variable where the device name is stored, need a temp variable
    char    temp[DEV_NAME_LENGTH];
    char    par_str[16];
    int     par_size = sizeof(par_str);
    memset(par_str, 0, sizeof(par_str));

    p_conf->end_point_type = epa_type;
    p_conf->end_point_mode = epa_mode;

    if (get_single_str_root(rdb_root, "dev_name", temp, &dev_name_size) ||
            get_single_ulong_root(rdb_root, "bit_rate", &p_conf->bit_rate) ||
            get_single_uchar_root(rdb_root, "data_bits", &p_conf->data_bits) ||
            get_single_uchar_root(rdb_root, "stop_bits", &p_conf->stop_bits) ||
            get_single_str_root(rdb_root, "parity", par_str, &par_size)) {
        dsm_dm_syslog(LOG_ERR,"read config failed");
        return -1;
    }

    if (strcmp(par_str, "none")==0) {
        p_conf->parity = 0;
    } else if (strcmp(par_str, "even")==0) {
        p_conf->parity = 1;
    } else if (strcmp(par_str, "odd")==0) {
        p_conf->parity = 2;
    } else {
        dsm_dm_syslog(LOG_ERR,"read config failed - parity valued invalid %s", par_str);
        return -1;
    }

    dev_name_size = DEV_NAME_LENGTH;
    if (strstr(temp, "/dev/") == NULL) {
        if ((strlen(temp) == 0) || (rdb_get(g_rdb_session_p, temp, p_conf->dev_name, &dev_name_size) != 0 )) {
            dsm_dm_syslog(LOG_ERR,"Invalid device name %s, %s", temp, p_conf->dev_name);
            return -1;
        }
    } else {
        // no redirection, /dev/ttyX is recorded directly in ep RDB properties
        strncpy(p_conf->dev_name, temp, DEV_NAME_LENGTH-1);
    }

    dsm_dm_syslog(LOG_INFO,"read serial config ok");
    return 0;
}

//
// Read tcp server configuration
//
static int read_tcp_server_config(const char *rdb_root, t_conf_tcp_server *p_conf)
{
    p_conf->end_point_mode = EP_MODE_RAW;

    if (get_single_int_root(rdb_root, "port_number", &p_conf->port_local) ||
        //get_single_int_root(rdb_root, "exclusive", &p_conf->exclusive) || @TODO - this may be supported in RDB
        get_single_int_root(rdb_root, "keep_alive", &p_conf->tcp_options.keep_alive) ||
        get_single_int_root(rdb_root, "keepcnt", &p_conf->tcp_options.keepcnt) ||
        get_single_int_root(rdb_root, "keepidle", &p_conf->tcp_options.keepidle) ||
        get_single_int_root(rdb_root, "keepintvl", &p_conf->tcp_options.keepintvl) ||
        //get_single_int_root(rdb_root, "no_delay", &p_conf->tcp_options.no_delay) ||
        get_single_int_root(rdb_root, "max_children", &p_conf->max_clients)) {
        return -1;
    }

    if (p_conf->port_local <= 0) {
        return -1;
    }

    // default to non-exclusive - e.g. new clients can connect
    p_conf->exclusive = FALSE;

    dsm_dm_syslog(LOG_INFO,"Read TCP Server end point ok");
    return 0;
}

//
// Read tcp client configuration
//
static int read_tcp_client_config(const char *rdb_root, t_conf_tcp_client *p_conf)
{
    p_conf->end_point_mode = EP_MODE_RAW;

    int ip_addr_size = IP_ADDR_LENGTH;
    //int ident_size = IDENT_LENGTH;
    if (get_single_str_root(rdb_root, "ip_address", p_conf->ip_or_host_remote, &ip_addr_size) ||
        get_single_int_root(rdb_root, "timeout", &p_conf->retry_timeout) ||
        //get_single_str_root(rdb_root, "ident", p_conf->ident, &ident_size) || we could easily add Ident
        get_single_int_root(rdb_root, "port_number", &p_conf->port_remote) ||
        //get_single_int_root(rdb_root, "no_delay", &p_conf->tcp_options.no_delay) || @TODO
        get_single_int_root(rdb_root, "keep_alive", &p_conf->tcp_options.keep_alive) ||
        get_single_int_root(rdb_root, "keepcnt", &p_conf->tcp_options.keepcnt) ||
        get_single_int_root(rdb_root, "keepidle", &p_conf->tcp_options.keepidle) ||
        get_single_int_root(rdb_root, "keepintvl", &p_conf->tcp_options.keepintvl)) {
        return -1;
    }

    if (p_conf->port_remote <= 0) {
        return -1;
    }

    dsm_dm_syslog(LOG_INFO,"Read TCP Client end point ok");
    return 0;
}

//
// Read tcp client Connect on Demand configuration
//
static int read_tcp_client_cod_config(const char *rdb_root, t_conf_tcp_client_cod *p_conf)
{
    p_conf->end_point_mode = EP_MODE_RAW;

    int ip_addr_size1 = IP_ADDR_LENGTH, ip_addr_size2 = IP_ADDR_LENGTH;
    int ident_start_size = IDENT_LENGTH, ident_end_size = IDENT_LENGTH;

    // All rdb vars should always be there, even if they contain an empty string. In this case
    // get_single_str_root and get_single_int_root return 0 (for success). If any are missing
    // the function will return -1 and DSM will be disabled.
    if (get_single_str_root(rdb_root, "primary_server_ip", p_conf->primary_server_ip_or_host, &ip_addr_size1) ||
        get_single_str_root(rdb_root, "backup_server_ip", p_conf->backup_server_ip_or_host, &ip_addr_size2) ||
        get_single_int_root(rdb_root, "timeout", &p_conf->inactivity_timeout) ||
        get_single_int_root(rdb_root, "buf_size", &p_conf->buf_size) ||
        get_single_str_root(rdb_root, "ident_start", p_conf->ident_start, &ident_start_size) ||
        get_single_str_root(rdb_root, "ident_end", p_conf->ident_end, &ident_end_size) ||
        get_single_int_root(rdb_root, "primary_server_port", &p_conf->primary_server_port) ||
        get_single_int_root(rdb_root, "backup_server_port", &p_conf->backup_server_port) ||
        //get_single_int_root(rdb_root, "no_delay", &p_conf->tcp_options.no_delay) || @TODO
        get_single_int_root(rdb_root, "keep_alive", &p_conf->tcp_options.keep_alive) ||
        get_single_int_root(rdb_root, "keepcnt", &p_conf->tcp_options.keepcnt) ||
        get_single_int_root(rdb_root, "keepidle", &p_conf->tcp_options.keepidle) ||
        get_single_int_root(rdb_root, "keepintvl", &p_conf->tcp_options.keepintvl)) {
        return -1;
    }

    // some extra sanity checking
    if (p_conf->buf_size <= 0) {
        dsm_dm_syslog(LOG_ERR,"Wrong buf size in COD client");
        return -1;
    }

    if ((p_conf->primary_server_port <= 0) || (p_conf->primary_server_port > MAX_PORT_NUMBER)) {
        dsm_dm_syslog(LOG_ERR,"Invalid primary server port number (%d)", p_conf->primary_server_port);
        return -1;
    }

    // must have a primary server address/name
    if (!p_conf->primary_server_ip_or_host[0]) {
        dsm_dm_syslog(LOG_ERR,"IP address or hostname of primary server is not specified");
        return -1;
    }

    // check backup server configuration
    if (p_conf->backup_server_ip_or_host[0]) {
        if ((p_conf->backup_server_port <= 0) || (p_conf->backup_server_port > MAX_PORT_NUMBER)) {
            // backup server specified, yet port number is invalid
            dsm_dm_syslog(LOG_ERR,"Invalid backup server port number (%d)", p_conf->backup_server_port);
            return -1;
        }
        // a variable for convenience
        p_conf->has_backup_server = TRUE;
    }

    // in COD, we are doing our own Nagle algorithm. Disable it for now
    // If needed, we can implement a configurable switch for this in the UI.
    p_conf->tcp_options.no_delay = TRUE;

    dsm_dm_syslog(LOG_INFO,"Read TCP Client COD end point ok");
    return 0;
}

#ifdef UDP_SUPPORT_ENABLED
static int read_udp_server_config(const char *rdb_root, t_conf_udp_server *p_conf)
{
    p_conf->end_point_mode = EP_MODE_RAW;
    dsm_dm_syslog(LOG_INFO,"Read UDP Server not implemented");
    return -1;
}

static int read_udp_client_config(const char *rdb_root, t_conf_udp_client *p_conf)
{
    p_conf->end_point_mode = EP_MODE_RAW;
    dsm_dm_syslog(LOG_INFO,"Read UDP Client not implemented");
    return -1;
}
#endif

//
// Print usage
//
static void usage(void)
{
    printf("Usage: dsm_data_mover epa_root epa_type epa_mode epb_root epb_type epb_mode\n"
            "For example: dsm_data_mover service.dsm.ep.conf.my_ser 1 4 service.dsm.ep.conf.my_tcp_cli 3 1\n"
            "because serial end point is type 1, mode 4 and tcp client end point is type 3, mode 1\n"
          );
}

//
// Common shutdown actions
//
static void _shutdown_me(const char *msg, int exit_code)
{
    if (g_comm_host)
        fclose(g_comm_host);
    dsm_dm_syslog(LOG_ERR, msg);
    rdb_close(&g_rdb_session_p);
    exit(exit_code);
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
        dsm_dm_syslog(LOG_DEBUG,"Signal received %d, quitting\n", signum);

        // Signal the threads to stop e.g. exit their almost forever loop
        // @TODO new code, check if this doesn't crash anything in working Modbus tests
        g_client_thread.do_exit = TRUE;
        g_server_thread.do_exit = TRUE;

        if (old_settings_stored)
            tcsetattr(fileno(g_comm_host), TCSANOW, &old_tio);
        if (g_comm_host)
            fclose(g_comm_host);
        if (g_rdb_session_p)
            rdb_close(&g_rdb_session_p);
        exit(0);
        break;

    default:
        break;
    }
}

//
// Catch SIGPIPE, log and do nothing else
//
static void sig_handler_pipe(int signum)
{
    switch(signum) {
    case SIGPIPE:
        //g_client_thread.connected_status = FALSE;
        dsm_dm_syslog(LOG_INFO, "Sig pipe received");
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
        dsm_dm_syslog(LOG_ERR, "Failed to set serial port defaults");
        return -1;
    }

    // set baud rate
    if (set_baud_rate(g_comm_host, g_conf_serial.bit_rate) < 0) {
        dsm_dm_syslog(LOG_ERR, "Failed to set baud rate %ld", g_conf_serial.bit_rate);
        return -1;
    }

    // set character format (data bits, stop bits, parity)
    if (set_character_format(g_comm_host, g_conf_serial.data_bits, g_conf_serial.parity, g_conf_serial.stop_bits) < 0) {
        dsm_dm_syslog(LOG_ERR, "Failed to set character format (data %d parity %d stop bits %d)",
               g_conf_serial.data_bits, g_conf_serial.parity, g_conf_serial.stop_bits);
        return -1;
    }
    return 0;
}

//
// Converts command line argument to integer for EP type
//
static int get_ep_type(const char *ep_str)
{
    int ep_type = atoi(ep_str);
    dsm_dm_syslog(LOG_DEBUG, "Argv %s, EP type %d\n", ep_str, ep_type);
    return ep_type;
}

//
// Converts command line argument to integer for EP mode
//
static int get_ep_mode(const char *ep_str)
{
    int ep_mode = atoi(ep_str);
    dsm_dm_syslog(LOG_DEBUG, "Argv %s, EP mode %d\n", ep_str, ep_mode);
    return ep_mode;
}

//
// C code main entry point
//
int main(int argc, char *argv[])
{
    int fd, epa_type, epb_type, epb_mode, cli_retry_timeout = 0;

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
        dsm_dm_syslog(LOG_ERR, "failed to open RDB!" );
        exit(EXIT_FAILURE);
    }

    epa_type = get_ep_type(argv[2]);
    if ((epa_type != EP_SERIAL) && (epa_type != EP_RS232) &&
        (epa_type != EP_RS485) && (epa_type != EP_RS422)) {
        dsm_dm_syslog(LOG_ERR, "Incorrect type of end point A, type %d", epa_type);
        exit(EXIT_FAILURE);
    }

    // set up signal handlers
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGPIPE, sig_handler_pipe);

    // initialize the serial end point config object regardless of end point b type
    init_serial_config(&g_conf_serial);

    // read serial configuration from RDB
    if (read_serial_config(argv[1], epa_type, get_ep_mode(argv[3]), &g_conf_serial) < 0) {
        _shutdown_me("failed to read serial configuration", EXIT_FAILURE);
    }

#ifdef DEEP_DEBUG
    // if needed, print it
    print_serial_config(&g_conf_serial);
#endif
    // open comms port
    g_comm_host = open_serial_port(g_conf_serial.dev_name);
    if (!g_comm_host) {
        _shutdown_me("Failed to open serial port", EXIT_FAILURE);
    }

    // save current serial port settings
    fd = fileno(g_comm_host);
    tcgetattr(fd, &old_tio);
    old_settings_stored = TRUE;

    // initialize it
    if (initialize_serial_port() < 0) {
        // restore old settings
        tcsetattr(fd, TCSANOW, &old_tio);
        // shutdown
        _shutdown_me("Failed to initialize serial port", EXIT_FAILURE);
    }

    // work out the type of end point B connected to our serial end point A
    epb_type = get_ep_type(argv[5]);
    epb_mode = get_ep_mode(argv[6]);

    if (epb_mode != EP_MODE_RAW) {
        // restore old settings
        tcsetattr(fd, TCSANOW, &old_tio);
        // shutdown
        _shutdown_me("End point B mode is not raw", EXIT_FAILURE);
    }

    reset_modbus_error_counters();

    //
    // Get the logging level, default to LOG_ERR if a special "debug enable" RDB variable is not found.
    // This is because we do not want to fill the log with messages that could be the result
    // of connecting the wrong device, or using a wrong configuration, to the serial line.
    // Normally, there will be no RDB variable for this but if needed, the user can
    // enable further log messages by creating the service.dsm.modbus.log_level variable
    // with a numeric value that matches LOG_xx definitions from syslog.h. Setting it to 7 (LOG_DEBUG)
    // will enable everything to be logged (if syslog capture filter level enables this level).
    //
    if (rdb_get_int(g_rdb_session_p, "service.dsm.modbus.logging_level", &dsm_data_mover_verbosity) != 0) {
        dsm_data_mover_verbosity = LOG_ERR;
    }
    dsm_dm_syslog(LOG_NOTICE, "Set dsm modbus logging level to %d", dsm_data_mover_verbosity);

    switch (epb_type)
    {
        case EP_TCP_SERVER:
            init_tcp_server_config(&g_conf_tcp_server);
            // read tcp server config from RDB
            if (read_tcp_server_config(argv[4], &g_conf_tcp_server) < 0) {
                _shutdown_me("Failed to read TCP server configuration", EXIT_FAILURE);
            }

#ifdef DEEP_DEBUG
            print_tcp_server_config(&g_conf_tcp_server);
#endif
            memset(&g_server_thread, 0, sizeof(g_server_thread));
            g_server_thread.sleep_time_ms = THREAD_SLEEP_TIME_MS;
            g_server_thread.select_timeout_ms = SELECT_TIMEOUT_MS;
            g_server_thread.listen_select_timeout_ms = SELECT_TIMEOUT_MS;
            g_server_thread.exclusive = g_conf_tcp_server.exclusive;
            g_server_thread.handle_data = TRUE;
            g_server_thread.is_udp = FALSE;
            g_server_thread.p_ep_config = &g_conf_tcp_server;
            if (pthread_mutex_init(&g_server_thread.mutex, NULL) != 0) {
                _shutdown_me("Failed to initialize server thread mutex", EXIT_FAILURE);
            }
            prepare_server_thread(&g_server_thread, &g_conf_tcp_server);
            break;

        case EP_TCP_CLIENT:
            init_tcp_client_config(&g_conf_tcp_client);
            if (read_tcp_client_config(argv[4], &g_conf_tcp_client) < 0) {
                _shutdown_me("Failed to read TCP client configuration", EXIT_FAILURE);
            }
#ifdef DEEP_DEBUG
            print_tcp_client_config(&g_conf_tcp_client);
#endif
            memset(&g_client_thread, 0, sizeof(g_client_thread));
            g_client_thread.sleep_time_ms = THREAD_SLEEP_TIME_MS;
            g_client_thread.select_timeout_ms = SELECT_TIMEOUT_MS;
            g_client_thread.handle_data = TRUE;
            g_client_thread.is_udp = FALSE;
            g_client_thread.p_ep_config = &g_conf_tcp_client;

            // shorthand
            cli_retry_timeout = ((t_conf_tcp_client *)g_client_thread.p_ep_config)->retry_timeout;
            if (pthread_mutex_init(&g_client_thread.mutex, NULL) != 0) {
                _shutdown_me("Failed to initialize client thread mutex", EXIT_FAILURE);
            }
            prepare_client_thread(&g_client_thread, FALSE);
            g_client_thread.start_connecting = TRUE;
            break;

        case EP_TCP_CLIENT_COD:
            init_tcp_client_cod_config(&g_conf_tcp_client_cod);
            if (read_tcp_client_cod_config(argv[4], &g_conf_tcp_client_cod) < 0) {
                _shutdown_me("Failed to read TCP COD client configuration", EXIT_FAILURE);
            }
#ifdef DEEP_DEBUG
            print_tcp_client_cod_config(&g_conf_tcp_client_cod);
#endif
            memset(&g_client_thread, 0, sizeof(g_client_thread));
            g_client_thread.sleep_time_ms = THREAD_SLEEP_TIME_MS;
            g_client_thread.select_timeout_ms = SELECT_TIMEOUT_MS;
            g_client_thread.handle_data = TRUE;
            g_client_thread.is_udp = FALSE;
            g_client_thread.p_ep_config = &g_conf_tcp_client_cod;
            cli_retry_timeout = 0; // there are no retries for COD connection

            if (pthread_mutex_init(&g_client_thread.mutex, NULL) != 0) {
                _shutdown_me("Failed to initialize client thread mutex", EXIT_FAILURE);
            }
            prepare_client_thread(&g_client_thread, TRUE);
            break;

        case EP_UDP_SERVER:
        case EP_UDP_CLIENT:
            // UDP not implemented yet
        default:
            dsm_dm_syslog(LOG_ERR, "Unsupported end point type %d", epb_type);
            exit(EXIT_FAILURE);
            break;
    }

    //
    // enter the main thread control loop. The main thread does nothing except when
    // client mode is active, and no connection exists, it sets the "start_connecting" flag
    // Note that only one other thread is running at the same time (client or server).
    //
    while (1) {
        // if retry timeout is 0, there is no retry
        if (g_client_thread.handle_data && !g_client_thread.connected_status && cli_retry_timeout) {
            g_client_thread.start_connecting = TRUE;
            usleep(cli_retry_timeout*1000000L); // sec to microseconds
        } else {
            usleep(POLL_TIME_US); // 100 milliseconds
        }
    }

    // tidy up
    tcsetattr(fd, TCSANOW, &old_tio);
    fclose(g_comm_host);
    rdb_close(&g_rdb_session_p);

    return 0;
}
