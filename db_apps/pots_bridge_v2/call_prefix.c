/*
 * Functions for converting the international call prefix for incoming and
 * outgoing calls.
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

#include "call_prefix.h"
#include "ezrdb.h"
#include "pbx_common.h"
#include <string.h>
#include <syslog.h>

#define RDB_NETWORK_MCC "wwan.0.system_network_status.MCC"

// international call dial prefix
struct mcc_country_calling_code_t {
    int mcc_start;
    int mcc_end;
    int local_call_prefix;
    const char* international_call_prefix;
};
struct mcc_country_calling_code_t _mccc[] = {
    {310, 316, 1, "011"},
    {0, 0, 0, NULL},
};

const char* call_prefix_get_outgoing_num_for_network(const char* num)
{
    int network_mcc;
    int network_matched = FALSE;

    static char new_num[MAX_NUM_OF_PHONE_NUM + 1];
    const char* new_num_ptr = new_num;

    struct mcc_country_calling_code_t* mccc_ptr;
    int prefix_len;

    /* get serving cell mcc */
    network_mcc = (int)ezrdb_get_int(RDB_NETWORK_MCC);

    mccc_ptr = _mccc;
    while (mccc_ptr->mcc_start) {

        network_matched = network_mcc >= mccc_ptr->mcc_start && network_mcc <= mccc_ptr->mcc_end;
        if (network_matched) {

            syslog(LOG_DEBUG, "network mcc matched (network_mcc=%d,mcc_start=%d,mcc_end=%d)", network_mcc, mccc_ptr->mcc_start,
                   mccc_ptr->mcc_end);

            prefix_len = strlen(mccc_ptr->international_call_prefix);
            if (!strncmp(num, mccc_ptr->international_call_prefix, prefix_len)) {
                snprintf(new_num, sizeof(new_num), "+%s", num + prefix_len);
                syslog(LOG_DEBUG, "international call (num=%s,new_num=%s)", num, new_num_ptr);
            } else {
                syslog(LOG_DEBUG, "no prefix information matched for outgoing num (network_mcc=%d)", network_mcc);
                new_num_ptr = num;
            }

            break;
        }

        mccc_ptr++;
    }

    if (!network_matched) {
        syslog(LOG_DEBUG, "no network information for outgoing cid (network_mcc=%d", network_mcc);
        new_num_ptr = num;
    }

    return new_num_ptr;
}

const char* call_prefix_get_incoming_cid_for_rj11(const char* cid_num)
{
    int network_mcc;
    int network_matched = FALSE;

    static char new_cid_num[MAX_NUM_OF_PHONE_NUM + 1];
    const char* new_cid_num_ptr = new_cid_num;
    char plus_prefix[MAX_NUM_OF_PHONE_NUM + 1];
    int plus_prefix_len;

    /* get serving cell mcc */
    network_mcc = (int)ezrdb_get_int(RDB_NETWORK_MCC);

    struct mcc_country_calling_code_t* mccc_ptr;

    mccc_ptr = _mccc;
    while (mccc_ptr->mcc_start) {

        network_matched = network_mcc >= mccc_ptr->mcc_start && network_mcc <= mccc_ptr->mcc_end;
        if (network_matched) {

            syslog(LOG_DEBUG, "network mcc matched (network_mcc=%d,mcc_start=%d,mcc_end=%d", network_mcc, mccc_ptr->mcc_start,
                   mccc_ptr->mcc_end);

            /* get plus prefix for local calls */
            plus_prefix_len = snprintf(plus_prefix, sizeof(plus_prefix), "+%d", mccc_ptr->local_call_prefix);

            /* if it is a local call */
            if ((plus_prefix_len > 0) && !strncmp(cid_num, plus_prefix, plus_prefix_len)) {
                new_cid_num_ptr = cid_num + plus_prefix_len;

                syslog(LOG_DEBUG, "local call (cid_num=%s,new_cid_num=%s)", cid_num, new_cid_num_ptr);
                /* if it is an international call */
            } else if (*cid_num == '+') {
                snprintf(new_cid_num, sizeof(new_cid_num), "%s%s", mccc_ptr->international_call_prefix, cid_num + 1);

                syslog(LOG_DEBUG, "international call (cid_num=%s,new_cid_num=%s)", cid_num, new_cid_num_ptr);

                /* everything else */
            } else {
                new_cid_num_ptr = cid_num;

                syslog(LOG_DEBUG, "plus(+) prefix number not found (cid_num=%s,new_cid_num=%s)", cid_num, new_cid_num_ptr);
            }

            break;
        }

        mccc_ptr++;
    }

    if (!network_matched) {

        syslog(LOG_DEBUG, "no network information for incoming cid (network_mcc=%d)", network_mcc);

        /* if num starts with plus(+), remove the plus. */
        if (*cid_num == '+') {
            new_cid_num_ptr = cid_num + 1;
            syslog(LOG_DEBUG, "remove plus(+) prefix number (cid_num=%s,new_cid_num=%s)", cid_num, new_cid_num_ptr);
        } else {
            new_cid_num_ptr = cid_num;

            syslog(LOG_DEBUG, "plus(+) prefix number not found (cid_num=%s,new_cid_num=%s)", cid_num, new_cid_num_ptr);
        }
    }

    return new_cid_num_ptr;
}
