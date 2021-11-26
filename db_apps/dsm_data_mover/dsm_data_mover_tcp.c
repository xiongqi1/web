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
// Subsequent usage will include one to many end point connections.
//
// Contains functions specific to TCP connection mode
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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h> //hostent
#include <pthread.h>
#include "rdb_ops.h"
#include "dsm_data_mover.h"

// modbus specific function
extern void move_data_modbus(int ser_fd, int socket_fd, BOOL rtu_mode, BOOL client_mode, t_move_result *p_result);

// reach out only for globals that are necessary
extern FILE *g_comm_host;

// sockets
int tcp_cli_sock = -1;
int tcp_srv_sock = -1;
int tcp_srv_listen_sock = -1;

// use the same buff size for reads from socket and serial port.
// this may need to be adjusted - chosen rather arbitrarily
#define TCP_MAX_BUF_SIZE (1024)
static u_char buff[TCP_MAX_BUF_SIZE];

//
// Set standard TCP options
//
int set_tcp_options(int tcp_sock, t_tcp_common_options *p_conf)
{
    int optval;
    socklen_t optlen = sizeof(optval);

    optval = 0;
    optlen = sizeof(optval);
    if (p_conf->keep_alive) {
        optval = 1;
        if (setsockopt(tcp_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
            dsm_dm_syslog(LOG_ERR, "Set keepalive option failed");
            return -1;
        }

        optval = p_conf->keepcnt;
        if (setsockopt(tcp_sock, IPPROTO_TCP, TCP_KEEPCNT, &optval, optlen) < 0) {
            dsm_dm_syslog(LOG_ERR, "Set TCP keepacount option failed");
            return -1;
        }

        optval = p_conf->keepidle;
        if (setsockopt(tcp_sock, IPPROTO_TCP, TCP_KEEPIDLE, &optval, optlen) < 0) {
            dsm_dm_syslog(LOG_ERR, "Set TCP keepidle option failed");
            return -1;
        }

        optval = p_conf->keepintvl;
        if (setsockopt(tcp_sock, IPPROTO_TCP, TCP_KEEPINTVL, &optval, optlen) < 0) {
            dsm_dm_syslog(LOG_ERR, "Set TCP keepintvl option failed");
            return -1;
        }
    }

    optval = p_conf->no_delay ? 1 : 0;
    if (setsockopt(tcp_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0 ) {
        dsm_dm_syslog(LOG_ERR, "setsockopt() tcp_nodelay failed");
        return -1;
    }

    dsm_dm_syslog(LOG_INFO, "TCP socket options: keepalive %d, keepcnt %d, keepidle %d, keepintvl %d, no delay %d",
              p_conf->keep_alive, p_conf->keepcnt, p_conf->keepidle, p_conf->keepintvl, p_conf->no_delay);

    return 0;
}

//
// Connect to remote server
// Return TRUE if successful, FALSE otherwise
//
BOOL do_connect_tcp_cli(t_cli_thread_info *p_thread_info)
{
    struct sockaddr_in tcp_remote_addr;

    // make a pointer to tcp client end point configuration
    t_conf_tcp_client *p_conf = (t_conf_tcp_client *)p_thread_info->p_ep_config;

    if (tcp_cli_sock != -1)
        do_disconnect_cli(p_thread_info->is_udp);

    tcp_cli_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_cli_sock == -1) {
        dsm_dm_syslog(LOG_ERR, "Error creating socket: %s", strerror(errno));
        return FALSE;
    }

    (void)set_tcp_options(tcp_cli_sock, &p_conf->tcp_options);

    memset((char *)&tcp_remote_addr, 0, sizeof(tcp_remote_addr));
    tcp_remote_addr.sin_family = AF_INET;
    tcp_remote_addr.sin_addr.s_addr = inet_addr(p_conf->ip_or_host_remote);
    tcp_remote_addr.sin_port = htons(p_conf->port_remote);

    if (tcp_remote_addr.sin_addr.s_addr == INADDR_NONE) {

        // try to resolve by hostname
        char ip_addr_temp[IP_ADDR_LENGTH];

        if (hostname_to_ip(p_conf->ip_or_host_remote, ip_addr_temp, SOCK_STREAM) != 0) {
            dsm_dm_syslog(LOG_ERR, "Failed to resolve remote server name: %s", p_conf->ip_or_host_remote);
            return FALSE;
        }

        dsm_dm_syslog(LOG_INFO, "Resolved name %s to IP addr %s", p_conf->ip_or_host_remote, ip_addr_temp);
        tcp_remote_addr.sin_addr.s_addr = inet_addr(ip_addr_temp);
    }

    // Connect to remote server
    if (connect(tcp_cli_sock, (struct sockaddr*) &tcp_remote_addr, sizeof(tcp_remote_addr)) == -1) {
        close(tcp_cli_sock);
        tcp_cli_sock = -1;
        dsm_dm_syslog(LOG_INFO, "Could not connect to remote TCP server: %s", strerror(errno));
        return FALSE;
    } else {
        if (p_conf->ident[0]) {
            return send_ident(tcp_cli_sock, p_conf->ident, FALSE);
        }
    }
    return TRUE;
}

//
// Connect to remote server
// In COD, there is the main, and backup servers. Try main, if this fails, try backup.
// There is still only 1:1 connection, e.g. serial<->main server or serial<->bckp server
// but not main and backup server simultaneously.
//
// Return TRUE if successful, FALSE otherwise
//
#define MAX_COD_SERVERS 2 // we support only 2 servers for connect on demand (main and backup).
BOOL do_connect_tcp_cli_cod(t_cli_thread_info *p_thread_info, BOOL send_id)
{
    struct sockaddr_in tcp_remote_addr;
    int i;
    char *ip_or_host; // will be used to point to either ip address1 or ip address2

    t_conf_tcp_client_cod *p_conf = (t_conf_tcp_client_cod *)p_thread_info->p_ep_config;

    if (tcp_cli_sock != -1)
        do_disconnect_cli(p_thread_info->is_udp);

    for (i = 0 ; i < MAX_COD_SERVERS ; i++) {

        if ((i != 0) && !p_conf->has_backup_server) {
            dsm_dm_syslog(LOG_INFO, "No backup server configured, quitting connect");
            return FALSE;
        }

        tcp_cli_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcp_cli_sock == -1) {
            dsm_dm_syslog(LOG_ERR, "Error creating socket: %s", strerror(errno));
            return FALSE;
        }

        (void)set_tcp_options(tcp_cli_sock, &p_conf->tcp_options);

        ip_or_host = (i == 0) ? p_conf->primary_server_ip_or_host : p_conf->backup_server_ip_or_host;
        memset((char *)&tcp_remote_addr, 0, sizeof(tcp_remote_addr));
        tcp_remote_addr.sin_family = AF_INET;
        tcp_remote_addr.sin_addr.s_addr = inet_addr(ip_or_host);

        // pick port 1 or 2
        tcp_remote_addr.sin_port = htons((i == 0) ? p_conf->primary_server_port : p_conf->backup_server_port);

        if (tcp_remote_addr.sin_addr.s_addr == INADDR_NONE) {

            // try to resolve by hostname
            char ip_addr_temp[IP_ADDR_LENGTH];

            if (hostname_to_ip(ip_or_host, ip_addr_temp, SOCK_STREAM) != 0) {
                dsm_dm_syslog(LOG_ERR, "Failed to resolve remote server name: %s", ip_or_host);

                // stop when last server has not been able to connect to
                if (i < (MAX_COD_SERVERS-1))
                    continue;
                return FALSE;
            }

            dsm_dm_syslog(LOG_INFO, "Resolved name %s to IP addr %s", ip_or_host, ip_addr_temp);
            tcp_remote_addr.sin_addr.s_addr = inet_addr(ip_addr_temp);
        }

        // Connect to remote server
        if (connect(tcp_cli_sock, (struct sockaddr*) &tcp_remote_addr, sizeof(tcp_remote_addr)) == -1) {
            dsm_dm_syslog(LOG_INFO, "Could not connect to remote TCP server no %d: %s", i+1, strerror(errno));
            close(tcp_cli_sock);
            tcp_cli_sock = -1;
        } else {
            if (send_id && p_conf->ident_start[0]) {
                return send_ident(tcp_cli_sock, p_conf->ident_start, FALSE);
            }
            return TRUE;
        }
    }
    return FALSE;
}
//
// This function would be called if DSM tool generated a command line that was based on dsm_data_mover and NOT socat
// At the moment, DSM will generate socat command line.
//
// It is used when both end points are in "raw" mode.
//
// It is fully capable (and partially tested) to move data from serial to TCP/UDP end points
// We retain this as the next stage of dsm_data_mover will support 1:N connections
//
// This is heavily borrowed from the Modem Emulator End point project - modem_emul_ep
// Arguments are:
// ser_fd - serial end point file descriptor
// socket_fd - socket end point fd
// select_timeout_ms - the value to use on combined (on 2 fd) select
// Errors are returned in p_sesult by reference.
//
/* static */void move_data_tcp_raw_mode(int ser_fd, int socket_fd, int select_timeout_ms, t_move_result *p_result)
{
    int bytes_received;
    fd_set fdsetR;
    struct timeval tv;

    // determine the highest fd out of two
    int max_fd = ser_fd > socket_fd ? ser_fd : socket_fd;

    // zeroise result
    memset(p_result, 0, sizeof(t_move_result));

    FD_ZERO(&fdsetR);
    FD_SET(ser_fd, &fdsetR);
    FD_SET(socket_fd, &fdsetR);

    //
    // Check if anything is there ready to be read from socket
    // or from the serial port
    //
    tv.tv_sec = 0;
    tv.tv_usec = select_timeout_ms * 1000;

    select(max_fd + 1, &fdsetR, (fd_set*)0, (fd_set*)0, &tv);

    // Part 1: serial port -> socket
    if (FD_ISSET(ser_fd, &fdsetR)) {

        bytes_received = read(ser_fd, buff, TCP_MAX_BUF_SIZE);

        if (bytes_received > 0) {
            if (send_all(socket_fd, buff, bytes_received, 0, SEND_TIMEOUT_SOCK_MS, FALSE, NULL, 0) < 0) {
                p_result->socket_send_err = TRUE;
            }
        } else if (bytes_received < 0) {
            p_result->ser_read_err = TRUE;
        }
    } else {
        bytes_received = 0;
    }

    // Part 2: socket -> serial port
    if (FD_ISSET(socket_fd, &fdsetR)) {

        bytes_received = recv(socket_fd, buff, TCP_MAX_BUF_SIZE, 0);

        if (bytes_received > 0) {
            if (send_all(ser_fd, buff, bytes_received, 0, SEND_TIMEOUT_SER_MS, TRUE, NULL, 0) < 0) {
                p_result->ser_write_err = TRUE;
            }
        } else if (bytes_received < 0) {
            p_result->socket_recv_err = TRUE;
        } else {
            // isset is true but could not read any bytes.
            p_result->socket_shutdown = TRUE;
        }
    }
}

