/*!
 * Copyright Notice:
 * Copyright (C) 2002-2010 Call Direct Cellular Solutions Pty. Ltd.
 */

// MM 27/08/2014
// Fixme
// 1) WWANOpen doesn't work (always returns false)
// 2) chan_signals are broken, therefore CD and RI will not work. Hardcoded CD to activate when Modem is online


#include <sys/times.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdarg.h>
#include <regex.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>

#include <linux/tcp.h>

#include "ModemEmulator.h"
#include "ModemEmulator_Utils.h"

#define MAX_BLOCK_SIZE          511
#define MAX_AT_CMD              129
#define SAFE_COPY(dst, src) strncpy((dst), (src), sizeof(dst)-1); (dst)[sizeof(dst)-1] = '\0';

extern char rdb_buf[MAX_RDB_VAR_SIZE];
extern int no_phone;

struct sockaddr_in UDPLocalAddr;
struct sockaddr_in UDPRemoteAddr;
struct sockaddr_in TCPRemoteAddr;
struct in_addr RemoteAddr;
struct sockaddr_in UDPSourceAddr;

WWANPARAMS* currentProfile;
CONFV250 confv250;

volatile int loopCheckV250 = 0;
volatile int currentConnectType;
volatile int pastThreadPaused = 0;
unsigned long nDnpTimer;
unsigned long last_port_vals = 0;
unsigned long this_port_vals = 0;

char* phone_at_name;
char* host_name=0;

u_short idle;
DIAL_DETAILS currDialDetails;
FILE* comm_phat=0;
FILE* comm_host;
int UDPPadSock = -1;
int TCPPadSock = -1;
int escape_cnt;
int modemQuietMode = -1;
int modemVerbose = -1;
int nModemState;
int scheduleHangup;
int g_aIoErr[IND_COUNT_PORT+1] = {0, };
int aChToFd[IND_COUNT_PORT+1] = {0, };
u_char modemAutoAnswer;
static unsigned chan_signals[IND_COUNT_PORT+1];
int SendATCommandRetry(FILE* pPort, char* pReq, char* pResp, int timeout_sec, int retries);
char *strcasestr(const char *haystack, const char *needle);
void ATPassthrough(FILE* serial, FILE* phone, char* Cmd, int timeout, int tx_only);
int keep_alive_cnt = 0;
const char* IgnoreMsgTable[] = {"CONNECT", "RING", "+CRING", "NO CARRIER", NULL};
int ready_to_rcv_data = 0;

time_t getUptime(void)
{
	struct tms t;
	clock_t c;
	time_t uptime;

	c = times(&t);
	uptime = c / sysconf(_SC_CLK_TCK);
	return uptime;
}

