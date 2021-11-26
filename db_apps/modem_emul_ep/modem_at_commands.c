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
// This is part of the rework of the "old" modem emulator integrated with Data Stream Manager
//
// Functions in this file are often unmodified code from the old modem emulator
// and are primarily individual AT command handlers

// The idea is to progressively clean them up and review functions reqiured from individual AT commmands.
// The most obvious problem that many do not correctly set the pointer to the character buffer
//  so they can only be used when they are last in the sequence
//
// Central function, CDCS_V250Engine, has been rewritten to use the new data driven AT handler
// structure
//
// TODO - rewrite CDCSGetATCommand as it looks like there is room for improvement
//

#include <sys/times.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/msg.h>

#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <regex.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <ctype.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/tcp.h>
#include "rdb_ops.h"
#include "modem_emul_ep.h"

// Cellular packet session management commands generally start with ^ e.g. ^PRFL
#define PRFX "^"
// length of this prefix
#define PRFXL 1

#define PING_COMMAND   "ping -c 4"

// only declare globals that are required - we don't put them in the header file
extern t_conf_v250 g_conf_v250;
extern FILE *g_comm_host;

static e_modem_state modem_state = COMMAND;

extern t_cli_thread_info g_client_thread;
extern t_srv_thread_info g_server_thread;

// couple of local vars to support ring in detection when IP Modem end point is used

// in 100 of milliseconds, the period since the very first ring was detected
static int ring_in_state = 0;

// ring in state will increment every 10 ms
static const int iter_factor = (1000000l / POLL_TIME_US);

// if ATA command was issued
static BOOL got_ata = FALSE;

e_modem_state get_modem_state(void)
{
    return modem_state;
}

// @TODO - work out what to do with this
// not required for stage 1
void checkModemAutoAnswer(int fForce)
{
}

void displayBanner(void)
{
    me_syslog(LOG_DEBUG,"send banner");
    fputs("\r\n", g_comm_host);
    fprintf(g_comm_host, getSingle("system.product.title"));
    fputs("\r\n", g_comm_host);
    fflush(g_comm_host);

}

// TOOD - this can be deleted this and isxdigit() instead from ctype.h could be used
static int unhex(char c)
{
    return (c >= '0' && c <= '9' ? c - '0': c >= 'A' && c <= 'F' ? c - 'A' + 10: c - 'a' + 10);
}

// TODO same as comment above
int isHex(char c)
{
    return (unhex(c)>=0 && unhex(c)<=15);
}

// Return the type of linebreak to be sent.
static const char* CDCS_V250CR(void)
{
    if ( (g_conf_v250.opt_1&SUPRESS_LF)==0 )
        return ("\r\n");
    else
        return ("\r");
}

static void CDCS_V250RespHdr(FILE* stream)
{
    if (stream)
        fputs(CDCS_V250CR(), stream);
}

static void CDCS_V250RespFtr(FILE* stream)
{
    fputs(CDCS_V250CR(), stream);
}

static void CDCS_V250UserResp_CR(FILE* stream)
{
    fputs("\r\n", stream);
}

static void CDCS_V250UserResp(FILE* stream, const char* str)
{
    me_syslog(LOG_DEBUG,"CDCS_V250UserResp('%s')", str);
    if( g_conf_v250.opt_1 &VERBOSE_RSLT )
        CDCS_V250RespHdr(stream);
    // for debug
//fprintf(stream, "+CDCS %s\n",str);
    fputs(str, stream);
    CDCS_V250RespFtr(stream);
    fflush(stream);
}

static void CDCS_V250UserResp_F(FILE* stream, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if( g_conf_v250.opt_1 &VERBOSE_RSLT )
        CDCS_V250RespHdr(stream);
    /* L&G don't want to any notification */
#ifdef V_SUPPRESS_RESPONSE_y
    if (stream == g_comm_host)
        return;
#endif
    vfprintf(stream, fmt, ap);
    CDCS_V250RespFtr(stream);
    fflush(stream);
}


// Splits up a string into an array of strings. It actually inserts
// nulls into the source string and returns an array of pointers to
// each substring.
static char** StrSplit(u_short* Num, char* String, char Delimiter)
{
    char* c;
    char** Result;
    int i;

    // Pass 1 - count the number of delimiters found.
    *Num = 1;
    for (c = (char*)strchr(String, Delimiter); c != NULL; c = (char*)strchr(c, Delimiter)) {
        c++;
        (*Num)++;
    }
    // Allocate storage for the array of pointers.
    if ((Result = (char**)malloc(*Num * sizeof(char*))) == NULL)
        return NULL;

    // Pass 2 - Split it
    i = 0;
    Result[i++] = String;
    for (c = (char*)strchr(String, Delimiter); c != NULL; c = (char*)strchr(c, Delimiter)) {
        *c = 0;
        Result[i++] = ++c;
    }
    return Result;
}

#ifdef V_SUPPRESS_RESPONSE_y
#define RESPONSE_SUPRESS_CHECK	{ if (stream == g_comm_host) break; }
#else
#define RESPONSE_SUPRESS_CHECK
#endif


static void CDCS_V250Resp(FILE* stream, int nCode)
{

    me_syslog(LOG_DEBUG,"CDCS_V250Resp(%d)", nCode);


    if( g_conf_v250.opt_1 & QUIET_ON) {
        switch (nCode) {
        case OK:
        case UPDATEREQ:
            CDCS_V250UserResp_CR(stream);
            break;
        }
    } else if (g_conf_v250.opt_1 & VERBOSE_RSLT) {
        switch (nCode) {
        case OK:
        case UPDATEREQ:
            CDCS_V250UserResp(stream, "OK");
            break;
        case CONNECT:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "CONNECT");
            break;
        case RING:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "RING");
            break;
        case NOCARRIER:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "NO CARRIER");
            break;
        case ERROR:
            CDCS_V250UserResp(stream, "ERROR");
            me_syslog(LOG_DEBUG,"CDCS_V250UserResp(stream, \"ERROR\")");
            break;
        case NODIALTONE:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "NO DIALTONE");
            break;
        case BUSY:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "BUSY");
            break;
        case NOANSWER:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "NO ANSWER");
            break;
        }
    } else {
        switch (nCode) {
        case OK:
        case UPDATEREQ:
            CDCS_V250UserResp(stream, "0");
            break;
        case CONNECT:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "1");
            break;
        case RING:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "2");
            break;
        case NOCARRIER:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "3");
            break;
        case ERROR:
            CDCS_V250UserResp(stream, "4");
            break;
        case NODIALTONE:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "6");
            break;
        case BUSY:
            RESPONSE_SUPRESS_CHECK
            CDCS_V250UserResp(stream, "7");
            break;
        case NOANSWER:
            CDCS_V250UserResp(stream, "8");
            RESPONSE_SUPRESS_CHECK
            break;
        }
    }
}


static void CDCS_V250ConnectMessage(FILE* stream)
{
    char strRate[8];

    if (g_conf_v250.opt_1 &QUIET_ON)
        return;
    sprintf(strRate, "%ld", get_baud_rate(stream));
    CDCS_V250UserResp_F(stream, "CONNECT %s", strRate);
}

// remove spaces in the string pointer by str
static char* spaceTrim(char* str)
{
    int slen;
    int i;
    slen = strlen(str);
    while (--slen >= 0) {
        if (isspace(str[slen])) {
            str[slen] = '\0';
            continue;
        }
        break;
    }
    for (slen = 0; str[slen]; slen++) {
        if (isspace(str[slen])) {
            continue;
        }
        break;
    }
    if (slen) {
        for (i = 0; str[slen+i] != '\0'; i++)
            str[i] = str[slen+i];
        str[i] = '\0';
    }
    return str;
}

// old function had a multitude of methods by which
// a dial number could be interpreted
static int decodeDialString(char* remote, t_dial_details *dd)
{
    char str[512];
    char* p;

    str[0] = 0; // terminate in case remote is NULL or points at 0
    if (remote && (*remote != 0)) {
        strncpy(str, remote, sizeof(str) - 1);
    }

    // trim spaces
    spaceTrim(str);

    // advance pointer past t or T character
    // if atdt was used
    p = str;
    if (*p == 't' || * p == 'T') {
        p++;
    }

    dd->dialled_str[0] = 0;
    strcpy(dd->dialled_str, p);

    me_syslog(LOG_INFO, "Dialled number %s\n", dd->dialled_str);

    return 0;
}

