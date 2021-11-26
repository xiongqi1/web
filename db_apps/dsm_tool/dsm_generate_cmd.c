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
// A part of the data stream manager tool (dsm_tool)
//
// Provides a single function
// for generaton of command line for invoking DSM-associated processes

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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "dsm_tool.h"
#include "rdb_ops.h"

#ifdef DEEP_DEBUG
#define CMD_LINE_ARG_0 "/bin/socat -d -d -d"
#else
#define CMD_LINE_ARG_0 "/bin/socat"
#endif

#define MODEM_EMULATOR_EXE "modem_emul_ep"
#define DSM_DATA_MOVER_EXE "dsm_data_mover"
#define DSM_BLUETOOTH_EXE "dsm_bluetooth_ep.sh"
#define USE_WRAPPER_FOR_CLIENT_CONNECTIONS

// approximately half of maximum command buffer size
#define MAX_SUBSTRING_LEN   (MAX_CMD_BUF_SIZE/2)

//
// Socat has a quirk with syntax of EXEC address.
// Command line should look like:
// /usr/bin/socat TCP-LISTEN:5000,max-children=1,fork,reuseaddr EXEC:'command arg1 arg2 arg3'
// We generate this command in RDB
// However, reading the command from the RDB and calling it in the template as:
// $cmd &
// doesn't work as quotes gets stripped and socat treats it as which is a wrong syntax:
// /usr/bin/socat TCP-LISTEN:5000,max-children=1,fork,reuseaddr EXEC:command arg1 arg2 arg3
//
// Therefore, just like in GPS end point, we generate a temp shell script (e.g. script_name.sh) containing:
// command arg1 agr2 arg3
// And then we generate the following syntax for socat command:
//
// /usr/bin/socat TCP-LISTEN:5000,max-children=1,fork,reuseaddr EXEC:/tmp/script_name.sh
static int generate_temp_file(char *file_name, int stream_number, char *cmd)
{
    // create a temp file, overwrite if necessary
    FILE *streamOut = fopen(file_name, "w");
    if (!streamOut)
    {
        return -1;
    }

    // make sure newly created temp file has appropriate permissions
    int nMode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    int fno = fileno(streamOut);
    if (fchmod(fno, nMode) < 0)
    {
        return -1;
    }

    // generate command line after socat's EXEC statement here
    fprintf(streamOut, "#!/bin/sh\n# Data stream manager stream %d auto generated script for socat's EXEC statement\n# \n", stream_number);
    fprintf(streamOut, "%s", cmd);

    // close the temp file
    fclose(streamOut);

    return 0;
}

// caller must allocate a buffer large enough for additional
// characters to fit. The buf has to be NULL terminated
static int add_parity_str(char *buf, int parity)
{
    //0-no parity, 1-even, 2-odd
    switch (parity)
    {
        case 1:
            strcat(buf, ",parenb=1,parodd=0");
            break;
        case 2:
            strcat(buf, ",parenb=1,parodd=1");
            break;
        default:
            // important! socat doesn't seem to default to "no parity"
            strcat(buf, ",parenb=0,parodd=0");
    }
    return 0;
}

// caller must allocate a buffer large enough for additional
// characters to fit. The buf has to be NULL terminated
// This is in stty format
static int add_parity_str_stty(char *buf, int parity)
{
    //0-no parity, 1-even, 2-odd
    switch (parity)
    {
        case 1:
            strcat(buf, " parenb -parodd");
            break;
        case 2:
            strcat(buf, " parenb parodd");
            break;
        default:
            strcat(buf, " -parenb -parodd");
    }
    return 0;
}


// Description:
//
// Creates a correct command line for launching
// a data stream process.
//
// For example, if 2 end points are connected
// by this data stream, it will generate correct
// command line syntax to launch a process (socat, modem_emul_ep or dsm_data_mover)
// that will manage the data stream.
// The end result may be
// /usr/bin/socat SOMEADDR1 SOMEADDR2
//
// It does not launch the process (template does!).
//
// Arguments:
// command buffer is allocated by the caller
// ds_index is the index into array of data streams
//
// The function assumes that all objects have been
// already read from the rdb
//
// Note that the ds has to be validated for
// this function to generate the command line
//

