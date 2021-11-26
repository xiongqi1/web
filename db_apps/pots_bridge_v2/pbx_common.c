/*
 * pbx_common -PBX common functions.
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

#include "pbx_common.h"
#include "pbx_config.h"
#include "netcomm_proslic.h"
#include <si_voice_datatypes.h>
#include <syslog.h>

// #include DEMO_INCLUDE

#define PROSLIC_PROMPT "--> "

/* If the header files are NOT defined, this is the fall back limit */
#define SI_DEMO_MAX_ENUM_DEFAULT 32

#ifdef FXS_CONSTANTS_HDR
#include FXS_CONSTANTS_HDR /* See makefile for how this is determined... */
#endif

#ifdef DAA_CONSTANTS_HDR
#include DAA_CONSTANTS_HDR /* See makefile for how this is determined... */
#endif

#ifdef SI3217X
#include "si3217x.h"
#include "vdaa.h"
extern Si3217x_General_Cfg Si3217x_General_Configuration;
#endif

#ifdef SI3226X
#include "si3226x.h"
#endif

#ifdef SI3218X
#include "si3218x.h"
#endif

#ifdef SI3219X
#include "si3219x.h"
#endif

#ifdef SI3228X
#include "si3228x.h"
#endif

#ifdef VDAA_SUPPORT
#include "vdaa.h"
#endif

#ifdef TSTIN_SUPPORT
#include "proslic_tstin_limits.h"
#endif

/*****************************************************************************************************/
#ifdef PERL

#ifndef NOFXS_SUPPORT
extern const char *Ring_preset_options[];
extern const char *DcFeed_preset_options[];
extern const char *Impedance_preset_options[];
extern const char *FSK_preset_options[];
extern const char *PulseMeter_preset_options[];
extern const char *Tone_preset_options[];
extern const char *PCM_preset_options[];
#endif

#ifdef VDAA_SUPPORT
extern const char *Vdaa_country_preset_options[];
extern const char *Vdaa_audioGain_preset_options[];
extern const char *Vdaa_ringDetect_preset_options[];
extern const char *Vdaa_PCM_preset_options[];
extern const char *Vdaa_hybrid_preset_options[];
#endif

#endif /* PERL */
/*****************************************************************************************************/

#define SI3217XB_NUMBER_OF_CHAN (SI3217XB_NUMBER_OF_DEVICE*SI3217X_CHAN_PER_DEVICE)
#define SI3217XC_NUMBER_OF_CHAN (SI3217XC_NUMBER_OF_DEVICE*SI3217X_CHAN_PER_DEVICE)
#define SI3218X_NUMBER_OF_CHAN (SI3218X_NUMBER_OF_DEVICE*SI3218X_CHAN_PER_DEVICE)
#define SI3219X_NUMBER_OF_CHAN (SI3219X_NUMBER_OF_DEVICE*SI3219X_CHAN_PER_DEVICE)
#define SI3226X_NUMBER_OF_CHAN (SI3226X_NUMBER_OF_DEVICE*SI3226X_CHAN_PER_DEVICE)
#define SI3228X_NUMBER_OF_CHAN (SI3228X_NUMBER_OF_DEVICE*SI3228X_CHAN_PER_DEVICE)
#define SI3050_NUMBER_OF_CHAN (SI3050_NUMBER_OF_DEVICE*SI3050_CHAN_PER_DEVICE)

/*****************************************************************************************************/
/* This is a simple initialization - all ports have the same device type.  For a more complex
 * system, a customer could key off of port_id and depending on value, fill in the "correct" setting
 * for their system. No memory allocation is done here.
 */
#ifndef SIVOICE_USE_CUSTOM_PORT_INFO
/**
 * @brief initiates port information.
 *
 * @param port is FXS port structure to initiate.
 * @param port_id is port id.
 */
