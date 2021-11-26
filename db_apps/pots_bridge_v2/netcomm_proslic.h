#ifndef __NETCOMM_PROSLIC_H_20180620__
#define __NETCOMM_PROSLIC_H_20180620__

/*
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "proslic.h"
#include <si_voice_datatypes.h>
#include <si_voice.h>

#define SILABS_BITS_PER_WORD    8        /* MUST be either 8, 16, or 32 */
#define SILABS_SPI_RATE         4000000  /* In terms of Hz */
#define SILABS_MAX_RAM_WAIT     1000     /* In terms of loop count */
#define SILABS_RESET_GPIO       135
#define SILABS_MAX_CHANNELS     4
//#define SILABS_RAM_BLOCK_ACCESS 1        /* Define this if you wish to use a single block to write to RAM, may not work on all systems */
#define SILABS_DEFAULT_DBG      7
#define SILABS_MIN_MSLEEP_TIME  30       /* threshold to call mdelay vs. msleep() */
#define SILABS_MSLEEP_SLOP      10       /* If the msleep() function is off by a constant value, set this number positive if it's too long or negative number for too short - terms of mSec */
#define PROSLIC_XFER_BUF        4        /* How many bytes to send in 1 transfer.  Either 1,2, or 4.  If setting SILABS_BITS_PER_WORD to 16 or 32, you MUST set this either 2 or 4 */

/******************* TIMER ********************/

#ifdef __linux__
#define SILABS_CLOCK CLOCK_MONOTONIC_RAW /* see clock_gettime() for options */
#else
#define SILABS_CLOCK CLOCK_MONOTONIC /* see clock_gettime() for options */
#endif

/***************** SPI USERSPACE *****************/
#ifdef __linux__

//#define SILABS_SPIDEV "/dev/spidev0.3"
//#define LINUX_GPIO   "/sys/class/gpio/gpio39/value" /* Which GPIO pin to use for reset? */
#define SILABS_MAX_SPI_SPEED 9000000 /* This is actually not the maximum physical speed, just a "safe" max. */
//#define SILABS_SPI_RATE      1000000 /* For IOC_XFER */
#define PROSLIC_MAX_RAM_WAIT  100


/* #define SPI_TRC printf */ /* Uncomment this line and comment the one below to get SPI debug */
#define SPI_TRC(...)
//#define SILABS_USE_IOC_XFER 1  /* Set this if your SPIDev implementation does not support read/write and just ioctl transfers */

#define SILABS_BITS_PER_WORD 8 /* MUST be either 8, 16 or 32 */
#define SILABS_RAMWRITE_BLOCK_MODE 1 /* If enabled, will send 24 bytes down vs. register access mode, in some systems this is more efficient */

#define SILABS_BYTE_LEN (SILABS_BITS_PER_WORD/8) /* Should be able to set this independent of BITS_PER_WORD */
#endif


/* netcomm_proslic.c */

typedef struct {
    int spi_fd;
    int reset_fd;

} SiVoice_CtrlIf_t;

typedef SiVoice_CtrlIf_t ctrl_S;

typedef struct {
    int dummy; /* put a placeholder here to avoid compiler warnings */
} systemTimer_S;


typedef struct {
    struct timespec timerObj;
} timeStamp;


/* ProSLIC register deinfes */
#define PROSLIC_REG_RINGCON_TI_EN (1<<3)
#define PROSLIC_REG_RINGCON_TA_EN (1<<4)

#define PROSLIC_REG_IRQEN1_FSKBUF_EMPTY (1<<7)
#define PROSLIC_REG_IRQEN1_FSKBUF_AVAIL (1<<6)
#define PROSLIC_REG_IRQEN1_RING_TI_IE (1<<5)
#define PROSLIC_REG_IRQEN1_RING_TA_IE (1<<4)
#define PROSLIC_REG_IRQEN1_OSC2_TI_IE (1<<3)
#define PROSLIC_REG_IRQEN1_OSC2_TA_IE (1<<2)
#define PROSLIC_REG_IRQEN1_OSC1_TI_IE (1<<1)
#define PROSLIC_REG_IRQEN1_OSC1_TA_IE (1<<0)
#define PROSLIC_REG_IRQEN1_OSCS_TA_IE (PROSLIC_REG_IRQEN1_OSC1_TA_IE|PROSLIC_REG_IRQEN1_OSC2_TA_IE)
#define PROSLIC_REG_IRQEN1_OSCS_TI_IE (PROSLIC_REG_IRQEN1_OSC1_TI_IE|PROSLIC_REG_IRQEN1_OSC2_TI_IE)