// ATD Command Handler
static char* CDCS_V250Cmd_D(FILE* stream, char* params, char* stat)
{
    t_dial_details dd;

    if (modem_state == ON_LINE_COMMAND) {
        *stat = ERROR;
        params += strlen(params);
        return params;
    }
    params++;
    if (decodeDialString(params, &dd) < 0) {
        *stat = ERROR;
        params += strlen(params);
        return params;
    }

    *stat = NORESP;

    // Do not attempt to connect at all if the DTR line is low.
    if (((g_conf_v250.opt_dtr == V250_DTR_COMMAND || g_conf_v250.opt_dtr == V250_DTR_HANGUP)) && !is_dtr_high()) {
        me_syslog(LOG_ERR, "Dial attempt with DTR asserted\n");
        *stat = ERROR;
        params += strlen(params);
        return params;
    }

    //else
    //	*stat = CDCS_V250CircuitConnect(stream, dd.dialled_str);

    // decide what to do based on the type of end point B connected
    if (is_epb_type_ppp()) {

        //
        // this is an important part:
        // we going into ON_LINE state and will remain there
        // until maintain_ppp_connection exits
        //
        modem_state = ON_LINE;

        dcd_on();
        CDCS_V250ConnectMessage(stream);

        // the following function does not return whilst modem remains in ON_LINE mode
        maintain_ppp_connection();

        //
        // We are here because "maintain_ppp_connection" has exited
        // Re-initialize the serial port to existing configuration settings
        // in case if anything was changed by PPP
        //
        // Since in one of two scenarios we have killed ppp daemon which was using the
        // serial port, it is better to close the port and re-open it again
        //
        fclose(g_comm_host);
        g_comm_host = open_serial_port(g_conf_v250.dev_name);
        initialize_serial_port();

        // back in command state!
        modem_state = COMMAND;
    }
    else if (is_epb_type_ip_modem()) {

        me_syslog(LOG_DEBUG, "Dialling ip cli connection");

        // flush all data that may have been sent by any old connections
        fflush(g_comm_host);

        // ask client thread to connect. Note that this may be
        // disabled by setting remote port to 0 in which case
        // the thread will not be running
        if (g_client_thread.is_running) {
            if (modem_state != COMMAND) {
                me_syslog(LOG_INFO, "Already connected");
            } else if (ring_in_state) {
                me_syslog(LOG_INFO, "Cannot dial out, ring detected");
            }
            else {
                // reset "failed" flag
                g_client_thread.connect_failed = FALSE;
                // reset any incoming call detection
                g_client_thread.start_connecting = TRUE;
            }
        } else {
            // what do we do here?
            CDCS_V250Resp(stream, NOCARRIER);
            me_syslog(LOG_ERR, "Dialling not possible - client mode not configured");
            *stat = ERROR;
            params += strlen(params);
            return params;
        }
    }
    else if (is_epb_type_csd()) {
        // not supported yet
        *stat = ERROR;
        params += strlen(params);
        return params;
    }
    else {
        // this cannot be!
        *stat = ERROR;
        params += strlen(params);
        return params;
    }

    params += strlen(params);
    return params;
}

void CDCS_V250Hangup(FILE* stream)
{
    me_syslog(LOG_DEBUG,"CDCS_V250Hangup(): ModemState = %d", modem_state);
    CDCS_V250Resp(stream, NOCARRIER);
    modem_state = COMMAND;
}

static char* CDCS_V250Cmd_H(FILE* stream, char* params, char* stat)
{
    params++;
    if (isdigit(*params))
        params++;

    // request that client and server thread disconnect
    if (g_client_thread.connected_status)
        g_client_thread.start_disconnecting = TRUE;

    if (g_server_thread.connected_status)
        g_server_thread.start_disconnecting = TRUE;

    // get client/server threads to disconnect ASAP
    sched_yield();

    // send hand up messages to the serial side
    CDCS_V250Hangup(stream);

    return params;
}

static char* CDCS_V250Cmd_A(FILE* stream, char* params, char* stat)
{
    // @TODO - dial in answer implement

    //fprintf(stderr, __func__ " stopping PPP\n");

    // set flag used for incoming connection detection / Ring state machine
    got_ata = TRUE;

    *stat = NORESP;
    return ++params;
}

static char* CDCS_V250Cmd_O(FILE* stream, char* params, char* stat)
{
    if (modem_state == ON_LINE_COMMAND) {
        me_syslog(LOG_INFO, "Re-entering online mode");
        CDCS_V250ConnectMessage(stream);
        modem_state = ON_LINE;
    } else
        *stat = NOCARRIER;

    return ++params;
}

static char* CDCS_V250StringParam(FILE* stream, char* params, char* rslt, char* addr, const char* stat, const char* help, u_short len)
{
    char eq = 0;

    params++;
    if (*params == '=') {
        eq = 1;
        params++;
    }
    if (*params == '?') {
        if (eq == 1)
            CDCS_V250UserResp_F(stream, help);
        else
            CDCS_V250UserResp_F(stream, stat, addr);
        return ++params;
    }
    if (eq) {
        strncpy(addr, params, len - 1);
        addr[len-1] = 0;
        *rslt = UPDATEREQ;
        params += strlen(params);
        return params;
    }
    *rslt = ERROR;
    return params;

}

static char* CDCS_V250CharParam(FILE* stream, char* params, char* rslt, u_char* addr, const char* stat, const char* help, u_short min, u_short max)
{
    char eq = 0;
    char dig = 0;
    char n;

    params++;

    if (*params == '=') {
        eq = 1;
        params++;
    }
    n = atoi(params); // Get value
    while (isdigit(*params))
        // Skip to last digit in value (not past it)
    {
        params++;
        dig++;
    }
    if (dig) {
        if ((n >= min) && (n <= max)) {
            *addr = n;
        } else
            *rslt = ERROR;
        params--;
    }
    if (*params == '?') {
        if (eq == 1)
            CDCS_V250UserResp_F(stream, help, min, max);
        else
            CDCS_V250UserResp_F(stream, stat, * addr);
    }
    return ++params;
}

static char* CDCS_V250ShortParam(FILE* stream, char* params, char* rslt, u_short* addr, const char* stat, const char* help, u_short min, u_short max)
{
    char eq = 0;
    char dig = 0;
    u_short n;

    params++;
    if (*params == '=') {
        eq = 1;
        params++;
    }
    n = atoi(params); // Get value
    while (isdigit(*params))
        // Skip to last digit in value (not past it)
    {
        params++;
        dig++;
    }
    if (dig) {
        if ((n >= min) && (n <= max)) {
            //setSingleInt( addr, n );
            *addr = n;
        } else
            *rslt = ERROR;
        params--;
    }
    if (*params == '?') {
        if (eq == 1)
            CDCS_V250UserResp_F(stream, help, min, max);
        else
            CDCS_V250UserResp_F(stream, stat, * addr);
    }
    return ++params;
}

/* static */ char* CDCS_V250LongHexParam(FILE* stream, char* params, char* rslt, u_char* addr, const char* stat, const char* help, u_long min, u_long max)
{
    char eq = 0;
    char dig = 0;
    u_long n;

    params++;
    if (*params == '=') {
        eq = 1;
        params++;
    }
    sscanf(params, "%lx", &n); // Get value
    while (isdigit(*params))
        // Skip to last digit in value (not past it)
    {
        params++;
        dig++;
    }
    if (dig) {
        if ((n >= min) && (n <= max)) {
            *addr = n;
        } else
            *rslt = ERROR;
        params--;
    }
    if (*params == '?') {
        if (eq == 1)
            CDCS_V250UserResp_F(stream, help, min, max);
        else
            CDCS_V250UserResp_F(stream, stat, * addr);
    }
    return ++params;
}

static char* CDCS_V250BinaryParam(FILE* stream, char* params, char* rslt, u_char* addr, u_char mask, const char* stat, const char* help)
{
    char eq = 0;

    params++;
    if (*params == '=') {
        eq = 1;
        params++;
    }
    if (*params == '1')
        *addr |= mask;
    if (*params == '0')
        *addr &= ~mask;
    if (*params == '?') {
        if (eq == 1)
            CDCS_V250UserResp(stream, help);
        else
            CDCS_V250UserResp_F(stream, stat, ((*addr &mask) ? 1 : 0));
    }
    return ++params;
}

// Command Handlers

// Turn on/off echo
static char* CDCS_V250Cmd_E(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250BinaryParam(stream, params, rslt, &g_conf_v250.opt_1, ECHO_ON, "E: %d", "E: (0-1)");
}

// Quiet mode on/off
static char* CDCS_V250Cmd_Q(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250BinaryParam(stream, params, rslt, &g_conf_v250.opt_1, QUIET_ON, "Q: %d", "Q: (0-1)");
}

// Verbose mode on/off
static char* CDCS_V250Cmd_V(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250BinaryParam(stream, params, rslt, &g_conf_v250.opt_1, VERBOSE_RSLT, "V: %d", "V: (0-1)");
}

// Enables or disables sending of an OK response when the unit receives the <CR>
//character by itself
static char* CDCS_V250Cmd_CROK(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250BinaryParam(stream, params, rslt, &g_conf_v250.opt_1, OK_ON_CR, PRFX "CROK: %d", PRFX "CROK: (0-1)");
}

