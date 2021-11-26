/**
 * @file com_interface.h
 * @brief legacy OWA backward compatible communication interface
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY Casa Systems ``AS IS''
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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "com_interface.h"
#include "nc_util.h"

static int fifo_fd_r, fifo_fd_w;

int com_channels_init()
{
    int fd;
    if ((access(MANAGER_FIFO_R, F_OK)==-1) &&
        (mkfifo(MANAGER_FIFO_R, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)==-1)) {
        BLOG_ERR("create read fifo\n");
        return -1;
    }

    if ((fd = open(MANAGER_FIFO_R, O_RDONLY))== -1) {
        BLOG_ERR("read read fifo\n");
        return -1;
    }
    fifo_fd_r = fd;

    if ((access(MANAGER_FIFO_W, F_OK)==-1) &&
        (mkfifo(MANAGER_FIFO_W, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)==-1)) {
        BLOG_ERR("create write fifo\n");
        return -1;
    }
    if ((fd = open(MANAGER_FIFO_W, O_WRONLY, O_NONBLOCK))== -1) {
        BLOG_ERR("open write fifo\n");
        return -1;
    }
    fifo_fd_w = fd;
    return 0;
}

void com_channels_close()
{
    close(fifo_fd_r);
    close(fifo_fd_w);
}

int com_get_read_fd(void)
{
    return fifo_fd_r;
}

int com_get_write_fd(void)
{
    return fifo_fd_w;
}

int read_message(char * msg)
{
    char buf[MAX_MANAGEMENT_PACKET_BUFF_SIZE];
    unsigned short msg_len =0;
    ssize_t read_bytes, temp_size=0;

    // read the first 2 bytes of message length
    read_bytes = MSG_LEN_BYTES;
    while (read_bytes){
        temp_size = read(fifo_fd_r, buf, read_bytes);
        if (temp_size <= 0) {  // any error without real read out
            BLOG_ERR("read message from pipe error\n");
            return (int)temp_size;
        }
        read_bytes = read_bytes - temp_size;
    }
    msg_len = (buf[0]<<8 & 0xff00)|buf[1];
    read_bytes = msg_len;
    while (read_bytes){
        temp_size = read(fifo_fd_r, buf, read_bytes);
        if (temp_size <= 0) {
            BLOG_ERR("read message from pipe error\n");
            return (int)temp_size;
        }
        read_bytes -= temp_size;
    }
    memcpy(msg, buf, msg_len);
    return msg_len;
}

int write_message(mgmt_packet_t * packet)
{
    char buf[MAX_MANAGEMENT_PACKET_BUFF_SIZE+MSG_LEN_BYTES];
    unsigned short msg_len;
    int write_bytes, temp_size=0;

    msg_len = htons(packet->len);
    memcpy(buf, &msg_len, sizeof(msg_len));
    memcpy(buf+MSG_LEN_BYTES, packet->tlv_pdus, packet->len);

    write_bytes = packet->len+MSG_LEN_BYTES;
    while (write_bytes){
        temp_size = write(fifo_fd_w, buf, packet->len+MSG_LEN_BYTES);
        if(temp_size <=0) {
            BLOG_ERR("Writing pipe problem");
            return -1;
        }
        write_bytes -= temp_size;
    }
    return packet->len+MSG_LEN_BYTES;
}
