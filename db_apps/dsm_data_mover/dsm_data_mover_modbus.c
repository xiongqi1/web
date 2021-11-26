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

// Data Stream Manager Data Mover application.
// Initial usage is to replace socat in Modbus applications.
// Intended future usage is 1:N data streams (one end point to many).
//
// Contains COMMON functions that apply to Modbus modes, of which there are 2
// Client agent (router acts as a TCP/IP networking client)
// Server gateway (router acts as a TCP/IP networking server)
// The implementation supports ASCII and RTU modes
// Refer to modbus specifications at http://www.modbus.org/specs.php for
// further description of Modbus protocol
//
// This implementation replaces the initial implementation of Modbus protocol
// inside socat which was unreliable and did not provide a path to multiple
// clients (e.g. different IP clients simultaneously connected). This is described i
// in modbus specifications as desirable but not mandatory functionality.
//
// It is stateless in a sense that it does not match individual serial-to-IP transactions
// but simply performs packet conversion and forwarding.
//
// For example, in server mode:
// 1) Two Modbus polls received from remote Modbus IP client
// 2) Both will be sent to the serial line as opposed to blocking the second poll until
//      serial packet is available as a response to the first poll. The stateful
//      implementation would require that packets, or at least transaction IDs, are buffered.
//
// In client mode:
// 1) Two Modbus polls are received from local serial client
// 2) Both are sent to remote server as they arrive, without waiting for the first
//      IP packet response from remote modbus IP server to arrive
//
// Multiple SIMULTANEOUS client connections are not supported in this release (but can be
// relatively easily added if needed).
//
// In other words this provides a fairly minimalistic implementation but with very easy upgrade path.
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
#include <ctype.h>
#include "rdb_ops.h"
#include "dsm_data_mover.h"

//
// Protocol constants
//
#define MBAP_BUF_SIZE 7
#define MB_ADDR_SIZE 1
#define MB_CRC_SIZE 2
#define MB_LRC_SIZE 1
#define MB_MIN_SIZE_ASCII 4     // min size of message - 1 unit, 1 function, 1 data, 1 LRC
#define MB_MIN_SIZE_RTU 4       // min size of message - 1 unit, 1 function, 1 data, 2 CRC
#define MB_MAX_MESSAGE 256        // max size of RTU framed Modbus serial packet
#define MB_MAX_ASC_MESSAGE 513     // max size of ASCII framed Modbus serial packet
#define MB_MAX_PDU (MB_MAX_MESSAGE-MB_ADDR_SIZE-MB_CRC_SIZE+MBAP_BUF_SIZE)    // max size of PDU carried over TCP

// if a body is being received and the socket fails to receive a full message in
// this many seconds, give up and start looking for another message
#define SOCKET_RX_TIMEOUT_SEC   5

// globally incrementing transaction id which we control in client modes
// in server modes, we respond with the same id as in the initiating transaction from client
static u_short g_transaction_id = 0;

/////////////////////////////////////////////////////////////////////////////
//
// Supports Modbus CRC-16 Calculation
//
/////////////////////////////////////////////////////////////////////////////
// High Order Byte Table
static const u_char auchCRCHi[256] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

// Low Order Byte Table
static const u_char auchCRCLo[256] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
    0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
    0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
    0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
    0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
    0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
    0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
    0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
    0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
    0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};

//
// A simple lookup structure based on bit rate, used to derive
// time constants that depend on serial bit rate setting
//
typedef struct {
    ulong bit_rate;     // bit rate
    int intra_sec;      // max char to char timeout in seconds
    int intra_usec;     // max char to char timeout in micro-seconds
    int total_sec;      // timeout in seconds after which the frame reception fails
} t_simple_lookup;

//
// According to Modbus over serial line spec V 1.02, page 13
// In PDU mode, message frames are separated by silence interval of at least 3.5 chars
// Above 19200 bits/sec, this remains at 1750 us rather than being even a shorter calculated value
// Standard character length is 11 bits, so character times are calculated as
// ((11*1000000)/(bit_rate)) * 3.5
//
// Here is the actual table for all supported bit rates
//--------------------------------------------------
//bitrate ! character time !  3.5 character time, us
//-------------------------------------------------
//300     ! 36666.6666666667 !  128333.333333333
//600     ! 18333.3333333333 !  64166.6666666667
//1200    ! 9166.6666666667  !  32083.3333333333
//2400    ! 4583.3333333333  !  16041.6666666667
//4800    ! 2291.6666666667  !  8020.8333333333
//9600    ! 1145.8333333333  !  4010.4166666667
//19200   ! 572.9166666667   !  2005.2083333333
//38400   ! 286.4583333333   !  1002.6041666667
//57600   ! 190.9722222222   !  668.4027777778
//115200  ! 95.4861111111    !  334.2013888889
#define MODBUS_RTU_INTRAFRAME_DEFAULT_US 1750
static const t_simple_lookup bit_rate_to_time[] = {
    { 300, 0, 128333 },
    { 600, 0, 64166 },
    { 1200, 0, 32083 },
    { 2400, 0, 16041 },
    { 4800, 0, 8020 },
    { 9600, 0, 4010 },
    { 19200, 0, 2005 },
    { 38400, 0, MODBUS_RTU_INTRAFRAME_DEFAULT_US },
    { 57600, 0, MODBUS_RTU_INTRAFRAME_DEFAULT_US },
    { 115200, 0, MODBUS_RTU_INTRAFRAME_DEFAULT_US },
};

