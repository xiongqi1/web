/*
 * Passive RTT measurement daemon
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

#include "uthash.h"
#include "rdb.h"
#include "tick_clock.h"

#include <stdio.h>
#include <errno.h>
#include <pcap.h>
#include <pcap/sll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>

//#define DEBUG_VERBOSE

/* maximum length 65535 of port address */
#define PORTSTRLEN 5
/* length 45 */
#define ADDRSTRLEN (INET6_ADDRSTRLEN - 1)
/* length 51 */
#define ADDRPORTSTRLEN (ADDRSTRLEN + 1 + PORTSTRLEN)
/* length 103 */
#define TRACKKEYLEN (ADDRPORTSTRLEN * 2 + 1)

/*

 500 maximum concurrent TCP connection is a heuristic number, which is highly unrealistic
 to be consumed or to cause OOM kill. Total memory usage can grow up to ~27KB memory.
 This limit can be increased for a better RTT accuracy.

 500 packets requires ~27KB buffer as sizeof(tcp_track_t) is ~56 bytes

*/

#define MAX_TRACK_PACKET	500
/* 1000 msec */
#define RDB_WRITE_INTERVAL	1000

/*
 /////////////////////////////////////////////////////////////////////////////
 // pcap callback parameters
 /////////////////////////////////////////////////////////////////////////////
*/

static int _dlh_sz; /* datalink layer header size */
static pcap_t *_pcaphndl; /* pcap live handle */
static const char* _rdb; /* latency rdb name */
static int _pkt_count = 0; /* ipv4 packet counter */
static int _pkt6_count = 0; /* ipv6 packet counter */

/*
 /////////////////////////////////////////////////////////////////////////////
 // garbage collection parameters
 /////////////////////////////////////////////////////////////////////////////
*/

static time_diff_ms_t _max_rtt; /* garbage collect threshold */
static time_ms_t _last_gg_time; /* last garbage collect time */
static int _track_mode;
static int _enable_ip4; /* whether ipv4 packet tracking is enabled */
static int _enable_ip6; /* whether ipv6 packet tracking is enabled */

/*
 /////////////////////////////////////////////////////////////////////////////
 // rdb parameters
 /////////////////////////////////////////////////////////////////////////////
*/
static time_diff_ms_t _rtt; /* current RTT */
static int _rtt_valid = 0; /* current RTT validation flag */
static time_ms_t _last_rtt_time; /* last RTT time */


#define GET_MS_FROM_TV(tv) (((time_ms_t)(tv)->tv_sec) * 1000L + (time_ms_t)((tv)->tv_usec / 1000L))

static int _exclude_dst_valid = 0;
static struct in_addr _exclude_dst;
/* excluded ipv6 destination address/subnet */
static int _exclude_dst6_valid = 0;
static struct in6_addr _exclude_dst6;
static int _exclude_dst6_mask;

/* hash key for ipv4 tracking */
struct tcp_track_key_t {
    struct in_addr src;
    struct in_addr dst;

    uint16_t sport;
    uint16_t dport;
};

/* hash key for ipv6 tracking */
struct tcp6_track_key_t {
    struct in6_addr src;
    struct in6_addr dst;

    uint16_t sport;
    uint16_t dport;
};

/* hash element for ipv4 tracking */
struct tcp_track_t {
    struct tcp_track_key_t key;

    time_ms_t ts;
    uint32_t seq;
    uint32_t ack;
    UT_hash_handle hh;
};

/* hash element for ipv6 tracking */
struct tcp6_track_t {
    struct tcp6_track_key_t key;

    time_ms_t ts;
    uint32_t seq;
    uint32_t ack;
    UT_hash_handle hh;
};

static struct tcp_track_t* _track = NULL; /* hash table for ipv4 tracking */
static struct tcp6_track_t* _track6 = NULL; /* hash table for ipv6 tracking */

/**
 * @brief Find an ipv4 packet element in UT hash table by src/dst address/port.
 *
 * @param src is source address.
 * @param sport is source port.
 * @param dst is destination address.
 * @param dport is destination port.
 *
 * @return packet element.
 */
struct tcp_track_t* find_pkt(struct in_addr src, u_int16_t sport, struct in_addr dst, u_int16_t dport)
{
    struct tcp_track_key_t key = {{0,}, {0,}, 0,};
    struct tcp_track_t* pkt;

    /* build key */
    key.src = src;
    key.sport = sport;
    key.dst = dst;
    key.dport = dport;

    HASH_FIND(hh, _track, &key, sizeof(key), pkt);

    return pkt;
}

