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
// Contains COMMON functions specific to IP modem end point
// Common means that they apply both in TCP and UDP modes
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
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h> //hostent
#include <pthread.h>
#include "rdb_ops.h"
#include "modem_emul_ep.h"
// include file ONLY used by end point #13 - IP Modem
#include "modem_ip.h"

// reach out only for globals that are necessary
extern FILE *g_comm_host;
extern t_conf_v250 g_conf_v250;
extern t_conf_ip_modem g_conf_ip_modem; // configuration of ip modem end point

// externs needed for TCP and UDP connections
extern int tcp_cli_sock;
extern int udp_cli_sock;
extern struct sockaddr_in udp_remote_addr;

//
// If there is an ID string in the properties, send it when we act as a client,
// first thing into the connection
// Returns the number of bytes sent
//
BOOL send_ident(int socket_fd, char *ident, BOOL use_udp)
{
    int identlen = strlen(ident);

    // some sanity checking
    if ((socket_fd < 0) || (identlen > IDENT_LENGTH) || (identlen <= 0)) {
        return FALSE;
    }

    me_syslog(LOG_INFO, "Sending ident %s identlen = %d", ident, identlen);
    return use_udp ?
        (sendto(socket_fd, ident, identlen, 0, (struct sockaddr*) &udp_remote_addr, sizeof(struct sockaddr_in)) == identlen) :
        (send(socket_fd, ident, identlen, 0) == identlen);
}

//
// Convert hostname string into dot decimal IP address string
// Return 0 if all is good, and -1 otherwise
int hostname_to_ip(const char *hostname, char *ip, int sock_type)
{
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *h;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Either IP4 or IP6
    hints.ai_socktype = sock_type; // as per argument

    if ((rv = getaddrinfo(hostname, NULL, &hints, &servinfo)) != 0) {
        return -1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        h = (struct sockaddr_in *) p->ai_addr;
        strcpy(ip, inet_ntoa( h->sin_addr));
        //me_syslog(LOG_DEBUG, "IP: %s", ip);
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    return 0;
}

//
// Enum supports escape sequence detection routine
//
typedef enum {
    WAITING_FOR_FIRST_SILENCE,
    DETECTING_TRIPLE_PLUS,
    WAITING_FOR_SECOND_SILENCE,
    ESCAPE_DETECTED,
} e_escape_detector_states;

//
// This function resets the state machine for searching for escape sequence
//
void reset_escape_search(t_escape_search *p)
{
    p->silence_count_ms = 0;
    p->escape_char_count = 0;
    p->escape_search_state = WAITING_FOR_FIRST_SILENCE;
    p->elapsed_interval_ms = 0;
    p->old_time.tv_nsec = -1;
    p->old_time.tv_sec = -1;
}

//
// This function is called every time when data is received from the serial port
// In fact server and client threads call this function independently from each other
// The algorithm does the following:
// 1) Calculates the elapsed time from last invokation of this function, PER THREAD
// 2) If is_timeout flag is set, then adds to the silence_count_ms
// 3) Built as a state machine that will detect the first silence, the three +++ and then
//      the second silence interval
//
// Can be called at any frequency as it does its own clock calcs
// if bytes_received is less then 0, then it indicates a read from serial device failed
// and search state machine is reset
//
// As discussed, it is thread save as each thread calls it with its own p_search argument
//
// Return TRUE if escape sequence was found and FALSE otherwise
BOOL escape_detector(t_escape_search *p_search, char *buff, int bytes_received, BOOL is_timeout)
{
    int i;

    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);

    // do not calculate the timeout for the first time
    if ((p_search->old_time.tv_nsec != -1) && (p_search->old_time.tv_sec != -1)) {
        p_search->elapsed_interval_ms = abs(cur_time.tv_nsec - p_search->old_time.tv_nsec) / 1000000;
    }
    // copy cur time into old
    p_search->old_time.tv_nsec = cur_time.tv_nsec;
    p_search->old_time.tv_sec = cur_time.tv_sec;

    // reset search state machine
    if (bytes_received < 0) {
        reset_escape_search(p_search);
    }

    switch (p_search->escape_search_state) {
        case WAITING_FOR_FIRST_SILENCE:
        case WAITING_FOR_SECOND_SILENCE:
            if (is_timeout) {

                p_search->silence_count_ms += p_search->elapsed_interval_ms;

                // need to properly test exact timeouts and possibly will need to adjust this
                if (p_search->silence_count_ms > 1000) {
                    // found 1 second of silence - go to next state
                    p_search->escape_search_state++;
                    me_syslog(LOG_INFO, "Going to state %d", p_search->escape_search_state);
                    if (p_search->escape_search_state == ESCAPE_DETECTED) {
                        reset_escape_search(p_search);
                        return TRUE;
                    }
                    p_search->silence_count_ms = 0;
                    p_search->escape_char_count = 0;
                }
            } else {
                // it is not a timeout, research search
                reset_escape_search(p_search);
            }
            break;
        case DETECTING_TRIPLE_PLUS:
            if (bytes_received > 3) {
                // cannot be more than 3 bytes!
                reset_escape_search(p_search);
            } else {
                for (i = 0 ; i < bytes_received ; i++) {
                    if (buff[i] != '+') {
                        reset_escape_search(p_search);
                    } else {
                        p_search->escape_char_count++;
                    }
                }
                if (p_search->escape_char_count == 3) {
                    me_syslog(LOG_INFO, "Going to state %d", p_search->escape_search_state);
                    p_search->escape_search_state = WAITING_FOR_SECOND_SILENCE;
                } else if (p_search->escape_char_count > 3) {
                    reset_escape_search(p_search);
                }
            }
            break;
    }
    return FALSE;
}

