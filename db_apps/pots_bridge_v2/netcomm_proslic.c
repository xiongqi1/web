/*
 * netcomm_proslic module provides interface between SiLab proslic and Linux driver
 *
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

#include "netcomm_proslic.h"
#include "utils.h"
#include "pbx_common.h"
#include "pbx_config.h"
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>

/* Basic register definitions, regardless of chipset ('4x, '2x, '17x, '26x compatible */
#define PROSLIC_CW_RD    0x60
#define PROSLIC_CW_WR    0x20
#define PROSLIC_CW_BCAST 0x80
#define PROSLIC_BCAST    0xFF

/* chanNumtoCID() implemented as a macro instead of a function */
#define CNUM_TO_CID(CHAN) ( (( (CHAN)<<4)&0x10) \
                            | (( (CHAN)<<2) & 0x8) \
                            | (( (CHAN)>>2) & 0x2) \
                            | (( (CHAN)>>4) & 0x1) \
                            | ( (CHAN) & 0x4) )
#define RAM_STAT_REG          4
#define RAM_ADDR_HI           5
#define RAM_DATA_B0           6
#define RAM_DATA_B1           7
#define RAM_DATA_B2           8
#define RAM_DATA_B3           9
#define RAM_ADDR_LO           10
#define RAM_ADR_HIGH_MASK(ADDR) (((ADDR)>>3)&0xE0)

#ifdef DEBUG
#undef SPI_TRC
#define SPI_TRC(...) LOGPRINT(__VA_ARGS__)
#endif

static int spi_fd = -1;
static int reset_fd = -1;

ctrl_S spiGciObj; /* Link to host spi obj (os specific) */
systemTimer_S timerObj; /* Link to host timer obj (os specific)*/
controlInterfaceType ProHWIntf; /* proslic hardware interface object */

struct fxs_info_t _fxs_info_obj;
struct fxs_info_t* _fxs_info = &_fxs_info_obj;


/**
 * @brief initializes SPIDEV and GPIO drivers.
 *
 * @param interfacePtr is ProSLIC channel control interface.
 *
 * @return TRUE when it succeeds. Otherwise, FALSE.
 */
int netcomm_proslic_SPI_Init(ctrl_S *interfacePtr)
{

    uint8_t bits = SILABS_BITS_PER_WORD;
    uint32_t mode =  SPI_MODE_3;
    uint32_t speed = SILABS_SPI_RATE;
    reset_fd = spi_fd = -1;

    if ((spi_fd =  open(SILABS_SPIDEV, O_RDWR)) < 0) {
        perror("Failed to open SPI device");
        abort();
    }

    #ifdef LINUX_GPIO
    if ((reset_fd = open(LINUX_GPIO, O_WRONLY)) < 0) {
        perror("Failed to open GPIO");
        close(spi_fd);
        abort();
    }
    #endif

    /* TODO: hook up IOC_XFER to menu */
    #ifdef SILABS_USE_IOC_XFER
    SPI_TRC("DBG: %s: using IOC transfers with bytelen = %d\n",
            __FUNCTION__, SILABS_BYTE_LEN);
    xfer.len = SILABS_BYTE_LEN;
    xfer.speed_hz = speed;
    xfer.delay_usecs = 0;
    xfer.bits_per_word = SILABS_BITS_PER_WORD;
    xfer.cs_change = 1; /* Must deassert CS for next transfer */
    #else
    SPI_TRC("DBG: %s: using read/write transfers with bytelen = %d\n",
            __FUNCTION__, SILABS_BYTE_LEN);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    #endif
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);

    interfacePtr->spi_fd = spi_fd;
    interfacePtr->reset_fd = reset_fd;
    return TRUE;
}


/**
 * @brief initializes timer.
 *
 * @param pTimerObj is ProSLIC timer object.
 */
void netcomm_TimerInit(systemTimer_S *pTimerObj)
{
    SILABS_UNREFERENCED_PARAMETER(pTimerObj);

}

/**
 * @brief resets SLIC.
 *
 * @param interfacePtr is ProSLIC channel interface.
 * @param inReset is reset flag.
 *
 * @return RC_NONE when it succeeds. Otherwise, RC_SPI_FAIL.
 */
int ctrl_ResetWrapper(void *interfacePtr, int inReset)
{
    char buf;
    SILABS_UNREFERENCED_PARAMETER(interfacePtr);

    SPI_TRC("DBG: %s: inReset = %d\n", __FUNCTION__, inReset);
    if (reset_fd >= 0) {
        if (inReset) {
            buf = '0';
        } else {
            buf = '1';
        }

        if (write(reset_fd,  &buf, 1) == 1) {
            return RC_NONE;
        }
    }
    return RC_SPI_FAIL;
}


