/*!
 * PDN manager - trace UE PDN bearer state
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

#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "pdn_mgr.h"

#include "def.h"
#include "lte_nas_parser.h"

/* PDN manager class */
struct pdn_mgr_t {

    /* total available PDN sessions */
    struct pdn_entity_t bearers[PDN_MGR_MAX_PDN];

    pdn_mgr_on_event_callback event_cb;

    int primary_default_bearer_to_create;

    int total_created_default_bearers;
    int total_created_dedicated_bearers;
};

/* PDN manager object - singleton */
static struct pdn_mgr_t _pdn_mgr_singleton;
static struct pdn_mgr_t *_pdn_mgr = &_pdn_mgr_singleton;

/* local functions */
static int pdn_mgr_link_dedicated_bearer(struct pdn_entity_t *dedicated_bearer, struct pdn_entity_t *default_bearer);
static int pdn_mgr_unlink_dedicated_bearer(struct pdn_entity_t *dedicated_bearer);

#define FOR_EACH_BEARER(i, bearer) __for_each(i, _pdn_mgr->bearers, bearer)

#define DEBUG_BEARER(bearer_name, info_name, bearer) \
    { \
        DEBUG("* [%s] %s bearer information (stat=%d,ebt=%d,pti=%d,ebi=%d,apn=%s,default_ebi=%d,primary=%d,emm=%d,esm=%d)", bearer_name, info_name, \
              bearer->stat, bearer->ebt, bearer->pti, bearer->ebi, bearer->apn, bearer->default_bearer_ebi, bearer->primary_default_bearer, \
              bearer->emm_cause, bearer->esm_cause); \
    } \
    while (0)

/*
        ### bearer search functions / level 1 ###

        Basic and maximum generic functions to find bearers.
*/

/*
 Find bearer.

 Parameters:
  comp : compare function.
  ref : reference pointer to provide to compare function.

 Return:
  On success, bearer is returned. Otherwise, NULL.
*/
static struct pdn_entity_t *_get_bearer_by(int (*comp)(struct pdn_entity_t *bearer, const void *ref), const void *ref)
{
    struct pdn_entity_t *bearer = NULL;
    int i;

    FOR_EACH_BEARER(i, bearer)
    {
        if (!comp(bearer, ref))
            goto found;
    }

    return NULL;

found:
    return bearer;
}

/*
        ### bearer search functions / level 2 ###

        Specific elements are used to find bearers by using level 1 function bearer search functions.
*/

/* compare function for EBI  */
static int _get_bearer_by_ebi_comp(struct pdn_entity_t *bearer, const void *ref)
{
    if (bearer->stat == PDN_STAT_DELETED)
        return -1;

    return bearer->ebi - *(const int *)ref;
}

/* compare function for PTI */
static int _get_bearer_by_pti_comp(struct pdn_entity_t *bearer, const void *ref)
{
    if (bearer->stat == PDN_STAT_DELETED)
        return -1;

    return bearer->pti - *(const int *)ref;
}

/* compare function for APN */
static int _get_bearer_by_apn_comp(struct pdn_entity_t *bearer, const void *ref)
{
    if (bearer->stat == PDN_STAT_DELETED)
        return -1;

    if (!bearer->apn || !ref)
        return -1;

    return strcmp(bearer->apn, (const char *)ref);
}

/* compare function for stat */
static int _get_bearer_by_stat_comp(struct pdn_entity_t *bearer, const void *ref)
{
    return bearer->stat - *(const int *)ref;
}

/* find bearer by EBI */
static inline struct pdn_entity_t *_get_bearer_by_ebi(int ebi)
{
    return _get_bearer_by(_get_bearer_by_ebi_comp, &ebi);
}

/* find bearer by PTI */
static inline struct pdn_entity_t *_get_bearer_by_pti(int pti)
{
    return _get_bearer_by(_get_bearer_by_pti_comp, &pti);
}

/* find bearer by APN */
static inline struct pdn_entity_t *_get_bearer_by_apn(const char *apn)
{
    return _get_bearer_by(_get_bearer_by_apn_comp, apn);
}

/* find bearer by stat */
static inline struct pdn_entity_t *_get_bearer_by_stat(int stat)
{
    return _get_bearer_by(_get_bearer_by_stat_comp, &stat);
}

/*
        ### bearer search functions / level 3 ###

        Combined activities are performed by using level 2 bearer search functions.
*/

/* find unused bearer */
static inline struct pdn_entity_t *_get_unused_bearer()
{
    return _get_bearer_by_stat(PDN_STAT_DELETED);
}

/* find bearer by EBI, APN or PTI when it possible */
static struct pdn_entity_t *_get_bearer_by_keys(const struct pdn_entity_t *bearer_info)
{
    struct pdn_entity_t *bearer = NULL;

    if (!bearer && bearer_info->ebi)
        bearer = _get_bearer_by_ebi(bearer_info->ebi);
    if (!bearer && bearer_info->apn)
        bearer = _get_bearer_by_apn(bearer_info->apn);
    if (!bearer && bearer_info->pti)
        bearer = _get_bearer_by_pti(bearer_info->pti);

    return bearer;
}

/*
        ### PDN numeric-type-to-string conversion functions ##
*/

/*
 Convert numeric PDN type to readable string.

 Parameters:
  ebt : EPS bearer type.

 Return:
  EPS bearer type string. Otherwise NULL.
*/
const char *pdn_mgr_get_ebt_name(int ebt)
{
    const char *ebt_str[] = {
        [PDN_TYPE_UNKNOWN] = NULL,
        [PDN_TYPE_DEFAULT] = "default",
        [PDN_TYPE_DEDICATED] = "dedicated",
    };

    const char **ebt_name = &ebt_str[ebt];

    return __is_in_boundary(ebt_name, ebt_str, sizeof(ebt_str)) ? *ebt_name : NULL;
}