//
// Disconnect client
//
void do_disconnect_cli(void)
{
    if (g_conf_ip_modem.is_udp) {
        // nothing to do
    }
    else {
        if (tcp_cli_sock != -1)  {
            if (close(tcp_cli_sock) == -1) {
                me_syslog(LOG_ERR, "error closing socket: %s", strerror(errno));
            }
        }
        tcp_cli_sock = -1;
    }
}

//
// A client thread, which is where we establish a connection to the server
// This code is identical for TCP and UDP sockets.
// So it is placed here, in the common file.
// The only run-time difference is which data mover function we call.
//
void *ip_modem_client_thread(void *arg)
{
    t_cli_thread_info *p_thread_info = (t_cli_thread_info *)arg;
    t_move_result res;
    t_escape_search escape_search;

    p_thread_info->connected_status = FALSE;

    if (!p_thread_info->select_timeout_ms) {
        me_syslog(LOG_ERR, "Incorrect select timeouts");
        return NULL;
    }

    while (!p_thread_info->do_exit) {

        // connection establishment if not connected already
        // start_connecting flag is set by main thread
        if (p_thread_info->start_connecting && !p_thread_info->connected_status) {
            p_thread_info->start_connecting = FALSE;
            if (g_conf_ip_modem.is_udp) {
                p_thread_info->connected_status = do_connect_udp_cli((t_conf_ip_modem *)&g_conf_ip_modem);
            } else {
                p_thread_info->connected_status = do_connect_tcp_cli((t_conf_ip_modem *)&g_conf_ip_modem);
            }

            if (!p_thread_info->connected_status)
                p_thread_info->connect_failed = TRUE;

             reset_escape_search(&escape_search);
        }

        // check if we need to disconnect. The do_disconnect flag is set by the main thread
        if (p_thread_info->start_disconnecting && p_thread_info->connected_status) {
            p_thread_info->connected_status = FALSE;
            p_thread_info->start_disconnecting = FALSE;
            do_disconnect_cli();
        }

        // if we are connected, and told to move data to/from serial port, then do just that!
        if (p_thread_info->connected_status && p_thread_info->handle_data && !p_thread_info->escape_seq_received &&
                (get_modem_state() == ON_LINE)) {

            if (g_conf_ip_modem.is_udp) {
                move_data_udp(fileno(g_comm_host),
                    udp_cli_sock,
                    p_thread_info->select_timeout_ms,
                    TRUE, &escape_search, &res, FALSE);
            } else {
                move_data_tcp(fileno(g_comm_host),
                    tcp_cli_sock,
                    p_thread_info->select_timeout_ms,
                    TRUE, &escape_search, &res);
            }

            // set connection status to FALSE if any of erroneous conditions were detected
            // on the socket
            if (res.socket_recv_err || res.socket_send_err || res.socket_shutdown) {
                p_thread_info->connected_status = FALSE;
                p_thread_info->connection_dropped = TRUE;
            }

            // check the result of escape sequence detector
            if (res.got_escape) {
                // set a flag, but let main thread worry about it
                pthread_mutex_lock(&p_thread_info->mutex);
                p_thread_info->escape_seq_received = TRUE;
                pthread_mutex_unlock(&p_thread_info->mutex);
            }
        } else {
            // in the absence of connection, just sleep!
            usleep(p_thread_info->sleep_time_ms * 1000);
        }

        p_thread_info->is_running = TRUE;
        status_logger("Client", p_thread_info->connected_status, p_thread_info->handle_data, p_thread_info->escape_seq_received, get_modem_state(), 0);
    }

    // we are outside the main loop, clean up and return
    do_disconnect_cli();
    p_thread_info->connected_status = FALSE;
    p_thread_info->is_running = FALSE;
    return NULL;
}