/**
 * @brief writes a value to SLIC register.
 *
 * @param interfacePtr is ProSLIC channel interface.
 * @param channel is ProSLIC channel interfance.
 * @param regAddr is SLIC register address.
 * @param regData is data to write to SLIC register.
 *
 * @return RC_NONE when it succeeds. Otherwise, RC_SPI_FAIL.
 */
int ctrl_WriteRegisterWrapper(void *interfacePtr, uInt8 channel,
                              uInt8 regAddr, uInt8 regData)
{
    uint8_t controlWord;
    SILABS_UNREFERENCED_PARAMETER(interfacePtr);

    if (channel == PROSLIC_BCAST) {
        controlWord = PROSLIC_CW_BCAST;
    } else {
        controlWord = CNUM_TO_CID(channel);
    }

    controlWord |= PROSLIC_CW_WR;
    SPI_TRC("DBG: %s: cw = 0x%02X ra = %d data = 0x%02X\n", __FUNCTION__,
            controlWord, regAddr, regData);

    #ifdef SILABS_USE_IOC_XFER
    xfer.rx_buf = NULL;
    xfer.tx_buf = &wrbuf;
    #if (SILABS_BYTE_LEN == 1)
    wrbuf = controlWord;
    /* NOTE: in theory, we could of done 3 SPI_IOC_MESSAGES as 1 call... */
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer);
    wrbuf = regAddr;
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer);
    wrbuf = regData;
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer) >= 0) {
        return RC_NONE;
    } else {
        return RC_SPI_FAIL;
    }
    #else /* 2 or 4 byte transfer */
    xfer.len = SILABS_BYTE_LEN;
    /* On system tested, we had to do a byte swap */
    wrbuf[0] = regAddr;
    wrbuf[1] = controlWord;

    #if (SILABS_BYTE_LEN == 2)
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer);
    wrbuf[0] = wrbuf[1] = regData;
    #else /* 4 byte  */
    wrbuf[2] = wrbuf[3] = regData;
    #endif /* 4 byte */

    #endif /*  2 or 4*/
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer) >= 0) {
        return RC_NONE;
    } else {
        return RC_SPI_FAIL;
    }

    #else /* Non-IOCTL transfer */

    #if (SILABS_BYTE_LEN == 1)
    /* Send 1 byte at a time since we need a CS deassert between bytes */
    write(spi_fd, &controlWord, 1);
    write(spi_fd, &regAddr, 1);

    /* In theory, we should check all access... */
    if (write(spi_fd, &regData, 1) == 1)
    #else
    #if (SILABS_BYTE_LEN == 4)
    xbuf[3] = controlWord;
    xbuf[2] = regAddr;
    xbuf[0] = xbuf[1] = regData;

    /* Need to adjust bits per word if we do a write vs. a read */
    controlWord = SILABS_BITS_PER_WORD;
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &controlWord);
    #else
    xbuf[0] = regAddr;
    xbuf[1] = controlWord;

    write(spi_fd, xbuf, 2);
    xbuf[0] = xbuf[1] = regData;
    #endif

    if (write(spi_fd, xbuf, SILABS_BYTE_LEN) == SILABS_BYTE_LEN)
    #endif /* multibyte transfer */
    {
        return RC_NONE;
    } else {
        perror("SPI WR Failed");
        return RC_SPI_FAIL;
    }
    #endif /* read/write transfer vs. IOC. */
}

/**
 * @brief reads a value from SLIC register.
 *
 * @param interfacePtr is ProSLIC channel interfance.
 * @param channel is channel number.
 * @param regAddr is SLIC register address.
 *
 * @return value that is read from SLIC register.
 */
