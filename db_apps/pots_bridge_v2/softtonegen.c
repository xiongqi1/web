/*
 * softtonegen - soft tone generator generates software tone with SLIC.
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

#define _GNU_SOURCE
#include "utils.h"
#include "netcomm_proslic.h"
#include "softtonegen.h"
#include <syslog.h>
#include <math.h>
#include FXS_CONSTANTS_HDR

#define SAMPLING_RATE 8000.0
#define SAMPLES_PER_MILISEC (SAMPLING_RATE / 1000)
/* 3 dBm 0TLP */
#define DTMF_0TLP_DBM 3
#define DTMF_HIGH_FREQ_DBM -6
#define DTMF_LOW_FREQ_DBM -8


struct softtonegen_info_t _tone_info[] = {
    [softtonegen_type_confirmation] = {
        {TONEGEN_FCC_CONFIRMATION_0, TONEGEN_FCC_CONFIRMATION_1, TONEGEN_FCC_CONFIRMATION_2, -1}
    },

    [softtonegen_type_error] = {
        {TONEGEN_FCC_SPECIAL_INFORMATION_0, TONEGEN_FCC_SPECIAL_INFORMATION_1, TONEGEN_FCC_SPECIAL_INFORMATION_2, -1}
    },

    [softtonegen_type_callwaiting] = {
        {TONEGEN_FCC_CALLWAITING_0, TONEGEN_FCC_CALLWAITING_1, -1}
    },

    [softtonegen_type_sas] = {
        {TONEGEN_FCC_SAS, -1}
    },

    [softtonegen_type_cas] = {
        {TONEGEN_FCC_CAS, -1}
    },

};

/**
 * @brief stops soft-tone generator.
 *
 * @param stg soft-tone generator object.
 */
void softtonegen_stop(struct softtonegen_t* stg)
{
    SiVoiceChanType_ptr cptr = stg->cptr;

    syslog(LOG_DEBUG, "[softtonegen] stop tone");

    /* disable oscillators and active timers */
    ProSLIC_ToneGenStop(stg->cptr);

    /* disable oscillator interrupts */
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_IRQEN1, 0,
                                      PROSLIC_REG_IRQEN1_OSCS_TA_IE | PROSLIC_REG_IRQEN1_OSCS_TI_IE);

    /* invalidate tone */
    stg->tone_idx = -1;
    stg->tone_type = -1;
}


/**
 * @brief plays next soft-tone.
 *
 * @param stg soft-tone generator object.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int softtonegen_play_soft_tone_next(struct softtonegen_t* stg)
{
    SiVoiceChanType_ptr cptr = stg->cptr;

    int tone;
    struct softtonegen_info_t* tone_info;

    /* bypass if soft tone is not configured */
    if (stg->tone_type < 0 || stg->tone_idx < 0) {
        goto err;
    }

    /* get tone */
    tone_info = &_tone_info[stg->tone_type];
    tone = tone_info->tones[stg->tone_idx];

    syslog(LOG_DEBUG, "[softtonegen] play soft tone next (type=%d,idx=%d,tone=%d)", stg->tone_type, stg->tone_idx, tone);

    /* bypass if soft tone finishes */
    if (tone < 0) {

        if (stg->repeat) {
            syslog(LOG_DEBUG, "[softtonegen] restart soft tone (tone_type=%d,tone_idx=%d)", stg->tone_type, stg->tone_idx);

            stg->tone_idx = 0;

            /* immediately schedule to play the first soft tone */
            stg->inactive_delay_ms = 0;
            stg->inactive_valid = TRUE;
            stg->inactive_timestamp = get_monotonic_msec();

        } else {
            softtonegen_stop(stg);

            if (stg->cb) {
                stg->cb(stg, stg->ref);
            }

            goto err;
        }

    } else {

        /* advance tone index */
        stg->tone_idx++;

        ProSLIC_Tone_Cfg* tone_cfg = NULL;

        /* get tone configuration */
        if (ProSLIC_GetToneGenCfg(cptr, tone, &tone_cfg) < 0) {
            syslog(LOG_ERR, "failed to obtain tone configuration");
            goto err;
        }

        /* get inactive delay */
        int inactive_delay_125us = (tone_cfg->osc1.tihi << 8 | tone_cfg->osc1.tilo);
        stg->inactive_delay_ms = inactive_delay_125us / (1000 / 125);

        syslog(LOG_DEBUG, "[softtonegen] get delay (type=%d,idx=%d,tone=%d,delay=%d)", stg->tone_type, stg->tone_idx, tone,
               stg->inactive_delay_ms);

        /* load hard tone */
        ProSLIC_ToneGenSetup(cptr, tone);

        /* enable oscillators and active timers */
        netcomm_proslic_reg_set_and_reset(cptr,
                                          PROSLIC_REG_OCON,
                                          PROSLIC_REG_OCON_OSCS_EN | PROSLIC_REG_OCON_OSCS_TA_EN,
                                          0);

    }

    return 0;

