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
// Contains miscellaneous helpers
// 1) RDB helpers
// 2) print helpers for debugging
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
#include <sys/ioctl.h>
#include <fcntl.h>
#include "rdb_ops.h"
#include "dsm_data_mover.h"

#define NAME_BUF_LEN     (128)

// a pointer to RDB session that has been established already
extern struct rdb_session *g_rdb_session_p;

//
// Get a single string into a buffer allocated by the caller
// The name of RDB variable is given (for convenience) by
// two pointers, *root and *name, for example, "service.dsm.ep.name"
// and "ip_address" will result in string being read from service.dsm.ep.name.ip_address
// RDB variable.
//
int get_single_str_root(const char *root, const char *name, char *str, int *size)
{
    char name_buf[NAME_BUF_LEN];
    snprintf(name_buf, NAME_BUF_LEN, "%s.%s", root, name);
    return rdb_get(g_rdb_session_p, name_buf, str, size);
}

//
// Get a single int by reference:
// note assumes that sizeof(int) == sizeof(u_long)
//
int get_single_ulong_root(const char *root, const char *name, u_long *my_u_long)
{
    char name_buf[NAME_BUF_LEN];
    snprintf(name_buf, NAME_BUF_LEN, "%s.%s", root, name);
    // careful on platforms where u_long is not 32 bits!
    // in this case, we may have to do some range checking and case explicitely
    return rdb_get_int(g_rdb_session_p, name_buf, (int *)my_u_long);
}

//
// Get a single int by reference
//
int get_single_int_root(const char *root, const char *name, int *my_int)
{
    char name_buf[NAME_BUF_LEN];
    snprintf(name_buf, NAME_BUF_LEN, "%s.%s", root, name);
    return rdb_get_int(g_rdb_session_p, name_buf, my_int);
}

//
// Get a single unsigned char by reference
//
int get_single_uchar_root(const char *root, const char *name, u_char *my_u_char)
{
    char name_buf[NAME_BUF_LEN];
    int myint, ret;
    snprintf(name_buf, NAME_BUF_LEN, "%s.%s", root, name);
    ret = rdb_get_int(g_rdb_session_p, name_buf, &myint);
    if (ret != 0) {
        *my_u_char = 0;
        return ret;
    }
    if ((myint <= 255) && (myint >=0)) {
        *my_u_char = (u_char)myint;
        return 0;
    }
    return -ERANGE;
}


#ifdef DEEP_DEBUG
//
// Helper to print/log the serial configuration read from RDB
//
int print_serial_config(t_conf_serial *p_conf)
{
    if (p_conf) {
        dsm_dm_syslog(LOG_INFO,
                "Device name %s\nBit rate %ld\nData bits %d\nStop bits %d\nParity %d\n",
                p_conf->dev_name,
                p_conf->bit_rate,
                p_conf->data_bits,
                p_conf->stop_bits,
                p_conf->parity);
        return 0;
    }
    return -1;
}

//
// Helper to print/log the TCP server configuration read from RDB
//
int print_tcp_server_config(t_conf_tcp_server *p_conf)
{
    t_tcp_common_options *p_tcp_conf;
    if (p_conf) {
        p_tcp_conf = (t_tcp_common_options *)(&p_conf->tcp_options);
        if (p_tcp_conf) {
            dsm_dm_syslog(LOG_INFO,
               "Port local %d\n"
               "Max clients %d\n"
               "Exclusive mode %d\n"
               "Disable Nagle %d\n"
               "Keep alive %d\n"
               "Keep count %d\n"
               "Keep interval %d\n"
               "Keep idle %d\n",
               p_conf->port_local,
               p_conf->max_clients,
               (int)p_conf->exclusive,
               (int)p_tcp_conf->no_delay,
               (int)p_tcp_conf->keep_alive,
               p_tcp_conf->keepcnt,
               p_tcp_conf->keepintvl,
               p_tcp_conf->keepidle);
               return 0;
        }
    }
    return -1;
}

//
// Helper to print/log the TCP client configuration read from RDB
//
int print_tcp_client_config(t_conf_tcp_client *p_conf)
{
    t_tcp_common_options *p_tcp_conf;
    if (p_conf) {
        p_tcp_conf = (t_tcp_common_options *)(&p_conf->tcp_options);
        if (p_tcp_conf) {
            dsm_dm_syslog(LOG_INFO,
               "IP Address remote %s\n"
               "Port remote %d\n"
               "Retry timeout %d\n"
               "Identifier %s\n"
               "Disable Nagle %d\n"
               "Keep alive %d\n"
               "Keep count %d\n"
               "Keep interval %d\n"
               "Keep idle %d\n",
               p_conf->ip_or_host_remote,
               p_conf->port_remote,
               p_conf->retry_timeout,
               p_conf->ident,
               (int)p_tcp_conf->no_delay,
               (int)p_tcp_conf->keep_alive,
               p_tcp_conf->keepcnt,
               p_tcp_conf->keepintvl,
               p_tcp_conf->keepidle);
            return 0;
        }
    }
    return -1;
}

//
// Helper to print/log the TCP client connect-on-demand (COD)
// configuration read from RDB
//
int print_tcp_client_cod_config(t_conf_tcp_client_cod *p_conf)
{
    t_tcp_common_options *p_tcp_conf;
    if (p_conf) {
        p_tcp_conf = (t_tcp_common_options *)(&p_conf->tcp_options);
        if (p_tcp_conf) {
            dsm_dm_syslog(LOG_INFO,
               "IP Addresses %s %s\n"
               "Port %d %d\n"
               "Retry timeout %d\n"
               "Buf size %d\n"
               "Identifier start %s\n"
               "Identifier end %s\n"
               "Disable Nagle %d\n"
               "Keep alive %d\n"
               "Keep count %d\n"
               "Keep interval %d\n"
               "Keep idle %d\n",
               p_conf->primary_server_ip_or_host,
               p_conf->backup_server_ip_or_host,
               p_conf->primary_server_port,
               p_conf->backup_server_port,
               p_conf->inactivity_timeout,
               p_conf->buf_size,
               p_conf->ident_start,
               p_conf->ident_end,
               (int)p_tcp_conf->no_delay,
               (int)p_tcp_conf->keep_alive,
               p_tcp_conf->keepcnt,
               p_tcp_conf->keepintvl,
               p_tcp_conf->keepidle);
            return 0;
        }
    }
    return -1;
}


#endif