/*
 Convert numeric event to readable string.

 Parameters:
  event : PDN event! numeric!

 Return:
  Event string. Otherwise NULL.
*/
const char *pdn_mgr_get_event_name(int event)
{
    static const char *event_str[] = {
        [PDN_EVENT_UNKNOWN] = "PDN_EVENT_UNKNOWN",
        /* global events */
        [PDN_EVENT_ATTACH_REQUEST] = "PDN_EVENT_ATTACH_REQUEST",
        [PDN_EVENT_ATTACH_ACCEPT] = "PDN_EVENT_ATTACH_ACCEPT",
        [PDN_EVENT_ATTACH_REJECT] = "PDN_EVENT_ATTACH_REJECT",
        [PDN_EVENT_ATTACH_COMPLETE] = "PDN_EVENT_ATTACH_COMPLETE",
        [PDN_EVENT_DETACH_REQUEST] = "PDN_EVENT_DETACH_REQUEST", /* broadcast DELETE to default bearers */
        [PDN_EVENT_DETACH_ACCEPT] = "PDN_EVENT_DETACH_ACCEPT",

        /* bearer events */
        [PDN_EVENT_CREATE] = "PDN_EVENT_CREATE",
        [PDN_EVENT_DELETE] = "PDN_EVENT_DELETE", /* broadcast to dedicate bearers */
        [PDN_EVENT_UPDATE] = "PDN_EVENT_UPDATE",

        [PDN_EVENT_PRECONNECT_REQUEST] = "PDN_EVENT_PRECONNECT_REQUEST",
        [PDN_EVENT_PRECONNECT_REJECT] = "PDN_EVENT_PRECONNECT_REJECT",
        [PDN_EVENT_CONNECT_REQUEST] = "PDN_EVENT_CONNECT_REQUEST",
        [PDN_EVENT_CONNECT_ACCEPT] = "PDN_EVENT_CONNECT_ACCEPT",
        [PDN_EVENT_CONNECT_REJECT] = "PDN_EVENT_CONNECT_REJECT",

        [PDN_EVENT_PREDISCONNECT_REQUEST] = "PDN_EVENT_PREDISCONNECT_REQUEST",
        [PDN_EVENT_PREDISCONNECT_REJECT] = "PDN_EVENT_PREDISCONNECT_REJECT",
        [PDN_EVENT_DISCONNECT_REQUEST] = "PDN_EVENT_DISCONNECT_REQUEST",
        [PDN_EVENT_DISCONNECT_ACCEPT] = "PDN_EVENT_DISCONNECT_ACCEPT", /* broadcast to dedicate bearers */
        [PDN_EVENT_DISCONNECT_REJECT] = "PDN_EVENT_DISCONNECT_REJECT",
    };

    return ((0 <= event) && (event < __countof(event_str))) ? event_str[event] : NULL;
}

/*
 Convert numeric stat to readable string.

 Parameters:
  stat : numeric stat.

 Return:
  Stat string. Otherwise NULL.
*/
static inline const char *_get_stat_name(int stat)
{
    static const char *stat_str[] = {
        [PDN_STAT_DELETED] = "PDN_STAT_DELETED",
        [PDN_STAT_CREATED] = "PDN_STAT_CREATED",
        [PDN_STAT_CONNECTED] = "PDN_STAT_CONNECTED",
        [PDN_STAT_DISCONNECTED] = "PDN_STAT_DISCONNECTED",
    };

    return ((0 <= stat) && (stat < __countof(stat_str))) ? stat_str[stat] : NULL;
}

/*
        ### PDN generic functions ##
*/

/*
 Reset bearer.
  Free any allocated members and zero the bearer.

 Parameters:
  bearer : bearer to reset
*/
static void pdn_mgr_reset_bearer(struct pdn_entity_t *bearer)
{
    free(bearer->apn);
    memset(bearer, 0, sizeof(*bearer));
}

/*
 Delete bearer.
   Free the bearer. Additionally, unlink the bearer if the bearer is dedicated. Default bearer is not allowed to
  be deleted until all of its dedicated bearers are deleted. Otherwise, the attempt will fail.

 Parameters:
  bearer : bearer to delete.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int pdn_mgr_delete_bearer(struct pdn_entity_t *bearer)
{
    if (bearer->head || bearer->tail) {
        ERR("failed to delete bearer - dedicated bearer exists");
        goto err;
    }

    /* if default bearer exists */
    if (bearer->default_bearer)
        pdn_mgr_unlink_dedicated_bearer(bearer);

    /* reset all members */
    pdn_mgr_reset_bearer(bearer);

    return 0;

err:
    return -1;
}

/*
 reset PTI.

 Parameters:
  bearer : bearer to delete.
*/
static void pdn_mgr_reset_bearer_pti(struct pdn_entity_t *bearer)
{
    bearer->pti = 0;
}

