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
// A data stream manager tool
//
// Provides the following services:
//
// 1) validation of DSM RDB data structures
// 2) generaton of command line for invoking DSM-associated processes
//
// Please refer to http://pdgwiki/mediawiki/index.php/Data_Stream_Switch_Design_Specification_April_2014
// for full aspects of design
//
// Some examples of using dsm_tool:
// 1) dsm_tool -d -r service.dsm - delete all rdb entries starting with dsm
// 2) dsm_tool -i -r service.dsm - add dynamic (non-persistent) RDB entries
// 3) dsm_tool -v -r service.dsm - validate DSM RDB data (but do not worry about creating commands)
// 4) dsm_tool -c -r service.dsm - validate and generate command line syntax for launching DS associated
//      processes
// 5) dsm_tool -i -c -r service.dsm - initialize, validate and generate command line syntax for launching DS associated
//      processes

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include "fcntl.h"
#include <pwd.h>
#include <sys/timerfd.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

#include "dsm_tool.h"
#include "rdb_ops.h"

// the one and only RDB session handle
struct rdb_session *g_rdb_session=NULL;

// a shorthand
#define GET_SINGLE_INT(rdb, ep_name, name, value) \
    do { \
        sprintf(rdb_var_name, "%s.%s.%s", rdb, ep_name, name); \
        if (rdb_get_int(g_rdb_session, rdb_var_name, &value) != 0) \
            return -1; \
    } while (0)

// globals

// Collection of streams
t_all_streams g_streams;

// Collection of end points
t_all_end_points g_end_points;

// default verbosity
int dsm_verbosity = LOG_ERR;

//
// Show the user how to invoke this programme
//
static void usage(char **argv)
{
    fprintf(stderr, "Data stream manager tool\n"
            "Usage:\n"
            "-i Initialize dynamic RDB variables\n"
            "-v Validate\n"
            "-c Create command lines\n"
            "-d Delete rdb entries starting with rdb_root\n"
            "-V increase verbosity\n"
            "-r [rdb_root] (mandatory)\n"
           );
}

//
// a simple wrapper around printf or syslog, qualified by verbosity
//
void dsm_syslog(int priority, const char *format, ...)
{
    va_list fmtargs;
    char buffer[1024];

    va_start(fmtargs,format);
    vsnprintf(buffer,sizeof(buffer)-1, format, fmtargs);
    va_end(fmtargs);

    if (priority <= dsm_verbosity)
    {
#ifdef V_LOGS_TO_PRINTF
        printf("DSM LOG, priority %d: %s\n", priority, buffer);
#else
        syslog(priority, "%s", buffer);
#endif
    }
}


//
// delete all variables in RDB root
//
// "buf" contains a pointer to a string with a list of all RDB
// variables in the RDB root.
//
// Note that it only deletes if the start of the string matches
// the rdb root. For example, if rdb root is ee_test, we
// do not want to delete aa.ee_test.xxx, but do want
// to delete ee_test.xxx
//
// Note that buf_len should be the text buffer plus NULL terminator
// Also note that buf is spoiled by this function (caller to be aware)
//
static int delete_vars(char *rdb_root, char *buf, int buf_len, char separator_char, BOOL clear_only)
{
    int i, result;

    // start of the buffer, points at the beginning of substring after the previous separator character
    char *buf_pos = buf;

    for (i = 0 ; i < buf_len ; i++)
    {
        if ((buf[i] == separator_char) || (buf[i]==0))
        {
            // spoil the caller's buffer but we don't care
            buf[i] = 0;
            // check that the match is indeed in the start position
            if (strstr(buf_pos, rdb_root) == buf_pos)
            {
				if (clear_only) {
					result = rdb_set_string(g_rdb_session, buf_pos, "");
				} else {
					result = rdb_delete(g_rdb_session, buf_pos);
				}
                if (result < 0)
                {
                    dsm_syslog(LOG_ERR, "Delete var %s error",  buf_pos);
                    return -1;
                }
            }
            buf_pos += &buf[i]-buf_pos+1; // increment pointer using pointer arithmetics
        }
    }

    return 0;
}

// delete RDB root
//
// a character that kernel RDB driver uses as a separator when it returns a list of RDB variables
#define SEPARATOR_CHAR '&'
static int delete_rdb_root(char *rdb_root, BOOL clear_only)
{
    char *name_buf;
    int buf_len;
    const int max_len = 10000;
    // just get a large chunk
    name_buf = malloc(buf_len = max_len);

    // flags (last argument) don't matter
    if (name_buf && (rdb_getnames(g_rdb_session, rdb_root, name_buf, &buf_len, 0) == 0))
    {
        if ((buf_len > 0) && (buf_len < max_len))
        {
            // just in case that get names forgets to terminate the buffer
            name_buf[buf_len] = 0;
            if (delete_vars(rdb_root, name_buf, buf_len + 1,
                            SEPARATOR_CHAR, clear_only) < 0)
            {
                dsm_syslog(LOG_ERR, "Delete vars fail %s", rdb_root);
            }
        }
    }

    free (name_buf);
    return 0;
}

