/*
 * serial_support.c:
 *    Support functions for serial port communication over bluetooth.
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#include "bt_sensord.h"

/*
 * Get the info for an rfcomm device.
 *
 * Parameters:
 *    sock        [in] The rfcomm socket bound to the remote BT device.
 *    devid       [in] The rfcomm device number.
 *    dev_info    [out] The rfcomm device info.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
static int
rfcomm_get_dev_info(int sock, int devid, struct rfcomm_dev_info *dev_info)
{
    struct rfcomm_dev_info di = { .id = devid };
    if (ioctl(sock, RFCOMMGETDEVINFO, &di) < 0) {
        return -1;
    }

    memcpy(dev_info, &di, sizeof(di));

    return 0;
}

/*
 * Checks whether an rfcomm channel is already bound to a device.
 *
 * Parameters:
 *    sock        [in] The rfcomm socket to be checked.
 *    devid       [in] The rfcomm device number.
 *
 * Returns:
 *    1 if bound and 0 if not bound.
 */
static int
rfcomm_is_dev_bound (int sock, int devid)
{
    struct rfcomm_dev_info dev_info;
    int ret = rfcomm_get_dev_info(sock, devid, &dev_info);

    return (ret == 0);
}

/*
 * Release an rfcomm channel.
 *
 * Parameters:
 *    sock        [in] The rfcomm socket to be released.
 *    devid       [in] The rfcomm device number.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
static
int rfcomm_release_dev (int sock, int dev)
{
        struct rfcomm_dev_req req;
        int err;

        memset(&req, 0, sizeof(req));
        req.dev_id = dev;

        err = ioctl(sock, RFCOMMRELEASEDEV, &req);
        if (err < 0) {
            perror("Can't release device");
        }

        return err;
}

/*
 * Bind an rfcomm channel.
 *
 * Parameters:
 *    sock        [in] The rfcomm socket to be bound.
 *    devid       [in] The rfcomm device number.
 *    flags       [in] Bind flags.
 *    src         [in] Source device address (e.g. local adapter).
 *    dst         [in] Destination device address (e.g. sensor device).
 *    channel     [in] Channel number to bind to.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
int
rfcomm_bind_dev (int sock, int dev, uint32_t flags, bdaddr_t *src,
                 bdaddr_t *dst, int channel)
{
	struct rfcomm_dev_req req;
	int err;

    if (rfcomm_is_dev_bound(sock, dev)) {
        /*
         * If the dev is already bound then release it first in case it
         * was previously used for a different destination.
         */
        dbgp("Release rfcomm%d\n", dev);
        rfcomm_release_dev(sock, dev);
    }

    memset(&req, 0, sizeof(req));
    req.dev_id = dev;
    req.flags = flags;
    bacpy(&req.src, src);
    bacpy(&req.dst, dst);

    if (channel > 0) {
        req.channel = channel;
    } else {
        req.channel = 1;
    }

    dbgp("Creating rfcomm%d....", dev);
    err = ioctl(sock, RFCOMMCREATEDEV, &req);
    dbgp("done\n");
    if (err == -1) {
        err = -errno;

        if (err == -EOPNOTSUPP) {
            fprintf(stderr, "RFCOMM TTY support not available\n");
        } else {
            perror("Can't create device");
        }
    }

    return err;
}

/*
 * Set serial interace attributes.
 *
 * Parameters:
 *    fd         [in] File descriptor for opened rfcomm device connected to the
 *               td2551.
 *    speed      [in] The serial baud rate to set.
 *    parity     [in] The parity mode to set.
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
int
set_interface_attribs (int fd, int speed, int parity)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0) {
        printf ("error %d from tcgetattr", errno);
        return -1;
    }

    tty.c_cflag = tty.c_iflag = tty.c_lflag;

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CREAD);// ignore modem controls,
    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr (fd, TCSANOW, &tty) != 0) {
        printf ("error %d from tcsetattr", errno);
        return -1;
    }
    return 0;
}
