#ifndef __EVDEV_H_20181024__
#define __EVDEV_H_20181024__

/*
 * EVDev reader.
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

#include <linux/input.h>

struct evdev_t;

typedef void (*evdev_event_callback_t)(struct evdev_t* evdev, int key_type, int key_code, int depressed);

struct evdev_t {
    int fd;
    char* dev_name;
    evdev_event_callback_t event_cb;

    unsigned char* key_bitmap_cur[EV_CNT];

    int key_bitmap_new_valid[EV_CNT];
    unsigned char* key_bitmap_new[EV_CNT];
};


void evdev_update_key_bitmap(struct evdev_t *evdev);
void evdev_set_fdset(struct evdev_t *evdev, int *max_fd, fd_set *fdsetp);
int evdev_process(struct evdev_t *evdev, fd_set *fdsetp);
void evdev_destroy(struct evdev_t *evdev);
struct evdev_t *evdev_create(const char *dev_name, evdev_event_callback_t event_cb);
void evdev_collection_set_fdset(int *max_fd, fd_set *fdsetp);
void evdev_collection_process(fd_set *fdsetp);
void evdev_collection_fini(void);
int evdev_collection_init(evdev_event_callback_t event_cb);
void evdev_collection_update_key_bitmap(void);
int evdev_set_key_flag_without_callback(struct evdev_t* evdev, int key_type, int key_code, int flag);
int evdev_get_key_flag(struct evdev_t* evdev, int key_type, int key_code);

#endif
