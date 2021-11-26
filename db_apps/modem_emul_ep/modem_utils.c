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
#include <sys/times.h> // need both time and times
#include <errno.h>
#include <termios.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "rdb_ops.h"
#include "modem_emul_ep.h"

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
// Print v250 configuration
//
int print_v250_config(t_conf_v250 *p_conf)
{
    if (p_conf) {
        printf("Device name %s\n"
               "Opt 1 %d\n"
               "DSR %d\n"
               "DTR %d\n"
               "DCD %d\n"
               "RI %d\n"
               "SW FC %d\n"
               "HW FC %d\n"
               "Bit rate %ld\n"
               "Data bits %d\n"
               "Stop bits %d\n"
               "Parity %d\n"
               "Auto answer enabled %d\n"
               "Modem auto-answer rings %d\n"
               "Backspace char %d\n",
               p_conf->dev_name,
               p_conf->opt_1,
               p_conf->opt_dsr,
               p_conf->opt_dtr,
               p_conf->opt_dcd,
               p_conf->opt_ri,
               p_conf->opt_sw_fc,
               p_conf->opt_hw_fc,
               p_conf->bit_rate,
               p_conf->data_bits,
               p_conf->stop_bits,
               p_conf->parity,
               p_conf->opt_auto_answer_enable,
               p_conf->opt_modem_auto_answer,
               p_conf->opt_backspace_character);
    } else {
        return -1;
    }
    return 0;
}

//
// Print ppp configuration
//
int print_ppp_config(t_conf_ppp *p_conf)
{
    if (p_conf) {
        printf("Debug %d\n"
               "Srv Address 1 %s\n"
               "Cli Address 2 %s\n"
               "Mtu %ld\n"
               "Mru %ld\n"
               "Exclusive %d\n",
               p_conf->debug,
               p_conf->ip_addr_srv,
               p_conf->ip_addr_cli,
               p_conf->mtu,
               p_conf->mru,
               (int)p_conf->exclusive);
    } else {
        return -1;
    }
    return 0;
}

//
// Print ip modem end point configuration
//
int print_ip_modem_config(t_conf_ip_modem *p_conf)
{
    if (p_conf) {
        printf(
               "IP Address remote %s\n"
               "Port remote %d\n"
               "Port local %d\n"
               "Identifier %s\n"
               "UDP mode %d\n"
               "Exclusive mode %d\n"
               "No TCP delay %d\n"
               "Keep alive %d\n"
               "Keep count %d\n"
               "Keep interval %d\n"
               "Keep idle %d\n",
               p_conf->ip_or_host_remote,
               p_conf->port_remote,
               p_conf->port_local,
               p_conf->ident,
               (int)p_conf->is_udp,
               (int)p_conf->exclusive,
               (int)p_conf->no_delay,
               (int)p_conf->keep_alive,
               p_conf->keepcnt,
               p_conf->keepintvl,
               p_conf->keepidle);

    } else {
        return -1;
    }
    return 0;
}

//
// Print configuration of CSD end point
//
int print_csd_config(t_conf_csd *p_conf)
{
    if (p_conf) {
        printf(
               "Inactivity timeout, mins %d\n"
               "Additional initialization string %s\n"
               "Init method %d\n",
               p_conf->inactivity_timeout_mins,
               p_conf->additional_init_str,
               p_conf->init_method);
    } else {
        return -1;
    }
    return 0;
}

