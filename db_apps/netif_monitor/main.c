/*
 * Network interface monitor
 *
 * This daemon monitors a given network interface and updates RDB for the link state.
 * It registers for netlink RTM_NEWLINK notification, which is sent by kernel to
 * inform about various link events, including IFF_RUNNING state. When the event
 * is received, it checks the message against the given network interface name.
 * If it matches, the RDB is set according to IFF_RUNNING flag.
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

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/cache.h>
#include <linux/if.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <rdb_ops.h>
#include "utils.h"

// the name of the RDB to record link state
#define LINK_STATE_RDB "service.netif_monitor.link_state_rdb"
// the interface to be monitored
#define IFACE_NAME "service.netif_monitor.iface_name"

#define BUFSZ 256
static char link_state_rdb[BUFSZ];
static char iface_name[BUFSZ];

static struct rdb_session * rdb_s;

volatile static sig_atomic_t terminate = 0;

/* signal handler for SIGINT, SIGQUIT and SIGTERM */
static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
}

/*
 * Process a single netlink RTM_NEWLINK message
 *
 * @param msgh A pointer to the netlink message header
 */
static void process_msg(struct nlmsghdr * msgh)
{
    struct ifinfomsg *iface;
    struct rtattr *attr;
    int len;
    char buf[256];
    int ret;

    iface = nlmsg_data(msgh);
    len = nlmsg_attrlen(msgh, sizeof(*iface));
    BLOG_DEBUG("flags=0x%x: %s\n", iface->ifi_flags,
               rtnl_link_flags2str(iface->ifi_flags, buf, sizeof(buf)));
    // loop over all attributes for the NEWLINK message
    for (attr = IFLA_RTA(iface); RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
        if (attr->rta_type == IFLA_IFNAME) {
            const char * name = (const char *) RTA_DATA(attr);
            BLOG_INFO("Interface %d: %s\n", iface->ifi_index, name);
            if (!strcmp(name, iface_name)) {
                BLOG_NOTICE("Interface %s changed state\n", name);
                ret = rdb_set_string(rdb_s, link_state_rdb,
                                     (iface->ifi_flags & IFF_RUNNING) ? "1" : "0");
                if (ret) {
                    BLOG_ERR("Failed to set RDB\n");
                }
                break;
            }
        }
    }
}

/*
 * callback function for netlink event notification
 *
 * @param msg A pointer to the netlink message
 * @param arg Argument passed in when registered the callback (unused now)
 * @return 0 for success; error code on failure
 */
static int cb(struct nl_msg * msg, void *arg)
{
    (void)arg;
    BLOG_DEBUG("callback ...\n");

    int len;
    struct nlmsghdr * nlh = nlmsg_hdr(msg);
    // msg could contain any number of netlink messages, iterate through it
    for (len = nlh->nlmsg_len; nlmsg_ok(nlh, len); nlh = nlmsg_next(nlh, &len)) {
        BLOG_DEBUG("received: nlmsg_type=0x%x\n", nlh->nlmsg_type);
        // we are only interested in NETLINK message
        if (nlh->nlmsg_type == RTM_NEWLINK) {
            process_msg(nlh);
        }
    }

    return 0;
}

/*
 * Do initial poll for link state and set the RDB
 *
 * @return 0 on success; negative error code on failure
 */
static int initial_poll(void)
{
    int ret;
    struct nl_sock * sock;
    struct nl_cache * cache;
    struct rtnl_link * link;
    if (!(sock = nl_socket_alloc())) {
        return -1;
    }

    if ((ret = nl_connect(sock, NETLINK_ROUTE))) {
        BLOG_ERR("failed to connect to netlink-route\n");
        goto fin_sock;
    }

    if ((ret = rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache)) < 0) {
        BLOG_ERR("failed to alloc cache: %d\n", ret);
        goto fin_sock;
    }
    if (!(link = rtnl_link_get_by_name(cache, iface_name))) {
        BLOG_ERR("failed to get link %s\n", iface_name);
        ret = -1;
        goto fin_cache;
    }

    ret = rdb_set_string(rdb_s, link_state_rdb,
                         (rtnl_link_get_flags(link) & IFF_RUNNING) ? "1" : "0");
    if (ret < 0) {
        BLOG_ERR("failed to write RDB %s\n", link_state_rdb);
    }

    rtnl_link_put(link);
