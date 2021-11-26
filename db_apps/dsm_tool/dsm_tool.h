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
// A data stream manager tool header file
//
#ifndef _DSM_TOOL_H_
#define _DSM_TOOL_H_

#include "dsm_tool_ep.h"

// some handy types
typedef int BOOL;

#ifndef NULL
#define NULL				0
#endif

#ifndef FALSE
#define FALSE				0
#endif

#ifndef TRUE
#define TRUE				1
#endif

#define MAX_END_POINTS      (50)
#define MAX_DATA_STREAMS    (20)

#define MAX_EXE_LINE_LENGTH (512)
#define MAX_EP_NAME_LENGTH  (128)
#define MAX_STREAM_NAME_LENGTH (128)
#define MAX_ERR_TEXT_LENGTH  (256)
#define MAX_CMD_BUF_SIZE     (1024)

#define RDB_NAME_LENGTH     (128)
#define RDB_VAL_LENGTH      (1024)
#define MAX_DEV_NAME_LENGTH (128)
#define MAX_IP_ADDRESS_LENGTH  (128)
#define MAX_EXE_LENGTH (128)
#define MAX_IDENT_LENGTH (32) // to match settings in DSM Data mover.
#define MAX_CMD_LENGTH (256)

#define MAX_UUID_LENGTH 37 // 8-4-4-4-12
#define MAX_BT_IDENT_LENGTH 128 // bluetooth name or address

// undefine this for release
// define when debugging only
//#define DEEP_DEBUG

// This option is removed due to generic socat does not support this option.
// GNU socat does not support max-children option in UDP-LISTEN, so this option is removed.
// However, it's possible to get a patch, original source is just pre-processed out.
#undef UDP_LISTEN_MAX_CHILDREN

//
// Serial type end point
//
typedef struct
{
    char dev_name[MAX_DEV_NAME_LENGTH];
    int bit_rate;
    int parity;
    int data_bits;
    int stop_bits;
} t_end_point_serial;

//
// Modem end point
//
typedef struct
{
    // same as serial end point
    t_end_point_serial serial;

    // properties specific to modem emulator
    int opt_1;
    int opt_dtr;
    int opt_dsr;
    int opt_sw_fc;
    int opt_hw_fc;
    int opt_dcd;
    int opt_ri;
    int opt_modem_auto_answer;
    BOOL opt_auto_answer_enable;
} t_end_point_modem;

//
// TCP server type end point
//
typedef struct
{
    int port_number;
    BOOL keep_alive;
    int keepcnt;
    int keepidle;
    int keepintvl;
    int max_children;
} t_end_point_tcp_server;

//
// UDP server type end point
//
typedef struct
{
    int port_number;
#ifdef UDP_LISTEN_MAX_CHILDREN
    int max_children;
#endif
} t_end_point_udp_server;

//
// TCP client type end point
//
typedef struct
{
    char ip_address[MAX_IP_ADDRESS_LENGTH];
    int  port_number;
    BOOL keep_alive;
    int keepcnt;
    int keepidle;
    int keepintvl;
    int timeout;
} t_end_point_tcp_client;

//
// TCP client connect-on-demand (COD) type end point
//
typedef struct
{
    char primary_server_ip_or_host[MAX_IP_ADDRESS_LENGTH];
    char backup_server_ip_or_host[MAX_IP_ADDRESS_LENGTH];
    int  primary_server_port;
    int  backup_server_port;
    BOOL keep_alive;
    int keepcnt;
    int keepidle;
    int keepintvl;
    int buf_size;
    int timeout;
    char ident_start[MAX_IDENT_LENGTH];    // identifier start
    char ident_end[MAX_IDENT_LENGTH];    // identifier end
} t_end_point_tcp_client_cod;

//
// UDP client type end point
//
typedef struct
{
    char ip_address[MAX_IP_ADDRESS_LENGTH];
    int  port_number;
    int  timeout;
} t_end_point_udp_client;

//
// GPS type end point
//
typedef struct
{
    BOOL raw_mode;
} t_end_point_gps;

//
// Generic script end point type
//
typedef struct
{
    char exec_name[MAX_EXE_LENGTH];
} t_end_point_exec;

//
// A ppp server end point
//
typedef struct
{
    // properties specific to PPP end point
    char ip_address_srv[MAX_IP_ADDRESS_LENGTH];
    char ip_address_cli[MAX_IP_ADDRESS_LENGTH];
    int  mtu;
    int  mru;
    BOOL raw_ppp;
    BOOL disable_ccp;
} t_end_point_ppp;

