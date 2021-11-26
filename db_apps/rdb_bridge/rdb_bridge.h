#ifndef RDB_BRIDGE_H_12453010042019
#define RDB_BRIDGE_H_12453010042019
/*
 * RDB bridge daemon global data structure and initialisation
 *
 * Borrowed some codes from cdcs_apps/padd, db_apps/timedaemon
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

#include "options.h"
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>

typedef enum status_t_ {
    INIT,
    DISCONNECTED,
    CONNECTED,
    SYNCHRONISED
} status_t;

/// Global data structures and variable (G)
struct global {
    int ExitSig;            /* Set when a signal is caught */
    int SigPipe;            /* Gets set if socket operation fails. */

    int masterfd;           /* Master socket FD */
    int remotefd;           /* Remote socket FD */

    long long last_net;     /* Timestamp of last network manager call */

    /* A flag whether first synchronisation is done
     * This is important because when RDB bridge daemon is launching, RDB template daemon
     * already browsed all subscribed variables so only changed variables after then will
     * be triggered but RDB bridge daemons need to synchronise all variables at lease once
     * each other.
     */
    int init_sync;

    status_t status;

    int delayed_trigger;    /* Total number of variables that is triggered before */
                            /* next rate limit time frame so delayed trigger event */
};
extern struct global G;   /* Declared in rdb_bridge.c */

/**
 * Initialise global variable
 *
 * @param   g     A pointer to the global variable
 *
 * @retval  0
 */
static inline int GlobalInit(struct global *g)
{
    memset(g, 0, sizeof(struct global));
    g->ExitSig = 0;
    g->SigPipe = 0;
    g->masterfd = -1;
    g->remotefd = -1;
    g->last_net = 0;
    g->init_sync = 0;
    g->status = INIT;
    g->delayed_trigger = 0;
    return 0;
}

#define CONFIG_MAX_LINE_LEN   (1024+256)
#define RDB_NAME_MAX_LEN      1024
#define RDB_VALUE_MAX_LEN     1024
#define MAX_CHILD_VARS        100
#define NETBUF_SIZE           2048
#define _STR(s) #s
#define STR(s) _STR(s)

/// RDB variable structure
struct rdb_var {
    char *name;
    char *alias_name;
    int rate_limit;             /* any value between 0~3600000(ms) */
    int triggered;              /* 1 : triggered before next rate limit time frame so delayed */
                                /* 0 : not triggered */
    struct timeval last_time;   /* Last triggered time */
    int var_num;                /* total number of RDB variable in this structure */
    char *child[MAX_CHILD_VARS];/* child variables, same rate limit */
                                /* max number changes in run-time */
    int force_sync;             /* 0 : Skip during initial synchronisation */
                                /* 1 : Synchronise during every initial synchronisation */
                                /* 2 : Synchronise only if this variable was triggered during offline period */
};

#define SAFE_STRNCPY(d,s,l)	{ strncpy((d),(s),(l)); (d)[(l)-1]=0; } while(0)

#endif
