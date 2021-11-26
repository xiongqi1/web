/*!
 * RDB interface for the project
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include "def.h"
#include "rdb.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#define die(...) \
    do { \
        fprintf(stderr, "ERROR: " __VA_ARGS__); \
        exit(-1); \
    } while (0)

int udp_fd = -1;
struct addrinfo *udp_ai = NULL;

void netsetup(void)
{
    const char *hostname = "localhost";
    const char *portname = "4711";
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_ADDRCONFIG;
    udp_ai = 0;
    int err = getaddrinfo(hostname, portname, &hints, &udp_ai);
    if (err != 0) {
        die("failed to udp_aiolve remote socket addudp_ais (err=%d)", err);
    }
    udp_fd = socket(udp_ai->ai_family, udp_ai->ai_socktype, udp_ai->ai_protocol);
    if (udp_fd == -1) {
        die("%s", strerror(errno));
    }
}

void sendstring(const char *msg, int size)
{
    int rval;
    if (size == 0)
        size = strlen(msg) + 1; // Include terminating '\0'
    rval = sendto(udp_fd, msg, size, MSG_NOSIGNAL, udp_ai->ai_addr, udp_ai->ai_addrlen);
    if (rval == -1) {
        die("%s", strerror(errno));
    }
}

//#define D(fmt,...) printf("RDB " fmt, ##__VA_ARGS__)
#define D(fmt, ...) \
    do { \
    } while (0)

/* Current time in seconds */
double now(void)
{
    static struct timeval T;
    double secs;
    gettimeofday(&T, NULL);
    secs = (double)T.tv_usec / 1e6 + T.tv_sec;
    return secs;
}

/*
        ### rdb functions ###
*/

/* Faking a minimal RDB here for config and such */
#define FRDB_MAXNAME 128
#define FRDB_MAXVAL 512

// Returns a sanitised version of the string (static buffer)
// When js is set, also replace ',' and ''' and '\'
const char *sanitise(const char *s, int js)
{
    static char buffer[FRDB_MAXVAL * 4]; // Enough space for every character escaped
    char *p = buffer;
    while (*s) {
        if (*s < ' ' || *s > 127) {
            p += sprintf(p, "\\x%02x", (unsigned char)*s);
        } else if (js && (*s == ',' || *s == '\'' || *s == '\\')) {
            p += sprintf(p, "\\x%02x", (unsigned char)*s);
        } else {
            *p = *s;
            p++;
        }
        s++;
    }
    *p = '\0';
    return buffer;
}

struct rdbe {
    struct rdbe *next;       // Simple linked list. Operates as stack with linear search
    int exists;              // Boolean flag - variable has been created
    int interesting;         // Boolean flag - variable will be dumped for websocket
    char name[FRDB_MAXNAME]; // Variable name
    char value[FRDB_MAXVAL]; // Variable content
};
struct rdbe *FRDB = NULL;
#define FILTER_ALL 0
#define FILTER_EXISTS 1
/* Returns pointer to frdb element matching name */
struct rdbe *frdb_find(const char *name, int filter)
{
    struct rdbe *e = FRDB;
    while (e) {
        if (strcmp(e->name, name) == 0) {
            break;
        }
        e = e->next;
    }
    // Skip entries that don't exist yet
    // if ((filter == FILTER_EXISTS) && (!e->exists)) return NULL;
    return e;
}

void frdb_dump_interesting(double t)
{
    struct rdbe *e = FRDB;
    printf("NOW='%.1f'", t);
    while (e) {
        if (e->interesting)
            printf(", %s='%s'", e->name, sanitise(e->value, 1));
        e = e->next;
    }
    printf("\n");
    fflush(stdout);
}

