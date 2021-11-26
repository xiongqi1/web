/**
 * @file fan5410x.h
 * @brief Provides public macros to control a fan5410x chip
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef _PRODUCT_FAN5410X_H_
#define _PRODUCT_FAN5410X_H_

/* The following macros are based on fan541x.h written by Jalal */

#define	FAN54005_SlaveAddr	(0xd4 >> 1)
#define	FAN54013_SlaveAddr	(0xd6 >> 1)
#define	FAN5401x_CTRL0		0x00
#define	FAN5401x_CTRL1		0x01
#define	FAN5401x_OREG		0x02
#define	FAN5401x_ICINFO		0x03
#define	FAN5401x_IBAT		0x04
#define	FAN5401x_SPCHARGER	0x05
#define	FAN5401x_SAFETY		0x06
#define	FAN5401x_MON		0x10

#define IinLIM_100mA		(0x0 << 6)
#define IinLIM_500mA		(0x1 << 6)
#define IinLIM_800mA		(0x2 << 6)
#define IinLIM_NoLim		(0x3 << 6)

#define Vlowv_3400mV		(0x0 << 4)
#define Vlowv_3500mV		(0x1 << 4)
#define Vlowv_3600mV		(0x2 << 4)
#define Vlowv_3700mV		(0x3 << 4)

#define Iterm_Enabled       (0x1 << 3)

#define Charger_Disabled	(0x1 << 2)

#define Iocharge_550mA		(0x0  << 4)
#define Iocharge_650mA		(0x01 << 4)
#define Iocharge_750mA		(0x02 << 4)
#define Iocharge_850mA		(0x03 << 4)
#define Iocharge_1050mA		(0x04 << 4)
#define Iocharge_1150mA		(0x05 << 4)
#define Iocharge_1350mA		(0x06 << 4)
#define Iocharge_1450mA		(0x07 << 4)

#define Iterm_49mA			(0x0)
#define Iterm_97mA			(0x1)
#define Iterm_146mA			(0x2)
#define Iterm_194mA			(0x3)
#define Iterm_243mA			(0x4)
#define Iterm_291mA			(0x5)
#define Iterm_340mA			(0x6)
#define Iterm_388mA			(0x7)

#define Isafe_550mA			(0x0 << 4)
#define Isafe_650mA			(0x1 << 4)
#define Isafe_750mA			(0x2 << 4)
#define Isafe_850mA			(0x3 << 4)
#define Isafe_1050mA		(0x4 << 4)
#define Isafe_1150mA		(0x5 << 4)
#define Isafe_1350mA		(0x6 << 4)
#define Isafe_1450mA		(0x7 << 4)

#define Vsafe_VOregMax_4200mV		(0x0)
#define Vsafe_VOregMax_4220mV		(0x1)
#define Vsafe_VOregMax_4240mV		(0x2)
#define Vsafe_VOregMax_4260mV		(0x3)
#define Vsafe_VOregMax_4280mV		(0x4)
#define Vsafe_VOregMax_4300mV		(0x5)
#define Vsafe_VOregMax_4320mV		(0x6)
#define Vsafe_VOregMax_4340mV		(0x7)
#define Vsafe_VOregMax_4360mV		(0x8)
#define Vsafe_VOregMax_4380mV		(0x9)
#define Vsafe_VOregMax_4400mV		(0xa)
#define Vsafe_VOregMax_4420mV		(0xb)
#define Vsafe_VOregMax_4440mV		(0xc)

//#define Voreg_4400mV		(0x2d << 2)

#define Voreg_ref_3500mV	(0x00)
#define Voreg_Coder(x)		((Voreg_ref_3500mV + (x-3500)/20) << 2)

#define VBAT_MON_MIN		(3.0)
#define VBAT_MON_L1			(3.2)
#define VBAT_MON_L2			(3.5)
#define VBAT_MON_L3			(3.7)
#define VBAT_MON_L4			(4.9)
#define VBAT_MON_Full		(4.1)

#define MON_CV				(0x1<< 0)
#define MON_VBUS_VALID		(0x1<< 1)
#define MON_IBUS			(0x1<< 2)
#define MON_ICHG			(0x1<< 3)
#define MON_T_120			(0x1<< 4)
#define MON_LIN_CHNG		(0x1<< 5)
#define MON_VBAT_CMP		(0x1<< 6)
#define MON_ITERM_CMP		(0x1<< 7)

#define FAN_5401_ChipId_Encoder(x)		(x & 0xfc)

#define FAN54010_ID			((0x4 << 5) | (0x3<<2))
#define FAN54011_ID			((0x4 << 5) | (0x1<<2))
#define FAN54012_ID			((0x4 << 5) | (0x3<<2))
#define FAN54013_ID			((0x4 << 5) | (0x5<<2))
#define FAN54014_ID			((0x4 << 5) | (0x7<<2))

#define FAN54005_ID			((0x4 << 5) | (0x5<<2))

#define CHARGER_STAT_BIT_MASK             0x3
#define CHARGER_STAT_BIT_POS              4
#define CHARGER_STAT_READY                0
#define CHARGER_STAT_CHARGER_IN_PROGRESS  1
#define CHARGER_STAT_CHARGER_DONE         2
#define CHARGER_STAT_FAULT                3

#endif /* _PRODUCT_FAN5410X_H_ */
