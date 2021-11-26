/**
 * EPS bearer RDB module - convert PDN activities into RDB.
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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "avg.h"
#include "def.h"
#include "eps_bearer_rdb.h"
#include "rdb.h"

#include "lte_nas_parser.h"

#define VALIDATE(condition) if(!(condition)) {return;}

/* bearer failure cause count structure */
struct bearer_fc_count_t {
    long long counts[MAX_EMM_FAILURE_CAUSE];
};

/* RDB statistics information - information per bearer */
struct bearer_stat_t {
    long long bearer_act_attempts;
    long long bearer_act_failures;
    long long bearer_act_accepts;
    struct avg_t avg_bearer_act_latency;
    long long bearer_deact_attempts;
    long long bearer_deact_failures;
    long long bearer_deact_count;
    long long bearer_deact_abnormal_count;
    struct avg_t avg_bearer_duration;

    struct bearer_fc_count_t fcs[nas_message_type_max];
};

/* bearer session information - information per session */
struct bearer_session_t {
    int index;

    time_t start_time;
    time_t start_time_monotonic;

    int used;
    char apn[MAX_APN_BUFFER_SIZE];
    ;
    struct bearer_stat_t default_bearer;
    struct bearer_stat_t dedicated_bearer;
};

/* private information - mainly for timestamps and connection to RDB session and statistics information per PDN bearer */
struct bearer_priv_t {

    struct pdn_entity_t *pdn_bearer; /* pdn bearer */
    struct bearer_session_t *sess;   /* bearer session */
    struct bearer_stat_t *stat;      /* bearer statistics */

    /* QxDM time-stamps for bearer activities */
    unsigned long long qctm_creat;
    unsigned long long qctm_preconn_req;
    unsigned long long qctm_conn_req;
    unsigned long long qctm_conn_fail;
    unsigned long long qctm_conn_accpt;
    unsigned long long qctm_predisconn_req;
    unsigned long long qctm_disconn_req;
    unsigned long long qctm_disconn;
};

struct eps_bearer_rdb_t {
    struct bearer_session_t sess[MAX_BEARER_SESSION]; /* sessions */
    int def_sess;                                     /* primary session */
};

/* EPS bearer RDB main singleton object */
static struct eps_bearer_rdb_t ebr_singletone;
static struct eps_bearer_rdb_t *ebr = &ebr_singletone;

