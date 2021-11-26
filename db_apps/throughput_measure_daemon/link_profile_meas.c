/*
 * Link profile measurement reads received and sent bytes for each link.profile internface
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

#include "link_profile_meas.h"
#include "proc_net_dev.h"
#include "rdb.h"
#include "tick_clock.h"

#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

static struct proc_net_dev_t _pnd = {0,};
static struct link_profile_meas_t _lpm[LINK_PROFILE_COUNT];

static int _last_update_valid = 0;
static time_ms_t _last_update_ms;
static int _sockfd = -1;

/**
 * @brief Reads received and sent bytes into a link.profile measurement object.
 *
 * @param lpm is a link.profile measurement object.
 */
void link_profile_meas_read(struct link_profile_meas_t* lpm)
{
    const char* netif_rdb;
    char netif[IFNAMSIZ];

    uint64_t recv_bytes = lpm->last_recv_bytes;
    uint64_t sent_bytes = lpm->last_sent_bytes;

    int dev_read = -1;
    int valid = 0;

    int pf_rdb_idx = lpm->profile_no + 1;
    struct ifreq ifr;

    /* read profile interface */
    netif_rdb = rdb_get_printf(LINK_POLICY_PREFIX".%d."LINK_POLICY_IFACE, pf_rdb_idx);
    strncpy(netif, netif_rdb, sizeof(netif));
    netif[IFNAMSIZ - 1] = 0;

    /* read recv bytes and sent bytes */
    if (*netif) {
        dev_read = proc_net_dev_get_bytes(&_pnd, netif, &recv_bytes, &sent_bytes);
        valid = *netif && !(dev_read < 0);
    }

    /* update diff */
    lpm->diff_valid = lpm->last_valid && valid;
    lpm->diff_recv_bytes = recv_bytes - lpm->last_recv_bytes;
    lpm->diff_sent_bytes = sent_bytes - lpm->last_sent_bytes;

    /* update last values */
    lpm->last_valid = valid;
    lpm->last_recv_bytes = recv_bytes;
    lpm->last_sent_bytes = sent_bytes;

    /* get interface running stat */
    strncpy(ifr.ifr_name, netif, sizeof(ifr.ifr_name));
    /* get network interface running flag */
    if (!valid || (ioctl(_sockfd, SIOCGIFFLAGS, &ifr) < 0)) {
        ifr.ifr_flags = 0;
    }

    lpm->if_running = (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);

}

/**
 * @brief Resets a link.profile measurement object to invalidate.
 *
 * @param lpm is a link.profile measurement object.
 */
static void link_profile_meas_reset(struct link_profile_meas_t* lpm)
{
    lpm->last_valid = 0;
    lpm->diff_valid = 0;
}

/**
 * @brief Finalize a link.profile measurement object to destory.
 *
 * @param lpm is a link.profile measurement object.
 */
void link_profile_meas_fini(struct link_profile_meas_t* lpm)
{
}

/**
 * @brief Initiates a link.profile measurement object to use.
 *
 * @param profile_no is a zero-based profile number.
 * @param lpm is a link.profile measurement object.
 *
 * @return
 */
static int link_profile_meas_init(int profile_no, struct link_profile_meas_t* lpm)
{
    link_profile_meas_reset(lpm);

    lpm->profile_no = profile_no;

    return 0;
}

/**
 * @brief Returns the corresponding link.profile measurement object to a profile number.
 *
 * @param profile_no is a zero-based profile number.
 *
 * @return
 */
struct link_profile_meas_t* link_profile_meas_collection_get(int profile_no)
{
    return &_lpm[profile_no];
}

/**
 * @brief Read received and sent bytes into all of link.profile objects in the collection.
 *
 * @param duration is a pointer to get duration in msec since the previous read.
 *
 * @return last true when the read is valid. Otherwise, false.
 */
int link_profile_meas_collection_read(time_diff_ms_t* duration)
{
    int valid = _last_update_valid;
    time_ms_t ms = tick_clock_get_ms();

    /* calculate duration */
    *duration = ms - _last_update_ms;

    /* renew update information */
    _last_update_valid = 1;
    _last_update_ms = ms;

    /* read proc_net_dev */
    proc_net_dev_read(&_pnd);

    return valid;
}

/**
 * @brief Finalize the link.profile collection to destroy.
 */
void link_profile_meas_collection_fini()
{
    int i;

    /* finish link_profile_mea objects */
    for (i = 0; i < LINK_PROFILE_COUNT; i++)
        link_profile_meas_fini(&_lpm[i]);

    /* finish proc_net_dev */
    proc_net_dev_fini(&_pnd);

    /* close socket */
    close(_sockfd);
    _sockfd = -1;
}

/**
 * @brief Initiate the link.profile collection to use.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int link_profile_meas_collection_init()
{
    int i;

    /* initiate module variables */
    memset(_lpm, 0, sizeof(_lpm));

    /* open socket */
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    /* create proc_net_dev object */
    if (proc_net_dev_init(&_pnd) < 0) {
        syslog(LOG_ERR, "failed to initiate proc_net_dev");
        goto err;
    }

    /* create link_profile_meas objects */
    for (i = 0; i < LINK_PROFILE_COUNT; i++) {
        if (link_profile_meas_init(i, &_lpm[i]) < 0) {
            syslog(LOG_ERR, "failed to initiate link_profile_meas");
            goto err;
        }
    }

    return 0;
err:
    link_profile_meas_collection_fini();
    return -1;
}
