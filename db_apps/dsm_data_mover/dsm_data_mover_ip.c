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
// Initial usage is to replace socat in Modbus applications.
// Intended future usage is 1:N data streams (one end point to many).
//
// Therefore, UDP support is included (although not used in Modbus)
//
// Threading model and connection established heavily utilizes code from
// the Modem Emulator end point project (db_apps/modem_emul_ep)
//
// Contains COMMON functions that apply both in TCP and UDP modes
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
#include <netdb.h> //hostent
#include <pthread.h>
#include "rdb_ops.h"
#include "dsm_data_mover.h"

// reach out only for globals that are necessary
extern FILE *g_comm_host;

// externs needed for TCP and UDP connections
extern int tcp_cli_sock;
extern int udp_cli_sock;
extern struct sockaddr_in udp_remote_addr;

//
// Get uptime
//
static time_t get_uptime(void)
{
    struct tms t;
    clock_t c;
    time_t uptime;

    c = times(&t);
    uptime = c / sysconf(_SC_CLK_TCK);
    return uptime;
}

//
// Used in debug mode - to log the state of each thread - no more frequently than once every 3 seconds
//
#define LOGGER_INTERVAL 3
extern void prepare_modbus_error_string(char *buf, int buf_size, BOOL non_zero_counters_only);
void status_logger(const char *prefix, BOOL connected_status, BOOL moving_data, int index, BOOL add_err_counters)
{
    static time_t last_time[2];
    static BOOL first[2] = { TRUE, TRUE };

    time_t current_time = get_uptime();
    if (first[index] || (abs((int)difftime(last_time[index], current_time)) > LOGGER_INTERVAL))
    {
        if (add_err_counters) {
            char buff[512];
            // last argument - only add non-zero error counters
            prepare_modbus_error_string(buff, sizeof(buff), TRUE);
            dsm_dm_syslog(LOG_DEBUG, "%s : conn status %d, moving data %d, %s",
                prefix, connected_status, moving_data, buff);
        } else {
            dsm_dm_syslog(LOG_DEBUG, "%s : conn status %d, moving data %d",
                prefix, connected_status, moving_data);
        }
        last_time[index] = current_time;
        first[index] = FALSE;
    }
}

// This function is a wrapper for send, sendto and write and is called when data received from
// serial end is to be sent to the socket. The need for this function arises because
// send, sendto and write may return less bytes than asked for - so we would like to
// continue to try and send the rest of the buffer in some reasonable way without blocking
// things too much.
//
// 1) Normal usage is to allow up to certain overall time limit to complete sending all the bytes.
//      for example, 10 seconds will allow up to 100 paced at 100ms iterations each calling send once
// 2) If total_timeout_ms argument is 0, only one iteration will be allowed -
//      in other words, use this if you want the entire buffer to be sent in one send/write call,
//      and if this is not possible (send blocks), an error is returned immediately
//
// Returns: 0 - success
//          -1 - failure
//
// Non-trivial arguments explained here:
// send_flags applicable to send and sendto only and passed through "as is"
// total_timeout_ms - 0 is a valid value. This function is not designed for write/send attempt to block forever
// is_serial - when socket is serial, uses write, otherwise send (TCP) or sendto (UDP)
// udp_addr - when UDP socket is in use, this should contain UDP socket address, otherwise it should be NULL in non-UDP modes
// udp_addrlen - is set to sizeof(struct sockaddr_in) by the caller, otherwise 0 in non-UDP modes
int send_all(int sock, u_char *buffer, int len, int send_flags, int total_timeout_ms, BOOL is_serial, struct sockaddr *udp_addr, int udp_addrlen)
{
    int nsent; // number of bytes sent
    const int sleep_ms = 100; // use 100 ms as a reasonable number for sleep
    fd_set fdsetW;
    struct timeval tv;

    // calculate the max number of iterations of this loop
    int iter = total_timeout_ms/sleep_ms;

    while(len > 0) {
        if (is_serial) {
            nsent = write(sock, buffer, len);
        } else {
            if (udp_addr) {
                nsent = sendto(sock, buffer, len, send_flags, udp_addr, udp_addrlen);
            } else {
                nsent = send(sock, buffer, len, send_flags);
            }
        }

        if (nsent == -1) {
            dsm_dm_syslog(LOG_ERR, "Sock send returns -1");
            return -1;
        }
        buffer += nsent;
        len -= nsent;

        if (len > 0) {
            // not everything has been sent yet
            if (iter-- <= 0) {
                dsm_dm_syslog(LOG_ERR, "Timed out in send_all (%d seconds)", total_timeout_ms);
                return -1;
            }

            //
            // Here, we have nothing much to do but wait and try to send the rest of
            // the buffer. We do not want to do it immediately as the condition that prevented
            // sending it in the first place is probably still there.
            //
            // Select on write is better than sleep as we may return sooner
            // than sleep.
            //
            // All of the below code in the current block is almost equivalent to usleep(1000*sleep_ms);
            FD_ZERO(&fdsetW);
            FD_SET(sock, &fdsetW);

            tv.tv_sec = 0;
            tv.tv_usec = sleep_ms * 1000;
            if (select(sock + 1, (fd_set *)0, &fdsetW, (fd_set*)0, &tv) < 0) {
                return -1;
            }
        }
    }
    return 0; // ok, all data sent
}

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

    dsm_dm_syslog(LOG_DEBUG, "Sending ident %s identlen = %d", ident, identlen);
    return use_udp ?
        (sendto(socket_fd, ident, identlen, 0, (struct sockaddr*) &udp_remote_addr, sizeof(struct sockaddr_in)) == identlen) :
        (send(socket_fd, ident, identlen, 0) == identlen);
}