/**
 * @brief Find an ipv6 packet element in UT hash table by src/dst address/port.
 *
 * @param src is source address.
 * @param sport is source port.
 * @param dst is destination address.
 * @param dport is destination port.
 *
 * @return packet element.
 */
struct tcp6_track_t* find_pkt6(struct in6_addr src, u_int16_t sport, struct in6_addr dst, u_int16_t dport)
{
    struct tcp6_track_key_t key;
    struct tcp6_track_t* pkt;

    /* build key */
    key.src = src;
    key.sport = sport;
    key.dst = dst;
    key.dport = dport;

    HASH_FIND(hh, _track6, &key, sizeof(key), pkt);

    return pkt;
}

#ifdef DEBUG_VERBOSE
/**
 * @brief Get an ipv4 packet's forward string representation
 *
 * @param pkt is a packet element to extract string key
 *
 * @return static string key
 */
static const char* get_pkt_fstr(struct tcp_track_t *pkt)
{
    /* addresses and keys */
    char saddr[INET_ADDRSTRLEN];
    char daddr[INET_ADDRSTRLEN];
    static char fwdkey[TRACKKEYLEN + 1]; /* ip(15)|port(5)+ip(15)|port(5) + NUL */

    /* convert IP addresses to strings */
    inet_ntop(AF_INET, &pkt->key.src, saddr, sizeof(saddr));
    inet_ntop(AF_INET, &pkt->key.dst, daddr, sizeof(daddr));

    /* build forward string */
    sprintf(fwdkey, "%s|%u+%s|%u", saddr, ntohs(pkt->key.sport), daddr, ntohs(pkt->key.dport));

    return fwdkey;
}

/**
 * @brief Get an ipv6 packet's forward string representation
 *
 * @param pkt is a packet element to extract string key
 *
 * @return static string key
 */
static const char* get_pkt6_fstr(struct tcp6_track_t *pkt)
{
    /* addresses and keys */
    char saddr[INET6_ADDRSTRLEN];
    char daddr[INET6_ADDRSTRLEN];
    static char fwdkey[TRACKKEYLEN + 1];

    /* convert IP addresses to strings */
    inet_ntop(AF_INET6, &pkt->key.src, saddr, sizeof(saddr));
    inet_ntop(AF_INET6, &pkt->key.dst, daddr, sizeof(daddr));

    /* build forward */
    sprintf(fwdkey, "%s|%u+%s|%u", saddr, ntohs(pkt->key.sport), daddr, ntohs(pkt->key.dport));

    return fwdkey;
}
#endif

/**
 * @brief Update an ipv4/ipv6 packet element.
 *
 * @param pkt is a packet element to update.
 * @param new_ts is a Epoch time in msec.
 * @param new_seq is TCP seq number.
 * @param new_ack is TCP seq number in SYN/ACK.
 */
#define update_pkt(pkt, new_ts, new_seq, new_ack) \
    do { \
        pkt->ts = new_ts; \
        pkt->seq = new_seq; \
        pkt->ack = new_ack; \
    } while (0)

/**
 * @brief Add a new ipv4 packet element to the track collection.
 *
 * @param src is source address.
 * @param sport is source port.
 * @param dst is destination address.
 * @param dport is destination port.
 *
 * @return the packet element that is newly created or NULL on failure.
 *
 */
struct tcp_track_t* add_pkt(struct in_addr src, u_int16_t sport, struct in_addr dst, u_int16_t dport)
{
    struct tcp_track_t *pkt;

    /* allocate packet */
    pkt = (struct tcp_track_t *)calloc(sizeof(*pkt), 1);
    if (!pkt) {
        syslog(LOG_ERR, "failed to allocate tcp_track_t");
        goto err;
    }

    /* build key */
    pkt->key.src = src;
    pkt->key.sport = sport;
    pkt->key.dst = dst;
    pkt->key.dport = dport;

    /* add hash entry */
    HASH_ADD(hh, _track, key, sizeof(pkt->key), pkt);

    /* increase packet count */
    _pkt_count++;

    return pkt;

err:
    return NULL;
}

/**
 * @brief Add a new ipv6 packet element to the track collection.
 *
 * @param src is source address.
 * @param sport is source port.
 * @param dst is destination address.
 * @param dport is destination port.
 *
 * @return the packet element that is newly created or NULL on failure.
 */
struct tcp6_track_t* add_pkt6(struct in6_addr src, u_int16_t sport, struct in6_addr dst, u_int16_t dport)
{
    struct tcp6_track_t *pkt;

    /* allocate packet */
    pkt = (struct tcp6_track_t *)calloc(sizeof(*pkt), 1);
    if (!pkt) {
        syslog(LOG_ERR, "failed to allocate tcp6_track_t");
        goto err;
    }

