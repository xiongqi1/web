#ifndef __IOMGR_H__10052016
#define __IOMGR_H__10052016

/*
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The IoMgr sits between the IO ( Industrial Io (IIo) and GPIO ) and our
 * RDB. Inputs are written to the RDB. Updates to the RDB are output.
 * Also Io modes are configured.
 */

#include <cdcs_rdb.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <syslog.h>

//#define TEST
#ifdef TEST
#include <stdio.h>
#define DBG(level,fmt,...) fprintf(level<LOG_ERR?stderr:stdout,"<%d:%s:%s> " fmt "\n",level,__FILE__,__FUNCTION__,##__VA_ARGS__)
#else
#define DBG(level,fmt,...)	syslog(level,"<%s:%s> " fmt,__FILE__,__FUNCTION__,##__VA_ARGS__)
#endif
#define GPIO_UNUSED		(-1)
#define GPIO_BANK_PIN(b,p)	(((b)*32)+(p))

#define IO_INFO_CAP_MASK_AIN	(1<<0)
#define IO_INFO_CAP_MASK_DIN	(1<<1)
#define IO_INFO_CAP_MASK_AOUT	(1<<2)
#define IO_INFO_CAP_MASK_DOUT	(1<<3)
#define IO_INFO_CAP_MASK_TEMP	(1<<4)
#define IO_INFO_CAP_MASK_1WIRE	(1<<5)
#define IO_INFO_CAP_MASK_CIN	(1<<6)
#define IO_INFO_CAP_MASK_NAMUR	(1<<7)
#define IO_INFO_CAP_MASK_CT	(1<<8)
#define IO_INFO_CAP_MASK_CC	(1<<9)
#define IO_INFO_CAP_MASK_VDIN	(1<<10)

// This also marks the end of modes above
#define IO_INFO_CAP_MASK_BUFFERED (1<<11)

/* power sources (index to info) */
#define IO_MGR_POWERSOURCE_NONE		0
#define IO_MGR_POWERSOURCE_DCJACK	(1<<13)
#define IO_MGR_POWERSOURCE_POE		(1<<14)
#define IO_MGR_POWERSOURCE_NMA1500	(1<<15)
#define IO_MGR_POWERSOURCE_MASK		(IO_MGR_POWERSOURCE_DCJACK|IO_MGR_POWERSOURCE_POE|IO_MGR_POWERSOURCE_NMA1500)

#define digitalInputMode (IO_INFO_CAP_MASK_DIN|IO_INFO_CAP_MASK_VDIN)
#define digitalOutputMode (IO_INFO_CAP_MASK_DOUT)
#define analogInputMode (IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_CIN|IO_INFO_CAP_MASK_NAMUR|IO_INFO_CAP_MASK_CT|IO_INFO_CAP_MASK_CC|IO_INFO_CAP_MASK_TEMP|IO_INFO_CAP_MASK_VDIN)
#define analogOutputMode (IO_INFO_CAP_MASK_AOUT)
#define isInputMode(mode) (0 != ((mode)&(digitalInputMode|analogInputMode)))
#define isOutputMode(mode)(0 != ((mode)&(digitalOutputMode|analogOutputMode)))

/*
 * Calculate the I/O scaling factor.
 *
 * From the schematic you will see a voltage divider:
 *   Vadc = I . R_bottom
 *   Where I = Vin/(R_top + R_bottom)
 *   Thus Vin = Vadc . (R_top + R_bottom)/R_bottom
 *   Ie.  Vin = Vadc . scalingFactor
 * Supports io_info_t.scale.
 * @param top     Top resistance value in voltage divider.
 * @param bottom  Bottom resistance value in voltage divider.
 * @return scalingFactor
 */
#define RESISTOR_DIVIDER(R_top, R_bottom) \
    ((((double)(R_top))+(R_bottom))/(R_bottom))

extern uint samplingFrequency;

class Io;
class IIoDevice;

void ioPushSamples(Io * pIo, uint * samples, int cnt );
void gpioCreate(const std::string & name, int basegpio, int cnt );
void iioCreate(IIoDevice * pIIoDev);

#endif
