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
// Initial usage is to replace socat in DSM Modbus modes.
//
// Intended future usage is 1:N data streams (one end point to many).
//
// Therefore, UDP support is included (although not used in Modbus)
//
// Contains functions specific to UDP connections
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h> // need both
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

// in UDP "server" mode, unless something has been sent to us, we have no idea where
// to send data to. When something is sent to us first time, the following variable is set to TRUE
static BOOL udp_client_known = FALSE;

// sockets and supporting infrastructure
struct sockaddr_in udp_remote_addr;
struct sockaddr_in udp_source_addr;
int udp_cli_sock = -1;
int udp_srv_sock = -1;

// use the same buff size for reads from socket and serial port.
// If needed, the buffers can be made a different size
#define UDP_MAX_BUF_SIZE (4096)
static u_char buff[UDP_MAX_BUF_SIZE];

//
// Connect to remote UDP "server"
// Return TRUE if successful, FALSE otherwise
// Note that with UDP no data is sent, and no "connection" is established as such
// We do check however that the remote address/URL is correct
//
// If programmed, identifier string is sent to remote UDP "server"
//
BOOL do_connect_udp_cli(t_cli_thread_info *p_thread_info)
{
    t_conf_udp_client *p_conf = (t_conf_udp_client *)p_thread_info->p_ep_config;

    if (udp_cli_sock < 0) {
        udp_cli_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp_cli_sock < 0) {
            dsm_dm_syslog(LOG_ERR, "Error creating socket: %s", strerror(errno));
            return FALSE;
        }

        memset((char *)&udp_remote_addr, 0, sizeof(udp_remote_addr));
        udp_remote_addr.sin_family = AF_INET;
        udp_remote_addr.sin_addr.s_addr = inet_addr(p_conf->ip_or_host_remote);
        udp_remote_addr.sin_port = htons(p_conf->port_remote);

        if (udp_remote_addr.sin_addr.s_addr == INADDR_NONE) {

            // try to resolve by hostname
            char ip_addr_temp[IP_ADDR_LENGTH];

            if (hostname_to_ip(p_conf->ip_or_host_remote, ip_addr_temp, SOCK_DGRAM) != 0) {
                dsm_dm_syslog(LOG_ERR, "Failed to resolve remote server name: %s", p_conf->ip_or_host_remote);
                return FALSE;
            }

            dsm_dm_syslog(LOG_INFO, "Resolved name %s to IP addr %s", p_conf->ip_or_host_remote, ip_addr_temp);
            udp_remote_addr.sin_addr.s_addr = inet_addr(ip_addr_temp);
        }
    }

    //
    // This is (correctly) outside previous "if" block as in UDP we do not
    // re-create udp_cli_sock when we disconnect and reconnect.
    //
    // If we have identifier string configured, we
    // send it into the socket using sendto and return
    // Otherwise, we consider ourselves connected.
    if (p_conf->ident[0]) {
        return send_ident(udp_cli_sock, p_conf->ident, TRUE);
    }
    return TRUE;
}

//
// Move data from/to serial to/from socket - this is UDP specific function
// utilizing sendto and recvfrom. Note it is the same in client and server mode.
//
void move_data_udp(int ser_fd, int socket_fd, int select_timeout_ms,
    t_move_result *p_result, BOOL server_mode)
{
    int bytes_received;
    struct timeval tv;
    fd_set fdsetR;

    socklen_t addr_size;
    addr_size = sizeof(udp_source_addr);

    memset(p_result, 0, sizeof(t_move_result));

    // determine the highest fd
    int max_fd = ser_fd > socket_fd ? ser_fd : socket_fd;

    FD_ZERO(&fdsetR);
    FD_SET(ser_fd, &fdsetR);
    FD_SET(socket_fd, &fdsetR);

    //
    // Check if there is anything available to read on socket
    // or on serial port
    //
    tv.tv_sec = 0;
    tv.tv_usec = select_timeout_ms * 1000;

    if (select(max_fd + 1, &fdsetR, (fd_set*)0, (fd_set*)0, &tv) < 0) {
        p_result->select_err = TRUE;
        return;
    }

    // in simulated server mode of UDP, we cannot send anything to the socket until we get the
    // first byte from the socket ourselves and fill udp_source_addr object
    // So, simply do not read serial data until we hear from the socket
    if (server_mode && !udp_client_known) {
        // if we wanted to buffer serial data before the first byte arrives from the socket,
        // we would do this here.
        // For now, do not read data from serial port as there is nowhere to send it
        p_result->socket_not_known = TRUE;

    } else {
            // Part 1: serial port -> socket
        if (FD_ISSET(ser_fd, &fdsetR)) {

            bytes_received = read(ser_fd, buff, UDP_MAX_BUF_SIZE);

            if (bytes_received > 0) {
                if (send_all(socket_fd, buff, bytes_received, 0, SEND_TIMEOUT_SOCK_MS, FALSE,
                    server_mode ? (struct sockaddr*) &udp_source_addr : (struct sockaddr*) &udp_remote_addr,
                    sizeof(struct sockaddr_in)) < 0) {
                    p_result->socket_send_err = TRUE;
                }
            } else if (bytes_received < 0) {
                p_result->ser_read_err = TRUE;
            }
        } else {
            bytes_received = 0;
        }
    }

    // Part 2: socket -> serial port
    if (FD_ISSET(socket_fd, &fdsetR)) {
        bzero(&udp_source_addr, sizeof(udp_source_addr));
        bytes_received = recvfrom(socket_fd, buff, UDP_MAX_BUF_SIZE, 0, (struct sockaddr*) &udp_source_addr, &addr_size);

        if (bytes_received > 0) {
            udp_client_known = TRUE;
            if (send_all(ser_fd, buff, bytes_received, 0, SEND_TIMEOUT_SER_MS, TRUE, NULL, 0) < 0) {
                p_result->ser_write_err = TRUE;
            }
        } else if (bytes_received < 0) {
            p_result->socket_recv_err = TRUE;
        }
    }
}

