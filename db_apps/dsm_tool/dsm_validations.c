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
// Provides validation functions for data streams
// and end points

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

#define PREVENT_1_TO_N_CONNECTIONS
#define PREVENT_LOOPBACK_CONNECTIONS
#define PREVENT_INVALID_IP_CONNECTIONS

// enable this before release
#define PREVENT_SERIAL_EP_IN_N_CONNECTIONS

//
// Helper function
// Check that mode is one of modbus modes
//
BOOL is_modbus_mode(int mode)
{
    return ((mode >= EP_MODE_MODBUS_GW_RTU) && (mode <= EP_MODE_MODBUS_CLIENT_ASCII));
}

//
// Helper function
// Check that type of end point is serial (which includes RSxxx end points)
//
BOOL is_serial_ep_type(int type)
{
    return ((type == EP_SERIAL) || (type == EP_RS232) ||
            (type == EP_RS485) || (type == EP_RS422));
}

//
// Helper function
// Check that type of end point is serial (which includes RSxxx end points),
// or modem end point which is also included
//
BOOL is_serial_or_modem_ep_type(int type)
{
    return (is_serial_ep_type(type) || (type == EP_MODEM_EMULATOR));
}

//
// Helper function
// Check that type of end point is RSxxx
//
BOOL is_rsXXX_ep_type(int type)
{
    return ((type == EP_RS232) || (type == EP_RS485) || (type == EP_RS422));
}

//
// Helper function
// Check that this type of end point can be connected to Modem Emulator end point
// So it can be:
// PPP (type 12)
// IP conn (type 13)
// CSD (type 14)
BOOL is_modem_compatible_type(int type)
{
    return ((type == EP_PPP) || (type == EP_IP_MODEM) || (type == EP_CSD));
}

//
// Helper function
// Check that this type of end point is bluetooth type (spp or gc)
BOOL is_bt_ep_type(int type)
{
    return ((type == EP_BT_SPP) || (type == EP_BT_GC));
}

//
// Helper function
// Check that this type of end point can be connected to BT end point
BOOL is_bt_compatible_type(int type)
{
    return (type == EP_GENERIC_EXEC);
}

//
// Helper function
// Check that type of end point is TCP or UDP
//
BOOL is_tcp_or_udp_type(int type)
{
    return ((type == EP_TCP_SERVER) || (type == EP_TCP_CLIENT) ||
            (type == EP_UDP_SERVER) || (type == EP_UDP_CLIENT) ||
            (type == EP_TCP_CLIENT_COD));
}

//
// Helper function
// Check that type of end point is a TCP or UDP server
//
BOOL is_server_ep_type(int type)
{
    return ((type == EP_TCP_SERVER) || (type == EP_UDP_SERVER));
}

//
// Helper function
// Check that type of end point is a TCP or UDP client
//
BOOL is_client_ep_type(int type)
{
    return ((type == EP_TCP_CLIENT) || (type == EP_TCP_CLIENT_COD) || (type == EP_UDP_CLIENT));
}


//
// Helper function
// Check if mode is valid
//
BOOL is_valid_mode(int mode)
{
    return ((mode >= EP_MODE_RAW) && (mode <= EP_MODE_MODBUS_CLIENT_ASCII));
}

//
// A helper function
// Get a pointer to an end point object
// given its name (which is what data stream knows)
//
t_end_point *get_end_point_ptr(char *ep_name)
{
    int i;
    for (i = 0 ; i < MAX_END_POINTS ; i++)
    {
        if (g_end_points.ep[i].type == 0)
        {
            break;
        }

        if (strcmp(g_end_points.ep[i].name, ep_name) == 0)
        {
            return (&g_end_points.ep[i]);
        }
    }
    return NULL;
}