void frdb_netdump_interesting(double t)
{
/* Dump to UDP. split into separate packets if too long.
 * MAXPACKET will be exceeded by one line at most */
#define MAXPACKET 1000
#define MAXLINE 256
    static unsigned sequence = 0;
    char buffer[MAXPACKET + MAXLINE] = ""; // Should not exceed MTU
    int offset = 0;
    struct rdbe *e = FRDB;
    offset += sprintf(buffer + offset, "NOW='%.1f',", t);
    while (e) {
        if (e->interesting) {
            if (offset >= MAXPACKET) {
                /* Send and reset */
                sendstring(buffer, offset + 1); // Include \0
                buffer[0] = '\0';
                offset = 0;
            }
            offset += sprintf(buffer + offset, "%s='%s',", e->name, sanitise(e->value, 1));
        }
        e = e->next;
    }
    offset += sprintf(buffer + offset, "end='%d',", sequence++);
    if (offset)
        sendstring(buffer, offset + 1); // Include \0
}

void frdb_set(const char *name, const char *val)
{
    static double t0 = 0.0;
    struct rdbe *e = frdb_find(name, FILTER_ALL);
    if (!e) {
        e = calloc(1, sizeof(*e));
        strncpy(e->name, name, FRDB_MAXNAME);
        e->next = FRDB;
        FRDB = e;
    }
    if (e->exists) {
        double t = now();
        D("SET %.0f %s, '%s'\n", t, name, sanitise(val, 0));
        if ((t - t0) >= 1.0) {
            t0 = t;
            frdb_dump_interesting(t);
            frdb_netdump_interesting(t);
        }
    } else {
        D("CRT %s, '%s'\n", name, sanitise(val, 0));
    }
    e->exists = 1;
    strncpy(e->value, val, FRDB_MAXVAL);
    e->value[FRDB_MAXVAL - 1] = '\0';
}

void frdb_set_interesting(const char *name)
{
    struct rdbe *e = frdb_find(name, FILTER_ALL);
    if (!e) {
        frdb_set(name, "");
        e = frdb_find(name, FILTER_ALL);
        e->exists = 0;
    }
    e->interesting = 1;
}

int frdb_get(const char *name, char *val)
{
    struct rdbe *e = frdb_find(name, FILTER_EXISTS);
    if (!e) {
        D("NOT %s\n", name);
        val = "";
        return -1;
    }
    strncpy(val, e->value, FRDB_MAXVAL);
    D("GET %s, '%s'\n", name, sanitise(val, 0));
    return strlen(e->value);
}

void frdb_subscribe(const char *name)
{
    D("SUB %s\n", name);
}

void frdb_dump(void)
{
    struct rdbe *e = FRDB;
    while (e) {
        D("DMP %s, %s\n", e->name, sanitise(e->value, 0));
        e = e->next;
    }
}

/* Fake RDB lib calls */
struct rdb_session *_s = NULL;
int rdb_fd(struct rdb_session *x)
{
    return 0;
}
void dbenum_destroy(struct dbenum_t *pEnum)
{
    return;
}
int dbenum_enumDb(struct dbenum_t *pEnum)
{
    return 0;
}
struct dbenumitem_t *dbenum_findNext(struct dbenum_t *pEnum)
{
    return NULL;
}
struct dbenumitem_t *dbenum_findFirst(struct dbenum_t *pEnum)
{
    return NULL;
}
struct dbenum_t *dbenum_create(struct rdb_session *s, int nFlags)
{
    return NULL;
}

/* rdb mutex */
static pthread_mutex_t rdb_mutex;

char _rdb_prefix[RDB_MAX_NAME_LEN];

void rdb_enter_csection()
{
    pthread_mutex_lock(&rdb_mutex);
}

void rdb_leave_csection()
{
    pthread_mutex_unlock(&rdb_mutex);
}

/*
 Close RDB.

 Params:
  None

 Return:
  None
*/
void rdb_fini()
{
    D("FIN\n");

    /* destroy rdb mutex */
    pthread_mutex_destroy(&rdb_mutex);
}