/*
 Reset bearer statistics into RDBs (wwan.x.pdpcontext.x.default_bearer_xxxx).

 Parameters:
  bname : Prefix for statistic RDB names - "default" or "dedicated".
  sess_idx : Session index for statistic RDB names - from 0 to MAX_BEARER_SESSION-1.
  stat : Bearer statistics.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int reset_bearer_stat_to_rdb(const char *bname, int sess_idx, const struct bearer_stat_t *stat)
{
    char nm[RDB_MAX_NAME_LEN];

    int r;

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_act_attempts", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_act_failures", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_act_accepts", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_avg_bearer_act_latency", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_deact_attempts", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_deact_failures", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_deact_count", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_avg_bearer_duration", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_act_failure_causes", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_reset(nm);

    if (0 == strcmp(bname, "default")) {
        r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_normal_releases", _rdb_prefix, sess_idx, bname);
        if (r < 0)
            return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
        _rdb_set_reset(nm);

        r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_abnormal_releases", _rdb_prefix, sess_idx, bname);
        if (r < 0)
            return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
        _rdb_set_reset(nm);
    }

    return 0;
}

/*
 Write bearer statistics into RDBs (wwan.x.pdpcontext.x.default_bearer_xxxx).

 Parameters:
  bname : Prefix for statistic RDB names - "default" or "dedicated".
  sess_idx : Session index for statistic RDB names - from 0 to MAX_BEARER_SESSION-1.
  stat : Bearer statistics.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int write_bearer_stat_to_rdb(const char *bname, int sess_idx, const struct bearer_stat_t *stat)
{
    char nm[RDB_MAX_NAME_LEN];
    char val[RDB_MAX_VAL_LEN];

    int rc;

    int j;
    int k;
    int i;

    int r;

    const long long *fc_count;
    const char *cause_name;
    const struct bearer_fc_count_t *fc;

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_act_attempts", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_int(nm, stat->bearer_act_attempts);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_act_failures", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_int(nm, stat->bearer_act_failures);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_act_accepts", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_int(nm, stat->bearer_act_accepts);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_avg_bearer_act_latency", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_int(nm, __round(stat->avg_bearer_act_latency.avg));

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_deact_attempts", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_int(nm, stat->bearer_deact_attempts);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_deact_failures", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_int(nm, stat->bearer_deact_failures);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_deact_count", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_int(nm, stat->bearer_deact_count);

    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_avg_bearer_duration", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_int(nm, __round(stat->avg_bearer_duration.avg));

    if (0 == strcmp(bname, "default")) {
        r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_normal_releases", _rdb_prefix, sess_idx, bname);
        if (r < 0)
            return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
        _rdb_set_int(nm, stat->bearer_deact_count - stat->bearer_deact_abnormal_count);

        r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_abnormal_releases", _rdb_prefix, sess_idx, bname);
        if (r < 0)
            return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
        _rdb_set_int(nm, stat->bearer_deact_abnormal_count);
    }

    /* collect emm failure causes */
    *val = 0;
    j = 0;
    __for_each(k, stat->fcs, fc)
    {
        __for_each(i, fc->counts, fc_count)
        {
            if (!*fc_count)
                continue;

            /* get fc_count name */
            cause_name = (k == nas_message_type_emm) ? lte_nas_parser_get_emm_cause_name_str(i) : lte_nas_parser_get_esm_cause_name_str(i);
            if (!cause_name) {
                ERR("unknown fc_count code (nas_type=%d,cause_code=%d)", k, i);
                continue;
            }

            /* add comma if any name is written */
            if (j) {
                rc = snprintf(&val[j], sizeof(val) - j, ",");
                if (rc < 0)
                    break;
                j += rc;
            }

            /* add name and count */
            rc = snprintf(&val[j], sizeof(val) - j, "%s=%lld", cause_name, *fc_count);
            if (rc < 0)
                break;
            j += rc;
        }
    }

    /* write failure causes into rdb */
    r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.%s_bearer_act_failure_causes", _rdb_prefix, sess_idx, bname);
    if (r < 0)
        return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_set_str(nm, val);

    return 0;
}

/*
 Write all of bearer statistic information into RDBs.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int write_sessions_to_rdb(time_t tm, time_t tm_monotonic)
{
    struct bearer_session_t *sess;
    char nm[RDB_MAX_NAME_LEN];
    int i;
    int r;

    time_t tm_start;

    __for_each(i, ebr->sess, sess)
    {
        r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.apn", _rdb_prefix, i);
        if (r < 0)
            return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */

        /* remove previously existing RDbs if no APN is assigned */
        if (!*sess->apn) {
            _rdb_set_reset(nm);

            reset_bearer_stat_to_rdb("default", i, &sess->default_bearer);
            reset_bearer_stat_to_rdb("dedicated", i, &sess->dedicated_bearer);
        } else {
            _rdb_set_str(nm, sess->apn);

            write_bearer_stat_to_rdb("default", i, &sess->default_bearer);
            write_bearer_stat_to_rdb("dedicated", i, &sess->dedicated_bearer);
        }

        /* set start time */
        r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.start_time", _rdb_prefix, i);
        if (r < 0)
            return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
        if (*sess->apn) {
            tm_start = tm - (tm_monotonic - sess->start_time_monotonic) / 1000;
            _rdb_set_uint(nm, tm_start);
        } else {
            _rdb_set_reset(nm);
        }

        /* set start time monotonic */
        r = snprintf(nm, sizeof(nm), "%spdpcontext.%d.start_time_monotonic", _rdb_prefix, i);
        if (r < 0)
            return -1; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
        if (*sess->apn)
            _rdb_set_uint(nm, sess->start_time_monotonic);
        else
            _rdb_set_reset(nm);
    }

    return 0;
}

/*
 Release RDB session.

 Parameters:
  sess : RDB session to release
*/
static void release_sess(struct bearer_session_t *sess)
{
    sess->used = 0;
}