//
// Validate modes. Not all modes can be applied to all end points
// for example, there is no point applying modbus modes to
// non-serial end point
//
static int validate_modes(t_data_stream *p_stream)
{
    int mode_a, mode_b;
    t_end_point *epa, *epb;


    // iterate through data streams and make sure
    // that they reference valid end points

    mode_a = p_stream->epa_mode;
    mode_b = p_stream->epb_mode;

    if (!is_valid_mode(mode_a))
    {
        snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH, "Data stream has end point A in invalid mode %d",
                 mode_a);
        return -1;
    }

    if (!is_valid_mode(mode_b))
    {
        snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH, "Data stream has end point B in invalid mode %d",
                 mode_b);
        return -1;
    }

    // simplify things by not letting EPB to be in modbus mode.
    if (is_modbus_mode(mode_b))
    {
        strcpy(p_stream->err_msg, "Data stream has end point B in modbus mode");
        return -1;
    }

    epa = get_end_point_ptr(p_stream->epa_name);
    epb = get_end_point_ptr(p_stream->epb_name);
    if (!epa || !epb)
    {
        snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH, "Data stream references non-existent end point <%s>",
                 !epa ? p_stream->epa_name : p_stream->epb_name);
        return -1;
    }

    // for modbus modes, the following applies
    // 1) the end point of the data stream (a/b)
    // in modbus mode can only be serial end point
    // 2) the other side can only be TCP/UDP server
    //      for Modbus Server Gateway mode, and
    //      can only be TCP/UDP client for
    //      Modbus Client Agent mode
    // 3) The other size of the connection can only
    //      be in "raw" mode.
    if (is_modbus_mode(mode_a))
    {
        //if (is_modbus_mode(mode_b))
        //{
        //   snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH, "Data stream %d <%s> has both end points in modbus mode",
        //       i, p_stream->name);
        //    return -1;
        //}

        // check that epa is serial type
        if (!is_serial_ep_type(epa->type))
        {
            snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH,
                     "Data stream end point <%s> in modbus mode, but end point is not serial",
                     epa->name);
            return -1;
        }

        // Gateway mode - the other side must be a server
        if ((mode_a ==  EP_MODE_MODBUS_GW_RTU) || (mode_a == EP_MODE_MODBUS_GW_ASCII))
        {
            // @TODO - no point allowing Modbus via UDP if we cannot test it?
            if ((epb->type != EP_UDP_SERVER) && (epb->type != EP_TCP_SERVER))
            {
                snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH,
                         "Data stream end point <%s> in modbus mode server gateway mode, the other end point should be a server",
                         epa->name);
                return -1;
            }
        }

        // Client agent mode - the other side must be a client
        if ((mode_a ==  EP_MODE_MODBUS_CLIENT_RTU) || (mode_a == EP_MODE_MODBUS_CLIENT_ASCII))
        {
            // @TODO - no point allowing Modbus via UDP if we cannot test it?
            if ((epb->type != EP_UDP_CLIENT) && (epb->type != EP_TCP_CLIENT))
            {
                snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH,
                         "Data stream end point <%s> in modbus mode client agent mode, the other end point should be a client",
                         epa->name);
                return -1;
            }
        }
    }

    // to simplify, we do not worry about End point B being in modbus mode as we do not let this happen
    // in the HTML/Javascript code. I am not sure that socat syntax is completely symmetric in
    // respect to modbus and really do not want to go there at all.
    // Typically, validations should have performed the same algorithm on End Point B - as
    // everything is completely symmetric in respect to End point A and End Point B

    // Bluetooth endpoint (spp & gc) must be in raw mode
    if (is_bt_ep_type(epa->type) && (mode_a != EP_MODE_RAW))
    {
        snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH,
                 "Bluetooth data stream end point <%s> must have a raw mode",
                 epa->name);
        return -1;
    }
    if (is_bt_ep_type(epb->type) && (mode_b != EP_MODE_RAW))
    {
        snprintf(p_stream->err_msg, MAX_ERR_TEXT_LENGTH,
                 "Bluetooth data stream end point <%s> must have a raw mode",
                 epb->name);
        return -1;
    }

    return 0;
}

//
// Reset all "valid" flags before validation
//
static void reset_valid_flags(void)
{
    int i;
    for (i = 0 ; i < MAX_DATA_STREAMS ; i++)
    {
        g_streams.ds[i].valid = FALSE;
    }
}