    /* build key */
    pkt->key.src = src;
    pkt->key.sport = sport;
    pkt->key.dst = dst;
    pkt->key.dport = dport;

    /* add hash entry */
    HASH_ADD(hh, _track6, key, sizeof(pkt->key), pkt);

    /* increase packet count */
    _pkt6_count++;

    return pkt;

err:
    return NULL;
}

/**
 * @brief Delete an ipv4 packet element.
 *
 * @param pkt is a packet element to delete.
 */
void del_pkt(struct tcp_track_t *pkt)
{
    /* delete hash entry */
    HASH_DEL(_track, pkt);
    free(pkt);

    /* decrease packet count */
    _pkt_count--;
}

/**
 * @brief Delete an ipv6 packet element.
 *
 * @param pkt is a packet element to delete.
 */
void del_pkt6(struct tcp6_track_t *pkt)
{
    /* delete hash entry */
    HASH_DEL(_track6, pkt);
    free(pkt);

    /* decrease packet count */
    _pkt6_count--;
}

/**
 * @brief Delete the ipv4 track collection.
 */
void del_track(void)
{
    struct tcp_track_t *pkt, *tmp;

    HASH_ITER(hh, _track, pkt, tmp) {
        del_pkt(pkt);
    }
}

/**
 * @brief Delete the ipv6 track collection.
 */
void del_track6(void)
{
    struct tcp6_track_t *pkt, *tmp;

    HASH_ITER(hh, _track6, pkt, tmp) {
        del_pkt6(pkt);
    }
}

/**
 * @brief Flush RTT into RDB every RDB write interval.
 *
 *
 * @param ts is current Epoch time in msec.
 */
void refresh_rdb(time_ms_t ts)
{

    /* bypass if timer is not expired */
    if (ts - _last_rtt_time < RDB_WRITE_INTERVAL) {
        return;
    }

    #ifdef DEBUG_VERBOSE
    syslog(LOG_DEBUG, "rdb refresh time got expired. (ts=%u)", ts);
    #endif

    if (_rtt_valid) {
        /* set rdb */
        rdb_set_printf(_rdb, "%u", _rtt);

        /* update last rtt time */
        _last_rtt_time = ts;
    }

    _rtt_valid = 0;
}

/**
 * @brief Remove a packet element when the ipv4 track collection has too many packets.
 */
void drop_track_if_too_many()
{
    struct tcp_track_t *pkt, *tmp;

    /* drop if we have too many of packets */
    HASH_ITER(hh, _track, pkt, tmp) {
        if (_pkt_count < MAX_TRACK_PACKET) {
            break;
        }

        #ifdef DEBUG_VERBOSE
        syslog(LOG_DEBUG, "%s ts:%u - drop", get_pkt_fstr(pkt), pkt->ts);
        #endif
        del_pkt(pkt);
    }

}

/**
 * @brief Remove a packet element when the ipv6 track collection has too many packets.
 */
void drop_track6_if_too_many()
{
    struct tcp6_track_t *pkt, *tmp;

    /* drop if we have too many of packets */
    HASH_ITER(hh, _track6, pkt, tmp) {
        if (_pkt6_count < MAX_TRACK_PACKET) {
            break;
        }

        #ifdef DEBUG_VERBOSE
        syslog(LOG_DEBUG, "%s ts:%u - drop", get_pkt6_fstr(pkt), pkt->ts);
        #endif
        del_pkt6(pkt);
    }

}

/**
 * @brief Remove too old packet elements from the ipv4/ipv6 track collections.
 *
 * @param ts is current Epoch time in msec.
 */
void refresh_track(time_ms_t ts)
{
    time_diff_ms_t elapsed = ts - _last_gg_time;

    /* bypass if it is not yet to collect */
    if (elapsed >= _max_rtt) {
        #ifdef DEBUG_VERBOSE
        syslog(LOG_DEBUG, "garbage collection time got expired.");
        #endif

        if (_enable_ip4) {
            struct tcp_track_t *pkt, *tmp;

            HASH_ITER(hh, _track, pkt, tmp) {
                /* bypass if not expired yet */
                if (ts - pkt->ts < _max_rtt) {
                    continue;
                }

                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "%s ts:%u - expired (count=%d)", get_pkt_fstr(pkt), pkt->ts, _pkt_count);
                #endif

                del_pkt(pkt);
            }
        }

        if (_enable_ip6) {
            struct tcp6_track_t *pkt, *tmp;

            HASH_ITER(hh, _track6, pkt, tmp) {
                /* bypass if not expired yet */
                if (ts - pkt->ts < _max_rtt) {
                    continue;
                }

                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "%s ts:%u - expired (count=%d)", get_pkt6_fstr(pkt), pkt->ts, _pkt6_count);
                #endif

                del_pkt6(pkt);
            }
        }

        /* update GG */
        _last_gg_time = ts;
    }
}