//
// Convert hostname string into dot decimal IP address string
// Return 0 if all is good, and -1 otherwise
//
int hostname_to_ip(const char *hostname, char *ip, int sock_type)
{
    struct addrinfo hints, *servinfo;
    struct sockaddr_in *h;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Either IP4 or IP6
    hints.ai_socktype = sock_type; // as per argument

    if ((getaddrinfo(hostname, NULL, &hints, &servinfo) != 0) ||
        (!servinfo)) {
        return -1;
    }

    // get the first address
    h = (struct sockaddr_in *) servinfo->ai_addr;
    strcpy(ip, inet_ntoa(h->sin_addr));
    dsm_dm_syslog(LOG_DEBUG, "IP address resolved: %s", ip);

    freeaddrinfo(servinfo); // all done with this structure
    return 0;
}


//
// Disconnect client
//
void do_disconnect_cli(BOOL is_udp)
{
    if (!is_udp) { // nothing to do for UDP
        if (tcp_cli_sock != -1)  {
            if (close(tcp_cli_sock) == -1) {
                dsm_dm_syslog(LOG_ERR, "error closing socket: %s", strerror(errno));
            }
            tcp_cli_sock = -1;
        }
    }
}

//
// A client thread, which is where we establish a connection to the server
// This code is identical for TCP and UDP sockets.
// So it is placed here, in the common file.
// The only run-time difference is which data mover function we call.
// This thread only runs in Modbus client agent modes.
//
void *client_thread(void *arg)
{
    t_cli_thread_info *p_thread_info = (t_cli_thread_info *)arg;
    t_move_result res;

    p_thread_info->connected_status = FALSE;

    if (!p_thread_info->select_timeout_ms) {
        dsm_dm_syslog(LOG_ERR, "Incorrect select timeouts");
        return NULL;
    }

    while (!p_thread_info->do_exit) {

        // connection establishment if not connected already
        // start_connecting flag is set by main thread
        if (p_thread_info->start_connecting && !p_thread_info->connected_status) {
            p_thread_info->start_connecting = FALSE;
            if (p_thread_info->is_udp) {
                p_thread_info->connected_status = do_connect_udp_cli(p_thread_info);
            } else {
                p_thread_info->connected_status = do_connect_tcp_cli(p_thread_info);
            }

            if (!p_thread_info->connected_status)
                p_thread_info->connect_failed = TRUE;
        }

        // check if we need to disconnect. The do_disconnect flag is set by the main thread
        if (p_thread_info->start_disconnecting && p_thread_info->connected_status) {
            p_thread_info->connected_status = FALSE;
            p_thread_info->start_disconnecting = FALSE;
            do_disconnect_cli(p_thread_info->is_udp);
        }

        // if we are connected, and told to move data to/from serial port, then do just that!
        if (p_thread_info->connected_status && p_thread_info->handle_data) {
            if (p_thread_info->is_udp) {
                move_data_udp(fileno(g_comm_host),
                    udp_cli_sock,
                    p_thread_info->select_timeout_ms,
                    &res, FALSE);
            } else {
                move_data_tcp(fileno(g_comm_host),
                    tcp_cli_sock,
                    p_thread_info->select_timeout_ms,
                    &res);
            }

            // set connection status to FALSE if any of erroneous conditions were detected
            // on the socket
            if (res.socket_recv_err || res.socket_send_err || res.socket_shutdown) {
                p_thread_info->connected_status = FALSE;
                p_thread_info->connection_dropped = TRUE;
            }
        } else {
            // in the absence of connection, just sleep!
            usleep(p_thread_info->sleep_time_ms * 1000);
        }

        p_thread_info->is_running = TRUE;
        status_logger("Client", p_thread_info->connected_status, p_thread_info->handle_data, 0, TRUE);
    }

    dsm_dm_syslog(LOG_DEBUG, "Client thread is exiting");
    // we are outside the main loop, clean up and return
    do_disconnect_cli(p_thread_info->is_udp);
    p_thread_info->connected_status = FALSE;
    p_thread_info->is_running = FALSE;
    return NULL;
}