/*
 Update EMM/ESM failure cause codes.
  Copy EMM/ESM cause codes from bearer information to bearer.

 Parameters:
  bearer_info : bearer information that contains new EMM/ESM code causes.
  bearer : bearer to update EMM/ESM code causes.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int pdn_mgr_update_causes(const char *event_name, struct pdn_entity_t *bearer, const struct pdn_entity_t *bearer_info)
{
    DEBUG("[%s] hand-over EMM/esm causes into bearer (ebt=%d,emm=%d,esm=%d)", event_name, bearer_info->ebt, bearer_info->emm_cause,
          bearer_info->esm_cause);
    bearer->emm_cause = bearer_info->emm_cause;
    bearer->esm_cause = bearer_info->esm_cause;

    return 0;
}

/*
 Update bearer information.
  This function does not allow to change any information (APN, PTI, EBI and EBT) but accept new information.

 Parameters:
  bearer_info : bearer information that contains new information.
  bearer : bearer to update.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int pdn_mgr_update_bearer(struct pdn_entity_t *bearer, const struct pdn_entity_t *bearer_info)
{
#define SET_IF_DEST_IS_ZERO_ELSE(d, s) \
    if (!(d)) { \
        (d) = (s); \
    } else if ((s) && (s) != (d))

    char *apn = bearer_info->apn ? strdup(bearer_info->apn) : NULL;

    /*
        ### set keys ###
    */

    SET_IF_DEST_IS_ZERO_ELSE(bearer->pti, bearer_info->pti)
    {
        ERR("attempt to update a different PTI (cur=%d,new=%d)", bearer->pti, bearer_info->pti);
        goto err;
    }

    SET_IF_DEST_IS_ZERO_ELSE(bearer->ebi, bearer_info->ebi)
    {
        ERR("attempt to update a different EBI (cur=%d,new=%d)", bearer->ebi, bearer_info->ebi);
        goto err;
    }

    SET_IF_DEST_IS_ZERO_ELSE(bearer->apn, apn)
    {
        if (strcmp(bearer->apn, apn)) {
            ERR("attempt to update APN (cur=%s,new=%s,alloc=%s)", bearer->apn, bearer_info->apn, apn);
            goto err;
        }

        /* we do not use new APN if the string is identical */
        free(apn);
    }

    /*
        ### update misc ###
    */

    SET_IF_DEST_IS_ZERO_ELSE(bearer->ebt, bearer_info->ebt)
    {
        ERR("attempt to update EBT (cur=%d,new=%d)", bearer->ebt, bearer_info->ebt);
        goto err;
    }

    SET_IF_DEST_IS_ZERO_ELSE(bearer->default_bearer_ebi, bearer_info->default_bearer_ebi)
    {
        ERR("attempt to update default bearer EBI (cur=%d,new=%d)", bearer->default_bearer_ebi, bearer_info->default_bearer_ebi);
        goto err;
    }

    return 0;

err:
    free(apn);
    return -1;
}

/*
 Create a bearer and update the bearer with given bearer information.

 Parameters:
  bearer_info : bear information to create a bearer.

 Return:
  PDN entity of created bearer. Otherwise, NULL.
*/
static struct pdn_entity_t *pdn_mgr_create_bearer(const struct pdn_entity_t *bearer_info)
{
    struct pdn_entity_t *bearer;
    struct pdn_entity_t *default_b = NULL;

    /*
        ### sanitize arguments ###
    */

    /* if existing */
    if (_get_bearer_by_keys(bearer_info)) {
        ERR("failed to create bearer - bearer already exists");
        goto err;
    }

    /* if default bearer does not exist */
    if (bearer_info->default_bearer_ebi) {
        struct pdn_entity_t default_bi;
        struct pdn_entity_t *dbi = &default_bi;

        memset(dbi, 0, sizeof(*dbi));

        dbi->ebi = bearer_info->default_bearer_ebi;
        DEBUG("[bearer-activity] search linked default bearer (ebi=%d,default_ebi=%d)", bearer_info->ebi, dbi->ebi);
        default_b = _get_bearer_by_keys(dbi);
        if (!default_b) {
            ERR("failed to create bearer - default bearer does not exist");
            goto err;
        }
    }

    /*
        ### set bearer information ###
    */

    /* create bearer */
    bearer = _get_unused_bearer();

    /* update keys */
    if (pdn_mgr_update_bearer(bearer, bearer_info) < 0) {
        ERR("failed to update bearer keys");
        goto err;
    }

    /* link to default bearer if existing */
    if (default_b) {
        DEBUG("[bearer-activity] linked default bearer found (ebi=%d,default_ebi=%d,apn=%s)", bearer_info->ebi, default_b->ebi, default_b->apn);
        pdn_mgr_link_dedicated_bearer(bearer, default_b);
    }

    return bearer;

err:
    return NULL;
}

/*
 Get first dedicated bearer.

 Parameters:
  bearer : default bearer to get the first dedicated bearer

 Return:
  PDN entity of a dedicated bearer. If there is no more bearer, return value will be NULL.
*/
static struct pdn_entity_t *pdn_mgr_get_first_dedicated_bearer(const struct pdn_entity_t *bearer)
{
    return bearer ? bearer->head : NULL;
}

/*
 Get next dedicated bearer.

 Parameters:
  bearer : dedicated bearer to get the next dedicated bearer.

 Return:
  PDN entity of a dedicated bearer. If there is no more bearer, return value will be NULL.
*/
static struct pdn_entity_t *pdn_mgr_get_next_dedicated_bearer(struct pdn_entity_t *bearer)
{
    return bearer ? bearer->next : NULL;
}

/*
 Link a dedicated bearer to its default bearer.

 Parameters:
  dedicated_bearer : dedicated bearer to be linked to its default bearer.
  default_bearer : default bearer to link the dedicated bearer.

 Return:
  Success 0. Otherwise, -1.
*/
static int pdn_mgr_link_dedicated_bearer(struct pdn_entity_t *dedicated_bearer, struct pdn_entity_t *default_bearer)
{
    struct pdn_entity_t *tail = default_bearer->tail;

    /* set default bearer */
    dedicated_bearer->default_bearer = default_bearer;

    /* set up link in the dedicated bearer */
    dedicated_bearer->prev = tail;
    dedicated_bearer->next = NULL;

    /* set up link in the last dedicated bearer */
    if (tail)
        tail->next = dedicated_bearer;

    /* fix up head */
    if (!default_bearer->head)
        default_bearer->head = dedicated_bearer;

    /* fix up tail */
    default_bearer->tail = dedicated_bearer;

    return 0;
}