//
// Swap low and hi if needed
// Range limit to max
// If hi is set to -1, assume max
//
// On exit, *lo is always lower than *hi,
// *lo is greater or equal to 0,
// and *hi is less or equal to max
//
void do_limits(int *lo, int *hi, int max)
{
    if (*hi == -1)
    {
        *hi = max;
    }

    if (*lo < 0)
    {
        *lo = 0;
    }

    if (*lo > *hi)
    {
        int tmp = *hi;
        *hi = *lo;
        *lo = tmp;
    }
}

//
// Initialize memory for streams
//
static void init_streams(void)
{
    memset(&g_streams, 0, sizeof(g_streams));
}

//
// Initialize memory for end points
//
static void init_eps(void)
{
    memset(&g_end_points, 0, sizeof(g_end_points));
}

//
// Free memory occupied by streames
//
static void delete_streams(void)
{
    // free (NULL); is completely valid

    int i;
    for (i = 0 ; i < MAX_DATA_STREAMS ; i++)
    {
        free (g_streams.ds[i].name);
        free (g_streams.ds[i].epa_name);
        free (g_streams.ds[i].epb_name);
    }
}

//
// Free memory occupied by end points
//
static void delete_eps(void)
{
    int i;
    for (i = 0 ; i < MAX_END_POINTS ; i++)
    {
        free (g_end_points.ep[i].name);
        free (g_end_points.ep[i].specific_props);
    }
}

//
// Free all memory
//
static void delete_all(void)
{
    delete_eps();
    delete_streams();
}

//
// Allocate all memory
//
static void init_all(void)
{
    init_eps();
    init_streams();
}

//
// Populate serial EP object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_serial_ep(char *rdb_root, char *ep_name, t_end_point_serial *ep_props)
{
    char rdb_var_name[RDB_NAME_LENGTH];
    char name[RDB_VAL_LENGTH];
    char temp[MAX_DEV_NAME_LENGTH];
    int len;

    sprintf(rdb_var_name, "%s.%s.dev_name", rdb_root, ep_name);

    memset(temp, 0, MAX_DEV_NAME_LENGTH);
    memset(ep_props->dev_name, 0, MAX_DEV_NAME_LENGTH);

    // We support for situations where dev name is a name of other RDB
    // variable that stores the name of the serial device
    len = MAX_DEV_NAME_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, temp, &len) != 0)
        return -1;

    if (strstr(temp, "/dev/") == NULL)
    {
        // read what is in sys dev RDB variable sys.hw.dev.X.name
        len = MAX_DEV_NAME_LENGTH - 1;
        if (!strlen(temp) || (rdb_get(g_rdb_session, temp, ep_props->dev_name, &len) != 0))
            return -1;
    }
    else
    {
        // no redirection, /dev/ttyX is recorded directly in ep RDB properties
        strcpy(ep_props->dev_name, temp);
    }

    // Note: an empty name is probably valid, so no check for this

    memset(name, 0, RDB_VAL_LENGTH);
    sprintf(rdb_var_name, "%s.%s.parity", rdb_root, ep_name);
    len = RDB_VAL_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, name, &len) != 0)
        return -1;

    // convert back to integer
    if (strcmp(name, "none") == 0)
    {
        ep_props->parity = 0;
    }
    else if (strcmp(name, "even") == 0)
    {
        ep_props->parity = 1;
    }
    else if (strcmp(name, "odd") == 0)
    {
        ep_props->parity = 2;
    }
    else
    {
        return -1;
    }

    GET_SINGLE_INT(rdb_root, ep_name, "bit_rate", ep_props->bit_rate);
    GET_SINGLE_INT(rdb_root, ep_name, "data_bits", ep_props->data_bits);
    GET_SINGLE_INT(rdb_root, ep_name, "stop_bits", ep_props->stop_bits);

    // check stop bits as there are no further checks
    if ((ep_props->stop_bits != 1) && (ep_props->stop_bits != 2))
    {
        return -1;
    }

    // check data bits as there are no further checks
    if ((ep_props->data_bits < 5) || (ep_props->data_bits > 8))
    {
        return -1;
    }

    return 0;
}