//
// Print termios structure
//
void print_termios(const char *prefix, struct termios *tio)
{
    if (tio) {
        printf("%s\nIflag 0%o\n"
            "Oflag 0%o\n"
            "Cflag 0%o\n"
            "Lflag 0%o\n"
            "ISpeed %d\n"
            "OSpeed %d\n",
            prefix,
            tio->c_iflag,
            tio->c_oflag,
            tio->c_cflag,
            tio->c_lflag,
            tio->c_ispeed,
            tio->c_ospeed);
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Below this line, a block of legacy RDB helper function which originated in the original modem emulator
// Typically, they are referenced from AT command handlers. As we clean up AT command handlers, references
// to these functions
//
//
// Never returns NULL, if string could not be read from the rdb then
// a valid pointer to a zero-terminated buffer is returned.
// Return a pointer to this buffer after placing a result in it
//
char *getSingle(const char *myname)
{
    // careful - must not be automatic as caller will
    // use the return as a valid pointer
    static char rdb_buf[512];
    int size = sizeof(rdb_buf);
    if(rdb_get(g_rdb_session_p, myname, rdb_buf, &size)<0) {
        *rdb_buf=0;
    }
    return rdb_buf;
}

//
// Gets one integer from RDB variable with a given name
//
int getSingleInt(const char *myname)
{
    return atoi(getSingle(myname));
}

//
// Sets the RDB variable with a given name to a given string
//
int setSingleStr(const char* name, const char *str)
{
    return rdb_set_string(g_rdb_session_p, name, str);
}

//
// Sets the RDB variable with a given name to a integer
//
int setSingleInt(const char* name, int var)
{
    char buf[32];
    sprintf (buf, "%d", var);
    return rdb_set_string(g_rdb_session_p, name, buf);
}

//
// Get uptime
//
time_t me_get_uptime(void)
{
    struct tms t;
    clock_t c;
    time_t uptime;

    c = times(&t);
    uptime = c / sysconf(_SC_CLK_TCK);
    return uptime;
}

//
// Used in debug mode - to log the state of each thread - no more frequently than once every 3 seconds
//
#define LOGGER_INTERVAL 3
void status_logger(const char *prefix, BOOL connected_status, BOOL moving_data, BOOL escape_received, int modem_state, int index)
{
    static time_t last_time[2];
    static BOOL first[2] = { TRUE, TRUE };

    time_t current_time = me_get_uptime();
    if (first[index] || (abs((int)difftime(last_time[index], current_time)) > LOGGER_INTERVAL))
    {
        me_syslog(LOG_INFO, "%s : conn status %d, moving data %d, escape seq %d, modem state %d",
            prefix, connected_status, moving_data, escape_received, modem_state);
        last_time[index] = current_time;
        first[index] = FALSE;
    }
}

//
// Load and clear a shared Boolean data member in a thread-safe manner.
// "Shared" means data shared between either the main and server, or main and client threads.
//
// BOOL *p_flag - pointer to Boolean member of thread control structure to check
// p_mutex - a pointer to the mutex to take and release
//
// Return TRUE if *p_flag was indeed set (and hence was cleared in this function)
//  FALSE otherwise
//
// The caller would call this function in the following fashion:
// if (protected_check_flag(&g_server_thread.some_bool, &g_server_thread.mutex))
// This, without the need thread safety, would be equivalent to:
// if (g_server_thread.some_bool == TRUE)
//     g_server_thread.some_bool = FALSE;
//
// The threading model explanation:
// 1) The shared data between threads is kept to absolute mimimum and consists of orthogonal
// BOOL flags. "Orthogonal" means that there are no invalid combinations of these flags
// Refer to t_srv_thread_info and t_cli_thread_info.
// 2) Given BOOL-s are 32-bit ints, write and reads are atomic - there is no way that
//  any of members of thread control structure could be only partially written.
//
// With the assumptions above, the only contention can occur during load and clear pattern,
// where a state transition can be missed:
//
// For example:
// Thread 1
// if (flag) {
//      ->reshed
//      clear flag
//      print "Changed state"
// }
//
//Thread 2
// if (some_Condition)
//      set flag
//
// If reshedule occurs as shown, then thread 2 may set the flag for the second (and subsequent) time(s)
// before Thread 1 is given a chance to continue and clear the flag.
// Therefore, the second/subsequent state transitions will be missed and "Changed state" will be printed only once
//
// As it stands, in case of this particular application we do not care if we miss such a state
// transition, as long as we detect that the state has been set.
//
// However, a mechanism is put in place to protect shared data against contention for future use,
// and as an example, escape_seq_received is protected. Please keep these guidelines in mind for future use
// in particular when adding new members to thread control structures.
//
BOOL protected_check_flag_set(BOOL *p_flag, pthread_mutex_t *p_mutex)
{
    BOOL ret = FALSE;
    pthread_mutex_lock(p_mutex);

    if (*p_flag) {
        ret = TRUE;
        *p_flag = FALSE;
    }

    pthread_mutex_unlock(p_mutex);
    return ret;
}

// This function is a wrapper for send, sendto and write and is called when data is to be sent to the file
// descriptor (socket or serial). The need for this function arises because
// send, sendto and write may return less bytes than asked for - so we would like to
// continue to try and send the rest of the buffer in some reasonable way without blocking
// things too much.
//
// 1) Normal usage is to allow up to certain overall time limit to complete sending all the bytes.
//      for example, 10 seconds will allow up to 100 paced at 100ms iterations each calling send once
// 2) If total_timeout_ms argument is 0, only one iteration will be allowed -
//      in other words, use this if you want the entire buffer to be sent in one send/write call,
//      and if this is not possible (send blocks), an error is returned immediately
//
// Returns: 0 - success
//          -1 - failure
//
// Non-trivial arguments explained here:
//
// send_flags applicable to send and sendto only and passed through "as is"
//
// total_timeout_ms - 0 is a valid value and means that the function will only do one send/sendto. 
//      (this function is not designed for write/send attempt to block forever)
//
// is_serial - when socket is serial, uses write, otherwise send (TCP) or sendto (UDP)
//
// udp_addr - when UDP socket is in use, this should contain UDP socket address, otherwise it should be NULL in non-UDP modes
//
// udp_addrlen - is set to sizeof(struct sockaddr_in) by the caller, otherwise 0 in non-UDP modes
//
int send_all(int write_fd, char *buffer, int len, int send_flags, int total_timeout_ms, BOOL is_serial, struct sockaddr *udp_addr, int udp_addrlen)
{
    int nsent; // number of bytes sent
    const int sleep_ms = 100; // use 100 ms as a reasonable number for sleep
    fd_set fdsetW;
    struct timeval tv;

    // calculate the max number of iterations of this loop
    int iter = total_timeout_ms/sleep_ms;

    while(len > 0) {
        if (is_serial) {
            nsent = write(write_fd, buffer, len);
        } else {
            if (udp_addr) {
                nsent = sendto(write_fd, buffer, len, send_flags, udp_addr, udp_addrlen);
            } else {
                nsent = send(write_fd, buffer, len, send_flags);
            }
        }

        if (nsent == -1) {
            me_syslog(LOG_ERR, "Sock send returns -1");
            return -1;
        }
        buffer += nsent;
        len -= nsent;

        if (len > 0) {
            // not everything has been sent yet
            if (iter-- <= 0) {
                me_syslog(LOG_ERR, "Timed out in send_all (%d seconds)", total_timeout_ms);
                return -1;
            }

            //
            // Here, we have nothing much to do but wait and try to send the rest of
            // the buffer. We do not want to do it immediately as the condition that prevented
            // sending it in the first place is probably still there.
            //
            // Select on write is better than sleep as we may return sooner
            // than sleep.
            //
            // All of the below code in the current block is almost equivalent to usleep(1000*sleep_ms);
            FD_ZERO(&fdsetW);
            FD_SET(write_fd, &fdsetW);

            tv.tv_sec = 0;
            tv.tv_usec = sleep_ms * 1000;
            if (select(write_fd + 1, (fd_set *)0, &fdsetW, (fd_set*)0, &tv) < 0) {
                return -1;
            }
        }
    }
    return 0; // ok, all data sent
}
