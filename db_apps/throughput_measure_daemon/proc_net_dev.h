#ifndef PROC_NET_DEV_H_02052018
#define PROC_NET_DEV_H_02052018

/*!
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

#include <stdint.h>

#define READ_BUF_SIZE (8*1024)
#define PROC_NET_DEV_FNAME "/proc/net/dev"

struct proc_net_dev_t {
    char* read_buf;

    int traffic_valid;

    uint64_t recv_bytes;
    uint64_t sent_bytes;

    uint64_t recv_bytes_diff;
    uint64_t sent_bytes_diff;

};

int proc_net_dev_get_bytes(struct proc_net_dev_t *pnd, const char *netif, uint64_t *recv_bytes, uint64_t *sent_bytes);
int proc_net_dev_read(struct proc_net_dev_t *pnd);
void proc_net_dev_fini(struct proc_net_dev_t *pnd);
int proc_net_dev_init(struct proc_net_dev_t *pnd);

#endif