//
// Populate serial EP object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_modem_ep(char *rdb_root, char *ep_name, t_end_point_modem *ep_props)
{
    char rdb_var_name[RDB_NAME_LENGTH];
    // populate common properties first
    int ret = populate_serial_ep(rdb_root, ep_name, &ep_props->serial);

    if (ret == 0)
    {
        // populate modem emulator props that are additional to serial end point
        GET_SINGLE_INT(rdb_root, ep_name, "opt_1", ep_props->opt_1);
        GET_SINGLE_INT(rdb_root, ep_name, "opt_dtr", ep_props->opt_dtr);
        GET_SINGLE_INT(rdb_root, ep_name, "opt_dsr", ep_props->opt_dsr);
        GET_SINGLE_INT(rdb_root, ep_name, "opt_sw_fc", ep_props->opt_sw_fc);
        GET_SINGLE_INT(rdb_root, ep_name, "opt_hw_fc", ep_props->opt_hw_fc);
        GET_SINGLE_INT(rdb_root, ep_name, "opt_dcd", ep_props->opt_dcd);
        GET_SINGLE_INT(rdb_root, ep_name, "opt_ri", ep_props->opt_ri);
        GET_SINGLE_INT(rdb_root, ep_name, "opt_modem_auto_answer", ep_props->opt_modem_auto_answer);
        return 0;
    }

    //
    // We also have another property, opt_auto_answer_enable, which has been added after the
    // first release. Therefore, we do not want to return an error if it is not there
    // but instead assume a default value (for backwards compatibility reasons)
    //
    sprintf(rdb_var_name, "%s.%s.opt_auto_answer_enable", rdb_root, ep_name);
    if (rdb_get_int(g_rdb_session, rdb_var_name, &ep_props->opt_auto_answer_enable) != 0)
    {
        ep_props->opt_auto_answer_enable = TRUE;
    }

    return -1;
}

//
// Populate TCP server end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_tcp_server_ep(char *rdb_root, char *ep_name, t_end_point_tcp_server *ep_props)
{
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "port_number", ep_props->port_number);
    GET_SINGLE_INT(rdb_root, ep_name, "keep_alive", ep_props->keep_alive);
    GET_SINGLE_INT(rdb_root, ep_name, "max_children", ep_props->max_children);
    if (ep_props->keep_alive)
    {
        GET_SINGLE_INT(rdb_root, ep_name, "keepcnt", ep_props->keepcnt);
        GET_SINGLE_INT(rdb_root, ep_name, "keepidle", ep_props->keepidle);
        GET_SINGLE_INT(rdb_root, ep_name, "keepintvl", ep_props->keepintvl);
    }

    return 0;
}

//
// Populate TCP client end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_tcp_client_ep(char *rdb_root, char *ep_name, t_end_point_tcp_client *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "port_number", ep_props->port_number);
    GET_SINGLE_INT(rdb_root, ep_name, "timeout", ep_props->timeout);
    GET_SINGLE_INT(rdb_root, ep_name, "keep_alive", ep_props->keep_alive);
    if (ep_props->keep_alive)
    {
        GET_SINGLE_INT(rdb_root, ep_name, "keepcnt", ep_props->keepcnt);
        GET_SINGLE_INT(rdb_root, ep_name, "keepidle", ep_props->keepidle);
        GET_SINGLE_INT(rdb_root, ep_name, "keepintvl", ep_props->keepintvl);
    }

    memset(ep_props->ip_address, 0, MAX_IP_ADDRESS_LENGTH);
    sprintf(rdb_var_name, "%s.%s.ip_address", rdb_root, ep_name);
    len = MAX_IP_ADDRESS_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->ip_address, &len) != 0)
        return -1;

    return 0;
}

static int populate_tcp_client_cod_ep(char *rdb_root, char *ep_name, t_end_point_tcp_client_cod *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "primary_server_port", ep_props->primary_server_port);
    GET_SINGLE_INT(rdb_root, ep_name, "backup_server_port", ep_props->backup_server_port);
    GET_SINGLE_INT(rdb_root, ep_name, "timeout", ep_props->timeout);
    GET_SINGLE_INT(rdb_root, ep_name, "buf_size", ep_props->buf_size);
    GET_SINGLE_INT(rdb_root, ep_name, "keep_alive", ep_props->keep_alive);
    if (ep_props->keep_alive)
    {
        GET_SINGLE_INT(rdb_root, ep_name, "keepcnt", ep_props->keepcnt);
        GET_SINGLE_INT(rdb_root, ep_name, "keepidle", ep_props->keepidle);
        GET_SINGLE_INT(rdb_root, ep_name, "keepintvl", ep_props->keepintvl);
    }

    memset(ep_props->primary_server_ip_or_host, 0, MAX_IP_ADDRESS_LENGTH);
    sprintf(rdb_var_name, "%s.%s.primary_server_ip", rdb_root, ep_name);
    len = MAX_IP_ADDRESS_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->primary_server_ip_or_host, &len) != 0)
        return -1;

    memset(ep_props->backup_server_ip_or_host, 0, MAX_IP_ADDRESS_LENGTH);
    sprintf(rdb_var_name, "%s.%s.backup_server_ip", rdb_root, ep_name);
    len = MAX_IP_ADDRESS_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->backup_server_ip_or_host, &len) != 0)
        return -1;

    memset(ep_props->ident_start, 0, MAX_IDENT_LENGTH);
    sprintf(rdb_var_name, "%s.%s.ident_start", rdb_root, ep_name);
    len = MAX_IDENT_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->ident_start, &len) != 0)
        return -1;

    memset(ep_props->ident_end, 0, MAX_IDENT_LENGTH);
    sprintf(rdb_var_name, "%s.%s.ident_end", rdb_root, ep_name);
    len = MAX_IDENT_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->ident_end, &len) != 0)
        return -1;

    return 0;
}

