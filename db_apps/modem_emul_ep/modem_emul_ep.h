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
// Header file for modem emulator end point
//

#ifndef _MODEM_EMUL_EP_H_
#define _MODEM_EMUL_EP_H_

#include <pthread.h>
// include low level stuff
#include "modem_hw.h"
#include "../dsm_tool/dsm_tool_ep.h"

// Possible AT Engine responses codes - as per legacy code
enum {
    OK, CONNECT, RING, NOCARRIER, ERROR, OTHER, NODIALTONE, BUSY, NOANSWER, NORESP, UPDATEREQ
};

// V250 Operating states. Only COMMAND, ON_LINE and ON_LINE_COMMAND are actually used
typedef enum {
    COMMAND,
    DISC_TIMEOUT,
    DTR_DIAL,
    ON_LINE,
    ON_LINE_COMMAND,
    CSD_DIAL,
    TELNET,
    LOCAL,
    DISABLE_V250
} e_modem_state;

//
// A simple struct to capture what was actually sent by host after ATD (dial) command
// At the moment, for stage 1, we save it but don't actually care
// In stage 2 and 3, for pad mode and CSD, this will be interpreted
// and additional structure members will be added
//
typedef struct {
    char dialled_str[128];
} t_dial_details;

//
// Supports the new data table driven AT command processor
//
typedef struct {
    int pre_increment;
    const char *match_str;
    char *(*processor)(FILE* stream, char* params, char* stat);
} t_at_command;

// poll time in the main loop
#define POLL_TIME_US    (100000)

// max length of serial port device name
#define DEV_NAME_LENGTH 128

// ip address length
#define IP_ADDR_LENGTH  32

// Identifier sent by the client length
#define IDENT_LENGTH    33

// at command length
#define MAX_CMD_LENGTH  256

// file name length
#define FILE_NAME_LENGTH 256

// possible values for opt_dtr in t_conf_v250 structure
#define V250_DTR_IGNORE			0
#define V250_DTR_COMMAND		1
#define V250_DTR_HANGUP			2
#define V250_DTR_AUTODIAL		3	// not used
#define V250_DTR_REVAUTODIAL	4	// not used
#define V250_DTR_LOPASSAT		5	// not used

// possible values for opt_dcd in t_conf_v250 structure
#define V250_DCD_ALWAYS			0	// always assert DCD when Modem Emul is running
#define V250_DCD_CONNECT		1	// DCD is on when modem is in connected state
#define V250_DCD_NEVER			2	// never activate DCD
#define V250_DCD_PDP			3	// old modem emulator incorrectly calls this PPP

// possible values for opt_ri in t_conf_v250 structure
#define V250_RI_ALWAYS			0	// always assert RI
#define V250_RI_RING			1	// assert RI when incoming call is detected
#define V250_RI_NEVER			2	// never assert RI

// possible values for opt_dsr in t_conf_v250 structure
#define V250_DSR_ALWAYS			0	// always assert DSR
#define V250_DSR_REGISTERED		1	// SIM is registered
#define V250_DSR_PDP			2	// old modem emulator incorrectly calls this PPP
#define V250_DSR_NEVER			3	// never assert DSR
#define V250_DSR_MIMIC_DTR      4	// just follow what DTR is doing

// bits for opt_1 member of t_conf_v250 structure
#define ECHO_ON                 0x0001
#define QUIET_ON                0x0002
#define OK_ON_CR                0x0004
#define SUPRESS_LF              0x0008
#define OK_ON_UNKNOWN           0x0010
#define VERBOSE_RSLT            0x0020

// Applicable to IP Modem End point client and server threads
// Thread timing related constants
// these could be adjusted if needed
#define THREAD_SLEEP_TIME_MS   (1000)
#define SELECT_TIMEOUT_MS      (50)

