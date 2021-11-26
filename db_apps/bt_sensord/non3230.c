/*
 * non3230.c:
 *    Nonin 3230 Pulse/Oximeter device handling.
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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <glib.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/hci.h>

#include "bt_sensord.h"
#include "non3230_priv.h"

#include "gatt/uuid.h"
#include "gatt/att.h"
#include "gatt/btio.h"
#include "gatt/gatttool.h"
#include "gatt/gattrib.h"
#include "gatt/gatt.h"
#include "gatt/util.h"

static GMainLoop *event_loop = NULL;
static GIOChannel *chan;
static GAttrib *attrib;
static nonin3230_cb_data_t cb_data;
static int connected = 0;
static bdaddr_t dst_addr;
static guint sync_timeout_gsource_id = 0;

/*
 * Opens the kernel bluetooth management interface.
 *
 * Returns:
 *    fd of management interface on success and -errno on error.
 */
static 
int mgmt_open (void)
{
    struct sockaddr_hci addr;
    int fd;

    fd = socket(PF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK,
                BTPROTO_HCI);
    if (fd < 0) {
        return -errno;
    }

    memset(&addr, 0, sizeof(addr));
    addr.hci_family = AF_BLUETOOTH;
    addr.hci_dev = HCI_DEV_NONE;
    addr.hci_channel = HCI_CHANNEL_CONTROL;

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        int err = -errno;
        close(fd);
        return err;
    }

    return fd;
}

/*
 * Creates a bluetooth management command request
 *
 * Parameters:
 *     opcode:    [in] The command opcode.
 *     index:     [in] The hci device index that the command is targetted for.
 *     length:    [in] The length in bytes of the param buffer.
 *     param:     [in] The command parameters.
 *
 * Returns:
 *    A pointer to a buffer containing the command packet or NULL on error.
 */
static void *
mgmt_create_request (unsigned short opcode, unsigned short index,
                     unsigned short length, const void *param)
{
	struct mgmt_hdr *hdr;
    void *buf;
    int buf_len;

	if (!opcode) {
		return NULL;
    }

	if (length > 0 && !param) {
		return NULL;
    }

	buf_len = length + MGMT_HDR_SIZE;
	buf = malloc(buf_len);
	if (!buf) {
		return NULL;
	}

	if (length > 0) {
		memcpy(buf + MGMT_HDR_SIZE, param, length);
    }

	hdr = buf;
	hdr->opcode = htobs(opcode);
	hdr->index = htobs(index);
	hdr->len = htobs(length);

    return buf;
}

/*
 * Unpair a bluetooth device.
 *
 * Parameters:
 *    bdaddr:    [in] Address of the deviced to unpair.
 *
 * Returns:
 *    void
 */
static int mgmt_fd;
static void
unpair_device (bdaddr_t *bdaddr)
{
    struct mgmt_cp_unpair_device cp;
    void *buf;
    
    mgmt_fd = mgmt_open();

    if (mgmt_fd < 0) {
        dbgp("Failed to open mgmt interface\n");
        return;
    }

    memset(&cp, 0, sizeof(cp));
    bacpy(&cp.addr.bdaddr, bdaddr);
	cp.addr.type = 1; // LE public
	cp.disconnect = 1;

    buf = mgmt_create_request(MGMT_OP_UNPAIR_DEVICE, 0, sizeof(cp), &cp);
    if (buf) {
        struct iovec iov;
        ssize_t ret;

        iov.iov_base = buf;
        iov.iov_len = sizeof(cp) + MGMT_HDR_SIZE;

        do {
            ret = writev(mgmt_fd, &iov, 1);
        } while (ret < 0 && errno == EINTR);

        free(buf);
    }
}

/*
 * Callback function for a gatt characteristics write request.
 *
 * Parameters:
 *     status:    [in] The response status for the write request.
 *     pdu:       [in] The response data.
 *     plen:      [in] The length in bytes of the response data.
 *     user_data: [in] User defined data that was provided during the write
 *                     request.
 *
 * Returns:
 *    void
 */
static void
char_write_req_cb (guint8 status, const guint8 *pdu, guint16 plen,
                   gpointer user_data)
{
	if (status != 0) {
		dbgp("Characteristic Write Request failed: "
						"%d\n", status);
        return;
	}

	dbgp("Characteristic value was written successfully\n");
}

/*
 * Callback function for a gatt notification from the server.
 * Handles measurement and control point notifications from the Nonin.
 *
 * Parameters:
 *     pdu:       [in] The notification data.
 *     len:       [in] The length in bytes of the data.
 *     user_data: [in] User defined data that was provided during
 *                     notification setup.
 *
 * Returns:
 *    void
 */