//
// Populate UDP server end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_udp_server_ep(char *rdb_root, char *ep_name, t_end_point_udp_server *ep_props)
{
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "port_number", ep_props->port_number);
#ifdef UDP_LISTEN_MAX_CHILDREN
    GET_SINGLE_INT(rdb_root, ep_name, "max_children", ep_props->max_children);
#endif
    return 0;
}

//
// Populate UDP client end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_udp_client_ep(char *rdb_root, char *ep_name, t_end_point_udp_client *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "port_number", ep_props->port_number);
    GET_SINGLE_INT(rdb_root, ep_name, "timeout", ep_props->timeout);

    memset(ep_props->ip_address, 0, MAX_IP_ADDRESS_LENGTH);
    sprintf(rdb_var_name, "%s.%s.ip_address", rdb_root, ep_name);
    len = MAX_IP_ADDRESS_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->ip_address, &len) != 0)
        return -1;

    return 0;
}

//
// Populate GPS end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_gps_ep(char *rdb_root, char *ep_name, t_end_point_gps *ep_props)
{
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "raw_mode", ep_props->raw_mode);

    return 0;
}

//
// Populate executable type end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_exe_ep(char *rdb_root, char *ep_name, t_end_point_exec *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    memset(ep_props->exec_name, 0, MAX_EXE_LENGTH);
    sprintf(rdb_var_name, "%s.%s.exec_name", rdb_root, ep_name);
    len = MAX_EXE_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->exec_name, &len) != 0)
        return -1;

    return 0;
}

//
// Populate PPP type end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_ppp_ep(char *rdb_root, char *ep_name, t_end_point_ppp *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "mtu", ep_props->mtu);
    GET_SINGLE_INT(rdb_root, ep_name, "mru", ep_props->mru);
    GET_SINGLE_INT(rdb_root, ep_name, "raw_ppp", ep_props->raw_ppp);
    GET_SINGLE_INT(rdb_root, ep_name, "disable_ccp", ep_props->disable_ccp);

    memset(ep_props->ip_address_srv, 0, MAX_IP_ADDRESS_LENGTH);
    sprintf(rdb_var_name, "%s.%s.ip_addr_srv", rdb_root, ep_name);
    len = MAX_IP_ADDRESS_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->ip_address_srv, &len) != 0)
        return -1;

    memset(ep_props->ip_address_cli, 0, MAX_IP_ADDRESS_LENGTH);
    sprintf(rdb_var_name, "%s.%s.ip_addr_cli", rdb_root, ep_name);
    len = MAX_IP_ADDRESS_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->ip_address_cli, &len) != 0)
        return -1;

    return 0;
}

//
// Populate IP Connection type end point object
// Returns:
// 0 on success
// <0 on failure
//
//
static int populate_ip_modem_ep(char *rdb_root, char *ep_name, t_end_point_ip_modem *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "port_local", ep_props->port_local);
    GET_SINGLE_INT(rdb_root, ep_name, "port_remote", ep_props->port_remote);
    GET_SINGLE_INT(rdb_root, ep_name, "is_udp", ep_props->is_udp);

    if (!ep_props->is_udp)
    {
        GET_SINGLE_INT(rdb_root, ep_name, "exclusive", ep_props->exclusive);
        GET_SINGLE_INT(rdb_root, ep_name, "no_delay", ep_props->no_delay);

        // keep alive block of properties
        GET_SINGLE_INT(rdb_root, ep_name, "keep_alive", ep_props->keep_alive);
        if (ep_props->keep_alive)
        {
            GET_SINGLE_INT(rdb_root, ep_name, "keepcnt", ep_props->keepcnt);
            GET_SINGLE_INT(rdb_root, ep_name, "keepidle", ep_props->keepidle);
            GET_SINGLE_INT(rdb_root, ep_name, "keepintvl", ep_props->keepintvl);
        }
    }

    memset(ep_props->ip_addr_srv, 0, MAX_IP_ADDRESS_LENGTH);
    sprintf(rdb_var_name, "%s.%s.ip_address", rdb_root, ep_name);
    len = MAX_IP_ADDRESS_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->ip_addr_srv, &len) != 0)
        return -1;

    memset(ep_props->ident, 0, MAX_IDENT_LENGTH);
    sprintf(rdb_var_name, "%s.%s.ident", rdb_root, ep_name);
    len = MAX_IDENT_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->ident, &len) != 0)
        return -1;

    return 0;
}

//
// Populate CSD type end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_csd_ep(char *rdb_root, char *ep_name, t_end_point_csd *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "inactivity_timeout_mins", ep_props->inactivity_timeout_mins);
    GET_SINGLE_INT(rdb_root, ep_name, "init_method", ep_props->init_method);

    memset(ep_props->additional_init_str, 0, MAX_CMD_LENGTH);
    sprintf(rdb_var_name, "%s.%s.additional_init_str", rdb_root, ep_name);
    len = MAX_CMD_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->additional_init_str, &len) != 0)
        return -1;

    // if we really wanted to, we could do some validation here but modem_emul_ep will do that
    // anyway, so there is nothing else to do
    return 0;
}