//Enables or disables sending the <LF> character in response messages.
static char* CDCS_V250Cmd_NOLF(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250BinaryParam(stream, params, rslt, &g_conf_v250.opt_1, SUPRESS_LF, PRFX "NOLF: %d", PRFX "NOLF: (0-1)");
}

static int getDigitParams(char** pp)
{
    int val = 0;
    char* params = * pp;
    if (!isdigit(*params))
        return 0;
    while (isdigit(*params)) {
        val *= 10;
        val += * params - '0';
        params++;
    }
    *pp = params;
    return val;
}

static char* CDCS_V250Cmd_S(FILE* stream, char* params, char* rslt)
{
    int val;
    u_char dummy;
    params++;
    val = getDigitParams(&params);

    if (val == 0)
        // Auto Answer Rings
    {
        --params;
        params = CDCS_V250CharParam(stream, params, rslt, &g_conf_v250.opt_modem_auto_answer, "%03d", "", 0, 255);
        checkModemAutoAnswer(1);
    }
    // @TODO - a few more S parameters could be implemented if we really wanted to
    else if (val == 5) {
        --params;
        params = CDCS_V250CharParam(stream, params, rslt, &g_conf_v250.opt_backspace_character, "%03d", "", 0, 255);
    } else if (val == 8)
        //Comma Dial Modifier Time
    {
        --params;
        params = CDCS_V250CharParam(stream, params, rslt, &dummy, "%03d", "", 0, 255);
    }
    return params;
}

static char * getVersion(char *buffer)
{
    FILE * fp;
    int i = 0;
    int temp;
    fp = fopen("/etc/version.txt", "r");
    sprintf(buffer, "-");
    if (fp == NULL) {
        return buffer;
    }
    while ((temp = fgetc(fp)) != EOF) {
        *(buffer + i) = (char)temp;
        if (iscntrl(*(buffer + i)) || (i > 20)) {
            break;
        }
        i++;
    }
    *(buffer + i) = 0;
    fclose(fp);
    return buffer;
}

static char* CDCS_V250Cmd_I(FILE* stream, char* params, char* stat)
{
    char version[20];
    int val;

    params++;
    val = getDigitParams(&params);
    switch (val) {
    default:
        displayBanner();
        break;
    case 1:
        fprintf(stream, "\r\nRevision %s\r\n ", getVersion(version));
        fflush(stream);
        break;
    }
    return params;
}

// @TODO - probably not needed, remove after review
static char* CDCS_V250Cmd_ICTO(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250ShortParam(stream, params, rslt, &g_conf_v250.icto, PRFX "ICTO: %d", PRFX "ICTO: (%u-%u)", 0, 255);
}

// @TODO - probably not needed, remove after review
static char* CDCS_V250Cmd_IDCT(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250ShortParam(stream, params, rslt, &g_conf_v250.idct, PRFX "IDCT: %d", PRFX "IDCT: (%u-%u)", 0, 65535);
}

// @TODO - probably not needed, remove after review
static char* CDCS_V250Cmd_SEST(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250ShortParam(stream, params, rslt, &g_conf_v250.sest, PRFX "SEST: %d", PRFX "SEST: (%u-%u)", 0, 65535);
}

static char* CDCS_V250Cmd_PRT(FILE* stream, char* params, char* rslt) //Periodic reset timer
{
    params++;
    char *pos;
    if (*params == '=') {
        params++;
        if (*params == '?') {
            CDCS_V250UserResp_F(stream, PRFX "PRT: (%u-%u)", 0, 65535);
            params += strlen(params);
            return params;
        }
        setSingleInt("service.systemmonitor.forcereset", atoi(params));
    } else if (*params == '?') {
        fprintf(stream, "\r\n");
        pos = getSingle("service.systemmonitor.forcereset");
        if(*pos==0)
            pos="0";
        fprintf(stream, PRFX "PRT: %s \r\n", pos );
    }
    params += strlen(params);
    return params;
}


// This command can be used to show or set a number of terminal specific options.
// The command expects / returns an integer value between 0 and 255 which
// represents a bitmap of various options
static char* CDCS_V250Cmd_COPT(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250CharParam(stream, params, rslt, &g_conf_v250.opt_1, PRFX "COPT: %d", PRFX "COPT: (%u-%u)", 0, 3);
}

// Save configuration to non-volatile storage
// The saved configuration values will be restored automatically on power up or if the
// Z command is issued
static char* CDCS_V250Cmd_AmpW(FILE* stream, char* params, char* rslt)
{
    //@TODO check this function - re-instate
    //CDCS_V250SaveConfig();
    params++;
    *rslt = NORESP;
    return params;
}

// Restore factory defaults
static char* CDCS_V250Cmd_AmpF(FILE* stream, char* params, char* rslt)
{
    // @TODO Re-instate
    //CDCS_V250SetDefaults();
    params++;
    *rslt = NORESP;
    return params;
}

//DTR Configuration
// This parameter determines how the terminal responds when the DTR circuit
// changes state.
// Default:&D2
// &D0 Ignore DTR.
// &D1 If in online data state upon an on-to-off transition, the terminal enters
//   online command state and issues an OK result code; the call remains
//  connected.
// &D2 If in online data state or online command state upon an on-to-off
//   transition, the terminal performs an orderly clear-down of the call
//  and returns to command state.
static char* CDCS_V250Cmd_AmpD(FILE* stream, char* params, char* rslt)
{
    params = CDCS_V250CharParam(stream, params, rslt, &g_conf_v250.opt_dtr, "&D: %d", "&D: (%u-%u)", 0, 8);

    return params;
}

//DSR Configuration
static char* CDCS_V250Cmd_AmpS(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250CharParam(stream, params, rslt, &g_conf_v250.opt_dsr, "&S: %d", "&S: (%u-%u)", 0, 4);
}


// DCD Configuration
static char* CDCS_V250Cmd_AmpC(FILE* stream, char* params, char* rslt)
{
    params = CDCS_V250CharParam(stream, params, rslt, &g_conf_v250.opt_dcd, "&C: %d", "&C: (%u-%u)", 0, 3);
    return params;
}

// RI Configuration
static char* CDCS_V250Cmd_AmpN(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250CharParam(stream, params, rslt, &g_conf_v250.opt_ri, "&N: %d", "&N: (%u-%u)", 0, 2);
}

// Loads the terminal configuration to non-volatile memory
static char* CDCS_V250Cmd_Z(FILE* stream, char* params, char* rslt)
{
    // @TODO - is this needed?
    //CDCS_V250Initialise(stream);
    return ++params;
}

// @TODO : investigate if needed
// also, re-written completely so might need to test
// if baud rate changes after this command, do we really want to change
// g_conf_v250.bit_rate to the one received via AT command (as we will never be able to go back?)
static char* CDCS_V250Cmd_IPR(FILE* stream, char* params, char* rslt)
{
    char strRate[8];
    u_long baud_rate = get_baud_rate(stream);

    // @TODO _ check for baud_rate being non-zero
    sprintf(strRate, "%ld", baud_rate);
    params = CDCS_V250StringParam(stream, params, rslt, strRate, "+IPR: %s", "+IPR: (300, 1200,2400,4800,9600,19200,38400,57600,115200,230400)", 8);
    if (*rslt == UPDATEREQ) {
        // get the new baud rate
        baud_rate = atol(strRate);

        // test if valid baud rate has been entered
        if (baud_rate_valid(baud_rate)) {
            CDCS_V250Resp(stream, OK); // Send response before baud change
            set_baud_rate(stream, baud_rate);

            // @TODO - this looks sus
            //g_conf_v250.bit_rate = baud_rate;
            *rslt = NORESP;
        }
    }
    return params;
}

#if 0
// @TODO - probably not needed
// character framing - data, stop bits and parity
static char* CDCS_V250Cmd_ICF(FILE* stream, char* params, char* rslt)
{
    u_short Num;
    char** P;
    char format = 0;
    char parity;
    int fd;
    struct termios tty_struct;

    fd = fileno(g_comm_host);
    tcgetattr(fd, &tty_struct);
    params++;
    if (*params == '=') {
        params++;
        if (*params == '?') {
            CDCS_V250UserResp_F(stream, "+ICF: (1-6),(0-1)");
            params++;
            return params;
        }
        // Split up the string by commas
        P = StrSplit(&Num, params, ',');
        // We must have 1 or 2 params
        parity = 1;
        switch (Num) {
        case 2:
            parity = atoi(P[1]);
            if (parity > 1 || parity < 0 )
                *rslt = ERROR;
        case 1:
            format = atoi(P[0]);
            if (format > 6)
                *rslt = ERROR;
            if (format == 0)
                *rslt = ERROR;
            break;
        default:
            *rslt = ERROR;
        }
        params += strlen(params);
        free(P);
        if (*rslt != ERROR) {
            CDCS_V250Resp(stream, OK); // Send response before format change
            *rslt = NORESP;
            set_character_format(g_comm_host, format, parity);
        }
    }
    if (*params == '?') {
        format = 1;
        parity = 1;
        if ((tty_struct.c_cflag &CSIZE) == CS7)
            format = 4;
        if (tty_struct.c_cflag &PARENB) {
            format++;
            if (tty_struct.c_cflag &PARODD)
                parity = 0;
        } else if ((tty_struct.c_cflag &CSTOPB) == 0)
            format += 2;
        CDCS_V250UserResp_F(stream, "+ICF: %d,%d", format, parity);
        params++;
    }
    return params;
}

