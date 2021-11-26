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
// This is rework of "old" modem emulator integrated with Data Stream Manager
//
// Contains functions dealing with the comms port via termios driver
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
#include <fcntl.h>

// only need definitions in the hardware file
#include "modem_hw.h"

//
// Open a serial device file by given device name
//
FILE *open_serial_port(const char *ext_port_name)
{
    FILE *ser_port_file;

    if ((ser_port_file = fopen(ext_port_name, "r+")) == 0)
        return NULL;

    return ser_port_file;
}

//
// Sets the hardware flow control (RTS/CTS) to on or off
//
int set_hw_flow_ctrl(FILE *ser_port_file, int on_or_off)
{
    struct termios tty_struct;
    int fd = fileno(ser_port_file);

    if (fd < 0)
        return -1;

    if (tcgetattr(fd, &tty_struct) < 0)
        return -1;

    if (on_or_off) {
        if (tty_struct.c_cflag & CRTSCTS)
            // not an error, already set
            return 0;
        tty_struct.c_cflag |= CRTSCTS;
    } else {
        if ((tty_struct.c_cflag & CRTSCTS) == 0)
            // not an error, already clear
            return 0;
        tty_struct.c_cflag &= ~CRTSCTS;
    }
    return tcsetattr(fd, TCSADRAIN, &tty_struct);
}

//
// Sets the software flow control (XON/XOFF) to on or off
//
int set_sw_flow_ctrl(FILE *ser_port_file, int on_or_off)
{
    struct termios tty_struct;
    int fd = fileno(ser_port_file);

    if (fd < 0)
        return -1;

    if (tcgetattr(fd, &tty_struct) < 0)
        return -1;

    if (on_or_off) {
        if ((tty_struct.c_cflag & (IXON|IXOFF)) == (IXON|IXOFF))
            // not an error, both bits are already set
            return 0;
        tty_struct.c_cflag |= (IXON|IXOFF);
    } else {
        if ((tty_struct.c_cflag & (IXON|IXOFF)) == 0)
            // not an error, already clear
            return 0;
        tty_struct.c_cflag &= ~(IXON|IXOFF);
    }
    return tcsetattr(fd, TCSADRAIN, &tty_struct);
}

// Support for setting and getting of the baud rate of the UART
//
// termios wants baud rate to be given in pre-defined units (e.g. B300)
// instead of actual integers. After some restructuring, we no longer need
// to expose these units to the world - set_baud_rate and get_baud_rate
// work in integers. However, 2 internal conversion functions are needed
// and here they are below
//
// Convert speed_t to unsigned long
// Clearly BXXX is NOT equal to integer XXX
// Hence the conversion function is required
// This is probably defined in the kernel(s) as an enum or #define
//
static u_long baud_to_ulong(speed_t baud)
{
    switch(baud) {
    case B300:
        return 300;
    case B600:
        return 600;
    case B1200:
        return 1200;
    case B2400:
        return 2400;
    case B4800:
        return 4800;
    case B9600:
        return 9600;
    case B19200:
        return 19200;
    case B38400:
        return 38400;
    case B57600:
        return 57600;
    case B115200:
        return 115200;
    case B230400:
        return 230400;
    default:
        // all invalid cases
        return 0;
    }
}

//
// Convert unsigned long to speed_t
//
static speed_t ulong_to_baud(u_long i)
{
    switch(i) {
    case 300:
        return B300;
    case 600:
        return B600;
    case 1200:
        return B1200;
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    default:
        // this is not perfect but none of the Bxxxx are defined as 0 so we are ok
        return 0;
    }
}

//
// Set baud rate : second argument is an integer, e.g. 9600
// If invalid baud rate is given, it is not set and an error
// is returned
//
int set_baud_rate(FILE *ser_port_file, u_long baud)
{
    struct termios tty_struct;
    int fd = fileno(ser_port_file);

    if (fd < 0)
        return -1;

    // convert u_long to speed_t
    speed_t baud_converted = ulong_to_baud(baud);

    if (baud_converted <= 0)
        return -1;

    if (tcgetattr(fd, &tty_struct) < 0)
        return -1;

    // Set baud rate
    cfsetospeed(&tty_struct, baud_converted);

    return tcsetattr(fd, TCSADRAIN, &tty_struct); // set termios struct in driver
}


