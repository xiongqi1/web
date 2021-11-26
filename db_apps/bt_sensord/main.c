/*
 * bt_sensord:
 *    Bluetooth sensor daemon. Reads data off all paired sensor devices and
 *    stores the data into RDB. Only handles certain devices. Unknown sensor
 *    and other non-sensor devices are ignored.
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
#include <unistd.h>
#include <stdlib.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

#include <rdb_ops.h>
#include "bt_sensord.h"
#include "td2551.h"
#include "mgh.h"

/* Default sleep time between each device poll */
#define SLEEP_SECS 10

static struct rdb_session *rdb_session = NULL;

/*
 * Constructs the RDB variable name for a bluetooth device entry.
 * Examples: bluetooth.device.0, bluetooth.device.1
 *
 * Parameters:
 *    full_var_name    [out] Will contain the constructed RDB
 *                     variable name.
 *    len              [in] The max length of full_var_name.
 *    var_name         [in] The variable within the device tree.
 *    dev_idx          [in] The index of the bluetooth device.
 *
 * Returns:
 *    full_var_name
 */
static char *
rdb_construct_dev_var (char *full_var_name, size_t len, char *var_name,
                       int dev_idx)
{
    snprintf(full_var_name, len, "%s.%d.%s", RDB_BT_DEV_PREFIX, dev_idx,
             var_name);
    return full_var_name;
}

/*
 * Constructs the RDB variable name for a bluetooth device data entry.
 * Examples: bluetooth.device.0.data.0, bluetooth.device.1.data.0
 *
 * Parameters:
 *    full_var_name    [out] Will contain the constructed RDB
 *                     variable name.
 *    len              [in] The max length of full_var_name.
 *    var_name         [in] The variable within the device tree.
 *    dev_idx          [in] The index of the bluetooth device.
 *    data_idx         [in] The index of the data entry for this device.
 *
 * Returns:
 *    full_var_name
 */
static char *
rdb_construct_data_var (char *full_var_name, size_t len, char *var_name,
                        int dev_idx, int data_idx)
{
    if (data_idx == RDB_DATA_GLOBAL_IDX) {
        snprintf(full_var_name, len, "%s.%d.data.%s", RDB_BT_DEV_PREFIX,
                 dev_idx, var_name);
    } else {
        snprintf(full_var_name, len, "%s.%d.data.%d.%s", RDB_BT_DEV_PREFIX,
                 dev_idx, data_idx, var_name);
    }
    return full_var_name;
}

/*
 * Writes data values to RDB. Also updates control indices.
 *
 * Parameters:
 *    dev_idx          [in] The index of the bluetooth device.
 *    data_time        [in] Timestamp of the data entry.
 *    data_value       [in] Value of the data entry
 *
 * Returns:
 *    void
 */