//
// Possible states of the receive state machine
//
typedef enum  {
    MB_STATE_RX_START,
    MB_STATE_RX_BODY,
    MB_STATE_RX_WAIT_EOF,
    MB_STATE_RX_CLEANUP
} e_receive_state;

//
// Modbus header when encapsulated in TCP packets
// See Modbus messaging implementation guide "MBAP Header Description"
//
typedef struct  {
    u_short transaction_id;  // This id is used for transaction matching
    u_short protocol_id;     // Modbus, set to 0
    u_short length;          // length of rest of the message
    u_char unit;             // Modbus unit Id, same as Modbus address in serial frame
} t_mbap_header;

//
// Error handling. We provide a mechanism to check at run time the status of
// modbus subsystem
//
typedef enum {
    MB_ERR_WRONG_START_OF_FRAME_ASCII = 0,
    MB_ERR_READ_ERROR_WRONG_NUM_BYTES,
    MB_ERR_FRAME_TIMEOUT,
    MB_ERR_START_OF_ASCII_FRAME_UNEXPECTED,
    MB_ERR_ASCII_FRAME_NO_FOOTER,
    MB_ERR_FRAME_SIZE_EXCEEDED,
    MB_ERR_CHECKSUM_ERROR,
    MB_ERR_FRAME_TOO_SHORT,
    MB_ERR_INTERNAL_ERROR_RECEIVE,
    MB_ERR_DISCARED_RTU_FRAME,
    MB_ERR_INVALID_RX_DATA,
    MB_ERR_INTERNAL_ERROR_SOCK_1,
    MB_ERR_INTERNAL_ERROR_SOCK_2,
    MB_ERR_INTERNAL_ERROR_SOCK_STATE,
    MB_ERR_SER_TX_ERR,
    MB_ERR_SOCKET_TX_ERR,
    MB_ERR_SER_RX_ERR,
    MB_ERR_SOCKET_RX_ERR,
    MB_ERR_INVALID_MBAP_HEADER,
    MB_ERR_INVALID_SOCK_DATA,
    MB_ERR_SOCKET_RX_TIMEOUT,

    // add any new error types here
    // at the same type, enter another element to err_data array to match

    MB_VALID_FRAME,     // a valid frame count not really an error - very handy too!


    MB_ERR_MAX
} e_error_types;

// supports error counter mechanism
typedef struct {
    u_long counter; // dynamic counter of errors
    const int level; // logging level
    const char *long_message;   // long text for syslog
    const char *short_message;  // short text for error string formatting - MUST be under 16 bytes!
} t_error_data;

// IMPORTANT! The order must match one-to-one e_error_types defined above
static t_error_data err_data[MB_ERR_MAX] = {
    //MB_ERR_WRONG_START_OF_FRAME_ASCII
    {0, LOG_INFO, "", "wrong SOF"},

    //MB_ERR_READ_ERROR_WRONG_NUM_BYTES
    {0, LOG_ERR, "Read error, wrong number of bytes", "read err"},

    // MB_ERR_FRAME_TIMEOUT
    {0, LOG_NOTICE, "ASCII Message timeout occurred prior to receiving the frame terminator sequence", "ascii t/o"},

    // MB_ERR_START_OF_ASCII_FRAME_UNEXPECTED,
    {0, LOG_NOTICE, "Start of frame detected in the middle of the frame", "unexp. SOF"},

    // MB_ERR_ASCII_FRAME_NO_FOOTER
    {0, LOG_NOTICE, "ASCII frame incomplete", "foot err"},

    // MB_ERR_FRAME_SIZE_EXCEEDED
    {0, LOG_NOTICE, "Message size exceeded without getting a valid frame", "msg size"},

    // MB_ERR_CHECKSUM_ERROR
    {0, LOG_NOTICE, "Checksum (CRC or LRC) error", "crc"},

    // MB_ERR_FRAME_TOO_SHORT
    {0, LOG_NOTICE, "Frame too short", "too short"},

    // MB_ERR_INTERNAL_ERROR_RECEIVE
    {0, LOG_ERR, "Internal error rx", "internal rx"},

    // MB_ERR_DISCARED_RTU_FRAME
    {0, LOG_INFO, "", "disc. frame"},

    // MB_ERR_INVALID_RX_DATA
    {0, LOG_NOTICE, "Invalid RX data", "rx data inv."},

    // MB_ERR_INTERNAL_ERROR_SOCK_1,
    {0, LOG_NOTICE, "Socket internal error 1", "sock int.err#1"},

    // MB_ERR_INTERNAL_ERROR_SOCK_2,
    {0, LOG_NOTICE, "Socket internal error 2", "sock int.err#2"},

    // MB_ERR_INTERNAL_ERROR_SOCK_STATE,
    {0, LOG_NOTICE, "Socket receive internal error", "sock int.err rx"},

    // MB_ERR_SER_TX_ERR
    {0, LOG_NOTICE, "Serial tx error", "ser rx err"},

    //MB_ERR_SOCKET_TX_ERR,
    {0, LOG_NOTICE, "Socket tx error", "sock tx err"},

    // MB_ERR_SER_RX_ERR
    {0, LOG_NOTICE, "Serial rx error", "ser rx err"},

    // MB_ERR_SOCKET_RX_ERR,
    {0, LOG_NOTICE, "Socket rx error", "sock rx err"},

    // MB_ERR_INVALID_MBAP_HEADER,
    {0, LOG_NOTICE, "Invalid MBAP header", "inv mbap"},

    // MB_ERR_INVALID_SOCK_DATA
    {0, LOG_NOTICE, "Invalid socket data received", "inv sock data"},

    // MB_ERR_SOCKET_RX_TIMEOUT
    {0, LOG_NOTICE, "Socket receive timeout", "sock rx t/o"},

    // MB_VALID_FRAME
    {0, LOG_DEBUG, "Valid frame received", "frame ok"},
};