/*
 Unlink a dedicated bearer from its default bearer.

 Parameters:
  dedicated_bearer : dedicated bearer to be unlinked from its default bearer.

 Return:
  Success 0. Otherwise, -1
*/
static int pdn_mgr_unlink_dedicated_bearer(struct pdn_entity_t *dedicated_bearer)
{
    struct pdn_entity_t *default_bearer = dedicated_bearer->default_bearer;
    struct pdn_entity_t *prev = dedicated_bearer->prev;
    struct pdn_entity_t *next = dedicated_bearer->next;

    /* link prev to next */
    if (prev)
        prev->next = dedicated_bearer->next;

    /* link next to prev */
    if (next)
        next->prev = dedicated_bearer->prev;

    /* fix up head in the default bearer */
    if (default_bearer->head == dedicated_bearer)
        default_bearer->head = next;

    /* fix up tail in the default bearer */
    if (default_bearer->tail == dedicated_bearer)
        default_bearer->tail = prev;

    /* remove default bearer information */
    dedicated_bearer->default_bearer_ebi = 0;
    dedicated_bearer->default_bearer = NULL;

    return 0;
}

/*
 Post an identical PDN event to all of dedicated bearers that are specified by filter.

 Parameters:
  ms : pointer to QxDM time-stamp.
  event : PDN event to post.
  bearer : bearer to post event.
  filter_cb : bearer filter function.

 Return:
  Success 0. Otherwise, -1
*/
static int pdn_mgr_post_event_to_dedicate_bearers(const unsigned long long *ms, int event, const struct pdn_entity_t *bearer,
                                                  pdn_mgr_bearer_filter_callback filter_cb)
{
    struct pdn_entity_t *dedicated_bearer;
    struct pdn_entity_t *dedicated_bearer_next;
    const char *event_name = pdn_mgr_get_event_name(event);

    if (!bearer) {
        ERR("invalid bearer pointer is provided");
        goto err;
    }

    DEBUG("post %s to all dedicated bearers", event_name);

    dedicated_bearer = pdn_mgr_get_first_dedicated_bearer(bearer);
    while (dedicated_bearer) {
        dedicated_bearer_next = pdn_mgr_get_next_dedicated_bearer(dedicated_bearer);

        DEBUG("post %s to dedicate bearer", event_name);
        DEBUG_BEARER("", "dedicate bearer to post event", dedicated_bearer);

        if (!filter_cb || filter_cb(dedicated_bearer))
            pdn_mgr_post_event(ms, event, dedicated_bearer);

        dedicated_bearer = dedicated_bearer_next;
    }

    return 0;
err:
    return -1;
}

/*
        ### bearer filter functions ###
*/

#if 0
/* filter created bearers */
static int filter_cb_to_delete(const struct pdn_entity_t* bearer)
{
    return bearer->stat == PDN_STAT_CREATED || bearer->stat == PDN_STAT_DISCONNECTED;
}
#endif

/* filter disconnected bearers */
static int filter_cb_to_disconnect(const struct pdn_entity_t *bearer)
{
    return bearer->stat == PDN_STAT_CONNECTED;
}

/*
 Log all of active default bearers including dedicated bearers into System log
*/
static void pdn_mgr_log_stat()
{
    int i;
    int p;
    int j;
    struct pdn_entity_t *b;
    struct pdn_entity_t *ded_bearer;
    struct pdn_entity_t *ded_bearer_next;

    /* delete default bearers first */
    p = 0;
    FOR_EACH_BEARER(i, b)
    {
        /* bypass if the bearer is already deleted */
        if (b->stat == PDN_STAT_DELETED)
            continue;

        /* bypass if the bearer is not default bearer */
        if (b->ebt != PDN_TYPE_DEFAULT)
            continue;

        p++;
        DEBUG("[bearer-stat] * #%d default bearer (ebi=%d,apn=%s,primary=%d,connected=%d)", p, b->ebi, b->apn, b->primary_default_bearer,
              b->stat == PDN_STAT_CONNECTED);

        j = 0;
        ded_bearer = pdn_mgr_get_first_dedicated_bearer(b);
        while (ded_bearer) {
            j++;

            ded_bearer_next = pdn_mgr_get_next_dedicated_bearer(ded_bearer);

            DEBUG("[bearer-stat]   +#%d dedicated bearer (linked_ebi=%d,ebi=%d,connected=%d)", j, ded_bearer->default_bearer_ebi, ded_bearer->ebi,
                  ded_bearer->stat == PDN_STAT_CONNECTED);

            ded_bearer = ded_bearer_next;
        }
    }

    if (!p)
        DEBUG("[bearer-stat] * no bearer exists");
}

