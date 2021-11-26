#ifndef _CDCS_MODEMEMULATOR_H_
#define _CDCS_MODEMEMULATOR_H_

/*!
 * Copyright Notice:
 * Copyright (C) 2002-2010 Call Direct Cellular Solutions Pty. Ltd.
 */
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/msg.h>
#include <unistd.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <termios.h>
#include "rdb_ops.h"
#include "libgpio.h"

#define MAX_RDB_VAR_SIZE 512

#define phone_port1  "/dev/ttyUSB2"
#if defined (SERIAL_PORT_TTYAPP1)
#define host_port  "/dev/ttyAPP1"
#elif defined (SERIAL_PORT_TTYAPP2)
#define host_port  "/dev/ttyAPP2"
#elif defined (SERIAL_PORT_TTYAPP3)
#define host_port  "/dev/ttyAPP3"
#elif defined (SERIAL_PORT_TTYAPP4)
#define host_port  "/dev/ttyAPP4"
#else
#define host_port  "/dev/ttyS2"
#endif

extern char* phone_at_name;
extern char* phone_serial_name;

extern char* host_name;

#define PASSTHRUBUFFSIZE 	2048
#define AT_RETRY_BUFFSIZE 	512
#define AT_TIMEOUT			100		/* ms */

#define CDCS_SUCCESS  		0
#define STATUS_IDLE   		0
#define STATUS_CONNECTED 	1
#define STATUS_DORMANT      2
#define STATUS_ENDED     	3
#define CONNECTION_ACTIVE   21

#define IND_AT_PORT   		0
//#define IND_SERIAL_PORT  1
#define IND_COUNT_PORT  	1

// V250 Responses
enum
{
	OK, CONNECT, RING, NOCARRIER, ERROR, OTHER, NODIALTONE, BUSY, NOANSWER, NORESP, UPDATEREQ
};


// V250 Modes
enum
{
	COMMAND, DISC_TIMEOUT, DTR_DIAL, ON_LINE, ON_LINE_COMMAND, CSD_DIAL, TELNET, LOCAL, DISABLE_V250
};


#define V250_DIALPORT_DIALSTR	0
#define V250_DIALPORT_PROFILE	1
#define V250_DIALPORT_CIRCUIT	2
#define V250_DIALPORT_PACKET	3

#define V250_DTR_IGNORE			0
#define V250_DTR_COMMAND		1
#define V250_DTR_HANGUP			2
#define V250_DTR_AUTODIAL		4
#define V250_DTR_REVAUTODIAL	5
#define V250_DTR_LOPASSAT		6

#define V250_DCD_ALWAYS			0
#define V250_DCD_CONNECT		1
#define V250_DCD_NEVER			2
#define V250_DCD_PPP			3

#define V250_RI_ALWAYS			0
#define V250_RI_RING			1
#define V250_RI_NEVER			2

#define V250_DSR_ALWAYS			0
#define V250_DSR_REGISTERED		1
#define V250_DSR_PPP			2
#define V250_DSR_NEVER			3

#define EMUL_STANDARD			0
#define EMUL_SIMOCO				1
#define EMUL_NUMERIC			2
#define EMUL_HIGHEST			2

//padm PAD Modes
enum
{
	PAD_DISABLED, PAD_TCP, PAD_UDP
};


//opt_1 options
#define ECHO_ON                 0x0001
#define QUIET_ON                0x0002
#define OK_ON_CR                0x0004
#define SUPRESS_LF              0x0008
#define OK_ON_UNKNOWN           0x0010
#define VERBOSE_RSLT            0x0020

//opt_2 options
#define IO_MONITOR              0x0001
#define OPT_PASSTHROUGH         0x0002
#define BANNER_OFF              0x0004
#define CPROG_OFF               0x0008

#define MAPP_PROT_TCP 0
#define MAPP_PROT_UDP 1
#define MAPP_PROT_ALL 2

typedef struct
{
	char protocol[4];
	char remoteSrcIPAddr[33];
	u_short localPort1;
	u_short localPort2;
	char destIPAddr[33];
	u_short destPort1;
	u_short destPort2;
} TABLE_MAPPING;


typedef struct
{
	u_char opt_1; /*!< \brief Bitmaped options 1 */
	u_char opt_2; /*!< \brief Bitmaped options 2 */
	//u_short opt_3;	/*!< \brief Bitmaped options 3 */
	u_char emul; /*!< \brief Device Emulation */
	u_char opt_gasc; /*!< \brief Current profile  */
	u_char opt_dtr; /*!< \brief DTR Options */
	u_char opt_dsr; /*!< \brief DSR Options */
	u_char opt_rts; /*!< \brief RTS Options */
	u_char opt_cts; /*!< \brief CTS Options */
	u_char opt_dcd; /*!< \brief DCD Options */
	u_char opt_ri; /*!< \brief RI Options */
	u_char opt_fc; /*!< \brief Flow Control options */
	u_short icto; /*!< \brief Intercharacter timeout */
	u_short idct; /*!< \brief Idle disconnect timeout */
	u_short sest; /*!< \brief Maximum session timeout */
	u_long ipr; /*!< \brief Baud rate of ext serial port */
	u_char format; /*!< \brief Character format of ext serial port */
	u_char parity; /*!< \brief Parity of ext serial port */
	char dialPort;
	char autoDialNumber[64];
	u_char modemAutoAnswer;
	u_short appt1;	/*!< \brief APT timer */
	u_char s7_timer;	// ATS7 Dial Timout value
	char ident[33]; /*!< \brief Local device ID */
	char ignoreStr[128]; /* optional ignore string */
	char shost[64];	/*!< \brief host */
#ifdef V_KEEP_SINGLE_TCP_CONN_y
	u_short tcp_timeout; /*!< \TCP connection timeout */
#endif
} CONFV250;