//
// Arguments are:
// ser_fd - serial end point file descriptor
// socket_fd - socket end point fd
// select_timeout_ms - the value to use on combined (on 2 fd) select
// Errors are returned in p_sesult by reference.
//
void move_data_tcp(int ser_fd, int socket_fd, int select_timeout_ms, t_move_result *p_result)
{
    switch (g_conf_serial.end_point_mode) {
        case EP_MODE_RAW:
            //
            // this is interesting. We will never get here, since dsm_tool will generate
            // socat-based command line for raw modes.
            // Nevertheless, this is fully functional
            //
            move_data_tcp_raw_mode(ser_fd, socket_fd, select_timeout_ms, p_result);
            break;

        case EP_MODE_MODBUS_GW_RTU:
            move_data_modbus(ser_fd, socket_fd, TRUE, FALSE, p_result);
            break;
        case EP_MODE_MODBUS_GW_ASCII:
            move_data_modbus(ser_fd, socket_fd, FALSE, FALSE, p_result);
            break;
        case EP_MODE_MODBUS_CLIENT_RTU:
            move_data_modbus(ser_fd, socket_fd, TRUE, TRUE, p_result);
            break;
        case EP_MODE_MODBUS_CLIENT_ASCII:
            move_data_modbus(ser_fd, socket_fd, FALSE, TRUE, p_result);
            break;
    }
}