/*
 Assign RDB session by PDN bearer.
   By matching the APN in PDN bearer to APNS in [link.profile], this function selects RDB session. When no identical
  APN is found in [link.profile], default [link.profile] RDB session can be used only for the primary default bearers.

 Parameters:
  pdn_bearer : PDN bearer to assign RDB session.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static struct bearer_session_t *assign_sess_by_pdn_bearer(struct pdn_entity_t *pdn_bearer)
{
    struct pdn_entity_t *parent_pdn_bearer = pdn_bearer->default_bearer;

    struct bearer_session_t *sess;
    int i = -1;
    int pf;

    char link_profile_rdb[RDB_MAX_NAME_LEN];

    char rdb_apn[MAX_APN_BUFFER_SIZE];

    struct bearer_session_t *preferred_sess = NULL;
    struct bearer_session_t *second_preferred_sess = NULL;
    struct bearer_session_t *default_sess = NULL;
    struct bearer_session_t *sess_to_use = NULL;

    int default_profile;
    int profile_enable;

    const char *apn;

    /* primary default bearer flag - is this bearer the primary default bearer or dedicated bearer that belongs to the primary default bearer? */
    int primary_default_bearer = pdn_bearer->primary_default_bearer || (parent_pdn_bearer && parent_pdn_bearer->primary_default_bearer);
    /* default bearer flag - is this bearer a default bearer? */
    int default_bearer = !parent_pdn_bearer;

    DEBUG("[bearer-activity] primary_default_bearer=%d", primary_default_bearer);

    /* get apn */
    apn = pdn_bearer->apn;
    if (parent_pdn_bearer)
        apn = parent_pdn_bearer->apn;

    __for_each(i, ebr->sess, sess)
    {
        pf = i + 1;

        /* get link.profile.x.enable */
        snprintf(link_profile_rdb, sizeof(link_profile_rdb), "link.profile.%d.enable", pf);
        profile_enable = _rdb_get_int(link_profile_rdb);

        /* compare to link.profile.x.apn */
        if (apn) {
            /* get profile apn */
            snprintf(link_profile_rdb, sizeof(link_profile_rdb), "link.profile.%d.apn", pf);
            _rdb_get_str(rdb_apn, sizeof(rdb_apn), link_profile_rdb);

            if (!preferred_sess && !strcmp(rdb_apn, apn) && profile_enable)
                preferred_sess = sess;

            if (!second_preferred_sess && !strcmp(rdb_apn, apn))
                second_preferred_sess = sess;
        }

        /* get link.profile.x.defaultroute */
        if (profile_enable && !default_sess) {
            snprintf(link_profile_rdb, sizeof(link_profile_rdb), "link.profile.%d.defaultroute", pf);
            default_profile = _rdb_get_int(link_profile_rdb);
            if (default_profile)
                default_sess = sess;
        }
    }

    /* get session to use */
    sess_to_use = preferred_sess;
    if (!sess_to_use)
        sess_to_use = second_preferred_sess;

    /* use default session for primary default bearer */
    if (!sess_to_use && primary_default_bearer) {
        DEBUG("[bearer-activity] session associated with default bearer");
        sess_to_use = default_sess;
    }

    /* bypass if no session exists */
    if (!sess_to_use) {
        ERR("[bearer-activity] no session found (apn=%s)", apn);
        goto err;
    }

    /* reuse if the session is already used */
    if (sess_to_use->used) {
        DEBUG("[bearer-activity] session is already taken (sess_idx=%d,apn=%s), reuse", sess_to_use->index, sess_to_use->apn);
    }

    if (default_bearer) {
        /* get profile apn */
        *sess_to_use->apn = 0;
        pf = sess_to_use->index + 1;
        snprintf(link_profile_rdb, sizeof(link_profile_rdb), "link.profile.%d.apn", pf);
        _rdb_get_str(sess_to_use->apn, sizeof(sess_to_use->apn), link_profile_rdb);
    }

    /* set used flag */
    sess_to_use->used = 1;

    return sess_to_use;

err:
    return NULL;
}

/*
 Assign RDB session and RDB statistic information to PDN bearer.

 Parameters:
  priv : private information for PDN bearer.
  pdn_bearer : PDN bearer to assign RDB session and RDB statistic information.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int assign_sess(struct bearer_priv_t *priv, struct pdn_entity_t *pdn_bearer)
{
    struct bearer_session_t *sess;

    /* get session */
    sess = assign_sess_by_pdn_bearer(pdn_bearer);
    if (!sess)
        goto err;

    /* connect bearer statistics */
    if (pdn_bearer->ebt == PDN_TYPE_DEFAULT)
        priv->stat = &sess->default_bearer;
    else if (pdn_bearer->ebt == PDN_TYPE_DEDICATED)
        priv->stat = &sess->dedicated_bearer;
    else
        priv->stat = NULL;

    /* connect session */
    priv->sess = sess;

    return 0;
