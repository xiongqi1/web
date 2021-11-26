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
// A lightweight file that merely defines end point types and modes
// This is expected to be included from other projects, in particular
// from modem_emul_ep and dsm_data_mover
//
#ifndef _DSM_TOOL_EP_H_
#define _DSM_TOOL_EP_H_

//
// Defined types of end points
//
typedef enum
{
    EP_SERIAL = 1,
    EP_TCP_SERVER = 2,
    EP_TCP_CLIENT = 3,
    EP_UDP_SERVER = 4,
    EP_UDP_CLIENT = 5,
    EP_GPS = 6,
    EP_GENERIC_EXEC = 7, // customer defined executable
    EP_RS232 = 8,
    EP_RS485 = 9,
    EP_RS422 = 10,
    EP_MODEM_EMULATOR = 11,
    EP_PPP = 12,
    EP_IP_MODEM = 13,
    EP_CSD = 14,
    EP_TCP_CLIENT_COD = 15,
    EP_BT_SPP = 16,
    EP_BT_GC = 17,
    // add new end points here


    EP_MAX_NUMBER // delimiter
} t_end_point_types;

//
// Defined modes for end points
//
typedef enum
{
    EP_MODE_RAW = 1,
    EP_MODE_MODBUS_GW_RTU = 2,
    EP_MODE_MODBUS_GW_ASCII = 3,
    EP_MODE_MODBUS_CLIENT_RTU = 4,
    EP_MODE_MODBUS_CLIENT_ASCII = 5
} t_modes;

#endif