//
// Populate SPP type end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_spp_ep (char *rdb_root, char *ep_name,
                            t_end_point_spp *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    memset(ep_props->uuid, 0, MAX_UUID_LENGTH);
    sprintf(rdb_var_name, "%s.%s.uuid", rdb_root, ep_name);
    len = MAX_UUID_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->uuid, &len) != 0)
        return -1;

    GET_SINGLE_INT(rdb_root, ep_name, "rule_operator", ep_props->rule_operator);
    GET_SINGLE_INT(rdb_root, ep_name, "rule_negate", ep_props->rule_negate);
    GET_SINGLE_INT(rdb_root, ep_name, "rule_property", ep_props->rule_property);

    memset(ep_props->rule_value, 0, MAX_BT_IDENT_LENGTH);
    sprintf(rdb_var_name, "%s.%s.rule_value", rdb_root, ep_name);
    len = MAX_BT_IDENT_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->rule_value, &len) != 0)
        return -1;

    GET_SINGLE_INT(rdb_root, ep_name, "conn_fail_retry",
                   ep_props->conn_fail_retry);
    GET_SINGLE_INT(rdb_root, ep_name, "conn_success_retry",
                   ep_props->conn_success_retry);

    return 0;
}

//
// Populate GC type end point object
// Returns:
// 0 on success
// <0 on failure
//
static int populate_gc_ep (char *rdb_root, char *ep_name,
                           t_end_point_gc *ep_props)
{
    int len;
    char rdb_var_name[RDB_NAME_LENGTH];

    GET_SINGLE_INT(rdb_root, ep_name, "rule_operator", ep_props->rule_operator);
    GET_SINGLE_INT(rdb_root, ep_name, "rule_negate", ep_props->rule_negate);
    GET_SINGLE_INT(rdb_root, ep_name, "rule_property", ep_props->rule_property);

    memset(ep_props->rule_value, 0, MAX_BT_IDENT_LENGTH);
    sprintf(rdb_var_name, "%s.%s.rule_value", rdb_root, ep_name);
    len = MAX_BT_IDENT_LENGTH - 1;
    if (rdb_get(g_rdb_session, rdb_var_name, ep_props->rule_value, &len) != 0)
        return -1;

    GET_SINGLE_INT(rdb_root, ep_name, "conn_fail_retry",
                   ep_props->conn_fail_retry);
    GET_SINGLE_INT(rdb_root, ep_name, "conn_success_retry",
                   ep_props->conn_success_retry);

    return 0;
}