err:
    return -1;
}

/*
 Reset RDB statistic information.

 Parameters:
  stat : RDB statistic information to reset.
*/
static void reset_bearer_stat(struct bearer_stat_t *stat)
{
    memset(stat, 0, sizeof(*stat));
    avg_init(&stat->avg_bearer_act_latency, "bearer_act_latency", 0);
    avg_init(&stat->avg_bearer_duration, "bearer_duration", 0);
}

/*
 Reset RDB session and all of its RDB statistics.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int eps_bearer_rdb_reset_all_sess(time_t tm, time_t tm_monotonic)
{
    struct bearer_session_t *sess;
    int i;

    char link_profile_rdb[RDB_MAX_NAME_LEN];
    char rdb_apn[MAX_APN_BUFFER_SIZE];
    int pf;

    DEBUG("[bearer-activity] * reset session information");
    __for_each(i, ebr->sess, sess)
    {
        sess->index = i;

        /* get profile apn */
        pf = i + 1;
        snprintf(link_profile_rdb, sizeof(link_profile_rdb), "link.profile.%d.apn", pf);
        _rdb_get_str(rdb_apn, sizeof(rdb_apn), link_profile_rdb);

        /* update session apn if session does not have apn */
        snprintf(sess->apn, sizeof(sess->apn), rdb_apn);

        reset_bearer_stat(&sess->default_bearer);
        reset_bearer_stat(&sess->dedicated_bearer);

        /* update session times */
        sess->start_time = tm;
        sess->start_time_monotonic = tm_monotonic;
    }

    return 0;
}

/*
 Periodically flush all of RDB sessions into RDB when flush interval timer gets expired.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int eps_bearer_rdb_perform_periodic_flush(time_t tm, time_t tm_monotonic)
{
    DEBUG("[bearer-activity] * flush bearer information");
    write_sessions_to_rdb(tm, tm_monotonic);

    return 0;
}

/*
        ### functions to update RDB statistics ###

         Following static inline functions are dealing with RDB statistic structure via priv, mainly increasing and updating
        RDB statistics.
*/

/* update duration in RDB statistics */
static inline void statUpdateAvgDuration(struct bearer_priv_t *priv, long long duration)
{
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;
    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);

    avg_feed(&stat->avg_bearer_duration, duration);
    DEBUG("[bearer-rdb] update %s_avg_bearer_duration (sess=%d,duration=%lld,avg=%d)", ebt_str, sess->index, duration,
          __round(stat->avg_bearer_duration.avg));
}

/* increase failure cause count in RDB statistics */
static inline int statAddActFailureCauses(struct bearer_priv_t *priv, int nas_type, int fc_code)
{
    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;
    const char *cause_name;

    struct bearer_fc_count_t *fc = &stat->fcs[nas_type];
    long long *fc_count;

    /* check nas type */
    if (!__is_in_boundary(fc, stat->fcs, sizeof(stat->fcs))) {
        ERR("incorrect nas type (nas_type=%d)", nas_type);
        goto err;
    }

    /* get cause name */
    cause_name = (nas_type == nas_message_type_emm) ? lte_nas_parser_get_emm_cause_name_str(fc_code) : lte_nas_parser_get_esm_cause_name_str(fc_code);

    /* check failure code */
    fc_count = &fc->counts[fc_code];
    if (!cause_name || !__is_in_boundary(fc_count, fc->counts, sizeof(fc->counts))) {
        ERR("incorrect failure cause (type=%d,fc_code=%d)", nas_type, fc_code);
        goto err;
    }

    (*fc_count)++;

    DEBUG("[bearer-rdb] increase %s_bearer_act_failure_causes (sess=%d,cause='%s' #%d, cnt=%lld)", ebt_str, sess->index, cause_name, fc_code,
          *fc_count);

    return 0;

err:
    return -1;
}

/* apply failure cause codes into RDB statistics */
static inline int statUpdateActFailureCausesIfExisting(struct bearer_priv_t *priv)
{
    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;
    int rc = 0;

    int cause_code_tbl[nas_message_type_max] = {
        [nas_message_type_emm] = pdn_bearer->emm_cause,
        [nas_message_type_esm] = pdn_bearer->esm_cause,
    };

    int *cause_code;
    int i;

    /* reset causes */
    pdn_bearer->emm_cause = 0;
    pdn_bearer->esm_cause = 0;

    /* add each of cause codes into rdb */
    __for_each(i, cause_code_tbl, cause_code)
    {
        if (*cause_code) {
            DEBUG("[bearer-activity] bearer cause code detected (sess=%d, type=%d, cause=%d)", sess->index, i, *cause_code);
            statAddActFailureCauses(priv, i, *cause_code);
        } else {
            DEBUG("[bearer-activity] no bearer cause code detected (sess=%d, type=%d)", sess->index, i);
        }
    }

    return rc;
}

