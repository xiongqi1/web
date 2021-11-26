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
// A part of data stream manager data mover daemon
// The initial application is to replace socat in modbus scenarios
//

#ifndef _DSM_DATA_MOVER_H_
#define _DSM_DATA_MOVER_H_

//#define DEEP_DEBUG
// Intended future usage is 1:N data streams (one end point to many).
// Therefore, initial UDP support is included
//#define UDP_SUPPORT_ENABLED

#include <pthread.h>
#include <sys/socket.h>
#include "../dsm_tool/dsm_tool_ep.h"

// make internal syslog wrapper function available even to low-level functions
extern void dsm_dm_syslog(int priority, const char *format, ...);

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

// poll time in the main loop
#define POLL_TIME_US    (100000)

// max length of serial port device name
#define DEV_NAME_LENGTH 128

// ip address length
#define IP_ADDR_LENGTH  32

// Identifier sent by the client length
#define IDENT_LENGTH    33

// highest port number for TCP/UDP sockets
#define MAX_PORT_NUMBER (65535)

// Thread timing related constants
// these could be adjusted if needed
#define THREAD_SLEEP_TIME_MS   (1000)       // thread sleeps 1 second in the absence of connection
#define SELECT_TIMEOUT_MS      (50)         // value in milliseconds used in select timeout

#define SEND_TIMEOUT_SER_MS     (10000)     // give up on all serial port sends in 10 seconds
#define SEND_TIMEOUT_SOCK_MS     (10000)    // give up on all socket sends in 10 seconds

// A structure that provided feedback to the caller about move data result
typedef struct {

    BOOL socket_recv_err;
    BOOL socket_send_err;
    BOOL ser_read_err;
    BOOL ser_write_err;
    BOOL socket_shutdown;
    BOOL got_escape;
    BOOL socket_not_known;
    BOOL select_err;
} t_move_result;

//
// This structure is accessed from both main and client thread
// Where necessary, members that can be written from both threads
// are protected using a mutex.
//
typedef struct {
    int     sleep_time_ms;          // written by main thread only
    int     select_timeout_ms;      // written by main thread only
    BOOL    do_exit;                // written by main thread only
    BOOL    is_running;             // written by client threads only
    BOOL    handle_data;            // written by main thread only
    BOOL    connected_status;       // written by client thread only
    //
    // the following members are protected via a mutex since they are
    // write-accessed from 2 threads. Due to programme logic, only
    // extendable to other members. Please see detailed explanation
    // in protected_check_flag function header for guidelines on
    // shared data protection in this app's threading model
    //
    BOOL    start_connecting;       // written by 2 threads
    BOOL    start_disconnecting;    // written by 2 threads
    BOOL    connection_dropped;     // written by 2 threads
    BOOL    connect_failed;         // written by 2 threads
    pthread_mutex_t mutex;          // a mutex to protect the above

    BOOL    is_udp;                 // is this a UDP "client"
    void    *p_ep_config;
} t_cli_thread_info;

//
// This structure is accessed from both main and server thread
// Where necessary, members that can be written from both threads
// are protected using a mutex
//
typedef struct {
    int     sleep_time_ms;          // written by main thread only
    int     select_timeout_ms;      // written by main thread only
    int     listen_select_timeout_ms; // written by main thread only
    BOOL    do_exit;                // written by main thread only
    BOOL    is_running;             // written by server thread only
    BOOL    exclusive;              // written by main thread only
    BOOL    handle_data;            // written by main thread only
    BOOL    connected_status;       // written by server thread only
    //
    // the following members are protected via a mutex since they are
    // write-accessed from 2 threads. Due to programme logic, only
    // extendable to other members. Please see detailed explanation
    // in protected_check_flag function header for guidelines on
    // shared data protection in this app's threading model
    //
    BOOL    connection_dropped;     // written by 2 threads
    BOOL    start_disconnecting;    // written by 2 threads
    BOOL    connect_trigger;        // written by 2 threads
    pthread_mutex_t mutex;          // a mutex to protect the above

    BOOL    is_udp;
    void    *p_ep_config;
} t_srv_thread_info;

typedef struct {
    BOOL keep_alive;
    int keepcnt;
    int keepintvl;
    int keepidle;
    BOOL no_delay;  // disable Nagle algorithm
} t_tcp_common_options;

typedef struct {
    int end_point_mode; // mode as per DSM
    int port_local;
    int exclusive;
    int max_clients;
    t_tcp_common_options tcp_options;
} t_conf_tcp_server;

typedef struct {
    int end_point_mode; // mode as per DSM
    char ip_or_host_remote[IP_ADDR_LENGTH]; // server (remote) ip address or host name
    int port_remote;
    t_tcp_common_options tcp_options;
    int retry_timeout;
    // in client modes, we can send this identifier string on connect
    char ident[IDENT_LENGTH];
} t_conf_tcp_client;