//
// Server thread. It is running in Modbus server modes.
// It creates a listening socket and if connect from remote client is detected, starts
// moving data
//
// It has two modes of operation - in exclusive mode, it allows the established connection to
// remain even if other clients try to connect. Otherwise, it kicks the older connection in
// favour of the new one.
//
void *tcp_server_thread(void *arg)
{
    t_srv_thread_info *p_thread_info = (t_srv_thread_info *)arg;
    fd_set listenset;
    int selectret;
    struct timeval listentv;
    struct sockaddr_in tcp_client_addr;
    socklen_t tcp_client_len = sizeof(tcp_client_addr);
    int new_fd;
    tcp_srv_sock = -1;
    t_move_result res;

    p_thread_info->connected_status = FALSE;
    while (!p_thread_info->do_exit) {
        FD_ZERO(&listenset);
        FD_SET(tcp_srv_listen_sock, &listenset);
        listentv.tv_sec = 0;
        listentv.tv_usec = p_thread_info->listen_select_timeout_ms * 1000;
        selectret = select(tcp_srv_listen_sock + 1, &listenset, 0, 0, &listentv);
        if (selectret > 0)
        {
            // NOTE that accept returns a new file descriptor each time. This is because being in
            // server mode we can have many clients connected to us.
            // Although in our case we limit it to one.
            new_fd = accept(tcp_srv_listen_sock, (struct sockaddr*) & tcp_client_addr, &tcp_client_len);
            if (new_fd == -1) {
                dsm_dm_syslog(LOG_ERR, "sock accept returns -1");
                continue;
            } else if (new_fd > 0) {
                // behaviour here depends on exclusive property
                if ((p_thread_info->exclusive) && (tcp_srv_sock != -1)) {
                    // kick out the new client ASAP
                    close(new_fd);
                } else {
                    // kick the old connection out
                    // replace the old client with the new one
                    if ((tcp_srv_sock != -1) && (tcp_srv_sock != new_fd))
                        close(tcp_srv_sock);
                    tcp_srv_sock = new_fd;
                }
                p_thread_info->connected_status = TRUE;
                p_thread_info->connect_trigger = TRUE;
                dsm_dm_syslog(LOG_INFO, "Connection established from client %s", inet_ntoa(tcp_client_addr.sin_addr));
            }
        }
        else if (selectret == -1)
        {
            dsm_dm_syslog(LOG_ERR, "sock select err is %s", strerror(errno));
        }


        // data connection part - almost the same as client
        if (p_thread_info->start_disconnecting && p_thread_info->connected_status) {
            p_thread_info->connected_status = FALSE;
            p_thread_info->start_disconnecting = FALSE;
            if (tcp_srv_sock != -1) {
                close(tcp_srv_sock);
                tcp_srv_sock = -1;
            }
            dsm_dm_syslog(LOG_DEBUG, "Disconnecting TCP client");
        }

        if (p_thread_info->connected_status && p_thread_info->handle_data){

            move_data_tcp(fileno(g_comm_host), tcp_srv_sock, p_thread_info->select_timeout_ms, &res);

            if (res.socket_recv_err || res.socket_send_err || res.socket_shutdown) {
                p_thread_info->connected_status = FALSE;
                p_thread_info->connection_dropped = TRUE;
                tcp_srv_sock = -1;
            }
        }
        // in the absence of connection, there is nothing else to do, and no need to sleep as we use listen with timeout in this loop

        status_logger("TCP Server", p_thread_info->connected_status, p_thread_info->handle_data, 1, TRUE);

        p_thread_info->is_running = TRUE;
    }

    dsm_dm_syslog(LOG_DEBUG, "TCP Server thread is exiting");

    if (tcp_srv_sock != -1)
        close(tcp_srv_sock);

    if (tcp_srv_listen_sock != -1)
        close(tcp_srv_listen_sock);

    p_thread_info->is_running = FALSE;

    return 0;
}