//
// structure that reflects v250 configuration which
// is 1:1 relationship with properties of modem emulator
// end point in RDB
//
typedef struct {
    char    dev_name[DEV_NAME_LENGTH]; // device name
    u_char opt_1; // Bitmaped options 1
    u_char opt_dtr; // DTR Options
    u_char opt_dsr; // DSR Options
    u_char opt_sw_fc; // S/w flow control Xon/Xoff
    u_char opt_hw_fc; // H/w flow control RTS/CTS
    u_char opt_dcd; // DCD Options
    u_char opt_ri; // RI Options
    u_short icto; // Intercharacter timeout - legacy, not used yet
    u_short idct; // Idle disconnect timeout - legacy, not used yet
    u_short sest; // Maximum session timeout - legacy, not used yet
    u_long bit_rate; // Baud rate of ext serial port
    u_char data_bits; // Number of data bits 5,6,7,8
    u_char parity; // Parity
    u_char stop_bits; // Number of stop bits 1 or 2
    u_char opt_modem_auto_answer; // Modem auto answer rings
    u_char opt_backspace_character; // backspace character used by AT engine
    u_char opt_auto_answer_enable; // enable auto answer
    u_char opt_in_call_connect; // connect incoming call at the expense of an established outgoing one
} t_conf_v250;

//
// structure that reflects ppp configuration which
// is almost a 1:1 relationship with properties of ppp
// end point in RDB
//
typedef struct {
    char    ip_addr_srv[IP_ADDR_LENGTH]; // server (our) ip address
    char    ip_addr_cli[IP_ADDR_LENGTH]; // dial up ppp client ip address
    u_char  debug; // cannot be changed by user - in debug mode, ppp is run in verbose mode
    u_long  mtu; // MTU, bytes
    u_long  mru; // MRU, bytes

    //
    // not fully supported
    // if this is set, then we assume no other pppd processes are possible
    //
    u_char  exclusive;

    // raw ppp: the client does not dial and AT command engine is bypassed
    BOOL  raw_ppp;
    // disable ccp: disabled compression control protocol field of PPP
    BOOL  disable_ccp;

    // this is stored for convenience when config is read.
    // slighly different to the actual device file name as forward slashes
    // are removed: /dev/ttyAPP4 -> ttyAPP4
    char    tty_name[DEV_NAME_LENGTH];
} t_conf_ppp;

// possible status of ppp connection
typedef enum {
    STATUS_IDLE = 0,
    STATUS_CONNECTED = 1,
    STATUS_UNDEFINED = 255
} e_ppp_connection_status;


//
// For stage 2 prototyping, we are going to use TCP client end point for
// filling in this structure.
//
// For final product, this will 1:1 with properties of new end point type
// which has:
// Incoming port number
// Outgoing IP address
// Outgoing port number
// TCP/UDP
// Misc properties such as which connection has priority, also retries, keep alive,
// etc.
//
typedef struct {
    char    ip_or_host_remote[IP_ADDR_LENGTH]; // server (remote) ip address or host name
    int     port_remote;
    BOOL    is_udp;
    int     port_local;
    // in client modes, we can send this identifier string on connect
    char    ident[IDENT_LENGTH];

    // TCP only options
    BOOL    keep_alive;
    int     keepcnt;
    int     keepidle;
    int     keepintvl;
    int     no_delay;
    BOOL    exclusive;

    // reserved for future use: if we are connected as client, drop connection on incoming call
    BOOL    drop_out_call_on_in_call;
} t_conf_ip_modem;

//
// CSD end point configuration
//
typedef struct
{
    // inactivity timeout in minutes
    int     inactivity_timeout_mins;
    char    additional_init_str[MAX_CMD_LENGTH]; // custom initialization
    int     init_method; // 0 - use default/fixed, 1 - no initialization, 2 use config settings (custom)
} t_conf_csd;

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
    // escape_seq_received member is protected but this is easily
    // extendable to other members. Please see detailed explanation
    // in protected_check_flag function header for guidelines on
    // shared data protection in this app's threading model
    //
    BOOL    start_connecting;       // written by 2 threads
    BOOL    start_disconnecting;    // written by 2 threads
    BOOL    connection_dropped;     // written by 2 threads
    BOOL    connect_failed;         // written by 2 threads
    BOOL    escape_seq_received;    // written by 2 threads
    pthread_mutex_t mutex;          // a mutex to protect the above
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
    // escape_seq_received member is protected but this is easily
    // extendable to other members. Please see detailed explanation
    // in protected_check_flag function header for guidelines on
    // shared data protection in this app's threading model
    //
    BOOL    connection_dropped;     // written by 2 threads
    BOOL    start_disconnecting;    // written by 2 threads
    BOOL    escape_seq_received;    // written by 2 threads
    BOOL    connect_trigger;        // written by 2 threads
    pthread_mutex_t mutex;          // a mutex to protect the above
} t_srv_thread_info;

