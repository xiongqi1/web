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

#ifndef MODEL_H_20090320_
#define MODEL_H_20090320_

#include "./commands.h"
#include "./notifications.h"

enum model_status_t {
	model_status_sim_ready=0,
	model_status_registered=1,
	model_status_attached=2,
	model_status_count
};

struct model_status_info_t {
	int status[model_status_count];
};

struct model_t
{
	int ( *const init )( void );
	
	int ( *const detect )(const char* manufacture, const char* model_name);
	int ( *const get_status )(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status);
	int ( *const set_status )(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status);

	const struct command_t* commands;
	const struct notification_t* notifications;
	const char* name;
	char variants[32];
};

#define AT_COP_TIMEOUT	(3*60)	/* max 5 min. for COP command. */
#define AT_COP_TIMEOUT_SCAN  (5*60)
#define AT_QUICK_COP_TIMEOUT (10)

void update_heartBeat();

/// initialize model
int model_physinit( void );
int model_init(const char* model);
int model_init_schedule(void);

/// handle AT notification (unsolicited response)
int model_notify( const char* notification );

/// run an RDB command
int model_run_command(const struct name_value_t* args,int* ignored);

/// run an RDB command, using default model
int model_run_command_default( const struct name_value_t* args );

/// return model name
const char* model_name( void );
const char* model_variants_name(void);
const struct model_t* get_model_instance();

int model_default_status(int* sim_ready,int* registered,int* attached);
extern int polling_dtmf_rdb_event(void);
extern int polling_rx_sms_event(void);
extern int is_ericsson;
extern int is_cinterion;
extern int is_cinterion_cdma;

#define run_once_define(name)		static int run_once_##name=0
#define run_once_func(name,func)	\
	do \
	{ \
		if(!run_once_##name) { \
			run_once_##name=(func); \
		} \
	} while(0)

#endif /* MODEL_H_20090320_ */