fin_cache:
    nl_cache_free(cache);
fin_sock:
    nl_socket_free(sock);
    return ret;
}

/*
 * the main loop
 *
 * @return 0 on success; error code on failure
 */
static int main_loop(void)
{
    int ret;
    fd_set fdset;
    struct nl_sock * sock;
    int sockfd;

    if (!(sock = nl_socket_alloc())) {
        return -1;
    }

    // disable sequence number checking since we only receive notification
    nl_socket_disable_seq_check(sock);

    ret = nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, cb, NULL);
    if (ret) {
        BLOG_ERR("failed to set socket callback\n");
        goto fin_sock;
    }

    // connect to routing netlink protocol, which contains link state
    // file descriptor is only created and socket bound after this call
    ret = nl_connect(sock, NETLINK_ROUTE);
    if (ret) {
        BLOG_ERR("failed to connect to netlink-route\n");
        goto fin_sock;
    }

    // set socket to nonblocking mode so that it can be terminated by signal
    // blocking mode won't work since it will keep retrying on EINTR
    ret = nl_socket_set_nonblocking(sock);
    if (ret) {
        BLOG_ERR("failed to set socket to nonblocking mode: %d\n", ret);
        goto fin_sock;
    }

    // join the RTNLGRP_LINK group to receive link event notification
    ret = nl_socket_add_membership(sock, RTNLGRP_LINK);
    if (ret) {
        BLOG_ERR("failed to join RTNLGRP_LINK\n");
        goto fin_sock;
    }

    // get fd for select
    sockfd = nl_socket_get_fd(sock);
    if (sockfd < 0) {
        BLOG_ERR("failed to get fd from netlink socket\n");
        ret = -1;
        goto fin_sock;
    }
    BLOG_DEBUG("sockfd=%d\n", sockfd);

    if (initial_poll() < 0) {
        BLOG_ERR("failed: initial_poll\n");
    }

    while (!terminate) {
        FD_ZERO(&fdset);
        FD_SET(sockfd, &fdset);

        ret = select(sockfd + 1, &fdset, NULL, NULL, NULL);
        if (ret < 0) { // error
            int tmp = errno;
            BLOG_ERR("select returned %d, errno: %d\n", ret, tmp);
            if (tmp == EINTR) { // interrupted by signal
                ret = 0;
            }
            goto fin_sock;
        } else if (ret > 0) { // available for receiving
            ret = nl_recvmsgs_default(sock);
            if (ret) {
                BLOG_ERR("failed: recvmsgs returns %d\n", ret);
            } else {
                BLOG_DEBUG("succeeded: recvmsgs\n");
            }
        }
    }

fin_sock:
    BLOG_NOTICE("main_loop exited\n");
    nl_socket_free(sock);

    return ret;
}

int main(void)
{
    int ret;

    openlog("netifmon", LOG_CONS, LOG_USER);

    if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        BLOG_ERR("Failed to open RDB\n");
        ret = -1;
        goto fin_log;
    }

    ret = rdb_get_string(rdb_s, LINK_STATE_RDB, link_state_rdb, BUFSZ);
    if (ret) {
        BLOG_ERR("Failed to get rdb %s (%d)\n", LINK_STATE_RDB, ret);
        goto fin_rdb;
    }
    ret = rdb_get_string(rdb_s, IFACE_NAME, iface_name, BUFSZ);
    if (ret) {
        BLOG_ERR("Failed to get rdb %s (%d)\n", IFACE_NAME, ret);
        goto fin_rdb;
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
        signal(SIGQUIT, sig_handler) == SIG_ERR ||
        signal(SIGTERM, sig_handler) == SIG_ERR) {
        ret = -errno;
        BLOG_ERR("Failed to register signal handler\n");
        goto fin_rdb;
    }

    ret = main_loop();
    if (ret) {
        BLOG_ERR("main loop terminated unexpectedly\n");
    } else {
        BLOG_NOTICE("main loop exited normally\n");
    }

fin_rdb:
    rdb_close(&rdb_s);
fin_log:
    closelog();

    return ret;
}