//
// This is TCP only functionality, we keep it in a common dsm_data_mover_ip.c file because
// it is similar to "normal" TCP client thread code.
//
// A client thread where connect on demand end point is used.
// This arrangement requires to listen for activity on the serial port, and only
// connect to the server and send data when the serial data becomes available.
//
#define COD_MAX_BUF_SIZE 3000
#define SOCK_MAX_BUF_SIZE 1500
// if EARLY CONNECT is defined, we attempt to establish a socket as soon as we get any data
// from the serial port, even if we are not ready to send yet. If it is NOT defined, then
// we will establish a connection only when we are ready to send the specified in configuration
// number of bytes. Both are valid, both are tested, and early connect is at the moment enabled
// - it can take some time to establish a TCP connection so we get it out of the way early.
// May consider, if necessary, have this configurable by the user via Web/RDB
#define COD_EARLY_CONNECT
void *client_thread_connect_on_demand(void *arg)
{
    static int total_bytes_received = 0;
    int no_data_count = 0;

    static u_char cod_buff[COD_MAX_BUF_SIZE];
    static u_char tcp_buff[SOCK_MAX_BUF_SIZE];

    // COD and "normal" client have the same thread info structure
    t_cli_thread_info *p_thread_info = (t_cli_thread_info *)arg;

    // ... however, a p_ep_config structure type is different from the "normal" thread.
    t_conf_tcp_client_cod *p_conf = (t_conf_tcp_client_cod *)p_thread_info->p_ep_config;

    p_thread_info->connected_status = FALSE;

    if (!p_thread_info->select_timeout_ms) {
        dsm_dm_syslog(LOG_ERR, "Incorrect select timeouts");
        return NULL;
    }

    if (p_thread_info->is_udp) {
        dsm_dm_syslog(LOG_ERR, "Connect-on-demand is a TCP-only option");
        return NULL;
    }

    if (p_conf->buf_size > COD_MAX_BUF_SIZE/2) {
        dsm_dm_syslog(LOG_ERR, "Connect-on-demand buffer size %d is too large (limit is %d)",
                      p_conf->buf_size, (int)COD_MAX_BUF_SIZE/2);
        return NULL;
    }

    while (!p_thread_info->do_exit) {

        int bytes_received;
        fd_set fdsetR;
        struct timeval tv;

        // get serial fd
        int ser_fd = fileno(g_comm_host);

        FD_ZERO(&fdsetR);
        FD_SET(ser_fd, &fdsetR);
        // socket may not be open yet as it is an "on-demand" connection
        if (tcp_cli_sock > 0)
            FD_SET(tcp_cli_sock, &fdsetR);

        //
        // Check if there is any data on the serial port
        //
        tv.tv_sec = 0;
        tv.tv_usec = SELECT_TIMEOUT_MS * 1000;

        // tcp_cli_sock can be -1!
        int max_fd = ser_fd > tcp_cli_sock ? ser_fd : tcp_cli_sock;
        select(max_fd + 1, &fdsetR, (fd_set*)0, (fd_set*)0, &tv);

        // Part 1: serial port -> socket
        if (FD_ISSET(ser_fd, &fdsetR)) {

            bytes_received = read(ser_fd, cod_buff+total_bytes_received, COD_MAX_BUF_SIZE/2);

            // too much log noise even for debug, but interesting to see this sometimes.
            //dsm_dm_syslog(LOG_DEBUG, "Data bytes received = %d, total = %d", bytes_received, total_bytes_received);

            if (bytes_received > 0) {

                total_bytes_received += bytes_received;

#ifdef COD_EARLY_CONNECT
                // connect as soon as there is a first lot of data
                if (!p_thread_info->connected_status) {
                    p_thread_info->connected_status = do_connect_tcp_cli_cod(p_thread_info, TRUE);
                    dsm_dm_syslog(LOG_DEBUG, "Connect attempted, result=%d", p_thread_info->connected_status);
                }
#endif

                if (total_bytes_received >= p_conf->buf_size) {

#ifndef COD_EARLY_CONNECT // optionally, do not even connect until there is enough data to send
                    // connect only when we are ready to send
                    if (!p_thread_info->connected_status) {
                        p_thread_info->connected_status = do_connect_tcp_cli_cod(p_thread_info, TRUE);
                        dsm_dm_syslog(LOG_DEBUG, "Connect attempted, result=%d", p_thread_info->connected_status);
                    }
#endif
                    if (p_thread_info->connected_status) {
                        if (send_all(tcp_cli_sock, cod_buff, total_bytes_received, 0, SEND_TIMEOUT_SOCK_MS, FALSE, NULL, 0) < 0) {
                            dsm_dm_syslog(LOG_ERR, "Failed to send_all %d bytes", total_bytes_received);
                            p_thread_info->connected_status = FALSE;
                            do_disconnect_cli(FALSE);
                        } else {
                            // this went well
                            dsm_dm_syslog(LOG_DEBUG, "Sent %d bytes successfully", total_bytes_received);
                        }
                    } else {
                        // This is NOT an error as such, just the server is offline. Throw out the serial data?
                        dsm_dm_syslog(LOG_INFO, "Have enough data to sent, but servers are offline. Discarding %d bytes of data",
                                      total_bytes_received);
                    }
                    // reset the total bytes count
                    total_bytes_received = 0;
                }
            } else { // bytes_received <= 0
                // @TODO - should we distinguish between 0 and <0 : can we get 0 if FD_ISSET is true?
                // we do this on the socket where 0 bytes means server shutdown, but on serial side 
                // this doesn't seem to be supported.
                dsm_dm_syslog(LOG_ERR, "Select indicated data on serial port, but read failed, bytes_received=%d", bytes_received);
                total_bytes_received = 0;
            }
            // reset "no data" counter, since we do have data
            no_data_count = 0;

            // a tiny sleep in case if we getting bombarded by data and select doesn't really yield at all.
            usleep(1000);
        } else {
            // check if the timeout since the last serial byte (greater than specified in config properties) has occurred.
            if (++no_data_count >= (1000/SELECT_TIMEOUT_MS)*p_conf->inactivity_timeout) {

                // we may have some leftover bytes that have not been sent yet
                if (p_thread_info->connected_status) {
                    if (total_bytes_received) {
                        int ret = send_all(tcp_cli_sock, cod_buff, total_bytes_received, 0, SEND_TIMEOUT_SOCK_MS, FALSE, NULL, 0);
                        dsm_dm_syslog(LOG_DEBUG, "Sent leftover data before closing the socket, count %d, ret %d", total_bytes_received, ret);
                    }

                    // see if we need to send the Tail identifier
                    if (p_conf->ident_end[0]) {
                        send_ident(tcp_cli_sock, p_conf->ident_end, FALSE);
                    }

                    // disconnect on timeout
                    dsm_dm_syslog(LOG_DEBUG, "Disconnected COD client after %d seconds of serial inactivity, count %d",
                            (int)p_conf->inactivity_timeout, no_data_count);
                    do_disconnect_cli(FALSE);
                    p_thread_info->connected_status = FALSE;
                }
                no_data_count = 0;
                total_bytes_received = 0;
            }
        }

        // Part 2: socket -> serial port
        // If we haven't connected, do not read anything
        if ((tcp_cli_sock > 0) && p_thread_info->connected_status && FD_ISSET(tcp_cli_sock, &fdsetR)) {

            int tcp_bytes_received = recv(tcp_cli_sock, tcp_buff, SOCK_MAX_BUF_SIZE, 0);

            dsm_dm_syslog(LOG_DEBUG, "Data from socket received count %d", tcp_bytes_received);
            if (tcp_bytes_received > 0) {
                if (send_all(ser_fd, tcp_buff, tcp_bytes_received, 0, SEND_TIMEOUT_SER_MS, TRUE, NULL, 0) < 0) {
                    dsm_dm_syslog(LOG_ERR, "Failed to send_all %d bytes", tcp_bytes_received);
                } else {
                    dsm_dm_syslog(LOG_DEBUG, "Sent % bytes from socket to serial", tcp_bytes_received);
                }
                // A judgement call: reset "no data" counter for timeout, since we have data from server, even
                // in the absence of serial data.
                no_data_count = 0;
            } else if (tcp_bytes_received < 0) {
                dsm_dm_syslog(LOG_ERR, "Select indicated data on socket, but recv failed");
            } else { // recv got 0 bytes, meaning server shutdown
                do_disconnect_cli(FALSE);
                p_thread_info->connected_status = FALSE;
                dsm_dm_syslog(LOG_ERR, "Recv got 0 bytes, server shutdown. Disconnecting.");
            }
        }

        p_thread_info->is_running = TRUE;
        status_logger("Client on demand", p_thread_info->connected_status, no_data_count, 0, FALSE);
    }

    dsm_dm_syslog(LOG_DEBUG, "Client COD thread is exiting");
    // we are outside the main loop, clean up and return
    do_disconnect_cli(FALSE);
    p_thread_info->connected_status = FALSE;
    p_thread_info->is_running = FALSE;
    return NULL;
}

