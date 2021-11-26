/*!
 * This module reads and tracks /proc/net/dev.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include "proc_net_dev.h"

#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/**
 * @brief Reads received and sent bytes from proc/net/dev content that exists in memory.
 *
 * @param pnd is a proc/net/dev object.
 * @param netif is a network interface name to read received and sent bytes.
 * @param recv_bytes is a pointer to get received bytes.
 * @param sent_bytes is a pointer to get sent bytes.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int proc_net_dev_get_bytes(struct proc_net_dev_t* pnd, const char* netif, uint64_t* recv_bytes, uint64_t* sent_bytes)
{
    char netif_str[1 + IFNAMSIZ + 1 ]; /* LF + IFNAMSIZ + : */
    char* p;
    int sscan_cnt;


    /* build netif string to search */
    snprintf(netif_str, sizeof(netif_str), "\n%s:", netif);

    /* search netif string */
    p = strstr(pnd->read_buf, netif_str);
    if (!p) {
        goto err;
    }

    /*
    	* stat file example
    	*
    	Inter-|   Receive                                                |  Transmit
    	face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    	eth0.35:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	sit0:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	lo:   14987     239    0    0    0     0          0         0    14987     239    0    0    0     0       0          0
    	rmnet_data7:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	rmnet_data6:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	rmnet_data5:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	rmnet_data4:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	rmnet_data3:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	rmnet_data2:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	rmnet_data1:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	rmnet_data0: 3438760524 2461837    0    0    0     0          0         0 31787792  566834    0    0    0     0       0          0
    	eth0:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
    	rmnet_ipa0: 3448607872 2461837    0    0    0     0          0         0 31787792  566834    0    0    0     0       0          0
    */

    /* skip LF */
    p++;

    /* parse line */
    sscan_cnt = sscanf(p, "%*s %llu %*s %*s %*s %*s %*s %*s %*s %llu %*s %*s %*s %*s %*s %*s %*s", recv_bytes, sent_bytes);
    if (sscan_cnt != 2) {
        goto err;
    }

    return 0;

err:
    return -1;
}

/**
 * @brief Reads net/proc/net into memory.
 *
 * @param pnd is a proc/net/dev object.
 *
 * @return
 */
int proc_net_dev_read(struct proc_net_dev_t* pnd)
{
    int fd = -1;
    int read_len;

    /* check to see if buffer is allocated */
    if (!pnd->read_buf) {
        syslog(LOG_ERR, "read buffer not allocated yet");
        goto err;
    }

    /* open dev */
    fd = open(PROC_NET_DEV_FNAME, O_RDONLY);
    if (fd < 0) {
        syslog(LOG_ERR, "failed to open %s - %s", PROC_NET_DEV_FNAME, strerror(errno));
        goto err;
    }

    /* read dev */
    read_len = read(fd, pnd->read_buf, READ_BUF_SIZE - 1);
    if (read_len < 0) {
        syslog(LOG_ERR, "failed to read %s - %s", PROC_NET_DEV_FNAME, strerror(errno));
        goto err;
    }

    pnd->read_buf[read_len] = 0;

    close(fd);

    return 0;

err:
    if(fd>=0) {
        close(fd);
    }

    *pnd->read_buf=0;

    return -1;
}

/**
 * @brief Finalizes a proc/net/dev object to destory.
 *
 * @param pnd is a proc/net/dev object.
 */
void proc_net_dev_fini(struct proc_net_dev_t* pnd)
{
    /* free read buffer */
    free(pnd->read_buf);
}

/**
 * @brief Initiate a proc/net/dev object to use.
 *
 * @param pnd is a proc/net/dev object.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int proc_net_dev_init(struct proc_net_dev_t* pnd)
{
    /* reset members */
    memset(pnd, 0, sizeof(*pnd));

    /* allocate read buffer */
    pnd->read_buf = malloc(READ_BUF_SIZE);
    if (!pnd->read_buf) {
        syslog(LOG_ERR, "failed to allocate read buffer - %s", strerror(errno));
        goto err;
    }

    return 0;

err:
    proc_net_dev_fini(pnd);
    return -1;
}