//
// Return TRUE if a given baud rate is valid for UART, 0 otherwise
//
BOOL baud_rate_valid(u_long baud)
{
    return (ulong_to_baud(baud) != 0) ? TRUE : FALSE;
}

//
// Get the baud rate of the serial port
// This is return in u_long
// Note : 0 return indicates an error
//
u_long get_baud_rate(FILE* port)
{
    speed_t baud_units;
    struct termios tty_struct;

    int fd = fileno(port);

    if (fd < 0)
        return 0;

    if (tcgetattr(fd, &tty_struct) < 0)
        return 0;

    baud_units = cfgetospeed(&tty_struct);
    return (baud_to_ulong(baud_units));
}

//
// Sets data bits (5/6/7/8), parity (none/even/odd) and stop bits (1/2)
// size = 5,6,7,8
// parity = 0,1,2 0-none, 1 even, 2-odd
// stop bits 0
//
int set_character_format(FILE *ser_port_file, int size, int parity, int stopb)
{
    u_long n;
    struct termios tty_struct;
    int fd = fileno(ser_port_file);

    if (fd < 0)
        return -1;

    // range checking

    // Databits
    switch (size) {
    case 5:
        n = CS5;
        break;
    case 6:
        n = CS6;
        break;
    case 7:
        n = CS7;
        break;
    case 8:
        n = CS8;
        break;
    default:
        return -1;
    }

    // Stopbits
    if ((stopb != 1) && (stopb != 2))
        return -1;

    // parity
    if ((parity < 0) || (parity > 2))
        return -1;

    // now get current settings
    if (tcgetattr(fd, &tty_struct) < 0)
        return -1;

    tty_struct.c_cflag = (tty_struct.c_cflag & ~CSIZE) | n;

    // Stop bits
    n = 0;
    if (stopb == 2)
        n = CSTOPB;

    tty_struct.c_cflag = (tty_struct.c_cflag & ~CSTOPB) | n;
    // Parity
    n = 0; // no parity
    if (parity) {
        if (parity == 2)
            // Odd
            n = PARENB | PARODD;
        else if (parity == 1)
            // Even
            n = PARENB;
    }
    tty_struct.c_cflag = (tty_struct.c_cflag & ~(PARENB | PARODD)) | n;
    return tcsetattr(fd, TCSADRAIN, &tty_struct); // set termios struct in driver
}

//
// Set serial port configuration to suit the modem emulator software
// These are not user configurable settings
//
static int set_port_details(FILE *ser_port_file, unsigned char vmin, unsigned char vtime, int blocking)
{
    struct termios tty_struct;
    int fd = fileno(ser_port_file);

    if (fd < 0)
        return -1;

    if (tcgetattr(fd, &tty_struct) < 0)
        return -1;

    // set up for raw operation
    tty_struct.c_lflag = 0; // no local flags
    tty_struct.c_oflag = 0; // no special output flags

    //
    // Disable input processing: no parity checking or marking, no
    // signals from break conditions, do not convert line feeds or
    // carriage returns, and do not map upper case to lower case.
    //
    // The only thing we want is to ignore break conditions
    tty_struct.c_iflag = IGNBRK;

    //
    // Set some reasonable settings that will be overwritten
    // Enable input, and hangup line (drop DTR) on last close.  Other
    // c_cflag flags are set when setting options like csize, stop bits, etc.
    // CLOCAL is very important
    //
    tty_struct.c_cflag = B9600 | CS8 | CREAD | CLOCAL;

    // set up according to parameters given to this function
    tty_struct.c_cc[VMIN] = vmin;
    tty_struct.c_cc[VTIME] = vtime;

    // set termios struct in driver
    if (tcsetattr(fd, TCSADRAIN, &tty_struct) < 0)
        return -1;

    //
    // now set up non-blocking operation -- i.e. return from read
    // immediately on receipt of each byte of data
    //
    if (!blocking)
        return (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) ? -1 : 0;

    return 0;
}

//
// Set port defaults in respect to all aspects of serial
// device configuration that are NOT controlled by
// the user.
//
int set_port_defaults(FILE *ser_port_file)
{
    return set_port_details(ser_port_file,  0, 1, 0); // vmin, vtime, blocking
}
