/*
 * Install Tool Socket Forward Server
 *
 * This daemon provides socket forward services described at:
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
#include "socket_server.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <rdb_ops.h>

// FIFO open modes
#ifdef LOOP_TEST
#define FIFO_OPEN_READ_FLAGS  O_RDWR
#define FIFO_OPEN_WRITE_FLAGS O_RDWR
#else
#define FIFO_OPEN_READ_FLAGS  O_RDONLY
#define FIFO_OPEN_WRITE_FLAGS O_WRONLY
#endif

#define POLL_INTERVAL_SECS  0xFFFF

// FIFO file names
#define FIFO_SOCKETS_IN      "/tmp/fifo_spm_socks"
#define FIFO_SOCKETS_OUT     "/tmp/fifo_socks_spm"

/* file descriptors */
static int fd_socks_in = -1, fd_socks_out = -1;
SocketServer socket_server;
FILE* fp_sock_in = nullptr;

volatile static int terminate = 0;

#define RDBV_BUF_LEN    8
#define LEGACY_OWA_CONNECTION "owa.legacy.connection"
static struct rdb_session * rdb_s = nullptr;

#ifdef CLI_DEBUG
static struct timespec start_time;
long int time_since_start()
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (long int) ((now.tv_sec- start_time.tv_sec)*1000 +
                       (now.tv_nsec - start_time.tv_nsec)/1000000);
}
#endif

/* signal handler for SIGINT, SIGQUIT and SIGTERM */
static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
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
 * @param fd File descriptor to save file handle
 * @param file_name File name of the FIFO
 * @param mode File open mode
 */
#define OPEN_FIFO(fd, file_name, mode)                  \
    do {                                                \
        fd = open(file_name, mode);                     \
        if (-1 == fd) {                                 \
            ret = errno;                                \
            BLOG_ERR("Failed to open %s\n", file_name); \
            goto ini_bad_exit;                          \
        }                                               \
    } while(0)

/*
 * Close file descriptor
 *
 * @param fd File descriptor to save file handle
 */
#define CLOSE_FD(fd)                                \
    do {                                            \
        if (fd != -1) {                             \
            (void)close(fd);                        \
            fd = -1;                                \
        }                                           \
    } while(0)

/*
 * Create and open FIFOs, start socket server
 *
 * @param socket_port TCP socket port number to listen on
 * @return 0 on success; negative error code on failure
 */
static int init(const char* socket_port)
{
    int ret = 0;
    uint16_t port = 0;
    char rdbv[RDBV_BUF_LEN];

    BLOG_NOTICE("initialising\n");

    port = (uint16_t)atoi(socket_port);
    if (0 == port) {
        ret = -1;
        BLOG_ERR("invalid port %s\n", socket_port);
        goto ini_bad_exit;
    }

    // create sockets out/in fifos
    CREATE_FIFO(FIFO_SOCKETS_IN);
    CREATE_FIFO(FIFO_SOCKETS_OUT);

    // open sockets fifos
    OPEN_FIFO(fd_socks_in,  FIFO_SOCKETS_IN,  FIFO_OPEN_READ_FLAGS);
    OPEN_FIFO(fd_socks_out, FIFO_SOCKETS_OUT, FIFO_OPEN_WRITE_FLAGS);

#ifdef LOOP_TEST
    fd_socks_in = fd_socks_out;
#endif

    if (rdb_open(nullptr, &rdb_s) < 0 || !rdb_s) {
         ret = -1;
         BLOG_ERR("could not open rdb\n");
         goto ini_bad_exit;
    }

    // wait for the legacy owa detection result
    for(;;) {
        int ret = rdb_get_string(rdb_s, LEGACY_OWA_CONNECTION, rdbv, sizeof(rdbv));
        if (!ret && !strcmp(rdbv, "2")) {
            break;
        }
        sleep(1);
    }

    socket_server.m_listener.Start(port);
    goto ini_good_exit;

ini_bad_exit:
    CLOSE_FD(fd_socks_in);
    CLOSE_FD(fd_socks_out);

ini_good_exit:
    if (rdb_s) {
        rdb_close(&rdb_s);
    }
    return ret;
}

/*
 * Read messages from scomm and forward to socket connections for processing
 *
 *
 */
