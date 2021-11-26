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

#include "meas_bot.h"

#include <string.h>



/*
	* use 32 bit int as it is practical

	bps = (diff*8) / (duration/1000)
*/

#define GET_BPS(bytes,ms) ((uint32_t)((bytes)*1000*8/(ms)))

/**
 * @brief Finalize a Measurement bot object to destroy.
 *
 * @param mb is a Measurement bot object.
 */
void meas_bot_fini(struct meas_bot_t* mb)
{
}

/**
 * @brief Initiate a Measurement bot object to use.
 *
 * @param mb is a Measurement bot object.
 * @param idle_bps is idle bps threshold for throughput measurement.
 */
void meas_bot_init(struct meas_bot_t* mb, uint32_t idle_bps)
{
    /* initiate members */
    memset(mb, 0, sizeof(*mb));

    mb->idle_bps = idle_bps;
}

/**
 * @brief Resets a Measurement bot object to invalidate.
 *
 * @param mb is a Measurement bot object.
 */
void meas_bot_reset(struct meas_bot_t* mb)
{
    mb->duration_for_avg = 0;
    mb->diff_for_avg = 0;
    mb->bps_valid = 0;
    mb->valid_diff_for_avg = 0;
    mb->peak_bps = 0;
    mb->peak_bps_valid = 0;
    mb->bytes = 0;

    /* reset rtt */
    mb->rtt = 0;
    mb->rtt_valid = 0;
    mb->rtt_count = 0;
}

/**
 * @brief Returns BPS validation flag.
 *
 * @param mb is a Measurement bot object.
 *
 * @return BPS validation flag.
 */
int meas_bot_is_bps_valid(struct meas_bot_t* mb)
{
    return mb->bps_valid;
}

/**
 * @brief Calculate BPS during average period.
 *
 * @param mb is a Measurement bot object.
 *
 * @return average BPS when BPS is valid. Otherwise, 0.
 */
int meas_bot_get_avg_bps(struct meas_bot_t* mb)
{
    return !mb->duration_for_avg ? 0 : GET_BPS(mb->diff_for_avg, mb->duration_for_avg);
}

/**
 * @brief Feed RTT value to a Measurement bot object.
 *
 * @param mb is a Measurement bot object.
 * @param rtt is a RTT.
 */
void meas_bot_feed_rtt(struct meas_bot_t* mb, uint32_t rtt)
{
    int count = mb->rtt_count + 1;

    mb->rtt = (mb->rtt * mb->rtt_count + rtt) / count;

    mb->rtt_count = count;
    mb->rtt_valid = 1;
}


/**
 * @brief Feed measurement duration and delta of received or sent bytes.
 *
 * @param mb is a Measurement bot object.
 * @param duration is a measurement duration.
 * @param diff is received or sent bytes.
 */
void meas_bot_feed(struct meas_bot_t* mb, unsigned duration, uint64_t diff)
{
    uint32_t bps;

    bps = GET_BPS(diff, duration);

    /* ignore if throughput is equal or lower than idle */
    if (bps > mb->idle_bps) {

        /* accumulate if bps is greater than idle bps */
        mb->duration_for_avg += duration;
        mb->diff_for_avg += diff;
    }

    /* update peak bps if necessary */
    if (!mb->peak_bps_valid || (mb->peak_bps < bps)) {
        mb->peak_bps = bps;
    }
    mb->peak_bps_valid = 1;

    /* accumulate total bytes */
    mb->bytes += diff;
    /* set peak bps validation flag */
    mb->bps_valid = 1;
}