int CDCSGetATCommand(FILE* dev, char* data, int size)
{
	fd_set readset;
	struct timeval tv;
	int rc;
	int fd;
	char ch;
	int echo;
	u_long timeout = 5000;
	int last_eol = 0;
	int i, got_at = 0;

	echo =  confv250.opt_1&ECHO_ON;
	fd = fileno(dev);
	for (rc = 0;rc < size;)
	{
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
		switch (select(fd + 1, &readset, 0, 0, &tv))
		{
			case - 1:
				/* error*/
				syslog(LOG_ERR, "CDCSGetATCommand %s", strerror(errno));
				return (rc);
			case 1:
				// there is one or more chars in buffer
				// NOTE - make sure fread is NOT used. This is because it causes problems
				// especially if extra characters like line-feeds are inserted, i.e
				// if from command line we do ati CR,LF instead of just CR.
				if ((i = read(fileno(dev), &ch, 1)) > 0)
				{
					if (ch == '\r' || ch == '\n')
					{
						if (last_eol == 0 || last_eol == ch)
						{
							last_eol = ch;
							*data = 0;
							return (rc);
						}
					}
					else
					{
						if ((rc == 0) && ((ch &~0x20) != 'A'))
						// The first character must be an A or a
						{
							*data = 0;
							return (-1);
						}
						got_at = 1;
						if (ch == 0x08)
						{
							if (rc)
							// Backspace
							{
								if (echo)
								{
									fputs("\010 \010", dev);
									fflush(dev);
								}
								rc--;
								data--;
								last_eol = 0;
								continue;
							}
						}
						if (echo)
						{
							fputc(ch, dev);
							fflush(dev);
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
				if (!(((nModemState == COMMAND) || (nModemState == ON_LINE_COMMAND)) && (rc != 0)))
				{
					*data = 0;

					return (-1);
				}
				break;
			default:
				syslog(LOG_ERR, "CDCSGetATCommand default %s", strerror(errno));
				break;
		}
	}
	// terminate the string..
	*data = 0;
	return (rc);
}

// Splits up a string into an array of strings. It actually inserts
// nulls into the source string and returns an array of pointers to
// each substring.
char** StrSplit(u_short* Num, char* String, char Delimiter)
{
	char* c;
	char** Result;
	int i;

	// Pass 1 - count the number of delimiters found.
	*Num = 1;
	for (c = (char*)strchr(String, Delimiter);c != NULL;c = (char*)strchr(c, Delimiter))
	{
		c++;
		(*Num)++;
	}
	// Allocate storage for the array of pointers.
	if ((Result = (char**)malloc(*Num * sizeof(char*))) == NULL)
		return NULL;

	// Pass 2 - Split it
	i = 0;
	Result[i++] = String;
	for (c = (char*)strchr(String, Delimiter);c != NULL;c = (char*)strchr(c, Delimiter))
	{
		*c = 0;
		Result[i++] = ++c;
	}
	return Result;
}

//
// Called in response to AT&F
// Set selected V.250 Parameters to default values
// Many legacy applications send AT&F as a matter of course to reset the V.250 interface to a known state
//
void CDCS_V250SetDefaults(void)
{
	setSingleVal( "confv250.Option1", (u_long)(ECHO_ON | VERBOSE_RSLT) );
	rdb_set_single( "confv250.opt_dtr", "0");
	rdb_set_single( "confv250.DSR_Action", "0");
	rdb_set_single( "confv250.opt_rts", "0");
	rdb_set_single( "confv250.opt_cts", "0");
	rdb_set_single( "confv250.opt_dcd", "1");
	rdb_set_single( "confv250.opt_ri", "0");
}

// eat data in host port
int eatDataUntilEmpty(int hSerial)
{
	int cbTotalEat = 0;

	int flagsOld;
	int flagsNew;

	// get current flags
	flagsOld = fcntl(hSerial, F_GETFL);
	if (flagsOld < 0)
		return flagsOld;

	// set new flags
	flagsNew = flagsOld | O_NONBLOCK;
	fcntl(hSerial, F_SETFL, flagsNew);

	// eating
	{
		char achBuf[256];
		int cbEat;

		while ((cbEat = read(hSerial, achBuf, sizeof(achBuf))) > 0)
			cbTotalEat += cbEat;
	}

	// restore flags
	fcntl(hSerial, F_SETFL, flagsOld);

	return cbTotalEat;
}

// opens up the uart (uart1) to the phone and sets the baud rate of the uart to 230400
void SetupPhone(void)
{
	int retries = 0;

	while (1)
	{
		if ((comm_phat = fopen(phone_at_name, "r+")) == 0)
		{
			if (++retries == 20)
			{
				syslog(LOG_INFO, "Unable to open phone AT port: %s", phone_at_name);
				//PowerDownPhone(1);
				CDCS_Sleep(1000);
				//PowerUpPhone(1);
				retries = 0;
			}
			CDCS_Sleep(1000);
		}
		else
		{
			cfg_serial_port(comm_phat, B230400);
			break;
		}
	}
}

// Return the type of linebreak to be sent.
const char* CDCS_V250CR(void)
{
	if ( (confv250.opt_1&SUPRESS_LF)==0 )
		return ("\r\n");
	else
		return ("\r");
}

void CDCS_V250RespHdr(FILE* stream)
{
	if (stream && confv250.emul != EMUL_SIMOCO)
		fputs(CDCS_V250CR(), stream);
}

void CDCS_V250RespFtr(FILE* stream)
{
	fputs(CDCS_V250CR(), stream);
}

void CDCS_V250UserResp_CR(FILE* stream)
{
	fputs("\r\n", stream);
}

void CDCS_V250UserResp(FILE* stream, const char* str)
{
	syslog(LOG_DEBUG,"CDCS_V250UserResp('%s')", str);
	if( confv250.opt_1 &VERBOSE_RSLT )
		CDCS_V250RespHdr(stream);
	// for debug
//fprintf(stream, "+CDCS %s\n",str);
	fputs(str, stream);
	CDCS_V250RespFtr(stream);
	fflush(stream);
}

void CDCS_V250UserResp_F(FILE* stream, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if( confv250.opt_1 &VERBOSE_RSLT )
		CDCS_V250RespHdr(stream);
	/* L&G don't want to any notification */
	#ifdef V_SUPPRESS_RESPONSE_y
	if (stream == comm_host)
		return;
	#endif
	vfprintf(stream, fmt, ap);
	CDCS_V250RespFtr(stream);
	fflush(stream);
}

#ifdef V_SUPPRESS_RESPONSE_y
#define RESPONSE_SUPRESS_CHECK	{ if (stream == comm_host) break; }
#else
#define RESPONSE_SUPRESS_CHECK
#endif
void CDCS_V250Resp(FILE* stream, int nCode)
{
	syslog(LOG_DEBUG,"CDCS_V250Resp(%d)", nCode);

	if( confv250.opt_1 &QUIET_ON )
	{
		switch (nCode)
		{
			case OK:
			case UPDATEREQ:
				CDCS_V250UserResp_CR(stream);
				break;
		}
	}
	else if( confv250.opt_1 &VERBOSE_RSLT )
	{
		switch (nCode)
		{
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
				syslog(LOG_DEBUG,"CDCS_V250UserResp(stream, \"ERROR\")");
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
	}
	else
	{
		switch (nCode)
		{
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

static char* listNoConnect[] =
{
	"OK", "RING", "NO CARRIER", "ERROR", "NO DIALTONE", "BUSY", "NO ANSWER", NULL
};

char* listVerboseNoConnect[] =
{
	"0", "2", "3", "4", "6", "7", "8", NULL
};

int checkNoConnectResponse(char* str)
{
	char **p_listNoConnect;
	int i;


	if( confv250.opt_1 &QUIET_ON )
	{
		return 1;
	}
	else if( confv250.opt_1 &VERBOSE_RSLT )
	{
		p_listNoConnect = listNoConnect;
	}
	else
	{
		p_listNoConnect = listVerboseNoConnect;
	}
	for (i = 0;listNoConnect[i] != NULL;i++)
	{
		if (strstr(str, p_listNoConnect[i])) {
			syslog(LOG_DEBUG,"checkNoConnectResponse() : '%s', '%s'", str, p_listNoConnect[i]);
			return i + 1;
		}
	}
	return 0;
}

void sendMuxV24Signal(int chan, unsigned char set_bits, unsigned char clear_bits)
{
	//TODO match 888
	int v24Set = ConvSendCtlToV24(set_bits);
	int v24Clr = ConvSendCtlToV24(clear_bits);

	if (v24Set)
		TogglePhysSerialStatOn(chan, v24Set);
	if (v24Clr)
		TogglePhysSerialStatOff(chan, v24Clr);
}

// Closes the PAD socket if required if the remote end disconnects in PAD mode and terminates
// the WWAN connection and resets the phone IF gasc is set to 0.

void CDCS_V250Hangup(FILE* stream)
{
	unsigned long remad;
	unsigned long lt;
	time_t current_time;
	time_t start_time;

	syslog(LOG_DEBUG,"CDCS_V250Hangup(): currentConnectType = %d, nModemState = %d", currentConnectType, nModemState);
	switch (currentConnectType)
	{
		case V250_DIALPORT_PROFILE:
			/* 1 */
			if (currDialDetails.padm == PAD_TCP)
			{
				if (TCPPadSock != -1)
				{
					if (close(TCPPadSock) == -1)
					{
						syslog(LOG_ERR, "error closing socket: %s", strerror(errno));
					}
					TCPPadSock = -1;
				}
			}
			break;
		case V250_DIALPORT_DIALSTR:
		case V250_DIALPORT_CIRCUIT:
			/* 2 */
			if (comm_phat && (nModemState == ON_LINE || nModemState == ON_LINE_COMMAND || nModemState == CSD_DIAL))
			{

#if !defined(V_SERIAL_HAS_FC_y)
				syslog(LOG_DEBUG,"send ATH to modem");
				ATPassthrough(stream, comm_phat, "H", AT_TIMEOUT, 1);
#endif

				/*  because the function sendMuxV24Signal(IND_AT_PORT, 0, S_RTC | S_DV) sometimes not hangup the csd call,
				this loop will temporary fix the problems and make sure that the csd call is realy hangup, by Aaron 22/01/2008 */
				start_time = getUptime();
				do
				{
					sendMuxV24Signal(IND_AT_PORT, 0, S_RTC | S_DV);
					usleep(1000000);
					sendMuxV24Signal(IND_AT_PORT, S_RTC, 0);
					//syslog(LOG_INFO, "chan_signals[IND_AT_PORT] = %02x", chan_signals[IND_AT_PORT]);
					current_time = getUptime();
					if (abs((int)difftime(start_time, current_time)) > 10)
					{
						syslog(LOG_INFO, "ERROR! can not hangup CSD call");
						break;
					}
				}
				while (chan_signals[IND_AT_PORT] & S_DV);
			}
			CDCS_V250Resp(stream, NOCARRIER);
			break;
		case V250_DIALPORT_PACKET:
			/* 3 */
			if (comm_phat && (nModemState == ON_LINE || nModemState == ON_LINE_COMMAND || nModemState == CSD_DIAL))
			{
				sendMuxV24Signal(IND_AT_PORT, 0, S_RTC | S_DV);
				sendMuxV24Signal(IND_AT_PORT, S_RTC, 0);
			}
			break;
	}

	if (stream && (confv250.opt_1 &QUIET_ON) == 0)
	{
		if (confv250.emul == EMUL_SIMOCO)
		{
			remad = remoteAsNumber(currentProfile->rhost);
			lt = ntohl(RemoteAddr.s_addr) - remad;
			CDCS_V250UserResp_F(stream, "HANGUP %ld 001234", lt);
		}
		else if (currentConnectType == V250_DIALPORT_PROFILE)
			CDCS_V250Resp(stream, NOCARRIER);
	}

	usleep(2500000);
	currentConnectType = V250_DIALPORT_DIALSTR;
	nModemState = COMMAND;
}

void CDCS_V250ConnectMessage(FILE* stream)
{
	speed_t current_speed;
	char strRate[8];

	if (confv250.opt_1 &QUIET_ON)
		return;
	current_speed = get_serial_port_baud(stream);
	sprintf(strRate, "%d", baud_to_int(current_speed));
	if (confv250.emul == EMUL_NUMERIC)
	{
		CDCS_V250UserResp_F(stream, "10");
	}
	else
	{
		CDCS_V250UserResp_F(stream, "CONNECT %s", strRate);
	}
}


int pausePassAt(int camp)
{
	int i;
	if (camp)
	{
		if (++pastThreadPaused == 1)
			return 0;
	}
	else
	{
		if (!pastThreadPaused)
			return 0;
	}
	for (i = 0;i < 10000;i++)
	{
		if (camp)
		{
			if (++pastThreadPaused == 1)
				return 0;
		}
		else
		{
			if (!pastThreadPaused)
				return 0;
		}
		CDCS_Sleep(1);
	}
	return 2;
}

int PingPongSerialModem(int fd_serial, int fd_modem, int inactivity_to, char *cmd_buf, int tx_only)
{
	char ser_inbuf[AT_RETRY_BUFFSIZE]={0, };
	char mdm_inbuf[AT_RETRY_BUFFSIZE]={0, };
	char lcmd_buf[AT_RETRY_BUFFSIZE]={0, };
	char tmp[AT_RETRY_BUFFSIZE]={0, };
	int mdm_len, ser_len, xamin_len;
	int mdm_start, ser_start;
	int ret;
	int count;
	unsigned long readMask, writeMask;
	int readList[2];
	int writeList[2];
	int timeout, cmd_len, sms_cmd, i;
	char bufc;
	#ifdef V_SUPPRESS_RESPONSE_y
	int iTbl = 0;
	char* pMsg;
	#endif
	int skip_sending_resp = 0;

	syslog(LOG_DEBUG,"----> PingPongSerialModem('%s'), tx_only = %d", cmd_buf, tx_only);
	keep_alive_cnt = 0;

	/* copy input command string to wipe out echo response */
	strcpy(lcmd_buf, cmd_buf);
	cmd_len = strlen(cmd_buf);
	sms_cmd = (strcasestr(cmd_buf, "CMGS") != 0);
	syslog(LOG_DEBUG,"sms_cmd = %d", sms_cmd);

	mdm_len = ser_len = xamin_len = 0;
	mdm_start = ser_start = 0;

	/* only read when there is no write data banked up */
	/* a write complete will cause _start = 0 and _len = 0 */
	while (1)
	{
		timeout = inactivity_to;
		if (ser_len)
		{
			writeList[1] = fd_modem;
			timeout = 0;
			readList[0] = -1;
		}
		else
		{
			writeList[1] = -1;
			readList[0] = fd_serial;
		}
		if (mdm_len)
		{
			timeout = 0;
			writeList[0] = fd_serial;
			readList[1] = -1;
		}
		else
		{
			writeList[0] = -1;
			readList[1] = fd_modem;
		}
		count = waitReadWriteFds(&readMask, &writeMask, timeout == 0 ? -1 : timeout, 2, readList, 2, writeList);

		// check for a timeout.........
		if (count == 0)
		{
			syslog(LOG_DEBUG,"<---- PingPongSerialModem() #1, mdm_inbuf = '%s'", mdm_inbuf);
			keep_alive_cnt = 0;
			if (sms_cmd && strcasestr(mdm_inbuf, "error") == 0)
				return 0;
			else
				return -1;
		}

		if (writeMask &1)//BITR_AT_PORT
		{
			if( strstr(mdm_inbuf + mdm_start, "ERROR") && (confv250.opt_1 &OK_ON_UNKNOWN))
			{
				syslog(LOG_DEBUG,"PingPongSerialModem(S) : --> OK");
				write(fd_serial, "\r\nOK\r\n", 6);
				ret = mdm_len - mdm_start;
			}
			else
			{
				skip_sending_resp = 0;
				/* for L&G, they don't want to receive this notification */
				#ifdef V_SUPPRESS_RESPONSE_y
				iTbl = 0;
				while ((pMsg = (char *)IgnoreMsgTable[iTbl++]) != 0) {
					if (strstr(mdm_inbuf + mdm_start, pMsg)) {
						syslog(LOG_DEBUG,"wipe out '%s' from the response packet to meter", pMsg);
						skip_sending_resp = 1;
						ret = mdm_len - mdm_start;
						break;
					}
				}
				#endif

				if (skip_sending_resp == 0) {
					(void) memset(tmp, 0x00, AT_RETRY_BUFFSIZE);
					(void) memcpy(tmp, mdm_inbuf + mdm_start, mdm_len - mdm_start);
					syslog(LOG_DEBUG,"PingPongSerialModem(S) : --> %s", tmp);
					ret = write(fd_serial, mdm_inbuf + mdm_start, mdm_len - mdm_start);
					//print_pkt(mdm_inbuf + mdm_start, mdm_len - mdm_start);
				}
			}
			if (ret < 0)
				ret = 0;
			mdm_start += ret;
			if (mdm_start >= mdm_len)
			{
				mdm_len = mdm_start = 0;
			}
		}
		if (writeMask &2)//BITR_SERIAL_PORT
		{
			(void) memset(tmp, 0x00, AT_RETRY_BUFFSIZE);
			(void) memcpy(tmp, ser_inbuf + ser_start, ser_len - ser_start);
			syslog(LOG_DEBUG,"PingPongSerialModem(P) : --> %s", tmp);
			ret = write(fd_modem, ser_inbuf + ser_start, ser_len - ser_start);
			//print_pkt(ser_inbuf + ser_start, ser_len - ser_start);
			 // check ^Z and Esc, only for AT+CMGS command.
			if (sms_cmd && (ser_inbuf[ser_len-1] == 26 || ser_inbuf[ser_len-1] == 27)) {
				syslog(LOG_DEBUG,"<---- PingPongSerialModem() #2");
				keep_alive_cnt = 0;
				return  ser_inbuf[ser_len-1];
			}
			if (ret < 0)
				ret = 0;
			ser_start += ret;
			if (ser_start >= ser_len)
			{
				ser_len = ser_start = 0;
			}
		}
		if (readMask &1) //BITR_AT_PORT
		{
			ser_len = read(fd_serial, ser_inbuf, sizeof(ser_inbuf));
			if (ser_len < 0)
				ser_len = 0;
			ser_inbuf[ser_len] = 0;
			syslog(LOG_DEBUG,"PingPongSerialModem(S) : <-- %s", ser_inbuf);
			//print_pkt(ser_inbuf, ser_len);

			/* copy input string from serial port to wipe out echo response */
			if (ser_len > 0) {
				(void) memset(lcmd_buf, 0x00, AT_RETRY_BUFFSIZE);
				(void) memcpy(lcmd_buf, ser_inbuf, ser_len);
				cmd_len = ser_len;
			}

			if ((confv250.opt_1 &ECHO_ON) && (ser_len)) {
				/* do not echo non-printerable characters to TE */
				for (i = 0; i < ser_len; i++) {
					bufc = ser_inbuf[i];
					/* BS / TAB / LF / CR or other printable characters */
					if (bufc >= 32 || bufc == 8 || bufc == 9 || bufc == 10 || bufc == 13) {
						write(fd_serial, (char *)&ser_inbuf[i], 1);
					}
				}
				//write(fd_serial, ser_inbuf, ser_len);
				syslog(LOG_DEBUG,"PingPongSerialModem(S) #2: --> %s", ser_inbuf);
				//print_pkt(ser_inbuf, ser_len);
			}
		}
		if (readMask &2)//BITR_SERIAL_PORT
		{
			mdm_len = read(fd_modem, mdm_inbuf, sizeof(mdm_inbuf));
			mdm_inbuf[mdm_len] = 0;
			syslog(LOG_DEBUG,"PingPongSerialModem(P) : <-- len %d, %s", mdm_len, mdm_inbuf);
			//print_pkt(mdm_inbuf, mdm_len);

			/* wipe out response string from phone module */
			if (cmd_len > 0 && strncmp(mdm_inbuf, lcmd_buf, cmd_len) == 0) {
				syslog(LOG_DEBUG,"PingPongSerialModem(P) : === found response, wipe out");
				if (mdm_len > cmd_len) {
					(void) memcpy(tmp, mdm_inbuf, AT_RETRY_BUFFSIZE);
					(void) memset(mdm_inbuf, 0x00, AT_RETRY_BUFFSIZE);
					(void) memcpy(mdm_inbuf, (char *)&tmp[cmd_len], mdm_len - cmd_len);
					mdm_len -= cmd_len;
				} else {
					mdm_len = 0;
				}
			}
			//mdm_inbuf[mdm_len] = 0;
			//fprintf (stderr, __func__ " %s", mdm_inbuf);
			if (mdm_len < 0)
				mdm_len = 0;
			// exit immediately after writing command and wiping out its echo if tx only mode
			if (tx_only && mdm_len <= 0) {
				syslog(LOG_DEBUG,"<---- PingPongSerialModem() #4");
				keep_alive_cnt = 0;
				return 0;
			}
		}
	}
	syslog(LOG_DEBUG,"<---- PingPongSerialModem() #3");
	keep_alive_cnt = 0;
}

// Sends a command to the phone module. There is a background
// thread which will redirect the response back to the DTE.
// Cmd is an AT command without the AT if you know what I mean,
// for example, "AT+CSQ" would just be "+CSQ". Also note there is
// no CR on the end of the command.
void ATPassthrough(FILE* serial, FILE* phone, char* Cmd, int timeout, int tx_only)
{
	char cmd_buf[512] = {0, };
	pausePassAt(1);
	if (phone != NULL)
	{
	    keep_alive_cnt = 0;
		sprintf(cmd_buf, "AT%s\r", Cmd);
		syslog(LOG_DEBUG,"ATPassthrough() : --> %s", cmd_buf);
		fputs(cmd_buf, phone);
		//fputs("\r", phone);
		fflush(phone);
		//strncat(cmd_buf, "\r", 1);

		if (serial)
		{
			PingPongSerialModem(fileno(serial), fileno(phone), timeout, (char *)&cmd_buf, tx_only);
		}
	}
	if (pastThreadPaused)
		pastThreadPaused--;
}


static char* makePadModeStr(int mode)
{
	switch (mode)
	{
		case PAD_DISABLED:
			return "DISABLED";
		case PAD_TCP:
			return "TCP";
		case PAD_UDP:
			return "UDP";
	}
	return "UNK";
}

char CDCS_V250SIMOCOConnect(FILE* stream, char* in_remote)
{
//TODO:match 888
	unsigned long remad;
	unsigned long to;

	// The host MUST dial a number
	if (!in_remote || * in_remote == '\0' || !strcmp(in_remote, currentProfile->rhost))
	{
		return ERROR;
	}

	remad = remoteAsNumber(currentProfile->rhost);
	if ((remad == 0xFFFFFFFF) || (remad == 0))
	{
		CDCS_V250UserResp(stream, "NO DIALTONE (NUMBER UNOBTAINABLE)");
		return NORESP;
	}

	CDCS_V250Resp(stream, OK);

	to = atol(in_remote);
	to = labs(to) % 1000;
	// Check for TMR SDM mode
	if (strchr(in_remote, '^'))
	{
		CDCS_V250UserResp_F(stream, "RING SDM BINARY INDIV %ld", to);

		CDCS_V250UserResp(stream, "CONNECT 1200");
		CDCS_V250Hangup(stream);

		return NORESP;
	}
	CDCS_V250UserResp_F(stream, "RING NPD %ld 001234", to);
	CDCS_V250UserResp(stream, "CONNECT 1200");
	currentConnectType = V250_DIALPORT_PROFILE;
	nModemState = ON_LINE;
	return NORESP;
}

char* makeRemoteDisplay(char* buf, unsigned blen, struct dialDetails* dd)
{
	if (blen < 3)
		return "";
	buf[blen-1] = '\0';
	snprintf(buf, blen - 1, "%s:%u:%s", dd->rhost, dd->padp, makePadModeStr(dd->padm));
	return buf;
}

int set_tcp_options(int TCPsock)
{
int optval;
socklen_t optlen = sizeof(optval);
char *p_value;

	if(currentProfile->tcp_nodelay) {
		if( setsockopt(TCPsock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0 )
			syslog(LOG_ERR, "setsockopt() tcp_nodelay failed");
		else
			syslog(LOG_INFO, "setsockopt() tcp_nodelay success");
	}


   /* Check the status for the keepalive option */
	if(getsockopt(TCPsock, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0) {
		perror("getsockopt()");
		close(TCPsock);
		syslog(LOG_DEBUG, "Check the status for the keepalive option failed");
		return 1;
	}
	/*else
		syslog(LOG_INFO, "SO_KEEPALIVE is %s\n", (optval ? "ON" : "OFF"));*/

	/* Set the keepalive option */
	p_value = getSingle( "system.config.tcp_keepalive_enable" );
	if(*p_value==0 || atoi(p_value)==1)
		optval = 1;
	else
		optval = 0;
	optlen = sizeof(optval);
	if(setsockopt(TCPsock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
		perror("setsockopt()");
		close(TCPsock);
		syslog(LOG_INFO, "Set the option active failed");
		return 1;
	}

	/* Check the status again */
	if(getsockopt(TCPsock, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0) {
		perror("getsockopt()");
		close(TCPsock);
		syslog(LOG_DEBUG, "Check the status for the keepalive failed");
		return 1;
	}
	else
		syslog(LOG_DEBUG, "TCP socket %i, KEEPALIVE is %s", TCPsock, (optval ? "ON" : "OFF"));
	return 0;
}

char CDCS_V250PADConnect(FILE* stream, struct dialDetails* dd)
{
	struct hostent* he;
	static int tcpConnectStatus = 255;
	char identbuf[128];
	int identlen;

	// Reset DNP timer
	nDnpTimer = 0;
	currDialDetails = *dd;
	if (confv250.emul == EMUL_SIMOCO)
	{
		return CDCS_V250SIMOCOConnect(stream, dd->rhost);
	}
////	setSerialPhoneNumber(makeRemoteDisplay(identbuf, sizeof(identbuf), dd), "PAD");
	if (dd->padm == PAD_DISABLED)
	{
		// Call Custom applications here
		return NORESP;
	}
	if ((currentProfile->pado &LENC_ADDRSER) == 0)
	{
		// Resolve host
		if ((he = gethostbyname(dd->rhost)) == 0)
		{
			CDCS_V250UserResp(stream, "UNABLE TO RESOLVE HOST");
			tcpConnectStatus = 0;
			return NODIALTONE;
		}
		memcpy(&RemoteAddr, he->h_addr_list[0], 4);
		if (dd->padm == PAD_TCP)
		{
			if (TCPPadSock != -1)
			{
				close(TCPPadSock);
				TCPPadSock = -1;
			}
			TCPPadSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (TCPPadSock == -1)
			{
				tcpConnectStatus = 0;
				return NODIALTONE;
			}

			set_tcp_options( TCPPadSock );

			TCPRemoteAddr.sin_family = AF_INET;
			TCPRemoteAddr.sin_addr.s_addr = RemoteAddr.s_addr;
			TCPRemoteAddr.sin_port = htons(dd->padp);
			// Connect to Host
			if (connect(TCPPadSock, (struct sockaddr*) &TCPRemoteAddr, sizeof(TCPRemoteAddr)) == -1)
			{
//syslog(LOG_ERR, "connect to TCPPadSock error: %s", strerror(errno));
				close(TCPPadSock);
				TCPPadSock = -1;
				tcpConnectStatus = 0;
				if(currentProfile->pado &PAD_AUTOANS) {
					return OK;
				}
				else {
					return NOANSWER;
				}
			}
			else
			{
				tcpConnectStatus = 1;
//syslog(LOG_INFO, "----tcpConnectStatus = 1----");
				if (confv250.ident[0])
				{
					identlen = unescape_to_buf(identbuf, confv250.ident, sizeof(identbuf) - 4);
					syslog(LOG_INFO, "confv250.ident[0] = %02x identlen = %d", confv250.ident[0], identlen);
					send(TCPPadSock, identbuf, identlen, 0);

				}
			}
		}
		else if (dd->padm == PAD_UDP && confv250.ident[0])
		{
			//identlen = unescape_to_buf(identbuf, ident, sizeof(identbuf) - 4);
			identlen = unescape_to_buf(identbuf, confv250.ident, sizeof(identbuf) - 4);
			if (UDPPadSock == -1)
			{
				UDPPadSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

				memset(&UDPLocalAddr, 0, sizeof(UDPLocalAddr));
				UDPLocalAddr.sin_family = AF_INET;
				UDPLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
				UDPLocalAddr.sin_port = htons(dd->padp);

				if (bind(UDPPadSock, (struct sockaddr*) &UDPLocalAddr, sizeof(UDPLocalAddr)) == -1)
				{
					syslog(LOG_INFO, "Failed to bind to UDP port");
				}
			}
			UDPRemoteAddr.sin_family = AF_INET;
			UDPRemoteAddr.sin_addr.s_addr = RemoteAddr.s_addr;
			UDPRemoteAddr.sin_port = htons(dd->padp);
			sendto(UDPPadSock, identbuf, identlen, 0, (struct sockaddr*) &UDPRemoteAddr, sizeof(struct sockaddr_in));
		}
	}
	currentConnectType = V250_DIALPORT_PROFILE;
	CDCS_V250ConnectMessage(stream);
	nModemState = ON_LINE;
	return NORESP;
}

char CDCS_V250CircuitConnect(FILE* stream, char* remote)
{
	char cmd_buf[64];
	if (comm_phat == NULL)
	{
		return NODIALTONE;
	}
	if (remote == NULL || * remote == '\0')
	{
		return NODIALTONE;
	}

	currentConnectType = V250_DIALPORT_CIRCUIT;
	////setSerialPhoneNumber(remote, "CIRCUIT");
	chan_signals[IND_AT_PORT] &= ~S_DV;
	nModemState = CSD_DIAL;
	//fputs("ATDT", comm_phat);
	//fputs(remote, comm_phat);
	//fputs("\r", comm_phat);
	//fflush(comm_phat);
	sprintf(cmd_buf, "DT%s", remote);
	ATPassthrough(comm_host, comm_phat, cmd_buf, AT_TIMEOUT, 1);
	fprintf(comm_host, "\r\n");
	return NORESP;
}

char CDCS_V250PacketConnect(FILE* stream, char* remote)
{
	char cmd_buf[64];
	if (comm_phat == NULL)
	{
		return NODIALTONE;
	}
	if (remote == NULL || * remote == '\0')
	{
		return NODIALTONE;
	}
	if (getSingleVal("confv250.PPPoEnable") || isAnyProfileEnabled() )
	{
		return BUSY;
	}
	currentConnectType = V250_DIALPORT_PACKET;
////	setSerialPhoneNumber(remote, "PACKET");
	nModemState = CSD_DIAL;
	chan_signals[IND_AT_PORT] &= ~S_DV;

	//fputs("ATDT", comm_phat);
	//fputs(remote, comm_phat);
	//fputs("\r", comm_phat);
	//fflush(comm_phat);
	sprintf(cmd_buf, "DT%s", remote);
	ATPassthrough(comm_host, comm_phat, cmd_buf, AT_TIMEOUT, 1);

	return NORESP;
}

int whichPortToUse(int defPort)
{
	int portToUse;
	if (confv250.emul == EMUL_SIMOCO)
		return V250_DIALPORT_PROFILE;
	portToUse = currentConnectType;
	if (portToUse == V250_DIALPORT_DIALSTR)
		portToUse = confv250.dialPort;
	if (portToUse == V250_DIALPORT_DIALSTR)
		portToUse = defPort;
	return portToUse;
}

int decodePadM(char* str)
{
	char* cp;
	for (cp = str;* cp;cp++)
	{
		*cp = toupper(*cp);
	}
	if (strstr("DISABLE", str))
		return PAD_DISABLED;
	if (strstr("TCP", str))
		return PAD_TCP;
	if (strstr("UDP", str))
		return PAD_UDP;
	return PAD_DISABLED;
}

void copyProfileToDialDetails(struct dialDetails* dd, int pnum)
{
	if (pnum > 0)
	{
		currentProfileIx = pnum;
		CDCS_getWWANProfile(currentProfile, currentProfileIx);
	}
	SAFE_COPY(dd->rhost, currentProfile->rhost);
	dd->padm = currentProfile->padm;
	dd->padp = currentProfile->padp;
	dd->profileNumber = currentProfileIx;
}

int padDetailsOK(struct dialDetails* dd)
{
	if (dd->rhost[0] && dd->padp)
	{
		dd->portToUse = V250_DIALPORT_PROFILE;
		return 0;
	}
	syslog(LOG_INFO, "Unable establish PAD session: bad host %s or port %u", dd->rhost, dd->padp);
	return -1;
}

int decodeRemotePAD(struct dialDetails* dd, char* remote)
{
	u_short argn;
	char** argv;

	copyProfileToDialDetails(dd, -1);
	if (!remote || * remote == '\0')
	{
		return padDetailsOK(dd);
	}
	argv = StrSplit(&argn, remote, ':');
	if (!argv)
		return padDetailsOK(dd);
	SAFE_COPY(dd->rhost, argv[0]);
	if (argn > 1)
	{
		dd->padp = atoi(argv[1]);
		if (argn > 2)
		{
			dd->padm = decodePadM(argv[2]);
		}
	}
	free(argv);
	return padDetailsOK(dd);
}

char* spaceTrim(char* str)
{
	int slen;
	int i;
	slen = strlen(str);
	while (--slen >= 0)
	{
		if (isspace(str[slen]))
		{
			str[slen] = '\0';
			continue;
		}
		break;
	}
	for (slen = 0;str[slen];slen++)
	{
		if (isspace(str[slen]))
		{
			continue;
		}
		break;
	}
	if (slen)
	{
		for (i = 0;str[slen+i] != '\0';i++)
			str[i] = str[slen+i];
		str[i] = '\0';
	}
	return str;
}

int allDigits(char* str)
{
	while (*str)
	{
		if (!isdigit(*str))
			return 0;
		str++;
	}
	return 1;
}

int IsRegExMatch(const char* lpszStr, char* lpszPat)
{
	int    status;
	regex_t    re;

	if (regcomp(&re, lpszPat, REG_EXTENDED | REG_NOSUB) != 0)
		return 0;

	status = regexec(&re, lpszStr, (size_t) 0, NULL, 0);
	regfree(&re);

	if (status != 0)
		return 0;

	return 1;
}

int decodeDialString(char* remote, struct dialDetails* dd)
{
	char str[512];
	char* p;
	int portToUse;
	int prof;
	int hasT;
	char autoDialNumber[64];
	int myremote=0;

	if(remote)
		myremote=atoi(remote);
	if( myremote>0 && myremote<=6 )
	{
		sprintf(str, "link.profile.%u.dialstr", atoi(remote));
		strncpy( autoDialNumber, getSingle(str), sizeof(autoDialNumber));
		currentProfileIx=myremote;
	}
	else if (confv250.autoDialNumber[0] != '\0')
	{
		strncpy(autoDialNumber, confv250.autoDialNumber, sizeof(confv250.autoDialNumber));
	}
	/*else
	{
		return -1;
	}*/
	portToUse = whichPortToUse(V250_DIALPORT_DIALSTR);
	str[0] = '\0';
	if (remote && * remote != '\0')
	{
		SAFE_COPY(str, remote);
	}
	else if ( *autoDialNumber != '\0' )
	{
		SAFE_COPY(str, autoDialNumber);
	}
	else
	{
		switch (portToUse)
		{
			case V250_DIALPORT_DIALSTR:
			case V250_DIALPORT_PROFILE:
				copyProfileToDialDetails(dd, -1);
				return padDetailsOK(dd);
		}
		return -1;
	}
	spaceTrim(str);
	p = str;
	hasT = 0;
	if (*p == 't' || * p == 'T')//Or 'p'
	{
		p++;
		hasT = 1;
	}
	switch (portToUse)
	{
		case V250_DIALPORT_DIALSTR:
			if (strlen(p) == 1)
			{
				if (isdigit(p[0]))
				{
					prof = p[0] - '0';
					if (prof == 0 || prof > 6)
						return -1;
					//prof--;
				}
				else if (p[0] == '*')
				{
					prof = -1;
				}
				else
					return -1;
				copyProfileToDialDetails(dd, prof);
				return padDetailsOK(dd);
			}
			if (allDigits(p))//  or','
			{
				SAFE_COPY(dd->rhost, p);
				dd->portToUse = V250_DIALPORT_CIRCUIT;
				return 0;
			}

			if (IsRegExMatch(p, "\\A(\\*99|\\*98|\\#777)"))
			{
				SAFE_COPY(dd->rhost, p);
				dd->portToUse = V250_DIALPORT_PACKET;
				return 0;
			}

			if (IsRegExMatch(p, "(\\.|:|[A-Z,a-z])"))
			{
			//	if (hasT && isalpha(*p))
			//		p--;
				return decodeRemotePAD(dd, p);
			}
			SAFE_COPY(dd->rhost, p);
			dd->portToUse = V250_DIALPORT_CIRCUIT;
			return 0;
		case V250_DIALPORT_PROFILE:
			return decodeRemotePAD(dd, p);
		case V250_DIALPORT_CIRCUIT:
			SAFE_COPY(dd->rhost, p);
			dd->portToUse = V250_DIALPORT_CIRCUIT;
			return 0;
		case V250_DIALPORT_PACKET:
			SAFE_COPY(dd->rhost, p);
			dd->portToUse = V250_DIALPORT_PACKET;
			return 0;
	}
	return -1;
}

// check to see if there is something in the host port or not
int isHostPortQueueEmpty(void)
{
	struct timeval tvZero = {0, };

	int hHost = fileno(comm_host);
	fd_set fdRead;

	int succ;

	// build read fd set
	FD_ZERO(&fdRead);
	FD_SET(hHost, &fdRead);

	// select
	succ = select(hHost + 1, &fdRead, NULL, NULL, &tvZero);

	return succ == 0;
}

// eat data in host port
int eatHostPortDataUntilEmpty(void)
{
	return eatDataUntilEmpty(fileno(comm_host));
}

// eat data in phone AT port
int eatPhonePortDataUntilEmpty(void)
{
	if(!comm_phat)
		return 0;

	return eatDataUntilEmpty(fileno(comm_phat));
}

// ATD Command Handler
char* CDCS_V250Cmd_D(FILE* stream, char* params, char* stat)
{
#define MAX_3G_WAITTING_TIME 120
	int i=0;
	struct dialDetails dd;
	char buf[32];

	if (nModemState == ON_LINE_COMMAND)
	{
		*stat = ERROR;
		params += strlen(params);
		return params;
	}
	params++;
	if (decodeDialString(params, &dd) < 0)
	{
		*stat = ERROR;
		params += strlen(params);
		return params;
	}
	*stat = NORESP;

	switch (dd.portToUse)
	{
		case V250_DIALPORT_PROFILE:
			confv250.opt_gasc=currentProfileIx;
			setSingleVal( "link.profile.profilenum", confv250.opt_gasc );
			setSingleVal( "webinterface.profileIdx", confv250.opt_gasc-1 );
			if ( !isAnyProfileEnabled() )
			{
				syslog(LOG_INFO, "Autocon %s %s", currentProfile->dial_number, currentProfile->apn_name);
				currentProfile->pado |= GPRS_AUTO_CON;
				sprintf(buf, "link.profile.%d.enable", dd.profileNumber );
				setSingleVal(buf, 1);
				setSingleVal("admin.remote.pad_encode", dd.padp);
				// don't return until the WWAN is up
				while (!WWANOpened())
				{
					CDCS_Sleep(1000);
					if ( !isHostPortQueueEmpty() && (++i > MAX_3G_WAITTING_TIME) )
					{
						eatHostPortDataUntilEmpty();
						*stat = NOCARRIER;
						params += strlen(params);
						return params;
					}
				}
				CDCS_Sleep(1000);
				*stat = CDCS_V250PADConnect(stream, &dd);
			}
			else if( isAnyProfileEnabled()==currentProfileIx )
			{
				currentProfile->pado |= GPRS_AUTO_CON;
				*stat = CDCS_V250PADConnect(stream, &dd);
			}
			else
				*stat = BUSY;
			// if we are configured OK, i.e PAD mode is either TCP or UDP.....
			if ((*stat != NORESP) && ( isAnyProfileEnabled() == 0))
			{
				;//.....then disable auto_attach since gasc is set to 0
			}
			break;
		case V250_DIALPORT_CIRCUIT:
			/* 1 */
			/*if (confv250.opt_gasc)  // atd*99# ???
			{
			  *stat = BUSY;
			}
			//else*/
			if (DTR_ON && (confv250.opt_dtr == V250_DTR_COMMAND || confv250.opt_dtr == V250_DTR_HANGUP))
			{
				*stat = ERROR;
			}
			else
				*stat = CDCS_V250CircuitConnect(stream, dd.rhost);
			break;
		case V250_DIALPORT_PACKET:
			/* 2 */
			if (DTR_ON && (confv250.opt_dtr == V250_DTR_COMMAND || confv250.opt_dtr == V250_DTR_HANGUP))
			{
				*stat = ERROR;
			}
			else
				*stat = CDCS_V250PacketConnect(stream, dd.rhost);
			break;
	}
	params += strlen(params);
	return params;
}

void CDCS_V250SaveConfig(void)
{
	// NOTE - flow control is not properly implemented at the moment, so comment this out,
	// but when it is this needs to be put back in, but things like comm_host should not really
	// be in this file so need to think about this.
	int            fd;
	struct termios tty_struct;
	char temp;
	char buf[32];

	fd = fileno(comm_host);
	tcgetattr(fd, &tty_struct);
	confv250.opt_fc = 0;
	tcgetattr(fd, &tty_struct);
	if (tty_struct.c_cflag & CRTSCTS)
	{
	confv250.opt_fc = 0x02;
	confv250.opt_fc |= 0x20;
	}
	setSingleVal("confv250.Option1", confv250.opt_1);
	setSingleVal("confv250.Option2", confv250.opt_2);
	setSingleVal("confv250.emul", confv250.emul);

	temp = getSingleVal( "link.profile.profilenum" );
	sprintf( buf,"link.profile.%u.enable", temp );
	setSingleVal( buf, (confv250.opt_gasc==0)? 0:1);

	setSingleVal("confv250.opt_dtr", confv250.opt_dtr);
	setSingleVal("confv250.opt_dsr", confv250.opt_dsr);
	setSingleVal("confv250.opt_rts", confv250.opt_rts);
	setSingleVal("confv250.opt_cts", confv250.opt_rts);
	setSingleVal("confv250.opt_dcd", confv250.opt_dcd);
	setSingleVal("confv250.opt_ri", confv250.opt_ri);
	setSingleVal("confv250.opt_fc", confv250.opt_fc);
	setSingleVal("confv250.Inter_Character_Timeout", confv250.icto);
	setSingleVal("confv250.Idle_Disconnect_Timeout", confv250.idct);
	setSingleVal("confv250.Maximum_Session_Timeout", confv250.sest);
	setSingleVal("confv250.Baud_rate", confv250.ipr);
	setSingleVal("confv250.Format", confv250.format);
	setSingleVal("confv250.Parity", confv250.parity);
	setSingleVal("confv250.dialPort", confv250.dialPort);
	setSingleVal("confv250.modemAutoAnswer", confv250.modemAutoAnswer);
	setSingleVal("confv250.Appt1", confv250.appt1);
	setSingleVal("confv250.ATS7var", confv250.s7_timer);
	rdb_set_single("confv250.Ident", confv250.ident);
	rdb_set_single("confv250.ignoreStr", confv250.ignoreStr);
	rdb_set_single("confv250.sHost", confv250.shost);
	rdb_set_single("confv250.autoDialNumber", confv250.autoDialNumber);

}

char* CDCS_V250Cmd_H(FILE* stream, char* params, char* stat)
{
	params++;
	if (isdigit(*params))
		params++;
	if (nModemState == ON_LINE_COMMAND)
	{
		*stat = NORESP;
		CDCS_V250Hangup(stream);
	}
	return params;
}

char* CDCS_V250Cmd_A(FILE* stream, char* params, char* stat)
{
	FILE* fp;
	int portToUse;

	switch (portToUse = whichPortToUse(V250_DIALPORT_CIRCUIT))
	{
		case V250_DIALPORT_PROFILE:
			fp = comm_phat;
			break;
		case V250_DIALPORT_CIRCUIT:
			/*if (confv250.opt_gasc)
			  fp = 0;
			else*/
			fp = comm_phat;
			break;
		case V250_DIALPORT_PACKET:
			fp = comm_phat;
			break;
		default:
			*stat = NORESP;
			return ++params;
	}

	if (fp)
	{
		//fputs("ATA\r", fp);
		//fflush(fp);
		ATPassthrough(comm_host, fp, "A", AT_TIMEOUT, 1);
		*stat = NORESP;
	}
	else
	{
		*stat = BUSY;
	}

	return ++params;
}

char* CDCS_V250Cmd_O(FILE* stream, char* params, char* stat)
{
	if (nModemState == ON_LINE_COMMAND)
	{
		CDCS_V250ConnectMessage(stream);
		nModemState = ON_LINE;
	}
	else
		*stat = NOCARRIER;

	return ++params;
}

char* CDCS_V250StringParam(FILE* stream, char* params, char* rslt, char* addr, const char* stat, const char* help, u_short len)
{
	char eq = 0;

	params++;
	if (*params == '=')
	{
		eq = 1;
		params++;
	}
	if (*params == '?')
	{
		if (eq == 1)
			CDCS_V250UserResp_F(stream, help);
		else
			CDCS_V250UserResp_F(stream, stat, addr);
		return ++params;
	}
	if (eq)
	{
		strncpy(addr, params, len - 1);
		addr[len-1] = 0;
		*rslt = UPDATEREQ;
		params += strlen(params);
		return params;
	}
	*rslt = ERROR;
	return params;

}
char* CDCS_V250CharParam(FILE* stream, char* params, char* rslt, u_char* addr, const char* stat, const char* help, u_short min, u_short max)
{
	char eq = 0;
	char dig = 0;
	char n;

	params++;

	if (*params == '=')
	{
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
	if (dig)
	{
		if ((n >= min) && (n <= max))
		{
			*addr = n;
		}
		else
			*rslt = ERROR;
		params--;
	}
	if (*params == '?')
	{
		if (eq == 1)
			CDCS_V250UserResp_F(stream, help, min, max);
		else
			CDCS_V250UserResp_F(stream, stat, * addr);
	}
	return ++params;
}

char* CDCS_V250ShortParam(FILE* stream, char* params, char* rslt, u_short* addr, const char* stat, const char* help, u_short min, u_short max)
{
	char eq = 0;
	char dig = 0;
	u_short n;

	params++;
	if (*params == '=')
	{
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
	if (dig)
	{
		if ((n >= min) && (n <= max))
		{
			//setSingleVal( addr, n );
			*addr = n;
		}
		else
			*rslt = ERROR;
		params--;
	}
	if (*params == '?')
	{
		if (eq == 1)
			CDCS_V250UserResp_F(stream, help, min, max);
		else
			CDCS_V250UserResp_F(stream, stat, * addr);
	}
	return ++params;
}

char* CDCS_V250LongHexParam(FILE* stream, char* params, char* rslt, u_char* addr, const char* stat, const char* help, u_long min, u_long max)
{
	char eq = 0;
	char dig = 0;
	u_long n;

	params++;
	if (*params == '=')
	{
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
	if (dig)
	{
		if ((n >= min) && (n <= max))
		{
			*addr = n;
		}
		else
			*rslt = ERROR;
		params--;
	}
	if (*params == '?')
	{
		if (eq == 1)
			CDCS_V250UserResp_F(stream, help, min, max);
		else
			CDCS_V250UserResp_F(stream, stat, * addr);
	}
	return ++params;
}

char* CDCS_V250BinaryParam(FILE* stream, char* params, char* rslt, u_char* addr, u_char mask, const char* stat, const char* help)
{
	char eq = 0;

	params++;
	if (*params == '=')
	{
		eq = 1;
		params++;
	}
	if (*params == '1')
		*addr |= mask;
	if (*params == '0')
		*addr &= ~mask;
	if (*params == '?')
	{
		if (eq == 1)
			CDCS_V250UserResp(stream, help);
		else
			CDCS_V250UserResp_F(stream, stat, ((*addr &mask) ? 1 : 0));
	}
	return ++params;
}

// Command Handlers
char* CDCS_V250Cmd_E(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250BinaryParam(stream, params, rslt, &confv250.opt_1, ECHO_ON, "E: %d", "E: (0-1)");
}

char* CDCS_V250Cmd_Q(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250BinaryParam(stream, params, rslt, &confv250.opt_1, QUIET_ON, "Q: %d", "Q: (0-1)");
}

char* CDCS_V250Cmd_V(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250BinaryParam(stream, params, rslt, &confv250.opt_1, VERBOSE_RSLT, "V: %d", "V: (0-1)");
}

char* CDCS_V250Cmd_CROK(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250BinaryParam(stream, params, rslt, &confv250.opt_1, OK_ON_CR, PRFX "CROK: %d", PRFX "CROK: (0-1)");
}

char* CDCS_V250Cmd_NOLF(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250BinaryParam(stream, params, rslt, &confv250.opt_1, SUPRESS_LF, PRFX "NOLF: %d", PRFX "NOLF: (0-1)");
}

int getDigitParams(char** pp)
{
	int val = 0;
	char* params = * pp;
	if (!isdigit(*params))
		return 0;
	while (isdigit(*params))
	{
		val *= 10;
		val += * params - '0';
		params++;
	}
	*pp = params;
	return val;
}

char* CDCS_V250Cmd_S(FILE* stream, char* params, char* rslt)
{
	int val;
	u_char dummy;
	params++;
	val = getDigitParams(&params);

	if (val == 0)
		// Auto Answer Rings
	{
		--params;
		params = CDCS_V250CharParam(stream, params, rslt, &confv250.modemAutoAnswer, "%03d", "", 0, 255);
		checkModemAutoAnswer(1);
	}
	if (val == 7)
		// Connection Completion Timeout
	{
		--params;
		params = CDCS_V250CharParam(stream, params, rslt, &confv250.s7_timer, "%03d", "", 0, 255);
	}
	if (val == 8)
		//Comma Dial Modifier Time
	{
		--params;
		params = CDCS_V250CharParam(stream, params, rslt, &dummy, "%03d", "", 0, 255);
	}
	return params;
}

char * getVersion(char *buffer)
{
	FILE * fp;
	int i = 0;
	int temp;
	fp = fopen("/etc/version.txt", "r");
	sprintf(buffer, "-");
	if (fp == NULL)
	{
		return buffer;
	}
	while ((temp = fgetc(fp)) != EOF)
	{
		*(buffer + i) = (char)temp;
		if (iscntrl(*(buffer + i)) || (i > 20))
		{
			break;
		}
		i++;
	}
	*(buffer + i) = 0;
	fclose(fp);
	return buffer;
}

char* CDCS_V250Cmd_I(FILE* stream, char* params, char* stat)
{
	char version[20];
	int val;
	params++;
	val = getDigitParams(&params);
	switch (val)
	{
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

char* CDCS_V250Cmd_ICTO(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250ShortParam(stream, params, rslt, &confv250.icto, PRFX "ICTO: %d", PRFX "ICTO: (%u-%u)", 0, 255);
}

char* CDCS_V250Cmd_IDCT(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250ShortParam(stream, params, rslt, &confv250.idct, PRFX "IDCT: %d", PRFX "IDCT: (%u-%u)", 0, 65535);
}

char* CDCS_V250Cmd_SEST(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250ShortParam(stream, params, rslt, &confv250.sest, PRFX "SEST: %d", PRFX "SEST: (%u-%u)", 0, 65535);
}

char* CDCS_V250Cmd_PRT(FILE* stream, char* params, char* rslt) //Periodic reset timer
{
	params++;
	char *pos;
	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			CDCS_V250UserResp_F(stream, PRFX "PRT: (%u-%u)", 0, 65535);
			params += strlen(params);
			return params;
		}
		setSingleVal("service.systemmonitor.forcereset", atoi(params));
	}
	else if (*params == '?')
	{
		fprintf(stream, "\r\n");
		pos = getSingle("service.systemmonitor.forcereset");
		if(*pos==0)
			pos="0";
		fprintf(stream, PRFX "PRT: %s \r\n", pos );
	}
	params += strlen(params);
	return params;
}


char* CDCS_V250Cmd_EMUL(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250CharParam(stream, params, rslt, &confv250.emul, PRFX "EMUL: %d", PRFX "EMUL: (%u-%u)", 0, EMUL_HIGHEST);
}


char* CDCS_V250Cmd_COPT(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250CharParam(stream, params, rslt, &confv250.opt_2, PRFX "COPT: %d", PRFX "COPT: (%u-%u)", 0, 3);
}


char* CDCS_V250Cmd_AmpW(FILE* stream, char* params, char* rslt)
{
	//TODO check this function
	CDCS_V250SaveConfig();
	params += 2;
	return params;
}


char* CDCS_V250Cmd_AmpF(FILE* stream, char* params, char* rslt)
{
	CDCS_V250SetDefaults();
	params += 2;
	return params;
}

//DTR Configuration
char* CDCS_V250Cmd_AmpD(FILE* stream, char* params, char* rslt)
{
	params = CDCS_V250CharParam(stream, params, rslt, &confv250.opt_dtr, "&D: %d", "&D: (%u-%u)", 0, 8);
	return params;
}

//DSR Configuration
char* CDCS_V250Cmd_AmpS(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250CharParam(stream, params, rslt, &confv250.opt_dsr, "&S: %d", "&S: (%u-%u)", 0, 4);
}

//RTS Configuration
char* CDCS_V250Cmd_ANDR(FILE* stream, char* params, char* rslt)
{
	char* res;
	int fd;
	struct termios tty_struct;

	fd = fileno(comm_host);
	tcgetattr(fd, &tty_struct);
	res = CDCS_V250CharParam(stream, params, rslt, &confv250.opt_rts, "&R: %d", "&R: (%u-%u)", 0, 2);
	if (tty_struct.c_cflag &CRTSCTS)
	{
		confv250.opt_fc = 0x02;
		confv250.opt_fc |= 0x20;
	}
	if (confv250.opt_rts == 2)
		tty_struct.c_cflag |= CRTSCTS;
	else
		tty_struct.c_cflag &= ~CRTSCTS;
	tcsetattr(fd, TCSADRAIN, &tty_struct); /* set termios struct in driver */
	return res;
}

//CTS Configuration
char* CDCS_V250Cmd_ANDQ(FILE* stream, char* params, char* rslt)
{
	char* res;
	int fd;
	struct termios tty_struct;

	fd = fileno(comm_host);
	tcgetattr(fd, &tty_struct);

	res = CDCS_V250CharParam(stream, params, rslt, &confv250.opt_cts, "&Q: %d", "&Q: (%u-%u)", 0, 4);
	if (confv250.opt_cts == 2)
		tty_struct.c_cflag |= CRTSCTS;
	else
		tty_struct.c_cflag &= ~CRTSCTS;
	tcsetattr(fd, TCSADRAIN, &tty_struct); /* set termios struct in driver */
	return res;
}

// DCD Configuration
char* CDCS_V250Cmd_AmpC(FILE* stream, char* params, char* rslt)
{
	params = CDCS_V250CharParam(stream, params, rslt, &confv250.opt_dcd, "&C: %d", "&C: (%u-%u)", 0, 3);
	return params;
}

// RI Configuration
char* CDCS_V250Cmd_AmpN(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250CharParam(stream, params, rslt, &confv250.opt_ri, "&N: %d", "&N: (%u-%u)", 0, 2);
}

char* CDCS_V250Cmd_Z(FILE* stream, char* params, char* rslt)
{
	CDCS_V250Initialise(stream);
	return ++params;
}


char* CDCS_V250Cmd_IPR(FILE* stream, char* params, char* rslt)
{
	char strRate[8];
	speed_t current_speed;
	u_long r;
	int fd;
	struct termios tty_struct;
	speed_t b = 0;

	fd = fileno(comm_host);
	tcgetattr(fd, &tty_struct);
	current_speed = cfgetospeed(&tty_struct);
	sprintf(strRate, "%d", baud_to_int(current_speed));
	params = CDCS_V250StringParam(stream, params, rslt, strRate, "+IPR: %s", "+IPR: (300, 1200,2400,4800,9600,19200,38400,57600,115200,230400)", 8);
	if (*rslt == UPDATEREQ)
	{
		r = atol(strRate);
		// test if valid baud rate and map to required value
		switch (r)
		{
			case 300:
				b = B300;
				break;
			case 1200:
				b = B1200;
				break;
			case 2400:
				b = B2400;
				break;
			case 4800:
				b = B4800;
				break;
			case 9600:
				b = B9600;
				break;
			case 19200:
				b = B19200;
				break;
			case 38400:
				b = B38400;
				break;
			case 57600:
				b = B57600;
				break;
			case 115200:
				b = B115200;
				break;
			case 230400:
				b = B230400;
				break;
			default:
				CDCS_V250UserResp_F(stream, "Invalid Rate %ld", r);
				*rslt = ERROR;
				break;
		}
		if (b)
		{
			CDCS_V250Resp(stream, OK); // Send response before baud change
			cfsetospeed(&tty_struct, b);
			tcsetattr(fd, TCSADRAIN, &tty_struct); /* set termios struct in driver */
			// store the value confv250.ipr
			confv250.ipr = baud_to_int(b);
			*rslt = NORESP;
		}
	}
	return params;
}

void setTTYCharacterFormat(FILE* tty, char format, char parity)
{
	u_long n;
	int fd;
	struct termios tty_struct;

	fd = fileno(tty);
	tcgetattr(fd, &tty_struct);
	// Use a sensible default if format is not set correctly
	if (format == 0)
		format = 3;
	if (format > 7)
		format = 3;
	// Databits
	if (format < 4)
		n = CS8;
	else
		n = CS7;
	tty_struct.c_cflag = (tty_struct.c_cflag & ~CSIZE) | n;
	// Stop bits
	n = 0;
	if (format == 1)
		n = CSTOPB;
	if (format == 4)
		n = CSTOPB;
	tty_struct.c_cflag = (tty_struct.c_cflag & ~CSTOPB) | n;
	// Parity
	n = 0; // no parity
	if ((format == 2) || (format == 5))
	{
		if (parity == 0)
			n = PARENB | PARODD;
		// Odd
		if (parity == 1)
			n = PARENB;
		// Even
	}
	confv250.format = format;
	confv250.parity = parity;
	tty_struct.c_cflag = (tty_struct.c_cflag & ~(PARENB | PARODD)) | n;
	tcsetattr(fd, TCSADRAIN, &tty_struct); /* set termios struct in driver */
}

char* CDCS_V250Cmd_ICF(FILE* stream, char* params, char* rslt)
{
	u_short Num;
	char** P;
	char format = 0;
	char parity;
	int fd;
	struct termios tty_struct;

	fd = fileno(comm_host);
	tcgetattr(fd, &tty_struct);
	params++;
	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			CDCS_V250UserResp_F(stream, "+ICF: (1-6),(0-1)");
			params++;
			return params;
		}
		// Split up the string by commas
		P = StrSplit(&Num, params, ',');
		// We must have 1 or 2 params
		parity = 1;
		switch (Num)
		{
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
		if (*rslt != ERROR)
		{
			CDCS_V250Resp(stream, OK); // Send response before format change
			*rslt = NORESP;
			setTTYCharacterFormat(comm_host, format, parity);
		}
	}
	if (*params == '?')
	{
		format = 1;
		parity = 1;
		if ((tty_struct.c_cflag &CSIZE) == CS7)
			format = 4;
		if (tty_struct.c_cflag &PARENB)
		{
			format++;
			if (tty_struct.c_cflag &PARODD)
				parity = 0;
		}
		else if ((tty_struct.c_cflag &CSTOPB) == 0)
			format += 2;
		CDCS_V250UserResp_F(stream, "+ICF: %d,%d", format, parity);
		params++;
	}
	return params;
}

void setHostFlowType(void)
{
	static unsigned char save_fc;
	struct termios tty_struct;
	int fd;

	if (save_fc == confv250.opt_fc)
		return ;
	save_fc = confv250.opt_fc;
	fd = fileno(comm_host);
	tcgetattr(fd, &tty_struct);

	if (confv250.opt_fc &0x22)
	{
		/* either on is hardware */
		confv250.opt_cts = 2;
		confv250.opt_rts = 2;
		if (tty_struct.c_cflag &CRTSCTS)
			return ;
		tty_struct.c_cflag |= CRTSCTS;
	}
	else
	{
		confv250.opt_cts = 0;
		confv250.opt_rts = 0;
		if ((tty_struct.c_cflag &CRTSCTS) == 0)
			return ;
		tty_struct.c_cflag &= ~CRTSCTS;
	}
	tcsetattr(fd, TCSADRAIN, &tty_struct);
}

char* CDCS_V250Cmd_IFC(FILE* stream, char* params, char* rslt)
{
	u_short Num;
	char** P;
	char topfc;
	char lowfc;
	char flow_in = 0;
	char flow_out = 0;

	params++;
	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			CDCS_V250UserResp_F(stream, "+IFC: (0,0),(2,2)");
			params++;
			return params;
		}
		// Split up the string by commas
		P = StrSplit(&Num, params, ',');
		// We must have 1 or 2 params
		switch (Num)
		{
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
		if (*rslt != ERROR)
		{
			confv250.opt_fc = (flow_in & 0xf) | ((flow_out << 4) & 0xf0);
			setHostFlowType();
		}
	}
	if (*params == '?')
	{
		topfc = confv250.opt_fc & 0xf0;
		lowfc = confv250.opt_fc & 0x0f;
		CDCS_V250UserResp_F(stream, "+IFC: %d,%d", topfc >> 4, lowfc);
		params++;
		*rslt = OK;
	}
	return params;
}

char* CDCS_V250Cmd_CMGS(FILE* stream, char* Cmd, char* stat)
{
	int ret;
	char cmd_buf[512] = {0, };

    keep_alive_cnt = 0;
	pausePassAt(1);
	sprintf(cmd_buf, "AT%s\r", Cmd);
	syslog(LOG_DEBUG,"CDCS_V250Cmd_CMGS() : --> %s", cmd_buf);
	fputs(cmd_buf, comm_phat);
	//fputs("\r", comm_phat);
	fflush(comm_phat);
	while (1)
	{
		/* cmd_buf should keep its contents with CMGS */
		if ((ret = PingPongSerialModem(fileno(stream), fileno(comm_phat), 50, (char *)&cmd_buf, 0)) < 0)
			break;
		if ((ret == 26) || (ret == 27))
			break;
	}
	if (pastThreadPaused)
		pastThreadPaused--;
	*stat = NORESP;
	return Cmd + strlen(Cmd);
}

#ifdef V_ME_LOCAL_CMD_y
char* CDCS_V250Cmd_CSQ(FILE* stream, char* Cmd, char* stat)
{
	char *p_value;
    keep_alive_cnt = 0;
	p_value = getSingle( "wwan.0.radio.information.signal_strength.raw" );
	if (!p_value) {
		strcpy(p_value, "+CSQ: 99, 99");
	}
	CDCS_V250UserResp(stream, p_value);
	*stat = OK;
	return Cmd + strlen(Cmd);
}
#endif

char* CDCS_V250Cmd_BAND(FILE* stream, char* params, char* rslt)
{
	char dig;
	char *prvparams = params - 4;
	int n;

	char *bstr;

	params++;
	if (*params == '?')
	{
		return prvparams;
	}
	if (*params == '=')
	{
		params++;
	}
	dig = sscanf(params, "%x", &n); // Get value
	if ((!dig) || (n < 0) || (n > 13))
	{
		return prvparams; //do passthrough
	}
	switch (n)
	{
		case 0x05:
			bstr = "2G";
			break;
		case 0x08:
			bstr = "WCDMA All";
			break;
		case 0x0c:
			bstr = "UMTS 850Mhz,2G";
			break;
		case 0x0d:
			bstr = "UMTS 850Mhz Only";
			break;
		default:
			bstr = "Autoband";
	}
	rdb_set_single( "wwan.0.currentband.cmd.param.band", bstr);
	rdb_set_single( "wwan.0.currentband.cmd.command", "set");
	return prvparams; //do passthrough
}

char* CDCS_V250Cmd_NOER(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250BinaryParam(stream, params, rslt, &confv250.opt_1, OK_ON_UNKNOWN, "NOER: %d", "NOER: (0-1)");
}


char* CDCS_V250Cmd_GASC(FILE* stream, char* params, char* rslt)
{
	char buf[32];
	int profile_enable = isAnyProfileEnabled();

	params++;
	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			CDCS_V250UserResp_F(stream, PRFX "GASC: (%u-%u)", 0, 6);
			params += strlen(params);
		}
		else
		{
			params -= 2;
			// if PPPoE is running then this has control of the phone port
			// so we cannot start up the PPP daemon otherwise it will also try gain control of it
			// and won't be able to.
			if ( getSingleVal( "service.pppoe.server.0.enable" ))
			{
				*rslt = ERROR;
				fprintf(stream, "\r\n");
				fprintf(stream, "PPPoE mode selected\r\n");
				params += strlen(params);
				return params;
			}
			params = CDCS_V250CharParam(stream, params, rslt, &confv250.opt_gasc, PRFX "GASC: %d", PRFX "GASC: (%u-%u)", 0, 6);
			if (confv250.opt_gasc)
			{
				if(profile_enable)
				{
					if(profile_enable != confv250.opt_gasc)
					{
						*rslt = BUSY;
						fprintf(stream, "\r\n");
						fprintf(stream, "Profile%u already enabled\r\n", profile_enable);
						params += strlen(params);
						return params;
					}
				}
				else
				{
					setSingleVal( "link.profile.profilenum", confv250.opt_gasc );
					setSingleVal( "webinterface.profileIdx", confv250.opt_gasc );
					sprintf( buf, "link.profile.%u.enable", confv250.opt_gasc );
					setSingleVal( buf, 1 );
					setSingleVal( "webinterface.profileIdx", confv250.opt_gasc-1 );
				}
			}
			else if(profile_enable)
			{
				sprintf( buf,"link.profile.%u.enable", profile_enable );
				setSingleVal( buf, 0 );
			}
		}
	}
	else if (*params == '?')
	{
		CDCS_V250UserResp_F(stream, PRFX "GASC: %d", profile_enable);
		params += strlen(params);
	}
	return params;
}

char* CDCS_V250Cmd_IDNT(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250StringParam(stream, params, rslt, confv250.ident, PRFX "IDNT: %s", PRFX "IDNT: (\"\")", sizeof(confv250.ident));
}


// AT^GRST Resets the WWAN connection.
char* CDCS_V250Cmd_GRST(FILE* stream, char* params, char* rslt)
{
	CDCS_WWAN_Terminate();
	params += strlen(params);
	return params;
}


// AT^GLUP=<domain name> (i.e www.google.com) Looks up the given domain name.
char* CDCS_V250Cmd_GLUP(FILE* stream, char* params, char* rslt)
{
	struct hostent* hp;

	params++;
	if (*params == '=')
	{
		params++;
		// Look up the name
		hp = gethostbyname(params);
		// Check for lookup failure
		if (hp != 0)
		{
			// Print the result
			// replaced by Yong for Defect#20 (Incorrect GLUP)
			struct in_addr* inaddr = (struct in_addr*)(hp->h_addr);
			CDCS_V250UserResp_F(stream, PRFX "GLUP: %s", inet_ntoa(*inaddr));

			params += strlen(params);
			return params;
		}
		else
			CDCS_V250UserResp_F(stream, PRFX "GLUP: Lookup failed.");
	}
	// Error
	*rslt = ERROR;
	return params;
}


// This routine passes all at commands to the phone module and so there is no interception involved
// i.e v250 engine is bypassed.
void DoDirectPassThru(void)
{
	size_t got;
	char* inbuf;
	struct timeval tv;
	fd_set readset;
	int phoneFd;
	int hostFd;
	int highestFd;
	int ret;
	size_t wret;
	speed_t host_baud;
	int i;

	do
	{
		inbuf = (char*)malloc(PASSTHRUBUFFSIZE);
		if (inbuf == NULL)
			CDCS_Sleep(1000);
	}
	while (inbuf == NULL);
	host_baud = get_serial_port_baud(comm_host);
	//kmip_cns_ThreadPaused = 1;
	// Configure the M200 serial port baud rate to match the external baud rate
	// SetPhoneBaud(comm_host, host_baud);
	fprintf(comm_host, "\r\nOK\r\n");
	// drop response on the floor
	// NOTE - we have to do the read to flush out what is in comm_phat otherwise
	// upon doing the next read we will read this response plus the response to the next read.
	// fread(inbuf, 1, PASSTHRUBUFFSIZE - 1, comm_phat);
	// make sure that the PAST thread doesn't also try to interfere!....
	nModemState = DISABLE_V250;
	FD_ZERO(&readset);
	// Set the select timeout....
	tv.tv_sec = 0;
	tv.tv_usec = 10000000;
	phoneFd = fileno(comm_phat);
	hostFd = fileno(comm_host);

	// we have to work out which is the highest descriptor since the first argument
	// to select has to be the highest descriptor plus one.
	if (phoneFd > hostFd)
	{
		highestFd = phoneFd;
	}
	else
	{
		highestFd = hostFd;
	}
	for (;;)
	{
		FD_SET(phoneFd, &readset);
		FD_SET(hostFd, &readset);
		ret = select(highestFd + 1, &readset, (fd_set*)0, (fd_set*)0, &tv);
		// check for a timeout.........
		if (ret == 0)
		{
			continue;
		}
		if (ret != -1)
		{
			if (FD_ISSET(phoneFd, &readset))
			{
				// NOTE - the phone port is set up for non-blocking so we don't have to
				// wait for PASSTHRUBUFFSIZE - 1 bytes to be read from the stream.
				// Otherwise it will block until this many bytes are read.
				// Keep in a while loop here since if more than PASSTHRUBUFFSIZE bytes
				// are read then we want to keep reading...
				while ((got = fread(inbuf, 1, PASSTHRUBUFFSIZE - 1, comm_phat)) > 0)
				{
					wret = fwrite(inbuf, 1, got, comm_host);
					fflush(comm_host);
					syslog(LOG_DEBUG, "fd %d --> fd %d (DoDirectPassThru)", phoneFd, hostFd);
					print_pkt(inbuf, got);
				}
				i = 0;
			}
			if (FD_ISSET(hostFd, &readset))
			{
				while ((got = fread(inbuf, 1, PASSTHRUBUFFSIZE - 1, comm_host)) > 0)
				{
					wret = fwrite(inbuf, 1, got, comm_phat);
					fflush(comm_phat);
					wret = fwrite(inbuf, 1, got, comm_host); // 24/01/2008 by Aaron
					fflush(comm_host);
					syslog(LOG_DEBUG, "fd %d --> fd %d (DoDirectPassThru) &", phoneFd, hostFd);
					syslog(LOG_DEBUG, "fd %d --> fd %d (DoDirectPassThru)", hostFd, hostFd);
					print_pkt(inbuf, got);
				}
				i = 0;
				if (got == -1) {}
			}
		}
		else
			{}
	}
}


// This routine passes all at commands to the phone module and so there is no interception involved
// i.e v250 engine is bypassed.
void DoDirectPassThruToTE(void)
{
	int got;
	char inbuf[PASSTHRUBUFFSIZE];
	struct timeval tv;
	fd_set readset;
	int phoneFd;
	int hostFd;
	int highestFd;
	int ret;
	//speed_t    phone_baud;
	//char    strRate[8];

	syslog(LOG_DEBUG,"Entering DoDirectPassThruToTE()");

	cfg_serial_port(comm_phat, B115200);
	cfg_serial_port(comm_host, B115200);
	pastThreadPaused++;
	fprintf(comm_host, "\r\nOK\r\n");
	// make sure that the PAST thread doesn't also try to interfere!....
	nModemState = DISABLE_V250;
	phoneFd = fileno(comm_phat);
	hostFd = fileno(comm_host);
	// we have to work out which is the highest descriptor since the first argument
	// to select has to be the highest descriptor plus one.
	if (phoneFd > hostFd)
	{
		highestFd = phoneFd;
	}
	else
	{
		highestFd = hostFd;
	}
	for (;;)
	{
		FD_ZERO(&readset);
		// Set the select timeout....
		tv.tv_sec = 0;
		tv.tv_usec = 100;
		FD_SET(phoneFd, &readset);
		FD_SET(hostFd, &readset);
		ret = select(highestFd + 1, &readset, (fd_set*)0, (fd_set*)0, &tv);
		// check for a timeout.........
		if (ret == 0)
		{
			continue;
		}
		if (ret != -1)
		{
			if (FD_ISSET(phoneFd, &readset))
			{
				// NOTE - the phone port is set up for non-blocking so we don't have to
				// wait for PASSTHRUBUFFSIZE - 1 bytes to be read from the stream.
				// Otherwise it will block until this many bytes are read.
				// Keep in a while loop here since if more than PASSTHRUBUFFSIZE bytes
				// are read then we want to keep reading...
				if ((got = read(phoneFd, inbuf, PASSTHRUBUFFSIZE)) > 0) {
					syslog(LOG_DEBUG,"MDM --> TE : '%s'", inbuf);
					write(hostFd, inbuf, got);
				}
			}
			if (FD_ISSET(hostFd, &readset))
			{
				if ((got = read(hostFd, inbuf, PASSTHRUBUFFSIZE)) > 0) {
					syslog(LOG_DEBUG,"TE --> MDM : '%s'", inbuf);
					write(phoneFd, inbuf, got);
				}
			}
		}
	}
	/* doesn't actually get here */
	pastThreadPaused--;
	syslog(LOG_DEBUG,"Exiting DoDirectPassThruToTE()");
}

char* CDCS_V250Cmd_QCDMG(FILE* stream, char* params, char* rslt)
{
	fputs("AT+IPR=115200\r", comm_phat);
	fflush(comm_phat);
	DoDirectPassThru();
	return params;
}

char* CDCS_V250Cmd_PAST(FILE* stream, char* params, char* rslt)
{
	params++;
	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			CDCS_V250UserResp_F(stream, PRFX "PAST: (0-2)");
			params += strlen(params);
			return params;
		}
		// don't let the user set past != 0 if PPPoE is enabled since the PAST thread wil try
		// to use the phone port which PPPoE will currently have.
		if (getSingleVal("service.pppoe.server.0.enable"))
		{
			*rslt = ERROR;
			fprintf(stream, "\r\n");
			fprintf(stream, "PPPoE mode selected\r\n");
			params += strlen(params);
			return params;
		}
		if (atoi(params) == 0)
		{
			confv250.opt_2 &= ~OPT_PASSTHROUGH;
		}
		else if(atoi(params) == 1)
		{
			confv250.opt_2 |= OPT_PASSTHROUGH;
		}
		else if (atoi(params) == 2)
		{
			confv250.opt_2 &= ~OPT_PASSTHROUGH;
			DoDirectPassThruToTE();
			//DoDirectPassThru();
		}
		else
		{
			*rslt = ERROR;
		}
	}
	else if (*params == '?')
	{
		fprintf(stream, "\r\n");

		if (confv250.opt_2 &OPT_PASSTHROUGH)
		{
			fprintf(stream, PRFX "PAST: 1 \r\n");
		}
		else
		{
			fprintf(stream, PRFX "PAST: 0 \r\n");
		}
	}
	params += strlen(params);
	return params;
}

char* CDCS_V250Cmd_MASQ(FILE* stream, char* params, char* rslt)
{
	params++;
	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			fprintf(stream, "\r\n^MASQ: (0-1)\r\n");
			params += strlen(params);
			return params;
		}
		if ( atoi(params) == 1 )
		{
			setSingleVal("service.dns.masquerade",1);
		}
		else if (atoi(params) == 0)
		{
			setSingleVal("service.dns.masquerade",0);
		}
		else
		{
			*rslt = ERROR;
		}
	}
	else if (*params == '?')
	{
		fprintf(stream, "\r\n^MASQ: %s\r\n", getSingle("service.dns.masquerade"));
	}
	params += strlen(params);
	return params;
}


// at^MAPP command is used for incoming NAT re-direction whereby the values
// for incoming IP, imcoming port, destination IP and dest port are entered
// in the IP tables file in /etc/rc.d/rc.ipmasq
// This is in the format:
// at+lcmapp=<remote source IP>,<incoming port>,<destination IP>,<destination port>
// The remote IP being the IP address of where the connection was initiated,i.e the source of the data
// so if at+lcmapp = 192.168.1.35,80,66.102.7.99,80 then any packets from IP address 192.168.1.35
// will be forwarded to www.google.com (Ip address 66.102.7.99, port 80).
// To do this the following line needs to be put in IP tables:
// $IPTABLES -A PREROUTING -t nat -p tcp -s 192.168.1.35 --dport 80 -j DNAT
// --to-destination 66.102.7.99:80
// This should also do a reverse NAT rule for the other direction to so we don't need an
// extra line for the reverse mapping.
char* CDCS_V250Cmd_MAPP(FILE* stream, char* params, char* rslt)
{
	u_short Num;
	char n=0;
	// This points to the array of pointers to each sub-string returned
	// in the StrSplit() routine
	char** P;
	char* pos;
	TABLE_MAPPING* p;
	char count = 0;

	p = (TABLE_MAPPING*)malloc(sizeof(TABLE_MAPPING));
	if (p == NULL)
	{
		*rslt = ERROR;
		params += strlen(params);
		return params;
	}
	// increment the pointer and check that '=' is the first character
	// after the at+lcMAPP command....
	params++;
	if (*params == '=')
	{
		// Split up the string by commas
		params++;
		P = StrSplit(&Num, params, ',');
		if(P[0])
			n = atoi(P[0]) - 1;
		// We must have the right number of params
		if (Num == 1)
		{
			if( CDCS_DeleteMappingConfig(n) < 0 )
			{
				*rslt = ERROR;
			}
			free(P);
			free(p);
			params += strlen(params);
			return params;
		}
		else if (Num != 6)
		{
			free(P);
			*rslt = ERROR;
			free(p);
			// advance the pointer to the end of the parameter string...
			params += strlen(params);
			return params;
		}

		// copy these values to the mapping and save
		if( strcmp(P[1],"all")==0 || strcmp(P[1],"ALL")==0 )
		{
			strcpy(p->protocol, "all" );
		}
		else if( strcmp(P[1],"tcp")==0 || strcmp(P[1],"TCP")==0 )
		{
			strcpy(p->protocol, "tcp" );
		}
		else if( strcmp(P[1],"udp")==0 || strcmp(P[1],"UDP")==0 )
		{
			strcpy(p->protocol, "udp" );
		}
		else
		{
			free(P);
			free(p);
			*rslt = ERROR;
			params += strlen(params);
			return params;
		}
		strncpy(p->remoteSrcIPAddr, P[2], sizeof(p->remoteSrcIPAddr));
		p->remoteSrcIPAddr[sizeof(p->remoteSrcIPAddr)-1] = 0;
		p->localPort1 = atoi(P[3]);
		p->localPort2 = p->localPort1;
		if ((pos = strstr(P[3], ":")))
			p->localPort2 = atoi(pos + 1);
		if (p->localPort2 < p->localPort1)
			p->localPort2 = p->localPort1;
		strncpy(p->destIPAddr, P[4], sizeof(p->destIPAddr));
		p->destIPAddr[sizeof(p->destIPAddr)-1] = 0;
		p->destPort1 = atoi(P[5]);
		if (p->localPort2 != p->localPort1)
			p->destPort2 = p->destPort1 + (p->localPort2 - p->localPort1);
		else
			p->destPort2 = p->destPort1;
		// Save mapping, but don't save a deleted one!....
		if (!((p->localPort1 == 0) && (p->destPort1 == 0)))
		{
			if( CDCS_SaveMappingConfig(p, n)<0 )
			{
				free(P);
				free(p);
				*rslt = ERROR;
				params += strlen(params);
				return params;
			}
		}
		else
		{
			if (CDCS_DeleteMappingConfig(n)<0)
			{
				free(P);
				free(p);
				*rslt = ERROR;
				params += strlen(params);
				return params;
			}
		}
		params += strlen(params);
		free(P);
	}
	else if (*params == '?')
	{
		fprintf(stream, "\r\n");

		while (CDCS_LoadMappingConfig(p, count) != -1)
		{
			count++;
			if ((p->localPort1) == (p->localPort2))
			{
				fprintf(stream, PRFX "MAPP: %d,%s,%s,%d,%s,%d\n", count, p->protocol, p->remoteSrcIPAddr, p->localPort1, p->destIPAddr, p->destPort1);
			}
			else
			{
				fprintf(stream, PRFX "MAPP: %d,%s,%s,%d:%d,%s,%d:%d\n", count, p->protocol, p->remoteSrcIPAddr, p->localPort1, p->localPort2, p->destIPAddr, p->destPort1, p->destPort2);
			}
			fprintf(stream, "\r\n");
			fflush(stream);
		}
		params++;
	}
	else
	{
		*rslt = ERROR;
	}
	free(p);
	return params;
}


// ETHI - Set or print the local IP address
char* CDCS_V250Cmd_ETHI(FILE* stream, char* params, char* rslt)
{
	params++;
	if (*params == '=')
	{
		++params;
		if (*params == '?')
		{
			fprintf(comm_host, "\r\n^ETHI: Sets the ethernet interface IP address\r\n");
			params += strlen(params);
			return params;
		}
		rdb_set_single("link.profile.0.address", params);
	}
	else if (*params == '?')
	{
		fprintf(comm_host, "\r\n%s\r\n", getSingle("link.profile.0.address") );
	}
	params += strlen(params);
	return params;
}

// ETHN - Set or print the local TCP/IP netmask
char* CDCS_V250Cmd_ETHN(FILE* stream, char* params, char* rslt)
{
	params++;
	if (*params == '=')
	{
		++params;
		if (*params == '?')
		{
			fprintf(comm_host, "\r\n^ETHN: Sets the ethernet interface subnet mask\r\n");
			params += strlen(params);
			return params;
		}
		rdb_set_single("link.profile.0.netmask", params);
	}
	else if (*params == '?')
	{
		fprintf(comm_host, "\r\n%s\r\n", getSingle("link.profile.0.netmask") );
	}
	params += strlen(params);
	return params;
}

// ETHG - Set or print the TCP/IP gateway
char* CDCS_V250Cmd_ETHG(FILE* stream, char* params, char* rslt)
{
	return params;
}

// ETHR Print the route table
char* CDCS_V250Cmd_ETHR(FILE* stream, char* params, char* rslt)
{
	FILE* pFile;
	char buff[100];

	params++;
	if (*params == '?')
	{
		if ((pFile = popen("route", "r")) == 0)
		{
			syslog(LOG_INFO, "failed to open pipe");
		}
		fprintf(stream, "\r\n\r\n");
		while (fgets(buff, sizeof(buff), pFile) != NULL)
		{
			fprintf(stream, "%s \r", buff);
		}
		pclose(pFile);
	}
	else
	{
		*rslt = ERROR;
	}
	params += strlen(params);
	return params;
}

// NTPS Set SNTP server
char* CDCS_V250Cmd_NTPS(FILE* stream, char* params, char* rslt)
{
	return params;
}

// AT^NTPU Set GM47 clock from SNTP server
char* CDCS_V250Cmd_NTPU(FILE* stream, char* params, char* rslt)
{
	return params;
}

char* CDCS_V250Cmd_DHCP(FILE* stream, char* params, char* rslt)
{
	u_short Num;
	char** P;
	char range[64];
	char dns1[20];
	char dns2[20];

	params++;
	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			fprintf(comm_host, "\r\n");
			fprintf(comm_host, "^DHCP: Sets the DHCP configuration \r\n");
			params += strlen(params);
			return params;
		}
		if ( getSingleVal("service.pppoe.server.0.enable") )
		{
			*rslt = ERROR;
			fprintf(stream, "\r\n");
			fprintf(stream, "PPPoE mode selected\r\n");
			params += strlen(params);
			return params;
		}
		// Split up the string by commas
		P = StrSplit(&Num, params, ',');
		// if we want to disable DHCP......
		if (atoi(P[0]) == 0)
		{
			rdb_set_single("service.dhcp.enable", "0" );
			params += strlen(params);
			free(P);
			return params;
		}

		// We must have the right number of params
		if (Num != 5)
		{
			*rslt = ERROR;
			params += strlen(params);
			free(P);
			return params;
		}
		sprintf( range, "%s,%s", P[0], P[1]);
		rdb_set_single("service.dhcp.range.0", range);
		setSingleVal("service.dhcp.lease.0", atoi(P[2]));
		rdb_set_single("service.dhcp.dns1.0", P[3]);
		rdb_set_single("service.dhcp.dns2.0", P[4]);
		rdb_set_single("service.dhcp.enable", "1" );
		free(P);
	}
	else if (*params == '?')
	{
		if( getSingleVal("service.dhcp.enable") )
		{
			strncpy(range, getSingle("service.dhcp.range.0"), 63);
			strncpy(dns1, getSingle("service.dhcp.dns1.0"), 19);
			strncpy(dns2, getSingle("service.dhcp.dns2.0"), 19);
			fprintf(stream, "\r\n");
			fprintf(stream, PRFX "DHCP: %s,%u,%s,%s\r\n", range, getSingleVal("service.dhcp.lease.0"), dns1, dns2);
			fflush(stream);
		}
		else
		{
			fprintf(stream, "\r\n");
			fprintf(stream, PRFX "DHCP: 0\r\n");
			fflush(stream);
		}
	}
	params += strlen(params);
	return params;
}


char* CDCS_V250Cmd_PING(FILE* stream, char* params, char* rslt)
{
	FILE* ping_out;
	int in_char;
	char cmd_buffer[300];

	params++;
	if (*params == '=')
	{
		params++;
		sprintf(cmd_buffer, "%s %s 2>&1 ", ping_command, params);
		if ((ping_out = popen(cmd_buffer, "r")) == 0)
		{
			syslog(LOG_INFO, "ping fail");
			return params;
		}
		while ((in_char = fgetc(ping_out)) != EOF)
		{
			if (in_char == '\n')
			{
				fputc('\r', comm_host);
			}
			fputc(in_char, comm_host);
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
char* CDCS_V250Cmd_LTPH(FILE* stream, char* params, char* rslt)
{
	u_short Num;
	char add1[64], add2[64];
	// This points to the array of pointers to each sub-string returned
	// in the StrSplit() routine
	char** P;
	// increment the pointer and check that '=' is the first character
	// after the at^LTPH command....
	params++;
	if (*params == '=')
	{
		// Split up the string by commas
		params++;
		P = StrSplit(&Num, params, ',');
		// We must have the right number of params
		if (Num != 5)
		{
			free(P);
			*rslt = ERROR;
			// advance the pointer to the end of the parameter string...
			params += strlen(params);
			return params;
		}
		rdb_set_single("service.systemmonitor.destaddress", P[0]);
		rdb_set_single("service.systemmonitor.destaddress2", P[1]);
		setSingleVal("service.systemmonitor.periodicpingtimer", atoi(P[2]));
		setSingleVal("service.systemmonitor.pingacceleratedtimer", atoi(P[3]));
		setSingleVal("service.systemmonitor.failcount", atoi(P[4]));
		params += strlen(params);
		free(P);
	}
	else if (*params == '?')
	{
		strncpy( add1, getSingle("service.systemmonitor.destaddress"), 63);
		strncpy( add2, getSingle("service.systemmonitor.destaddress2"), 63);
		fprintf(stream, "\r\n");
		fprintf(stream, PRFX "LTPH: %s,%s,%d,%d,%d,%d\r\n", add1, add2,	\
		getSingleVal("service.systemmonitor.periodicpingtimer"), getSingleVal("service.systemmonitor.pingacceleratedtimer"), getSingleVal("service.systemmonitor.failcount"), -1);
		fflush(stream);
		params++;
	}
	else
	{
		*rslt = ERROR;
	}
	return params;
}

char* CDCS_V250Cmd_IFCG(FILE* stream, char* params, char* rslt)
{
	FILE* pFile;
	char buff[100];

	params++;
	if (*params == '?')
	{
		if ((pFile = popen("ifconfig", "r")) == 0)
		{
			syslog(LOG_INFO, "failed to open pipe");
		}

		fprintf(stream, "\r\n");
		fprintf(stream, "\r\n");

		while (fgets(buff, sizeof(buff), pFile) != NULL)
		{
			fprintf(stream, "%s \r", buff);
		}

		pclose(pFile);
	}
	else
	{
		*rslt = ERROR;
	}
	params += strlen(params);
	return params;
}

char* CDCS_V250Cmd_TEST(FILE* stream, char* params, char* rslt)
{
	u_char n = 255;

	params = CDCS_V250CharParam(stream, params, rslt, &n, PRFX "TEST: %d", PRFX "TEST: (%u-%u)", 0, 1);
//TODO: ??
	/*if (n == 1)
	{
		kmip_cns_ThreadPaused = 1;
		moduleOfflineSent = 1;
		kmip_cns_ThreadPaused = 0;
	}*/
	return params;
}

char* CDCS_V250Cmd_MACA(FILE* stream, char* params, char* rslt)
{
	u_short Num;
	int i;
	// This points to the array of pointers to each sub-string returned
	// in the StrSplit() routine
	char** P;
	char buf[128];

	params++;
	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			fprintf(comm_host, "\r\n");
			fprintf(comm_host, "MACA: Sets the MAC address of the router's ethernet interface \r\n");
			params += strlen(params);
			return params;
		}
		if (getSingleVal("service.pppoe.server.0.enable"))
		{
			fprintf(stream, "\r\n");
			fprintf(stream, "MACA: Cannot re-configure mac Addr since PPPoE is running \r\n");
			fprintf(stream, "\r\n");
			*rslt = ERROR;
			params += strlen(params);
			return params;
		}
		if (strlen(params) == 0) // If no MAC address specified use factory MAC
		{
			*rslt = ERROR;
			params += strlen(params);
			return params;
		}
		// Split up the string by :'s
		P = StrSplit(&Num, params, ':');
		// We must have the right number of params
		if (Num != 6)
		{
			free(P);
			*rslt = ERROR;
			params += strlen(params);
			return params;
		}
		else
		{
			if(strlen(P[5])>2 && *(P[5]+2)==' ')
				*(P[5]+2)=0;
			for(i=0; i<6; i++)
			{
				if( strlen(P[i])!=2 || !isHex( *(P[i]) ) || !isHex( *(P[i]+1) ) )
				{
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
			rdb_set_single("systeminfo.mac.eth0", buf);
			free(P);
		}
	}
	else if (*params == '?')
	{
		fprintf(stream, "\r\n%s \r\n", getSingle("systeminfo.mac.eth0"));
	}

	params += strlen(params);
	return params;
}


char* CDCS_V250Cmd_SHWT(FILE* stream, char* params, char* rslt)
{
	return params;
}

char* CDCS_V250Cmd_SHWM(FILE* stream, char* params, char* rslt)
{
	return params;
}

char* CDCS_V250Cmd_MIPP(FILE* stream, char* params, char* rslt)
{
	return params;
}

char* CDCS_V250Cmd_SSQRL(FILE* stream, char* params, char* rslt)
{
	return params;
}

char* CDCS_V250Cmd_PRFL(FILE* stream, char* params, char* rslt)
{
	u_short Num;
	char** PP;
	char n;
	WWANPARAMS* p;
	int ret = -1;
	int p6;

	p = (WWANPARAMS*)malloc(sizeof(WWANPARAMS));
	if (p == NULL)
	{
		*rslt = ERROR;
		params += strlen(params);
		return params;
	}
	params++;
	if (*params == '=')
	{
		// Split up the string by commas
		params++;
		PP = StrSplit(&Num, params, ',');
		if (PP == NULL)
		{
			*rslt = ERROR;
			params += strlen(params);
			return params;
		}
		// We must have the right number of params
		if ((Num != 10) )//&& (use_umts))
		{
			free(PP);
			free(p);
			*rslt = ERROR;
			params += strlen(params);
			return params;
		}

		// Profile to modify
		n = atoi(PP[0]);
		// Profile mod
		ret = CDCS_getWWANProfile(p, n);
		if (ret == -1)
		{
			free(PP);
			*rslt = ERROR;
			params += strlen(params);
			free(p);
			return params;
		}
		// if this profile is configurable...........
		if ( p->readOnly )
		{
			fprintf(stream, "\r\nThis profile is read-only, make sure you use a configurable profile\r\n");
			free(PP);
			*rslt = ERROR;
			params += strlen(params);
			free(p);
			return params;
		}
		strncpy(p->dial_number, PP[1], sizeof(p->dial_number));
		p->dial_number[sizeof(p->dial_number)-1] = 0;
		strcpy(p->user, PP[2]);
		strcpy(p->pass, PP[3]);
		p->padm = atoi(PP[4]); //defect #52
		if (atoi(PP[5]))
			p->pado |= PAD_AUTOANS;
		else
			p->pado &= ~PAD_AUTOANS;
		p6 = atoi(PP[6]);
		if (p6 &1)
			p->pado |= LENC_ADDRSER;
		else
			p->pado &= ~LENC_ADDRSER;
		if (p6 &2)
			p->pado |= LENC_DNP3;
		else
			p->pado &= ~LENC_DNP3;
		strcpy(p->rhost, PP[7]);
		p->padp = atoi(PP[8]);
		strcpy(p->apn_name, PP[9]);
		CDCS_SaveWWANConfig(p, n);  // Save profile
		if (currentProfileIx == n)
		{
			CDCS_getWWANProfile(currentProfile, currentProfileIx);
		}
		params += strlen(params);
		free(PP);
	}
	else if (*params == '?')
	{
		fprintf(stream, "\r\n");
		//profile mod
		for (n = 1;n <= 6;n++)
		{
			CDCS_getWWANProfile(p, n);
			p6 = 0;
			if (p->pado &LENC_ADDRSER)
				p6 |= 1;
			if (p->pado &LENC_DNP3)
				p6 |= 2;
			fprintf(stream, PRFX "PRFL: %d,%s,%s,%s,%d,%d,%d,%s,%d,%s\r\n", n, p->dial_number, p->user, p->pass, p->padm, (p->pado &PAD_AUTOANS) == PAD_AUTOANS, p6, p->rhost, p->padp, p->apn_name);
			fflush(stream);
		}
		params++;
	}
	else
		*rslt = ERROR;
	free(p);
	return params;
}

// This at command is used to set the password for a user who wants
// to send at commands to the modem. i.e for TELNET
char* CDCS_V250Cmd_RMAU(FILE* stream, char* params, char* rslt)
{
	return params;
}

// resets the 822
char* CDCS_V250Cmd_RESET(FILE* stream, char* params, char* rslt)
{
	system("rdb_set service.system.reset 1");
	return ++params;
}

// added by Yong for Defect#9 (AT^FACTORY)
char* CDCS_V250Cmd_FACTORY(FILE* stream, char* params, char* rslt)
{
	fprintf(stream, "\r\nFactory default configuration is applied. Rebooting...\r\n");
	fflush(stream);
	system("dbcfg_default -f");
	system("rdb_set service.system.reset 1");
//	system("/sbin/reboot");
	return ++params;
}

// This at command is used for configuring PPPoE setup
char* CDCS_V250Cmd_PPPoE(FILE* stream, char* params, char* rslt)
{
	//char pResp[PASSTHRUBUFFSIZE];
	params++;

	if (*params == '=')
	{
		params++;
		if (*params == '?')
		{
			fprintf(stream, "\r\n");
			fprintf(stream, "^PPPOE: (0-1)\r\n");
			params += strlen(params);
			return params;
		}
		if (isAnyProfileEnabled())
		{
			fprintf(stream, "\r\n");
			fprintf(stream, "a WWAN session is already active\r\n");

			*rslt = ERROR;

			params += strlen(params);
			return params;
		}
		// NOTE - there should be no existing WWAN session already up if pppoe is intended to run..
		if ((atoi(params) == 1) && !isAnyProfileEnabled() )
		{
			setSingleVal("service.pppoe.server.0.enable",1);
		}
		else if (atoi(params) == 0)
		{
			setSingleVal("service.pppoe.server.0.enable",0);
		}
		else
		{
			*rslt = ERROR;
		}
	}
	else if (*params == '?')
	{
		fprintf(stream, "\r\n");
		fprintf(stream, "^PPPOE: %s\r\n", getSingle("service.pppoe.server.0.enable"));
	}
	params += strlen(params);
	return params;
}

int writeAll(int fd, char* buf, unsigned len, volatile unsigned* signals)
{
	unsigned written = 0;
	int ret;
	int fds[1];
	unsigned long mask;
	while (written < len)
	{
		/* if (signals && ((*signals) & S_RTR) == 0) {
		waitReadWriteFds(NULL, &mask, 50, 0, NULL, 0, NULL);
		continue;
		} */
		if (written)
		{
			fds[0] = fd;
			if (waitReadWriteFds(NULL, &mask, 5, 0, NULL, 1, fds) <= 0)
			{
				continue;
			}
			/* if (signals && ((*signals) & S_RTR) == 0)
			continue; */
		}
		ret = write(fd, &buf[written], len - written);
		if (ret < 0)
		{
			if (errno == EAGAIN || errno == EINTR)
				continue;
			return -1;
		}
		if (!ret)
			return ret;
		written += len;
	}
	return (int)written;
}

int ReadHostWritePhat(void)
{
	char inbuf[PASSTHRUBUFFSIZE];
	struct timeval tv;
	fd_set readset;
	int phoneFd;
	int hostFd;
	int retvar = 0;
	int got;

	phoneFd = fileno(comm_phat);
	hostFd = fileno(comm_host);

	FD_ZERO(&readset);
	FD_SET(hostFd, &readset);
	tv.tv_sec = 0;
	tv.tv_usec = 5 * 1000;
	if (select(hostFd + 1, &readset, (fd_set*)0, (fd_set*)0, &tv) < 0)
		return -1;
	if ((got = read(hostFd, inbuf, PASSTHRUBUFFSIZE)) > 0)
	{
		writeAll(phoneFd, inbuf, got, NULL);
		syslog(LOG_DEBUG, "fd %d --> fd %d (ReadHostWritePhat)", hostFd, phoneFd);
		print_pkt(inbuf, got);
		retvar = 1;
	}
	return retvar;
}

void ReadHostWriteAT(void)
{
	while (confv250.opt_dtr == V250_DTR_LOPASSAT && DTR_ON)
	{
		if (pausePassAt(0))
			continue;
		if (ReadHostWritePhat() == -1)
			break;
	}
	return ;
}

int pacedRead(int fd, char* buff, unsigned len, unsigned to)
{
	int got;
	struct timeval tv;
	fd_set readset;
	int ret;

	got = 0;
	while (got < len)
	{
		if (to)
		{
			FD_ZERO(&readset);
			FD_SET(fd, &readset);
			tv.tv_sec = 0;
			tv.tv_usec = to * 1000;
			ret = select(fd + 1, &readset, (fd_set*)0, (fd_set*)0, &tv);

			//if (ret <= 0) syslog(LOG_INFO, "sel = %d %d", ret, to); //MJC timeout debug
			if (ret == 0)
				return got;
			if (ret < 0)
				return got;
		}
		ret = read(fd, buff + got, len - got);

		if (ret < 0)
		{
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN)
			{
				if (to)
					continue;
			}
			return got;
		}
		//syslog(LOG_INFO, "ret = %d len = %d", ret, len); //MJC timeout debug
		syslog(LOG_DEBUG, "<-- fd %d (pacedRead)", fd);
		print_pkt(buff + got, ret);
		got += ret;
		if (got >= len)
			return got;
		continue;
	}
	return got;
}

unsigned long msNow(void)
{
	struct timeval now;
	unsigned long n;
	gettimeofday(&now, NULL);
	n = now.tv_sec * 1000;
	n += now.tv_usec / 1000;
	return n;
}

int CDCS_V250DNPSerialIn(FILE* stream, char* buff, unsigned sz)
{
	unsigned long to;
	unsigned long mcast_mask;
	DNPHEADER* dnp = (DNPHEADER*)buff;
	unsigned long nInterPacketTime;
	int len;
	int x;
	struct sockaddr_in sad;

	// Start off by waiting for a start sequence (0x05 0x64)
	if ((pacedRead(fileno(stream), buff, 10, confv250.icto ? confv250.icto : 1) < 10) || buff[0] != 0x05 || buff[1] != 0x64)
		return 0;

	len = dnp->length - 5;
	if (len)
	{
		x = len / 16; // There is a CRC every 16 bytes
		if ((len % 16) != 0)
			x++;
		// And one on the last block too.
		len += (x * 2); // And each CRC is 2 bytes long

		if ((pacedRead(fileno(stream), buff + 10, len, confv250.icto ? len* 20 * confv250.icto : len* 20) < len))
			return 0;
	}
	// Calculate destination address
	//
	// How long ago did we last get a message on the GPRS interface?
	if (nDnpTimer == 0)
	{
		// If we've never sent a message then we use DNP - IP Address translation
		nInterPacketTime = 0xFFFF;
	}
	else
	{
		nInterPacketTime = msNow() - nDnpTimer;

		// convert this to seconds...
		nInterPacketTime = nInterPacketTime / 1000;
	}
	// if the timer has not expired then we can send straight back to the device that originally
	// sent the DNP message
	if ((confv250.appt1 == 0xFFFF) || (nInterPacketTime < confv250.appt1))
	{
		to = RemoteAddr.s_addr;
	}
	// otherwise if the timer has expired then we send to the destination given in the DNP message.
	// This timer functionality relates to another Master device taking over a previous Master
	// that is down due to failed comms (if this is a Slave of course).
	else if ((confv250.appt1 == 0) || (nInterPacketTime >= confv250.appt1))
	{
		// NOTE - inet_addr returns the address in network byte order, so convert it to host byte order
		// and do the same for mcast_mask so that if we are sending to a gateway address t hen
		// we are performing the logic correctly.
		to = inet_addr(currentProfile->rhost);

		// the mcast_mask is the scenario for a gateway whereby the shost could be set to
		// 192.168.96.8 and so DNP addresses 96.8 to 96.15 get sent to IP address 192.168.96.8
		mcast_mask = ntohl(inet_addr(confv250.shost));
		to = ntohl(to);
		to |= dnp->destlo;

		// if we are sending to a gateway address.....
		if ((mcast_mask != 0) && ((to &mcast_mask) == mcast_mask))
		{
			// chop off the lower 3 bits and obtain the gateway address to send to
			to &= 0xFFFFFFF8;
		}

		// now convert back to network byte order ready to send to the network......
		to = htonl(to);
	}
	bzero(&UDPRemoteAddr, sizeof(UDPRemoteAddr));
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = to;
	sad.sin_port = htons(currentProfile->padp);
	sendto(UDPPadSock, buff, len + 10, 0, (struct sockaddr*) &sad, sizeof(struct sockaddr_in));
	return len + 8;
}

char* CDCS_V250Cmd_APT1(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250ShortParam(stream, params, rslt, &confv250.appt1, "^APT1: %d", "^APT1: (%u-%u)", 0, 65535);
}

char* CDCS_V250Cmd_SHST(FILE* stream, char* params, char* rslt)
{
	return CDCS_V250StringParam(stream, params, rslt, confv250.shost, "^SHST: %s", "^SHST: (\"\")", sizeof(confv250.shost));
}

void CDCS_V250Engine(FILE* stream, int mode)
{
	int lcmd_len;
	char buff[MAX_AT_CMD];
	char* pCmd;
	char stat;
	int fPastCmd = 0;
	int found_caret_cmd = 0;

	stat = OK;
	if ((nModemState != COMMAND) && (nModemState != ON_LINE_COMMAND))
	{
		return ;
	}
	switch (confv250.opt_dtr)
	{
		case V250_DTR_LOPASSAT:
			if (DTR_ON)
				ReadHostWriteAT();
			return ;
	}
	if ((lcmd_len = CDCSGetATCommand(stream, buff, sizeof(buff))) == -1)
		return ;
	pCmd = buff;

	// print pkt for debug
	//if (buff[0] == 0xac && lcmd_len != 129) {
		syslog(LOG_DEBUG, "CDCSGetATCommand(), len = %d", lcmd_len);
		if (lcmd_len > 16)
			print_pkt(&buff[0], 16);
		else
			print_pkt(&buff[0], lcmd_len);
	//}
	//Ignore blank lines or print OK
	if (lcmd_len == 0)
	{
		if (confv250.opt_1 &OK_ON_CR)
			CDCS_V250Resp(stream, stat);
		return ;
	}

	if (confv250.opt_dtr == V250_DTR_LOPASSAT && DTR_ON)
	{
	    keep_alive_cnt = 0;
		pausePassAt(0);
		fwrite(buff, 1, lcmd_len, comm_phat);
		fputs("\r", comm_phat);
		syslog(LOG_DEBUG, "--> fd %d (CDCS_V250Engine)", fileno(comm_phat));
		print_pkt(buff, lcmd_len);
		fflush(comm_phat);
		return ;
	}
	// Ignore lines that don't start with AT
	if (strncasecmp(buff, "AT", 2) != 0)
		return ;
	pCmd += 2;

#define BYPASS_NOPHONE(x) \
	if(no_phone) { \
		stat=OK; \
		pCmd+=x; \
		continue; \
	}

	// Process all commands
	while (*pCmd)
	{
		if (stat == ERROR)
			break;
		// we apply special command set when we do not have a phone module
		// this is work-around for modem emulator but eventually we have to emulator all AT commands by talking to simple_at_manager or cnsmgr
		if(no_phone) {
			switch (*pCmd) {
			case 'd':
			case 'D':
				pCmd = CDCS_V250Cmd_D(stream, pCmd, &stat);
				continue;

			case 'i':
			case 'I':
				pCmd = CDCS_V250Cmd_I(stream, pCmd, &stat);
				continue;

			case 'h':
			case 'H':
				pCmd = CDCS_V250Cmd_H(stream, pCmd, &stat);
				continue;

			case '^':
				break;

			case '&':
				pCmd+=2;
				stat=OK;
				continue;

			default:
				// fixme: if we do not understand, we assume it is a single command
				stat=OK;
				pCmd++;
				continue;

			}
		} // Last command processed generated an error
		else {
			switch (*pCmd) {
			case 'a':
			case 'A':
				pCmd = CDCS_V250Cmd_A(stream, pCmd, &stat);
				continue;
			case 'd':
			case 'D':
				pCmd = CDCS_V250Cmd_D(stream, pCmd, &stat);
				continue;
			case 'e':
			case 'E':
				pCmd = CDCS_V250Cmd_E(stream, pCmd, &stat);
				continue;
			case 'h':
			case 'H':
				pCmd = CDCS_V250Cmd_H(stream, pCmd, &stat);
				continue;
			case 'i':
			case 'I':
				/* Cinterion module supports ATI1 or ATI2 */
				if (*(pCmd+1) == '1' || *(pCmd+1) == '2') {
					break;
				} else {
					pCmd = CDCS_V250Cmd_I(stream, pCmd, &stat);
					continue;
				}
			case 'o':
			case 'O':
				pCmd = CDCS_V250Cmd_O(stream, pCmd, &stat);
				continue;
			case 'q':
			case 'Q':
				pCmd = CDCS_V250Cmd_Q(stream, pCmd, &stat);
				continue;
			case 's':
			case 'S':
				pCmd = CDCS_V250Cmd_S(stream, pCmd, &stat);
				continue;
			case 'v':
			case 'V':
				pCmd = CDCS_V250Cmd_V(stream, pCmd, &stat);
				continue;
			case 'z':
			case 'Z':
				pCmd = CDCS_V250Cmd_Z(stream, pCmd, &stat);
				continue;
			case '&':
				switch (pCmd[1])
				{
					case 'd':
					case 'D':
						pCmd += 1;
						pCmd = CDCS_V250Cmd_AmpD(stream, pCmd, &stat);
						continue;
					case 's':
					case 'S':
						pCmd += 1;
						pCmd = CDCS_V250Cmd_AmpS(stream, pCmd, &stat);
						continue;
					case 'c':
					case 'C':
						pCmd += 1;
						pCmd = CDCS_V250Cmd_AmpC(stream, pCmd, &stat);
						continue;
					case 'n':
					case 'N':
						pCmd += 1;
						pCmd = CDCS_V250Cmd_AmpN(stream, pCmd, &stat);
						continue;
					case 'f':
					case 'F':
						pCmd += 2;
						CDCS_V250Cmd_AmpF(stream, pCmd, &stat);
						continue;
					case 'w':
					case 'W':
						pCmd += 2;
						CDCS_V250Cmd_AmpW(stream, pCmd, &stat);
						continue;
				}
				break;
			case '+':
				if (strncasecmp(pCmd, "+IPR", 4) == 0)
				{
					pCmd += 3;
					pCmd = CDCS_V250Cmd_IPR(stream, pCmd, &stat);
					continue;
				}
				else if (strncasecmp(pCmd, "+ICF", 4) == 0)
				{
					pCmd += 3;
					pCmd = CDCS_V250Cmd_ICF(stream, pCmd, &stat);
					continue;
				}
				else if (strncasecmp(pCmd, "+IFC", 4) == 0)
				{
					pCmd += 3;
					pCmd = CDCS_V250Cmd_IFC(stream, pCmd, &stat);
					continue;
				}
				else if (strncasecmp(pCmd, "+CMGS=", 6) == 0)
				{
					pCmd = CDCS_V250Cmd_CMGS(stream, pCmd, &stat);
					continue;
				}
#ifdef V_ME_LOCAL_CMD_y
				else if (strncasecmp(pCmd, "+CSQ", 4) == 0)
				{
					pCmd = CDCS_V250Cmd_CSQ(stream, pCmd, &stat);
					continue;
				}
#endif
				break;
			default:
				if (strncasecmp(pCmd, "!BAND", 5) == 0)
				{
					pCmd += 4;
					pCmd = CDCS_V250Cmd_BAND(stream, pCmd, &stat);
				}
				break;
			}
		}
		if (strncasecmp(pCmd, PRFX, PRFXL) == 0)
		{
			//syslog(LOG_DEBUG, "found caret command '%s'", pCmd);
			pCmd += PRFXL;
			found_caret_cmd = 1;
			if (strncasecmp(pCmd, "PRT", 3) == 0) {
				pCmd += 2;
				pCmd = CDCS_V250Cmd_PRT(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "CROK", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_CROK(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "NOLF", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_NOLF(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "ICTO", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_ICTO(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "IDCT", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_IDCT(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "SEST", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_SEST(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "IDNT", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_IDNT(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "NOER", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_NOER(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "COPT", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_COPT(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "GRST", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_GRST(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "GLUP", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_GLUP(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "TEST", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_TEST(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "PAST", 4) == 0) {
				fPastCmd = 1;
				pCmd += 3;
				pCmd = CDCS_V250Cmd_PAST(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "SHWT", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_SHWT(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "SHWM", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_SHWM(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "ETHI", 4) == 0)	{
				pCmd += 3;
				pCmd = CDCS_V250Cmd_ETHI(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "ETHN", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_ETHN(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "ETHR", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_ETHR(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "DHCP", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_DHCP(stream, pCmd, &stat);
			}
			/*if (strncasecmp(pCmd, "^NTPU", 5) == 0) {
			pCmd += 4;
			pCmd = CDCS_V250Cmd_NTPU(stream,pCmd,&stat);
			continue;
			}
			if (strncasecmp(pCmd, "^NTPS", 5) == 0) {
			pCmd += 4;
			pCmd = CDCS_V250Cmd_NTPS(stream,pCmd,&stat);
			continue;
			} */
			else if (strncasecmp(pCmd, "PING", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_PING(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "LTPH", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_LTPH(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "MAPP", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_MAPP(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "MASQ", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_MASQ(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "PRFL", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_PRFL(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "GASC", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_GASC(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "IFCG", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_IFCG(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "EMUL", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_EMUL(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "MACA", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_MACA(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "RMAU", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_RMAU(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "PPPoE", 5) == 0) {
				pCmd += 4;
				pCmd = CDCS_V250Cmd_PPPoE(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "FACTORY", 7) == 0) {
				// added by Yong for Defect#9 (AT^FACTORY)
				pCmd += 6;
				pCmd = CDCS_V250Cmd_FACTORY(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "RESET", 5) == 0) {
				pCmd += 4;
				pCmd = CDCS_V250Cmd_RESET(stream, pCmd, &stat);
			} else if (strncasecmp(pCmd, "TEST", 4) == 0) {
				pCmd += 3;
				pCmd = CDCS_V250Cmd_TEST(stream, pCmd, &stat);
			} else {
				/* in order to process Cinterion ^ commands */
				pCmd -= PRFXL;
				found_caret_cmd = 0;
			}
			if (found_caret_cmd)
				continue;
		}
		// Unknown command. Do we do passthrough ?
		if (confv250.opt_2 &OPT_PASSTHROUGH)
		{
			/*if (currentConnectType == V250_DIALPORT_CIRCUIT)
			{
				stat = ERROR;
				break;
			}*/
			ATPassthrough(stream, comm_phat, pCmd, AT_TIMEOUT, 0);
			stat = NORESP;
			break;
		}
		stat = ERROR;
		break; // Unknown command
	}

	if (stat != NORESP)
	{
		// if at^noer=1 is set then just return OK regardless of the fact it is an unknown command..
		if (confv250.opt_1 &OK_ON_UNKNOWN)
		{
			CDCS_V250Resp(stream, OK);
		}
		else
		{
			CDCS_V250Resp(stream, stat);
			// ...otherwise return error indicating an unknown command
		}
	}
}

void CDCS_PADAnswer(FILE* stream)
{
	unsigned long lt;
	unsigned long remad;
	int wait_cnt = 0;

	copyProfileToDialDetails(&currDialDetails, -1);
	currentConnectType = V250_DIALPORT_PROFILE;
	nModemState = ON_LINE;
	syslog(LOG_INFO, "CDCS_PADAnswer"); //MJC
	nDnpTimer = 0;
	if ((confv250.opt_1 &QUIET_ON) == 0)
	{
		if (confv250.emul == EMUL_SIMOCO)
		{
			remad = remoteAsNumber(currentProfile->rhost);
			lt = ntohl(RemoteAddr.s_addr) - remad;
			CDCS_V250UserResp_F(stream, "RING NPD %ld 001234", lt);
			CDCS_V250UserResp(stream, "CONNECT 1200");
		}
		else
			CDCS_V250ConnectMessage(stream);
	}
	/* wait until modem_emulator_loop is ready to process incoming packet
	 * data between serial port before sending data to serial port.
	 * Unless first few bytes may be lost. */
	while (!ready_to_rcv_data && (wait_cnt<20)) {
		CDCS_Sleep(100);
		wait_cnt++;
	}
	syslog(LOG_INFO, "waited for %ld ms before ready to receive packet data", (long int)(wait_cnt*100));
	ready_to_rcv_data = 0;
}

int CDCS_V250AddressedSerialOut(FILE* dev, const char* p, u_short len)
{
	char obuff[130];
	int i;
	u_short n;

	//321NutPrintBinary_P (dev,PSTR("\x7E\x99"),2);    // Header
	//321NutPrintBinary(dev,(char *)(&RemoteAddr),4);
	fwrite("\x7E\x99", 1, 2, dev);
	fwrite((char*)(&RemoteAddr), 1, 4, dev);
	n = 0;
	while (n < len)
	{
		// Send 128 byte chunks
		for (i = 0;i < 128;)
		{
			if ((*p) == 0x7D)
			{
				obuff[i++] = 0x7D;
				obuff[i] = 0x5d;
			}
			else if ((*p) == 0x7E)
			{
				obuff[i++] = 0x7D;
				obuff[i] = 0x5e;
			}
			else
			{
				obuff[i] = (*p);
			}
			p++;
			n++;
			i++;
			if (n == len)
				break;
		}
		fwrite(obuff, 1, i, dev);
		syslog(LOG_DEBUG, "--> fd %d (CDCS_V250AddressedSerialOut)", fileno(dev));
		print_pkt(obuff, i);
	}
	fputc(0x7E, dev);
	fflush(dev);
	return 0;
}

typedef struct
{
	int fd;
	char savebuff[MAX_BLOCK_SIZE+1];
	char ignoreStr[MAX_BLOCK_SIZE+1];
	int lenIgnore;
	int startSeen;
	int saved;
} WEEDER;


int weeder(WEEDER* weptr, char* buff, int maxlen, int icto)
{
	int got;
	int maxcmp;
	char* ptr;
	int sofar;
	int maxOut;

	while (1)
	{
		if (weptr->saved && weptr->startSeen != 0)
		{
			maxOut = weptr->saved;
			if (weptr->startSeen > 0)
			{
				if (maxOut > weptr->startSeen)
					maxOut = weptr->startSeen;
			}
			if (maxOut > maxlen)
				maxOut = maxlen;
			memcpy(buff, weptr->savebuff, maxOut);
			if (weptr->startSeen > 0)
				weptr->startSeen -= maxOut;
			if (weptr->saved > maxOut)
				memmove(weptr->savebuff, weptr->savebuff + maxOut, weptr->saved - maxOut);
			weptr->saved -= maxOut;
			return maxOut;
		}
		if (weptr->lenIgnore == 0) {
			return pacedRead(weptr->fd, buff, maxlen, icto);
		}
		got = pacedRead(weptr->fd, weptr->savebuff + weptr->saved, sizeof(weptr->savebuff) - weptr->saved, icto);
		if (got <= 0)
			return got;
		sofar = weptr->saved;
		weptr->saved += got;
		while (1)
		{
			if (weptr->startSeen >= 0)
			{
				ptr = weptr->savebuff + weptr->startSeen;
				maxcmp = weptr->saved - weptr->startSeen;
				if (maxcmp > weptr->lenIgnore)
					maxcmp = weptr->lenIgnore;
				if (!memcmp(ptr + sofar, weptr->ignoreStr + sofar, maxcmp - sofar))
				{
					if (maxcmp == weptr->lenIgnore)
					{
						weptr->saved -= weptr->lenIgnore;
						memmove(ptr, ptr + maxcmp, weptr->saved - (ptr - weptr->savebuff));
					}
					else
						break;
				}
				else
					ptr++;
			}
			else
			{
				ptr = weptr->savebuff;
			}
			ptr = memchr(ptr, weptr->ignoreStr[0], weptr->saved - (ptr - weptr->savebuff));
			sofar = 0;
			if (ptr)
			{
				weptr->startSeen = ptr - weptr->savebuff;
				continue;
			}
			weptr->startSeen = -1;
			break;
		}
	}
}

void init_weeder(WEEDER* weptr, int fd, char* ignoreStr)
{
	weptr->lenIgnore = 0;
	if (ignoreStr && * ignoreStr)
	{
		weptr->lenIgnore = unescape_to_buf(weptr->ignoreStr, ignoreStr, sizeof(weptr->ignoreStr) - 4);
		if (weptr->lenIgnore < 0)
		{
			weptr->lenIgnore = 0;
		}
	}
	weptr->startSeen = -1;
	weptr->fd = fd;
	weptr->saved = 0;
}

void check_weeder(WEEDER* weptr, char* ignoreStr)
{
	unsigned len;

	len = strlen(ignoreStr);
	if (len == weptr->lenIgnore && (!len || !strncmp(ignoreStr, weptr->ignoreStr, len)))
		return ;
	weptr->lenIgnore = 0;
	if (ignoreStr && * ignoreStr)
	{
		weptr->lenIgnore = unescape_to_buf(weptr->ignoreStr, ignoreStr, sizeof(weptr->ignoreStr) - 4);
		if (weptr->lenIgnore < 0)
		{
			weptr->lenIgnore = 0;
		}
	}
	weptr->startSeen = -1;
}

void checkModemQuietMode(int fForce)
{
	char buff[128];
	if (modemQuietMode != (confv250.opt_1 &QUIET_ON))
	{
		modemQuietMode = (confv250.opt_1 & QUIET_ON);
		if (modemQuietMode)
			sprintf(buff, "ATQ1");
		else
			sprintf(buff, "ATQ0");
		SetATStr(comm_phat, buff, fForce);
	}
}

void checkModemVerbose(int fForce)
{
	char buff[128];

	if (modemVerbose != (confv250.opt_1 &VERBOSE_RSLT))
	{
		modemVerbose = (confv250.opt_1 & VERBOSE_RSLT);
		if (modemVerbose)
			sprintf(buff, "ATV1");
		else
			sprintf(buff, "ATV0");
		SetATStr(comm_phat, buff, fForce);
	}
}

void checkModemAutoAnswer(int fForce)
{
	char buff[128];
	if (confv250.dialPort == V250_DIALPORT_CIRCUIT || confv250.dialPort == V250_DIALPORT_DIALSTR) {
		if (modemAutoAnswer != confv250.modemAutoAnswer) {
			modemAutoAnswer = confv250.modemAutoAnswer;
			sprintf(buff, "ATS0=%d", modemAutoAnswer);
			SetATStr(comm_phat, buff, fForce);
		}
	}
}

void print_pkt(char* pbuf, int len)
{
	unsigned char* buf = (unsigned char *)pbuf;
	unsigned char buf2[256] = {0x0,};
	int i, j = len/16, k = len % 16;
	unsigned char buf3[16] = {0x0,};
	for (i = 0; i < j; i++)	{
		(void) memset(buf3,0x00, 16);
		(void) memcpy(buf3, &buf[i*16], 16);
		syslog(LOG_DEBUG, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ; %s",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15], buf3);
	}
	j = i;
	for (i = 0; i < k; i++)
		sprintf((char *)buf2, "%s%02x ", buf2, buf[j*16+i]);
	(void) memset(buf3,0x00, 16);
	(void) memcpy(buf3, &buf[j*16], k);
	syslog(LOG_DEBUG, "%s; %s", buf2, buf3);
}

void ModemEmulatorLoop(FILE* stream)
{
	int got;
	int i;
	int fd;
	int icto;
	int lms = nModemState;
	char buff[MAX_BLOCK_SIZE+1];
	char* pMsg;
	u_short l;
	WEEDER weed;
	struct dialDetails dd;
	time_t current_time;
	time_t start_time;

	fd = fileno(stream);
	escape_cnt = 0;
	scheduleHangup = 0;
	init_weeder(&weed, fd, confv250.ignoreStr);
	//syslog(LOG_DEBUG, "nModemState = %d", nModemState); //MJC
	while (1)
	{
#ifdef RESTART_PROCESS_VIA_SIGHUP
		// Dut to structural fault, below routine is placed in the sighup_handler function.
		//if (received_sighup)
		//	sighup_restart();
#endif

		if (lms != nModemState)
		{
			lms = nModemState;
			syslog(LOG_INFO, "nModemState is set to %d", nModemState); //MJC
		}
		// scheduleHangup is raised if phone DCD drops in a call
		// Since we're not in a call ignore it
		if (scheduleHangup)
			scheduleHangup = 0;
		if (loopCheckV250)
		{
			check_weeder(&weed, confv250.ignoreStr);
			loopCheckV250 = 0;
		}
		checkModemAutoAnswer(0);
		checkModemVerbose(0);
		checkModemQuietMode(0);
		CDCS_V250Engine(stream, LOCAL);
		if (nModemState == DTR_DIAL)
		{
			if (decodeDialString(NULL, &dd) < 0)
				continue;
			switch (dd.portToUse)
			{
				case V250_DIALPORT_DIALSTR:
					continue;
				case V250_DIALPORT_PROFILE:
					CDCS_V250PADConnect(stream, &dd);
					break;
				case V250_DIALPORT_CIRCUIT:
					CDCS_V250CircuitConnect(stream, dd.rhost);
					break;
				case V250_DIALPORT_PACKET:
					CDCS_V250PacketConnect(stream, dd.rhost);
					break;
			}
		}

		// V.250 Says if we receive ANY characters from the host after the host issues an ATD
		// And before a connection is established we should abort the connection
		// Therefore if we are in CSD_DIAL we should listen for data from the host and send it to the phone
		// Also we should abort our call attempt
		// Be careful though as a valid command could end with <CR> or <CR><LF> and we don't want that to cause an abort
		//
		if (currentConnectType == V250_DIALPORT_CIRCUIT)
		{
			start_time = getUptime();

			while (nModemState != ON_LINE)
			{
				if (ReadHostWritePhat() > 0)
				{
					CDCS_V250Hangup(stream);
					break;
				}
				CDCS_Sleep(500);
				current_time = getUptime();
				if (abs((int)difftime(start_time, current_time)) > confv250.s7_timer)
				{
					syslog(LOG_INFO, "CSD call time out");
					if (!(chan_signals[IND_AT_PORT] & S_DV))
					{
						CDCS_Sleep(8000); // fix when ATS7=0, the moden will reponce NO CARRIER then CONNECT 29/01/2008
					}
					CDCS_V250Hangup(stream);
					continue;
				}
			}
		}
		while ((nModemState == ON_LINE) || (nModemState == CSD_DIAL))
		{
			if (lms != nModemState)
			{
				lms = nModemState;
				syslog(LOG_INFO, "nModemState = %d", nModemState);
				ready_to_rcv_data = 1;
			}
			// If using addressed serial protocol
			if (currentProfile->pado &LENC_ADDRSER) {
				;
			}
			else if (currentProfile->pado &LENC_DNP3) {
				CDCS_V250DNPSerialIn(stream, buff, MAX_BLOCK_SIZE);
			}
			else
			{
				l = 0;
				pMsg = buff;
				if (loopCheckV250)
				{
					check_weeder(&weed, confv250.ignoreStr);
					loopCheckV250 = 0;
				}

				// Get data, only apply intercharacter timeout in PAD mode
				icto = 1;
				if ((confv250.icto) && (currentConnectType == V250_DIALPORT_PROFILE))
					icto = confv250.icto;
				got = weeder(&weed, pMsg, sizeof(buff), icto);

				if (got > 0)
				{
					idle = 0;
					if (got <= 3)
					{
						for (i = 0;i < got;i++)
						{
							if (pMsg[i] == '+')
							{
								escape_cnt++;
							}
							else
							{
								escape_cnt = 0;
								break;
							}
						}
					}
					// Send data
					switch (currentConnectType)
					{
						case V250_DIALPORT_PROFILE:
							if ((currentProfile->padm == PAD_TCP) && (TCPPadSock != -1))
								send(TCPPadSock, buff, got + l, 0);
							else if (currentProfile->padm == PAD_UDP)
							{
								UDPRemoteAddr.sin_family = AF_INET;
								UDPRemoteAddr.sin_addr.s_addr = RemoteAddr.s_addr;
								if(UDPSourceAddr.sin_port==0) {
									UDPRemoteAddr.sin_port = htons(currentProfile->padp);
								}
								else {
									UDPRemoteAddr.sin_port = UDPSourceAddr.sin_port;
								}
								//syslog(LOG_ERR, "The Remoter port number is %d\n",ntohs(UDPRemoteAddr.sin_port));
								sendto(UDPPadSock, buff, got + l, 0, (struct sockaddr*) &UDPRemoteAddr, sizeof(struct sockaddr_in));
							}
							break;
						case V250_DIALPORT_DIALSTR:
						case V250_DIALPORT_CIRCUIT:
						case V250_DIALPORT_PACKET:
							if (comm_phat) {
								writeAll(fileno(comm_phat), buff, got + l, &chan_signals[IND_AT_PORT]);
								syslog(LOG_DEBUG, "--> fd %d (ModemEmulatorLoop)", fileno(comm_phat));
								print_pkt(buff, got + l);
							}
							break;
					}
				}
				if (escape_cnt == 3)
				{
					syslog(LOG_INFO,"Got 3 escape chars +++");
					// user has entered escape sequence
					CDCS_V250Resp(stream, OK);
					nModemState = ON_LINE_COMMAND;
					lms=ON_LINE_COMMAND;
					while (nModemState == ON_LINE_COMMAND)
					{
						CDCS_V250Engine(stream, LOCAL);
					}
					syslog(LOG_DEBUG,"exit from CDCS_V250Engine()");
					escape_cnt = 0;
				}
			}

			// scheduleHangup is raised if phone DCD drops in a call
			if (scheduleHangup)
			{
				syslog(LOG_INFO, " scheduleHangup processed"); //MJC
				scheduleHangup = 0;
				CDCS_V250Hangup(stream);
			}
		}
		/* while online */
	}
}

void CDCS_V250Initialise(FILE* stream)
{
	int fd = 0;
	struct termios tty_struct;

	confv250.opt_1 = getSingleVal("confv250.Option1");//&ECHO_ON;//&SUPRESS_LF
	confv250.opt_2 = getSingleVal("confv250.Option2");
	confv250.emul = getSingleVal("confv250.emul");
	confv250.opt_gasc = isAnyProfileEnabled();
	confv250.opt_dtr = getSingleVal("confv250.opt_dtr");
	confv250.opt_dsr = getSingleVal("confv250.opt_dsr");
	confv250.opt_rts = getSingleVal("confv250.opt_rts");
	confv250.opt_rts = getSingleVal("confv250.opt_cts");
	confv250.opt_dcd = getSingleVal("confv250.opt_dcd");
	confv250.opt_ri = getSingleVal("confv250.opt_ri");
	confv250.opt_fc = getSingleVal("confv250.opt_fc");
	confv250.icto = getSingleVal("confv250.Inter_Character_Timeout");
	confv250.idct = getSingleVal("confv250.Idle_Disconnect_Timeout");
	confv250.sest = getSingleVal("confv250.Maximum_Session_Timeout");
	confv250.ipr = getSingleVal("confv250.Baud_rate");
	confv250.format = getSingleVal("confv250.Format");
	confv250.parity = getSingleVal("confv250.Parity");
	confv250.dialPort = getSingleVal("confv250.dialPort");
	strncpy( confv250.autoDialNumber, getSingle("confv250.autoDialNumber"), sizeof(confv250.autoDialNumber));
	confv250.modemAutoAnswer = getSingleVal("confv250.modemAutoAnswer");
	confv250.appt1 = getSingleVal("confv250.Appt1");
	confv250.s7_timer = getSingleVal("confv250.ATS7var");
	strncpy( confv250.ident, getSingle("confv250.Ident"), sizeof(confv250.ident));
	strncpy( confv250.ignoreStr, getSingle("confv250.ignoreStr"), sizeof(confv250.ignoreStr));
	strncpy( confv250.shost, getSingle("confv250.sHost"), sizeof(confv250.shost));
#ifdef V_KEEP_SINGLE_TCP_CONN_y
	confv250.tcp_timeout= getSingleVal("confv250.tcp_timeout");
#endif

	currentConnectType = V250_DIALPORT_DIALSTR;
	nModemState = COMMAND;
	if (confv250.opt_fc != 0x22)
		confv250.opt_fc = 0;
	if (stream)
	{
		fd = fileno(stream);
		// Setup flow control, High nibble = flow_in Low Nibble = flow_out
		if (((confv250.opt_fc &0x0F) == 0x02) || ((confv250.opt_fc &0xF0) == 0x20))
		{
			tcgetattr(fd, &tty_struct);
			tty_struct.c_cflag |= CRTSCTS;
			tcsetattr(fd, TCSADRAIN, &tty_struct);
		}
	}
}

speed_t get_serial_port_baud(FILE* port)
{
	int fd;
	struct termios tty_struct;

	fd = fileno(port);
	tcgetattr(fd, &tty_struct);
	return cfgetospeed(&tty_struct);
}

#if defined(V_SERIAL_HAS_FC_y)
void host_dcd_on()
{
	//syslog(LOG_DEBUG,("%s", __func__);
#ifdef CONTROL_DIRECT_GPIO_PIN
	gpio_gpio(DCE_PORT_DCD_OUT);
	gpio_set_output(DCE_PORT_DCD_OUT, 0);
#else
	/* Falcon & Eagle B/D : OUT1 = DCD, refer to kernel/arch/arm/boot/dts/imx28_falcon.dts */
	if (TogglePhysSerialStatOn(IND_COUNT_PORT, TIOCM_OUT1) < 0) {
		syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
	}
#endif
}

void host_dcd_off()
{
	//syslog(LOG_DEBUG,"%s", __func__);
#ifdef CONTROL_DIRECT_GPIO_PIN
	gpio_gpio(DCE_PORT_DCD_OUT);
	gpio_set_output(DCE_PORT_DCD_OUT, 1);
#else
	/* Falcon & Eagle B/D : OUT1 = DCD, refer to kernel/arch/arm/boot/dts/imx28_falcon.dts */
	if (TogglePhysSerialStatOff(IND_COUNT_PORT, TIOCM_OUT1) < 0) {
		syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
	}
#endif
}

void host_ri_on()
{
	//syslog(LOG_DEBUG,"%s", __func__);
#ifdef CONTROL_DIRECT_GPIO_PIN
	gpio_gpio(DCE_PORT_RI_OUT);
	gpio_set_output(DCE_PORT_RI_OUT, 0);
#else
	/* Falcon & Eagle B/D : OUT2 = RI, refer to kernel/arch/arm/boot/dts/imx28_falcon.dts */
	if (TogglePhysSerialStatOn(IND_COUNT_PORT, TIOCM_OUT2) < 0) {
		syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
	}
#endif
}

void host_ri_off()
{
	//syslog(LOG_DEBUG,"%s", __func__);
#ifdef CONTROL_DIRECT_GPIO_PIN
	gpio_gpio(DCE_PORT_RI_OUT);
	gpio_set_output(DCE_PORT_RI_OUT, 1);
#else
	/* Falcon & Eagle B/D : OUT2 = RI, refer to kernel/arch/arm/boot/dts/imx28_falcon.dts */
	if (TogglePhysSerialStatOff(IND_COUNT_PORT, TIOCM_OUT2) < 0) {
		syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
	}
#endif
}

void host_dsr_on()
{
	//syslog(LOG_DEBUG,("%s", __func__);
#ifdef CONTROL_DIRECT_GPIO_PIN
	gpio_gpio(DCE_PORT_DSR_OUT);
	gpio_set_output(DCE_PORT_DSR_OUT, 0);
#else
	/* Falcon & Eagle B/D : DTR(in DTE side) refers to DSR (in DCE side),
	 * refer to kernel/arch/arm/boot/dts/imx28_falcon.dts and schematic */
	if (TogglePhysSerialStatOn(IND_COUNT_PORT, TIOCM_DTR) < 0) {
		syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
	}
#endif
}

void host_dsr_off()
{
	//syslog(LOG_DEBUG,("%s", __func__);
#ifdef CONTROL_DIRECT_GPIO_PIN
	gpio_gpio(DCE_PORT_DSR_OUT);
	gpio_set_output(DCE_PORT_DSR_OUT, 1);
#else
	/* Falcon & Eagle B/D : DTR(in DTE side) refers to DSR (in DCE side),
	 * refer to kernel/arch/arm/boot/dts/imx28_falcon.dts and schematic */
	if (TogglePhysSerialStatOff(IND_COUNT_PORT, TIOCM_DTR) < 0) {
		syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
	}
#endif
}
#else
void host_dcd_on() {}
void host_dcd_off() {}
void host_ri_on() {}
void host_ri_off() {}
void host_dsr_on() {}
void host_dsr_off() {}
#endif


// Toggles the External Serial port's DSR line accordingly
void handleDSRLine(void)
{
char *pos;
	//syslog(LOG_DEBUG,"opt_dsr = %d", confv250.opt_dsr);
	switch (confv250.opt_dsr)
	{
		case V250_DSR_ALWAYS:
			host_dsr_on();
			break;

		case V250_DSR_REGISTERED:
			pos=getSingle("wwan.0.system_network_status.service_type");
			if ( *pos == 0 || strstr( pos, "Invalid" )!=NULL )//NO_SERVICE
				host_dsr_off();
			else
				host_dsr_on();
			break;
		case V250_DSR_PPP:
			if (WWANOpened())
				host_dsr_on();
			else
				host_dsr_off();
			break;
		case V250_DSR_NEVER:
			host_dsr_off();
			break;
	}
}

// Toggles the External Serial port's DCD line accordingly
void handleDCDLine(void)
{
	//syslog(LOG_DEBUG,"opt_dcd = %d", confv250.opt_dcd);
	switch (confv250.opt_dcd)
	{
		case V250_DCD_ALWAYS:
			/* 0 */
			host_dcd_on();
			break;
		case V250_DCD_CONNECT:
			/* 1 */
			if (nModemState != ON_LINE_COMMAND && nModemState != ON_LINE)
			{
				host_dcd_off();
				break;
			}
			switch (whichPortToUse(V250_DIALPORT_DIALSTR))
			{
				case V250_DIALPORT_PROFILE:
					host_dcd_on();
					break;
				case V250_DIALPORT_CIRCUIT:
				case V250_DIALPORT_PACKET:
					// since chan_signals are broken (S_DV bit can never be set
					// at least make the CD line activate when modem state is on line
					// if (chan_signals[IND_AT_PORT] &S_DV)
					if (nModemState == ON_LINE)
						host_dcd_on();
					else
						host_dcd_off();
					break;
			}
			break;
		case V250_DCD_NEVER:
			/* 2 */
			host_dcd_off();
			break;
		case V250_DCD_PPP:
			/* 3 */
			if (WWANOpened())
				host_dcd_on();
			else
				host_dcd_off();
			break;
	}
}

static int checkTimeExpired(struct timeval* future)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	if (now.tv_sec < future->tv_sec)
		return 0;
	if (now.tv_sec > future->tv_sec)
		return 1;
	/* seconds equal */
	if (now.tv_usec < future->tv_usec)
		return 0;
	return 1;
}

int setFutureTime(int timeout, struct timeval* future)
{
	gettimeofday(future, NULL);
	if (!timeout)
		return 0;
	future->tv_usec += 1000 * timeout;
	while (future->tv_usec > 1000000)
	{
		future->tv_usec = future->tv_usec - 1000000;
		future->tv_sec++;
	}
	return 1;
}

void handleRILine(void)
{
	static struct timeval future;
	static int is_on = 0;

	//syslog(LOG_DEBUG,"opt_ri = %d", confv250.opt_ri);
	switch (confv250.opt_ri)
	{
		case V250_RI_ALWAYS:
			host_ri_on();
			is_on = 1;
			setFutureTime(250, &future);
			break;
		case V250_RI_RING:
			switch (whichPortToUse(V250_DIALPORT_CIRCUIT))
			{
				case V250_DIALPORT_PROFILE:
					host_ri_off();
					is_on = 0;
					break;
				case V250_DIALPORT_CIRCUIT:
				case V250_DIALPORT_PACKET:
					if (chan_signals[IND_AT_PORT] &S_IC)
					{
						host_ri_on();
						setFutureTime(250, &future);
						is_on = 1;
					}
					else
					{
						if (is_on && checkTimeExpired(&future))
						{
							host_ri_off();
							is_on = 0;
						}
					}
					break;
			}
			break;
		case V250_RI_NEVER:
			host_ri_off();
			is_on = 0;
			break;
	}
}

int canPassThroughAT(void)
{
	int fd;
	fd = fileno(comm_phat);

	if (pastThreadPaused)
		return -1;
	if (comm_phat == NULL)
		return -1;

	if ( currentConnectType == V250_DIALPORT_DIALSTR || currentConnectType == V250_DIALPORT_CIRCUIT )
		return fd;

	// If we're ON_LINE ignore anything received on the primary AT port
	if (nModemState == ON_LINE)
		return -1;

	// if we are in command mode OR online in circuit switched mode......
	if (confv250.opt_2 &OPT_PASSTHROUGH)
		return fd;

	if ((currentProfile->padm == PAD_DISABLED) && ((nModemState == ON_LINE) || (nModemState == CSD_DIAL)))
		return fd;

	return -1;
}

#if defined(V_SERIAL_HAS_FC_y)
void checkSetCTS(int on)
{
	static int is_on = -1;
	if (is_on == -1 || (on && is_on == 0) || (on == 0 && is_on))
	{
		is_on = on;
		syslog(LOG_INFO, "Set CTS %s", on ? "On" : "Off");
#ifdef CONTROL_DIRECT_GPIO_PIN
		gpio_gpio(DCE_PORT_CTS_OUT);
		gpio_set_output(DCE_PORT_CTS_OUT, on ? 0:1);
#else
		/* Falcon & Eagle B/D : RTS(in DTE side) refers to CTS (in DCE side),
		 * refer to kernel/arch/arm/boot/dts/imx28_falcon.dts and schematic */
		if (on && TogglePhysSerialStatOn(IND_COUNT_PORT, TIOCM_RTS) < 0) {
				syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
		} else if (!on && TogglePhysSerialStatOff(IND_COUNT_PORT, TIOCM_RTS) < 0) {
				syslog(LOG_INFO,"%s: failed to change serial port status", __func__);
		}
#endif
	}
}
#endif

void handleRTRSignal(void)
{
	unsigned signals;

	if ((confv250.opt_fc &0x22) == 0)
		return ;
	switch (whichPortToUse(V250_DIALPORT_CIRCUIT))
	{
		case V250_DIALPORT_PROFILE:
			break;
		case V250_DIALPORT_PACKET:
		case V250_DIALPORT_CIRCUIT:
			signals = chan_signals[IND_AT_PORT];
#if defined(V_SERIAL_HAS_FC_y)
			// MM 27/08/2014
			// This is wrong. We should never do this by hand - let the driver
			// do hw flow control. This also has the negative effect of
			// disabling hw flow control as soon as ckeckSetCTS is called
			// See code in the UART driver /kernel/drivers/tty/serial/mxs-auart.c
			// function mxs_auart_set_mctrl
			// In fact the whole handleRTRSignal function should go.
			//if (((signals &S_RTR) == 0) && (nModemState == ON_LINE))
			//{
			//	checkSetCTS(1);
			//}
			//else
			//	checkSetCTS(0);
#endif
			break;
	}
}

int readAndBuffer(FILE* instream, int weed, char* buf, unsigned int len)
{
	int got;

	if (!instream)
		return 0;
	if (len <= 1)
	{
		errno = EINVAL;
		return -1;
	}
	if ((got = read(fileno(instream), buf, len - 1)) > 0)
	{
		if (weed && got)
		{
			buf[got] = '\0';
		}
	}
	return got;
}
/*
static char* modemStateDesc[] =
{
	"Command", "Disc Timeout", "DTR Dial", "On Line", "On Line Cmd", "CSD Dial", "Telnet", "Local", "Disable V250",
};

void displayModemState(void)
{
	char* msta;
	msta = "UNK";
	if (nModemState >= 0 && nModemState < sizeof(modemStateDesc) / sizeof(modemStateDesc[0]))
		msta = modemStateDesc[nModemState];
	strncpy(msgbuf.modemStateString, msta, sizeof(msgbuf.modemStateString) - 1);
	msgbuf.modemStateString[sizeof(msgbuf.modemStateString)-1] = '\0';

	// syslog(LOG_INFO, "nModemStat %d %s", nModemState, msta);
}*/

int ignoreIfQuiet(const char* pRecv, int cbRecv)
{
	int cbNewRecv = cbRecv;

	if (confv250.opt_1 & QUIET_ON)
	{
		int iTbl = 0;
		char* pMsg;

		while ((pMsg = (char *)IgnoreMsgTable[iTbl++]) != 0)
		{
			if (strstr(pRecv, pMsg)) {
				syslog(LOG_DEBUG,"ignoreIfQuiet(): '%s', '%s'", pRecv, pMsg);
				cbNewRecv = 0;
			}
		}
	}
	return cbNewRecv;
}

void* ME_Rx(void* arg)
{
	// rxBuff Needs to be big enough to receive large UDP datagrams
	// Protocols such as ZModem which use large packet sizes are likely to produce large datagrams
	// In theory UDP supports up to 65535 byte datagrams.
	// In our case we expect that these UDP datagrams are going to arrive over the WAN interface

	char rxBuff[2048];
	u_long addr = 0;
	u_long rhost_addr;
	u_short currentPADPort = 0;
	struct hostent* he;
	struct timeval tv;
	struct timeval listentv;
	struct timeval udptv;
	struct sockaddr_in TCPClientAddr;
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	socklen_t tcpClientLen = sizeof(TCPClientAddr);
	FILE* stream;
	fd_set readset;
	fd_set udpset;
	fd_set listenset;
	int got;
	int ret;
	int selectret;
	int newFd = -1;
	int TCPListenSock = 0;
	char newSockCreated = 0;
	in_addr_t current_s_addr;
	addr_size = sizeof(UDPSourceAddr);
	stream = (FILE*)arg;
	int one=1;
#ifdef V_KEEP_SINGLE_TCP_CONN_y
 	int timeout_cnt=0;
#endif

	while (1)
	{
		// Accept incoming TCP connections.
		// NOTE - listening on port 0 is invalid, so don't try to accept on it
#ifdef V_KEEP_SINGLE_TCP_CONN_y
		if ((currentProfile->padm == PAD_TCP) && (currentProfile->pado &PAD_AUTOANS) && (currentProfile->padp != 0))
#else
		if ((currentProfile->padm == PAD_TCP) && (nModemState == COMMAND) && (currentProfile->pado &PAD_AUTOANS) && (currentProfile->padp != 0))
#endif
		{
			//syslog(LOG_ERR, "ME_Rx1:%u - %u - %u - %u - %u",currentProfile->padm, nModemState, currentProfile->pado, currentProfile->padp, TCPListenSock);
			if (TCPListenSock == 0)
			{
				if ((TCPListenSock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				{
					syslog(LOG_INFO, "could not create socket");
					CDCS_Sleep(1000);
					continue;
				}
				newSockCreated = 1;
			}
			if (newSockCreated)
			{
				/* After program termination the socket is still busy for a while.
				This allows an immediate restart of the daemon after termination. */
				if (setsockopt(TCPListenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&one,sizeof(one))<0)
				{
					syslog(LOG_INFO, "Can't set SO_REUSEADDR on socket");
					continue;
				}
				// Bind our local address so that the client can send to us
				set_tcp_options( TCPListenSock );
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family = AF_INET;
				serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
				serverAddr.sin_port = htons(currentProfile->padp);

				if (bind(TCPListenSock, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0)
				{
					CDCS_Sleep(1000);
					continue;
				}
				// NOTE - the address that has connected (the client) is filled in in the accept call
				ret = listen(TCPListenSock, 5);
				if (ret == -1)
				{
					//syslog(LOG_INFO, "TCPListenSock ret == -1, continue");
					CDCS_Sleep(1000);
					continue;
				}
				newSockCreated = 0;
			}

			// if the port has been changed using at+lcprfl then we need to re-bind the port, but in order to do
			// this we have to re-create the socket since we can only bind and listen once.
			if (currentProfile->padp != currentPADPort)
			{
				if(currentPADPort==0) { //check for startup
					currentPADPort = currentProfile->padp;
					continue;
				}
				close(TCPListenSock);
				TCPListenSock = 0;
				currentPADPort = currentProfile->padp;
				CDCS_Sleep(1000);
				continue;
			}
			FD_ZERO(&listenset);
			FD_SET(TCPListenSock, &listenset);
			listentv.tv_sec = 0;
			listentv.tv_usec = 1000000;
			selectret = select(TCPListenSock + 1, &listenset, 0, 0, &listentv);
			if (selectret == 0)
			{
#ifdef V_KEEP_SINGLE_TCP_CONN_y
				if (TCPPadSock == -1) {
					//syslog(LOG_INFO, "TCPListenSock selectret == 0, continue");
					CDCS_Sleep(1000);
					continue;
				} else {
					goto data_receive;
				}
#else
				CDCS_Sleep(1000);
				continue;
#endif
			}
			if (selectret != -1)
			{
				// NOTE that accept returns a new file descriptor each time. This is because being in
				// server mode we can have many clients in theory connected to us.
				// Although in our case we limit it to one.
				// NOTE - at the moment this call blocks until a client wants to connect...

				newFd = accept(TCPListenSock, (struct sockaddr*) & TCPClientAddr, &tcpClientLen);
				if (newFd == -1) {
					syslog(LOG_INFO, "sock accept returns -1");
					continue;
				}
#ifdef V_KEEP_SINGLE_TCP_CONN_y
				if (TCPPadSock != -1) {
					timeout_cnt = 0;
					//#define SWITCH_TO_NEW_CONN
					#define REJECT_NEW_CONN
					#ifdef SWITCH_TO_NEW_CONN
					syslog(LOG_INFO, "switch to new connection %d", newFd);
					close(TCPPadSock);
					ready_to_rcv_data = 1;
					#elif defined REJECT_NEW_CONN
					syslog(LOG_INFO, "reject new connection %d", newFd);
					close(newFd);
					newFd = TCPPadSock;
					goto data_receive;
					#else
					goto data_receive;
					#endif
				}
#endif
				current_s_addr = TCPClientAddr.sin_addr.s_addr;
				syslog(LOG_INFO, "PAD_TCP connection established %s", inet_ntoa(TCPClientAddr.sin_addr));
			}
			else
			{
				syslog(LOG_INFO, "sock select err is %s", strerror(errno));
			}
			TCPPadSock = newFd;
			// for socket non-blocking. Make sure that we set this to non-blocking otherwise
			// it will block on subsequent accept calls in ONLINE mode and then no data can be processed.
			// We need to call accept in online mode so that we can accept a new connection in and kill the old one.
			fcntl(TCPListenSock, F_SETFL, O_NONBLOCK);
			// Reject packets from the wrong remote host
			he = gethostbyname(currentProfile->rhost);
			if (he)
			{
				memcpy(&rhost_addr, he->h_addr_list[0], 4);
				// If remote host is set to 0.0.0.0 then accept anything
				if (rhost_addr)
				{
					if (rhost_addr != TCPClientAddr.sin_addr.s_addr)
					{
						syslog(LOG_INFO, "closing socket since wrong rem host");
						close(TCPPadSock);
						TCPPadSock = -1;
						continue;
					}
				}
			}
			/* for L&G, they don't want to receive this notification */
			#ifndef V_SUPPRESS_RESPONSE_y
			fprintf(comm_host, "RING\r\n");
			fflush(comm_host);
			#endif
			CDCS_PADAnswer(stream);
		}
#ifdef V_KEEP_SINGLE_TCP_CONN_y

data_receive:
#endif
		// Get incoming data
		got = 0;
		if (currentProfile->padm == PAD_UDP)
		{
			if (UDPPadSock == -1)
			{
				UDPPadSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
				bzero(&UDPLocalAddr, sizeof(UDPLocalAddr));
				UDPLocalAddr.sin_family = AF_INET;
				UDPLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
				UDPLocalAddr.sin_port = htons(currentProfile->padp);

				if (bind(UDPPadSock, (struct sockaddr*) &UDPLocalAddr, sizeof(UDPLocalAddr)) == -1)
				{
					syslog(LOG_INFO, "Failed to bind to UDP port");
				}
			}
			FD_ZERO(&udpset);
			FD_SET(UDPPadSock, &udpset);
			udptv.tv_sec = 0;
			udptv.tv_usec = 1000000;
			selectret = select(UDPPadSock + 1, &udpset, 0, 0, &udptv);

			if (selectret == 0)
			{
				continue;
			}
			if (selectret != -1)
			{
				bzero(&UDPSourceAddr, sizeof(UDPSourceAddr));
				got = recvfrom(UDPPadSock, rxBuff, sizeof(rxBuff), 0, (struct sockaddr*) & UDPSourceAddr, &addr_size);
				// FixMeRussell - what if we get error here? close socket?
				// syslog(LOG_INFO, " UDP got %d", got);
			}
			else
			{
				syslog(LOG_INFO, "sock select err is %s", strerror(errno));
			}
		}

		if ( (currentProfile->padm == PAD_TCP) && (TCPPadSock != -1) && (nModemState == ON_LINE))
		{
		/*	we not't want to accept a new connection in and kill the old one
			if (currentProfile->pado &PAD_AUTOANS)
			{
	//syslog(LOG_INFO, "currentProfile->pado == PAD_AUTOANS");
				// for socket non-blocking
				// We need to call accept in online mode so that we can accept a new connection in and kill the old one.
				FD_ZERO(&listenset);
				FD_SET(TCPListenSock, &listenset);
				listentv.tv_sec = 0;
				listentv.tv_usec = 1000000;
				selectret = select(TCPListenSock + 1, &listenset, 0, 0, &listentv);
				if (selectret != -1)
				{
	//syslog(LOG_INFO, "creatting new connection");
					newFd = accept(TCPListenSock, (struct sockaddr*) & TCPClientAddr, &tcpClientLen);
					if (newFd != -1) {
	//syslog(LOG_INFO, "--------------newFd != -1");
						sleep(2);
						if(currentProfile->connection_op==3) {
							close(newFd);//reject 2nd connection.
							syslog(LOG_INFO, "PAD_TCP connection rejected %s (option3)", inet_ntoa(TCPClientAddr.sin_addr));
						}
						else if(currentProfile->connection_op==2) {
							// accept 2nd connection.
							close(TCPPadSock);
							TCPPadSock = newFd;
							current_s_addr = TCPClientAddr.sin_addr.s_addr;
							syslog(LOG_INFO, "PAD_TCP new connection accepted %s (option2)", inet_ntoa(TCPClientAddr.sin_addr));
						}
						else {
							if(current_s_addr==TCPClientAddr.sin_addr.s_addr) {
								close(TCPPadSock);
								TCPPadSock = newFd;
								syslog(LOG_INFO, "PAD_TCP new connection accepted %s (option1)", inet_ntoa(TCPClientAddr.sin_addr));
							}
							else {
								close(newFd);//reject 2nd connection.
								syslog(LOG_INFO, "PAD_TCP connection rejected %s (option1)", inet_ntoa(TCPClientAddr.sin_addr));
							}
						}
					}
				}
			}*/

			// Set 1s read timeout
			FD_ZERO(&readset);
			FD_SET(TCPPadSock, &readset);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			ret = select(TCPPadSock + 1, &readset, 0, 0, &tv);
			// timeout
			if (ret == 0)
			{
#ifdef V_KEEP_SINGLE_TCP_CONN_y
				if (timeout_cnt++ >= (confv250.tcp_timeout/2)) {
					syslog(LOG_INFO, "TCPPadSock timeout(%d), hang-up", confv250.tcp_timeout);
					close(TCPPadSock);
					TCPPadSock = -1;
					CDCS_V250Hangup(stream);
				} else {
					//syslog(LOG_INFO, "increase TCPPadSock timeout count %d", timeout_cnt);
					continue;
				}
#else
				continue;
#endif
			}
#ifdef V_KEEP_SINGLE_TCP_CONN_y
			timeout_cnt = 0;
#endif
			if (ret != -1)
			{
				if ((got = recv(TCPPadSock, rxBuff, sizeof(rxBuff), 0)) <= 0)
				{
					close(TCPPadSock);
					TCPPadSock = -1;
					syslog(LOG_INFO, "socket closed due to receive error in PAD so calling hangup and got is %d, result is '%s'", got, strerror(errno));
					CDCS_V250Hangup(stream);
				}
				//syslog(LOG_INFO, __func__ "TCP got %d", got);
			}
			else
			{
				syslog(LOG_INFO, "err is %s", strerror(errno));
			}
		}
		//data is available
		if (got > 0)
		{
			idle = 0;
			if (nModemState == COMMAND)
			{
				// NOTE - this part is specifically for when the PAD mode is UDP and we are set up
				// for auto answer mode. For TCP auto answer is won't come here because it uses the
				// accept call and by then it would have gone into ON_LINE mode.
				// So for UDP as soon as the first bit of data arrives we go online.
				// NOTE - that for UDP the remote address that initiated the communication has
				// to be stored so that we know who to talk back to. With TCP this is not needed
				// since an actual session is set up between the 2 parties.
				if (currentProfile->pado &PAD_AUTOANS)
				{
					if (UDPSourceAddr.sin_addr.s_addr != 0)
					{
						RemoteAddr.s_addr = UDPSourceAddr.sin_addr.s_addr;
						/* for L&G, they don't want to receive this notification */
						#ifndef V_SUPPRESS_RESPONSE_y
						// replaced by Yong for Defect#15 (RING missing in UDP server)
						fprintf(comm_host, "RING\r\n");
						fflush(comm_host);
						#endif
						CDCS_PADAnswer(stream);
					}
				}
				else
				{
					continue;
				}
			}
			// Buffer data while in ON_LINE_COMMAND mode
			while (nModemState == ON_LINE_COMMAND)
			{
				CDCS_Sleep(100);
			}
			if (currentProfile->pado &LENC_ADDRSER)
			{
				RemoteAddr.s_addr = addr;
				UDPRemoteAddr.sin_addr.s_addr = RemoteAddr.s_addr;
				CDCS_V250AddressedSerialOut(stream, rxBuff, got);
			}
			else
			{
				// Folowing is HORRIBLE hack
				// At the time of writing if you fwrite over 1024 bytes to the serial port in one go
				// Only the 1st 1024 bytes actually make it out the port
				// Further fwrite returns the number of bytes you asked it to write
				// not the number it actually wrote
				// Splitting large buffers into 2 writes and inserting a 20ms delay
				// works around the problem and in an effort to get this out the door that'l do for now
				ret = 0;
				if (got > 512)
				{
					ret = fwrite(rxBuff, 1, 512, stream);
					syslog(LOG_DEBUG, "--> fd %d (ME_Rx) : got %d > 512", fileno(stream), got);
					print_pkt(rxBuff, 512);
					fflush(stream);
					if (ret < 0)
						ret = 0;
					usleep(20000);
				}
				syslog(LOG_DEBUG, "--> fd %d (ME_Rx) (%d-%d) bytes", fileno(stream), got, ret);
				print_pkt(rxBuff + ret, got - ret);
				fwrite(rxBuff + ret, 1, got - ret, stream);
				fflush(stream);
				/* clear buffer & counter for safe */
				got = 0;
				memset(rxBuff, 0x00, 2048);
			}
		}
		CDCS_Sleep(100);
	}
	return 0;
}

// This thread primarily handles the external serial port's control lines
void* ME_V24(void* arg)
{
	FILE* stream;
	int i;
	int ret;
	int readFds[IND_COUNT_PORT+1];
	int writeFds[1];
	int fd;
	char inbuf[PASSTHRUBUFFSIZE];
	char atbuf[PASSTHRUBUFFSIZE];
	int prevModemState = COMMAND;
	unsigned long readMask;
	unsigned long writeMask;
	unsigned int start_cnt;
	unsigned int end_cnt;
	char ser_inbuf[PASSTHRUBUFFSIZE];

	last_port_vals = 0;
	this_port_vals = 0;
	stream = (FILE*)arg;

	printf( "ME_V24 Started\n");
#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
	V24_init_GPIO();
#endif
	setHostFlowType();
	start_cnt = end_cnt = 0;
	inbuf[sizeof(inbuf)-1] = '\0';
	while (1)
	{

		// check logmask
		if ((keep_alive_cnt % 20) == 0) {
			check_logmask_change();
		}

		setHostFlowType();
		handleRTRSignal();
		if (prevModemState != nModemState)
		{
			prevModemState = nModemState;
			//displayModemState();
		}

		this_port_vals = 0;
#if defined(V_SERIAL_HAS_FC_y)
#ifdef CONTROL_DIRECT_GPIO_PIN
		this_port_vals |= (gpio_read(DCE_PORT_RTS_IN)? TIOCM_RTS:0);
		this_port_vals |= (gpio_read(DCE_PORT_DTR_IN)? TIOCM_DTR:0);
		this_port_vals |= (gpio_read(DCE_PORT_CTS_OUT)? TIOCM_CTS:0);
		this_port_vals |= (gpio_read(DCE_PORT_DSR_OUT)? TIOCM_DSR:0);
		this_port_vals |= (gpio_read(DCE_PORT_RI_OUT)? TIOCM_RI:0);
		this_port_vals |= (gpio_read(DCE_PORT_DCD_OUT)? TIOCM_CD:0);
#else
		if (GetPhysSerialStatInt(IND_COUNT_PORT, 1, (int *)&this_port_vals) < 0) {
			syslog(LOG_DEBUG,"failed to read serial port status");
		}
#endif
#else
		//this_port_vals |= IOCTL_DTR;
#endif
		if (this_port_vals != last_port_vals)
		{
			;//printf("Port status change:%lx\n",this_port_vals);
		}
		if (DTR_ON)
		{
			/* pin ON, means STOP, colour GREEN */
			switch (confv250.opt_dtr)
			{
				case V250_DTR_IGNORE:
					/* 0 */
					break;
				case V250_DTR_COMMAND:
					/* 1 */
					if ((nModemState >= DTR_DIAL) && (escape_cnt != 3))
					{
						syslog(LOG_INFO, " DTR Low, request ON_LINE_COMMAND");
						escape_cnt = 3;
					}
					break;
				case V250_DTR_HANGUP:
					/* 2 */
					if ((nModemState >= DTR_DIAL) && (scheduleHangup != 1))
					{
						syslog(LOG_INFO, " DTR Low, scheduleHangup");
						scheduleHangup = 1; // You can't call CDCS_V250Hangup() from this thread
					}
					break;
				case V250_DTR_AUTODIAL:
					/* 4 */
					if ((nModemState >= DTR_DIAL) && (scheduleHangup != 1))
					{
						syslog(LOG_INFO, " DTR Low, scheduleHangup");
						scheduleHangup = 1; // You can't call CDCS_V250Hangup() from this thread
					}
					break;
				case V250_DTR_REVAUTODIAL:
					/* 3 */
					if (nModemState < DTR_DIAL)
					{
						syslog(LOG_INFO, " DTR Low, DTR_DIAL");
						nModemState = DTR_DIAL;
					}
					break;
			}
		}
		else
		{
			/* pin OFF, means GO, colour RED */
			switch (confv250.opt_dtr)
			{
				case V250_DTR_IGNORE:
					/* 0 */
					break;
				case V250_DTR_COMMAND:
					/* 1 */
					// Do Nothing refer to V.250 Spec
					break;
				case V250_DTR_HANGUP:
					/* 2 */
					break;
				case V250_DTR_AUTODIAL:
					/* 3 */
					if (nModemState < DTR_DIAL)
					{
						nModemState = DTR_DIAL;
						syslog(LOG_INFO,  " DTR High, DTR_DIAL");
					}
					break;
				case V250_DTR_REVAUTODIAL:
					/* 4 */
					if ((nModemState >= DTR_DIAL) && (scheduleHangup != 1))
					{
						syslog(LOG_INFO, " DTR High, scheduleHangup");
						scheduleHangup = 1; // You can't call CDCS_V250Hangup() from this thread
					}
					break;
			}
		}
		last_port_vals = this_port_vals;
		handleDCDLine();
		handleRILine();
		handleDSRLine();

		/* for L&G, they don't want to receive this notification */
		#ifdef V_SUPPRESS_RESPONSE_y
		if (start_cnt < end_cnt && stream == comm_host) {
			int iTbl = 0;
			char* pMsg;
			while ((pMsg = (char *)IgnoreMsgTable[iTbl++]) != 0) {
				if (strstr(&inbuf[start_cnt], pMsg)) {
					syslog(LOG_DEBUG,"do not send '%s' to meter", pMsg);
					start_cnt = end_cnt = 0;
					break;
				}
			}
		}
		#endif


		if (start_cnt < end_cnt)
		{
			writeFds[0] = fileno(stream);
			ret = waitReadWriteFds(NULL, &writeMask, 500, 0, NULL, 1, writeFds);
			if (ret <= 0)
			{
				syslog(LOG_ERR, "Should be muxed off, not reading PTY");
				/* timeout */
				continue;
			}

			ret = write(fileno(stream), &inbuf[start_cnt], end_cnt - start_cnt);
			syslog(LOG_DEBUG, "--> fd %d (ME_V24)", fileno(stream));
			if (inbuf[start_cnt] == 0x0d)
				syslog(LOG_DEBUG, "    '%s'", &inbuf[start_cnt]);
			print_pkt(&inbuf[start_cnt], end_cnt - start_cnt);
			if (ret == 0)
			{
				/* ouch */
				syslog(LOG_ERR, "Host comm port closed, restart");
				exit(1);
			}
			if (ret < 0)
			{
				if (errno == EAGAIN || errno == EINTR)
					continue;
				/* ouch */
				syslog(LOG_ERR, "Host comm port error, restart");
				exit(1);
			};
			start_cnt += ret;
		}
		/* using push back as flow control */
		if (start_cnt >= end_cnt)
			start_cnt = end_cnt = 0;
		while (1)
		{
			for (i = 0;i < IND_COUNT_PORT;i++)
			{
				readFds[i] = -1;
			}
			if ((fd = canPassThroughAT()) >= 0)
				readFds[IND_AT_PORT] = fd;
			//readMuxdSignals(stream);
			ret = waitReadWriteFds(&readMask, NULL, 50, IND_COUNT_PORT, readFds, 0, NULL);

			if (ret <= 0)
			{
				/* timeout */
				break;
			}
			if (!readMask) {
				//syslog(LOG_DEBUG,"readMask:0");
				break;
			}
			//syslog(LOG_DEBUG,"readMask:0x%02lx", readMask);
			if (readMask &BITR_AT_PORT)
			{
				if (canPassThroughAT() >= 0 && comm_phat)
				{
					ret = readAndBuffer(comm_phat, 0, atbuf, sizeof(atbuf));
					if (nModemState != ON_LINE)
						ret = ignoreIfQuiet(atbuf, ret);

					if(ret > 0) {
						syslog(LOG_DEBUG, "<-- fd %d (ME_V24) #1", fileno(comm_phat));
						if (atbuf[0] == 0x0d)
							syslog(LOG_DEBUG, "    '%s'", atbuf);
						//print_pkt((char *)&atbuf[0], ret);
						if ( nModemState != ON_LINE || currentConnectType == V250_DIALPORT_DIALSTR || currentConnectType == V250_DIALPORT_CIRCUIT ) {
							if(strstr(atbuf, "CONNECT")) {
								nModemState = ON_LINE;
							}
							if ( nModemState == ON_LINE && (currentConnectType == V250_DIALPORT_DIALSTR || currentConnectType == V250_DIALPORT_CIRCUIT) ) {
								if(strstr(atbuf, "NO CARRIER\r")) {
									syslog(LOG_DEBUG,"Got 'NO CARRIER' from remote");
									if(currentConnectType == V250_DIALPORT_CIRCUIT)
										ret=0;
									CDCS_V250Hangup(stream);
								}
							}
							#ifdef NO_USE
							if(strstr(atbuf, "RING") || strstr(atbuf, "NO CARRIER\r")) {
								// for Cinterion module
								if (is_cinterion_module) {
									syslog(LOG_DEBUG,"send AT to modem");
									send_keep_alive_at();
								}
							}
							#endif

							if (start_cnt > 0)
							{
								for (i = 0;i < end_cnt - start_cnt;i++)
									inbuf[i] = inbuf[i+start_cnt];
								end_cnt -= start_cnt;
								start_cnt = 0;
							}
							for (i = 0; i<ret && i<sizeof(inbuf)-end_cnt-1; i++)
								inbuf[i+end_cnt] = atbuf[i];
							end_cnt += i;
						}
					}
				} else {
				}
			}
			if ((readMask &BITR_SERIAL_PORT) && (pausePassAt(0) == 0))
			{
				if ( (currentConnectType == V250_DIALPORT_DIALSTR || currentConnectType == V250_DIALPORT_CIRCUIT) && comm_phat)
				{
					ret = readAndBuffer(comm_phat, 0, &inbuf[start_cnt], sizeof(inbuf) - end_cnt - 1);
					if (nModemState != ON_LINE)
						ret = ignoreIfQuiet(&inbuf[start_cnt], ret);

					if (ret > 0) {
						end_cnt += ret;
						syslog(LOG_DEBUG, "<-- fd %d (ME_V24) #2", fileno(comm_phat));
						if (inbuf[start_cnt] == 0x0d)
							syslog(LOG_DEBUG, "    '%s'", &inbuf[start_cnt]);
						print_pkt(&inbuf[start_cnt], ret);
					}
					if (end_cnt && end_cnt > start_cnt && (chan_signals[IND_AT_PORT] &S_DV) == 0 && nModemState == CSD_DIAL)
					{
						inbuf[end_cnt] = '\0';
						if (checkNoConnectResponse(&inbuf[start_cnt]))
						{
							currentConnectType = V250_DIALPORT_DIALSTR;
							nModemState = COMMAND;
						}
					}
				}
			}
		}
		// 400 times : 20 seconds
		if (keep_alive_cnt++ >= 400) {
			if (is_cinterion_module && nModemState == COMMAND) {
				syslog(LOG_DEBUG,"----------------------------------------------------------------------");
				syslog(LOG_DEBUG,"send AT to modem");
				send_keep_alive_at();
			}
			keep_alive_cnt = 0;
		}

	}
	return 0;
}

/*
 * vim:ts=4:sw=4
      */