unsigned char ctrl_ReadRegisterWrapper(void *interfacePtr,
                                       uInt8 channel, uInt8 regAddr)
{
    uint8_t controlWord;
    uint8_t data;
    #ifdef SILABS_USE_IOC_XFER
    int rc;
    #endif

    SILABS_UNREFERENCED_PARAMETER(interfacePtr);

    controlWord = CNUM_TO_CID(channel);

    controlWord |= PROSLIC_CW_RD;
    SPI_TRC("DBG: %s: cw = 0x%02x ra = %d\n", __FUNCTION__, controlWord, regAddr);

    #ifdef SILABS_USE_IOC_XFER
    xfer.rx_buf = NULL;
    #if (SILABS_BYTE_LEN == 1)
    xfer.tx_buf = &wrbuf;
    rdbuf = 0xFF;
    wrbuf = controlWord;
    #else
    xfer.tx_buf = wrbuf;
    *rdbuf = 0xFF;
    wrbuf[1] =
        controlWord; /* On the tested system, we had to send the bytes in reverse order */
    wrbuf[0] = regAddr;
    xfer.len = 2;
    xfer.bits_per_word = 16;
    #endif
    rc = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer);
    if (rc < 0) {
        perror("SPI RD");
        return RC_SPI_FAIL;
    }

    #if (SILABS_BYTE_LEN == 1)
    wrbuf = regAddr;
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer);
    xfer.rx_buf = &rdbuf;
    #else
    xfer.rx_buf = rdbuf;
    #endif
    xfer.tx_buf = NULL;
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer) >= 0) {
        #if (SILABS_BYTE_LEN == 1)
        SPI_TRC("DBG: %s: cw = 0x%02x ra = %d data = 0x%02X\n", __FUNCTION__,
                controlWord, regAddr, rdbuf);
        return rdbuf;
        #else
        SPI_TRC("DBG: %s: cw = 0x%02x ra = %d data = 0x%02X 0x%02X\n", __FUNCTION__,
                controlWord, regAddr, *rdbuf, rdbuf[1]);
        return *rdbuf;
        #endif
    } else {
        perror("SPI RD");
        return RC_SPI_FAIL;
    }
    #else /* Regular read/write */
    #if (SILABS_BYTE_LEN == 1)
    /* Send 1 byte at a time since we need a CS deassert between bytes */

    write(spi_fd, &controlWord, 1);
    write(spi_fd, &regAddr, 1);
    data = 0xFF;

    /* In theory, we should check all access... */
    if (read(spi_fd, &data, 1) == 1) {
        SPI_TRC("DBG: %s: cw = %02x ra = %d data = 0x%02X\n", __FUNCTION__,
                controlWord, regAddr, data);
        return data;
    }
    #else /* Multibyte */
    xbuf[0] = regAddr;
    xbuf[1] = controlWord;

    /* Need to adjust bits per word if we do a write vs. a read */
    controlWord = 16;
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &controlWord);

    write(spi_fd, xbuf, 2);
    *xbuf = 0xFF;
    if (read(spi_fd, xbuf, 2) == 2) {
        SPI_TRC("DBG: %s: cw = %02x ra = %d data = 0x%02X\n", __FUNCTION__,
                controlWord, regAddr, *xbuf);
        return *xbuf;
    }
    #endif
    else {
        perror("SPI RD1 Failed");
        return 0xFF; /* RC_SPI_FAIL is the alternative return code */
    }
    #endif
}

/**
 * @brief waits until RAM is accessible.
 *
 * @param interfacePtr is ProSLIC channel interface.
 * @param channel is channel number.
 *
 * @return RC_NONE when it succeeds. Otherwise, RC_SPI_FAIL.
 */
static int wait_ram(void *interfacePtr, unsigned char channel)
{
    uint32_t timeout = PROSLIC_MAX_RAM_WAIT;

    while ((ctrl_ReadRegisterWrapper(interfacePtr, channel, RAM_STAT_REG) & 0x01)
           && timeout) {
        timeout--;
    }

    if (timeout) {
        return RC_NONE;
    } else {
        return RC_SPI_FAIL;
    }
}

/**
 * @brief writes a value to SLIC RAM.
 *
 * @param interfacePtr is ProSLIC channel interface.
 * @param channel is channel number.
 * @param ramAddr is SLIC RAM address.
 * @param data is data to write.
 *
 * @return RC_NONE when it succeeds. Otherwise, RC_SPI_FAIL.
 */
int ctrl_WriteRAMWrapper(void *interfacePtr, uInt8 channel,
                         uInt16 ramAddr, ramData data)
{
    ramData myData = data;
    if (wait_ram(interfacePtr, channel) != RC_NONE) {
        return RC_SPI_FAIL;
    }

    SPI_TRC("DBG: %s: ramloc = %0d data = 0x%04X\n", __FUNCTION__,
            ramAddr, myData);
    #ifdef SILABS_RAM_BLOCK_ACCESS
    {
        uInt8 ramWriteData[6 * 4]; /* This encapsulates the 6 reg writes into 1 block */
        const uInt8 regAddr[6] = {RAM_ADDR_HI, RAM_DATA_B0, RAM_DATA_B1, RAM_DATA_B2, RAM_DATA_B3, RAM_ADDR_LO};
        int i;
        uInt8 scratch;

        /* Setup control word & registers for ALL the reg access */
        scratch = CNUM_TO_CID(channel) | PROSLIC_CW_WR;

        for (i = 0; i < 6; i++) {
            ramWriteData[i << 2]     = regAddr[i];
            ramWriteData[(i << 2) + 1] = scratch
                                         ; /* On system tested, we had to do a swap of CW + Reg addr */
        }

        ramWriteData[2] = ramWriteData[3] = RAM_ADR_HIGH_MASK(ramAddr);

        ramWriteData[6] = ramWriteData[7] = (uInt8)(myData << 3);
        myData = myData >> 5;

        ramWriteData[10] = ramWriteData[11] = (uInt8)(myData & 0xFF);
        myData = myData >> 8;

        ramWriteData[14] = ramWriteData[15] = (uInt8)(myData & 0xFF);
        myData = myData >> 8;

        ramWriteData[18] = ramWriteData[19] = (uInt8)(myData & 0xFF);

        ramWriteData[22] = ramWriteData[23] = (uInt8)(ramAddr & 0xFF);

        if (write(spi_fd, ramWriteData, 24) == 24) {
            return RC_NONE;
        } else {
            perror("SPI WR Failed");
            return RC_SPI_FAIL;
        }
    }
    #else
    ctrl_WriteRegisterWrapper(interfacePtr, channel, RAM_ADDR_HI,
                              RAM_ADR_HIGH_MASK(ramAddr));

    ctrl_WriteRegisterWrapper(interfacePtr, channel, RAM_DATA_B0,
                              ((unsigned char)(myData << 3)));

    myData = myData >> 5;

    ctrl_WriteRegisterWrapper(interfacePtr, channel, RAM_DATA_B1,
                              ((unsigned char)(myData & 0xFF)));

    myData = myData >> 8;

    ctrl_WriteRegisterWrapper(interfacePtr, channel, RAM_DATA_B2,
                              ((unsigned char)(myData & 0xFF)));

    myData = myData >> 8;

    ctrl_WriteRegisterWrapper(interfacePtr, channel, RAM_DATA_B3,
                              ((unsigned char)(myData & 0xFF)));

    return (ctrl_WriteRegisterWrapper(interfacePtr, channel, RAM_ADDR_LO,
                                      (unsigned char)(ramAddr & 0xFF)));
    #endif
}