//
// Starts Client thread. Note that this thread is running only in Modbus client modes
//
// Return 0 on success, or error number otherwise
//
int prepare_client_thread(t_cli_thread_info *p_cli_thread, BOOL connect_on_demand)
{
    pthread_t thread_info;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // set stack to 128 Kb as it is a reasonable size
    pthread_attr_setstacksize(&attr, 128*1024);

    int ret = pthread_create(&thread_info, &attr, connect_on_demand ?
            client_thread_connect_on_demand : client_thread, p_cli_thread);

    dsm_dm_syslog(LOG_DEBUG, "Preparing connection, started CLI thread %d", ret);

    return 0;
}

//
// Starts Server thread - this is done in Modbus server modes
//
// Return 0 on success, or error number otherwise
//
int prepare_server_thread(t_srv_thread_info *p_srv_thread, t_conf_tcp_server *p_conf)
{
    int ret;
    // call a specific to protocol function depending on the configuration
    if (p_srv_thread->is_udp) {
        ret = prepare_server_thread_udp(p_srv_thread);
        dsm_dm_syslog(LOG_DEBUG, "Preparing connection, starting UDP server thread %d", ret);
    } else {
        ret = prepare_server_thread_tcp(p_srv_thread, p_conf);
        dsm_dm_syslog(LOG_DEBUG, "Preparing connection, starting TCP server thread %d", ret);
    }
    return ret;
}

