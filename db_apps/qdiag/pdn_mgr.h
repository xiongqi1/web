/*!
 * C header for PDN manager
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

#ifndef __PDN_MGR_H__
#define __PDN_MGR_H__

/* total traceable maximum PDN session number - 4 bit in 3GPP */
#define PDN_MGR_MAX_PDN 16

struct pdn_entity_t;

/* PDN event for PDN activity */
enum
{
    PDN_EVENT_UNKNOWN = 0,

    /* global events */
    PDN_EVENT_ATTACH_REQUEST,
    PDN_EVENT_ATTACH_ACCEPT,
    PDN_EVENT_ATTACH_REJECT,
    PDN_EVENT_ATTACH_COMPLETE,
    PDN_EVENT_DETACH_REQUEST, /* broadcast DELETE to default bearers */
    PDN_EVENT_DETACH_ACCEPT,

    /* bearer events */
    PDN_EVENT_CREATE,
    PDN_EVENT_DELETE, /* broadcast to dedicate bearers */
    PDN_EVENT_UPDATE,

    PDN_EVENT_PRECONNECT_REQUEST,
    PDN_EVENT_PRECONNECT_REJECT,
    PDN_EVENT_CONNECT_REQUEST,
    PDN_EVENT_CONNECT_ACCEPT,
    PDN_EVENT_CONNECT_REJECT,

    PDN_EVENT_PREDISCONNECT_REQUEST,
    PDN_EVENT_PREDISCONNECT_REJECT,
    PDN_EVENT_DISCONNECT_REQUEST,
    PDN_EVENT_DISCONNECT_ACCEPT, /* broadcast to dedicate bearers */
    PDN_EVENT_DISCONNECT_REJECT,
};

/* PDN bearer state */
enum
{
    PDN_STAT_DELETED = 0,
    PDN_STAT_CREATED,
    PDN_STAT_CONNECTED,
    PDN_STAT_DISCONNECTED,
};

/* PDN bearer type */
enum
{
    PDN_TYPE_UNKNOWN = 0,
    PDN_TYPE_DEFAULT,
    PDN_TYPE_DEDICATED,
};

/* PDN event callback */
typedef int (*pdn_mgr_on_event_callback)(const unsigned long long *ms, int event, const char *event_name, struct pdn_entity_t *bearer);
/* Bearer filter function */
typedef int (*pdn_mgr_bearer_filter_callback)(const struct pdn_entity_t *bearer);

/* PDN entity class */
struct pdn_entity_t {
    int stat;
    int ebt; /* EPS bearer type */

    int req_connection;         /* flag to avoid duplicated activation request by QC */
    int primary_default_bearer; /* primary default bearer flag */

    int disconnection_in_process; /* flag to avoid false integrity errors with UE internal state */

    /* reason codes */
    int emm_cause;
    int esm_cause;

    /* keys - following members in the priority order */
    int ebi;   /* EPS bearer ID */
    char *apn; /* access point name */
    int pti;   /* procedure transaction ID */

    /* double-linked list of dedicated bearers */
    struct pdn_entity_t *next;
    struct pdn_entity_t *prev;

    /*
        ### valid for default bearers ###
    */

    /* last dedicated bearer */
    struct pdn_entity_t *head;
    struct pdn_entity_t *tail;

    /* statistics - only default bearers contain statistics information */

    /*
        ### valid for default bearers ###
    */

    int default_bearer_ebi;              /* default bearer EPS bearer ID */
    struct pdn_entity_t *default_bearer; /* pointer to default bearer */

    /* private member */
    void *priv;
};

/* pdn_mgr.c */
const char *pdn_mgr_get_ebt_name(int ebt);
const char *pdn_mgr_get_event_name(int event);
int pdn_mgr_check_ue_stat_integrity(const unsigned long long *ms, int ebi, int bs);
int pdn_mgr_post_event(const unsigned long long *ms, int event, const struct pdn_entity_t *bearer_info);
void pdn_mgr_set_event(pdn_mgr_on_event_callback event_cb);
void pdn_mgr_init(void);
void pdn_mgr_fini(void);
#endif