//
// Reset the error counters
//
void reset_modbus_error_counters(void)
{
    int i;
    for (i = 0 ; i < MB_ERR_MAX ; i++) {
        err_data[i].counter = 0;
    }
}

//
// Called with an error number as per e_error_type enumeration
// Increments the error counter, and if necessary, logs a message to a system log
//
static void handle_condition(int err_no)
{
    if (err_no < MB_ERR_MAX) {
        err_data[err_no].counter++;
        if (strlen(err_data[err_no].long_message)) {
            dsm_dm_syslog(err_data[err_no].level, err_data[err_no].long_message);
        }
    }
}

//
// Formats modbus error string in the caller provided buffer.
// If last argument is non-zero, only adds non-zero counters to the output
//
void prepare_modbus_error_string(char *buf, int buf_size, BOOL non_zero_counters_only)
{
    int i;
    char short_buf[32];

    char *start = buf;
    memset(buf, 0, buf_size);
    for (i = 0 ; i < MB_ERR_MAX ; i++) {
        // last condition - always add packet count
        if (err_data[i].counter || !non_zero_counters_only || (i == (MB_ERR_MAX - 1))) {
            sprintf(short_buf, "%s=%ld,", err_data[i].short_message, err_data[i].counter);
            strcat(buf, short_buf);
            buf += strlen(short_buf);

            if ((buf - start) > (buf_size - sizeof(short_buf))) {
                // will exceed available the buffer size, so bail
                return;
            }
        }
    }
}

//
// Calculates CRC16 as per Modbus algorithm
// Refer to CRC generation section in "Modbus over the serial line" spec
//
static u_short calc_crc16(const u_char *ptr, int len)
{
    u_char crc_hi = 0xFF; // high CRC byte
    u_char crc_lo = 0xFF; // low CRC byte
    unsigned int index;    // will index into CRC lookup

    while (len--) { // pass through message buffer
        index = crc_hi ^ *ptr++;     // calculate the CRC
        crc_hi = crc_lo ^ auchCRCHi[index];
        crc_lo = auchCRCLo[index];
    }
    return ((crc_hi << 8) | crc_lo);
}

//
// A helper to confirm that the CRC is correct.
// return TRUE if correct and FALSE otherwise
//
static BOOL check_crc16(const u_char *buf, int buflen)
{
    u_short crc;
    u_char crc_hi, crc_lo;

    // sanity check
    if (buflen < MB_CRC_SIZE)
        return FALSE;

    // calculate
    crc = calc_crc16(buf, buflen-MB_CRC_SIZE);
    crc_hi = (crc>>8) & 0xFF;
    crc_lo = crc & 0xFF;

    // Check this against the given CRC
    return ((buf[buflen-2] == crc_hi) && (buf[buflen-1] == crc_lo));
}

//
// Calculates LRC as per modbus spec
// Refer to LRC generation section in "Modbus over the serial line" spec
//
static u_char calc_lrc(const u_char *buf, int len)
{
    u_char lrc = 0; // LRC char initialized
    while (len--)
        lrc += *buf++; // add buffer byte without carry

    // return twos complement
    return ((u_char)(-((char)lrc)));
}

//
// A helper to cconfirm the LRC is correct
// return TRUE if correct and FALSE otherwise
//
static BOOL check_lrc(const u_char *buf, int buflen)
{
    u_char lrc;

    // sanity check
    if (buflen < MB_LRC_SIZE)
        return FALSE;

    lrc = calc_lrc(buf, buflen - MB_LRC_SIZE);

    return (buf[buflen-MB_LRC_SIZE] == lrc);
}

// Based on bit rate and ASCII/RTU mode, return the:
// 1) maximum gap between frames (seconds and microseconds, both returned by reference in *sec and *usec
// 2) total frame delay beyond which the receiver will give up on reception,
//  this is returned by reference in *total_secs. This is fairly relaxed and in the order of second(s)
//
// Uses a lookup table to work out the values.
// The function will return reasonable values even if bit_rate is not in the table
//
static void get_timeouts(u_long bit_rate, BOOL rtu_mode, int *sec, int *usec, int *total_secs)
{
    int i;

    if (rtu_mode) {
        // set to defaults in case if bit rate is not found in the table
        *sec = 0;
        *usec = MODBUS_RTU_INTRAFRAME_DEFAULT_US;
        *total_secs = 1;

        if (bit_rate == 0)
            return;

        // override if found in the table
        for (i = 0 ; i < sizeof(bit_rate_to_time) /sizeof(t_simple_lookup); i++) {
            if (bit_rate_to_time[i].bit_rate == bit_rate) {
                *sec = bit_rate_to_time[i].intra_sec;
                *usec = bit_rate_to_time[i].intra_usec;
                // max 12 bits (as we allow 2 stop bits) per byte, calculate total number of bits, divide by bit rate
                // and arrive the worst case scenario for how long the complete frame should take on serial line.
                // We add one second for safety. On higher bit rates, this first component becomes
                // negligible/zero, and we end up with 1 second total timeout. Note, on slower bit rates
                // it can take quite some time for one frame- for example, on 300 bits/sec it will
                // take 20 seconds with ASCII encoding
                *total_secs = (MB_MAX_MESSAGE*12)/bit_rate + 1;
                break;
            }
        }
        dsm_dm_syslog(LOG_DEBUG, "Bit rate %d, RTU timeout %d %d", bit_rate, *sec, *usec);
    } else {
        //
        // for ASCII mode, the timings are completely different as we have
        // framing characters and the frames are determined by that rather than timing on the line
        //
        *sec = 1;
        *usec = 0;
        *total_secs = 1;

        if (bit_rate == 0)
            return;

        *total_secs = (MB_MAX_ASC_MESSAGE*12)/bit_rate + 1;
        dsm_dm_syslog(LOG_DEBUG, "Bit rate %d, ASCII timeout %d %d", bit_rate, *sec, *usec);
    }
}

