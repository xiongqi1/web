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

#include "evdev.h"
#include "lstddef.h"

#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

/* ioctl buffer length for EVIOCGNAME */
#define EVIOCGNAME_LEN 256

/* maximum read count without yielding */
#define MAX_EVDEV_READ_COUNT 10

/* input device directory */
#define DEV_INPUT_DIR "/dev/input"

/* maximum number of input devices */
#define MAX_INPUT_DEVICE 10

struct evdev_key_type_t {
    int valid;
    int num_of_key;
    int sizeof_key_bitmap;
    int eviocgkey_cmd;
};

#define EVDEV_KEY_TYPE_ENTITY(key_type,key_count,eviocgkey_cmd) \
    [key_type]={1,(key_count),((key_count)+7)/8,eviocgkey_cmd}

static const struct evdev_key_type_t _evdev_key_type[EV_CNT] = {
    EVDEV_KEY_TYPE_ENTITY(EV_KEY, KEY_CNT, EVIOCGKEY(0)),
    EVDEV_KEY_TYPE_ENTITY(EV_SW, SW_CNT, EVIOCGSW(0)), { 0, },
};

#define KEY_CODE_OFFSET(key_code) ((key_code)/8)
#define KEY_CODE_MASK(key_cde) (1<<((key_code)%8))

/* collection of ev devices */
static struct evdev_t* _evdev_collection[MAX_INPUT_DEVICE] = { 0, };

/**
 * @brief updates a bit in key bitmap.
 *
 * @param evdev is an evdev object.
 * @param key_type is key type.
 * @param key_code is key code.
 * @param flag is a depressed flag (0=released, 1=depressed)
 * @param cb is a flag to decide whether to call callback or not. (false=do not call, true=call)
 *
 * @return 0 when it succeeds. otherwise, -1.
 */
static int evdev_set_key_flag_internal(struct evdev_t* evdev, int key_type, int key_code, int flag, int cb)
{
    int offset;
    int mask;

    int cur;
    int new;

    if (!evdev->key_bitmap_cur[key_type]) {
        syslog(LOG_ERR, "key type not configured to use (key_type=%d)", key_type);
        goto err;
    }

    /* get offset and mask */
    offset = KEY_CODE_OFFSET(key_code);
    mask = KEY_CODE_MASK(key_code);

    /* get previous and current flags */
    cur = (evdev->key_bitmap_cur[key_type][offset] & mask) != 0;
    new = flag != 0;

    if (new) {
        evdev->key_bitmap_cur[key_type][offset] |= mask;
    } else {
        evdev->key_bitmap_cur[key_type][offset] &= ~mask;
    }

    if (cb) {
        /* call event callback if changed */
        if ((cur && !new) || (!cur && new)) {
            syslog(LOG_DEBUG, "key %s (type=%d,code=%d)", !new ? "released" : "depressed", key_type, key_code);
            evdev->event_cb(evdev, key_type, key_code, new);
        }
    }

    return 0;

err:
    return -1;
}

/**
 * @brief updates a bit in key bitmap and call update call-back.
 *
 * @param evdev is an evdev object.
 * @param key_type is key type.
 * @param key_code is key code.
 * @param flag is a depressed flag (0=released, 1=depressed)
 *
 * @return 0 when it succeeds. otherwise, -1.
 */
int evdev_set_key_flag(struct evdev_t* evdev, int key_type, int key_code, int flag)
{
    return evdev_set_key_flag_internal(evdev, key_type, key_code, flag, TRUE);
}

/**
 * @brief updates a bit in key bitmap without calling call-back.
 *
 * @param evdev is an evdev object.
 * @param key_type is key type.
 * @param key_code is key code.
 * @param flag is a depressed flag (0=released, 1=depressed)
 * @param cb is a flag to decide whether to call callback or not. (false=do not call, true=call)
 *
 * @return 0 when it succeeds. otherwise, -1.
 */
int evdev_set_key_flag_without_callback(struct evdev_t* evdev, int key_type, int key_code, int flag)
{
    return evdev_set_key_flag_internal(evdev, key_type, key_code, flag, FALSE);
}

/**
 * @brief reads key bitmaps from evdev.
 *
 * @param evdev is an evdev object.
 */