/*
 Perform integrity check between UE and PDN state machine to process bearer dropping without NAS OTA signaling.

 Parameters:
  ms : pointer to QxDM time-stamp.
  ebi : EPS bearer ID.
  bs : UE bearer state.

 Return:
  Success 0. Otherwise, -1
*/
int pdn_mgr_check_ue_stat_integrity(const unsigned long long *ms, int ebi, int bs)
{
    struct pdn_entity_t bi = {
        0,
    };
    struct pdn_entity_t *bearer_info = &bi;
    struct pdn_entity_t *bearer;

    int ue_active;
    int pdn_active;

    bearer_info->ebi = ebi;
    bearer = _get_bearer_by_keys(bearer_info);

    /* bypass if no EBI found */
    if (!bearer) {
        DEBUG("no EBI found in UE");
        goto err;
    }

    /* we are not interested in ACTIVE_PENDING or MODIFY */
    if (bs & 0x01)
        goto fini;

    /* get active stat */
    ue_active = bs == 2;
    pdn_active = bearer->stat == PDN_STAT_CONNECTED;

    /* if UE think the bearer is active while PDN does not */
    if (ue_active && !pdn_active) {

        DEBUG("UE bearer state is active while PDN bearer is inactive (ebi=%d)", ebi);
    }
    /* if UE think the bearer is inactive while PDN does not */
    else if (!ue_active && pdn_active) {
        if (bearer->disconnection_in_process) {
            DEBUG("broken bearer integrity detected - internal UE bearer state becomes inactive after PDN bearer disconnection is requested (ebi=%d)",
                  ebi);
        } else {
            DEBUG("broken bearer integrity detected - UE bearer state is inactive while PDN bearer is active (ebi=%d)", ebi);
            bearer_info->esm_cause = ESM_CAUSE_NETCOMM_DISCONNECTION_WITHOUT_NAS_SIGNALING;

            DEBUG("perform procedure of abnormal disconnection for bearer (ebi=%d)", ebi);
            pdn_mgr_post_event(ms, PDN_EVENT_DISCONNECT_REJECT, bearer_info);
        }
    }

fini:
    return 0;

err:
    return -1;
}