/**
 * @brief reads a value from SLIC RAM.
 *
 * @param interfacePtr is ProSLIC channel interface.
 * @param channel is channel number.
 * @param ramAddr is SLIC RAM address.
 *
 * @return value is that read from SLIC RAM.
 */
ramData ctrl_ReadRAMWrapper(void *interfacePtr,
                            uInt8 channel,
                            uInt16 ramAddr)

{
    ramData data;

    if (wait_ram(interfacePtr, channel) != RC_NONE) {
        return RC_SPI_FAIL;
    }

    ctrl_WriteRegisterWrapper(interfacePtr, channel, RAM_ADDR_HI,
                              RAM_ADR_HIGH_MASK(ramAddr));

    ctrl_WriteRegisterWrapper(interfacePtr, channel, RAM_ADDR_LO,
                              (unsigned char)(ramAddr & 0xFF));

    if (wait_ram(interfacePtr, channel) != RC_NONE) {
        return RC_SPI_FAIL;
    }

    data = ctrl_ReadRegisterWrapper(interfacePtr, channel, RAM_DATA_B3);
    data = data << 8;
    data |= ctrl_ReadRegisterWrapper(interfacePtr, channel, RAM_DATA_B2);
    data = data << 8;
    data |= ctrl_ReadRegisterWrapper(interfacePtr, channel, RAM_DATA_B1);
    data = data << 8;
    data |= ctrl_ReadRegisterWrapper(interfacePtr, channel, RAM_DATA_B0);

    data = data >> 3;

    SPI_TRC("DBG: %s: ramloc = %0d data = 0x%04X\n", __FUNCTION__,
            ramAddr, data);
    return data;
}

/**
 * @brief delays a certain time.
 *
 * @param hTimer is ProSLIC timer object.
 * @param timeInMs is delay in msec.
 *
 * @return RC_NONE when it succeeds. Otherwise, RC_SPI_FAIL.
 */
int time_DelayWrapper(void *hTimer, int timeInMs)
{
    SILABS_UNREFERENCED_PARAMETER(hTimer);

    #ifdef SILABS_USE_USLEEP
    usleep(timeInMs * 1000);
    #else
    struct timespec myDelay;
    if (timeInMs >= 1000) {
        myDelay.tv_sec = timeInMs / 1000;
    } else {
        myDelay.tv_sec = 0;
    }

    myDelay.tv_nsec = (timeInMs - (myDelay.tv_sec * 1000)) * 1000000UL;

    nanosleep(&myDelay, NULL);
    #endif
    return RC_NONE;
}

/**
 * @brief gets elapsed time.
 *
 * @param hTimer is ProSLIC timer object.
 * @param startTime is start time.
 * @param timeInMs is time in msec as result.
 *
 * @return RC_NONE when it succeeds. Otherwise, RC_SPI_FAIL.
 */