static void
notification_cb (const uint8_t *pdu, uint16_t len, gpointer user_data)
{
    time_t current_time_secs;
    struct tm *current_time;
    nonin3230_cb_data_t *cb_data = (nonin3230_cb_data_t *)user_data;
    bt_device_data_t *data;
    att_hdr_t *att_hdr;
    nonin3230_oximetry_data_t *oximetry_data;
    nonin3230_cp_mcomplete_t mcomplete;
    nonin3230_cp_response_t *cp_response;

    if (!pdu || !cb_data || !cb_data->data) {
        g_main_loop_quit(event_loop);
    }

    att_hdr = (att_hdr_t *)&pdu[0];
    dbgp("Notification for handle %d\n", att_hdr->handle);

    switch (att_hdr->handle) {
    case NON3230_OXIMETRY_MEASUREMENT_CHAR_HND:
        /*
         * This is measurement data.
         * Wait for the Nonin to indicate that the data has synced.
         * Once the data has synced it can be stored.
         */
        oximetry_data = (nonin3230_oximetry_data_t *)&pdu[sizeof(att_hdr_t)];

        if (!(oximetry_data->status & 0x1)) {
            dbgp("Not synced yet...\n");
            return;
        }
        dbgp("Synced!\n");

        data = cb_data->data;
        current_time_secs = time(NULL);
        current_time = localtime(&current_time_secs);
        
        if (data && current_time) {
            dbgp("Oximetry: %d %d on %02d-%02d-%02d at %02d:%02d\n",
                 oximetry_data->spo2, get_be16(&oximetry_data->pulse_rate),
                 current_time->tm_mday, current_time->tm_mon + 1,
                 1900 + current_time->tm_year, current_time->tm_hour,
                 current_time->tm_min);

            data[0].timestamp.year = 1900 + current_time->tm_year;
            data[0].timestamp.month = current_time->tm_mon;
            data[0].timestamp.day = current_time->tm_mday;
            data[0].timestamp.hour = current_time->tm_hour;
            data[0].timestamp.minute = current_time->tm_min;

            snprintf(data[0].value, MAX_RDB_BT_VAL_LEN,
                     "%d",
                     get_be16(&oximetry_data->pulse_rate));

            cb_data->data_len++;
        }

        /*
         * Send the Nonin a measurement complete command to the Nonin.
         * The Nonin will provide a visual indication of the complete and
         * will turn off the bluetooth radio.
         */
        mcomplete.command = NON3230_OXIMETRY_CTRLPNT_MCOMPLETE_REQ;
        bcopy(CP_MCOMPLETE_VAL, mcomplete.value, sizeof(mcomplete.value));
        gatt_write_char(attrib, NON3230_OXIMETRY_CTRLPNT_CHAR_HND,
                        (unsigned char *)&mcomplete,
                        sizeof(mcomplete), char_write_req_cb, NULL);

        break;

    case NON3230_OXIMETRY_CTRLPNT_CHAR_HND:
        /*
         * The Nonin has acknowledged our measurement complete command.
         * All done so exit the main loop.
         */
        cp_response = (nonin3230_cp_response_t *) &pdu[sizeof(att_hdr_t)];

        dbgp("cp response command 0x%x\n", cp_response->command);
        if (cp_response->command == NON3230_OXIMETRY_CTRLPNT_MCOMPLETE_RES) {
            g_main_loop_quit(event_loop);
        }
        break;
    default:
        /* Nothing to do for all other notifications */
        break;
    }
}

/*
 * Disconnect the connection to the device and clean up.
 */
static
void disconnect_io()
{
    if (chan) {
        g_io_channel_shutdown(chan, FALSE, NULL);
        g_io_channel_unref(chan);
        chan = NULL;
        connected = 0;
        unpair_device(&dst_addr);
        dbgp("Disconnected\n");
    }
}

/*
 * Callback if the connection times out.
 *
 * Parameters:
 *     user_data: [in] User defined data that was provided during
 *                timeout setup.
 *
 * Returns:
 *    void
 */
static gboolean
connect_timeout_cb (gpointer user_data)
{
    if (!connected) {
        /*
         * Disconnect. But don't exit the main loop yet. Wait for the connect
         * callback which will be called when the disconnect happens. The main
         * loop exit will occur in the connect callback at that point.
         */
        dbgp("Connect timed out.\n");
        disconnect_io();
    }
    return FALSE;
}

/*
 * Callback if the measurement sycn times out.
 *
 * Parameters:
 *     user_data: [in] User defined data that was provided during
 *                timeout setup.
 *
 * Returns:
 *    void
 */