/**
 * @brief check if an IPv6 address is within a subnet.
 *
 * @param addr points to the IPv6 address to be checked.
 * @param net points to the subnet address.
 * @param mask the number of bits of the subnet: 1-128.
 */
static int ip6_in_net(const struct in6_addr * addr, const struct in6_addr * net, int mask)
{
    int bytes = mask / 8;
    int bits = mask % 8;
    uint8_t bmask = bits ? (uint8_t) (-128 >> (bits - 1)) : 0;
    int i;
    for (i = 0; i < bytes; i++) {
        if (addr->s6_addr[i] != net->s6_addr[i]) {
            return 0;
        }
    }
    if (bytes >= 16 || bits == 0) {
        return 1;
    }
    return (addr->s6_addr[bytes] & bmask) == (net->s6_addr[bytes] & bmask);
}

/**
 * @brief Track packets and calculate RTT.
 *
 * @param args points to the args from pcap loop.
 * @param header points to pcap packet capture header.
 * @param packet points to pcap raw packet.
 */
void pkt_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
    const struct timeval * tv = &header->ts;
    time_ms_t ts = GET_MS_FROM_TV(tv);

    /* headers */
    const struct ip* ip_header;
    const struct ip6_hdr* ip6_header = NULL;
    int ip_header_length;

    const struct tcphdr* tcp_header;

    int is_ip6; /* whether this is an ipv6 packet */

    #ifdef DEBUG_VERBOSE
    /* addresses and keys */
    char saddr[ADDRSTRLEN + 1];
    char daddr[ADDRSTRLEN + 1];
    char src[ADDRPORTSTRLEN + 1]; /* ip|port + NUL */
    char dst[ADDRPORTSTRLEN + 1];
    char fwdkey[TRACKKEYLEN + 1]; /* ip|port+ip|port + NUL */
    char revkey[TRACKKEYLEN + 1];
    #endif

    /* TCP members */
    u_char th_flags;
    uint32_t th_seq;
    uint32_t th_ack;

    struct tcp_track_t * pkt;
    struct tcp6_track_t * pkt6;
    time_diff_ms_t rtt;

    #ifdef DEBUG_VERBOSE
    const char* flags_str;
    #endif

    /*
     /////////////////////////////////////////////////////////////////////////////
     // get ip and tcp header pointers
     /////////////////////////////////////////////////////////////////////////////
    */

    /* skip datalink and get IP header */
    packet += _dlh_sz;
    ip_header = (const struct ip *)packet;
    is_ip6 = ip_header->ip_v == 6;

    #ifdef DEBUG_VERBOSE
    syslog(LOG_DEBUG, "got an IPv%d packet", is_ip6 ? 6 : 4);
    #endif

    if (is_ip6) {
        ip6_header = (const struct ip6_hdr *)packet;
        ip_header_length = sizeof(struct ip6_hdr);
    } else {
        ip_header_length = ip_header->ip_hl * 4;
    }

    /* skip ip header and get TCP header */
    packet += ip_header_length;
    tcp_header = (const struct tcphdr *)packet;


    /*
     /////////////////////////////////////////////////////////////////////////////
     // build keys and addresses
     /////////////////////////////////////////////////////////////////////////////
    */

    #ifdef DEBUG_VERBOSE
    /* convert IP addresses to strings */
    if (is_ip6) {
        inet_ntop(AF_INET6, &ip6_header->ip6_src, saddr, sizeof(saddr));
        inet_ntop(AF_INET6, &ip6_header->ip6_dst, daddr, sizeof(daddr));
    } else {
        inet_ntop(AF_INET, &ip_header->ip_src, saddr, sizeof(saddr));
        inet_ntop(AF_INET, &ip_header->ip_dst, daddr, sizeof(daddr));
    }

    /* build IP|port strings */
    sprintf(src, "%s|%u", saddr, ntohs(tcp_header->th_sport));
    sprintf(dst, "%s|%u", daddr, ntohs(tcp_header->th_dport));

    /* build forward and reverse key */
    sprintf(fwdkey, "%s+%s", src, dst);
    sprintf(revkey, "%s+%s", dst, src);
    #endif

    /*
     /////////////////////////////////////////////////////////////////////////////
     // track SYN, SYN/ACK and ACK
     /////////////////////////////////////////////////////////////////////////////
    */

    /* get TCP members */
    th_flags = tcp_header->th_flags;
    th_seq = ntohl(tcp_header->th_seq);
    th_ack = ntohl(tcp_header->th_ack);

    #ifdef DEBUG_VERBOSE
    switch (th_flags) {
        case TH_SYN:
            flags_str = "SYN";
            break;

        case TH_SYN | TH_ACK:
            flags_str = "SYN,ACK";
            break;

        case TH_ACK:
            flags_str = "ACK";
            break;

        default:
            flags_str = "OTHERS";
            break;
    }


    syslog(LOG_DEBUG, "%s ts:%u seq:%u ack:%u flag:%s,0x%02x - got", fwdkey, ts, th_seq, th_ack, flags_str, th_flags);
    #endif

    /* keep track in sustainable size */
    if (_enable_ip4) {
        drop_track_if_too_many();
    }
    if (_enable_ip6) {
        drop_track6_if_too_many();
    }

    /* perform garbage collection */
    refresh_track(ts);

    /* perform rdb activity */
    refresh_rdb(ts);

    switch (th_flags) {
        case TH_SYN:
            if ((!is_ip6 && _exclude_dst_valid &&
                 _exclude_dst.s_addr == ip_header->ip_dst.s_addr) ||
                (is_ip6 && _exclude_dst6_valid &&
                 ip6_in_net(&ip6_header->ip6_dst, &_exclude_dst6, _exclude_dst6_mask))) {
                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "%s ts:%u seq:%u - ignore pkt(SYNC)", fwdkey, ts, th_seq);
                #endif

                break;
            }

            /* find or create a new tcp track */
            if (is_ip6) {
                pkt6 = find_pkt6(ip6_header->ip6_src, tcp_header->th_sport, ip6_header->ip6_dst, tcp_header->th_dport);
                if (!pkt6) {
                    #ifdef DEBUG_VERBOSE
                    syslog(LOG_DEBUG, "%s ts:%u seq:%u - add pkt(SYNC)", fwdkey, ts, th_seq);
                    #endif
                    pkt6 = add_pkt6(ip6_header->ip6_src, tcp_header->th_sport, ip6_header->ip6_dst, tcp_header->th_dport);
                    if (!pkt6) {
                        break;
                    }
                } else {
                    #ifdef DEBUG_VERBOSE
                    syslog(LOG_DEBUG, "%s ts:%u seq:%u - reuse pkt(SYNC)", fwdkey, ts, th_seq);
                    #endif
                }

                /* update tcp track - update seq and ack to track */
                update_pkt(pkt6, ts, th_seq, 0);
            } else {
                pkt = find_pkt(ip_header->ip_src, tcp_header->th_sport, ip_header->ip_dst, tcp_header->th_dport);
                if (!pkt) {
                    #ifdef DEBUG_VERBOSE
                    syslog(LOG_DEBUG, "%s ts:%u seq:%u - add pkt(SYNC)", fwdkey, ts, th_seq);
                    #endif
                    pkt = add_pkt(ip_header->ip_src, tcp_header->th_sport, ip_header->ip_dst, tcp_header->th_dport);
                    if (!pkt) {
                        break;
                    }
                } else {
                    #ifdef DEBUG_VERBOSE
                    syslog(LOG_DEBUG, "%s ts:%u seq:%u - reuse pkt(SYNC)", fwdkey, ts, th_seq);
                    #endif
                }

                /* update tcp track - update seq and ack to track */
                update_pkt(pkt, ts, th_seq, 0);
            }

            break;


        case TH_SYN | TH_ACK:
            if (is_ip6) {
                /* find tcp track */
                pkt6 = find_pkt6(ip6_header->ip6_dst, tcp_header->th_dport, ip6_header->ip6_src, tcp_header->th_sport);
                if (!pkt6) {
                    break;
                }

                /* bypass if tracking information is not matching */
                if (th_ack != pkt6->seq + 1) {
                    break;
                }

                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "%s ts:%u seq:%u ack:%u - track(SYN,ACK)", get_pkt6_fstr(pkt6), pkt6->ts, pkt6->seq, pkt6->ack);
                #endif

                /* finish packet if track mode is 0 */
                if (_track_mode == 0) {
                    goto fini_tracking;
                }

                /* update ack to track */
                pkt6->ack = th_seq;
                pkt6->seq = pkt6->seq + 1;
            } else {
                /* find tcp track */
                pkt = find_pkt(ip_header->ip_dst, tcp_header->th_dport, ip_header->ip_src, tcp_header->th_sport);
                if (!pkt) {
                    break;
                }

                /* bypass if tracking information is not matching */
                if (th_ack != pkt->seq + 1) {
                    break;
                }

                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "%s ts:%u seq:%u ack:%u - track(SYN,ACK)", get_pkt_fstr(pkt), pkt->ts, pkt->seq, pkt->ack);
                #endif

                /* finish packet if track mode is 0 */
                if (_track_mode == 0) {
                    goto fini_tracking;
                }

                /* update ack to track */
                pkt->ack = th_seq;
                pkt->seq = pkt->seq + 1;
            }

            break;

        case TH_ACK:
            if (is_ip6) {
                /* find tcp track */
                pkt6 = find_pkt6(ip6_header->ip6_src, tcp_header->th_sport, ip6_header->ip6_dst, tcp_header->th_dport);
                if (!pkt6) {
                    break;
                }

                /* bypass if tracking information is not matching */
                if (th_seq != pkt6->seq || th_ack != pkt6->ack + 1) {
                    break;
                }

                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "%s ts:%u seq:%u ack:%u - track(ACK)", get_pkt6_fstr(pkt6), pkt6->ts, pkt6->seq, pkt6->ack);
                #endif
            } else {
                /* find tcp track */
                pkt = find_pkt(ip_header->ip_src, tcp_header->th_sport, ip_header->ip_dst, tcp_header->th_dport);
                if (!pkt) {
                    break;
                }

                /* bypass if tracking information is not matching */
                if (th_seq != pkt->seq || th_ack != pkt->ack + 1) {
                    break;
                }

                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "%s ts:%u seq:%u ack:%u - track(ACK)", get_pkt_fstr(pkt), pkt->ts, pkt->seq, pkt->ack);
                #endif
            }

