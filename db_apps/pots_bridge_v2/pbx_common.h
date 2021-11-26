#ifndef __PBX_COMMON_H_20180620__
#define __PBX_COMMON_H_20180620__

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

#ifdef TSTIN_SUPPORT
#include "proslic_tstin.h"
#endif

#define NUM_OF_IRQS 4

/* maximum number of digits to call */
#define MAX_NUM_OF_PHONE_NUM 64
#define MAX_LENGTH_OF_CID 512

/* IRQ storage structure */
struct irq_storage_t {
    uInt8 irq_save[NUM_OF_IRQS]; /* Save/restore IRQ settings for various functions */
};

/* FXS port structure */
struct fxs_port_t {

    /* FXS port information */
    int deviceType;
    int numberOfDevice;
    int numberOfChan;
    int chanPerDevice;

    int chan_base_idx;

    SiVoiceChanType_ptr channels;
    SiVoiceChanType_ptr *cptrs;
    SiVoiceDeviceType_ptr devices;

};

/* FXS information structure */
struct fxs_info_t {
    struct fxs_port_t *ports;
    int num_of_channels;
};

typedef enum {
    DEMO_RING_PRESET,
    DEMO_DCFEED_PRESET,
    DEMO_IMPEDANCE_PRESET,
    DEMO_FSK_PRESET,
    DEMO_PM_PRESET,
    DEMO_TONEGEN_PRESET,
    DEMO_PCM_PRESET,
    DEMO_VDAA_COUNTRY_PRESET,
    DEMO_VDAA_AUDIO_GAIN_PRESET,
    DEMO_VDAA_RING_VALIDATION_PRESET,
    DEMO_VDAA_PCM_PRESET,
    DEMO_VDAA_HYBRID_PRESET,
} demo_preset_t;

int  pbx_get_preset(demo_preset_t preset_enum);
int  pbx_init_devices(struct fxs_port_t *port);
int  pbx_load_presets(struct fxs_port_t *port);
int  pbx_port_alloc(struct fxs_port_t *port, int *base_channel_index, controlInterfaceType *proslic_api_hwIf);
void pbx_init_port_info(struct fxs_port_t *port, unsigned int port_id);
void pbx_port_free(struct fxs_port_t *port);
void pbx_shutdown(struct fxs_port_t *port);

#endif