//
// Starts Client thread. Note that this thread is running "in parallel" with the
// server thread as we need both.
// Since we are trying to emulate a real modem, we do not know if the
// incoming or outgoing call will occur first
// Return 0 on success, or error number otherwise
int prepare_ip_modem_cli(t_cli_thread_info *p_cli_thread)
{
    pthread_t thread_info;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 128*1024);

    int ret = pthread_create(&thread_info, &attr, ip_modem_client_thread, p_cli_thread);

    me_syslog(LOG_DEBUG, "Preparing ip connection, started CLI thread %d", ret);

    return ret;
}

//
// Starts Server thread
// Return 0 on success, or error number otherwise
//
int prepare_ip_modem_srv(t_srv_thread_info *p_srv_thread, t_conf_ip_modem *p_conf)
{
    // call a specific to protocol function depending on the configuration
    if (g_conf_ip_modem.is_udp) {
        return prepare_ip_modem_srv_udp(p_srv_thread);
    } else {
        return prepare_ip_modem_srv_tcp(p_srv_thread, p_conf);
    }
}

//
// Functions to control the modem control lines. These functions
// are specific to the type of end point connected to the modem emulator end point
// At the momemnt, these 3 functions return 0 regardless. An error could be returned (-1) and
// will be propagated back to caller function - control_modem_output_lines_xxx if something
// went badly wrong.
//
// Control the External Serial port's DSR line according to the state of the device
//
static int ip_modem_control_dsr(void)
{
    switch (g_conf_v250.opt_dsr) {
    case V250_DSR_ALWAYS:
        dsr_on();
        break;

    case V250_DSR_REGISTERED:
    case V250_DSR_PDP:
        if ((get_modem_state() != ON_LINE_COMMAND) && (get_modem_state() != ON_LINE))
            dsr_off();
        else
            dsr_on();
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
static int ip_modem_control_dcd(void)
{

    switch (g_conf_v250.opt_dcd) {

    case V250_DCD_ALWAYS:
        dcd_on();
        break;

    case V250_DCD_CONNECT:
    case V250_DCD_PDP:
        if ((get_modem_state() != ON_LINE_COMMAND) && (get_modem_state() != ON_LINE))
            dcd_off();
        else
            dcd_on();
        break;

    case V250_DCD_NEVER:
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
static int ip_modem_control_ri(void)
{
    switch (g_conf_v250.opt_ri) {
    case V250_RI_ALWAYS:
        ri_on();
        break;

    case V250_RI_RING:
        // get ring state from modem ip end point module
        if (get_ring_ind_state()) {
            ri_on();
        } else {
            ri_off();
        }
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
int control_modem_output_lines_ip(void)
{
    if (ip_modem_control_dcd() < 0) {
        me_syslog(LOG_ERR,"failed to control DCD");
        return -1;
    }
    if (ip_modem_control_ri() < 0) {
        me_syslog(LOG_ERR,"failed to control RI");
        return -1;
    }
    if (ip_modem_control_dsr() < 0) {
        me_syslog(LOG_ERR,"failed to control DSR");
        return -1;
    }

    return 0;
}