fini_tracking:
            if (is_ip6) {
                /* calculate RTT */
                rtt = ts - pkt6->ts;

                /* set rdb */
                if (rtt < _max_rtt) {
                    #ifdef DEBUG_VERBOSE
                    syslog(LOG_DEBUG, "%s ts:%u ts2:%u rtt:%u - remove pkt", get_pkt6_fstr(pkt6), pkt6->ts, ts, rtt);
                    #endif

                    _rtt = rtt;
                    _rtt_valid = 1;
                } else {
                    #ifdef DEBUG_VERBOSE
                    syslog(LOG_DEBUG, "%s ts:%u ts2:%u rtt:%u - too big rtt (max=%d)", get_pkt6_fstr(pkt6), pkt6->ts, ts, rtt, _max_rtt);
                    #endif
                }

                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "hash info : [%s] count %u ", get_pkt6_fstr(pkt6), HASH_COUNT(_track6));
                #endif

                /* remove hash entry */
                del_pkt6(pkt6);
            } else {
                /* calculate RTT */
                rtt = ts - pkt->ts;

                /* set rdb */
                if (rtt < _max_rtt) {
                    #ifdef DEBUG_VERBOSE
                    syslog(LOG_DEBUG, "%s ts:%u ts2:%u rtt:%u - remove pkt", get_pkt_fstr(pkt), pkt->ts, ts, rtt);
                    #endif

                    _rtt = rtt;
                    _rtt_valid = 1;
                } else {
                    #ifdef DEBUG_VERBOSE
                    syslog(LOG_DEBUG, "%s ts:%u ts2:%u rtt:%u - too big rtt (max=%d)", get_pkt_fstr(pkt), pkt->ts, ts, rtt, _max_rtt);
                    #endif
                }

                #ifdef DEBUG_VERBOSE
                syslog(LOG_DEBUG, "hash info : [%s] count %u ", get_pkt_fstr(pkt), HASH_COUNT(_track));
                #endif

                /* remove hash entry */
                del_pkt(pkt);
            }
            break;
    }

}

