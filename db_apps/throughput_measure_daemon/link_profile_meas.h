#ifndef LINK_PROFILE_MEAS_H_02052018
#define LINK_PROFILE_MEAS_H_02052018

/*
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

struct link_profile_meas_t {

    int profile_no;

    int last_valid;
    uint64_t last_recv_bytes;
    uint64_t last_sent_bytes;

    int diff_valid;
    uint64_t diff_recv_bytes;
    uint64_t diff_sent_bytes;

    int if_running;
};

#define LINK_PROFILE_COUNT 6

void link_profile_meas_read(struct link_profile_meas_t *lpm);
struct link_profile_meas_t *link_profile_meas_collection_get(int profile_no);
int link_profile_meas_collection_read(time_diff_ms_t* duration);
void link_profile_meas_collection_fini(void);
int link_profile_meas_collection_init(void);

#endif