// When devices are setup to communicate on a MODBUS serial line using ASCII,
// each 8–bit byte in a message is sent as two ASCII characters.
// Example : The byte 0X5B is encoded as two characters : 0x35 and 0x42 ( 0x35 ="5", and 0x42 ="B" in ASCII ).

// A helper to convert ASCII alpha-numeric character 0-9 A-F back to binary
// 0xFF is returned on failure
static u_char asc_to_bin(u_char my_char)
{
    if ((my_char >= '0') && (my_char <= '9')) {
        return (u_char)(my_char - '0');
    } else if ((my_char >= 'A') && (my_char <= 'F')) {
        return (u_char)(my_char - 'A' + 0x0A);
    } else {
        dsm_dm_syslog(LOG_ERR, "asc to bin error, char is 0x%02X", my_char);
    }
    return 0xFF;
}

//
// A helper convert binary character to ASCII alpha-numeric 0-9 or A-F for ASCII encoding
// 0xFF is returned on failure
//
static u_char bin_to_asc(u_char my_char)
{
    if (my_char <= 0x09) {
        return (u_char)('0' + my_char);
    } else if ((my_char >= 0x0A) && (my_char <= 0x0F)) {
        return (u_char)(my_char - 0x0A + 'A');
    } else {
        dsm_dm_syslog(LOG_ERR, "bin to asc error, char is 0x%02X", my_char);
    }
    return 0xFF;
}

//
// Reads data from the serial port in ASCII mode.
// Will return in one of 2 cases:
// 1) a complete message is received (length of message, excluding LRC is returned)
// 2) a timeout or an error occurred. In this case, 0 is returned
//
// The arguments are as follows:
// int fd = file descriptor of the serial port
// *buff - pointer to the buffer (allocated by the caller, must be at least MB_MAX_MESSAGE in size)
// tv_sec and tv_usec - a maximum gap between characters allowed before all data is discarded
//
// This function is called only when receive data is detected on the serial port, and only exits
// as described above
//
// The algorithm is based on a state machine which:
// 1) In START state, waits for start of frame character ':'
// 2) In RECEIVE BODY state, reads bytes byte by byte, also monitoring for timeout in excess of
//      a specified timeout. If the first of "tail" characters is detected (\r), it switches to
//      MB_STATE_RX_WAIT_EOF state
// 3) In MB_STATE_RX_WAIT_EOF state, if \n is received, it switches to CLEANUP state
// 4) In cleanup state, the LRC is checked and if successful, function returns the number of bytes
//      in this frame, so the caller can use the buffer to its liking.
//
static int ascii_read(int fd, u_char *buff, long tv_sec, long tv_usec)
{
    int count = 0, ret;
    u_char read_char;
    u_char converted_char;
    e_receive_state rcv_state = MB_STATE_RX_START; // receive state machine
    BOOL is_high_nibble = TRUE; // toggle
    struct timeval timeout;
    fd_set fdsetR;

    while (TRUE) {

        switch (rcv_state) {

        case MB_STATE_RX_START:
            // read one byte, expecting ':'
            // we are only here if something is waiting to be received since select returned > 0
            ret = read(fd, &read_char, 1);
            if (ret == 1) {
                if (read_char == ':') {
                    // prepare to receive message body
                    count = 0;
                    is_high_nibble = TRUE;
                    rcv_state = MB_STATE_RX_BODY;
                } else {
                    handle_condition(MB_ERR_WRONG_START_OF_FRAME_ASCII);
                    // stay in the same state hunting for Start of Frame
                }
            } else {
                // we expect to get one byte, so everything else is an error
                handle_condition(MB_ERR_READ_ERROR_WRONG_NUM_BYTES);
                return 0;
            }
            break;

        case MB_STATE_RX_BODY:
        case MB_STATE_RX_WAIT_EOF:
            // Read one character at a time. A CR LF- character sequence signals the end of the data
            // block, which the state machine go to cleanup state.
            // Other characters are part of the data block and their
            // ASCII value is converted back to a binary representation.
            FD_ZERO(&fdsetR);
            FD_SET(fd, &fdsetR);
            timeout.tv_sec = tv_sec;
            timeout.tv_usec = tv_usec;
            ret = select(fd+1, &fdsetR, NULL, NULL, &timeout);
            if (ret == 0) {
                // cannot be a valid frame without \r\n
                handle_condition(MB_ERR_FRAME_TIMEOUT);
                return 0;
            } else if (ret > 0) {
                ret = read(fd, &read_char, 1);
                if (ret != 1) {
                    handle_condition(MB_ERR_READ_ERROR_WRONG_NUM_BYTES);
                    return 0;
                }
                if( read_char == ':' ) {
                    // this is wrong, this character signals start of frame
                    // all characters so far are discarded, and we stay in
                    // this state
                    is_high_nibble = TRUE;
                    count = 0;
                    handle_condition(MB_ERR_START_OF_ASCII_FRAME_UNEXPECTED);
                } else if (rcv_state == MB_STATE_RX_WAIT_EOF) {
                    if( read_char == '\n' ) {
                        rcv_state = MB_STATE_RX_CLEANUP;
                    } else {
                        handle_condition(MB_ERR_ASCII_FRAME_NO_FOOTER);
                        return 0;
                    }
                } else if (read_char == '\r') {
                    rcv_state = MB_STATE_RX_WAIT_EOF;
                } else {
                    converted_char = asc_to_bin(read_char);
                    if (is_high_nibble) {
                        buff[count] = (u_char)(converted_char << 4);
                        is_high_nibble = FALSE;
                    } else {
                        buff[count++] |= converted_char;
                        if(count >= MB_MAX_MESSAGE) {
                            handle_condition(MB_ERR_FRAME_SIZE_EXCEEDED);
                            return 0;
                        }
                        is_high_nibble = TRUE;
                    }
                }
            }
            break;

        case MB_STATE_RX_CLEANUP:
            if (count >= MB_MIN_SIZE_ASCII) {
                if(check_lrc(buff, count) == TRUE) {
                    handle_condition(MB_VALID_FRAME);
                    return count - MB_LRC_SIZE;
                } else {
                    handle_condition(MB_ERR_CHECKSUM_ERROR);
                    return 0;
                }
            } else {
                handle_condition(MB_ERR_FRAME_TOO_SHORT);
                return 0;
            }
            break;
        }
    }

    handle_condition(MB_ERR_INTERNAL_ERROR_RECEIVE);
    return 0;
}

