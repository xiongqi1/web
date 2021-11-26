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


#ifndef CALLS_H_20090421_
#define CALL_H_20090421_

#include "slic/types.h"

#define CALLS_NUMBER_SIZE 64
#define CALLS_MAX_MULTIPARTY_CALLS 6
#define CALLS_SIZE CALLS_MAX_MULTIPARTY_CALLS + 1 // plus incoming or waiting call

typedef enum { mobile_originated = 0, mobile_terminated = 1 } cc_direction_enum;
typedef enum { call_active = 0, call_held, call_dialing, call_alerting, call_incoming, call_waiting, call_dummy } cc_state_enum;
enum { cc_state_size = call_waiting + 1 };
typedef enum { call_voice = 0 } cc_mode_enum;

struct call_state_t
{
	cc_state_enum state;
	BOOL in_multiparty;
	BOOL empty;
};

struct call_t
{
	unsigned int id;
	cc_direction_enum direction;
	cc_mode_enum mode;
	char number[CALLS_NUMBER_SIZE];
	struct call_state_t current;
	struct call_state_t last;
};

struct call_control_t
{
	struct call_t calls[ CALLS_SIZE ];
	unsigned int count[cc_state_size];
	unsigned int count_all;
};

int calls_from_csv( struct call_t* calls, const char* call_list );

void calls_clear( struct call_t* calls );

unsigned int calls_count( struct call_t* calls, cc_state_enum state );

const struct call_t* calls_first_of( struct call_t* calls, cc_state_enum state );

void calls_init( struct call_t* calls );

int call_control_init( int idx, struct call_control_t* cc );

int call_control_update( int idx, struct call_control_t* cc, BOOL initialize );

void call_print( const struct call_t* call );

void calls_print( const struct call_t* call );

const struct call_t* calls_call( const struct call_t* call, unsigned int id );

BOOL call_disconnected( const struct call_t* call, int idx );
BOOL call_empty( const struct call_t* call );

unsigned int get_last_multi_party_call_id(struct call_t* calls);
unsigned int get_first_call_id_of(struct call_t* calls, cc_state_enum state);

BOOL outgoing_call_connected(const struct call_t* call);

#endif /* CALL_H_20090421_ */