/*
 Open RDB.

 Params:
  None

 Return:
  0 = success. Otherwise, failure.
*/
int rdb_init()
{
    /* create rdb mutex */
    pthread_mutex_init(&rdb_mutex, NULL);

    D("INI\n");

    netsetup();

    frdb_set("qdiagd.config.rdb_update_interval", "5");
    frdb_set("qdiagd.config.servcell_sampling_interval", "5");
    frdb_set("qdiagd.config.rrc_rdb_update_interval", "5");

    // frdb_set("link.profile.1.apn","telstra.internet");

    static const char *interesting[] = { "wwan.0.radio_stack.rrc_stat.rrc_stat",
                                         "wwan.0.servcell_info.total_prbs_received",
                                         "wwan.0.servcell_info.max_ue_tx_power",
                                         "wwan.0.servcell_info.mac_i_bler_received",
                                         "wwan.0.servcell_info.max_ul_harq_transmissions",
                                         "wwan.0.servcell_info.mac_i_bler_received",
                                         "wwan.0.rrc_session.0.rrc_release_cause",
                                         "lte.rrc.session.rlc_dl_bytes",
                                         "lte.rrc.session.rlc_ul_bytes",
                                         "lte.rrc.3.cell_freq",
                                         "lte.rrc.3.cell_band_ul",
                                         "lte.rrc.3.cell_band_dl",
                                         "lte.rrc.3.scell_bandname",
                                         "lte.rrc.3.scell_band",
                                         "lte.rrc.3.scell_rsrq",
                                         "lte.rrc.3.scell_rsrp",
                                         "lte.rrc.3.scell_earfcn",
                                         "lte.rrc.3.scell_phys_id",
                                         "lte.rrc.2.cell_freq",
                                         "lte.rrc.2.cell_band_ul",
                                         "lte.rrc.2.cell_band_dl",
                                         "lte.rrc.2.scell_bandname",
                                         "lte.rrc.2.scell_band",
                                         "lte.rrc.2.scell_rsrq",
                                         "lte.rrc.2.scell_rsrp",
                                         "lte.rrc.2.scell_earfcn",
                                         "lte.rrc.2.scell_phys_id",
                                         "lte.rrc.1.cell_freq",
                                         "lte.rrc.1.cell_band_ul",
                                         "lte.rrc.1.cell_band_dl",
                                         "lte.rrc.1.scell_bandname",
                                         "lte.rrc.1.scell_band",
                                         "lte.rrc.1.scell_rsrq",
                                         "lte.rrc.1.scell_rsrp",
                                         "lte.rrc.1.scell_earfcn",
                                         "lte.rrc.1.scell_phys_id",
                                         "lte.rrc.0.cell_freq",
                                         "lte.rrc.0.cell_band_ul",
                                         "lte.rrc.0.cell_band_dl",
                                         "lte.rrc.0.scell_bandname",
                                         "lte.rrc.0.scell_band",
                                         "lte.rrc.0.scell_rsrq",
                                         "lte.rrc.0.scell_rsrp",
                                         "lte.rrc.0.scell_earfcn",
                                         "lte.rrc.0.scell_phys_id",
                                         "wwan.0.radio_stack.nr5g.up",
                                         "wwan.0.radio_stack.nr5g.scs",
                                         "wwan.0.radio_stack.nr5g.bw",
                                         "wwan.0.radio_stack.nr5g.pci",
                                         "wwan.0.radio_stack.nr5g.arfcn",
                                         "wwan.0.radio_stack.nr5g.mcs",
                                         "wwan.0.radio_stack.nr5g.layers",
                                         "wwan.0.radio_stack.nr5g.rsrp",
                                         "wwan.0.radio_stack.nr5g.rsrq",
                                         "wwan.0.radio_stack.nr5g.snr",
                                         "wwan.0.radio_stack.nr5g.ssb_index",
                                         "wwan.0.radio_stack.nr5g.total_tb",
                                         "wwan.0.radio_stack.nr5g.bad_tb",
                                         "wwan.0.radio_stack.nr5g.good_bytes",
                                         "wwan.0.radio_stack.nr5g.bad_bytes",
                                         "" };
    const char **ip = interesting;
    while (*ip[0] != '\0') {
        frdb_set_interesting(*ip);
        ip++;
    }

    // frdb_dump();

    return 0;
}

