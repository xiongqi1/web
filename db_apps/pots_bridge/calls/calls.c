/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "cdcs_syslog.h"
#include "../pots_rdb_operations.h"
#include "./calls.h"

static const char* cc_direction_names[] = { "outgoing", "incoming" };
static const char* cc_state_names[] = { "active", "held", "dialing", "alerting", "incoming", "waiting" };
static const char* cc_mode_names[] = { "voice" };

static const char* call_from_csv(struct call_t* calls, const char* entry)
{
	const char* p = entry;
	char* n;
	int id = atoi(p);
	int i = id - 1;
	if (id <= 0 || id > CALLS_SIZE)
	{
		SYSLOG_ERR("expected call id between 1 and %d, got %d", CALLS_SIZE, id);
		return NULL;
	}
	calls[i].current.empty = FALSE;
	while (*p++ != ',');
	calls[i].direction = (cc_direction_enum)(*p - '0');
	p += 2;
	calls[i].current.state = (cc_state_enum)(*p - '0');
	p += 2;
	calls[i].mode = (cc_mode_enum)(*p - '0');
	p += 2;
	calls[i].current.in_multiparty = *p - '0';
	calls[i].number[0] = 0;
	n = calls[i].number;
	++p;
	if (*p == '\0')
	{
		return p;
	}
	if (*p == '\n')
	{
		return ++p;
	}
	if (*p == ',')
	{
		++p;
	}
	while (*p != 0 && *p != '\n' && *p != ',')
	{
		if (*p != '"')
		{
			*n++ = *p;
		}
		++p;
	}
	*n = 0;
	while (*p != '\0' && *p++ != '\n');
	return p;
}

int calls_from_csv(struct call_t* calls, const char* call_list)
{
	const char* p = call_list;
	if (!p)
	{
		SYSLOG_ERR("got NULL call list");
		return -1;
	}
	while (*p)
	{
		if ((p = call_from_csv(calls, p)) == NULL)
		{
			return -1;
		}
	}
	return 0;
}

static void call_save(struct call_t* call)
{
	call->last.state = call->current.state;
	call->last.empty = call->current.empty;
	call->last.in_multiparty = call->current.in_multiparty;
}

static void call_restore(struct call_t* call)
{
	call->current.state = call->last.state;
	call->current.empty = call->last.empty;
	call->current.in_multiparty = call->last.in_multiparty;
}

BOOL call_disconnected(const struct call_t* call, int idx)
{
	static BOOL prev_curr_empty[CALLS_SIZE];
	static BOOL prev_last_empty[CALLS_SIZE];
	static int initialzed = 0;
	int i;
	BOOL disconnected;
	if (initialzed == 0) {
		for (i = 0; i < CALLS_SIZE; i++) {
			prev_curr_empty[i] = prev_last_empty[i] = TRUE;
		}
		initialzed = 1;
	}
#if (0)	
	SYSLOG_DEBUG("pc_empty[%d] %d, pl_empty[%d] %d, c_empty %d, l_empty %d",
		idx, prev_curr_empty[idx], idx, prev_last_empty[idx], call->current.empty, call->last.empty);
	if (!prev_curr_empty[idx] && !prev_last_empty[idx] && call->current.empty && call->last.empty) {
		SYSLOG_DEBUG("==============================================================");
		SYSLOG_DEBUG("   abrupted disconnection condition detected  ");
		SYSLOG_DEBUG("==============================================================");
	}
#endif	
	/* add more condition because when incoming call drop soon after ringing, sometimes curr_empty and last_empty
	 * status change from both 0 to both 1 and previous disconnect condition can not be fulfilled. */
	disconnected = (call->current.empty && !call->last.empty) ||
				   (!prev_curr_empty[idx] && !prev_last_empty[idx] && call->current.empty && call->last.empty);
	prev_curr_empty[idx] = call->current.empty;
	prev_last_empty[idx] = call->last.empty;
	//SYSLOG_DEBUG("old disconnected %d, new disconnected %d", (call->current.empty && !call->last.empty), disconnected);
	if (disconnected) {
		for (i = 0; i < CALLS_SIZE; i++) {
			prev_curr_empty[i] = prev_last_empty[i] = TRUE;
		}
	}
	return disconnected;
}

BOOL call_empty(const struct call_t* call)
{
	//SYSLOG_DEBUG("c_empty %d, l_empty %d", call->current.empty, call->last.empty);
	return call->current.empty && call->last.empty;
}

void call_clear(struct call_t* call)
{
	call->current.empty = TRUE;
}

void calls_clear(struct call_t* calls)
{
	int i;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		call_clear(&calls[i]);
	}
}

static void calls_save(struct call_t* calls)
{
	int i;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		call_save(&calls[i]);
	}
}

static void calls_restore(struct call_t* calls)
{
	int i;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		call_restore(&calls[i]);
	}
}

void calls_init(struct call_t* calls)
{
	int i;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		calls[i].id = i + 1;
	}
	calls_clear(calls);
	calls_save(calls);
}