static void
rdb_write_data (int dev_idx, char *data_time, char *data_value)
{
    int start_idx, count, max_count, ret, next_idx;
    char rdb_var[MAX_RDB_BT_VAR_LEN];
    char rdb_value[MAX_RDB_BT_VAL_LEN];

    ret = rdb_get_int(rdb_session,
                      RDB_BT_DATA_START_IDX(rdb_var, MAX_RDB_BT_VAR_LEN,
                                            dev_idx),
                      &start_idx);

    if (ret) {
        dbgp("Write data failed. Unable to get start index.");
        return;
    }

    ret = rdb_get_int(rdb_session,
                      RDB_BT_DATA_COUNT(rdb_var, MAX_RDB_BT_VAR_LEN, dev_idx),
                      &count);

    if (ret) {
        dbgp("Write data failed. Unable to get count.");
        return;
    }

    ret = rdb_get_int(rdb_session,
                      RDB_BT_DATA_MAX_COUNT(rdb_var, MAX_RDB_BT_VAR_LEN,
                                            dev_idx),
                      &max_count);

    if (ret) {
        dbgp("Write data failed. Unable to get max count.");
        return;
    }

    if (count == max_count) {
        /* Data buffer full. Overwrite the oldest entry by moving start_idx */
        start_idx++;
        if (start_idx == max_count) {
            /* Buffer has wrapped */
            start_idx = 0;
        }

        snprintf(rdb_value, MAX_RDB_BT_VAL_LEN, "%d", start_idx);
        ret = rdb_set_string(rdb_session,
                             RDB_BT_DATA_START_IDX(rdb_var, MAX_RDB_BT_VAR_LEN,
                                                   dev_idx),
                             rdb_value);

        if (ret) {
            dbgp("Write data failed. Unable to update start index.");
            return;
        }
    } else {
        /* Data buffer not full. Add a new entry */

        count++;

        snprintf(rdb_value, MAX_RDB_BT_VAL_LEN, "%d", count);
        ret = rdb_set_string(rdb_session,
                             RDB_BT_DATA_COUNT(rdb_var, MAX_RDB_BT_VAR_LEN,
                                               dev_idx),
                             rdb_value);

        if (ret) {
            dbgp("Write data failed. Unable to update count.");
            return;
        }
    }

    /* Next index to write to with wrapping if required */
    next_idx = (start_idx + count - 1) % max_count;

    rdb_update(rdb_session,
               RDB_BT_DATA_TIME(rdb_var, MAX_RDB_BT_VAR_LEN,
                                dev_idx, next_idx),
               data_time, strlen(data_time) + 1, PERSIST, 0);
    rdb_update(rdb_session,
               RDB_BT_DATA_VAL(rdb_var, MAX_RDB_BT_VAR_LEN,
                               dev_idx, next_idx),
               data_value, strlen(data_value) + 1, PERSIST, 0);
}

/*
 * Find and return info on all paired devices.
 *
 * Parameters:
 *    paired_devices   [out] Will contain the paired devices info.
 *
 * Returns:
 *    The number of paired devices found
 */
static int
get_paired_devices (bt_device_t paired_devices[])
{
    int ix, ret, num_paired;
    char rdb_value[MAX_RDB_BT_VAL_LEN];
    char rdb_var[MAX_RDB_BT_VAR_LEN];
    int rdb_value_len;

    num_paired = 0;
    for (ix = 0; ix < MAX_BT_DEVICES; ix++) {
        rdb_value_len = sizeof(rdb_value);
        ret = rdb_get(rdb_session,
                      RDB_BT_DEV_ADDR(rdb_var, MAX_RDB_BT_VAR_LEN, ix),
                      rdb_value, &rdb_value_len);

        if (ret) {
            if (ret != -ENOENT) {
                /*
                 * ENOENT means no paired devices which is ok. All other
                 * non-zero values are real errors.
                 */
                perror("Failed to get rdb BT address");
            }
            break;
        }

        str2ba(rdb_value, &(paired_devices[ix].addr));

        rdb_value_len = sizeof(rdb_value);
        ret = rdb_get_int(rdb_session,
                          RDB_BT_DEV_TYPE(rdb_var, MAX_RDB_BT_VAR_LEN, ix),
                          (int *)&paired_devices[ix].type);

        if (ret) {
            perror("Failed to get rdb BT type");
            break;
        }

        rdb_value_len = sizeof(paired_devices[ix].serial_num);
        ret = rdb_get(rdb_session,
                      RDB_BT_DEV_SERIAL_NUM(rdb_var, MAX_RDB_BT_VAR_LEN, ix),
                      paired_devices[ix].serial_num, &rdb_value_len);
        if (ret) {
            perror("Failed to get rdb BT serial_num");
            break;
        }

        dbgp("found paired: addr=%s, type=%d\n", rdb_value,
             paired_devices[ix].type);
        num_paired++;
    }

    dbgp("Num paired: %d\n", num_paired);
    return num_paired;
}

/*
 * A noop sensor data read function.
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
static int
dummy_sensor_read_data (bdaddr_t *src, bdaddr_t *dst, bt_device_data_t *data,
                        int max_data_len)
{
    return 0;
}

extern int nonin3230_read_data(bdaddr_t *src, bdaddr_t *dst, bt_device_data_t *data,
                           int max_data_len);

/*
 * The list of sensor read functions.
 * IMPORTANT: The order is important - each entry MUST match the corresponding
 * type in the bt_device_type_t enum.
 */