void evdev_read_key_bitmap_from_dev(struct evdev_t* evdev)
{
    int eviocg;
    int key_type;
    const struct evdev_key_type_t* evdev_key_type;
    unsigned char* key_bitmap;

    syslog(LOG_DEBUG, "update key bitmap (dev_name=%s)", evdev->dev_name);

    for (key_type = 0; key_type < COUNTOF(evdev->key_bitmap_new); key_type++) {
        key_bitmap = evdev->key_bitmap_new[key_type];
        if (!key_bitmap) {
            continue;
        }

        /* read key bitmap */
        evdev_key_type = &_evdev_key_type[key_type];

        syslog(LOG_DEBUG, "read key bitmap (evdev=%p,key_type=%d,num_of_key=%d,sizeof_key_bitmap=%d,key_bitmap=%p)",
               evdev, key_type, evdev_key_type->num_of_key, evdev_key_type->sizeof_key_bitmap, key_bitmap);

        eviocg = evdev_key_type->eviocgkey_cmd | evdev_key_type->sizeof_key_bitmap << _IOC_SIZESHIFT;
        if (ioctl(evdev->fd, eviocg, evdev->key_bitmap_new[key_type]) < 0) {
            syslog(LOG_ERR, "failed to get key bitmap (type=%d,err=%d,strerror='%s')", key_type, errno,
                   strerror(errno));
            evdev->key_bitmap_new_valid[key_type] = FALSE;
        } else {
            evdev->key_bitmap_new_valid[key_type] = TRUE;
        }
    }
}

/**
 * @brief reads key bitmaps from evdev and get key state.
 *
 * @param evdev is an evdev object.
 * @param key_type is key type.
 * @param key_code is key code.
 *
 * @return key state.
 */
int evdev_get_key_flag(struct evdev_t* evdev, int key_type, int key_code)
{

    int offset;
    int mask;
    int flag;

    evdev_read_key_bitmap_from_dev(evdev);

    offset = KEY_CODE_OFFSET(key_code);
    mask = KEY_CODE_MASK(key_code);
    flag = evdev->key_bitmap_new[key_type][offset] & mask;

    return flag;
}

/**
 * @brief reads key bitmaps from evdev and call each of update call-back.
 *
 * @param evdev is an evdev object.
 */
void evdev_update_key_bitmap(struct evdev_t* evdev)
{
    int key_type;
    int key_code;

    int offset;
    int mask;
    int flag;

    const struct evdev_key_type_t* evdev_key_type;

    evdev_read_key_bitmap_from_dev(evdev);

    for (key_type = 0; key_type < COUNTOF(evdev->key_bitmap_new); key_type++) {

        if (!evdev->key_bitmap_new_valid[key_type]) {
            continue;
        }

        /* read key bitmap */
        evdev_key_type = &_evdev_key_type[key_type];

        /* apply key bitmap */
        for (key_code = 0; key_code < evdev_key_type->num_of_key; key_code++) {
            offset = KEY_CODE_OFFSET(key_code);
            mask = KEY_CODE_MASK(key_code);
            flag = evdev->key_bitmap_new[key_type][offset] & mask;

            evdev_set_key_flag(evdev, key_type, key_code, flag);
        }
    }
}

/**
 * @brief remove all events from buffer.
 *
 * @param evdev is an evdev object.
 */
static void evdev_waste_input_event(struct evdev_t* evdev)
{
    int len;
    struct input_event input_event;

    do {
        len = read(evdev->fd, &input_event, sizeof(input_event));
    } while (len > 0);
}