// @TODO - meant to setup flow control but this a) probably not needed
// b) if it is needed, has to be done differently as config options have changed.
static char* CDCS_V250Cmd_IFC(FILE* stream, char* params, char* rslt)
{
    u_short Num;
    char** P;
    char topfc;
    char lowfc;
    char flow_in = 0;
    char flow_out = 0;

    params++;
    if (*params == '=') {
        params++;
        if (*params == '?') {
            CDCS_V250UserResp_F(stream, "+IFC: (0,0),(2,2)");
            params++;
            return params;
        }
        // Split up the string by commas
        P = StrSplit(&Num, params, ',');
        // We must have 1 or 2 params
        switch (Num) {
        case 2:
            flow_out = atoi(P[1]);
            if (flow_out > 2)
                *rslt = ERROR;
            flow_in = atoi(P[0]);
            if (flow_in > 3)
                *rslt = ERROR;
            *rslt = OK;
            break;
        case 1:
            flow_in = atoi(P[0]);
            if (flow_in > 3)
                *rslt = ERROR;
            flow_out = flow_in;
            *rslt = OK;
            break;
        default:
            *rslt = ERROR;
            break;
        }
        params += strlen(params);
        free(P);
        if (*rslt != ERROR) {
            g_conf_v250.opt_fc = (flow_in & 0xf) | ((flow_out << 4) & 0xf0);
            setHostFlowType();
        }
    }
    if (*params == '?') {
        topfc = g_conf_v250.opt_fc & 0xf0;
        lowfc = g_conf_v250.opt_fc & 0x0f;
        CDCS_V250UserResp_F(stream, "+IFC: %d,%d", topfc >> 4, lowfc);
        params++;
        *rslt = OK;
    }
    return params;
}
#endif

static char* CDCS_V250Cmd_CSQ(FILE* stream, char* Cmd, char* stat)
{
    char *p_value;

    p_value = getSingle( "wwan.0.radio.information.signal_strength.raw" );
    if (!p_value) {
        strcpy(p_value, "+CSQ: 99, 99");
    }

    CDCS_V250UserResp(stream, p_value);
    *stat = OK;

    // to be able to process commands sequentially - eg +CSQ^ETHN
    //return Cmd + strlen(Cmd);
    return Cmd + 4;
}

//Enables or disables sending an ERROR response to unknown commands
static char* CDCS_V250Cmd_NOER(FILE* stream, char* params, char* rslt)
{
    return CDCS_V250BinaryParam(stream, params, rslt, &g_conf_v250.opt_1, OK_ON_UNKNOWN, "NOER: %d", "NOER: (0-1)");
}

// AT^GLUP=<domain name> (i.e www.google.com) Looks up the given domain name.
static char* CDCS_V250Cmd_GLUP(FILE* stream, char* params, char* rslt)
{
    struct hostent* hp;

    params++;
    if (*params == '=') {
        params++;
        // Look up the name
        hp = gethostbyname(params);
        // Check for lookup failure
        if (hp != 0) {
            // Print the result
            // replaced by Yong for Defect#20 (Incorrect GLUP)
            struct in_addr* inaddr = (struct in_addr*)(hp->h_addr);
            CDCS_V250UserResp_F(stream, PRFX "GLUP: %s", inet_ntoa(*inaddr));

            params += strlen(params);
            return params;
        } else
            CDCS_V250UserResp_F(stream, PRFX "GLUP: Lookup failed.");
    }
    // Error
    *rslt = ERROR;
    return params;
}

/* static */ char* CDCS_V250Cmd_QCDMG(FILE* stream, char* params, char* rslt)
{
    //fputs("AT+IPR=115200\r", comm_phat);
    //fflush(comm_phat);
    //DoDirectPassThru();
    return params;
}

static char* CDCS_V250Cmd_MASQ(FILE* stream, char* params, char* rslt)
{
    params++;
    if (*params == '=') {
        params++;
        if (*params == '?') {
            fprintf(stream, "\r\n^MASQ: (0-1)\r\n");
            params += strlen(params);
            return params;
        }
        if ( atoi(params) == 1 ) {
            setSingleInt("service.dns.masquerade",1);
        } else if (atoi(params) == 0) {
            setSingleInt("service.dns.masquerade",0);
        } else {
            *rslt = ERROR;
        }
    } else if (*params == '?') {
        fprintf(stream, "\r\n^MASQ: %s\r\n", getSingle("service.dns.masquerade"));
    }
    params += strlen(params);
    return params;
}


// ETHI - Set or print the local IP address
static char* CDCS_V250Cmd_ETHI(FILE* stream, char* params, char* rslt)
{
    params++;
    if (*params == '=') {
        ++params;
        if (*params == '?') {
            fprintf(g_comm_host, "\r\n^ETHI: Sets the ethernet interface IP address\r\n");
            params += strlen(params);
            return params;
        }
        setSingleStr("link.profile.0.address", params);
    } else if (*params == '?') {
        fprintf(g_comm_host, "\r\n%s\r\n", getSingle("link.profile.0.address") );
    }
    params += strlen(params);
    return params;
}

// ETHN - Set or print the local TCP/IP netmask
static char* CDCS_V250Cmd_ETHN(FILE* stream, char* params, char* rslt)
{
    params++;
    if (*params == '=') {
        ++params;
        if (*params == '?') {
            fprintf(g_comm_host, "\r\n^ETHN: Sets the ethernet interface subnet mask\r\n");
            params += strlen(params);
            return params;
        }
        setSingleStr("link.profile.0.netmask", params);
    } else if (*params == '?') {
        fprintf(g_comm_host, "\r\n%s\r\n", getSingle("link.profile.0.netmask") );
    }
    params += strlen(params);
    return params;
}

// ETHG - Set or print the TCP/IP gateway
/* static */char* CDCS_V250Cmd_ETHG(FILE* stream, char* params, char* rslt)
{
    return params;
}

// ETHR Print the route table
static char* CDCS_V250Cmd_ETHR(FILE* stream, char* params, char* rslt)
{
    FILE* pFile;
    char buff[100];

    params++;
    if (*params == '?') {
        if ((pFile = popen("route", "r")) == 0) {
            me_syslog(LOG_INFO, "failed to open pipe");
        }
        fprintf(stream, "\r\n\r\n");
        while (fgets(buff, sizeof(buff), pFile) != NULL) {
            fprintf(stream, "%s \r", buff);
        }
        pclose(pFile);
    } else {
        *rslt = ERROR;
    }
    params += strlen(params);
    return params;
}

// NTPS Set SNTP server
/* static */char* CDCS_V250Cmd_NTPS(FILE* stream, char* params, char* rslt)
{
    return params;
}

// AT^NTPU Set GM47 clock from SNTP server
/* static */char* CDCS_V250Cmd_NTPU(FILE* stream, char* params, char* rslt)
{
    return params;
}

static char* CDCS_V250Cmd_DHCP(FILE* stream, char* params, char* rslt)
{
    u_short Num;
    char** P;
    char range[64];
    char dns1[20];
    char dns2[20];

    params++;
    if (*params == '=') {
        params++;
        if (*params == '?') {
            fprintf(g_comm_host, "\r\n");
            fprintf(g_comm_host, "^DHCP: Sets the DHCP configuration \r\n");
            params += strlen(params);
            return params;
        }
        if ( getSingleInt("service.pppoe.server.0.enable") ) {
            *rslt = ERROR;
            fprintf(stream, "\r\n");
            fprintf(stream, "PPPoE mode selected\r\n");
            params += strlen(params);
            return params;
        }
        // Split up the string by commas
        P = StrSplit(&Num, params, ',');
        // if we want to disable DHCP......
        if (atoi(P[0]) == 0) {
            setSingleStr("service.dhcp.enable", "0" );
            params += strlen(params);
            free(P);
            return params;
        }

        // We must have the right number of params
        if (Num != 5) {
            *rslt = ERROR;
            params += strlen(params);
            free(P);
            return params;
        }
        sprintf( range, "%s,%s", P[0], P[1]);
        setSingleStr("service.dhcp.range.0", range);
        setSingleInt("service.dhcp.lease.0", atoi(P[2]));
        setSingleStr("service.dhcp.dns1.0", P[3]);
        setSingleStr("service.dhcp.dns2.0", P[4]);
        setSingleStr("service.dhcp.enable", "1" );
        free(P);
    } else if (*params == '?') {
        if( getSingleInt("service.dhcp.enable") ) {
            strncpy(range, getSingle("service.dhcp.range.0"), 63);
            strncpy(dns1, getSingle("service.dhcp.dns1.0"), 19);
            strncpy(dns2, getSingle("service.dhcp.dns2.0"), 19);
            fprintf(stream, "\r\n");
            fprintf(stream, PRFX "DHCP: %s,%u,%s,%s\r\n", range, getSingleInt("service.dhcp.lease.0"), dns1, dns2);
            fflush(stream);
        } else {
            fprintf(stream, "\r\n");
            fprintf(stream, PRFX "DHCP: 0\r\n");
            fflush(stream);
        }
    }
    params += strlen(params);
    return params;
}

