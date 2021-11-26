#ifndef _ModemEmulator_UTILS_H_
#define _ModemEmulator_UTILS_H_

/*!
 * Copyright Notice:
 * Copyright (C) 2002-2010 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * $Log: ModemEmulator_Utils.h,v $
 *
 * Revision 1.0  2010/03/31
 * created
 *
 */

#include <sys/types.h>

/*for debugging*/
#define print_bits(n) printf("%d%d%d%d%d%d%d%d", ((n&128) == 128), \
                             ((n&64) == 64),((n&32) == 32),((n&16) == 16), \
                             ((n&8) == 8),((n&4) == 4),((n&2) == 2), \
                             ((n&1) == 1));


int GetPhysSerialStat(int iCh, int* pStat);
int SetPhysSerialStat(int iCh, int* pStat);
void InitPhysSerialStat(int iCh, int fd);

// V.24 signals: flow control, ready to communicate, ring indicator, data valid
#define S_FC  	0x02
#define S_RTC  	0x04  /* DTR/DSR */
#define S_RTR  	0x08  /* RTS/CTS */
#define S_IC  	0x40  /* RI */
#define S_DV  	0x80  /* DCD */

#define BITR_AT_PORT  		1
#define BITR_SERIAL_PORT 	2

#if defined(V_GPIO_STYLE_atmel)
/* NTC-6908 control GPIO directly. */
#define CONTROL_DIRECT_GPIO_PIN
#define DCE_PORT_RTS_IN    PIN_B(29)	// 61
#define DCE_PORT_DTR_IN    PIN_B(31)	// 63
#define DCE_PORT_CTS_OUT   PIN_B(28)	// 60
#define DCE_PORT_DSR_OUT   PIN_B(30)	// 62
#define DCE_PORT_DCD_OUT   PIN_B(16)	// 48
#define DCE_PORT_RI_OUT    PIN_B(17)	// 49
#define IOCTL_DTR	TIOCM_DTR
#elif defined(V_GPIO_STYLE_freescale)

#if defined(V_SERIAL_HAS_FC_y)
/* NTC-NWL12 uses AUART4 for external serial port. */
#define TIOCM_OUT1	0x2000				// 1:4, 36		DCD
#define TIOCM_OUT2	0x4000				// 1:26, 58		RI
#else
/* NTC-1101 uses AUART1 for external serial port without flow control lines. */
#endif
#define IOCTL_DTR	TIOCM_DSR

#elif defined(V_GPIO_STYLE_qualcomm) || defined(V_GPIO_STYLE_ti)
#define CONTROL_DIRECT_GPIO_PIN
#define DCE_PORT_RTS_IN    PIN_B(29)	// 61
#define DCE_PORT_DTR_IN    PIN_B(31)	// 63
#define DCE_PORT_CTS_OUT   PIN_B(28)	// 60
#define DCE_PORT_DSR_OUT   PIN_B(30)	// 62
#define DCE_PORT_DCD_OUT   PIN_B(16)	// 48
#define DCE_PORT_RI_OUT    PIN_B(17)	// 49
#define IOCTL_DTR	TIOCM_DTR
#else
#error V_GPIO_STYLE should be defined!
#endif

#define DTR_ON 	(this_port_vals & IOCTL_DTR)

#define CHK(function) {int rval; rval=function; syslog(LOG_INFO, "CHK[" #function "]=>%d\n",rval); rval=rval; }

int setNotify( char* name, char sigh );
int CDCS_SaveWWANConfig(WWANPARAMS* p, int num);
int CDCS_getWWANProfile(WWANPARAMS* p, int num);
char *getSingle( char *myname );
char *getSingleNA( char *myname );
int setSingleVal( char* name, u_long var);
int getSingleVal( char *myname );
int isAnyProfileEnabled( void );
int cfg_serial_port(FILE* port, speed_t baud);
const char* get3GIfName();
#if defined(V_SERIAL_HAS_FC_y) && defined(CONTROL_DIRECT_GPIO_PIN)
int V24_init_GPIO();
#endif
int ConvSendCtlToV24(unsigned char bCtl);
int ConvSendCtlToV24(unsigned char bCtl);
int TogglePhysSerialStatOff(int iCh, int nStat);
int CDCS_SaveMappingConfig(TABLE_MAPPING* p, int num );
int GetPhysSerialStatInt(int iCh, int fGet, int* pStat);
int TogglePhysSerialStatOn(int iCh, int nStat);
void print_pkt(char* pbuf, int len);
void send_keep_alive_at( void );
void check_logmask_change(void);
#endif