void pbx_init_port_info(struct fxs_port_t *port, unsigned int port_id)
{
    SILABS_UNREFERENCED_PARAMETER(port_id);
    #ifdef SI3217X
    port->deviceType = SI3217X_TYPE;
    port->numberOfDevice = SI3217XB_NUMBER_OF_DEVICE;
    port->numberOfChan = SI3217XB_NUMBER_OF_CHAN;
    port->chanPerDevice = SI3217X_CHAN_PER_DEVICE;
    #endif

    #ifdef SI3226X
    port->deviceType = SI3226X_TYPE;
    port->numberOfDevice = SI3226X_NUMBER_OF_DEVICE;
    port->numberOfChan = SI3226X_NUMBER_OF_CHAN;
    port->chanPerDevice = SI3226X_CHAN_PER_DEVICE;
    #endif

    #ifdef SI3218X
    port->deviceType     = SI3218X_TYPE;
    port->numberOfDevice = SI3218X_NUMBER_OF_DEVICE;
    port->numberOfChan   = SI3218X_NUMBER_OF_CHAN;
    port->chanPerDevice  = SI3218X_CHAN_PER_DEVICE;
    #endif

    #ifdef SI3219X
    port->deviceType     = SI3219X_TYPE;
    port->numberOfDevice = SI3219X_NUMBER_OF_DEVICE;
    port->numberOfChan   = SI3219X_NUMBER_OF_CHAN;
    port->chanPerDevice  = SI3219X_CHAN_PER_DEVICE;
    #endif

    #ifdef SI3228X
    port->deviceType     = SI3228X_TYPE;
    port->numberOfDevice = SI3228X_NUMBER_OF_DEVICE;
    port->numberOfChan   = SI3228X_NUMBER_OF_CHAN;
    port->chanPerDevice  = SI3228X_CHAN_PER_DEVICE;
    #endif

    #if defined(SI3050_CHIPSET) && !defined(SI3217X)
    port->deviceType     = SI3050_TYPE;
    port->numberOfDevice = SI3050_NUMBER_OF_DEVICE;
    port->numberOfChan   = SI3050_NUMBER_OF_CHAN;
    port->chanPerDevice  = SI3050_CHAN_PER_DEVICE;
    #endif

}
#endif /* SIVOICE_USE_CUSTOM_PORT_INFO */

/**
 * @brief allocate FXS port.
 *
 * @param port is FXS port to allocate.
 * @param base_channel_index is logical base channel index.
 * @param proslic_api_hwIf is ProSLIC hardware control interface type.
 *
 * @return
 */
int pbx_port_alloc(struct fxs_port_t *port, int *base_channel_index, controlInterfaceType *proslic_api_hwIf)
{
    int i;
    static unsigned int chan_ndx = 0;

    syslog(LOG_DEBUG, "creating ProSLIC devices");

    /* First allocate all the needed data structures for ProSLIC API */
    if (SiVoice_createDevices(&(port->devices),
                              (uInt32)(port->numberOfDevice)) != RC_NONE) {
        return RC_NO_MEM;
    }

    if (SiVoice_createChannels(&(port->channels),
                               (uInt32)port->numberOfChan) != RC_NONE) {
        goto free_mem;
    }

    /* Demo specific code */
    port->cptrs = SIVOICE_CALLOC(sizeof(SiVoiceChanType_ptr),
                                 port->numberOfChan);

    /* Back to API init code */
    #ifdef TSTIN_SUPPORT
    LOGPRINT("%sCreating Inward Test Objs...\n", LOGPRINT_PREFIX);
    if (ProSLIC_createTestInObjs(&(port->pTstin),
                                 (uInt32)(port->numberOfChan) != RC_NONE)) {
        goto free_mem;
    }
    #endif

    /* Demo specific code */
    port->chan_base_idx = *base_channel_index;
    (*base_channel_index) += port->numberOfChan;

    /* For 3050 based devices we need to ensure the  channel number is even */
    if (port->deviceType == SI3050_TYPE) {
        chan_ndx += ((port->chan_base_idx) & 1);
    }

    for (i = 0; i < port->numberOfChan; i++) {
        /* If supporting more than 1 SPI interface, a code change is needed here - channel count may not match
         * and the pointer to the api_hwIf may change for the SPI object... */
        if (SiVoice_SWInitChan(&(port->channels[i]),
                               chan_ndx, port->deviceType,
                               &(port->devices[i / port->chanPerDevice]), proslic_api_hwIf) != RC_NONE) {
            goto free_mem;
        }
        /* For Si350 AND a SPI designed for PROSLIC, the channel index needs to be
         * in steps of 2 vs. 1
         */
        if (port->deviceType == SI3050_TYPE) {
            chan_ndx += ((i + 1) << 1);
        } else {
            chan_ndx++;
        }
        port->cptrs[i] = &(port->channels[i]); /* Demo specific */
        #ifdef TSTIN_SUPPORT
        LOGPRINT("%sConfiguring Inward Tests...\n", LOGPRINT_PREFIX);
        ProSLIC_testInPcmLpbkSetup(port->pTstin, &ProSLIC_testIn_PcmLpbk_Test);
        ProSLIC_testInDcFeedSetup(port->pTstin, &ProSLIC_testIn_DcFeed_Test);
        ProSLIC_testInRingingSetup(port->pTstin, &ProSLIC_testIn_Ringing_Test);
        ProSLIC_testInBatterySetup(port->pTstin, &ProSLIC_testIn_Battery_Test);
        ProSLIC_testInAudioSetup(port->pTstin, &ProSLIC_testIn_Audio_Test);
        #endif

        SiVoice_setSWDebugMode(&(port->channels[i]),
                               TRUE); /* Enable debug mode for all channels */
        #ifdef ENABLE_INITIAL_LOGGING
        SiVoice_setTraceMode(&(port->channels[i]),
                             TRUE); /* Enable trace mode for all channels */
        #endif
    }


    return RC_NONE;

free_mem:
    pbx_port_free(port);
    return RC_NO_MEM;
}