int generate_cmd(char *cmd_buf, int ds_index, int cmd_buf_size)
{
    // sanity check
    if ((ds_index >= MAX_DATA_STREAMS) || (ds_index < 0) || !cmd_buf)
    {
        return -1;
    }

    // shorthand pointer
    t_data_stream *p_stream = &g_streams.ds[ds_index];

    if (!p_stream->valid)
    {
        return -1;
    }

    // get pointers to end points
    t_end_point *p_epa = get_end_point_ptr(p_stream->epa_name);
    t_end_point *p_epb = get_end_point_ptr(p_stream->epb_name);

    // should not happen as all has been validated
    if (!p_epa || !p_epb)
    {
        return -1;
    }

    //
    // Handle cases where socat is NOT underlying process for data stream.
    // Firstly, check if the modem emulator end point process should handle this data stream.
    // Syntax generation is easy because modem emulator will read RDB data for end points
    // and do the validation itself.
    // The command line is as simple as EXE_NAME EPA_RDB_NAME EPA_TYPE EPA_MODE EPB_RDB_NAME EPB_TYPE EPB_MODE
    // for example: modem_emul_ep service.dsm.ep.conf.my_modem 11 0 service.dsm.ep.conf.my_ppp 12 0
    // We want modem emulator to be the end point A and ppp/others to be the end point B.
    //
    if ((p_epa->type == EP_MODEM_EMULATOR) && is_modem_compatible_type(p_epb->type))
    {
        snprintf(cmd_buf, cmd_buf_size, "%s service.dsm.ep.conf.%s %d %d service.dsm.ep.conf.%s %d %d",
                 MODEM_EMULATOR_EXE, p_epa->name, p_epa->type, p_stream->epa_mode, p_epb->name, p_epb->type, p_stream->epb_mode);
        return 0;
    }
    else if ((p_epb->type == EP_MODEM_EMULATOR) && is_modem_compatible_type(p_epa->type)) // same as above, but end points swapped
    {
        snprintf(cmd_buf, cmd_buf_size, "%s service.dsm.ep.conf.%s %d %d service.dsm.ep.conf.%s %d %d",
                 MODEM_EMULATOR_EXE, p_epb->name, p_epb->type, p_stream->epb_mode, p_epa->name, p_epa->type, p_stream->epa_mode);
        return 0;
    }
    else if ((p_epb->type == EP_TCP_CLIENT_COD) && is_serial_ep_type(p_epa->type))
    {
        snprintf(cmd_buf, cmd_buf_size, "%s service.dsm.ep.conf.%s %d %d service.dsm.ep.conf.%s %d %d",
                 DSM_DATA_MOVER_EXE, p_epa->name, p_epa->type, p_stream->epa_mode, p_epb->name, p_epb->type, p_stream->epb_mode);
        return 0;
    }
    else if ((p_epa->type == EP_TCP_CLIENT_COD) && is_serial_ep_type(p_epb->type)) // same as above case, but end points swapped
    {
        snprintf(cmd_buf, cmd_buf_size, "%s service.dsm.ep.conf.%s %d %d service.dsm.ep.conf.%s %d %d",
                 DSM_DATA_MOVER_EXE, p_epb->name, p_epb->type, p_stream->epb_mode, p_epa->name, p_epa->type, p_stream->epa_mode);
        return 0;
    }
    else if ((p_epa->type == EP_BT_SPP || p_epa->type == EP_BT_GC) &&
             (p_epb->type == EP_GENERIC_EXEC))
    {
        snprintf(cmd_buf, cmd_buf_size,
                 "%s service.dsm.ep.conf.%s %d service.dsm.ep.conf.%s %d",
                 DSM_BLUETOOTH_EXE, p_epa->name, p_epa->type, p_epb->name,
                 p_epb->type);
        return 0;
    }
    else if ((p_epb->type == EP_BT_SPP || p_epb->type == EP_BT_GC) &&
             (p_epa->type == EP_GENERIC_EXEC))
    {
        /* same as above case, but end points swapped */
        snprintf(cmd_buf, cmd_buf_size,
                 "%s service.dsm.ep.conf.%s %d service.dsm.ep.conf.%s %d",
                 DSM_BLUETOOTH_EXE, p_epb->name, p_epb->type, p_epa->name,
                 p_epa->type);
        return 0;
    }
    else if (is_modbus_mode(p_stream->epa_mode))
    {
        // dsm_data_mover will handle modbus mode
        if (is_serial_ep_type(p_epa->type) && ((p_epb->type == EP_TCP_SERVER) || (p_epb->type == EP_TCP_CLIENT)))
        {
            snprintf(cmd_buf, cmd_buf_size, "%s service.dsm.ep.conf.%s %d %d service.dsm.ep.conf.%s %d %d",
                 DSM_DATA_MOVER_EXE, p_epa->name, p_epa->type, p_stream->epa_mode, p_epb->name, p_epb->type, p_stream->epb_mode);
            return 0;
        } 
        else if (is_serial_ep_type(p_epb->type) && ((p_epa->type == EP_TCP_SERVER) || (p_epa->type == EP_TCP_CLIENT)))
        {
            snprintf(cmd_buf, cmd_buf_size, "%s service.dsm.ep.conf.%s %d %d service.dsm.ep.conf.%s %d %d",
                 DSM_DATA_MOVER_EXE, p_epb->name, p_epb->type, p_stream->epb_mode, p_epa->name, p_epa->type, p_stream->epa_mode);
            return 0;
        }
        dsm_syslog(LOG_ERR, "Invalid end point types for modbus stream %s", p_stream->name);
        return -1;
    }

    // Every other type of stream is handled by socat. So here is socat section
    {
        // command line consists of process name (socat), followed
        // by left and right addresses
        char *left_side = malloc(MAX_SUBSTRING_LEN);
        char *right_side = malloc(MAX_SUBSTRING_LEN);
        char *p;

        if (!left_side || !right_side)
        {
            dsm_syslog(LOG_ERR, "Malloc failed");
            return -1;
        }

        memset(left_side, 0, MAX_SUBSTRING_LEN);
        memset(right_side, 0, MAX_SUBSTRING_LEN);

        // added to command line, with a comma, if we want socat to fork processes
        char fork_opt[6] = {'f', 'o', 'r', 'k', ',', 0};

        // due to specific socat behaviour when connecting client to server,
        // we cannot use fork, for example we want
        // /usr/bin/socat TCP:192.168.20.134:10000 TCP-LISTEN:10001,reuseaddr
        // Is NOT applicable to socat modes
        // Allow client to server connections (client to client and server to server
        // are not allowed
        if ((is_server_ep_type(p_epa->type) && is_client_ep_type(p_epb->type)) ||
            (is_server_ep_type(p_epb->type) && is_client_ep_type(p_epa->type)))
        {
            fork_opt[0] = 0;
        }

        //
        // in some cases (where we are clients connecting to the server), we want to retry
        // the connection if the server is not available or shuts down
        //
        int timeout[2] = {0, 0};
        char max_children_opt[32];
        char tmp_file_name[128];
        int swap_leftright = 0; // to swap left and right endpoint

        //
        // everything is completely symmetric - so the same code can do both end point A and B syntax.
        // (in socat terminology, "address A" and "address B")
        //
        int iter;
        for (iter = 0; iter < 2 ; iter++)
        {
            t_end_point *p_ep;
            char *buf;

            if (iter == 0)
            {
                p_ep = p_epa;
                buf = left_side;
            }
            else
            {
                p_ep = p_epb;
                buf = right_side;
            }

            t_end_point_serial *p_ser;
            t_end_point_tcp_server *p_tcp_srv;
            t_end_point_tcp_client *p_tcp_client;
            t_end_point_udp_server *p_udp_srv;
            t_end_point_udp_client *p_udp_client;
            t_end_point_exec *p_exe;
            switch (p_ep->type)
            {
            case EP_SERIAL:
            case EP_RS232:
            case EP_RS485:
            case EP_RS422:
                p_ser = (t_end_point_serial *)p_ep->specific_props;

                snprintf(buf, MAX_SUBSTRING_LEN, "%s,raw,b%d,cs%d,cstopb=%d,echo=0",
                             p_ser->dev_name,p_ser->bit_rate, p_ser->data_bits, p_ser->stop_bits-1);

                add_parity_str(buf, p_ser->parity);
                break;

            case EP_TCP_SERVER:
                p_tcp_srv = (t_end_point_tcp_server *)p_ep->specific_props;
                // cannot use max-children socat option without fork
                if (fork_opt[0]==0)
                {
                    max_children_opt[0]=0;
                }
                else
                {
                    snprintf(max_children_opt, sizeof(max_children_opt), "max-children=%d,", p_tcp_srv->max_children);
                }
                if (p_tcp_srv->keep_alive)
                {
                    snprintf(buf, MAX_SUBSTRING_LEN,
                             "TCP-LISTEN:%d,keepalive,keepintvl=%d,keepidle=%d,keepcnt=%d,%s%sreuseaddr",
                             p_tcp_srv->port_number,
                             p_tcp_srv->keepintvl,
                             p_tcp_srv->keepidle,
                             p_tcp_srv->keepcnt,
                             max_children_opt,
                             fork_opt);
                }
                else
                {
                    snprintf(buf, MAX_SUBSTRING_LEN, "TCP-LISTEN:%d,%s%sreuseaddr",
                             p_tcp_srv->port_number,
                             max_children_opt,
                             fork_opt);
                }
                break;

            case EP_TCP_CLIENT:
                p_tcp_client = (t_end_point_tcp_client *)p_ep->specific_props;

                if (p_tcp_client->keep_alive)
                {
                    snprintf(buf, MAX_SUBSTRING_LEN, "TCP:%s:%d,keepalive,keepintvl=%d,keepidle=%d,keepcnt=%d",
                             p_tcp_client->ip_address,
                             p_tcp_client->port_number,
                             p_tcp_client->keepintvl,
                             p_tcp_client->keepidle,
                             p_tcp_client->keepcnt);
                }
                else
                {
                    snprintf(buf, MAX_SUBSTRING_LEN, "TCP:%s:%d",
                             p_tcp_client->ip_address,
                             p_tcp_client->port_number);
                }
                timeout[iter] = p_tcp_client->timeout;
                break;

            case EP_UDP_SERVER:
                // for some reason, UDP-LISTEN doesn't work but UDP4-LISTEN at least starts socat process
                p_udp_srv = (t_end_point_udp_server *)p_ep->specific_props;
                if (fork_opt[0] == 0)
                {
                    snprintf(buf, MAX_SUBSTRING_LEN, "UDP4-LISTEN:%d,reuseaddr", p_udp_srv->port_number);
                }
                else
                {
#ifdef UDP_LISTEN_MAX_CHILDREN
                    snprintf(buf, MAX_SUBSTRING_LEN, "UDP4-LISTEN:%d,max-children=%d,%sreuseaddr",
                             p_udp_srv->port_number, p_udp_srv->max_children, fork_opt);
#else
                    snprintf(buf, MAX_SUBSTRING_LEN, "UDP4-LISTEN:%d,%sreuseaddr",
                             p_udp_srv->port_number, fork_opt);
#endif
                }
                break;

            case EP_UDP_CLIENT:
                p_udp_client = (t_end_point_udp_client *)p_ep->specific_props;
                snprintf(buf, MAX_SUBSTRING_LEN, "UDP:%s:%d", p_udp_client->ip_address, p_udp_client->port_number);
                timeout[iter] = p_udp_client->timeout;
                break;

            case EP_GPS:
                // to avoid having quotation marks and hence problems with executing it in the shell
                // dsm_gps_cmd.sh has whatever is needed to launch GPS data process
                // at this time it is simply gpspipe -r
                // Raw mode property of this endpoint is ignored (raw mode is always on)
                strcpy(buf,"EXEC:dsm_gps_cmd.sh");

                // swap if GPS is not in the right address - GPS has to be the right address for multiple connection of socat
                swap_leftright = (iter != 1);
                break;

            case EP_GENERIC_EXEC:
                p_exe = (t_end_point_exec *)p_ep->specific_props;
                sprintf(tmp_file_name, "/tmp/%d_dsm.sh", ds_index);
                if (generate_temp_file(tmp_file_name, ds_index, p_exe->exec_name) != 0)
                {
                    dsm_syslog(LOG_ERR, "Failed to generate temp file %s for stream %s", tmp_file_name, p_stream->name);
                    free (left_side);
                    free (right_side);
                    return -1;
                }
                snprintf(buf, MAX_SUBSTRING_LEN, "EXEC:%s", tmp_file_name);
                break;

            default:
                // error
                dsm_syslog(LOG_ERR, "Invalid end point type for stream %s", p_stream->name);
                free (left_side);
                free (right_side);
                return -1;
            }
        }

#ifndef USE_WRAPPER_FOR_CLIENT_CONNECTIONS
        timeout[0] = 0;
        timeout[1] = 0;
#endif

        // use larger timeout of the two end points
        int max_timeout = (timeout[0] >= timeout[1]) ? timeout[0] : timeout[1];

        // swap left and right
        if (swap_leftright)
        {
            dsm_syslog(LOG_INFO, "swap positions (%s,%s) -> (%s,%s)", left_side, right_side, right_side, left_side);

            p = left_side;
            left_side = right_side;
            right_side = p;
        }

        if (max_timeout)
        {
            // the socat will be started through the restart wrapper
            snprintf(cmd_buf, cmd_buf_size, "restart_script.sh %d %s %s %s", max_timeout, CMD_LINE_ARG_0, left_side, right_side);
        }
        else
        {
            // run socat directly, without a wrapper
            snprintf(cmd_buf, cmd_buf_size, "%s %s %s", CMD_LINE_ARG_0, left_side, right_side);
        }

        free (left_side);
        free (right_side);
    }   // end of socat syntax generator block

    return 0;
}