/**
 * @brief Prints command line usage screen.
 *
 * @param fp is a FILE pointer to output the usage screen.
 */
void print_usage(FILE* fp)
{
    fprintf(fp,
            "\n"
            "passive_rtt_measure - passive rtt measure daemon v1.0\n"
            "\n"
            "usage>\n"
            "	passive_rtt_measure -i <network interface> -r <rdb name> -f <PCAP filter>\n"
            "\n"
            "options>\n"
            "	-h: print this help screen\n"
            "	-i : network interface name to measure RTT\n"
            "	-r : RDB name to write RTT result\n"
            "	-t : maximum RTT in msecond\n"
            "	-f : PCAP filter to capture RTT\n"
            "	-m : RTT packet track method (0=syn-syn/ack, 1=syn-syn/ack-ack)\n"
            "	-x : exclude destination IPv4 address\n"
            "	-X : exclude destination IPv6 address/subnet\n"
            "	-4 : enable IPv4\n"
            "	-6 : enable IPv6\n"
            "\n");
}

static int sig_term_caught = 0; /* true when signal got caught to terminate */

/**
 * @brief Handles signals and set termination flag.
 *
 * @param sig is a signal number.
 */
void sighandler(int sig)
{
    sig_term_caught = 1;

    /* break pcap loop */
    if (_pcaphndl) {
        pcap_breakloop(_pcaphndl);
    }
}