/*
 post event to PDN manager.

 Parameters:
  ms : QxDM time-stamp.
  bearer_info: bearer information that contains id, type, apn or etc.
  event : bearer activity.

 Return:
   Success 0. Otherwise, -1.
*/
int pdn_mgr_post_event(const unsigned long long *ms, int event, const struct pdn_entity_t *bearer_info)
{
    const char *event_name = pdn_mgr_get_event_name(event);

    struct pdn_entity_t *bearer = NULL;
    int att_rej_flag = 0;
    int bearer_to_stay_flag = 0;

    int ignore_pdn_event = 0;

    if (!event_name) {
        ERR("unknown event received (event=%d)", event);
        goto err;
    }

    /* get bearer */
    if (bearer_info)
        bearer = _get_bearer_by_keys(bearer_info);

    /*
        ### debug information ###
    */

    DEBUG("[%s] event received", event_name);
    if (bearer_info)
        DEBUG_BEARER(event_name, "bearer-param", bearer_info);

    /*
        ### pre-process event ###
    */

    /* print debug information */
    if (bearer) {
        DEBUG_BEARER(event_name, "bearer-before", bearer);
    }

#define GO_TO_ERR_IF_NO_BEARER_EXISTS() \
    { \
        if (!bearer) { \
            ERR("[%s] bearer not found", event_name); \
            goto err; \
        } \
    } \
    while (0)

    switch (event) {
            /*
                    ### attach ###
            */

        case PDN_EVENT_ATTACH_REQUEST:
            DEBUG("[%s] post DETACH_REQUEST by ATTACH_REQUEST", event_name);
            pdn_mgr_post_event(ms, PDN_EVENT_DETACH_REQUEST, bearer_info);

            DEBUG("[%s] wait for primary default bearer creation", event_name);
            _pdn_mgr->primary_default_bearer_to_create = 1;
            break;

        case PDN_EVENT_ATTACH_ACCEPT:
        case PDN_EVENT_ATTACH_COMPLETE:
            break;

            /*
                    ### detach ###
            */

        case PDN_EVENT_ATTACH_REJECT:
            att_rej_flag = 1;
        case PDN_EVENT_DETACH_REQUEST: {
            int i;
            struct pdn_entity_t *b;
            int forwarding_event = att_rej_flag ? PDN_EVENT_PRECONNECT_REJECT : PDN_EVENT_DISCONNECT_ACCEPT;

            /* hand-over causes into all bearers */
            if (att_rej_flag) {
                FOR_EACH_BEARER(i, b)
                {
                    if (b->stat == PDN_STAT_DELETED)
                        continue;
                    pdn_mgr_update_causes(event_name, b, bearer_info);
                }
            }

            /* delete default bearers first */
            FOR_EACH_BEARER(i, b)
            {

                /* bypass if the bearer is not default bearer */
                if (b->ebt != PDN_TYPE_DEFAULT)
                    continue;

                /* bypass if the bearer is already deleted */
                if (b->stat == PDN_STAT_DELETED)
                    continue;

                /* delete deleting */
                DEBUG("[%s] post %s by %s (#%d)", event_name, pdn_mgr_get_event_name(forwarding_event), event_name, i);
                pdn_mgr_post_event(ms, forwarding_event, b);
            }

            /* delete left-over (orphan) bearers */
            FOR_EACH_BEARER(i, b)
            {

                /* bypass if the bearer is already deleted */
                if (b->stat == PDN_STAT_DELETED)
                    continue;

                /* delete deleting */
                DEBUG("[%s] post %s by %s (#%d)", event_name, pdn_mgr_get_event_name(forwarding_event), event_name, i);
                pdn_mgr_post_event(ms, forwarding_event, b);
            }
            break;
        }

        case PDN_EVENT_DETACH_ACCEPT:
            break;

            /*
                    ### PDN connection ###
            */

        case PDN_EVENT_PRECONNECT_REQUEST:
            /* delete any existing bearer */
            if (bearer) {
                DEBUG("[%s] bearer exists, post PDN_EVENT_DISCONNECT_ACCEPT by PRECONNECT_REQUEST to existing bearer", event_name);
                pdn_mgr_post_event(ms, PDN_EVENT_DISCONNECT_ACCEPT, bearer_info);
            }

            bearer = NULL;
            DEBUG("[%s] post CREATE by PRECONNECT_REQUEST", event_name);
            pdn_mgr_post_event(ms, PDN_EVENT_CREATE, bearer_info);

            /* find the newly created bearer */
            bearer = _get_bearer_by_keys(bearer_info);
            break;

        case PDN_EVENT_CONNECT_REQUEST: {
            struct pdn_entity_t *identical_bearer = NULL;

            /* do some garbage collection from 3GPP - find any bearer that already has the identical EBI assigned */
            if (bearer_info->ebi) {
                /*
                        This additional condition is to avoid buggy QC diag behavior sending "Activate default EPS
                        bearer context request" twice - as an embedded message inside of EMM and as an ESM message unwrapped from the EMM message.
                */
                if (bearer_info->pti && (bearer_info->pti == bearer->pti) && (bearer->req_connection)) {
                    DEBUG("[%s] identical PTI detected. use this request as an update instead", event_name);
                    ignore_pdn_event = 1;
                } else {
                    identical_bearer = _get_bearer_by_ebi(bearer_info->ebi);
                    if (identical_bearer) {
                        DEBUG("[%s] existing previous dead connectivity detected, delete bearer", event_name);
                        pdn_mgr_post_event(ms, PDN_EVENT_DISCONNECT_ACCEPT, bearer_info);
                    }
                }
            }

            /* find the newly created bearer after the dead bearer gets removed */
            bearer = _get_bearer_by_keys(bearer_info);
            if (!bearer) {
                DEBUG("[%s] bearer does not exists, post CREATE by CONNECT_REQUEST", event_name);
                pdn_mgr_post_event(ms, PDN_EVENT_CREATE, bearer_info);
            }

            /* find the newly created bearer */
            bearer = _get_bearer_by_keys(bearer_info);
            if (!bearer) {
                ERR("[%s] bearer does not exist after creating", event_name);
                goto err;
            }

            DEBUG("[%s] post UPDATE by CONNECT_REQUEST to bearer", event_name);
            pdn_mgr_post_event(ms, PDN_EVENT_UPDATE, bearer_info);
            break;
        }

        case PDN_EVENT_CONNECT_ACCEPT:
            GO_TO_ERR_IF_NO_BEARER_EXISTS();

            /* bypass if bearer is not just created */
            if (bearer->stat != PDN_STAT_CREATED) {
                ERR("[%s] incorrect bearer stat", event_name);
                goto err;
            }
            break;

            /*
                    ### PDN disconnection ###
            */

        case PDN_EVENT_PREDISCONNECT_REQUEST:
            GO_TO_ERR_IF_NO_BEARER_EXISTS();
            break;

        case PDN_EVENT_DISCONNECT_REQUEST:
            GO_TO_ERR_IF_NO_BEARER_EXISTS();

            /* set flag */
            bearer->disconnection_in_process = 1;
            break;

        case PDN_EVENT_DISCONNECT_ACCEPT:
            GO_TO_ERR_IF_NO_BEARER_EXISTS();

            /* bypass if bearer is not connected */
            if (bearer->stat != PDN_STAT_CONNECTED) {
                DEBUG("[%s] bearer is not connected, ignore PDN event", event_name);
                ignore_pdn_event = 1;
            }

            /* post disconnect to children */
            pdn_mgr_post_event_to_dedicate_bearers(ms, PDN_EVENT_DISCONNECT_ACCEPT, bearer_info, filter_cb_to_disconnect);
            break;

            /*
                    ### PDN reject ###
            */

        case PDN_EVENT_PRECONNECT_REJECT:
        case PDN_EVENT_CONNECT_REJECT:
        case PDN_EVENT_PREDISCONNECT_REJECT:
        case PDN_EVENT_DISCONNECT_REJECT: {
            GO_TO_ERR_IF_NO_BEARER_EXISTS();

            pdn_mgr_update_causes(event_name, bearer, bearer_info);

            /*
                ## special condition to keep bearer ##

                This is a special condition about ESM_CAUSE_LAST_PDN_DISCONNECTION_NOT_ALLOWED(0x31).
            */
            bearer_to_stay_flag =
                (event == PDN_EVENT_PREDISCONNECT_REJECT) && (bearer_info->esm_cause == ESM_CAUSE_LAST_PDN_DISCONNECTION_NOT_ALLOWED);
            break;
        }

            /*
                    ### PDN maintain ###
            */

        case PDN_EVENT_CREATE:
            /* create bearer */
            bearer = pdn_mgr_create_bearer(bearer_info);
            if (!bearer) {
                ERR("failed to create a bearer, PDN_EVENT_CREATE creation failure");
                goto err;
            }

            /* it is the primary default bearer if no bearer exists */
            if (_pdn_mgr->primary_default_bearer_to_create)
                DEBUG("[%s] primary default bearer created", event_name);

            /* store primary default flag */
            bearer->primary_default_bearer = _pdn_mgr->primary_default_bearer_to_create;
            _pdn_mgr->primary_default_bearer_to_create = 0;

            break;

        case PDN_EVENT_DELETE:
            GO_TO_ERR_IF_NO_BEARER_EXISTS();

            /* bypass if bearer is not created nor connected */
            if (bearer->stat != PDN_STAT_CREATED && bearer->stat != PDN_STAT_DISCONNECTED) {
                ERR("[%s] failed to delete bearer, incorrect stat", event_name);
                goto err;
            }

            /* post delete to children */
            pdn_mgr_post_event_to_dedicate_bearers(ms, PDN_EVENT_DELETE, bearer_info, NULL);
            break;

        case PDN_EVENT_UPDATE:
            GO_TO_ERR_IF_NO_BEARER_EXISTS();

            /* update bearer */
            if (pdn_mgr_update_bearer(bearer, bearer_info) < 0) {
                ERR("[%s] failed to update bearer", event_name);
                goto err;
            }
            break;

        default:
            break;
    }

    /*
        ### process event ###
    */

    /* call event */
    if (_pdn_mgr->event_cb) {
        if (ignore_pdn_event) {
            DEBUG("[%s] ignore event, do not call event handler", event_name);
        } else {
            DEBUG("[%s] call event callback", event_name);
            _pdn_mgr->event_cb(ms, event, event_name, bearer);
        }
    }

    /*
        ### post-process event ###
    */

    switch (event) {
            /*
                    ### attach ###
            */

        case PDN_EVENT_ATTACH_REQUEST:
            break;

        case PDN_EVENT_ATTACH_REJECT:
            break;

        case PDN_EVENT_ATTACH_ACCEPT:
        case PDN_EVENT_ATTACH_COMPLETE:
            break;

            /*
                    ### detach ###
            */

        case PDN_EVENT_DETACH_REQUEST:
        case PDN_EVENT_DETACH_ACCEPT:
            break;

            /*
                    ### PDN connection ###
            */

        case PDN_EVENT_PRECONNECT_REQUEST:
            break;

        case PDN_EVENT_CONNECT_REQUEST:
            bearer->req_connection = 1;
            break;

        case PDN_EVENT_CONNECT_ACCEPT:
            DEBUG("[%s] reset PTI", event_name);
            pdn_mgr_reset_bearer_pti(bearer);

            bearer->stat = PDN_STAT_CONNECTED;

            /* log stat */
            pdn_mgr_log_stat();
            break;

            /*
                    ### PDN disconnection ###
            */

        case PDN_EVENT_PREDISCONNECT_REQUEST:
        case PDN_EVENT_DISCONNECT_REQUEST:
            break;

        case PDN_EVENT_DISCONNECT_ACCEPT:
            bearer->stat = PDN_STAT_DISCONNECTED;

            DEBUG("[%s] post DELETE by DISCONNECT_ACCEPT to bearer", event_name);
            pdn_mgr_post_event(ms, PDN_EVENT_DELETE, bearer_info);
            break;

            /*
                    ### PDN reject ###
            */

        case PDN_EVENT_PRECONNECT_REJECT:
        case PDN_EVENT_CONNECT_REJECT:
        case PDN_EVENT_PREDISCONNECT_REJECT:
        case PDN_EVENT_DISCONNECT_REJECT: {
            if (!bearer_to_stay_flag) {
                DEBUG("[%s] post PDN_EVENT_DISCONNECT_ACCEPT by %s to existing bearer", event_name, event_name);
                pdn_mgr_post_event(ms, PDN_EVENT_DISCONNECT_ACCEPT, bearer_info);
            } else {
                DEBUG("[%s] bearer not allowed to be disconnected", event_name);
            }
            break;
        }

            /*
                    ### PDN maintain ###
            */

        case PDN_EVENT_CREATE:
            bearer->stat = PDN_STAT_CREATED;

            if (bearer->ebt == PDN_TYPE_DEFAULT) {
                _pdn_mgr->total_created_default_bearers++;
                DEBUG("[%s] add default bearer (default_bearers=%d,dedicated_bearers=%d)", event_name, _pdn_mgr->total_created_default_bearers,
                      _pdn_mgr->total_created_dedicated_bearers);
            } else if (bearer->ebt == PDN_TYPE_DEDICATED) {
                _pdn_mgr->total_created_dedicated_bearers++;
                DEBUG("[%s] add dedicated bearer (default_bearers=%d,dedicated_bearers=%d)", event_name, _pdn_mgr->total_created_default_bearers,
                      _pdn_mgr->total_created_dedicated_bearers);
            } else {
                ERR("[%s] unknown EBT (ebt=%d)", event_name, bearer->ebt);
                goto err;
            }

            /* log stat */
            pdn_mgr_log_stat();
            break;

        case PDN_EVENT_DELETE:
            /* update stat */
            bearer->stat = PDN_STAT_DELETED;

            if (bearer->ebt == PDN_TYPE_DEFAULT) {
                _pdn_mgr->total_created_default_bearers--;
                DEBUG("[%s] remove default bearer (default_bearers=%d,dedicated_bearers=%d)", event_name, _pdn_mgr->total_created_default_bearers,
                      _pdn_mgr->total_created_dedicated_bearers);
            } else if (bearer->ebt == PDN_TYPE_DEDICATED) {
                _pdn_mgr->total_created_dedicated_bearers--;
                DEBUG("[%s] remove dedicated bearer (default_bearers=%d,dedicated_bearers=%d)", event_name, _pdn_mgr->total_created_default_bearers,
                      _pdn_mgr->total_created_dedicated_bearers);
            } else {
                ERR("[%s] unknown EBT (ebt=%d)", event_name, bearer->ebt);
                goto err;
            }

            if (pdn_mgr_delete_bearer(bearer) < 0) {
                DEBUG("[%s] failed to delete bearer", event_name);
                goto err;
            }

            /* log stat */
            pdn_mgr_log_stat();
            break;

        case PDN_EVENT_UPDATE:
            break;

        default:
            ERR("[%si] unknown event found (event=%d)", event_name, event);
            goto err;
    }

    /* print debug information */
    if (bearer) {
        DEBUG_BEARER(event_name, "bearer-after", bearer);
    }

    return 0;
err:
    return -1;
}