// Description:
//
// Generates a command that runs before (is_post = FALSE) or after
// (is_post = TRUE) the data stream is launched.
// At the moment, the only need for this arises when RS422/RS485
// end points are used on a multi-mode serial port to switch it
// to correct mode before launching socat
//
// If 0 is returned, the caller can write contents of cmd_buf
// to an RDB or whatever else it pleases
//
int generate_pre_post_cmd(char *cmd_buf, int ds_index, int cmd_buf_size, BOOL is_post)
{
    // sanity check
    if ((ds_index >= MAX_DATA_STREAMS) || (ds_index < 0) || !cmd_buf)
    {
        return -1;
    }

    // shorthand pointer
    t_data_stream *p_stream = &g_streams.ds[ds_index];

    if (!p_stream->valid)
    {
        return -1;
    }

    // get pointers to end points
    t_end_point *p_epa = get_end_point_ptr(p_stream->epa_name);
    t_end_point *p_epb = get_end_point_ptr(p_stream->epb_name);

    // should not happen as all has been validated
    if (!p_epa || !p_epb)
    {
        return -1;
    }

    if (is_rsXXX_ep_type(p_epa->type) && is_rsXXX_ep_type(p_epb->type))
    {
        // both sides cannot be in rs modes
        return -1;
    }

    //
    // Socat bug workaround.
    //
    // The problem is that in serial-server modes, socat tries to restore the
    // serial line setting that existed before the main socat process started at the wrong point in time.
    // This point in time is when client disconnects. So if the client re-connects again, the
    // line may be in the wrong state (e.g. in the state which the UART was before the main socat process
    // started).
    //
    // DSM has no knowledge when clients come and go as everything is handled by socat. So the possible fixes are:
    //
    // 1) Patch socat to avoid restoring the UART but this proves problematic due to complexity of socat
    // 2) Use dsm_data_mover instead of socat - best solution except it doesn't handle more than one client connection.
    // 3) (Chosen) To issue a stty command before the main socat process starts, using the existing pre_cmd
    //      rdb variable. In some modes, pre_cmd will contains 2 separate commands, comma separated.
    //
    t_end_point_serial *p_ser = NULL;
    if (is_serial_ep_type(p_epa->type) && is_server_ep_type(p_epb->type))
    {
        p_ser = (t_end_point_serial *)p_epa->specific_props;
    }
    else if (is_serial_ep_type(p_epb->type) && is_server_ep_type(p_epa->type))
    {
        p_ser = (t_end_point_serial *)p_epb->specific_props;
    }
    if (is_modbus_mode(p_stream->epa_mode) || is_post)
    {
        p_ser = NULL;
    }
    char stty_str[128]; // a buffer to generate an additional, semi-colon separated command (stty) if socat is in server-serial mode.
    memset(stty_str, 0, sizeof(stty_str));
    if (p_ser) // if p_ser is NULL, leave stty_str containing zeros
    {
        snprintf(stty_str, sizeof(stty_str), ";stty -F %s raw speed %d cs%d %s -echo", p_ser->dev_name, p_ser->bit_rate,
                p_ser->data_bits, (p_ser->stop_bits - 1) ? "cstopb" : "-cstopb");
        add_parity_str_stty(stty_str, p_ser->parity);
    }
    // end of main part of socat fix

    if ((p_epa->type == EP_RS485) || (p_epb->type == EP_RS485))
    {
        snprintf(cmd_buf, cmd_buf_size, "sys -r %s%s", is_post ? "rs232" : "rs485", stty_str);
        return 0;
    }
    else if ((p_epa->type == EP_RS422) || (p_epb->type == EP_RS422))
    {
        snprintf(cmd_buf, cmd_buf_size, "sys -r %s%s", is_post ? "rs232" : "rs422", stty_str);
        return 0;
    }
    else if ((p_epa->type == EP_RS232) || (p_epb->type == EP_RS232))
    {
        // switch to RS232 in case if it wasn't already RS232
        // on cleanup doesn't hurt to re-assert the mode
        snprintf(cmd_buf, cmd_buf_size, "sys -r rs232%s", stty_str);
        return 0;
    }
    else if (stty_str[0]) // this is only TRUE for socat fix.
    {
        // skip the first character (;)
        strncpy(cmd_buf, &stty_str[1], sizeof(stty_str)-1);
        return 0;
    }

    return 1;
}

