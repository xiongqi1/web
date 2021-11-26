/*
 * @file main.c
 * @brief Install Tool Serial Port Manager
 *
 * This daemon provides services of layer 1 and 2 described at:
 * https://pdgwiki.netcommwireless.com/mediawiki/index.php/Titan_NRB-0200_System
 *
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
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

#include "utils.h"
#include "serial_process.h"
#include "fifo_process.h"

#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <rdb_ops.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

// FIFO open modes
#ifdef LOOP_TEST
#define FIFO_OPEN_READ_FLAGS  "rb+"
#define FIFO_OPEN_WRITE_FLAGS "wb+"
#else
#define FIFO_OPEN_READ_FLAGS  "rb"
#define FIFO_OPEN_WRITE_FLAGS "wb"
#endif

#define POLL_INTERVAL_SECS  0xFFFF

// FIFO file names
#define FIFO_MANAGEMENT_OUT "/tmp/fifo_spm_mgmt"
#define FIFO_MANAGEMENT_IN  "/tmp/fifo_mgmt_spm"
#define FIFO_SOCKETS_OUT    "/tmp/fifo_spm_socks"
#define FIFO_SOCKETS_IN     "/tmp/fifo_socks_spm"

// RDB keys to wartch
#define OWA_SERIAL_BAUD_RATE_RDB "owa.serial.baud_rate"
#define OWA_CONNECTED_RDB        "owa.connected"

// Default baud rate
#define SERIAL_PORT_DEFAULT_BAUD_RATE 115200

#define INIT_RDB_NAMES_LEN  1000
#define RDBV_BUF_LEN        16

/* the following RDBs are watched for changes */
static const char * const rdb_watches[] = {
    OWA_SERIAL_BAUD_RATE_RDB,
    OWA_CONNECTED_RDB
};

static struct rdb_session * rdb_s;
static char * triggered_rdbs = NULL;

/* file pointers and descriptors */
static FILE *fp_serial = nullptr;
static FILE *fp_mgmt_out = nullptr, *fp_mgmt_in = nullptr;
static FILE *fp_socks_out = nullptr, *fp_socks_in = nullptr;

static int fd_serial = -1, fd_max = -1;
static int fd_mgmt_in = -1, fd_socks_in = -1;
static int fd_rdb = -1;

volatile static int terminate = 0;

/* signal handler for SIGINT, SIGQUIT and SIGTERM */
static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
}

/*
 * Open serial port with default settings
 *
 * @param serial_port Serial port device file path
 * @return 0 on success; negative error code on failure
 */
static int open_serial_port(const char* serial_port)
{
    struct termios options;
    memset(&options, 0, sizeof(options));

    fp_serial = fopen(serial_port, "rb+");
    if (NULL == fp_serial) {
        int ret = errno;
        BLOG_ERR("Failed to open %s\n", serial_port);
        return ret;
    }

    fd_serial = fileno(fp_serial);
    fcntl(fd_serial, F_SETFD, O_RDWR | O_NOCTTY);
    tcflush(fd_serial, TCIOFLUSH);

    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;

    // fastest, though 1.5 times of frame base size of unecessary
    // select returns can't be avoided
    options.c_lflag = ICANON;
    options.c_cc[VEOL]= 0xC0;

    // more effcient and good for large packets, however the
    // the 1/10 seconds penalty is huge
    //options.c_cc[VMIN] = 0xFF;
    //options.c_cc[VTIME] = 1;

    return tcsetattr(fd_serial, TCSANOW, &options);
}

/*
 * Create FIFO file
 *
 * @param file_name File name of the FIFO
 */
#define CREATE_FIFO(file_name)                              \
    do {                                                    \
        if (access(file_name, R_OK | W_OK) &&               \
            mkfifo(file_name, S_IRUSR | S_IWUSR)) {         \
            ret = errno;                                    \
            BLOG_ERR("Failed to create %s\n", file_name);   \
            goto ini_bad_exit;                              \
        }                                                   \
    } while(0)

/*
 * Open FIFO file
 *
 * @param fp File pointer to save file handle
 * @param file_name File name of the FIFO
 * @param mode File open mode
 */
#define OPEN_FIFO(fp, file_name, mode)                  \
    do {                                                \
        fp = fopen(file_name, mode);                    \
        if (NULL == fp) {                               \
            ret = errno;                                \
            BLOG_ERR("Failed to open %s\n", file_name); \
            goto ini_bad_exit;                          \
        }                                               \
    } while(0)


/*
 * Close file pointer
 *
 * @param fp File pointer to save file handle
 */
#define CLOSE_FP(fp)                                \
    do {                                            \
        if (fp != nullptr) {                        \
            (void)fclose(fp);                       \
            fp = nullptr;                           \
        }                                           \
    } while(0)


/*
 * Get larger value
 *
 * @param a Value A
 * @param b Value B
 */
#define MAX(a, b) ((a)>(b)?(a):(b))