//
// Reads data from the serial port in RTU mode.
// Will return in one of 2 cases:
// 1) a complete message is received (length of message, excluding the CRC is returned)
// 2) a timeout or an error occurred. In this case, 0 is returned
//
// The arguments are as follows:
// int fd = file descriptor of the serial port
// *buff - pointer to the buffer (allocated by the caller, must be at least MB_MAX_MESSAGE in size)
// tv_sec and tv_usec - a maximum gap between characters allowed before all data is discarded
// frame_start - means this function was called after a timeout in excess of frame timeout was
//  detected.
//
// This function is called only when receive data is detected on the serial port, and only exits
// as described above
//
// The algorithm is based on a state machine which:
// 1) In START state, ignores everything unless a frame timeout was detected, and reads one byte
// 2) In RX BODY state, reads bytes byte by byte, also monitoring for timeout in excess of
//      the frame timeout. After each byte received, CRC is calculated (because the size of the
//      message is NOT known beforehand. If CRC is valid, the function returns the number of bytes in the
//      frame. If timeout is received, a frame is expected and state switches to CLEANUP
// 3) In cleanup state, the CRC is checked and if successful, function returns the number of bytes
//      in this frame, so the caller can use the buffer to its liking.
//
static int rtu_read(int fd, u_char *buff, long tv_sec, long tv_usec, BOOL frame_start)
{
    int count = 0;
    int rc;
    fd_set fdsetR;
    e_receive_state rcv_state = MB_STATE_RX_START;
    struct timeval timeout;

    while (TRUE) {

        switch (rcv_state) {

        case MB_STATE_RX_START:
            if (!frame_start) {
                // flush out everything - hunting for the silence interval to detect the start of frame
                handle_condition(MB_ERR_DISCARED_RTU_FRAME);
                tcflush(fd, TCIOFLUSH);
                return 0;
            }

            //
            // Read one byte. We are only here if something has been received already
            // e.g. select returned != 0
            //
            if (read(fd, buff, 1) == 1) {
                count++;
            } else {
                handle_condition(MB_ERR_READ_ERROR_WRONG_NUM_BYTES);
                return 0;
            }
            rcv_state = MB_STATE_RX_BODY;
            break;

        case MB_STATE_RX_BODY:
            FD_ZERO(&fdsetR);
            FD_SET(fd, &fdsetR);
            timeout.tv_sec = tv_sec;
            // @TODO - this may need to be reviewed - but should only matter on very low bit rates
            // for some reason, the diagslave slave simulator used in ATS when used on slow bit rates
            // inserts a long silence before the last CRC byte.Therefore, the whole frame is deemed invalid by us.
            // as we do not wait long enough to receive the last byte in the frame.
            // To work around, we make the maximum character-to-character delay value a lot more relaxed.
            if (g_conf_serial.bit_rate < 19200)
                timeout.tv_usec = tv_usec*4;
            else
                timeout.tv_usec = tv_usec;
            rc = select(fd+1, &fdsetR, NULL, NULL, &timeout);
            if (rc == 0) {
                // timed out on intrabyte timeout value. End of frame (possibly with an error)
                rcv_state = MB_STATE_RX_CLEANUP;
                dsm_dm_syslog(LOG_DEBUG, "Select timed out");
            } else if (rc > 0) {
                if (read(fd, buff + count, 1) == 1) {
                    count++;
                    // see if this is a valid frame
                    if (count >= MB_MIN_SIZE_RTU) { // min frame size
                        if (check_crc16(buff, count)) {
                            handle_condition(MB_VALID_FRAME);
                            return count - MB_CRC_SIZE;
                        }
                    }
                } else {
                    handle_condition(MB_ERR_READ_ERROR_WRONG_NUM_BYTES);
                }

                if(count >= MB_MAX_MESSAGE) {
                    handle_condition(MB_ERR_FRAME_SIZE_EXCEEDED);
                    return 0;
                }
            }
            break;

        case MB_STATE_RX_CLEANUP:
            if (count >= MB_MIN_SIZE_RTU) {
                if (check_crc16(buff, count)) {
                    handle_condition(MB_VALID_FRAME);
                    return count - MB_CRC_SIZE;
                } else {
                    handle_condition(MB_ERR_CHECKSUM_ERROR);
                    return 0;
                }
            } else {
                handle_condition(MB_ERR_FRAME_TOO_SHORT);
                return 0;
            }
            break;

        case MB_STATE_RX_WAIT_EOF:
        default:
            handle_condition(MB_ERR_INTERNAL_ERROR_RECEIVE);
            break;
        }
    }
}