// Description:
//
// For modem emulator, we need to kill it using SIGTERM command
// so that ppp daemon can be cleanly killed
// It is the same for BT_SPP & BT_GC data streams
BOOL kill_method_alt(int ds_index)
{
    // sanity check
    if ((ds_index >= MAX_DATA_STREAMS) || (ds_index < 0))
    {
        return FALSE;
    }

    // shorthand pointer
    t_data_stream *p_stream = &g_streams.ds[ds_index];

    if (!p_stream->valid)
    {
        return FALSE;
    }

    // get pointers to end points
    t_end_point *p_epa = get_end_point_ptr(p_stream->epa_name);
    t_end_point *p_epb = get_end_point_ptr(p_stream->epb_name);

    // should not happen as all has been validated
    if (!p_epa || !p_epb)
    {
        return FALSE;
    }

    if (((p_epa->type == EP_MODEM_EMULATOR) && is_modem_compatible_type(p_epb->type)) ||
        ((p_epb->type == EP_MODEM_EMULATOR) && is_modem_compatible_type(p_epa->type)))
    {
        return TRUE;
    }

    if ((is_bt_ep_type(p_epa->type) && is_bt_compatible_type(p_epb->type)) ||
        (is_bt_ep_type(p_epb->type) && is_bt_compatible_type(p_epa->type))) {
        return TRUE;
    }

    return FALSE;
}