/* update latency in RDB statistics */
static inline void statUpdateAvgActLatency(struct bearer_priv_t *priv, long long latency)
{
    VALIDATE(priv && priv->pdn_bearer && priv->stat && priv->sess);

    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;

    avg_feed(&stat->avg_bearer_act_latency, latency);
    DEBUG("[bearer-rdb] update %s_avg_bearer_act_latency (sess=%d,latency=%lld,avg=%d)", ebt_str, sess->index, latency,
          __round(stat->avg_bearer_act_latency.avg));
}

/* increase activation attempts in RDB statistics */
static inline void statIncreaseActAttempts(struct bearer_priv_t *priv)
{
    VALIDATE(priv && priv->pdn_bearer && priv->stat && priv->sess);

    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;

    stat->bearer_act_attempts++;
    DEBUG("[bearer-rdb] increase %s_bearer_act_attempts (sess=%d,new=%lld)", ebt_str, sess->index, stat->bearer_act_attempts);
}

/* increase activation accepts in RDB statistics */
static inline void statIncreaseActAccepts(struct bearer_priv_t *priv)
{
    VALIDATE(priv && priv->pdn_bearer && priv->stat && priv->sess);

    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;

    stat->bearer_act_accepts++;
    DEBUG("[bearer-rdb] increase %s_bearer_act_accepts (sess=%d,new=%lld)", ebt_str, sess->index, stat->bearer_act_accepts);
}

/* increase activation failures in RDB statistics */
static inline void statIncreaseActFailures(struct bearer_priv_t *priv)
{
    VALIDATE(priv && priv->pdn_bearer && priv->stat && priv->sess);

    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;

    stat->bearer_act_failures++;
    DEBUG("[bearer-rdb] increase %s_bearer_act_failures (sess=%d,new=%lld)", ebt_str, sess->index, stat->bearer_act_failures);
}

/* increase deactivation attempts in RDB statistics */
static inline void statIncreaseDeactAttempts(struct bearer_priv_t *priv)
{
    VALIDATE(priv && priv->pdn_bearer && priv->stat && priv->sess);

    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;

    stat->bearer_deact_attempts++;
    DEBUG("[bearer-rdb] increase %s_bearer_deact_attempts (sess=%d,new=%lld)", ebt_str, sess->index, stat->bearer_deact_attempts);
}

/* increase deactivation count in RDB statistics */
static inline void statIncreaseDeactCount(struct bearer_priv_t *priv)
{
    VALIDATE(priv && priv->pdn_bearer && priv->stat && priv->sess);

    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;

    stat->bearer_deact_count++;
    DEBUG("[bearer-rdb] increase %s_bearer_deact_count (sess=%d,new=%lld)", ebt_str, sess->index, stat->bearer_deact_count);
}

/* increase abnormal deactivation count in RDB statistics */
static inline void statIncreaseDeactAbnormalCount(struct bearer_priv_t *priv)
{
    VALIDATE(priv && priv->pdn_bearer && priv->stat && priv->sess);

    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;

    stat->bearer_deact_abnormal_count++;
    DEBUG("[bearer-rdb] increase %s_bearer_deact_abnormal_count (sess=%d,new=%lld)", ebt_str, sess->index, stat->bearer_deact_abnormal_count);
}

/* increase deactivation failures in RDB statistics */
static inline void statIncreaseDeactFailures(struct bearer_priv_t *priv)
{
    VALIDATE(priv && priv->pdn_bearer && priv->stat && priv->sess);

    const char *ebt_str = pdn_mgr_get_ebt_name(priv->pdn_bearer->ebt);
    struct bearer_stat_t *stat = priv->stat;
    struct bearer_session_t *sess = priv->sess;

    stat->bearer_deact_failures++;
    DEBUG("[bearer-rdb] increase %s_bearer_deact_failures (sess=%d,new=%lld)", ebt_str, sess->index, stat->bearer_deact_failures);
}

