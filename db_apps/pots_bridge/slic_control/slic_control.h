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


#ifndef SLIC_CONTROL_H_20090220_
#define SLIC_CONTROL_H_20090220_


#include "slic/types.h"
#include "slic/slic_control.h"
#include "../fsk/packet.h"
#include "./tone_pattern.h"
#ifdef USE_ALSA
#include "./alsa_control.h"
#else
#include "slic/slic_ioctl.h"
#endif

/* slic cal data save/restore feature */
#include "slic/calibration.h"


typedef enum { slic_tone_pattern_failure, slic_tone_pattern_enabled, slic_tone_pattern_disabled } slic_tone_pattern_t;

// see S002_2001, Appendix E; implement more distinctive rings as needed
/* support international telephony profile */
typedef enum
{
	slic_distinctive_ring_0 = 0,
	slic_distinctive_ring_1,
	slic_distinctive_ring_canada
} slic_distinctive_ring_enum;
/* end of support international telephony profile */

/* append slic_led_control */
typedef enum { led_off, led_on, led_flash_on, led_flash_off, led_vmwi_on } pots_led_control_type;

int slic_open();

void slic_close( int slic_fd );
int slic_play( int slic_fd, slic_tone_enum tone );
int slic_play_pattern( int slic_fd, const struct slic_array_ptr_t* pattern );
int slic_play_fixed_pattern( int slic_fd, slic_tone_pattern_t pattern, int is_blocking );
int slic_play_distinctive_ring( int slic_fd, slic_distinctive_ring_enum pattern );
int slic_fsk_send( int slic_fd, const fsk_packet* );
void slic_handle_events( int idx );
int slic_get_loop_state( int slic_fd, slic_on_off_hook_enum* loop_state );
int slic_enable_pcm( int slic_fd, slic_pcm_type pcm_type );
int slic_enable_pcm_dynamic( int slic_fd, BOOL enabled, int idx );
int slic_set_loopback_mode( int slic_fd, int loopback_mode );
extern void slic_handle_pots_led( int cid, pots_led_control_type mode );
int slic_register_event_handler( slic_event_enum event, int ( *callback )( int idx, const struct slic_event_t* ) );
int slic_set_multi_channel_loopback_mode( int slic_fd, int on_off );
int slic_send_vmwi_fsk( int slic_fd, const fsk_packet* packet );

#define MAX_DEVICE_NAME_SIZE	255
#define RDB_VALUE_SIZE_SMALL 	128
#define RDB_VALUE_SIZE_LARGE 	256

#define MAX_CALL_TYPE_INDEX		4
typedef enum { NONE = 0, VOICE_ON_3G, VOICE_ON_IP, FAX_ON_IP } pots_call_type;
struct slic_t
{
	int slic_fd;
	int cid;
	char dev_name[MAX_DEVICE_NAME_SIZE];
	pots_call_type call_type;
	BOOL call_active;
	char rdb_name_at_cmd[RDB_VALUE_SIZE_SMALL];
	char rdb_name_phone_cmd[RDB_VALUE_SIZE_SMALL];
	char rdb_name_calls_list[RDB_VALUE_SIZE_SMALL];
	char rdb_name_cmd_result[RDB_VALUE_SIZE_SMALL];
	char rdb_name_dtmf_cmd[RDB_VALUE_SIZE_SMALL];
};

typedef enum { CPLD_MODE_LOOP = 0, CPLD_MODE_VOICE, CPLD_MODE_BUFFER } slic_cpld_mode;

extern struct slic_t slic_info[MAX_CHANNEL_NO];
extern void initialize_slic_variables ( void );
extern void assign_slic_channel_to_call_fuction( int index, struct slic_t *slic_p );
extern void assign_slic_channel_to_rdb_variables(int index, struct slic_t *slic_p);
extern int slic_get_channel_id( int slic_fd, int* cid );
extern void display_slic_info_db( void );
extern int get_cid_from_slic_fd( int slic_fd );
extern int get_max_slic_fd( void );
extern int check_call_resource_available( int index, pots_call_type call_type );
extern void set_slic_call_active_state( int index, BOOL active );
extern int get_slic_first_call_active_index( pots_call_type call_type );
extern int get_slic_next_call_active_index(pots_call_type call_type, int idx);
extern int get_slic_other_call_active_index(int idx);
extern int is_match_slic_call_type(int index, pots_call_type call_type);

#ifdef USE_ALSA
#ifdef MULTI_CHANNEL_SLIC
#error Using new pots interface for ALSA is not implemented yet!
#endif
#endif

/* change PCM clk selecting gpio to phone module when phone module was detected. */
int slic_change_pcm_clk_source( int slic_fd, int direction );

/* slic cal data save/restore feature */
int slic_initialize_device_hw( int slic_fd, slic_cal_data_type *cal_data_p, BOOL need_cal );
int write_slic_cal_data_to_NV(BOOL force_cal);
/* end of slic cal data save/restore feature */

/* support international telephony profile */
int slic_set_tel_profile( int slic_fd, tel_profile_type tel_profile );
/* end of support international telephony profile */

/* slic register content display */
int slic_display_reg( int slic_fd );
/* end of slic register content display */

/* free run mode set */
int slic_set_freerun_mode( int slic_fd, int freerun_mode );
/* end of free run mode set */

int slic_tx_mute_override(int slic_fd, int mute_override);

int slic_check_mode(int slic_fd, char *mode_name);
int slic_toggle_debug_mode(int slic_fd, int debug_mode);
int slic_reg_rw(int slic_fd, reg_rw_cmd_type *cmd);
int get_cmd_retry_count(int index);
int slic_set_dtmf_tx_mode(int slic_fd, dtmf_tx_type tx_mode);
void slic_change_cpld_mode(slic_cpld_mode cpld_mode);
int slic_play_dtmf_tones(int slic_fd, const char* dial, BOOL autodial_mode);

#endif /* SLIC_CONTROL_H_20090220_ */