int time_TimeElapsedWrapper(void *hTimer, void *startTime, int *timeInMs)
{
    SILABS_UNREFERENCED_PARAMETER(hTimer);

    if ((startTime != NULL) && (timeInMs != NULL)) {
        #ifdef SILABS_USE_TIMEVAL
        struct timeval timeNow;
        struct timeval result;
        gettimeofday(&timeNow, NULL);

        timersub(&(timeNow), &(((timeStamp *)startTime)->timerObj), &result);

        *timeInMs = (result.tv_usec / 1000) + (result.tv_sec * 1000);
        #else
        struct timespec timeNow;
        struct timespec result;
        clock_gettime(SILABS_CLOCK, &timeNow);

        if ((timeNow.tv_nsec - ((timeStamp *)(startTime))->timerObj.tv_nsec) < 0) {
            result.tv_sec = timeNow.tv_sec - 1
                            - ((timeStamp *)(startTime))->timerObj.tv_sec;
            result.tv_nsec = 1000000000UL + timeNow.tv_nsec
                             - ((timeStamp *)(startTime))->timerObj.tv_nsec;
        } else {
            result.tv_sec = timeNow.tv_sec -
                            ((timeStamp *)(startTime))->timerObj.tv_sec;
            result.tv_nsec = timeNow.tv_nsec -
                             ((timeStamp *)(startTime))->timerObj.tv_nsec;
        }
        *timeInMs = (result.tv_nsec / 1000000L) + (result.tv_sec * 1000);
        #endif
    }

    return RC_NONE;
}

/**
 * @brief gets current time.
 *
 * @param hTimer is ProSLIC time object.
 * @param time is current time as result.
 *
 * @return always 0.
 */
int time_GetTimeWrapper(void *hTimer, void *time)
{
    SILABS_UNREFERENCED_PARAMETER(hTimer);
    if (time != NULL) {
        #ifdef SILABS_USE_TIMEVAL
        gettimeofday(&(((timeStamp *)time)->timerObj), NULL);
        #else
        clock_gettime(SILABS_CLOCK, &(((timeStamp *)time)->timerObj));
        #endif
    }
    return 0;
}

/**
 * @brief gets maximum SPI bus speed.
 *
 * @return maximum SPI speed.
 */
uint32_t get_max_spi_speed(void)
{
    static uint32_t max_speed = 0;

    if (max_speed == 0) {
        if (ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &max_speed) == 0) {
            if (max_speed >= SILABS_MAX_SPI_SPEED) {
                max_speed =
                    SILABS_MAX_SPI_SPEED; /* Clamp it to a "safe" limit, your particular chipset may actually run faster than this... */
            }
        } else {
            max_speed = SILABS_MAX_SPI_SPEED;
        }
    }

    return max_speed;
}


/**
 * @brief sets SPI bus speed.
 *
 * @param speed is SPI bus speed to set.
 */
void set_spi_speed(uint32_t speed)
{
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
}

/**
 * @brief configures ProSLIC voice control interface.
 *
 * @param ProHWIntf is ProSLIC voice control interface.
 */
void vmbSetup(SiVoiceControlInterfaceType *ProHWIntf)
{
    uint32_t max_spi_speed = 2000000; /* Hz */

    max_spi_speed = get_max_spi_speed();
    set_spi_speed(max_spi_speed);
}


/**
 * @brief resets SLIC.
 *
 * @param ctrlInterface is ProSLIC voice control interface.
 */
static void vmbReset(SiVoiceControlInterfaceType *ctrlInterface)
{
    /* Setup code keeps part in reset at prompt.... */
    ctrlInterface->Reset_fptr(ctrlInterface->hCtrl, 0);

    /* For ISI, we needed to extend the delay to access the part -
       somewhere between 1 - 2 seconds is needed.
       Normally this delay can be shorter. */
    ctrlInterface->Delay_fptr(ctrlInterface->hTimer, 1500);
}


/**
 * @brief initializes ProSLIC SDK.
 *
 * @param ProHWIntf is ProSLIC voice control interface.
 * @param spiIf is ProSLIC SPI bus interface.
 * @param timerObj is ProSLIC timer object.
 */
void netcomm_initControlInterfaces(SiVoiceControlInterfaceType *ProHWIntf, void *spiIf, void *timerObj)
{

    SiVoice_setControlInterfaceCtrlObj(ProHWIntf, spiIf);
    SiVoice_setControlInterfaceTimerObj(ProHWIntf, timerObj);
    SiVoice_setControlInterfaceSemaphore(ProHWIntf, NULL);

    SiVoice_setControlInterfaceReset(ProHWIntf, ctrl_ResetWrapper);

    SiVoice_setControlInterfaceWriteRegister(ProHWIntf, ctrl_WriteRegisterWrapper);
    SiVoice_setControlInterfaceReadRegister(ProHWIntf, ctrl_ReadRegisterWrapper);
    SiVoice_setControlInterfaceWriteRAM(ProHWIntf, ctrl_WriteRAMWrapper);
    SiVoice_setControlInterfaceReadRAM(ProHWIntf, ctrl_ReadRAMWrapper);

    SiVoice_setControlInterfaceDelay(ProHWIntf, time_DelayWrapper);
    SiVoice_setControlInterfaceTimeElapsed(ProHWIntf, time_TimeElapsedWrapper);
    SiVoice_setControlInterfaceGetTime(ProHWIntf, time_GetTimeWrapper);

    ctrl_ResetWrapper(spiIf,
                      1); /* Since we may be changing clocks in vmbSetup() , keep the part in reset */

    /*
      	** Configuration of SPI and PCM internal/external
    */
    vmbSetup(ProHWIntf);

    /*
     ** Call host/system reset function (omit if ProSLIC tied to global reset)
     */

    vmbReset(ProHWIntf);
}