/*
        ### PDN manager event functions ###

        Following functions are for each of PDN events. Each of these functions to convert activities into actual statistics.
*/

/* when PDN bearer is created */
static inline void statOnCreat(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->qctm_creat && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;

    /* update creation time */
    priv->qctm_creat = *ms;
    DEBUG("[bearer-activity] bearer created (ebt=%d,ebi=%d)", pdn_bearer->ebt, pdn_bearer->ebi);
}

/* when PDN bearer connection fails */
static inline void statOnConnFail(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;

    /* update connection request */
    priv->qctm_conn_fail = *ms;
    /* increase attempt */
    statIncreaseActFailures(priv);

    DEBUG("[bearer-activity] bearer connection failed (ebt=%d,ebi=%d,apn=%s)", pdn_bearer->ebt, pdn_bearer->ebi, sess->apn);
    statUpdateActFailureCausesIfExisting(priv);
}

/* when PDN bearer pre-connection is requested */
static inline void statOnPreconnReq(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;

    /* update connection request */
    priv->qctm_preconn_req = *ms;

    if (priv->pdn_bearer->ebt == PDN_TYPE_DEFAULT) {
        /* increase attempt */
        statIncreaseActAttempts(priv);
    }

    DEBUG("[bearer-activity] bearer connection requested (ebt=%d,ebi=%d,apn=%s)", pdn_bearer->ebt, pdn_bearer->ebi, sess->apn);
}

/* when PDN bearer connection is requested */
static inline void statOnConnReq(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;

    /* update connection request */
    priv->qctm_conn_req = *ms;

    if (priv->pdn_bearer->ebt == PDN_TYPE_DEDICATED) {
        /* increase attempt */
        statIncreaseActAttempts(priv);
    }

    DEBUG("[bearer-activity] bearer connection requested (ebt=%d,ebi=%d,apn=%s)", pdn_bearer->ebt, pdn_bearer->ebi, sess->apn);
}

/* when PDN bearer connection is accepted */
static inline void statOnConnAccpt(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;
    long long latency;

    /* update connection time */
    priv->qctm_conn_accpt = *ms;
    /* increase accept */
    statIncreaseActAccepts(priv);
    /* get latency and feed it to avg */
    latency = (int)(priv->qctm_conn_accpt - priv->qctm_conn_req);
    statUpdateAvgActLatency(priv, latency);

    DEBUG("[bearer-activity] bearer connection accepted (latency=%lld ms,ebt=%d,ebi=%d,apn=%s)", latency, pdn_bearer->ebt, pdn_bearer->ebi,
          sess->apn);
}

/* when PDN bearer pre-disconnection is requested */
static inline void statOnPredisconnReq(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;

    /* update connection request */
    priv->qctm_predisconn_req = *ms;
    /* increase */
    statIncreaseDeactAttempts(priv);

    DEBUG("[bearer-activity] bearer predisconnection requested (ebt=%d,ebi=%d,apn=%s)", pdn_bearer->ebt, pdn_bearer->ebi, sess->apn);
}

/* when PDN bearer disconnection is requested */
static inline void statOnDisconnReq(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;

    /* update connection request */
    priv->qctm_disconn_req = *ms;

    if (priv->pdn_bearer->ebt == PDN_TYPE_DEDICATED) {
        /* increase */
        statIncreaseDeactAttempts(priv);
    }

    DEBUG("[bearer-activity] bearer disconnection requested (ebt=%d,ebi=%d,apn=%s)", pdn_bearer->ebt, pdn_bearer->ebi, sess->apn);
}

/* when PDN bearer disconnection is accepted */
static inline void statOnDisconnAccpt(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;
    unsigned long long duration;

    /* update connection request */
    priv->qctm_disconn = *ms;
    /* increase accept */
    statIncreaseDeactCount(priv);
    /* get duration and feed it to avg */
    duration = priv->qctm_disconn - priv->qctm_conn_accpt;
    statUpdateAvgDuration(priv, duration);

    // if abnormal release:
    if (pdn_bearer->emm_cause || pdn_bearer->esm_cause) {
        statIncreaseDeactAbnormalCount(priv);
    }

    DEBUG("[bearer-activity] bearer disconnection accepted (duration=%lld ms,ebt=%d,ebi=%d,apn=%s)", duration, pdn_bearer->ebt, pdn_bearer->ebi,
          sess->apn);
}