/*
 set main callback of PDN manager.

 Parameters:
  on_event: new callback
*/
void pdn_mgr_set_event(pdn_mgr_on_event_callback event_cb)
{
    _pdn_mgr->event_cb = event_cb;
}

/*
 Initiate the singleton object of PDN manager.
*/
void pdn_mgr_init()
{
    memset(_pdn_mgr, 0, sizeof(*_pdn_mgr));
}

/*
 Finalize the singleton object of PDN manager.
*/
void pdn_mgr_fini()
{
    int i;
    struct pdn_entity_t *bearer;

    /* for each of bearers */
    FOR_EACH_BEARER(i, bearer)
    {
        pdn_mgr_reset_bearer(bearer);
    }
}

#ifdef CONFIG_UNIT_TEST

#include <stdio.h>

//#define PRINTF(...) DEBUG(__VA_ARGS__)
#define PRINTF(...) printf(__VA_ARGS__)

int pdn_mgr_on_event(int event, const char *event_name, struct pdn_entity_t *bearer)
{
    if (bearer) {
        PRINTF("######## got %s event (apn=%s,ebi=%d,linked_ebi=%d)\n", event_name, bearer->apn, bearer->id, bearer ? bearer->default_bearer_ebi : 0);
    } else {
        PRINTF("######## got %s event\n", event_name);
    }
}