//
// Read end points from database
//
// call with low limit 0 and upper limit -1 for reading all available end points
// Returns:
// 0 on success
// <0 on failure
static int read_end_points(char *rdb_root, int lo_limit, int high_limit, int *err_index)
{
    int i;
    int len;
    int ret = 0;
    char rdb_var_name[RDB_NAME_LENGTH];
    char name[RDB_VAL_LENGTH];
    int ep_type;

    do_limits(&lo_limit, &high_limit, MAX_END_POINTS);

    for (i = lo_limit ; i < high_limit ; i++)
    {
        // read common properties first
        *err_index = i;

        // try to read name of end point
        sprintf(rdb_var_name, "%s.%d.name", rdb_root, i);
        memset(name, 0, sizeof(name));
        len = sizeof(name) - 1;
        if (rdb_get(g_rdb_session, rdb_var_name, name, &len) != 0 ||
			len == 0 || strlen(name) == 0 )
        {
            // this is fine - simply means there are no more end points
            return 0;
        }

        sprintf(rdb_var_name, "%s.%d.type", rdb_root, i);
        if (rdb_get_int(g_rdb_session, rdb_var_name, &ep_type) != 0)
        {
            dsm_syslog(LOG_ERR, "Failed to read type of end point %s", name);
            return -1;
        }

        // validate type before allocating any memory
        if ((ep_type < EP_SERIAL) || (ep_type >= EP_MAX_NUMBER))
        {
            dsm_syslog(LOG_ERR, "Invalid end point type %d in ep number %d", ep_type, i);
            return -1;
        }

        // all looks good, we have both name and type
        g_end_points.ep[i].type = ep_type;
        g_end_points.ep[i].name = malloc(strlen(name)+1);
        strcpy(g_end_points.ep[i].name, name);

        // read specific properties dependent on type - these reside under
        // RDB entry with a matching name, e.g. they are not indexed like
        // the common properties
        // xxx.ep.conf.0.type 1
        // xxx.ep.conf.0.name EP1
        // xxx.ep.conf.EP1.bit_rate 19200

        switch (ep_type)
        {
            // a bit of poor man's C++
        case EP_SERIAL:
        case EP_RS232:
        case EP_RS485:
        case EP_RS422:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_serial));
            ret = populate_serial_ep(rdb_root, g_end_points.ep[i].name,
                                     (t_end_point_serial *)g_end_points.ep[i].specific_props);
            break;
        case EP_MODEM_EMULATOR:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_modem));
            ret = populate_modem_ep(rdb_root, g_end_points.ep[i].name,
                                     (t_end_point_modem *)g_end_points.ep[i].specific_props);
            break;
        case EP_TCP_SERVER:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_tcp_server));
            ret = populate_tcp_server_ep(rdb_root, g_end_points.ep[i].name,
                                         (t_end_point_tcp_server *)g_end_points.ep[i].specific_props);
            break;
        case EP_TCP_CLIENT:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_tcp_client));
            ret = populate_tcp_client_ep(rdb_root, g_end_points.ep[i].name,
                                         (t_end_point_tcp_client *)g_end_points.ep[i].specific_props);
            break;
        case EP_TCP_CLIENT_COD:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_tcp_client_cod));
            ret = populate_tcp_client_cod_ep(rdb_root, g_end_points.ep[i].name,
                                         (t_end_point_tcp_client_cod *)g_end_points.ep[i].specific_props);
            break;
        case EP_UDP_SERVER:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_udp_server));
            ret = populate_udp_server_ep(rdb_root, g_end_points.ep[i].name,
                                         (t_end_point_udp_server *)g_end_points.ep[i].specific_props);
            break;
        case EP_UDP_CLIENT:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_udp_client));
            ret = populate_udp_client_ep(rdb_root, g_end_points.ep[i].name,
                                         (t_end_point_udp_client *)g_end_points.ep[i].specific_props);
            break;
        case EP_GPS:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_gps));
            ret = populate_gps_ep(rdb_root, g_end_points.ep[i].name,
                                  (t_end_point_gps *)g_end_points.ep[i].specific_props);
            break;
        case EP_GENERIC_EXEC:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_exec));
            ret = populate_exe_ep(rdb_root, g_end_points.ep[i].name,
                                  (t_end_point_exec *)g_end_points.ep[i].specific_props);
            break;
        case EP_PPP:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_ppp));
            ret = populate_ppp_ep(rdb_root, g_end_points.ep[i].name,
                                  (t_end_point_ppp *)g_end_points.ep[i].specific_props);
            break;
        case EP_IP_MODEM:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_ip_modem));
            ret = populate_ip_modem_ep(rdb_root, g_end_points.ep[i].name,
                                  (t_end_point_ip_modem *)g_end_points.ep[i].specific_props);
            break;
        case EP_CSD:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_csd));
            ret = populate_csd_ep(rdb_root, g_end_points.ep[i].name,
                                  (t_end_point_csd *)g_end_points.ep[i].specific_props);
            break;

        case EP_BT_SPP:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_spp));
            ret = populate_spp_ep(rdb_root, g_end_points.ep[i].name,
                                  (t_end_point_spp *)
                                  g_end_points.ep[i].specific_props);
            break;

        case EP_BT_GC:
            g_end_points.ep[i].specific_props = malloc(sizeof(t_end_point_gc));
            ret = populate_gc_ep(rdb_root, g_end_points.ep[i].name,
                                 (t_end_point_gc *)
                                 g_end_points.ep[i].specific_props);
            break;

        default:
            dsm_syslog(LOG_ERR, "Invalid end point type %d in ep number %d", ep_type, i);
            return -1;
        }

        if (ret != 0)
        {
            dsm_syslog(LOG_ERR, "Invalid data for end point type %d in ep number %d", ep_type, i);
            return -1;
        }
    }
    return ret;
}

//
// Read data stream information from the RDB
//
// call with low limit 0 and upper limit -1 for reading all available end points
static int read_data_streams(char *rdb_root, int lo_limit, int high_limit)
{
    int len;
    int i;
    char rdb_var_name[RDB_NAME_LENGTH];
    char name[RDB_VAL_LENGTH];
    int enabled;

    do_limits(&lo_limit, &high_limit, MAX_DATA_STREAMS);

    for (i = lo_limit ; i < high_limit ; i++)
    {

        sprintf(rdb_var_name, "%s.%d.name", rdb_root, i);
        memset(name, 0, sizeof(name));
        len = sizeof(name) - 1;
        if (rdb_get(g_rdb_session, rdb_var_name, name, &len) != 0)
        {
            // no problem, simply no more streams
            return 0;
        }
        g_streams.ds[i].name = malloc(strlen(name)+1);
        strcpy(g_streams.ds[i].name, name);

        sprintf(rdb_var_name, "%s.%d.enabled", rdb_root, i);
        if (rdb_get_int(g_rdb_session, rdb_var_name, &enabled) != 0)
        {
            snprintf(g_streams.ds[i].err_msg, MAX_ERR_TEXT_LENGTH, "Failed to read enabled RDB variable");
            continue;
        }
        g_streams.ds[i].enabled = enabled;

        sprintf(rdb_var_name, "%s.%d.epa_name", rdb_root, i);
        memset(name, 0, sizeof(name));
        len = sizeof(name) - 1;
        if (rdb_get(g_rdb_session, rdb_var_name, name, &len) != 0)
        {
            snprintf(g_streams.ds[i].err_msg, MAX_ERR_TEXT_LENGTH, "Failed to read EPA name RDB variable");
            continue;
        }
        g_streams.ds[i].epa_name = malloc(strlen(name)+1);
        strcpy(g_streams.ds[i].epa_name, name);

        sprintf(rdb_var_name, "%s.%d.epb_name", rdb_root, i);
        memset(name, 0, sizeof(name));
        len = sizeof(name) - 1;
        if (rdb_get(g_rdb_session, rdb_var_name, name, &len) != 0)
        {
            snprintf(g_streams.ds[i].err_msg, MAX_ERR_TEXT_LENGTH, "Failed to read EPB name RDB variable");
            continue;
        }
        g_streams.ds[i].epb_name = malloc(strlen(name)+1);
        strcpy(g_streams.ds[i].epb_name, name);

        sprintf(rdb_var_name, "%s.%d.epa_mode", rdb_root, i);
        if (rdb_get_int(g_rdb_session, rdb_var_name, &g_streams.ds[i].epa_mode) != 0)
        {
            snprintf(g_streams.ds[i].err_msg, MAX_ERR_TEXT_LENGTH, "Failed to read EPA mode RDB variable");
            continue;
        }

        sprintf(rdb_var_name, "%s.%d.epb_mode", rdb_root, i);
        if (rdb_get_int(g_rdb_session, rdb_var_name, &g_streams.ds[i].epb_mode) != 0)
        {
            snprintf(g_streams.ds[i].err_msg, MAX_ERR_TEXT_LENGTH, "Failed to read EPB mode RDB variable");
            continue;
        }
    }

    return 0;
}