/* when PDN bearer pre-connection fails */
static inline void statOnPreconnFail(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;

    statIncreaseActFailures(priv);

    DEBUG("[bearer-activity] bearer pre-connection failed (cause=%d,ebt=%d,ebi=%d,apn=%s)", pdn_bearer->emm_cause, pdn_bearer->ebt, pdn_bearer->ebi,
          sess->apn);
    statUpdateActFailureCausesIfExisting(priv);
}

/* when PDN bearer pre-disconnection fails */
static inline void statOnPredisconnFail(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;

    statIncreaseDeactFailures(priv);

    DEBUG("[bearer-activity] bearer pre-disconnection failed (cause=%d,ebt=%d,ebi=%d,apn=%s)", pdn_bearer->emm_cause, pdn_bearer->ebt,
          pdn_bearer->ebi, sess->apn);
    statUpdateActFailureCausesIfExisting(priv);
}

/* when PDN bearer disconnection fails */
static inline void statOnDisconnFail(struct bearer_priv_t *priv, const unsigned long long *ms)
{
    VALIDATE(priv && priv->pdn_bearer && priv->sess && ms);

    struct pdn_entity_t *pdn_bearer = priv->pdn_bearer;
    struct bearer_session_t *sess = priv->sess;

    statIncreaseDeactFailures(priv);

    DEBUG("[bearer-activity] bearer disconnection failed (cause=%d,ebt=%d,ebi=%d,apn=%s)", pdn_bearer->esm_cause, pdn_bearer->ebt, pdn_bearer->ebi,
          sess->apn);
    statUpdateActFailureCausesIfExisting(priv);
}

/*
        ### EPS bearer RDB functions ###
*/