int main(int argc, char *argv[])
{
    struct pdn_entity_t bearer_info;

    int rc;
    int i;

    PRINTF("* init pdn mgr\n");
    pdn_mgr_init();

    PRINTF("set event callback\n");
    pdn_mgr_set_event(pdn_mgr_on_event);

    unsigned long long ms = 0;

    for (i = 0; i < 100; i++) {
        PRINTF("* test #1\n");
        PRINTF("1. create data and connect data\n");
        PRINTF("2. create dedicated data, ims, ems\n");
        PRINTF("3. delete ems, ims and data\n");

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 5;
        bearer_info.apn = "nxtgenphone";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.id = 5;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CONNECT, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEDICATED;
        bearer_info.id = 8;
        bearer_info.default_bearer_id = 5;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 6;
        bearer_info.apn = "ims";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 7;
        bearer_info.apn = "ems";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.id = 7;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_DELETE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.id = 6;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_DELETE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.id = 5;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_DELETE, &bearer_info);
        if (rc < 0)
            goto err;
    }

    for (i = 0; i < 100; i++) {
        PRINTF("* test #2\n");
        PRINTF("1. create data and connect data\n");
        PRINTF("2. create dedicated data, ims, ems\n");
        PRINTF("3. detach\n");
        PRINTF("3. delete all\n");

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 5;
        bearer_info.apn = "nxtgenphone";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.id = 5;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CONNECT, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEDICATED;
        bearer_info.id = 8;
        bearer_info.default_bearer_id = 5;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 6;
        bearer_info.apn = "ims";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 7;
        bearer_info.apn = "ems";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        rc = pdn_mgr_post_event(&ms, PDN_EVENT_DETACH, NULL);
        if (rc < 0)
            goto err;

        rc = pdn_mgr_post_event(&ms, PDN_EVENT_DELETE_ALL, NULL);
        if (rc < 0)
            goto err;
    }

    for (i = 0; i < 100; i++) {
        PRINTF("* test #3\n");
        PRINTF("1. create data and connect data\n");
        PRINTF("2. create dedicated data, ims, ems\n");
        PRINTF("3. create and connect dedicated ims\n");
        PRINTF("4. delete dedicated ims\n");
        PRINTF("5. detach\n");

        rc = pdn_mgr_post_event(&ms, PDN_EVENT_ATTACH_REQUEST, NULL);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 5;
        bearer_info.apn = "nxtgenphone";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.id = 5;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CONNECT, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEDICATED;
        bearer_info.id = 8;
        bearer_info.default_bearer_id = 5;
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 6;
        bearer_info.apn = "ims";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        memset(&bearer_info, 0, sizeof(bearer_info));
        bearer_info.type = PDN_TYPE_DEFAULT;
        bearer_info.id = 7;
        bearer_info.apn = "ems";
        rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
        if (rc < 0)
            goto err;

        int j;
        for (j = 0; j < 10; j++) {
            memset(&bearer_info, 0, sizeof(bearer_info));
            bearer_info.type = PDN_TYPE_DEDICATED;
            bearer_info.id = 9;
            bearer_info.default_bearer_id = 6;
            rc = pdn_mgr_post_event(&ms, PDN_EVENT_CREATE, &bearer_info);
            if (rc < 0)
                goto err;

            memset(&bearer_info, 0, sizeof(bearer_info));
            bearer_info.id = 9;
            rc = pdn_mgr_post_event(&ms, PDN_EVENT_CONNECT, &bearer_info);
            if (rc < 0)
                goto err;

#if 0
            memset(&bearer_info, 0, sizeof(bearer_info));
            bearer_info.id = 9;
            rc = pdn_mgr_post_event(&ms, PDN_EVENT_DISCONNECT, &bearer_info);
            if (rc < 0)
                goto err;
#endif

            memset(&bearer_info, 0, sizeof(bearer_info));
            bearer_info.id = 9;
            rc = pdn_mgr_post_event(&ms, PDN_EVENT_DELETE, &bearer_info);
            if (rc < 0)
                goto err;
        }

        rc = pdn_mgr_post_event(&ms, PDN_EVENT_DETACH, NULL);
        if (rc < 0)
            goto err;

        rc = pdn_mgr_post_event(&ms, PDN_EVENT_DELETE_ALL, NULL);
        if (rc < 0)
            goto err;
    }

    pdn_mgr_fini();

    return 0;

err:
    PRINTF("failed\n");
    return -1;
}

#endif