/*
 * Initialise rdb watchers, open serial port, create and open FIFOs
 *
 * @param serial_port File name of the serial port connected to a legacy OWA
 * @return 0 on success; negative error code on failure
 */
static int init(const char* serial_port)
{
    int ret = 0;
    unsigned int idx;
    const char * rdbk;

    BLOG_NOTICE("initialising\n");

    // open serial port
    ret = open_serial_port(serial_port);
    if (ret) {
        return ret;
    }

    // create management out/in fifos
    CREATE_FIFO(FIFO_MANAGEMENT_OUT);
    CREATE_FIFO(FIFO_MANAGEMENT_IN);

    // create sockets out/in fifos
    CREATE_FIFO(FIFO_SOCKETS_OUT);
    CREATE_FIFO(FIFO_SOCKETS_IN);

    // open management fifos
    OPEN_FIFO(fp_mgmt_out, FIFO_MANAGEMENT_OUT, FIFO_OPEN_WRITE_FLAGS);
    OPEN_FIFO(fp_mgmt_in,  FIFO_MANAGEMENT_IN,  FIFO_OPEN_READ_FLAGS);

    // open sockets fifos
    OPEN_FIFO(fp_socks_out, FIFO_SOCKETS_OUT, FIFO_OPEN_WRITE_FLAGS);
    OPEN_FIFO(fp_socks_in,  FIFO_SOCKETS_IN,  FIFO_OPEN_READ_FLAGS);

#ifdef LOOP_TEST
    fd_mgmt_in  = fileno(fp_mgmt_out);
    fd_socks_in = fileno(fp_socks_out);
#else
    fd_mgmt_in  = fileno(fp_mgmt_in);
    fd_socks_in = fileno(fp_socks_in);
#endif

    fd_max = MAX(fd_rdb, fd_max);
    fd_max = MAX(fd_serial, fd_max);
    fd_max = MAX(fd_mgmt_in, fd_max);
    fd_max = MAX(fd_socks_in, fd_max);

   // watch RDBs
    for (idx = 0; idx < ARRAY_SIZE(rdb_watches); idx++) {
        rdbk = rdb_watches[idx];
        ret = rdb_subscribe(rdb_s, rdbk);
        if (ret < 0 && ret != -ENOENT) {
            BLOG_ERR("Failed to subscribe %s\n", rdbk);
            goto ini_bad_exit;
        }
    }

    goto ini_good_exit;

ini_bad_exit:
    CLOSE_FP(fp_serial);
    CLOSE_FP(fp_mgmt_out);
    CLOSE_FP(fp_mgmt_in);
    CLOSE_FP(fp_socks_out);
    CLOSE_FP(fp_socks_in);

    fd_serial = -1;
    fd_mgmt_in = -1;
    fd_socks_in = -1;
    fd_max = -1;

ini_good_exit:
    return ret;
}

/*
 * Check if Install Tool is connected to an OWA
 *
 * @retval true Connected
 * @retval false Not connected
 */
static bool is_owa_connected(void)
{
    char rdbv[RDBV_BUF_LEN];
    int ret;
    ret = rdb_get_string(rdb_s, OWA_CONNECTED_RDB, rdbv, sizeof(rdbv));
    return !ret && !strcmp(rdbv, "1");
}

/*
 * Change serial port baud rate
 *
 * @param speed Baud rate speed
 * @retval 1 Changed
 * @retval 0 Not changed
 */
static int change_baud_rate(int speed)
{
    struct termios options;
    int ret;

    ret = tcgetattr(fd_serial, &options);
    if (ret) {
        BLOG_ERR("Could not get serial option: %d\n", errno);
        return 0;
    }

    switch (speed) {
    case 9600:
        speed = B9600;
        break;

    case 19200:
        speed = B19200;
        break;

    case 38400:
        speed = B38400;
        break;

    case 57600:
        speed = B57600;
        break;

    case 230400:
        speed = B230400;
        break;

    case 115200:
    default:
        speed = B115200;
        break;
    }

    if (cfsetospeed(&options, speed)) {
        BLOG_ERR("Could not change speed option: %d\n", errno);
        return 0;
    }

    if (tcsetattr(fd_serial, TCSADRAIN, &options)) {
        BLOG_ERR("Could not change terminal speed: %d\n", errno);
        return 0;
    }

    return 1;
}

/*
 * Process a single RDB trigger
 *
 * @param rdbk RDB key that is triggered.
 * @retval -1 error
 */
static int process_single(const char * rdbk)
{
    if (!rdbk) {
        BLOG_ERR("Rdb service is not open");
        return -1;
    }

    if (!strcmp(rdbk, OWA_CONNECTED_RDB)) {
        // There can be multiple owa.connected triggers as the pins debounce
        if (!is_owa_connected()) {
            terminate = 1;
        }
    }
    else if (!strcmp(rdbk, OWA_SERIAL_BAUD_RATE_RDB)) {
        char rdbv[RDBV_BUF_LEN];
        int ret;

        ret = rdb_get_string(rdb_s, OWA_SERIAL_BAUD_RATE_RDB, rdbv, sizeof(rdbv));
        if (!ret) {
            BLOG_ERR("Could not read rdb: %s\n", OWA_SERIAL_BAUD_RATE_RDB);
            return 0;
        }
        change_baud_rate(atoi(rdbv));
    }
    else {
        return -1;
    }

    return 0;
}