/**
 * @brief reads events from evdev.
 *
 * @param evdev is an evdev object.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int evdev_read_input_event(struct evdev_t* evdev)
{
    struct input_event input_event;
    int len;
    int read_cnt;

    const struct evdev_key_type_t* evdev_key_type;

    read_cnt = 0;

    while (read_cnt++ < MAX_EVDEV_READ_COUNT) {
        /* read input event */
        len = read(evdev->fd, &input_event, sizeof(input_event));
        if (len < 0) {
            if (errno == EAGAIN) {
                syslog(LOG_DEBUG, "retry to read evdev (dev=%s,err='%s')", evdev->dev_name, strerror(errno));
                break;
            }
            syslog(LOG_ERR, "failed to read evdev (dev=%s,err='%s')", evdev->dev_name, strerror(errno));
            goto err;
        }

        /* check range */
        if (!(input_event.type < COUNTOF(_evdev_key_type))) {
            syslog(LOG_ERR, "out range of key type (input_event.type=%d)", input_event.type);
            continue;
        }

        evdev_key_type = &_evdev_key_type[input_event.type];

        /* bypass if not valid key type */
        if (!evdev_key_type->valid) {
            continue;
        }

        /* break if there is no more data */
        if (!len) {
            break;
        }

        /* resynchroise if a broken packet is read */
        if (len != sizeof(input_event)) {
            syslog(LOG_ERR, "incorrect input event size!!!");

            evdev_waste_input_event(evdev);
            evdev_update_key_bitmap(evdev);

            continue;
        }

        syslog(LOG_DEBUG, "event detected (type=%d,code=%d,value=%d)", input_event.type, input_event.code,
               input_event.value);
        evdev_set_key_flag(evdev, input_event.type, input_event.code, input_event.value);
    }

    return 0;

err:
    return -1;
}

/**
 * @brief configures FDSET.
 *
 * @param evdev is an evdev object.
 * @param max_fd is a pointer to max_fd.
 * @param fdsetp is a pionter to fdset.
 */
void evdev_set_fdset(struct evdev_t* evdev, int* max_fd, fd_set* fdsetp)
{
    FD_SET(evdev->fd, fdsetp);

    if (evdev->fd > *max_fd) {
        *max_fd = evdev->fd;
    }
}

/**
 * @brief reads input events and processes if received.
 *
 * @param evdev is an evdev object.
 * @param fdsetp is a pointer to fdset.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int evdev_process(struct evdev_t* evdev, fd_set* fdsetp)
{
    if (!FD_ISSET(evdev->fd, fdsetp)) {
        goto err;
    }

    return evdev_read_input_event(evdev);

err:
    return -1;
}

/**
 * @brief destroys an evdev object.
 *
 * @param evdev is an evdev object.
 */
void evdev_destroy(struct evdev_t* evdev)
{
    int i;

    if (!evdev) {
        return;
    }

    if (evdev->fd >= 0) {
        close(evdev->fd);
    }

    free(evdev->dev_name);

    for (i = 0; i < COUNTOF(evdev->key_bitmap_cur); i++) {
        free(evdev->key_bitmap_cur[i]);
        free(evdev->key_bitmap_new[i]);
    }
    free(evdev);
}

/**
 * @brief creates an evdev object.
 *
 * @param dev_name is dev name.
 * @param event_cb is a call-back function.
 *
 * @return
 */
struct evdev_t* evdev_create(const char* dev_name, evdev_event_callback_t event_cb)
{
    struct evdev_t* evdev;
    int i;

    char dev_name_from_drv[EVIOCGNAME_LEN];

    const struct evdev_key_type_t* evdev_key_type;

    /* allocate object memory */
    evdev = (struct evdev_t*) calloc(1, sizeof(*evdev));
    if (!evdev) {
        syslog(LOG_ERR, "failed to allocate memory for evdev");
        goto err;
    }

    /* initiate fd */
    evdev->fd = -1;

    /* duplicate dev name */
    evdev->dev_name = strdup(dev_name);
    if (!evdev->dev_name) {
        syslog(LOG_ERR, "failed to allocate memory for evdev dev name");
        goto err;
    }

    /* allocate key bitmap */
    for (i = 0; i < COUNTOF(_evdev_key_type); i++) {
        evdev_key_type = &_evdev_key_type[i];
        if (evdev_key_type->valid) {

            evdev->key_bitmap_cur[i] = (unsigned char*) calloc(1, evdev_key_type->sizeof_key_bitmap);
            evdev->key_bitmap_new[i] = (unsigned char*) calloc(1, evdev_key_type->sizeof_key_bitmap);

            syslog(LOG_DEBUG, "add key type (evdev=%p,key_type=%d,key_bitmap_new=%p)", evdev, i,
                   evdev->key_bitmap_new[i]);

            if (!evdev->key_bitmap_cur[i] || !evdev->key_bitmap_new[i]) {
                syslog(LOG_ERR, "failed to allocate bitmap buffer");
                goto err;
            }
        }
    }