/**
 * @brief start FXS port.
 *
 * @param port is FXS port to start.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int pbx_init_devices(struct fxs_port_t *port)
{
    int rc = 0;

    if (port->deviceType != SI3050_TYPE) {
        LOGPRINT("Initializing ProSLIC...\n");
        if ((rc = ProSLIC_Init(port->cptrs, port->numberOfChan)) != RC_NONE) {
            syslog(LOG_ERR, "ProSLIC_Init() failed #%d", rc);
            return (-1);
        }
    }

    /*
     ** Longitudinal Balance Calibration
     **
    */
    LOGPRINT("Starting Longitudinal Balance Calibration...\n");
    rc = ProSLIC_LBCal(port->cptrs, port->numberOfChan);
    if (rc != RC_NONE) {
        LOGPRINT("LB CAL ERROR : %d\n", rc);
    }

    return rc;
}

/**
 * @brief loads presets for FXS port.
 *
 * @param port is FXS port to load presets.
 *
 * @return
 */

/*

	* Cadence settings - US

	RING / ProSLIC_RingSetup()        : RING_MAX_VBAT_PROVISIONING
	DC Feed / ProSLIC_DCFeedSetup()   : DCFEED_48V_20MA
	Impedance / ProSLIC_ZsynthSetup() : ZSYN_600_0_0_30_0
	FSK / ProSLIC_FSKSetup()          : DEFAULT_FSK
	PCM / ProSLIC_PCMSetup()          : PCM_16LIN
	TONE                              : US (https://www.3amsystems.com/World_Tone_Database)

	TODO: add more cadence settings for countries.

*/


int pbx_load_presets(struct fxs_port_t *port)
{
    int i;
    SiVoiceChanType_ptr cptr;

    int rx_offset = 1;
    int tx_offset = 16;

    for (i = 0; i < port->numberOfChan; i++) {
        cptr = port->cptrs[i];

        ProSLIC_RingSetup(cptr, 0);
        ProSLIC_DCFeedSetup(cptr, 0);
        ProSLIC_RingSetup(cptr, 0);
        ProSLIC_ZsynthSetup(cptr, 0);
        ProSLIC_PCMSetup(cptr, PCM_16LIN);

        /* setup channel */
        ProSLIC_AudioGainSetup(cptr, 0, -5, ZSYN_600_0_0_30_0);
        ProSLIC_PCMTimeSlotSetup(cptr, PBX_TIMESLOTS(i) + rx_offset, PBX_TIMESLOTS(i) + tx_offset);
    }
    return 0;
}

/**
 * @brief shuts down FXS port.
 *
 * @param port is FXS prot to shut down.
 */
void pbx_shutdown(struct fxs_port_t *port)
{
    int i;
    SiVoiceChanType_ptr chanPtr;

    for (i = 0; i < port->numberOfChan; i++) {
        chanPtr = port->cptrs[i];
        ProSLIC_ShutdownChannel(chanPtr);
    }
}

/**
 * @brief frees FXS port.
 *
 * @param port is FXS port to free.
 */
void pbx_port_free(struct fxs_port_t *port)
{
    SiVoice_destroyChannels(&(port->channels));
    SiVoice_destroyDevices(&(port->devices));

    if (port->cptrs != NULL) {
        SIVOICE_FREE(port->cptrs);
    }
}