//
// Make sure that the EP referenced in data streams actually exist
//
void validate_ds(int lo_limit, int high_limit)
{
    int i, j;
    int found_a, found_b;
    char *epa_name;
    char *epb_name;

    // we are often comparing two streams  in this function, so this is
    // a shorthand for code neatness
    t_data_stream *p_stream1, *p_stream2;

    reset_valid_flags();
    do_limits(&lo_limit, &high_limit, MAX_DATA_STREAMS);

    // count the number of structures, valid or not
    g_streams.configed_data_streams = 0;
    for (i = lo_limit ; i < high_limit ; i++)
    {
        p_stream1 = &g_streams.ds[i];

        // this is used to detect the last entry
        if (p_stream1->name == NULL)
        {
            break;
        }

        g_streams.configed_data_streams++;

        // 1) iterate through data streams and make sure
        // that they reference valid end points

        // optimize
        epa_name = p_stream1->epa_name;
        epb_name = p_stream1->epb_name;

        // look for names in End Point list
        found_a = 0;
        found_b = 0;
        for (j = 0 ; j < MAX_END_POINTS ; j++)
        {
            if (g_end_points.ep[j].type == 0)
            {
                break;
            }

            if (!strcmp(g_end_points.ep[j].name, epa_name))
            {
                found_a++;
                if (p_stream1->enabled)
                {
                    g_end_points.ep[j].referenced = TRUE;
                }
            }

            if (!strcmp(g_end_points.ep[j].name, epb_name))
            {
                found_b++;
                if (p_stream1->enabled)
                {
                    g_end_points.ep[j].referenced = TRUE;
                }
            }
        }

        if (found_a == 0)
        {
            snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                     "Data stream references non-existent end point A %s",
                     epa_name);
            continue;
        }

        if (found_b == 0)
        {
            snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                     "Data stream references a non-existent end point B %s",
                     epb_name);
            continue;
        }

#ifdef PREVENT_LOOPBACK_CONNECTIONS
        // 2) prevent loopbacks
        if (!strcmp(p_stream1->epa_name, p_stream1->epb_name))
        {
            snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                     "Data stream references the same end point <%s> on both ends of connection",
                     epa_name);
            continue;
        }
#endif

#ifdef PREVENT_1_TO_N_CONNECTIONS
        // 3) prevent 1:N connections, meaning that the same end point
        // cannot be referenced more than once in enabled data streams
        if (p_stream1->enabled)
        {
            BOOL do_continue = FALSE;
            for (j = lo_limit ; j < high_limit ; j++)
            {
                p_stream2 = &g_streams.ds[j];
                if (p_stream2->name == NULL)
                {
                    break;
                }

                // do not validate disabled data streams
                if (!p_stream2->enabled)
                {
                    continue;
                }

                if (i != j)
                {
                    if ((!strcmp(p_stream1->epa_name, p_stream2->epa_name)) ||
                            (!strcmp(p_stream1->epb_name, p_stream2->epb_name)) ||
                            (!strcmp(p_stream1->epa_name, p_stream2->epb_name)) ||
                            (!strcmp(p_stream1->epb_name, p_stream2->epa_name)))
                    {
                        snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                                 "Data stream end point <%s> or <%s> are referenced elsewhere (in data stream %d <%s>)",
                                 p_stream1->epa_name, p_stream1->epb_name,
                                 j, p_stream2->name);

                        // outer loop should continue without setting valid flag
                        do_continue = TRUE;
                        break;
                    }
                }
            }
            if (do_continue)
            {
                continue;
            }
        }
#endif

        t_end_point *p_epa = get_end_point_ptr(p_stream1->epa_name);
        t_end_point *p_epb = get_end_point_ptr(p_stream1->epb_name);
        //
        // 4) We allow dsm to act as a data relay between arbitrary UDP/TCP end points.
        // However, it doesn't make sense to allow client to client and server to
        // server connections
        //