err:
    return -1;

}

/**
 * @brief plays hard-tone.
 *
 * @param stg is soft-tone generator object.
 * @param tone is tone index.
 */
int softtonegen_play_hard_tone(struct softtonegen_t* stg, int tone)
{
    int enable_timer;

    ProSLIC_Tone_Cfg* tone_cfg = NULL;
    SiVoiceChanType_ptr cptr = stg->cptr;

    softtonegen_stop(stg);


    if (ProSLIC_GetToneGenCfg(cptr, tone, &tone_cfg) < 0) {
        syslog(LOG_ERR, "failed to obtain tone configuration");
        goto err;
    }

    /* enable timer when any of timer is configured */
    enable_timer = tone_cfg->osc1.tahi || tone_cfg->osc1.talo || tone_cfg->osc1.tihi || tone_cfg->osc1.tilo
                   || tone_cfg->osc2.tahi || tone_cfg->osc2.talo || tone_cfg->osc2.tihi || tone_cfg->osc2.tilo;

    /* load hard tone */
    ProSLIC_ToneGenSetup(cptr, tone);

    syslog(LOG_DEBUG, "[softtonegen] play hard tone (tone=%d,en_timer=%d)", tone, enable_timer);


    /* enable oscillators and active timers */
    if (enable_timer) {
        netcomm_proslic_reg_set_and_reset(cptr,
                                          PROSLIC_REG_OCON,
                                          PROSLIC_REG_OCON_OSCS_EN | PROSLIC_REG_OCON_OSCS_TA_EN | PROSLIC_REG_OCON_OSCS_TI_EN,
                                          0);
    }	else {
        netcomm_proslic_reg_set_and_reset(cptr,
                                          PROSLIC_REG_OCON,
                                          PROSLIC_REG_OCON_OSCS_EN,
                                          PROSLIC_REG_OCON_OSCS_TA_EN | PROSLIC_REG_OCON_OSCS_TI_EN);
    }

    return 0;

err:
    return -1;
}

/**
 * @brief plays raw tone.
 *
 * @param stg is soft-tone generator object.
 * @param tone1_hz is tone1 Hz.
 * @param tone2_hz is tone2 Hz.
 * @param volume1 is volume for tone1
 * @param volume2 is volume for tone2
 * @param timer_active is timer for active duration.
 * @param timer_inactive is timer for inactive duration.
 * @return 0 when it succeeds. Otherwise, -1.
 */