/*
 Receive PDN event.
   This function is a main function to convert PDN activities into RDB as a callback function for PDN manager
  event. Based on PDN event, the function maintains RDB session and its statistics.

 Parameters:
  ms : QxDM time-stamp
  event : PDN event
  event_name : PDN event name as a string
  pdn_bearer : PDN bearer

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int eps_bearer_rdb_on_pdn_mgr_event(const unsigned long long *ms, int event, const char *event_name, struct pdn_entity_t *pdn_bearer)
{
    struct bearer_priv_t *priv = pdn_bearer ? pdn_bearer->priv : NULL;
    const char *apn = NULL;

#define GOTO_ERR_IF_NO_PRIV_EXISTS(m, ...) \
    if (!priv) { \
        ERR(m, ##__VA_ARGS__); \
        goto err; \
    }

#define GOTO_ERR_IF_NO_SESS_CONNECTED(m, ...) \
    if (!priv->sess) { \
        ERR(m, ##__VA_ARGS__); \
        goto err; \
    }

#define GOTO_ERR_IF_NO_STAT_CONNECTED(m, ...) \
    if (!priv->stat) { \
        ERR(m, ##__VA_ARGS__); \
        goto err; \
    }

    /* get apn */
    if (pdn_bearer) {
        apn = pdn_bearer->apn;
        if (pdn_bearer->default_bearer)
            apn = pdn_bearer->default_bearer->apn;
    }

    if (pdn_bearer) {
        DEBUG("[bearer-activity] PDN_EVENT ######## got %s #%d event (ebi=%d,linked_ebi=%d,apn=%s,emm=%d,esm=%d)\n", event_name, event,
              pdn_bearer->ebi, pdn_bearer->default_bearer_ebi, apn, pdn_bearer->emm_cause, pdn_bearer->esm_cause);
    } else {
        DEBUG("[bearer-activity] PDN_EVENT ######## got %s #%d  event\n", event_name, event);
    }

    rdb_enter_csection();
    {
        switch (event) {
            case PDN_EVENT_ATTACH_REQUEST:
                break;

            case PDN_EVENT_ATTACH_ACCEPT:

            case PDN_EVENT_ATTACH_REJECT:
                break;

            case PDN_EVENT_ATTACH_COMPLETE:
            case PDN_EVENT_DETACH_REQUEST:
            case PDN_EVENT_DETACH_ACCEPT:
                break;

            case PDN_EVENT_CREATE: {
                struct bearer_session_t *sess;

                /* allocate priv */
                pdn_bearer->priv = calloc(1, sizeof(*priv));
                if (!pdn_bearer->priv) {
                    ERR("[bearer-activity] failed to allocate private member to pdn_bearer");
                    goto err;
                }
                priv = pdn_bearer->priv;

                /* connect pdn bearer to priv */
                priv->pdn_bearer = pdn_bearer;

                statOnCreat(priv, ms);

                /* try to associate session - only for primary default bearer */
                assign_sess(priv, pdn_bearer);
                sess = priv->sess;

                if (sess) {
                    GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                    GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected (ebt=%d,apn=%s)", pdn_bearer->ebt, apn);
                    DEBUG("[bearer-activity] session associated (sess_idx=%d,apn=%s)", sess->index, sess->apn);
                } else {
                    DEBUG("[bearer-activity] session not associated yet (ebt=%d,apn=%s)", pdn_bearer->ebt, apn);
                }

                break;
            }

            case PDN_EVENT_DELETE:
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected (ebt=%d,apn=%s)", pdn_bearer->ebt, apn);
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected (apn=%s)", apn);

                statUpdateActFailureCausesIfExisting(priv);

                /* release session */
                release_sess(priv->sess);

                free(pdn_bearer->priv);
                pdn_bearer->priv = NULL;
                break;

            case PDN_EVENT_UPDATE: {
                struct bearer_session_t *sess;

                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                sess = priv->sess;

                DEBUG("[bearer-activity] session apn updated (idx=%d,apn=%s)", sess->index, apn);
                snprintf(sess->apn, sizeof(sess->apn), "%s", apn);
                break;
            }

            case PDN_EVENT_PRECONNECT_REQUEST: {
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                statOnPreconnReq(priv, ms);
                break;
            }

            case PDN_EVENT_PRECONNECT_REJECT: {
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                statOnPreconnFail(priv, ms);
                break;
            }

            case PDN_EVENT_CONNECT_REQUEST: {
                struct bearer_session_t *sess;

                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");

                sess = priv->sess;

                /* check APN */
                if (!apn) {
                    ERR("[bearer-activity] no APN exists");
                    goto err;
                }

                if (!sess) {
                    assign_sess(priv, pdn_bearer);
                    sess = priv->sess;

                    GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected (ebt=%d,apn=%s)", pdn_bearer->ebt, apn);
                    GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected (apn=%s)", apn);

                    DEBUG("[bearer-activity] session associated (sess_idx=%d,apn=%s)", sess->index, sess->apn);
                }

                DEBUG("[bearer-activity] session apn updated (idx=%d,apn=%s)", sess->index, apn);
                snprintf(sess->apn, sizeof(sess->apn), "%s", apn);

                statOnConnReq(priv, ms);
                break;
            }

            case PDN_EVENT_CONNECT_REJECT:
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                statOnConnFail(priv, ms);
                break;

            case PDN_EVENT_CONNECT_ACCEPT:
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                statOnConnAccpt(priv, ms);
                break;

            case PDN_EVENT_PREDISCONNECT_REQUEST:
                statOnPredisconnReq(priv, ms);
                break;

            case PDN_EVENT_PREDISCONNECT_REJECT:
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                statOnPredisconnFail(priv, ms);
                break;

            case PDN_EVENT_DISCONNECT_REQUEST:
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                statOnDisconnReq(priv, ms);
                break;

            case PDN_EVENT_DISCONNECT_REJECT:
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                statOnDisconnFail(priv, ms);
                break;

            case PDN_EVENT_DISCONNECT_ACCEPT:
                GOTO_ERR_IF_NO_PRIV_EXISTS("[bearer-activity] no priv exists");
                GOTO_ERR_IF_NO_STAT_CONNECTED("[bearer-activity] no stat connected");
                GOTO_ERR_IF_NO_SESS_CONNECTED("[bearer-activity] no sessions connected");

                statOnDisconnAccpt(priv, ms);
                break;

            default:
                ERR("[bearer-activity] unknown PDN event (event=%d)", event);
                goto err;
        }

        rdb_leave_csection();
        return 0;

    err:
        rdb_leave_csection();
        return -1;
    }
}

/*
 Initiate EPS bearer RDB instance.
   The function allocates and initiates all of required objects of EPS bearer RDB object.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int eps_bearer_rdb_init()
{
    memset(ebr, 0, sizeof(*ebr));

    /* reset all session information */
    memset(ebr->sess, 0, sizeof(ebr->sess));
    eps_bearer_rdb_reset_all_sess(0, 0);

    return 0;
}

/*
 Finalize EPS bearer RDB instance.
*/
void eps_bearer_rdb_fini() {}
