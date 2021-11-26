/**
 * Declare common C defines or structures for the project
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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

#include "def.h"

time_t get_monotonic_ms()
{
    struct timespec T;

    clock_gettime(CLOCK_MONOTONIC, &T);

    return (T.tv_sec * 1000) + (T.tv_nsec / 1000000);
}

/*
 Convert Qualcomm QxDM timestamp to mseconds.

 Parameters:
  ts : pointer to 64 bit Qualcomm QxDM timestamp.

 Return:
  Converted C time is returned.
*/
unsigned long long get_ms_from_ts(const unsigned long long *ts)
{
    /*
        ### Qualcomm QxDM timestamp ###

        upper 48 bits represent elapsed time since 6 Jan 1980 00:00:00 in 1.25 ms units. The low order 16 bits represent elapsed time
        since the last 1.25 ms tick in 1/32 chip units (this 16 bit counter wraps at the value 49152).

        ### C time ###

        It represents the number of seconds elapsed since the Epoch, 1970-01-01 00:00:00 +0000 (UTC).
    */

    unsigned long long ticks_qc = *ts >> 16;
    unsigned long long ticks_chip = *ts & 0xffff;

    /*
        * floating point calculation to get seconds

        seconds = (ticks_qc * 1.25 + ticks_chip / 49152 * 1.25) / 1000

        * maximum precious integer calculation with minimum loss but without overflow.

        seconds = (ticks_qc * 125 + ticks_chips * 125 / 49152) / 100
    */

    return (ticks_qc * 125 + ticks_chip * 125 / 49152) / 100;
}

/*
 Convert Qualcomm QxDM timestamp to ctime.

 Parameters:
  ts : pointer to 64 bit Qualcomm QxDM timestamp.

 Return:
  Converted C time is returned.
*/
time_t get_ctime_from_ts(unsigned long long *ts)
{
    unsigned long long ms = get_ms_from_ts(ts);

    /* Qualcomm time base 6 Jan 1980 00:00:00 */
#define QC_TIMESTAMP_BASE 315964800UL

    unsigned long long sec = ms / 1000 + QC_TIMESTAMP_BASE;

    return sec;
}