static gboolean
sync_timeout_cb (gpointer user_data)
{
    dbgp("Sync timeout\n");
    g_main_loop_quit(event_loop);
    return FALSE;
}

/*
 * Callback when the device connect completes.
 *
 * Parameters:
 *     io:        [in] The socket IO channel opened to the device.
 *     err:       [in] NULL if success. Contains the error if connection failed.
 *     user_data: [in] User defined data that was provided during
 *                connection setup.
 *
 * Returns:
 *    void
 */
static void
connect_cb (GIOChannel *io, GError *err, gpointer user_data)
{
    unsigned char cccd;
    nonin3230_cp_sync_t cp;

    if (chan != io) {
        /* Connect has timed out */
        dbgp("Old connect cb ignored\n");
        g_main_loop_quit(event_loop);
        return;
    }

    if (err) {
		dbgp("%s\n", err->message);
        g_main_loop_quit(event_loop);
		return;
	}

    dbgp("Connected\n");

    if (!attrib) {
        attrib = g_attrib_new(io);

        g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
                          notification_cb, &cb_data, NULL);
    }

    /* Enable Oximetry Service notifications */
    cccd = ATT_CCCD_NOTI_ENABLE_VAL;
    gatt_write_char(attrib, NON3230_OXIMETRY_MEASUREMENT_CCCD_HND, &cccd,
                    sizeof(cccd), char_write_req_cb, NULL);
    gatt_write_char(attrib, NON3230_OXIMETRY_CTRLPNT_CCCD_HND, &cccd,
                    sizeof(cccd), char_write_req_cb, NULL);
    
    cp.command = NON3230_OXIMETRY_CTRLPNT_SYNC_REQ;
    cp.val = NN_SYNC_SECS;
    gatt_write_char(attrib, NON3230_OXIMETRY_CTRLPNT_CHAR_HND,
                    (unsigned char *)&cp,
                    sizeof(cp), char_write_req_cb, NULL);

    connected = 1;

    sync_timeout_gsource_id =
        g_timeout_add_seconds(NN_SYNC_TIMEOUT_SECS, sync_timeout_cb, NULL);
}

/*
 * Connects to a Nonin 3230 and reads all available data from it.
 *
 * Parameters:
 *   src              [in] The source BT address (ie, local adapter).
 *   dst              [in] The destination BT address (ie, sensor device).
 *   data             [out] Any read data will be stored in this array.
 *   max_data_len     [in] Max entries that can be stored in the data array.
 *
 * Returns:
 *    The number of read and stored data entries.
 */
int
nonin3230_read_data (bdaddr_t *src, bdaddr_t *dst, bt_device_data_t *data,
                     int max_data_len)
{
    char src_str[NN_BADDR_BUF_LEN];
    char dst_str[NN_BADDR_BUF_LEN];
    GError *gerr = NULL;
    guint timeout_src_id;
    GSource *gsource;

    dbgp("nonin3230_read_data: start\n");

    sync_timeout_gsource_id = 0;

    bacpy(&dst_addr, dst);
    unpair_device(dst);

    ba2str(src, src_str);
    ba2str(dst, dst_str);

    /*
     * gatt_connect does not pass the user data to the connect callback.
     * So a global is used. Which means this function is not reentrant.
     */
    cb_data.data = data;
    cb_data.data_len = 0;
    cb_data.max_data_len = max_data_len;

    chan = gatt_connect(src_str, dst_str, "public", "low", 0, 0, connect_cb,
                        &gerr);

    if (!chan) {
        dbgp("nonin3230_read_data: Unable to connect to device.\n");
        goto done;
    }

    if (gerr) {                
        g_error_free(gerr);
    }

    dbgp("nonin3230_read_data: Connect started\n");

    if (!event_loop) {
        event_loop = g_main_loop_new(NULL, FALSE);
    }

    timeout_src_id = g_timeout_add_seconds(NN_CONN_TIMEOUT_SECS,
                                           connect_timeout_cb, NULL);

    g_main_loop_run(event_loop);

    /* Clean up - detach all timeout callbacks */

    gsource = g_main_context_find_source_by_id(NULL, timeout_src_id);

    if (gsource) {
        g_source_destroy(gsource);
    }

    if (sync_timeout_gsource_id > 0) {
        gsource =
            g_main_context_find_source_by_id(NULL, sync_timeout_gsource_id);
        if (gsource) {
            g_source_destroy(gsource);
        }
    }

    disconnect_io();

    if (mgmt_fd > 0) {
        close(mgmt_fd);
        mgmt_fd = -1;
    }

 done:

    return cb_data.data_len;
}