//
// Server thread. It is always running (unless local port is set to 0 in the configuration)
// Creates if necessary a UDP socket, and then waits for any data to arrive from socket
// If no data has been received from socket, then there is no point sending serial data
// anywhere as we do not know the sender's details
//
void *udp_server_thread(void* arg)
{
    t_srv_thread_info *p_thread_info = (t_srv_thread_info *)arg;
    t_move_result res;

    p_thread_info->connected_status = FALSE;
    while (!p_thread_info->do_exit) {

        if (p_thread_info->start_disconnecting && p_thread_info->connected_status) {
            p_thread_info->connected_status = FALSE;
            p_thread_info->start_disconnecting = FALSE;
            // this is neater than keeping the existing socket
            // we want all data which was sent prior to disconnect to be flushed
            if (udp_srv_sock != -1) {
                close(udp_srv_sock);
                udp_srv_sock = -1;
            }
        }

        // detect anything written to our socket - which is how we assume the client has connected in UDP
        if (!p_thread_info->connected_status) {

            if (udp_srv_sock == -1) {
                // Create the UDP socket
                if ((udp_srv_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                    dsm_dm_syslog(LOG_ERR, "UDP socket creation failed %d", errno);
                    p_thread_info->do_exit = TRUE;
                }

                // Construct the server sockaddr_in structure
                struct sockaddr_in serverAddr;
                bzero(&serverAddr, sizeof(serverAddr));
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
                serverAddr.sin_port = htons(((t_conf_udp_server *)p_thread_info->p_ep_config)->port_local);

                // Bind the socket
                if (bind(udp_srv_sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
                    dsm_dm_syslog(LOG_ERR, "Failed to bind UDP socket%d", errno);
                    p_thread_info->do_exit = TRUE;
                }
            }

            struct timeval tv;
            fd_set readset;
            FD_ZERO(&readset);
            FD_SET(udp_srv_sock, &readset);
            tv.tv_sec = 0;
            tv.tv_usec = 100000;

            // check if someone has send us data - but do not read this data yet
            if (select(udp_srv_sock + 1, &readset, (fd_set*)0, (fd_set*)0, &tv) > 0) {
                p_thread_info->connected_status = TRUE;
                p_thread_info->connect_trigger = TRUE;
                udp_client_known = FALSE;
                dsm_dm_syslog(LOG_DEBUG, "Connect detected from UDP client");
            }
        }

        // data connection part - move data if necessary
        if (p_thread_info->connected_status && p_thread_info->handle_data) {

            move_data_udp(fileno(g_comm_host),
                udp_srv_sock,
                p_thread_info->select_timeout_ms,
                &res, TRUE);

            if (res.socket_recv_err || res.socket_send_err || res.socket_shutdown) {
                p_thread_info->connected_status = FALSE;
                p_thread_info->connection_dropped = TRUE;
            }
        }
        else {
            // in the absence of connection, just sleep!
            usleep(p_thread_info->sleep_time_ms * 1000);
        }

        status_logger("UDP Server", p_thread_info->connected_status, p_thread_info->handle_data, 1, FALSE);

        p_thread_info->is_running = TRUE;
    }

    dsm_dm_syslog(LOG_DEBUG, "UDP Server thread is exiting");

    if (udp_srv_sock != -1)
        close(udp_srv_sock);

    p_thread_info->is_running = FALSE;

    return 0;
}

//
// Does all necessary preparation for UDP "server", including
// starting the UDP server thread.
//
int prepare_server_thread_udp(t_srv_thread_info *p_srv_thread)
{
    int ret;

    pthread_t thread_info;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // set stack to 128 Kb as it is a reasonable size
    pthread_attr_setstacksize(&attr, 128*1024);

    ret = pthread_create(&thread_info, &attr, udp_server_thread, p_srv_thread);
    dsm_dm_syslog(LOG_DEBUG, "Started UDP SRV thread %d", ret);
    return 0;
}