static char* CDCS_V250Cmd_PING(FILE* stream, char* params, char* rslt)
{
    FILE* ping_out;
    int in_char;
    char cmd_buffer[300];

    params++;
    if (*params == '=') {
        params++;
        sprintf(cmd_buffer, "%s %s 2>&1 ", PING_COMMAND, params);
        if ((ping_out = popen(cmd_buffer, "r")) == 0) {
            me_syslog(LOG_INFO, "ping fail");
            return params;
        }
        while ((in_char = fgetc(ping_out)) != EOF) {
            if (in_char == '\n') {
                fputc('\r', g_comm_host);
            }
            fputc(in_char, g_comm_host);
        }
        pclose(ping_out);
        params += strlen(params);
        return params;
    }
    // Error
    *rslt = ERROR;
    return params;
}

// AT^LTPH - This routine allows many options to be specified in one command
// for the PING facility in the GPRS modem.
// This allows specifying the IP address to ping with an inter-ping
// timer and other options all at the same time.
static char* CDCS_V250Cmd_LTPH(FILE* stream, char* params, char* rslt)
{
    u_short Num;
    char add1[64], add2[64];
    // This points to the array of pointers to each sub-string returned
    // in the StrSplit() routine
    char** P;
    // increment the pointer and check that '=' is the first character
    // after the at^LTPH command....
    params++;
    if (*params == '=') {
        // Split up the string by commas
        params++;
        P = StrSplit(&Num, params, ',');
        // We must have the right number of params
        if (Num != 5) {
            free(P);
            *rslt = ERROR;
            // advance the pointer to the end of the parameter string...
            params += strlen(params);
            return params;
        }
        setSingleStr("service.systemmonitor.destaddress", P[0]);
        setSingleStr("service.systemmonitor.destaddress2", P[1]);
        setSingleInt("service.systemmonitor.periodicpingtimer", atoi(P[2]));
        setSingleInt("service.systemmonitor.pingacceleratedtimer", atoi(P[3]));
        setSingleInt("service.systemmonitor.failcount", atoi(P[4]));
        params += strlen(params);
        free(P);
    } else if (*params == '?') {
        strncpy( add1, getSingle("service.systemmonitor.destaddress"), 63);
        strncpy( add2, getSingle("service.systemmonitor.destaddress2"), 63);
        fprintf(stream, "\r\n");
        fprintf(stream, PRFX "LTPH: %s,%s,%d,%d,%d,%d\r\n", add1, add2,
                getSingleInt("service.systemmonitor.periodicpingtimer"),
                getSingleInt("service.systemmonitor.pingacceleratedtimer"),
                getSingleInt("service.systemmonitor.failcount"), -1);
        fflush(stream);
        params++;
    } else {
        *rslt = ERROR;
    }
    return params;
}

static char* CDCS_V250Cmd_IFCG(FILE* stream, char* params, char* rslt)
{
    FILE* pFile;
    char buff[100];

    params++;
    if (*params == '?') {
        if ((pFile = popen("ifconfig", "r")) == 0) {
            me_syslog(LOG_INFO, "failed to open pipe");
        }

        fprintf(stream, "\r\n");
        fprintf(stream, "\r\n");

        while (fgets(buff, sizeof(buff), pFile) != NULL) {
            fprintf(stream, "%s \r", buff);
        }

        pclose(pFile);
    } else {
        *rslt = ERROR;
    }
    params += strlen(params);
    return params;
}

static char* CDCS_V250Cmd_TEST(FILE* stream, char* params, char* rslt)
{
    u_char n = 255;

    params = CDCS_V250CharParam(stream, params, rslt, &n, PRFX "TEST: %d", PRFX "TEST: (%u-%u)", 0, 1);
//@TODO: what is this?
    /*if (n == 1)
    {
    	kmip_cns_ThreadPaused = 1;
    	moduleOfflineSent = 1;
    	kmip_cns_ThreadPaused = 0;
    }*/
    return params;
}

static char* CDCS_V250Cmd_MACA(FILE* stream, char* params, char* rslt)
{
    u_short Num;
    int i;
    // This points to the array of pointers to each sub-string returned
    // in the StrSplit() routine
    char** P;
    char buf[128];

    params++;
    if (*params == '=') {
        params++;
        if (*params == '?') {
            fprintf(g_comm_host, "\r\n");
            fprintf(g_comm_host, "MACA: Sets the MAC address of the router's ethernet interface \r\n");
            params += strlen(params);
            return params;
        }
        if (getSingleInt("service.pppoe.server.0.enable")) {
            fprintf(stream, "\r\n");
            fprintf(stream, "MACA: Cannot re-configure mac Addr since PPPoE is running \r\n");
            fprintf(stream, "\r\n");
            *rslt = ERROR;
            params += strlen(params);
            return params;
        }
        if (strlen(params) == 0) { // If no MAC address specified use factory MAC
            *rslt = ERROR;
            params += strlen(params);
            return params;
        }
        // Split up the string by :'s
        P = StrSplit(&Num, params, ':');
        // We must have the right number of params
        if (Num != 6) {
            free(P);
            *rslt = ERROR;
            params += strlen(params);
            return params;
        } else {
            if(strlen(P[5])>2 && *(P[5]+2)==' ')
                *(P[5]+2)=0;
            for(i=0; i<6; i++) {
                if( strlen(P[i])!=2 || !isHex( *(P[i]) ) || !isHex( *(P[i]+1) ) ) {
                    free(P);
                    *rslt = ERROR;
                    params += strlen(params);
                    return params;
                }
            }
            system("ifconfig eth0 down");
            sprintf(buf, "ifconfig eth0 hw ether %s:%s:%s:%s:%s:%s", P[0], P[1], P[2], P[3], P[4], P[5]);
            system(buf);
            sprintf(buf, "ifconfig br0 hw ether %s:%s:%s:%s:%s:%s", P[0], P[1], P[2], P[3], P[4], P[5]);
            system(buf);
            system("ifconfig eth0 up");
            sprintf(buf, "environment -w ethaddr %s:%s:%s:%s:%s:%s", P[0], P[1], P[2], P[3], P[4], P[5]);
            system(buf);
            sprintf(buf, "%s:%s:%s:%s:%s:%s", P[0], P[1], P[2], P[3], P[4], P[5]);
            setSingleStr("systeminfo.mac.eth0", buf);
            free(P);
        }
    } else if (*params == '?') {
        fprintf(stream, "\r\n%s \r\n", getSingle("systeminfo.mac.eth0"));
    }

    params += strlen(params);
    return params;
}

static char* CDCS_V250Cmd_SHWT(FILE* stream, char* params, char* rslt)
{
    return params;
}

static char* CDCS_V250Cmd_SHWM(FILE* stream, char* params, char* rslt)
{
    return params;
}

/* static */char* CDCS_V250Cmd_MIPP(FILE* stream, char* params, char* rslt)
{
    return params;
}

/* static */char* CDCS_V250Cmd_SSQRL(FILE* stream, char* params, char* rslt)
{
    return params;
}

// This at command is used to set the password for a user who wants
// to send at commands to the modem. i.e for TELNET
static char* CDCS_V250Cmd_RMAU(FILE* stream, char* params, char* rslt)
{
    return params;
}

// resets the 822
static char* CDCS_V250Cmd_RESET(FILE* stream, char* params, char* rslt)
{
    system("rdb_set service.system.reset 1");
    return ++params;
}

// added by Yong for Defect#9 (AT^FACTORY)
static char* CDCS_V250Cmd_FACTORY(FILE* stream, char* params, char* rslt)
{
    fprintf(stream, "\r\nFactory default configuration is applied. Rebooting...\r\n");
    fflush(stream);
    system("dbcfg_default -f");
    system("rdb_set service.system.reset 1");
//	system("/sbin/reboot");
    return ++params;
}

