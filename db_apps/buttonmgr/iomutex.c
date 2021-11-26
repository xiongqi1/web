/*
 * IO mutex.
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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

#include "iomutex.h"
#include "lstddef.h"

#include <linux/stddef.h>
#include <linux/limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

/**
 * @brief destroys an object of iomutex_t.
 *
 * @param m is an object of iomutex.
 */
void iomutex_destroy(struct iomutex_t* m)
{
    if (!m) {
        return;
    }

    /* close lock file */
    if (!(m->fd < 0)) {
        close(m->fd);
    }

    free(m);
}

/**
 * @brief enters or leaves iomutex.
 *
 * @param m is an object of iomutex.
 * @param enter is a flag to select 'enter(1)' or 'leave(0)'.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int iomutex_enter_or_leave(struct iomutex_t* m, int enter)
{
    if (m->fd < 0) {
        goto err;
    }

    return lockf(m->fd, enter ? F_LOCK : F_ULOCK, 0);

err:
    return -1;
}

/**
 * @brief enters iomutex.
 *
 * @param m is an object of iomutex.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int iomutex_enter(struct iomutex_t* m)
{
    return iomutex_enter_or_leave(m, TRUE);
}

/**
 * @brief leaves iomutex.
 *
 * @param m is an object of iomutex.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int iomutex_leave(struct iomutex_t* m)
{
    return iomutex_enter_or_leave(m, FALSE);
}

/**
 * @brief creates an object of iomutex.
 *
 * @param name is a lock file name in /tmp directory.
 *
 * @return an object of iomutex.
 */
struct iomutex_t* iomutex_create(const char* name)
{
    struct iomutex_t* m;
    char fname[PATH_MAX];
    int fd;

    /* allocate memory */
    m = calloc(1, sizeof(*m));
    if (!m) {
        goto err;
    }

    /* initiate members */
    m->fd = -1;

    /* open lock file */
    snprintf(fname, sizeof(fname), "/tmp/.ipc-mutex-%s", name);
    fd = open(fname, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        goto err;
    }

    return m;

err:
    iomutex_destroy(m);
    return NULL;
}