//
// Use data in raw IP character buffer to format mbap header
// allocated by the caller. Takes care of Big endian byte order
// as required by Modbus protocol
//
static void buf2mbap(const u_char *buf, t_mbap_header *mbapheader)
{
    mbapheader->transaction_id = (buf[0]<<8) + buf[1];
    mbapheader->protocol_id = (buf[2]<<8) + buf[3];
    mbapheader->length = (buf[4]<<8) + buf[5];
    mbapheader->unit = buf[6];
}

//
// Use data in the mbap header structure to format the IP buffer
// allocated by the caller. Takes care of Big endian byte order
// as required by Modbus protocol
//
static void mbap2buf(const t_mbap_header *mbapheader, u_char *buf)
{
    buf[0] = (mbapheader->transaction_id >> 8) & 0xFF;
    buf[1] = mbapheader->transaction_id & 0xFF;

    buf[2] = (mbapheader->protocol_id >> 8) & 0xFF;
    buf[3] = mbapheader->protocol_id & 0xFF;

    buf[4] = (mbapheader->length >> 8) & 0xFF;
    buf[5] = mbapheader->length & 0xFF;

    buf[6] = mbapheader->unit;
}

//
// A helper to prepare the MBAP header and do a few validations, as
// well as to add transaction ID to it.
//
static int prepare_mbap_header(const u_char *ipbuf, t_mbap_header *header)
{
    buf2mbap(ipbuf, header);

    dsm_dm_syslog(LOG_DEBUG, "Prepare header: protocol id %d, unit %d, length %d, tx %d",
                  header->protocol_id, header->unit, header->length, header->transaction_id);

    if(header->protocol_id != 0) {
        dsm_dm_syslog(LOG_ERR,"Invalid protocol ID\n");
        return -1;
    }

    if((header->length + MB_CRC_SIZE) > MB_MAX_MESSAGE) {
        dsm_dm_syslog(LOG_ERR,"Length in mbap invalid, %d", header->length);
        return -1;
    }

    // memorize the transaction id. This only matters in server mode.
    g_transaction_id = header->transaction_id;

    return 0;
}

//
// Given a source buffer which contains a modbus ip packet,
// convert it into Modbus RTU serial frame in destbuf provided by the caller
// Returns the number of bytes to transmit serially
//
static int build_serial_rtu_pdu(const u_char *srcbuf, u_char *destbuf)
{
    u_short crc;
    t_mbap_header mbap;

    // prepare MBAP header so we can validate easily
    if (prepare_mbap_header(srcbuf, &mbap) < 0) {
        return -1;
    }

    //
    // @TODO for stateful implementations,
    // here is the place to check the header's transaction Id as needed
    // and do transaction matching
    //

    // copy the unit address
    destbuf[0] = mbap.unit;

    // copy message body from IP packet
    memcpy(&destbuf[1], &srcbuf[MBAP_BUF_SIZE], mbap.length - 1);

    // Calculate the CRC
    crc = calc_crc16(destbuf, mbap.length);

    // append the CRC
    destbuf[mbap.length] = (crc>>8) & 0xFF;
    destbuf[mbap.length + 1] = crc & 0xFF;

    // return total number of bytes including the CRC
    return mbap.length + MB_CRC_SIZE;
}

//
// Given a source buffer which contains a modbus ip packet,
// convert it into Modbus ASCII serial frame in destbuf provided by the caller
// Returns the number of bytes to transmit serially or -1 on error
//
static int build_serial_ascii_pdu(const u_char *srcbuf, u_char *destbuf)
{
    u_char lrc;
    int i;

    t_mbap_header mbap;

    // prepare MBAP header so we can validate easily
    if (prepare_mbap_header(srcbuf, &mbap) < 0) {
        return -1;
    }

    //
    // @TODO for stateful implementations,
    // here is the place to check the header's transaction Id as needed
    // and do transaction matching
    //

    // increment to last byte of MBAP header (unit address)
    srcbuf += MBAP_BUF_SIZE - 1;

    // Calculate the LRC
    lrc = calc_lrc(srcbuf, mbap.length);

    // format the destination buffer

    // 1) copy the start char
    *destbuf++ = ':';

    // 2) copy the body, encoding it as required in ASCII mode
    for(i = 0; i < mbap.length; i++) {
        *destbuf++ = bin_to_asc(*srcbuf >> 4);
        *destbuf++ = bin_to_asc(*srcbuf++ & 0x0F);
    }

    // 3) add LRC
    *destbuf++ = bin_to_asc(lrc >> 4);
    *destbuf++ = bin_to_asc(lrc & 0x0F);

    // 4) finally, add the terminator CR LF sequence
    *destbuf++ = '\r';
    *destbuf = '\n';

    // return the size of ASCII encoded serial frame
    // everything apart from start of frame and end of frame
    // are encoded as 2 bytes
    return (2 * (mbap.length + MB_LRC_SIZE) + 3);
}