///////////////////////////////////////////////////////////////////////////////
// slic functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief enables Caller ID.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param depth is threshold byte. FSK interrupt occurs when FSK FIFO is below this threshold byte.
 *
 * @return RC_NONE when it succeeds. Otherwise, ProSLIC RC error code.
 */
int netcomm_proslic_enable_cid(SiVoiceChanType_ptr cptr, int depth)
{
    syslog(LOG_DEBUG, "[SLIC %llu ms] enable CID", get_time_elapsed_msec());

    /* set FSK depth */
    netcomm_proslic_reg_set_with_mask(cptr, PROSLIC_REG_FSKDEPTH, depth, PROSLIC_REG_FSKDEPTH_FSKBUF_DEPTH);

    return ProSLIC_EnableCID(cptr);
}

/**
 * @brief dibbles Caller ID.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 *
 * @return RC_NONE when it succeeds. Otherwise, ProSLIC RC error code.
 */
int netcomm_proslic_disable_cid(SiVoiceChanType_ptr cptr)
{
    syslog(LOG_DEBUG, "[SLIC %llu ms] disable CID", get_time_elapsed_msec());

    /* reset cid buffer */
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_FSKDEPTH, PROSLIC_REG_FSKDEPTH_FSK_FLUSH, 0);
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_FSKDEPTH, 0, PROSLIC_REG_FSKDEPTH_FSK_FLUSH);

    return ProSLIC_DisableCID(cptr);
}

/**
 * @brief sets SLIC linefeed.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param linefeed is linefeed to set.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int netcomm_proslic_set_linefeed(SiVoiceChanType_ptr cptr, uInt8 linefeed)
{
    const char* line_feed_names[] = {
        [LF_OPEN] = "LF_OPEN",
        [LF_FWD_ACTIVE] = "LF_FWD_ACTIVE",
        [LF_FWD_OHT] = "LF_FWD_OHT",
        [LF_TIP_OPEN] = "LF_TIP_OPEN",
        [LF_RINGING] = "LF_RINGING",
        [LF_REV_ACTIVE] = "LF_REV_ACTIVE",
        [LF_REV_OHT] = "LF_REV_OHT",
        [LF_RING_OPEN] = "LF_RING_OPEN",
    };

    if (linefeed >= __countof(line_feed_names)) {
        syslog(LOG_ERR, "[SLIC] invalid line feed specified (linefeed=%d)", linefeed);
        goto err;
    }

    syslog(LOG_DEBUG, "[SLIC %llu ms] set linefeed to [%s#%d]", get_time_elapsed_msec(), line_feed_names[linefeed],
           linefeed);

    return ProSLIC_SetLinefeedStatus(cptr, linefeed);

err:
    return -1;
}

/**
 * @brief sets ProSLIC register.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param addr is ProSLIC register address.
 * @param val is value to set.
 */
void netcomm_proslic_reg_set(SiVoiceChanType_ptr cptr, uInt8 addr, uInt8 val)
{
    syslog(LOG_DEBUG, "[SLIC-REG] set reg, [val 0x%02x] ==> [addr %d]", val, addr);

    SiVoice_WriteReg(cptr, addr, val);
}

/**
 * @brief send CID to SLIC
 *
 *
 * @param pProslic is ProSLIC voice channel type object pointer.
 * @param buffer CID data to send
 * @param numBytes CID data length.
 * @param timestamp if timestamp is not NULL, time-stamp is written to this pointer after finishing to push CID to Kernel driver.
 *
 * @return RC_NONE when it succeeds.
 */
int netcomm_proslic_sendcid(proslicChanType_ptr pProslic, uInt8 *buffer, uInt8 numBytes, uint64_t* timestamp)
{
    int stat;

    syslog(LOG_DEBUG, "[SLIC %llu ms] send CID (len=%d)", get_time_elapsed_msec(), numBytes);

    log_dump_hex("FSK data block", buffer, numBytes);

    stat = ProSLIC_SendCID(pProslic, buffer, numBytes);

    if (timestamp) {
        *timestamp = get_monotonic_msec();
    }

    return stat;
}

/**
 * @brief sets ProSLIC register after masking value with mask.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param addr is ProSLIC register address.
 * @param set is value to set.
 * @param mask is value mask.
 */