/*
 * Process RDB trigger events
 *
 */
static void process_triggers()
{
    char * rdbk;
    int ret;
    int rdb_names_len = INIT_RDB_NAMES_LEN;

    ret = rdb_getnames_alloc(rdb_s, "", &triggered_rdbs, &rdb_names_len,
                            TRIGGERED);
    if (ret < 0) {
        BLOG_ERR("Failed to get triggered RDBs, ret:%d\n", ret);
        return;
    }

    rdbk = strtok(triggered_rdbs, "&");
    while (rdbk) {
        ret = process_single(rdbk);
        if (ret < 0) {
            BLOG_ERR("Failed to process triggered RDB %s\n", rdbk);
        }
        rdbk = strtok(NULL, "&");
    }
}

/*
 * Watch on fds and process fd events
 *
 */
static int main_loop(void)
{
    int ret;
    fd_set fdset;

    BLOG_INFO("entering main_loop\n");

    if (!is_owa_connected()) {
        BLOG_WARNING("OWA not connected\n");
        return 0;
    }

    char * triggered_rdbs = (char*)malloc(INIT_RDB_NAMES_LEN);
    if (!triggered_rdbs) {
        return -ENOMEM;
    }

    FifoProcess fifo_mgmt(fp_mgmt_out, fp_mgmt_in, 1, 0);
    FifoProcess fifo_socks(fp_socks_out, fp_socks_in, 1, 1);
    struct timeval timeout;

    while (!terminate) {
        BLOG_DEBUG("start a loop\n");

        FD_ZERO(&fdset);

        FD_SET(fd_rdb, &fdset);
        FD_SET(fd_serial, &fdset);
        FD_SET(fd_mgmt_in, &fdset);
        FD_SET(fd_socks_in, &fdset);

        // this allows singals to cause select to return
        timeout.tv_sec = POLL_INTERVAL_SECS;
        timeout.tv_usec = 0;

        ret = select(fd_max + 1, &fdset, NULL, NULL, &timeout);

        if (ret < 0) {
            int rval = ret;
            if (errno == EINTR) {
                rval = 0;
            }

            BLOG_ERR("select returned %d, errno: %d\n", ret, errno);
            free(triggered_rdbs);
            return rval;
        }

        if (ret && FD_ISSET(fd_serial, &fdset)) {
            ret--;
            BLOG_DEBUG("serial port event\n");
            serial_process_data(fd_serial, &fifo_mgmt, &fifo_socks);
        }

        if (ret && FD_ISSET(fd_socks_in, &fdset)) {
            ret--;
            BLOG_DEBUG("socket service event\n");
            fifo_socks.ProcessData(fp_serial);
        }

        if (ret && FD_ISSET(fd_mgmt_in, &fdset)) {
            ret--;
            BLOG_DEBUG("management service event\n");
            fifo_mgmt.ProcessData(fp_serial);
        }

        if (ret && FD_ISSET(fd_rdb, &fdset)) {
            ret--;
            BLOG_DEBUG("rdb event\n");
            process_triggers();
        }
    }

    BLOG_NOTICE("exiting\n");
    free(triggered_rdbs);
    return 0;
}

/*
 * Program main entrance
 *
 *@param argc Number of arguments
 *@param argv Argument string array
 *@return 0 on normal exit; negative error code on abnormal exit
 */
int main(int argc, const char* argv[])
{
    int ret;
    openlog("spman", LOG_CONS, LOG_USER);
    BLOG_NOTICE("starting\n");

    if (2 != argc) {
        BLOG_ERR("Usage: %s <serial_port_file>\n", argv[0]);
        return -1;
    }

    if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        BLOG_ERR("Failed to open RDB\n");
        return -1;
    }

    fd_rdb = rdb_fd(rdb_s);
    if (fd_rdb < 0) {
        BLOG_ERR("Failed to get rdb_fd\n");
        ret = -1;
        goto fin;
    }

    ret = init(argv[1]);
    if (ret < 0) {
        BLOG_ERR("Failed to init\n");
        goto fin;
    }
    BLOG_NOTICE("initialised\n");

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
        signal(SIGQUIT, sig_handler) == SIG_ERR ||
        signal(SIGTERM, sig_handler) == SIG_ERR) {
        ret = -errno;
        BLOG_ERR("Failed to register signal handler\n");
        goto fin;
    }

    ret = main_loop();
    if (ret < 0) {
        BLOG_ERR("main loop terminated unexpectedly\n");
    } else {
        BLOG_NOTICE("main loop exited normally\n");
    }

fin:
    rdb_close(&rdb_s);
    closelog();

    return ret;
}
