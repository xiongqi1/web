/**
 * @file max172xx.h
 * @brief Defines MAX172xx chip information.
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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
 */

#ifndef MAX172XX_H_09520010052019
#define MAX172XX_H_09520010052019

/*
 * MAX172XX has 3 register areas (Table 16, MAX17201-MAX17215 datasheet):
 * 1) 0x000-0x0ff: ModelGauge m5 data
 * 2) 0x100-0x17f: SBS data
 * 3) 0x180-0x1ff: non-volatile data
 * There are two 8-bit I2C slave addresses for the above areas:
 * 0x6C (7-bit addr: 0x36): for registers 0x000-0x0ff
 * 0x16 (7-bit addr: 0x0b): for registers 0x100-0x1ff
 */
// i2c address for non volatile registers (shadow RAM)
#define MAX172XX_I2C_NADDR 0x0b

#define MAX172XX_REG_COMMAND_ADDR 0x60

#define MAX172XX_REG_PACKCFG_ADDR 0xbd

#define MAX172XX_REG_PACKCFG_TEMP_AIN1 0x12
#define MAX172XX_REG_PACKCFG_VOLT_BATT 0x4 // 100b voltage measured of CELL1, CELL2 & Vbatt pins
#define MAX172XX_REG_PACKCFG_TEMP_SHIFT 11
#define MAX172XX_REG_PACKCFG_VOLT_SHIFT 8
#define MAX172XX_REG_PACKCFG_BALCFG_SHIFT 5
#define MAX172XX_REG_PACKCFG_NCELLS_SHIFT 0

#define MAX172XX_REG_FULL_CAP_REP_ADDR 0x35

#define MAX172XX_REG_CONFIG2_ADDR 0xbb

// Non-volatile registers. They are 8-bit address (Table 27, MAX17201-MAX17215 datasheet)
#define MAX172XX_REG_NICHG_TERM_ADDR 0x9c
#define MAX172XX_REG_NV_EMPTY_ADDR 0x9e
#define MAX172XX_REG_NFULL_CAP_NOM_ADDR 0xa5
#define MAX172XX_REG_NFULL_CAP_REP_ADDR 0xa9
#define MAX172XX_REG_NDESIGN_CAP_ADDR 0xb3
#define MAX172XX_REG_NPACKCFG_ADDR 0xb5
#define MAX172XX_REG_NFULL_SOC_THR_ADDR 0xc6

#define MAX172XX_REG_NNVCFG0_ADDR 0xb8
#define MAX172XX_REG_NNVCFG1_ADDR 0xb9
#define MAX172XX_REG_NNVCFG2_ADDR 0xba

// power-on-reset time (millisec)
#define MAX172XX_POR_TIME_MS 10

// reset delay time (second)
#define MAX172XX_RESET_DELAY_SEC 5

#endif // MAX172XX_H_09520010052019