#ifdef PREVENT_INVALID_IP_CONNECTIONS
        if (p_epa && p_epb)
        {
            if (is_client_ep_type(p_epa->type) && is_client_ep_type(p_epb->type))
            {
                snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                         "Data stream end points <%s> or <%s> cannot be both clients",
                         p_stream1->epa_name, p_stream1->epb_name);
                continue;
            }

            if (is_server_ep_type(p_epa->type) && is_server_ep_type(p_epb->type))
            {
                snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                         "Data stream end points <%s> or <%s> cannot be both servers",
                         p_stream1->epa_name, p_stream1->epb_name);
                continue;
            }
        }
#endif
        // 5 check client on demand is connected to serial on the other end
        if (p_epa && p_epb)
        {
            if (((p_epa->type == EP_TCP_CLIENT_COD) && !is_serial_ep_type(p_epb->type)) ||
                ((p_epb->type == EP_TCP_CLIENT_COD) && !is_serial_ep_type(p_epa->type)))
            {
                snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                        "Connect on demand client end point connected to incorrect (non-serial) end point. See stream %s",
                        p_stream1->name);
                continue;
            }
        }

        // 6 check that modem emulator is connected to a relevant EP
        if (p_epa && p_epb)
        {
            if (((p_epa->type == EP_MODEM_EMULATOR) && !is_modem_compatible_type(p_epb->type)) ||
                ((p_epb->type == EP_MODEM_EMULATOR) && !is_modem_compatible_type(p_epa->type)))
            {
                snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                        "Modem emulator end point connected to incorrect end point. See stream %s",
                        p_stream1->name);
                continue;
            }

            if (((p_epa->type != EP_MODEM_EMULATOR) && is_modem_compatible_type(p_epb->type)) ||
                ((p_epb->type != EP_MODEM_EMULATOR) && is_modem_compatible_type(p_epa->type)))
            {
                snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                        "End point of this type can only be connected to Modem Emulator. See stream %s",
                        p_stream1->name);
                continue;
            }
        }

        // 7) check that Bluetooth (spp or gc) endpoint is connected to a relevant EP
        if (p_epa && p_epb)
        {
            if ((is_bt_ep_type(p_epa->type) &&
                 !is_bt_compatible_type(p_epb->type)) ||
                (is_bt_ep_type(p_epb->type) &&
                 !is_bt_compatible_type(p_epa->type)))
            {
                snprintf(p_stream1->err_msg, MAX_ERR_TEXT_LENGTH,
                        "Bluetooth end point connected to"
                         " incorrect end point. See stream %s",
                         p_stream1->name);
                continue;
            }
        }

        // 8) check that modes are valid
        if (validate_modes(p_stream1) == 0)
        {
            dsm_syslog(LOG_INFO, "Successfully validated data stream %d <%s>", i, p_stream1->name);
            p_stream1->valid = TRUE;
            strcpy(p_stream1->err_msg, "Ok");
        }
    }
}

// a helper to add a port number to the list of ports
static void add_port_no_to_str(int port_no, char *str, int max_len)
{
    // port number is limited to 65535 so port is 5 char, plus space plus terminator=7 bytes
    char port_no_text[7];

    // space separated string
    snprintf(port_no_text, sizeof(port_no_text), "%d ", port_no);

    if ((strlen(str) + strlen(port_no_text) + 1) < max_len)
    {
        strcat(str, port_no_text);
    }
}

