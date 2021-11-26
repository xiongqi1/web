#ifndef __IIO_H__28112015
#define __IIO_H__28112015

/*
 * Industrial IO support
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

#include <sys/select.h>

/* maximum iio device counts in an iio */
#define IIO_DEVICE_CHANNEL_MAX	10

/* iio sysfs directory information */
#define IIO_DIR_DEVICE_DIR	"/sys/bus/iio/devices"
#define IIO_DIR_DEVICE_FILES	"iio:device[0-9]*"

/* iio sysfs entry names */
#define IIO_DIR_UEVENT		"uevent"
#define IIO_DIR_NAME		"name"
#define IIO_DIR_MODE		"mode"
#define IIO_DIR_BUFFER_LEN	"buffer/length"
#define IIO_DIR_BUFFER_EN	"buffer/enable"
#define IIO_DIR_SCAN_ELEMENTS	"scan_elements"

/* maximum content length of each sysfs entry */
#define IIO_MAX_SYSFS_CONTENT_LEN 64

/* kernel device buffer length - 4 second buffer for 6 channels */
#define IIO_DEV_BUFFER_LEN	(1000*4*6)

void on_iio_dev(void * iio_dev,int ch_index,unsigned int* ch_samples,int sample_count,unsigned long long ms64);

void iio_set_config_integer(const char *name, int value);
void iio_destroy();
int iio_create(void);
int iio_set_output(const struct io_info_t * io ,int val);
int iio_set_mode(const struct io_info_t * io ,int val);

void fdsset_iio(fd_set * fds);
void process_iio(fd_set * fds);
int get_iio_maxhandle();

#endif