/**
 * @brief Parse an ipv6 network address string
 *
 * @param str The ipv6 network address as a string. e.g. 2001:db8:123::/48
 * @param addr A pointer to struct in6_addr to hold the parsed network address
 * @param mask An integer pointer to hold the parsed network mask
 * @note The content in str could be modified by this function
 */
static void parse_ip6_netmask(char * str, struct in6_addr * addr, int * mask)
{
    char * p;
    p = strchr(str, '/');
    if (!p) {
        *mask = 128;
    } else {
        *mask = atoi(p+1);
        *p = '\0';
    }
    inet_pton(AF_INET6, str, addr);
}

/**
 * @brief Monitors a network interface and measures RTT.
 *
 * @param argc is a command line option count.
 * @param argv[] are command line options.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int main(int argc, char *argv[])
{
    int opt;

    /* command line options */
    const char* opt_nwif = NULL;
    const char* opt_rdb = NULL;
    const char* opt_pcapfltr = NULL;
    int opt_max_rtt = 10000;
    int opt_track_mode = 0;
    int opt_enable_ip4 = 0;
    int opt_enable_ip6 = 0;

    int optarg_int;

    /* pcap parameters */
    const char* dev;
    const char* pcapfltr;

    char errbuf[PCAP_ERRBUF_SIZE]; /* pcap error buffer */

    int dlt; /* pcap datalink type */
    struct bpf_program fp; /* compiled filter expression */

    int retcode = -1;
    int stat;
    time_ms_t ts;
    struct timeval tv;

    /* get options */
    while ((opt = getopt(argc, argv, "hi:r:f:r:m:x:X:46")) != EOF) {
        switch (opt) {
            case 'h':
                print_usage(stdout);
                exit(0);
                break;

            case 'i':
                opt_nwif = optarg;
                break;

            case 'r':
                opt_rdb = optarg;
                break;

            case 'f':
                opt_pcapfltr = optarg;
                break;

            case 't':
                optarg_int = atoi(optarg);
                if (optarg_int) {
                    opt_max_rtt = optarg_int;
                }
                break;

            case 'm':
                opt_track_mode = atoi(optarg);
                break;

            case 'x':
                _exclude_dst_valid = 1;
                inet_pton(AF_INET, optarg, &(_exclude_dst));
                break;

            case 'X':
                _exclude_dst6_valid = 1;
                parse_ip6_netmask(optarg, &_exclude_dst6, &_exclude_dst6_mask);
                break;

            case '4':
                opt_enable_ip4 = 1;
                break;

            case '6':
                opt_enable_ip6 = 1;
                break;

            case ':':
                print_usage(stderr);
                exit(-1);
                break;

            case '?':
                print_usage(stderr);
                exit(-1);
                break;

            default:
                print_usage(stderr);
                exit(-1);
                break;
        }
    }

    /* check mandatory options */
    if (!opt_nwif || !opt_rdb) {
        fprintf(stderr, "mandatory options are missing");
        print_usage(stderr);
        exit(-1);
    }

    if (!opt_enable_ip6) {
        /* enable ipv4 if ipv6 is not enabled for backward compatibility */
        opt_enable_ip4 = 1;
    }

    /* select default filter */
    if (!opt_pcapfltr) {
        switch (opt_track_mode) {
            case 0:
                if (opt_enable_ip4 && opt_enable_ip6) {
                    /*
                     * pcap does not support embedded protocol bpf under ipv6
                     * currently we only track ipv6 without extension headers
                     * for efficiency purpose
                     */
                    opt_pcapfltr = "(ip6 && ip6[6] == 0x06 && (ip6[53] == 0x02 || ip6[53] == 0x12)) || tcp[13] == 0x02 || tcp[13] == 0x12";
                } else if (opt_enable_ip4) { /* ipv4 only */
                    opt_pcapfltr = "tcp[tcpflags] == tcp-syn || tcp[tcpflags] == (tcp-syn|tcp-ack)";
                } else { /* ipv6 only */
                    opt_pcapfltr = "ip6 && ip6[6] == 0x06 && (ip6[53] == 0x02 || ip6[53] == 0x12)";
                }
                break;

            default:
                if (opt_enable_ip4 && opt_enable_ip6) {
                    opt_pcapfltr = "(ip6 && ip6[6] == 0x06 && (ip6[53] & 0x12 != 0)) || (tcp[13] & 0x12 != 0)";
                } else if (opt_enable_ip4) {
                    opt_pcapfltr = "(tcp[tcpflags] & (tcp-syn|tcp-ack)) != 0";
                } else {
                    opt_pcapfltr = "ip6 && ip6[6] == 0x06 && (ip6[53] & 0x12 != 0)";
                }
                break;
        }
    }

    /* setup pcap parameters */
    dev = opt_nwif;
    pcapfltr = opt_pcapfltr;

    /* set global parameters */
    _rdb = opt_rdb;
    _max_rtt = opt_max_rtt;
    _track_mode = opt_track_mode;
    _enable_ip4 = opt_enable_ip4;
    _enable_ip6 = opt_enable_ip6;

    /* catch signals */
    signal(SIGHUP, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    /* initiate rdb */
    rdb_init();

    /*
     /////////////////////////////////////////////////////////////////////////////
     // initiate pcap - open
     /////////////////////////////////////////////////////////////////////////////
    */

    /* open pcap handle - 1000 msec timeout */
    _pcaphndl = pcap_open_live(dev, BUFSIZ, 0, 1000, errbuf);
    if (!_pcaphndl) {
        syslog(LOG_ERR, "couldn't open device %s: %s", dev, errbuf);
        goto fini;
    }

    /*
     /////////////////////////////////////////////////////////////////////////////
     // get pcap datalink header size
     /////////////////////////////////////////////////////////////////////////////
    */

    /* get datalink type */
    dlt = pcap_datalink(_pcaphndl);
    syslog(LOG_INFO, "device %s datalink type: %s (%d)", dev, pcap_datalink_val_to_name(dlt), dlt);


    /* get datalink header size */
    switch (dlt) {
        case DLT_LINUX_SLL:
            _dlh_sz = SLL_HDR_LEN; /* SLL header defined in pcap/sll.h */
            break;

        case DLT_EN10MB:
            _dlh_sz = sizeof(struct ether_header); /* ethernet header defined in net/ethernet.h */
            break;

        default:
            syslog(LOG_ERR, "unsupported datalink type (device %s datalink type: %s #%d)", dev, pcap_datalink_val_to_name(dlt), dlt);
            goto fini;
    }

    /*
     /////////////////////////////////////////////////////////////////////////////
     // compile and apply pcap filter
     /////////////////////////////////////////////////////////////////////////////
    */

    /* compile filter */
    if (pcap_compile(_pcaphndl, &fp, pcapfltr, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        syslog(LOG_ERR, "couldn't parse filter %s: %s", pcapfltr, pcap_geterr(_pcaphndl));
        goto fini;
    }

    /* set filter */
    if (pcap_setfilter(_pcaphndl, &fp) == -1) {
        syslog(LOG_ERR, "couldn't install filter %s: %s", pcapfltr, pcap_geterr(_pcaphndl));
        goto fini;
    }

    /*
     /////////////////////////////////////////////////////////////////////////////
     // prepare for pcap loop
     /////////////////////////////////////////////////////////////////////////////
    */


    /* get ms */
    gettimeofday(&tv, NULL);
    ts = GET_MS_FROM_TV(&tv);

    /* initiate start times */
    _last_rtt_time = ts;
    _last_gg_time = ts;

    /*
     /////////////////////////////////////////////////////////////////////////////
     // pcap loop
     /////////////////////////////////////////////////////////////////////////////
    */

    while (1) {
        /* pcap dispatch */
        stat = pcap_dispatch(_pcaphndl, 0, pkt_handler, NULL);
        if (stat == -2) {
            syslog(LOG_INFO, "pcap_dispatch() stops! - %s",strerror(errno));
            break;
        } else if (stat < 0) {
            syslog(LOG_ERR, "couldn't start loop - %s", strerror(errno));
            goto fini;
        }

        /* get ms */
        gettimeofday(&tv, NULL);
        ts = GET_MS_FROM_TV(&tv);

        /* perform garbage collection */
        refresh_track(ts);

        /* perform rdb activity */
        refresh_rdb(ts);
    }

    /* return success code */
    retcode = 0;
fini:
    del_track();
    del_track6();


    /* close pcap */
    if (_pcaphndl) {
        pcap_close(_pcaphndl);
    }

    /* finish rdb */
    rdb_fini();

    return retcode;
}