//
// C code entry point for the DSM tool
//
int main(int argc, char *argv[])
{
    // vars that can be overwritten from command line

    BOOL do_initialize = FALSE;
    BOOL do_delete = FALSE;
    BOOL do_clear = FALSE;
    BOOL do_validate = FALSE;
    BOOL do_create_cmds = FALSE;
    char *rdb_root = NULL;
    int opt;

    // parse command line arguments:
    while ((opt = getopt(argc, argv, "idCvcr:Vh")) != -1)
    {
        switch (opt)
        {
        case 'i':
            do_initialize = TRUE;
            break;

        case 'd':
            do_delete = TRUE;
            break;

        case 'C':
            do_clear = TRUE;
            break;

        case 'r':
            rdb_root = optarg;
            break;

        case 'v':
            do_validate = TRUE;
            break;

        case 'V':
            dsm_verbosity++;
            break;

        case 'c':
            do_validate = TRUE;
            do_create_cmds = TRUE;
            break;

        default:
            usage(argv);
            exit(EXIT_FAILURE);
        }
    }

    // has to either have an abs delay or RDB time in variable
    if (!rdb_root)
    {
        fprintf(stderr, "Invalid RDB root\n");
        exit(EXIT_FAILURE);
    }

    // last argument don't care?
    openlog("dsm_tool", LOG_PID, LOG_LOCAL5);
    setlogmask(LOG_UPTO(dsm_verbosity));

#if 0
    // test logging levels
    dsm_syslog(LOG_ERR, "Error");
    dsm_syslog(LOG_ERR+1, "Error+V");
    dsm_syslog(LOG_ERR+2, "Error+V+V");
    dsm_syslog(LOG_ERR+3, "Error+V+V+V");
    dsm_syslog(LOG_ERR+4, "Error+V+V+V+V"); //LOG_INFO
#endif

    // can now use syslog
    // syslog(priority, "%s", buffer);

    // Open RDB session
    if ((rdb_open(NULL, &g_rdb_session) < 0) || !g_rdb_session)
    {
        dsm_syslog(LOG_CRIT, "Failed to open RDB");
        closelog();
        return EXIT_FAILURE;
    }

    // if deleting, nothing else to do but delete RDB structures
    if (do_delete)
    {
        dsm_syslog(LOG_INFO, "Deleting rdb root %s", rdb_root);
        int ret = delete_rdb_root(rdb_root, do_clear);
        rdb_close(&g_rdb_session);
        closelog();
        return ret;
    }

    // prepare to read EP and stream data

    // 1) initialize memory and so on
    init_all();

    // 2) read all end points and data streams
    char rdb_tmp[RDB_NAME_LENGTH];
    snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.conf", rdb_root);
    int err_index = -1;
    int ret_ep = read_end_points(rdb_tmp, 0, -1, &err_index);
    snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.conf", rdb_root);
    int ret_ds = read_data_streams(rdb_tmp, 0, -1);

    // any syntax error, and we must exit
    if ((ret_ep < 0) || (ret_ds < 0))
    {
        dsm_syslog(LOG_ERR, "RDB data for end point %d invalid", err_index);

        if (do_validate)
        {
            // write EP error codes to RDB (common to all EPs)
            // so the user interface can show a reasonable message
            // to explain what happened
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.validated", rdb_root);
            rdb_update_string(g_rdb_session, rdb_tmp, "0", 0, 0);
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.error_msg", rdb_root);
            char err_text[128];
            if ((err_index != -1) && strlen(g_end_points.ep[err_index].name))
            {
                snprintf(err_text, sizeof(err_text), "End point %s data invalid, DSM disabled", g_end_points.ep[err_index].name);
            }
            else
            {
                snprintf(err_text, sizeof(err_text), "End point number %d data invalid, DSM disabled", err_index);
            }
            rdb_update_string(g_rdb_session, rdb_tmp, err_text, 0, 0);
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.used_ser_port_list", rdb_root);
            rdb_update_string(g_rdb_session, rdb_tmp, "", 0, 0);

            // clear tcp & udp server port list
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.used_tcp_port_list", rdb_root);
            rdb_update_string(g_rdb_session, rdb_tmp, "", 0, 0);
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.used_udp_port_list", rdb_root);
            rdb_update_string(g_rdb_session, rdb_tmp, "", 0, 0);
        }

        rdb_close(&g_rdb_session);
        closelog();
        delete_all();
        return EXIT_FAILURE;
    }

    // debugging only
#ifdef DEEP_DEBUG
    print_eps();
    print_streams();
#endif

    int i;

    // write dynamic RDB entries, one per stream
    if (do_initialize)
    {
        for (i = 0 ; i < MAX_DATA_STREAMS ; i++)
        {
            // an initialized stream (even if disabled)
            // has a name
            if (!g_streams.ds[i].name)
            {
                break;
            }
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.pid", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, "0", 0, 0);
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.validated", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, "0", 0, 0);
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.command_line", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, "", 0, 0);
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.error_msg", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, "", 0, 0);
        }
    }

    if (do_validate)
    {
        g_streams.configed_data_streams = 0;
        g_end_points.configed_end_points = 0;
        validate_all();

        // write EP error codes to RDB (common to all EPs)
        snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.validated", rdb_root);
        rdb_update_string(g_rdb_session, rdb_tmp, g_end_points.valid ? "1" : "0", 0, 0);
        snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.error_msg", rdb_root);
        rdb_update_string(g_rdb_session, rdb_tmp, g_end_points.err_msg, 0, 0);
        snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.used_ser_port_list", rdb_root);
        rdb_update_string(g_rdb_session, rdb_tmp, g_end_points.ser_port_list, 0, 0);

        // update variables that will be useful for punching holes in SPI firewall
        // in case if incoming connections are required in DSM
        snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.used_tcp_port_list", rdb_root);
        rdb_update_string(g_rdb_session, rdb_tmp, g_end_points.tcp_port_list, 0, 0);
        snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.ep.used_udp_port_list", rdb_root);
        rdb_update_string(g_rdb_session, rdb_tmp, g_end_points.udp_port_list, 0, 0);

        // write DS error codes to RDB (one per stream)
        for (i = 0 ; i < g_streams.configed_data_streams ; i++)
        {
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.validated", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, g_streams.ds[i].valid ? "1" : "0", 0, 0);

            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.error_msg", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, g_streams.ds[i].err_msg, 0, 0);
        }
    }

    if (do_create_cmds)
    {
        char buf[MAX_CMD_BUF_SIZE];
        int i;
        for (i = 0 ; i < g_streams.configed_data_streams ; i++)
        {
            // do not stop, but skip over invalid streams
            if (!g_streams.ds[i].valid)
            {
                continue;
            }

            // blank out the command line
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.command_line", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, "", 0, 0);

            memset(buf, 0, sizeof(buf));
            if (!generate_cmd(buf, i, MAX_CMD_BUF_SIZE))
            {
                rdb_update_string(g_rdb_session, rdb_tmp, buf, 0, 0);
            }

            //
            // Most of the time, template uses SIGKILL to kill underlying process
            // However, for modem emulator with ppp daemon it should be SIGTERM
            //
            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.kill_signo", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, kill_method_alt(i) ? "SIGTERM" : "", 0, 0);

            // generate pre and post scripts. Reuse the buffer

            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.pre_cmd", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, "", 0, 0);
            memset(buf, 0, sizeof(buf));

            if (generate_pre_post_cmd(buf, i, MAX_CMD_BUF_SIZE, FALSE) == 0)
            {
                rdb_update_string(g_rdb_session, rdb_tmp, buf, 0, 0);
            }
            // otherwise, there is no pre-init command

            snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.post_cmd", rdb_root, i);
            rdb_update_string(g_rdb_session, rdb_tmp, "", 0, 0);
            memset(buf, 0, sizeof(buf));

            if (generate_pre_post_cmd(buf, i, MAX_CMD_BUF_SIZE, TRUE) == 0)
            {
                snprintf(rdb_tmp, RDB_NAME_LENGTH, "%s.stream.%d.post_cmd", rdb_root, i);
                rdb_update_string(g_rdb_session, rdb_tmp, buf, 0, 0);
            }
            // otherwise, there is no post (cleanup) command
        }
    }

    // close rdb
    rdb_close(&g_rdb_session);

    // close log
    closelog();

    //
    // delete all memory
    // TODO - should validate with valgrind that there are no memory leaks
    // cannot validate because mock_rdb is leaking memory
    //
    delete_all();

    return (EXIT_SUCCESS);
}