//////////////////////////////////////////////////////////////////////////////////
// entry point into AT command processor
extern void CDCS_V250Engine(FILE *stream);

//////////////////////////////////////////////////////////////////////////////////
// prototypes for functions that deal with PPP end point
extern e_ppp_connection_status get_ppp_connection_status(void);
extern void terminate_ppp_daemon(void);
extern int prepare_ppp_files(t_conf_v250 *p_conf_v250, t_conf_ppp *p_conf_ppp);
extern void maintain_ppp_connection(void);

//////////////////////////////////////////////////////////////////////////////////
// prototypes for functions that deal with ip modem end point
extern int prepare_ip_modem_cli(t_cli_thread_info *p_cli_thread);
extern int prepare_ip_modem_srv(t_srv_thread_info *p_srv_thread, t_conf_ip_modem *p_conf);

//////////////////////////////////////////////////////////////////////////////////
// prototypes for functions that deal with CSD end point
extern int prepare_csd(t_conf_csd *p_conf);

//////////////////////////////////////////////////////////////////////////////////
// modem state checker function
extern e_modem_state get_modem_state(void);

// ring indicator helper function
extern BOOL get_ring_ind_state(void);

// functions that help to work out which End Point B is Modem Emulator connected to
extern BOOL is_epb_type_ppp(void);
extern BOOL is_epb_type_ip_modem(void);
extern BOOL is_epb_type_csd(void);

//////////////////////////////////////////////////////////////////////////////////
// prototypes for functions that live in modem_ser_port.c
extern FILE *open_serial_port(const char *ext_port_name);
extern int set_hw_flow_ctrl(FILE *ser_port_file, int on_or_off);
extern int set_sw_flow_ctrl(FILE *ser_port_file, int on_or_off);
extern int set_baud_rate(FILE *ser_port_file, u_long baud);
extern int set_character_format(FILE *ser_port_file, int size, int parity, int stopb);
extern int set_port_defaults(FILE *ser_port_file);
extern u_long get_baud_rate(FILE* port);
extern int initialize_serial_port(void);

//////////////////////////////////////////////////////////////////////////////////
// prototypes for functions that live in modem_emul_utils.c
extern int get_single_str_root(const char *root, const char *name, char *res, int *size);
extern int get_single_ulong_root(const char *root, const char *name, u_long *my_u_long);
extern int get_single_int_root(const char *root, const char *name, int *my_int);
extern int get_single_uchar_root(const char *root, const char *name, u_char *my_u_char);

#ifdef DEEP_DEBUG
// print helpers - only used for debugging
extern int print_ppp_config(t_conf_ppp *p_conf_ppp);
extern int print_v250_config(t_conf_v250 *p_conf_v250);
extern int print_ip_modem_config(t_conf_ip_modem *p_conf);
extern int print_csd_config(t_conf_csd *p_conf);
extern void print_termios(const char *prefix, struct termios *tio);
#endif

// legacy functions from modem emulator
extern char *getSingle(const char *myname);
extern int setSingleStr(const char *name, const char *buf);
extern int getSingleInt(const char *myname);
extern int setSingleInt(const char *name, int var);

// multi-threading support
extern BOOL protected_check_flag_set(BOOL *p_flag, pthread_mutex_t *p_mutex);

// send function usable for sockets and serial ports
extern int send_all(int sock, char *buffer, int len, int send_flags, int total_timeout_ms, BOOL is_serial, struct sockaddr *udp_addr, int udp_addrlen);

// debugging and logging support
extern void status_logger(const char *prefix, BOOL connected_status, BOOL moving_data, BOOL escape_received, int modem_state, int index);
#endif