void netcomm_proslic_reg_set_with_mask(SiVoiceChanType_ptr cptr, uInt8 addr, uInt8 set, uInt8 mask)
{
    uInt8 reg;
    uInt8 val;

    /* setup continuous ring */
    reg = SiVoice_ReadReg(cptr, addr);
    val = (reg & ~mask) | (set & mask);

    syslog(LOG_DEBUG, "[SLIC-REG] set with mask, [val 0x%02x] ==> [addr %d] (old=0x%02x,set=0x%02x,mask=0x%02x) ", val,
           addr, reg, set, mask);

    SiVoice_WriteReg(cptr, addr, val);
}

/**
 * @brief enables or disables ProSLIC DTMF detection.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param en is to enable or disable (0=disable,1=enable)
 */
void netcomm_proslic_enable_dtmf_detection(SiVoiceChanType_ptr cptr, int en)
{
    if (en) {
        syslog(LOG_DEBUG, "[DOWNLINK-DTMF] enable DTMF detection");
    } else {
        syslog(LOG_DEBUG, "[DOWNLINK-DTMF] disable DTMF detection");
    }

    netcomm_proslic_reg_set_with_mask(cptr, PROSLIC_REG_TONEN, en ? 0x00 : PROSLIC_REG_TONEN_DTMF_DIS,
                                      PROSLIC_REG_TONEN_DTMF_DIS);
}

/**
 * @brief read ProSLIC RAM
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param addr is ProSLIC register address.
* @return RAM value.
  */
uInt32 netcomm_proslic_ram_get(SiVoiceChanType_ptr cptr, uInt16 addr)
{
    ctrl_ReadRAM_fptr ReadRAM = cptr->deviceId->ctrlInterface->ReadRAM_fptr;
    void* pProHW = cptr->deviceId->ctrlInterface->hCtrl;

    return ReadRAM(pProHW, cptr->channel, addr);
}

/**
 * @brief write ProSLIC RAM
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param addr is ProSLIC register address.
* @return RAM value.
  */
uInt32 netcomm_proslic_ram_set(SiVoiceChanType_ptr cptr, uInt16 addr, ramData val)
{
    ctrl_WriteRAM_fptr WriteRAM = cptr->deviceId->ctrlInterface->WriteRAM_fptr;
    void* pProHW = cptr->deviceId->ctrlInterface->hCtrl;

    return WriteRAM(pProHW, cptr->channel, addr, val);
}

/**
 * @brief sets and resets ProSLIC register.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param addr is ProSLIC register address.
 * @param set is value to set.
 * @param reset is value to reset.
 */
void netcomm_proslic_reg_set_and_reset(SiVoiceChanType_ptr cptr, uInt8 addr, uInt8 set, uInt8 reset)
{
    uInt8 reg;
    uInt8 val;

    /* setup continuous ring */
    reg = SiVoice_ReadReg(cptr, addr);
    val = (reg | set) & ~reset;

    syslog(LOG_DEBUG, "[SLIC-REG] set and reset, [val 0x%02x] ==> [addr %d] (old=0x%02x,set=0x%02x,reset=0x%02x) ", val,
           addr, reg, set, reset);

    SiVoice_WriteReg(cptr, addr, val);
}

/**
 * @brief mute or unmute TX.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param mute is a flag to mute or unmute (1:mute, 0:unmute)
 */
void netcomm_proslic_mute_tx(SiVoiceChanType_ptr cptr, int mute)
{
    if (mute) {
        syslog(LOG_DEBUG, "[SLIC-REG] mute tx");
        netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_DIGCON, PROSLIC_REG_DIGCON_DTX_MUTE, 0);
    } else {
        syslog(LOG_DEBUG, "[SLIC-REG] unmute tx");
        netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_DIGCON, 0, PROSLIC_REG_DIGCON_DTX_MUTE);
    }
}

/**
 * @brief mute or unmute RX.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param mute is a flag to mute or unmute (1:mute, 0:unmute)
 */
void netcomm_proslic_mute_rx(SiVoiceChanType_ptr cptr, int mute)
{
    if (mute) {
        syslog(LOG_DEBUG, "[SLIC-REG] mute rx");
        netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_DIGCON, PROSLIC_REG_DIGCON_DRX_MUTE, 0);
    } else {
        syslog(LOG_DEBUG, "[SLIC-REG] unmute rx");
        netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_DIGCON, 0, PROSLIC_REG_DIGCON_DRX_MUTE);
    }
}

/**
 * @brief reads DTMF digit.
 *
 * @param cptr is ProSLIC voice channel type object pointer.
 * @param pDigit is a pointer to digit as result.
 *
 * @return RC_NONE when it succeeds or RC_IGNORE if the ProSLIC voice channel driver is
 */
int netcomm_proslic_dtmf_read_digit(proslicChanType_ptr cptr, uInt8 *pDigit)
{
    if (cptr->channelType != PROSLIC)  {
        return RC_IGNORE;
    }

    *pDigit = SiVoice_ReadReg(cptr, PROSLIC_REG_TONDTMF);

    return RC_NONE;
}