//
// Does all necessary preparation for TCP server, including
// starting the TCP server thread.
//
int prepare_server_thread_tcp(t_srv_thread_info *p_srv_thread, t_conf_tcp_server *p_conf)
{
    int one = 1;
    int ret;
    struct sockaddr_in serverAddr;

    pthread_t thread_info;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // set stack to 128 Kb as it is a reasonable size
    pthread_attr_setstacksize(&attr, 128*1024);

    // create a listening socket, and start a thread that will always run
    // monitoring incoming connections
    if ((tcp_srv_listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        dsm_dm_syslog(LOG_ERR, "TCP socket creation failed %d", errno);
        return -1;
    }

    //
    // After program termination the socket is still busy for a while.
    // This allows an immediate restart of the daemon after termination.
    //
    if (setsockopt(tcp_srv_listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&one,sizeof(one))<0) {
        dsm_dm_syslog(LOG_ERR, "Can't set SO_REUSEADDR on socket");
    }

    // Bind our local address so that the client can send to us
    (void)set_tcp_options(tcp_srv_listen_sock, &p_conf->tcp_options);
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(p_conf->port_local);

    if (bind(tcp_srv_listen_sock, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0) {
        dsm_dm_syslog(LOG_ERR, "Bind failed %d", errno);
        return -1;
    }
    fcntl(tcp_srv_listen_sock, F_SETFL, O_NONBLOCK);
    if (listen(tcp_srv_listen_sock, 5) < 0) {
        dsm_dm_syslog(LOG_ERR, "Listen failed %d", errno);
        return -1;
    }

    ret = pthread_create(&thread_info, &attr, tcp_server_thread, p_srv_thread);

    dsm_dm_syslog(LOG_DEBUG, "Preparing data mover, started TCP SRV thread %d", ret);

    return 0;
}