//
//
// Build an IP PDU
// As an input, we have a pointer to the serial frame buffer
// No need to check CRC or LRC as we have done this before we assumed the frame was valid
// Create a mbap header based on g_transaction_id (which is incremented if we are in client mode,
// and uses the last received tx if in server mode).
// Note that passed msg_body_length does not include CRC (PDU mode) or LRC (ASCII mode)
//
static int build_ip_from_serial(const u_char *srcbuf, int msg_body_length, u_char *destbuf, BOOL client_mode)
{
    t_mbap_header mbap;

    // sanity check
    if ((msg_body_length <= 0) || (srcbuf == NULL) || (destbuf == NULL))
        return 0;

    // prepare a new MBAP header
    memset(&mbap, 0, sizeof(mbap));
    mbap.transaction_id = g_transaction_id;
    if (client_mode)
        g_transaction_id++;
    mbap.length = msg_body_length;
    mbap.unit = *srcbuf;
    mbap.protocol_id = 0;

    dsm_dm_syslog(LOG_DEBUG, "MBAP header : protocol id %d, unit %d, length %d, tx %d",
                  mbap.protocol_id, mbap.unit, mbap.length, mbap.transaction_id);

    // Copy the mbap into the buffer
    mbap2buf(&mbap, destbuf);

    // Copy the data, less the unit address (which is now in the header) and the CRC
    memcpy(destbuf + MBAP_BUF_SIZE, srcbuf + MB_ADDR_SIZE, msg_body_length - MB_ADDR_SIZE);

    // subtract 1 for unit address that goes inside MBAP header, and add the size of the MBAP header
    return msg_body_length - MB_ADDR_SIZE + MBAP_BUF_SIZE;
}

