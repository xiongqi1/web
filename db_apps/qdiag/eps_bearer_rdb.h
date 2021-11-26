#ifndef __EPS_BEARER_RDB__
#define __EPS_BEARER_RDB__

#include "pdn_mgr.h"

/* max RDB session */
#define MAX_BEARER_SESSION 6

/* use short bearer rdb flush interval for test mode */
#ifdef TEST_MODE_SHORT_BEARER_RDB_FLUSH
#warning "[TEST_MODE] short bearer rdb flush is applied"
#undef BEARER_RDB_FLUSH_INTERVAL
#define BEARER_RDB_FLUSH_INTERVAL (30 * 1000)
#endif

int eps_bearer_rdb_reset_all_sess(time_t tm, time_t tm_monotonic);
int eps_bearer_rdb_perform_periodic_flush(time_t tm, time_t tm_monotonic);
int eps_bearer_rdb_on_pdn_mgr_event(const unsigned long long *ms, int event, const char *event_name, struct pdn_entity_t *pdn_bearer);
int eps_bearer_rdb_init(void);
void eps_bearer_rdb_fini(void);
#endif
