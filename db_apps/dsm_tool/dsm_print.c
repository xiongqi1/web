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
// Provides some debug print functions (not included in the release build)
//

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

#ifdef DEEP_DEBUG
void print_streams(void)
{
    int i;
    for (i = 0 ; i < MAX_DATA_STREAMS ; i++)
    {
        if (g_streams.ds[i].name)
        {
            printf("Data stream %d, enabled %d, name %s, EPA name %s, EPB name %s, EPA mode %d, EPB mode %d, Validated %d, Msg %s\n",
                   i, g_streams.ds[i].enabled, g_streams.ds[i].name, g_streams.ds[i].epa_name,
                   g_streams.ds[i].epb_name, g_streams.ds[i].epa_mode, g_streams.ds[i].epb_mode,
                   g_streams.ds[i].valid, g_streams.ds[i].err_msg);

        }
        else
        {
            // stop at the very first empty stream
            return;
        }
    }
}

void print_eps(void)
{
    int i;
    t_end_point_modem *modem_ep;
    t_end_point_ip_modem *ip_modem_ep;
    t_end_point_serial *serial_ep;
    t_end_point_tcp_server *tcp_server_ep;
    t_end_point_tcp_client *tcp_client_ep;
    t_end_point_tcp_client_cod *tcp_client_ep_cod;
    t_end_point_spp *bt_spp_ep;
    t_end_point_gc *bt_gc_ep;

    for (i = 0 ; i < MAX_END_POINTS ; i++)
    {
        if (g_end_points.ep[i].type)
        {
            printf("End point %d, type %d, name %s", i, g_end_points.ep[i].type, g_end_points.ep[i].name);
            switch (g_end_points.ep[i].type)
            {
            case EP_SERIAL:
            case EP_RS232:
            case EP_RS485:
            case EP_RS422:
                serial_ep = (t_end_point_serial *)(g_end_points.ep[i].specific_props);
                printf(", serial port name %s, parity %d, data bits %d, stop bits %d, bit rate %d \n",
                       serial_ep->dev_name,
                       serial_ep->parity,
                       serial_ep->data_bits,
                       serial_ep->stop_bits,
                       serial_ep->bit_rate
                      );
                break;

            case EP_MODEM_EMULATOR:
                modem_ep = (t_end_point_modem *)(g_end_points.ep[i].specific_props);
                printf(", serial port name %s, parity %d, data bits %d, stop bits %d, bit rate %d"
                       ",opt1 %02x, dtr %d, dsr %d, sw fc %d, hw fc %d, dcd %d, ri %d, autorings %d\n",
                       modem_ep->serial.dev_name,
                       modem_ep->serial.parity,
                       modem_ep->serial.data_bits,
                       modem_ep->serial.stop_bits,
                       modem_ep->serial.bit_rate,
                       modem_ep->opt_1,
                       modem_ep->opt_dtr,
                       modem_ep->opt_dsr,
                       modem_ep->opt_sw_fc,
                       modem_ep->opt_hw_fc,
                       modem_ep->opt_dcd,
                       modem_ep->opt_ri,
                       modem_ep->opt_modem_auto_answer
                      );
                break;

            case  EP_TCP_SERVER:
                tcp_server_ep = (t_end_point_tcp_server *)(g_end_points.ep[i].specific_props);
                printf(", tcp server port %d, keep alive options %d (%d, %d, %d), max_children %d\n",
                       tcp_server_ep->port_number,
                       tcp_server_ep->keep_alive,
                       tcp_server_ep->keepcnt,
                       tcp_server_ep->keepidle,
                       tcp_server_ep->keepintvl,
                       tcp_server_ep->max_children
                      );
                break;

            case EP_TCP_CLIENT:
                tcp_client_ep = (t_end_point_tcp_client *)(g_end_points.ep[i].specific_props);
                printf(", tcp client, ip address %s, port %d,timeout, keep alive options %d (%d, %d, %d) %d\n",
                       tcp_client_ep->ip_address,
                       tcp_client_ep->port_number,
                       tcp_client_ep->timeout,
                       tcp_client_ep->keep_alive,
                       tcp_client_ep->keepcnt,
                       tcp_client_ep->keepidle,
                       tcp_client_ep->keepintvl
                      );
                break;

            case EP_TCP_CLIENT_COD:
                tcp_client_ep_cod = (t_end_point_tcp_client_cod *)(g_end_points.ep[i].specific_props);
                printf(", tcp COD client, ip addresses %s %s, ports %d %d,timeout, buf size %d, keep alive options %d (%d, %d, %d) %d\n",
                       tcp_client_ep_cod->primary_server_ip_or_host,
                       tcp_client_ep_cod->backup_server_ip_or_host,
                       tcp_client_ep_cod->primary_server_port,
                       tcp_client_ep_cod->backup_server_port,
                       tcp_client_ep_cod->timeout,
                       tcp_client_ep_cod->buf_size,
                       tcp_client_ep_cod->keep_alive,
                       tcp_client_ep_cod->keepcnt,
                       tcp_client_ep_cod->keepidle,
                       tcp_client_ep_cod->keepintvl
                      );
                break;

            case EP_UDP_SERVER:
#ifdef UDP_LISTEN_MAX_CHILDREN
                printf(", udp server port %d, max_children %d\n",
                       ((t_end_point_udp_server *)(g_end_points.ep[i].specific_props))->port_number,
                       ((t_end_point_udp_server *)(g_end_points.ep[i].specific_props))->max_children
                      );
#else
                printf(", udp server port %d\n",
                       ((t_end_point_udp_server *)(g_end_points.ep[i].specific_props))->port_number
                      );
#endif
                break;

            case EP_UDP_CLIENT:
                printf(", udp client, ip address %s, port %d, timeout %d\n",
                       ((t_end_point_udp_client *)(g_end_points.ep[i].specific_props))->ip_address,
                       ((t_end_point_udp_client *)(g_end_points.ep[i].specific_props))->port_number,
                       ((t_end_point_udp_client *)(g_end_points.ep[i].specific_props))->timeout
                      );

                break;

            case EP_GPS:
                printf(", GPS end point, raw mode %d\n",
                       ((t_end_point_gps *)(g_end_points.ep[i].specific_props))->raw_mode
                      );
                break;

            case EP_GENERIC_EXEC:
                printf(", Generic Executable end point, executable name is %s\n",
                       ((t_end_point_exec *)(g_end_points.ep[i].specific_props))->exec_name
                       );
                break;

            case EP_PPP:
                printf(", ppp server %s, ppp client %s, mru %d, mtu %d, raw %d, disable ccp %d\n",
                       ((t_end_point_ppp *)(g_end_points.ep[i].specific_props))->ip_address_srv,
                       ((t_end_point_ppp *)(g_end_points.ep[i].specific_props))->ip_address_cli,
                       ((t_end_point_ppp *)(g_end_points.ep[i].specific_props))->mru,
                       ((t_end_point_ppp *)(g_end_points.ep[i].specific_props))->mtu,
                       ((t_end_point_ppp *)(g_end_points.ep[i].specific_props))->raw_ppp,
                       ((t_end_point_ppp *)(g_end_points.ep[i].specific_props))->disable_ccp
                      );
                break;

            case EP_IP_MODEM:
                ip_modem_ep = (t_end_point_ip_modem *)(g_end_points.ep[i].specific_props);
                printf(", ip conn server %s, port local %d, port remote %d, udp mode %d, ident %s, exclusive %d, no delay %d\n"
                       "keep alive options %d (%d, %d, %d)\n",
                       ip_modem_ep->ip_addr_srv,
                       ip_modem_ep->port_local,
                       ip_modem_ep->port_remote,
                       ip_modem_ep->is_udp,
                       ip_modem_ep->ident,
                       ip_modem_ep->exclusive,
                       ip_modem_ep->no_delay,
                       ip_modem_ep->keep_alive,
                       ip_modem_ep->keepcnt,
                       ip_modem_ep->keepidle,
                       ip_modem_ep->keepintvl
                      );
                break;

            case EP_BT_SPP:
                bt_spp_ep = (t_end_point_spp *)(g_end_points.ep[i].specific_props);
                printf(", BT SPPEP, uuid %s, rule (%d,%d,%d,%s), "
                       "retry (%d,%d)\n", bt_spp_ep->uuid,
                       bt_spp_ep->rule_operator, bt_spp_ep->rule_negate,
                       bt_spp_ep->rule_property, bt_spp_ep->rule_value,
                       bt_spp_ep->conn_fail_retry,
                       bt_spp_ep->conn_success_retry);
                break;

            case EP_BT_GC:
                bt_gc_ep = (t_end_point_gc *)(g_end_points.ep[i].specific_props);
                printf(", BT GCEP, rule (%d,%d,%d,%s), retry (%d,%d)\n",
                       bt_gc_ep->rule_operator, bt_gc_ep->rule_negate,
                       bt_gc_ep->rule_property, bt_gc_ep->rule_value,
                       bt_gc_ep->conn_fail_retry, bt_gc_ep->conn_success_retry);
                break;

            default:
                printf("\n");
                break;
            }
        }
        else
        {
            // stop at the very first empty end point
            return;
        }
    }
}
#endif