//
// Called from move_data_tcp function (which can run in client or server thread)
// to move data from serial to socket and vice versa for modbus data streams
//
// Arguments are:
// ser_fd - serial end point file descriptor
// socket_fd - socket end point fd
// rtu_mode - TRUE in RTU mode and FALSE in ASCII mode
// client_mode - TRUE in client agent mode and FALSE in server gateway mode
// p_sesult - errors are returned in p_result by reference.
//
void move_data_modbus(int ser_fd, int socket_fd, BOOL rtu_mode, BOOL client_mode, t_move_result *p_result)
{
    int rx_bytes, tx_bytes;
    fd_set fdsetR;
    struct timeval timeout;

    // used to work out when to give up on socket rx
    int select_timeout_ms;

    // determine the highest fd out of two
    int max_fd = ser_fd > socket_fd ? ser_fd : socket_fd;

    // MBAP header
    t_mbap_header mbap_header;

    // a buffer used to send and receive serial frames.
    // It is longer in ASCII mode than in RTU mode, so we use the longest needed size
    u_char ser_buff[MB_MAX_ASC_MESSAGE];

    // importantly, all of the below vars have to be static!
    // IP buffer - this has to be static as it is meant to keep its old data in subsequent calls to this function
    static u_char ip_buff[MB_MAX_PDU];

    // ... and the following variables have to be static as well
    static int expected_length = 0;
    // timeouts
    static int gap_sec = -1, gap_usec = -1, total_frame_sec = -1;
    // receive state
    static int socket_rx_state = 0;
    // socket timeout counter
    static int socket_timeout_count = 0;
    // flag indicating that that there was a serial timeout
    static BOOL had_serial_timeout = FALSE;
    // total bytes received into the socket
    static int rx_total_socket_bytes = 0;

    // get timeout. Do this once only for efficiency, as config cannot change without restarting
    if (gap_sec == -1) {
        get_timeouts(g_conf_serial.bit_rate, rtu_mode, &gap_sec, &gap_usec, &total_frame_sec);
    }

    // zeroise result returned by reference
    memset(p_result, 0, sizeof(t_move_result));

    // Combine select file descriptors to avoid any lags as modbus polling can be very fast.
    FD_ZERO(&fdsetR);
    FD_SET(ser_fd, &fdsetR);
    FD_SET(socket_fd, &fdsetR);

    // First time around, in ASCII mode, use a smallish arbitrary timeout of 10 ms
    // In RTU mode, use exact ones calculated by get_timeouts. This is to guarantee
    // that we are calling rtu_read after a frame gap was detected
    if (rtu_mode) {
        timeout.tv_sec = gap_sec;
        timeout.tv_usec = gap_usec;
    } else {
        timeout.tv_sec = 0;
        timeout.tv_usec = 10*1000; // 10 milliseconds
    }

    // calculate this so we can work out socket timeout
    select_timeout_ms = timeout.tv_sec*1000 + timeout.tv_usec / 1000;

    select(max_fd + 1, &fdsetR, (fd_set*)0, (fd_set*)0, &timeout);

    // Part 1: serial port -> socket
    if (FD_ISSET(ser_fd, &fdsetR)) {

        //
        // Read serial data as fd is set
        // Note that we are using only half the serial buffer even in ASCII mode
        // as we pack bytes 2:1 as we receive them. But we need a full length buffer for transmit
        //
        rx_bytes = rtu_mode ?
                         rtu_read(ser_fd, ser_buff, gap_sec, gap_usec, had_serial_timeout) :
                         ascii_read(ser_fd, ser_buff, gap_sec, gap_usec);

        had_serial_timeout = FALSE;
        if (rx_bytes < 0) {
            p_result->ser_read_err = TRUE;
            handle_condition(MB_ERR_SER_RX_ERR);
        } else if (rx_bytes > 0) {
            dsm_dm_syslog(LOG_DEBUG, "%d bytes in receive buffer", rx_bytes);

            // build IP buffer from serial
            if ((tx_bytes = build_ip_from_serial(ser_buff, rx_bytes, ip_buff, client_mode)) > 0) {
                // send it!
                if (send_all(socket_fd, ip_buff, tx_bytes, 0, SEND_TIMEOUT_SOCK_MS, FALSE, NULL, 0) < 0) {
                    p_result->socket_send_err = TRUE;
                    handle_condition(MB_ERR_SOCKET_TX_ERR);
                }
            } else {
                handle_condition(MB_ERR_INVALID_RX_DATA);
            }
        } else {
            // this is a more or less expected situation if an invalid (non-modbus) data is being sent to the serial port
            tcflush(ser_fd, TCIOFLUSH);
            handle_condition(MB_ERR_INVALID_RX_DATA);
        }
    } else {
        // this is normal
        had_serial_timeout = TRUE;
    }

    // Part 2: socket -> serial port
    if (FD_ISSET(socket_fd, &fdsetR)) {
        socket_timeout_count = 0; // reset socket timeout counter
        switch (socket_rx_state) {
        case 0: // receiving header
            // receive up to the size of MBAP header
            rx_bytes = recv(socket_fd, (ip_buff + rx_total_socket_bytes), MBAP_BUF_SIZE - rx_total_socket_bytes, 0);
            if (rx_bytes < 0) {
                handle_condition(MB_ERR_SER_RX_ERR);
                p_result->socket_recv_err = TRUE;
            } else if (rx_bytes > MBAP_BUF_SIZE - rx_total_socket_bytes) {
                // cannot receive more than asked for!
                handle_condition(MB_ERR_INTERNAL_ERROR_SOCK_1);
                return;
            } else {
                rx_total_socket_bytes += rx_bytes;
                if (rx_total_socket_bytes == MBAP_BUF_SIZE) {
                    rx_total_socket_bytes = 0;

                    buf2mbap(ip_buff, &mbap_header);
                    if ((mbap_header.protocol_id == 0) && ((mbap_header.length + MB_CRC_SIZE) <= MB_MAX_MESSAGE) &&
                            (mbap_header.length > 1) && (mbap_header.length <= (MB_MAX_PDU - MBAP_BUF_SIZE))) {
                        // looks like the header is correct, start receiving body
                        expected_length = mbap_header.length - 1; // unit id already received in the header
                        socket_rx_state = 1;
                    } else {
                        handle_condition(MB_ERR_INVALID_MBAP_HEADER);
                    }
                }
                // otherwise, just stay in the same state to receive the rest of the header
            }
            break;

        case 1: // receiving body
            // receive up to the size expected as per the header
            rx_bytes = recv(socket_fd, (ip_buff + MBAP_BUF_SIZE + rx_total_socket_bytes),
                            expected_length - rx_total_socket_bytes, 0);
            if (rx_bytes < 0) {
                p_result->socket_recv_err = TRUE;
                handle_condition(MB_ERR_SOCKET_RX_ERR);
            } else if (rx_bytes > expected_length - rx_total_socket_bytes) {
                // cannot receive more than asked for!
                handle_condition(MB_ERR_INTERNAL_ERROR_SOCK_2);
                return;
            } else {
                rx_total_socket_bytes += rx_bytes;
                if (rx_total_socket_bytes >= expected_length) {
                    tx_bytes = rtu_mode ?
                               build_serial_rtu_pdu(ip_buff, ser_buff) :
                               build_serial_ascii_pdu(ip_buff, ser_buff);

                    dsm_dm_syslog(LOG_DEBUG, "%d bytes to transmit, expected length=%d, rx bytes=%d, total rx bytes=%d",
                                  tx_bytes, expected_length, rx_bytes, rx_total_socket_bytes);
                    if (tx_bytes > 0) {
                        // timeout for send depends on bit rate
                        if (send_all(ser_fd, ser_buff, tx_bytes, 0, total_frame_sec*1000, TRUE, NULL, 0) < 0) {
                            p_result->ser_write_err = TRUE;
                            handle_condition(MB_ERR_SER_TX_ERR);
                        }
                    } else {
                        handle_condition(MB_ERR_INVALID_SOCK_DATA);
                    }
                    rx_total_socket_bytes = 0;
                    socket_rx_state = 0;
                }
                // otherwise just stay in this state to receive the rest of the body
            }
            break;

        default:
            // internal error
            handle_condition(MB_ERR_INTERNAL_ERROR_SOCK_STATE);
            break;
        }
    } else if (select_timeout_ms) {
        // in body receiving state, have some mechanism to detect inactivity in the socket rx
        // and switch back to receiving the body
        if (++socket_timeout_count > ((SOCKET_RX_TIMEOUT_SEC * 1000) / select_timeout_ms)) {
            socket_timeout_count = 0;
            if (socket_rx_state == 1) {
                handle_condition(MB_ERR_SOCKET_RX_TIMEOUT);
                // give up and go to state 0 to receive a new packet
                rx_total_socket_bytes = 0;
                socket_rx_state = 0;
            }
        }
    }
}