int softtonegen_play_raw_tone(struct softtonegen_t* stg, int tone1_hz, int tone2_hz, int volume1, int volume2,
                              int timer_active,
                              int timer_inactive)
{
    double tone1_coeff;
    double tone2_coeff;

    double tone1_freq;
    double tone2_freq;

    double tone1_amp;
    double tone2_amp;

    int enable_timer;


    ProSLIC_Tone_Cfg tone_cfg_obj;
    ProSLIC_Tone_Cfg* tone_cfg = &tone_cfg_obj;

    SiVoiceChanType_ptr cptr = stg->cptr;

    /* get frequency and amplitude */
    tone1_coeff = cos(2 * M_PI * tone1_hz / SAMPLING_RATE);
    tone2_coeff = cos(2 * M_PI * tone2_hz / SAMPLING_RATE);
    tone1_freq = tone1_coeff * (1 << 14);
    tone2_freq = tone2_coeff * (1 << 14);
    tone1_amp = sqrt((1 - tone1_coeff) / (1 + tone1_coeff)) * (double)((1 << 15) - 1) / 4 * (double)volume1 / 100 / 1.11;
    tone2_amp = sqrt((1 - tone1_coeff) / (1 + tone1_coeff)) * (double)((1 << 15) - 1) / 4 * (double)volume2 / 100 / 1.11;

    /* configure SLIC - OSC1 */
    tone_cfg->osc1.freq = ((int)tone1_freq) << 13;
    tone_cfg->osc1.amp = ((int)tone1_amp) << 13;
    tone_cfg->osc1.phas = 0;
    tone_cfg->osc1.tahi = (timer_active >> 8) & 0xff;
    tone_cfg->osc1.talo = timer_active & 0xff;
    tone_cfg->osc1.tihi = (timer_inactive >> 8) & 0xff;
    tone_cfg->osc1.tilo = timer_inactive & 0xff;
    /* configure SLIC - OSC2 */
    tone_cfg->osc2.freq = ((int)tone2_freq) << 13;
    tone_cfg->osc2.amp = ((int)tone2_amp) << 13;
    tone_cfg->osc2.phas = 0;
    tone_cfg->osc2.tahi = (timer_active >> 8) & 0xff;
    tone_cfg->osc2.talo = timer_active & 0xff;
    tone_cfg->osc2.tihi = (timer_inactive >> 8) & 0xff;
    tone_cfg->osc2.tilo = timer_inactive & 0xff;

    tone_cfg->omode = (tone1_hz ? 0x06 : 0x00) | (tone2_hz ? 0x60 : 0x00);

    /* enable timer when any of timer is configured */
    enable_timer = tone_cfg->osc1.tahi || tone_cfg->osc1.talo || tone_cfg->osc1.tihi || tone_cfg->osc1.tilo
                   || tone_cfg->osc2.tahi || tone_cfg->osc2.talo || tone_cfg->osc2.tihi || tone_cfg->osc2.tilo;

    ProSLIC_ToneGenSetupPtr(cptr, tone_cfg);

    syslog(LOG_DEBUG, "play tones (tone1=%d Hz,tone2=%d Hz, act=%d,inact=%d)", tone1_hz, tone2_hz,
           timer_active, timer_inactive);

    /* enable oscillators and active timers */
    if (enable_timer) {
        netcomm_proslic_reg_set_and_reset(cptr,
                                          PROSLIC_REG_OCON,
                                          PROSLIC_REG_OCON_OSCS_EN | PROSLIC_REG_OCON_OSCS_TA_EN | PROSLIC_REG_OCON_OSCS_TI_EN,
                                          0);
    }	else {
        netcomm_proslic_reg_set_and_reset(cptr,
                                          PROSLIC_REG_OCON,
                                          PROSLIC_REG_OCON_OSCS_EN,
                                          PROSLIC_REG_OCON_OSCS_TA_EN | PROSLIC_REG_OCON_OSCS_TI_EN);
    }

    return 0;
}


