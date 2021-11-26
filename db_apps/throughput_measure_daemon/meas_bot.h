#ifndef MEAS_BOT_H_02052018
#define MEAS_BOT_H_02052018

/*
 * Measurement bot calculates each measurement into averages.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
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

#include "tick_clock.h"
#include <stdint.h>

struct meas_bot_t {
    int bps_valid;

    int peak_bps_valid;
    uint32_t peak_bps;

    int valid_diff_for_avg;
    time_diff_ms_t duration_for_avg;
    uint64_t diff_for_avg;

    uint32_t idle_bps;

    uint64_t bytes;

    int rtt;
    int rtt_valid;
    int rtt_count;
};

void meas_bot_init(struct meas_bot_t* mb, uint32_t idle_bps);
void meas_bot_reset(struct meas_bot_t *mb);
int meas_bot_get_avg_bps(struct meas_bot_t *mb);
void meas_bot_feed(struct meas_bot_t *mb, unsigned duration, uint64_t diff);
void meas_bot_fini(struct meas_bot_t* mb);
int meas_bot_is_bps_valid(struct meas_bot_t* mb);
void meas_bot_feed_rtt(struct meas_bot_t* mb, uint32_t rtt);


#endif