/*
 Subscribe RDB.

 Params:
  rdb : rdb variable to subscribe.

 Return:
  0 = success. Otherwise, failure.
*/
int __rdb_subscribe(const char *rdb)
{
    frdb_subscribe(rdb);
    return 0;
}

/*
 Read RDB with no error logging.

 Params:
  str : buffer to get RDB value.
  str_len : size of buffer.
  rdb : rdb to read.

 Return:
  Return RDB value. Otherwise, blank string.
*/
char *_rdb_get_str_quiet(char *str, int str_len, const char *rdb)
{
    frdb_get(rdb, str);
    return str;
}

int _rdb_exists(const char *rdb)
{
    D("EXI %s\n", rdb);
    return frdb_find(rdb, FILTER_EXISTS) != NULL;
}

/*
 Read RDB.

 Params:
  str : buffer to get RDB value.
  str_len : size of buffer.
  rdb : rdb to read.

 Return:
  Return RDB value. Otherwise, blank string.
*/
char *_rdb_get_str(char *str, int str_len, const char *rdb)
{
    frdb_get(rdb, str);
    return str;
}

/*
 Read RDB as long long integer.

 Params:
  rdb : rdb to read.

 Return:
  Return integer RDB value. Otherwise, zero.
*/
long long _rdb_get_int(const char *rdb)
{
    struct rdbe *e = frdb_find(rdb, FILTER_EXISTS);
    long long v;
    if (e) {
        v = atoi(e->value);
    } else {
        v = 0;
    }
    D("RIN %s, %lld\n", rdb, v);
    return v;
}

/*
 Read RDB as long long integer with no error logging.

 Params:
  rdb : rdb to read.

 Return:
  Return integer RDB value. Otherwise, zero.
*/
long long _rdb_get_int_quiet(const char *rdb)
{
    return _rdb_get_int(rdb);
}

/*
 Write a value to RDB

 Params:
  rdb : rdb to write.
  val : value to write the rdb variable.

 Return:
  0 = success. Otherwise, failure.

 Note:
  This function creates RDB variable when the value does not exist.
*/
int _rdb_set_str(const char *rdb, const char *val)
{
    /* use blank string if val is NULL */
    if (!val)
        val = "";
    frdb_set(rdb, val);
    return 0;
}

int _rdb_set_reset(const char *rdb)
{
    frdb_set(rdb, "");
    return 0;
}

int _rdb_set_uint(const char *rdb, unsigned long long val)
{
    char s[100];
    sprintf(s, "%llu", val);
    frdb_set(rdb, s);
    return 0;
}

int _rdb_set_tenths_decimal(const char *rdb, long double val)
{
    char s[100];
    sprintf(s, "%.1LF", val);
    frdb_set(rdb, s);
    return 0;
}

int _rdb_set_int(const char *rdb, long long val)
{
    char s[100];
    sprintf(s, "%lld", val);
    frdb_set(rdb, s);
    return 0;
}

const char *_rdb_get_prefix_rdb(char *var, int var_len, const char *rdb)
{
    snprintf(var, var_len, "%s%s", _rdb_prefix, rdb);
    // D("GETPREFIX %s -> %s\n", rdb, var);
    return var;
}

const char *_rdb_get_suffix_rdb(char *var, int var_len, const char *rdb, const char *suffix)
{
    snprintf(var, var_len, "%s%s%s", _rdb_prefix, rdb, suffix);
    // D("GETSUFFIX %s,%s -> %s\n", rdb, suffix, var);
    return var;
}

/*
 Reset RDB values that starts with the prefix

 Params:
  prefix : prefix to reset.

 Return:
  None
*/
void reset_rdb_sets(const char *prefix)
{
    D("RESET %s*\n", prefix);
}