static int isProfileEnabled(int n)
{
    char buf[32];
    sprintf( buf,"link.profile.%u.enable", n );
    return getSingleInt( buf );
}

static int isAnyProfileEnabled( void )
{
    char buf[32];
    int i;
    for( i=1; ; i++) {
        sprintf( buf,"link.profile.%u.dev", i );
        if( strncmp( getSingle(buf), "wwan.0", 6 )==0 ) {
            if(isProfileEnabled(i))
                return i;
        } else
            break;

    }
    return 0;
}

// This at command is used for configuring PPPoE setup
static char* CDCS_V250Cmd_PPPoE(FILE* stream, char* params, char* rslt)
{
    if (*params == '=') {
        params++;
        if (*params == '?') {
            fprintf(stream, "\r\n");
            fprintf(stream, "^PPPOE: (0-1)\r\n");
            params += strlen(params);
            return params;
        }
        if (isAnyProfileEnabled()) {
            fprintf(stream, "\r\n");
            fprintf(stream, "a WWAN session is already active\r\n");

            *rslt = ERROR;

            params += strlen(params);
            return params;
        }
        // NOTE - there should be no existing WWAN session already up if pppoe is intended to run..
        if ((atoi(params) == 1) && !isAnyProfileEnabled() ) {
            setSingleInt("service.pppoe.server.0.enable",1);
        } else if (atoi(params) == 0) {
            setSingleInt("service.pppoe.server.0.enable",0);
        } else {
            *rslt = ERROR;
        }
    } else if (*params == '?') {
        fprintf(stream, "\r\n");
        fprintf(stream, "^PPPOE: %s\r\n", getSingle("service.pppoe.server.0.enable"));
    }
    params += strlen(params);
    return params;
}

// the command handler dispatcher table
// each member has 3 fields:
// 1) pointer pre-increment, which is pointer to the string with command,
//      generally just after the command itself, e.g. for ATDstring the handler
//      will get a pointer at string
//      However this is inconsistent across all commands and needs to be reviewed
// 2) Command string (follows AT or previous command, for example ATA will invoke CDCS_V250Cmd_A)
// 3) Function to handle the command
static t_at_command at_command[]= {
    { 0, "A", CDCS_V250Cmd_A},
    { 0, "D", CDCS_V250Cmd_D},
    { 0, "E", CDCS_V250Cmd_E},
    { 0, "H", CDCS_V250Cmd_H},
    { 0, "I", CDCS_V250Cmd_I},
    { 0, "O", CDCS_V250Cmd_O},
    { 0, "Q", CDCS_V250Cmd_Q},
    { 0, "S", CDCS_V250Cmd_S},
    { 0, "V", CDCS_V250Cmd_V},
    { 0, "Z", CDCS_V250Cmd_Z},
    { 1, "&D", CDCS_V250Cmd_AmpD},
    { 1, "&S", CDCS_V250Cmd_AmpS},
    { 1, "&C", CDCS_V250Cmd_AmpC},
    { 1, "&N", CDCS_V250Cmd_AmpN},
    { 1, "&F", CDCS_V250Cmd_AmpF},
    { 1, "&W", CDCS_V250Cmd_AmpW},
    { 3, "+IPR", CDCS_V250Cmd_IPR},
    { 0, "+CSQ", CDCS_V250Cmd_CSQ},
    { PRFXL + 2, PRFX "PRT", CDCS_V250Cmd_PRT},
    { PRFXL + 3, PRFX "CROK", CDCS_V250Cmd_CROK},
    { PRFXL + 3, PRFX "NOLF", CDCS_V250Cmd_NOLF},
    { PRFXL + 3, PRFX "ICTO", CDCS_V250Cmd_ICTO},
    { PRFXL + 3, PRFX "IDCT", CDCS_V250Cmd_IDCT},
    { PRFXL + 3, PRFX "SEST", CDCS_V250Cmd_SEST},
    { PRFXL + 3, PRFX "NOER", CDCS_V250Cmd_NOER},
    { PRFXL + 3, PRFX "COPT", CDCS_V250Cmd_COPT},
    { PRFXL + 3, PRFX "GLUP", CDCS_V250Cmd_GLUP},
    { PRFXL + 3, PRFX "TEST", CDCS_V250Cmd_TEST},
    { PRFXL + 3, PRFX "SHWT", CDCS_V250Cmd_SHWT},
    { PRFXL + 3, PRFX "SHWM", CDCS_V250Cmd_SHWM},
    { PRFXL + 3, PRFX "ETHI", CDCS_V250Cmd_ETHI},
    { PRFXL + 3, PRFX "ETHN", CDCS_V250Cmd_ETHN},
    { PRFXL + 3, PRFX "ETHR", CDCS_V250Cmd_ETHR},
    { PRFXL + 3, PRFX "DHCP", CDCS_V250Cmd_DHCP},
    { PRFXL + 3, PRFX "PING", CDCS_V250Cmd_PING},
    { PRFXL + 3, PRFX "LTPH", CDCS_V250Cmd_LTPH},
    { PRFXL + 3, PRFX "MASQ", CDCS_V250Cmd_MASQ},
    { PRFXL + 3, PRFX "IFCG", CDCS_V250Cmd_IFCG},
    { PRFXL + 3, PRFX "MACA", CDCS_V250Cmd_MACA},
    { PRFXL + 3, PRFX "RMAU", CDCS_V250Cmd_RMAU},
    { PRFXL + 4, PRFX "PPPoE", CDCS_V250Cmd_PPPoE},
    { PRFXL + 6, PRFX "FACTORY", CDCS_V250Cmd_FACTORY},
    { PRFXL + 4, PRFX "RESET", CDCS_V250Cmd_RESET},
};

// called from the loop in the CDCS_V250Engine
//
// Looks through the table of at commands and handlers and if
// a match is found, an AT handler is called
// Args:
//      FILE *stream - a file where the command is being read from, in
//          our case this is generally the serial port
//      char *cmd - a pointer to current position in AT command line
//      char *stat - will return the response code, as per enum defined in header file,
//          for example NOCARRIER
//      int *foundCmd - will return TRUE if command was found in the table and
//          a command handler was called
//
// Return:
//      A pointer which is generally a few characters past *cmd parameter
//      The handler may increment the command string pointer which will be
//      returned by this function - normally this will be done so that the next
//      command that follows in the same line could be parsed correctly
//
static char *processCommand(FILE *stream, char *cmd, char *stat, int *foundCmd)
{
    int i;
    *foundCmd = 0;
    for (i = 0 ; i < sizeof(at_command)/sizeof(t_at_command) ; i++) {
        if (strncasecmp(cmd, at_command[i].match_str, strlen(at_command[i].match_str)) == 0) {
            if (!at_command[i].processor) {
                return cmd;
            }
            *foundCmd = 1;
            return at_command[i].processor(stream, cmd + at_command[i].pre_increment, stat);
        }
    }
    return cmd;
}

//
// This function reads data from UART and does some light parsing
// before passing it to AT command handlers
//
// TODO - this needs to be fully cleaned up and tested as some parts look
// pretty bad.
//
static int CDCSGetATCommand(FILE *f, char* data, int size)
{
    fd_set readset;
    struct timeval tv;
    int rc;
    char ch;
    int echo;
    u_long timeout = 5000;
    int last_eol = 0;
    int i, got_at = 0;

    int fd = fileno(f);

    echo =  g_conf_v250.opt_1&ECHO_ON;
    for (rc = 0; rc < size;) {
        FD_ZERO(&readset);
        FD_SET(fd, &readset);
        /* Before receiving first byte of AT command, check quickly
         * to give a chance to other threads unless it will stop 5 seconds here.
         * Change to long check time after receiving leading 'A'. */
        if (got_at) {
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = (timeout % 1000) * 1000;
        } else {
            tv.tv_sec = 0;
            tv.tv_usec = 100;
        }
        // do we have something to read.....
        int sel_res = select(fd + 1, &readset, 0, 0, &tv);
        //printf("Select result %d\n", sel_res);
        switch (sel_res) {
        case - 1:
            /* error*/
            me_syslog(LOG_ERR, "CDCSGetATCommand select returns -1, error = %s", strerror(errno));
            return (rc);
        case 1:
            // there is one or more chars in buffer
            // NOTE - make sure fread is NOT used. This is because it causes problems
            // especially if extra characters like line-feeds are inserted, i.e
            // if from command line we do ati CR,LF instead of just CR.
            if ((i = read(fd, &ch, 1)) > 0) {
                if (ch == '\r' || ch == '\n') {
                    if (last_eol == 0 || last_eol == ch) {
                        last_eol = ch;
                        *data = 0;
                        return (rc);
                    }
                } else {
                    if ((rc == 0) && ((ch &~0x20) != 'A'))
                        // The first character must be an A or a
                    {
                        *data = 0;
                        return (-1);
                    }
                    got_at = 1;
                    if (ch == g_conf_v250.opt_backspace_character) {
                        if (rc)
                            // Backspace
                        {
                            if (echo) {
                                write(fd, "\010 \010", strlen("\010 \010"));
                                //fflush(dev);
                            }
                            rc--;

                            // @TODO may be check for overflow - but looks like "rc" counter is in sync with "data" pointer
                            data--;

                            // terminate
                            *data = 0;

                            last_eol = 0;
                            continue;
                        }
                    }
                    if (echo) {
                        //fputc(ch, dev);
                        //fflush(dev);
                        write(fd, &ch, 1);
                    }
                    last_eol = 0;
                    *data++ = ch;
                    *data = 0;
                    rc++;
                }
                continue;
            }
            continue;

            // NOTE - for some reason an echo for each character after a select doesn't work.
            // This maybe because if select gets a bunch of characters at once it the echo needs
            // to echo them all out at once, so we then have a one-one relation.
            // You would have thought that echoing one character at a time would be simpler and
            // be more likely to work but that is not the case!
            break;
        case 0:
            /* timeout */
            // If still in command mode wait for more data otherwise give up.
            // Give up if we time out on the first character
            if (!(((modem_state == COMMAND) || (modem_state == ON_LINE_COMMAND)) && (rc != 0))) {
                *data = 0;

                return (-1);
            }
            break;
        default:
            //me_syslog(LOG_ERR, "CDCSGetATCommand default %s", strerror(errno));
            break;
        }
    }
    // terminate the string..
    *data = 0;
    return (rc);
}