    /* open device */
    evdev->fd = open(dev_name, O_RDONLY | O_NONBLOCK);
    if (evdev->fd < 0) {
        syslog(LOG_ERR, "failed to open evdev (dev_name=%s,err=%d,strerror='%s'", dev_name, errno, strerror(errno));
        goto err;
    }

    /* make sure we are talking to EV dev */
    if (ioctl(evdev->fd, EVIOCGNAME(sizeof(dev_name_from_drv)), dev_name_from_drv) < 0) {
        syslog(LOG_ERR, "failed to access evdev (dev_name=%s,err=%d,strerror='%s'", dev_name, errno, strerror(errno));
        goto err;
    }

    /* update members */
    evdev->event_cb = event_cb;

    return evdev;

err:
    evdev_destroy(evdev);
    return NULL;
}

/**
 * @brief sets FDSET accordingly to all of evdev objects.
 *
 * @param max_fd is a pointer to max_fd
 * @param fdsetp is a pointer to fdset.
 */
void evdev_collection_set_fdset(int *max_fd, fd_set *fdsetp)
{
    int i;

    struct evdev_t* evdev;

    for (i = 0; i < COUNTOF(_evdev_collection); i++) {
        evdev = _evdev_collection[i];
        if (evdev) {
            evdev_set_fdset(evdev, max_fd, fdsetp);
        }
    }
}

/**
 * @brief reads and processes all of evdev objects.
 *
 * @param fdsetp is a pointer to fdset.
 */
void evdev_collection_process(fd_set* fdsetp)
{
    int i;

    struct evdev_t* evdev;

    for (i = 0; i < COUNTOF(_evdev_collection); i++) {
        evdev = _evdev_collection[i];
        if (evdev) {
            evdev_process(evdev, fdsetp);
        }
    }
}

/**
 * @brief destroys all evdev objects.
 */
void evdev_collection_fini(void)
{
    int i;

    struct evdev_t* evdev;

    for (i = 0; i < COUNTOF(_evdev_collection); i++) {
        evdev = _evdev_collection[i];
        evdev_destroy(evdev);

        _evdev_collection[i] = NULL;
    }
}

/**
 * @brief updates key bitmaps of all evdev objects.
 */
void evdev_collection_update_key_bitmap(void)
{
    int i;

    struct evdev_t* evdev;

    for (i = 0; i < COUNTOF(_evdev_collection); i++) {
        evdev = _evdev_collection[i];
        if (evdev) {
            evdev_update_key_bitmap(evdev);
        }
    }
}

/**
 * @brief creates all evdev objects.
 *
 * @param event_cb is a callback when an event is received.
 *
 * @return 0 when succeeds. Otherwise, -1.
 */
int evdev_collection_init(evdev_event_callback_t event_cb)
{
    DIR* dir;
    struct dirent* de;
    struct stat stbuf;
    int i;
    char fname[PATH_MAX];

    syslog(LOG_DEBUG, "initiate evdev collection");
    /* open input directory */
    dir = opendir(DEV_INPUT_DIR);
    if (!dir) {
        syslog(LOG_ERR, "failed to open dev input directory (dir=%s)", DEV_INPUT_DIR);
        goto err;
    }

    i = 0;

    de = readdir(dir);
    while (de && (i < MAX_INPUT_DEVICE)) {

        /* get full path */
        snprintf(fname, sizeof(fname), "%s/%s", DEV_INPUT_DIR, de->d_name);

        syslog(LOG_DEBUG, "input event device found (fname=%s)", fname);

        /* if not a directory */
        if (!stat(fname, &stbuf) && !S_ISDIR(stbuf.st_mode)) {
            _evdev_collection[i] = evdev_create(fname, event_cb);
            if (!_evdev_collection[i]) {
                syslog(LOG_DEBUG, "failed to create evdev (i=%d,dev=%s)", i, fname);
            } else {
                syslog(LOG_DEBUG, "evdev sucessfully created (i=%d,dev=%s)", i, fname);
                i++;
            }
        }

        de = readdir(dir);
    }

    return 0;

err:
    return -1;
}