typedef struct
{
	char dial_number[32];
	char apn_name[128];
	char user[128];
	char pass[128];
	char rhost[128]; /* Pad_IP */
	u_char padm; /* Pad_Mode */
	u_char pado; /* Auto_Answer&PAD_AUTOANS   Serial_Prot(Local Encoding)&LENC_ADDRSER Serial_DNP&LENC_DNP3*/
	u_short padp; /* Pad_Port */
	u_char readOnly;
	u_char connection_op;
	u_char tcp_nodelay;
} WWANPARAMS;

//pado options
#define LENC_DNP3				0x04
#define LENC_ADDRSER            0x10
#define GPRS_AUTO_CON           0x20
#define GPRS_D_DEMAND           0x40
#define PAD_AUTOANS             0x80

extern int currentProfileIx;
extern struct timespec dialTime;
extern int config_fd;
extern int nModemState;
extern char currMacString[20];
extern int _callStatus;
extern int is_cinterion_module;

int getPID(char* process);

void displayBanner(void);

char** StrSplit(u_short* Num, char* String, char Delimiter);

void getModel(u_char* respBuff, unsigned int maxresplen);
int TermPPPIfNeeded(void);
int CDCS_Sleep(u_long timems);
void SetPhoneBaud(FILE* pPort, speed_t baudToSet);
void killProcess(char* processName);
speed_t get_serial_port_baud(FILE* port);

void CDCS_V250Initialise(FILE* stream);

void InitPhysSerialStat(int iCh, int fd);
void setTTYCharacterFormat(FILE* tty, char format, char parity);

unsigned long remoteAsNumber(char* host);
int baud_to_int(speed_t baud);
speed_t int_to_baud(int i);
int waitReadWriteFds(unsigned long* readMask, unsigned long* writeMask, int timeout, int nReadFds, int* listReadFds, int nWriteFds, int* listWriteFds);
int unescape_to_buf(char* d, char* s, int maxd);
void abortConnecting(void);
int writeToChatScript(char* dialNum, char* apn);
int WWANOpened(void);
void CDCS_WWAN_Terminate(void);
char checkPPPDRunning(void);
void disablePPPServerMode(void);
int splitat(char* buf, char** newptr, char** ptrs, int nitems, char atchar);
int cfg_port_details(int fd, speed_t baud, unsigned char vmin, unsigned char vtime, int blocking);
int CDCS_getWWANProfile(WWANPARAMS* p, int num);
int readDHCPFile(char* startRange, char* endRange, char* leaseTime, char* ddnSuffix, char* winsip1, char* winsip2, char* dns1, char* dns2, char* netmask);
void getMACAddr(char* macAddress);
void saveEthConfig(char* macString);
int writeToDHCPFile(char* startRange, char* endRange, char* leaseTime, char* ddnSuffix, char* winsip1, char* winsip2, char* dns1, char* dns2);
void getInterfaceSubnetMask(char* mask, char* iface);
void insertRules(char* mapping, char* iface);
void stopDHCP(void);
int isHex(char c);
int CDCS_LoadMappingConfig(TABLE_MAPPING* p, int num);
int CDCS_DeleteMappingConfig(int num);
int eatHostPortDataUntilEmpty(void);
int eatPhonePortDataUntilEmpty(void);

void SetATStr(FILE* fp, char* str, int fForce);
void host_dcd_on();
void host_dcd_off();
void host_dsr_on();
void host_dsr_off();

void checkModemAutoAnswer(int fForce);
void checkModemVerbose(int fForce);
void checkModemQuietMode(int fForce);
void ModemEmulatorLoop(FILE* stream);
void* ME_Rx(void* arg);
void* ME_V24(void* arg);
void CDCS_V250Hangup(FILE* stream);
int SendATCommand(FILE* pPort, char* pReq, char* pResp, int resp_size, int timeout_sec);

extern volatile int loopCheckV250;

// Flag Bits
#define Phone_Status  0x01
#define Phone_Enable  0x02
#define Auto_GPRS_Attach 0x04
#define GPRS_Attached  0x08
#define MIP_ReRegister  0x10
#define Get_CDMA_Status  0x20
#define Disable_CDMA_Status 0x40

#define PRFX "^"
#define PRFXL 1

#define ping_command   "ping -c 4"

typedef struct dnp_header
{
	u_char starthi; //!< \brief 0 Start code 0x0564
	u_char startlo; //!< \brief 1 Start code 0x0564
	u_char length; //!< \brief 2 Frame length
	u_char control; //!< \brief 3 Control byte
	u_char desthi; //!< \brie4 4 Destination address
	u_char destlo; //!< \brief 5 Destination address
	u_char srchi; //!< \brief 6 Source Address
	u_char srclo; //!< \brief 7 Source Address
	u_char crchi; //!< \brief 8 Frame CRC
	u_char crclo; //!< \brief 9 Frame CRC
} DNPHEADER; //__attribute__((packed))

typedef struct dialDetails
{
	char rhost[128];
	u_char padm;
	u_short padp;
	int portToUse;
	int profileNumber;
}DIAL_DETAILS;

#define RESTART_PROCESS_VIA_SIGHUP

#ifdef RESTART_PROCESS_VIA_SIGHUP
/* This is set to true when a signal is received. */
static volatile sig_atomic_t received_sighup = 0;
extern void sighup_restart(void);
#endif

#endif