int softtonegen_play_dtmf_tone(struct softtonegen_t* stg, const char digit)
{
    struct dtmf_digit_tone_t {
        int tone1;
        int tone2;
    };

    const struct dtmf_digit_tone_t* dtmf_digit_tone;
    static const struct dtmf_digit_tone_t dtmf_digit_tones[] = {
        ['1'] = {697, 1209},
        ['2'] = {697, 1336},
        ['3'] = {697, 1477},
        ['A'] = {697, 1633},
        ['4'] = {770, 1209},
        ['5'] = {770, 1336},
        ['6'] = {770, 1477},
        ['B'] = {770, 1633},
        ['7'] = {852, 1209},
        ['8'] = {852, 1336},
        ['9'] = {852, 1477},
        ['C'] = {852, 1633},
        ['*'] = {941, 1209},
        ['0'] = {941, 1336},
        ['#'] = {941, 1477},
        ['D'] = {941, 1633},
    };

    int volume1_dBm;
    int volume2_dBm;
    int volume1;
    int volume2;

    /* check range */
    if ((digit < 0) || (digit > __countof(dtmf_digit_tones))) {
        syslog(LOG_ERR, "DTMF digit out of range (digit='%c')", digit);
        goto err;
    }

    dtmf_digit_tone = &dtmf_digit_tones[(int)digit];

    if (!dtmf_digit_tone->tone1 && dtmf_digit_tone->tone2) {
        syslog(LOG_ERR, "unknown DTMF digit (digit='%c')", digit);
        goto err;
    }

    volume1_dBm = DTMF_LOW_FREQ_DBM;
    volume2_dBm = DTMF_HIGH_FREQ_DBM;

    volume1 = (int)((double)100 * pow10(((double)volume1_dBm - DTMF_0TLP_DBM) / 20));
    volume2 = (int)((double)100 * pow10(((double)volume2_dBm - DTMF_0TLP_DBM) / 20));

    syslog(LOG_DEBUG, "play DTMF tone (digit='%c',vol1=%d,vol2=%d)", digit, volume1, volume2);
    softtonegen_play_raw_tone(stg, dtmf_digit_tone->tone1, dtmf_digit_tone->tone2, volume1, volume2, 0, 0);

    return 0;

err:
    return -1;
}

/**
 * @brief plays soft-tone.
 *
 * @param stg is soft-tone generator object.
 * @param tone_type is soft-tone type.
 * @param repeat is repeat flag (0=disable auto-repeat, 1=enable auto-repeat)
 *
 * @return
 */
int softtonegen_play_soft_tone(struct softtonegen_t* stg, int tone_type, int repeat, softtonegen_play_callback cb,
                               void* ref)
{
    SiVoiceChanType_ptr cptr = stg->cptr;

    softtonegen_stop(stg);

    syslog(LOG_DEBUG, "[softtonegen] start soft tone (tone_type=%d,repeat=%d)", tone_type, repeat);

    /* initiate softtone */
    stg->tone_type = tone_type;
    stg->tone_idx = 0;
    stg->repeat = repeat;
    stg->cb = cb;
    stg->ref = ref;

    /* enable oscillator interrupts */
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_IRQEN1, PROSLIC_REG_IRQEN1_OSCS_TA_IE, 0);

    /* start soft tone */
    return softtonegen_play_soft_tone_next(stg);
}

/**
 * @brief finalize soft-tone generator
 *
 * @param stg is soft-tone generator object.
 */
void softtonegen_fini(struct softtonegen_t* stg)
{
    softtonegen_stop(stg);
}

/**
 * @brief initiates soft-tone generator.
 *
 * @param stg is soft-tone generator object.
 * @param cptr is SLIC voice channel type pointer.
 *
 * @return
 */
int softtonegen_init(struct softtonegen_t* stg, SiVoiceChanType_ptr cptr)
{
    memset(stg, 0, sizeof(*stg));

    stg->cptr = cptr;
    stg->tone_type = -1;
    stg->tone_idx = -1;

    return 0;
}

int softtonegen_feed_execution(struct softtonegen_t* stg)
{
    uint64_t now;

    if (stg->inactive_valid) {
        now = get_monotonic_msec();

        if (now - stg->inactive_timestamp >= stg->inactive_delay_ms) {

            syslog(LOG_DEBUG, "[softtonegen] delay expired (tone_type=%d,tone_idx=%d)", stg->tone_type, stg->tone_idx);

            stg->inactive_valid = FALSE;

            softtonegen_play_soft_tone_next(stg);
        }
    }

    return 0;
}

/**
 * @brief processes soft-tone interrupt.
 *
 * @param stg is soft-tone generator object.
 * @param irq is IRQ number.
 *
 * @return
 */
int softtonegen_process_interrupt(struct softtonegen_t* stg, int irq)
{
    switch (irq) {
        case IRQ_OSC1_T1:

            syslog(LOG_DEBUG, "[softtonegen] start delay (tone_type=%d,tone_idx=%d)", stg->tone_type, stg->tone_idx);
            stg->inactive_valid = TRUE;
            stg->inactive_timestamp = get_monotonic_msec();
            break;
    }

    return 0;
}