static void call_control_count(struct call_control_t* cc)
{
	int i;
	cc->count_all = 0;
	for (i = 0; i < cc_state_size; ++i)
	{
		cc->count[i] = 0;
	};
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (!cc->calls[i].current.empty)
		{
			++cc->count[ cc->calls[i].current.state ];
		}
	}
	for (i = 0; i < cc_state_size; ++i)
	{
		cc->count_all += cc->count[i];
	}
}

extern char* call_type_name[MAX_CALL_TYPE_INDEX];
#ifdef BLOCK_CALL_CONTROL_UPDATE_DURING_DTMF_KEY_PROCESSING
extern BOOL need_to_block_call_control_update(void);
#endif
int call_control_update(int idx, struct call_control_t* cc, BOOL initialize)
{
	int ret;
	char call_list[1024] = {0,};

#ifdef BLOCK_CALL_CONTROL_UPDATE_DURING_DTMF_KEY_PROCESSING
	/* block call list update command during outband DTMF key processing to prevent
	 * missing digits.
	 */
	if (need_to_block_call_control_update() && !initialize)
	{
		SYSLOG_ERR("ignore call list update command during outband DTMF command processing");
		return -1;
	}
#endif

	if (!initialize)
	{
		if (send_rdb_command_blocking((const char *)slic_info[idx].rdb_name_at_cmd, RDB_SCMD_CALLS_LIST, get_cmd_retry_count(idx)) != 0)
		{
			SYSLOG_ERR("failed to retrieve call list");
			return -1;
		}
		if (rdb_get_single(rdb_variable((const char *)slic_info[idx].rdb_name_calls_list, "", ""), call_list, sizeof(call_list)) != 0)
		{
			SYSLOG_ERR("failed to get calls list (%s)", strerror(errno));
			return -1;
		}
	}
	//SYSLOG_ERR("calls list = (%s)", call_list);
	calls_save(cc->calls);
	calls_clear(cc->calls);
	if ((ret = calls_from_csv(cc->calls, call_list)) != 0)
	{
		calls_restore(cc->calls);
	}
	call_control_count(cc);
	/* update call list with null list and set to error here to set poll timer */
	if (strlen(call_list) == 0)
		ret = -1;
	//SYSLOG_ERR("ret = %d)", ret);
	return ret;
}

int call_control_init(int idx, struct call_control_t* cc)
{
	calls_init(cc->calls);
	(void) call_control_update(idx, cc, TRUE);
	return 0;
}

unsigned int calls_count(struct call_t* calls, cc_state_enum state)
{
	unsigned int i, count;
	for (i = count = 0; i < CALLS_SIZE; count += (!calls[i].current.empty && calls[i].current.state == state), ++i);
	return count;
}

const struct call_t* calls_first_of(struct call_t* calls, cc_state_enum state)
{
	unsigned int i;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (!calls[i].current.empty && calls[i].current.state == state)
		{
			return &calls[i];
		}
	}
	return NULL;
}

void call_print(const struct call_t* call)
{
	if (call->current.empty)
	{
		SYSLOG_DEBUG("id %u: empty", call->id);
		return;
	}
	SYSLOG_DEBUG("call id %u: %s; %s; %s; %sin multiparty; #: '%s'"
	             , call->id
	             , cc_direction_names[ call->direction ]
	             , cc_state_names[ call->current.state ]
	             , call->mode > call_voice ? "other" : cc_mode_names[ call->mode ]
	             , call->current.in_multiparty ? "" : "not "
	             , call->number);
}

void calls_print(const struct call_t* calls)
{
	int i, k;
	for (i = k = 0; i < CALLS_SIZE; k += (!calls[i].current.empty), ++i);
	//SYSLOG_DEBUG( "%d call(s)%s", k, k > 0 ? ":" : "" );
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (!calls[i].current.empty)
		{
			call_print(&calls[i]);
		}
	}
}

const struct call_t* calls_call(const struct call_t* calls, unsigned int id)
{
	if (id == 0 || id > CALLS_SIZE || calls[ id - 1 ].current.empty)
	{
		return NULL;
	}
	return &calls[ id - 1 ];
}

unsigned int get_last_multi_party_call_id(struct call_t* calls)
{
	unsigned int i, cnt = 0, last_call_id = 0;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (calls[i].current.in_multiparty)
		{
			cnt++;
			last_call_id = calls[i].id;
		}
	}
	return ((cnt > 1)? last_call_id:0);
}

unsigned int get_first_call_id_of(struct call_t* calls, cc_state_enum state)
{
	unsigned int i;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (!calls[i].current.empty &&
		    (state == call_dummy || calls[i].current.state == state))
		{
			return calls[i].id;
		}
	}
	return 0;
}

BOOL outgoing_call_connected(const struct call_t* call)
{
	//SYSLOG_DEBUG("c_empty %d, l_empty %d", call->current.empty, call->last.empty);
	if (!call)
		return FALSE;
	if (call->direction == mobile_originated &&
		(call->last.state ==  call_dialing || call->last.state ==  call_alerting)&&
		call->current.state == call_active)
		return TRUE;
	return FALSE;
}