//
// Make sure that in case of serial end points
// the same serial device is NOT used more than once.
// If end point is not referenced in any data stream, then
// it is ignored even if serial port is used more than once.
//
// Also, in case of TCP and UDP server end points, make sure
// that the same port number is NOT used more than once.
// Note - TCP and UDP can use the same port with no conflicts
//
int validate_ep(int lo_limit, int high_limit)
{
    int i, j;
    do_limits(&lo_limit, &high_limit, MAX_END_POINTS);
    t_end_point *p_ep1, *p_ep2;
    t_end_point_ip_modem *p_ip_modem;

    g_end_points.valid = FALSE;
    strcpy(g_end_points.err_msg, "No referenced end points found");
    g_end_points.configed_end_points = 0;
    memset(g_end_points.ser_port_list, 0, sizeof(g_end_points.ser_port_list));
    memset(g_end_points.tcp_port_list, 0, sizeof(g_end_points.tcp_port_list));
    memset(g_end_points.udp_port_list, 0, sizeof(g_end_points.udp_port_list));
    for (i = lo_limit ; i < high_limit ; i++)
    {
        p_ep1 = &g_end_points.ep[i];
        if (p_ep1->type == 0)
        {
            break;
        }

        // do not worry about unreferenced end points
        if (!p_ep1->referenced)
        {
            dsm_syslog(LOG_INFO, "End point %d <%s> is not referenced", i, p_ep1->name);
            continue;
        }

        g_end_points.configed_end_points++;

        for (j = lo_limit ; j < high_limit ; j++)
        {
            p_ep2 = &g_end_points.ep[j];
            // no more entries in the table
            if (p_ep2->type == 0)
            {
                break;
            }

            // do not worry about unreferenced end points
            if (!p_ep2->referenced)
            {
                continue;
            }

            // check for name uniqueness
            if ((i!=j) && (!strcmp(p_ep1->name, p_ep2->name)))
            {
                snprintf(g_end_points.err_msg, MAX_ERR_TEXT_LENGTH,
                         "Two end points, index %d and %d, have the same name %s. DSM disabled", i, j, p_ep2->name);
                return -1;
            }

            //
            // EPs referenced in data stream, should not
            // reference the same resource (e.g. server port or serial port name)
            //
            if ((i!=j) && (p_ep1->type == EP_TCP_SERVER) && (p_ep2->type == EP_TCP_SERVER))
            {
                if (((t_end_point_tcp_server *)p_ep1->specific_props)->port_number ==
                        ((t_end_point_tcp_server *)p_ep2->specific_props)->port_number)
                {
                    snprintf(g_end_points.err_msg, MAX_ERR_TEXT_LENGTH,
                             "Two TCP server end points, %s and %s, use the same TCP listening port %d. DSM disabled",
                             p_ep1->name, p_ep2->name,
                             ((t_end_point_tcp_server *)p_ep2->specific_props)->port_number);
                    return -1;
                }
            }

            if ((i!=j) && (p_ep1->type == EP_UDP_SERVER) && (p_ep2->type == EP_UDP_SERVER))
            {
                if (((t_end_point_udp_server *)p_ep1->specific_props)->port_number ==
                        ((t_end_point_udp_server *)p_ep2->specific_props)->port_number)
                {
                    snprintf(g_end_points.err_msg, MAX_ERR_TEXT_LENGTH,
                             "Two UDP server end points, %s and %s, use the same UDP listening port %d. DSM disabled",
                             p_ep1->name, p_ep2->name,
                             ((t_end_point_udp_server *)p_ep2->specific_props)->port_number);
                    return -1;
                }
            }

            // IP modem checks. If local port is enabled, depending on UDP/TCP option
            // this may conflict with other end point server port
            if ((i!=j) && (p_ep1->type == EP_IP_MODEM))
            {
                p_ip_modem = (t_end_point_ip_modem *)p_ep1->specific_props;
                if (p_ip_modem->port_local)
                {
                    if (p_ip_modem->is_udp)
                    {
                        if (p_ep2->type == EP_UDP_SERVER)
                        {
                            if (p_ip_modem->port_local == ((t_end_point_udp_server *)p_ep2->specific_props)->port_number)
                            {
                                snprintf(g_end_points.err_msg, MAX_ERR_TEXT_LENGTH,
                                    "Two end points, %s and %s, use the same UDP listening port %d. DSM disabled",
                                    p_ep1->name, p_ep2->name, p_ip_modem->port_local);
                                return -1;
                            }
                        }
                    }
                    else
                    {
                        if (p_ep2->type == EP_TCP_SERVER)
                        {
                            if (p_ip_modem->port_local == ((t_end_point_tcp_server *)p_ep2->specific_props)->port_number)
                            {
                                snprintf(g_end_points.err_msg, MAX_ERR_TEXT_LENGTH,
                                    "Two end points, %s and %s, use the same TCP listening port %d. DSM disabled",
                                    p_ep1->name, p_ep2->name, p_ip_modem->port_local);
                                return -1;
                            }
                        }
                    }
                }
            }

#ifdef PREVENT_SERIAL_EP_IN_N_CONNECTIONS
            if ((i!=j) && is_serial_or_modem_ep_type(p_ep1->type) && is_serial_or_modem_ep_type(p_ep2->type))
            {
                if (!strcmp(((t_end_point_serial *)p_ep1->specific_props)->dev_name,
                            ((t_end_point_serial *)p_ep2->specific_props)->dev_name))
                {
                    snprintf(g_end_points.err_msg, MAX_ERR_TEXT_LENGTH,
                             "Two serial end points, %s and %s, use the same serial port name %s. DSM disabled",
                             p_ep1->name, p_ep2->name,
                             ((t_end_point_serial *)p_ep2->specific_props)->dev_name);
                    return -1;
                }
            }
#endif
        }

        // add a serial device name to a list of used serial points
        if (is_serial_or_modem_ep_type(p_ep1->type))
        {
            //
            // if we get here, this end point is referenced in an enabled data stream
            // add it to the list - this is simply a service we do so template's job
            // becomes easier. It needs to turn off all services (padd, modem emulator)
            // whose ports become taken up be DSM
            //
            char *dev_name = (p_ep1->type == EP_MODEM_EMULATOR) ?
                ((t_end_point_modem *)p_ep1->specific_props)->serial.dev_name :
                ((t_end_point_serial *)p_ep1->specific_props)->dev_name;
            // +2 one for comma, one for null terminator
            if ((strlen(g_end_points.ser_port_list) + strlen(dev_name) + 2) < sizeof(g_end_points.ser_port_list))
            {
                strcat(g_end_points.ser_port_list, dev_name);
                strcat(g_end_points.ser_port_list, ",");
                //dsm_syslog(LOG_INFO, "Debugging: serial port list after iter %d %s", i, g_end_points.ser_port_list);
            }
        }
        else if (p_ep1->type == EP_TCP_SERVER)
        {
            add_port_no_to_str(((t_end_point_tcp_server *)p_ep1->specific_props)->port_number,
                               g_end_points.tcp_port_list, sizeof(g_end_points.tcp_port_list));
        }
        else if (p_ep1->type == EP_UDP_SERVER)
        {
            add_port_no_to_str(((t_end_point_udp_server *)p_ep1->specific_props)->port_number,
                               g_end_points.udp_port_list, sizeof(g_end_points.udp_port_list));
        }
        else if (p_ep1->type == EP_IP_MODEM)
        {
            // IP modem server mode will also need to be added to the list of ports for firewall hole punching
            p_ip_modem = (t_end_point_ip_modem *)p_ep1->specific_props;
            if (p_ip_modem->port_local)
            {
                if (p_ip_modem->is_udp)
                    add_port_no_to_str(p_ip_modem->port_local, g_end_points.udp_port_list, sizeof(g_end_points.udp_port_list));
                else
                    add_port_no_to_str(p_ip_modem->port_local, g_end_points.tcp_port_list, sizeof(g_end_points.tcp_port_list));
            }
        }
        // otherwise nothing to do for other end point types

        dsm_syslog(LOG_INFO, "Successfully validated end point %d <%s>", i, p_ep1->name);
    }

    if (g_end_points.configed_end_points)
    {
        strcpy (g_end_points.err_msg, "Ok");
    }
    g_end_points.valid = TRUE;
    return 0;
}

int validate_all(void)
{
    // validate ds first as it sets "referenced" flag on eps
    validate_ds(0, -1);

    // validate EPs
    if (validate_ep(0, -1) != 0)
    {
        dsm_syslog(LOG_ERR, "Failed to validate end points, error %s", g_end_points.err_msg);
        return -1;
    }

    return 0;
}