static sensor_read_fn_t sensor_reader[] = {
    dummy_sensor_read_data,
    td2551_read_data,
    nonin3230_read_data,
    mgh_read_data,
};

static data_post_fn_t data_poster[] = {
    NULL, /* DEV_TYPE_UNKNOWN */
    NULL, /* DEV_TYPE_TD2251_WEIGHT_SCALE */
    NULL, /* DEV_TYPE_NN3350_PULSE_OXIMETER */
    hpoint_data_post, /* DEV_TYPE_MGH_GLUCOMETER */
};

/*
 * Initialise RDB.
 */
static void
rdb_init(void)
{
    int ret;

    ret = rdb_open(NULL, &rdb_session);
    if (ret) {
        perror("Unable to open rdb");
        exit (1);
    }
}

/*
 * Program usage.
 *
 * Parameters:
 *   progname    Name of the program
 */
static void
usage (char *progname)
{
    printf("Usage:\n");
    printf("%s [options]\n\n", progname);
    printf("Options:\n");
    printf(" -s <seconds>    Seconds to sleep between device reads "
           "(default=10). \n");
    printf("\n");
}

/*
 * Bluetooth sensor daemon.
 * Periodically finds all paired devices, invokes the data read function for the
 * each device (if known) and stores any read data into RDB.
 */
int main (int argc, char**argv)
{
    bdaddr_t src;
    int hci_dev_num, dev_ix, data_ix, num_paired;
    int ret = 0;
    bt_device_t paired_devices[MAX_BT_DEVICES];
    bt_device_t *paired_dev;
    bt_device_data_t sensor_data[MAX_SENSOR_DATA_LEN];
    int c;
    int sleep_secs = SLEEP_SECS;
    int daemonize = 0;

    while ((c = getopt (argc, argv, "dhs:")) != -1) {
        switch (c) {
        case 'd':
            daemonize = 1;
            break;
        case 's':
            sleep_secs = atoi(optarg);
            break;
        case 'h':
            usage(argv[0]);
            exit(0);
        default:
            exit(1);
        }
    }

    dbgp("sleep secs: %d\n", sleep_secs);

    rdb_init();

    hci_dev_num = 0;

    num_paired = get_paired_devices(paired_devices);

    while (1) {
        sleep(sleep_secs);

        ret = hci_devba(hci_dev_num, &src);
        if (ret) {
            dbgp("Can't get bluetooth device address.");
            continue;
        }

        for (dev_ix = 0; dev_ix < num_paired; dev_ix++) {
            int num_data;

            paired_dev = &(paired_devices[dev_ix]);

            if (paired_dev->type >= DEV_TYPE_MAX) {
                /* Unknown sensor device type - skip it */
                continue;
            }

            num_data =
                sensor_reader[paired_dev->type](&src, &paired_dev->addr,
                                                sensor_data,
                                                MAX_SENSOR_DATA_LEN);

            dbgp("Device %d read %d data items\n", dev_ix, num_data);

            for (data_ix = 0; data_ix < num_data; data_ix++) {
                char timestamp[MAX_RDB_BT_VAL_LEN];

                snprintf(timestamp, MAX_RDB_BT_VAL_LEN,
                         "%02d-%02d-%04d %02d:%02d",
                         sensor_data[data_ix].timestamp.day,
                         sensor_data[data_ix].timestamp.month,
                         sensor_data[data_ix].timestamp.year,
                         sensor_data[data_ix].timestamp.hour,
                         sensor_data[data_ix].timestamp.minute);

                rdb_write_data(dev_ix, timestamp,
                               sensor_data[data_ix].value);
            }

            if (num_data > 0) {
                system("speaker-play < /usr/lib/sounds/reboot");

                if (data_poster[paired_dev->type]) {
                    data_poster[paired_dev->type](paired_dev, sensor_data,
                                                  num_data);
                }
            }
        }

        if (!daemonize) {
            break;
        }
    }

    rdb_close(&rdb_session);
    return ret;
}