// @TODO - fix this - should print any length buffer and do it better than 16 format specifiers
static void print_buf(char* pbuf, int len)
{
    unsigned char* buf = (unsigned char *)pbuf;

    int i, j = len/16;
    for (i = 0; i < j; i++) {
        fprintf(stderr, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n\r",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15]);
    }
}

// This function determines if the physical RI line
// should be on or off
// If we wanted to change the cadence, this is the place to do this...
BOOL get_ring_ind_state(void)
{
    int ring_second;

    if ((ring_in_state <= 0) || (iter_factor <= 0)) {
        return FALSE;
    }

    ring_second = ring_in_state / iter_factor;

    // Australia and UK
    // 400 ms on, 200 ms off, 400 ms on, 2000 ms off
    switch (ring_second % 3) {
        case 0:
            // first second out of 3
            switch (ring_in_state % 10) {
                case 4:
                case 5:
                    // off only during 200 ms in the middle
                    return FALSE;
                default:
                    return TRUE;
            }
            //break;
        default:
            // other seconds - 1 and 2, always off
            return FALSE;
            //break;
    }

    return FALSE;
}

//
// Ring in state machine. Called from main thread every 100 milliseconds
// When returns TRUE, the connection is established - which
// can be the result of receiving AT Answer (ATA) command,
// or optionally, if auto-connect configuration option is used.
// Used by ip modem end point only
//
// ring in state will increment every 100 ms
BOOL ring_in_state_machine(FILE *stream)
{
    if ((ring_in_state == 0) || (iter_factor < 1)) {
        return FALSE;
    }

    me_syslog(LOG_INFO, "ring in state %d", ring_in_state);

    if ((ring_in_state % iter_factor) == 0) {
        CDCS_V250UserResp(stream, "RING");
    }

    if (g_conf_v250.opt_auto_answer_enable) {
        if (((ring_in_state / iter_factor) >= g_conf_v250.opt_modem_auto_answer) || got_ata) {
            ring_in_state = 0;
            return TRUE;
        }
    } else {
        if (got_ata) {
            // got ATA when no autoanswer is enabled
            me_syslog(LOG_INFO, "Got ATA");
            ring_in_state = 0;
            return TRUE;
        }

        if ((ring_in_state / iter_factor) >= g_conf_v250.opt_modem_auto_answer) {
            // give up on rings
            ring_in_state = 0;
            g_server_thread.start_disconnecting = TRUE;
            return FALSE;
        }
    }

    ring_in_state++;
    return FALSE;
}

// Functions that support maintain_ip_modem_connection, which is
// called from the main thread, this is where all logic happens
// which glues AT commands to the client and server threads. Please read comments in
// the header of maintain_ip_modem_connection function.
//
// The main thread communicates with data mover (client and server) threads
// Using thread control structures, as described below:
// handle_data : set by main thread, the client/server threads will move data if this is set
// start_connecting: set by main thread to tell client/server threads that they need to connect
// start_disconnecting: set by main thread to tell client/server threads that they need to disconnect
// connected_status: set by client/server thread, read by main thread
// connect_failed: set by client/server thread when connect attempt is failed, cleared by main thread
// escape_seq_received: set by client/server thread, cleared by main thread
//
// Importantly, connnected_status and handle_data cannot be set on both threads (as only either
// the server or the client can be communicating with the serial port at any given time)
//
// In particular the following state transitions are all handled here:
// 1) In ONLINE state
//  - On DTR asserted, we may want to disconnect the connection
//      or enter the ONLINE COMMAND MODE
//  - If a thread is meant to be moving data (handle_data is set), and we detected connection has dropped, switch to COMMAND state
//      and send NO_CARRIER text to serial port as configured
//  - If escape sequence is received, switch to ONLINE COMMAND state (same handling for client/server cases)
// 2) In COMMAND state
//   - If connected status has become TRUE, modem enters ONLINE state
//   - If incoming connection has been detected (server mode), start RING state machine which
//          may conclude in entering ONLINE state
//
// The function at the moment is a bit clumsy mainly due to the fact that it has several duplicate
// bits of logic common to client and server modes. Probably will have a few bugs which will need to be ironed out
// after testing. But the concept is simple - communicate with threads via thread control structures
//

// handle DTR line detection and resulting modem state changes
static void handle_dtr_ip_modem(FILE *stream)
{
    // work out what to do if the DTR line dropped
    if (!is_dtr_high()) {
        if (g_conf_v250.opt_dtr == V250_DTR_HANGUP) {
            if (g_client_thread.connected_status) {
                g_client_thread.start_disconnecting = TRUE;
                CDCS_V250Hangup(stream);
            } else if (g_server_thread.connected_status) {
                g_server_thread.start_disconnecting = TRUE;
                CDCS_V250Hangup(stream);
            }
            //me_syslog(LOG_DEBUG, "Detected DTR low");
        } else if (g_conf_v250.opt_dtr == V250_DTR_COMMAND) {
            if ((g_client_thread.connected_status) || (g_server_thread.connected_status)) {
                if (modem_state == ON_LINE) {
                    CDCS_V250Resp(stream, OK);
                    me_syslog(LOG_DEBUG, "Entered online command state");
                    modem_state = ON_LINE_COMMAND;
                    CDCS_V250Resp(stream, OK);
                }
            }
        }
    }
}

//
// Processes connection_dropped flags that are set by client and server threads
// Firstly, they need to be cleared
// Secondly, unless both client and server were connected, we need to re-enter
// COMMAND mode.
// Thirdly, if a pending call in is there, we could now let it go through its normal
// sequence (RING and so on) by setting connect_trigger flag.
//
static void process_conn_dropped(FILE *stream)
{
    // transition of modem status from ON_LINE or ON_LINE_COMMAND to COMMAND when connection drops
    if (g_server_thread.connection_dropped) {
        g_server_thread.connection_dropped = FALSE;
        me_syslog(LOG_DEBUG, "Remote client disconnected");

        if (!g_client_thread.connected_status) {
            modem_state = COMMAND;
            CDCS_V250Resp(stream, NOCARRIER);
        }
    }

    if (g_client_thread.connection_dropped) {
        g_client_thread.connection_dropped = FALSE;
        me_syslog(LOG_DEBUG, "Connection to remote server dropped");

        // if outgoing connection dropped, allow waiting incoming calls
        if (g_server_thread.connected_status) {
            g_server_thread.connect_trigger = TRUE;
        }
        modem_state = COMMAND;
        CDCS_V250Resp(stream, NOCARRIER);
    }
}

// handles state transitions when no connections have been established yet
static void process_connection_none(FILE *stream)
{
    g_server_thread.handle_data = FALSE;
    g_client_thread.handle_data = FALSE;

    // both client and server are disconnected
    switch (modem_state) {
        case ON_LINE:
        case ON_LINE_COMMAND:
            break;

        case COMMAND:
            // detect unsuccessful attempt to connect
            if (g_client_thread.connect_failed) {
                g_client_thread.connect_failed = FALSE;
                CDCS_V250Resp(stream, NOCARRIER);
            }
            break;

        default:
            me_syslog(LOG_ERR, "Internal error, modem state %d", modem_state);
            exit (1);
            break;
    }
}

// handles state transitions when server connection has been established
static void process_connection_server(FILE *stream)
{
    g_client_thread.handle_data = FALSE;
    g_server_thread.handle_data = TRUE;

    // only the server is connected
    switch (modem_state) {
        case ON_LINE:
        case ON_LINE_COMMAND:
            if (modem_state == ON_LINE) {
                // escape sequence detection
                if (protected_check_flag_set(&g_server_thread.escape_seq_received, &g_server_thread.mutex)) {
                    modem_state = ON_LINE_COMMAND;
                    CDCS_V250Resp(stream, OK);
                    me_syslog(LOG_DEBUG, "Entered online command state");
                }
            }
            break;

        case COMMAND:
            // incoming call detection
            if (g_server_thread.connect_trigger) {
                // kick of the ring state machine
                ring_in_state = 1;
                got_ata = FALSE;
                g_server_thread.connect_trigger = FALSE;
            }

            if (ring_in_state_machine(stream)) {
                CDCS_V250ConnectMessage(stream);
                me_syslog(LOG_DEBUG, (modem_state == COMMAND) ? "Entered online state" : "Re-entered online state");
                modem_state = ON_LINE;
            }
            break;

        default:
            me_syslog(LOG_ERR, "Internal error, modem state %d", modem_state);
            exit (1);
            break;
    }
}

// handles state transitions when client connection has been established
static void process_connection_client(FILE *stream)
{
    g_server_thread.handle_data = FALSE;
    g_client_thread.handle_data = TRUE;
    // only the client is connected
    switch (modem_state) {
        case ON_LINE:
        case ON_LINE_COMMAND:
            // escape sequence detection
            if (modem_state == ON_LINE) {
                if (protected_check_flag_set(&g_client_thread.escape_seq_received, &g_client_thread.mutex)) {
                    modem_state = ON_LINE_COMMAND;
                    CDCS_V250Resp(stream, OK);
                    me_syslog(LOG_DEBUG, "Entered online command state");
                }
            }
            break;

        case COMMAND:
            if (!g_client_thread.start_disconnecting) {
                // transition into online state
                CDCS_V250ConnectMessage(stream);
                modem_state = ON_LINE;
                me_syslog(LOG_DEBUG, "Entered online state");
            }
            break;

        default:
            me_syslog(LOG_ERR, "Internal error, modem state %d", modem_state);
            exit (1);
            break;
    }
}

// handles state transitions when both client and server connection have been established
static void process_connection_both(FILE *stream)
{
    //
    // FUTURE FUNCTIONALITY
    // this is where we can decide which thread handles data
    // when both are connected, we have an option to kick out the old connection (client)
    // and connect server instead.
    // Reserved for future use - opt_in_call_connect is always FALSE as RDB variable
    // has not been created yet by WebUI. But if we somehow managed to set the flag,
    // it even works by pulling the rug from under the client thread and letting server
    // thread to get all the data.
    //
    if (g_conf_v250.opt_in_call_connect) {
        g_client_thread.handle_data = FALSE;
        g_server_thread.handle_data = TRUE;
    } else {
        g_server_thread.handle_data = FALSE;
        g_client_thread.handle_data = TRUE;
    }

    // both are connected
    switch (modem_state) {
        case ON_LINE:
            // escape sequence detection
            if (protected_check_flag_set(&g_server_thread.escape_seq_received, &g_server_thread.mutex)) {
                modem_state = ON_LINE_COMMAND;
                CDCS_V250Resp(stream, OK);
                me_syslog(LOG_DEBUG, "Entered online command state");
            }

            if (protected_check_flag_set(&g_client_thread.escape_seq_received, &g_client_thread.mutex)) {
                modem_state = ON_LINE_COMMAND;
                CDCS_V250Resp(stream, OK);
                me_syslog(LOG_DEBUG, "Entered online command state");
            }
            break;

        case COMMAND:
        case ON_LINE_COMMAND:
            // nothing to do
            break;

        default:
            me_syslog(LOG_ERR, "Internal error, modem state %d", modem_state);
            exit (1);
            break;
    }
}

//
// Top level function called from main thread.
//
// Importantly, connnected_status and handle_data cannot be set on both threads (as only either
// the server or the client can be communicating with the serial port at any given time)
//
// In particular the following state transitions are all handled here:
// 1) In ONLINE state
//  - On DTR asserted, we may want to disconnect the connection
//      or enter the ONLINE COMMAND MODE
//  - If a thread is meant to be moving data (handle_data is set), and we detected connection has dropped, switch to COMMAND state
//      and send NO_CARRIER text to serial port as configured
//  - If escape sequence is received, switch to ONLINE COMMAND state (same handling for client/server cases)
// 2) In COMMAND state
//   - If connected status has become TRUE, modem enters ONLINE state
//   - If incoming connection has been detected (server mode), start RING state machine which
//          may conclude in entering ONLINE state
//
void maintain_ip_modem_connection(FILE *stream)
{
    handle_dtr_ip_modem(stream);
    process_conn_dropped(stream);

    if (!g_server_thread.connected_status && !g_client_thread.connected_status) {
        process_connection_none(stream);
    } else if (g_server_thread.connected_status && !g_client_thread.connected_status) {
        process_connection_server(stream);
    } else if (!g_server_thread.connected_status && g_client_thread.connected_status) {
        process_connection_client(stream);
    } else {
        process_connection_both(stream);
    }

    // log an error if this ever happens
    if (g_server_thread.handle_data && g_client_thread.handle_data) {
        me_syslog(LOG_ERR, "Error: handle_data set on both client and server threads");
    }
}


//
// The top level function of AT command handler module, called from main loop
//
// The actual AT parsing code is the same for all end points, but things behave different
// in relation to which Modem State (COMMAND, ON_LINE, ON_LINE_COMMAND) we are in
//
// 1) If PPP end point is connected, ONLINE_COMMAND state does not exist - and
//      we do not parse AT commands when in ON_LINE mode, because data is handled by
//      PPP daemon.
// 2) If other end points are connected, we parse AT commands in COMMAND and ON_LINE_COMMAND
//      states
//
void CDCS_V250Engine(FILE *stream)
{
    int lcmd_len;
    char buff[128+1];
    char* pCmd;
    char stat;
    int found = 0;

    stat = OK;

    if (is_epb_type_ppp()) {
        //
        // when supporting ppp, we do not interpret any data on serial port as
        // potential AT commands. In fact we shouldn't even be here until
        // PPP connection terminates and we re-enter command mode
        //
        if (modem_state != COMMAND) {
            me_syslog(LOG_ERR, "CDCS_V250Engine is called whilst modem is in non-command state %d", modem_state);
            return;
        }
    } else if (is_epb_type_ip_modem()) {
        //
        // for non-ppp end points, we continue to run this function (called in the main thread)
        // but do not parse AT commands unless we the modem is in COMMAND or ON_LINE_COMMAND state
        //
        maintain_ip_modem_connection(stream);
        if ((modem_state != COMMAND) && (modem_state != ON_LINE_COMMAND)) {
            // skip the AT command line interpreter
            return;
        }
    } else {
        //
        // we do not process AT commands in CSD mode. Data is sent transparently from ext. serial port
        // to the module and vice-versa.
        // This is similar to pass-through mode of the old modem emulator.
        //
        me_syslog(LOG_ERR, "Internal error - invalid end point B type");
        return;
    }

    // clear buffer
    memset(buff, 0, sizeof(buff));

    if ((lcmd_len = CDCSGetATCommand(stream, buff, sizeof(buff))) == -1)
        return;

    pCmd = buff;

    if (lcmd_len == 0) {
        if (g_conf_v250.opt_1 & OK_ON_CR)
            CDCS_V250Resp(stream, stat);
        return;
    }

    me_syslog(LOG_DEBUG, "CDCSGetATCommand(), len = %d", lcmd_len);
    fprintf(stderr, "%s\n", buff);
    print_buf(&buff[0], sizeof(buff));

    // Ignore lines that don't start with AT
    if (strncasecmp(buff, "AT", 2) != 0)
        return;

    // increment pointer to just past AT
    pCmd += 2;

    // Process all commands
    while (*pCmd) {

        pCmd = processCommand(stream, pCmd, &stat, &found);

        if (!found || (stat == ERROR)) {
            stat = ERROR;
            break; // Unknown command
        }
    }

    if (stat != NORESP) {
        if (g_conf_v250.opt_1 & OK_ON_UNKNOWN) {
            // respond with OK on unknown command if this configuration option is set...
            CDCS_V250Resp(stream, OK);
        } else {
            // ...otherwise return error indicating an unknown command
            CDCS_V250Resp(stream, stat);
        }
    }
}