//
// An IP conn end point
//
typedef struct
{
    // properties specific to IP conn end point
    char ip_addr_srv[MAX_IP_ADDRESS_LENGTH];
    int port_local;
    int port_remote;
    int is_udp;
    int exclusive;
    BOOL keep_alive;
    int keepcnt;
    int keepidle;
    int keepintvl;
    BOOL no_delay;
    char ident[MAX_IDENT_LENGTH];    // identifier
} t_end_point_ip_modem;

// CSD end point
typedef struct
{
    // inactivity timeout in minutes
    int     inactivity_timeout_mins;
    char    additional_init_str[MAX_CMD_LENGTH]; // custom initialization commands
    int     init_method; // 0 - no initialization, 1 - use default/fixed, 2 use modem emul ep
} t_end_point_csd;

/* Bluetooth Serial Port Profile (SPP) end point */
typedef struct
{
    char uuid[MAX_UUID_LENGTH];
    int rule_operator;
    BOOL rule_negate;
    int rule_property;
    char rule_value[MAX_BT_IDENT_LENGTH];
    int conn_fail_retry;
    int conn_success_retry;
} t_end_point_spp;

/* Bluetooth Gatt Client (GC) end point */
typedef struct
{
    int rule_operator;
    BOOL rule_negate;
    int rule_property;
    char rule_value[MAX_BT_IDENT_LENGTH];
    int conn_fail_retry;
    int conn_success_retry;
} t_end_point_gc;

//
// A poor man's base class for end point
//
typedef struct
{
    char    *name;      // name of end point
    int     type;       // type as per enum above
    void    *specific_props;  // type specific properties
    BOOL    referenced; // referenced in data streams
} t_end_point;

//
// A collection of all end points
//
typedef struct
{
    t_end_point ep[MAX_END_POINTS];
    BOOL    valid;          // common status
    char    err_msg[MAX_ERR_TEXT_LENGTH]; // common error message
    int     configed_end_points;    // number of existing end point objects
    // this supports mutual exclusion mechanism with modem emulator and padd
    char    ser_port_list[RDB_VAL_LENGTH]; // list of serial ports in use
    char    tcp_port_list[RDB_VAL_LENGTH]; // list of TCP server port numbers
    char    udp_port_list[RDB_VAL_LENGTH]; // list of UDP server port numbers
} t_all_end_points;

//
// Data stream structure
//
typedef struct
{
    BOOL    enabled;        // enabled or not
    char    *name;          // name of data stream
    char    *epa_name;      // name of end point A
    int     epa_mode;       // mode of end point A
    char    *epb_name;      // name of end point B
    int     epb_mode;       // mode of end point B
    BOOL    valid;          // is this data stream valid?
    char    err_msg[MAX_ERR_TEXT_LENGTH]; // and error text/ok
} t_data_stream;

//
// A collection of all data streams
//
typedef struct
{
    t_data_stream ds[MAX_DATA_STREAMS];
    int configed_data_streams;      // a handy counter of number of data streams i
} t_all_streams;

// array of pointers to streams
extern t_all_streams g_streams;

// array of pointers to end points
extern t_all_end_points g_end_points;

// extern functions to use across all 'C' modules of dsm_tool
extern int validate_all(void);
extern void do_limits(int *lo, int *hi, int max);
extern BOOL is_modbus_mode(int mode);
extern BOOL is_valid_mode(int mode);
extern BOOL is_tcp_or_udp_type(int type);
extern BOOL is_server_ep_type(int type);
extern BOOL is_client_ep_type(int type);
extern BOOL is_serial_ep_type(int type);
extern BOOL is_serial_or_modem_ep_type(int type);
extern BOOL is_rsXXX_ep_type(int type);
extern BOOL is_modem_compatible_type(int type);
extern t_end_point *get_end_point_ptr(char *ep_name);
extern int generate_pre_post_cmd(char *cmd_buf, int ds_index, int cmd_buf_size, BOOL is_post);
extern BOOL kill_method_alt(int ds_index);
extern int generate_cmd(char *cmd_buf, int ds_index, int cmd_buf_size);
extern void dsm_syslog(int priority, const char *format, ...);
extern void print_streams(void);
extern void print_eps(void);

// a global RDB session handle
extern struct rdb_session *g_rdb_session;

#endif