#define PROSLIC_REG_OCON_OSC1_EN (1<<0)
#define PROSLIC_REG_OCON_OSC1_TI_EN (1<<1)
#define PROSLIC_REG_OCON_OSC1_TA_EN (1<<2)
#define PROSLIC_REG_OCON_OSC1_EN_SYNC_1 (1<<3)
#define PROSLIC_REG_OCON_OSC2_EN (1<<4)
#define PROSLIC_REG_OCON_OSC2_TI_EN (1<<5)
#define PROSLIC_REG_OCON_OSC2_TA_EN (1<<6)
#define PROSLIC_REG_OCON_OSC2_EN_SYNC_1 (1<<7)

#define PROSLIC_REG_OCON_OSCS_EN (PROSLIC_REG_OCON_OSC1_EN|PROSLIC_REG_OCON_OSC2_EN)
#define PROSLIC_REG_OCON_OSCS_TA_EN (PROSLIC_REG_OCON_OSC1_TA_EN|PROSLIC_REG_OCON_OSC2_TA_EN)
#define PROSLIC_REG_OCON_OSCS_TI_EN (PROSLIC_REG_OCON_OSC1_TI_EN|PROSLIC_REG_OCON_OSC2_TI_EN)

#define PROSLIC_REG_PCMTXHI_TX_START (0x03<<0)
#define PROSLIC_REG_PCMRXHI_TX_START (0x03<<0)
#define PROSLIC_REG_PCMRXHI_TX_EDGE (1<<4)

#define PROSLIC_REG_TONDTMF_VALID (1<<5)
#define PROSLIC_REG_TONDTMF_VALID_TONE (1<<4)


#define PROSLIC_REG_DIGCON_DRX_MUTE (1<<0)
#define PROSLIC_REG_DIGCON_DTX_MUTE (1<<1)


#define PBX_TIMESLOTS(TS) (PCM_PRESET_NUM_8BIT_TS*(TS)*8)

#define PROSLIC_REG_TONEN 62
#define PROSLIC_REG_TONEN_DTMF_PASS (0x03<<4)
#define PROSLIC_REG_TONEN_DTMF_DIS (1<<2)
#define PROSLIC_REG_TONEN_RXMDM_DIS (1<<1)
#define PROSLIC_REG_TONEN_TXMDM_DIS (1<<0)

#define PROSLIC_REG_FSKDEPTH_FSK_FLUSH (1<<3)
#define PROSLIC_REG_FSKDEPTH_FSKBUF_DEPTH (0x07)

#define PROSLIC_REG_RAMSTAT 4

#define PROSLIC_RAM_PD_CADC     1539

#define PROSLIC_REG_MSTRSTAT_RUNNING 0x1F

#define PROSLIC_REG_LINEFEED_STAT_MASK (0x07<<4)


int netcomm_proslic_SPI_Init(ctrl_S *interfacePtr);
void netcomm_TimerInit(systemTimer_S *pTimerObj);
void netcomm_initControlInterfaces(SiVoiceControlInterfaceType *ProHWIntf, void *spiIf, void *timerObj);

int netcomm_proslic_sendcid(proslicChanType_ptr pProslic, uInt8 *buffer, uInt8 numBytes, uint64_t* timestamp);
int netcomm_proslic_enable_cid(SiVoiceChanType_ptr cptr, int depth);
int netcomm_proslic_disable_cid(SiVoiceChanType_ptr cptr);
int netcomm_proslic_set_linefeed(SiVoiceChanType_ptr cptr, uInt8 linefeed);
void netcomm_proslic_reg_set(SiVoiceChanType_ptr cptr, uInt8 addr, uInt8 val);
void netcomm_proslic_reg_set_with_mask(SiVoiceChanType_ptr cptr, uInt8 addr, uInt8 set, uInt8 mask);
void netcomm_proslic_reg_set_and_reset(SiVoiceChanType_ptr cptr, uInt8 addr, uInt8 set, uInt8 reset);
int netcomm_proslic_dtmf_read_digit(proslicChanType_ptr cptr, uInt8 *pDigit);
uInt32 netcomm_proslic_ram_get(SiVoiceChanType_ptr cptr, uInt16 addr);
uInt32 netcomm_proslic_ram_set(SiVoiceChanType_ptr cptr, uInt16 addr, ramData val);
void netcomm_proslic_enable_dtmf_detection(SiVoiceChanType_ptr cptr, int en);

void netcomm_proslic_mute_tx(SiVoiceChanType_ptr cptr, int mute);
void netcomm_proslic_mute_rx(SiVoiceChanType_ptr cptr, int mute);

int netcomm_proslic_init(void);
void netcomm_proslic_fini(void);

struct fxs_port_t* pbx_get_port(int chan_idx);
SiVoiceChanType_ptr pbx_get_cptr(int chan_idx);

extern struct fxs_info_t* _fxs_info;

#endif
