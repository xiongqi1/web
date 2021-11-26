#ifndef __SOFTTONEGEN_H_20180710__
#define __SOFTTONEGEN_H_20180710__

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

struct softtonegen_t;

typedef void (*softtonegen_play_callback)(struct softtonegen_t* stg, void* ref);

struct softtonegen_t {

    int tone_type;
    int tone_idx;

    int repeat;

    int inactive_valid;
    uint64_t inactive_timestamp;
    int inactive_delay_ms;

    SiVoiceChanType_ptr cptr;

    softtonegen_play_callback cb;
    void* ref;
};

#define SOFTTONEGEN_MAX_TONE_SEQ	4


struct softtonegen_info_t {
    int tones[SOFTTONEGEN_MAX_TONE_SEQ];
};

enum {
    softtonegen_type_confirmation,
    softtonegen_type_error,
    softtonegen_type_callwaiting,
    softtonegen_type_sas,
    softtonegen_type_cas,
};

int softtonegen_play_dtmf_tone(struct softtonegen_t* stg, const char digit);
int softtonegen_play_hard_tone(struct softtonegen_t* stg, int tone);
int softtonegen_play_soft_tone(struct softtonegen_t* stg, int tone_type, int repeat, softtonegen_play_callback cb,
                               void* ref);
void softtonegen_stop(struct softtonegen_t* stg);

int softtonegen_feed_execution(struct softtonegen_t* stg);
int softtonegen_process_interrupt(struct softtonegen_t* stg, int irq);

int softtonegen_init(struct softtonegen_t* stg, SiVoiceChanType_ptr cptr);
void softtonegen_fini(struct softtonegen_t* stg);

#endif