typedef struct {
    int end_point_mode; // mode as per DSM
    BOOL has_backup_server; // if backup server is configured: this is NOT in RDB
    int primary_server_port;   // primary server
    int backup_server_port;   // backup server
    t_tcp_common_options tcp_options;   // tcp options
    int inactivity_timeout; // inactivity timeout in seconds
    int buf_size;           // size of tx buffer - minimum tx size
    char primary_server_ip_or_host[IP_ADDR_LENGTH]; // main server (remote) ip address or host name
    char backup_server_ip_or_host[IP_ADDR_LENGTH]; // backup server (remote) ip address or host name
    // we can send this identifier string on connect
    char ident_start[IDENT_LENGTH];
    // we can send this identifier string before disconnect
    char ident_end[IDENT_LENGTH];
} t_conf_tcp_client_cod;

typedef struct {
    int end_point_mode; // mode as per DSM
    int port_local;
} t_conf_udp_server;

typedef struct {
    int end_point_mode; // mode as per DSM
    char ip_or_host_remote[IP_ADDR_LENGTH]; // server (remote) ip address or host name
    int port_remote;
    int retry_timeout;
    // in client modes, we can send this identifier string on connect
    char ident[IDENT_LENGTH];
} t_conf_udp_client;

// reflect the exact contents of the web page and underlying RDB structure for serial end point
typedef struct {
    char dev_name[DEV_NAME_LENGTH]; // device name
    u_long bit_rate;
    u_char data_bits;
    u_char stop_bits;
    u_char parity;
    // 4 types of end point generate the same data in RDB
    // generic serial, RS232, RS485, RS422
    int end_point_type; // which end point type is it as above
    int end_point_mode; // mode as per DSM
} t_conf_serial;

//////////////////////////////////////////////////////////////////////////////////
// prototypes for functions that deal TCP/UDP sides of connection
extern int prepare_client_thread(t_cli_thread_info *p_cli_thread, BOOL connect_on_demand);
extern int prepare_server_thread(t_srv_thread_info *p_srv_thread, t_conf_tcp_server *p_conf);
extern void do_disconnect_cli(BOOL is_udp);
extern BOOL do_connect_udp_cli(t_cli_thread_info *p_thread_info);
extern BOOL do_connect_tcp_cli(t_cli_thread_info *p_thread_info);
extern BOOL do_connect_tcp_cli_cod(t_cli_thread_info *p_thread_info, BOOL send_id);
extern void move_data_udp(int ser_fd, int socket_fd, int select_timeout_ms,
    t_move_result *p_result, BOOL server_mode);
extern void move_data_tcp(int ser_fd, int socket_fd, int select_timeout_ms, t_move_result *p_result);
extern int prepare_server_thread_udp(t_srv_thread_info *p_srv_thread);
extern int prepare_server_thread_tcp(t_srv_thread_info *p_srv_thread, t_conf_tcp_server *p_conf);
extern int hostname_to_ip(const char *hostname, char *ip, int sock_type);
extern void status_logger(const char *prefix, BOOL connected_status, BOOL moving_data, int index, BOOL add_err_counters);
extern BOOL send_ident(int socket_fd, char *ident, BOOL use_udp);
extern int send_all(int sock, u_char *buffer, int len, int send_flags, int total_timeout_ms, BOOL is_serial, struct sockaddr *udp_addr, int udp_addrlen);

//////////////////////////////////////////////////////////////////////////////////
// prototypes for functions that live in dsm_data_mover_ser_port.c
extern FILE *open_serial_port(const char *ext_port_name);
extern int set_baud_rate(FILE *ser_port_file, u_long baud);
extern int set_character_format(FILE *ser_port_file, int size, int parity, int stopb);
extern int set_port_defaults(FILE *ser_port_file);
extern int initialize_serial_port(void);

//////////////////////////////////////////////////////////////////////////////////
// prototypes for functions that live in dsm_data_mover_utils.c
extern int get_single_str_root(const char *root, const char *name, char *res, int *size);
extern int get_single_ulong_root(const char *root, const char *name, u_long *my_u_long);
extern int get_single_int_root(const char *root, const char *name, int *my_int);
extern int get_single_uchar_root(const char *root, const char *name, u_char *my_u_char);

#ifdef DEEP_DEBUG
// print helpers - only used for debugging
extern int print_serial_config(t_conf_serial *p_conf);
extern int print_tcp_server_config(t_conf_tcp_server *p_conf);
extern int print_tcp_client_config(t_conf_tcp_client *p_conf);
extern int print_tcp_client_cod_config(t_conf_tcp_client_cod *p_conf);

#endif


extern t_conf_serial g_conf_serial;
extern t_conf_tcp_client g_conf_tcp_client;
extern t_conf_tcp_client_cod g_conf_tcp_client_cod;
extern t_conf_tcp_server g_conf_tcp_server;
extern t_conf_udp_client g_conf_udp_client;
extern t_conf_udp_server g_conf_udp_server;
extern int g_epa_mode;
#endif