/**
 * @brief set channel state.
 *
 * @param port is FXS port object.
 *
 * @return always 0.
 */
static int pbx_set_chan_state(struct fxs_port_t *port)
{
    int i;

    SiVoiceChanType_ptr chanPtr;

    for (i = 0; i < port->numberOfChan; i++) {
        chanPtr = port->cptrs[i];

        netcomm_proslic_set_linefeed(chanPtr, LF_FWD_ACTIVE);
        ProSLIC_EnableInterrupts(chanPtr);

    }
    return 0;
}


/**
 * @brief gets FXS port by channel index.
 *
 * @param chan_idx is channel index.
 *
 * @return FXS port.
 */
struct fxs_port_t* pbx_get_port(int chan_idx)
{
    int i;

    if (chan_idx >= _fxs_info->num_of_channels) {
        return NULL;
    }

    /* Determine which port we're on */
    for (i = 0; chan_idx < _fxs_info->ports[i].chan_base_idx; i++) {
    }

    return &_fxs_info->ports[i];
}

/**
 * @brief gets SLIC voice channel type pointer by channel index.
 *
 * @param chan_idx is channel index.
 *
 * @return SLIC voice channel type pointer.
 */
SiVoiceChanType_ptr pbx_get_cptr(int chan_idx)
{
    struct fxs_port_t* port = pbx_get_port(chan_idx);

    return port->cptrs[(chan_idx - port->chan_base_idx)];
}


int netcomm_proslic_init(void)
{
    int rc = -1;
    int i;

    ///////////////////////////////////////////////////////////////////////////////
    // initiate ProSLIC SDK
    ///////////////////////////////////////////////////////////////////////////////

    /* initiate host SPI */
    syslog(LOG_DEBUG, "initiate SPIDEV");
    if (!netcomm_proslic_SPI_Init(&spiGciObj)) {
        syslog(LOG_ERR, "cannot connect to SPIDEV");
        goto fini;
    }

    /* initiate system timer */
    syslog(LOG_DEBUG, "initiate system timer");
    netcomm_TimerInit(&timerObj);

    /* link function pointers */
    syslog(LOG_DEBUG, "link function pointers");
    netcomm_initControlInterfaces(&ProHWIntf, &spiGciObj, &timerObj);

    ///////////////////////////////////////////////////////////////////////////////
    // initiate ports
    ///////////////////////////////////////////////////////////////////////////////

    syslog(LOG_DEBUG, "allocate memory for ports");
    _fxs_info->ports = SIVOICE_CALLOC(sizeof(struct fxs_port_t), PBX_PORT_COUNT);
    if (!_fxs_info->ports) {
        syslog(LOG_ERR, "failed to allocate memory");
        goto fini;
    }

    _fxs_info->num_of_channels = 0;

    /*
     ** This demo supports single device/BOM option only - for now...
     */
    for (i = 0; i < PBX_PORT_COUNT; i++) {
        pbx_init_port_info(&_fxs_info->ports[i], i);

        /* We assume in our sivoice_init_port_info that we have a 178/179 - which
         is a SLIC + VDAA device, 2 ports, if the VDAA support is disabled,
         stop here and not allocate/init the 2nd port.

         NOTE: for the CID demo, we never have VDAA support enabled since we can't
         decode it!
         */
        #if defined(SI3217X)
        /* Subtract from #chan the device count since we doubled it */
        _fxs_info->ports[i].numberOfChan -= _fxs_info->ports[i].numberOfDevice;
        _fxs_info->ports[i].chanPerDevice--;
        #endif
        if (pbx_port_alloc(&_fxs_info->ports[i], &(_fxs_info->num_of_channels), &ProHWIntf) != RC_NONE) {
            syslog(LOG_ERR, "Failed to allocate for port %u - exiting program.", i);
            goto fini;
        }

        /* Generic demo framework setup */
        if (pbx_init_devices(&_fxs_info->ports[i]) < 0) {
            syslog(LOG_ERR, "failed to initiate devices");
            goto fini;
        }

        if (pbx_load_presets(&_fxs_info->ports[i]) < 0) {
            syslog(LOG_ERR, "failed to load presets");
            goto fini;
        }

        if (pbx_set_chan_state(&_fxs_info->ports[i]) < 0) {
            syslog(LOG_ERR, "failed to set channel state");
            goto fini;
        }

        syslog(LOG_DEBUG, "port: %d Number of devices: %d Number of channels: %d", i,
               _fxs_info->ports[i].numberOfDevice, _fxs_info->ports[i].numberOfChan);
    }

    rc = 0;

fini:
    return rc;
}

void netcomm_proslic_fini(void)
{
    int i;

    if (_fxs_info->ports) {
        for (i = 0; i < PBX_PORT_COUNT; i++) {
            pbx_port_free(&(_fxs_info->ports[i]));
        }
    }

    SIVOICE_FREE(_fxs_info->ports);

}