static void scomm_inbound()
{
    BLOG_DEBUG("read from scomm\n");

    uint16_t len = 0;

    if (1 != fread(&len, sizeof(len), 1, fp_sock_in)) {
        BLOG_ERR("can't read fifo %d", errno);
        terminate = true;
        return;
    }

    len = ntohs(len);
    uint8_t* buffer = new uint8_t[len];

    if (!buffer) {
        BLOG_ERR("can't allocate buffer %d\n", len);
        return;
    }

    if (1 != fread(buffer, len, 1, fp_sock_in)) {
        BLOG_ERR("can't read for %d bytes, error %d\n", len, errno);
        delete[] buffer;
        return;
    }

#ifdef CLI_DEBUG
    for(uint16_t z=0; z<len; z++) {
        BLOG_DEBUG_DATA("%02X%s", buffer[z], ((z+1)%16 && z!=(len-1))?" ":"\n");
    }
#endif

    SocketMessage* message = SocketMessage::Decode(buffer, len);
    socket_server.OnMessage(*message);
    delete message;
    delete[] buffer;
}

#define IS_FD_INVALID(flag) ((flag & POLLERR) || (flag & POLLHUP))

/*
 * Watch on fds and process fd events
 *
 */
static int main_loop(void)
{
    int ret = 0;
    pollfd fdset[POLL_SIZE];
    FileBuffer scomm(fd_socks_out);
    fp_sock_in = fdopen(fd_socks_in, "rb+");

    BLOG_INFO("entering main_loop\n");

    while (!terminate) {
        BLOG_DEBUG("\nstarts a loop\n");

        memset(&fdset, 0, sizeof(fdset));
        fdset[POLL_FIFO_IN_POS].fd = fd_socks_in;
        fdset[POLL_FIFO_IN_POS].events = POLLIN | POLLERR | POLLHUP;

        fdset[POLL_FIFO_OUT_POS].fd = fd_socks_out;
        fdset[POLL_FIFO_OUT_POS].events = (scomm.IsEmpty()?0:POLLOUT) | POLLERR | POLLHUP;

        fdset[POLL_SERVER_SOCKET_POS].fd = socket_server.m_listener.GetFd();
        fdset[POLL_SERVER_SOCKET_POS].events =
                        (socket_server.HasHalfOpenCloseChannel()?0:POLLIN) | POLLERR | POLLHUP;

        int registered = POLL_NONE_SOCKET_CONN;
        registered += socket_server.RegisterEvents(&fdset[registered], MAX_SOCKET_CHANNEL);

        BLOG_DEBUG("polling on %d fds\n", registered);
        ret = poll(fdset, registered, -1);
        BLOG_DEBUG("poll returns %d\n", ret);
        usleep(5);

#ifdef CLI_DEBUG
        for(int z =0; z<POLL_SIZE; z++) {
            if (fdset[z].revents) {
                BLOG_DEBUG("%d - fd %d, events %d\n", z, fdset[z].fd, fdset[z].revents);
            }
        }
#endif

        if (IS_FD_INVALID(fdset[POLL_FIFO_IN_POS].revents)) {
            BLOG_ERR("error in fifo in\n");
            return -1;
        }

        if (IS_FD_INVALID(fdset[POLL_FIFO_OUT_POS].revents)) {
            BLOG_ERR("error in fifo out\n");
            return -1;
        }

        if (IS_FD_INVALID(fdset[POLL_SERVER_SOCKET_POS].revents)) {
            BLOG_ERR("error in server socket\n");
            return -1;
        }

        // process scomm inbound messages
        if (fdset[POLL_FIFO_IN_POS].revents & POLLIN) {
            scomm_inbound();
        }

        // process socket related messages
        socket_server.ProcessEvents(fdset, POLL_SIZE, scomm);

        // process scomm out
        (void)scomm.OnWrite(fdset[POLL_FIFO_OUT_POS].revents);
    }

    BLOG_NOTICE("exiting\n");
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

#ifdef CLI_DEBUG
    clock_gettime(CLOCK_REALTIME, &start_time);
#endif

    openlog("sockrel", LOG_CONS, LOG_USER);
    BLOG_NOTICE("starting\n");

    if (2 != argc) {
        BLOG_ERR("Usage: %s <port_number>\n", argv[0]);
        return -1;
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
    if (-1 != fd_socks_in ) {
        close(fd_socks_in);
    }

    if (-1 != fd_socks_out ) {
        close(fd_socks_out);
    }

    return ret;
}
