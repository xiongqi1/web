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
// Header file for modem emulator end point, with functions specific
// to end point #13 only (a so-called Modem IP end point).
//

#ifndef _MODEM_IP_H_
#define _MODEM_IP_H_

// A structure that provided feedback to the caller about move data result
typedef struct {

    BOOL socket_recv_err;
    BOOL socket_send_err;
    BOOL ser_read_err;
    BOOL ser_write_err;
    BOOL socket_shutdown;
    BOOL got_escape;
    BOOL socket_not_known;
} t_move_result;

//
// Support escape search
//
typedef struct
{
    int silence_count_ms;
    int escape_char_count;
    int escape_search_state;
    int elapsed_interval_ms;
    struct timespec old_time;
} t_escape_search;

// declarations of functions used by EP#13 internally
extern BOOL do_connect_udp_cli(t_conf_ip_modem *p_conf);
extern BOOL do_connect_tcp_cli(t_conf_ip_modem *p_conf);
extern void do_disconnect_cli(void);
extern void move_data_udp(int ser_fd, int socket_fd, int select_timeout_ms,
    BOOL do_escape_search, t_escape_search *p_escape_search, t_move_result *p_result, BOOL server_mode);
extern void move_data_tcp(int ser_fd, int socket_fd, int select_timeout_ms,
    BOOL do_escape_search, t_escape_search *p_escape_search, t_move_result *p_result);
extern int prepare_ip_modem_srv_udp(t_srv_thread_info *p_srv_thread);
extern int prepare_ip_modem_srv_tcp(t_srv_thread_info *p_srv_thread, t_conf_ip_modem *p_conf);
extern BOOL escape_detector(t_escape_search *p_search, char *buff, int bytes_received, BOOL is_timeout);
extern void reset_escape_search(t_escape_search *p);
extern int hostname_to_ip(const char *hostname, char *ip, int sock_type);
extern BOOL send_ident(int socket_fd, char *ident, BOOL use_udp);
#endif

